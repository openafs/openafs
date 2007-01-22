/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PROBE_VALUE_TYPES_H
#define _OSI_TRACE_CONSUMER_CACHE_PROBE_VALUE_TYPES_H 1


/*
 * osi tracing framework
 * trace consumer library
 * probe value cache
 * types
 */

#include <osi/osi_mutex.h>
#include <osi/osi_refcnt.h>
#include <osi/osi_event.h>
#include <trace/common/probe.h>
#include <trace/consumer/cache/object_types.h>

typedef struct osi_trace_consumer_probe_arg_val_cache {
    /* cached probe argument value */
    osi_int64 osi_volatile arg_val;

    /* timestamp of last update */
    osi_time32_t osi_volatile arg_update;
} osi_trace_consumer_probe_arg_val_cache_t;

typedef struct osi_trace_consumer_probe_val_cache {
    osi_trace_consumer_cache_object_header_t hdr;

    /* 
     * BEGIN fields synchronized by lock
     */
    /* pointer to associated probe info object */
    struct osi_trace_probe_info_cache * osi_volatile probe_info;

    /* probe argument value vector */
    osi_trace_consumer_probe_arg_val_cache_t * osi_volatile arg_vec;

    /* probe value change watcher */
    osi_event_hook_t hook;

    /* index of cache pool from which the argument value vector was allocated */
    osi_uint32 osi_volatile arg_cache_index;

    /* timestamp of last tracepoint record */
    osi_time32_t osi_volatile update;

    /* number of valid arguments */
    osi_uint8 osi_volatile arg_valid;
    osi_uint8 spare1[3];
    /*
     * END fields synchronized by lock
     */

    osi_refcnt_t refcnt;
    osi_mutex_t lock;
} osi_trace_consumer_probe_val_cache_t;



#endif /* _OSI_TRACE_CONSUMER_CACHE_PROBE_VALUE_TYPES_H */
