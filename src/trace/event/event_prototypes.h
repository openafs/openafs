/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_EVENT_EVENT_PROTOTYPES_H
#define _OSI_TRACE_EVENT_EVENT_PROTOTYPES_H 1


/*
 * tracepoint event functions
 */

#include <osi/osi_var_args.h>


/*
 * the following functions are generally called
 * by the osi_TracePoint() macro
 *
 * these functions are invoked when an activated tracepoint
 * is encountered.  They are responsible for allocating an
 * osi_TracePoint_record structure from the ring buffer
 * and populating it.
 */


/*
 * abstract event trace functions
 */
osi_extern osi_result osi_TraceFunc_VarEvent(osi_trace_probe_id_t id,
					     osi_Trace_EventType type,
					     int nf,
					     osi_va_list args);

#if defined(OSI_ENV_VARIADIC_MACROS)
osi_extern osi_result osi_TraceFunc_Event(osi_trace_probe_id_t id, osi_Trace_EventType type, int nf, ...);
#else
#define osi_TraceFunc_Event osi_TraceFunc_VarEvent
#endif


/*
 * function prologue and epilogue trace functions
 */
#if defined(OSI_ENV_VARIADIC_MACROS)
osi_extern osi_result osi_TraceFunc_FunctionPrologue(osi_trace_probe_id_t, int, ...);
osi_extern osi_result osi_TraceFunc_FunctionEpilogue(osi_trace_probe_id_t, int, osi_register_int);
#else
osi_extern osi_result osi_TraceFunc_FunctionPrologue(osi_trace_probe_id_t, int, osi_va_list);
osi_extern osi_result osi_TraceFunc_FunctionEpilogue(osi_trace_probe_id_t, int, osi_va_list);
#endif


/*
 * function to deal with inserts into kernel trace buffer from
 * userspace apps
 */
#if defined(OSI_ENV_KERNELSPACE)
osi_extern osi_result osi_TraceFunc_UserTraceTrap(void * record_buf,
						  osi_uint32 record_version,
						  osi_size_t record_size);
#endif


/*
 * icl framework trace interfaces
 */
osi_extern osi_result osi_TraceFunc_iclEvent(osi_trace_probe_id_t id, 
					     osi_Trace_EventType type, 
					     int nf, ...);
osi_extern osi_result osi_TraceFunc_iclVarEvent(osi_trace_probe_id_t id,
						osi_Trace_EventType type,
						int nf,
						osi_va_list args);


#endif /* _OSI_TRACE_EVENT_EVENT_PROTOTYPES_H */

