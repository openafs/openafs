/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <trace/common/trace_impl.h>

#if defined(OSI_ENV_KERNELSPACE)
#include <osi/osi_kernel.h>
#include <trace/gen_rgy.h>
#include <trace/event/event.h>
#include <trace/generator/generator.h>

osi_result
osi_TraceFunc_UserTraceTrap(void * rec_buf,
			    osi_uint32 rec_version,
			    osi_size_t rec_len)
{
    osi_result res = OSI_OK;
    int code;
    osi_Trace_EventHandle handle;
    osi_TraceFunc_Prologue();
    osi_TraceBuffer_Allocate(&handle);
    osi_TracePoint_record_ptr_t rec;
    osi_size_t copy_len;
    osi_uintptr_t user_buf, kernel_buf;

    /*
     * zero values for version and len fields imply
     * we're dealing with a userspace process which
     * emits v2 records
     */
    rec.version = (osi_TracePoint_record_version_t) (rec_version) ? 
	rec_version : OSI_TRACEPOINT_RECORD_VERSION_V2;
    rec_len = (rec_len) ?
	rec_len : sizeof(osi_TracePoint_record_v2);


    /* adjust for preamble fields we don't bother copying in */
    rec_len -= OSI_TRACEPOINT_RECORD_COPYIN_OFFSET;
    user_buf = (osi_uintptr_t) rec_buf;
    user_buf += OSI_TRACEPOINT_RECORD_COPYIN_OFFSET;
    kernel_buf = (osi_uintptr_t) osi_Trace_EventHandle_Record(&handle);
    kernel_buf += OSI_TRACEPOINT_RECORD_COPYIN_OFFSET;

    osi_kernel_handle_copy_in(user_buf,
			      kernel_buf,
			      rec_len,
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

void
osi_TracePoint_record_VX_TrapStamp(osi_Trace_EventHandle * handle)
{
    osi_TracePoint_record_ptr_t * rec;

    rec = osi_Trace_EventHandle_RecordPtr(handle);

    switch (rec->ptr.preamble->version) {
    case 0:
	/* if version field is zero, then record was emitted 
	 * by a buggy v2 implementation... */
        (void)osi_timeval_get32(&(rec->ptr.v2->timestamp));
        rec->ptr.v2->pid = osi_proc_current_id();
        rec->ptr.v2->tid = osi_thread_current_id();
	rec->ptr.v2->version = OSI_TRACEPOINT_RECORD_VERSION_V2;
	break;
    case OSI_TRACEPOINT_RECORD_VERSION_V2:
        (void)osi_timeval_get32(&(rec->ptr.v2->timestamp));
        rec->ptr.v2->pid = osi_proc_current_id();
        rec->ptr.v2->tid = osi_thread_current_id();
	break;
    case OSI_TRACEPOINT_RECORD_VERSION_V3:
	__osi_TracePoint_record_TrapStamp(rec->ptr.v3);
	break;
    }
}


#endif /* OSI_ENV_KERNELSPACE */
