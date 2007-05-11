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
#include <osi/osi_mem.h>
#include <osi/osi_object_cache.h>
#include <trace/consumer/local_namespace.h>
#include <trace/analyzer/var.h>
#include <trace/analyzer/var_impl.h>
#include <trace/analyzer/logic.h>
#include <trace/analyzer/logic_impl.h>

/*
 * osi tracing framework
 * data analysis library
 * logic component
 */

osi_mem_object_cache_t * osi_trace_anly_logic_cache = osi_NULL;

/*
 * create a logic object
 *
 * [OUT] var_out    -- address in which to store new var object pointer
 * [IN] logic_op    -- boolean logic operation to perform
 * [IN] var_inputs  -- var object fan-in vector
 *
 * returns:
 *   OSI_OK on success
 *   OSI_ERROR_NOMEM on out of memory condition
 *   OSI_TRACE_ANLY_ERROR_INVALID_LOGIC_OP invalid logic operation passed
 *   see __osi_trace_anly_var_alloc for additional return codes
 */
osi_result 
osi_trace_anly_logic_create(osi_trace_anly_var_t ** var_out,
			    osi_trace_anly_logic_op_t logic_op,
			    osi_trace_anly_fan_in_vec_t * var_input)
{
    osi_result res = OSI_OK;
    osi_trace_anly_var_t * var;

    if (osi_compiler_expect_false(logic_op >= OSI_TRACE_ANLY_LOGIC_OP_MAX_ID)) {
	res = OSI_TRACE_ANLY_ERROR_INVALID_LOGIC_OP;
	goto error;
    }

    res = __osi_trace_anly_var_alloc(OSI_TRACE_ANLY_VAR_LOGIC,
				     &var,
				     var_input,
				     0);

    if (OSI_RESULT_OK_LIKELY(res)) {
	var->var_data.logic->logic_op = logic_op;
    }

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
osi_trace_anly_logic_destroy(osi_trace_anly_var_t * var)
{
    return osi_trace_anly_var_put(var);
}

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
typedef osi_uint64 osi_trace_anly_logic_eval_t;
#else /* !OSI_ENV_NATIVE_INT64_TYPE */
typedef osi_uint32 osi_trace_anly_logic_eval_t;
osi_static void
__osi_trace_anly_logic_eval_convert(osi_trace_anly_var_input_vec_t * in,
				    osi_trace_anly_logic_eval_t * out)
{
    size_t i;
    osi_uint32 hi, lo;

    for (i = 0; i < in->vec_len ; i++) {
	SplitInt64(in->vec[i], hi, lo);
	out[i] = hi | lo;
    }
}
#endif /* !OSI_ENV_NATIVE_INT64_TYPE */

/*
 * internal procedure to evaluate boolean NOT
 *
 * [IN] var      -- pointer to var object
 * [IN] val1     -- value 1
 * [OUT] result  -- address into which to store result
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
__osi_trace_anly_logic_evaluate_NOT(osi_trace_anly_var_t * var,
				    osi_uint64 val1,
				    osi_uint64 * result)
{
    osi_result res = OSI_OK;

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    *result = !val1;
#else
    osi_uint32 v1, hi, lo;
    SplitInt64(val1, hi, lo);
    v1 = hi | lo;
    lo = !v1;
    FillInt64(*result, 0, lo);
#endif

    return res;
}

/*
 * internal procedure to evaluate boolean AND
 *
 * [IN] var      -- pointer to var object
 * [IN] val_vec  -- value vector
 * [IN] vec_len  -- value vector length
 * [OUT] result  -- address into which to store result
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
__osi_trace_anly_logic_evaluate_AND(osi_trace_anly_var_t * var,
				    osi_trace_anly_logic_eval_t * val_vec,
				    size_t vec_len,
				    osi_trace_anly_logic_eval_t * result)
{
    osi_result res = OSI_OK;
    size_t i;
    osi_uint32 answer = 1;

    for (i = 0; i < vec_len ; i++) {
	if (!val_vec[i]) {
	    answer = 0;
	    break;
	}
    }

    *result = (osi_trace_anly_logic_eval_t) answer;

    return res;
}

/*
 * internal procedure to evaluate boolean OR
 *
 * [IN] var      -- pointer to var object
 * [IN] val_vec  -- value vector
 * [IN] vec_len  -- value vector length
 * [OUT] result  -- address into which to store result
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
__osi_trace_anly_logic_evaluate_OR(osi_trace_anly_var_t * var,
				   osi_trace_anly_logic_eval_t * val_vec,
				   size_t vec_len,
				   osi_trace_anly_logic_eval_t * result)
{
    osi_result res = OSI_OK;
    size_t i;
    osi_uint32 answer = 0;

    for (i = 0; i < vec_len ; i++) {
	if (val_vec[i]) {
	    answer = 1;
	    break;
	}
    }

    *result = (osi_trace_anly_logic_eval_t) answer;

    return res;
}

/*
 * internal procedure to evaluate boolean XOR
 *
 * [IN] var      -- pointer to var object
 * [IN] val_vec  -- value vector
 * [IN] vec_len  -- value vector length
 * [OUT] result  -- address into which to store result
 *
 * returns:
 *   OSI_OK always
 */
osi_static osi_result
__osi_trace_anly_logic_evaluate_XOR(osi_trace_anly_var_t * var,
				    osi_trace_anly_logic_eval_t * val_vec,
				    size_t vec_len,
				    osi_trace_anly_logic_eval_t * result)
{
    osi_result res = OSI_OK;
    size_t i;
    osi_uint32 answer = 0;

    for (i = 0; i < vec_len ; i++) {
	if (val_vec[i]) {
	    answer++;
	}
    }

    *result = (osi_trace_anly_logic_eval_t) (answer == 1);

    return res;
}

/*
 * internal procedure to evaluate boolean operations
 *
 * [IN] var      -- pointer to var object
 * [IN] val_vec  -- value vector
 * [IN] vec_len  -- value vector length
 * [OUT] result  -- address into which to store result
 *
 * returns:
 *   OSI_OK on success
 *   OSI_TRACE_ANLY_ERROR_INVALID_LOGIC_OP
 */
osi_static osi_result
__osi_trace_anly_logic_evaluate(osi_trace_anly_var_t * var,
				osi_trace_anly_logic_eval_t * val_vec,
				size_t vec_len,
				osi_trace_anly_logic_eval_t * result)
{
    osi_result res;
    size_t i;
    osi_trace_anly_logic_eval_t answer;

    switch (var->var_data.logic->logic_op) {
    case OSI_TRACE_ANLY_LOGIC_OP_AND:
	res = __osi_trace_anly_logic_evaluate_AND(var,
						  val_vec,
						  vec_len,
						  result);
	break;

    case OSI_TRACE_ANLY_LOGIC_OP_OR:
	res = __osi_trace_anly_logic_evaluate_OR(var,
						 val_vec,
						 vec_len,
						 result);
	break;

    case OSI_TRACE_ANLY_LOGIC_OP_XOR:
	res = __osi_trace_anly_logic_evaluate_XOR(var,
						  val_vec,
						  vec_len,
						  result);
	break;

    case OSI_TRACE_ANLY_LOGIC_OP_NAND:
	res = __osi_trace_anly_logic_evaluate_AND(var,
						  val_vec,
						  vec_len,
						  &answer);
	*result = !answer;
	break;

    case OSI_TRACE_ANLY_LOGIC_OP_NOR:
	res = __osi_trace_anly_logic_evaluate_OR(var,
						 val_vec,
						 vec_len,
						 &answer);
	*result = !answer;
	break;

    case OSI_TRACE_ANLY_LOGIC_OP_XNOR:
	res = __osi_trace_anly_logic_evaluate_XOR(var,
						  val_vec,
						  vec_len,
						  &answer);
	*result = !answer;
	break;

    default:
	res = OSI_TRACE_ANLY_ERROR_INVALID_LOGIC_OP;
    }

    return res;
}


/*
 * update a logic object
 *
 * [IN] var        -- logic object
 * [IN] update     -- information for fan-in element which triggered this event
 * [IN] input_vec  -- fan in data
 * [IN] sdata      -- opaque data for logic type
 */
osi_static osi_result 
osi_trace_anly_logic_update(osi_trace_anly_var_t * var,
			    osi_trace_anly_var_update_t * update,
			    osi_trace_anly_var_input_vec_t * input_vec,
			    void * sdata,
			    osi_uint64 * result)
{
    osi_result res = OSI_OK;
    osi_trace_anly_logic_eval_t answer;

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
    res = __osi_trace_anly_logic_evaluate(var,
					  input_vec->vec,
					  input_vec->vec_len,
					  result);
#else /* !OSI_ENV_NATIVE_INT64_TYPE */
    osi_trace_anly_logic_eval_t * iv32;

    iv32 = (osi_trace_anly_logic_eval_t *)
	osi_mem_alloc(input_vec->vec_len * sizeof(osi_trace_anly_logic_eval_t));
    if (osi_compiler_expect_false(iv32 == osi_NULL)) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    __osi_trace_anly_logic_eval_convert(input_vec,
					iv32);

    res = __osi_trace_anly_logic_evaluate(var,
					  iv32,
					  input_vec->vec_len,
					  &answer);

    FillInt64(*result, 0, answer);

 cleanup:
    osi_mem_free(iv32, input_vec->vec_len);
#endif /* !OSI_ENV_NATIVE_INT64_TYPE */

 error:
    return res;
}

osi_result
osi_trace_anly_logic_PkgInit(void)
{
    osi_result res = OSI_OK;

    osi_trace_anly_logic_cache =
	osi_mem_object_cache_create("osi_trace_anly_logic",
				    sizeof(osi_trace_anly_logic_data_t),
				    0,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_NULL,
				    osi_trace_impl_mem_object_cache_opts());
    if (osi_trace_anly_logic_cache == osi_NULL) {
	res = OSI_ERROR_NOMEM;
	goto error;
    }

    res = __osi_trace_anly_var_type_register(OSI_TRACE_ANLY_VAR_LOGIC,
					     osi_trace_anly_logic_cache,
					     &osi_trace_anly_logic_update,
					     osi_NULL);

 error:
    return res;
}

osi_result
osi_trace_anly_logic_PkgShutdown(void)
{
    osi_result res = OSI_OK;

    res = __osi_trace_anly_var_type_unregister(OSI_TRACE_ANLY_VAR_LOGIC);

    osi_mem_object_cache_destroy(osi_trace_anly_logic_cache);
    osi_trace_anly_logic_cache = osi_NULL;

    return res;
}
