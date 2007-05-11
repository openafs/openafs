/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
 *
 * types:
 *
 *   osi_trace_console_proc_iterator_t
 *     -- process iterator function prototype
 *
 *
 * macros:
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_DECL(sym)
 *     -- declare an iteration function of name $sym$
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_PROTOTYPE(sym)
 *     -- emit a prototype for an iteration function of name $sym$
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_STATIC_DECL(sym)
 *     -- declare a static iteration function of name $sym$
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_STATIC_PROTOTYPE(sym)
 *     -- emit a prototype for a static iteration function of name $sym$
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_AGENT
 *     -- access the osi_Trace_rpc_addr4 * agent pointer arg in an iterator
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_INFO
 *     -- access the osi_Trace_rpc_proc_info * pointer arg in an iterator
 *
 *   OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_ROCK
 *     -- access the void * rock pointer arg in an iterator
 *
 */

#include <trace/console/proc_client_types.h>
#include <trace/rpc/agent_api.h>

#define OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_AGENT agent
#define OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_INFO  info
#define OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_ROCK  rock

#define OSI_TRACE_CONSOLE_PROC_ITERATOR_DECL(sym) \
osi_result \
sym (osi_Trace_rpc_addr4 * OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_AGENT, \
     osi_Trace_rpc_proc_info * OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_INFO, \
     void * OSI_TRACE_CONSOLE_PROC_ITERATOR_FUNC_ARG_ROCK)
#define OSI_TRACE_CONSOLE_PROC_ITERATOR_PROTOTYPE(sym) \
osi_extern OSI_TRACE_CONSOLE_PROC_ITERATOR_DECL(sym)
#define OSI_TRACE_CONSOLE_PROC_ITERATOR_STATIC_DECL(sym) \
osi_static OSI_TRACE_CONSOLE_PROC_ITERATOR_DECL(sym)
#define OSI_TRACE_CONSOLE_PROC_ITERATOR_STATIC_PROTOTYPE(sym) \
osi_static OSI_TRACE_CONSOLE_PROC_ITERATOR_DECL(sym)


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
			       void * rock);

osi_extern osi_result
osi_trace_console_proc_list(osi_Trace_rpc_addr4 * agent,
			    osi_Trace_rpc_proc_info * proc_vec,
			    osi_size_t proc_vec_len);

OSI_INIT_FUNC_PROTOTYPE(osi_trace_console_proc_client_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_trace_console_proc_client_PkgShutdown);

#endif /* _OSI_TRACE_CONSOLE_PROC_CLIENT_H */
