/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_RECORD_TYPES_H
#define _OSI_TRACE_COMMON_RECORD_TYPES_H 1

/*
 * tracepoint record
 * public type definitions
 *
 * this structure is the output of a tracepoint
 * ring buffers are made up of arrays of this struct
 */

#include <osi/osi_time.h>


/* 
 * total trace record length (in bytes) 
 * MUST always be the same;
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
 * The following four fields are a REQUIRED preamble for
 * ALL tracepoint record type definitions
 */
typedef struct osi_TracePoint_record_hdr {
    osi_timeval32_t timestamp;
    osi_int32  pid;
    osi_int32  tid;
    osi_uint8  version;
} osi_TracePoint_record_hdr;
/* first three fields of preamble are never copied in from userspace */
#define OSI_TRACEPOINT_RECORD_COPYIN_OFFSET 16


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
    osi_timeval32_t timestamp;
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

/*
 * the following is the VERSION 3 tracepoint record definition:
 *
 * fields:
 *   version     -- tracepoint record version number (always 2 for the moment)
 *   gen_id      -- generator id of the context which created the record
 *   tags        -- vector of osi_Trace_EventType enumerations
 *   probe       -- osi_trace_probe_id_t value
 *   timestamp   -- 64-bit record timestamp
 *   pid         -- pid of context which generated the trace record
 *   tid         -- id of thread which generated the trace record
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
 *   nargs
 *   payload
 *
 * filled in by kernel trap handler:
 *   timestamp
 *   pid
 *   tid
 *   cpu_id
 *
 * note that fields filled in by kernel are specifically located
 * at the top of the structure to reduce copyin overhead
 */
typedef struct osi_TracePoint_record_v3 {
    osi_timespec32_t timestamp;
    osi_int32  pid;
    osi_int32  tid;
    osi_uint8  version;
    osi_uint8  nargs;
    osi_uint8  tags[2];
    osi_uint32 probe;
    osi_uint32 gen_id;
    osi_uint32 cpu_id;
    osi_register_int payload[OSI_TRACEPOINT_PAYLOAD_VEC_LEN];
} osi_TracePoint_record_v3;

typedef osi_TracePoint_record_v3 osi_TracePoint_record;

typedef enum {
    OSI_TRACEPOINT_RECORD_VERSION_V1 = 1,  /* only for posterity's sake */
    OSI_TRACEPOINT_RECORD_VERSION_V2 = 2,  /* original version released to public */
    OSI_TRACEPOINT_RECORD_VERSION_V3 = 3,  /* adds cpu_id, and makes gen_id 32 bits wide */
} osi_TracePoint_record_version_t;

/* the current record serialization version */
#define OSI_TRACEPOINT_RECORD_VERSION OSI_TRACEPOINT_RECORD_VERSION_V3

typedef union {
    void * opaque;
    osi_TracePoint_record_hdr * preamble;
    osi_TracePoint_record_v2 * v2;
    osi_TracePoint_record_v3 * v3;
    osi_TracePoint_record * cur;
} osi_TracePoint_record_ptr_u;

typedef struct {
    osi_TracePoint_record_ptr_u ptr;
    osi_TracePoint_record_version_t version;
} osi_TracePoint_record_ptr_t;


#endif /* _OSI_TRACE_COMMON_RECORD_TYPES_H */
