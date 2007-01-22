/*
 * Copyright 2006, Sine Nomine Associates and others.
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

#define osi_trace_buffer_PkgInit()      (OSI_OK)
#define osi_trace_buffer_PkgShutdown()  (OSI_OK)


/* handle to a trace event that's being recorded */
typedef struct osi_Trace_EventHandle {
    osi_TracePoint_record _ldata;
    osi_TracePoint_record * data;
} osi_Trace_EventHandle;



/*
 * allocate a tracepoint record
 *
 * void osi_TraceBuffer_Allocate(osi_Trace_EventHandle *)
 */

#define osi_TraceBuffer_Allocate(event) \
    osi_Macro_Begin \
        (event)->data = &(event)->_ldata; \
        (event)->data->nargs = 0; \
    osi_Macro_End


/*
 * trace event commit
 */

#define osi_TraceBuffer_Commit(event) \
    osi_Macro_Begin \
        int _rv; \
        osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_INSERT, (long)((event)->data), 0, 0, &_rv); \
    osi_Macro_End


/*
 * trace event rollback
 */

#define osi_TraceBuffer_Rollback(event)


#endif /* _OSI_TRACE_GENERATOR_TRAP_H */
