/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * implementation of a counter
 * using spinlocks
 */

#include <osi/osi_impl.h>
#include <osi/osi_counter.h>

#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER)
void
osi_counter_init(osi_counter_t * obj, osi_counter_val_t val)
{
    osi_spinlock_options_t opt;

    obj->counter = val;

    osi_spinlock_options_Init(&opt);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY, 1);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_spinlock_Init(&obj->lock, &opt);
    osi_spinlock_options_Destroy(&opt);
}

void
osi_counter_reset(osi_counter_t * obj, osi_counter_val_t val)
{
    obj->counter = val;
}

void
osi_counter_destroy(osi_counter_t * obj)
{
    osi_spinlock_Destroy(&obj->lock);
}
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER */


#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER16)
void
osi_counter16_init(osi_counter16_t * obj, osi_counter16_val_t val)
{
    osi_spinlock_options_t opt;

    obj->counter = val;

    osi_spinlock_options_Init(&opt);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY, 1);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_spinlock_Init(&obj->lock, &opt);
    osi_spinlock_options_Destroy(&opt);
}

void
osi_counter16_reset(osi_counter16_t * obj, osi_counter16_val_t val)
{
    obj->counter = val;
}

void
osi_counter16_destroy(osi_counter16_t * obj)
{
    osi_spinlock_Destroy(&obj->lock);
}
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER16 */


#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER32)
void
osi_counter32_init(osi_counter32_t * obj, osi_counter32_val_t val)
{
    osi_spinlock_options_t opt;

    obj->counter = val;

    osi_spinlock_options_Init(&opt);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY, 1);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_spinlock_Init(&obj->lock, &opt);
    osi_spinlock_options_Destroy(&opt);
}

void
osi_counter32_reset(osi_counter32_t * obj, osi_counter32_val_t val)
{
    obj->counter = val;
}

void
osi_counter32_destroy(osi_counter32_t * obj)
{
    osi_spinlock_Destroy(&obj->lock);
}
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER32 */


#if defined(OSI_IMPLEMENTS_LEGACY_COUNTER64)
void
osi_counter64_init(osi_counter64_t * obj, osi_counter64_val_t val)
{
    osi_spinlock_options_t opt;

    obj->counter = val;

    osi_spinlock_options_Init(&opt);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_PREEMPTIVE_ONLY, 1);
    osi_spinlock_options_Set(&opt, OSI_SPINLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_spinlock_Init(&obj->lock, &opt);
    osi_spinlock_options_Destroy(&opt);
}

void
osi_counter64_reset(osi_counter64_t * obj, osi_counter64_val_t val)
{
    obj->counter = val;
}

void
osi_counter64_destroy(osi_counter64_t * obj)
{
    osi_spinlock_Destroy(&obj->lock);
}
#endif /* OSI_IMPLEMENTS_LEGACY_COUNTER64 */
