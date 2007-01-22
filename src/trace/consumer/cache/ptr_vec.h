/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PTR_VEC_H
#define _OSI_TRACE_CONSUMER_CACHE_PTR_VEC_H 1


/*
 * osi tracing framework
 * trace consumer library
 * cache pointer vector object
 */

#include <trace/consumer/cache/ptr_vec_types.h>

osi_extern osi_result 
osi_trace_consumer_cache_ptr_vec_Init(osi_trace_consumer_cache_ptr_vec_t *);
osi_extern osi_result 
osi_trace_consumer_cache_ptr_vec_Destroy(osi_trace_consumer_cache_ptr_vec_t *);

osi_extern osi_result
osi_trace_consumer_cache_ptr_vec_extend(osi_trace_consumer_cache_ptr_vec_t *,
					size_t needed_index);
osi_extern osi_result
osi_trace_consumer_cache_ptr_vec_free(osi_trace_consumer_cache_ptr_vec_t *);


osi_extern osi_result
osi_trace_consumer_cache_ptr_vec_PkgInit(void);
osi_extern osi_result
osi_trace_consumer_cache_ptr_vec_PkgShutdown(void);

#endif /* _OSI_TRACE_CONSUMER_CACHE_PTR_VEC_H */
