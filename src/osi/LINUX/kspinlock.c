/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_spinlock.h>

void
osi_spinlock_Init(osi_spinlock_t * lock, osi_spinlock_options_t * opts)
{
    spin_lock_init(&lock->lock);
    osi_spinlock_options_Copy(&lock->opts, opts);
}
