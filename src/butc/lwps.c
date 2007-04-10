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

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <conio.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <afs/procmgmt.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <time.h>
#include <lwp.h>
#include <lock.h>
#include <afs/tcdata.h>
#include <afs/bubasics.h>	/* PA */
#include <afs/budb_client.h>
#include <afs/volser.h>
#include <afs/com_err.h>
#include "error_macros.h"
#include <afs/afsutil.h>
#include <errno.h>
#include "butc_xbsa.h"

/* GLOBAL CONFIGURATION PARAMETERS */
extern int queryoperator;
extern int tapemounted;
extern char *opencallout;
extern char *closecallout;
extern char *whoami;
extern char *extractDumpName();
extern int BufferSize;		/* Size in B stored for header info */
FILE *restoretofilefd;
#ifdef xbsa
extern char *restoretofile;
extern int forcemultiple;
#endif

/* XBSA Global Parameters */
afs_int32 xbsaType;
#ifdef xbsa
struct butx_transactionInfo butxInfo;
#endif

static struct TapeBlock {		/* A 16KB tapeblock */
    char mark[BUTM_HDRSIZE];	/* Header info */
    char data[BUTM_BLKSIZE];	/* data */
} *bufferBlock;

afs_int32 dataSize;		/* Size of data to read on each xbsa_ReadObjectData() call (CONF_XBSA) */
afs_int32 tapeblocks;		/* Number of tape datablocks in buffer (!CONF_XBSA) */

/* notes
 *	Need to re-write to:
 *	1) non-interactive tape handling (optional)
 *	2) compute tape and volume sizes for the database
 *	3) compute and use tape id's for tape tracking (put on tape label)
 *	4) status management
 */

/* All the relevant info shared between Restorer and restoreVolume */
struct restoreParams {
    struct dumpNode *nodePtr;
    afs_int32 frag;
    char mntTapeName[BU_MAXTAPELEN];
    afs_int32 tapeID;
    struct butm_tapeInfo *tapeInfoPtr;
};

/* Abort checks are done after each  BIGCHUNK of data transfer */
#define	BIGCHUNK 102400

#define HEADER_CHECKS(vhptr, header)					\
{									\
    afs_int32 magic, versionflags;						\
	                                                                \
    versionflags = ntohl(vhptr.versionflags);				\
    if ( versionflags == TAPE_VERSION_0 ||				\
	     versionflags == TAPE_VERSION_1 ||				\
	     versionflags == TAPE_VERSION_2 ||				\
       	     versionflags == TAPE_VERSION_3 || 		                \
	     versionflags == TAPE_VERSION_4 )       {			\
									\
	    magic = ntohl(vhptr.magic);	/* another sanity check */	\
	    if (magic == TC_VOLBEGINMAGIC ||				\
		magic == TC_VOLENDMAGIC ||				\
		magic == TC_VOLCONTD )                 {		\
									\
		memcpy(header, &vhptr, sizeof(struct volumeHeader));	\
		return (0);						\
	    } /* magic */						\
	} /* versionflags */				 		\
}

extern FILE *logIO;
extern FILE *ErrorlogIO;
extern FILE *centralLogIO;
extern FILE *lastLogIO;
extern afs_int32 lastPass;	/* Set true during last pass of dump */
extern int debugLevel;
extern int autoQuery;
extern struct tapeConfig globalTapeConfig;
extern struct deviceSyncNode *deviceLatch;
extern char globalCellName[];
struct timeval tp;
struct timezone tzp;

/* forward declaration */
afs_int32 readVolumeHeader( /*char *buffer,afs_int32 bufloc,(struct volumeHeader *)vhptr */ );

/* The on-disk volume header or trailer can differ in size from platform to platform */
static struct TapeBlock tapeBlock;
char tapeVolumeHT[sizeof(struct volumeHeader) + 2 * sizeof(char)];

void
PrintLog(log, error1, error2, str, a, b, c, d, e, f, g, h, i, j)
     FILE *log;
     afs_int32 error1, error2;
     char *str, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    char *err1, *err2;

    fprintf(log, str, a, b, c, d, e, f, g, h, i, j);
    if (error1) {
	err2 = "vols";
	switch (error1) {
	case VSALVAGE:
	    err1 = "Volume needs to be salvaged";
	    break;
	case VNOVNODE:
	    err1 = "Bad vnode number quoted";
	    break;
	case VNOVOL:
	    err1 = "Volume not attached, does not exist, or not on line";
	    break;
	case VVOLEXISTS:
	    err1 = "Volume already exists";
	    break;
	case VNOSERVICE:
	    err1 = "Volume is not in service";
	    break;
	case VOFFLINE:
	    err1 = "Volume is off line";
	    break;
	case VONLINE:
	    err1 = "Volume is already on line";
	    break;
	case VDISKFULL:
	    err1 = "Partition is full";
	    break;
	case VOVERQUOTA:
	    err1 = "Volume max quota exceeded";
	    break;
	case VBUSY:
	    err1 = "Volume temporarily unavailable";
	    break;
	case VMOVED:
	    err1 = "Volume has moved to another server";
	    break;
	default:
	    err1 = (char *)afs_error_message(error1);
	    err2 = (char *)afs_error_table_name(error1);
	    break;
	}
	if (error1 == -1)
	    fprintf(log, "     Possible communication failure");
	else
	    fprintf(log, "     %s: %s", err2, err1);
	if (error2)
	    fprintf(log, ": %s", afs_error_message(error2));
	fprintf(log, "\n");
    }
    fflush(log);
}

void
TapeLog(debug, task, error1, error2, str, a, b, c, d, e, f, g, h, i, j)
     int debug;
     afs_int32 task, error1, error2;
     char *str, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    time_t now;
    char tbuffer[32], *timestr;

    now = time(0);
    timestr = afs_ctime(&now, tbuffer, sizeof(tbuffer));
    timestr[24] = '\0';

    fprintf(logIO, "%s: ", timestr);
    if (task)
	fprintf(logIO, "Task %u: ", task);
    PrintLog(logIO, error1, error2, str, a, b, c, d, e, f, g, h, i, j);

    if (lastPass && lastLogIO) {
	fprintf(lastLogIO, "%s: ", timestr);
	if (task)
	    fprintf(lastLogIO, "Task %u: ", task);
	PrintLog(lastLogIO, error1, error2, str, a, b, c, d, e, f, g, h, i,
		 j);
    }

    /* Now print to the screen if debug level requires */
    if (debug <= debugLevel)
	PrintLog(stdout, error1, error2, str, a, b, c, d, e, f, g, h, i, j);
}

void
TLog(task, str, a, b, c, d, e, f, g, h, i, j)
     afs_int32 task;
     char *str, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    /* Sends message to TapeLog and stdout */
    TapeLog(0, task, 0, 0, str, a, b, c, d, e, f, g, h, i, j);
}

void
ErrorLog(debug, task, error1, error2, str, a, b, c, d, e, f, g, h, i, j)
     int debug;
     afs_int32 task, error1, error2;
     char *str, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    time_t now;
    char tbuffer[32], *timestr;

    now = time(0);
    timestr = afs_ctime(&now, tbuffer, sizeof(tbuffer));
    timestr[24] = '\0';
    fprintf(ErrorlogIO, "%s: ", timestr);

    /* Print the time and task number */
    if (task)
	fprintf(ErrorlogIO, "Task %u: ", task);
    PrintLog(ErrorlogIO, error1, error2, str, a, b, c, d, e, f, g, h, i, j);

    TapeLog(debug, task, error1, error2, str, a, b, c, d, e, f, g, h, i, j);
}

void
ELog(task, str, a, b, c, d, e, f, g, h, i, j)
     afs_int32 task;
     char *str, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    /* Sends message to ErrorLog, TapeLog and stdout */
    ErrorLog(0, task, 0, 0, str, a, b, c, d, e, f, g, h, i, j);
}

/* first proc called by anybody who intends to use the device */
void
EnterDeviceQueue(devLatch)
     struct deviceSyncNode *devLatch;
{
    ObtainWriteLock(&(devLatch->lock));
    devLatch->flags = TC_DEVICEINUSE;
}

/* last proc called by anybody finishing using the device */
void
LeaveDeviceQueue(devLatch)
     struct deviceSyncNode *devLatch;
{
    devLatch->flags = 0;
    ReleaseWriteLock(&(devLatch->lock));
}

#define BELLTIME 60		/* 60 seconds before a bell rings */
#define BELLCHAR 7		/* ascii for bell */


#ifdef AFS_PTHREAD_ENV
#ifdef AFS_NT40_ENV
/* WaitForKeystroke : Wait until a key has been struck or time (secconds)
 * runs out and return to caller. The NT version of this function will return
 * immediately after a key has been pressed (doesn't wait for cr).
 * Input:
 *   seconds: wait for <seconds> seconds before returning. If seconds < 0,
 *            wait infinitely.
 * Return Value:
 *    1:  Keyboard input available
 *    0:  seconds elapsed. Timeout.
 *
 * STOLEN FROM LWP_WaitForKeystroke()
 */
int
WaitForKeystroke(int seconds)
{
    time_t startTime, nowTime;
    double timeleft = 1;
    struct timeval twait;

    time(&startTime);
    twait.tv_sec = 0;
    twait.tv_usec = 250;
    if (seconds >= 0)
	timeleft = seconds;

    do {
	/* check if we have a keystroke */
	if (_kbhit())
	    return 1;
	if (timeleft == 0)
	    break;

	/* sleep for  LWP_KEYSTROKE_DELAY ms and let other
	 * process run some*/
	select(0, 0, 0, 0, &twait);

	if (seconds > 0) {	/* we only worry about elapsed time if 
				 * not looping forever (seconds < 0) */
	    time(&nowTime);
	    timeleft = seconds - difftime(nowTime, startTime);
	}
    } while (timeleft > 0);
    return 0;
}
#else /* AFS_NT40)ENV */
extern int WaitForKeystroke(int);
/*
 *      STOLEN FROM LWP_WaitForKeystroke()
 */
int
WaitForKeystroke(int seconds)
{
    fd_set rdfds;
    int code;
    struct timeval twait;
    struct timeval *tp = NULL;

#ifdef AFS_LINUX20_ENV
    if (stdin->_IO_read_ptr < stdin->_IO_read_end)
	return 1;
#else
    if (stdin->_cnt > 0)
	return 1;
#endif
    FD_ZERO(&rdfds);
    FD_SET(fileno(stdin), &rdfds);

    if (seconds >= 0) {
	twait.tv_sec = seconds;
	twait.tv_usec = 0;
	tp = &twait;
    }
    code = select(1 + fileno(stdin), &rdfds, NULL, NULL, tp);
    return (code == 1) ? 1 : 0;
}
#endif

/* GetResponseKey() - Waits for a specified period of time and
 * returns a char when one has been typed by the user.
 * Input:
 *    seconds - how long to wait for a key press.
 *    *key    - char entered by user
 * Return Values: 
 *    0 - Time ran out before the user typed a key.
 *    1 - Valid char is being returned.
 *
 *    STOLEN FROM LWP_GetResponseKey();
 */
int
GetResponseKey(int seconds, char *key)
{
    int rc;

    if (key == NULL)
	return 0;		/* need space to store char */
    fflush(stdin);		/* flush all existing data and start anew */

    rc = WaitForKeystroke(seconds);
    if (rc == 0) {		/* time ran out */
	*key = 0;
	return rc;
    }

    /* now read the char. */
#ifdef AFS_NT40_ENV
    *key = getche();		/* get char and echo it to screen */
#else
    *key = getchar();
#endif
    return rc;
}
#endif /* AFS_PTHREAD_ENV
        * 
        * /* FFlushInput
        * *     flush all input
        * * notes:
        * *     only external clients are in recoverDb.c. Was static. PA
        */
void
FFlushInput()
{
    int w;

    fflush(stdin);

    while (1) {
#ifdef AFS_PTHREAD_ENV
	w = WaitForKeystroke(0);
#else
	w = LWP_WaitForKeystroke(0);
#endif /* AFS_PTHREAD_ENV */

	if (w) {
#ifdef AFS_NT40_ENV
	    getche();
#else
	    getchar();
#endif /* AFS_NT40_ENV */
	} else {
	    return;
	}
    }
}

int
callOutRoutine(taskId, tapePath, flag, name, dbDumpId, tapecount)
     afs_int32 taskId;
     char *tapePath;
     int flag;
     char *name;
     afs_uint32 dbDumpId;
     int tapecount;
{
    afs_int32 code = 0;
    int pid;

    char StapePath[256];
    char ScallOut[256];
    char Scount[10];
    char Sopcode[16];
    char Sdumpid[16];
    char Stape[40];
    char *callOut;

    char *CO_argv[10];
    char *CO_envp[1];


    callOut = opencallout;
    switch (flag) {
    case READOPCODE:
	strcpy(Sopcode, "restore");
	break;
    case APPENDOPCODE:
	strcpy(Sopcode, "appenddump");
	break;
    case WRITEOPCODE:
	strcpy(Sopcode, "dump");
	break;
    case LABELOPCODE:
	strcpy(Sopcode, "labeltape");
	break;
    case READLABELOPCODE:
	strcpy(Sopcode, "readlabel");
	break;
    case SCANOPCODE:
	strcpy(Sopcode, "scantape");
	break;
    case RESTOREDBOPCODE:
	strcpy(Sopcode, "restoredb");
	break;
    case SAVEDBOPCODE:
	strcpy(Sopcode, "savedb");
	break;
    case CLOSEOPCODE:
	strcpy(Sopcode, "unmount");
	callOut = closecallout;
	break;
    default:
	strcpy(Sopcode, "unknown");
	break;
    }

    if (!callOut)		/* no script to call */
	return 0;

    strcpy(ScallOut, callOut);
    CO_argv[0] = ScallOut;

    strcpy(StapePath, tapePath);
    CO_argv[1] = StapePath;

    CO_argv[2] = Sopcode;

    if (flag == CLOSEOPCODE) {
	CO_argv[3] = NULL;
    } else {
	sprintf(Scount, "%d", tapecount);
	CO_argv[3] = Scount;

	/* The tape label name - special case labeltape */
	if (!name || (strcmp(name, "") == 0))	/* no label */
	    strcpy(Stape, "none");
	else {			/* labeltape */
#ifdef AFS_NT40_ENV
	    if (!strcmp(name, TC_NULLTAPENAME))	/* pass "<NULL>" instead of <NULL> */
		strcpy(Stape, TC_QUOTEDNULLTAPENAME);
	    else
#endif
		strcpy(Stape, name);
	}
	CO_argv[4] = Stape;

	/* The tape id */
	if (!dbDumpId)
	    strcpy(Sdumpid, "none");
	else
	    sprintf(Sdumpid, "%u", dbDumpId);
	CO_argv[5] = Sdumpid;

	CO_argv[6] = NULL;
    }

    CO_envp[0] = NULL;

    pid = spawnprocve(callOut, CO_argv, CO_envp, 2);
    if (pid < 0) {
	ErrorLog(0, taskId, errno, 0,
		 "Call to %s outside routine %s failed\n", Sopcode, callOut);
	return 0;
    }

    return (pid);
}

/*
 * unmountTape
 *     Unmounts a tape and prints a warning if it can't unmount it.
 *     Regardless of error, the closecallout routine will be called
 *     (unless a tape is not mounted in the first place).
 */
unmountTape(taskId, tapeInfoPtr)
     afs_int32 taskId;
     struct butm_tapeInfo *tapeInfoPtr;
{
    afs_int32 code;
    int cpid, status, rcpid;

    code = butm_Dismount(tapeInfoPtr);
    if (code && (code != BUTM_NOMOUNT))
	ErrorLog(0, taskId, code, (tapeInfoPtr)->error,
		 "Warning: Can't close tape\n");

    if (tapemounted && closecallout) {
	setStatus(taskId, CALL_WAIT);

	cpid =
	    callOutRoutine(taskId, globalTapeConfig.device, CLOSEOPCODE, "",
			   0, 1);
	while (cpid) {		/* Wait until return */
	    status = 0;
	    rcpid = waitpid(cpid, &status, WNOHANG);
	    if (rcpid > 0) {
		tapemounted = 0;
		break;
	    }
	    if (rcpid == -1 && errno != EINTR) {
		tapemounted = 0;
		afs_com_err(whoami, errno,
			"Error waiting for callout script to terminate.");
		break;
	    }
#ifdef AFS_PTHREAD_ENV
	    sleep(1);
#else
	    IOMGR_Sleep(1);
#endif

	    if (checkAbortByTaskId(taskId)) {
		TLog(taskId, "Callout routine has been aborted\n");
		if (kill(cpid, SIGKILL))	/* Cancel callout */
		    ErrorLog(0, taskId, errno, 0,
			     "Kill of callout process %d failed\n", cpid);
		break;
	    }
	}
    }
    clearStatus(taskId, CALL_WAIT);
}

/* PrintPrompt
 *	print out prompt to operator
 * calledby:
 *	PromptForTape only.
 */

void static
PrintPrompt(flag, name, dumpid)
     int flag;
     char *name;
{
    char tapename[BU_MAXTAPELEN + 32];
    char *dn;

    TAPENAME(tapename, name, dumpid);

    printf("******* OPERATOR ATTENTION *******\n");
    printf("Device :  %s \n", globalTapeConfig.device);

    switch (flag) {
    case READOPCODE:		/* mount for restore */
	printf("Please put in tape %s for reading", tapename);
	break;

    case APPENDOPCODE:		/* mount for dump (appends) */

	dn = extractDumpName(name);

	if (!dn || !dumpid)
	    printf("Please put in last tape of dump set for appending dump");
	else
	    printf
		("Please put in last tape of dump set for appending dump %s (DumpID %u)",
		 dn, dumpid);
	break;

    case WRITEOPCODE:		/* mount for dump */
	if (strcmp(name, "") == 0)
	    printf("Please put in tape for writing");

	/* The name is what we are going to label the tape as */
	else
	    printf("Please put in tape %s for writing", tapename);
	break;

    case LABELOPCODE:		/* mount for labeltape */
	printf("Please put in tape to be labelled as %s", tapename);
	break;

    case READLABELOPCODE:	/* mount for readlabel */
	printf("Please put in tape whose label is to be read");
	break;

    case SCANOPCODE:		/* mount for scantape */
	if (strcmp(name, "") == 0)
	    printf("Please put in tape to be scanned");
	else
	    printf("Please put in tape %s for scanning", tapename);
	break;

    case RESTOREDBOPCODE:	/* Mount for restoredb */
	printf("Please insert a tape %s for the database restore", tapename);
	break;

    case SAVEDBOPCODE:		/* Mount for savedb */
	printf("Please insert a writeable tape %s for the database dump",
	       tapename);
	break;

    default:
	break;
    }
    printf(" and hit return when done\n");
}

/* PromptForTape
 *	Prompt the operator to change the tape. 
 *      Use to be a void routine but now returns an error. Some calls
 *      don't use the error code.
 * notes:
 *	only external clients are in recoverDb.c. Was static PA
 */
afs_int32
PromptForTape(flag, name, dbDumpId, taskId, tapecount)
     int flag;
     char *name;
     afs_uint32 dbDumpId;	/* Specific dump ID - If non-zero */
     afs_uint32 taskId;
     int tapecount;
{
    register afs_int32 code = 0;
    afs_int32 wcode;
    afs_int32 start = 0;
    char inchr;
    int CallOut;
    int cpid, status, rcpid;

    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    if (dbDumpId)
	TapeLog(2, taskId, 0, 0, "Prompt for tape %s (%u)\n", name, dbDumpId);
    else
	TapeLog(2, taskId, 0, 0, "Prompt for tape %s\n", name);

    CallOut = (opencallout ? 1 : 0);
    if (CallOut) {
	setStatus(taskId, CALL_WAIT);

	cpid =
	    callOutRoutine(taskId, globalTapeConfig.device, flag, name,
			   dbDumpId, tapecount);
	if (cpid == 0)
	    CallOut = 0;	/* prompt at screen */

	while (CallOut) {	/* Check if callout routine finished */
	    status = 0;
	    rcpid = waitpid(cpid, &status, WNOHANG);
	    if (rcpid > 0) {
		if (rcpid != cpid)
		    wcode = -1;
		else if (WIFEXITED(status))
		    wcode = WEXITSTATUS(status);
		else
		    wcode = -1;

		if (wcode == 0) {
		    break;	/* All done */
		} else if (wcode == 1) {
		    ERROR_EXIT(TC_ABORTEDBYREQUEST);	/* Abort */
		} else if ((flag == READOPCODE) && (wcode == 3)) {
		    ERROR_EXIT(TC_SKIPTAPE);	/* Restore: skip the tape */
		} else {
		    TLog(taskId,
			 "Callout routine has exited with code %d: will prompt\n",
			 wcode);
		    CallOut = 0;	/* Switch to keyboard input */
		    break;
		}
	    }
	    /* if waitpid experienced an error, we prompt */
	    if (rcpid == -1 && errno != EINTR) {
		afs_com_err(whoami, errno,
			"Error waiting for callout script to terminate.");
		TLog(taskId,
		     "Can't get exit status from callout script. will prompt\n");
		CallOut = 0;
		break;
	    }
#ifdef AFS_PTHREAD_ENV
	    sleep(1);
#else
	    IOMGR_Sleep(1);
#endif

	    if (checkAbortByTaskId(taskId)) {
		printf
		    ("This tape operation has been aborted by the coordinator.\n");

		if (kill(cpid, SIGKILL))	/* Cancel callout */
		    ErrorLog(0, taskId, errno, 0,
			     "Kill of callout process %d failed\n", cpid);

		ERROR_EXIT(TC_ABORTEDBYREQUEST);
	    }
	}
    }

    if (!CallOut) {
	clearStatus(taskId, CALL_WAIT);
	setStatus(taskId, OPR_WAIT);

	PrintPrompt(flag, name, dbDumpId);

	/* Loop until we get ok to go ahead (or abort) */
	while (1) {
	    if (time(0) > start + BELLTIME) {
		start = time(0);
		FFlushInput();
		putchar(BELLCHAR);
		fflush(stdout);
	    }
#ifdef AFS_PTHREAD_ENV
	    wcode = GetResponseKey(5, &inchr);	/* inchr stores key read */
#else
	    wcode = LWP_GetResponseKey(5, &inchr);	/* inchr stores key read */
#endif
	    if (wcode == 1) {	/* keyboard input is available */

		if ((inchr == 'a') || (inchr == 'A')) {
		    printf("This tape operation has been aborted.\n");
		    ERROR_EXIT(TC_ABORTEDBYREQUEST);	/* Abort command */
		} else if ((flag == READOPCODE)
			   && ((inchr == 's') || (inchr == 'S'))) {
		    printf("This tape will be skipped.\n");
		    ERROR_EXIT(TC_SKIPTAPE);	/* Restore: skip the tape */
		}
		break;		/* continue */
	    }

	    if (checkAbortByTaskId(taskId)) {
		printf
		    ("This tape operation has been aborted by the coordinator.\n");
		ERROR_EXIT(TC_ABORTEDBYREQUEST);
	    }
	}

    }

    printf("Thanks, now proceeding with tape ");
    switch (flag) {
    case RESTOREDBOPCODE:
    case READOPCODE:
	printf("reading");
	break;

    case APPENDOPCODE:
	printf("append writing");
	break;

    case SAVEDBOPCODE:
    case WRITEOPCODE:
	printf("writing");
	break;

    case LABELOPCODE:
	printf("labelling");
	break;

    case READLABELOPCODE:
	printf("label reading");
	break;

    case SCANOPCODE:
	printf("scanning");
	break;

    default:
	printf("unknown");
	break;
    }

    printf(" operation.\n");
    if (!CallOut)
	printf("**********************************\n");

    TapeLog(2, taskId, 0, 0, "Proceeding with tape operation\n");
    tapemounted = 1;

  error_exit:
    clearStatus(taskId, (OPR_WAIT | CALL_WAIT));
    return (code);
}


/* VolHeaderToHost
 *	convert the fields in the tapeVolHeader into host byte order,
 *	placing the converted copy of the structure into the hostVolHeader
 * entry:
 *	tapeVolHeader - points to volume header read from tape
 *	hostVolHeader - pointer to struct for result
 * exit:
 *	hostVolHeader - information in host byte order
 */

afs_int32
VolHeaderToHost(hostVolHeader, tapeVolHeader)
     struct volumeHeader *hostVolHeader, *tapeVolHeader;
{
    switch (ntohl(tapeVolHeader->versionflags)) {
    case TAPE_VERSION_0:
	/* sizes in bytes and fields in host order */
	memcpy(tapeVolHeader, hostVolHeader, sizeof(struct volumeHeader));
	break;

    case TAPE_VERSION_1:
    case TAPE_VERSION_2:
    case TAPE_VERSION_3:	/* for present */
    case TAPE_VERSION_4:
	/* sizes in K and fields in network order */
	/* do the conversion field by field */

	strcpy(hostVolHeader->preamble, tapeVolHeader->preamble);
	strcpy(hostVolHeader->postamble, tapeVolHeader->postamble);
	strcpy(hostVolHeader->volumeName, tapeVolHeader->volumeName);
	strcpy(hostVolHeader->dumpSetName, tapeVolHeader->dumpSetName);
	hostVolHeader->volumeID = ntohl(tapeVolHeader->volumeID);
	hostVolHeader->server = ntohl(tapeVolHeader->server);
	hostVolHeader->part = ntohl(tapeVolHeader->part);
	hostVolHeader->from = ntohl(tapeVolHeader->from);
	hostVolHeader->frag = ntohl(tapeVolHeader->frag);
	hostVolHeader->magic = ntohl(tapeVolHeader->magic);
	hostVolHeader->contd = ntohl(tapeVolHeader->contd);
	hostVolHeader->dumpID = ntohl(tapeVolHeader->dumpID);
	hostVolHeader->level = ntohl(tapeVolHeader->level);
	hostVolHeader->parentID = ntohl(tapeVolHeader->parentID);
	hostVolHeader->endTime = ntohl(tapeVolHeader->endTime);
	hostVolHeader->versionflags = ntohl(tapeVolHeader->versionflags);
	hostVolHeader->cloneDate = ntohl(tapeVolHeader->cloneDate);
	break;

    default:
	return (TC_BADVOLHEADER);
    }
    return (0);
}

afs_int32
ReadVolHeader(taskId, tapeInfoPtr, volHeaderPtr)
     afs_int32 taskId;
     struct butm_tapeInfo *tapeInfoPtr;
     struct volumeHeader *volHeaderPtr;
{
    afs_int32 code = 0;
    afs_int32 nbytes;
    struct volumeHeader volHead;

    /* Read the volume header */
    code =
	butm_ReadFileData(tapeInfoPtr, tapeBlock.data, sizeof(tapeVolumeHT),
			  &nbytes);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfoPtr->error,
		 "Can't read volume header on tape\n");
	ERROR_EXIT(code);
    }

    code = readVolumeHeader(tapeBlock.data, 0L, &volHead);
    if (code) {
	ErrorLog(0, taskId, code, 0,
		 "Can't find volume header on tape block\n");
	ERROR_EXIT(code);
    }

    code = VolHeaderToHost(volHeaderPtr, &volHead);
    if (code) {
	ErrorLog(0, taskId, code, 0, "Can't convert volume header\n");
	ERROR_EXIT(code);
    }

  error_exit:
    return code;
}

afs_int32 static
GetVolumeHead(taskId, tapeInfoPtr, position, volName, volId)
     afs_int32 taskId;
     struct butm_tapeInfo *tapeInfoPtr;
     afs_int32 position;
     char *volName;
     afs_int32 volId;
{
    afs_int32 code = 0;
    struct volumeHeader tapeVolHeader;

    /* Position directly to the volume and read the header */
    if (position) {
	code = butm_Seek(tapeInfoPtr, position);
	if (code) {
	    ErrorLog(0, taskId, code, tapeInfoPtr->error,
		     "Can't seek to position %u on tape\n", position);
	    ERROR_EXIT(code);
	}

	code = butm_ReadFileBegin(tapeInfoPtr);
	if (code) {
	    ErrorLog(0, taskId, code, tapeInfoPtr->error,
		     "Can't read FileBegin on tape\n");
	    ERROR_EXIT(code);
	}

	/* Read the volume header */
	code = ReadVolHeader(taskId, tapeInfoPtr, &tapeVolHeader);
	if (code)
	    ERROR_EXIT(code);

	/* Check if volume header matches */
	if (strcmp(tapeVolHeader.volumeName, volName))
	    ERROR_EXIT(TC_BADVOLHEADER);
	if (volId && (tapeVolHeader.volumeID != volId))
	    ERROR_EXIT(TC_BADVOLHEADER);
	if (tapeVolHeader.magic != TC_VOLBEGINMAGIC)
	    ERROR_EXIT(TC_BADVOLHEADER);
    }

    /* Do a sequential search for the volume */
    else {
	while (1) {
	    code = butm_ReadFileBegin(tapeInfoPtr);
	    if (code) {
		ErrorLog(0, taskId, code, tapeInfoPtr->error,
			 "Can't read FileBegin on tape\n");
		ERROR_EXIT(code);
	    }

	    code = ReadVolHeader(taskId, tapeInfoPtr, &tapeVolHeader);
	    if (code)
		ERROR_EXIT(TC_VOLUMENOTONTAPE);

	    /* Test if we found the volume */
	    if ((strcmp(tapeVolHeader.volumeName, volName) == 0)
		&& (!volId || (volId == tapeVolHeader.volumeID)))
		break;

	    /* skip to the next HW EOF marker */
	    code = SeekFile(tapeInfoPtr, 1);
	    if (code) {
		ErrorLog(0, taskId, code, tapeInfoPtr->error,
			 "Can't seek to next EOF on tape\n");
		ERROR_EXIT(code);
	    }
	}
    }

  error_exit:
    return code;
}

afs_int32
GetRestoreTape(taskId, tapeInfoPtr, tname, tapeID, prompt)
     afs_int32 taskId;
     struct butm_tapeInfo *tapeInfoPtr;
     char *tname;
     afs_int32 tapeID;
     int prompt;
{
    struct butm_tapeLabel tapeLabel;
    afs_int32 code = 0, rc;
    int tapecount = 1;
    struct budb_dumpEntry dumpEntry;

    /* Make sure that the dump/tape is not a XBSA dump */
    rc = bcdb_FindDumpByID(tapeID, &dumpEntry);
    if (!rc && (dumpEntry.flags & (BUDB_DUMP_ADSM | BUDB_DUMP_BUTA))) {
	ErrorLog(0, taskId, 0, 0,
		 "Volumes from dump %u are XBSA dumps (skipping)\n", tapeID);
	ERROR_EXIT(TC_SKIPTAPE);
    }

    while (1) {
	if (prompt) {
	    code =
		PromptForTape(READOPCODE, tname, tapeID, taskId, tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	prompt = 1;
	tapecount++;

	code = butm_Mount(tapeInfoPtr, tname);
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error, "Can't open tape\n");
	    goto newtape;
	}

	code = butm_ReadLabel(tapeInfoPtr, &tapeLabel, 1);
	if (code) {
	    ErrorLog(0, taskId, code, tapeInfoPtr->error,
		     "Can't read tape label\n");
	    goto newtape;
	}

	/* Now check the label to see if the tapename matches or tapeids match */
	if (strcmp(TNAME(&tapeLabel), tname)
	    || ((tapeLabel.structVersion >= TAPE_VERSION_3)
		&& (tapeLabel.dumpid != tapeID))) {
	    char expectedName[BU_MAXTAPELEN + 32],
		gotName[BU_MAXTAPELEN + 32];

	    TAPENAME(expectedName, tname, tapeID);
	    LABELNAME(gotName, &tapeLabel);

	    TapeLog(0, taskId, 0, 0,
		    "Tape label expected %s, label seen %s\n", expectedName,
		    gotName);
	    goto newtape;
	}

	break;

      newtape:
	unmountTape(taskId, tapeInfoPtr);
    }

  error_exit:
    return code;
}

afs_int32
xbsaRestoreVolumeData(call, rparamsPtr)
     register struct rx_call *call;
     struct restoreParams *rparamsPtr;
{
    afs_int32 code = 0;
#ifdef xbsa
    afs_int32 curChunk, rc;
    afs_uint32 totalWritten;
    afs_int32 headBytes, tailBytes, w;
    afs_int32 taskId;
    struct volumeHeader volTrailer;
    afs_int32 vtsize = 0;
    int found;
    struct dumpNode *nodePtr;
    struct tc_restoreDesc *Restore;
    afs_int32 bytesRead, tbuffersize, endData = 0;
    char *buffer = (char *)bufferBlock, tbuffer[256];

    nodePtr = rparamsPtr->nodePtr;
    Restore = nodePtr->restores;
    taskId = nodePtr->taskID;

    /* Read the volume fragment one block at a time until
     * find a volume trailer
     */
    curChunk = BIGCHUNK + 1;
    tbuffersize = 0;
    totalWritten = 0;

    while (!endData) {
	rc = xbsa_ReadObjectData(&butxInfo, buffer, dataSize, &bytesRead,
				 &endData);
	if (restoretofile && (bytesRead > 0)) {
	    fwrite(buffer, bytesRead, 1, restoretofilefd);	/* Save to a file */
	}
	if (rc != XBSA_SUCCESS) {
	    ErrorLog(0, taskId, rc, 0,
		     "Unable to read volume data from the server\n");
	    ERROR_EXIT(rc);
	}

	/* Periodically update status structure and check if should abort */
	curChunk += bytesRead;
	if (curChunk > BIGCHUNK) {
	    curChunk = 0;
	    lock_Status();
	    nodePtr->statusNodePtr->nKBytes = totalWritten / 1024;
	    unlock_Status();

	    if (checkAbortByTaskId(taskId))
		ERROR_EXIT(TC_ABORTEDBYREQUEST);
	}

	if (!endData && (bytesRead > 0)) {
	    /* Fill tbuffer up with data from end of buffer and write
	     * the remainder of buffer out.
	     */
	    if ((tbuffersize == 0) || (bytesRead >= sizeof(tbuffer))) {
		/* Write out contents of tbuffer */
		if (tbuffersize) {
		    w = rx_Write(call, tbuffer, tbuffersize);
		    if (w != tbuffersize) {
			ErrorLog(0, taskId, -1, 0,
				 "Error in RX write: Wrote %d bytes\n", w);
			ERROR_EXIT(-1);
		    }
		    totalWritten += w;
		}
		/* fill tbuffer with end of buffer */
		bytesRead -= sizeof(tbuffer);
		memcpy(tbuffer, buffer + bytesRead, sizeof(tbuffer));
		tbuffersize = sizeof(tbuffer);
		/* Write out whatever is left over in buffer */
		if (bytesRead) {
		    w = rx_Write(call, buffer, bytesRead);
		    if (w != bytesRead) {
			ErrorLog(0, taskId, -1, 0,
				 "Error in RX data write: Wrote %d bytes\n",
				 w);
			ERROR_EXIT(-1);
		    }
		    totalWritten += w;
		    bytesRead = 0;
		}
	    } else if ((tbuffersize + bytesRead) <= sizeof(tbuffer)) {
		/* Copy all of buffer into tbuffer (it will fit) */
		memcpy(tbuffer + tbuffersize, buffer, bytesRead);
		tbuffersize += bytesRead;
		bytesRead = 0;
	    } else {
		/* We need to write some of tbuffer out and fill it with buffer */
		int towrite = bytesRead - (sizeof(tbuffer) - tbuffersize);
		w = rx_Write(call, tbuffer, towrite);
		if (w != towrite) {
		    ErrorLog(0, taskId, -1, 0,
			     "Error in RX write: Wrote %d bytes\n", w);
		    ERROR_EXIT(-1);
		}
		totalWritten += w;
		tbuffersize -= w;

		/* Move the data in tbuffer up */
		memcpy(tbuffer, tbuffer + towrite, tbuffersize);

		/* Now copy buffer in */
		memcpy(tbuffer + tbuffersize, buffer, bytesRead);
		tbuffersize += bytesRead;
		bytesRead = 0;
	    }
	}
    }

    /* Pull the volume trailer from the last two buffers */
    found =
	FindVolTrailer2(tbuffer, tbuffersize, &headBytes, buffer, bytesRead,
			&tailBytes, &volTrailer);

    if (!found) {
	ErrorLog(0, taskId, TC_MISSINGTRAILER, 0, "Missing volume trailer\n");
	ERROR_EXIT(TC_MISSINGTRAILER);
    }

    /* Now rx_write the data in the last two blocks */
    if (headBytes) {
	w = rx_Write(call, tbuffer, headBytes);
	if (w != headBytes) {
	    ErrorLog(0, taskId, -1, 0,
		     "Error in RX trail1 write: Wrote %d bytes\n", w);
	    ERROR_EXIT(-1);
	}
	totalWritten += w;
    }
    if (tailBytes) {
	w = rx_Write(call, buffer, tailBytes);
	if (w != tailBytes) {
	    ErrorLog(0, taskId, -1, 0,
		     "Error in RX trail2 write: Wrote %d bytes\n", w);
	    ERROR_EXIT(-1);
	}
	totalWritten += w;
    }

  error_exit:
#endif /*xbsa */
    return code;
}

/* restoreVolume
 *	sends the contents of volume dump  to Rx Stream associated
 *	with <call>
 */

afs_int32
restoreVolumeData(call, rparamsPtr)
     register struct rx_call *call;
     struct restoreParams *rparamsPtr;
{
    afs_int32 curChunk;
    afs_uint32 totalWritten = 0;
    afs_int32 code;
    afs_int32 headBytes, tailBytes, w;
    afs_int32 taskId;
    afs_int32 nbytes;		/* # bytes data in last tape block read */
    struct volumeHeader tapeVolTrailer;
    int found;
    int moretoread;
    afs_int32 startRbuf, endRbuf, startWbuf, endWbuf, buf, pbuf, lastbuf;
    struct tc_restoreDesc *Restore;
    struct dumpNode *nodePtr;
    struct butm_tapeInfo *tapeInfoPtr;
    char *origVolName;
    afs_int32 origVolID;

    nodePtr = rparamsPtr->nodePtr;
    taskId = nodePtr->taskID;
    Restore = nodePtr->restores;
    tapeInfoPtr = rparamsPtr->tapeInfoPtr;
    origVolName = Restore[rparamsPtr->frag].oldName;
    origVolID = Restore[rparamsPtr->frag].origVid;

    /* Read the volume one fragment at a time */
    while (rparamsPtr->frag < nodePtr->arraySize) {
	/*w */
	curChunk = BIGCHUNK + 1;	/* Check if should abort */

	/* Read the volume fragment one block at a time until
	 * find a volume trailer
	 */
	moretoread = 1;
	startRbuf = 0;
	endRbuf = 0;
	startWbuf = 0;
	while (moretoread) {
	    /* Fill the circular buffer with tape blocks
	     * Search for volume trailer in the process.
	     */
	    buf = startRbuf;
	    do {
		code =
		    butm_ReadFileData(tapeInfoPtr, bufferBlock[buf].data,
				      BUTM_BLKSIZE, &nbytes);
		if (code) {
		    ErrorLog(0, taskId, code, tapeInfoPtr->error,
			     "Can't read FileData on tape %s\n",
			     rparamsPtr->mntTapeName);
		    ERROR_EXIT(code);
		}
		curChunk += BUTM_BLKSIZE;

		/* Periodically update status structure and check if should abort */
		if (curChunk > BIGCHUNK) {
		    curChunk = 0;

		    lock_Status();
		    nodePtr->statusNodePtr->nKBytes = totalWritten / 1024;
		    unlock_Status();

		    if (checkAbortByTaskId(taskId))
			ERROR_EXIT(TC_ABORTEDBYREQUEST);
		}

		/* step to next block in buffer */
		pbuf = buf;
		buf = ((buf + 1) == tapeblocks) ? 0 : (buf + 1);

		/* If this is the end of the volume, the exit the loop */
		if ((nbytes != BUTM_BLKSIZE)
		    ||
		    (FindVolTrailer
		     (bufferBlock[pbuf].data, nbytes, &tailBytes,
		      &tapeVolTrailer)))
		    moretoread = 0;

	    } while (moretoread && (buf != endRbuf));

	    /* Write the buffer upto (but not including) the last read block
	     * If volume is completely read, then leave the last two blocks.
	     */
	    lastbuf = endWbuf = pbuf;
	    if (!moretoread && (endWbuf != startWbuf))
		endWbuf = (endWbuf == 0) ? (tapeblocks - 1) : (endWbuf - 1);

	    for (buf = startWbuf; buf != endWbuf;
		 buf = (((buf + 1) == tapeblocks) ? 0 : (buf + 1))) {
		w = rx_Write(call, bufferBlock[buf].data, BUTM_BLKSIZE);
		if (w != BUTM_BLKSIZE) {
		    ErrorLog(0, taskId, -1, 0, "Error in RX write\n");
		    ERROR_EXIT(-1);
		}
		totalWritten += BUTM_BLKSIZE;
	    }

	    /* Setup pointers to refill buffer */
	    startRbuf = ((lastbuf + 1) == tapeblocks) ? 0 : (lastbuf + 1);
	    endRbuf = endWbuf;
	    startWbuf = endWbuf;
	}

	/* lastbuf is last block read and it has nbytes of data
	 * startWbuf is the 2nd to last block read 
	 * Seach for the volume trailer in these two blocks.
	 */
	if (lastbuf == startWbuf)
	    found =
		FindVolTrailer2(NULL, 0, &headBytes,
				bufferBlock[lastbuf].data, nbytes, &tailBytes,
				&tapeVolTrailer);
	else
	    found =
		FindVolTrailer2(bufferBlock[startWbuf].data, BUTM_BLKSIZE,
				&headBytes, bufferBlock[lastbuf].data, nbytes,
				&tailBytes, &tapeVolTrailer);
	if (!found) {
	    ErrorLog(0, taskId, TC_MISSINGTRAILER, 0,
		     "Missing volume trailer on tape %s\n",
		     rparamsPtr->mntTapeName);
	    ERROR_EXIT(TC_MISSINGTRAILER);
	}

	/* Now rx_write the data in the last two blocks */
	if (headBytes) {
	    w = rx_Write(call, bufferBlock[startWbuf].data, headBytes);
	    if (w != headBytes) {
		ErrorLog(0, taskId, -1, 0, "Error in RX write\n");
		ERROR_EXIT(-1);
	    }
	    totalWritten += headBytes;
	}
	if (tailBytes) {
	    w = rx_Write(call, bufferBlock[lastbuf].data, tailBytes);
	    if (w != tailBytes) {
		ErrorLog(0, taskId, -1, 0, "Error in RX write\n");
		ERROR_EXIT(-1);
	    }
	    totalWritten += tailBytes;
	}

	/* Exit the loop if the volume is not continued on next tape */
	if (!tapeVolTrailer.contd)
	    break;		/* We've read the entire volume */

	/* Volume is continued on next tape. 
	 * Step to the next volume fragment and prompt for its tape.
	 * When a volume has multiple frags, those frags are on different
	 * tapes. So we know that we need to prompt for a tape.
	 */
	rparamsPtr->frag++;
	if (rparamsPtr->frag >= nodePtr->arraySize)
	    break;

	unmountTape(taskId, tapeInfoPtr);
	strcpy(rparamsPtr->mntTapeName, Restore[rparamsPtr->frag].tapeName);
	rparamsPtr->tapeID =
	    (Restore[rparamsPtr->frag].
	     initialDumpId ? Restore[rparamsPtr->frag].
	     initialDumpId : Restore[rparamsPtr->frag].dbDumpId);
	code =
	    GetRestoreTape(taskId, tapeInfoPtr, rparamsPtr->mntTapeName,
			   rparamsPtr->tapeID, 1);
	if (code)
	    ERROR_EXIT(code);

	/* Position to the frag and read the volume header */
	code =
	    GetVolumeHead(taskId, tapeInfoPtr,
			  Restore[rparamsPtr->frag].position, origVolName,
			  origVolID);
	if (code) {
	    ErrorLog(0, taskId, code, 0,
		     "Can't find volume %s (%u) on tape %s\n", origVolName,
		     origVolID, rparamsPtr->mntTapeName);
	    ERROR_EXIT(TC_VOLUMENOTONTAPE);
	}
    }				/*w */

  error_exit:
    return code;
}

/* SkipTape
 *    Find all the volumes on a specific tape and mark them to skip.
 */
SkipTape(Restore, size, index, tapename, tapeid, taskid)
     struct tc_restoreDesc *Restore;
     afs_int32 size, index, tapeid, taskid;
     char *tapename;
{
    afs_int32 i, tid;

    for (i = index; i < size; i++) {
	if (Restore[i].flags & RDFLAG_SKIP)
	    continue;
	tid =
	    (Restore[i].initialDumpId ? Restore[i].initialDumpId : Restore[i].
	     dbDumpId);
	if ((strcmp(Restore[i].tapeName, tapename) == 0) && (tid == tapeid)) {
	    SkipVolume(Restore, size, i, Restore[i].origVid, taskid);
	}
    }
    return 0;
}

/* SkipVolume
 *    Find all the entries for a volume and mark them to skip.
 */
SkipVolume(Restore, size, index, volid, taskid)
     struct tc_restoreDesc *Restore;
     afs_int32 size, index, volid, taskid;
{
    afs_int32 i;
    int report = 1;

    for (i = index; i < size; i++) {
	if (Restore[i].flags & RDFLAG_SKIP)
	    continue;
	if (Restore[i].origVid == volid) {
	    Restore[i].flags |= RDFLAG_SKIP;
	    if (report) {
		TLog(taskid, "Restore: Skipping %svolume %s (%u)\n",
		     ((Restore[i].dumpLevel == 0) ? "" : "remainder of "),
		     Restore[i].oldName, volid);
		report = 0;
	    }
	}
    }
    return 0;
}

xbsaRestoreVolume(taskId, restoreInfo, rparamsPtr)
     afs_uint32 taskId;
     struct tc_restoreDesc *restoreInfo;
     struct restoreParams *rparamsPtr;
{
    afs_int32 code = 0;
#ifdef xbsa
    afs_int32 rc;
    afs_int32 newServer, newPart, newVolId;
    char *newVolName;
    int restoreflags, havetrans = 0, startread = 0;
    afs_int32 bytesRead, endData = 0;
    afs_uint32 dumpID;
    struct budb_dumpEntry dumpEntry;
    char volumeNameStr[XBSA_MAX_PATHNAME], dumpIdStr[XBSA_MAX_OSNAME];
    struct volumeHeader volHeader, hostVolHeader;

    if (restoretofile) {
	restoretofilefd = fopen(restoretofile, "w+");
    }

    dumpID = restoreInfo->dbDumpId;

    rc = bcdb_FindDumpByID(dumpID, &dumpEntry);
    if (rc) {
	ErrorLog(0, taskId, rc, 0, "Can't read database for dump %u\n",
		 dumpID);
	ERROR_EXIT(rc);
    }

    /* ADSM servers restore ADSM and BUTA dumps */
    if ((xbsaType == XBSA_SERVER_TYPE_ADSM)
	&& !(dumpEntry.flags & (BUDB_DUMP_ADSM | BUDB_DUMP_BUTA))) {
	ELog(taskId,
	     "The dump requested by this restore operation for the "
	     "volumeset is incompatible with this instance of butc\n");
	/* Skip the entire dump (one dump per tape) */
	ERROR_EXIT(TC_SKIPTAPE);
    }

    /* make sure we are connected to the correct server. */
    if ((strlen((char *)dumpEntry.tapes.tapeServer) != 0)
	&& (strcmp((char *)dumpEntry.tapes.tapeServer, butxInfo.serverName) !=
	    0)) {
	if ((dumpEntry.flags & (BUDB_DUMP_XBSA_NSS | BUDB_DUMP_BUTA))
	    && !forcemultiple) {
	    TLog(taskId,
		 "Dump %d is on server %s but butc is connected "
		 "to server %s (attempting to restore)\n", dumpID,
		 (char *)dumpEntry.tapes.tapeServer, butxInfo.serverName);
	} else {
	    TLog(taskId,
		 "Dump %d is on server %s but butc is connected "
		 "to server %s (switching servers)\n", dumpID,
		 (char *)dumpEntry.tapes.tapeServer, butxInfo.serverName);

	    rc = InitToServer(taskId, &butxInfo,
			      (char *)dumpEntry.tapes.tapeServer);
	    if (rc != XBSA_SUCCESS)
		ERROR_EXIT(TC_SKIPTAPE);
	}
    }

    /* Start a transaction and query the server for the correct fileset dump */
    rc = xbsa_BeginTrans(&butxInfo);
    if (rc != XBSA_SUCCESS) {
	ELog(taskId, "Unable to create a new transaction\n");
	ERROR_EXIT(TC_SKIPTAPE);
    }
    havetrans = 1;

    if (dumpEntry.flags & BUDB_DUMP_BUTA) {	/* old buta style names */
	sprintf(dumpIdStr, "/%d", dumpID);
	strcpy(volumeNameStr, "/");
	strcat(volumeNameStr, restoreInfo->oldName);
    } else {			/* new butc names */
	extern char *butcdumpIdStr;
	strcpy(dumpIdStr, butcdumpIdStr);
	sprintf(volumeNameStr, "/%d", dumpID);
	strcat(volumeNameStr, "/");
	strcat(volumeNameStr, restoreInfo->oldName);
    }

    rc = xbsa_QueryObject(&butxInfo, dumpIdStr, volumeNameStr);
    if (rc != XBSA_SUCCESS) {
	ELog(taskId,
	     "Unable to locate object (%s) of dump (%s) on the server\n",
	     volumeNameStr, dumpIdStr);
	ERROR_EXIT(rc);
    }

    rc = xbsa_EndTrans(&butxInfo);
    havetrans = 0;
    if (rc != XBSA_SUCCESS) {
	ELog(taskId, "Unable to terminate the current transaction\n");
	ERROR_EXIT(rc);
    }

    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    /* Now start a transaction on the volume to restore and read the
     * volumeheader. We do this before starting a transaction on
     * volserver to restore the volume because the XBSA server may take
     * a while to mount and seek to the volume causing the volserver to
     * time out.
     */
    rc = xbsa_BeginTrans(&butxInfo);
    if (rc != XBSA_SUCCESS) {
	ELog(taskId, "Unable to create a new transaction\n");
	ERROR_EXIT(TC_SKIPTAPE);
    }
    havetrans = 1;

    rc = xbsa_ReadObjectBegin(&butxInfo, (char *)&volHeader,
			      sizeof(volHeader), &bytesRead, &endData);
    if (restoretofile && (bytesRead > 0)) {
	fwrite((char *)&volHeader, bytesRead, 1, restoretofilefd);	/* Save to a file */
    }
    if (rc != XBSA_SUCCESS) {
	ELog(taskId,
	     "Unable to begin reading of the volume from the server\n");
	ERROR_EXIT(rc);
    }
    startread = 1;

    if ((bytesRead != sizeof(volHeader)) || endData) {
	ELog(taskId,
	     "The size of data read (%d) does not equal the size of data requested (%d)\n",
	     bytesRead, sizeof(volHeader));
	ERROR_EXIT(TC_BADVOLHEADER);
    }

    /* convert and check the volume header */
    rc = VolHeaderToHost(&hostVolHeader, &volHeader);
    if (rc) {
	ErrorLog(0, taskId, code, 0, "Can't convert volume header\n");
	ERROR_EXIT(rc);
    }

    if ((strcmp(hostVolHeader.volumeName, restoreInfo->oldName) != 0)
	|| (restoreInfo->origVid
	    && (hostVolHeader.volumeID != restoreInfo->origVid))
	|| (hostVolHeader.magic != TC_VOLBEGINMAGIC))
	ERROR_EXIT(TC_BADVOLHEADER);

    /* Set up prior restoring volume data */
    newVolName = restoreInfo->newName;
    newVolId = restoreInfo->vid;
    newServer = restoreInfo->hostAddr;
    newPart = restoreInfo->partition;
    restoreflags = 0;
    if ((restoreInfo->dumpLevel == 0)
	|| (restoreInfo->flags & RDFLAG_FIRSTDUMP))
	restoreflags |= RV_FULLRST;
    if (!(restoreInfo->flags & RDFLAG_LASTDUMP))
	restoreflags |= RV_OFFLINE;

    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    /* Start the restore of the volume data. This is the code we want to return */
    code =
	UV_RestoreVolume(htonl(newServer), newPart, newVolId, newVolName,
			 restoreflags, xbsaRestoreVolumeData,
			 (char *)rparamsPtr);
  error_exit:
    if (startread) {
	rc = xbsa_ReadObjectEnd(&butxInfo);
	if (rc != XBSA_SUCCESS) {
	    ELog(taskId,
		 "Unable to terminate reading of the volume from the server\n");
	    ERROR_EXIT(rc);
	}
    }

    if (havetrans) {
	rc = xbsa_EndTrans(&butxInfo);
	if (rc != XBSA_SUCCESS) {
	    ELog(taskId, "Unable to terminate the current transaction\n");
	    if (!code)
		code = rc;
	}
    }

    if (restoretofile && restoretofilefd) {
	fclose(restoretofilefd);
    }
#endif
    return (code);
}

restoreVolume(taskId, restoreInfo, rparamsPtr)
     afs_uint32 taskId;
     struct tc_restoreDesc *restoreInfo;
     struct restoreParams *rparamsPtr;
{
    afs_int32 code = 0, rc;
    afs_int32 newServer, newPart, newVolId;
    char *newVolName;
    int restoreflags;
    afs_uint32 tapeID;
    struct butm_tapeInfo *tapeInfoPtr = rparamsPtr->tapeInfoPtr;

    /* Check if we need a tape and prompt for one if so */
    tapeID =
	(restoreInfo->initialDumpId ? restoreInfo->
	 initialDumpId : restoreInfo->dbDumpId);
    if ((rparamsPtr->frag == 0)
	|| (strcmp(restoreInfo->tapeName, rparamsPtr->mntTapeName) != 0)
	|| (tapeID != rparamsPtr->tapeID)) {
	/* Unmount the previous tape */
	unmountTape(taskId, tapeInfoPtr);

	/* Remember this new tape */
	strcpy(rparamsPtr->mntTapeName, restoreInfo->tapeName);
	rparamsPtr->tapeID = tapeID;

	/* Mount a new tape */
	rc = GetRestoreTape(taskId, tapeInfoPtr, rparamsPtr->mntTapeName,
			    rparamsPtr->tapeID,
			    ((rparamsPtr->frag == 0) ? autoQuery : 1));
	if (rc)
	    ERROR_EXIT(rc);
    }

    /* Seek to the correct spot and read the header information */
    rc = GetVolumeHead(taskId, tapeInfoPtr, restoreInfo->position,
		       restoreInfo->oldName, restoreInfo->origVid);
    if (rc)
	ERROR_EXIT(rc);

    /* Set up prior restoring volume data */
    newVolName = restoreInfo->newName;
    newVolId = restoreInfo->vid;
    newServer = restoreInfo->hostAddr;
    newPart = restoreInfo->partition;
    restoreflags = 0;
    if ((restoreInfo->dumpLevel == 0)
	|| (restoreInfo->flags & RDFLAG_FIRSTDUMP))
	restoreflags |= RV_FULLRST;
    if (!(restoreInfo->flags & RDFLAG_LASTDUMP))
	restoreflags |= RV_OFFLINE;

    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    /* Start the restore of the volume data. This is the code we
     * want to return.
     */
    code =
	UV_RestoreVolume(htonl(newServer), newPart, newVolId, newVolName,
			 restoreflags, restoreVolumeData, (char *)rparamsPtr);

    /* Read the FileEnd marker for the volume and step to next FM */
    rc = butm_ReadFileEnd(tapeInfoPtr);
    if (rc) {
	ErrorLog(0, taskId, rc, tapeInfoPtr->error,
		 "Can't read EOF on tape\n");
    }

  error_exit:
    return (code);
}

/* Restorer
 *	created as a LWP by the server stub, <newNode> is a pointer to all
 *	the parameters Restorer needs
 */
Restorer(newNode)
     struct dumpNode *newNode;
{
    afs_int32 code = 0, tcode;
    afs_uint32 taskId;
    char *newVolName;
    struct butm_tapeInfo tapeInfo;
    struct tc_restoreDesc *Restore;
    struct tc_restoreDesc *RestoreDesc;
    struct restoreParams rparams;
    afs_int32 allocbufferSize;
    time_t startTime, endTime;
    afs_int32 goodrestore = 0;

    taskId = newNode->taskID;
    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);
    clearStatus(taskId, DRIVE_WAIT);

    printf("\n\n");
    TLog(taskId, "Restore\n");

    memset(&tapeInfo, 0, sizeof(tapeInfo));
    if (!CONF_XBSA) {
	tapeInfo.structVersion = BUTM_MAJORVERSION;
	tcode = butm_file_Instantiate(&tapeInfo, &globalTapeConfig);
	if (tcode) {
	    ErrorLog(0, taskId, tcode, tapeInfo.error,
		     "Can't initialize the tape module\n");
	    ERROR_EXIT(tcode);
	}
    }

    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    memset(&rparams, 0, sizeof(rparams));
    rparams.nodePtr = newNode;
    rparams.tapeInfoPtr = &tapeInfo;
    Restore = newNode->restores;	/* Array of vol fragments to restore */

    /* Allocate memory in which to restore the volumes data into */
    if (CONF_XBSA) {
	allocbufferSize = dataSize = BufferSize;
    } else {
	/* Must have at least two tape blocks */
	tapeblocks = BufferSize / BUTM_BLOCKSIZE;
	if (tapeblocks < 2)
	    tapeblocks = 2;
	allocbufferSize = tapeblocks * BUTM_BLOCKSIZE;	/* This many full tapeblocks */
    }
    bufferBlock = NULL;
    bufferBlock = (struct TapeBlock *)malloc(allocbufferSize);
    if (!bufferBlock)
	ERROR_EXIT(TC_NOMEMORY);
    memset(bufferBlock, 0, allocbufferSize);

    startTime = time(0);
    for (rparams.frag = 0; (rparams.frag < newNode->arraySize);
	 rparams.frag++) {
	RestoreDesc = &Restore[rparams.frag];

	/* Skip the volume if it was requested to */
	if (RestoreDesc->flags & RDFLAG_SKIP) {
	    if (RestoreDesc->flags & RDFLAG_LASTDUMP) {
		/* If the volume was restored, should bring it online */
	    }
	    continue;
	}

	newVolName = RestoreDesc->newName;

	/* Make sure the server to restore to is good */
	if (!RestoreDesc->hostAddr) {
	    ErrorLog(0, taskId, 0, 0, "Illegal host ID 0 for volume %s\n",
		     newVolName);
	    ERROR_EXIT(TC_INTERNALERROR);
	}

	if (checkAbortByTaskId(taskId))
	    ERROR_EXIT(TC_ABORTEDBYREQUEST);

	TapeLog(1, taskId, 0, 0, "Restoring volume %s\n", newVolName);
	lock_Status();
	strncpy(newNode->statusNodePtr->volumeName, newVolName,
		BU_MAXNAMELEN);
	unlock_Status();

	/* restoreVolume function takes care of all the related fragments
	 * spanning various tapes. On return the complete volume has been
	 * restored 
	 */
	if (CONF_XBSA) {
	    tcode = xbsaRestoreVolume(taskId, RestoreDesc, &rparams);
	} else {
	    tcode = restoreVolume(taskId, RestoreDesc, &rparams);
	}
	if (tcode) {
	    if (tcode == TC_ABORTEDBYREQUEST) {
		ERROR_EXIT(tcode);
	    } else if (tcode == TC_SKIPTAPE) {
		afs_uint32 tapeID;
		tapeID =
		    (RestoreDesc->initialDumpId ? RestoreDesc->
		     initialDumpId : RestoreDesc->dbDumpId);
		SkipTape(Restore, newNode->arraySize, rparams.frag,
			 RestoreDesc->tapeName, tapeID, taskId);
	    } else {
		ErrorLog(0, taskId, tcode, 0, "Can't restore volume %s\n",
			 newVolName);
		SkipVolume(Restore, newNode->arraySize, rparams.frag,
			   RestoreDesc->origVid, taskId);
	    }
	    rparams.frag--;
	    continue;
	}

	goodrestore++;
    }

  error_exit:
    endTime = time(0);
    if (!CONF_XBSA) {
	unmountTape(taskId, &tapeInfo);
    } else {
#ifdef xbsa
	code = InitToServer(taskId, &butxInfo, 0);	/* Return to original server */
#endif
    }

    if (bufferBlock)
	free(bufferBlock);

    if (code == TC_ABORTEDBYREQUEST) {
	ErrorLog(0, taskId, 0, 0, "Restore: Aborted by request\n");
	clearStatus(taskId, ABORT_REQUEST);
	setStatus(taskId, ABORT_DONE);
    } else if (code) {
	TapeLog(0, taskId, code, 0, "Restore: Finished with errors\n");
	setStatus(taskId, TASK_ERROR);
    } else {
	TLog(taskId, "Restore: Finished\n");
    }

    if (centralLogIO && startTime) {
	long timediff;
	afs_int32 hrs, min, sec, tmp;
	char line[1024];
	struct tm tmstart, tmend;

	localtime_r(&startTime, &tmstart);
	localtime_r(&endTime, &tmend);
	timediff = (int)endTime - (int)startTime;
	hrs = timediff / 3600;
	tmp = timediff % 3600;
	min = tmp / 60;
	sec = tmp % 60;

	sprintf(line,
		"%-5d  %02d/%02d/%04d %02d:%02d:%02d  "
		"%02d/%02d/%04d %02d:%02d:%02d  " "%02d:%02d:%02d  "
		"%d of %d volume%s restored\n", taskId, tmstart.tm_mon + 1,
		tmstart.tm_mday, tmstart.tm_year + 1900, tmstart.tm_hour,
		tmstart.tm_min, tmstart.tm_sec, tmend.tm_mon + 1,
		tmend.tm_mday, tmend.tm_year + 1900, tmend.tm_hour,
		tmend.tm_min, tmend.tm_sec, hrs, min, sec, goodrestore,
		newNode->arraySize, ((newNode->arraySize > 1) ? "s" : ""));

	fwrite(line, strlen(line), 1, centralLogIO);
	fflush(centralLogIO);
    }

    setStatus(taskId, TASK_DONE);

    FreeNode(taskId);
    LeaveDeviceQueue(deviceLatch);
    return (code);
}

/* this is just scaffolding, creates new tape label with name <tapeName> */

GetNewLabel(tapeInfoPtr, pName, AFSName, tapeLabel)
     struct butm_tapeInfo *tapeInfoPtr;
     char *pName, *AFSName;
     struct butm_tapeLabel *tapeLabel;
{
    struct timeval tp;
    struct timezone tzp;
    afs_uint32 size;

    memset(tapeLabel, 0, sizeof(struct butm_tapeLabel));

    if (!CONF_XBSA) {
	butm_GetSize(tapeInfoPtr, &size);
	if (!size)
	    size = globalTapeConfig.capacity;
    } else {
	size = 0;		/* no tape size */
    }
    gettimeofday(&tp, &tzp);

    tapeLabel->structVersion = CUR_TAPE_VERSION;
    tapeLabel->creationTime = tp.tv_sec;
    tapeLabel->size = size;
    tapeLabel->expirationDate = 0;	/* 1970 sometime */
    tapeLabel->dumpPath[0] = 0;	/* no path name  */
    tapeLabel->useCount = 0;
    strcpy(tapeLabel->AFSName, AFSName);
    strcpy(tapeLabel->pName, pName);
    strcpy(tapeLabel->cell, globalCellName);
    strcpy(tapeLabel->comment, "AFS Backup Software");
    strcpy(tapeLabel->creator.name, "AFS 3.6");
    strcpy(tapeLabel->creator.instance, "");
    strcpy(tapeLabel->creator.cell, globalCellName);
}

/* extracts trailer out of buffer, nbytes is set to total data in buffer - trailer size */
afs_int32
ExtractTrailer(buffer, size, nbytes, volTrailerPtr)
     char *buffer;
     afs_int32 *nbytes;
     afs_int32 size;
     struct volumeHeader *volTrailerPtr;
{
    afs_int32 code = 0;
    afs_int32 startPos;
    struct volumeHeader tempTrailer;

    for (startPos = 0;
	 startPos <=
	 (size - sizeof(struct volumeHeader) + sizeof(tempTrailer.pad));
	 startPos++) {
	code = readVolumeHeader(buffer, startPos, &tempTrailer);
	if (code == 0) {
	    code = VolHeaderToHost(volTrailerPtr, &tempTrailer);
	    if (code)
		break;

	    if (nbytes)
		*nbytes = startPos;
	    return 1;		/* saw the trailer */
	}
    }

    if (nbytes)
	*nbytes = size / 2;
    return 0;			/* did not see the trailer */
}

int
FindVolTrailer(buffer, size, dSize, volTrailerPtr)
     char *buffer;
     afs_int32 size;
     struct volumeHeader *volTrailerPtr;
     afs_int32 *dSize;		/* dataSize */
{
    afs_int32 offset, s;
    int found;

    *dSize = size;
    if (!buffer)
	return 0;

    s = sizeof(struct volumeHeader) + sizeof(volTrailerPtr->pad);
    if (s > size)
	s = size;

    found = ExtractTrailer((buffer + size - s), s, &offset, volTrailerPtr);
    if (found)
	*dSize -= (s - offset);
    return found;
}

int
FindVolTrailer2(buffera, sizea, dataSizea, bufferb, sizeb, dataSizeb,
		volTrailerPtr)
     char *buffera;
     afs_int32 sizea;
     afs_int32 *dataSizea;
     char *bufferb;
     afs_int32 sizeb;
     afs_int32 *dataSizeb;
     struct volumeHeader *volTrailerPtr;
{
    afs_int32 offset, s;
    afs_int32 headB, tailB;
    int found = 0;

    if (!buffera)
	sizea = 0;
    if (!bufferb)
	sizeb = 0;
    *dataSizea = sizea;
    *dataSizeb = sizeb;

    s = sizeof(struct volumeHeader) + sizeof(volTrailerPtr->pad);
    if (sizeb >= s) {
	found = FindVolTrailer(bufferb, sizeb, dataSizeb, volTrailerPtr);
    } else {
	tailB = sizeb;
	headB = (s - sizeb);	/*(s > sizeb) */
	if (headB > sizea) {
	    headB = sizea;
	    s = headB + tailB;
	    if (!s)
		return 0;
	}

	memset(tapeVolumeHT, 0, sizeof(tapeVolumeHT));
	if (headB)
	    memcpy(tapeVolumeHT, buffera + sizea - headB, headB);
	if (tailB)
	    memcpy(tapeVolumeHT + headB, bufferb, tailB);
	if (ExtractTrailer(tapeVolumeHT, s, &offset, volTrailerPtr)) {
	    found = 1;
	    if (offset > headB) {
		/* *dataSizea remains unchanged */
		*dataSizeb = offset - headB;
	    } else {
		*dataSizea -= (headB - offset);	/*(headB >= offset) */
		*dataSizeb = 0;
	    }
	}
    }
    return found;
}

/* Returns true or false depending on whether the tape is expired or not */

Date
ExpirationDate(dumpid)
     afs_int32 dumpid;
{
    afs_int32 code;
    Date expiration = 0;
    struct budb_dumpEntry dumpEntry;
    struct budb_tapeEntry tapeEntry;
    struct budb_volumeEntry volEntry;

    if (dumpid) {
	/*
	 * Get the expiration date from DB if its there. The expiration of any tape
	 * will be the most future expiration of any dump in the set. Can't use
	 * bcdb_FindTape because dumpid here pertains to the initial dump id.
	 */
	code = bcdb_FindLastTape(dumpid, &dumpEntry, &tapeEntry, &volEntry);
	if (!code)
	    expiration = tapeEntry.expires;
    }
    return (expiration);
}

int
tapeExpired(tapeLabelPtr)
     struct butm_tapeLabel *tapeLabelPtr;
{
    Date expiration;
    struct timeval tp;
    struct timezone tzp;

    expiration = ExpirationDate(tapeLabelPtr->dumpid);
    if (!expiration)
	expiration = tapeLabelPtr->expirationDate;

    gettimeofday(&tp, &tzp);
    return ((expiration < tp.tv_sec) ? 1 : 0);
}

/* updateTapeLabel
 *	given the label on the tape, delete any old information from the
 *	database. 

 * Deletes all entries that match the volset.dumpnode
 *	and the dump path.
 */

updateTapeLabel(labelIfPtr, tapeInfoPtr, newLabelPtr)
     struct labelTapeIf *labelIfPtr;
     struct butm_tapeInfo *tapeInfoPtr;
     struct butm_tapeLabel *newLabelPtr;
{
    struct butm_tapeLabel oldLabel;
    afs_int32 i, code = 0;
    afs_uint32 taskId;
    int tapeIsLabeled = 0;
    int interactiveFlag;
    int tapecount = 1;

    interactiveFlag = autoQuery;
    taskId = labelIfPtr->taskId;

    while (1) {
	if (interactiveFlag) {
	    code =
		PromptForTape(LABELOPCODE, TNAME(newLabelPtr), 0,
			      labelIfPtr->taskId, tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	interactiveFlag = 1;
	tapecount++;

	/* mount the tape */
	code = butm_Mount(tapeInfoPtr, newLabelPtr->AFSName);
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error, "Can't open tape\n");
	    goto newtape;
	}

	code = butm_ReadLabel(tapeInfoPtr, &oldLabel, 1);	/* will rewind the tape */
	if (!code) {
	    tapeIsLabeled = 1;

	    if ((strcmp(newLabelPtr->AFSName, "") != 0)
		&& (strcmp(oldLabel.pName, "") != 0)) {
		/* We are setting the AFS name, yet tape 
		 * has a permanent name (not allowed).
		 */
		TLog(taskId, "Can't label. Tape has permanent label '%s'\n",
		     oldLabel.pName);
		goto newtape;
	    }

	    if (!tapeExpired(&oldLabel)) {
		if (!queryoperator) {
		    TLog(taskId, "This tape has not expired\n");
		    goto newtape;
		}
		if (Ask("This tape has not expired - proceed") == 0)
		    goto newtape;
	    }

	    /* Keep the permanent name */
	    if (strcmp(newLabelPtr->pName, "") == 0) {
		strcpy(newLabelPtr->pName, oldLabel.pName);
	    } else if (strcmp(newLabelPtr->pName, TC_NULLTAPENAME) == 0) {
		strcpy(newLabelPtr->pName, "");
	    }
	}

	/* extract useful information from the old label */
	if (tapeIsLabeled && oldLabel.structVersion >= TAPE_VERSION_3) {
	    newLabelPtr->dumpid = 0;
	    newLabelPtr->useCount = oldLabel.useCount + 1;
	}

	/* now write the new label */
	code = butm_Create(tapeInfoPtr, newLabelPtr, 1);	/* will rewind the tape */
	if (code) {
	    ErrorLog(0, taskId, code, tapeInfoPtr->error,
		     "Can't label tape\n");
	    goto newtape;
	}

	break;

      newtape:
	unmountTape(taskId, tapeInfoPtr);
    }

    /* delete obsolete information from the database */
    if (tapeIsLabeled && oldLabel.structVersion >= TAPE_VERSION_3) {
	/* delete based on dump id */
	if (oldLabel.dumpid) {
	    i = bcdb_deleteDump(oldLabel.dumpid, 0, 0, 0);
	    if (i && (i != BUDB_NOENT))
		ErrorLog(0, taskId, i, 0,
			 "Warning: Can't delete old dump %u from database\n",
			 oldLabel.dumpid);
	}
    }

  error_exit:
    unmountTape(taskId, tapeInfoPtr);
    return (code);
}

/* Labeller
 *	LWP created by the server stub. Labels the tape with name and size
 *	specified by <label>
 */

Labeller(labelIfPtr)
     struct labelTapeIf *labelIfPtr;
{
    struct tc_tapeLabel *label = &labelIfPtr->label;

    struct butm_tapeLabel newTapeLabel;
    struct butm_tapeInfo tapeInfo;
    afs_uint32 taskId;
    afs_int32 code = 0;

    taskId = labelIfPtr->taskId;
    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);
    clearStatus(taskId, DRIVE_WAIT);

    printf("\n\n");
    TLog(taskId, "Labeltape\n");

    memset(&tapeInfo, 0, sizeof(tapeInfo));
    tapeInfo.structVersion = BUTM_MAJORVERSION;
    code = butm_file_Instantiate(&tapeInfo, &globalTapeConfig);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfo.error,
		 "Can't initialize the tape module\n");
	ERROR_EXIT(code);
    }

    GetNewLabel(&tapeInfo, label->pname, label->afsname, &newTapeLabel);
    if (label->size)
	newTapeLabel.size = label->size;
    else
	newTapeLabel.size = globalTapeConfig.capacity;

    code = updateTapeLabel(labelIfPtr, &tapeInfo, &newTapeLabel);
    if (code)
	ERROR_EXIT(code);

  error_exit:
    if (code == TC_ABORTEDBYREQUEST) {
	ErrorLog(0, taskId, 0, 0, "Labeltape: Aborted by request\n");
	clearStatus(taskId, ABORT_REQUEST);
	setStatus(taskId, ABORT_DONE);
    } else if (code) {
	ErrorLog(0, taskId, code, 0, "Labeltape: Finished with errors\n");
	setStatus(taskId, TASK_ERROR);
    } else {
	TLog(taskId, "Labelled tape %s size %u Kbytes\n",
	     TNAME(&newTapeLabel), newTapeLabel.size);
    }
    setStatus(labelIfPtr->taskId, TASK_DONE);

    free(labelIfPtr);
    LeaveDeviceQueue(deviceLatch);
    return (code);
}

/* PrintTapeLabel
 *	print out the tape label.
 */

PrintTapeLabel(labelptr)
     struct butm_tapeLabel *labelptr;
{
    char tapeName[BU_MAXTAPELEN + 32];
    time_t t;

    printf("Tape label\n");
    printf("----------\n");
    TAPENAME(tapeName, labelptr->pName, labelptr->dumpid);
    printf("permanent tape name = %s\n", tapeName);
    TAPENAME(tapeName, labelptr->AFSName, labelptr->dumpid);
    printf("AFS tape name = %s\n", tapeName);
    t = labelptr->creationTime;
    printf("creationTime = %s", ctime(&t));
    if (labelptr->expirationDate) {
        t = labelptr->expirationDate;
	printf("expirationDate = %s", cTIME(&t));
    }
    printf("cell = %s\n", labelptr->cell);
    printf("size = %u Kbytes\n", labelptr->size);
    printf("dump path = %s\n", labelptr->dumpPath);

    if (labelptr->structVersion >= TAPE_VERSION_3) {
	printf("dump id = %u\n", labelptr->dumpid);
	printf("useCount = %d\n", labelptr->useCount);
    }
    printf("-- End of tape label --\n\n");
}

/* ReadLabel 
 * 	Read the label from a tape.
 *	Currently prints out a "detailed" summary of the label but passes
 *	back only selected fields.
 */

ReadLabel(label)
     struct tc_tapeLabel *label;
{
    struct butm_tapeLabel newTapeLabel;
    struct butm_tapeInfo tapeInfo;
    afs_uint32 taskId;
    Date expir;
    afs_int32 code = 0;
    int interactiveFlag;
    int tapecount = 1;

    EnterDeviceQueue(deviceLatch);
    taskId = allocTaskId();	/* reqd for lower level rtns */

    printf("\n\n");
    TLog(taskId, "Readlabel\n");

    memset(&tapeInfo, 0, sizeof(tapeInfo));
    tapeInfo.structVersion = BUTM_MAJORVERSION;
    code = butm_file_Instantiate(&tapeInfo, &globalTapeConfig);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfo.error,
		 "Can't initialize the tape module\n");
	ERROR_EXIT(code);
    }
    memset(&newTapeLabel, 0, sizeof(newTapeLabel));

    interactiveFlag = autoQuery;

    while (1) {
	if (interactiveFlag) {
	    code = PromptForTape(READLABELOPCODE, "", 0, taskId, tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	interactiveFlag = 1;
	tapecount++;

	code = butm_Mount(&tapeInfo, "");
	if (code) {
	    TapeLog(0, taskId, code, tapeInfo.error, "Can't open tape\n");
	    goto newtape;
	}
	break;

      newtape:
	unmountTape(taskId, &tapeInfo);
    }

    code = butm_ReadLabel(&tapeInfo, &newTapeLabel, 1);	/* will rewind the tape */
    if (code) {
	if (code == BUTM_NOLABEL) {
	    printf("Tape is unlabelled\n");
	    ERROR_EXIT(code);
	}
	ErrorLog(1, taskId, code, tapeInfo.error, "Can't read tape label\n");
	ERROR_EXIT(code);
    }

    /* copy the fields to be passed to the caller */
    label->size = newTapeLabel.size;
    label->tapeId = newTapeLabel.dumpid;
    strcpy(label->afsname, newTapeLabel.AFSName);
    strcpy(label->pname, newTapeLabel.pName);


    expir = ExpirationDate(newTapeLabel.dumpid);
    if (expir)
	newTapeLabel.expirationDate = expir;

    PrintTapeLabel(&newTapeLabel);

  error_exit:
    unmountTape(taskId, &tapeInfo);

    if (code == TC_ABORTEDBYREQUEST)
	ErrorLog(0, taskId, 0, 0, "ReadLabel: Aborted by request\n");
    else if (code && (code != BUTM_NOLABEL))
	ErrorLog(0, taskId, code, 0, "ReadLabel: Finished with errors\n");
    else
	TLog(taskId, "ReadLabel: Finished\n");

    LeaveDeviceQueue(deviceLatch);
    return (code);
}

/* Function to read volume header and trailer structure from tape, taking
   into consideration, different word alignment rules.
*/
afs_int32
readVolumeHeader(buffer, bufloc, header)
     /*in */
     char *buffer;		/* buffer to read header from */
				/*in */ afs_int32 bufloc;
				/* header's location in buffer */
						/*out */ struct volumeHeader *header;
						/* header structure */

{
    struct volumeHeader vhptr, *tempvhptr;
    afs_int32 firstSplice = (afs_int32) ((char*)& vhptr.pad - (char*) & vhptr);
    int padLen = sizeof(vhptr.pad);	/* pad to achieve 4 byte alignment */
    int nextSplice = sizeof(struct volumeHeader) - firstSplice - padLen;

    /* Four cases are to be handled
     * 
     * Volume Header (byte alignment)
     * -----------------------
     * Tape   In Core
     * ----   -------
     * Case 1:  4       1
     * Case 2:  4       4
     * Case 3:  1       1
     * Case 4:  1       4
     * -----------------------
     * 
     * Case 2 and Case 3 are identical cases and handled the same way.
     * Case 1 and Case 4 are separate cases. In one case the pad needs
     * to be removed and in the other, it needs to be spliced in. The
     * four cases are handled as follows
     */
    tempvhptr = (struct volumeHeader *)(buffer + bufloc);
    if ((strncmp(tempvhptr->preamble, "H++NAME#", 8) == 0)
	&& (strncmp(tempvhptr->postamble, "T--NAME#", 8) == 0)) {
	/* Handle Cases 2 & 3 */
	memcpy(&vhptr, buffer + bufloc, sizeof(struct volumeHeader));
	HEADER_CHECKS(vhptr, header);

	/* Handle Case 4 */
	memset(&vhptr, 0, sizeof(struct volumeHeader));
	memcpy(&vhptr, buffer + bufloc, firstSplice);
	memset(&vhptr.pad, 0, padLen);
	memcpy(&vhptr.volumeID, buffer + bufloc + firstSplice, nextSplice);
	HEADER_CHECKS(vhptr, header);

	/* Handle Case 1 */
	memset(&vhptr, 0, sizeof(struct volumeHeader));
	memcpy(&vhptr, buffer + bufloc, firstSplice);
	memcpy(&vhptr + firstSplice, buffer + bufloc + firstSplice + padLen,
	       nextSplice);
	HEADER_CHECKS(vhptr, header);

    }
    return (TC_BADVOLHEADER);
}
