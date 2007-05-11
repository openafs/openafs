/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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

OSI_INIT_FUNC_DECL(osi_common_options_PkgInit)
{
    osi_options_val_t val;

    val.type = OSI_OPTION_VAL_TYPE_BOOL;
    val.val.v_bool = OSI_FALSE;

    osi_mutex_options_Init(__osi_common_opts_ptr(mutex));
    osi_mutex_options_Set(__osi_common_opts_ptr(mutex),
			  OSI_MUTEX_OPTION_TRACE_ALLOWED,
			  0);

    osi_condvar_options_Init(__osi_common_opts_ptr(condvar));
    osi_condvar_options_Set(__osi_common_opts_ptr(condvar),
			    OSI_CONDVAR_OPTION_TRACE_ALLOWED,
			    0);

    osi_rwlock_options_Init(__osi_common_opts_ptr(rwlock));
    osi_rwlock_options_Set(__osi_common_opts_ptr(rwlock),
			   OSI_RWLOCK_OPTION_TRACE_ALLOWED,
			   0);

    osi_shlock_options_Init(__osi_common_opts_ptr(shlock));
    osi_shlock_options_Set(__osi_common_opts_ptr(shlock),
			   OSI_SHLOCK_OPTION_TRACE_ALLOWED,
			   0);

    osi_spinlock_options_Init(__osi_common_opts_ptr(spinlock));
    osi_spinlock_options_Set(__osi_common_opts_ptr(spinlock),
			     OSI_SPINLOCK_OPTION_TRACE_ALLOWED,
			     0);

    osi_spin_rwlock_options_Init(__osi_common_opts_ptr(spin_rwlock));
    osi_spin_rwlock_options_Set(__osi_common_opts_ptr(spin_rwlock),
				OSI_SPIN_RWLOCK_OPTION_TRACE_ALLOWED,
				0);

    osi_mem_object_cache_options_Init(__osi_common_opts_ptr(mem_object_cache));
    osi_mem_object_cache_options_Set(__osi_common_opts_ptr(mem_object_cache),
				     OSI_MEM_OBJECT_CACHE_OPTION_TRACE_ALLOWED,
				     0);

    osi_thread_options_Init(__osi_common_opts_ptr(thread));
    osi_thread_options_Set(__osi_common_opts_ptr(thread),
			   OSI_THREAD_OPTION_TRACE_ALLOWED,
			   0);

    osi_demux_options_Init(__osi_common_opts_ptr(demux));
    osi_demux_options_Set(__osi_common_opts_ptr(demux),
			  OSI_DEMUX_OPTION_TRACE_ALLOWED,
			  &val);

#if defined(OSI_ENV_USERSPACE)
    osi_vector_options_Init(__osi_common_opts_ptr(vector));
    osi_vector_options_Set(__osi_common_opts_ptr(vector),
			   OSI_VECTOR_OPTION_TRACE_ALLOWED,
			   &val);

    osi_heap_options_Init(__osi_common_opts_ptr(heap));
    osi_heap_options_Set(__osi_common_opts_ptr(heap),
			 OSI_HEAP_OPTION_TRACE_ALLOWED,
			 &val);
#endif /* OSI_ENV_USERSPACE */

    return OSI_OK;
}

OSI_FINI_FUNC_DECL(osi_common_options_PkgShutdown)
{
    osi_mutex_options_Destroy(__osi_common_opts_ptr(mutex));
    osi_condvar_options_Destroy(__osi_common_opts_ptr(condvar));
    osi_rwlock_options_Destroy(__osi_common_opts_ptr(rwlock));
    osi_shlock_options_Destroy(__osi_common_opts_ptr(shlock));
    osi_spinlock_options_Destroy(__osi_common_opts_ptr(spinlock));
    osi_spin_rwlock_options_Destroy(__osi_common_opts_ptr(spin_rwlock));
    osi_mem_object_cache_options_Destroy(__osi_common_opts_ptr(mem_object_cache));
    osi_thread_options_Destroy(__osi_common_opts_ptr(thread));
    osi_demux_options_Destroy(__osi_common_opts_ptr(demux));
#if defined(OSI_ENV_USERSPACE)
    osi_vector_options_Destroy(__osi_common_opts_ptr(vector));
    osi_heap_options_Destroy(__osi_common_opts_ptr(heap));
#endif /* OSI_ENV_USERSPACE */

    return OSI_OK;
}
