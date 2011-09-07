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
#include <afs/stds.h>

#include <roken.h>

#include <afs/opr.h>
#include <afs/com_err.h>
#include <afs/bubasics.h>

#include "bc.h"
#include "error_macros.h"
#include "bucoord_internal.h"
#include "bucoord_prototypes.h"

extern dlqlinkT statusHead;	/* chain of status blocks */
extern struct Lock statusQueueLock;	/* access control for status chain */
extern struct Lock cmdLineLock;	/* lock on the cmdLine */

/* task status management
 *
 * These routines are common the backup coordinator and tape coordinator
 */

void
initStatus(void)
{
    dlqInit(&statusHead);
    Lock_Init(&statusQueueLock);
    Lock_Init(&cmdLineLock);
}

/* lock managment */

void
lock_Status(void)
{
    ObtainWriteLock(&statusQueueLock);
}

void
unlock_Status(void)
{
    ReleaseWriteLock(&statusQueueLock);
}

void
lock_cmdLine(void)
{
    ObtainWriteLock(&cmdLineLock);
}
void
unlock_cmdLine(void)
{
    ReleaseWriteLock(&cmdLineLock);
}

/* general */

void
clearStatus(afs_uint32 taskId, afs_uint32 flags)
{
    statusP ptr;

    ObtainWriteLock(&statusQueueLock);
    ptr = findStatus(taskId);
    if (ptr == 0) {
	ReleaseWriteLock(&statusQueueLock);
	return;
    }

    ptr->flags &= ~flags;
    ReleaseWriteLock(&statusQueueLock);
}

statusP
createStatusNode(void)
{
    statusP ptr;

    ptr = calloc(1, sizeof(*ptr));
    if (ptr == 0) {
	return (0);
    }

    /* link it onto the chain of status entries */
    ObtainWriteLock(&statusQueueLock);
    dlqLinkb(&statusHead, (dlqlinkP) ptr);
    ptr->flags = STARTING;
    ReleaseWriteLock(&statusQueueLock);

    return (ptr);
}

void
deleteStatusNode(statusP ptr)
{
    ObtainWriteLock(&statusQueueLock);
    dlqUnlink((dlqlinkP) ptr);

    if (ptr->cmdLine)
	free(ptr->cmdLine);
    free(ptr);
    ReleaseWriteLock(&statusQueueLock);
}

statusP
findStatus(afs_uint32 taskId)
{
    statusP ptr = 0;
    dlqlinkP dlqPtr;

    dlqPtr = statusHead.dlq_next;
    while (dlqPtr != &statusHead) {
	if (((statusP) dlqPtr)->taskId == taskId) {
	    ptr = (statusP) dlqPtr;
	    break;
	}
	dlqPtr = dlqPtr->dlq_next;
    }

    return (ptr);
}

void
setStatus(afs_uint32 taskId, afs_uint32 flags)
{
    statusP ptr;

    ObtainWriteLock(&statusQueueLock);
    ptr = findStatus(taskId);
    if (ptr == 0) {
	ReleaseWriteLock(&statusQueueLock);
	return;
    }

    ptr->flags |= flags;
    ReleaseWriteLock(&statusQueueLock);
}
