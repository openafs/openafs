/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UMEM_OBJECT_CACHE_IMPL_H
#define	_OSI_SOLARIS_UMEM_OBJECT_CACHE_IMPL_H

/*
 * osi mem object cache interface
 * solaris libumem backend
 * implementation details
 */

osi_static osi_inline osi_result
_osi_mem_object_cache_create(osi_mem_object_cache_t * cache,
			     char * name,
			     osi_size_t size, size_t align,
			     void * spec_data,
			     osi_mem_object_cache_constructor_t * ctor,
			     osi_mem_object_cache_destructor_t * dtor,
			     osi_mem_object_cache_reclaim_t * reclaim)
{
    osi_result res = OSI_OK;
    cache->handle = umem_cache_create(name, size, align, ctor, dtor, reclaim, spec_data, NULL, 0);
    if (osi_compiler_expect_false(cache->handle == osi_NULL)) {
	res = OSI_FAIL;
    }
    return res;
}

osi_static osi_inline osi_result
_osi_mem_object_cache_destroy(osi_mem_object_cache_t * cache)
{
    umem_cache_destroy(cache->handle);
    return OSI_OK;
}

#define _osi_mem_object_cache_alloc(cache) \
    umem_cache_alloc((cache)->handle, UMEM_DEFAULT)
#define _osi_mem_object_cache_alloc_nosleep(cache) \
    _osi_mem_object_cache_alloc(cache)
#define _osi_mem_object_cache_free(cache, buf) \
    umem_cache_free((cache)->handle, buf)

#endif /* _OSI_SOLARIS_UMEM_OBJECT_CACHE_IMPL_H */
