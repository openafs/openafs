/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * inter-process asynchronous messaging
 * initialization routines
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/mail.h>
#include <trace/gen_rgy.h>
#include <trace/mail/common.h>
#include <trace/mail/handler.h>
#include <trace/mail/xid.h>
#include <trace/mail/rpc.h>

osi_init_func_t * __osi_trace_mail_handler_init_ptr = osi_NULL;
osi_fini_func_t * __osi_trace_mail_handler_fini_ptr = osi_NULL;
osi_init_func_t * __osi_trace_mail_thread_init_ptr = osi_NULL;
osi_fini_func_t * __osi_trace_mail_thread_fini_ptr = osi_NULL;
osi_init_func_t * __osi_trace_mail_rpc_init_ptr = osi_NULL;
osi_fini_func_t * __osi_trace_mail_rpc_fini_ptr = osi_NULL;
osi_init_func_t * __osi_trace_mail_xid_init_ptr = osi_NULL;
osi_fini_func_t * __osi_trace_mail_xid_fini_ptr = osi_NULL;

osi_result
osi_trace_mail_PkgInit(void)
{
    osi_result res;

    res = osi_trace_mail_allocator_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    if (__osi_trace_mail_xid_init_ptr) {
	res = (*__osi_trace_mail_xid_init_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

#if defined(OSI_KERNELSPACE_ENV)
    res = osi_trace_mail_postmaster_PkgInit();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }
#endif

    if (__osi_trace_mail_handler_init_ptr) {
	res = (*__osi_trace_mail_handler_init_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

    if (__osi_trace_mail_rpc_init_ptr) {
	res = (*__osi_trace_mail_rpc_init_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

    if (__osi_trace_mail_thread_init_ptr) {
	res = (*__osi_trace_mail_thread_init_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

 error:
    return res;
}

osi_result
osi_trace_mail_PkgShutdown(void)
{
    osi_result res;

    if (__osi_trace_mail_thread_fini_ptr) {
	res = (*__osi_trace_mail_thread_fini_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

    if (__osi_trace_mail_rpc_fini_ptr) {
	res = (*__osi_trace_mail_rpc_fini_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

    if (__osi_trace_mail_handler_fini_ptr) {
	res = (*__osi_trace_mail_handler_fini_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

#if defined(OSI_KERNELSPACE_ENV)
    res = osi_trace_mail_postmaster_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }
#endif

    if (__osi_trace_mail_xid_fini_ptr) {
	res = (*__osi_trace_mail_xid_fini_ptr)();
	if (OSI_RESULT_FAIL(res)) {
	    goto error;
	}
    }

    res = osi_trace_mail_allocator_PkgShutdown();
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

 error:
    return res;
}
