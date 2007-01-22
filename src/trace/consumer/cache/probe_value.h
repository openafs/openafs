/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PROBE_VALUE_H
#define _OSI_TRACE_CONSUMER_CACHE_PROBE_VALUE_H 1


/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * executable binary metadata cache
 */

#include <trace/gen_rgy.h>
#include <trace/consumer/cache/probe_value_types.h>
#include <trace/consumer/cache/probe_info_types.h>

osi_extern osi_result
osi_trace_consumer_probe_val_cache_alloc(osi_trace_consumer_probe_info_cache_t *,
					 osi_trace_consumer_probe_val_cache_t **);
osi_extern osi_result
osi_trace_consumer_probe_val_cache_get(osi_trace_consumer_probe_val_cache_t *);
osi_extern osi_result
osi_trace_consumer_probe_val_cache_put(osi_trace_consumer_probe_val_cache_t *);
osi_extern osi_result 
osi_trace_consumer_probe_val_cache_populate(osi_trace_consumer_probe_val_cache_t *,
					    osi_TracePoint_record *);
osi_extern osi_result
osi_trace_consumer_probe_val_cache_lookup_arg(osi_trace_consumer_probe_val_cache_t *,
					      osi_uint32 arg_index,
					      osi_trace_consumer_probe_arg_val_cache_t *);

osi_extern osi_result
osi_trace_consumer_probe_val_cache_PkgInit(void);
osi_extern osi_result
osi_trace_consumer_probe_val_cache_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_CACHE_PROBE_VALUE_H */
