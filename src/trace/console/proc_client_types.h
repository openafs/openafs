/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSOLE_PROC_CLIENT_TYPES_H
#define _OSI_TRACE_CONSOLE_PROC_CLIENT_TYPES_H 1

/*
 * osi tracing framework
 * trace console
 * instrumented process rpc client
 * public type definitions
 */

typedef osi_result osi_trace_console_proc_iterator_t(osi_Trace_rpc_addr4 * agent,
						     osi_Trace_rpc_proc_info * info,
						     void * sdata);

#endif /* _OSI_TRACE_CONSOLE_PROC_CLIENT_TYPES_H */
