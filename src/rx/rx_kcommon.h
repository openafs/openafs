/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_kcommon.h - Common kernel RX header for all system types.
 */

#ifndef _RX_KCOMMON_H_
#define _RX_KCOMMON_H_

#include "../h/types.h"
#include "../h/param.h"
#ifndef AFS_LINUX22_ENV
#include "../h/systm.h"
#endif
#include "../h/time.h"
#ifdef AFS_SUN56_ENV
#include "../h/vfs.h"		/* stops SUN56 socketvar.h warnings */
#include "../h/stropts.h"	/* stops SUN56 socketvar.h warnings */
#include "../h/stream.h"	/* stops SUN56 socketvar.h errors */
#endif
#include "../h/socket.h"
#ifndef AFS_LINUX22_ENV
#include "../h/socketvar.h"
#include "../h/protosw.h"
#ifndef AFS_SUN5_ENV
#include "../h/domain.h"
#include "../h/dir.h"
#include "../h/buf.h"
#include "../h/mbuf.h"
#endif
#endif /* AFS_LINUX22_ENV */
#ifdef AFS_SGI62_ENV
#include "../h/hashing.h"
#endif
#include "../netinet/in.h"
#include "../net/route.h"
#include "../netinet/in_systm.h"
#include "../netinet/ip.h"
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX22_ENV)
#include "../netinet/in_pcb.h"
#endif /* ! AFS_HPUX110_ENV && ! AFS_LINUX22_ENV */
#ifndef AFS_LINUX22_ENV
#include "../netinet/ip_var.h"
#include "../netinet/ip_icmp.h"
#endif /* AFS_LINUX22_ENV */
#include "../netinet/udp.h"
#if !defined(AFS_SGI62_ENV) && !defined(AFS_LINUX22_ENV)
#include "../netinet/udp_var.h"
#endif
#if defined(AFS_HPUX102_ENV) || (defined(AFS_SGI62_ENV) && !defined(AFS_SGI64_ENV))
#include "../h/user.h"
#endif
#ifdef AFS_LINUX22_ENV
#define _LINUX_CODA_FS_I
struct coda_inode_info {};
#include "../h/sched.h"
#include "../h/netdevice.h"
#else
#include "../h/proc.h"
#include "../h/file.h"
#endif
#include "../net/if.h"
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX22_ENV)
#include "../netinet/in_var.h"
#endif /* ! AFS_HPUX110_ENV && ! AFS_LINUX22_ENV */
#ifndef AFS_LINUX22_ENV
#include "../rpc/types.h"
#endif
#include "../afs/afs_osi.h"
#include "../rx/rx_kmutex.h"
#include "../afs/lock.h"
#ifndef AFS_LINUX22_ENV
#include "../rpc/xdr.h"
#endif
#include "../rx/rx.h"
#include "../rx/rx_globals.h"
#include "../afs/longc_procs.h"
#include "../afs/afs_stats.h"
#include "../h/errno.h"

extern afs_int32 afs_termState;
extern int (*rxk_GetPacketProc)(); /* set to packet allocation procedure */
extern int (*rxk_PacketArrivalProc)();

#define	MAXRXPORTS  20
typedef unsigned short rxk_ports_t[MAXRXPORTS];
typedef char *rxk_portRocks_t[MAXRXPORTS];
extern rxk_ports_t rxk_ports;
extern rxk_portRocks_t rxk_portRocks;

extern struct osi_socket *rxk_NewSocket(short aport);
extern struct ifnet *rxi_FindIfnet();

extern int rxk_initDone;

#endif /* _RX_KCOMMON_H_ */
