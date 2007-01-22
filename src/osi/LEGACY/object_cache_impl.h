/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_LEGACY_MEM_OBJECT_CACHE_IMPL_H
#define	_OSI_LEGACY_MEM_OBJECT_CACHE_IMPL_H

/*
 * osi mem object cache interface
 * legacy backend
 * implementation details
 */

#if defined(OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE)

#include <osi/osi_mem.h>

osi_extern osi_result _osi_mem_object_cache_create(osi_mem_object_cache_t * cache,
						   char * name, 
						   size_t size, size_t align,
						   void * spec_data,
						   osi_mem_object_cache_constructor_t * ctor,
						   osi_mem_object_cache_destructor_t * dtor,
						   osi_mem_object_cache_reclaim_t * reclaim);

osi_extern osi_result _osi_mem_object_cache_destroy(osi_mem_object_cache_t *);

/* 
 * it's ok to inline these because they are only called from the 
 * wrapper functions in src/osi/COMMON/object_cache.c
 */

osi_static osi_inline void *
_osi_mem_object_cache_alloc(osi_mem_object_cache_t * cache)
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
	    code = (*cache->handle.ctor)(ret, cache->handle.sdata, 0);
	    if (osi_compiler_expect_false(code != 0)) {
		goto error;
	    }
	}
	osi_refcnt_inc(&cache->handle.cache_usage);
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

osi_static osi_inline void *
_osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t * cache)
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
	    code = (*cache->handle.ctor)(ret, cache->handle.sdata, 0);
	    if (osi_compiler_expect_false(code != 0)) {
		goto error;
	    }
	}
	osi_refcnt_inc(&cache->handle.cache_usage);
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

osi_static osi_inline void
_osi_mem_object_cache_free(osi_mem_object_cache_t * cache, void * buf)
{
    if (cache->handle.dtor) {
	(*cache->handle.dtor)(buf, cache->handle.sdata);
    }
    if (cache->handle.align) {
	osi_mem_aligned_free(buf, cache->handle.len);
    } else {
	osi_mem_free(buf, cache->handle.len);
    }
    osi_refcnt_dec(&cache->handle.cache_usage);
}

#endif /* OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE */

#endif /* _OSI_LEGACY_MEM_OBJECT_CACHE_IMPL_H */
