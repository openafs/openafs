/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_event.h>
#include <trace/consumer/cache/generator.h>
#include <trace/consumer/cache/generator_impl.h>
#include <trace/consumer/cache/binary.h>
#include <trace/consumer/cache/ptr_vec.h>


/*
 * osi tracing framework
 * trace consumer cache
 * trace generator metadata cache
 */

struct osi_trace_consumer_gen_cache_directory osi_trace_consumer_gen_cache;

/* static prototypes */
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_PROTOTYPE(osi_trace_consumer_gen_cache_ctor);
OSI_MEM_OBJECT_CACHE_DTOR_STATIC_PROTOTYPE(osi_trace_consumer_gen_cache_dtor);


/*
 * constructor for an osi_trace_consumer_gen_cache_t object
 *
 * [IN] buf    -- newly allocated gen cache object
 * [IN] sdata  -- opaque data pointer
 * [IN] flags  -- implementation-specific flags
 *
 * returns:
 *   0 always
 */
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_DECL(osi_trace_consumer_gen_cache_ctor)
{
    osi_trace_consumer_gen_cache_t * gen =
	OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;

    gen->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_GEN;
    osi_refcnt_init(&gen->refcnt, 0);
    osi_trace_consumer_cache_ptr_vec_Init(&gen->probe_vec);
    osi_event_hook_Init(&gen->hook);
    osi_event_hook_set_rock(&gen->hook, 
			    gen);
    return 0;
}

/*
 * destructor for an osi_trace_consumer_gen_cache_t object
 *
 * [IN] buf    -- newly allocated gen cache object
 * [IN] sdata  -- opaque data pointer
 *
 */
OSI_MEM_OBJECT_CACHE_DTOR_STATIC_DECL(osi_trace_consumer_gen_cache_dtor)
{
    osi_trace_consumer_gen_cache_t * gen =
	OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;

    osi_event_hook_Destroy(&gen->hook);
    osi_trace_consumer_cache_ptr_vec_Destroy(&gen->probe_vec);
    osi_refcnt_destroy(&gen->refcnt);
    gen->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_INVALID;
}

/* 
 * allocate an osi_trace_consumer_gen_cache structure
 *
 * [IN] info      -- new generator info structure
 * [OUT] gen_out  -- location in which to store newly allocated gen pointer
 *
 * preconditions:
 *   none
 *
 * postconditions:
 *   generator allocated
 *   gen info field populated
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_gen_cache_alloc(osi_trace_generator_info_t * info,
				   osi_trace_consumer_gen_cache_t ** gen_out)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_gen_cache_t * gen;

    *gen_out = gen = (osi_trace_consumer_gen_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_gen_cache.cache);
    if (osi_compiler_expect_false(gen == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    gen->info = *info;
    gen->bin = osi_NULL;
    osi_refcnt_reset(&gen->refcnt, 1);

 error:
    return res;
}

/* 
 * free an osi_trace_consumer_gen_cache_t structure
 *
 * [IN] gen  -- pointer to cache gen structure
 *
 * preconditions:
 *   none
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result 
__osi_trace_consumer_gen_cache_free(void * buf)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_gen_cache_t * gen = buf;

    /* reclaim pointer vec space */
    osi_trace_consumer_cache_ptr_vec_free(&gen->probe_vec);
    osi_mem_object_cache_free(osi_trace_consumer_gen_cache.cache, gen);

    return res;
}

/* 
 * invalidate all the children of a gen cache object
 *
 * [IN] gen       -- pointer to gen cache object
 *
 * postconditions:
 *   all probe val cache objects have been detached
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_gen_cache_invalidate(osi_trace_consumer_gen_cache_t * gen)
{
    osi_result res = OSI_OK;
    osi_uint32 i;
    osi_trace_consumer_probe_val_cache_t * probe;
    osi_trace_consumer_cache_ptr_vec_t vec;

    res = osi_trace_consumer_cache_ptr_vec_Init(&vec);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    /* 
     * move out the pointer vec under the lock 
     */
    osi_rwlock_WrLock(&osi_trace_consumer_gen_cache.lock);

    res = osi_trace_consumer_cache_ptr_vec_extend(&vec,
						  gen->probe_vec.vec_len);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error_sync;
    }

    for (i = 0; i < gen->probe_vec.vec_len; i++) {
	vec.vec[i].probe_val = gen->probe_vec.vec[i].probe_val;
	gen->probe_vec.vec[i].opaque = osi_NULL;
    }

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);


    /* 
     * now free all the probe val cache objects outside the lock 
     */
    for (i = 0; i < vec.vec_len; i++) {
	probe = vec.vec[i].probe_val;
	if (probe != osi_NULL) {
	    osi_trace_consumer_probe_val_cache_put(probe);
	}
    }

    (void)osi_trace_consumer_cache_ptr_vec_Destroy(&vec);

 error:
    return res;

 error_sync:
    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);
    goto error;
}

/* 
 * register an osi_trace_consumer_gen_cache structure
 *
 * [IN] gen  -- pointer to cache gen structure
 *
 * postconditions:
 *   gen refcnt incremented
 *   generated registered with osi_trace_consumer_gen_cache.gen_vec
 *   bin allocated (if necessary) and referenced
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on invalid generator id
 */
osi_result 
osi_trace_consumer_gen_cache_register(osi_trace_consumer_gen_cache_t * gen)
{
    osi_result res, code = OSI_OK;
    void * slot;

    osi_rwlock_WrLock(&osi_trace_consumer_gen_cache.lock);

    code = osi_trace_consumer_cache_ptr_vec_extend(&osi_trace_consumer_gen_cache.gen_vec,
						  gen->info.gen_id);
    if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	goto error;
    }

    /* handle races */
    slot = osi_trace_consumer_gen_cache.gen_vec.vec[gen->info.gen_id].opaque;
    if (osi_compiler_expect_false(slot != osi_NULL)) {
	code = OSI_FAIL;
	goto error;
    }

    /* make sure associated bin structure exists, and get a ref on it */
    res = osi_trace_consumer_bin_cache_lookup(&gen->info,
					      &gen->bin);
    if (OSI_RESULT_FAIL(res)) {
	code = osi_trace_consumer_bin_cache_alloc(&gen->info, &gen->bin);
	if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	    goto error;
	}
	code = osi_trace_consumer_bin_cache_register(gen->bin);
	if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	    osi_trace_consumer_bin_cache_put(gen->bin);
	    gen->bin = osi_NULL;
	    goto error;
	}
    }

    /* register this gen */
    osi_trace_consumer_gen_cache.gen_vec.vec[gen->info.gen_id].gen = gen;
    osi_list_Prepend(&osi_trace_consumer_gen_cache.gen_list,
		     gen, 
		     osi_trace_consumer_gen_cache_t, 
		     gen_list);

    /* add one ref to account for being on the gen_vec and gen_list */
    osi_refcnt_inc(&gen->refcnt);

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    (void)osi_event_fire(&gen->hook,
			 OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_GEN_REGISTER,
			 osi_NULL);

 error:
    return code;
}

/* 
 * unregister an osi_trace_consumer_gen_cache structure
 *
 * [IN] gen  -- pointer to cache gen structure
 *
 * postconditions:
 *   bin ref dropped
 *   generator no longer referenced in osi_trace_consumer_gen_cache.gen_vec
 *   generator no longer on osi_trace_consumer_gen_cache.gen_list
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_result 
osi_trace_consumer_gen_cache_unregister(osi_trace_consumer_gen_cache_t * gen)
{
    osi_result res = OSI_OK;
    void * slot;

    osi_rwlock_WrLock(&osi_trace_consumer_gen_cache.lock);

    /* handle races */
    slot = osi_trace_consumer_gen_cache.gen_vec.vec[gen->info.gen_id].opaque;
    if (osi_compiler_expect_false(slot == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    /* drop bin ref */
    if (osi_compiler_expect_true(gen->bin != osi_NULL)) {
	res = osi_trace_consumer_bin_cache_put(gen->bin);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
	gen->bin = osi_NULL;
    }

    /* unregister this gen */
    osi_trace_consumer_gen_cache.gen_vec.vec[gen->info.gen_id].opaque = osi_NULL;
    osi_list_Remove(gen, 
		    osi_trace_consumer_gen_cache_t, 
		    gen_list);

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    /* drop the ref from the gen_vec and gen_list */
    osi_trace_consumer_gen_cache_put(gen);

 done:
    return res;

 error:
    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);
    goto done;
}

/* 
 * register a probe into a gen cache object
 *
 * [IN] gen       -- pointer to gen cache object
 * [IN] probe_id  -- probe id
 * [IN] probe     -- pointer to probe val cache object
 *
 * postconditions:
 *   probe refcount incremented
 *   probe registered in gen probe vec
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_gen_cache_register_probe(osi_trace_consumer_gen_cache_t * gen,
					    osi_trace_probe_id_t probe_id,
					    osi_trace_consumer_probe_val_cache_t * probe)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;
    osi_trace_consumer_probe_val_cache_t * old_probe = osi_NULL;

    res = osi_trace_consumer_probe_val_cache_get(probe);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_rwlock_WrLock(&osi_trace_consumer_gen_cache.lock);

    res = osi_trace_consumer_cache_ptr_vec_extend(&gen->probe_vec,
						  probe_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);
	goto error;
    }

    old_probe = gen->probe_vec.vec[probe_id].probe_val;
    gen->probe_vec.vec[probe_id].probe_val = probe;

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    osi_event_fire(&gen->hook,
		   OSI_TRACE_CONSUMER_GEN_CACHE_EVENT_PROBE_REGISTER,
		   probe);

    if (old_probe != osi_NULL) {
	osi_trace_consumer_probe_val_cache_put(old_probe);
    }

 error:
    return res;
}

/* 
 * unregister a probe from a gen
 *
 * [IN] gen       -- pointer to gen cache object
 * [IN] probe_id  -- probe id
 *
 * returns:
 *   OSI_OK always
 */
osi_result 
osi_trace_consumer_gen_cache_unregister_probe(osi_trace_consumer_gen_cache_t * gen,
					      osi_trace_probe_id_t probe_id)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;
    osi_trace_consumer_probe_val_cache_t * probe = osi_NULL;

    osi_rwlock_WrLock(&osi_trace_consumer_gen_cache.lock);

    if (gen->probe_vec.vec_len <= probe_id) {
	res = OSI_FAIL;
	goto error_sync;
    }

    probe = gen->probe_vec.vec[probe_id].probe_val;
    gen->probe_vec.vec[probe_id].opaque = osi_NULL;

 error_sync:
    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    if (probe != osi_NULL) {
	osi_trace_consumer_probe_val_cache_put(probe);
    } else {
	res = OSI_FAIL;
    }

 done:
    return res;
}

/*
 * subscribe to this gen's event feed
 *
 * [IN] gen    -- pointer to gen cache object
 * [IN] sub    -- pointer to subscription object 
 *
 * returns:
 *   see osi_event_subscribe()
 */
osi_result
osi_trace_consumer_gen_cache_event_subscribe(osi_trace_consumer_gen_cache_t * gen,
					     osi_event_subscription_t * sub)
{
    osi_result res;

    res = osi_event_subscribe(&gen->hook,
			      sub);

    return res;
}

/*
 * get a ref on a gen cache object
 * 
 * [IN] gen  -- pointer to gen object
 *
 * preconditions:
 *   calling thread must already hold a ref on gen
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_gen_cache_get(osi_trace_consumer_gen_cache_t * gen)
{
    osi_result res = OSI_OK;

    osi_refcnt_inc(&gen->refcnt);

    return res;
}

/*
 * put back a ref on a gen cache object
 * 
 * [IN] gen  -- pointer to gen object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_gen_cache_put(osi_trace_consumer_gen_cache_t * gen)
{
    osi_result res = OSI_OK;

    (void)osi_refcnt_dec_action(&gen->refcnt,
				0,
				&__osi_trace_consumer_gen_cache_free,
				gen,
				&res);

    return res;
}

/* 
 * lookup an osi_trace_consumer_gen_cache structure
 *
 * [IN] gen_id    -- generator id
 * [OUT] gen_out  -- location in which to store gen pointer
 *
 * postconditions:
 *   ref acquired on gen
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_result 
osi_trace_consumer_gen_cache_lookup(osi_trace_gen_id_t gen_id,
				    osi_trace_consumer_gen_cache_t ** gen_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_consumer_gen_cache_t * gen;

    osi_rwlock_RdLock(&osi_trace_consumer_gen_cache.lock);

    if (osi_compiler_expect_true(gen_id < osi_trace_consumer_gen_cache.gen_vec.vec_len)) {
	*gen_out = gen = osi_trace_consumer_gen_cache.gen_vec.vec[gen_id].gen;
	if (gen != osi_NULL) {
	    res = osi_trace_consumer_gen_cache_get(gen);
	}
    }

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    return res;
}

/* 
 * lookup an osi_trace_consumer_bin_cache structure
 * based upon a gen_id
 *
 * [IN] gen_id    -- generator id
 * [OUT] bin_out  -- location in which to store bin pointer
 *
 * postconditions:
 *   ref acquired on bin
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_result 
osi_trace_consumer_gen_cache_lookup_bin(osi_trace_gen_id_t gen_id,
					osi_trace_consumer_bin_cache_t ** bin_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_consumer_gen_cache_t * gen;
    osi_trace_consumer_bin_cache_t * bin;

    osi_rwlock_RdLock(&osi_trace_consumer_gen_cache.lock);

    if (osi_compiler_expect_true(gen_id < osi_trace_consumer_gen_cache.gen_vec.vec_len)) {
	gen = osi_trace_consumer_gen_cache.gen_vec.vec[gen_id].gen;
	if (gen != osi_NULL) {
	    *bin_out = bin = gen->bin;
	    res = osi_trace_consumer_bin_cache_get(bin);
	}
    }

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    return res;
}

/* 
 * lookup an osi_trace_probe_val_cache structure
 * based upon a gen_id
 *
 * [IN] gen_id      -- generator id
 * [OUT] probe_out  -- location in which to store probe pointer
 *
 * postconditions:
 *   ref acquired on probe
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_result 
osi_trace_consumer_gen_cache_lookup_probe(osi_trace_gen_id_t gen_id,
					  osi_trace_probe_id_t probe_id,
					  osi_trace_consumer_probe_val_cache_t ** probe_out)
{
    osi_result res = OSI_FAIL;
    osi_trace_consumer_gen_cache_t * gen;
    osi_trace_consumer_probe_val_cache_t * probe;

    osi_rwlock_RdLock(&osi_trace_consumer_gen_cache.lock);

    if (osi_compiler_expect_true(gen_id < osi_trace_consumer_gen_cache.gen_vec.vec_len)) {
	gen = osi_trace_consumer_gen_cache.gen_vec.vec[gen_id].gen;
	if (gen != osi_NULL) {
	    if (osi_compiler_expect_true(probe_id < gen->probe_vec.vec_len)) {
		*probe_out = probe = gen->probe_vec.vec[probe_id].probe_val;
		res = osi_trace_consumer_probe_val_cache_get(probe);
	    }
	}
    }

    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    return res;
}


OSI_INIT_FUNC_DECL(osi_trace_consumer_gen_cache_PkgInit)
{
    osi_result res;
    size_t align;

    res = osi_cache_max_alignment(&align);
    if (OSI_RESULT_FAIL(res)) {
	align = 32;
    }

    osi_rwlock_Init(&osi_trace_consumer_gen_cache.lock,
		    osi_trace_impl_rwlock_opts());

    osi_list_Init(&osi_trace_consumer_gen_cache.gen_list);

    res = osi_trace_consumer_cache_ptr_vec_Init(&osi_trace_consumer_gen_cache.gen_vec);

    osi_trace_consumer_gen_cache.cache =
	osi_mem_object_cache_create("osi_trace_consumer_gen_cache",
				    sizeof(osi_trace_consumer_gen_cache_t),
				    align,
				    osi_NULL,
				    &osi_trace_consumer_gen_cache_ctor,
				    &osi_trace_consumer_gen_cache_dtor,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_consumer_gen_cache.cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
    }

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_consumer_gen_cache_PkgShutdown)
{
    osi_result res, code = OSI_OK;
    osi_trace_consumer_gen_cache_t * gen;
    osi_list_head clear_queue;

    osi_list_Init(&clear_queue);

    /* first, atomically destroy the gen_list and gen_vec */
    osi_rwlock_WrLock(&osi_trace_consumer_gen_cache.lock);
    osi_list_SpliceAppend(&clear_queue,
			  &osi_trace_consumer_gen_cache.gen_list);
    osi_trace_consumer_cache_ptr_vec_Destroy(&osi_trace_consumer_gen_cache.gen_vec);
    osi_rwlock_Unlock(&osi_trace_consumer_gen_cache.lock);

    /* now go back and drop refs on everything */
    for (osi_list_Scan_Immutable(&clear_queue,
				 gen,
				 osi_trace_consumer_gen_cache_t,
				 gen_list)) {
	res = osi_trace_consumer_gen_cache_put(gen);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}
    }

    /* XXX we'll try to destroy the cache, but if any other threads
     * still hold refs, then we'll fail miserably */
    osi_mem_object_cache_destroy(osi_trace_consumer_gen_cache.cache);

    osi_rwlock_Destroy(&osi_trace_consumer_gen_cache.lock);

 error:
    return code;
}
