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
 * message header manipulation
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <trace/mail.h>
#include <trace/gen_rgy.h>
#if defined(OSI_KERNELSPACE_ENV)
#include <trace/KERNEL/gen_rgy.h>
#endif

/*
 * prepare header fields in an outgoing message
 *
 * [IN] msg -- pointer to message structure
 * [IN] dst -- destination gen id (or mcast or bcast address)
 * [IN] msg_xid -- transaction id for this message
 * [IN] msg_ref_xid -- reference transaction id
 * [IN] proto_type -- upper level message type
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_prepare_send(osi_trace_mail_message_t * msg,
			    osi_trace_gen_id_t dst,
			    osi_trace_mail_xid_t msg_xid,
			    osi_trace_mail_xid_t msg_ref_xid,
			    osi_trace_mail_msg_t proto_type)
{
    msg->envelope.env_proto_version = 1;
    msg->envelope.env_len = sizeof(osi_trace_mail_envelope_t);
    msg->envelope.env_dst = dst;
    msg->envelope.env_xid = msg_xid;
    msg->envelope.env_ref_xid = msg_ref_xid;
    msg->envelope.env_proto_type = (osi_uint16) proto_type;
    msg->envelope.env_flags = 0;
    return osi_trace_gen_id(&msg->envelope.env_src);
}

/*
 * prepare a response message
 *
 * [IN] msg    -- pointer to message structure
 * [IN] rmsg  -- pointer to our response message
 * [IN] msg_xid -- transaction id for this message
 * [IN] proto_type -- upper level message type
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_mail_prepare_response(osi_trace_mail_message_t * msg,
				osi_trace_mail_message_t * rmsg,
				osi_trace_mail_xid_t msg_xid,
				osi_trace_mail_msg_t proto_type)
{
    rmsg->envelope.env_proto_version = 1;
    rmsg->envelope.env_len = sizeof(osi_trace_mail_envelope_t);
    rmsg->envelope.env_dst = msg->envelope.env_src;
    rmsg->envelope.env_xid = msg_xid;
    rmsg->envelope.env_ref_xid = msg->envelope.env_xid;
    rmsg->envelope.env_proto_type = (osi_uint16) proto_type;
    rmsg->envelope.env_flags = 
	(msg->envelope.env_flags & OSI_TRACE_MAIL_ENV_FLAG_RPC_REQ) << 1;
    return osi_trace_gen_id(&rmsg->envelope.env_src);
}
