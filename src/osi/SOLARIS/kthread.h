/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KTHREAD_H
#define _OSI_SOLARIS_KTHREAD_H 1

#define OSI_IMPLEMENTS_THREAD 1

#include <sys/thread.h>
#include <osi/osi_proc.h>

typedef struct {
    kthread_t * handle;
    struct osi_thread_run_arg * args;
} osi_thread_p;

typedef kthread_t osi_thread_t;
typedef id_t osi_thread_id_t;
typedef pri_t osi_prio_t;

osi_extern osi_result
osi_thread_create(osi_thread_p * thr,
		  void * stk, size_t stk_len,
		  void * (*func)(void *),
		  void * args_in, size_t args_len,
		  osi_proc_p proc,
		  osi_prio_t prio,
		  osi_thread_options_t * opt);

#define osi_thread_current_id() (curthread->t_tid)
#define osi_thread_id(x) ((x)->t_tid)
#define osi_thread_current() (curthread)
#define osi_thread_to_proc(x) (ttoproc(x))

#define osi_thread_PkgInit      osi_thread_event_PkgInit
#define osi_thread_PkgShutdown  osi_thread_event_PkgShutdown

#endif /* _OSI_SOLARIS_KTHREAD_H */
