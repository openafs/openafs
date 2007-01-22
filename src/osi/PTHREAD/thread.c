/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_thread.h>
#include <osi/osi_mem.h>
#include <osi/COMMON/thread_event_impl.h>

osi_result
osi_thread_PkgInit(void)
{
    return osi_thread_event_PkgInit();
}

osi_result
osi_thread_PkgShutdown(void)
{
    return osi_thread_event_PkgShutdown();
}

osi_result
osi_thread_create(osi_thread_p * idp,
		  void * stk, size_t stk_len,
		  void * (*func)(void *),
		  void * argv, size_t argv_len,
		  osi_proc_p proc,
		  osi_prio_t prio,
		  osi_thread_options_t * opt)
{
    osi_result res = OSI_OK;
    struct osi_thread_run_arg * args;
    int code, attr_inited = 0;
    pthread_attr_t attr;

    res = __osi_thread_run_args_alloc(&args);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    __osi_thread_run_args_set_fp(args, func);
    __osi_thread_run_args_set_args(args, argv, argv_len);
    __osi_thread_run_args_set_opts(args, opt);

    code = pthread_attr_init(&attr);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
	goto error;
    }
    attr_inited = 1;

    code = pthread_attr_setstacksize(&attr, stk_len);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
	goto error;
    }

    code = pthread_attr_setstackaddr(&attr, stk);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
	goto error;
    }

    if (__osi_thread_run_args_get_opts(args)->detached) {
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
	    res = OSI_FAIL;
	    goto error;
	}
    }

    code = pthread_create(&idp->handle, 
			  &attr, 
			  &osi_thread_run, 
			  (void *)args);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
    }

    pthread_attr_destroy(&attr);
    idp->args = args;

 done:
    return res;

 error:
    if (args != osi_NULL) {
	__osi_thread_run_args_free(args);
    }
    if (attr_inited) {
	pthread_attr_destroy(&attr);
    }
    goto done;
}

osi_result
osi_thread_createU(osi_thread_p * idp, 
		   void * (*func)(void *), 
		   void * argv,
		   osi_thread_options_t * opt)
{
    osi_result res = OSI_OK;
    struct osi_thread_run_arg * args;
    int code, attr_inited = 0;
    pthread_attr_t attr;

    res = __osi_thread_run_args_alloc(&args);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    __osi_thread_run_args_set_fp(args, func);
    __osi_thread_run_args_set_args(args, argv, 0);
    __osi_thread_run_args_set_opts(args, opt);

    code = pthread_attr_init(&attr);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
	goto error;
    }
    attr_inited = 1;

    if (__osi_thread_run_args_get_opts(args)->detached) {
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
	    res = OSI_FAIL;
	    goto error;
	}
    }

    code = pthread_create(&idp->handle, 
			  osi_NULL, 
			  &osi_thread_run, 
			  (void *)args);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
    }

    pthread_attr_destroy(&attr);
    idp->args = args;

 done:
    return res;

 error:
    if (args != osi_NULL) {
	__osi_thread_run_args_free(args);
    }
    if (attr_inited) {
	pthread_attr_destroy(&attr);
    }
    goto done;
}
