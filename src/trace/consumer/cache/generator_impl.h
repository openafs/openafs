/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_GENERATOR_IMPL_H
#define _OSI_TRACE_CONSUMER_CACHE_GENERATOR_IMPL_H 1


/*
 * osi tracing framework
 * trace consumer library
 * trace generator metadata cache
 * implementation details
 *
 * do not include outside of cache framework
 */

#include <osi/osi_object_cache.h>
#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>
#include <trace/consumer/cache/ptr_vec_types.h>

struct osi_trace_consumer_gen_cache_directory {
    /*
     * BEGIN sync block
     * the following fields are IMMUTABLE after package initialization
     */
    /* osi_trace_consumer_gen_cache_t memory object cache */
    osi_mem_object_cache_t * cache;
    /*
     * END sync block
     */

    /*
     * BEGIN sync block
     * the following fields are synchronized by lock
     */
    /* vector of pointers to gen cache structs
     * indexed by gen_id */
    osi_trace_consumer_cache_ptr_vec_t gen_vec;

    /* list of all gen cache structs */
    osi_list_head_volatile gen_list;
    /*
     * END sync block
     */

    osi_rwlock_t lock;
};


#endif /* _OSI_TRACE_CONSUMER_CACHE_GENERATOR_IMPL_H */
