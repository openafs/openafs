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
#include <osi/osi_string.h>
#include <osi/osi_list.h>
#include <osi/osi_object_cache.h>
#include <trace/mail.h>
#include <trace/syscall.h>
#include <trace/consumer/module.h>
#include <trace/consumer/directory.h>
#include <trace/consumer/i2n.h>
#include <trace/consumer/i2n_mail.h>
#include <trace/consumer/i2n_thread.h>
#include <trace/consumer/cache/binary.h>
#include <trace/consumer/cache/generator.h>
#include <trace/consumer/cache/gen_coherency.h>
#include <trace/consumer/cache/probe_info.h>
#include <trace/consumer/cache/ptr_vec.h>

/* static prototypes */
osi_static osi_result 
osi_trace_consumer_i2n_cache_add_entry(osi_trace_gen_id_t gen_id,
				       osi_trace_probe_id_t probe_id,
				       const char * probe_name,
				       size_t probe_name_len);


/* 
 * add a (gen id, probe id, probe name) tuple into the cache 
 *
 * [IN] gen_id          -- generator id
 * [IN] probe_id        -- probe id
 * [IN] probe_name      -- probe name
 * [IN] probe_name_len  -- length of probe name string (excluding null term)
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result 
osi_trace_consumer_i2n_cache_add_entry(osi_trace_gen_id_t gen_id,
				       osi_trace_probe_id_t probe_id,
				       const char * probe_name,
				       size_t probe_name_len)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_bin_cache_t * bin = osi_NULL;
    osi_trace_consumer_probe_info_cache_t * probe = osi_NULL;

    res = osi_trace_consumer_gen_cache_lookup_bin(gen_id,
						  &bin);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	res = OSI_FAIL;
	goto error;
    }

    res = osi_trace_consumer_bin_cache_lookup_probe(bin,
						    probe_id,
						    &probe);
    if (OSI_RESULT_OK(res)) {
	/* nothing to do; probe is already registered */
	goto cleanup;
    }

    res = osi_trace_consumer_probe_info_cache_alloc(probe_name,
						    probe_name_len,
						    &probe);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	probe = osi_NULL;
	goto cleanup;
    }

    res = osi_trace_consumer_bin_cache_register_probe(bin,
						      probe_id,
						      probe);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto cleanup;
    }

    res = osi_trace_consumer_bin_cache_put(bin);

 error:
    return res;

 cleanup:
    if (bin) {
	osi_trace_consumer_bin_cache_put(bin);
    }
    if (probe) {
	osi_trace_consumer_probe_info_cache_put(probe);
    }
    goto error;
}

/*
 * try to do an I2N translation from the cache
 *
 * [IN] gen_id          -- generator id
 * [IN] probe_id        -- probe id
 * [OUT] probe_name     -- buffer into which to store probe name
 * [IN] probe_name_len  -- size of probe_name buffer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on failure
 */
osi_result
osi_trace_consumer_i2n_lookup_cached(osi_trace_gen_id_t gen_id,
				     osi_trace_probe_id_t probe_id,
				     char * probe_name,
				     size_t probe_name_len)
{
    osi_result res = OSI_FAIL;
    osi_trace_consumer_bin_cache_t * bin = osi_NULL;
    osi_trace_consumer_probe_info_cache_t * probe = osi_NULL;

    res = osi_trace_consumer_gen_cache_lookup_bin(gen_id,
						  &bin);
    if (OSI_RESULT_FAIL(res)) {
	goto error;
    }

    res = osi_trace_consumer_bin_cache_lookup_probe(bin,
						    probe_id,
						    &probe);
    if (OSI_RESULT_FAIL(res)) {
	goto cleanup;
    }

    res = osi_trace_consumer_probe_info_cache_lookup_name(probe,
							  probe_name,
							  probe_name_len);

    (void)osi_trace_consumer_probe_info_cache_put(probe);
    (void)osi_trace_consumer_bin_cache_put(bin);

 error:
    return res;

 cleanup:
    if (bin) {
	osi_trace_consumer_bin_cache_put(bin);
    }
    if (probe) {
	osi_trace_consumer_probe_info_cache_put(probe);
    }
    goto error;
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
osi_trace_consumer_i2n_lookup(osi_trace_gen_id_t gen_id,
			      osi_trace_probe_id_t probe_id,
			      char * probe_name,
			      size_t probe_name_len)
{
    osi_result res;

    res = osi_trace_consumer_i2n_lookup_cached(gen_id, 
					       probe_id,
					       probe_name, 
					       probe_name_len);
    if (OSI_RESULT_FAIL(res)) {
	/*
	 * XXX need to make sure that osi_trace_module_info() is called
	 * at some point to populate bin cache
	 */

	/* cache miss */
	res = osi_trace_directory_I2N(gen_id, probe_id, probe_name, 
				      probe_name_len, OSI_FALSE);
	if (OSI_RESULT_OK(res)) {
	    probe_name[probe_name_len-1] = '\0';
	    probe_name_len = osi_string_len(probe_name);
	    (void)osi_trace_consumer_i2n_cache_add_entry(gen_id, 
							 probe_id, 
							 probe_name, 
							 probe_name_len);
	} else if (res != OSI_ERROR_REQUEST_QUEUED) {
	    /* XXX someday we may want to add a negative resolve cache */
	}
    }

    return res;
}

osi_result
osi_trace_consumer_i2n_PkgInit(void)
{
    osi_result res;

    res = osi_trace_consumer_cache_ptr_vec_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_probe_info_cache_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_probe_val_cache_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_bin_cache_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_gen_cache_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_i2n_mail_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_gen_cache_coherency_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_i2n_thread_PkgInit();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_consumer_i2n_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    res = osi_trace_consumer_i2n_thread_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_gen_cache_coherency_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_i2n_mail_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_gen_cache_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_bin_cache_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_probe_val_cache_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_probe_info_cache_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_consumer_cache_ptr_vec_PkgShutdown();
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

 error:
    return res;
}
