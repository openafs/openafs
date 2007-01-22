/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_GEN_COHERENCY_H
#define _OSI_TRACE_CONSUMER_CACHE_GEN_COHERENCY_H 1


/*
 * osi tracing framework
 * trace consumer library
 * trace generator metadata cache
 */

#include <trace/gen_rgy.h>

osi_extern osi_result
osi_trace_consumer_cache_notify_gen_up(osi_trace_gen_id_t);
osi_extern osi_result
osi_trace_consumer_cache_notify_gen_down(osi_trace_gen_id_t);

osi_extern osi_result
osi_trace_consumer_gen_cache_coherency_PkgInit(void);
osi_extern osi_result
osi_trace_consumer_gen_cache_coherency_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_CACHE_GEN_COHERENCY_H */
