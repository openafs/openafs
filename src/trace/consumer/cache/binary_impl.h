/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_BINARY_IMPL_H
#define _OSI_TRACE_CONSUMER_CACHE_BINARY_IMPL_H 1


/*
 * osi tracing framework
 * trace consumer
 * executable binary cache
 * INTERNAL implementation details
 */

/* bin hash parameters */
#define OSI_TRACE_CONSUMER_BIN_CACHE_HASH_BUCKETS_LOG2    2
#define OSI_TRACE_CONSUMER_BIN_CACHE_HASH_BUCKETS         4
#define OSI_TRACE_CONSUMER_BIN_CACHE_HASH_MASK            3

struct osi_trace_consumer_bin_cache_directory {
    /*
     * BEGIN sync block
     * the following fields are IMMUTABLE after package initialization
     */
    /* osi_trace_consumer_bin_cache_t memory object cache */
    osi_mem_object_cache_t * cache;
    /*
     * END sync block
     */

    /* 
     * BEGIN sync block
     * the following fields are synchronized by lock
     */
    /* list of all bin cache structs */
    osi_list_head_volatile bin_list;

    /* hash of all bin cache structs */
    osi_list_head_volatile bin_hash[OSI_TRACE_CONSUMER_BIN_CACHE_HASH_BUCKETS];
    /*
     * END sync block
     */

    osi_rwlock_t lock;
};

#endif /* _OSI_TRACE_CONSUMER_CACHE_BINARY_IMPL_H */
