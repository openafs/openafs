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

void
osi_shlock_Init(osi_shlock_t * lock, osi_shlock_options_t * opts)
{
    Lock_Init(&lock->lock);
    osi_shlock_options_Copy(&lock->opts, opts);
}

void
osi_shlock_Destroy(osi_shlock_t * lock)
{
    Lock_Destroy(&lock->lock);
}

int
osi_shlock_NBRdLock(osi_shlock_t * lock)
{
    int code;

    ObtainReadLockNoBlock(&lock->lock, code);

    return (code == 0) ? 1 : 0;
}

int
osi_shlock_NBShLock(osi_shlock_t * lock)
{
    int code;

    ObtainSharedLockNoBlock(&lock->lock, code);

    return (code == 0) ? 1 : 0;
}

int
osi_shlock_NBWrLock(osi_shlock_t * lock)
{
    int code;

    ObtainWriteLockNoBlock(&lock->lock, code);

    return (code == 0) ? 1 : 0;
}
