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

#ifndef RXK_TIMEDSLEEP_ENV
#define rxi_ReScheduleEvents    0	/* Not needed by kernel */
#endif

/* This is a no-op, because the kernel server procs are pre-allocated */
#define rxi_StartServerProcs(x) (void)0

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
#define osi_Assert(exp) (void)((exp) || (osi_AssertFailK( #exp , __FILE__, __LINE__), 0))

#define	osi_Msg printf)(
#define osi_VMsg vprintf)(

#define	osi_YieldIfPossible()
#define	osi_WakeupAndYieldIfPossible(x)	    rx_Wakeup(x)

#if !defined(AFS_DARWIN80_ENV) || defined(UKERNEL)
# ifdef UKERNEL
# define rx_ifnet_t struct usr_ifnet *
# define rx_ifaddr_t struct usr_ifaddr *
# else
# define rx_ifnet_t struct ifnet *
# define rx_ifaddr_t struct ifaddr *
#endif
#define rx_ifnet_mtu(x) (x)->if_mtu
#define rx_ifnet_flags(x) (x?(x)->if_flags:0)
#if defined(AFS_OBSD46_ENV) || defined(AFS_FBSD81_ENV)
#define rx_ifaddr_withnet(x) ifa_ifwithnet(x, 0)
#else
#define rx_ifaddr_withnet(x) ifa_ifwithnet(x)
#endif
#define rx_ifnet_metric(x) (x?(x)->if_data.ifi_metric:0)
#define rx_ifaddr_ifnet(x) (x?(x)->ifa_ifp:0)
#define rx_ifaddr_address_family(x) (x)->ifa_addr->sa_family
#define rx_ifaddr_address(x, y, z) memcpy(y, (x)->ifa_addr, z)
#define rx_ifaddr_netmask(x, y, z) memcpy(y, (x)->ifa_netmask, z)
#define rx_ifaddr_dstaddress(x, y, z) memcpy(y, (x)->ifa_dstaddr, z)
#else
#define rx_ifnet_t ifnet_t
#define rx_ifaddr_t ifaddr_t
#define rx_ifaddr_withnet(x) ifaddr_withnet(x)
#define rx_ifnet_mtu(x) ifnet_mtu(x)
#define rx_ifnet_flags(x) ifnet_flags(x)
#define rx_ifnet_metric(x) ifnet_metric(x)
#define rx_ifaddr_ifnet(x) ifaddr_ifnet(x)
#define rx_ifaddr_address_family(x) ifaddr_address_family(x)
#define rx_ifaddr_address(x, y, z) ifaddr_address(x, y, z)
#define rx_ifaddr_netmask(x, y, z) ifaddr_netmask(x, y, z)
#define rx_ifaddr_dstaddress(x, y, z) ifaddr_dstaddress(x, y, z)
#endif

#endif /* __RX_KERNEL_INCL_ */
