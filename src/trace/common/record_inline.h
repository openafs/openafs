/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_RECORD_INLINE_H
#define _OSI_TRACE_COMMON_RECORD_INLINE_H 1

/*
 * osi trace
 * common definitions
 * tracepoint record interface
 * inline getters/setters
 */

#include <trace/common/record_impl.h>

/*
 * due to the ILP32 limitations, direct field access
 * is guarded by the following interfaces:
 *
 *  osi_result osi_TracePoint_record_arg_set(osi_TracePoint_record_ptr_t *,
 *                                           osi_uint32 arg,
 *                                           osi_register_int value);
 *    -- set value of arg N
 *
 *  osi_result osi_TracePoint_record_arg_add(osi_TracePoint_record_ptr_t *,
 *                                           osi_register_int value);
 *    -- append new arg
 *
 *  osi_result osi_TracePoint_record_arg_get(osi_TracePoint_record_ptr_t *,
 *                                           osi_uint32 arg,
 *                                           osi_register_int * value);
 *    -- get value of arg N
 *
 *  osi_result osi_TracePoint_record_arg_set64(osi_TracePoint_record_ptr_t *,
 *                                             osi_uint32 arg,
 *                                             osi_int64 value);
 *  osi_result osi_TracePoint_record_arg_add64(osi_TracePoint_record_ptr_t *,
 *                                             osi_int64 value);
 *  osi_result osi_TracePoint_record_arg_get64(osi_TracePoint_record_ptr_t *,
 *                                             osi_uint32 arg,
 *                                             osi_int64 * value);
 */


/*
 * INTERNAL
 * inline methods for getting and setting payload members in
 * records of the current version
 */

osi_inline_define(
osi_result
_osi_TracePoint_record_arg_set64(osi_TracePoint_record * record,
				 osi_uint32 arg_in,
				 osi_int64 value)
{
    osi_result res = OSI_OK;
    osi_uint32 arg;

    if (osi_compiler_expect_false(arg_in >= OSI_TRACEPOINT_MAX_ARGS)) {
	res = OSI_FAIL;
	goto error;
    }

    arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
    __osi_TracePoint_record_arg_set64(record, arg, value);

 error:
    return OSI_OK;
}
)
osi_inline_prototype(
osi_result
_osi_TracePoint_record_arg_set64(osi_TracePoint_record * record,
				 osi_uint32 arg_in,
				 osi_int64 value)
)

osi_inline_define(
osi_result
_osi_TracePoint_record_arg_get64(osi_TracePoint_record * record,
				 osi_uint32 arg_in,
				 osi_int64 * value)
{
    osi_result res = OSI_OK;
    osi_uint32 arg;

    if (osi_compiler_expect_false(arg_in >= OSI_TRACEPOINT_MAX_ARGS)) {
	res = OSI_FAIL;
	goto error;
    }

    arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
    __osi_TracePoint_record_arg_get64(record, arg, value);

 error:
    return res;
}
)
osi_inline_prototype(
osi_result
_osi_TracePoint_record_arg_get64(osi_TracePoint_record * record,
				 osi_uint32 arg_in,
				 osi_int64 * value)
)

osi_inline_define(
osi_result
_osi_TracePoint_record_arg_add64(osi_TracePoint_record * record,
				 osi_int64 value)
{
    osi_uint32 arg = record->nargs++;
    return _osi_TracePoint_record_arg_set64(record,
					   arg,
					   value);
}
)
osi_inline_prototype(
osi_result
_osi_TracePoint_record_arg_add64(osi_TracePoint_record * record,
				 osi_int64 value)
)


/*
 * PUBLIC
 * methods for getting and setting payload members
 */

osi_inline_define(
osi_result
osi_TracePoint_record_arg_set64(osi_TracePoint_record_ptr_t * record,
				osi_uint32 arg_in,
				osi_int64 value)
{
    osi_result res = OSI_OK;
    osi_uint32 arg;

    if (osi_compiler_expect_false(arg_in >= OSI_TRACEPOINT_MAX_ARGS)) {
	res = OSI_FAIL;
	goto error;
    }

    if (record->version == OSI_TRACEPOINT_RECORD_VERSION) {
        arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
        __osi_TracePoint_record_arg_set64(record->ptr.cur, arg, value);
    } else {
        res = osi_TracePoint_record_VX_arg_set64(record, arg_in, value);
    }

 error:
    return res;
}
)
osi_inline_prototype(
osi_result
osi_TracePoint_record_arg_set64(osi_TracePoint_record_ptr_t * record,
				osi_uint32 arg_in,
				osi_int64 value)
)

osi_inline_define(
osi_result
osi_TracePoint_record_arg_get64(osi_TracePoint_record_ptr_t * record,
				osi_uint32 arg_in,
				osi_int64 * value)
{
    osi_result res = OSI_OK;
    osi_uint32 arg;

    if (osi_compiler_expect_false(arg_in >= OSI_TRACEPOINT_MAX_ARGS)) {
	res = OSI_FAIL;
	goto error;
    }

    if (record->version == OSI_TRACEPOINT_RECORD_VERSION) {
        arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
        __osi_TracePoint_record_arg_get64(record->ptr.cur, arg, value);
    } else {
        res = osi_TracePoint_record_VX_arg_get64(record, arg_in, value);
    }

 error:
    return res;
}
)
osi_inline_prototype(
osi_result
osi_TracePoint_record_arg_get64(osi_TracePoint_record_ptr_t * record,
				osi_uint32 arg_in,
				osi_int64 * value)
)

osi_inline_define(
osi_result
osi_TracePoint_record_arg_add64(osi_TracePoint_record_ptr_t * record,
				osi_int64 value)
{
    osi_result res;
    osi_uint32 arg;

    if (record->version == OSI_TRACEPOINT_RECORD_VERSION) {
        arg = record->ptr.cur->nargs++;
        res = _osi_TracePoint_record_arg_set64(record->ptr.cur,
					       arg,
					       value);
    } else {
        res = osi_TracePoint_record_VX_arg_add64(record, value);
    }

    return res;
}
)
osi_inline_prototype(
osi_result
osi_TracePoint_record_arg_add64(osi_TracePoint_record_ptr_t * record,
				osi_int64 value)
)


#if (OSI_TYPE_REGISTER_BITS == 64)

#define osi_TracePoint_record_arg_set(rec, arg, val) osi_TracePoint_record_arg_set64(rec, arg, val)
#define osi_TracePoint_record_arg_get(rec, arg, val) osi_TracePoint_record_arg_get64(rec, arg, val)
#define osi_TracePoint_record_arg_add(rec, val) osi_TracePoint_record_arg_add64(rec, val)

#elif (OSI_TYPE_REGISTER_BITS == 32)

osi_inline_define(
osi_result
osi_TracePoint_record_arg_set(osi_TracePoint_record_ptr_t * record,
			      osi_uint32 arg_in,
			      osi_register_int value)
{
    osi_int64 val_out;

    FillInt64(val_out, 0, value);
    return osi_TracePoint_record_arg_set64(record, arg_in, val_out);
}
)
osi_inline_prototype(
osi_result
osi_TracePoint_record_arg_set(osi_TracePoint_record_ptr_t * record,
			      osi_uint32 arg_in,
			      osi_register_int value)
)

osi_inline_define(
osi_result
osi_TracePoint_record_arg_get(osi_TracePoint_record_ptr_t * record,
			      osi_uint32 arg,
			      osi_register_int * value)
{
    osi_result res;
    osi_int64 val_in;
    osi_int32 hi;
    osi_int32 lo;

    res = osi_TracePoint_record_arg_get64(record, arg, &val_in);
    SplitInt64(val_in, hi, lo);
    *value = lo;

    return res;
}
)
osi_inline_prototype(
osi_result
osi_TracePoint_record_arg_get(osi_TracePoint_record_ptr_t * record,
			      osi_uint32 arg_in,
			      osi_register_int * value)
)

osi_inline_define(
osi_result
osi_TracePoint_record_arg_add(osi_TracePoint_record_ptr_t * record,
			      osi_register_int value)
{
    osi_int64 val_out;

    FillInt64(val_out, 0, value);
    return osi_TracePoint_record_arg_add64(record,
					   val_out);
}
)
osi_inline_prototype(
osi_result
osi_TracePoint_record_arg_add(osi_TracePoint_record_ptr_t * record,
			      osi_register_int value)
)

#else /* OSI_TYPE_REGISTER_BITS != 32 && OSI_TYPE_REGISTER_BITS != 64 */
#error "this value of sizeof(osi_register_int) is not supported"
#endif

#endif /* _OSI_TRACE_COMMON_RECORD_INLINE_H */
