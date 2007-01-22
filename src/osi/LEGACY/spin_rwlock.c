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

#if defined(OSI_IMPLEMENTS_LEGACY_SPIN_RWLOCK)

#include <osi/osi_rwlock.h>

osi_static void map_options(osi_spin_rwlock_options_t * spin_opts,
			    osi_rwlock_options_t * rwlock_opts);

void
osi_spin_rwlock_Init(osi_spin_rwlock_t * lock, osi_spin_rwlock_options_t * opts)
{
    osi_spin_rwlock_options_t spin_opts;
    osi_rwlock_options_t rwlock_opts;

    osi_spin_rwlock_options_Copy(&spin_opts, opts);
    osi_rwlock_options_Init(&rwlock_opts);
    map_options(&spin_opts, &rwlock_opts);

    osi_rwlock_Init(lock, &rwlock_opts);
}

osi_static void 
map_options(osi_spin_rwlock_options_t * spin_opts,
	    osi_rwlock_options_t * rwlock_opts)
{
    osi_rwlock_options_Set(rwlock_opts,
			   OSI_RWLOCK_OPTION_PREEMPTIVE_ONLY,
			   spin_opts->preemptive_only);
    osi_rwlock_options_Set(rwlock_opts,
			   OSI_RWLOCK_OPTION_TRACE_ALLOWED,
			   spin_opts->trace_allowed);
    osi_rwlock_options_Set(rwlock_opts,
			   OSI_RWLOCK_OPTION_TRACE_ENABLED,
			   spin_opts->trace_enabled);
}

#endif /* OSI_IMPLEMENTS_LEGACY_SPIN_RWLOCK */
