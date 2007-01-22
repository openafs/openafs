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
 * trace data generator library
 * probe point directory
 * mail interface
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/generator/activation.h>
#include <trace/generator/directory.h>
#include <osi/osi_mem.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <osi/osi_atomic.h>
#include <trace/USERSPACE/mail.h>

osi_static osi_result osi_trace_directory_msg_i2n_req(osi_trace_mail_message_t * msg);

/*
 * probe directory
 * async messaging handlers
 */
osi_static osi_result
osi_trace_directory_msg_i2n_req(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_probe_i2n_request_t * req;
    osi_trace_mail_msg_probe_i2n_response_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;

    /* parse incoming message */
    req = (osi_trace_mail_msg_probe_i2n_request_t *) 
	msg->body;

    /* allocate a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_i2n_response_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }
    res = (osi_trace_mail_msg_probe_i2n_response_t * ) msg_out->body;

    /* perform probe i2n lookup */
    rcode = osi_trace_directory_I2N(req->probe_id, res->probe_name, sizeof(res->probe_name));

    osi_mem_zero(res, sizeof(*res));
    res->code = rcode;
    res->probe_id = req->probe_id;

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_RES);
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

/* XXX n2i handler and bulk resolve handlers still to come... */


/*
 * probe directory
 * mail interface
 * initialization routines
 */
osi_result
osi_trace_directory_msg_PkgInit(void)
{
    osi_result code = OSI_OK;

    code = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_REQ,
					       &osi_trace_directory_msg_i2n_req);

    return code;
}

osi_result
osi_trace_directory_msg_PkgShutdown(void)
{
    osi_result code = OSI_OK;

    code = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_REQ,
						 &osi_trace_directory_msg_i2n_req);

    return code;
}
