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
#include <osi/osi_cache.h>
#include <osi/osi_trace.h>
#include <osi/osi_tracepoint_table.h>
#include <osi/COMMON/object_cache_impl.h>

osi_mem_object_cache_t __osi_mem_object_cache_base_cache;

osi_mem_object_cache_t * 
osi_mem_object_cache_create(char * name, 
			    osi_size_t size, 
			    osi_size_t align,
			    void * spec_data,
			    osi_mem_object_cache_constructor_t * ctor,
			    osi_mem_object_cache_destructor_t * dtor,
			    osi_mem_object_cache_reclaim_t * reclaim,
			    osi_mem_object_cache_options_t * options)
{
    osi_mem_object_cache_t * cache;

    if (!options || options->trace_allowed) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_create_start),
			    osi_Trace_Args8(name, size, align, spec_data,
					    ctor, dtor, reclaim, options));
    }

    cache = (osi_mem_object_cache_t *)
	_osi_mem_object_cache_alloc(&__osi_mem_object_cache_base_cache);
    if (osi_compiler_expect_false(cache == osi_NULL)) {
	goto error;
    }

    if (options) {
	osi_mem_copy(&cache->options, options, sizeof(osi_mem_object_cache_options_t));
    } else {
	cache->options.trace_enabled = 0;
	cache->options.trace_allowed = 1;
    }

    if (OSI_RESULT_FAIL_UNLIKELY(_osi_mem_object_cache_create(cache, name,
							      size, align, spec_data, 
							      ctor, dtor, reclaim))) {
	goto error;
    }

    if (!options || options->trace_allowed) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_create_finish),
			    osi_Trace_Args1(cache));
    }

 done:
    return cache;

 error:
    if (cache) {
	_osi_mem_object_cache_free(&__osi_mem_object_cache_base_cache, cache);
	cache = osi_NULL;
    }
    if (!options || options->trace_allowed) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_create_abort),
			    osi_Trace_Args0());
    }
    goto done;
}

osi_result
osi_mem_object_cache_destroy(osi_mem_object_cache_t * cache)
{
    osi_result res;
    int trace = cache->options.trace_allowed;

    if (trace) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_destroy_start),
			    osi_Trace_Args1(cache));
    }

    res = _osi_mem_object_cache_destroy(cache);
    _osi_mem_object_cache_free(&__osi_mem_object_cache_base_cache, cache);

    if (trace) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_destroy_finish),
			    osi_Trace_Args0());
    }
    return res;
}

void *
osi_mem_object_cache_alloc(osi_mem_object_cache_t * cache)
{
    void * ret;
    if (osi_compiler_expect_false(cache->options.trace_allowed && cache->options.trace_enabled)) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_alloc_start),
			    osi_Trace_Args1(cache));
    }
    ret = _osi_mem_object_cache_alloc(cache);
    if (osi_compiler_expect_false(cache->options.trace_allowed && cache->options.trace_enabled)) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_alloc_finish),
			    osi_Trace_Args2(cache, ret));
    }
    return ret;
}

void *
osi_mem_object_cache_alloc_nosleep(osi_mem_object_cache_t * cache)
{
    void * ret;
    if (osi_compiler_expect_false(cache->options.trace_allowed && cache->options.trace_enabled)) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_alloc_nosleep_start),
			    osi_Trace_Args1(cache));
    }
    ret = _osi_mem_object_cache_alloc_nosleep(cache);
    if (osi_compiler_expect_false(cache->options.trace_allowed && cache->options.trace_enabled)) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_alloc_nosleep_finish),
			    osi_Trace_Args2(cache, ret));
    }
    return ret;
}

void
osi_mem_object_cache_free(osi_mem_object_cache_t * cache, void * buf)
{
    if (osi_compiler_expect_false(cache->options.trace_allowed && cache->options.trace_enabled)) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_free_start),
			    osi_Trace_Args2(cache, buf));
    }
    _osi_mem_object_cache_free(cache, buf);
    if (osi_compiler_expect_false(cache->options.trace_allowed && cache->options.trace_enabled)) {
	osi_Trace_OSI_Event(osi_Trace_OSI_ProbeId(mem_cache_free_finish),
			    osi_Trace_Args1(cache));
    }
}

int
osi_mem_object_cache_dummy_ctor(void * buf, void * sdata, int flags)
{
    return 0;
}
void
osi_mem_object_cache_dummy_dtor(void * buf, void * sdata)
{
    return;
}
void
osi_mem_object_cache_dummy_reclaim(void * sdata)
{
    return;
}

osi_result
osi_mem_object_cache_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_size_t align;

    if (OSI_RESULT_FAIL_UNLIKELY(osi_cache_max_alignment(&align))) {
	/* default to something sane */
	align = 32;
    }

    res = _osi_mem_object_cache_create(&__osi_mem_object_cache_base_cache,
				       "osi_mem_object_cache",
				       sizeof(osi_mem_object_cache_t),
				       align,
				       osi_NULL,
				       osi_NULL,
				       osi_NULL,
				       osi_NULL);

 error:
    return res;

}

osi_result
osi_mem_object_cache_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    res = _osi_mem_object_cache_destroy(&__osi_mem_object_cache_base_cache);

    return res;
}
