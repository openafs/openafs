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
#include <trace/syscall.h>
#include <trace/gen_rgy.h>
#include <trace/mail.h>
#include <trace/consumer/gen_rgy.h>
#include <trace/consumer/gen_rgy_mail.h>
#include <trace/consumer/cache/gen_coherency.h>

/*
 * osi tracing framework
 * trace consumer library
 * generator registry mail handlers
 */

/* handle mail related to gen rgy updates */

osi_static osi_result
osi_trace_gen_rgy_msg_up(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK;
    osi_trace_mail_msg_gen_up_t * req;
    osi_trace_gen_id_t gen_id;

    /* parse incoming message */
    req = (osi_trace_mail_msg_gen_up_t *)
	msg->body;

    gen_id = (osi_trace_gen_id_t) req->gen_id;
    code = osi_trace_consumer_cache_notify_gen_up(gen_id);

 error:
    return code;
}

osi_static osi_result
osi_trace_gen_rgy_msg_down(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_gen_down_t * req;
    osi_trace_gen_id_t gen_id;

    /* parse incoming message */
    req = (osi_trace_mail_msg_gen_down_t *)
	msg->body;

    gen_id = (osi_trace_gen_id_t) req->gen_id;
    code = osi_trace_consumer_cache_notify_gen_down(gen_id);

 error:
    return code;
}

osi_result
osi_trace_gen_rgy_msg_PkgInit(void)
{
    osi_result res, code;

    code = OSI_OK;

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_GEN_UP,
					      &osi_trace_gen_rgy_msg_up);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_GEN_DOWN,
					      &osi_trace_gen_rgy_msg_down);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    return code;
}

osi_result
osi_trace_gen_rgy_msg_PkgShutdown(void)
{
    osi_result res, code;

    code = OSI_OK;

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_GEN_UP,
						&osi_trace_gen_rgy_msg_up);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_GEN_DOWN,
						&osi_trace_gen_rgy_msg_down);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    return code;
}
