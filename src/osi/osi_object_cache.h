/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
 * types:
 *
 *  osi_mem_object_cache_t * handle;
 *    -- opaque handle to a memory object cache
 *
 *  osi_mem_object_cache_constructor_t *
 *    -- object constructor function type
 *
 *  osi_mem_object_cache_destructor_t *
 *    -- object destructor function type
 *
 *  osi_mem_object_cache_reclaim_t *
 *    -- object cache memory reclaim function type
 *
 *  osi_mem_object_cache_options_t options;
 *    -- options passed in when creating an object cache
 *
 *
 * macros:
 *
 *  OSI_MEM_OBJECT_CACHE_CTOR_PROTOTYPE(sym)
 *    -- emit a prototype for a constructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_CTOR_DECL(sym)
 *    -- emit a declaration for a constructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_CTOR_STATIC_PROTOTYPE(sym)
 *    -- emit a prototype for a static constructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_CTOR_STATIC_DECL(sym)
 *    -- emit a declaration for a static constructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_DTOR_PROTOTYPE(sym)
 *    -- emit a prototype for a destructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_DTOR_DECL(sym)
 *    -- emit a declaration for a destructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_DTOR_STATIC_PROTOTYPE(sym)
 *    -- emit a prototype for a static destructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_DTOR_STATIC_DECL(sym)
 *    -- emit a declaration for a static destructor function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_RECLAIM_PROTOTYPE(sym)
 *    -- emit a prototype for a reclaim function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_RECLAIM_DECL(sym)
 *    -- emit a declaration for a reclaim function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_RECLAIM_STATIC_PROTOTYPE(sym)
 *    -- emit a prototype for a static reclaim function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_RECLAIM_STATIC_DECL(sym)
 *    -- emit a declaration for a static reclaim function of name $sym$
 *
 *  OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF
 *    -- access the buffer pointer argument
 *
 *  OSI_MEM_OBJECT_CACHE_FUNC_ARG_ROCK
 *    -- access the rock opaque pointer argument
 *
 *
 * interfaces:
 *
 *  osi_mem_object_cache_t * 
 *  osi_mem_object_cache_create(char * cache_name,
 *                              osi_size_t object_size,
 *                              osi_size_t object_alignment,
 *                              void * rock,
 *                              osi_mem_object_cache_constructor_t *,
 *                              osi_mem_object_cache_destructor_t *,
 *                              osi_mem_object_cache_reclaim_t *,
 *                              osi_mem_object_cache_options_t * options);
 *    -- create an object cache
 *       $cache_name$ is a human-readable cache name for debugging purposes
 *       $object_size$ is the size of the objects in the cache
 *       $object_alignment$ (if non-zero) specifies the desired alignment; zero specifies default malloc alignment
 *       $rock$ is an opaque pointer to pass to ctor, dtor, and reclaim
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
 *    -- allocate an object from the cache; sleeps on low memory for OSI_ENV_KERNELSPACE
 *
 *  void * osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t *);
 *    -- allocate an object from the cache; returns NULL on low memory for OSI_ENV_KERNELSPACE
 *
 *  void osi_mem_object_cache_free(osi_mem_object_cache_t *, void * buf);
 *    -- free an object from the cache
 */

#include <osi/COMMON/object_cache.h>
#include <osi/COMMON/object_cache_options.h>

#endif /* _OSI_OSI_OBJECT_CACHE_H */
