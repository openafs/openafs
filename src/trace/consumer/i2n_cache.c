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
#include <osi/osi_cache.h>
#include <osi/osi_mem.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_string.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <trace/mail.h>
#include <trace/consumer/activation.h>
#include <trace/consumer/cursor.h>
#include <trace/syscall.h>
#include <trace/consumer/i2n_cache.h>
#include <trace/consumer/i2n_cache_types.h>
#include <trace/consumer/i2n_cache_impl.h>
#include <trace/consumer/probe_cache_impl.h>

/*
 * osi tracing framework
 * trace consumer
 * probe id to name mapping cache
 */

/* master cache object */
struct osi_trace_consumer_i2n_cache osi_trace_consumer_i2n_cache;

/* object memory caches */
osi_mem_object_cache_t * osi_trace_consumer_i2n_cache_bin_entry_cache[OSI_TRACE_CONSUMER_I2N_CACHE_BIN_ENTRY_CACHE_BUCKETS];
osi_mem_object_cache_t * osi_trace_consumer_i2n_cache_gen_entry_cache;
osi_mem_object_cache_t * osi_trace_consumer_i2n_cache_bin_cache;
osi_mem_object_cache_t * osi_trace_consumer_i2n_cache_gen_cache;

/* ugly hacks for the bin entry object caches */
static osi_uint8 osi_trace_consumer_i2n_cache_indices[OSI_TRACE_CONSUMER_I2N_CACHE_BIN_ENTRY_CACHE_BUCKETS];
static osi_uint8 osi_trace_consumer_i2n_cache_names[OSI_TRACE_CONSUMER_I2N_CACHE_BIN_ENTRY_CACHE_BUCKETS];

/* static function prototypes */
osi_static osi_result 
osi_trace_consumer_i2n_cache_entry_alloc(const char * probe_name,
					 size_t probe_name_len,
					 osi_trace_probe_cache_t *,
					 osi_trace_consumer_i2n_cache_bin_entry_t **);
osi_static osi_result
osi_trace_consumer_i2n_cache_entry_free(osi_trace_consumer_i2n_cache_bin_entry_t *);
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_alloc(osi_trace_generator_info_t *,
				       struct osi_trace_consumer_i2n_cache_bin **);
osi_static osi_result
osi_trace_consumer_i2n_cache_bin_free(struct osi_trace_consumer_i2n_cache_bin *);
osi_static osi_result
osi_trace_consumer_i2n_cache_bin_register(struct osi_trace_consumer_i2n_cache_bin *);
osi_static osi_result
osi_trace_consumer_i2n_cache_bin_unregister(struct osi_trace_consumer_i2n_cache_bin *);
osi_static osi_result
osi_trace_consumer_i2n_cache_bin_lookup(osi_trace_generator_info_t *,
					struct osi_trace_consumer_i2n_cache_bin **,
					int keep_lock);
osi_static osi_result
osi_trace_consumer_i2n_cache_bin_get(struct osi_trace_consumer_i2n_cache_bin *,
				     int keep_lock);
osi_static osi_result
osi_trace_consumer_i2n_cache_bin_put(struct osi_trace_consumer_i2n_cache_bin *,
				     int have_lock);
osi_static osi_result 
osi_trace_consumer_i2n_cache_gen_alloc(osi_trace_generator_info_t *,
				       struct osi_trace_consumer_i2n_cache_gen **);
osi_static osi_result
osi_trace_consumer_i2n_cache_gen_free(struct osi_trace_consumer_i2n_cache_gen *);
osi_static osi_result
osi_trace_consumer_i2n_cache_gen_register(struct osi_trace_consumer_i2n_cache_gen *);
osi_static osi_result
osi_trace_consumer_i2n_cache_gen_unregister(struct osi_trace_consumer_i2n_cache_gen *);
osi_static osi_result 
osi_trace_consumer_i2n_cache_gen_lookup(osi_trace_gen_id_t,
					struct osi_trace_consumer_i2n_cache_gen **);
osi_static osi_result
osi_trace_consumer_i2n_cache_ptr_vec_extend(osi_trace_consumer_i2n_cache_ptr_vec_t *,
					    size_t needed_val);
osi_static osi_result
osi_trace_consumer_i2n_cache_ptr_vec_free(osi_trace_consumer_i2n_cache_ptr_vec_t *);
osi_static osi_result 
osi_trace_consumer_i2n_cache_add_entry(osi_trace_gen_id_t gen,
				       osi_trace_probe_id_t id,
				       const char * name,
				       size_t len,
				       osi_trace_probe_cache_t *);
osi_static osi_result
osi_trace_consumer_i2n_cache_lookup_cached(osi_trace_gen_id_t gen,
					   osi_trace_probe_id_t id,
					   char * name,
					   size_t name_len);
osi_static osi_result
osi_trace_consumer_i2n_cache_msg_i2n_res(osi_trace_mail_message_t * msg);
osi_static osi_result
osi_trace_consumer_i2n_cache_msg_gen_down(osi_trace_mail_message_t * msg);

/* mem object cache functions */
osi_static int  osi_trace_consumer_i2n_cache_entry_ctor(void * buf, void * sdata, int flags);
osi_static int  osi_trace_consumer_i2n_cache_gen_ctor(void * buf, void * sdata, int flags);
osi_static void osi_trace_consumer_i2n_cache_gen_dtor(void * buf, void * sdata);

osi_static int 
osi_trace_consumer_i2n_cache_entry_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_consumer_i2n_cache_bin_entry_t * entry;
    osi_uint8 * index = (osi_uint8 *) sdata;

    entry = (osi_trace_consumer_i2n_cache_bin_entry_t *) buf;
    entry->cache_idx = *index;
    return 0;
}

osi_static int
osi_trace_consumer_i2n_cache_bin_ctor(void * buf, void * sdata, int flags)
{
    struct osi_trace_consumer_i2n_cache_bin * bin;
    osi_rwlock_options_t opt;

    bin = (struct osi_trace_consumer_i2n_cache_bin *) buf;

    osi_rwlock_options_Init(&opt);
    osi_rwlock_options_Set(&opt, OSI_RWLOCK_OPTION_TRACE_ALLOWED, 0);
    osi_rwlock_Init(&bin->lock, &opt);
    osi_rwlock_options_Destroy(&opt);

    return 0;
}

osi_static void
osi_trace_consumer_i2n_cache_bin_dtor(void * buf, void * sdata)
{
    struct osi_trace_consumer_i2n_cache_bin * bin;

    bin = (struct osi_trace_consumer_i2n_cache_bin *) buf;
    osi_rwlock_Destroy(&bin->lock);
}

/* 
 * allocate an osi_trace_consumer_i2n_cache_bin_entry structure
 *
 * [IN] probe_name      -- probe name
 * [IN] probe_name_len  -- string length of new entry (NOT including null term)
 * [IN] probe           -- pointer to probe object
 * [INOUT] entry_out    -- pointer to location in which to store cache entry pointer
 *
 * postconditions:
 *   entry structure allocated. nhits zero'd. name field populated
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_entry_alloc(const char * probe_name,
					 size_t probe_name_len,
					 osi_trace_probe_cache_t * probe,
					 osi_trace_consumer_i2n_cache_bin_entry_t ** entry_out)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_i2n_cache_bin_entry_t * entry;
    osi_uint8 index;
    size_t len;

    len = probe_name_len + sizeof(osi_trace_consumer_i2n_cache_bin_entry_t);

    /* simple binary search to determine which cache to alloc from */
    if (len <= osi_trace_consumer_i2n_cache.entry_buckets[1]) {
	if (len <= osi_trace_consumer_i2n_cache.entry_buckets[0]) {
	    index = 0;
	} else {
	    index = 1;
	}
    } else {
	if (len <= osi_trace_consumer_i2n_cache.entry_buckets[2]) {
	    index = 2;
	} else {
	    index = 3;
	}
    }

    entry = (osi_trace_consumer_i2n_cache_bin_entry_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_i2n_cache_bin_entry_cache[index]);
    if (osi_compiler_expect_false(entry == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    entry->nhits = 0;
    osi_string_lcpy(entry->name, probe_name, probe_name_len+1);
    entry->probe = probe;
    *entry_out = entry;

 error:
    return res;
}

/* 
 * free an osi_trace_consumer_i2n_cache_bin_entry structure
 *
 * [IN] entry  -- pointer to cache entry structure
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_entry_free(osi_trace_consumer_i2n_cache_bin_entry_t * entry)
{
    osi_mem_object_cache_free(osi_trace_consumer_i2n_cache_bin_entry_cache[entry->cache_idx], 
			      entry);
    return OSI_OK;
}

/* 
 * allocate an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] info        -- new bin info structure
 * [INOUT] bin_out  -- location in which to store newly allocated bin pointer
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
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_alloc(osi_trace_generator_info_t * info,
				       struct osi_trace_consumer_i2n_cache_bin ** bin_out)
{
    osi_result res = OSI_OK;
    struct osi_trace_consumer_i2n_cache_bin * bin;

    bin = (struct osi_trace_consumer_i2n_cache_bin *)
	osi_mem_object_cache_alloc(osi_trace_consumer_i2n_cache_bin_cache);
    if (osi_compiler_expect_false(bin == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    bin->name_vec_len = 0;
    bin->name_vec = osi_NULL;
    bin->nrefs = 1;
    bin->nvalid = 0;
    bin->nhits = 0;
    bin->nmisses = 0;
    bin->info = *info;

    if (bin_out) {
	*bin_out = bin;
    }

 error:
    return res;
}

/* 
 * free an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] bin  -- pointer to cache bin structure
 *
 * preconditions:
 *   bin->nrefs == 0
 *   bin->lock not held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on non-zero refcount
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_free(struct osi_trace_consumer_i2n_cache_bin * bin)
{
    osi_result res = OSI_OK;
    osi_uint32 i;

    if (bin->nrefs) {
	res = OSI_FAIL;
	goto error;
    }

    if (bin->name_vec_len) {
	for (i = 0; i < bin->name_vec_len; i++) {
	    if (bin->name_vec[i]) {
		res = osi_trace_consumer_i2n_cache_entry_free(bin->name_vec[i]);
		if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		    goto error;
		}
		bin->name_vec[i] = osi_NULL;
	    }
	}
	res = osi_trace_consumer_i2n_cache_name_vec_free(bin);
    }

    osi_mem_object_cache_free(osi_trace_consumer_i2n_cache_bin_cache, bin);

 error:
    return res;
}

/* 
 * register an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] bin  -- bin pointer
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock is held in exclusive mode
 *
 * postconditions:
 *   bin refcount incremented
 *   bin registered in osi_trace_consumer_i2n_cache.bin_list
 *   bin registered in osi_trace_consumer_i2n_cache.bin_hash
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_register(struct osi_trace_consumer_i2n_cache_bin * bin)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;

    res = osi_trace_consumer_i2n_cache_bin_get(bin, 0);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_list_Prepend(&osi_trace_consumer_i2n_cache.bin_list,
		     bin, struct osi_trace_consumer_i2n_cache_bin, bin_list);

    hash = ((osi_uint32) bin->info.programType) & 
	OSI_TRACE_CONSUMER_I2N_CACHE_BIN_HASH_MASK;
    osi_list_Prepend(&osi_trace_consumer_i2n_cache.bin_hash[hash],
		     bin, struct osi_trace_consumer_i2n_cache_bin, bin_list);

 error:
    return res;
}

/* 
 * unregister an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] bin  -- bin pointer
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock is held in exclusive mode
 *   bin->lock held in exclusive mode
 *   bin refcount == 1
 *   bin registered in osi_trace_consumer_i2n_cache.bin_list
 *   bin registered in osi_trace_consumer_i2n_cache.bin_hash
 *
 * postconditions:
 *   bin no longer on osi_trace_consumer_i2n_cache.bin_list
 *   bin no longer in osi_trace_consumer_i2n_cache.bin_hash
 *   bin refcount == 0
 *   bin->lock not held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when refcount is wrong
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_unregister(struct osi_trace_consumer_i2n_cache_bin * bin)
{
    osi_result res = OSI_OK;
    osi_uint32 hash;

    if (bin->nrefs != 1) {
	res = OSI_FAIL;
	goto error;
    }

    osi_list_Remove(bin, struct osi_trace_consumer_i2n_cache_bin, bin_list);

    hash = ((osi_uint32) bin->info.programType) & 
	OSI_TRACE_CONSUMER_I2N_CACHE_BIN_HASH_MASK;
    osi_list_Remove(bin, struct osi_trace_consumer_i2n_cache_bin, bin_list);

    osi_trace_consumer_i2n_cache_bin_put(bin, 1);

 error:
    return res;
}

/* 
 * get a ref to an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] info        -- info structure query
 * [INOUT] bin_out  -- location in which to store bin structure pointer
 * [IN] keep_lock   -- keep lock on bin after return
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock held in shared or exclusive mode
 *   bin->lock not held
 *
 * postconditions:
 *   reference count incremented on bin
 *   if keep_lock != 0, then bin->lock held in exclusive mode
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on lookup failure
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_lookup(osi_trace_generator_info_t * info,
					struct osi_trace_consumer_i2n_cache_bin ** bin_out,
					int keep_lock)
{
    osi_result res = OSI_FAIL;
    struct osi_trace_consumer_i2n_cache_bin * bin;
    osi_uint32 hash;

    hash = ((osi_uint32) info->programType) & 
	OSI_TRACE_CONSUMER_I2N_CACHE_BIN_HASH_MASK;

    for (osi_list_Scan_Immutable(&osi_trace_consumer_i2n_cache.bin_hash[hash],
				 bin,
				 struct osi_trace_consumer_i2n_cache_bin, 
				 bin_list)) {
	if ((bin->info.programType == info->programType) &&
	    (bin->info.module_count == info->module_count) &&
	    (bin->info.module_version_cksum_type != OSI_TRACE_MODULE_CKSUM_TYPE_NONE) &&
	    (bin->info.module_version_cksum_type == info->module_version_cksum_type) &&
	    (bin->info.module_version_cksum == info->module_version_cksum)) {
	    *bin_out = bin;
	    osi_rwlock_WrLock(&bin->lock);
	    bin->nrefs++;
	    if (!keep_lock) {
		osi_rwlock_Unlock(&bin->lock);
	    }
	    res = OSI_OK;
	    break;
	}
    }

    return res;
}

/* 
 * get a ref on an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] bin        -- pointer to cache bin structure
 * [IN] keep_lock  -- hold bin->lock after return
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock held in shared or exclusive mode
 *   bin->lock not held
 *
 * postconditions:
 *   ref held on bin
 *   if keep_lock != 0, then bin->lock held in exclusive mode
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_get(struct osi_trace_consumer_i2n_cache_bin * bin,
				     int keep_lock)
{
    osi_result res = OSI_OK;

    osi_rwlock_WrLock(&bin->lock);
    bin->nrefs++;
    if (!keep_lock) {
	osi_rwlock_Unlock(&bin->lock);
    }

    return res;
}

/* 
 * put back a ref on an osi_trace_consumer_i2n_cache_bin structure
 *
 * [IN] bin        -- pointer to cache bin structure
 * [IN] have_lock  -- already hold bin->lock
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock held in shared or exclusive mode
 *   ref held on bin
 *   if have_lock != 0, then bin->lock held in exclusive mode
 *
 * postconditions:
 *   refcount on bin decremented
 *   bin->lock not held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure to deallocate bin
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_bin_put(struct osi_trace_consumer_i2n_cache_bin * bin,
				     int have_lock)
{
    osi_result res = OSI_OK;

    if (!have_lock) {
	osi_rwlock_WrLock(&bin->lock);
    }
    bin->nrefs--;
    osi_rwlock_Unlock(&bin->lock);

    return res;
}

/* 
 * allocate an osi_trace_consumer_i2n_cache_gen structure
 *
 * [IN] info        -- new generator info structure
 * [INOUT] gen_out  -- location in which to store newly allocated gen pointer
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
osi_static osi_result 
osi_trace_consumer_i2n_cache_gen_alloc(osi_trace_generator_info_t * info,
				       struct osi_trace_consumer_i2n_cache_gen ** gen_out)
{
    osi_result res = OSI_OK;
    struct osi_trace_consumer_i2n_cache_gen * gen;

    gen = (struct osi_trace_consumer_i2n_cache_gen *)
	osi_mem_object_cache_alloc(osi_trace_consumer_i2n_cache_gen_cache);
    if (osi_compiler_expect_false(gen == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    gen->info = *info;
    gen->bin = osi_NULL;

    if (gen_out) {
	*gen_out = gen;
    }

 error:
    return res;
}

/* 
 * free an osi_trace_consumer_i2n_cache_gen structure
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
osi_trace_consumer_i2n_cache_gen_free(struct osi_trace_consumer_i2n_cache_gen * gen)
{
    osi_result res = OSI_OK;

    osi_mem_object_cache_free(osi_trace_consumer_i2n_cache_gen_cache, gen);

    return res;
}

/* 
 * register an osi_trace_consumer_i2n_cache_gen structure
 *
 * [IN] gen  -- pointer to cache gen structure
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock is held in exclusive mode
 *
 * postconditions:
 *   generated registered with osi_trace_consumer_i2n_cache.gen_vec
 *   bin allocated (if necessary) and referenced
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on invalid generator id
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_gen_register(struct osi_trace_consumer_i2n_cache_gen * gen)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(gen->info.gen_id >= 
				  osi_trace_consumer_i2n_cache.gen_vec.vec_len)) {
	res = OSI_FAIL;
	goto error;
    }

    /* handle races */
    if (osi_compiler_expect_false(osi_trace_consumer_i2n_cache.gen_vec.vec[gen->info.gen_id].opaque != osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    /* make sure associated bin structure exists, and get a ref on it */
    if (OSI_RESULT_FAIL(osi_trace_consumer_i2n_cache_bin_lookup(&gen->info,
								&gen->bin,
								0))) {
	res = osi_trace_consumer_i2n_cache_bin_alloc(&gen->info, &gen->bin);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
	res = osi_trace_consumer_i2n_cache_bin_register(gen->bin);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    osi_trace_consumer_i2n_cache_bin_put(gen->bin, 0);
	    osi_trace_consumer_i2n_cache_bin_free(gen->bin);
	    gen->bin = osi_NULL;
	    goto error;
	}
    }

    /* register this gen */
    osi_trace_consumer_i2n_cache.gen_vec[gen->info.gen_id] = gen;
    osi_list_Prepend(&osi_trace_consumer_i2n_cache.gen_list,
		     gen, struct osi_trace_consumer_i2n_cache_gen, gen_list);

 error:
    return res;
}

/* 
 * unregister an osi_trace_consumer_i2n_cache_gen structure
 *
 * [IN] gen  -- pointer to cache gen structure
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock is held in exclusive mode
 *
 * postconditions:
 *   bin ref dropped
 *   generator no longer referenced in osi_trace_consumer_i2n_cache.gen_vec
 *   generator no longer on osi_trace_consumer_i2n_cache.gen_list
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_gen_unregister(struct osi_trace_consumer_i2n_cache_gen * gen)
{
    osi_result res = OSI_OK;

    /* handle races */
    if (osi_compiler_expect_false(osi_trace_consumer_i2n_cache.gen_vec[gen->info.gen_id] == osi_NULL)) {
	res = OSI_FAIL;
	goto error;
    }

    /* drop bin ref */
    if (osi_compiler_expect_true(gen->bin != osi_NULL)) {
	res = osi_trace_consumer_i2n_cache_bin_put(gen->bin, 0);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
	gen->bin = osi_NULL;
    }

    /* unregister this gen */
    osi_trace_consumer_i2n_cache.gen_vec[gen->info.gen_id] = osi_NULL;
    osi_list_Remove(gen, struct osi_trace_consumer_i2n_cache_gen, gen_list);

 error:
    return res;
}

/* 
 * lookup an osi_trace_consumer_i2n_cache_gen structure
 *
 * [IN] gen_id      -- generator id
 * [INOUT] gen_out  -- location in which to store gen pointer
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock is held either shared or exclusive mode
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on invalid generator id
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_gen_lookup(osi_trace_gen_id_t gen_id,
					struct osi_trace_consumer_i2n_cache_gen ** gen_out)
{
    osi_result res = OSI_FAIL;

    if (osi_compiler_expect_true(gen_id < osi_trace_consumer_i2n_cache.gen_vec.vec_len)) {
	*gen_out = osi_trace_consumer_i2n_cache.gen_vec[gen_id];
	if (osi_trace_consumer_i2n_cache.gen_vec[gen_id] != osi_NULL) {
	    res = OSI_OK;
	}
    }

    return res;
}

/* 
 * extend an osi_trace_consumer_i2n_cache_ptr_vec_t to at least the desired length
 *
 * [IN] vec         -- pointer to osi_trace_consumer_i2n_cache_ptr_vec_t structure
 * [IN] needed_val  -- new maximum vector index value needed
 *
 * preconditions:
 *   lock which controls access to this vector must be held exclusively
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_consumer_i2n_cache_ptr_vec_extend(osi_trace_consumer_i2n_cache_ptr_vec_t * vec,
					    size_t needed_val)
{
    osi_result res = OSI_OK;
    void * new_vec;
    osi_trace_consumer_i2n_cache_bin_entry_t ** new_vec;
    size_t align, delta, old_buf_sz, delta_buf_sz;

    if (needed_val < vec->vec_len) {
	return OSI_OK;
    }

    align = osi_trace_consumer_i2n_cache.align;

    old_buf_sz = vec->vec_len * 
	sizeof(osi_trace_consumer_i2n_cache_object_ptr_t);

    delta = (needed_val + 1) - vec->vec_len;
    if (delta & (osi_trace_consumer_i2n_cache.ptr_vec_chunk_size - 1)) {
	delta &= ~(osi_trace_consumer_i2n_cache.ptr_vec_chunk_size-1);
	delta += osi_trace_consumer_i2n_cache.ptr_vec_chunk_size;
    }

    delta_buf_sz = delta * 
	sizeof(osi_trace_consumer_i2n_cache_object_ptr_t);

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
 * free the vector associated with an osi_trace_consumer_i2n_cache_ptr_vec_t structure
 *
 * [IN] vec  -- pointer to osi_trace_consumer_i2n_cache_ptr_vec_t structure
 *
 * preconditions:
 *   lock which controls access to this vector is held exclusively
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_consumer_i2n_cache_ptr_vec_free(osi_trace_consumer_i2n_cache_ptr_vec_t * vec)
{
    osi_result res = OSI_OK;

    if (vec->vec_len) {
	osi_mem_aligned_free(vec->vec, 
			     vec->vec_len * sizeof(osi_trace_consumer_i2n_cache_object_ptr_t));
    }
    vec->vec = osi_NULL;
    vec->vec_len = 0;

    return res;
}

/* 
 * add a (gen id, probe id, probe name) tuple into the cache 
 *
 * [IN] gen_id          -- generator id
 * [IN] probe_id        -- probe id
 * [IN] probe_name      -- probe name
 * [IN] probe_name_len  -- length of probe name string (excluding null term)
 * [IN] probe           -- pointer to probe object
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock not held
 *   probe->lock held
 *
 * postconditions:
 *   probe->refcnt incremented
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_add_entry(osi_trace_gen_id_t gen_id,
				       osi_trace_probe_id_t probe_id,
				       const char * probe_name,
				       size_t probe_name_len,
				       osi_trace_probe_cache_t * probe)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_i2n_cache_bin_entry_t * entry = osi_NULL;
    struct osi_trace_consumer_i2n_cache_gen * gen = osi_NULL;
    struct osi_trace_consumer_i2n_cache_bin * bin = osi_NULL;

    res = osi_trace_consumer_i2n_cache_entry_alloc(probe_name, 
						   probe_name_len, 
						   probe,
						   &entry);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    osi_rwlock_RdLock(&osi_trace_consumer_i2n_cache.lock);

    res = osi_trace_consumer_i2n_cache_gen_lookup(gen_id, &gen);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error_sync;
    }

    res = osi_trace_consumer_i2n_cache_bin_get(gen->bin, 1);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error_sync;
    }
    bin = gen->bin;

    if (probe_id >= bin->name_vec_len) {
	res = osi_trace_consumer_i2n_cache_name_vec_extend(bin, probe_id);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error_sync;
	}
    }

    bin->name_vec[probe_id] = entry;
    res = osi_trace_consumer_i2n_cache_bin_put(gen->bin, 1);
    osi_rwlock_Unlock(&osi_trace_consumer_i2n_cache.lock);

 done:
    return res;

 error_sync:
    if (bin) {
	osi_trace_consumer_i2n_cache_bin_put(bin, 1);
    }
    osi_rwlock_Unlock(&osi_trace_consumer_i2n_cache.lock);

 error:
    if (entry) {
	osi_trace_consumer_i2n_cache_entry_free(entry);
    }
    goto done;
}

/*
 * try to do an I2N translation from the cache
 *
 * [IN] gen_id          -- generator id
 * [IN] probe_id        -- probe id
 * [OUT] probe_name     -- buffer into which to store probe name
 * [IN] probe_name_len  -- size of probe_name buffer
 *
 * preconditions:
 *   osi_trace_consumer_i2n_cache.lock not held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure
 */
osi_static osi_result
osi_trace_consumer_i2n_cache_lookup_cached(osi_trace_gen_id_t gen_id,
					   osi_trace_probe_id_t probe_id,
					   char * probe_name,
					   size_t probe_name_len)
{
    osi_result res = OSI_FAIL;
    struct osi_trace_consumer_i2n_cache_gen * gen;
    struct osi_trace_consumer_i2n_cache_bin * bin;
    osi_trace_consumer_i2n_cache_bin_entry_t * entry;

    if (osi_compiler_expect_false(gen_id >= osi_trace_consumer_i2n_cache.gen_vec_len)) {
	goto error;
    }

    osi_rwlock_RdLock(&osi_trace_consumer_i2n_cache.lock);
    gen = osi_trace_consumer_i2n_cache.gen_vec[gen_id];
    if (osi_compiler_expect_true(gen != osi_NULL)) {
	bin = gen->bin;
	if (osi_compiler_expect_true(bin != osi_NULL)) {
	    osi_rwlock_RdLock(&bin->lock);
	    if (osi_compiler_expect_true(bin->name_vec_len > probe_id)) {
		entry = bin->name_vec[probe_id];
		if (osi_compiler_expect_true(entry != osi_NULL)) {
		    osi_string_lcpy(probe_name, entry->name, probe_name_len);
		    res = OSI_OK;

		    /* update stats; yeah, racy, but write lock for lookup seems stupid */
		    entry->nhits++;
		    bin->nhits++;
		} else {
		    bin->nmisses++;
		}
	    } else {
		bin->nmisses++;
	    }
	}
    }
    osi_rwlock_Unlock(&bin->lock);
    osi_rwlock_Unlock(&osi_trace_consumer_i2n_cache.lock);

 error:
    return res;
}

/*
 * try to do a probe lookup from the cache
 *
 * [IN] gen_id          -- generator id
 * [IN] probe_id        -- probe id
 * [INOUT] probe_out    -- address in which to store probe pointer
 *
 * postconditions:
 *   returns probe object locked
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure
 */
osi_result
osi_trace_consumer_i2n_cache_lookup_probe(osi_trace_gen_id_t gen_id,
					  osi_trace_probe_id_t probe_id,
					  osi_trace_probe_cache_t ** probe_out)
{
    osi_result res = OSI_FAIL;
    struct osi_trace_consumer_i2n_cache_gen * gen;
    struct osi_trace_consumer_i2n_cache_bin * bin;
    osi_trace_consumer_i2n_cache_bin_entry_t * entry;

    if (osi_compiler_expect_false(gen_id >= osi_trace_consumer_i2n_cache.gen_vec_len)) {
	goto error;
    }

    osi_rwlock_RdLock(&osi_trace_consumer_i2n_cache.lock);
    gen = osi_trace_consumer_i2n_cache.gen_vec[gen_id];
    if (osi_compiler_expect_true(gen != osi_NULL)) {
	bin = gen->bin;
	if (osi_compiler_expect_true(bin != osi_NULL)) {
	    osi_rwlock_RdLock(&bin->lock);
	    if (osi_compiler_expect_true(bin->name_vec_len > probe_id)) {
		entry = bin->name_vec[probe_id];
		if (osi_compiler_expect_true(entry != osi_NULL)) {
		    if (osi_compiler_expect_true(entry->probe != osi_NULL)) {
			*probe_out = entry->probe;
			res = OSI_OK;
			osi_mutex_Lock(&entry->probe->lock);
		    }

		    /* update stats; yeah, racy, but write lock for lookup seems stupid */
		    entry->nhits++;
		    bin->nhits++;
		} else {
		    bin->nmisses++;
		}
	    } else {
		bin->nmisses++;
	    }
	}
    }
    osi_rwlock_Unlock(&bin->lock);
    osi_rwlock_Unlock(&osi_trace_consumer_i2n_cache.lock);

 error:
    return res;
}

/*
 * perform I2N translation, preferring i2n cache, then going out of process
 *
 * [IN] gen_id          -- generator id
 * [IN] probe_id        -- probe id
 * [INOUT] probe_name   -- buffer into which to copy probe name
 * [IN] probe_name_len  -- length of probe name buffer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on lookup failure
 */
osi_result
osi_trace_consumer_i2n_cache_lookup(osi_trace_gen_id_t gen_id,
				    osi_trace_probe_id_t probe_id,
				    char * probe_name,
				    size_t probe_name_len)
{
    osi_result res;

    res = osi_trace_consumer_i2n_cache_lookup_cached(gen_id, 
						     probe_id,
						     probe_name, 
						     probe_name_len);
    if (OSI_RESULT_FAIL(res)) {
	/* cache miss */
	res = osi_trace_directory_I2N(gen_id, probe_id, probe_name, 
				      probe_name_len);
	if (OSI_RESULT_OK(res)) {
	    probe_name[probe_name_len-1] = '\0';
	    probe_name_len = osi_string_len(probe_name);
	    (void)osi_trace_consumer_i2n_cache_add_entry(gen_id, 
							 probe_id, 
							 probe_name, 
							 probe_name_len,
							 osi_NULL);
	} else if (res != OSI_ERROR_REQUEST_QUEUED) {
	    /* XXX someday we may want to add a negative resolve cache */
	}
    }

    return res;
}


osi_result
osi_trace_consumer_i2n_cache_PkgInit(void)
{
    int i;
    osi_result res;
    osi_uint32 max_id;
    size_t align;

    osi_mem_zero(&osi_trace_consumer_i2n_cache, sizeof(osi_trace_consumer_i2n_cache));

    if (OSI_RESULT_FAIL_UNLIKELY(osi_cache_max_alignment(&align))) {
	align = 64;
    }
    osi_trace_consumer_i2n_cache.align = align;

    res = osi_trace_consumer_kernel_config_lookup(OSI_TRACE_KERNEL_CONFIG_GEN_RGY_MAX_ID,
						  &max_id);
    if (OSI_RESULT_FAIL(res)) {
	max_id = OSI_TRACE_GEN_RGY_MAX_ID;
    }

    osi_list_Init(&osi_trace_consumer_i2n_cache.bin_list);
    for (i = 0; i < OSI_TRACE_CONSUMER_I2N_CACHE_BIN_HASH_BUCKETS; i++) {
	osi_list_Init(&osi_trace_consumer_i2n_cache.bin_hash[i]);
    }
    osi_rwlock_Init(&osi_trace_consumer_i2n_cache.lock, &rwopts);

    /* initialize the gen_vec */
    osi_trace_consumer_i2n_cache.gen_vec = (struct osi_trace_consumer_i2n_cache_gen **)
	osi_mem_aligned_alloc((max_id + 1) * sizeof(struct osi_trace_consumer_i2n_cache_gen *), 
			      align);
    if (osi_compiler_expect_false(osi_trace_consumer_i2n_cache.gen_vec == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }
    osi_trace_consumer_i2n_cache.gen_vec_len = max_id + 1;
    osi_mem_zero(osi_trace_consumer_i2n_cache.gen_vec,
		 (max_id + 1) * sizeof(struct osi_trace_consumer_i2n_cache_gen *));


    osi_trace_consumer_i2n_cache.ptr_vec_chunk_size = 
	OSI_TRACE_CONSUMER_I2N_CACHE_PTR_VEC_CHUNK_SIZE_DEFAULT;

    /* setup the entry object caches */
    osi_trace_consumer_i2n_cache.entry_buckets[0] = 32;
    osi_trace_consumer_i2n_cache.entry_buckets[1] = 48;
    osi_trace_consumer_i2n_cache.entry_buckets[2] = 64;
    osi_trace_consumer_i2n_cache.entry_buckets[3] = 256;

    osi_trace_consumer_i2n_cache_names[0] = "osi_trace_consumer_i2n_cache_bin_entry_cache_32";
    osi_trace_consumer_i2n_cache_names[1] = "osi_trace_consumer_i2n_cache_bin_entry_cache_48";
    osi_trace_consumer_i2n_cache_names[2] = "osi_trace_consumer_i2n_cache_bin_entry_cache_64";
    osi_trace_consumer_i2n_cache_names[3] = "osi_trace_consumer_i2n_cache_bin_entry_cache_256";

    for (i = 0; i < OSI_TRACE_CONSUMER_I2N_CACHE_BIN_ENTRY_CACHE_BUCKETS; i++) {
	osi_trace_consumer_i2n_cache_indices[i] = i;
	osi_trace_consumer_i2n_cache_bin_entry_cache[i] =
	    osi_mem_object_cache_create(osi_trace_consumer_i2n_cache_names[i],
					(osi_trace_consumer_i2n_cache.entry_buckets[i] +
					 sizeof(osi_trace_consumer_i2n_cache_bin_entry_t)),
					0,
					&osi_trace_consumer_i2n_cache_indices[i],
					&osi_trace_consumer_i2n_cache_entry_ctor,
					osi_NULL,
					osi_NULL,
					&cache_opts);
    }

    /* setup the other object caches */
    osi_trace_consumer_i2n_cache_bin_cache =
	osi_mem_object_cache_create("osi_trace_consumer_i2n_cache_bin_cache",
				    sizeof(struct osi_trace_consumer_i2n_cache_bin),
				    align,
				    osi_NULL,
				    &osi_trace_consumer_i2n_cache_bin_ctor,
				    &osi_trace_consumer_i2n_cache_bin_dtor,
				    osi_NULL,
				    &cache_opts);

    osi_trace_consumer_i2n_cache_gen_cache =
	osi_mem_object_cache_create("osi_trace_consumer_i2n_cache_gen_cache",
				    sizeof(struct osi_trace_consumer_i2n_cache_gen),
				    align,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    &cache_opts);

   res = osi_trace_consumer_i2n_cache_mail_PkgInit();

 error:
    return res;
}

osi_result
osi_trace_consumer_i2n_cache_PkgShutdown(void)
{
    int i;
    osi_result res;
    osi_trace_consumer_i2n_cache_bin_entry_t * entry;
    struct osi_trace_consumer_i2n_cache_bin * bin, * nbin;
    struct osi_trace_consumer_i2n_cache_gen * gen, * ngen;

    res = osi_trace_consumer_i2n_cache_mail_PkgShutdown();

    osi_rwlock_WrLock(&osi_trace_consumer_i2n_cache.lock);

    for (osi_list_Scan(&osi_trace_consumer_i2n_cache.gen_list,
		       gen, ngen,
		       struct osi_trace_consumer_i2n_cache_gen,
		       gen_list)) {
	res = osi_trace_consumer_i2n_cache_gen_unregister(gen);
	if (OSI_RESULT_OK_LIKELY(res)) {
	    osi_trace_consumer_i2n_cache_gen_free(gen);
	}
    }

    for (osi_list_Scan(&osi_trace_consumer_i2n_cache.bin_list,
		       bin, nbin,
		       struct osi_trace_consumer_i2n_cache_bin,
		       bin_list)) {
	res = osi_trace_consumer_i2n_cache_bin_unregister(bin);
	if (OSI_RESULT_OK_LIKELY(res)) {
	    osi_trace_consumer_i2n_cache_bin_free(bin);
	}
    }

    osi_mem_object_cache_destroy(osi_trace_consumer_i2n_cache_gen_cache);
    osi_mem_object_cache_destroy(osi_trace_consumer_i2n_cache_bin_cache);
    for (i = 0; i < OSI_TRACE_CONSUMER_I2N_CACHE_BIN_ENTRY_CACHE_BUCKETS; i++) {
	osi_mem_object_cache_destroy(osi_trace_consumer_i2n_cache_bin_entry_cache[i]);
    }

    if (osi_trace_consumer_i2n_cache.gen_vec) {
	osi_mem_aligned_free(osi_trace_consumer_i2n_cache.gen_vec,
			     osi_trace_consumer_i2n_cache.gen_vec_len *
			     sizeof(struct osi_trace_consumer_i2n_cache_gen *));
	osi_trace_consumer_i2n_cache.gen_vec = osi_NULL;
	osi_trace_consumer_i2n_cache.gen_vec_len = 0;
    }

 error_sync:
    osi_rwlock_Unlock(&osi_trace_consumer_i2n_cache.lock);
    osi_rwlock_Destroy(&osi_trace_consumer_i2n_cache.lock);

 error:
    return res;
}
