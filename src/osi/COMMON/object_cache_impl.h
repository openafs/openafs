/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_OBJECT_CACHE_IMPL_H
#define _OSI_COMMON_OBJECT_CACHE_IMPL_H 1

/*
 * OSI memory object cache internal interface
 *
 * this file specifies the internal interfaces used by backend object cache
 * drivers.
 *
 * the externally-visible interfaces are implemented here and in
 * src/osi/COMMON/object_cache.c
 *
 *
 * internal drivers define:
 *
 * _osi_mem_object_cache_handle_t
 *   -- internal object cache handle type
 *
 * osi_result
 * _osi_mem_object_cache_create(cache,name,size,align,spec_data,ctor,dtor,reclaim)
 *   -- internal function to initialize the object cache
 *
 * void _osi_mem_object_cache_destroy(cache)
 *   -- internal function to destory the object cache
 *
 * void * _osi_mem_object_cache_alloc(cache)
 *   -- internal function to allocate an object
 *
 * void * _osi_mem_object_cache_alloc_nosleep(cache)
 *   -- internal function to allocate an object
 *
 * void _osi_mem_object_cache_free(cache, ptr)
 *   -- internal function to free an object
 *
 */

#include <osi/COMMON/object_cache_types.h>

#if defined(OSI_KERNELSPACE_ENV)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem_object_cache_impl.h>
#endif

#else /* !OSI_KERNELSPACE_ENV */

#if defined(OSI_SUN59_ENV)
#include <osi/SOLARIS/umem_object_cache_impl.h>
#endif

#endif /* !OSI_KERNELSPACE_ENV */

#include <osi/LEGACY/object_cache_impl.h>

#endif /* _OSI_COMMON_OBJECT_CACHE_IMPL_H */
