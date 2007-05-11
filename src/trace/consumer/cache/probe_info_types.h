/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_TYPES_H
#define _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_TYPES_H 1


/*
 * osi tracing framework
 * trace consumer library
 * probe metadata cache
 * types
 */

#include <osi/osi_mutex.h>
#include <osi/osi_refcnt.h>
#include <osi/osi_event.h>
#include <trace/common/probe.h>
#include <trace/consumer/cache/object_types.h>

typedef struct osi_trace_consumer_probe_arg_info_cache {
    /* argument stability metadata */
    osi_trace_probe_stability_t osi_volatile arg_stability;
} osi_trace_consumer_probe_arg_info_cache_t;

typedef struct osi_trace_consumer_probe_info_cache {
    osi_trace_consumer_cache_object_header_t hdr;

    /* probe type metadata */
    osi_trace_probe_type_t probe_type;

    /*
     * BEGIN fields synchronized by lock 
     */

    /* probe stability metadata */
    osi_trace_probe_stability_t osi_volatile probe_stability;

    /* vector of argument metadata structs */
    osi_trace_consumer_probe_arg_info_cache_t * arg_vec;

    /* index of cache pool from which arg_vec was allocated */
    osi_uint8 osi_volatile arg_cache_index;

    /* number of arguments associated with this probe */
    osi_uint8 osi_volatile arg_count;
    osi_uint8 osi_volatile arg_valid;
    osi_uint8 spare1;

    /* probe info change watcher */
    osi_event_hook_t hook;

    /*
     * END fields synchronized by lock
     */

    /* index of cache pool from which this object was allocated */
    osi_uint8 osi_volatile probe_cache_index;
    osi_uint8 spare2[3];

    osi_refcnt_t refcnt;
    osi_mutex_t lock;

    char probe_name[1];
} osi_trace_consumer_probe_info_cache_t;


#endif /* _OSI_TRACE_CONSUMER_CACHE_PROBE_INFO_TYPES_H */
