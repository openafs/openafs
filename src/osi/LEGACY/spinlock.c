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

#if defined(OSI_IMPLEMENTS_LEGACY_SPINLOCK)

#include <osi/osi_mutex.h>

osi_static void map_options(osi_spinlock_options_t * spin_opts,
			    osi_mutex_options_t * mutex_opts);

void
osi_spinlock_Init(osi_spinlock_t * lock, osi_spinlock_options_t * opts)
{
    osi_spinlock_options_t spin_opts;
    osi_mutex_options_t mutex_opts;

    osi_spinlock_options_Copy(&spin_opts, opts);
    osi_mutex_options_Init(&mutex_opts);
    map_options(&spin_opts, &mutex_opts);

    osi_mutex_Init(lock, &mutex_opts);
}

osi_static void 
map_options(osi_spinlock_options_t * spin_opts,
	    osi_mutex_options_t * mutex_opts)
{
    osi_mutex_options_Set(mutex_opts,
			  OSI_MUTEX_OPTION_PREEMPTIVE_ONLY,
			  spin_opts->preemptive_only);
    osi_mutex_options_Set(mutex_opts,
			  OSI_MUTEX_OPTION_TRACE_ALLOWED,
			  spin_opts->trace_allowed);
    osi_mutex_options_Set(mutex_opts,
			  OSI_MUTEX_OPTION_TRACE_ENABLED,
			  spin_opts->trace_enabled);
}

#endif /* OSI_IMPLEMENTS_LEGACY_SPINLOCK */
