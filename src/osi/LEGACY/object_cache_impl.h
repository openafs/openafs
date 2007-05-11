/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_MEM_OBJECT_CACHE_IMPL_H
#define _OSI_LEGACY_MEM_OBJECT_CACHE_IMPL_H 1

/*
 * osi mem object cache interface
 * legacy backend
 * implementation-private details
 */

#if defined(OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE)

#include <osi/osi_mem.h>
#include <osi/LEGACY/object_cache_impl_types.h>
#include <osi/LEGACY/object_cache_types.h>

/* 
 * it's ok to inline these because they are only called from the 
 * wrapper functions in src/osi/COMMON/object_cache.c
 */


_OSI_MEM_OBJECT_CACHE_IMPL_DECL_CREATE()
{
    cache->handle.name = name;
    cache->handle.len = size;
    cache->handle.align = align;
    cache->handle.ctor = ctor;
    cache->handle.dtor = dtor;
    cache->handle.reclaim = reclaim;
    cache->handle.rock = rock;

#if defined(OSI_DEBUG_MEM_OBJECT_CACHE)
    osi_refcnt_init(&cache->handle.cache_usage, 1);
#endif

    return OSI_OK;
}

#if defined(OSI_DEBUG_MEM_OBJECT_CACHE)
/* worker function to call when refcount reaches zero */
osi_static osi_result
osi_mem_legacy_object_cache_destroy(void * rock)
{
    osi_mem_object_cache_t * cache = rock;

    osi_refcnt_destroy(&cache->handle.cache_usage);

    return OSI_OK;
}

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_DESTROY()
{
    osi_result res = OSI_FAIL;
    int code;

    code = osi_refcnt_dec_action(&cache->handle.cache_usage, 
				 0,
				 &osi_mem_legacy_object_cache_destroy, 
				 (void *)cache,
				 &res);

    if (!code || OSI_RESULT_FAIL(res)) {
	(osi_Msg "WARNING: osi_mem_object_cache '%s' was destroyed with actively allocated objects\n",
	 cache->handle.name);
    }
    return res;
}
#else /* !OSI_DEBUG_MEM_OBJECT_CACHE */
_OSI_MEM_OBJECT_CACHE_IMPL_DECL_DESTROY()
{
    return OSI_OK;
}
#endif /* !OSI_DEBUG_MEM_OBJECT_CACHE */


_OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC()
{
    void * ret;
    int code;

    if (cache->handle.align) {
	ret = osi_mem_aligned_alloc(cache->handle.len, cache->handle.align);
    } else {
	ret = osi_mem_alloc(cache->handle.len);
    }
    if (osi_compiler_expect_true(ret != osi_NULL)) {
	if (cache->handle.ctor) {
	    code = (*cache->handle.ctor)(ret, cache->handle.rock, 0);
	    if (osi_compiler_expect_false(code != 0)) {
		goto error;
	    }
	}
#if defined(OSI_DEBUG_MEM_OBJECT_CACHE)
	osi_refcnt_inc(&cache->handle.cache_usage);
#endif
    }

 done:
    return ret;

 error:
    if (cache->handle.align) {
	osi_mem_aligned_free(ret, cache->handle.len);
    } else {
	osi_mem_free(ret, cache->handle.len);
    }
    ret = osi_NULL;
    goto done;
}

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_ALLOC_NOSLEEP()
{
    void * ret;
    int code;

    if (cache->handle.align) {
	ret = osi_mem_aligned_alloc_nosleep(cache->handle.len, cache->handle.align);
    } else {
	ret = osi_mem_alloc_nosleep(cache->handle.len);
    }
    if (osi_compiler_expect_true(ret != osi_NULL)) {
	if (cache->handle.ctor) {
	    code = (*cache->handle.ctor)(ret, cache->handle.rock, 0);
	    if (osi_compiler_expect_false(code != 0)) {
		goto error;
	    }
	}
#if defined(OSI_DEBUG_MEM_OBJECT_CACHE)
	osi_refcnt_inc(&cache->handle.cache_usage);
#endif
    }

 done:
    return ret;

 error:
    if (cache->handle.align) {
	osi_mem_aligned_free(ret, cache->handle.len);
    } else {
	osi_mem_free(ret, cache->handle.len);
    }
    ret = osi_NULL;
    goto done;
}

_OSI_MEM_OBJECT_CACHE_IMPL_DECL_FREE()
{
    if (cache->handle.dtor) {
	(*cache->handle.dtor)(buf, cache->handle.rock);
    }
    if (cache->handle.align) {
	osi_mem_aligned_free(buf, cache->handle.len);
    } else {
	osi_mem_free(buf, cache->handle.len);
    }
#if defined(OSI_DEBUG_MEM_OBJECT_CACHE)
    osi_refcnt_dec(&cache->handle.cache_usage);
#endif
}

#endif /* OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE */

#endif /* _OSI_LEGACY_MEM_OBJECT_CACHE_IMPL_H */
