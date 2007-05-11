/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>
#include <trace/common/record.h>
#include <trace/common/record_impl.h>



osi_result
osi_TracePoint_record_VX_arg_set64(osi_TracePoint_record_ptr_t * record,
				   osi_uint32 arg_in,
				   osi_int64 value)
{
    osi_result res = OSI_OK;
    osi_uint32 arg;

    switch(record->version) {
    case OSI_TRACEPOINT_RECORD_VERSION_V2:
        arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
        __osi_TracePoint_record_arg_set64(record->ptr.v2, arg, value);
	break;
    case OSI_TRACEPOINT_RECORD_VERSION_V3:
        arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
        __osi_TracePoint_record_arg_set64(record->ptr.v3, arg, value);
	break;
    default:
	res = OSI_UNIMPLEMENTED;
    }

    return res;
}

osi_result
osi_TracePoint_record_VX_arg_get64(osi_TracePoint_record_ptr_t * record,
				   osi_uint32 arg_in,
				   osi_int64 * value)
{
    osi_result res = OSI_OK;
    osi_uint32 arg;

    switch(record->version) {
    case OSI_TRACEPOINT_RECORD_VERSION_V2:
        arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
        __osi_TracePoint_record_arg_get64(record->ptr.v2, arg, value);
	break;
    case OSI_TRACEPOINT_RECORD_VERSION_V3:
        arg = OSI_TRACEPOINT_RECORD_ARG(arg_in);
        __osi_TracePoint_record_arg_get64(record->ptr.v3, arg, value);
	break;
    default:
	res = OSI_UNIMPLEMENTED;
    }

    return res;
}

osi_result
osi_TracePoint_record_VX_arg_add64(osi_TracePoint_record_ptr_t * record,
				   osi_int64 value)
{
    osi_result res;
    osi_uint32 arg;

    switch(record->version) {
    case OSI_TRACEPOINT_RECORD_VERSION_V2:
	arg = record->ptr.v2->nargs++;
	break;
    case OSI_TRACEPOINT_RECORD_VERSION_V3:
	arg = record->ptr.v3->nargs++;
	break;
    default:
	res = OSI_UNIMPLEMENTED;
	goto error;
    }

    if (osi_compiler_expect_false(arg >= OSI_TRACEPOINT_MAX_ARGS)) {
	res = OSI_FAIL;
	goto error;
    }

    res = osi_TracePoint_record_VX_arg_set64(record,
					     arg,
					     value);

 error:
    return res;
}
