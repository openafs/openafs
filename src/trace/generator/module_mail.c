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
 * trace module registry
 * mail handlers
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_string.h>
#include <trace/generator/module.h>
#include <trace/generator/module_mail.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/USERSPACE/mail.h>

/* static prototypes */
osi_static osi_result osi_trace_module_msg_info(osi_trace_mail_message_t * msg);
osi_static osi_result osi_trace_module_msg_lookup(osi_trace_mail_message_t * msg);
osi_static osi_result osi_trace_module_msg_lookup_by_name(osi_trace_mail_message_t * msg);
osi_static osi_result osi_trace_module_msg_lookup_by_prefix(osi_trace_mail_message_t * msg);


osi_static osi_result
osi_trace_module_msg_info(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_module_info_res_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;
    osi_trace_generator_info_t info;

    rcode = osi_trace_module_info(&info);

    /* allocate a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_info_res_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }
    res = (osi_trace_mail_msg_module_info_res_t *) msg_out->body;
    res->code = (osi_uint32) rcode;
    res->programType = info.programType;
    res->epoch = info.epoch;
    res->probe_count = info.probe_count;
    res->module_count = info.module_count;
    res->module_version_cksum = info.module_version_cksum;
    res->module_version_cksum_type = (osi_uint8) info.module_version_cksum_type;

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_MODULE_INFO_RES);
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
osi_trace_module_msg_lookup(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_module_lookup_req_t * req;
    osi_trace_mail_msg_module_lookup_res_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;
    osi_trace_module_info_t info;

    /* parse incoming message */
    req = (osi_trace_mail_msg_module_lookup_req_t *)
	msg->body;

    /* perform module lookup */
    rcode = osi_trace_module_lookup(req->module_id, &info);
	
    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_lookup_res_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_lookup_res_t *) msg_out->body;
    res->code = rcode;
    if (OSI_RESULT_OK_LIKELY(rcode)) {
	res->module_id = info.id;
	res->module_version = info.version;
	osi_string_lcpy(&res->module_name, info.name, sizeof(res->module_name));
	osi_string_lcpy(&res->module_prefix, info.prefix, sizeof(res->module_prefix));
    }

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_RES);
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
osi_trace_module_msg_lookup_by_name(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_module_lookup_by_name_req_t * req;
    osi_trace_mail_msg_module_lookup_by_name_res_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;
    osi_trace_module_info_t info;

    /* parse incoming message */
    req = (osi_trace_mail_msg_module_lookup_by_name_req_t *)
	msg->body;
    req->module_name[sizeof(req->module_name)-1] = '\0';

    /* perform module lookup */
    rcode = osi_trace_module_lookup_by_name(req->module_name, &info);
	
    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_lookup_by_name_res_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_lookup_by_name_res_t *) msg_out->body;
    res->code = rcode;
    if (OSI_RESULT_OK_LIKELY(rcode)) {
	res->module_id = info.id;
	res->module_version = info.version;
	osi_string_lcpy(&res->module_prefix, info.prefix, sizeof(res->module_prefix));
    }

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_RES);
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
osi_trace_module_msg_lookup_by_prefix(osi_trace_mail_message_t * msg)
{
    osi_result code = OSI_OK, rcode;
    osi_trace_mail_msg_module_lookup_by_prefix_req_t * req;
    osi_trace_mail_msg_module_lookup_by_prefix_res_t * res;
    osi_trace_mail_message_t * msg_out = osi_NULL;
    osi_trace_module_info_t info;

    /* parse incoming message */
    req = (osi_trace_mail_msg_module_lookup_by_prefix_req_t *)
	msg->body;
    req->module_prefix[sizeof(req->module_prefix)-1] = '\0';

    /* perform module lookup */
    rcode = osi_trace_module_lookup_by_prefix(req->module_prefix, &info);
	
    /* send a response message */
    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_lookup_by_prefix_res_t),
				    &msg_out);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_lookup_by_prefix_res_t *) msg_out->body;
    res->code = rcode;
    if (OSI_RESULT_OK_LIKELY(rcode)) {
	res->module_id = info.id;
	res->module_version = info.version;
	osi_string_lcpy(&res->module_name, info.name, sizeof(res->module_name));
    }

    code = osi_trace_mail_prepare_response(msg,
					   msg_out,
					   0,
					   OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_RES);
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
osi_trace_module_msg_PkgInit(void)
{
    osi_result res, code;

    code = OSI_OK;

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_MODULE_INFO_REQ,
					      &osi_trace_module_msg_info);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_REQ,
					      &osi_trace_module_msg_lookup);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_REQ,
					      &osi_trace_module_msg_lookup_by_name);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    res = osi_trace_mail_msg_handler_register(OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_REQ,
					      &osi_trace_module_msg_lookup_by_prefix);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = OSI_FAIL;
    }

    return code;
}

osi_result
osi_trace_module_msg_PkgShutdown(void)
{
    osi_result res, code;

    code = OSI_OK;

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_MODULE_INFO_REQ,
						&osi_trace_module_msg_info);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_REQ,
						&osi_trace_module_msg_lookup);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_REQ,
						&osi_trace_module_msg_lookup_by_name);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    res = osi_trace_mail_msg_handler_unregister(OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_REQ,
						&osi_trace_module_msg_lookup_by_prefix);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
    }

    return code;
}
