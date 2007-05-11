/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
 * _osi_mem_object_cache_create(cache,name,size,align,rock,ctor,dtor,reclaim)
 *   -- internal function to initialize the object cache
 *
 * osi_result
 * _osi_mem_object_cache_destroy(cache)
 *   -- internal function to destory the object cache
 *
 * void * _osi_mem_object_cache_alloc(cache)
 *   -- internal function to allocate an object
 *
 * void * _osi_mem_object_cache_alloc_nosleep(cache)
 *   -- internal function to allocate an object
 *
 * void _osi_mem_object_cache_free(cache, buf)
 *   -- internal function to free an object
 *
 *
 * macro backend declaration interfaces:
 *
 * _OSI_MEM_OBJECT_CACHE_IMPL_DECL_CREATE() {
 *    cache->handle = ...
 *    ....
 *    return OSI_OK;
 * }
 *
 * _OSI_MEM_OBJECT_CACHE_IMPL_DECL_DESTROY() {
 *    ...
 * }
 *
 * _OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC() {
 *    ...
 * }
 *
 * _OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC_NOSLEEP() {
 *    ...
 * }
 *
 * _OSI_MEM_OBJECT_CACHE_IMPL_DECL_FREE() {
 *    ...
 * }
 *
 * internal implementations should not require any .c files; the interfaces can
 * be implemented as inlines and/or macros.  They will only show up in one
 * compilation unit -- ${CC} -c src/osi/COMMON/object_cache.c
 */

#define _OSI_MEM_OBJECT_CACHE_IMPL_DECL_CREATE() \
    osi_static osi_inline osi_result \
    _osi_mem_object_cache_create(osi_mem_object_cache_t * cache, \
                                 char * name, \
                                 osi_size_t size, \
                                 osi_size_t align, \
                                 void * rock, \
                                 osi_mem_object_cache_constructor_t * ctor, \
                                 osi_mem_object_cache_destructor_t * dtor, \
                                 osi_mem_object_cache_reclaim_t * reclaim)
#define _OSI_MEM_OBJECT_CACHE_IMPL_DECL_DESTROY() \
    osi_static osi_inline osi_result \
    _osi_mem_object_cache_destroy(osi_mem_object_cache_t * cache)
#define _OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC() \
    osi_static osi_inline void * \
    _osi_mem_object_cache_alloc(osi_mem_object_cache_t * cache)
#define _OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC_NOSLEEP() \
    osi_static osi_inline void * \
    _osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t * cache)
#define _OSI_MEM_OBJECT_CACHE_IMPL_DECL_FREE() \
    osi_static osi_inline void \
    _osi_mem_object_cache_free(osi_mem_object_cache_t * cache, \
                                void * buf)


/* 
 * ctor/dtor/reclaim functions for backends 
 * which don't support NULL handler pointers 
 */
OSI_MEM_OBJECT_CACHE_CTOR_PROTOTYPE(osi_mem_object_cache_dummy_ctor);
OSI_MEM_OBJECT_CACHE_DTOR_PROTOTYPE(osi_mem_object_cache_dummy_dtor);
OSI_MEM_OBJECT_CACHE_RECLAIM_PROTOTYPE(osi_mem_object_cache_dummy_reclaim);


/* now include the right back-end implementation header */
#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_SUN5_ENV)
#include <osi/SOLARIS/kmem_object_cache_impl.h>
#endif

#else /* !OSI_ENV_KERNELSPACE */

#if defined(OSI_SUN59_ENV)
#include <osi/SOLARIS/umem_object_cache_impl.h>
#endif

#endif /* !OSI_ENV_KERNELSPACE */

#include <osi/LEGACY/object_cache_impl.h>


#endif /* _OSI_COMMON_OBJECT_CACHE_IMPL_H */
