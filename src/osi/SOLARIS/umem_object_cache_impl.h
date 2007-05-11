/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_SOLARIS_UMEM_OBJECT_CACHE_IMPL_H
#define _OSI_SOLARIS_UMEM_OBJECT_CACHE_IMPL_H 1

/*
 * osi mem object cache interface
 * solaris libumem backend
 * implementation-private details
 */

#include <osi/SOLARIS/umem_object_cache_impl_types.h>
#include <osi/SOLARIS/umem_object_cache_types.h>

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_CREATE()
{
    osi_result res = OSI_OK;
    cache->handle = umem_cache_create(name, 
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
    umem_cache_destroy(cache->handle);
    return OSI_OK;
}

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC()
{
    return umem_cache_alloc(cache->handle, UMEM_DEFAULT);
}
_OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC_NOSLEEP()
{
    return umem_cache_alloc(cache->handle, UMEM_DEFAULT);
}
_OSI_MEM_OBJECT_CACHE_IMPL_DECL_FREE()
{
    umem_cache_free(cache->handle, buf);
}

#endif /* _OSI_SOLARIS_UMEM_OBJECT_CACHE_IMPL_H */
