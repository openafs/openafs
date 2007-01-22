/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_spin_rwlock.h>

void
osi_spin_rwlock_Init(osi_spin_rwlock_t * lock, osi_spin_rwlock_options_t * opts)
{
    rwlock_init(&lock->lock);
    osi_spin_rwlock_options_Copy(&lock->opts, opts);
}
