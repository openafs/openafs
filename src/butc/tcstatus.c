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

#include <roken.h>

#include <afs/com_err.h>
#include <afs/afs_lock.h>
#include <afs/bubasics.h>
#include <afs/tcdata.h>
#include <afs/butc.h>
#include <afs/budb_client.h>
#include <afs/bucoord_prototypes.h>

#include "butc_internal.h"
#include "error_macros.h"
#include "butc_xbsa.h"
#include "afs/audit.h"

/* tape coordinator - task status management */
extern afs_int32 xbsaType;

dlqlinkT statusHead;
struct Lock statusQueueLock;
struct Lock cmdLineLock;

static afs_int32 SGetStatus(struct rx_call *call, afs_uint32 taskId,
			    struct tciStatusS *statusPtr);
static afs_int32 SEndStatus(struct rx_call *call, afs_uint32 taskId);
static afs_int32 SRequestAbort(struct rx_call *call, afs_uint32 taskId);
static afs_int32 SScanStatus(struct rx_call *call, afs_uint32 *taskId,
			     struct tciStatusS *statusPtr, afs_uint32 *flags);

/* STC_GetStatus
 *	get the status of a task
 * entry:
 *	taskId - task for which status required
 * exit:
 *	statusPtr - filled in with task status
 */

afs_int32
STC_GetStatus(struct rx_call *call, afs_uint32 taskId,
	      struct tciStatusS *status)
{
    afs_int32 code;

    code = SGetStatus(call, taskId, status);
    osi_auditU(call, TC_GetStatusEvent, code,
	       AUD_INT, taskId, AUD_TSTT, status, AUD_END);
    return code;
}

static afs_int32
SGetStatus(struct rx_call *call, afs_uint32 taskId,
	   struct tciStatusS *statusPtr)
{
    statusP ptr;
    int retval = 0;

    memset(statusPtr, 0, sizeof(*statusPtr));
    if (callPermitted(call) == 0)
	return (TC_NOTPERMITTED);

    lock_Status();
    ptr = findStatus(taskId);
    if (ptr) {
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
STC_EndStatus(struct rx_call *call, afs_uint32 taskId)
{
    afs_int32 code;

    code = SEndStatus(call, taskId);
    osi_auditU(call, TC_EndStatusEvent, code, AUD_INT, taskId, AUD_END);
    return code;
}

static afs_int32
SEndStatus(struct rx_call *call, afs_uint32 taskId)
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
STC_RequestAbort(struct rx_call *call, afs_uint32 taskId)
{
    afs_int32 code;

    code = SRequestAbort(call, taskId);
    osi_auditU(call, TC_RequestAbortEvent, code, AUD_INT, taskId, AUD_END);
    return code;
}

static afs_int32
SRequestAbort(struct rx_call *call, afs_uint32 taskId)
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
STC_ScanStatus(struct rx_call *call, afs_uint32 *taskId,
	       struct tciStatusS *status, afs_uint32 *flags)
{
    afs_int32 code;

    code = SScanStatus(call, taskId, status, flags);
    osi_auditU(call, TC_ScanStatusEvent, code,
	       AUD_INT, *taskId, AUD_TSTT, status, AUD_INT, *flags, AUD_END);
    return code;
}

static afs_int32
SScanStatus(struct rx_call *call, afs_uint32 *taskId,
	    struct tciStatusS *statusPtr, afs_uint32 *flags)
{
    statusP ptr = 0;
    dlqlinkP dlqPtr;

    memset(statusPtr, 0, sizeof(*statusPtr));
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

int
checkAbortByTaskId(afs_uint32 taskId)
{
    statusP statusPtr;
    int retval = 0;

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
getStatusFlag(afs_uint32 taskId, afs_uint32 flag)
{
    statusP statusPtr;
    int retval = 0;

    lock_Status();
    statusPtr = findStatus(taskId);
    if (statusPtr) {
	retval = statusPtr->flags & flag;
    }
    unlock_Status();
    return (retval);
}
