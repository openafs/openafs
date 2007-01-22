/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_shlock.h>
#include <afs/lock.h>

void
osi_shlock_Init(osi_shlock_t * lock, osi_shlock_options_t * opts)
{
    Lock_Init(&lock->lock);
    osi_shlock_options_Copy(&lock->opts, opts);
}

int
osi_shlock_NBShLock(osi_shlock_t * lock)
{
    int code, haveGlock;

    haveGlock = ISAFS_GLOCK();
    if (!haveGlock) {
	AFS_GLOCK();
    }

    code = NBObtainSharedLock(&lock->lock, 0x7fffffff);

    if (!haveGlock) {
	AFS_GUNLOCK();
    }

    return (code == 0);
}

int
osi_shlock_NBWrLock(osi_shlock_t * lock)
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
