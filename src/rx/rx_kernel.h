
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/* Definitions specific to the in-kernel implementation of Rx, for in-kernel clients */

#ifndef __RX_KERNEL_INCL_
#define	__RX_KERNEL_INCL_   1

#define osi_Alloc afs_osi_Alloc
#define osi_Free  afs_osi_Free

extern int (*rxk_GetPacketProc)(); /* set to packet allocation procedure */
extern int (*rxk_PacketArrivalProc)();

extern void rxi_StartListener();
#define rxi_ReScheduleEvents    0 /* Not needed by kernel */

/* This is a no-op, because the kernel server procs are pre-allocated */
#define rxi_StartServerProcs(x)

/* Socket stuff */
typedef struct socket *osi_socket;
#define	OSI_NULLSOCKET	((osi_socket) 0)

extern osi_socket rxi_GetUDPSocket();

#if (!defined(AFS_GLOBAL_SUNLOCK) && !defined(RX_ENABLE_LOCKS)) || (defined(AFS_HPUX_ENV) && !defined(RX_ENABLE_LOCKS))
#define	osi_rxSleep(a)	afs_osi_Sleep(a)
#define	osi_rxWakeup(a)	afs_osi_Wakeup(a)
#endif

extern void osi_Panic();
extern int osi_utoa(char *buf, size_t len, unsigned long val);
extern void osi_AssertFailK(const char *expr, const char *file, int line);
#define osi_Assert(e) (void)((e) || (osi_AssertFailK(#e, __FILE__, __LINE__), 0))

#define	osi_Msg printf)(

#define	osi_YieldIfPossible()
#define	osi_WakeupAndYieldIfPossible(x)	    rx_Wakeup(x)

#include "../afs/longc_procs.h"

#endif /* __RX_KERNEL_INCL_ */
