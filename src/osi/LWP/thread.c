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
#include <osi/osi_proc.h>
#include <osi/osi_mem.h>
#include <lwp/lwp.h>
#include <osi/COMMON/thread_event_impl.h>

PROCESS osi_lwp_main_thread;

osi_result
osi_thread_PkgInit(void)
{
    osi_result res;
    int code;

    code = LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY,
					&osi_lwp_main_thread);
    if (code != LWP_SUCCESS) {
	(osi_Msg "%s: failed to initialize LWP\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    code = IOMGR_Initialize();
    if (code != LWP_SUCCESS) {
	(osi_Msg "%s: failed to initialize LWP IOMGR subsystem\n", __osi_func__);
	res = OSI_FAIL;
	goto error;
    }

    res = osi_thread_event_PkgInit();

 error:
    return res;
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
    int code;

    res = __osi_thread_run_args_alloc(&args);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    __osi_thread_run_args_set_fp(args, func);
    __osi_thread_run_args_set_args(args, argv, argv_len);
    __osi_thread_run_args_set_opts(args, opt);

    code = LWP_CreateProcess(&osi_thread_run, stk_len, prio,
			     (void *)args, "osi_thread_run",
			     &idp->handle);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
    } else if (stk != osi_NULL) {
	osi_mem_free(stk, stk_len);
    }

    idp->args = args;

 done:
    return res;

 error:
    if (args != osi_NULL) {
	__osi_thread_run_args_free(args);
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
    int code;

    res = __osi_thread_run_args_alloc(&args);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    __osi_thread_run_args_set_fp(args, func);
    __osi_thread_run_args_set_args(args, argv, 0);
    __osi_thread_run_args_set_opts(args, opt);

    code = LWP_CreateProcess(&osi_thread_run, 65536, LWP_NORMAL_PRIORITY,
			     (void *) args, "osi_thread_run",
			     &idp->handle);
    if (osi_compiler_expect_false(code != 0)) {
	res = OSI_FAIL;
    }

    idp->args = args;

 done:
    return res;

 error:
    if (args != osi_NULL) {
	__osi_thread_run_args_free(args);
    }
    goto done;
}
