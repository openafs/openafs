/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_CONSOLE_REGISTRATION_H
#define _OSI_TRACE_CONSOLE_CONSOLE_REGISTRATION_H 1

/*
 * osi tracing framework
 * trace console
 * registration interface
 */

osi_extern osi_result
osi_trace_console_register(osi_Trace_rpc_addr4 * agent,
			   osi_Trace_rpc_console_options *,
			   osi_Trace_rpc_bulkaddrs4 * addrs);
osi_extern osi_result
osi_trace_console_unregister(osi_Trace_rpc_addr4 * agent);

osi_extern osi_result
osi_trace_console_registration_PkgInit(void);
osi_extern osi_result
osi_trace_console_registration_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSOLE_CONSOLE_REGISTRATION_H */
