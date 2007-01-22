/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_COUNTER_H
#define _OSI_TRACE_ANALYZER_COUNTER_H 1

/*
 * osi tracing framework
 * data analysis library
 * counter component
 */

#include <trace/analyzer/var.h>

osi_extern osi_result osi_trace_anly_counter_create(osi_trace_anly_var_t **,
						    osi_trace_anly_fan_in_vec_t *);
osi_extern osi_result osi_trace_anly_counter_destroy(osi_trace_anly_var_t *);

osi_extern osi_result osi_trace_anly_counter_PkgInit(void);
osi_extern osi_result osi_trace_anly_counter_PkgShutdown(void);

#endif /* _OSI_TRACE_ANALYZER_COUNTER_H */
