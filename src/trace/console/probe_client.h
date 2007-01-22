/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_PROBE_CLIENT_H
#define _OSI_TRACE_CONSOLE_PROBE_CLIENT_H 1

/*
 * osi tracing framework
 * trace console
 * types for trap support
 */

#include <trace/rpc/agent_api.h>

osi_extern osi_result
osi_trace_console_probe_enable(osi_Trace_rpc_addr4 *,
			       osi_Trace_rpc_proc_handle,
			       const char * filter,
			       osi_uint32 * nhits);
osi_extern osi_result
osi_trace_console_probe_disable(osi_Trace_rpc_addr4 *,
				osi_Trace_rpc_proc_handle,
				const char * filter,
				osi_uint32 * nhits);

osi_extern osi_result
osi_trace_console_probe_client_PkgInit(void);
osi_extern osi_result
osi_trace_console_probe_client_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSOLE_PROBE_CLIENT_H */
