/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <windows.h>
#include <pthread.h>
#include <afs/errmap_nt.h>
#include <afs/secutil_nt.h>

#include "procmgmt.h"
#include "pmgtprivate.h"



/* Signal disposition table and associated definitions and locks */

typedef struct {
    struct sigaction action;	/* signal action information */
} sigtable_entry_t;

static sigtable_entry_t signalTable[NSIG];	/* signal table; slot 0 unused */
static pthread_mutex_t signalTableLock;	/* lock protects signalTable */

/* Global signal block lock; all signal handlers are serialized for now */
static pthread_mutex_t signalBlockLock;

/* Named pipe prefix for sending signals */
#define PMGT_SIGNAL_PIPE_PREFIX  "\\\\.\\pipe\\TransarcAfsSignalPipe"

/* Macro to test process exit status for an uncaught exception */
#define PMGT_IS_EXPSTATUS(status)   (((status) & 0xF0000000) == 0xC0000000)



/* Child process table and associated definitions and locks */

typedef struct {
    HANDLE p_handle;		/* process handle (NULL if table entry not valid) */
    BOOL p_reserved;		/* table entry is reserved for spawn */
    DWORD p_id;			/* process id */
    BOOL p_terminated;		/* process terminated; status available/valid */
    DWORD p_status;		/* status of terminated process */
} proctable_entry_t;

/* Max number of active child processes supported */
#define PMGT_CHILD_MAX  100

static proctable_entry_t procTable[PMGT_CHILD_MAX];	/* child process table */
static int procEntryCount;	/* count of valid entries in procTable */
static int procTermCount;	/* count of terminated entries in procTable */

/* lock protects procTable, procEntryCount, and procTermCount */
static pthread_mutex_t procTableLock;

/* Named shared memory prefix for passing a data buffer to a child process */
#define PMGT_DATA_MEM_PREFIX  "TransarcAfsSpawnDataMemory"

/* Named event prefix for indicating that a data buffer has been read */
#define PMGT_DATA_EVENT_PREFIX  "TransarcAfsSpawnDataEvent"

/* event signals termination of a child process */
static pthread_cond_t childTermEvent;


/* Exported data values */

void *pmgt_spawnData = NULL;
size_t pmgt_spawnDataLen = 0;


/* General definitions */

#define DWORD_OF_ONES   ((DWORD)0xFFFFFFFF)	/* a common Win32 failure code */






/* -----------------  Signals  ---------------- */


/*
 * SignalIsDefined() -- Determine if an integer value corresponds to a
 *     signal value defined for this platform.
 */
static int
SignalIsDefined(int signo)
{
    int isDefined = 0;

    if (signo >= 1 && signo <= (NSIG - 1)) {
	/* value is in valid range; check specifics */
	switch (signo) {
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGILL:
	case SIGABRT:
	case SIGFPE:
	case SIGKILL:
	case SIGSEGV:
	case SIGTERM:
	case SIGUSR1:
	case SIGUSR2:
	case SIGCHLD:
	case SIGTSTP:
	    isDefined = 1;
	    break;
	}
    }
    return isDefined;
}


/*
 * DefaultActionHandler() -- Execute the default action for the given signal.
 */
static void __cdecl
DefaultActionHandler(int signo)
{
    switch (signo) {
    case SIGHUP:
    case SIGINT:
    case SIGKILL:
    case SIGTERM:
    case SIGUSR1:
    case SIGUSR2:
	/* default action is "exit" */
	ExitProcess(PMGT_SIGSTATUS_ENCODE(signo));
	break;
    case SIGQUIT:
    case SIGILL:
    case SIGABRT:
    case SIGFPE:
    case SIGSEGV:
	/* default action is "core" */
	/* Best we can do is to raise an exception that can be caught by
	 * Dr. Watson, which can in turn generate a crash dump file.
	 * The default exception handler will call ExitProcess() with
	 * our application-specific exception code.
	 */
	RaiseException((DWORD) PMGT_SIGSTATUS_ENCODE(signo),
		       EXCEPTION_NONCONTINUABLE, 0, NULL);
	break;
    case SIGCHLD:
	/* default action is "ignore" */
	break;
    case SIGTSTP:
	/* default action is "stop" */
	/* No good way to implement this from inside a process so ignore */
	break;
    default:
	/* no default action for specified signal value; just ignore */
	break;
    }
}


/*
 * ProcessSignal() -- Execute the specified or default handler for the given
 *     signal; reset the signal's disposition to SIG_DFL if necessary.
 *     If the signal's disposition is SIG_IGN then no processing takes place.
 *
 * ASSUMPTIONS: signo is valid (i.e., SignalIsDefined(signo) is TRUE).
 */
static void
ProcessSignal(int signo)
{
    struct sigaction sigEntry;

    if (signo != SIGKILL) {
	/* serialize signals, but never block processing of SIGKILL */
	(void)pthread_mutex_lock(&signalBlockLock);
    }

    /* fetch disposition of signo, updating it if necessary */

    (void)pthread_mutex_lock(&signalTableLock);
    sigEntry = signalTable[signo].action;

    if ((sigEntry.sa_handler != SIG_IGN) && (sigEntry.sa_flags & SA_RESETHAND)
	&& (signo != SIGILL)) {
	signalTable[signo].action.sa_handler = SIG_DFL;
    }
    (void)pthread_mutex_unlock(&signalTableLock);

    /* execute handler */

    if (sigEntry.sa_handler != SIG_IGN) {
	if (sigEntry.sa_handler == SIG_DFL) {
	    sigEntry.sa_handler = DefaultActionHandler;
	}
	(*sigEntry.sa_handler) (signo);
    }

    if (signo != SIGKILL) {
	(void)pthread_mutex_unlock(&signalBlockLock);
    }
}


/*
 * RemoteSignalThread() -- Thread spawned to process remote signal.
 *
 *     Param must be the signal number.
 */
static DWORD WINAPI
RemoteSignalThread(LPVOID param)
{
    int signo = (int)(intptr_t)param;
    DWORD rc = 0;

    if (SignalIsDefined(signo)) {
	/* process signal */
	ProcessSignal(signo);
    } else if (signo != 0) {
	/* invalid signal value */
	rc = -1;
    }
    return rc;
}


/*
 * RemoteSignalListenerThread() -- Thread spawned to receive and process
 *     remotely generated signals; never returns.
 *
 *     Param must be a handle for a duplex server message pipe in blocking
 *     mode.
 */
static DWORD WINAPI
RemoteSignalListenerThread(LPVOID param)
{
    HANDLE sigPipeHandle = (HANDLE) param;

    while (1) {
	/* wait for pipe client to connect */

	if ((ConnectNamedPipe(sigPipeHandle, NULL))
	    || (GetLastError() == ERROR_PIPE_CONNECTED)) {
	    /* client connected; read signal value */
	    int signo;
	    DWORD bytesXfered;

	    if ((ReadFile
		 (sigPipeHandle, &signo, sizeof(signo), &bytesXfered, NULL))
		&& (bytesXfered == sizeof(signo))) {
		HANDLE sigThreadHandle;
		DWORD sigThreadId;

		/* ACK signal to release sender */
		(void)WriteFile(sigPipeHandle, &signo, sizeof(signo),
				&bytesXfered, NULL);

		/* spawn thread to process signal; we do this so that
		 * we can always process a SIGKILL even if a signal handler
		 * invoked earlier fails to return (blocked/spinning).
		 */
		sigThreadHandle = CreateThread(NULL,	/* default security attr. */
					       0,	/* default stack size */
					       RemoteSignalThread, (LPVOID) (intptr_t)signo,	/* thread argument */
					       0,	/* creation flags */
					       &sigThreadId);	/* thread id */

		if (sigThreadHandle != NULL) {
		    (void)CloseHandle(sigThreadHandle);
		}
	    }
	    /* nothing to do if ReadFile, WriteFile or CreateThread fails. */

	} else {
	    /* connect failed; this should never happen */
	    Sleep(2000);	/* sleep 2 seconds to avoid tight loop */
	}

	(void)DisconnectNamedPipe(sigPipeHandle);
    }

    /* never reached */
    return (0);
}





/*
 * pmgt_SigactionSet() -- Examine and/or specify the action for a given
 *     signal (Unix sigaction() semantics).
 */
int
pmgt_SigactionSet(int signo, const struct sigaction *actionP,
		  struct sigaction *old_actionP)
{
    /* validate arguments */

    if (!SignalIsDefined(signo) || signo == SIGKILL) {
	/* invalid signal value or signal can't be caught/ignored */
	errno = EINVAL;
	return -1;
    }

    if (actionP && actionP->sa_handler == SIG_ERR) {
	/* invalid signal disposition */
	errno = EINVAL;
	return -1;
    }

    /* fetch and/or set disposition of signo */

    (void)pthread_mutex_lock(&signalTableLock);

    if (old_actionP) {
	*old_actionP = signalTable[signo].action;
    }

    if (actionP) {
	signalTable[signo].action = *actionP;
    }

    (void)pthread_mutex_unlock(&signalTableLock);

    return 0;
}


/*
 * pmgt_SignalSet() -- Specify the disposition for a given signal
 *     value (Unix signal() semantics).
 */
void (__cdecl * pmgt_SignalSet(int signo, void (__cdecl * dispP) (int))) (int) {
    struct sigaction newAction, oldAction;

    /* construct action to request Unix signal() semantics */

    newAction.sa_handler = dispP;
    sigemptyset(&newAction.sa_mask);
    newAction.sa_flags = SA_RESETHAND;

    if (!pmgt_SigactionSet(signo, &newAction, &oldAction)) {
	/* successfully set new signal action */
	return oldAction.sa_handler;
    } else {
	/* failed to set signal action; errno will have been set */
	return SIG_ERR;
    }
}


/*
 * pmgt_SignalRaiseLocal() -- Raise a signal in this process (C raise()
 *     semantics).
 */
int
pmgt_SignalRaiseLocal(int signo)
{
    int rc = 0;

    /* Process signal directly in the context of the calling thread.
     * This is the same as if the signal had been raised in this process
     * and this thread chosen to execute the handler.
     */

    if (SignalIsDefined(signo)) {
	/* process signal */
	ProcessSignal(signo);
    } else if (signo != 0) {
	/* invalid signal value */
	errno = EINVAL;
	rc = -1;
    }
    return rc;
}


/*
 * pmgt_SignalRaiseLocalByName() -- Raise a signal in this process where
 *     the signal is specified by name (C raise() semantics).
 *
 *     Upon successful completion, *libSigno is set to the process management
 *     library's constant value for signame.
 *
 *     Note: exists to implement the native-signal redirector (redirect_nt.c),
 *           which can't include procmgmt.h and hence can't get the SIG* decls.
 */
int
pmgt_SignalRaiseLocalByName(const char *signame, int *libSigno)
{
    int rc = 0;
    int signo;

    if (!strcmp(signame, "SIGHUP")) {
	signo = SIGHUP;
    } else if (!strcmp(signame, "SIGINT")) {
	signo = SIGINT;
    } else if (!strcmp(signame, "SIGQUIT")) {
	signo = SIGQUIT;
    } else if (!strcmp(signame, "SIGILL")) {
	signo = SIGILL;
    } else if (!strcmp(signame, "SIGABRT")) {
	signo = SIGABRT;
    } else if (!strcmp(signame, "SIGFPE")) {
	signo = SIGFPE;
    } else if (!strcmp(signame, "SIGKILL")) {
	signo = SIGKILL;
    } else if (!strcmp(signame, "SIGSEGV")) {
	signo = SIGSEGV;
    } else if (!strcmp(signame, "SIGTERM")) {
	signo = SIGTERM;
    } else if (!strcmp(signame, "SIGUSR1")) {
	signo = SIGUSR1;
    } else if (!strcmp(signame, "SIGUSR2")) {
	signo = SIGUSR2;
    } else if (!strcmp(signame, "SIGCLD")) {
	signo = SIGCLD;
    } else if (!strcmp(signame, "SIGCHLD")) {
	signo = SIGCHLD;
    } else if (!strcmp(signame, "SIGTSTP")) {
	signo = SIGTSTP;
    } else {
	/* unknown signal name */
	errno = EINVAL;
	rc = -1;
    }

    if (rc == 0) {
	*libSigno = signo;
	rc = pmgt_SignalRaiseLocal(signo);
    }
    return rc;
}


/*
 * pmgt_SignalRaiseRemote() -- Raise a signal in the specified process (Unix
 *     kill() semantics).
 *
 *     Note: only supports sending signal to a specific (single) process.
 */
int
pmgt_SignalRaiseRemote(pid_t pid, int signo)
{
    BOOL fsuccess;
    char sigPipeName[sizeof(PMGT_SIGNAL_PIPE_PREFIX) + 20];
    DWORD ackBytesRead;
    int signoACK;
    int status = 0;

    /* validate arguments */

    if ((pid <= (pid_t) 0) || (!SignalIsDefined(signo) && signo != 0)) {
	/* invalid pid or signo */
	errno = EINVAL;
	return -1;
    }

    /* optimize for the "this process" case */

    if (pid == (pid_t) GetCurrentProcessId()) {
	return pmgt_SignalRaiseLocal(signo);
    }

    /* send signal to process via named pipe */

    sprintf(sigPipeName, "%s%d", PMGT_SIGNAL_PIPE_PREFIX, (int)pid);

    fsuccess = CallNamedPipe(sigPipeName,	/* process pid's signal pipe */
			     &signo,	/* data written to pipe */
			     sizeof(signo),	/* size of data to write */
			     &signoACK,	/* data read from pipe */
			     sizeof(signoACK),	/* size of data read buffer */
			     &ackBytesRead,	/* number of bytes actually read */
			     5 * 1000);	/* 5 second timeout */

    if (!fsuccess) {
	/* failed to send signal via named pipe */
	status = -1;

	if (signo == SIGKILL) {
	    /* could be a non-AFS process, which might still be kill-able */
	    HANDLE procHandle;

	    if (procHandle =
		OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD) pid)) {
		if (TerminateProcess
		    (procHandle, PMGT_SIGSTATUS_ENCODE(SIGKILL))) {
		    /* successfully killed process */
		    status = 0;
		} else {
		    errno = nterr_nt2unix(GetLastError(), EPERM);
		}
		(void)CloseHandle(procHandle);
	    } else {
		if (GetLastError() == ERROR_INVALID_PARAMETER) {
		    errno = ESRCH;
		} else if (GetLastError() == ERROR_ACCESS_DENIED) {
		    errno = EPERM;
		} else {
		    errno = nterr_nt2unix(GetLastError(), EPERM);
		}
	    }
	} else {
	    /* couldn't open pipe so can't send (non-SIGKILL) signal */
	    errno = nterr_nt2unix(GetLastError(), EPERM);
	}
    }

    return status;
}




/* -----------------  Processes  ---------------- */


/*
 * StringArrayToString() -- convert a null-terminated array of strings,
 *     such as argv, into a single string of space-separated elements
 *     with each element quoted (in case it contains space characters
 *     or is of zero length).
 */
static char *
StringArrayToString(char *strArray[])
{
    int strCount = 0;
    int byteCount = 0;
    char *buffer = NULL;

    for (strCount = 0; strArray[strCount] != NULL; strCount++) {
	/* sum all string lengths */
	byteCount += (int)strlen(strArray[strCount]);
    }

    /* put all strings into buffer; guarantee buffer is at least one char */
    buffer = (char *)malloc(byteCount + (strCount * 3) /* quotes+space */ +1);
    if (buffer != NULL) {
	int i;

	buffer[0] = '\0';

	for (i = 0; i < strCount; i++) {
	    char *bufp = buffer + strlen(buffer);

	    if (i == strCount - 1) {
		/* last string; no trailing space */
		sprintf(bufp, "\"%s\"", strArray[i]);
	    } else {
		sprintf(bufp, "\"%s\" ", strArray[i]);
	    }
	}
    }

    return (buffer);
}


/*
 * StringArrayToMultiString() -- convert a null-terminated array of strings,
 *     such as envp, into a multistring.
 */
static char *
StringArrayToMultiString(char *strArray[])
{
    int strCount = 0;
    int byteCount = 0;
    char *buffer = NULL;

    for (strCount = 0; strArray[strCount] != NULL; strCount++) {
	/* sum all string lengths */
	byteCount += strlen(strArray[strCount]);
    }

    /* put all strings into buffer; guarantee buffer is at least two chars */
    buffer = (char *)malloc(byteCount + strCount + 2);
    if (buffer != NULL) {
	if (byteCount == 0) {
	    buffer[0] = '\0';
	    buffer[1] = '\0';
	} else {
	    int i;
	    char *bufp = buffer;

	    for (i = 0; i < strCount; i++) {
		int strLen = strlen(strArray[i]);

		if (strLen > 0) {
		    /* can not embed zero length string in a multistring */
		    strcpy(bufp, strArray[i]);
		    bufp += strLen + 1;
		}
	    }
	    bufp = '\0';	/* terminate multistring */
	}
    }

    return (buffer);
}



/*
 * ComputeWaitStatus() -- Compute an appropriate wait status value from
 *     a given process termination (exit) code.
 */
static int
ComputeWaitStatus(DWORD exitStatus)
{
    int waitStatus;

    if (PMGT_IS_SIGSTATUS(exitStatus)) {
	/* child terminated due to an unhandled signal */
	int signo = PMGT_SIGSTATUS_DECODE(exitStatus);
	waitStatus = WSIGNALED_ENCODE(signo);
    } else if (PMGT_IS_EXPSTATUS(exitStatus)) {
	/* child terminated due to an uncaught exception */
	int signo;

	switch (exitStatus) {
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_STACK_CHECK:
	case EXCEPTION_FLT_UNDERFLOW:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
	    signo = SIGFPE;
	    break;
	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	    signo = SIGILL;
	    break;
	case CONTROL_C_EXIT:
	    signo = SIGINT;
	    break;
	default:
	    signo = SIGSEGV;
	    break;
	}
	waitStatus = WSIGNALED_ENCODE(signo);
    } else {
	/* child terminated normally */
	waitStatus = WEXITED_ENCODE(exitStatus);
    }

    return waitStatus;
}



/*
 * CreateChildDataBuffer() -- Create and fill a named data buffer to pass to
 *     a child process, along with a corresponding buffer read event.
 *
 * ASSUMPTIONS: child process is linked with this process management library;
 *     otherwise no data transfer will take place.
 */
static BOOL
CreateChildDataBuffer(DWORD pid,	/* child pid */
		      void *datap,	/* data to place in buffer */
		      size_t dataLen,	/* size of data in bytes */
		      HANDLE * bufMemHandlep,	/* buffer memory handle */
		      HANDLE * bufEventHandlep)
{				/* buffer read event handle */
    BOOL fsuccess = FALSE;
    DWORD bufMemSize = dataLen + (DWORD)sizeof(size_t);
    char bufMemName[sizeof(PMGT_DATA_MEM_PREFIX) + 20];
    char bufEventName[sizeof(PMGT_DATA_EVENT_PREFIX) + 20];

    sprintf(bufMemName, "%s%d", PMGT_DATA_MEM_PREFIX, (int)pid);
    sprintf(bufEventName, "%s%d", PMGT_DATA_EVENT_PREFIX, (int)pid);

    /* Create and initialize named shared memory and named event */

    *bufMemHandlep = CreateFileMapping(INVALID_HANDLE_VALUE,	/* page-file backed */
				       NULL, PAGE_READWRITE, 0, bufMemSize,
				       bufMemName);

    if (*bufMemHandlep != NULL) {
	void *bufMemp;

	bufMemp =
	    MapViewOfFile(*bufMemHandlep, FILE_MAP_WRITE, 0, 0, bufMemSize);

	if (bufMemp != NULL) {
	    /* copy data into shared memory, prefixed with data size */
	    size_t *memp = (size_t *) bufMemp;

	    *memp++ = dataLen;
	    memcpy((void *)memp, datap, dataLen);

	    if (UnmapViewOfFile(bufMemp)) {
		/* create buffer read event */
		*bufEventHandlep =
		    CreateEvent(NULL, FALSE /* manual reset */ ,
				FALSE /* initial state */ ,
				bufEventName);
		if (*bufEventHandlep != NULL) {
		    fsuccess = TRUE;
		}
	    }
	}

	if (!fsuccess) {
	    (void)CloseHandle(*bufMemHandlep);
	}
    }

    if (!fsuccess) {
	*bufMemHandlep = *bufEventHandlep = NULL;
    }
    return fsuccess;
}



/*
 * ReadChildDataBuffer() -- Read data buffer passed to child from parent,
 *     if any, and place in allocated storage.
 */
static BOOL
ReadChildDataBuffer(void **datap,	/* allocated data buffer */
		    size_t * dataLen)
{				/* size of data buffer returned */
    BOOL fsuccess = FALSE;
    char bufMemName[sizeof(PMGT_DATA_MEM_PREFIX) + 20];
    char bufEventName[sizeof(PMGT_DATA_EVENT_PREFIX) + 20];
    HANDLE bufMemHandle, bufEventHandle;

    sprintf(bufMemName, "%s%d", PMGT_DATA_MEM_PREFIX,
	    (int)GetCurrentProcessId());
    sprintf(bufEventName, "%s%d", PMGT_DATA_EVENT_PREFIX,
	    (int)GetCurrentProcessId());

    /* Attempt to open named event and named shared memory */

    bufEventHandle = OpenEvent(EVENT_MODIFY_STATE, FALSE, bufEventName);

    if (bufEventHandle != NULL) {
	bufMemHandle = OpenFileMapping(FILE_MAP_READ, FALSE, bufMemName);

	if (bufMemHandle != NULL) {
	    void *bufMemp;

	    bufMemp = MapViewOfFile(bufMemHandle, FILE_MAP_READ, 0, 0, 0);

	    if (bufMemp != NULL) {
		/* read data size and data from shared memory */
		size_t *memp = (size_t *) bufMemp;

		*dataLen = *memp++;
		*datap = (void *)malloc(*dataLen);

		if (*datap != NULL) {
		    memcpy(*datap, (void *)memp, *dataLen);
		    fsuccess = TRUE;
		}
		(void)UnmapViewOfFile(bufMemp);
	    }

	    (void)CloseHandle(bufMemHandle);
	}

	(void)SetEvent(bufEventHandle);
	(void)CloseHandle(bufEventHandle);
    }

    if (!fsuccess) {
	*datap = NULL;
	*dataLen = 0;
    }
    return fsuccess;
}



/*
 * ChildMonitorThread() -- Thread spawned to monitor status of child process.
 *
 *     Param must be index into child process table.
 */
static DWORD WINAPI
ChildMonitorThread(LPVOID param)
{
    int tidx = (int)(intptr_t)param;
    HANDLE childProcHandle;
    BOOL fsuccess;
    DWORD rc = -1;

    /* retrieve handle for child process from process table and duplicate */

    (void)pthread_mutex_lock(&procTableLock);

    fsuccess = DuplicateHandle(GetCurrentProcess(),	/* source process handle */
			       procTable[tidx].p_handle,	/* source handle to dup */
			       GetCurrentProcess(),	/* target process handle */
			       &childProcHandle,	/* target handle (duplicate) */
			       0,	/* access (ignored here) */
			       FALSE,	/* not inheritable */
			       DUPLICATE_SAME_ACCESS);

    (void)pthread_mutex_unlock(&procTableLock);

    if (fsuccess) {
	/* wait for child process to terminate */

	if (WaitForSingleObject(childProcHandle, INFINITE) == WAIT_OBJECT_0) {
	    /* child process terminated; mark in table and signal event */
	    (void)pthread_mutex_lock(&procTableLock);

	    procTable[tidx].p_terminated = TRUE;
	    (void)GetExitCodeProcess(childProcHandle,
				     &procTable[tidx].p_status);
	    procTermCount++;

	    (void)pthread_mutex_unlock(&procTableLock);

	    (void)pthread_cond_broadcast(&childTermEvent);

	    /* process/raise SIGCHLD; do last in case handler never returns */
	    ProcessSignal(SIGCHLD);
	    rc = 0;
	}

	(void)CloseHandle(childProcHandle);
    }

    /* note: nothing can be done if DuplicateHandle() or WaitForSingleObject()
     *       fail; however, this should never happen.
     */
    return rc;
}



/*
 * pmgt_ProcessSpawnVEB() -- Spawn a process (Unix fork()/execve() semantics)
 *
 *     Returns pid of the child process ((pid_t)-1 on failure with errno set).
 *
 *     Notes: A senvp value of NULL results in Unix fork()/execv() semantics.
 *            Open files are not inherited; child's stdin, stdout, and stderr
 *                are set to parent's console.
 *            If spath does not specify a filename extension ".exe" is used.
 *            If sdatap is not NULL, and sdatalen > 0, data is passed to child.
 *            The spath and sargv[] strings must not contain quote chars (").
 *
 * ASSUMPTIONS: sargv[0] is the same as spath (or its last component).
 */
pid_t
pmgt_ProcessSpawnVEB(const char *spath, char *sargv[], char *senvp[],
		     void *sdatap, size_t sdatalen)
{
    int tidx;
    char *pathbuf, *argbuf, *envbuf;
    char pathext[_MAX_EXT];
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    HANDLE monitorHandle = NULL;
    HANDLE bufMemHandle, bufEventHandle;
    DWORD monitorId, createFlags;
    BOOL passingBuffer = (sdatap != NULL && sdatalen > 0);
    BOOL fsuccess;

    /* verify arguments */
    if (!spath || !sargv) {
	errno = EFAULT;
	return (pid_t) - 1;
    } else if (*spath == '\0') {
	errno = ENOENT;
	return (pid_t) - 1;
    }

    /* create path with .exe extension if no filename extension supplied */
    if (!(pathbuf = (char *)malloc(strlen(spath) + 5 /* .exe */ ))) {
	errno = ENOMEM;
	return ((pid_t) - 1);
    }
    strcpy(pathbuf, spath);

    _splitpath(pathbuf, NULL, NULL, NULL, pathext);
    if (*pathext == '\0') {
	/* no filename extension supplied for spath; .exe is assumed */
	strcat(pathbuf, ".exe");
    }

    /* create command line argument string */
    argbuf = StringArrayToString(sargv);

    if (!argbuf) {
	free(pathbuf);
	errno = ENOMEM;
	return ((pid_t) - 1);
    }

    /* create environment variable block (multistring) */
    if (senvp) {
	/* use environment variables provided */
	envbuf = StringArrayToMultiString(senvp);

	if (!envbuf) {
	    free(pathbuf);
	    free(argbuf);
	    errno = ENOMEM;
	    return ((pid_t) - 1);
	}
    } else {
	/* use default environment variables */
	envbuf = NULL;
    }

    /* set process creation flags */
    createFlags = CREATE_SUSPENDED | NORMAL_PRIORITY_CLASS;

    if (getenv(PMGT_SPAWN_DETACHED_ENV_NAME) != NULL) {
	createFlags |= DETACHED_PROCESS;
    }

    /* clear start-up info; use defaults */
    memset((void *)&startInfo, 0, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);

    /* perform the following as a logically atomic unit:
     *     1) allocate a process table entry
     *     2) spawn child process (suspended)
     *     3) create data buffer to pass (optional)
     *     4) initialize process table entry
     *     5) start child watcher thread
     *     6) resume spawned child process
     */

    (void)pthread_mutex_lock(&procTableLock);

    for (tidx = 0; tidx < PMGT_CHILD_MAX; tidx++) {
	if (procTable[tidx].p_handle == NULL
	    && procTable[tidx].p_reserved == FALSE) {
	    procTable[tidx].p_reserved = TRUE;
	    break;
	}
    }
    (void)pthread_mutex_unlock(&procTableLock);

    if (tidx >= PMGT_CHILD_MAX) {
	/* no space left in process table */
	free(pathbuf);
	free(argbuf);
	free(envbuf);

	errno = EAGAIN;
	return (pid_t) - 1;
    }

    fsuccess = CreateProcess(pathbuf,	/* executable path */
			     argbuf,	/* command line argument string */
			     NULL,	/* default process security attr */
			     NULL,	/* default thread security attr */
			     FALSE,	/* do NOT inherit handles */
			     createFlags,	/* creation control flags */
			     envbuf,	/* environment variable block */
			     NULL,	/* current directory is that of parent */
			     &startInfo,	/* startup info block */
			     &procInfo);

    free(pathbuf);
    free(argbuf);
    free(envbuf);

    if (!fsuccess) {
	/* failed to spawn process */
	errno = nterr_nt2unix(GetLastError(), ENOENT);

	(void)pthread_mutex_lock(&procTableLock);
	procTable[tidx].p_reserved = FALSE;	/* mark entry as not reserved */
	(void)pthread_mutex_unlock(&procTableLock);

	return (pid_t) - 1;
    }

    if (passingBuffer) {
	/* create named data buffer and read event for child */
	fsuccess =
	    CreateChildDataBuffer(procInfo.dwProcessId, sdatap, sdatalen,
				  &bufMemHandle, &bufEventHandle);
	if (!fsuccess) {
	    (void)pthread_mutex_lock(&procTableLock);
	    procTable[tidx].p_reserved = FALSE;	/* mark entry not reserved */
	    (void)pthread_mutex_unlock(&procTableLock);

	    (void)TerminateProcess(procInfo.hProcess,
				   PMGT_SIGSTATUS_ENCODE(SIGKILL));
	    (void)CloseHandle(procInfo.hThread);
	    (void)CloseHandle(procInfo.hProcess);

	    errno = EAGAIN;
	    return (pid_t) - 1;
	}
    }

    (void)pthread_mutex_lock(&procTableLock);

    procTable[tidx].p_handle = procInfo.hProcess;
    procTable[tidx].p_id = procInfo.dwProcessId;
    procTable[tidx].p_terminated = FALSE;

    procEntryCount++;

    /* Note: must hold procTableLock during monitor thread creation so
     * that if creation fails we can clean up process table before another
     * thread has a chance to see this procTable entry.  Continue to hold
     * procTableLock while resuming child process, since the procTable
     * entry contains a copy of the child process handle which we might use.
     */
    monitorHandle = CreateThread(NULL,	/* default security attr. */
				 0,	/* default stack size */
				 ChildMonitorThread, (LPVOID)(intptr_t) tidx,	/* thread argument */
				 0,	/* creation flags */
				 &monitorId);	/* thread id */

    if (monitorHandle == NULL) {
	/* failed to start child monitor thread */
	procTable[tidx].p_handle = NULL;	/* invalidate table entry */
	procTable[tidx].p_reserved = FALSE;	/* mark entry as not reserved */
	procEntryCount--;

	(void)pthread_mutex_unlock(&procTableLock);

	(void)TerminateProcess(procInfo.hProcess,
			       PMGT_SIGSTATUS_ENCODE(SIGKILL));
	(void)CloseHandle(procInfo.hThread);
	(void)CloseHandle(procInfo.hProcess);

	if (passingBuffer) {
	    (void)CloseHandle(bufMemHandle);
	    (void)CloseHandle(bufEventHandle);
	}

	errno = EAGAIN;
	return (pid_t) - 1;
    }

    /* Resume child process, which was created suspended to implement spawn
     * atomically.  If resumption fails, which it never should, terminate
     * the child process with a status of SIGKILL.  Spawn still succeeds and
     * the net result is the same as if the child process received a spurious
     * SIGKILL signal; the child monitor thread will then handle this.
     */
    if (ResumeThread(procInfo.hThread) == DWORD_OF_ONES) {
	(void)TerminateProcess(procInfo.hProcess,
			       PMGT_SIGSTATUS_ENCODE(SIGKILL));

	if (passingBuffer) {
	    /* child will never read data buffer */
	    (void)SetEvent(bufEventHandle);
	}
    }

    (void)pthread_mutex_unlock(&procTableLock);

    (void)CloseHandle(procInfo.hThread);
    (void)CloseHandle(monitorHandle);

    /* After spawn returns, signals can not be sent to the new child process
     * until that child initializes its signal-receiving mechanism (assuming
     * the child is linked with this library).  Shorten (but sadly don't
     * eliminate) this window of opportunity for failure by yielding this
     * thread's time slice.
     */
    (void)SwitchToThread();

    /* If passing a data buffer to child, wait until child reads buffer
     * before closing handles and thus freeing resources; if don't wait
     * then parent can not safely exit immediately after returning from
     * this call (hence why wait is not done in a background thread).
     */
    if (passingBuffer) {
	WaitForSingleObject(bufEventHandle, 10000);
	/* note: if wait times out, child may not get to read buffer */
	(void)CloseHandle(bufMemHandle);
	(void)CloseHandle(bufEventHandle);
    }

    return (pid_t) procInfo.dwProcessId;
}



/*
 * pmgt_ProcessWaitPid() -- Wait for child process status; i.e., wait
 *     for child to terminate (Unix waitpid() semantics).
 *
 *     Note: does not support waiting for process in group (i.e., pid
 *           equals (pid_t)0 or pid is less than (pid_t)-1.
 */
pid_t
pmgt_ProcessWaitPid(pid_t pid, int *statusP, int options)
{
    pid_t rc;
    int tidx;
    BOOL statusFound = FALSE;
    DWORD waitTime;

    /* validate arguments */
    if (pid < (pid_t) - 1 || pid == (pid_t) 0) {
	errno = EINVAL;
	return (pid_t) - 1;
    }

    /* determine how long caller is willing to wait for child */

    waitTime = (options & WNOHANG) ? 0 : INFINITE;

    /* get child status */

    (void)pthread_mutex_lock(&procTableLock);

    while (1) {
	BOOL waitForChild = FALSE;

	if (procEntryCount == 0) {
	    /* no child processes */
	    errno = ECHILD;
	    rc = (pid_t) - 1;
	} else {
	    /* determine if status is available for specified child id */

	    if (pid == (pid_t) - 1) {
		/* CASE 1: pid matches any child id */

		if (procTermCount == 0) {
		    /* status not available for any child ... */
		    if (waitTime == 0) {
			/* ... and caller is not willing to wait */
			rc = (pid_t) 0;
		    } else {
			/* ... but caller is willing to wait */
			waitForChild = TRUE;
		    }
		} else {
		    /* status available for some child; locate table entry */
		    for (tidx = 0; tidx < PMGT_CHILD_MAX; tidx++) {
			if (procTable[tidx].p_handle != NULL
			    && procTable[tidx].p_terminated == TRUE) {
			    statusFound = TRUE;
			    break;
			}
		    }

		    if (!statusFound) {
			/* should never happen; indicates a bug */
			errno = EINTR;	/* plausible lie for failure */
			rc = (pid_t) - 1;
		    }
		}

	    } else {
		/* CASE 2: pid must match a specific child id */

		/* locate table entry */
		for (tidx = 0; tidx < PMGT_CHILD_MAX; tidx++) {
		    if (procTable[tidx].p_handle != NULL
			&& procTable[tidx].p_id == (DWORD) pid) {
			break;
		    }
		}

		if (tidx >= PMGT_CHILD_MAX) {
		    /* pid does not match any child id */
		    errno = ECHILD;
		    rc = (pid_t) - 1;
		} else if (procTable[tidx].p_terminated == FALSE) {
		    /* status not available for specified child ... */
		    if (waitTime == 0) {
			/* ... and caller is not willing to wait */
			rc = (pid_t) 0;
		    } else {
			/* ... but caller is willing to wait */
			waitForChild = TRUE;
		    }
		} else {
		    /* status is available for specified child */
		    statusFound = TRUE;
		}
	    }
	}

	if (waitForChild) {
	    (void)pthread_cond_wait(&childTermEvent, &procTableLock);
	} else {
	    break;
	}
    }				/* while() */

    if (statusFound) {
	/* child status available */
	if (statusP) {
	    *statusP = ComputeWaitStatus(procTable[tidx].p_status);
	}
	rc = (pid_t) procTable[tidx].p_id;

	/* clean up process table */
	(void)CloseHandle(procTable[tidx].p_handle);
	procTable[tidx].p_handle = NULL;
	procTable[tidx].p_reserved = FALSE;

	procEntryCount--;
	procTermCount--;
    }

    (void)pthread_mutex_unlock(&procTableLock);
    return rc;
}






/* -----------------  General  ---------------- */



/*
 * PmgtLibraryInitialize() -- Initialize process management library.
 */
static int
PmgtLibraryInitialize(void)
{
    int rc, i;
    HANDLE sigPipeHandle;
    char sigPipeName[sizeof(PMGT_SIGNAL_PIPE_PREFIX) + 20];
    HANDLE sigListenerHandle;
    DWORD sigListenerId;

    /* initialize mutex locks and condition variables */

    if ((rc = pthread_mutex_init(&signalTableLock, NULL))
	|| (rc = pthread_mutex_init(&signalBlockLock, NULL))
	|| (rc = pthread_mutex_init(&procTableLock, NULL))
	|| (rc = pthread_cond_init(&childTermEvent, NULL))) {
	errno = rc;
	return -1;
    }

    /* initialize signal disposition table */

    for (i = 0; i < NSIG; i++) {
	if (SignalIsDefined(i)) {
	    /* initialize to default action for defined signals */
	    signalTable[i].action.sa_handler = SIG_DFL;
	    sigemptyset(&signalTable[i].action.sa_mask);
	    signalTable[i].action.sa_flags = 0;
	} else {
	    /* initialize to ignore for undefined signals */
	    signalTable[i].action.sa_handler = SIG_IGN;
	}
    }

    /* initialize child process table */

    for (i = 0; i < PMGT_CHILD_MAX; i++) {
	procTable[i].p_handle = NULL;
	procTable[i].p_reserved = FALSE;
    }
    procEntryCount = 0;
    procTermCount = 0;

    /* retrieve data buffer passed from parent in spawn, if any */

    if (!ReadChildDataBuffer(&pmgt_spawnData, &pmgt_spawnDataLen)) {
	pmgt_spawnData = NULL;
	pmgt_spawnDataLen = 0;
    }

    /* create named pipe for delivering signals to this process */

    sprintf(sigPipeName, "%s%d", PMGT_SIGNAL_PIPE_PREFIX,
	    (int)GetCurrentProcessId());

    sigPipeHandle = CreateNamedPipe(sigPipeName,	/* pipe for this process */
				    PIPE_ACCESS_DUPLEX |	/* full duplex pipe */
				    WRITE_DAC,	/* DACL write access */
				    PIPE_TYPE_MESSAGE |	/* message type pipe */
				    PIPE_READMODE_MESSAGE |	/* message read-mode */
				    PIPE_WAIT,	/* blocking mode */
				    1,	/* max of 1 pipe instance */
				    64,	/* output buffer size (advisory) */
				    64,	/* input buffer size (advisory) */
				    1000,	/* 1 sec default client timeout */
				    NULL);	/* default security attr. */

    if (sigPipeHandle == INVALID_HANDLE_VALUE) {
	/* failed to create signal pipe */
	errno = nterr_nt2unix(GetLastError(), EIO);
	return -1;
    }

    /* add entry to signal pipe ACL granting local Administrators R/W access */

    (void)ObjectDaclEntryAdd(sigPipeHandle, SE_KERNEL_OBJECT,
			     LocalAdministratorsGroup,
			     GENERIC_READ | GENERIC_WRITE, GRANT_ACCESS,
			     NO_INHERITANCE);

    /* start signal pipe listener thread */

    sigListenerHandle = CreateThread(NULL,	/* default security attr. */
				     0,	/* default stack size */
				     RemoteSignalListenerThread, (LPVOID) sigPipeHandle,	/* thread argument */
				     0,	/* creation flags */
				     &sigListenerId);	/* thread id */

    if (sigListenerHandle != NULL) {
	/* listener thread started; bump priority */
	(void)SetThreadPriority(sigListenerHandle, THREAD_PRIORITY_HIGHEST);
	(void)CloseHandle(sigListenerHandle);
    } else {
	/* failed to start listener thread */
	errno = EAGAIN;
	(void)CloseHandle(sigPipeHandle);
	return -1;
    }

    /* redirect native NT signals into this process management library */

    if (pmgt_RedirectNativeSignals()) {
	/* errno set by called function */
	return -1;
    }

    return 0;
}


/*
 * DllMain() -- Entry-point function called by the DllMainCRTStartup()
 *     function in the MSVC runtime DLL (msvcrt.dll).
 *
 *     Note: the system serializes calls to this function.
 */
BOOL WINAPI
DllMain(HINSTANCE dllInstHandle,	/* instance handle for this DLL module */
	DWORD reason,		/* reason function is being called */
	LPVOID reserved)
{				/* reserved for future use */
    if (reason == DLL_PROCESS_ATTACH) {
	/* library is being attached to a process */
	if (PmgtLibraryInitialize()) {
	    /* failed to initialize library */
	    return FALSE;
	}

	/* disable thread attach/detach notifications */
	(void)DisableThreadLibraryCalls(dllInstHandle);
    }

    return TRUE;
}
