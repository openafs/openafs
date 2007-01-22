/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_AGENT_TRAP_CLIENT_H
#define _OSI_TRACE_AGENT_TRAP_CLIENT_H 1

/*
 * osi tracing framework
 * trace agent
 * remote trap interface
 */

#include <rx/rx.h>
#include <trace/consumer/record_types.h>
#include <trace/encoding/trap_types.h>

osi_extern osi_result
osi_trace_agent_trap_send(osi_TracePoint_record_queue_t *);
osi_extern osi_result
osi_trace_agent_trap_send_raw(struct rx_connection ** conns,
			      osi_uint32 * results,
			      osi_uint32 nconns,
			      void * encoding,
			      osi_size_t encoding_size,
			      osi_trace_trap_encoding_t encoding_type,
			      osi_uint32 encoding_version);
osi_extern osi_result
osi_trace_agent_trap_send_vec_raw(struct rx_connection ** conns,
				  osi_uint32 * results,
				  osi_uint32 nconns,
				  void ** encoding,
				  osi_size_t * encoding_size,
				  osi_trace_trap_encoding_t encoding_type,
				  osi_uint32 encoding_version,
				  osi_uint32 nencodings);


osi_extern osi_result
osi_trace_agent_trap_client_PkgInit(void);
osi_extern osi_result
osi_trace_agent_trap_client_PkgShutdown(void);

#endif /* _OSI_TRACE_AGENT_TRAP_CLIENT_H */
