/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_TIMER_H
#define _OSI_TRACE_ANALYZER_TIMER_H 1

/*
 * osi tracing framework
 * data analysis library
 * timer component
 */

#include <trace/analyzer/var.h>

osi_extern osi_result osi_trace_anly_timer_create(osi_trace_anly_var_t **,
						  osi_trace_anly_fan_in_vec_t *);
osi_extern osi_result osi_trace_anly_timer_destroy(osi_trace_anly_var_t *);

OSI_INIT_FUNC_PROTOTYPE(osi_trace_anly_timer_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_trace_anly_timer_PkgShutdown);

#endif /* _OSI_TRACE_ANALYZER_TIMER_H */
