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
#include <trace/gen_rgy.h>
#include <trace/event/event.h>
#include <trace/generator/generator.h>

#if defined(OSI_ENV_VARIADIC_MACROS)

osi_result
osi_TraceFunc_FunctionPrologue(osi_trace_probe_id_t id, int nf, ...)
{
    osi_result res;
    va_list args;

    va_start(args, nf);
    res = osi_TraceFunc_VarEvent(id, osi_Trace_Event_FunctionPrologue_Id, nf, args);
    va_end(args);

    return res;
}

osi_result
osi_TraceFunc_FunctionEpilogue(osi_trace_probe_id_t probe_id, 
			       int ret_present, 
			       osi_register_int ret_value)
{
    osi_Trace_EventHandle handle;
    osi_TraceFunc_Prologue();
    osi_TraceBuffer_Allocate(&handle);

    handle.data->probe = probe_id;
    handle.data->tags[0] = (osi_uint8) osi_Trace_Event_FunctionEpilogue_Id;
    handle.data->tags[1] = (osi_uint8) osi_Trace_Event_Null_Id;

    if (ret_present) {
	osi_TracePoint_record_arg_add(handle.data, ret_value);
    }

    osi_TraceBuffer_Stamp(&handle);
    osi_TraceBuffer_Commit(&handle);
    osi_TraceFunc_Epilogue();
    return OSI_OK;
}

#else /* !OSI_ENV_VARIADIC_MACROS */

osi_result
osi_TraceFunc_FunctionPrologue(osi_trace_probe_id_t id, int nf, va_list args)
{
    return osi_TraceFunc_VarEvent(id, osi_Trace_Event_FunctionPrologue_Id, nf, args);
}

osi_result
osi_TraceFunc_FunctionEpilogue(osi_trace_probe_id_t id, int ret_present, va_list args)
{
    osi_Trace_EventHandle handle;
    osi_TraceFunc_Prologue();
    osi_TraceBuffer_Allocate(&handle);

    handle.data->probe = id;
    handle.data->tags[0] = (osi_uint8) osi_Trace_Event_FunctionEpilogue_Id;

    if (ret_present) {
	osi_TracePoint_record_arg_add(handle.data, va_arg(args, osi_register_int));
    }

    osi_TraceBuffer_Stamp(&handle);
    osi_TraceBuffer_Commit(&handle);
    osi_TraceFunc_Epilogue();
    return OSI_OK;
}

#endif /* !OSI_ENV_VARIADIC_MACROS */
