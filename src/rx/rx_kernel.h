/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Definitions specific to the in-kernel implementation of Rx, for in-kernel clients */

#ifndef __RX_KERNEL_INCL_
#define	__RX_KERNEL_INCL_   1

#define osi_Alloc afs_osi_Alloc
#define osi_Free  afs_osi_Free

#define rxi_ReScheduleEvents    0	/* Not needed by kernel */

/* This is a no-op, because the kernel server procs are pre-allocated */
#define rxi_StartServerProcs(x) 0

/* Socket stuff */
typedef struct socket *osi_socket;
#define	OSI_NULLSOCKET	((osi_socket) 0)

#if (!defined(AFS_GLOBAL_SUNLOCK) && !defined(RX_ENABLE_LOCKS))
#include "afs/icl.h"
#include "afs/afs_trace.h"
#endif
#define osi_rxSleep(a)  afs_Trace2(afs_iclSetp, CM_TRACE_RXSLEEP, \
        ICL_TYPE_STRING, __FILE__, ICL_TYPE_INT32, __LINE__); afs_osi_Sleep(a)
#define osi_rxWakeup(a) if (afs_osi_Wakeup(a) == 0) afs_Trace2(afs_iclSetp, \
        CM_TRACE_RXWAKE, ICL_TYPE_STRING, __FILE__, ICL_TYPE_INT32, __LINE__)

extern int osi_utoa(char *buf, size_t len, unsigned long val);
#define osi_Assert(e) (void)((e) || (osi_AssertFailK(#e, __FILE__, __LINE__), 0))

#define	osi_Msg printf)(

#define	osi_YieldIfPossible()
#define	osi_WakeupAndYieldIfPossible(x)	    rx_Wakeup(x)

#ifndef AFS_DARWIN80_ENV
#define ifnet_mtu(x) (x)->if_mtu
#define AFS_IFNET_T struct ifnet *
#else
#define AFS_IFNET_T ifnet_t
#endif

#ifndef ifnet_flags
#define ifnet_flags(x) (x?(x)->if_flags:0)
#endif

#include "afs/longc_procs.h"

#endif /* __RX_KERNEL_INCL_ */
