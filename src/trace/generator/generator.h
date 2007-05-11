/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
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
#include <osi/osi_cpu.h>
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

#if defined(OSI_ENV_KERNELSPACE)

#if defined(OSI_IMPLEMENTS_TIME_TIMESPEC_NON_UNIQUE_GET)
#define __osi_TracePoint_record_TimeStamp(record) \
    osi_timespec_get32(&((record)->timestamp))
#else /* !OSI_IMPLEMENTS_TIME_TIMESPEC */
#define __osi_TracePoint_record_TimeStamp(record) \
    osi_Macro_Begin \
        osi_timeval32_t _now; \
        osi_timeval_get32(&_now); \
        (record)->timestamp.tv_sec = _now.tv_sec; \
        (record)->timestamp.tv_nsec = _now.tv_usec * 1000; \
    osi_Macro_End
#endif /* !OSI_IMPLEMENTS_TIME_TIMESPEC */

#define osi_TraceBuffer_Stamp(event) \
    __osi_TracePoint_record_Stamp(osi_Trace_EventHandle_Record(event))
#define __osi_TracePoint_record_Stamp(record) \
    osi_Macro_Begin \
        __osi_TracePoint_record_TimeStamp(record); \
        (record)->pid = osi_proc_current_id(); \
        (record)->tid = osi_thread_current_id(); \
        (record)->cpu_id = osi_cpu_current_id(); \
        (record)->version = OSI_TRACEPOINT_RECORD_VERSION; \
        (record)->gen_id = OSI_TRACE_GEN_RGY_KERNEL_ID; \
    osi_Macro_End


/*
 * stamp trace records coming in from userspace processes via the trace trap.
 * handle the common case where userspace and kernelspace are using the same
 * record version inline; otherwise call out to the versioned stamp function.
 */
osi_extern void osi_TracePoint_record_VX_TrapStamp(osi_Trace_EventHandle *);
#define osi_TraceBuffer_TrapStamp(event) \
    osi_Macro_Begin \
        if (osi_Trace_EventHandle_RecordPtr(event)->ptr.preamble->version == OSI_TRACEPOINT_RECORD_VERSION) { \
            __osi_TracePoint_record_TrapStamp(osi_Trace_EventHandle_Record(event)); \
        } else { \
            osi_TracePoint_record_VX_TrapStamp(event); \
        } \
    osi_Macro_End

#define __osi_TracePoint_record_TrapStamp(record) \
    osi_Macro_Begin \
        __osi_TracePoint_record_TimeStamp(record); \
        (record)->pid = osi_proc_current_id(); \
        (record)->tid = osi_thread_current_id(); \
        (record)->cpu_id = osi_cpu_current_id(); \
    osi_Macro_End

#else /* !OSI_ENV_KERNELSPACE */

#define osi_TraceBuffer_Stamp(event) \
    __osi_TracePoint_record_Stamp(osi_Trace_EventHandle_Record(event))
#define __osi_TracePoint_record_Stamp(record) \
    osi_Macro_Begin \
        osi_trace_gen_id_t _gen_id; \
        osi_trace_gen_id(&_gen_id); \
        (record)->version = OSI_TRACEPOINT_RECORD_VERSION; \
        (record)->gen_id = _gen_id; \
    osi_Macro_End

#endif /* !OSI_ENV_KERNELSPACE */


#endif /* _OSI_TRACE_GENERATOR_GENERATOR_H */
