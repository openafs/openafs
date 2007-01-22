/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_object_cache.h>

#if defined(OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE)

#include <osi/COMMON/object_cache_impl.h>

/*
 * XXX cache usage refcounting should eventually be forced to
 * conditional compilation to improve performance
 * (probably keyed off debug build autoconf flags)
 */


osi_result
_osi_mem_object_cache_create(osi_mem_object_cache_t * cache,
			     char * name, 
			     size_t size, size_t align,
			     void * spec_data,
			     osi_mem_object_cache_constructor_t * ctor,
			     osi_mem_object_cache_destructor_t * dtor,
			     osi_mem_object_cache_reclaim_t * reclaim)
{
    cache->handle.name = name;
    cache->handle.len = size;
    cache->handle.align = align;
    cache->handle.ctor = ctor;
    cache->handle.dtor = dtor;
    cache->handle.reclaim = reclaim;
    cache->handle.sdata = spec_data;

    osi_refcnt_init(&cache->handle.cache_usage, 1);

    return OSI_OK;
}

/* worker function to call when refcount reaches zero */
osi_static osi_result
osi_mem_legacy_object_cache_destroy(void * sdata)
{
    osi_mem_object_cache_t * cache =
	(osi_mem_object_cache_t *) sdata;

    osi_refcnt_destroy(&cache->handle.cache_usage);

    return OSI_OK;
}

osi_result
_osi_mem_object_cache_destroy(osi_mem_object_cache_t * cache)
{
    osi_result res = OSI_FAIL;
    int code;

    code = osi_refcnt_dec_action(&cache->handle.cache_usage, 
				 0,
				 &osi_mem_legacy_object_cache_destroy, 
				 (void *)cache,
				 &res);

    if (!code || OSI_RESULT_FAIL(res)) {
	osi_Panic("osi_mem_object_cache '%s' was destroyed with actively allocated objects",
		  cache->handle.name);
    }
    return res;
}

#endif /* OSI_IMPLEMENTS_LEGACY_MEM_OBJECT_CACHE */
