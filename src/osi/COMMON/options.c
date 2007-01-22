/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi
 * common support
 * internal default osi object options repository
 */

#include <osi/osi_impl.h>

struct osi_common_options osi_common_options;

osi_result
osi_common_options_PkgInit(void)
{
    osi_mutex_options_Init(&osi_common_options.mutex_opts);
    osi_mutex_options_Set(&osi_common_options.mutex_opts,
			  OSI_MUTEX_OPTION_TRACE_ALLOWED,
			  0);

    osi_condvar_options_Init(&osi_common_options.condvar_opts);
    osi_condvar_options_Set(&osi_common_options.condvar_opts,
			    OSI_CONDVAR_OPTION_TRACE_ALLOWED,
			    0);

    osi_rwlock_options_Init(&osi_common_options.rwlock_opts);
    osi_rwlock_options_Set(&osi_common_options.rwlock_opts,
			   OSI_RWLOCK_OPTION_TRACE_ALLOWED,
			   0);

    osi_shlock_options_Init(&osi_common_options.shlock_opts);
    osi_shlock_options_Set(&osi_common_options.shlock_opts,
			   OSI_SHLOCK_OPTION_TRACE_ALLOWED,
			   0);

    osi_spinlock_options_Init(&osi_common_options.spinlock_opts);
    osi_spinlock_options_Set(&osi_common_options.spinlock_opts,
			     OSI_SPINLOCK_OPTION_TRACE_ALLOWED,
			     0);

    osi_spin_rwlock_options_Init(&osi_common_options.spin_rwlock_opts);
    osi_spin_rwlock_options_Set(&osi_common_options.spin_rwlock_opts,
				OSI_SPIN_RWLOCK_OPTION_TRACE_ALLOWED,
				0);

    osi_mem_object_cache_options_Init(&osi_common_options.mem_object_cache_opts);
    osi_mem_object_cache_options_Set(&osi_common_options.mem_object_cache_opts,
				     OSI_MEM_OBJECT_CACHE_OPTION_TRACE_ALLOWED,
				     0);

    osi_thread_options_Init(&osi_common_options.thread_opts);
    osi_thread_options_Set(&osi_common_options.thread_opts,
			   OSI_THREAD_OPTION_TRACE_ALLOWED,
			   0);

    return OSI_OK;
}

osi_result
osi_common_options_PkgShutdown(void)
{
    osi_mutex_options_Destroy(&osi_common_options.mutex_opts);
    osi_condvar_options_Destroy(&osi_common_options.condvar_opts);
    osi_rwlock_options_Destroy(&osi_common_options.rwlock_opts);
    osi_shlock_options_Destroy(&osi_common_options.shlock_opts);
    osi_spinlock_options_Destroy(&osi_common_options.spinlock_opts);
    osi_spin_rwlock_options_Destroy(&osi_common_options.spin_rwlock_opts);
    osi_mem_object_cache_options_Destroy(&osi_common_options.mem_object_cache_opts);
    osi_thread_options_Destroy(&osi_common_options.thread_opts);

    return OSI_OK;
}
