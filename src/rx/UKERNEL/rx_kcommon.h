/* Copyright Transarc Corporation 1998 - All Rights Reserved.
 *
 * rx_kcommon.h - Common kernel RX header for all system types.
 */

#ifndef _RX_KCOMMON_H_
#define _RX_KCOMMON_H_

#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/afsincludes.h"
#include "../rx/rx_globals.h"
#include "../rx/rx_kmutex.h"
#include "../afs/lock.h"
#include "../rx/rx.h"
#include "../rx/rx_globals.h"
#include "../afs/longc_procs.h"
#include "../afs/afs_stats.h"

extern struct usr_ifnet *usr_ifnet;
extern struct usr_in_ifaddr *usr_in_ifaddr;
extern struct usr_domain inetdomain;
extern struct usr_protosw udp_protosw;

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
