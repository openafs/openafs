/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_GENERATOR_TYPES_H
#define _OSI_TRACE_CONSUMER_CACHE_GENERATOR_TYPES_H 1

#include <osi/osi_list.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_refcnt.h>
#include <osi/osi_event.h>
#include <trace/consumer/cache/object_types.h>
#include <trace/consumer/cache/ptr_vec_types.h>


/*
 * osi tracing framework
 * trace consumer
 * generator cache
 * INTERNAL type definitions
 *
 * including these type definitions
 * outside of the i2n and probe cache
 * subsystems would be a serious
 * layering violation
 */


/*
 * This cache performs id to name translation services on behalf of consumer 
 * processes.  Because there is significant overhead in performing these name
 * lookups (e.g. context switches), we are going to aggressively cache this 
 * data in the consumers.
 *
 * The primary purpose of the cache is to store (gen id, probe id, probe name) 
 * tuples.  The main lookup method allows one to index off (gen id, probe id) 
 * tuples in order to find the associated human-readable probe name.
 *
 * The main cache control structure, osi_trace_consumer_i2n_cache, contains
 * immutable configuration fields, as well as volatile cache state fields.
 * Configuration fields include:
 *   entry_buckets -- the total allocation size of a bucket in each entry cache
 *   name_vec_chunk_size -- osi_trace_consumer_i2n_cache_gen.gen_vec_len is 
 *       always an integer multiple of this value
 *   gen_vec_len -- length of osi_trace_consumer_i2n_cache.gen_vec.  this value
 *       is queried from the kernel during i2n_cache_PkgInit
 * Cache State Fields:
 *   gen_vec -- vector of generators, indexed by gen_id
 *   bin_list -- linked list of all osi_trace_consumer_i2n_cache_bin structures
 *   bin_hash -- hash table of al osi_trace_consumer_i2n_cache_bin structures
 *
 * Each generator for which we have cached probes has an 
 * osi_trace_consumer_i2n_cache_gen structure associated with it.  These 
 * structures are allocated out of the 
 * osi_trace_consumer_i2n_cache_gen_cache object cache.  They are registered in
 * the osi_trace_consumer_i2n_cache structure's gen_vec, indexed by their gen
 * id.  They contain a pointer to a bin structure, and hold a ref on that bin.
 *
 * Each bin object maintains a vector of pointers mapping onto cache
 * entries.  This vector is index by the numerical probe id value.  The size of
 * this vector is controlled dynamically the function
 * osi_trace_consumer_i2n_cache_name_vec_extend().  This allocator keeps the
 * vector len a multiple of osi_trace_consumer_i2n_cache.name_vec_chunk_size.
 *
 * Actual cache data rows are represented by the 
 * osi_trace_consumer_i2n_cache_bin_entry structures.  These structures are 
 * allocated out of the osi_trace_consumer_i2n_cache_bin_entry_cache[] object 
 * caches.  There are currently four of these caches; each for a different 
 * range of probe name string lengths.
 *
 * Cache updates are event-driven, usually triggered by osi_trace_mail handler 
 * functions.
 */

typedef enum {
    OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_PROBE_REGISTER,
    OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_GEN_REGISTER,
    OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_GEN_UNREGISTER,
    OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_MAX_ID
} osi_trace_consumer_gen_cache_event_t;

/* forward declare */
struct osi_trace_consumer_bin_cache;

/* trace generator-specific information */
typedef struct osi_trace_consumer_gen_cache {
    osi_trace_consumer_cache_object_header_t hdr;

    /* list of all known trace generators */
    osi_list_element_volatile gen_list;

    /* generator trace versioning information structure */
    osi_trace_generator_info_t info;

    /* bin object associated with this gen */
    struct osi_trace_consumer_bin_cache * osi_volatile bin;

    /* vector of probe value cache pointers */
    osi_trace_consumer_cache_ptr_vec_t probe_vec;

    /* gen info change watcher */
    osi_event_hook_t hook;

    /* generator object reference count */
    osi_refcnt_t refcnt;
} osi_trace_consumer_gen_cache_t;


#endif /* _OSI_TRACE_CONSUMER_CACHE_GENERATOR_TYPES_H */
