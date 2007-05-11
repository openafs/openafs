/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_KMEM_OBJECT_CACHE_IMPL_H
#define _OSI_SOLARIS_KMEM_OBJECT_CACHE_IMPL_H 1

/*
 * osi mem object cache interface
 * solaris kmem slab allocator backend
 * implementation-private details
 */

#include <osi/SOLARIS/kmem_object_cache_impl_types.h>
#include <osi/SOLARIS/kmem_object_cache_types.h>

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_CREATE()
{
    osi_result res = OSI_OK;
    cache->handle = kmem_cache_create(name, 
				      size, 
				      align, 
				      ctor, 
				      dtor, 
				      reclaim, 
				      rock, 
				      NULL, 
				      0);
    if (osi_compiler_expect_false(cache->handle == osi_NULL)) {
	res = OSI_FAIL;
    }
    return res;
}

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_DESTROY()
{
    kmem_cache_destroy(cache->handle);
    return OSI_OK;
}

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC()
{
    return kmem_cache_alloc(cache->handle, KM_SLEEP);
}
_OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC_NOSLEEP()
{
    return kmem_cache_alloc(cache->handle, KM_NOSLEEP);
}
_OSI_MEM_OBJECT_CACHE_IMPL_DECL_FREE()
{
    kmem_cache_free(cache->handle, buf);
}

#endif /* _OSI_SOLARIS_KMEM_OBJECT_CACHE_IMPL_H */
