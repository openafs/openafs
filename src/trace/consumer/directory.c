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
 * probe point directory
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_string.h>
#include <trace/consumer/directory.h>
#include <trace/syscall.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/mail/rpc.h>
#include <trace/USERSPACE/mail.h>

osi_static osi_result osi_trace_directory_N2I_msg(osi_trace_gen_id_t gen,
						  const char * probe_name,
						  osi_trace_probe_id_t * probe_id);
osi_static osi_result osi_trace_directory_I2N_msg(osi_trace_gen_id_t gen,
						  osi_trace_probe_id_t probe_id,
						  char * probe_name,
						  size_t buf_len);

osi_static osi_result
osi_trace_directory_N2I_msg(osi_trace_gen_id_t gen,
			    const char * probe_name,
			    osi_trace_probe_id_t * probe_id)
{
    osi_trace_mail_msg_probe_n2i_request_t * req;
    osi_trace_mail_msg_probe_n2i_response_t * res;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto done;
    }

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_n2i_request_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_probe_n2i_request_t * ) msg->body;
    req->spare = 0;
    osi_string_ncpy(req->probe_name, probe_name, sizeof(req->probe_name));

    code = osi_trace_mail_prepare_send(msg,
				       gen,
				       xid,
				       0,
				       OSI_TRACE_MAIL_MSG_DIRECTORY_N2I_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = OSI_ERROR_REQUEST_QUEUED;
    
 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }

 done:
    return code;
}

/* 
 * map a probe name to its id 
 *
 * [IN] gen          -- generator id
 * [IN] probe_name   -- name of probe to lookup
 * [INOUT] probe_id  -- address in which to store registered id of probe
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_REQUEST_QUEUED for userspace case
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on lookup failure
 */
osi_result
osi_trace_directory_N2I(osi_trace_gen_id_t gen,
			const char * probe_name, 
			osi_trace_probe_id_t * probe_id)
{
    osi_result res;
    int rv;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_LOOKUP_N2I,
				(long) probe_name, 
				(long) probe_id, 
				0,
				&rv);
    } else {
	res = osi_trace_directory_N2I_msg(gen, probe_name, probe_id);
    }
    return res;
}

osi_static osi_result
osi_trace_directory_I2N_msg(osi_trace_gen_id_t gen,
			    osi_trace_probe_id_t probe_id,
			    char * probe_name,
			    size_t buf_len)
{
    osi_trace_mail_msg_probe_i2n_request_t * req;
    osi_trace_mail_msg_probe_i2n_response_t * res;
    osi_trace_mail_message_t * msg = osi_NULL;
    osi_trace_mail_xid_t xid;
    osi_result code;

    code = osi_trace_mail_msg_alloc(sizeof(osi_trace_mail_msg_probe_i2n_request_t),
				    &msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    req = (osi_trace_mail_msg_probe_i2n_request_t * ) msg->body;
    req->spare = 0;
    req->probe_id = probe_id;

    code = osi_trace_mail_xid_alloc(&xid);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_prepare_send(msg, gen, xid, 0, 
				       OSI_TRACE_MAIL_MSG_DIRECTORY_I2N_REQ);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = osi_trace_mail_send(msg);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    code = OSI_ERROR_REQUEST_QUEUED;

 error:
    if (osi_compiler_expect_true(msg != osi_NULL)) {
	osi_trace_mail_msg_put(msg);
    }
    return code;
}

/* 
 * map a probe id to its name
 *
 * [IN] gen            -- generator id
 * [IN] probe_id       -- id of probe to lookup
 * [INOUT] probe_name  -- address of buffer in which to registered name of probe
 * [IN] buflen         -- length of name buffer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_REQUEST_QUEUED for userspace case
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on probe lookup failure
 */
osi_result
osi_trace_directory_I2N(osi_trace_gen_id_t gen,
			osi_trace_probe_id_t probe_id, 
			char * probe_name, 
			size_t buflen)
{
    osi_result res;
    int rv;

    if (gen == OSI_TRACE_GEN_RGY_KERNEL_ID) {
	res = osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_LOOKUP_I2N,
				(long) probe_id, 
				(long) probe_name, 
				(long) buflen,
				&rv);
    } else {
	res = osi_trace_directory_I2N_msg(gen, probe_id, probe_name, buflen);
    }

    return res;
}

/*
 * probe directory
 * initialization/shutdown routines
 */
osi_result
osi_trace_directory_PkgInit(void)
{
    return OSI_OK;
}

osi_result
osi_trace_directory_PkgShutdown(void)
{
    return OSI_OK;
}
