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

#ifdef UKERNEL
#include <UKERNEL/rx_kcommon.h>
#else

#ifndef _RX_KCOMMON_H_
#define _RX_KCOMMON_H_

#ifdef AFS_LINUX22_ENV
#ifndef _LINUX_CODA_FS_I
#define _LINUX_CODA_FS_I 1
#define _CODA_HEADER_ 1
struct coda_inode_info {
};
#endif
#endif
#ifdef AFS_DARWIN_ENV
#ifndef _MACH_ETAP_H_
#define _MACH_ETAP_H_
typedef unsigned short etap_event_t;
#endif
#endif


#include "h/types.h"
#include "h/param.h"
#ifndef AFS_LINUX22_ENV
#include "h/systm.h"
#endif
#include "h/time.h"
#ifdef AFS_SUN56_ENV
#include "h/vfs.h"		/* stops SUN56 socketvar.h warnings */
#include "h/stropts.h"		/* stops SUN56 socketvar.h warnings */
#include "h/stream.h"		/* stops SUN56 socketvar.h errors */
#include "h/disp.h"
#endif
#include "h/socket.h"
#if !defined(AFS_LINUX22_ENV) && !defined(AFS_OBSD_ENV)
#include "h/socketvar.h"
#if !defined(AFS_SUN5_ENV) && !defined(AFS_XBSD_ENV)
#include "h/domain.h"
#if !defined(AFS_HPUX110_ENV)
#include "h/dir.h"
#endif
#include "h/buf.h"
#if !defined(AFS_HPUX110_ENV)
#include "h/mbuf.h"
#endif
#else /* !defined(AFS_SUN5_ENV) && !defined(AFS_XBSD_ENV) */
#if defined(AFS_FBSD_ENV)
#include "h/dirent.h"
#include "h/socket.h"
#include "h/domain.h"
#if defined(AFS_FBSD50_ENV)
#include "h/bio.h"
#endif
#include "h/buf.h"
#include "h/mbuf.h"
#endif /* AFS_FBSD_ENV */
#endif /* !defined(AFS_SUN5_ENV) && !defined(AFS_XBSD_ENV) */
#endif /* !defined(AFS_LINUX22_ENV) && !defined(AFS_OBSD_ENV) */
#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#ifdef AFS_FBSD_ENV
#include "h/sysctl.h"
#endif
#ifdef AFS_OBSD_ENV
#include "h/socket.h"
#include "h/domain.h"
#include "h/buf.h"
#include "net/if.h"
#include "h/signalvar.h"
#endif /* AFS_OBSD_ENV */
#include "netinet/in.h"
#ifdef AFS_LINUX22_ENV
#include "linux/route.h"
#else
#include "net/route.h"
#endif
#if defined(HAVE_IN_SYSTM_H) || !defined(AFS_LINUX22_ENV)
#include "netinet/in_systm.h"
#endif
#include "netinet/ip.h"
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX22_ENV) && !defined(AFS_DARWIN60_ENV) && !defined(AFS_OBSD_ENV)
#include "netinet/in_pcb.h"
#endif /* ! AFS_HPUX110_ENV && ! AFS_LINUX22_ENV */
#ifndef AFS_LINUX22_ENV
#if !defined(AFS_DARWIN60_ENV)
#include "netinet/ip_var.h"
#endif
#include "netinet/ip_icmp.h"
#endif /* AFS_LINUX22_ENV */
#include "netinet/udp.h"
#if !defined(AFS_SGI62_ENV) && !defined(AFS_LINUX22_ENV) && !defined(AFS_DARWIN60_ENV)
#include "netinet/udp_var.h"
#endif
#if defined(AFS_HPUX102_ENV) || (defined(AFS_SGI62_ENV) && !defined(AFS_SGI64_ENV))
#include "h/user.h"
#endif
#ifdef AFS_LINUX22_ENV
#include "h/sched.h"
#if defined(FREEZER_H_EXISTS)
#include "h/freezer.h"
#endif
#include "h/netdevice.h"
#include "linux/if.h"
#else
#if !defined(AFS_OBSD_ENV)
#include "h/proc.h"
#include "h/file.h"
#endif
#include "net/if.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_LINUX22_ENV) && !defined(AFS_DARWIN60_ENV)
#include "netinet/in_var.h"
#endif /* ! AFS_HPUX110_ENV && ! AFS_LINUX22_ENV */
#if !defined(AFS_LINUX22_ENV) && !defined(AFS_DUX40_ENV)
#include "rpc/types.h"
#endif
#include "afs/afs_osi.h"
#include "rx_kmutex.h"
#include "afs/lock.h"
#include "rx/xdr.h"
#include "rx/rx.h"
#include "rx/rx_globals.h"
#include "afs/afs_stats.h"
#include "h/errno.h"
#ifdef KERNEL
#include "afs/sysincludes.h"
#include "afsincludes.h"
#endif
#if defined(AFS_OBSD_ENV)
#include "afs/sysincludes.h"
#include "netinet/in_pcb.h"
#endif

#define        MAXRXPORTS  20
typedef unsigned short rxk_ports_t[MAXRXPORTS];
typedef char *rxk_portRocks_t[MAXRXPORTS];
extern rxk_ports_t rxk_ports;
extern rxk_portRocks_t rxk_portRocks;

#if defined(AFS_XBSD_ENV)
extern struct domain inetdomain;
#endif /* AFS_XBSD_ENV */

#if defined(AFS_SUN510_ENV)
extern struct afs_ifinfo afsifinfo[ADDRSPERSITE];
#endif

#endif /* _RX_KCOMMON_H_ */

#endif
