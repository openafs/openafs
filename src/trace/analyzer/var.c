/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_rwlock.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_trace.h>
#include <trace/common/options.h>
#include <trace/analyzer/var.h>
#include <trace/analyzer/var_impl.h>
#include <trace/analyzer/counter.h>
#include <trace/analyzer/sum.h>

/*
 * osi tracing framework
 * data analysis library
 * counter component
 */

osi_mem_object_cache_t * osi_trace_anly_var_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_anly_fan_in_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_anly_fan_in_list_cache = osi_NULL;

struct osi_trace_anly_var_type_rgy_entry {
    osi_uint32 osi_volatile valid;
    osi_mem_object_cache_t * osi_volatile cache;
    osi_trace_anly_var_update_func_t * osi_volatile update_func;
    void * osi_volatile sdata;
};

struct {
    struct osi_trace_anly_var_type_rgy_entry entries[OSI_TRACE_ANLY_VAR_MAX_ID];
    osi_rwlock_t lock;
} osi_trace_anly_var_type_rgy;

/*
 * var object constructor
 */
osi_static int
__osi_trace_anly_var_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_anly_var_t * var = (osi_trace_anly_var_t *) buf;

    osi_mutex_Init(&var->var_lock, &osi_trace_common_options.mutex_opts);
    osi_refcnt_init(&var->var_refcount, 0);
    osi_list_Init(&var->var_fan_in_list);

    return 0;
}

/*
 * var object destructor
 */
osi_static void
__osi_trace_anly_var_dtor(void * buf, void * sdata)
{
    osi_trace_anly_var_t * var = (osi_trace_anly_var_t *) buf;

    osi_mutex_Destroy(&var->var_lock);
    osi_refcnt_destroy(&var->var_refcount);
}

/*
 * fan in object constructor
 */
osi_static int
__osi_trace_anly_fan_in_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_anly_fan_in_t * obj = (osi_trace_anly_fan_in_t *) buf;

    obj->var = osi_NULL;

    return 0;
}

/*
 * fan in list object constructor
 */
osi_static int
__osi_trace_anly_fan_in_list_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_anly_fan_in_list_t * obj = (osi_trace_anly_fan_in_list_t *) buf;
    osi_uint32 i;

    for (i = 0; i < OSI_TRACE_ANLY_FAN_IN_LIST_RECORD_SIZE; i++) {
	obj->fan_in[i] = osi_NULL;
    }

    return 0;
}

/*
 * register a var type
 *
 * [IN] var_type    -- variable type
 * [IN] var_cache   -- object cache for private data object
 * [IN] var_update  -- update function pointer
 * [IN] var_sdata   -- opaque data for update function pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_ANLY_ERROR_BAD_VAR_TYPE when var type is invalid
 *   OSI_TRACE_ANLY_ERROR_VAR_TYPE_REGISTERED when var type already registered
 */
osi_result
__osi_trace_anly_var_type_register(osi_trace_anly_var_type_t var_type,
				   osi_mem_object_cache_t * var_cache,
				   osi_trace_anly_var_update_func_t * var_update,
				   void * var_sdata)
{
    osi_result res = OSI_OK;

    if (var_type >= OSI_TRACE_ANLY_VAR_MAX_ID) {
	res = OSI_TRACE_ANLY_ERROR_BAD_VAR_TYPE;
	goto error;
    }

    osi_rwlock_WrLock(&osi_trace_anly_var_type_rgy.lock);
    if (osi_compiler_expect_false(osi_trace_anly_var_type_rgy.entries[var_type].valid)) {
	res = OSI_TRACE_ANLY_ERROR_VAR_TYPE_REGISTERED;
    } else {
	osi_trace_anly_var_type_rgy.entries[var_type].valid = 1;
	osi_trace_anly_var_type_rgy.entries[var_type].cache = var_cache;
	osi_trace_anly_var_type_rgy.entries[var_type].update_func = var_update;
	osi_trace_anly_var_type_rgy.entries[var_type].sdata = var_sdata;
    }
 error_sync:
    osi_rwlock_Unlock(&osi_trace_anly_var_type_rgy.lock);

 error:
    return res;
}

/*
 * unregister a var type
 *
 * [IN] var_type   -- variable type
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_ANLY_ERROR_BAD_VAR_TYPE when var type is invalid
 *   OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN when var type not registered
 */
osi_result
__osi_trace_anly_var_type_unregister(osi_trace_anly_var_type_t var_type)
{
    osi_result res = OSI_OK;

    if (var_type >= OSI_TRACE_ANLY_VAR_MAX_ID) {
	res = OSI_TRACE_ANLY_ERROR_BAD_VAR_TYPE;
	goto error;
    }

    osi_rwlock_WrLock(&osi_trace_anly_var_type_rgy.lock);
    if (osi_compiler_expect_false(!osi_trace_anly_var_type_rgy.entries[var_type].valid)) {
	res = OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN;
    } else {
	osi_trace_anly_var_type_rgy.entries[var_type].valid = 0;
    }
    osi_rwlock_Unlock(&osi_trace_anly_var_type_rgy.lock);

 error:
    return res;
}

/* allocate a fan-in object
 *
 * [IN] obj_out  -- address in which to store pointer to new fan-in object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_anly_fan_in_alloc(osi_trace_anly_fan_in_t ** obj_out)
{
    osi_result res = OSI_OK;
    osi_trace_anly_fan_in_t * obj;

    *obj_out = obj = (osi_trace_anly_fan_in_t *)
	osi_mem_object_cache_alloc(osi_trace_anly_fan_in_cache);
    if (osi_compiler_expect_false(obj == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

/* free a fan-in object
 *
 * [IN] obj  -- pointer to fan-in object
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_anly_fan_in_free(osi_trace_anly_fan_in_t * obj)
{
    osi_result res = OSI_OK;

    if (obj->var != osi_NULL) {
	res = osi_trace_anly_var_put(obj->var);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
	obj->var = osi_NULL;
    }

    osi_mem_object_cache_free(osi_trace_anly_fan_in_cache,
			      obj);

 error:
    return res;
}

/* allocate a fan-in list object
 *
 * [IN] obj_out  -- address in which to store pointer to new fan-in list object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_anly_fan_in_list_alloc(osi_trace_anly_fan_in_list_t ** obj_out)
{
    osi_result res = OSI_OK;
    osi_trace_anly_fan_in_list_t * obj;
    osi_uint32 i;

    *obj_out = obj = (osi_trace_anly_fan_in_list_t *)
	osi_mem_object_cache_alloc(osi_trace_anly_fan_in_list_cache);
    if (osi_compiler_expect_false(obj == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

/* free a fan-in list object
 *
 * [IN] obj  -- pointer to fan-in list object
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_OBJECT_REFERENCED when list object is still on a list
 *   see osi_trace_anly_fan_in_free()
 */
osi_result
osi_trace_anly_fan_in_list_free(osi_trace_anly_fan_in_list_t * obj)
{
    osi_result res = OSI_OK;
    osi_uint32 i;

    if (osi_list_IsOnList(obj, osi_trace_anly_fan_in_list_t, fan_in_list)) {
	res = OSI_ERROR_OBJECT_REFERENCED;
	goto error;
    }

    for (i = 0; i < OSI_TRACE_ANLY_FAN_IN_LIST_RECORD_SIZE ; i++) {
	if (obj->fan_in[i] != osi_NULL) {
	    res = osi_trace_anly_fan_in_free(obj->fan_in[i]);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto error;
	    }
	    obj->fan_in[i] = osi_NULL;
	}
    }

    osi_mem_object_cache_free(osi_trace_anly_fan_in_list_cache,
			      obj);

 error:
    return res;
}

/*
 * allocate a var object
 *
 * [IN] var_type    -- type of variable to allocate
 * [INOUT] var_out  -- address in which to store pointer to new var object
 * [IN] fan_in_vec  -- pointer to fan in vector
 * [IN] lock_hold   -- whether to return with lock held
 *
 * postconditions:
 *   var->var_id allocated from consumer local namespace
 *   var->var_data allocated and attached
 *   var->var_refcount = 1
 *   var->var_lock held if lock_hold is non-zero
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_TRACE_ANLY_ERROR_BAD_VAR_TYPE on invalid var_type argument
 *   OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN on var_type argument not registered
 */
osi_result 
__osi_trace_anly_var_alloc(osi_trace_anly_var_type_t var_type,
			   osi_trace_anly_var_t ** var_out,
			   osi_trace_anly_fan_in_vec_t * fan_in_vec,
			   int lock_hold)
{
    osi_result res = OSI_OK;
    osi_trace_anly_var_t * var;
    osi_mem_object_cache_t * cache = osi_NULL;
    osi_trace_anly_fan_in_list_t * fil, * nfil;
    osi_trace_anly_fan_in_t * fan_in;
    osi_uint32 i, j;

    /* allocate the basic var object and tag its type */
    *var_out = var = (osi_trace_anly_var_t *)
	osi_mem_object_cache_alloc(osi_trace_anly_var_cache);
    if (osi_compiler_expect_false(var == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    var->var_type = var_type;

    /* allocate type-specific data storage */
    osi_rwlock_RdLock(&osi_trace_anly_var_type_rgy.lock);
    if (osi_compiler_expect_false(!osi_trace_anly_var_type_rgy.entries[var_type].valid)) {
	res = OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN;
	goto rollback_sync;
    } else {
	cache = osi_trace_anly_var_type_rgy.entries[var_type].cache;
	if (cache != osi_NULL) {
	    var->var_data.raw =
		osi_mem_object_cache_alloc(cache);
	    if (osi_compiler_expect_false(var->var_data.raw == osi_NULL)) {
		res = OSI_ERROR_NOMEM;
		goto rollback_sync;
	    }
	}
    }
    osi_rwlock_Unlock(&osi_trace_anly_var_type_rgy.lock);

    /* build variable fan-in list */
    for (i = 0, j = 0 ; i < fan_in_vec->vec_len ; i++) {
	j = i % OSI_TRACE_ANLY_FAN_IN_LIST_RECORD_SIZE;
	if (!j) {
	    res = osi_trace_anly_fan_in_list_alloc(&fil);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		goto rollback;
	    }
	    osi_list_Append(&var->var_fan_in_list,
			    fil,
			    osi_trace_anly_fan_in_list_t,
			    fan_in_list);
	}
	res = osi_trace_anly_fan_in_alloc(&fan_in);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto rollback;
	}
	fil->fan_in[j] = fan_in;

	osi_mem_copy(fan_in, 
		     &fan_in_vec->vec[i], 
		     sizeof(osi_trace_anly_fan_in_t));
	if (fan_in->var != osi_NULL) {
	    res = osi_trace_anly_var_get(fan_in->var);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		fan_in->var = osi_NULL;
		goto rollback;
	    }
	}
    }

    /* initialize refcount and tie into consumer namespace */
    osi_refcnt_reset(&var->var_refcount, 1);
    res = osi_trace_consumer_local_namespace_alloc(&var->var_id);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto rollback;
    }
    if (lock_hold) {
	osi_mutex_Lock(&var->var_lock);
    }

 done:
    return res;

 rollback_sync:
    osi_rwlock_Unlock(&osi_trace_anly_var_type_rgy.lock);

 rollback:
    if (var->var_data.raw != osi_NULL) {
	if (cache != osi_NULL) {
	    osi_mem_object_cache_free(cache, var->var_data.raw);
	}
	var->var_data.raw = osi_NULL;
    }
    for (osi_list_Scan(&var->var_fan_in_list,
		       fil, nfil,
		       osi_trace_anly_fan_in_list_t,
		       fan_in_list)) {
	osi_list_Remove(fil,
			osi_trace_anly_var_fan_in_list_t,
			fan_in_list);
	osi_trace_anly_fan_in_list_free(fil);
    }
    osi_mem_object_cache_free(osi_trace_anly_var_cache, var);

 error:
    *var_out = osi_NULL;
    goto done;
}

/*
 * free a var object
 *
 * [IN] var  -- pointer to var object to free
 *
 * preconditions:
 *   var->var_refcount is zero
 *   var->var_lock is held
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_OBJECT_REFERENCED when var object is still on a var list => refcount bug
 *   OSI_FAIL on memory leak due to no object cache being registered
 *   OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN on memory leak due to var type not being registered
 */
osi_result 
__osi_trace_anly_var_free(osi_trace_anly_var_t * var)
{
    osi_result res = OSI_OK;
    osi_mem_object_cache_t * cache;
    osi_trace_anly_fan_in_list_t * fil, * nfil;

    if (osi_list_IsOnList(var, osi_trace_anly_var_t, var_list)) {
	res = OSI_ERROR_OBJECT_REFERENCED;
	goto error_sync;
    }

    if (osi_compiler_expect_false(var->var_type >= OSI_TRACE_ANLY_VAR_MAX_ID)) {
	res = OSI_TRACE_ANLY_ERROR_BAD_VAR_TYPE;
	goto error_sync;
    }

    /* try to destroy the fan-in list */
    for (osi_list_Scan(&var->var_fan_in_list,
		       fil, nfil,
		       osi_trace_anly_fan_in_list_t,
		       fan_in_list)) {
	osi_list_Remove(fil, osi_trace_anly_fan_in_list_t, fan_in_list);
	res = osi_trace_anly_fan_in_list_free(fil);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error_sync;
	}
    }

    /* try to free the type-specific data object attached to the var */
    if (var->var_data.raw != osi_NULL) {
	osi_rwlock_RdLock(&osi_trace_anly_var_type_rgy.lock);
	if (osi_compiler_expect_true(osi_trace_anly_var_type_rgy.entries[var->var_type].valid)) {
	    cache = osi_trace_anly_var_type_rgy.entries[var->var_type].cache;
	    if (cache) {
		osi_mem_object_cache_free(cache, var->var_data.raw);
	    } else {
		res = OSI_FAIL;
	    }
	} else {
	    res = OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN;
	}
	osi_rwlock_Unlock(&osi_trace_anly_var_type_rgy.lock);
	var->var_data.raw = osi_NULL;
    }

    osi_mutex_Unlock(&var->var_lock);
    osi_mem_object_cache_free(osi_trace_anly_var_cache, var);

 done:
    return res;

 error_sync:
    osi_mutex_Unlock(&var->var_lock);
    goto done;
}

/*
 * get a reference on a var object
 *
 * [IN] var  -- pointer to var object
 * 
 * returns:
 *   OSI_OK always
 */
osi_result
osi_trace_anly_var_get(osi_trace_anly_var_t * var)
{
    osi_refcnt_inc(&var->var_refcount);
    return OSI_OK;
}

/*
 * free var object refcnt action function
 *
 * [IN] obj  -- opaque pointer to var object
 *
 * returns:
 *   see __osi_trace_anly_var_free
 */
osi_static osi_result
__osi_trace_anly_var_free_action(void * obj)
{
    osi_trace_anly_var_t * var;

    var = (osi_trace_anly_var_t *) obj;
    osi_mutex_Lock(&var->var_lock);
    return __osi_trace_anly_var_free(var);
}

/*
 * put back a reference on a var object
 *
 * [IN] var  -- pointer to var object
 * 
 * preconditions:
 *   var->var_lock is NOT held
 *
 * returns:
 *   see __osi_trace_anly_var_free_action
 */
osi_result
osi_trace_anly_var_put(osi_trace_anly_var_t * var)
{
    osi_result res;
    (void)osi_refcnt_dec_action(&var->var_refcount,
				0,
				&__osi_trace_anly_var_free_action,
				var,
				&res);
    return res;
}

/*
 * update a variable based upon some new input
 *
 * [IN] var
 * [IN] update
 *
 * preconditions:
 *   caller must hold ref on var
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN if var type is not registered
 *   see return codes for registered update function pointer
 */
osi_result 
osi_trace_anly_var_update(osi_trace_anly_var_t * var,
			  osi_trace_anly_var_update_t * update)
{
    osi_result res = OSI_OK;
    osi_trace_anly_var_update_func_t * fp = osi_NULL;
    osi_trace_anly_var_input_vec_t input_vec;
    void * sdata;
    osi_uint64 output;

    osi_rwlock_RdLock(&osi_trace_anly_var_type_rgy.lock);
    if (osi_compiler_expect_false(!osi_trace_anly_var_type_rgy.entries[var->var_type].valid)) {
	res = OSI_TRACE_ANLY_ERROR_VAR_TYPE_UNKNOWN;
    } else {
	fp = osi_trace_anly_var_type_rgy.entries[var->var_type].update_func;
	sdata = osi_trace_anly_var_type_rgy.entries[var->var_type].sdata;
    }
    osi_rwlock_Unlock(&osi_trace_anly_var_type_rgy.lock);

    /* XXX build input vec */

    if (fp != osi_NULL) {
	res = (*fp)(var, update, &input_vec, sdata, &output);
    }

    /* XXX handle gate fan-out and update probe cache */

    return res;
}

/*
 * initialize the analyzer var package
 */
osi_result 
osi_trace_anly_var_PkgInit(void)
{
    osi_result code = OSI_OK;

    osi_trace_anly_var_cache =
	osi_mem_object_cache_create("osi_trace_anly_var",
				    sizeof(osi_trace_anly_var_t),
				    0,
				    osi_NULL,
				    &__osi_trace_anly_var_ctor,
				    &__osi_trace_anly_var_dtor,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_anly_var_cache == osi_NULL) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_anly_fan_in_list_cache =
	osi_mem_object_cache_create("osi_trace_anly_fan_in_list",
				    sizeof(osi_trace_anly_fan_in_list_t),
				    0,
				    osi_NULL,
				    &__osi_trace_anly_fan_in_list_ctor,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_anly_fan_in_list_cache == osi_NULL) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_anly_fan_in_cache =
	osi_mem_object_cache_create("osi_trace_anly_fan_in",
				    sizeof(osi_trace_anly_fan_in_t),
				    0,
				    osi_NULL,
				    &__osi_trace_anly_fan_in_ctor,
				    osi_NULL,
				    osi_NULL,
				    &osi_trace_common_options.mem_object_cache_opts);
    if (osi_trace_anly_fan_in_cache == osi_NULL) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return code;
}

/*
 * shut down the analyzer var package
 */
osi_result 
osi_trace_anly_var_PkgShutdown(void)
{
    osi_result code = OSI_OK;

    osi_mem_object_cache_destroy(osi_trace_anly_fan_in_list_cache);
    osi_mem_object_cache_destroy(osi_trace_anly_fan_in_cache);
    osi_mem_object_cache_destroy(osi_trace_anly_var_cache);

    return code;
}

