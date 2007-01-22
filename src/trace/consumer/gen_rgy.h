/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_GEN_RGY_H
#define _OSI_TRACE_CONSUMER_GEN_RGY_H 1


/*
 * osi tracing framework
 * generator registry
 * kernel support
 */

#include <trace/mail.h>
#include <trace/USERSPACE/mail.h>
#include <trace/gen_rgy.h>


osi_extern osi_result osi_trace_gen_rgy_sys_lookup_I2A(osi_trace_gen_id_t,
						       osi_trace_generator_address_t *);
osi_extern osi_result osi_trace_gen_rgy_sys_lookup_A2I(osi_trace_generator_address_t *,
						       osi_trace_gen_id_t *);

#endif /* _OSI_TRACE_CONSUMER_GEN_RGY_H */
