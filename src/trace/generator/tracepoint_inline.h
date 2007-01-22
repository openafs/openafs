/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_TRACEPOINT_INLINE_H
#define _OSI_TRACE_TRACEPOINT_INLINE_H 1


/*
 * inline tracepoint support for platforms which don't support variadic macros
 */


#if !defined(OSI_ENV_VARIADIC_MACROS)

#include <stdarg.h>

#if !defined(OSI_TRACEPOINT_DISABLE)

#define _osi_TracePoint_Decl_VariadicEvent(name, type) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, int nf, ...) \
{ \
    va_list args; \
    va_start(args, nf); \
    osi_TracePoint(id, osi_TraceFunc_VarEvent, type, nf, args); \
    va_end(args); \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, int, ...))

#define _osi_TracePoint_Decl_VariadicEvent_Fire(name, type) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, int nf, ...) \
{ \
    va_list args; \
    va_start(args, nf); \
    osi_TracePoint_Fire(id, osi_TraceFunc_VarEvent, type, nf, args); \
    va_end(args); \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, int, ...))

#define _osi_TracePoint_Decl_VariadicEvent_VarType(name) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, osi_Trace_EventType type, int nf, ...) \
{ \
    va_list args; \
    va_start(args, nf); \
    osi_TracePoint(id, osi_TraceFunc_VarEvent, type, nf, args); \
    va_end(args); \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, osi_Trace_EventType, int, ...))

#define _osi_TracePoint_Decl_VariadicEvent_VarType_Fire(name) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, osi_Trace_EventType type, int nf, ...) \
{ \
    va_list args; \
    va_start(args, nf); \
    osi_TracePoint_Fire(id, osi_TraceFunc_VarEvent, type, nf, args); \
    va_end(args); \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, osi_Trace_EventType, int, ...))

#else /* OSI_TRACEPOINT_DISABLE */

#define _osi_TracePoint_Decl_VariadicEvent(name, type) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, int nf, ...) \
{ \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, int, ...))

#define _osi_TracePoint_Decl_VariadicEvent_Fire(name, type) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, int nf, ...) \
{ \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, int, ...))

#define _osi_TracePoint_Decl_VariadicEvent_VarType(name) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, osi_Trace_EventType type, int nf, ...) \
{ \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, osi_Trace_EventType, int, ...))

#define _osi_TracePoint_Decl_VariadicEvent_VarType_Fire(name) \
osi_inline_define( \
void \
name (osi_trace_probe_id_t id, osi_Trace_EventType type, int nf, ...) \
{ \
} \
) \
osi_inline_prototype(void name (osi_trace_probe_id_t, osi_Trace_EventType, int, ...))

#endif /* OSI_TRACEPOINT_DISABLE */

_osi_TracePoint_Decl_VariadicEvent(osi_Trace_OSI_Event, osi_Trace_Event_osi_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_OSI_Event_Fire, osi_Trace_Event_osi_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_Rx_Event, osi_Trace_Event_RxEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_Rx_Event_Fire, osi_Trace_Event_RxEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_Ubik_Event, osi_Trace_Event_UbikEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_Ubik_Event_Fire, osi_Trace_Event_UbikEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_Vol_Event, osi_Trace_Event_VolPkgEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_Vol_Event_Fire, osi_Trace_Event_VolPkgEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_FS_Event, osi_Trace_Event_FSEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_FS_Event_Fire, osi_Trace_Event_FSEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_VS_Event, osi_Trace_Event_VolserEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_VS_Event_Fire, osi_Trace_Event_VolserEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_CM_Event, osi_Trace_Event_CMEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_CM_Event_Fire, osi_Trace_Event_CMEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_Bozo_Event, osi_Trace_Event_BozoEvent_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_Bozo_Event_Fire, osi_Trace_Event_BozoEvent_Id)
_osi_TracePoint_Decl_VariadicEvent(osi_Trace_icl_Event, osi_Trace_Event_icl_Id)
_osi_TracePoint_Decl_VariadicEvent_Fire(osi_Trace_icl_Event_Fire, osi_Trace_Event_icl_Id)
_osi_TracePoint_Decl_VariadicEvent_VarType(osi_Trace_Stats_Event)
_osi_TracePoint_Decl_VariadicEvent_VarType_Fire(osi_Trace_Stats_Event_Fire)

#endif /* !OSI_ENV_VARIADIC_MACROS */


#endif /* _OSI_TRACE_TRACEPOINT_INLINE_H */
