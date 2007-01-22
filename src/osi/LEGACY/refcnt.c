/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * implementation of reference counting
 * using spinlocks
 */

#include <osi/osi_impl.h>
#include <osi/osi_refcnt.h>

#if defined(OSI_IMPLEMENTS_LEGACY_REFCNT)
void
osi_refcnt_init(osi_refcnt_t * obj, osi_refcnt_val_t val)
{
    osi_spinlock_options_t opt;

    obj->refcnt = val;

    osi_spinlock_options_Init(&opt);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY, 1);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_spinlock_Init(&obj->lock, &opt);
    osi_spinlock_options_Destroy(&opt);
}

void
osi_refcnt_destroy(osi_refcnt_t * obj)
{
    osi_spinlock_Destroy(&obj->lock);
}
#endif /* OSI_IMPLEMENTS_LEGACY_REFCNT */
