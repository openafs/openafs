/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_USERSPACE_GEN_RGY_H
#define _OSI_TRACE_USERSPACE_GEN_RGY_H 1


/*
 * osi tracing framework
 * generator registry
 * kernel support
 */

#include <trace/gen_rgy.h>


osi_extern osi_result osi_trace_gen_rgy_register(osi_trace_generator_address_t *, 
						 osi_trace_gen_id_t *);
osi_extern osi_result osi_trace_gen_rgy_unregister(osi_trace_gen_id_t);

osi_extern osi_result osi_trace_gen_rgy_get(osi_trace_gen_id_t);
osi_extern osi_result osi_trace_gen_rgy_put(osi_trace_gen_id_t);

osi_extern osi_result osi_trace_gen_rgy_get_by_addr(osi_trace_generator_address_t *,
						    osi_trace_gen_id_t *);

/* get/set generator address */
osi_extern osi_result osi_trace_gen_id(osi_trace_gen_id_t *);
osi_extern osi_result osi_trace_gen_id_set(osi_trace_gen_id_t);

#endif /* _OSI_TRACE_USERSPACE_GEN_RGY_H */
