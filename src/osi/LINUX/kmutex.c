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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)

void
osi_mutex_Init(osi_mutex_t * lock, osi_mutex_options_t * opts)
{
    mutex_init(&lock->lock);
    osi_mutex_options_Copy(&lock->opts, opts);
}

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) */

void
osi_mutex_Init(osi_mutex_t * lock, osi_mutex_options_t * opts)
{
    init_MUTEX(&lock->lock);
    osi_mutex_options_Copy(&lock->opts, opts);
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) */
