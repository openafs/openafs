/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_OBJECT_TYPES_H
#define _OSI_TRACE_CONSUMER_CACHE_OBJECT_TYPES_H 1

/*
 * osi tracing framework
 * trace consumer cache framework
 * INTERNAL object cache types
 *
 * including these type definitions
 * outside of the i2n and probe cache
 * subsystems would be a serious
 * layering violation
 */


/* cache object type */
typedef enum {
    OSI_TRACE_CONSUMER_CACHE_OBJECT_INVALID,
    OSI_TRACE_CONSUMER_CACHE_OBJECT_PROBE_INFO,
    OSI_TRACE_CONSUMER_CACHE_OBJECT_PROBE_VALUE,
    OSI_TRACE_CONSUMER_CACHE_OBJECT_BIN,
    OSI_TRACE_CONSUMER_CACHE_OBJECT_GEN,
    OSI_TRACE_CONSUMER_CACHE_OBJECT_POINTER_VEC,
    OSI_TRACE_CONSUMER_CACHE_OBJECT_MAX_ID
} osi_trace_consumer_cache_object_type_t;


/* forward declare the various object types */
struct osi_trace_consumer_cache_object_header;
struct osi_trace_consumer_bin_cache;
struct osi_trace_consumer_gen_cache;
struct osi_trace_consumer_probe_info_cache;
struct osi_trace_consumer_probe_val_cache;
struct osi_trace_consumer_probe_arg_info_cache;
struct osi_trace_consumer_probe_arg_val_cache;

/* generic cache object pointer type */
typedef union {
    void * osi_volatile opaque;
    struct osi_trace_consumer_cache_object_header * osi_volatile header;
    struct osi_trace_consumer_bin_cache * osi_volatile bin;
    struct osi_trace_consumer_gen_cache * osi_volatile gen;
    struct osi_trace_consumer_probe_info_cache * osi_volatile probe_info;
    struct osi_trace_consumer_probe_val_cache * osi_volatile probe_val;
} osi_trace_consumer_cache_object_ptr_t;



/* object header
 * this is the first element in all objects in the cache */
typedef struct {
    osi_trace_consumer_cache_object_type_t osi_volatile type;
} osi_trace_consumer_cache_object_header_t;


#endif /* _OSI_TRACE_CONSUMER_CACHE_OBJECT_TYPES_H */
