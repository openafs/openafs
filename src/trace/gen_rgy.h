/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_GEN_RGY_H
#define _OSI_TRACE_GEN_RGY_H 1


/*
 * osi tracing framework
 * generator registry
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
    osi_uint32 spare1[5];
} osi_trace_generator_info_t;

/* reserved generator addresses */
#define OSI_TRACE_GEN_RGY_KERNEL_ID         0
#define OSI_TRACE_GEN_RGY_MAX_ID            239   /* max regular id */
#define OSI_TRACE_GEN_RGY_UCAST_MAX         239   /* max unicast id */
#define OSI_TRACE_GEN_RGY_MCAST_MIN         240   /* first multicast id */
#define OSI_TRACE_GEN_RGY_MCAST_MAX         254   /* last multicast id */
#define OSI_TRACE_GEN_RGY_MCAST_GENERATOR   253   /* trace generator mcast group */
#define OSI_TRACE_GEN_RGY_MCAST_CONSUMER    254   /* trace consumer mcast group */
#define OSI_TRACE_GEN_RGY_BCAST_ID          255   /* general broadcast id */

/* initialization routines */
osi_extern osi_result osi_trace_gen_rgy_PkgInit(void);
osi_extern osi_result osi_trace_gen_rgy_PkgShutdown(void);

/* documentation for other common interfaces */

/*
 * get our generator id
 *
 * osi_result osi_trace_gen_id(osi_trace_gen_id_t * id_out);
 * [INOUT] id_out  -- pointer into which we store our gen id
 *
 * returns:
 *   OSI_FAIL when we don't have a gen_rgy registration
 *   OSI_OK otherwise
 */

#endif /* _OSI_TRACE_GEN_RGY_H */
