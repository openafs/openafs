/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_MODULE_RGY_H
#define _OSI_TRACE_CONSUMER_MODULE_RGY_H 1


/*
 * osi tracing framework
 * trace consumer library
 * trace module registry
 */

#include <trace/common/module.h>
#include <trace/gen_rgy.h>

osi_extern osi_result osi_trace_module_info(osi_trace_gen_id_t gen,
					    osi_trace_generator_info_t *);
osi_extern osi_result osi_trace_module_lookup(osi_trace_gen_id_t gen,
					      osi_trace_module_id_t id,
					      osi_trace_module_info_t * info);
osi_extern osi_result osi_trace_module_lookup_by_name(osi_trace_gen_id_t gen,
						      const char * name,
						      osi_trace_module_info_t * info);
osi_extern osi_result osi_trace_module_lookup_by_prefix(osi_trace_gen_id_t gen,
							const char * prefix,
							osi_trace_module_info_t * info);

osi_extern osi_result osi_trace_module_PkgInit(void);
osi_extern osi_result osi_trace_module_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_MODULE_RGY_H */
