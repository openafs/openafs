/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_thread.h>
#include <osi/osi_cpu.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_condvar.h>
#include <trace/syscall.h>
#include <trace/USERSPACE/mail.h>
#include <trace/mail/handler.h>
#include <trace/common/options.h>
#include <errno.h>

#include <osi/osi_lib_init.h>

osi_lib_init_prototype(__osi_trace_mail_userspace_init);

osi_extern osi_init_func_t * __osi_trace_mail_thread_init_ptr;
osi_extern osi_fini_func_t * __osi_trace_mail_thread_fini_ptr;

osi_lib_init_decl(__osi_trace_mail_userspace_init)
{
    __osi_trace_mail_thread_init_ptr = &osi_trace_mail_thread_PkgInit;
    __osi_trace_mail_thread_fini_ptr = &osi_trace_mail_thread_PkgShutdown;
}


/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 */

osi_result
osi_trace_mail_send(osi_trace_mail_message_t * msg)
{
    osi_result res;
    int rv;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_SEND, 
			    (long)msg, 
			    0, 
			    0,
			    &rv);

    return res;
}

osi_result
osi_trace_mail_sendv(osi_trace_mail_mvec_t * mv)
{
    osi_result res;
    int rv;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_SENDV,
			    (long)mv,
			    0,
			    0,
			    &rv);

    return res;
}

osi_result
osi_trace_mail_check(osi_trace_gen_id_t gen,
		     osi_trace_mail_message_t * msg,
		     osi_uint32 flags)
{
    osi_result res;
    int rv;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_CHECK, 
			    (long) gen,
			    (long) msg,
			    (long) flags,
			    &rv);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 done:
    return res;

 error:
    if (errno == EWOULDBLOCK) {
	res = OSI_TRACE_MAIL_RESULT_NO_MAIL;
    }
    goto done;
}

osi_result
osi_trace_mail_checkv(osi_trace_gen_id_t gen,
		      osi_trace_mail_mvec_t * mv,
		      osi_uint32 flags)
{
    osi_result res;
    int rv;

    res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_CHECKV,
			    (long) gen,
			    (long) mv,
			    (long) flags,
			    &rv);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 done:
    return res;

 error:
    if (errno == EWOULDBLOCK) {
	res = OSI_TRACE_MAIL_RESULT_NO_MAIL;
    }
    goto done;
}

osi_result
osi_trace_mail_thread_PkgInit(void)
{
    osi_result res;
    osi_thread_p tid;
    osi_options_val_t opt;

    res = osi_config_options_Get(OSI_OPTION_TRACE_START_MAIL_THREAD,
				 &opt);
    if (OSI_RESULT_FAIL(res)) {
	(osi_Msg "failed to get TRACE_START_MAIL_THREAD option value\n");
	goto error;
    }

    if (opt.val.v_bool == OSI_TRUE) {
	res = osi_thread_createU(&tid, 
				 &osi_trace_mail_receiver, 
				 osi_NULL,
				 &osi_trace_common_options.thread_opts);
    }

 error:
    return res;
}

osi_result
osi_trace_mail_thread_PkgShutdown(void)
{
    return OSI_OK;
}
