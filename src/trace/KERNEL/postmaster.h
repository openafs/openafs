/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_POSTMASTER_H
#define _OSI_TRACE_KERNEL_POSTMASTER_H 1


/*
 * osi tracing framework
 * inter-process asynchronous messaging
 * kernel postmaster
 * public interfaces
 */

#include <trace/gen_rgy.h>
#include <trace/mail.h>

/*
 * messaging api
 */
osi_extern osi_result osi_trace_mail_send(osi_trace_mail_message_t * msg);
osi_extern osi_result osi_trace_mail_check(osi_trace_gen_id_t gen_id,
					   osi_trace_mail_message_t ** msg,
					   osi_uint32 flags);

/*
 * tap api
 */
osi_extern osi_result osi_trace_mail_tap_add(osi_trace_gen_id_t tappee,
					     osi_trace_gen_id_t tapper);
osi_extern osi_result osi_trace_mail_tap_del(osi_trace_gen_id_t tappee,
					     osi_trace_gen_id_t tapper);

/* postmaster syscall interfaces */
osi_extern int osi_trace_mail_sys_send(void *);
osi_extern int osi_trace_mail_sys_sendv(void *);
osi_extern int osi_trace_mail_sys_check(osi_trace_gen_id_t,
					void *,
					osi_uint32 flags);
osi_extern int osi_trace_mail_sys_checkv(osi_trace_gen_id_t,
					 void *,
					 osi_uint32 flags,
					 long * retVal);
osi_extern int osi_trace_mail_sys_tap(long tappee,
				      long tapper);
osi_extern int osi_trace_mail_sys_untap(long tappee,
					long tapper);

#if (!OSI_DATAMODEL_IS(OSI_ILP32_ENV))
osi_extern int osi_trace_mail_sys32_send(void *);
osi_extern int osi_trace_mail_sys32_sendv(void *);
osi_extern int osi_trace_mail_sys32_check(osi_trace_gen_id_t,
					  void *,
					  osi_uint32 flags);
osi_extern int osi_trace_mail_sys32_checkv(osi_trace_gen_id_t,
					   void *,
					   osi_uint32 flags,
					   long * retVal);
#endif /* !OSI_DATAMODEL_IS(OSI_ILP32_ENV) */


osi_extern osi_result osi_trace_mail_postmaster_PkgInit(void);
osi_extern osi_result osi_trace_mail_postmaster_PkgShutdown(void);

#endif /* _OSI_TRACE_KERNEL_POSTMASTER_H */
