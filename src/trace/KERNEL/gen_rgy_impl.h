/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_KERNEL_GEN_RGY_IMPL_H
#define _OSI_TRACE_KERNEL_GEN_RGY_IMPL_H 1


/*
 * osi tracing framework
 * generator registry
 * kernel support
 * internal implementation details
 */

#include <trace/KERNEL/postmaster_types.h>
#include <trace/KERNEL/gen_rgy_types.h>

osi_extern void __osi_trace_gen_rgy_free(osi_trace_generator_registration_t *);
osi_extern osi_result osi_trace_gen_rgy_mailbox_get(osi_trace_gen_id_t,
						    osi_trace_mailbox_t **);

#if (OSI_ENV_INLINE_INCLUDE)
#include <trace/KERNEL/gen_rgy_inline.h>
#endif

#endif /* _OSI_TRACE_KERNEL_GEN_RGY_IMPL_H */
