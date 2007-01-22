/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ANALYZER_VAR_H
#define _OSI_TRACE_ANALYZER_VAR_H 1

/*
 * osi tracing framework
 * data analysis library
 * counter component
 */

#include <trace/analyzer/var_types.h>

osi_extern osi_result osi_trace_anly_var_get(osi_trace_anly_var_t *);
osi_extern osi_result osi_trace_anly_var_put(osi_trace_anly_var_t *);

osi_extern osi_result osi_trace_anly_var_update(osi_trace_anly_var_t *,
						osi_trace_anly_var_update_t *);

osi_extern osi_result osi_trace_anly_var_PkgInit(void);
osi_extern osi_result osi_trace_anly_var_PkgShutdown(void);

#endif /* _OSI_TRACE_ANALYZER_VAR_H */
