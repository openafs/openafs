/*
 * Copyright 2006, Sine Nomine Associates and others.
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
 * trace internal default osi object options repository
 */

#include <osi/osi_object_cache.h>
#include <osi/osi_condvar.h>
#include <osi/osi_mutex.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_thread.h>
#include <osi/osi_shlock.h>

struct osi_trace_common_options {
    osi_mutex_options_t mutex_opts;
    osi_condvar_options_t condvar_opts;
    osi_rwlock_options_t rwlock_opts;
    osi_shlock_options_t shlock_opts;
    osi_mem_object_cache_options_t mem_object_cache_opts;
    osi_thread_options_t thread_opts;
};

osi_extern struct osi_trace_common_options osi_trace_common_options;

osi_extern osi_result osi_trace_common_options_PkgInit(void);
osi_extern osi_result osi_trace_common_options_PkgShutdown(void);

#endif /* _OSI_TRACE_COMMON_OPTIONS_H */
