/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_ACTIVATION_H
#define _OSI_TRACE_CONSUMER_ACTIVATION_H 1


/*
 * osi tracing framework
 * trace data consumer library
 * tracepoint activation control
 */

#include <trace/activation.h>
#include <trace/gen_rgy.h>

osi_extern osi_result osi_TracePoint_Enable(osi_trace_gen_id_t gen,
					    osi_trace_probe_id_t probe);

osi_extern osi_result osi_TracePoint_Disable(osi_trace_gen_id_t gen,
					     osi_trace_probe_id_t probe);

osi_extern osi_result osi_TracePoint_EnableByFilter(osi_trace_gen_id_t gen,
						    const char * filter,
						    osi_uint32 * nhits);

osi_extern osi_result osi_TracePoint_DisableByFilter(osi_trace_gen_id_t gen,
						     const char * filter,
						     osi_uint32 * nhits);

#endif /* _OSI_TRACE_CONSUMER_ACTIVATION_H */
