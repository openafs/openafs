/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_cache.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_demux.h>
#include <osi/COMMON/demux_types.h>
#include <osi/COMMON/demux_impl.h>

osi_mem_object_cache_t * osi_demux_cache = osi_NULL;
osi_mem_object_cache_t * osi_demux_action_cache = osi_NULL;

osi_static int
osi_demux_cache_ctor(void * buf, void * rock, int flags);
osi_static void
osi_demux_cache_dtor(void * buf, void * rock);

osi_static osi_result
osi_demux_action_alloc(osi_demux_action_t **);
osi_static osi_result
osi_demux_action_free(osi_demux_action_t *);

osi_static int
osi_demux_cache_ctor(void * buf, void * rock, int flags)
{
    osi_demux_t * handle = buf;
    osi_uint32 i;

    for (i = 0; i < OSI_DEMUX_HASH_TABLE_SIZE; i++) {
	osi_list_Init_Head(&handle->hash_table[i].hash_chain);
	osi_rwlock_Init(&handle->hash_table[i].chain_lock,
			osi_impl_rwlock_opts());
    }

    return 0;
}

osi_static void
osi_demux_cache_dtor(void * buf, void * rock)
{
    osi_demux_t * handle = buf;
    osi_uint32 i;

    for (i = 0; i < OSI_DEMUX_HASH_TABLE_SIZE; i++) {
	osi_rwlock_Destroy(&handle->hash_table[i].chain_lock);
    }
}


/*
 * allocate a demux object
 *
 * [OUT] demux_out
 * [IN] demux_rock
 * [IN] opts
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_demux_create(osi_demux_t ** demux_out, 
		 void * demux_rock,
		 osi_demux_options_t * opts)
{
    osi_result res = OSI_OK;
    osi_demux_t * demux;

    *demux_out = demux = (osi_demux_t *)
	osi_mem_object_cache_alloc(osi_demux_cache);
    if (osi_compiler_expect_false(demux == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    demux->rock = demux_rock;
    osi_demux_options_Copy(&demux->opts, opts);

 error:
    return res;
}

/*
 * destroy a demux object
 *
 * [IN] demux
 *
 * returns:
 *   OSI_OK always
 */
osi_result
osi_demux_destroy(osi_demux_t * demux)
{
    osi_result res = OSI_OK;
    osi_demux_action_t * action, * na;
    osi_uint32 i;

    /* free all the action objects first */
    for (i = 0; i < OSI_DEMUX_HASH_TABLE_SIZE; i++) {
	osi_rwlock_WrLock(&demux->hash_table[i].chain_lock);
	for (osi_list_Scan(&demux->hash_table[i].hash_chain,
			   action, na,
			   osi_demux_action_t,
			   hash_chain)) {
	    osi_list_Remove(action,
			    osi_demux_action_t,
			    hash_chain);
	    osi_demux_action_free(action);
	}
	osi_rwlock_Unlock(&demux->hash_table[i].chain_lock);
    }

    osi_mem_object_cache_free(osi_demux_cache, demux);

    return res;
}

/*
 * allocate an action object
 *
 * [OUT] action_out
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_demux_action_alloc(osi_demux_action_t ** action_out)
{
    osi_result res = OSI_OK;
    osi_demux_action_t * action;

    *action_out = action = (osi_demux_action_t *)
	osi_mem_object_cache_alloc(osi_demux_action_cache);
    if (osi_compiler_expect_false(action == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

/*
 * free an action object
 *
 * [IN] action
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if action is still on a linked list
 */
osi_static osi_result
osi_demux_action_free(osi_demux_action_t * action)
{
    osi_result res = OSI_OK;

    if (osi_compiler_expect_false(osi_list_IsOnList(action,
						    osi_demux_action_t,
						    hash_chain))) {
	res = OSI_FAIL;
	goto error;
    }

    osi_mem_object_cache_free(osi_demux_action_cache, action);

 error:
    return res;
}

/*
 * register an action with a demux object
 *
 * [IN] demux        --
 * [IN] action       --
 * [IN] action_rock  --
 * [IN] action_fp    --
 * [IN] action_opts  -- options
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if a terminal action is already registered
 *   OSI_FAIL if identical record already exists
 *   OSI_FAIL if action function pointer is NULL
 */
osi_result
osi_demux_action_register(osi_demux_t * demux, 
			  osi_uint32 action, 
			  void * action_rock, 
			  osi_demux_action_func_t * action_fp,
			  osi_demux_action_options_t * action_opts)
{
    osi_result res, code = OSI_OK;
    osi_demux_action_t * iter, * action_obj;
    osi_uint32 chain;

    if (osi_compiler_expect_false(action_fp == osi_NULL)) {
	code = OSI_FAIL;
	goto error;
    }

    chain = OSI_DEMUX_HASH_TABLE_FUNC(action);

    res = osi_demux_action_alloc(&action_obj);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	code = res;
	goto error;
    }

    osi_rwlock_WrLock(&demux->hash_table[chain].chain_lock);

    /* check for duplicates */
    for (osi_list_Scan_Immutable(&demux->hash_table[chain].hash_chain,
				 iter,
				 osi_demux_action_t,
				 hash_chain)) {
	if ((iter->action == action) &&
	    ((iter->opts.terminal == OSI_TRUE) ||
	     ((iter->action_rock == action_rock) &&
	      (iter->action_fp == action_fp)))) {
	    res = OSI_FAIL;
	    goto error_sync;
	}
    }

    /* register new action entry */
    action_obj->action = action;
    action_obj->action_rock = action_rock;
    action_obj->action_fp = action_fp;
    osi_demux_action_options_Copy(&action_obj->opts, action_opts);
    osi_list_Append(&demux->hash_table[chain].hash_chain,
		    action_obj,
		    osi_demux_action_t,
		    hash_chain);
    osi_rwlock_Unlock(&demux->hash_table[chain].chain_lock);

 error:
    return code;

 error_sync:
    osi_rwlock_Unlock(&demux->hash_table[chain].chain_lock);
    osi_demux_action_free(action_obj);
    goto error;
}

/*
 * unregister an action from a demux object
 *
 * [IN] demux        -- demux object
 * [IN] action       -- action id
 * [IN] action_rock  -- rock to associate with this action
 * [IN] action_fp    -- action function pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL if no such record found in demux
 */
osi_result
osi_demux_action_unregister(osi_demux_t * demux, 
			    osi_uint32 action,
			    void * action_rock,
			    osi_demux_action_func_t * action_fp)
{
    osi_result res, code = OSI_FAIL;
    osi_uint32 chain;
    osi_demux_action_t * iter, * na;

    chain = OSI_DEMUX_HASH_TABLE_FUNC(action);

    osi_rwlock_WrLock(&demux->hash_table[chain].chain_lock);
    for (osi_list_Scan(&demux->hash_table[chain].hash_chain,
		       iter, na,
		       osi_demux_action_t,
		       hash_chain)) {
	if ((iter->action == action) &&
	    (iter->action_rock == action_rock) &&
	    (iter->action_fp == action_fp)) {
	    code = OSI_OK;
	    osi_list_Remove(iter,
			    osi_demux_action_t,
			    hash_chain);
	    osi_demux_action_free(iter);
	    break;
	}
    }
    osi_rwlock_Unlock(&demux->hash_table[chain].chain_lock);

    return code;
}


/*
 * perform a mux call
 *
 * [IN] demux
 * [IN] action
 * [IN] caller_rock
 *
 * returns:
 *   OSI_ERROR_DEMUX_NO_SUCH_ACTION if no action handler registered
 */
osi_result
osi_demux_call(osi_demux_t * demux,
	       osi_uint32 action,
	       void * caller_rock)
{
    osi_result res, code = OSI_OK;
    osi_demux_action_t * iter;
    osi_uint32 chain;

    chain = OSI_DEMUX_HASH_TABLE_FUNC(action);

    osi_rwlock_RdLock(&demux->hash_table[chain].chain_lock);
    for (osi_list_Scan_Immutable(&demux->hash_table[chain].hash_chain,
				 iter,
				 osi_demux_action_t,
				 hash_chain)) {
	if (iter->action == action) {
	    res = (*iter->action_fp)(action,
				     caller_rock,
				     iter->action_rock,
				     demux->rock);
	    if (OSI_RESULT_FAIL(res)) {
		if (iter->opts.error_is_fatal == OSI_TRUE) {
		    code = res;
		    break;
		}
	    }
	    if (iter->opts.terminal == OSI_TRUE) {
		break;
	    }
	}
    }
    osi_rwlock_Unlock(&demux->hash_table[chain].chain_lock);

    return code;
}



osi_result
osi_demux_PkgInit(void)
{
    osi_result res, code = OSI_OK;
    osi_size_t align;

    res = osi_cache_max_alignment(&align);
    if (OSI_RESULT_FAIL(res)) {
	align = 64;
    }

    osi_demux_cache =
	osi_mem_object_cache_create("osi_demux",
				    sizeof(osi_demux_t),
				    align,
				    osi_NULL,
				    &osi_demux_cache_ctor,
				    &osi_demux_cache_dtor,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_demux_cache == osi_NULL)) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_demux_action_cache =
	osi_mem_object_cache_create("osi_demux_action",
				    sizeof(osi_demux_action_t),
				    align,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_demux_action_cache == osi_NULL)) {
	code = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return code;
}

osi_result
osi_demux_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    if (osi_demux_cache) {
	osi_mem_object_cache_destroy(osi_demux_cache);
    }
    if (osi_demux_action_cache) {
	osi_mem_object_cache_destroy(osi_demux_action_cache);
    }

    return res;
}
