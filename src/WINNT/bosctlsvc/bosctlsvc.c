/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file implements the AFS BOS control service.  Basically, it provides
 * a mechanism to start and stop the AFS bosserver via the NT SCM; it also
 * supports bosserver restart.
 */


#include <afs/param.h>
#include <afs/stds.h>

#include <param.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <time.h>
#include <process.h>

#include <WINNT/afsevent.h>
#include <WINNT/afsreg.h>
#include <afs/procmgmt.h>
#include <afs/dirpath.h>
#include <afs/bnode.h>
#include <afs/afsicf.h>

/* Define globals */

#define BOSSERVER_STARTMSG_EXE  "afslegal.exe"

#define BOSSERVER_RESTART_ARG_MAX  2  /* "-noauth", "-log" */
#define BOSSERVER_WAIT_TIME_HINT  60  /* seconds */
#define BOSSERVER_STOP_TIME_MAX  (FSSDTIME + 60)  /* seconds */

#define BOS_CONTROLS_ACCEPTED  SERVICE_ACCEPT_STOP

static CRITICAL_SECTION bosCtlStatusLock;  /* protects bosCtlStatus */
static SERVICE_STATUS bosCtlStatus;
static SERVICE_STATUS_HANDLE bosCtlStatusHandle;

/* note: events arranged in priority order in case multiple signaled */
#define BOS_STOP_EVENT 0
#define BOS_EXIT_EVENT 1
#define BOS_EVENT_COUNT 2
static HANDLE bosCtlEvent[BOS_EVENT_COUNT];


/* Declare local functions */

static void AsyncSignalCatcher(int signo);

static void BosCtlStatusInit(DWORD initState);

static DWORD BosCtlStatusUpdate(DWORD newState,
				DWORD exitCode,
				BOOL isWin32Code);

static DWORD BosCtlStatusReport(void);

static void WINAPI BosCtlHandler(DWORD controlCode);

static void WINAPI BosCtlMain(DWORD argc,
			      LPTSTR *argv);

static void BosserverDoStopEvent(pid_t cpid,
				 DWORD *stopStatus,
				 BOOL *isWin32Code);

static void BosserverDoExitEvent(pid_t cpid,
				 BOOL *doWait,
				 BOOL *doRestart,
				 char **restartArgv,
				 DWORD *stopStatus,
				 BOOL *isWin32Code);

static void BosserverRun(DWORD argc,
			 LPTSTR *argv,
			 DWORD *stopStatus,
			 BOOL *isWin32Code);

static void BosserverStartupMsgDisplay(void);




/*
 * AsyncSignalCatcher() -- Handle asynchronous signals sent to process
 */
static void
AsyncSignalCatcher(int signo)
{
    if (signo == SIGCHLD) {
	(void) SetEvent(bosCtlEvent[BOS_EXIT_EVENT]);
    }
}


/*
 * BosCtlStatusInit() -- initialize BOS control service status structure
 */
static void
BosCtlStatusInit(DWORD initState)
{
    bosCtlStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    bosCtlStatus.dwCurrentState = initState;

    if (initState == SERVICE_RUNNING) {
	bosCtlStatus.dwControlsAccepted = BOS_CONTROLS_ACCEPTED;
    } else {
	bosCtlStatus.dwControlsAccepted = 0;
    }

    bosCtlStatus.dwWin32ExitCode = 0;
    bosCtlStatus.dwServiceSpecificExitCode = 0;
    bosCtlStatus.dwCheckPoint = 0;
    bosCtlStatus.dwWaitHint = BOSSERVER_WAIT_TIME_HINT * 1000; /* millisecs */

    InitializeCriticalSection(&bosCtlStatusLock);
}


/*
 * BosCtlStatusUpdate() -- update BOS control service status and report to SCM
 */
static DWORD
BosCtlStatusUpdate(DWORD newState, DWORD exitCode, BOOL isWin32Code)
{
    DWORD rc = 0;

    EnterCriticalSection(&bosCtlStatusLock);

    /* SERVICE_STOPPED is a terminal state; never transition out of it */
    if (bosCtlStatus.dwCurrentState != SERVICE_STOPPED) {

	if ((bosCtlStatus.dwCurrentState == newState) &&
	    (newState == SERVICE_START_PENDING ||
	     newState == SERVICE_STOP_PENDING)) {
	    /* continuing a pending state; increment checkpoint value */
	    bosCtlStatus.dwCheckPoint++;
	} else {
	    /* not continuing a pending state; reset checkpoint value */
	    bosCtlStatus.dwCheckPoint = 0;
	}

	bosCtlStatus.dwCurrentState = newState;

	if (newState == SERVICE_RUNNING) {
	    bosCtlStatus.dwControlsAccepted = BOS_CONTROLS_ACCEPTED;
	} else {
	    bosCtlStatus.dwControlsAccepted = 0;
	}

	if (isWin32Code) {
	    bosCtlStatus.dwWin32ExitCode = exitCode;
	    bosCtlStatus.dwServiceSpecificExitCode = 0;
	} else {
	    bosCtlStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
	    bosCtlStatus.dwServiceSpecificExitCode = exitCode;
	}
    }

    if (!SetServiceStatus(bosCtlStatusHandle, &bosCtlStatus)) {
	rc = GetLastError();
    }

    LeaveCriticalSection(&bosCtlStatusLock);

    return rc;
}


/*
 * BosCtlStatusReport() -- report current BOS control service status to SCM
 */
static DWORD
BosCtlStatusReport(void)
{
    DWORD rc = 0;

    EnterCriticalSection(&bosCtlStatusLock);

    if (!SetServiceStatus(bosCtlStatusHandle, &bosCtlStatus)) {
	rc = GetLastError();
    }

    LeaveCriticalSection(&bosCtlStatusLock);

    return rc;
}


/*
 * BosCtlHandler() -- control handler for BOS control service
 */
static void WINAPI
BosCtlHandler(DWORD controlCode)
{
    switch (controlCode) {
      case SERVICE_CONTROL_STOP:
	(void) SetEvent(bosCtlEvent[BOS_STOP_EVENT]);
	(void) BosCtlStatusUpdate(SERVICE_STOP_PENDING, 0, TRUE);
	break;

      default:
	(void) BosCtlStatusReport();
	break;
    }
}


/*
 * BosCtlMain() -- main function for BOS control service
 */
static void WINAPI
BosCtlMain(DWORD argc, LPTSTR *argv)
{
    DWORD status;
    BOOL isWin32Code;
    struct sigaction childAction;

    /* Initialize status structure */
    BosCtlStatusInit(SERVICE_START_PENDING);

    /* Create events used by service control handler and signal handler */
    if ((bosCtlEvent[BOS_STOP_EVENT] = CreateEvent(NULL,
						   FALSE /* manual reset */,
						   FALSE /* initial state */,
						   TEXT("BosCtlSvc Stop Event"))) == NULL) {
	status = GetLastError();
    }

    if ((bosCtlEvent[BOS_EXIT_EVENT] = CreateEvent(NULL,
						   FALSE /* manual reset */,
						   FALSE /* initial state */,
						   TEXT("BosCtlSvc Exit Event"))) == NULL) {
	status = GetLastError();
    }

    /* Register service control handler */
    bosCtlStatusHandle = RegisterServiceCtrlHandler(AFSREG_SVR_SVC_NAME,
						    BosCtlHandler);
    if (bosCtlStatusHandle == 0) {
	/* failed to register control handler for service; can not continue */
	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_HANDLER_REG_FAILED,
				   (int)GetLastError(), NULL);
	/* can not report status to SCM w/o a valid bosCtlStatusHandle */
	return;
    }

    /* Stop immediately if required system resources could not be obtained */
    if (bosCtlEvent[BOS_STOP_EVENT] == NULL ||
	bosCtlEvent[BOS_EXIT_EVENT] == NULL) {
	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES,
				   (int)status, NULL);
	(void) BosCtlStatusUpdate(SERVICE_STOPPED, status, TRUE);
	return;
    }

    /* Report pending start status */
    if (status = BosCtlStatusUpdate(SERVICE_START_PENDING, 0, TRUE)) {
	/* can't inform SCM of pending start; give up before really start */
	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_SCM_COMM_FAILED,
				   (int)status, NULL);
	(void) BosCtlStatusUpdate(SERVICE_STOPPED, status, TRUE);
	return;
    }

    /* For XP SP2 and above, open required ports */
    icf_CheckAndAddAFSPorts(AFS_PORTSET_SERVER);

    /* Initialize the dirpath package so can access local bosserver binary */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
	/* sw install directory probably not in registry; can not continue */
	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_NO_INSTALL_DIR, 0, NULL);
	(void) BosCtlStatusUpdate(SERVICE_STOPPED, 0, TRUE);
	return;
    }

    /* Install SIGCHLD handler to catch bosserver restarts and failures */
    childAction.sa_handler = AsyncSignalCatcher;
    sigfillset(&childAction.sa_mask);
    childAction.sa_flags = 0;

    if (sigaction(SIGCHLD, &childAction, NULL)) {
	/* handler install should never fail, but can't continue if it does */
	status = errno;
	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_INTERNAL_ERROR,
				   (int)status, NULL);
	(void) BosCtlStatusUpdate(SERVICE_STOPPED, status, FALSE);
	return;
    }

    /* Run the AFS bosserver, handling stop and exit events */
    BosserverRun(argc, argv, &status, &isWin32Code);

    (void) BosCtlStatusUpdate(SERVICE_STOPPED, status, isWin32Code);
}


/*
 * BosserverDoStopEvent() -- Handle a stop event for the AFS bosserver.
 */
static void
BosserverDoStopEvent(pid_t cpid, DWORD *stopStatus, BOOL *isWin32Code)
{
    (void) BosCtlStatusUpdate(SERVICE_STOP_PENDING, 0, TRUE);

    if (kill(cpid, SIGQUIT) == 0) {
	/* bosserver has been told to stop; wait for this to happen */
	BOOL gotWaitStatus = FALSE;
	time_t timeStart = time(NULL);

	do {
	    int waitStatus;
	    DWORD status;

	    if (waitpid(cpid, &waitStatus, WNOHANG) == cpid) {
		/* bosserver status available */
		if (WIFEXITED(waitStatus) && WEXITSTATUS(waitStatus) == 0) {
		    /* bosserver exited w/o any error */
		    *stopStatus = 0;
		    *isWin32Code = TRUE;
		} else {
		    *stopStatus = waitStatus;
		    *isWin32Code = FALSE;
		}
		gotWaitStatus = TRUE;
		break;
	    }

	    /* wait for bosserver status to become available;
	     * update BOS control service status periodically.
	     */
	    status = WaitForSingleObject(bosCtlEvent[BOS_EXIT_EVENT],
					 BOSSERVER_WAIT_TIME_HINT * 1000 / 2);
	    if (status == WAIT_FAILED) {
		/* failed to wait on event; should never happen */
		Sleep(2000);  /* sleep to avoid tight loop if event problem */
	    }
	    (void) BosCtlStatusUpdate(SERVICE_STOP_PENDING, 0, TRUE);
	} while (difftime(time(NULL), timeStart) < BOSSERVER_STOP_TIME_MAX);

	if (!gotWaitStatus) {
	    /* timed out waiting to get bosserver status */
	    *stopStatus = EBUSY;
	    *isWin32Code = FALSE;

	    (void) ReportWarningEventAlt(AFSEVT_SVR_BCS_BOSSERVER_STOP_TIMEOUT,
					 (int)*stopStatus, NULL);
	}

    } else {
	/* can't tell bosserver to stop; should never happen */
	*stopStatus = errno;
	*isWin32Code = FALSE;

	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_BOSSERVER_STOP_FAILED,
				   (int)*stopStatus, NULL);
    }
}


/*
 * BosserverDoExitEvent() -- Handle an exit event for the AFS bosserver.
 *
 *     The output arguments for this function are set as follows:
 *         Case 1: bosserver did not exit (spurious SIGCHLD);
 *                 *doWait is set to TRUE.
 *         Case 2: bosserver exited with restart code;
 *                 *doRestart is set to TRUE, restartArgv[] is defined.
 *         Case 3: bosserver exited with non-restart code;
 *                 *stopStatus and *isWin32Code are defined.
 */
static void
BosserverDoExitEvent(pid_t cpid,
		     BOOL *doWait,
		     BOOL *doRestart,
		     char **restartArgv,
		     DWORD *stopStatus,
		     BOOL *isWin32Code)
{
    int waitStatus;

    *doWait = FALSE;
    *doRestart = FALSE;

    if (waitpid(cpid, &waitStatus, WNOHANG) == cpid) {
	/* bosserver status available */

	if (WIFEXITED(waitStatus)) {
	    /* bosserver exited normally; check for restart code */
	    int exitCode = WEXITSTATUS(waitStatus);

	    if (BOSEXIT_DORESTART(exitCode)) {
		/* bosserver requests restart */
		int i;
		*doRestart = TRUE;

		/* set up bosserver argument list */
		restartArgv[0] = (char *)AFSDIR_SERVER_BOSVR_FILEPATH;
		i = 1;

		if (exitCode & BOSEXIT_NOAUTH_FLAG) {
		    /* pass "-noauth" to new bosserver */
		    restartArgv[i] = "-noauth";
		    i++;
		}
		if (exitCode & BOSEXIT_LOGGING_FLAG) {
		    /* pass "-log" to new bosserver */
		    restartArgv[i] = "-log";
		    i++;
		}
		restartArgv[i] = NULL;
	    }
	}

	if (!(*doRestart)) {
	    /* bosserver exited with non-restart code; set status */
	    *stopStatus = waitStatus;
	    *isWin32Code = FALSE;

	    (void) ReportWarningEventAlt(AFSEVT_SVR_BCS_BOSSERVER_EXIT,
					 (int)*stopStatus, NULL);
	}

    } else {
	/* bosserver status NOT available; assume spurious SIGCHLD */
	*doWait = TRUE;
    }
}


/*
 * BosserverRun() -- Run the AFS bosserver, handling stop and exit events.
 *
 *     Input args are those passed to the service's main function (BosCtlMain).
 *     Output args are the stop status and status type of the bosserver.
 */
static void
BosserverRun(DWORD argc,
	     LPTSTR *argv,
	     DWORD *stopStatus,
	     BOOL *isWin32Code)
{
    DWORD status, i;
    BOOL doRestart, doWait;
    char **spawn_argv;

    /* Display bosserver startup (legal) message; first start only */
    /* BosserverStartupMsgDisplay(); */

    /* Set env variable forcing process mgmt lib to spawn processes detached */
    (void)putenv(PMGT_SPAWN_DETACHED_ENV_NAME "=1");

    /* Alloc block with room for at least BOSSERVER_RESTART_ARG_MAX args */
    i = max((argc + 1), (BOSSERVER_RESTART_ARG_MAX + 2));
    spawn_argv = (char **)malloc(i * sizeof(char *));

    if (spawn_argv == NULL) {
	/* failed to malloc required space; can not continue */
	*stopStatus = ENOMEM;
	*isWin32Code = FALSE;

	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_INSUFFICIENT_RESOURCES,
				   (int)*stopStatus, NULL);
	return;
    }

    /* Set initial bosserver args to those supplied via StartService() */
    spawn_argv[0] = (char *)AFSDIR_SERVER_BOSVR_FILEPATH;

    for (i = 1; i < argc; i++) {
	spawn_argv[i] = argv[i];
    }
    spawn_argv[i] = NULL;

    /* Start/restart bosserver and wait for either a stop or exit event */
    doRestart = FALSE;

    do {
	pid_t cpid;

	if (doRestart) {
	    /* restarting bosserver; log informational message */
	    (void) ReportInformationEventAlt(AFSEVT_SVR_BCS_BOSSERVER_RESTART,
					     NULL);
	    doRestart = FALSE;
	}

	cpid = spawnprocve(spawn_argv[0], spawn_argv, NULL, 0);

	if (cpid == (pid_t)-1) {
	    /* failed to start/restart the bosserver process */
	    *stopStatus = errno;
	    *isWin32Code = FALSE;

	    (void) ReportErrorEventAlt(AFSEVT_SVR_BCS_BOSSERVER_START_FAILED,
				       (int)*stopStatus, NULL);
	    break;
	}

	if (status = BosCtlStatusUpdate(SERVICE_RUNNING, 0, TRUE)) {
	    /* can't tell SCM we're running so quit; should never occur */
	    (void) ReportErrorEventAlt(AFSEVT_SVR_BCS_SCM_COMM_FAILED,
				       (int)status, NULL);
	    (void) SetEvent(bosCtlEvent[BOS_STOP_EVENT]);
	}

	/* bosserver is running; wait for an event of interest */

	Sleep(5000);  /* bosserver needs time to register signal handler */

	do {
	    doWait = FALSE;

	    status = WaitForMultipleObjects(BOS_EVENT_COUNT,
					    bosCtlEvent, FALSE, INFINITE);

	    if ((status - WAIT_OBJECT_0) == BOS_STOP_EVENT) {
		/* stop event signaled */
		BosserverDoStopEvent(cpid, stopStatus, isWin32Code);

	    } else if ((status - WAIT_OBJECT_0) == BOS_EXIT_EVENT) {
		/* exit event signaled; see function comment for outcomes */
		BosserverDoExitEvent(cpid,
				     &doWait,
				     &doRestart, spawn_argv,
				     stopStatus, isWin32Code);

	    } else {
		/* failed to wait on events; should never happen */
		Sleep(2000);  /* sleep to avoid tight loop if event problem */
		doWait = TRUE;
	    }
	} while (doWait);
    } while(doRestart);

    return;
}


/*
 * BosserverStartupMsgDisplay() -- display Windows version of AFS bosserver
 *     startup (legal) message.
 */
static void
BosserverStartupMsgDisplay(void)
{
    char *msgPath;

    if (!ConstructLocalBinPath(BOSSERVER_STARTMSG_EXE, &msgPath)) {
	/* Use C runtime spawn; don't need all the machinery in the
	 * process management library.
	 */
	(void)_spawnl(_P_DETACH, msgPath, BOSSERVER_STARTMSG_EXE, NULL);
	free(msgPath);
    }
}


/*
 * main() -- start dispatcher thread for BOS control service
 */
int main(void)
{
    SERVICE_TABLE_ENTRY dispatchTable[] = {{AFSREG_SVR_SVC_NAME, BosCtlMain},
					   {NULL, NULL}};

    (void) ReportInformationEventAlt(AFSEVT_SVR_BCS_STARTED, NULL);

    if (!StartServiceCtrlDispatcher(dispatchTable)) {
	/* unable to connect to SCM */
	(void) ReportErrorEventAlt(AFSEVT_SVR_BCS_SCM_COMM_FAILED,
				   (int)GetLastError(), NULL);
    }

    (void) ReportInformationEventAlt(AFSEVT_SVR_BCS_STOPPED, NULL);
    return 0;
}
