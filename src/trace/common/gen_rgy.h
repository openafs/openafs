/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_COMMON_GEN_RGY_H
#define _OSI_TRACE_COMMON_GEN_RGY_H 1


/*
 * osi tracing framework
 * generator registry
 */

#include <trace/common/gen_rgy_types.h>

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
OSI_INIT_FUNC_PROTOTYPE(osi_trace_gen_rgy_PkgInit);
OSI_FINI_FUNC_PROTOTYPE(osi_trace_gen_rgy_PkgShutdown);

#endif /* _OSI_TRACE_COMMON_GEN_RGY_H */
