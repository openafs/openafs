/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_AGENT_CONSOLE_H
#define _OSI_TRACE_AGENT_CONSOLE_H 1

#include <osi/osi_list.h>
#include <trace/gen_rgy.h>
#include <trace/agent/trap.h>
#include <trace/agent/console_types.h>

/*
 * osi tracing framework
 * trace agent library
 * console registry
 */



osi_extern osi_result osi_trace_console_rgy_register(osi_trace_console_handle_t *,
						     osi_trace_console_addr_t * addr_vec,
						     osi_size_t addr_vec_len);
osi_extern osi_result osi_trace_console_rgy_unregister(osi_trace_console_handle_t *);

osi_extern osi_result osi_trace_console_rgy_enable(osi_trace_console_handle_t *);
osi_extern osi_result osi_trace_console_rgy_disable(osi_trace_console_handle_t *);

osi_extern osi_result osi_trace_console_rgy_probe_enable(osi_trace_console_handle_t *,
							 osi_trace_gen_id_t,
							 const char * filter,
							 osi_uint32 * nhits);
osi_extern osi_result osi_trace_console_rgy_probe_disable(osi_trace_console_handle_t *,
							  osi_trace_gen_id_t,
							  const char * filter,
							  osi_uint32 * nhits);

osi_extern osi_result osi_trace_console_trap(osi_trace_gen_id_t,
					     osi_trace_probe_id_t,
					     osi_trace_trap_encoding_t,
					     void * trap_payload,
					     osi_size_t trap_payload_len);

OSI_INIT_FUNC_PROTOTYPE(osi_trace_console_rgy_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_trace_console_rgy_PkgShutdown);

#endif /* _OSI_TRACE_AGENT_CONSOLE_H */
