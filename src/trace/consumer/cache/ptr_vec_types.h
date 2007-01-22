/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PTR_VEC_TYPES_H
#define _OSI_TRACE_CONSUMER_CACHE_PTR_VEC_TYPES_H 1


/*
 * osi tracing framework
 * trace consumer library
 * cache pointer vector
 * types
 */

#include <osi/osi_mutex.h>
#include <osi/osi_refcnt.h>
#include <osi/osi_event.h>
#include <trace/consumer/cache/object_types.h>

/* pointer vector type */
typedef struct osi_trace_consumer_cache_ptr_vec {
    osi_trace_consumer_cache_object_header_t hdr;
    size_t osi_volatile vec_len;
    osi_trace_consumer_cache_object_ptr_t * osi_volatile vec;
} osi_trace_consumer_cache_ptr_vec_t;


#endif /* _OSI_TRACE_CONSUMER_CACHE_PTR_VEC_TYPES_H */
