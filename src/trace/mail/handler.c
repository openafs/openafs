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
 * inter-process asynchronous messaging system
 * message receive handler
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_mem.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/mail/handler.h>
#include <trace/mail/rpc_impl.h>

#include <osi/osi_lib_init.h>

osi_lib_init_prototype(__osi_trace_mail_handler_init);

osi_extern osi_init_func_t * __osi_trace_mail_handler_init_ptr;
osi_extern osi_fini_func_t * __osi_trace_mail_handler_fini_ptr;

osi_lib_init_decl(__osi_trace_mail_handler_init)
{
    __osi_trace_mail_handler_init_ptr = &osi_trace_mail_msg_handler_PkgInit;
    __osi_trace_mail_handler_fini_ptr = &osi_trace_mail_msg_handler_PkgShutdown;
}


/*
 * *for the moment*,
 * the handler structure is so bloody simple we don't
 * even need to bother with the overhead of synchronization
 */
struct osi_trace_mail_handler_table {
    osi_trace_mail_msg_handler_t * volatile handler_vec[OSI_TRACE_MAIL_MSG_ID_MAX];
};
struct osi_trace_mail_handler_table osi_trace_mail_handler_table;

/*
 * handle an incoming async message
 *
 * [IN] msg  -- pointer to a message
 *
 * returns:
 *   OSI_TRACE_MAIL_ERROR_BAD_MESSAGE_TYPE
 *     when the header contains an unknown upper level protocol id
 *   OSI_TRACE_MAIL_ERROR_NO_HANDLER 
 *     when no handler is registered for this upper level protocol id
 *   see return codes from handler routine(s)
 */
osi_result
osi_trace_mail_msg_recv_handler(osi_trace_mail_message_t * msg)
{
    osi_result res = OSI_TRACE_MAIL_ERROR_NO_HANDLER;
    osi_trace_mail_msg_t proto;
    osi_trace_mail_msg_handler_t * handler;

    proto = (osi_trace_mail_msg_t) msg->envelope.env_proto_type;
    if (osi_compiler_expect_false(proto >= OSI_TRACE_MAIL_MSG_ID_MAX)) {
	res = OSI_TRACE_MAIL_ERROR_BAD_MESSAGE_TYPE;
	goto done;
    }

    /* if this is an RPC response, try to run any rpc event handlers */
    if (msg->envelope.env_flags & OSI_TRACE_MAIL_ENV_FLAG_RPC_RES) {
	res = osi_trace_mail_rpc_deliver(msg);
    }

    /* check to see if there's a handler func for this message type */
    handler = osi_trace_mail_handler_table.handler_vec[proto];
    if (osi_compiler_expect_false(handler == osi_NULL)) {
	goto done;
    }
    res = (*handler)(msg);

 done:
    return res;
}

/*
 * register an async message handler
 *
 * [IN] proto_type  -- upper level protocol id
 * [IN] handler     -- function pointer to message handler routine
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when an invalid upper level protocol id is passed in
 */
osi_result
osi_trace_mail_msg_handler_register(osi_trace_mail_msg_t proto_type,
				    osi_trace_mail_msg_handler_t * handler)
{
    osi_result res = OSI_OK;

    if (proto_type >= OSI_TRACE_MAIL_MSG_ID_MAX) {
	res = OSI_FAIL;
	goto error;
    }

    osi_trace_mail_handler_table.handler_vec[proto_type] = handler;

 error:
    return res;
}

/*
 * unregister an async message handler
 *
 * [IN] proto_type  -- upper level protocol id
 * [IN] handler     -- function pointer to message handler routine
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when the upper level protocol id is invalid,
 *            when no such handler function was registered
 */
osi_result
osi_trace_mail_msg_handler_unregister(osi_trace_mail_msg_t proto_type,
				      osi_trace_mail_msg_handler_t * handler)
{
    osi_result res = OSI_OK;

    if (proto_type >= OSI_TRACE_MAIL_MSG_ID_MAX) {
	res = OSI_FAIL;
	goto error;
    }

    if (osi_trace_mail_handler_table.handler_vec[proto_type] == handler) {
	osi_trace_mail_handler_table.handler_vec[proto_type] = osi_NULL;
    } else {
	res = OSI_FAIL;
    }

 error:
    return res;
}

osi_result
osi_trace_mail_msg_handler_PkgInit(void)
{
    osi_mem_zero(&osi_trace_mail_handler_table, sizeof(osi_trace_mail_handler_table));
    return OSI_OK;
}

osi_result
osi_trace_mail_msg_handler_PkgShutdown(void)
{
    osi_uint32 i;

    for (i = 0; i < OSI_TRACE_MAIL_MSG_ID_MAX; i++) {
	osi_trace_mail_handler_table.handler_vec[i] = osi_NULL;
    }

    return OSI_OK;
}
