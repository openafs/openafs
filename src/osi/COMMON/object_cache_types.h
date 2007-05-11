/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_CACHE_TYPES_H
#define _OSI_COMMON_OBJECT_CACHE_TYPES_H 1

/*
 * osi memory object cache
 * common support
 * public type definitions
 */

typedef struct {
    osi_uint8 trace_allowed;       /* whether or not cache tracing is allowed */
    osi_uint8 trace_enabled;       /* enable cache tracing */
} osi_mem_object_cache_options_t;
/* defaults:  { 1, 0 } */

typedef enum {
    OSI_MEM_OBJECT_CACHE_OPTION_TRACE_ALLOWED,
    OSI_MEM_OBJECT_CACHE_OPTION_TRACE_ENABLED,
    OSI_MEM_OBJECT_CACHE_OPTION_MAX_ID
} osi_mem_object_cache_options_param_t;


/* forward declare */
struct osi_mem_object_cache;
typedef struct osi_mem_object_cache osi_mem_object_cache_t;

#endif /* _OSI_COMMON_OBJECT_CACHE_TYPES_H */
