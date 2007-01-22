/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_MEM_OBJECT_CACHE_TYPES_H
#define	_OSI_LEGACY_MEM_OBJECT_CACHE_TYPES_H

/*
 * osi mem object cache interface
 * legacy backend
 * type definitions
 */

#if defined(OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE)

#include <osi/osi_refcnt.h>

/*
 * this is a highly simplistic object allocator emulation layer for
 * platforms which don't have an object cache interface in their allocators
 *
 * XXX when I find time, this should become osi_mem_local aware, at the very least...
 */

typedef struct {
    char * name;
    osi_size_t len;
    osi_size_t align;
    osi_mem_object_cache_constructor_t * ctor;
    osi_mem_object_cache_destructor_t * dtor;
    osi_mem_object_cache_reclaim_t * reclaim;
    void * sdata;
    osi_refcnt_t cache_usage;    /* count of outstanding allocations + 1 */
} _osi_mem_object_cache_handle_t;

#endif /* OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE */

#endif /* _OSI_LEGACY_MEM_OBJECT_CACHE_TYPES_H */
