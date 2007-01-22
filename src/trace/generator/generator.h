/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GENERATOR_GENERATOR_H
#define _OSI_TRACE_GENERATOR_GENERATOR_H 1


/*
 * osi tracing framework
 * trace event handle allocation, commit, and rollback interface
 *
 * types:
 *
 *  osi_Trace_EventHandle
 *    -- implementation-defined typedef structure.
 *       MUST contain the following data element:
 *           osi_TracePoint_record * data;
 *
 * interfaces:
 *
 *   osi_result osi_trace_buffer_PkgInit(void);
 *     -- initialize the trace ring buffer
 *
 *   osi_result osi_trace_buffer_PkgShutdown(void);
 *     -- shutdown the trace ring buffer
 *
 *   void osi_TraceBuffer_Allocate(osi_Trace_EventHandle * event);
 *     -- allocate a data element and set event->data
 *
 *   void osi_TraceBuffer_Commit(osi_Trace_EventHandle * event);
 *     -- commit trace event
 *
 *   void osi_TraceBuffer_Rollback(osi_Trace_EventHandle * event);
 *     -- rollback trace event
 *
 */


#include <osi/osi_proc.h>
#include <osi/osi_thread.h>
#include <trace/gen_rgy.h>
#include <trace/generator/activation.h>

#if OSI_TRACE_COMMIT_METHOD_IS(OSI_TRACE_COMMIT_METHOD_BUFFER)
#include <trace/generator/buffer.h>
#elif OSI_TRACE_COMMIT_METHOD_IS(OSI_TRACE_COMMIT_METHOD_SYSCALL)
#include <trace/generator/trap.h>
#else
#error "this trace commit method is not yet supported"
#endif


/*
 * trace event stamp
 * (fills in fields such as timestamp, pid, etc.)
 *
 * TrapStamp is specifically for OSI_TRACE_SYSCALL_OP_INSERT
 */

#if defined(OSI_KERNELSPACE_ENV)

#define osi_TraceBuffer_Stamp(event) \
    osi_Macro_Begin \
        (event)->data->timestamp = 0; \
        (event)->data->pid = osi_proc_current_id(); \
        (event)->data->tid = osi_thread_current_id(); \
        (event)->data->version = OSI_TRACEPOINT_RECORD_VERSION; \
        (event)->data->gen_id = OSI_TRACE_GEN_RGY_KERNEL_ID; \
    osi_Macro_End

#define osi_TraceBuffer_TrapStamp(event) \
    osi_Macro_Begin \
        (event)->data->timestamp = 0; \
        (event)->data->pid = osi_proc_current_id(); \
        (event)->data->tid = osi_thread_current_id(); \
    osi_Macro_End

#else /* !OSI_KERNELSPACE_ENV */

#define osi_TraceBuffer_Stamp(event) \
    osi_Macro_Begin \
        osi_trace_gen_id_t _gen_id; \
        (event)->data->version = OSI_TRACEPOINT_RECORD_VERSION; \
        osi_trace_gen_id(&_gen_id); \
        (event)->data->gen_id = _gen_id; \
    osi_Macro_End

#endif /* !OSI_KERNELSPACE_ENV */


#endif /* _OSI_TRACE_GENERATOR_GENERATOR_H */
