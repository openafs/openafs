/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_IMPL_H
#define _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_IMPL_H 1


/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * probe metadata cache
 */

#include <osi/osi_object_cache.h>

/* number of entry size buckets */
#define OSI_TRACE_CONSUMER_PROBE_ARG_INFO_CACHE_BUCKETS 4
#define OSI_TRACE_CONSUMER_PROBE_INFO_CACHE_BUCKETS 4


struct osi_trace_consumer_probe_info_cache_directory {
    /*
     * BEGIN sync block
     * the following fields are IMMUTABLE after package initialization
     */
    /* osi_trace_consumer_probe_info_cache_t memory object cache */
    struct {
	osi_mem_object_cache_t * cache;
	osi_uint32 size;
	osi_uint8 index;
	char * name;
	osi_uint32 probe_name_len;
    } cache[OSI_TRACE_CONSUMER_PROBE_INFO_CACHE_BUCKETS];

    struct {
	osi_mem_object_cache_t * cache;
	osi_uint32 size;
	osi_uint8 index;
	char * name;
    } arg_cache[OSI_TRACE_CONSUMER_PROBE_ARG_INFO_CACHE_BUCKETS];
    /*
     * END sync block
     */
};

#endif /* _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_IMPL_H */
