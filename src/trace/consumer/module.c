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
 * trace consumer library
 * trace module registry
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_string.h>
#include <trace/consumer/module.h>
#include <trace/syscall.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/mail/rpc.h>
#include <trace/USERSPACE/mail.h>

osi_static osi_result osi_trace_module_info_msg(osi_trace_gen_id_t gen,
						osi_trace_generator_info_t * info);
osi_static osi_result osi_trace_module_lookup_msg(osi_trace_gen_id_t gen,
						  osi_trace_module_id_t id,
						  osi_trace_module_info_t * info);
osi_static osi_result osi_trace_module_lookup_by_name_msg(osi_trace_gen_id_t gen,
							  const char * module_name,
							  osi_trace_module_info_t * info);
osi_static osi_result osi_trace_module_lookup_by_prefix_msg(osi_trace_gen_id_t gen,
							    const char * module_prefix,
							    osi_trace_module_info_t * info);

osi_static osi_result
osi_trace_module_info_msg(osi_trace_gen_id_t gen,
			  osi_trace_generator_info_t * info)
{
    osi_trace_mail_msg_module_info_req_t * req;
    osi_trace_mail_msg_module_info_res_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_info_req_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_module_info_req_t * ) msg->body;
    osi_mem_zero(req, sizeof(*req));

    code = osi_trace_mail_prepare_send(msg, gen, xid, 0, 
				       OSI_TRACE_MAIL_MSG_MODULE_INFO_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_info_res_t * ) rmsg->body;

    code = (osi_result) res->code;
    if (OSI_RESULT_OK(code)) {
	info->gen_id = rmsg->envelope.env_src;
	info->programType = res->programType;
	info->module_count = res->module_count;
	info->module_version_cksum = res->module_version_cksum;
	info->module_version_cksum_type = res->module_version_cksum_type;
    }

    (void)osi_trace_mail_msg_put(rmsg);

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

/* 
 * get trace module summary from a generator
 *
 * [IN] gen      -- generator id
 * [INOUT] info  -- pointer in which to generator metadata
 */
osi_result
osi_trace_module_info(osi_trace_gen_id_t gen,
		      osi_trace_generator_info_t * info)
{
    osi_result res;
    int rv;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_INFO,
				(long) info, 
				0,
				0,
				&rv);
    } else {
	res = osi_trace_module_info_msg(gen, info);
    }
    return res;
}

osi_static osi_result
osi_trace_module_lookup_msg(osi_trace_gen_id_t gen,
			    osi_trace_module_id_t module_id,
			    osi_trace_module_info_t * info)
{
    osi_trace_mail_msg_module_lookup_req_t * req;
    osi_trace_mail_msg_module_lookup_res_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_lookup_req_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_module_lookup_req_t * ) msg->body;
    req->spare = 0;
    req->module_id = module_id;

    code = osi_trace_mail_prepare_send(msg, gen, xid, 0, 
				       OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_lookup_res_t * ) rmsg->body;

    code = (osi_result) res->code;
    if (OSI_RESULT_OK(code)) {
	info->id = res->module_id;
	info->version = res->module_version;
	res->module_name[sizeof(res->module_name)-1] = '\0';
	res->module_prefix[sizeof(res->module_prefix)-1] = '\0';
	osi_string_lcpy(info->name, res->module_name, sizeof(info->name));
	osi_string_lcpy(info->prefix, res->module_prefix, sizeof(info->prefix));
    }

    (void)osi_trace_mail_msg_put(rmsg);

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

/* 
 * map a module id to its metadata
 *
 * [IN] gen      -- generator id
 * [IN] id       -- module id
 * [INOUT] info  -- place to store module metadata info structure
 */
osi_result
osi_trace_module_lookup(osi_trace_gen_id_t gen,
			osi_trace_module_id_t module_id,
			osi_trace_module_info_t * info)
{
    osi_result res;
    int rv;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP,
				(long) module_id, 
				(long) info, 
				0,
				&rv);
    } else {
	res = osi_trace_module_lookup_msg(gen, module_id, info);
    }

    return res;
}

osi_static osi_result
osi_trace_module_lookup_by_name_msg(osi_trace_gen_id_t gen,
				    const char * module_name,
				    osi_trace_module_info_t * info)
{
    osi_trace_mail_msg_module_lookup_by_name_req_t * req;
    osi_trace_mail_msg_module_lookup_by_name_res_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_lookup_by_name_req_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_module_lookup_by_name_req_t * ) msg->body;
    req->spare = 0;
    osi_string_lcpy(&req->module_name, module_name, sizeof(req->module_name));

    code = osi_trace_mail_prepare_send(msg, gen, xid, 0, 
				       OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_NAME_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_lookup_by_name_res_t * ) rmsg->body;

    code = (osi_result) res->code;
    if (OSI_RESULT_OK(code)) {
	info->id = res->module_id;
	info->version = res->module_version;
	res->module_prefix[sizeof(res->module_prefix)-1] = '\0';
	osi_string_lcpy(info->name, module_name, sizeof(info->name));
	osi_string_lcpy(info->prefix, res->module_prefix, sizeof(info->prefix));
    }

    (void)osi_trace_mail_msg_put(rmsg);

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

/* 
 * map a module name to its metadata
 *
 * [IN] gen      -- generator id
 * [IN] name     -- module name
 * [INOUT] info  -- place to store module metadata info structure
 */
osi_result
osi_trace_module_lookup_by_name(osi_trace_gen_id_t gen,
				const char * module_name,
				osi_trace_module_info_t * info)
{
    osi_result res;
    int rv;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_NAME,
				(long) module_name, 
				(long) info, 
				0,
				&rv);
    } else {
	res = osi_trace_module_lookup_by_name_msg(gen, module_name, info);
    }

    return res;
}

osi_static osi_result
osi_trace_module_lookup_by_prefix_msg(osi_trace_gen_id_t gen,
				      const char * module_prefix,
				      osi_trace_module_info_t * info)
{
    osi_trace_mail_msg_module_lookup_by_prefix_req_t * req;
    osi_trace_mail_msg_module_lookup_by_prefix_res_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_module_lookup_by_prefix_req_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_module_lookup_by_prefix_req_t * ) msg->body;
    req->spare = 0;
    osi_string_lcpy(&req->module_prefix, module_prefix, sizeof(req->module_prefix));

    code = osi_trace_mail_prepare_send(msg, gen, xid, 0, 
				       OSI_TRACE_MAIL_MSG_MODULE_LOOKUP_BY_PREFIX_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_module_lookup_by_prefix_res_t * ) rmsg->body;

    code = (osi_result) res->code;
    if (OSI_RESULT_OK(code)) {
	info->id = res->module_id;
	info->version = res->module_version;
	res->module_name[sizeof(res->module_name)-1] = '\0';
	osi_string_lcpy(info->name, res->module_name, sizeof(info->name));
	osi_string_lcpy(info->prefix, module_prefix, sizeof(info->prefix));
    }

    (void)osi_trace_mail_msg_put(rmsg);

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    osi_trace_mail_xid_retire(xid);

 done:
    return code;
}

/* 
 * map a module prefix to its metadata
 *
 * [IN] gen      -- generator id
 * [IN] prefix   -- module prefix
 * [INOUT] info  -- place to store module metadata info structure
 */
osi_result
osi_trace_module_lookup_by_prefix(osi_trace_gen_id_t gen,
				  const char * module_prefix,
				  osi_trace_module_info_t * info)
{
    osi_result res;
    int rv;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_PREFIX,
				(long) module_prefix, 
				(long) info, 
				0,
				&rv);
    } else {
	res = osi_trace_module_lookup_by_prefix_msg(gen, module_prefix, info);
    }

    return res;
}

/*
 * probe directory
 * initialization routines
 */
osi_result
osi_trace_module_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_module_PkgShutdown(void)
{
    return OSI_OK;
}
