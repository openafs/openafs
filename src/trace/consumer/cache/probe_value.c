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
#include <osi/osi_mutex.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_event.h>
#include <trace/common/options.h>
#include <trace/consumer/cache/probe_value.h>
#include <trace/consumer/cache/probe_value_impl.h>
#include <trace/consumer/cache/probe_info.h>

/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * probe value cache
 */

struct osi_trace_consumer_probe_val_cache_directory osi_trace_consumer_probe_val_cache;


/* static prototypes */
osi_static int
osi_trace_consumer_probe_val_cache_ctor(void * buf, void * sdata, int flags);
osi_static void
osi_trace_consumer_probe_val_cache_dtor(void * buf, void * sdata);
osi_static int
osi_trace_consumer_probe_arg_val_cache_ctor(void * buf, void * sdata, int flags);
osi_static void
osi_trace_consumer_probe_arg_val_cache_dtor(void * buf, void * sdata);
osi_static osi_result
osi_trace_consumer_probe_arg_val_cache_alloc(osi_trace_consumer_probe_val_cache_t * probe,
					     osi_uint32 arg_count);
osi_static osi_result
osi_trace_consumer_probe_arg_val_cache_free(osi_trace_consumer_probe_val_cache_t * probe);
osi_static osi_result
__osi_trace_consumer_probe_val_cache_free(void * buf);


/*
 * probe value object cache element constructor
 */
osi_static int
osi_trace_consumer_probe_val_cache_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_consumer_probe_val_cache_t * probe = buf;

    probe->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_PROBE_VALUE;
    osi_mutex_Init(&probe->lock, 
		   &osi_trace_common_options.mutex_opts);
    osi_refcnt_init(&probe->refcnt, 0);
    osi_event_hook_Init(&probe->hook);

    return 0;
}

/*
 * probe value object cache element destructor
 */
osi_static void
osi_trace_consumer_probe_val_cache_dtor(void * buf, void * sdata)
{
    osi_trace_consumer_probe_val_cache_t * probe = buf;

    osi_event_hook_Destroy(&probe->hook);
    osi_mutex_Destroy(&probe->lock);
    osi_refcnt_destroy(&probe->refcnt);
    probe->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_INVALID;
}

/*
 * allocate an probe argument value vector from the cache
 * and attach it to the passed probe value object
 *
 * [IN] probe      -- pointer to probe value object to which arg value vector
 *                    will be attached
 * [IN] arg_count  -- required vector length
 *
 * preconditions:
 *   arg_count is a legal value
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_consumer_probe_arg_val_cache_alloc(osi_trace_consumer_probe_val_cache_t * probe,
					     osi_uint32 arg_count)
{
    osi_result res = OSI_OK;
    osi_uint32 index;

    /* binary search */
    if (arg_count <= osi_trace_consumer_probe_val_cache.arg_cache[1].size) {
	if (arg_count <= osi_trace_consumer_probe_val_cache.arg_cache[0].size) {
	    index = 0;
	} else {
	    index = 1;
	}
    } else {
	if (arg_count <= osi_trace_consumer_probe_val_cache.arg_cache[2].size) {
	    index = 2;
	} else {
	    index = 3;
	}
    }

    osi_mutex_Lock(&probe->lock);

    probe->arg_cache_index = index;
    probe->arg_vec = (osi_trace_consumer_probe_arg_val_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_probe_val_cache.arg_cache[index].cache);
    if (osi_compiler_expect_false(probe->arg_vec == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error_sync;
    }

 error_sync:
    osi_mutex_Unlock(&probe->lock);

 error:
    return res;
}

/*
 * free an argument value vector from a probe value object
 *
 * [IN] probe  -- pointer to probe value object
 *
 * preconditions:
 *   probe->lock held
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_consumer_probe_arg_val_cache_free(osi_trace_consumer_probe_val_cache_t * probe)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_probe_arg_val_cache_t * arg;
    osi_uint32 idx;

    osi_mutex_Lock(&probe->lock);
    arg = probe->arg_vec;
    idx = probe->arg_cache_index;
    probe->arg_vec = osi_NULL;
    probe->arg_valid = 0;
    osi_mutex_Unlock(&probe->lock);

    if (arg) {
	osi_mem_object_cache_free(osi_trace_consumer_probe_val_cache.arg_cache[idx].cache,
				  arg);
    }

    return res;
}

/*
 * allocate a probe value cache object
 *
 * [IN] probe_info  -- pointer to probe info cache object
 * [OUT] probe_out  -- address in which to store probe value object pointer
 *
 * postconditions:
 *   probe->refcnt = 1
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_probe_val_cache_alloc(osi_trace_consumer_probe_info_cache_t * probe_info,
					 osi_trace_consumer_probe_val_cache_t ** probe_out)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_probe_val_cache_t * probe;

    *probe_out = probe = (osi_trace_consumer_probe_val_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_probe_val_cache.cache);
    if (osi_compiler_expect_false(probe == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_refcnt_reset(&probe->refcnt, 1);
    probe->arg_valid = 0;
    probe->update = 0;

    probe->probe_info = probe_info;
    osi_trace_consumer_probe_info_cache_get(probe_info);

 error:
    return res;
}

/*
 * free an probe value cache entry when the refcnt reaches zero
 *
 * [IN] probe  -- pointer to probe value cache object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on error
 */
osi_static osi_result
__osi_trace_consumer_probe_val_cache_free(void * buf)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_probe_val_cache_t * probe = buf;

    if (probe->arg_vec != osi_NULL) {
	res = osi_trace_consumer_probe_arg_val_cache_free(probe);
    }

    osi_trace_consumer_probe_info_cache_put(probe->probe_info);
    probe->probe_info = osi_NULL;

    osi_mem_object_cache_free(osi_trace_consumer_probe_val_cache.cache, probe);

    return res;
}

/*
 * increment probe value cache object reference count
 *
 * [IN] probe  -- pointer to probe value cache object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_probe_val_cache_get(osi_trace_consumer_probe_val_cache_t * probe)
{
    osi_result res = OSI_OK;

    osi_refcnt_inc(&probe->refcnt);

    return res;
}

/*
 * decrement probe value cache object reference count
 *
 * [IN] probe  -- pointer to probe value cache object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on object deallocation failure
 */
osi_result
osi_trace_consumer_probe_val_cache_put(osi_trace_consumer_probe_val_cache_t * probe)
{
    osi_result res = OSI_OK;

    (void)osi_refcnt_dec_action(&probe->refcnt,
				0,
				&__osi_trace_consumer_probe_val_cache_free,
				probe,
				&res);

    return res;
}

/*
 * populate probe value cache based upon tracepoint record
 *
 * [IN] probe_info  -- pointer to probe info cache object
 * [IN] probe_val   -- pointer to probe value cache object
 * [IN] rec         -- pointer to tracepoint record object
 *
 * preconditions:
 *   probe->lock held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when rec->nargs is invalid
 */
osi_result 
osi_trace_consumer_probe_val_cache_populate(osi_trace_consumer_probe_val_cache_t * probe,
					    osi_TracePoint_record * rec)
{
    osi_result res = OSI_OK;
    osi_uint8 i, count;
    osi_uint16 mask;
    osi_time32_t update_ts;

    res = osi_time_approx_get32(&update_ts, 
				OSI_TIME_APPROX_SAMP_INTERVAL_DEFAULT);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	update_ts = 0;
    }

    osi_mutex_Lock(&probe->lock);

    count =
	osi_trace_consumer_probe_val_cache.arg_cache[probe->arg_cache_index].size;
    count = MIN(count, rec->nargs);

    for (i = 0, mask = 1; i < count; i++, mask <<= 1) {
	(void)osi_TracePoint_record_arg_get64(rec, i, 
					      &probe->arg_vec[i].arg_val);
	probe->arg_valid |= mask;
	probe->arg_vec[i].arg_update = update_ts;
    }

    probe->update = update_ts;
    osi_event_fire(&probe->hook,
		   0,
		   osi_NULL);

 error_sync:
    osi_mutex_Unlock(&probe->lock);

 error:
    return res;
}

/*
 * lookup argument cache info
 *
 * [IN] probe     -- pointer to probe value cache object
 * [IN] arg       -- argument index
 * [OUT] val_out  -- pointer to value structure
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when no cached data exists
 */
osi_result
osi_trace_consumer_probe_val_cache_lookup_arg(osi_trace_consumer_probe_val_cache_t * probe,
					      osi_uint32 arg,
					      osi_trace_consumer_probe_arg_val_cache_t * val_out)
{
    osi_result res;

    osi_mutex_Lock(&probe->lock);

    if (osi_compiler_expect_false(!(probe->arg_valid & (1 << arg)))) {
	res = OSI_FAIL;
	goto error_sync;
    }

    osi_mem_copy(val_out, &probe->arg_vec[arg], sizeof(*val_out));

 error_sync:
    osi_mutex_Unlock(&probe->lock);

 error:
    return res;
}

osi_result
osi_trace_consumer_probe_val_cache_PkgInit(void)
{
    int i;
    osi_result res = OSI_OK;

    /* setup the probe arg value cache object caches */
    osi_trace_consumer_probe_val_cache.arg_cache[0].size = 3;
    osi_trace_consumer_probe_val_cache.arg_cache[1].size = 6;
    osi_trace_consumer_probe_val_cache.arg_cache[2].size = 9;
    osi_trace_consumer_probe_val_cache.arg_cache[3].size = OSI_TRACEPOINT_MAX_ARGS;

    osi_trace_consumer_probe_val_cache.arg_cache[0].name = "osi_trace_consumer_probe_arg_cache_3";
    osi_trace_consumer_probe_val_cache.arg_cache[1].name = "osi_trace_consumer_probe_arg_cache_6";
    osi_trace_consumer_probe_val_cache.arg_cache[2].name = "osi_trace_consumer_probe_arg_cache_9";
    osi_trace_consumer_probe_val_cache.arg_cache[3].name = "osi_trace_consumer_probe_arg_cache_"
	osi_Macro_ToString(OSI_TRACEPOINT_MAX_ARGS);

    for (i = 0; i < OSI_TRACE_CONSUMER_PROBE_ARG_VAL_CACHE_BUCKETS; i++) {
	osi_trace_consumer_probe_val_cache.arg_cache[i].index = i;
	osi_trace_consumer_probe_val_cache.arg_cache[i].cache =
	    osi_mem_object_cache_create(osi_trace_consumer_probe_val_cache.arg_cache[i].name,
					(osi_trace_consumer_probe_val_cache.arg_cache[i].size *
					 sizeof(osi_trace_consumer_probe_arg_val_cache_t)),
					0,
					&osi_trace_consumer_probe_val_cache.arg_cache[i].index,
					osi_NULL,
					osi_NULL,
					osi_NULL,
					&osi_trace_common_options.mem_object_cache_opts);
	if (osi_compiler_expect_false(osi_trace_consumer_probe_val_cache.arg_cache[i].cache == osi_NULL)) {
	    res = OSI_ERROR_NOMEM;
	    goto error;
	}
    }

    /* setup the probe value object cache */
    osi_trace_consumer_probe_val_cache.cache =
	osi_mem_object_cache_create("osi_trace_consumer_probe_val_cache",
				    sizeof(osi_trace_consumer_probe_val_cache_t),
				    0,
				    osi_NULL,
				    &osi_trace_consumer_probe_val_cache_ctor,
				    &osi_trace_consumer_probe_val_cache_dtor,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_compiler_expect_false(osi_trace_consumer_probe_val_cache.cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

osi_result
osi_trace_consumer_probe_val_cache_PkgShutdown(void)
{
    int i;
    osi_result res = OSI_OK;

    osi_mem_object_cache_destroy(osi_trace_consumer_probe_val_cache.cache);
    osi_trace_consumer_probe_val_cache.cache = osi_NULL;

    for (i = 0; i < OSI_TRACE_CONSUMER_PROBE_ARG_VAL_CACHE_BUCKETS; i++) {
	osi_mem_object_cache_destroy(osi_trace_consumer_probe_val_cache.arg_cache[i].cache);
	osi_trace_consumer_probe_val_cache.arg_cache[i].cache = osi_NULL;
    }

 error:
    return res;
}
