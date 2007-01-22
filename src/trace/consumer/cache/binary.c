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
#include <osi/osi_rwlock.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <trace/common/options.h>
#include <trace/consumer/cache/binary.h>
#include <trace/consumer/cache/binary_impl.h>
#include <trace/consumer/cache/probe_info.h>
#include <trace/consumer/cache/ptr_vec.h>

/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * executable binary metadata cache
 */

/* master cache object */
struct osi_trace_consumer_bin_cache_directory osi_trace_consumer_bin_cache;

/* static prototypes */
osi_static int
osi_trace_consumer_bin_cache_ctor(void * buf, void * sdata, int flags);
osi_static void
osi_trace_consumer_bin_cache_dtor(void * buf, void * sdata);


/*
 * constructor for an osi_trace_consumer_bin_cache_t object
 *
 * [IN] buf    -- newly allocated bin cache object
 * [IN] sdata  -- opaque data pointer
 * [IN] flags  -- implementation-specific flags
 *
 * returns:
 *   0 always
 */
osi_static int
osi_trace_consumer_bin_cache_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_consumer_bin_cache_t * bin = buf;

    bin->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_BIN;
    osi_refcnt_init(&bin->refcnt, 0);
    osi_trace_consumer_cache_ptr_vec_Init(&bin->probe_vec);

    return 0;
}

/*
 * destructor for an osi_trace_consumer_bin_cache_t object
 *
 * [IN] buf    -- newly allocated bin cache object
 * [IN] sdata  -- opaque data pointer
 *
 */
osi_static void
osi_trace_consumer_bin_cache_dtor(void * buf, void * sdata)
{
    osi_trace_consumer_bin_cache_t * bin = buf;

    osi_trace_consumer_cache_ptr_vec_Destroy(&bin->probe_vec);
    osi_refcnt_destroy(&bin->refcnt);
    bin->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_INVALID;
}

/* 
 * allocate an osi_trace_consumer_bin_cache object
 *
 * [IN] info      -- new bin info structure
 * [OUT] bin_out  -- location in which to store newly allocated bin pointer
 *
 * preconditions:
 *   none
 *
 * postconditions:
 *   bin allocated with refcount of 1
 *   gen info field populated
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result 
osi_trace_consumer_bin_cache_alloc(osi_trace_generator_info_t * info,
				   osi_trace_consumer_bin_cache_t ** bin_out)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_bin_cache_t * bin;

    *bin_out = bin = (osi_trace_consumer_bin_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_bin_cache.cache);
    if (osi_compiler_expect_false(bin == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_refcnt_reset(&bin->refcnt, 1);
    bin->info = *info;

 error:
    return res;
}

/* 
 * free an osi_trace_consumer_bin_cache object
 *
 * [IN] buf  -- opaque pointer to bin cache object
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result 
__osi_trace_consumer_bin_cache_free(void * buf)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_bin_cache_t * bin = buf;

    /* free up pointer vec size */
    osi_trace_consumer_cache_ptr_vec_free(&bin->probe_vec);
    osi_mem_object_cache_free(osi_trace_consumer_bin_cache.cache, bin);

 error:
    return res;
}

/* 
 * register an osi_trace_consumer_bin_cache object
 *
 * [IN] bin  -- pointer to bin cache object
 *
 * postconditions:
 *   bin refcount incremented
 *   bin registered in osi_trace_consumer_bin_cache.bin_list
 *   bin registered in osi_trace_consumer_bin_cache.bin_hash
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_bin_cache_register(osi_trace_consumer_bin_cache_t * bin)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;

    res = osi_trace_consumer_bin_cache_get(bin);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    hash = ((osi_uint32) bin->info.programType) & 
	OSI_TRACE_CONSUMER_BIN_CACHE_HASH_MASK;

    osi_rwlock_WrLock(&osi_trace_consumer_bin_cache.lock);
    osi_list_Append(&osi_trace_consumer_bin_cache.bin_list,
		     bin, 
		     osi_trace_consumer_bin_cache_t, 
		     bin_list);
    osi_list_Append(&osi_trace_consumer_bin_cache.bin_hash[hash],
		     bin, 
		     osi_trace_consumer_bin_cache_t, 
		     bin_list);
    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

 error:
    return res;
}

/* 
 * unregister an osi_trace_consumer_bin_cache object
 *
 * [IN] bin  -- pointer to bin cache object
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_bin_cache_unregister(osi_trace_consumer_bin_cache_t * bin)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;

    osi_rwlock_WrLock(&osi_trace_consumer_bin_cache.lock);

    /* handle races */
    if (!osi_list_IsOnList(bin,
			   osi_trace_consumer_bin_cache_t,
			   bin_list)) {
	res = OSI_FAIL;
	goto error;
    }

    osi_list_Remove(bin, osi_trace_consumer_bin_cache_t, bin_list);

    hash = ((osi_uint32) bin->info.programType) & 
	OSI_TRACE_CONSUMER_BIN_CACHE_HASH_MASK;
    osi_list_Remove(bin, osi_trace_consumer_bin_cache_t, bin_hash);

    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

    res = osi_trace_consumer_bin_cache_put(bin);

 done:
    return res;

 error:
    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);
    goto done;
}

/* 
 * register a probe into a bin cache object
 *
 * [IN] bin       -- pointer to bin cache object
 * [IN] probe_id  -- probe id
 * [IN] probe     -- pointer to probe info cache object
 *
 * postconditions:
 *   probe refcount incremented
 *   probe registered in bin probe vec
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_bin_cache_register_probe(osi_trace_consumer_bin_cache_t * bin,
					    osi_trace_probe_id_t probe_id,
					    osi_trace_consumer_probe_info_cache_t * probe)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;
    osi_trace_consumer_probe_info_cache_t * old_probe = osi_NULL;

    res = osi_trace_consumer_probe_info_cache_get(probe);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_rwlock_WrLock(&osi_trace_consumer_bin_cache.lock);

    res = osi_trace_consumer_cache_ptr_vec_extend(&bin->probe_vec,
						  probe_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error_sync;
    }

    old_probe = bin->probe_vec.vec[probe_id].probe_info;
    bin->probe_vec.vec[probe_id].probe_info = probe;

 error_sync:
    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

    if (old_probe != osi_NULL) {
	osi_trace_consumer_probe_info_cache_put(old_probe);
    }

 error:
    return res;
}

/* 
 * unregister a probe from a bin
 *
 * [IN] bin       -- pointer to bin cache object
 * [IN] probe_id  -- probe id
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_bin_cache_unregister_probe(osi_trace_consumer_bin_cache_t * bin,
					      osi_trace_probe_id_t probe_id)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;
    osi_trace_consumer_probe_info_cache_t * probe = osi_NULL;

    osi_rwlock_WrLock(&osi_trace_consumer_bin_cache.lock);

    if (bin->probe_vec.vec_len <= probe_id) {
	res = OSI_FAIL;
	goto error_sync;
    }

    probe = bin->probe_vec.vec[probe_id].probe_info;
    bin->probe_vec.vec[probe_id].opaque = osi_NULL;

 error_sync:
    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

    if (probe != osi_NULL) {
	osi_trace_consumer_probe_info_cache_put(probe);
    } else {
	res = OSI_FAIL;
    }

 done:
    return res;
}

/*
 * get a ref on a bin cache object
 * 
 * [IN] bin  -- pointer to bin cache object
 *
 * preconditions:
 *   calling thread must already hold a ref on bin
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_bin_cache_get(osi_trace_consumer_bin_cache_t * bin)
{
    osi_result res = OSI_OK;

    osi_refcnt_inc(&bin->refcnt);

    return res;
}

/*
 * put back a ref on a bin cache object
 * 
 * [IN] gen  -- pointer to bin cache object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_bin_cache_put(osi_trace_consumer_bin_cache_t * bin)
{
    osi_result res = OSI_OK;

    (void)osi_refcnt_dec_action(&bin->refcnt,
				0,
				&__osi_trace_consumer_bin_cache_free,
				bin,
				&res);

    return res;
}

/* 
 * get a ref to an osi_trace_consumer_bin_cache structure
 *
 * [IN] info      -- info structure query
 * [OUT] bin_out  -- location in which to store bin structure pointer
 *
 * postconditions:
 *   reference count incremented on bin
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on lookup failure
 */
osi_result 
osi_trace_consumer_bin_cache_lookup(osi_trace_generator_info_t * info,
				    osi_trace_consumer_bin_cache_t ** bin_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_consumer_bin_cache_t * bin;
    osi_uint32 hash;

    hash = ((osi_uint32) info->programType) & 
	OSI_TRACE_CONSUMER_BIN_CACHE_HASH_MASK;

    osi_rwlock_RdLock(&osi_trace_consumer_bin_cache.lock);

    for (osi_list_Scan_Immutable(&osi_trace_consumer_bin_cache.bin_hash[hash],
				 bin,
				 osi_trace_consumer_bin_cache_t, 
				 bin_hash)) {
	if ((bin->info.programType == info->programType) &&
	    (bin->info.module_count == info->module_count) &&
	    (bin->info.module_version_cksum_type != OSI_TRACE_MODULE_CKSUM_TYPE_NONE) &&
	    (bin->info.module_version_cksum_type == info->module_version_cksum_type) &&
	    (bin->info.module_version_cksum == info->module_version_cksum)) {
	    *bin_out = bin;
	    res = osi_trace_consumer_bin_cache_get(bin);
	    break;
	}
    }

    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

    return res;
}

/* 
 * lookup an osi_trace_probe_info_cache structure
 *
 * [IN] bin         -- pointer to bin cache object
 * [IN] probe_id    -- probe id
 * [OUT] probe_out  -- location in which to store probe metadata cache pointer
 *
 * postconditions:
 *   ref acquired on probe
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_result 
osi_trace_consumer_bin_cache_lookup_probe(osi_trace_consumer_bin_cache_t * bin,
					  osi_trace_probe_id_t probe_id,
					  osi_trace_consumer_probe_info_cache_t ** probe_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_consumer_probe_info_cache_t * probe;

    osi_rwlock_RdLock(&osi_trace_consumer_bin_cache.lock);

    if (osi_compiler_expect_true(probe_id < bin->probe_vec.vec_len)) {
	*probe_out = probe = bin->probe_vec.vec[probe_id].probe_info;
	res = osi_trace_consumer_probe_info_cache_get(probe);
    }

    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

    return res;
}

osi_result
osi_trace_consumer_bin_cache_PkgInit(void)
{
    int i;
    osi_result res = OSI_OK;
    osi_uint32 max_id;
    size_t align;

    osi_rwlock_Init(&osi_trace_consumer_bin_cache.lock,
		    &osi_trace_common_options.rwlock_opts);

    osi_list_Init(&osi_trace_consumer_bin_cache.bin_list);
    for (i = 0; i < OSI_TRACE_CONSUMER_BIN_CACHE_HASH_BUCKETS; i++) {
	osi_list_Init(&osi_trace_consumer_bin_cache.bin_hash[i]);
    }

    osi_trace_consumer_bin_cache.cache =
	osi_mem_object_cache_create("osi_trace_consumer_bin_cache",
				    sizeof(osi_trace_consumer_bin_cache_t),
				    align,
				    osi_NULL,
				    &osi_trace_consumer_bin_cache_ctor,
				    &osi_trace_consumer_bin_cache_dtor,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_compiler_expect_false(osi_trace_consumer_bin_cache.cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
    }

 error:
    return res;
}

osi_result
osi_trace_consumer_bin_cache_PkgShutdown(void)
{
    int i;
    osi_result res, code = OSI_OK;
    osi_trace_consumer_bin_cache_t * bin;
    osi_list_head clear_queue;

    osi_list_Init(&clear_queue);

    osi_rwlock_WrLock(&osi_trace_consumer_bin_cache.lock);
    osi_list_SpliceAppend(&clear_queue,
			  &osi_trace_consumer_bin_cache.bin_list);
    for (i = 0; i < OSI_TRACE_CONSUMER_BIN_CACHE_HASH_BUCKETS; i++) {
	osi_list_Init(&osi_trace_consumer_bin_cache.bin_hash[i]);
    }
    osi_rwlock_Unlock(&osi_trace_consumer_bin_cache.lock);

    for (osi_list_Scan_Immutable(&clear_queue,
				 bin,
				 osi_trace_consumer_bin_cache_t,
				 bin_list)) {
	res = osi_trace_consumer_bin_cache_put(bin);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    osi_mem_object_cache_destroy(osi_trace_consumer_bin_cache.cache);

 error_sync:
    osi_rwlock_Destroy(&osi_trace_consumer_bin_cache.lock);

 error:
    return code;
}
