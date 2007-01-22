/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_MODULE_H
#define _OSI_TRACE_GENERATOR_MODULE_H 1

/*
 * modular tracepoint table support
 */

#include <trace/common/module.h>
#include <trace/gen_rgy.h>

osi_extern osi_result osi_trace_module_register(osi_trace_module_header_t *);
osi_extern osi_result osi_trace_module_unregister(osi_trace_module_header_t *);

osi_extern osi_result osi_trace_module_info(osi_trace_generator_info_t *);

osi_extern osi_result osi_trace_module_lookup(osi_trace_module_id_t,
					      osi_trace_module_info_t *);
osi_extern osi_result osi_trace_module_lookup_by_name(const char * name,
						      osi_trace_module_info_t *);
osi_extern osi_result osi_trace_module_lookup_by_prefix(const char * prefix,
							osi_trace_module_info_t *);

osi_extern osi_result osi_trace_module_PkgInit(void);
osi_extern osi_result osi_trace_module_PkgShutdown(void);

#endif /* _OSI_TRACE_GENERATOR_MODULE_H */
