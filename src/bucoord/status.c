/* Copyright (C) 1998 Transarc Corporation - All rights reserved */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#endif
#include <afs/com_err.h>
#include <afs/bubasics.h>
#include "bc.h"
#include "error_macros.h"


extern dlqlinkT statusHead;		/* chain of status blocks */
extern struct Lock statusQueueLock;	/* access control for status chain */
extern struct Lock cmdLineLock;	        /* lock on the cmdLine */

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

clearStatus(taskId, flags)
     afs_uint32 taskId;
     afs_uint32 flags;
{
     statusP ptr;

     extern statusP findStatus();

     ObtainWriteLock(&statusQueueLock);
     ptr = findStatus(taskId);
     if ( ptr == 0 )
     {
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
    if ( ptr == 0 )
    {
	return(0);
    }
    bzero(ptr, sizeof(*ptr));

    /* link it onto the chain of status entries */
    ObtainWriteLock(&statusQueueLock);
    dlqLinkb(&statusHead, (dlqlinkP) ptr);
    ptr->flags = STARTING;
    ReleaseWriteLock(&statusQueueLock);

    return(ptr);
}

deleteStatusNode(ptr)
     statusP ptr;
{
    ObtainWriteLock(&statusQueueLock);
    dlqUnlink( (dlqlinkP) ptr);

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
    while ( dlqPtr != &statusHead )
    {
        if ( ((statusP) dlqPtr)->taskId == taskId )
        {
            ptr = (statusP) dlqPtr;
            break;
        }
        dlqPtr = dlqPtr->dlq_next;
    }

    return(ptr);
}

setStatus(taskId, flags)
     afs_uint32 taskId;
     afs_uint32 flags;
{
     statusP ptr;

     ObtainWriteLock(&statusQueueLock);
     ptr = findStatus(taskId);
     if ( ptr == 0 )
     {
	 ReleaseWriteLock(&statusQueueLock);
         return;
     }

     ptr->flags |= flags;
     ReleaseWriteLock(&statusQueueLock);
}

