/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_USERSPACE_MAIL_H
#define _OSI_TRACE_USERSPACE_MAIL_H 1


/*
 * osi tracing framework
 * inter-process asynchronous messaging
 * userspace interface
 */

#include <trace/gen_rgy.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>

osi_extern osi_result osi_trace_mail_send(osi_trace_mail_message_t *);
osi_extern osi_result osi_trace_mail_check(osi_trace_gen_id_t,
					   osi_trace_mail_message_t *,
					   osi_uint32 flags);

osi_extern osi_result osi_trace_mail_sendv(osi_trace_mail_mvec_t *);
osi_extern osi_result osi_trace_mail_checkv(osi_trace_gen_id_t,
					    osi_trace_mail_mvec_t *,
					    osi_uint32 flags);

/* message recv thread */
osi_extern void * osi_trace_mail_receiver(void *);

/* main incoming message handler */
osi_extern osi_result osi_trace_mail_msg_recv_handler(osi_trace_mail_message_t *);


osi_extern osi_result osi_trace_mail_thread_PkgInit(void);
osi_extern osi_result osi_trace_mail_thread_PkgShutdown(void);

#endif /* _OSI_TRACE_USERSPACE_MAIL_H */
