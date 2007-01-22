/*
 * Copyright 2006, Sine Nomine Associates and others.
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
 * types
 */

typedef enum {
    OSI_TRACE_TRAP_ENCODING_ASCII,
    OSI_TRACE_TRAP_ENCODING_ASN1,
    OSI_TRACE_TRAP_ENCODING_MAX_ID
} osi_trace_trap_encoding_t;

#endif /* _OSI_TRACE_ENCODING_TRAP_TYPES_H */
