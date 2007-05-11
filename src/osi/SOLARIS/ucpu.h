/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UCPU_H
#define _OSI_SOLARIS_UCPU_H 1


#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>

typedef processorid_t osi_cpu_id_t;

typedef osi_result osi_cpu_iterator_t(osi_cpu_id_t, void *);
typedef osi_result osi_cpu_monitor_t(osi_cpu_id_t, osi_cpu_event_t, void *);

osi_extern osi_result osi_cpu_count(osi_int32 *);
osi_extern osi_result osi_cpu_min_id(osi_cpu_id_t *);
osi_extern osi_result osi_cpu_max_id(osi_cpu_id_t *);
osi_extern osi_result osi_cpu_list_iterate(osi_cpu_iterator_t * , void *);

#define OSI_IMPLEMENTS_CPU_BIND 1

osi_extern osi_result osi_cpu_bind_thread_current(osi_cpu_id_t);
osi_extern osi_result osi_cpu_unbind_thread_current(void);

#define osi_cpu_PkgInit       osi_null_init_func
#define osi_cpu_PkgShutdown   osi_null_fini_func

#endif /* _OSI_SOLARIS_UCPU_H */
