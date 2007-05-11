/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_object_cache.h>
#include <osi/osi_thread.h>
#include <osi/osi_time.h>
#include <osi/data/heap.h>
#include <trace/consumer/local_namespace.h>
#include <trace/analyzer/var.h>
#include <trace/analyzer/var_impl.h>
#include <trace/analyzer/timer.h>
#include <trace/analyzer/timer_impl.h>

/*
 * osi tracing framework
 * data analysis library
 * timer component
 */

osi_mem_object_cache_t * osi_trace_anly_timer_cache = osi_NULL;
osi_heap_t * osi_trace_anly_timer_heap = osi_NULL;

OSI_MEM_OBJECT_CACHE_CTOR_STATIC_DECL(osi_trace_anly_timer_ctor)
{
    osi_trace_anly_timer_data_t * data = OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;

    return 0;
}

OSI_MEM_OBJECT_CACHE_DTOR_STATIC_DECL(osi_trace_anly_timer_dtor)
{
    osi_trace_anly_timer_data_t * data = OSI_MEM_OBJECT_CACHE_FUNC_ARG_BUF;
}

/*
 * create a timer object
 *
 * [OUT] var_out   -- address in which to store new var object pointer
 * [IN] var_input  -- var fan-in vector
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   see __osi_trace_anly_var_alloc for additional return codes
 */
osi_result 
osi_trace_anly_timer_create(osi_trace_anly_var_t ** var_out,
			    osi_trace_anly_fan_in_vec_t * var_input)
{
    osi_result res = OSI_OK;

    res = __osi_trace_anly_var_alloc(OSI_TRACE_ANLY_VAR_TIMER,
				     var_out,
				     var_input,
				     0);

 error:
    return res;
}

/*
 * destroy a var object
 *
 * [IN] var  -- pointer to var object
 *
 * returns:
 *   see osi_trace_anly_var_put
 */
osi_result 
osi_trace_anly_timer_destroy(osi_trace_anly_var_t * var)
{
    return osi_trace_anly_var_put(var);
}

/*
 * increment a timer object
 *
 * [IN] var        -- timer object
 * [IN] update     -- fan in data for specific element which triggered update
 * [IN] input_vec  -- fan-in input data vector
 * [IN] sdata      -- opaque data for timer type
 * [OUT] result    -- address in which to store result
 */
osi_static osi_result 
osi_trace_anly_timer_update(osi_trace_anly_var_t * var,
			    osi_trace_anly_var_update_t * update,
			    osi_trace_anly_var_input_vec_t * input_vec,
			    void * sdata,
			    osi_uint64 * result)
{
    osi_result res, code = OSI_OK;
    osi_heap_xid_t * xid;
    osi_trace_anly_timer_data_t * value = var->var_data.timer;
    osi_time32_t * key = &(var->var_data.timer->timestamp);
    osi_uint32 hi, lo;

    if (update->arg == OSI_TRACE_ANLY_TIMER_FAN_IN_TIMEOUT) {
	code = osi_heap_tx_begin(osi_trace_anly_timer_heap,
				 &xid);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	if (value->active == OSI_TRUE) {
	    code = osi_heap_tx_remove(osi_trace_anly_timer_heap,
				      xid,
				      key,
				      value);
	}
	if (OSI_RESULT_OK_LIKELY(code)) {
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
	    value->timestamp = update->val;
#else
	    SplitInt64(update->val, hi, lo);
	    value->timestamp = lo;
#endif
	    code = osi_heap_tx_insert(osi_trace_anly_timer_heap,
				      xid,
				      key,
				      value);
	    if (OSI_RESULT_OK_LIKELY(code)) {
		value->active = OSI_TRUE;
	    } else {
		value->active = OSI_FALSE;
	    }
	}
	res = osi_heap_tx_commit(osi_trace_anly_timer_heap,
				 xid);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
	*result = 0;
#else
        hi = lo = 0;
	FillInt64(*result, hi, lo);
#endif
    } else if (update->arg == OSI_TRACE_ANLY_TIMER_FAN_IN_RESET) {
	code = osi_heap_tx_begin(osi_trace_anly_timer_heap,
				 &xid);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	if (value->active == OSI_TRUE) {
	    code = osi_heap_tx_remove(osi_trace_anly_timer_heap,
				      xid,
				      key,
				      value);
	    if (OSI_RESULT_OK_LIKELY(code)) {
		value->active = OSI_FALSE;
	    }
	}
	res = osi_heap_tx_commit(osi_trace_anly_timer_heap,
				 xid);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    code = res;
	}

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
	*result = 0;
#else
        hi = lo = 0;
	FillInt64(*result, hi, lo);
#endif
    } else if (update->arg == OSI_TRACE_ANLY_TIMER_FAN_IN_FIRE) {
	*result = update->val;
    } else {
	/* bad fan-in */
	code = OSI_FAIL;
    }

 error:
    return code;
}

/*
 * fire a timer event
 *
 * [IN] var
 *
 * returns:
 *   see osi_trace_anly_var_update()
 */
osi_static osi_result
osi_trace_anly_timer_fire(osi_trace_anly_var_t * var)
{
    osi_trace_anly_var_update_t var_update;

    var_update.addr.probe = 0;
    var_update.addr.gen = 0;
    var_update.addr.arg = 0;
    var_update.addr.var = osi_NULL;
    var_update.arg = OSI_TRACE_ANLY_TIMER_FAN_IN_FIRE;
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    var_update.val = 1;
#else
    FillInt64(var_update.val, 0, 1);
#endif

    return osi_trace_anly_var_update(var,
				     &var_update);
}

/*
 * timeout heap comparator
 */
OSI_HEAP_COMPARATOR_DECL(osi_trace_anly_timer_comparator)
{
    int ret;
    osi_time32_t * key1 = OSI_HEAP_COMPARATOR_ARG_KEY1;
    osi_time32_t * key2 = OSI_HEAP_COMPARATOR_ARG_KEY2;

    if (*key1 > *key2) {
	ret = 1;
    } else if (*key1 == *key2) {
	ret = 0;
    } else {
	ret = -1;
    }

    return ret;
}

/*
 * timeout control thread
 */
OSI_THREAD_FUNC_DECL(osi_trace_anly_timer_thread)
{
    osi_result res;
    osi_trace_anly_timer_data_t * value;
    osi_time32_t * key;
    osi_heap_xid_t * xid;
    osi_time32_t cur_time;

    while (1) {
	res = osi_time_approx_get32(&cur_time,
				    OSI_TIME_APPROX_SAMP_INTERVAL_DEFAULT);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	res = osi_heap_tx_begin(osi_trace_anly_timer_heap,
				&xid);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	while (1) {
	    res = osi_heap_tx_peek_root(osi_trace_anly_timer_heap,
					xid,
					&key,
					&value);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		break;
	    }

	    if (*key > cur_time) {
		break;
	    }

	    res = osi_heap_tx_dequeue_root(osi_trace_anly_timer_heap,
					   xid,
					   &key,
					   &value);
	    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
		break;
	    }
	    value->active = OSI_FALSE;

	    osi_AssertDebug(*key <= cur_time);

	    res = osi_trace_anly_var_fire(value->var);
	}

	res = osi_heap_tx_commit(osi_trace_anly_timer_heap,
				 xid);
	if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	    goto error;
	}

	sleep(1);
    }

 error:
    return osi_NULL;
}

OSI_INIT_FUNC_DECL(osi_trace_anly_timer_PkgInit)
{
    osi_result res = OSI_OK;

    osi_trace_anly_timer_cache =
	osi_mem_object_cache_create("osi_trace_anly_timer",
				    sizeof(osi_trace_anly_timer_data_t),
				    0,
				    osi_NULL,
				    &osi_trace_anly_timer_ctor,
				    &osi_trace_anly_timer_dtor,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_trace_anly_timer_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    res = osi_heap_alloc(&osi_trace_anly_timer_heap,
			 &osi_trace_anly_timer_comparator,
			 OSI_HEAP_TYPE_MINHEAP,
			 osi_NULL,
			 osi_trace_impl_heap_opts());
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = __osi_trace_anly_var_type_register(OSI_TRACE_ANLY_VAR_TIMER,
					     osi_trace_anly_timer_cache,
					     &osi_trace_anly_timer_update,
					     osi_NULL);

 error:
    return res;
}

OSI_FINI_FUNC_DECL(osi_trace_anly_timer_PkgShutdown)
{
    osi_result res = OSI_OK;

    /* XXX safely shutdown timer thread */

    res = __osi_trace_anly_var_type_unregister(OSI_TRACE_ANLY_VAR_TIMER);
    if (OSI_RESULT_FAIL_UNLIKELY(res)) {
	goto error;
    }

    res = osi_heap_free(osi_trace_anly_timer_heap);

    osi_mem_object_cache_destroy(osi_trace_anly_timer_cache);
    osi_trace_anly_timer_cache = osi_NULL;

 error:
    return res;
}
