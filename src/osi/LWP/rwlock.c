/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_rwlock.h>

void
osi_rwlock_Init(osi_rwlock_t * lock, osi_rwlock_options_t * opts)
{
    osi_rwlock_options_Copy(&lock->opts, opts);

    if (!lock->opts.preemptive_only) {
	Lock_Init(&lock->lock);
    }
}

void
osi_rwlock_Destroy(osi_rwlock_t * lock)
{
    if (!lock->opts.preemptive_only) {
	Lock_Destroy(&lock->lock);
    }
}

int
osi_rwlock_NBRdLock(osi_rwlock_t * lock)
{
    int code;

    if (lock->opts.preemptive_only)
	return 1;

    ObtainReadLockNoBlock(&lock->lock, code);
    return (code == 0);
}

int
osi_rwlock_NBWrLock(osi_rwlock_t * lock)
{
    int code;

    if (lock->opts.preemptive_only)
	return 1;

    ObtainWriteLockNoBlock(&lock->lock, code);
    return (code == 0);
}
