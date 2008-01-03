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
#else
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#endif
#include <stdio.h>
#include <string.h>
#include <afs/com_err.h>
#include <lock.h>
#include <afs/bubasics.h>
#include <afs/tcdata.h>
#include <afs/butc.h>
#include "error_macros.h"
#include "butc_xbsa.h"

/* tape coordinator - task status management */
extern statusP findStatus();
extern afs_int32 xbsaType;

dlqlinkT statusHead;
struct Lock statusQueueLock;
struct Lock cmdLineLock;

/* STC_GetStatus
 *	get the status of a task
 * entry:
 *	taskId - task for which status required
 * exit:
 *	statusPtr - filled in with task status
 */

afs_int32
STC_GetStatus(call, taskId, statusPtr)
     struct rx_call *call;
     afs_uint32 taskId;
     struct tciStatusS *statusPtr;
{
    statusP ptr;
    int retval = 0;

    if (callPermitted(call) == 0)
	return (TC_NOTPERMITTED);

    lock_Status();
    ptr = findStatus(taskId);
    if (ptr) {
	/* strcpy(statusPtr->status, ptr->status); */

	strcpy(statusPtr->taskName, ptr->taskName);
	strcpy(statusPtr->volumeName, ptr->volumeName);
	statusPtr->taskId = ptr->taskId;
	statusPtr->flags = ptr->flags;
	statusPtr->nKBytes = ptr->nKBytes;
	statusPtr->dbDumpId = ptr->dbDumpId;
	statusPtr->lastPolled = ptr->lastPolled;
	statusPtr->volsFailed = ptr->volsFailed;
	ptr->lastPolled = time(0);
    } else
	retval = TC_NODENOTFOUND;
    unlock_Status();

    return (retval);
}

afs_int32
STC_EndStatus(call, taskId)
     struct rx_call *call;
     afs_uint32 taskId;
{
    statusP ptr;
    int retval = 0;

    if (callPermitted(call) == 0)
	return (TC_NOTPERMITTED);

    lock_Status();
    ptr = findStatus(taskId);
    unlock_Status();

    if (ptr)
	deleteStatusNode(ptr);
    else
	retval = TC_NODENOTFOUND;

    return (retval);
}

afs_int32
STC_RequestAbort(call, taskId)
     struct rx_call *call;
     afs_uint32 taskId;
{
    statusP ptr;
    int retval = 0;

    if (callPermitted(call) == 0)
	return (TC_NOTPERMITTED);

    lock_Status();
    ptr = findStatus(taskId);
    if (ptr)
	ptr->flags |= ABORT_REQUEST;
    else
	retval = TC_NODENOTFOUND;
    unlock_Status();
    return (retval);
}

/* STC_ScanStatus
 *	Get status of all tasks on the butc, successively. Initial call
 *	should come in with TSK_STAT_FIRST flag set to initialize the
 *	scan.
 * entry:
 *	taskId - specifies the task whose status is to be returned
 *		(unless TSK_STAT_FIRST set in which case it is ignored)
 * exit:
 *	taskId - id of next task in the list
 *	flags - TSK_STAT_END will be set when one reaches the end of
 *		the task list. taskId is not updated in this case.
 * return values:
 *	0 - normal
 *	TC_NOTASKS - no tasks active
 */

afs_int32
STC_ScanStatus(call, taskId, statusPtr, flags)
     struct rx_call *call;
     afs_uint32 *taskId;
     struct tciStatusS *statusPtr;
     afs_uint32 *flags;
{
    statusP ptr = 0;
    statusP nextPtr = 0;
    dlqlinkP dlqPtr;

    if (callPermitted(call) == 0)
	return (TC_NOTPERMITTED);

    lock_Status();

    if (CONF_XBSA)
	*flags |= TSK_STAT_XBSA;
    if (xbsaType == XBSA_SERVER_TYPE_ADSM)
	*flags |= TSK_STAT_ADSM;

    if (*flags & TSK_STAT_FIRST) {
	/* find first status node */
	dlqPtr = statusHead.dlq_next;
	if (dlqPtr == &statusHead) {
	    /* no status nodes */
	    *flags |= (TSK_STAT_NOTFOUND | TSK_STAT_END);
	    unlock_Status();
	    return (0);
	}
	ptr = (statusP) dlqPtr;
    } else {
	ptr = findStatus(*taskId);
	if (ptr == 0) {
	    /* in the event that the set of tasks has changed, just
	     * finish, letting the caller retry
	     */

	    *flags |= (TSK_STAT_NOTFOUND | TSK_STAT_END);
	    unlock_Status();
	    return (0);
	}
    }

    /* ptr is now set to the status node we wish to return. Determine
     * what the next node will be
     */

    if (ptr->link.dlq_next == &statusHead)
	*flags |= TSK_STAT_END;
    else
	*taskId = ((statusP) ptr->link.dlq_next)->taskId;

    strcpy(statusPtr->taskName, ptr->taskName);
    strcpy(statusPtr->volumeName, ptr->volumeName);
    statusPtr->taskId = ptr->taskId;
    statusPtr->flags = ptr->flags;
    statusPtr->nKBytes = ptr->nKBytes;
    statusPtr->lastPolled = ptr->lastPolled;

    unlock_Status();
    return (0);
}


/* ---------------------------------
 * misc. status management routines
 * ---------------------------------
 */

/* checkAbortByTaskId
 * exit:
 *	0 - continue
 *	n - abort requested
 */

checkAbortByTaskId(taskId)
     afs_uint32 taskId;
{
    statusP statusPtr;
    int retval = 0;

    extern statusP findStatus();

    lock_Status();
    statusPtr = findStatus(taskId);
    if (statusPtr) {
	retval = statusPtr->flags & ABORT_REQUEST;
    }
    unlock_Status();
    return (retval);
}

/* getStatusFlag
 * 	For backwards compatibility. Queries flag status
 * exit:
 *	0 - flag clear
 *	n - flag set
 */

afs_uint32
getStatusFlag(taskId, flag)
     afs_uint32 taskId;
     afs_uint32 flag;
{
    statusP statusPtr;
    int retval = 0;
    extern statusP findStatus();

    lock_Status();
    statusPtr = findStatus(taskId);
    if (statusPtr) {
	retval = statusPtr->flags & flag;
    }
    unlock_Status();
    return (retval);
}
