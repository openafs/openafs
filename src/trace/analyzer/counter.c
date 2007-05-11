/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <osi/osi_atomic.h>
#include <osi/osi_object_cache.h>
#include <trace/consumer/local_namespace.h>
#include <trace/analyzer/var.h>
#include <trace/analyzer/var_impl.h>
#include <trace/analyzer/counter.h>
#include <trace/analyzer/counter_impl.h>

/*
 * osi tracing framework
 * data analysis library
 * counter component
 */

osi_mem_object_cache_t * osi_trace_anly_counter_cache = osi_NULL;

osi_static int
osi_trace_anly_counter_ctor(void * buf, void * sdata, int flags)
{
    osi_trace_anly_counter_data_t * data = buf;

    osi_counter_init(&data->counter, 0);

    return 0;
}

osi_static void
osi_trace_anly_counter_dtor(void * buf, void * sdata)
{
    osi_trace_anly_counter_data_t * data = buf;

    osi_counter_destroy(&data->counter);
}

/*
 * create a counter object
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
osi_trace_anly_counter_create(osi_trace_anly_var_t ** var_out,
			      osi_trace_anly_fan_in_vec_t * var_input)
{
    osi_result res = OSI_OK;

    res = __osi_trace_anly_var_alloc(OSI_TRACE_ANLY_VAR_COUNTER,
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
osi_trace_anly_counter_destroy(osi_trace_anly_var_t * var)
{
    return osi_trace_anly_var_put(var);
}

/*
 * increment a counter object
 *
 * [IN] var        -- counter object
 * [IN] update     -- fan in data for specific element which triggered update
 * [IN] input_vec  -- fan-in input data vector
 * [IN] sdata      -- opaque data for counter type
 * [OUT] result    -- address in which to store result
 */
osi_static osi_result 
osi_trace_anly_counter_update(osi_trace_anly_var_t * var,
			      osi_trace_anly_var_update_t * update,
			      osi_trace_anly_var_input_vec_t * input_vec,
			      void * sdata,
			      osi_uint64 * result)
{
    osi_counter_val_t nv;

    osi_counter_inc_nv(&var->var_data.counter->counter, &nv);

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    *result = nv;
#else
    FillInt64(*result, 0, nv);
#endif

    return OSI_OK;
}

osi_result
osi_trace_anly_counter_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_trace_anly_counter_cache =
	osi_mem_object_cache_create("osi_trace_anly_counter",
				    sizeof(osi_trace_anly_counter_data_t),
				    0,
				    osi_NULL,
				    &osi_trace_anly_counter_ctor,
				    &osi_trace_anly_counter_dtor,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_trace_anly_counter_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    res = __osi_trace_anly_var_type_register(OSI_TRACE_ANLY_VAR_COUNTER,
					     osi_trace_anly_counter_cache,
					     &osi_trace_anly_counter_update,
					     osi_NULL);

 error:
    return res;
}

osi_result
osi_trace_anly_counter_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    res = __osi_trace_anly_var_type_unregister(OSI_TRACE_ANLY_VAR_COUNTER);

    osi_mem_object_cache_destroy(osi_trace_anly_counter_cache);
    osi_trace_anly_counter_cache = osi_NULL;

    return res;
}
