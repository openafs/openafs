/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_PTHREAD_THREAD_H
#define	_OSI_PTHREAD_THREAD_H

#define OSI_IMPLEMENTS_THREAD 1

#include <pthread.h>
#include <osi/osi_proc.h>

typedef struct {
    pthread_t handle;
    struct osi_thread_run_arg * args;
} osi_thread_p;

typedef pthread_t osi_thread_id_t;
typedef int osi_prio_t;

osi_extern osi_result osi_thread_create(osi_thread_p * idp,
					void * stk, size_t stk_len,
					void * (*func)(void *),
					void * args, size_t args_len,
					osi_proc_p proc,
					osi_prio_t prio,
					osi_thread_options_t * opt);

osi_extern osi_result osi_thread_createU(osi_thread_p *, 
					 void * (*fp)(void *), 
					 void *,
					 osi_thread_options_t *);

#define osi_thread_current_id() (pthread_self())
#define osi_thread_id(x) ((x)->handle)

#endif /* _OSI_PTHREAD_THREAD_H */
