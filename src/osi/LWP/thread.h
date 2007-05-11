/*
 * Copyright 2005-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LWP_THREAD_H
#define _OSI_LWP_THREAD_H 1


#include <lwp/lwp.h>
#include <osi/osi_proc.h>

typedef struct {
    PROCESS handle;
    struct osi_thread_run_arg * args;
} osi_thread_p;

typedef int osi_thread_id_t;
typedef int osi_prio_t;

#define osi_thread_current_id()   ((LWP_ThreadId())->index)
#define osi_thread_id(x)   (x)    ((x)->handle->index)

osi_extern osi_result osi_thread_create(osi_thread_p * idp,
					void * stk, size_t stk_len,
					void * (*func)(void *),
					void * args_in, size_t args_len,
					osi_proc_p proc,
					osi_prio_t prio,
					osi_thread_options_t * opt);

osi_extern osi_result osi_thread_createU(osi_thread_p * idp, 
					 void * (*fp)(void *), 
					 void * args_in,
					 osi_thread_options_t * opt);

OSI_INIT_FUNC_PROTOTYPE(osi_thread_PkgInit);
#define osi_thread_PkgShutdown  osi_thread_event_PkgShutdown

#endif /* _OSI_LWP_THREAD_H */
