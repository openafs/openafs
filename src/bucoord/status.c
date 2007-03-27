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
    ("$Header: /cvs/openafs/src/bucoord/status.c,v 1.7.2.1 2007/01/05 03:34:09 shadow Exp $");

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#endif
#include <afs/com_err.h>
#include <afs/bubasics.h>
#include "bc.h"
#include "error_macros.h"


extern dlqlinkT statusHead;	/* chain of status blocks */
extern struct Lock statusQueueLock;	/* access control for status chain */
extern struct Lock cmdLineLock;	/* lock on the cmdLine */

/* task status management
 *
 * These routines are common the backup coordinator and tape coordinator
 */

initStatus()
{
    dlqInit(&statusHead);
    Lock_Init(&statusQueueLock);
    Lock_Init(&cmdLineLock);
}

/* lock managment */

lock_Status()
{
    ObtainWriteLock(&statusQueueLock);
}

unlock_Status()
{
    ReleaseWriteLock(&statusQueueLock);
}

lock_cmdLine()
{
    ObtainWriteLock(&cmdLineLock);
}

unlock_cmdLine()
{
    ReleaseWriteLock(&cmdLineLock);
}

/* general */

void
clearStatus(taskId, flags)
     afs_uint32 taskId;
     afs_uint32 flags;
{
    statusP ptr;

    extern statusP findStatus();

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
createStatusNode()
{
    statusP ptr;

    ptr = (statusP) malloc(sizeof(*ptr));
    if (ptr == 0) {
	return (0);
    }
    memset(ptr, 0, sizeof(*ptr));

    /* link it onto the chain of status entries */
    ObtainWriteLock(&statusQueueLock);
    dlqLinkb(&statusHead, (dlqlinkP) ptr);
    ptr->flags = STARTING;
    ReleaseWriteLock(&statusQueueLock);

    return (ptr);
}

deleteStatusNode(ptr)
     statusP ptr;
{
    ObtainWriteLock(&statusQueueLock);
    dlqUnlink((dlqlinkP) ptr);

    if (ptr->cmdLine)
	free(ptr->cmdLine);
    free(ptr);
    ReleaseWriteLock(&statusQueueLock);
}

statusP
findStatus(taskId)
     afs_uint32 taskId;
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
setStatus(taskId, flags)
     afs_uint32 taskId;
     afs_uint32 flags;
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
