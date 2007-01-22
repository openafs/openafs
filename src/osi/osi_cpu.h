/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_CPU_H
#define _OSI_OSI_CPU_H 1


/*
 * platform-independent osi_cpu API
 *
 * please note that on certain platforms these interfaces
 * will act as if there is 1 cpu, regardless of reality
 *
 *  osi_cpu_id_t;
 *  osi_result (osi_cpu_iterator_t *)(osi_cpu_id_t, void *);
 *
 *  osi_result osi_cpu_count(osi_int32 *);
 *    -- returns the count of online CPUs
 *
 *  osi_result osi_cpu_min_id(osi_cpu_id_t *);
 *    -- returns the min cpu id
 *
 *  osi_result osi_cpu_max_id(osi_cpu_id_t *);
 *    -- returns the max cpu id
 *
 *  osi_result osi_cpu_list_iterate(osi_cpu_iterator_t *, void *);
 *    -- iterate over all online CPUs, calling the
 *       passed function pointer for each CPU
 *
 * for platforms where the CPU id namespace is "sparse" the
 * following macro will be defined
 * defined(OSI_CPU_ID_SPARSE_NAMESPACE)
 *
 * the following interface requires
 * defined(KERNEL) && !defined(UKERNEL)
 *
 *  osi_cpu_id_t osi_cpu_current();
 *    -- returns the current CPU id
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_CPU_BIND)
 *
 *  osi_result osi_cpu_bind_thread_current(osi_cpu_id_t);
 *    -- bind a thread to a cpu
 *
 *  osi_result osi_cpu_unbind_thread_current();
 *    -- unbind a thread from a cpu
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_CPU_EVENT_MONITOR)
 *
 *  osi_cpu_event_t;
 *  osi_result (osi_cpu_monitor_t *)(osi_cpu_id_t, osi_cpu_event_t, void *);
 *
 *  osi_result osi_cpu_monitor_register(osi_cpu_monitor_t *, void *);
 *    -- register a function that gets called on cpu state changes
 *
 *  osi_result osi_cpu_monitor_unregister(osi_cpu_monitor_t *, void *);
 *    -- unregister a cpu event monitoring function
 */

typedef enum {
    OSI_CPU_EVENT_INIT,
    OSI_CPU_EVENT_CONFIG,
    OSI_CPU_EVENT_UNCONFIG,
    OSI_CPU_EVENT_ONLINE,
    OSI_CPU_EVENT_OFFLINE,
    OSI_CPU_EVENT_PARTITION_JOIN,
    OSI_CPU_EVENT_PARTITION_LEAVE
} osi_cpu_event_t;


#define OSI_CPU_RESULT_ITERATOR_STOP  OSI_RESULT_SUCCESS_CODE(OSI_CPU_ERROR_ITERATOR_STOP)


/* these are only to be called by osi_Init and osi_Shutdown */
osi_extern osi_result osi_cpu_PkgInit(void);
osi_extern osi_result osi_cpu_PkgShutdown(void);


/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kcpu.h>
#else
#include <osi/LEGACY/cpu.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/ucpu.h>
#else
#include <osi/LEGACY/cpu.h>
#endif

#endif /* !OSI_KERNELSPACE_ENV */


#endif /* _OSI_OSI_CPU_H */
