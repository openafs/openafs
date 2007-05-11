/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_mutex.h>
#include <osi/osi_string.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_event.h>
#include <trace/mail.h>
#include <trace/mail/msg.h>
#include <trace/consumer/cache/probe_info.h>
#include <trace/consumer/cache/probe_info_impl.h>

/*
 * osi tracing framework
 * trace consumer library
 * cache subsystem
 * probe metadata cache
 */

struct osi_trace_consumer_probe_info_cache_directory osi_trace_consumer_probe_info_cache;


/* static prototypes */
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_PROTOTYPE(osi_trace_consumer_probe_info_cache_ctor);
OSI_MEM_OBJECT_CACHE_DTOR_STATIC_PROTOTYPE(osi_trace_consumer_probe_info_cache_dtor);
osi_static osi_result
osi_trace_consumer_probe_arg_info_cache_alloc(osi_trace_consumer_probe_info_cache_t * probe,
					      osi_uint32 arg_count);
osi_static osi_result
osi_trace_consumer_probe_arg_info_cache_free(osi_trace_consumer_probe_info_cache_t * probe);
osi_static osi_result
__osi_trace_consumer_probe_info_cache_free(void * buf);


/*
 * probe info object cache element constructor
 */
OSI_MEM_OBJECT_CACHE_CTOR_STATIC_DECL(osi_trace_consumer_probe_info_cache_ctor)
{
    osi_trace_consumer_probe_info_cache_t * probe =
	OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;

    probe->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_PROBE_INFO;
    probe->probe_cache_index = *((osi_uint8 *)OSI_MEM_OBJECT_CACHE_FUNC_ARG_ROCK);
    osi_mutex_Init(&probe->lock, 
		   osi_trace_impl_mutex_opts());
    osi_refcnt_init(&probe->refcnt, 0);
    osi_event_hook_Init(&probe->hook);
    osi_event_hook_set_rock(&probe->hook,
			    probe);

    return 0;
}

/*
 * probe info object cache element destructor
 */
OSI_MEM_OBJECT_CACHE_DTOR_STATIC_DECL(osi_trace_consumer_probe_info_cache_dtor)
{
    osi_trace_consumer_probe_info_cache_t * probe =
	OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;

    osi_event_hook_Destroy(&probe->hook);
    osi_mutex_Destroy(&probe->lock);
    osi_refcnt_destroy(&probe->refcnt);
    probe->hdr.type = OSI_TRACE_CONSUMER_CACHE_OBJECT_INVALID;
}

/*
 * allocate an probe argument info vector from the cache
 * and attach it to the passed probe info object
 *
 * [IN] probe      -- pointer to probe info object to which arg vector
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
osi_trace_consumer_probe_arg_info_cache_alloc(osi_trace_consumer_probe_info_cache_t * probe,
					      osi_uint32 arg_count)
{
    osi_result res = OSI_OK;
    osi_uint8 index;

    /* binary search */
    if (arg_count <= osi_trace_consumer_probe_info_cache.arg_cache[1].size) {
	if (arg_count <= osi_trace_consumer_probe_info_cache.arg_cache[0].size) {
	    index = 0;
	} else {
	    index = 1;
	}
    } else {
	if (arg_count <= osi_trace_consumer_probe_info_cache.arg_cache[2].size) {
	    index = 2;
	} else {
	    index = 3;
	}
    }

    osi_mutex_Lock(&probe->lock);

    probe->arg_cache_index = index;
    probe->arg_vec = (osi_trace_consumer_probe_arg_info_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_probe_info_cache.arg_cache[index].cache);
    if (osi_compiler_expect_false(probe->arg_vec == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error_sync;
    }
    probe->arg_valid = 0;

    /* default to an event probe type */
    probe->probe_type = OSI_TRACE_PROBE_TYPE_EVENT;

 error_sync:
    osi_mutex_Unlock(&probe->lock);

    return res;
}

/*
 * free an argument info vector from a probe info object
 *
 * [IN] probe  -- pointer to probe object
 *
 * preconditions:
 *   probe->lock held
 *
 * postconditions:
 *   if arg vec was not null, it is freed
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_consumer_probe_arg_info_cache_free(osi_trace_consumer_probe_info_cache_t * probe)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_probe_arg_info_cache_t * arg;
    osi_uint8 idx;

    arg = probe->arg_vec;
    idx = probe->arg_cache_index;
    probe->arg_vec = osi_NULL;
    probe->arg_valid = 0;

    if (arg) {
	osi_mem_object_cache_free(osi_trace_consumer_probe_info_cache.arg_cache[idx].cache,
				  arg);
    }

    return res;
}

/*
 * allocate a probe info cache object
 *
 * [IN] probe_name      -- probe name string
 * [IN] probe_name_len  -- length of probe_name
 * [OUT] probe_out      -- address in which to store probe info cache object pointer
 *
 * postconditions:
 *   probe->refcnt = 1
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_consumer_probe_info_cache_alloc(const char * probe_name,
					  size_t probe_name_len,
					  osi_trace_consumer_probe_info_cache_t ** probe_out)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_probe_info_cache_t * probe;
    osi_uint8 index;

    /* binary search */
    if (probe_name_len <= osi_trace_consumer_probe_info_cache.cache[1].probe_name_len) {
	if (probe_name_len <= osi_trace_consumer_probe_info_cache.cache[0].probe_name_len) {
	    index = 0;
	} else {
	    index = 1;
	}
    } else {
	if (probe_name_len <= osi_trace_consumer_probe_info_cache.cache[2].probe_name_len) {
	    index = 2;
	} else {
	    index = 3;
	}
    }

    *probe_out = probe = (osi_trace_consumer_probe_info_cache_t *)
	osi_mem_object_cache_alloc(osi_trace_consumer_probe_info_cache.cache[index].cache);
    if (osi_compiler_expect_false(probe == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_refcnt_reset(&probe->refcnt, 1);
    probe->arg_count = 0;
    probe->arg_valid = 0;

 error:
    return res;
}

/*
 * free a probe info cache object when the refcnt reaches zero
 *
 * [IN] probe  -- opaque pointer to probe info cache object
 *
 * preconditions:
 *   probe->lock held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on error
 */
osi_static osi_result
__osi_trace_consumer_probe_info_cache_free(void * buf)
{
    osi_result res = OSI_OK;
    osi_trace_consumer_probe_info_cache_t * probe = buf;
    osi_mem_object_cache_t * pool;

    if (probe->arg_vec != osi_NULL) {
	res = osi_trace_consumer_probe_arg_info_cache_free(probe);
    }

    osi_mutex_Unlock(&probe->lock);

    pool = osi_trace_consumer_probe_info_cache.cache[probe->probe_cache_index].cache;
    osi_mem_object_cache_free(pool, probe);

    return res;
}

/*
 * subscribe to this probe's event feed
 *
 * [IN] probe  -- pointer to probe info cache object
 * [IN] sub    -- pointer to subscription object 
 *
 * returns:
 *   see osi_event_subscribe()
 */
osi_result
osi_trace_consumer_probe_info_cache_event_subscribe(osi_trace_consumer_probe_info_cache_t * probe,
						    osi_event_subscription_t * sub)
{
    osi_result res;

    res = osi_event_subscribe(&probe->hook,
			      sub);

    return res;
}

/*
 * increment probe info cache object reference count
 *
 * [IN] probe  -- pointer to probe info cache object
 *
 * preconditions:
 *   probe->lock held
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_consumer_probe_info_cache_get(osi_trace_consumer_probe_info_cache_t * probe)
{
    osi_result res = OSI_OK;

    osi_refcnt_inc(&probe->refcnt);

    return res;
}

/*
 * decrement probe info cache object reference count
 *
 * [IN] probe  -- pointer to probe info cache object
 *
 * preconditions:
 *   probe->lock held
 *
 * postconditions:
 *   probe->lock not held
 *   probe deallocated if refcount reaches zero
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on object deallocation failure
 */
osi_result
osi_trace_consumer_probe_info_cache_put(osi_trace_consumer_probe_info_cache_t * probe)
{
    osi_result res = OSI_OK;
    int code;

    code = osi_refcnt_dec_action(&probe->refcnt,
				 0,
				 &__osi_trace_consumer_probe_info_cache_free,
				 probe,
				 &res);
    if (!code) {
	/* action didn't fire */
	osi_mutex_Unlock(&probe->lock);
    }

    return res;
}

/*
 * populate a probe info cache object based upon i2n protocol message
 *
 * [IN] probe  -- probe info cache object pointer
 * [IN] res    -- i2n protocol response message pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result 
osi_trace_consumer_probe_info_cache_populate(osi_trace_consumer_probe_info_cache_t * probe,
					     osi_trace_mail_msg_probe_i2n_response_t * res)
{
    osi_result code = OSI_OK;
    osi_uint8 arg;
    osi_uint16 mask;
    osi_uint32 len;

    if (res->arg_count > OSI_TRACEPOINT_MAX_ARGS) {
	code = OSI_FAIL;
	goto error;
    }

    osi_mutex_Lock(&probe->lock);

    if (osi_trace_consumer_probe_info_cache.arg_cache[probe->arg_cache_index].size < res->arg_count) {
	code = osi_trace_consumer_probe_arg_info_cache_free(probe);
	if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	    goto error;
	}

	code = osi_trace_consumer_probe_arg_info_cache_alloc(probe, res->arg_count);
	if (OSI_RESULT_FAIL_UNLIKELY(code)) {
	    goto error;
	}
    }

    probe->probe_stability.stability = (osi_trace_probe_stability_type_t)
	res->probe_stability;
    probe->probe_stability.u.ttl = res->probe_ttl;
    probe->arg_count = res->arg_count;

    for (arg = 0, mask = 1; arg < probe->arg_count; arg++, mask <<= 1) {
	if (res->arg_override & mask) {
	    probe->arg_vec[arg].arg_stability.u.ttl = res->arg_ttl[arg];
	    probe->arg_vec[arg].arg_stability.stability = 
		(osi_trace_probe_stability_type_t) res->arg_stability[arg];
	} else {
	    probe->arg_vec[arg].arg_stability.u.ttl = res->probe_ttl;
	    probe->arg_vec[arg].arg_stability.stability = 
		(osi_trace_probe_stability_type_t) res->probe_stability;
	}
    }

    len =
	osi_trace_consumer_probe_info_cache.cache[probe->probe_cache_index].probe_name_len;
    osi_string_lcpy(probe->probe_name, res->probe_name, MIN(len, sizeof(res->probe_name)));

 error_sync:
    osi_mutex_Unlock(&probe->lock);

 error:
    return code;
}

/*
 * lookup argument cache info
 *
 * [IN] probe      -- pointer to probe info cache object
 * [IN] arg        -- argument index
 * [OUT] info_out  -- address in which to store pointer to probe arg info cache object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when no cached data exists
 */
osi_result
osi_trace_consumer_probe_info_cache_lookup_arg(osi_trace_consumer_probe_info_cache_t * probe,
					       osi_uint32 arg,
					       osi_trace_consumer_probe_arg_info_cache_t * info_out)
{
    osi_result res;

    osi_mutex_Lock(&probe->lock);

    if (osi_compiler_expect_false(arg >= probe->arg_count)) {
	res = OSI_FAIL;
	goto error_sync;
    }

    osi_mem_copy(info_out, &probe->arg_vec[arg], sizeof(*info_out));

 error_sync:
    osi_mutex_Unlock(&probe->lock);

 error:
    return res;
}

/*
 * lookup probe name
 *
 * [IN] probe           -- pointer to probe info cache object
 * [OUT] probe_name     -- buffer into which we will store probe name
 * [IN] probe_name_len  -- length of probe_name buffer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL when no cached data exists
 */
osi_result
osi_trace_consumer_probe_info_cache_lookup_name(osi_trace_consumer_probe_info_cache_t * probe,
						char * probe_name,
						size_t probe_name_len)
{
    osi_result res;

    /* XXX WARNING
     * we are really on immutability assumptions here by not
     * acquiring the probe mutex */

    osi_string_lcpy(probe_name, 
		    probe->probe_name,
		    probe_name_len);

 error:
    return res;
}

OSI_INIT_FUNC_DECL(osi_trace_consumer_probe_info_cache_PkgInit)
{
    int i;
    osi_result res = OSI_OK;

    /* setup the probe arg object caches */
    osi_trace_consumer_probe_info_cache.arg_cache[0].size = 3;
    osi_trace_consumer_probe_info_cache.arg_cache[1].size = 6;
    osi_trace_consumer_probe_info_cache.arg_cache[2].size = 9;
    osi_trace_consumer_probe_info_cache.arg_cache[3].size = OSI_TRACEPOINT_MAX_ARGS;

    osi_trace_consumer_probe_info_cache.arg_cache[0].name = 
	"osi_trace_consumer_probe_arg_info_cache_3";
    osi_trace_consumer_probe_info_cache.arg_cache[1].name = 
	"osi_trace_consumer_probe_arg_info_cache_6";
    osi_trace_consumer_probe_info_cache.arg_cache[2].name = 
	"osi_trace_consumer_probe_arg_info_cache_9";
    osi_trace_consumer_probe_info_cache.arg_cache[3].name = 
	"osi_trace_consumer_probe_arg_info_cache_"
	osi_Macro_ToString(OSI_TRACEPOINT_MAX_ARGS);

    for (i = 0; i < OSI_TRACE_CONSUMER_PROBE_ARG_INFO_CACHE_BUCKETS; i++) {
	osi_trace_consumer_probe_info_cache.arg_cache[i].index = i;
	osi_trace_consumer_probe_info_cache.arg_cache[i].cache =
	    osi_mem_object_cache_create(osi_trace_consumer_probe_info_cache.arg_cache[i].name,
					(osi_trace_consumer_probe_info_cache.arg_cache[i].size *
					 sizeof(osi_trace_consumer_probe_arg_info_cache_t)),
					0,
					&osi_trace_consumer_probe_info_cache.arg_cache[i].index,
					osi_NULL,
					osi_NULL,
					osi_NULL,
					osi_trace_impl_mem_object_cache_opts());
	if (osi_compiler_expect_false(osi_trace_consumer_probe_info_cache.arg_cache[i].cache == osi_NULL)) {
	    res = OSI_ERROR_NOMEM;
	    goto error;
	}
    }

    /* setup the probe arg object caches */
    osi_trace_consumer_probe_info_cache.cache[0].probe_name_len = 32;
    osi_trace_consumer_probe_info_cache.cache[1].probe_name_len = 48;
    osi_trace_consumer_probe_info_cache.cache[2].probe_name_len = 64;
    osi_trace_consumer_probe_info_cache.cache[3].probe_name_len = 
	OSI_TRACE_MAX_PROBE_NAME_LEN;

    osi_trace_consumer_probe_info_cache.arg_cache[0].name = 
	"osi_trace_consumer_probe_info_cache_32";
    osi_trace_consumer_probe_info_cache.arg_cache[1].name = 
	"osi_trace_consumer_probe_info_cache_48";
    osi_trace_consumer_probe_info_cache.arg_cache[2].name = 
	"osi_trace_consumer_probe_info_cache_64";
    osi_trace_consumer_probe_info_cache.arg_cache[3].name = 
	"osi_trace_consumer_probe_info_cache_"
	osi_Macro_ToString(OSI_TRACE_MAX_PROBE_NAME_LEN);

    /* setup the probe object cache */
    for (i = 0; i < OSI_TRACE_CONSUMER_PROBE_INFO_CACHE_BUCKETS; i++) {
	osi_trace_consumer_probe_info_cache.cache[i].index = i;
	osi_trace_consumer_probe_info_cache.cache[i].size = 
	    sizeof(osi_trace_consumer_probe_info_cache_t) +
	    osi_trace_consumer_probe_info_cache.cache[i].probe_name_len;
	osi_trace_consumer_probe_info_cache.cache[i].cache =
	    osi_mem_object_cache_create(osi_trace_consumer_probe_info_cache.cache[i].name,
					osi_trace_consumer_probe_info_cache.cache[i].size,
					0,
					&osi_trace_consumer_probe_info_cache.cache[i].index,
					&osi_trace_consumer_probe_info_cache_ctor,
					&osi_trace_consumer_probe_info_cache_dtor,
					osi_NULL,
					osi_trace_impl_mem_object_cache_opts());
	if (osi_compiler_expect_false(osi_trace_consumer_probe_info_cache.cache[i].cache == osi_NULL)) {
	    res = OSI_ERROR_NOMEM;
	    goto error;
	}
    }

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_consumer_probe_info_cache_PkgShutdown)
{
    osi_result res = OSI_OK;
    int i;

    for (i = 0; i < OSI_TRACE_CONSUMER_PROBE_INFO_CACHE_BUCKETS; i++) {
	osi_mem_object_cache_destroy(osi_trace_consumer_probe_info_cache.cache[i].cache);
	osi_trace_consumer_probe_info_cache.cache[i].cache = osi_NULL;
    }

    for (i = 0; i < OSI_TRACE_CONSUMER_PROBE_ARG_INFO_CACHE_BUCKETS; i++) {
	osi_mem_object_cache_destroy(osi_trace_consumer_probe_info_cache.arg_cache[i].cache);
	osi_trace_consumer_probe_info_cache.arg_cache[i].cache = osi_NULL;
    }

 error:
    return res;
}
