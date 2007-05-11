/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi tracing framework
 * inter-process asynchronous messaging system
 * Remote Procedure Call layer
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_mem.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_condvar.h>
#include <osi/osi_mutex.h>
#include <trace/mail.h>
#include <trace/mail/rpc.h>
#include <trace/USERSPACE/mail.h>

#include <osi/osi_lib_init.h>

osi_lib_init_prototype(__osi_trace_mail_rpc_init);

osi_extern osi_init_func_t * __osi_trace_mail_rpc_init_ptr;
osi_extern osi_fini_func_t * __osi_trace_mail_rpc_fini_ptr;

osi_lib_init_decl(__osi_trace_mail_rpc_init)
{
    __osi_trace_mail_rpc_init_ptr = &osi_trace_mail_rpc_PkgInit;
    __osi_trace_mail_rpc_fini_ptr = &osi_trace_mail_rpc_PkgShutdown;
}

typedef enum {
    OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_NONE,
    OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_FUNC,
    OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV,
    OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_MAX_ID
} osi_trace_mail_rpc_callback_type_t;

typedef struct {
    osi_trace_mail_message_t * osi_volatile msg;
    osi_uint32 osi_volatile done;
    osi_condvar_t cv;
} osi_trace_mail_rpc_callback_cv_t;

typedef struct {
    osi_trace_mail_rpc_callback_t * osi_volatile func;
    void * osi_volatile sdata;
} osi_trace_mail_rpc_callback_func_t;

typedef union {
    void * osi_volatile sdata;
    osi_trace_mail_rpc_callback_func_t * osi_volatile func;
    osi_trace_mail_rpc_callback_cv_t * osi_volatile cv;
} osi_trace_mail_rpc_callback_ptr_t;

typedef struct osi_trace_mail_rpc_callback_registration {
    osi_list_element_volatile callback_hash_chain;
    osi_trace_mail_xid_t osi_volatile xid;
    osi_trace_mail_rpc_callback_type_t osi_volatile callback_type;
    osi_trace_mail_rpc_callback_ptr_t callback_ptr;
} osi_trace_mail_rpc_callback_registration_t;

#define OSI_TRACE_MAIL_RPC_CALLBACK_HASH_BUCKETS 16   /* must be of the form 2^n */
#define OSI_TRACE_MAIL_RPC_CALLBACK_HASH_MASK \
    (OSI_TRACE_MAIL_RPC_CALLBACK_HASH_BUCKETS-1)

typedef struct {
    osi_list_head_volatile hash_chain;
    osi_mutex_t chain_lock;
} osi_trace_mail_rpc_callback_hash_chain_t;

struct {
    osi_trace_mail_rpc_callback_hash_chain_t hash[OSI_TRACE_MAIL_RPC_CALLBACK_HASH_BUCKETS];
} osi_trace_mail_rpc_callback_registry;

osi_mem_object_cache_t * osi_trace_mail_rpc_callback_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_mail_rpc_callback_cv_cache = osi_NULL;
osi_mem_object_cache_t * osi_trace_mail_rpc_callback_func_cache = osi_NULL;

/*
 * this hash function is built specifically 
 * to produce a 4-bit hash value
 */
osi_static osi_inline osi_uint32
osi_trace_mail_rpc_xid_hash(osi_trace_mail_xid_t xid)
{
    osi_uint32 hash;
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    hash = (osi_uint32) (xid ^ (xid >> 32));
#else
    osi_uint32 hi, lo;
    SplitInt64(xid, hi, lo);
    hash = hi ^ lo;
#endif
    hash ^= (hash >> 4);
    hash ^= (hash >> 8);
    hash ^= (hash >> 16);
    hash &= OSI_TRACE_MAIL_RPC_CALLBACK_HASH_MASK;
    return hash;
}

osi_static int
osi_trace_mail_rpc_callback_cv_cache_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_mail_rpc_callback_cv_t * cv;

    cv = (osi_trace_mail_rpc_callback_cv_t *) buf;

    osi_condvar_Init(&cv->cv,
		     osi_trace_impl_condvar_opts());

    return 0;
}

osi_static void
osi_trace_mail_rpc_callback_cv_cache_dtor(void * buf, void * sdata)
{
    osi_trace_mail_rpc_callback_cv_t * cv;

    cv = (osi_trace_mail_rpc_callback_cv_t *) buf;

    osi_condvar_Destroy(&cv->cv);
}

/*
 * allocate a callback cv object
 *
 * [INOUT] cv_out  -- address in which to store cv object pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_mail_rpc_callback_cv_alloc(osi_trace_mail_rpc_callback_cv_t ** cv_out)
{
    osi_result res = OSI_OK;
    osi_trace_mail_rpc_callback_cv_t * cv;

    *cv_out = cv = (osi_trace_mail_rpc_callback_cv_t *)
	osi_mem_object_cache_alloc(osi_trace_mail_rpc_callback_cv_cache);
    if (osi_compiler_expect_false(cv == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    cv->msg = osi_NULL;

 error:
    return res;
}

/*
 * free a callback cv object
 *
 * [IN] cv  -- pointer to cv object
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_mail_rpc_callback_cv_free(osi_trace_mail_rpc_callback_cv_t * cv)
{
    osi_mem_object_cache_free(osi_trace_mail_rpc_callback_cv_cache,
			      cv);
    return OSI_OK;
}

/*
 * allocate a callback func object
 *
 * [INOUT] func_out  -- address in which to store func object pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_mail_rpc_callback_func_alloc(osi_trace_mail_rpc_callback_func_t ** func_out)
{
    osi_result res = OSI_OK;
    osi_trace_mail_rpc_callback_func_t * func;

    *func_out = func = (osi_trace_mail_rpc_callback_func_t *)
	osi_mem_object_cache_alloc(osi_trace_mail_rpc_callback_func_cache);
    if (osi_compiler_expect_false(func == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

 error:
    return res;
}

/*
 * free a callback func object
 *
 * [IN] func  -- pointer to func object
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_mail_rpc_callback_func_free(osi_trace_mail_rpc_callback_func_t * func)
{
    osi_mem_object_cache_free(osi_trace_mail_rpc_callback_func_cache,
			      func);
    return OSI_OK;
}

/*
 * allocate a callback object
 *
 * [IN] type       -- callback type
 * [INOUT] cb_out  -- address in which to store callback object pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_static osi_result
osi_trace_mail_rpc_callback_alloc(osi_trace_mail_rpc_callback_type_t type,
				  osi_trace_mail_rpc_callback_registration_t ** cb_out)
{
    osi_result res = OSI_OK;
    osi_trace_mail_rpc_callback_registration_t * cb;

    *cb_out = cb = (osi_trace_mail_rpc_callback_registration_t *)
	osi_mem_object_cache_alloc(osi_trace_mail_rpc_callback_cache);
    if (osi_compiler_expect_false(cb == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    switch (type) {
    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV:
	res = osi_trace_mail_rpc_callback_cv_alloc(&cb->callback_ptr.cv);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
	break;

    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_FUNC:
	res = osi_trace_mail_rpc_callback_func_alloc(&cb->callback_ptr.func);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}
	break;

    default:
	cb->callback_ptr.sdata = osi_NULL;
    }

    cb->callback_type = type;

 done:
    return res;

 error:
    if (cb != osi_NULL) {
	osi_mem_object_cache_free(osi_trace_mail_rpc_callback_cache, cb);
	*cb_out = cb = osi_NULL;
    }
    goto done;
}

/*
 * free a callback cv object
 *
 * [IN] cb  -- pointer to callback object
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
osi_trace_mail_rpc_callback_free(osi_trace_mail_rpc_callback_registration_t * cb)
{
    osi_result res = OSI_OK;

    switch (cb->callback_type) {
    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV:
	res = osi_trace_mail_rpc_callback_cv_free(cb->callback_ptr.cv);
	break;
    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_FUNC:
	res = osi_trace_mail_rpc_callback_func_free(cb->callback_ptr.func);
	break;
    }
    osi_mem_object_cache_free(osi_trace_mail_rpc_callback_cache,
			      cb);
    return OSI_OK;
}

/*
 * register a callback handler for a mail xid
 * (asynchronous rpc interface)
 *
 * [IN] xid    -- xid to associate with callback
 * [IN] func   -- function pointer to callback handler
 * [IN] sdata  -- opaque data to pass into callback handler
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_mail_rpc_callback(osi_trace_mail_xid_t xid,
			    osi_trace_mail_rpc_callback_t * cbfp,
			    void * sdata)
{
    osi_result res;
    osi_trace_mail_rpc_callback_registration_t * cb;
    osi_uint32 hash;

    hash = osi_trace_mail_rpc_xid_hash(xid);

    res = osi_trace_mail_rpc_callback_alloc(OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_FUNC,
					    &cb);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    cb->xid = xid;
    cb->callback_ptr.func->func = cbfp;
    cb->callback_ptr.func->sdata = sdata;

    osi_mutex_Lock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);
    osi_list_Append(&osi_trace_mail_rpc_callback_registry.hash[hash].hash_chain,
		    cb,
		    osi_trace_mail_rpc_callback_registration_t,
		    callback_hash_chain);
    osi_mutex_Unlock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);

 error:
    return res;
}

/*
 * setup for synchronous waiting on an xid
 * (synchronous rpc interface)
 *
 * [IN] xid             -- xid on which to wait for a response
 * [INOUT]  handle_out  -- address at which to store callback pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 */
osi_result
osi_trace_mail_rpc_sync_start(osi_trace_mail_xid_t xid,
			      osi_trace_mail_rpc_callback_handle_t * handle_out)
{
    osi_result res;
    osi_trace_mail_rpc_callback_registration_t * cb;
    osi_uint32 hash;
    osi_trace_mail_message_t * msg;

    hash = osi_trace_mail_rpc_xid_hash(xid);

    res = osi_trace_mail_rpc_callback_alloc(OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV,
					    &cb);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }
    cb->xid = xid;
    cb->callback_ptr.cv->done = 0;
    *handle_out = cb;

    osi_mutex_Lock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);
    osi_list_Append(&osi_trace_mail_rpc_callback_registry.hash[hash].hash_chain,
		    cb,
		    osi_trace_mail_rpc_callback_registration_t,
		    callback_hash_chain);
    osi_mutex_Unlock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);

 error:
    return res;
}

/*
 * cancel synchronous waiting on an xid
 * (synchronous rpc interface)
 *
 * [IN] handle  -- handle for which we wish to cancel
 *
 * returns:
 *  see osi_trace_mail_rpc_callback_free()
 */
osi_result
osi_trace_mail_rpc_sync_cancel(osi_trace_mail_rpc_callback_handle_t handle)
{
    osi_result res = OSI_OK;
    osi_trace_mail_rpc_callback_registration_t * cb;
    osi_uint32 hash;
    osi_trace_mail_message_t * msg;

    cb = handle;
    hash = osi_trace_mail_rpc_xid_hash(cb->xid);

    osi_mutex_Lock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);
    osi_list_Remove(cb,
		    osi_trace_mail_rpc_callback_registration_t,
		    callback_hash_chain);
    osi_mutex_Unlock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);

    /* handle ref races */
    if (cb->callback_type == OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV) {
	if ((cb->callback_ptr.cv->done) && 
	    (cb->callback_ptr.cv->msg != osi_NULL)) {
	    (void)osi_trace_mail_msg_put(cb->callback_ptr.cv->msg);
	}
    }

    res = osi_trace_mail_rpc_callback_free(cb);

 error:
    return res;
}

/*
 * internal function to check event queue
 * (synchronous rpc interface)
 *
 * [IN] handle   -- opaque handle to callback struct
 * [IN] msg_out  -- address in which to store message pointer
 * [IN] wait     -- flag to determine whether or not to wait
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on callback delivery failure 
 *     (e.g. we know a callback delivery was attempted, but something went
 *      wrong -- typically a msg refcount problem)
 */
osi_static osi_result
__osi_trace_mail_rpc_sync_check(osi_trace_mail_rpc_callback_handle_t handle,
				osi_trace_mail_message_t ** msg_out,
				int wait)
{
    osi_result res = OSI_OK;
    osi_trace_mail_rpc_callback_registration_t * cb;
    osi_uint32 hash;
    osi_trace_mail_message_t * msg;

    cb = handle;
    hash = osi_trace_mail_rpc_xid_hash(cb->xid);

    /* block this thread until a response message arrives */
    osi_mutex_Lock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);
    if (cb->callback_ptr.cv->done) {
	/* nothing to do */
    } else if (wait) {
	while (cb->callback_ptr.cv->done == 0) {
	    osi_condvar_Wait(&cb->callback_ptr.cv->cv,
			     &osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);
	}
    } else {
	res = OSI_FAIL;
    }
    osi_mutex_Unlock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);

    if (OSI_RESULT_OK(res)) {
	/* if we are done */
	*msg_out = msg = cb->callback_ptr.cv->msg;
	if (osi_compiler_expect_false(msg == osi_NULL)) {
	    res = OSI_FAIL;
	}

	(void)osi_trace_mail_rpc_callback_free(cb);
    }

 error:
    return res;
}

/*
 * wait for a response message on a particular xid
 * (synchronous rpc interface)
 *
 * [IN] handle   -- opaque handle to callback struct
 * [IN] msg_out  -- address in which to store message pointer
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_FAIL on callback delivery failure 
 *     (e.g. we know a callback delivery was attempted, but something went
 *      wrong -- typically a msg refcount problem)
 */
osi_result
osi_trace_mail_rpc_sync_wait(osi_trace_mail_rpc_callback_handle_t handle,
			     osi_trace_mail_message_t ** msg_out)
{
    return __osi_trace_mail_rpc_sync_check(handle, 
					   msg_out, 
					   1 /* possibly sleep on condvar */);
}

/*
 * check for a response message on a particular xid
 * (synchronous rpc interface)
 *
 * [IN] handle   -- opaque handle to callback struct
 * [IN] msg_out  -- address in which to store message pointer
 *
 * returns:
 *   see __osi_trace_mail_rpc_sync_check
 */
osi_result
osi_trace_mail_rpc_sync_check(osi_trace_mail_rpc_callback_handle_t handle,
			      osi_trace_mail_message_t ** msg_out)
{
    return __osi_trace_mail_rpc_sync_check(handle, 
					   msg_out, 
					   0 /* never sleep on condvar */);
}

/*
 * attempt to deliver a mail message to the rpc layer
 *
 * [IN] msg  -- pointer to message object
 *
 * postconditions:
 *   each delivered cv callback is given a ref on msg
 *
 * returns:
 *   OSI_OK on successful delivery
 *   OSI_FAIL if no rpc callback is registered
 */
osi_result
osi_trace_mail_rpc_deliver(osi_trace_mail_message_t * msg)
{
    osi_result res, code = OSI_OK;
    osi_uint32 hash;
    osi_trace_mail_rpc_callback_registration_t * cb, * ncb;
    osi_trace_mail_xid_t xid;

    xid = msg->envelope.env_ref_xid;
    hash = osi_trace_mail_rpc_xid_hash(xid);

    osi_mutex_Lock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);
    for (osi_list_Scan(&osi_trace_mail_rpc_callback_registry.hash[hash].hash_chain,
		       cb, ncb,
		       osi_trace_mail_rpc_callback_registration_t,
		       callback_hash_chain)) {
	if (cb->xid == xid) {
	    osi_list_Remove(cb,
			    osi_trace_mail_rpc_callback_registration_t,
			    callback_hash_chain);
	    switch (cb->callback_type) {
	    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_FUNC:
		res = (*cb->callback_ptr.func->func)(msg,
						     cb->callback_ptr.func->sdata);
		if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		    code = res;
		}
		(void)osi_trace_mail_rpc_callback_free(cb);
		break;

	    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV:
		res = osi_trace_mail_msg_get(msg);
		if (OSI_RESULT_OK_LIKELY(res)) {
		    cb->callback_ptr.cv->msg = msg;
		}
		cb->callback_ptr.cv->done = 1;
		osi_condvar_Signal(&cb->callback_ptr.cv->cv);
		break;

	    default:
		(void)osi_trace_mail_rpc_callback_free(cb);
	    }
	}
    }
    osi_mutex_Unlock(&osi_trace_mail_rpc_callback_registry.hash[hash].chain_lock);

 error:
    return code;
}

/*
 * send a message which initiates an RPC
 *
 * [IN] msg  -- pointer to message object
 *
 * returns:
 *   see osi_trace_mail_send()
 */
osi_result
osi_trace_mail_rpc_send(osi_trace_mail_message_t * msg)
{
    msg->envelope.env_flags |= OSI_TRACE_MAIL_ENV_FLAG_RPC_REQ;
    return osi_trace_mail_send(msg);
}

/*
 * make a synchronous RPC call
 *
 * [IN] request      -- request message pointer
 * [INOUT] response  -- address in which to store response message pointer
 *
 * returns:
 *   OSI_OK on success
 *   see osi_trace_mail_wait_xid()
 *   see osi_trace_mail_rpc_send()
 */
osi_result
osi_trace_mail_rpc_call(osi_trace_mail_message_t * request,
			osi_trace_mail_message_t ** response)
{
    osi_result res;
    osi_trace_mail_rpc_callback_handle_t handle;

    res = osi_trace_mail_rpc_sync_start(request->envelope.env_xid,
					&handle);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_trace_mail_rpc_send(request);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	(void)osi_trace_mail_rpc_sync_cancel(handle);
	goto error;
    }

    res = osi_trace_mail_rpc_sync_wait(handle,
				       response);

 error:
    return res;
}

osi_result
osi_trace_mail_rpc_PkgInit(void)
{
    osi_result res = OSI_OK;
    osi_uint32 i;

    osi_trace_mail_rpc_callback_cache =
	osi_mem_object_cache_create("osi_trace_mail_rpc_callback",
				    sizeof(osi_trace_mail_rpc_callback_registration_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_mail_rpc_callback_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_mail_rpc_callback_func_cache =
	osi_mem_object_cache_create("osi_trace_mail_rpc_callback_func",
				    sizeof(osi_trace_mail_rpc_callback_func_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_mail_rpc_callback_func_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    osi_trace_mail_rpc_callback_cv_cache =
	osi_mem_object_cache_create("osi_trace_mail_rpc_callback_cv",
				    sizeof(osi_trace_mail_rpc_callback_cv_t),
				    0,
				    osi_NULL,
				    &osi_trace_mail_rpc_callback_cv_cache_ctor,
				    &osi_trace_mail_rpc_callback_cv_cache_dtor,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_compiler_expect_false(osi_trace_mail_rpc_callback_cache == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    /* initialize callback hash */
    for (i = 0; i < OSI_TRACE_MAIL_RPC_CALLBACK_HASH_BUCKETS; i++) {
	osi_list_Init(&osi_trace_mail_rpc_callback_registry.hash[i].hash_chain);
	osi_mutex_Init(&osi_trace_mail_rpc_callback_registry.hash[i].chain_lock,
		       osi_trace_impl_mutex_opts());
    }

 error:
    return res;
}

osi_result
osi_trace_mail_rpc_PkgShutdown(void)
{
    osi_result res = OSI_OK;
    osi_uint32 i;
    osi_trace_mail_rpc_callback_registration_t * cb, * ncb;

    osi_mem_object_cache_destroy(osi_trace_mail_rpc_callback_cache);
    osi_mem_object_cache_destroy(osi_trace_mail_rpc_callback_cv_cache);

    for (i = 0; i < OSI_TRACE_MAIL_RPC_CALLBACK_HASH_BUCKETS; i++) {
	osi_mutex_Lock(&osi_trace_mail_rpc_callback_registry.hash[i].chain_lock);
	for (osi_list_Scan(&osi_trace_mail_rpc_callback_registry.hash[i].hash_chain,
			   cb, ncb,
			   osi_trace_mail_rpc_callback_registration_t,
			   callback_hash_chain)) {
	    osi_list_Remove(cb,
			    osi_trace_mail_rpc_callback_registration_t,
			    callback_hash_chain);
	    switch(cb->callback_type) {
	    case OSI_TRACE_MAIL_RPC_CALLBACK_TYPE_CV:
		cb->callback_ptr.cv->done = 1;
		osi_condvar_Signal(&cb->callback_ptr.cv->cv);
		break;

	    default:
		(void)osi_trace_mail_rpc_callback_free(cb);
	    }
	}
	osi_mutex_Unlock(&osi_trace_mail_rpc_callback_registry.hash[i].chain_lock);
	osi_mutex_Destroy(&osi_trace_mail_rpc_callback_registry.hash[i].chain_lock);
    }

 error:
    return res;
}
