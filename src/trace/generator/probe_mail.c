/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


/*
 * osi tracing framework
 * trace data generator library
 * probe point data registry
 * mail handlers
 */

#include <trace/common/trace_impl.h>
#include <trace/generator/activation.h>
#include <trace/generator/probe_impl.h>
#include <trace/generator/probe.h>
#include <trace/generator/probe_mail.h>
#include <trace/generator/directory.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/USERSPACE/mail.h>

/* static prototypes */
osi_static osi_result osi_trace_probe_msg_activate(osi_trace_mail_message_t * msg);
osi_static osi_result osi_trace_probe_msg_deactivate(osi_trace_mail_message_t * msg);
osi_static osi_result osi_trace_probe_msg_activate_id(osi_trace_mail_message_t * msg);
osi_static osi_result osi_trace_probe_msg_deactivate_id(osi_trace_mail_message_t * msg);


osi_static osi_result
osi_trace_probe_msg_activate(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_probe_activate_request_t * req;
    osi_trace_mail_msg_probe_activate_response_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;
    osi_uint32 nhits = 0;

    /* parse incoming message */
    req = (osi_trace_mail_msg_probe_activate_request_t *)
	msg->body;
    req->probe_filter[sizeof(req->probe_filter)-1] = '\0';


    /* perform probe activation */
    rcode = osi_trace_probe_enable_by_filter(req->probe_filter,
					     &nhits);


    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_activate_response_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_probe_activate_response_t *) msg_out->body;
    res->spares[0] = 0;
    res->spares[1] = 0;
    res->code = rcode;
    res->nhits = nhits;

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_RES);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg_out != osi_NULL)) {
	osi_trace_mail_msg_put(msg_out);
    }
    return code;
}

osi_static osi_result
osi_trace_probe_msg_deactivate(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_probe_deactivate_request_t * req;
    osi_trace_mail_msg_probe_deactivate_response_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;
    osi_uint32 nhits = 0;

    /* parse incoming message */
    req = (osi_trace_mail_msg_probe_deactivate_request_t *)
	msg->body;
    req->probe_filter[sizeof(req->probe_filter)-1] = '\0';


    /* perform probe activation */
    rcode = osi_trace_probe_disable_by_filter(req->probe_filter,
					      &nhits);


    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_deactivate_response_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_probe_deactivate_response_t * ) msg_out->body;
    res->spares[0] = 0;
    res->spares[1] = 0;
    res->code = rcode;
    res->nhits = nhits;

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_RES);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg_out != osi_NULL)) {
	osi_trace_mail_msg_put(msg_out);
    }
    return code;
}

osi_static osi_result
osi_trace_probe_msg_activate_id(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_probe_activate_id_request_t * req;
    osi_trace_mail_msg_result_code_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;

    /* parse incoming message */
    req = (osi_trace_mail_msg_probe_activate_id_request_t *)
	msg->body;


    /* perform probe activation */
    rcode = osi_trace_probe_enable_by_id(req->probe_id);


    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_result_code_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_result_code_t * ) msg_out->body;
    res->spare = 0;
    res->code = rcode;

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_RET_CODE);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg_out != osi_NULL)) {
	osi_trace_mail_msg_put(msg_out);
    }
    return code;
}

osi_static osi_result
osi_trace_probe_msg_deactivate_id(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_probe_activate_id_request_t * req;
    osi_trace_mail_msg_result_code_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;

    /* parse incoming message */
    req = (osi_trace_mail_msg_probe_activate_id_request_t *)
	msg->body;


    /* perform probe deactivation */
    rcode = osi_trace_probe_disable_by_id(req->probe_id);


    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_result_code_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_result_code_t * ) msg_out->body;
    res->spare = 0;
    res->code = rcode;

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_RET_CODE);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

 error:
    if (osi_compiler_expect_true(msg_out != osi_NULL)) {
	osi_trace_mail_msg_put(msg_out);
    }
    return code;
}

osi_result
osi_trace_probe_rgy_msg_PkgInit(void)
{
    osi_result res, code;

    code = OSI_OK;

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_REQ,
					      &osi_trace_probe_msg_activate);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_REQ,
					      &osi_trace_probe_msg_deactivate);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_ID,
					      &osi_trace_probe_msg_activate_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_ID,
					      &osi_trace_probe_msg_deactivate_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    return code;
}

osi_result
osi_trace_probe_rgy_msg_PkgShutdown(void)
{
    osi_result res, code;

    code = OSI_OK;

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_REQ,
						&osi_trace_probe_msg_activate);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_REQ,
						&osi_trace_probe_msg_deactivate);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_ID,
						&osi_trace_probe_msg_activate_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_ID,
						&osi_trace_probe_msg_deactivate_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    return code;
}
