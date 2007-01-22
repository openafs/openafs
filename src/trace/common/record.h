/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_RECORD_H
#define _OSI_TRACE_COMMON_RECORD_H 1

/*
 * tracepoint record
 * this structure is the output of a tracepoint
 * ring buffers are made up of arrays of this struct
 */

/* 
 * total trace record length MUST always be the same;
 * regardless of ISA issues
 */
#if (OSI_TYPE_REGISTER_BITS == 32)
#define OSI_TRACEPOINT_PAYLOAD_VEC_LEN 24
#elif (OSI_TYPE_REGISTER_BITS == 64)
#define OSI_TRACEPOINT_PAYLOAD_VEC_LEN 12
#else
#error "invalid size of osi_register_int; please file a bug"
#endif

/* 
 * although ILP32 processes can technically stuff 24 args,
 * we limit to 12 because some use cases use two adjacent
 * fields to create a fake uint64_t 
 *
 * originally, register int's were used to make arg dumps
 * convenient.  now I'm beginnning to rethink that decision.
 * for now, we'll consider it a length 12 vector in which
 * only even indices are used on ILP32 platforms...
 */
#define OSI_TRACEPOINT_MAX_ARGS 12


/*
 * the following is the VERSION 2 tracepoint record definition:
 * (record version 1 was an early prototype, which was never
 *  released publicly)
 *
 * fields:
 *   version     -- tracepoint record version number (always 2 for the moment)
 *   gen_id      -- generator id of the context which created the record
 *   tags        -- vector of osi_Trace_EventType enumerations
 *   probe       -- osi_trace_probe_id_t value
 *   timestamp   -- 64-bit record timestamp
 *   pid         -- pid of context which generated the trace record
 *   tid         -- id of thread which generated the trace record
 *   spare1      -- spare field
 *   nargs       -- number of arguments in vector
 *   payload     -- payload vector
 *
 * for userspace trap probes, field fill-in is broken down as follows:
 *
 * filled in by userspace process context:
 *   version
 *   gen_id
 *   tags
 *   probe
 *   spare1
 *   nargs
 *   payload
 *
 * filled in by kernel trap handler:
 *   timestamp
 *   pid
 *   tid
 *
 * note that fields filled in by kernel are specifically located
 * at the top of the structure to reduce copyin overhead
 */
typedef struct osi_TracePoint_record_v2 {
    osi_time64 timestamp;
    osi_int32  pid;
    osi_int32  tid;
    osi_uint8  version;
    osi_uint8  gen_id;
    osi_uint8  tags[2];
    osi_uint32 probe;
    osi_uint32 spare1;
    osi_uint8 nargs;
    osi_uint8 spare2;
    osi_uint16 spare3;
    osi_register_int payload[OSI_TRACEPOINT_PAYLOAD_VEC_LEN];
} osi_TracePoint_record_v2;

typedef osi_TracePoint_record_v2 osi_TracePoint_record;

#define OSI_TRACEPOINT_RECORD_VERSION_V1 1  /* only for posterity's sake */
#define OSI_TRACEPOINT_RECORD_VERSION_V2 2

/* the current record serialization version */
#define OSI_TRACEPOINT_RECORD_VERSION 2


#if (OSI_ENV_INLINE_INCLUDE)
#include <trace/common/record_inline.h>
#endif


#endif /* _OSI_TRACE_COMMON_RECORD_H */
