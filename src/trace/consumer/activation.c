/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_string.h>
#include <trace/consumer/activation.h>
#include <trace/syscall.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/mail/rpc.h>
#include <trace/USERSPACE/mail.h>

/*
 * osi tracing framework
 * trace data consumer library
 * tracepoint activation control
 */

osi_static osi_result osi_TracePoint_Enable_msg(osi_trace_gen_id_t gen,
						osi_trace_probe_id_t probe);
osi_static osi_result osi_TracePoint_Disable_msg(osi_trace_gen_id_t gen,
						 osi_trace_probe_id_t probe);
osi_static osi_result osi_TracePoint_EnableByFilter_msg(osi_trace_gen_id_t gen,
							const char * filter,
							osi_uint32 * nhits);
osi_static osi_result osi_TracePoint_DisableByFilter_msg(osi_trace_gen_id_t gen,
							 const char * filter,
							 osi_uint32 * nhits);


osi_static osi_result
osi_TracePoint_Enable_msg(osi_trace_gen_id_t gen,
			  osi_trace_probe_id_t probe)
{
    osi_trace_mail_msg_probe_activate_id_request_t * req;
    osi_trace_mail_msg_result_code_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_activate_id_request_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_probe_activate_id_request_t * ) msg->body;
    req->spare = 0;
    req->probe_id = probe;

    code = osi_trace_mail_prepare_send(msg, 
				       gen, 
				       xid, 
				       0, 
				       OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_ID);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_result_code_t * ) rmsg->body;
    code = (osi_result) res->code;

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
 * activate a tracepoint by id
 *
 * [IN] gen -- generator id
 * [IN] probe -- probe id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on activation failure
 */
osi_result
osi_TracePoint_Enable(osi_trace_gen_id_t gen,
		      osi_trace_probe_id_t probe)
{
    int rv;
    osi_result res;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_ENABLE_BY_ID,
				(long) probe, 
				0, 
				0,
				&rv);
    } else {
	res = osi_TracePoint_Enable_msg(gen, probe);
    }

 error:
    return res;
}

osi_static osi_result
osi_TracePoint_Disable_msg(osi_trace_gen_id_t gen,
			   osi_trace_probe_id_t probe)
{
    osi_trace_mail_msg_probe_deactivate_id_request_t * req;
    osi_trace_mail_msg_result_code_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_deactivate_id_request_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_probe_deactivate_id_request_t * ) msg->body;
    req->spare = 0;
    req->probe_id = probe;

    code = osi_trace_mail_prepare_send(msg, 
				       gen, 
				       xid, 
				       0, 
				       OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_ID);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_result_code_t *) rmsg->body;
    code = (osi_result) res->code;

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
 * deactivate a tracepoint by id
 *
 * [IN] gen -- generator id
 * [IN] probe -- probe id
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on deactivation failure
 */
osi_result
osi_TracePoint_Disable(osi_trace_gen_id_t gen,
		       osi_trace_probe_id_t probe)
{
    int rv;
    osi_result res;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_DISABLE_BY_ID,
				(long) probe, 
				0, 
				0,
				&rv);
    } else {
	res = osi_TracePoint_Disable_msg(gen, probe);
    }

 error:
    return res;
}

osi_static osi_result
osi_TracePoint_EnableByFilter_msg(osi_trace_gen_id_t gen,
				  const char * filter,
				  osi_uint32 * nhits)
{
    osi_trace_mail_msg_probe_activate_request_t * req;
    osi_trace_mail_msg_probe_activate_response_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_activate_request_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_probe_activate_request_t * ) msg->body;
    req->spare = 0;
    osi_string_ncpy(req->probe_filter, filter, sizeof(req->probe_filter));

    code = osi_trace_mail_prepare_send(msg, 
				       gen, 
				       xid, 
				       0, 
				       OSI_TRACE_MAIL_MSG_PROBE_ACTIVATE_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_probe_activate_response_t * ) rmsg->body;
    code = (osi_result) res->code;
    if (OSI_RESULT_OK(code)) {
	*nhits = res->nhits;
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
 * activate tracepoints matching filter
 *
 * [IN] gen       -- generator id
 * [IN] filter    -- probe name filter
 * [INOUT] nhits  -- address in which to store number of probes hit by filter
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on activation failure
 */
osi_result
osi_TracePoint_EnableByFilter(osi_trace_gen_id_t gen,
			      const char * filter,
			      osi_uint32 * nhits)
{
    int rv;
    osi_result res;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_ENABLE,
				(long) filter, 
				(long) nhits, 
				0,
				&rv);
    } else {
	res = osi_TracePoint_EnableByFilter_msg(gen, filter, nhits);
    }

 error:
    return res;
}

osi_static osi_result
osi_TracePoint_DisableByFilter_msg(osi_trace_gen_id_t gen,
				   const char * filter,
				   osi_uint32 * nhits)
{
    osi_trace_mail_msg_probe_deactivate_request_t * req;
    osi_trace_mail_msg_probe_deactivate_response_t * res;
    osi_trace_mail_message_t * rmsg, * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_deactivate_request_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_probe_deactivate_request_t * ) msg->body;
    req->spare = 0;
    osi_string_ncpy(req->probe_filter, filter, sizeof(req->probe_filter));

    code = osi_trace_mail_prepare_send(msg, 
				       gen, 
				       xid, 
				       0, 
				       OSI_TRACE_MAIL_MSG_PROBE_DEACTIVATE_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_rpc_call(msg, &rmsg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    res = (osi_trace_mail_msg_probe_deactivate_response_t * ) rmsg->body;
    code = (osi_result) res->code;
    if (OSI_RESULT_OK(code)) {
	*nhits = res->nhits;
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
 * deactivate tracepoints matching filter
 *
 * [IN] gen       -- generator id
 * [IN] filter    -- probe name filter
 * [INOUT] nhits  -- address in which to store number of probes hit by filter
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on deactivation failure
 */
osi_result
osi_TracePoint_DisableByFilter(osi_trace_gen_id_t gen,
			       const char * filter,
			       osi_uint32 * nhits)
{
    int rv;
    osi_result res = OSI_OK;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_DISABLE,
				(long) filter, 
				(long) nhits, 
				0,
				&rv);
    } else {
	res = osi_TracePoint_DisableByFilter_msg(gen, filter, nhits);
    }

 error:
    return res;
}
