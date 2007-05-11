/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_BINARY_H
#define _OSI_TRACE_CONSUMER_CACHE_BINARY_H 1


/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * executable binary metadata cache
 */

#include <trace/gen_rgy.h>
#include <trace/consumer/cache/binary_types.h>
#include <trace/consumer/cache/probe_info_types.h>

osi_extern osi_result 
osi_trace_consumer_bin_cache_alloc(osi_trace_generator_info_t *,
				   osi_trace_consumer_bin_cache_t **);
osi_extern osi_result
osi_trace_consumer_bin_cache_register(osi_trace_consumer_bin_cache_t *);
osi_extern osi_result
osi_trace_consumer_bin_cache_unregister(osi_trace_consumer_bin_cache_t *);
osi_extern osi_result
osi_trace_consumer_bin_cache_register_probe(osi_trace_consumer_bin_cache_t *,
					    osi_trace_probe_id_t,
					    osi_trace_consumer_probe_info_cache_t *);
osi_extern osi_result
osi_trace_consumer_bin_cache_unregister_probe(osi_trace_consumer_bin_cache_t *,
					      osi_trace_probe_id_t);
osi_extern osi_result
osi_trace_consumer_bin_cache_get(osi_trace_consumer_bin_cache_t *);
osi_extern osi_result
osi_trace_consumer_bin_cache_put(osi_trace_consumer_bin_cache_t *);
osi_extern osi_result
osi_trace_consumer_bin_cache_lookup(osi_trace_generator_info_t *,
				    osi_trace_consumer_bin_cache_t **);
osi_extern osi_result
osi_trace_consumer_bin_cache_lookup_probe(osi_trace_consumer_bin_cache_t *,
					  osi_trace_probe_id_t,
					  osi_trace_consumer_probe_info_cache_t **);
osi_extern osi_result
osi_trace_consumer_bin_cache_event_subscribe(osi_trace_consumer_bin_cache_t *,
					     osi_event_subscription_t *);

OSI_INIT_FUNC_PROTOTYPE(osi_trace_consumer_bin_cache_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_trace_consumer_bin_cache_PkgShutdown);


#endif /* _OSI_TRACE_CONSUMER_CACHE_BINARY_H */
