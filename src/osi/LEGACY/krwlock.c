/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_rwlock.h>
#include <afs/lock.h>

void
osi_rwlock_Init(osi_rwlock_t * lock, osi_rwlock_options_t * opts)
{
    Lock_Init(&lock->lock);
    osi_rwlock_options_Copy(&lock->opts, opts);
}

int
osi_rwlock_NBWrLock(osi_rwlock_t * lock)
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

    return (code == 0);
}
