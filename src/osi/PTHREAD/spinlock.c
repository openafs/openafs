/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>

#if defined(HAVE_PTHREAD_SPINLOCK)

#include <osi/osi_spinlock.h>
#include <osi/PTHREAD/spinlock.h>

void
osi_spinlock_init(osi_spinlock_t * lock, osi_spinlock_options_t * opts)
{
    osi_Assert(pthread_spin_init(&lock->lock, 
				 PTHREAD_PROCESS_PRIVATE)==0);
    osi_spinlock_options_Copy(&lock->opts, opts);
}

void
osi_spinlock_destroy(osi_spinlock_t * lock)
{
    osi_Assert(pthread_spin_destroy(&lock->lock)==0);
}

#endif /* PTHREAD_HAVE_SPINLOCK */
