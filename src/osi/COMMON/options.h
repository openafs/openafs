/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OPTIONS_H
#define _OSI_COMMON_OPTIONS_H 1

/*
 * osi
 * common support
 * internal default osi object options repository
 *
 * macro interfaces:
 *
 *   osi_impl_mutex_opts()
 *     -- returns a pointer to an osi_mutex_options_t
 *
 *   osi_impl_condvar_opts()
 *     -- returns a pointer to an osi_condvar_options_t
 *
 *   osi_impl_rwlock_opts()
 *     -- returns a pointer to an osi_rwlock_options_t
 *
 *   osi_impl_shlock_opts()
 *     -- returns a pointer to an osi_shlock_options_t
 *
 *   osi_impl_spinlock_opts()
 *     -- returns a pointer to an osi_spinlock_options_t
 *
 *   osi_impl_spin_rwlock_opts()
 *     -- returns a pointer to an osi_spin_rwlock_options_t
 *
 *   osi_impl_mem_object_cache_opts()
 *     -- returns a pointer to an osi_mem_object_cache_options_t
 *
 *   osi_impl_thread_opts()
 *     -- returns a pointer to an osi_thread_options_t
 *
 *   osi_impl_demux_opts()
 *     -- returns a pointer to an osi_demux_options_t
 *
 *   osi_impl_vector_opts()
 *     -- returns a pointer to an osi_vector_options_t
 *
 *   osi_impl_heap_opts()
 *     -- returns a pointer to an osi_heap_options_t
 *
 */

/*
 * EVERYTHING BELOW THIS LINE IS
 * IMPLEMENTATION-PRIVATE
 *
 * DO NOT DIRECTLY REFERENCE 
 * struct osi_common_options 
 * IN YOUR CODE!!!
 */

#include <osi/COMMON/condvar_options.h>
#include <osi/COMMON/mutex_options.h>
#include <osi/COMMON/object_cache_options.h>
#include <osi/COMMON/rwlock_options.h>
#include <osi/COMMON/shlock_options.h>
#include <osi/COMMON/spinlock_options.h>
#include <osi/COMMON/spin_rwlock_options.h>
#include <osi/COMMON/thread_option.h>
#include <osi/COMMON/demux_options.h>
#if defined(OSI_ENV_USERSPACE)
#include <osi/COMMON/data/vector_options.h>
#include <osi/COMMON/data/heap_options.h>
#endif /* OSI_ENV_USERSPACE */


#define __osi_common_opts_mbr(pkg)          osi_##pkg##_options_t pkg##_opts
#define __osi_common_opts_ptr(pkg)          &osi_common_options.pkg##_opts

struct osi_common_options {
    __osi_common_opts_mbr(mutex);
    __osi_common_opts_mbr(condvar);
    __osi_common_opts_mbr(rwlock);
    __osi_common_opts_mbr(shlock);
    __osi_common_opts_mbr(spinlock);
    __osi_common_opts_mbr(spin_rwlock);
    __osi_common_opts_mbr(mem_object_cache);
    __osi_common_opts_mbr(thread);
    __osi_common_opts_mbr(demux);
#if defined(OSI_ENV_USERSPACE)
    __osi_common_opts_mbr(vector);
    __osi_common_opts_mbr(heap);
#endif /* OSI_ENV_USERSPACE */
};

osi_extern struct osi_common_options osi_common_options;


#define osi_impl_mutex_opts()               __osi_common_opts_ptr(mutex)
#define osi_impl_condvar_opts()             __osi_common_opts_ptr(condvar)
#define osi_impl_rwlock_opts()              __osi_common_opts_ptr(rwlock)
#define osi_impl_shlock_opts()              __osi_common_opts_ptr(shlock)
#define osi_impl_spinlock_opts()            __osi_common_opts_ptr(spinlock)
#define osi_impl_spin_rwlock_opts()         __osi_common_opts_ptr(spin_rwlock)
#define osi_impl_mem_object_cache_opts()    __osi_common_opts_ptr(mem_object_cache)
#define osi_impl_thread_opts()              __osi_common_opts_ptr(thread)
#define osi_impl_demux_opts()               __osi_common_opts_ptr(demux)
#define osi_impl_vector_opts()              __osi_common_opts_ptr(vector)
#define osi_impl_heap_opts()                __osi_common_opts_ptr(heap)

OSI_INIT_FUNC_PROTOTYPE(osi_common_options_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_common_options_PkgShutdown);

#endif /* _OSI_COMMON_OPTIONS_H */
