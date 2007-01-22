/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mutex.h>
#include <afs/lock.h>

void
osi_mutex_Init(osi_mutex_t * lock, osi_mutex_options_t * opts)
{
    Lock_Init(&lock->lock);
    osi_mutex_options_Copy(&lock->opts, opts);
}

int
osi_mutex_NBLock(osi_mutex_t * lock)
{
    int code, haveGlock;

    haveGlock = ISAFS_GLOCK();
    if (!haveGlock) {
	AFS_GLOCK();
    }

    code = NBObtainWriteLock(&lock->lock, 0x7fffffff);

    if (!haveGlock) {
	AFS_GUNLOCK();
    }

    return (code == 0) ? 1 : 0;
}
