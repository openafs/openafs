/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_trace.h>
#include <osi/osi_mem.h>
#include <osi/osi_cache.h>
#include <trace/consumer/cache/ptr_vec.h>
#include <trace/consumer/cache/ptr_vec_impl.h>


struct osi_trace_consumer_cache_ptr_vec_config {
    size_t chunk_size;
};

osi_static struct osi_trace_consumer_cache_ptr_vec_config 
osi_trace_consumer_cache_ptr_vec_config;


/*
 * initialize an osi_trace_consumer_cache_ptr_vec_t
 *
 * [IN] vec  -- pointer to vector object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_cache_ptr_vec_Init(osi_trace_consumer_cache_ptr_vec_t * vec)
{
    osi_result res = OSI_OK;

    vec->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_POINTER_VEC;
    vec->vec_len = 0;
    vec->vec = osi_NULL;

    return res;
}

/*
 * destroy an osi_trace_consumer_cache_ptr_vec_t
 *
 * [IN] vec  -- pointer to vector object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_cache_ptr_vec_Destroy(osi_trace_consumer_cache_ptr_vec_t * vec)
{
    osi_result res;

    res = osi_trace_consumer_cache_ptr_vec_free(vec);
    vec->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_INVALID;

    return res;
}

/* 
 * extend an osi_trace_consumer_cache_ptr_vec_t to at least the desired length
 *
 * [IN] vec         -- pointer to osi_trace_consumer_cache_ptr_vec_t structure
 * [IN] needed_val  -- new maximum vector index value needed
 *
 * preconditions:
 *   lock which controls access to this vector must be held exclusively
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_cache_ptr_vec_extend(osi_trace_consumer_cache_ptr_vec_t * vec,
					size_t needed_val)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_cache_object_ptr_t * new_vec;
    size_t align, delta, old_buf_sz, delta_buf_sz, chunk_sz;

    if (needed_val < vec->vec_len) {
	return OSI_OK;
    }

    chunk_sz = osi_trace_consumer_cache_ptr_vec_config.chunk_size;

    res = osi_cache_max_alignment(&align);
    if (OSI_RESULT_FAIL(res)) {
	align = 32;
    }

    old_buf_sz = vec->vec_len * 
	sizeof(osi_trace_consumer_cache_object_ptr_t);

    delta = (needed_val + 1) - vec->vec_len;
    if (delta & (chunk_sz - 1)) {
	delta &= ~(chunk_sz - 1);
	delta += chunk_sz;
    }

    delta_buf_sz = delta * 
	sizeof(osi_trace_consumer_cache_object_ptr_t);

    new_vec = osi_mem_aligned_alloc((old_buf_sz + delta_buf_sz), align);
    if (osi_compiler_expect_false(new_vec == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    if (vec->vec_len) {
	osi_mem_copy(new_vec, vec->vec, old_buf_sz);
	osi_mem_aligned_free(vec->vec, old_buf_sz);
    }
    osi_mem_zero(&new_vec[vec->vec_len], delta_buf_sz);
    vec->vec = new_vec;
    vec->vec_len += delta;

 error:
    return res;
}

/* 
 * free the vector associated with an osi_trace_consumer_cache_ptr_vec_t structure
 *
 * [IN] vec  -- pointer to osi_trace_consumer_cache_ptr_vec_t structure
 *
 * preconditions:
 *   lock which controls access to this vector is held exclusively
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_cache_ptr_vec_free(osi_trace_consumer_cache_ptr_vec_t * vec)
{
    osi_result res = OSI_OK;

    if (vec->vec_len) {
	osi_mem_aligned_free(vec->vec, 
			     vec->vec_len * sizeof(osi_trace_consumer_cache_object_ptr_t));
    }
    vec->vec = osi_NULL;
    vec->vec_len = 0;

    return res;
}

osi_result
osi_trace_consumer_cache_ptr_vec_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_trace_consumer_cache_ptr_vec_config.chunk_size = 
	OSI_TRACE_CONSUMER_CACHE_PTR_VEC_CHUNK_SIZE_DEFAULT;

 error:
    return res;
}

osi_result
osi_trace_consumer_cache_ptr_vec_PkgShutdown(void)
{
    osi_result res = OSI_OK;

 error:
    return res;
}
