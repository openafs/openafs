/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/rx/UKERNEL/rx_knet.c,v 1.10.2.3 2006/01/26 20:55:38 shadow Exp $");

#include "rx/rx_kcommon.h"


#define SECONDS_TO_SLEEP	0
#define NANO_SECONDS_TO_SLEEP	100000000	/* 100 milliseconds */
#define LOOPS_PER_WAITCHECK	10	/* once per second */

unsigned short usr_rx_port = 0;

struct usr_ifnet *usr_ifnet = NULL;
struct usr_in_ifaddr *usr_in_ifaddr = NULL;

void rxk_InitializeSocket();

void
afs_rxevent_daemon(void)
{
    struct timespec tv;
    struct clock temp;
    int i = 0;

    AFS_GUNLOCK();
    while (1) {
	tv.tv_sec = SECONDS_TO_SLEEP;
	tv.tv_nsec = NANO_SECONDS_TO_SLEEP;
	usr_thread_sleep(&tv);
	/*
	 * Check for shutdown, don't try to stop the listener
	 */
	if (afs_termState == AFSOP_STOP_RXEVENT
	    || afs_termState == AFSOP_STOP_RXK_LISTENER) {
	    AFS_GLOCK();
	    afs_termState = AFSOP_STOP_COMPLETE;
	    afs_osi_Wakeup(&afs_termState);
	    return;
	}
	rxevent_RaiseEvents(&temp);
	if (++i >= LOOPS_PER_WAITCHECK) {
	    i = 0;
	    afs_osi_CheckTimedWaits();
	}
    }
}


/* Loop to listen on a socket. Return setting *newcallp if this
 * thread should become a server thread.  */
void
rxi_ListenerProc(osi_socket usockp, int *tnop, struct rx_call **newcallp)
{
    struct rx_packet *tp;
    afs_uint32 host;
    u_short port;
    int rc;

    /*
     * Use the rxk_GetPacketProc and rxk_PacketArrivalProc routines
     * to allocate rx_packet buffers and pass them to the RX layer
     * for processing.
     */
    while (1) {
	tp = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE);
	usr_assert(tp != NULL);
	rc = rxi_ReadPacket(usockp, tp, &host, &port);
	if (rc != 0) {
	    tp = rxi_ReceivePacket(tp, usockp, host, port, tnop, newcallp);
	    if (newcallp && *newcallp) {
		if (tp) {
		    rxi_FreePacket(tp);
		}
		return;
	    }
	}
	if (tp) {
	    rxi_FreePacket(tp);
	}
	if (afs_termState == AFSOP_STOP_RXEVENT) {
	    afs_termState = AFSOP_STOP_RXK_LISTENER;
	    afs_osi_Wakeup(&afs_termState);
	}
    }
}

/* This is the listener process request loop. The listener process loop
 * becomes a server thread when rxi_ListenerProc returns, and stays
 * server thread until rxi_ServerProc returns. */
void
rxk_Listener(void)
{
    int threadID;
    osi_socket sock = (osi_socket) rx_socket;
    struct rx_call *newcall;
    struct usr_socket *usockp;

    /*
     * Initialize the rx_socket and start the receiver threads
     */
    rxk_InitializeSocket();

    usockp = (struct usr_socket *)rx_socket;
    assert(usockp != NULL);

    AFS_GUNLOCK();
    while (1) {
	newcall = NULL;
	threadID = -1;
	rxi_ListenerProc(sock, &threadID, &newcall);
	/* assert(threadID != -1); */
	/* assert(newcall != NULL); */
	sock = OSI_NULLSOCKET;
	rxi_ServerProc(threadID, newcall, &sock);
	if (sock == OSI_NULLSOCKET) {
	    break;
	}
    }
    AFS_GLOCK();
}

/* This is the server process request loop. The server process loop
 * becomes a listener thread when rxi_ServerProc returns, and stays
 * listener thread until rxi_ListenerProc returns. */
void
rx_ServerProc(void)
{
    osi_socket sock;
    int threadID;
    struct rx_call *newcall = NULL;

    rxi_MorePackets(rx_maxReceiveWindow + 2);	/* alloc more packets */
    rxi_dataQuota += rx_initSendWindow;	/* Reserve some pkts for hard times */
    /* threadID is used for making decisions in GetCall.  Get it by bumping
     * number of threads handling incoming calls */
    threadID = rxi_availProcs++;

    AFS_GUNLOCK();
    while (1) {
	sock = OSI_NULLSOCKET;
	rxi_ServerProc(threadID, newcall, &sock);
	if (sock == OSI_NULLSOCKET) {
	    break;
	}
	newcall = NULL;
	threadID = -1;
	rxi_ListenerProc(sock, &threadID, &newcall);
	/* assert(threadID != -1); */
	/* assert(newcall != NULL); */
    }
    AFS_GLOCK();
}

/*
 * At this point, RX wants a socket, but still has not initialized the
 * rx_port variable or the pointers to the packet allocater and arrival
 * routines. Allocate the socket buffer here, but don't open it until
 * we start the receiver threads.
 */
osi_socket *
rxk_NewSocketHost(afs_uint32 ahost, short aport)
{
    struct usr_socket *usockp;

    usockp = (struct usr_socket *)afs_osi_Alloc(sizeof(struct usr_socket));
    usr_assert(usockp != NULL);

    usockp->sock = -1;

    return (osi_socket *)usockp;
}

osi_socket *
rxk_NewSocket(short aport)
{
    return rxk_NewSocketHost(htonl(INADDR_ANY), aport);
}

/*
 * This routine is called from rxk_Listener. By this time rx_port
 * is set to 7001 and rx_socket points to the socket buffer
 * we allocated in rxk_NewSocket. Now is the time to bind our
 * socket and start the receiver threads.
 */
void
rxk_InitializeSocket(void)
{
    int rc, sock, i;
#ifdef AFS_USR_AIX_ENV
    unsigned long len, optval, optval0, optlen;
#else /* AFS_USR_AIX_ENV */
    int len, optval, optval0, optlen;
#endif /* AFS_USR_AIX_ENV */
    struct usr_socket *usockp;
    struct sockaddr_in lcladdr;

    usr_assert(rx_socket != NULL);
    usockp = (struct usr_socket *)rx_socket;

#undef socket
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    usr_assert(sock >= 0);

    memset((void *)&lcladdr, 0, sizeof(struct sockaddr_in));
    lcladdr.sin_family = AF_INET;
    lcladdr.sin_port = htons(usr_rx_port);
    lcladdr.sin_addr.s_addr = INADDR_ANY;
    rc = bind(sock, (struct sockaddr *)&lcladdr, sizeof(struct sockaddr_in));
    usr_assert(rc >= 0);
    len = sizeof(struct sockaddr_in);
    rc = getsockname(sock, (struct sockaddr *)&lcladdr, &len);
    usr_assert(rc >= 0);
#ifdef AFS_USR_LINUX22_ENV
    optval0 = 131070;
#else
    optval0 = 131072;
#endif
    optval = optval0;
    rc = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&optval,
		    sizeof(optval));
    usr_assert(rc == 0);
    optlen = sizeof(optval);
    rc = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (void *)&optval, &optlen);
    usr_assert(rc == 0);
    /* usr_assert(optval == optval0); */
#ifdef AFS_USR_LINUX22_ENV
    optval0 = 131070;
#else
    optval0 = 131072;
#endif
    optval = optval0;
    rc = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *)&optval,
		    sizeof(optval));
    usr_assert(rc == 0);
    optlen = sizeof(optval);
    rc = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *)&optval, &optlen);
    usr_assert(rc == 0);
    /* usr_assert(optval == optval0); */

#ifdef AFS_USR_AIX_ENV
    optval = 1;
    rc = setsockopt(sock, SOL_SOCKET, SO_CKSUMRECV, (void *)&optval,
		    sizeof(optval));
    usr_assert(rc == 0);
#endif /* AFS_USR_AIX_ENV */

    usockp->sock = sock;
    usockp->port = lcladdr.sin_port;

    /*
     * Set the value of rx_port to reflect the address we actually
     * are listening on, since the kernel is probably already using 7001.
     */
    rx_port = usockp->port;
}

int
rxk_FreeSocket(struct usr_socket *sockp)
{
    return 0;
}

void
osi_StopListener(void)
{
    rxk_FreeSocket((struct usr_socket *)rx_socket);
}

int
osi_NetSend(osi_socket sockp, struct sockaddr_in *addr, struct iovec *iov,
	    int nio, afs_int32 size, int stack)
{
    int rc;
    int i;
    unsigned long tmp;
    struct usr_socket *usockp = (struct usr_socket *)sockp;
    struct msghdr msg;
    struct iovec tmpiov[64];

    /*
     * The header is in the first iovec
     */
    usr_assert(nio > 0 && nio <= 64);
    for (i = 0; i < nio; i++) {
	tmpiov[i].iov_base = iov[i].iov_base;
	tmpiov[i].iov_len = iov[i].iov_len;
    }

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *)addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = &tmpiov[0];
    msg.msg_iovlen = nio;

    rc = sendmsg(usockp->sock, &msg, 0);
    if (rc < 0) {
	return errno;
    }
    usr_assert(rc == size);

    return 0;
}

void
shutdown_rxkernel(void)
{
    rxk_initDone = 0;
    rxk_shutdownPorts();
}

void
rx_Finalize(void)
{
    usr_assert(0);
}

/*
 * Recvmsg.
 *
 */
int
rxi_Recvmsg(osi_socket socket, struct msghdr *msg_p, int flags)
{
    int ret;
    do {
	ret = recvmsg(socket->sock, msg_p, flags);
    } while (ret == -1 && errno == EAGAIN);
    return ret;
}
