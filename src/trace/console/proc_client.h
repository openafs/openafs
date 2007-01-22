/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_PROC_CLIENT_H
#define _OSI_TRACE_CONSOLE_PROC_CLIENT_H 1

/*
 * osi tracing framework
 * trace console
 * instrumented process rpc client
 */

#include <trace/console/proc_client_types.h>
#include <trace/rpc/agent_api.h>

osi_extern osi_result
osi_trace_console_proc_agent(osi_Trace_rpc_addr4 * agent,
			     osi_Trace_rpc_proc_handle * agent_proc_handle);

osi_extern osi_result
osi_trace_console_proc_info(osi_Trace_rpc_addr4 * agent,
			    osi_Trace_rpc_proc_handle,
			    osi_Trace_rpc_proc_info * info);

osi_extern osi_result
osi_trace_console_proc_foreach(osi_Trace_rpc_addr4 * agent,
			       osi_trace_console_proc_iterator_t * fp,
			       void * sdata);

osi_extern osi_result
osi_trace_console_proc_client_PkgInit(void);
osi_extern osi_result
osi_trace_console_proc_client_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSOLE_PROC_CLIENT_H */
