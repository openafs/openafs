/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_THREAD_H
#define _OSI_OSI_THREAD_H 1


/*
 * platform-independent osi_thread API
 *
 * all interfaces in this file require
 * defined(OSI_IMPLEMENTS_THREAD)
 *
 *  osi_thread_p -- opaque handle to thread info
 *  osi_thread_id_t -- thread id type
 *  OSI_THREAD_DEFAULT_STACK_SIZE
 *
 *  osi_result osi_thread_create(osi_thread_p * idp,
 *                               void * stack, size_t stack_size,
 *                               (void *)(*proc)(void *), void * args, size_t args_len,
 *                               osi_proc_p pp, osi_prio_t pri,
 *                               osi_thread_options_t *);
 *    -- create a new thread
 *
 *  osi_thread_id_t osi_thread_current_id();
 *    -- get the current thread id
 *
 *  osi_thread_id_t osi_thread_id(osi_thread_p);
 *    -- get the thread id
 *
 *  void osi_thread_options_Init(osi_thread_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_thread_options_Destroy(osi_thread_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_thread_options_Set(osi_thread_options_t *, osi_thread_options_param_t, long val);
 *    -- set the value of a parameter
 *
 * the following interfaces form an event-driven mechanism to allow functions to be
 * called by pointer on thread create and exit events
 *
 *  osi_thread_event_type_t
 *    -- event type enumeration
 *
 *  osi_thread_event_handler_t
 *    -- function pointer of the form osi_result (*)(osi_thread_id_t, osi_thread_event_type_t, void *);
 *
 *  osi_thread_event_t
 *    -- handle to event handler control object
 *
 *  osi_result osi_thread_event_create(osi_thread_event_t **, osi_thread_event_handler_t, void *);
 *    -- create a new event handler object, and populate the function pointer and opaque data
 *
 *  osi_result osi_thread_event_destroy(osi_thread_event_t *);
 *    -- free a thread event handler object
 *
 *  osi_result osi_thread_event_register(osi_thread_event_t *, osi_uint32 events);
 *    -- register a thread create event handler; $events$ is a bitmask of osi_thread_event_type_t's
 *
 *  osi_result osi_thread_event_unregister(osi_thread_event_t *, osi_uint32 events);
 *    -- un-register a thread create event handler; $events$ is a bitmask of osi_thread_event_type_t's
 *
 * the following interface requires
 * defined(OSI_ENV_USERSPACE)
 *
 *  osi_result osi_thread_createU(osi_thread_p *, (void *)(*proc)(void *), void * args,
 *                                osi_thread_options_t *);
 *    -- userspace-only shortcut for thread creation
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_THREAD_BIND)
 *
 *  osi_result osi_thread_bind_current(osi_cpu_id_t cpuid);
 *    -- bind this thread to cpu cpuid
 *
 *  osi_result osi_thread_unbind_current();
 *    -- unbind this thread from a cpu
 *
 * the following interface requires
 * defined(OSI_IMPLEMENTS_THREAD_DETACH)
 *
 *  void osi_thread_current_detach();
 *    -- detach the current thread
 *
 * the following interfaces require
 * defined(OSI_ENV_KERNELSPACE)
 *
 *  osi_thread_t -- kernel thread info structure typedef
 *
 *  osi_thread_p osi_thread_current();
 *    -- get the current thread pointer
 *
 *  osi_proc_p osi_thread_to_proc(osi_thread_p);
 *    -- get the process pointer from a thread pointer
 *
 */

#include <osi/COMMON/thread_option.h>
#include <osi/COMMON/thread.h>


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kthread.h>
#elif defined(OSI_LINUX_ENV)
#include <osi/LINUX/kthread.h>
#else
#include <osi/LEGACY/kthread.h>
#endif

#elif defined(UKERNEL)

#include <osi/LEGACY/kthread.h>

#else /* !KERNEL */

#if defined(OSI_ENV_PTHREAD)
#include <osi/PTHREAD/thread.h>
#elif defined(OSI_ENV_LWP)
#include <osi/LWP/thread.h>
#endif /* OSI_ENV_LWP */

#endif /* !KERNEL */


#include <osi/COMMON/thread_bind.h>
#include <osi/COMMON/thread_event.h>

#endif /* _OSI_OSI_THREAD_H */
