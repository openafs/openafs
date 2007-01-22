/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_PROBE_H
#define _OSI_TRACE_COMMON_PROBE_H 1


/*
 * osi tracing framework
 * common probe type
 */

#include <osi/osi_time.h>

/*
 * The following enumeration is used to describe cache values within
 * the tracing framework.  Data analyzers use this value to determine
 * when cached analysis inputs should be considered stale.
 */
typedef enum {
    OSI_TRACE_PROBE_CACHE_STABILITY_UNSPECIFIED, /* trace data with unspecified stability */
    OSI_TRACE_PROBE_CACHE_STABILITY_UNSTABLE,    /* trace data which should never be cached */
    OSI_TRACE_PROBE_CACHE_STABILITY_STABLE,      /* trace data which can always be cached */
    OSI_TRACE_PROBE_CACHE_STABILITY_EXPIRABLE,   /* trace data which has a TTL */
    OSI_TRACE_PROBE_CACHE_STABILITY_CLOCKED,     /* trace data which is updated on a timer */
    OSI_TRACE_PROBE_CACHE_STABILITY_MAX_ID
} osi_trace_probe_stability_type_t;

typedef struct {
    osi_trace_probe_stability_type_t stability;
    union {
	osi_time32_t ttl; /* for expirable type */
    } u;
} osi_trace_probe_stability_t;

#endif /* _OSI_TRACE_COMMON_PROBE_H */
