/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_ENCODING_TRAP_TYPES_H
#define _OSI_TRACE_ENCODING_TRAP_TYPES_H 1

/*
 * osi tracing framework
 * remote trap data encoding
 * public type definitions
 */

typedef enum {
    OSI_TRACE_TRAP_ENCODING_ASCII,
    OSI_TRACE_TRAP_ENCODING_ASN1_DER,
    OSI_TRACE_TRAP_ENCODING_ASN1_BER,
    OSI_TRACE_TRAP_ENCODING_XDR,
    OSI_TRACE_TRAP_ENCODING_MAX_ID
} osi_trace_trap_encoding_t;

typedef enum {
    OSI_TRACE_TRAP_ENC_FIELD_NULL,
    OSI_TRACE_TRAP_ENC_FIELD_RECORD_BEGIN,
    OSI_TRACE_TRAP_ENC_FIELD_RECORD_TERMINAL,
    OSI_TRACE_TRAP_ENC_FIELD_TIMESTAMP,
    OSI_TRACE_TRAP_ENC_FIELD_PID,
    OSI_TRACE_TRAP_ENC_FIELD_TID,
    OSI_TRACE_TRAP_ENC_FIELD_VERSION,
    OSI_TRACE_TRAP_ENC_FIELD_GEN_ID,
    OSI_TRACE_TRAP_ENC_FIELD_PROGRAM_TYPE,
    OSI_TRACE_TRAP_ENC_FIELD_TAGS,
    OSI_TRACE_TRAP_ENC_FIELD_PROBE_ID,
    OSI_TRACE_TRAP_ENC_FIELD_PROBE_NAME,
    OSI_TRACE_TRAP_ENC_FIELD_NARGS,
    OSI_TRACE_TRAP_ENC_FIELD_PAYLOAD,
    OSI_TRACE_TRAP_ENC_FIELD_CPUID,
    OSI_TRACE_TRAP_ENC_FIELD_MAX_ID
} osi_trace_trap_enc_field_t;

typedef enum {
    OSI_TRACE_TRAP_ENC_FIELD_TYPE_NULL,
    OSI_TRACE_TRAP_ENC_FIELD_TYPE_SIGNED,
    OSI_TRACE_TRAP_ENC_FIELD_TYPE_UNSIGNED,
    OSI_TRACE_TRAP_ENC_FIELD_TYPE_ARRAY,
    OSI_TRACE_TRAP_ENC_FIELD_TYPE_STRING,
    OSI_TRACE_TRAP_ENC_FIELD_TYPE_MAX_ID
} osi_trace_trap_enc_field_type_t;

#endif /* _OSI_TRACE_ENCODING_TRAP_TYPES_H */
