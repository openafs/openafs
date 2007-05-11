/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_BINARY_TYPES_H
#define _OSI_TRACE_CONSUMER_CACHE_BINARY_TYPES_H 1

#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_event.h>
#include <trace/consumer/cache/object_types.h>
#include <trace/consumer/cache/ptr_vec_types.h>

/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * executable binary metadata cache
 * INTERNAL type definitions
 *
 * including these type definitions
 * outside of the i2n and probe cache
 * subsystems would be a serious
 * layering violation
 */


/* struct which maps probe ids onto names for a specific binary
 * (e.g. there can be multiple gen ids which represent the
 *  same executable) */
typedef struct osi_trace_consumer_bin_cache {
    osi_trace_consumer_cache_object_header_t hdr;

    /* list of all known instrumented binaries */
    osi_list_element_volatile bin_list;

    /* hash chain of instrumented binaries */
    osi_list_element_volatile bin_hash;

    /* binary trace versioning metadata */
    osi_trace_generator_info_t info;

    /* pointers to osi_trace_consumer_probe_info_cache_t objects */
    struct osi_trace_consumer_cache_ptr_vec probe_vec;

    /* binary info change watcher */
    osi_event_hook_t hook;

    osi_refcnt_t refcnt;
} osi_trace_consumer_bin_cache_t;


#endif /* _OSI_TRACE_CONSUMER_CACHE_BINARY_TYPES_H */
