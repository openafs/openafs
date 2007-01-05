/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/stds.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <afs/com_err.h>
#include <afs/bubasics.h>
#include <lock.h>
#include <afs/tcdata.h>
#include "bc.h"
#include "error_macros.h"

#define	SET_FLAG(set)				\
    lock_Status();				\
    curPollPtr->flags |= (set);			\
    unlock_Status();

#define CLEAR_FLAG(clear) 			\
    lock_Status();				\
    curPollPtr->flags &= ~(clear);		\
    unlock_Status();

extern struct bc_config *bc_globalConfig;
extern afs_int32 bc_GetConn(struct bc_config *aconfig, afs_int32 aport, struct rx_connection **tconn);
extern statusP findStatus(afs_uint32 taskId);

/* globals for backup coordinator status management */

dlqlinkT statusHead;		/* chain of status blocks */
struct Lock statusQueueLock;	/* access control for status chain */
struct Lock cmdLineLock;	/* lock on the cmdLine */

afs_int32 lastTaskCode;		/* Error code from task that last finished */

/* nextItem
 *	get next item for status interrogation, if any.
 */
static statusP
nextItem(linkPtr)
     statusP linkPtr;
{
    dlqlinkP ptr;

    ptr = (dlqlinkP) linkPtr;

    /* if last known item has terminated, reset ptr */
    if (ptr == 0) {
	ptr = &statusHead;
	if (dlqEmpty(ptr))
	    return (0);
    }

    ptr = ptr->dlq_next;

    /* if we're back at the head again */
    if (ptr == &statusHead)
	return (0);
    return ((statusP) ptr);
}

#ifdef notdef
static statusP
nextItem(linkPtr)
     statusP linkPtr;
{
    dlqlinkP ptr;

    ptr = (dlqlinkP) linkPtr;

    /* if last known item has terminated, reset ptr */
    if (ptr == 0) {
	ptr = &statusHead;
	if (dlqEmpty(ptr))
	    return (0);
    }

    ptr = ptr->dlq_next;

    /* if we're back at the head again */
    if (ptr == &statusHead) {
	ptr = ptr->dlq_next;
    }
    return ((statusP) ptr);
}
#endif /* notdef */

char *cmdLine;

int
cmdDispatch()
{
#define	MAXV	100
    char **targv[MAXV];		/*Ptr to parsed argv stuff */
    afs_int32 targc;		/*Num parsed arguments */
    afs_int32 code;
    char *internalCmdLine;

    internalCmdLine = cmdLine;
    unlock_cmdLine();

    code = cmd_ParseLine(internalCmdLine, targv, &targc, MAXV);
    if (code) {
	printf("Couldn't parse line: '%s'", error_message(code));
	return (1);
    }
    free(internalCmdLine);

    /*
     * Because the "-at" option cannot be wildcarded, we cannot fall
     * into recusive loop here by setting dispatchCount to 1.
     */
    doDispatch(targc, targv, 1);
    cmd_FreeArgv(targv);
    return(0);
}

statusWatcher()
{
    struct rx_connection *tconn = (struct rc_connection *)0;
    statusP curPollPtr = 0;

    struct tciStatusS statusPtr;

    /* task information */
    afs_uint32 taskFlags;
    afs_uint32 localTaskFlags;
    afs_uint32 temp;		/* for flag manipulation */
    afs_int32 jobNumber;
    afs_int32 taskId;
    afs_int32 port;
    afs_int32 atTime;
    PROCESS dispatchPid;

    afs_int32 code = 0;

    lastTaskCode = 0;

    while (1) {			/*w */
	if (tconn)
	    rx_DestroyConnection(tconn);
	tconn = (struct rc_connection *)0;

	lock_Status();
	curPollPtr = nextItem(curPollPtr);

	if (curPollPtr == 0) {
#ifdef AFS_PTHREAD_ENV
	    struct timespec delaytime;
	    unlock_Status();
	    delayTime.tv_sec = 5;
	    delayTime.tv_nsec = 0;
	    pthread_delay_np(&delayTime);
#else
	    unlock_Status();
	    IOMGR_Sleep(5);	/* wait a while */
#endif /*else AFS_PTHREAD_ENV */
	    continue;
	}

	/* save useful information */
	localTaskFlags = curPollPtr->flags;
	taskId = curPollPtr->taskId;
	port = curPollPtr->port;
	atTime = curPollPtr->scheduledDump;
	jobNumber = curPollPtr->jobNumber;
	unlock_Status();

	/* reset certain flags; local kill; */
	CLEAR_FLAG(ABORT_LOCAL);

	/* An abort request before the command even started */
	if (atTime && (localTaskFlags & ABORT_REQUEST)) {
	    if (localTaskFlags & NOREMOVE) {
		curPollPtr->flags |= (STARTING | ABORT_DONE);	/* Will ignore on other passes */
		curPollPtr->scheduledDump = 0;
	    } else {
		deleteStatusNode(curPollPtr);
	    }
	    curPollPtr = 0;
	    continue;
	}

	/* A task not started yet - check its start time */
	if (localTaskFlags & STARTING || atTime) {
	    /*
	     * Start a timed dump if its time has come.  When the job is 
	     * started, it will allocate its own status structure so this 
	     * one is no longer needed: delete it. 
	     *
	     * Avoid multiple processes trouncing the cmdLine by placing 
	     * lock around it.
	     */
	    if (atTime && (atTime <= time(0))) {
		lock_cmdLine();	/* Will unlock in cmdDispatch */

		cmdLine = curPollPtr->cmdLine;
		lock_Status();
		curPollPtr->cmdLine = 0;
		unlock_Status();

		printf("Starting scheduled dump: job %d\n", jobNumber);
		printf("schedD> %s\n", cmdLine);

		code =
		    LWP_CreateProcess(cmdDispatch, 16384, LWP_NORMAL_PRIORITY,
				      (void *)2, "cmdDispatch", &dispatchPid);
		if (code) {
		    if (cmdLine)
			free(cmdLine);
		    unlock_cmdLine();
		    printf("Couldn't create cmdDispatch task\n");
		}

		if (localTaskFlags & NOREMOVE) {
		    curPollPtr->flags |= STARTING;	/* Will ignore on other passes */
		    curPollPtr->flags |= (code ? TASK_ERROR : TASK_DONE);
		    curPollPtr->scheduledDump = 0;
		} else {
		    deleteStatusNode(curPollPtr);
		}
		curPollPtr = 0;
	    }
	    continue;
	}

	if (localTaskFlags & ABORT_LOCAL) {
	    /* kill the local task */
	    if ((localTaskFlags & CONTACT_LOST) != 0) {
		printf("Job %d: in contact with butc at port %d\n", jobNumber,
		       port);
		printf("Job %d cont: Local kill ignored - use normal kill\n",
		       jobNumber);
	    }
	}

	code = (afs_int32) bc_GetConn(bc_globalConfig, port, &tconn);
	if (code) {
	    SET_FLAG(CONTACT_LOST);
	    continue;
	}

	if (CheckTCVersion(tconn)) {
	    SET_FLAG(CONTACT_LOST);
	    continue;
	}

	/* Send abort to TC requst if we have to */
	if (localTaskFlags & ABORT_REQUEST) {
	    code = TC_RequestAbort(tconn, taskId);
	    if (code) {
		com_err("statusWatcher", code, "; Can't post abort request");
		com_err("statusWatcher", 0, "...Deleting job");
		if (localTaskFlags & NOREMOVE) {
		    curPollPtr->flags |= (STARTING | TASK_ERROR);
		    curPollPtr->scheduledDump = 0;
		} else {
		    deleteStatusNode(curPollPtr);
		}
		curPollPtr = 0;
		continue;
	    } else {
		lock_Status();
		curPollPtr->flags &= ~ABORT_REQUEST;
		curPollPtr->flags |= ABORT_SENT;
		unlock_Status();
	    }
	}

	/* otherwise just get the status */
	code = TC_GetStatus(tconn, taskId, &statusPtr);
	if (code) {
	    if (code == TC_NODENOTFOUND) {
		printf("Job %d: %s - no such task on port %d, deleting\n",
		       jobNumber, curPollPtr->taskName, port);

		if (localTaskFlags & NOREMOVE) {
		    curPollPtr->flags |= (STARTING | TASK_ERROR);
		    curPollPtr->scheduledDump = 0;
		} else {
		    deleteStatusNode(curPollPtr);	/* delete this status node */
		}
		curPollPtr = 0;
		continue;
	    }

	    SET_FLAG(CONTACT_LOST);
	    continue;
	}

	/* in case we previously lost contact or couldn't find */
	CLEAR_FLAG(CONTACT_LOST);

	/* extract useful status */
	taskFlags = statusPtr.flags;

	/* update local status */
	lock_Status();

	/* remember some status flags in local struct */
	temp =
	    (DRIVE_WAIT | OPR_WAIT | CALL_WAIT | TASK_DONE | ABORT_DONE |
	     TASK_ERROR);
	curPollPtr->flags &= ~temp;	/* clear */
	curPollPtr->flags |= (taskFlags & temp);	/* update */

	curPollPtr->dbDumpId = statusPtr.dbDumpId;
	curPollPtr->nKBytes = statusPtr.nKBytes;
	strcpy(curPollPtr->volumeName, statusPtr.volumeName);
	curPollPtr->volsFailed = statusPtr.volsFailed;
	curPollPtr->lastPolled = statusPtr.lastPolled;
	unlock_Status();

	/* Are we done */
	if (taskFlags & TASK_DONE) {	/*done */
	    if (taskFlags & ABORT_DONE) {
		if (curPollPtr->dbDumpId)
		    printf("Job %d: %s: DumpID %u Aborted", jobNumber,
			   curPollPtr->taskName, curPollPtr->dbDumpId);
		else
		    printf("Job %d: %s Aborted", jobNumber,
			   curPollPtr->taskName);

		if (taskFlags & TASK_ERROR)
		    printf(" with errors\n");
		else
		    printf("\n");

		lastTaskCode = 1;
	    }

	    else if (taskFlags & TASK_ERROR) {
		if (!(localTaskFlags & SILENT)) {
		    if (curPollPtr->dbDumpId)
			printf("Job %d: DumpID %u Failed with errors\n",
			       jobNumber, curPollPtr->dbDumpId);
		    else
			printf("Job %d Failed with errors\n", jobNumber);
		}
		lastTaskCode = 2;
	    }

	    else {
		if (!(localTaskFlags & SILENT)) {
		    if (curPollPtr->dbDumpId)
			printf("Job %d: %s: DumpID %u finished", jobNumber,
			       curPollPtr->taskName, curPollPtr->dbDumpId);
		    else
			printf("Job %d: %s finished", jobNumber,
			       curPollPtr->taskName);

		    if (curPollPtr->volsTotal) {
			printf(". %d volumes dumped",
			       (curPollPtr->volsTotal -
				curPollPtr->volsFailed));
			if (curPollPtr->volsFailed)
			    printf(", %d failed", curPollPtr->volsFailed);
		    }

		    printf("\n");
		}
		lastTaskCode = 0;
	    }

	    /* make call to destroy task on server */
	    code = TC_EndStatus(tconn, taskId);
	    if (code)
		printf("Job %d: %s, error in job termination cleanup\n",
		       jobNumber, curPollPtr->taskName);

	    if (localTaskFlags & NOREMOVE) {
		curPollPtr->flags |= STARTING;
		curPollPtr->scheduledDump = 0;
	    } else {
		deleteStatusNode(curPollPtr);	/* unlink and destroy local task */
	    }
	    curPollPtr = 0;
	}			/*done */
    }				/*w */
}

/* bc_jobNumber
 *	Allocate a job number. Computes the maximum of all the job numbers
 *	and then returns the maximum+1.
 *	If no jobs are found, returns 1.
 */

afs_int32
bc_jobNumber()
{
    afs_int32 retval = 0;
    dlqlinkP ptr;

    ptr = statusHead.dlq_next;
    while (ptr != &statusHead) {
	/* compute max of all job numbers */
	if (((statusP) ptr)->jobNumber > retval)
	    retval = ((statusP) ptr)->jobNumber;

	ptr = ptr->dlq_next;
    }
    retval++;
    return (retval);
}

/* waitForTask
 *    Wait for a specific task to finish and then return.
 *    Return the task's flags when it's done. If the job
 *    had been cleaned up, then just return 0.
 */
waitForTask(taskId)
     afs_uint32 taskId;
{
    statusP ptr;
    afs_int32 done = 0, rcode, t;

    t = (TASK_DONE | ABORT_DONE | TASK_ERROR);
    while (!done) {
	/* Sleep 2 seconds */
#ifdef AFS_PTHREAD_ENV
	struct timespec delaytime;
	delayTime.tv_sec = 2;
	delayTime.tv_nsec = 0;
	pthread_delay_np(&delayTime);
#else
	IOMGR_Sleep(2);
#endif /*else AFS_PTHREAD_ENV */

	/* Check if we are done */
	lock_Status();
	ptr = findStatus(taskId);
	if (!ptr || (ptr->flags & t)) {
	    rcode = (ptr ? ptr->flags : 0);
	    done = 1;
	}
	unlock_Status();
    }
    return rcode;
}
