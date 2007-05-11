/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_AGENT_CONSOLE_IMPL_H
#define _OSI_TRACE_AGENT_CONSOLE_IMPL_H 1

#include <trace/agent/console.h>

/*
 * osi tracing framework
 * trace agent library
 * console registry
 */

osi_extern osi_result osi_trace_console_rgy_console_addr_alloc(osi_trace_console_addr_t **,
							       osi_size_t addr_vec_len);
osi_extern osi_result osi_trace_console_rgy_console_addr_free(osi_trace_console_addr_t *);


#endif /* _OSI_TRACE_AGENT_CONSOLE_IMPL_H */
