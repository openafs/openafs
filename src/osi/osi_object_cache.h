/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_OBJECT_CACHE_H
#define _OSI_OSI_OBJECT_CACHE_H 1

/*
 * platform-independent osi_object_cache API
 * dynamic memory object cache interface
 *
 * this api is modeled after the Solaris kernel slab allocator api
 *
 * for those unfamiliar with slab allocators, the basic idea is to
 * provide a dynamically sized pool of initialized objects which
 * can very quickly be allocated and deallocated without rebuilding
 * their immutable state each time (e.g. initializing mutexes, etc.)
 *
 * the following interfaces require
 * defined(OSI_IMPLEMENTS_MEM_OBJECT_CACHE)
 *
 *  osi_mem_object_cache_t * handle;
 *    -- opaque handle to a memory object cache
 *
 *  osi_mem_object_cache_options_t options;
 *    -- options passed in when creating an object cache
 *
 *  osi_mem_object_cache_t * 
 *  osi_mem_object_cache_create(char * cache_name,
 *                              size_t object_size,
 *                              size_t object_alignment,
 *                              void * spec_data,
 *                              int (*ctor)(void * buf, void * spec_data, int flags),
 *                              void (*dtor)(void * buf, void * spec_data),
 *                              void (*reclaim)(void * spec_data),
 *                              osi_mem_object_cache_options_t * options);
 *    -- create an object cache
 *       $cache_name$ is a human-readable cache name for debugging purposes
 *       $object_size$ is the size of the objects in the cache
 *       $object_alignment$ (if non-zero) specifies the desired alignment; zero specifies default malloc alignment
 *       $spec_data$ is an opaque pointer to pass to ctor, dtor, and reclaim
 *       $ctor$ is a constructor to call when the cache is expanded; can be NULL
 *       $dtor$ is a destructor to call when the cache is shrunk; can be NULL
 *       $reclaim$ is a function which can be called in low memory conditions; can be NULL
 *       $options$ is a pointer to osi-global object cache options; can be NULL
 *
 *  osi_result osi_mem_object_cache_destroy(osi_mem_object_cache_t *);
 *    -- destroy an object cache
 *
 *  void osi_mem_object_cache_options_Init(osi_mem_object_cache_options_t *);
 *    -- initialize the options structure to defaults
 *
 *  void osi_mem_object_cache_options_Destroy(osi_mem_object_cache_options_t *);
 *    -- destroy the options structure
 *
 *  void osi_mem_object_cache_options_Set(osi_mem_object_cache_options_t *, 
 *                                        osi_mem_object_cache_options_param_t, 
 *                                        int val);
 *    -- set the value of a parameter
 *
 *  void * osi_mem_object_cache_alloc(osi_mem_object_cache_t *);
 *    -- allocate an object from the cache; sleeps on low memory for OSI_KERNELSPACE_ENV
 *
 *  void * osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t *);
 *    -- allocate an object from the cache; returns NULL on low memory for OSI_KERNELSPACE_ENV
 *
 *  void osi_mem_object_cache_free(osi_mem_object_cache_t *, void * buf);
 *    -- free an object from the cache
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


/* now include the right back-end implementation header */
#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem_object_cache.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#if defined(OSI_SUN59_ENV)
#include <osi/SOLARIS/umem_object_cache.h>
#endif

#endif /* !OSI_KERNELSPACE_ENV */

#include <osi/LEGACY/object_cache.h>
#include <osi/COMMON/object_cache.h>
#include <osi/COMMON/object_cache_options.h>

#endif /* _OSI_OSI_OBJECT_CACHE_H */
