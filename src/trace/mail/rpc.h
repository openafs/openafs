/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_MAIL_RPC_H
#define _OSI_TRACE_MAIL_RPC_H 1

/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 * Remote Procedure Call layer
 */

#include <trace/mail.h>
#include <trace/mail/msg.h>

typedef osi_result osi_trace_mail_rpc_callback_t(osi_trace_mail_message_t *, 
						 void * sdata);

struct osi_trace_mail_rpc_callback_registration;
typedef struct osi_trace_mail_rpc_callback_registration * osi_trace_mail_rpc_callback_handle_t;

osi_extern osi_result osi_trace_mail_rpc_send(osi_trace_mail_message_t *);

/* synchronous rpc api */
osi_extern osi_result osi_trace_mail_rpc_call(osi_trace_mail_message_t * request,
					      osi_trace_mail_message_t ** response);

/* split synchronous rpc api */
osi_extern osi_result osi_trace_mail_rpc_sync_start(osi_trace_mail_xid_t,
						    osi_trace_mail_rpc_callback_handle_t *);
osi_extern osi_result osi_trace_mail_rpc_sync_cancel(osi_trace_mail_rpc_callback_handle_t);
osi_extern osi_result osi_trace_mail_rpc_sync_wait(osi_trace_mail_rpc_callback_handle_t,
						   osi_trace_mail_message_t ** msg_out);
osi_extern osi_result osi_trace_mail_rpc_sync_check(osi_trace_mail_rpc_callback_handle_t,
						    osi_trace_mail_message_t ** msg_out);

/* asynchronous rpc api */
osi_extern osi_result osi_trace_mail_rpc_callback(osi_trace_mail_xid_t,
						  osi_trace_mail_rpc_callback_t *,
						  void * sdata);


osi_extern osi_result osi_trace_mail_rpc_PkgInit(void);
osi_extern osi_result osi_trace_mail_rpc_PkgShutdown(void);

#endif /* _OSI_TRACE_MAIL_RPC_H */
