/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_TRACEPOINT_H
#define _OSI_TRACE_TRACEPOINT_H 1


/*
 * the following macros are the actual tracepoints
 * which get placed into code.  For most platforms
 * they are variadic macros.  For older compilers
 * we provide static inline variadic functions.
 */

#if defined(OSI_ENV_VARIADIC_MACROS)

#if !defined(OSI_TRACEPOINT_DISABLE)

#define osi_Trace_OSI_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_osi_Id, __VA_ARGS__)
#define osi_Trace_OSI_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_osi_Id, __VA_ARGS__)
#define osi_Trace_Rx_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_RxEvent_Id, __VA_ARGS__)
#define osi_Trace_Rx_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_RxEvent_Id, __VA_ARGS__)
#define osi_Trace_Ubik_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_UbikEvent_Id, __VA_ARGS__)
#define osi_Trace_Ubik_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_UbikEvent_Id, __VA_ARGS__)
#define osi_Trace_Vol_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_VolPkgEvent_Id, __VA_ARGS__)
#define osi_Trace_Vol_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_VolPkgEvent_Id, __VA_ARGS__)
#define osi_Trace_FS_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_FSEvent_Id, __VA_ARGS__)
#define osi_Trace_FS_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_FSEvent_Id, __VA_ARGS__)
#define osi_Trace_VS_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_VolserEvent_Id, __VA_ARGS__)
#define osi_Trace_VS_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_VolserEvent_Id, __VA_ARGS__)
#define osi_Trace_CM_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_CMEvent_Id, __VA_ARGS__)
#define osi_Trace_CM_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_CMEvent_Id, __VA_ARGS__)
#define osi_Trace_Bozo_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, osi_Trace_Event_BozoEvent_Id, __VA_ARGS__)
#define osi_Trace_Bozo_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, osi_Trace_Event_BozoEvent_Id, __VA_ARGS__)
#define osi_Trace_icl_Event(f, ...) \
    osi_TracePoint(f, osi_TraceFunc_iclEvent, osi_Trace_Event_icl_Id, __VA_ARGS__)
#define osi_Trace_icl_Event_Fire(f, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_iclEvent, osi_Trace_Event_icl_Id, __VA_ARGS__)
#define osi_Trace_Stats_Event(f, ev, ...) \
    osi_TracePoint(f, osi_TraceFunc_Event, ev, __VA_ARGS__)
#define osi_Trace_Stats_Event_Fire(f, ev, ...) \
    osi_TracePoint_Fire(f, osi_TraceFunc_Event, ev, __VA_ARGS__)

#else  /* OSI_TRACEPOINT_DISABLE */

#define osi_Trace_OSI_Event(f, ...)
#define osi_Trace_OSI_Event_Fire(f, ...)
#define osi_Trace_Rx_Event(f, ...)
#define osi_Trace_Rx_Event_Fire(f, ...)
#define osi_Trace_Ubik_Event(f, ...)
#define osi_Trace_Ubik_Event_Fire(f, ...)
#define osi_Trace_Vol_Event(f, ...)
#define osi_Trace_Vol_Event_Fire(f, ...)
#define osi_Trace_FS_Event(f, ...)
#define osi_Trace_FS_Event_Fire(f, ...)
#define osi_Trace_VS_Event(f, ...)
#define osi_Trace_VS_Event_Fire(f, ...)
#define osi_Trace_CM_Event(f, ...)
#define osi_Trace_CM_Event_Fire(f, ...)
#define osi_Trace_Bozo_Event(f, ...)
#define osi_Trace_Bozo_Event_Fire(f, ...)
#define osi_Trace_icl_Event(f, ...)
#define osi_Trace_icl_Event_Fire(f, ...)
#define osi_Trace_Stats_Event(f, ev, ...)
#define osi_Trace_Stats_Event_Fire(f, ev, ...)

#endif /* OSI_TRACEPOINT_DISABLE */

#else /* !OSI_ENV_VARIADIC_MACROS */

#if (OSI_ENV_INLINE_INCLUDE)
#include <trace/tracepoint_inline.h>
#endif

#endif /* !OSI_ENV_VARIADIC_MACROS */


#endif /* _OSI_TRACE_TRACEPOINT_H */
