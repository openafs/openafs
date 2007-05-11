/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_CACHE_IMPL_TYPES_H
#define _OSI_COMMON_OBJECT_CACHE_IMPL_TYPES_H 1

/*
 * osi mem object cache interface
 * common support
 * implementation-private type definitions
 */

#if !defined(OSI_MEM_OBJECT_CACHE_BACKEND_TYPE_DEFINED)
#error "do not include this header directly; let the backend include it once the appropriate types are defined"
#endif

struct osi_mem_object_cache {
    _osi_mem_object_cache_handle_t handle;
    osi_mem_object_cache_options_t options;
};

#endif /* _OSI_COMMON_OBJECT_CACHE_IMPL_TYPES_H */
