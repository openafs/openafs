/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>

#if defined(OSI_KERNELSPACE_ENV)
#include <osi/osi_trace.h>
#include <osi/osi_kernel.h>
#include <trace/gen_rgy.h>
#include <trace/event/event.h>
#include <trace/generator/generator.h>

osi_result
osi_TraceFunc_UserTraceTrap(osi_TracePoint_record * rec)
{
    osi_result res = OSI_OK;
    int code;
    osi_Trace_EventHandle handle;
    osi_TraceFunc_Prologue();
    osi_TraceBuffer_Allocate(&handle);

    osi_kernel_handle_copy_in(&rec->version,
			      &(handle.data->version),
			      (sizeof(osi_TracePoint_record) -
			       offsetof(osi_TracePoint_record, version)),
			      code,
			      error);

    osi_TraceBuffer_TrapStamp(&handle);
    osi_TraceBuffer_Commit(&handle);

 done:
    osi_TraceFunc_Epilogue();
    return res;

 error:
    osi_TraceBuffer_Rollback(&handle);
    res = OSI_FAIL;
    goto done;
}

#endif /* OSI_KERNELSPACE_ENV */
