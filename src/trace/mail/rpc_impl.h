/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_MAIL_RPC_IMPL_H
#define _OSI_TRACE_MAIL_RPC_IMPL_H 1

/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 * Remote Procedure Call layer
 */

#include <trace/mail.h>
#include <trace/mail/msg.h>

osi_extern osi_result osi_trace_mail_rpc_deliver(osi_trace_mail_message_t *);

#endif /* _OSI_TRACE_MAIL_RPC_IMPL_H */
