/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _RX_INTERNAL_H_
#define _RX_INTERNAL_H_

#ifdef AFS_NT40_ENV
#include "rx.h"

/* rx.c */
int rxi_IsConnInteresting(struct rx_connection *aconn);

/* rx_lwp.c */
extern void rxi_Delay(int seconds);
extern void rxi_InitializeThreadSupport(void);
extern void rxi_Sleep(void *addr);
extern void rxi_Wakeup(void *addr);
extern int rxi_Recvmsg(osi_socket socket, struct msghdr *msg_p, int flags);
extern int rxi_Sendmsg(osi_socket socket, struct msghdr *msg_p, int flags);

/* rx_packet.c */
extern void rxi_FreePacket(struct rx_packet *p);
extern int rxi_AllocDataBuf(struct rx_packet *p, int nb, int class);
extern void rxi_MorePackets(int apackets);
extern afs_int32 rx_SlowGetInt32(struct rx_packet *packet, size_t offset);
extern afs_int32 rx_SlowPutInt32(struct rx_packet *packet, size_t offset, afs_int32 data);
extern afs_int32 rx_SlowWritePacket(struct rx_packet *packet, int offset,
				int resid, char *in);
extern void rxi_SendPacket(struct rx_connection * conn, struct rx_packet *p,
		    int istack);
extern afs_int32 rx_SlowReadPacket(struct rx_packet *packet,
			       u_int offset, int resid,	char *out);
extern void rxi_FreeAllPackets(void);

/* rx_user.c */
extern osi_socket rxi_GetUDPSocket(u_short port);
extern void rxi_InitPeerParams(struct rx_peer *pp);


/* rx_event.c */
extern struct rxevent *rxevent_Post(struct clock *when, void (*func)(),
			     void *arg, void *arg1);


#endif /* AFS_NT40_ENV */
#endif /* _RX_INTERNAL_H_ */
