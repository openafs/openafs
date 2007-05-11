/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_GEN_RGY_TYPES_H
#define _OSI_TRACE_COMMON_GEN_RGY_TYPES_H 1


/*
 * osi tracing framework
 * generator registry
 * public type definitions
 */

typedef osi_uint8 osi_trace_gen_id_t;

typedef struct osi_trace_generator_address {
    osi_uint32 programType;
    osi_uint32 pid;
} osi_trace_generator_address_t;

typedef struct osi_trace_generator_info {
    osi_uint32 gen_id;
    osi_uint32 programType;
    osi_int32  epoch;
    osi_uint32 module_count;
    osi_uint32 probe_count;
    osi_uint32 module_version_cksum;
    osi_uint32 module_version_cksum_type;
    osi_uint32 probe_id_max;
    osi_uint32 spare1[24];
} osi_trace_generator_info_t;

#endif /* _OSI_TRACE_COMMON_GEN_RGY_TYPES_H */
