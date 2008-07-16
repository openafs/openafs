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

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afsincludes.h"
#include "rx/rx_globals.h"
#include "rx_kmutex.h"
#include "afs/lock.h"
#include "rx/rx.h"
#include "rx/rx_globals.h"
#include "afs/longc_procs.h"
#include "afs/afs_stats.h"

extern struct usr_ifnet *usr_ifnet;
extern struct usr_in_ifaddr *usr_in_ifaddr;
extern struct usr_domain inetdomain;
extern struct usr_protosw udp_protosw;

#define        MAXRXPORTS  20
typedef unsigned short rxk_ports_t[MAXRXPORTS];
typedef char *rxk_portRocks_t[MAXRXPORTS];
extern rxk_ports_t rxk_ports;
extern rxk_portRocks_t rxk_portRocks;

#ifndef ifnet_flags
#define ifnet_flags(x) (x?(x)->if_flags:0)
#endif

#endif /* _RX_KCOMMON_H_ */
