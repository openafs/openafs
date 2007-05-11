/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_TRAP_H
#define _OSI_TRACE_GENERATOR_TRAP_H 1


/*
 * osi tracing framework
 * trace trap implementation
 */

#include <trace/syscall.h>

#define osi_trace_buffer_PkgInit       osi_null_init_func
#define osi_trace_buffer_PkgShutdown   osi_null_fini_func


/* handle to a trace event that's being recorded */
typedef struct osi_Trace_EventHandle {
    osi_TracePoint_record _ldata;
    osi_TracePoint_record_ptr_t data;
} osi_Trace_EventHandle;

#define osi_Trace_EventHandle_Record(event) \
    ((event)->data.ptr.cur)
#define osi_Trace_EventHandle_RecordPtr(event) \
    (&((event)->data))
#define osi_Trace_EventHandle_Field_Set(event, field, val) \
    (event)->data.ptr.cur->field = val
#define osi_Trace_EventHandle_Field_Get(event, field, val) \
    val = (event)->data.ptr.cur->field


/*
 * allocate a tracepoint record
 *
 * void osi_TraceBuffer_Allocate(osi_Trace_EventHandle *)
 */

#define osi_TraceBuffer_Allocate(event) \
    osi_Macro_Begin \
        (event)->data.ptr.cur = &(event)->_ldata; \
        (event)->_ldata.nargs = 0; \
        (event)->data.version = (event)->_ldata.version = \
            OSI_TRACEPOINT_RECORD_VERSION; \
    osi_Macro_End


/*
 * trace event commit
 */

#define osi_TraceBuffer_Commit(event) \
    osi_Macro_Begin \
        int _rv; \
        osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_INSERT, \
                          (long)((event)->data.ptr.opaque), \
                          (long)OSI_TRACEPOINT_RECORD_VERSION, \
                          (long)sizeof(osi_TracePoint_record), \
                          &_rv); \
    osi_Macro_End


/*
 * trace event rollback
 */

#define osi_TraceBuffer_Rollback(event)


#endif /* _OSI_TRACE_GENERATOR_TRAP_H */
