/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_thread.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <osi/COMMON/thread_event_impl.h>


osi_result
osi_thread_create(osi_thread_p * thr,
		  void * stk, size_t stk_len,
		  void * (*func)(void *),
		  void * argv, size_t argv_len,
		  osi_proc_p proc,
		  osi_prio_t prio,
		  osi_thread_options_t * opt)
{
    osi_result res = OSI_OK;

    res = __osi_thread_run_args_alloc(&thr->args);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    __osi_thread_run_args_set_fp(thr->args, func);
    __osi_thread_run_args_set_args(thr->args, argv, argv_len);
    __osi_thread_run_args_set_opts(thr->args, opt);

    thr->handle = thread_create(stk, stk_len, &osi_thread_run, 
				(void *)thr->args, sizeof(struct osi_thread_run_arg),
				proc, TS_RUN, prio);
    if (osi_compiler_expect_false(thr->handle == NULL)) {
	res = OSI_FAIL;
	goto error;
    }

 done:
    return res;

 error:
    if (thr->args != osi_NULL) {
	__osi_thread_run_args_free(thr->args);
    }
    goto done;
}
