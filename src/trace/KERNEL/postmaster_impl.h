/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_POSTMASTER_IMPL_H
#define _OSI_TRACE_KERNEL_POSTMASTER_IMPL_H


/*
 * osi tracing framework
 * inter-process asynchronous messaging
 * kernel postmaster
 */

#include <trace/gen_rgy.h>
#include <trace/KERNEL/postmaster_types.h>
#include <trace/KERNEL/gen_rgy_types.h>

/*
 * mailbox control api
 */
osi_extern osi_result osi_trace_mailbox_init(osi_trace_mailbox_t *);
osi_extern osi_result osi_trace_mailbox_destroy(osi_trace_mailbox_t *);
osi_extern osi_result osi_trace_mailbox_open(osi_trace_mailbox_t * mbox);
osi_extern osi_result osi_trace_mailbox_shut(osi_trace_mailbox_t * mbox);
osi_extern osi_result osi_trace_mailbox_clear(osi_trace_mailbox_t * mbox);
osi_extern osi_result osi_trace_mailbox_link(osi_trace_mailbox_t * mbox,
					     osi_trace_generator_registration_t *);
osi_extern osi_result osi_trace_mailbox_unlink(osi_trace_mailbox_t * mbox);
osi_extern osi_result osi_trace_mailbox_gen_id_set(osi_trace_mailbox_t * mbox,
						   osi_trace_gen_id_t);

/* 
 * bcast api 
 */
osi_extern osi_result osi_trace_mail_bcast_add(osi_trace_mailbox_t * mbox);
osi_extern osi_result osi_trace_mail_bcast_del(osi_trace_mailbox_t * mbox);

/*
 * mcast api
 */
osi_extern osi_result osi_trace_mail_mcast_init(osi_trace_gen_id_t mcast_id);
osi_extern osi_result osi_trace_mail_mcast_destroy(osi_trace_gen_id_t mcast_id);
osi_extern osi_result osi_trace_mail_mcast_add(osi_trace_gen_id_t mcast_id, 
					       osi_trace_mailbox_t * mbox);
osi_extern osi_result osi_trace_mail_mcast_del(osi_trace_gen_id_t mcast_id, 
					       osi_trace_mailbox_t * mbox);

#endif /* _OSI_TRACE_KERNEL_POSTMASTER_IMPL_H */
