/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_TYPES_H
#define _OSI_TRACE_COMMON_TYPES_H 1

/*
 * osi tracing framework
 * common type definitions
 */

typedef osi_uint32 osi_trace_probe_id_t;

typedef enum {
    osi_Trace_Event_Null_Id,
    osi_Trace_Event_AbortedEvent_Id,
    osi_Trace_Event_FunctionPrologue_Id,
    osi_Trace_Event_FunctionEpilogue_Id,
    osi_Trace_Event_icl_Id,
    osi_Trace_Event_osi_Id,
    osi_Trace_Event_StatsUpdate_Id,
    osi_Trace_Event_Synchronization_Id,
    osi_Trace_Event_CMEvent_Id,
    osi_Trace_Event_FSEvent_Id,
    osi_Trace_Event_VolserEvent_Id,
    osi_Trace_Event_VolPkgEvent_Id,
    osi_Trace_Event_UbikEvent_Id,
    osi_Trace_Event_RxEvent_Id,
    osi_Trace_Event_BozoEvent_Id,
    osi_Trace_Event_Generic_Id,
    osi_Trace_Event_Max_Id
} osi_Trace_EventType;

typedef void (*osi_tracefunc_t)(osi_trace_probe_id_t, osi_Trace_EventType, int nf, ...);

#include <trace/common/record.h>

#endif /* _OSI_TRACE_COMMON_TYPES_H */
