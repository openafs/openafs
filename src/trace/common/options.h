/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_OPTIONS_H
#define _OSI_TRACE_COMMON_OPTIONS_H 1

/*
 * osi tracing framework
 * common support
 * default osi object options repository
 *
 * we are now using the osi implementation-private options
 * repository, since osi_trace_common_options had merely become
 * a duplicate
 *
 * HOWEVER, you should ALWAYS reference osi_trace_common_*_opts()
 * within trace code so that we retain the flexibility to fork
 * the options structures in the future, should it ever become
 * necessary!
 */

#define osi_trace_impl_mutex_opts()               osi_impl_mutex_opts()
#define osi_trace_impl_condvar_opts()             osi_impl_condvar_opts()
#define osi_trace_impl_rwlock_opts()              osi_impl_rwlock_opts()
#define osi_trace_impl_shlock_opts()              osi_impl_shlock_opts()
#define osi_trace_impl_spinlock_opts()            osi_impl_spinlock_opts()
#define osi_trace_impl_spin_rwlock_opts()         osi_impl_spin_rwlock_opts()
#define osi_trace_impl_mem_object_cache_opts()    osi_impl_mem_object_cache_opts()
#define osi_trace_impl_thread_opts()              osi_impl_thread_opts()
#define osi_trace_impl_demux_opts()               osi_impl_demux_opts()
#define osi_trace_impl_vector_opts()              osi_impl_vector_opts()

#endif /* _OSI_TRACE_COMMON_OPTIONS_H */
