/*
 * Copyright 2005-2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mutex.h>

void
osi_mutex_Init(osi_mutex_t * lock, osi_mutex_options_t * opts)
{
    osi_mutex_options_Copy(&lock->opts, opts);

    if (!lock->opts.preemptive_only) {
	Lock_Init(&lock->lock);
    }
}

void
osi_mutex_Destroy(osi_mutex_t * lock)
{
    if (!lock->opts.preemptive_only) {
	Lock_Destroy(&lock->lock);
    }
}

int
osi_mutex_NBLock(osi_mutex_t * lock)
{
    int code;

    if (lock->opts.preemptive_only)
      return 1;

    ObtainWriteLockNoBlock(&lock->lock, code);

    return (code == 0);
}
