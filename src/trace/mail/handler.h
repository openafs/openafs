/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_MAIL_HANDLER_H
#define _OSI_TRACE_MAIL_HANDLER_H 1

/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 * message receive handler
 */

#include <trace/mail.h>
#include <trace/mail/msg.h>

typedef osi_result osi_trace_mail_msg_handler_t(osi_trace_mail_message_t *);

osi_extern osi_result osi_trace_mail_msg_handler_PkgInit(void);
osi_extern osi_result osi_trace_mail_msg_handler_PkgShutdown(void);

/*
 * register a handler function
 *
 * [IN] proto_type -- env_proto_type field from envelope
 * [IN] handler -- handler function pointer
 */
osi_extern osi_result
osi_trace_mail_msg_handler_register(osi_trace_mail_msg_t proto_type,
				    osi_trace_mail_msg_handler_t * handler);

/*
 * unregister a handler function
 *
 * [IN] proto_type -- env_proto_type field from envelope
 * [IN] handler -- handler function pointer
 */
osi_extern osi_result
osi_trace_mail_msg_handler_unregister(osi_trace_mail_msg_t proto_type,
				      osi_trace_mail_msg_handler_t * handler);

#endif /* _OSI_TRACE_MAIL_HANDLER_H */
