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


#include "rx/rx_kcommon.h"

#ifdef AFS_DARWIN80_ENV
#define soclose sock_close
#endif

#ifdef RXK_UPCALL_ENV
extern int rxinit_status;

void
rx_upcall(socket_t so, void *arg, __unused int waitflag)
{
    mbuf_t m;
    int error = 0;
    int i, flags = 0;
    struct msghdr msg;
    struct sockaddr_storage ss;
    struct sockaddr *sa = NULL;
    struct sockaddr_in from;
    struct rx_packet *p;
    afs_int32 rlen;
    afs_int32 tlen;
    afs_int32 savelen;          /* was using rlen but had aliasing problems */
    size_t nbytes, resid, noffset;

    /* if rx is shut down, but the socket is not closed yet, we
       can't process packets. just return now. */
    if (rxinit_status)
	return;

    /* See if a check for additional packets was issued */
    rx_CheckPackets();

    p = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE);
    rx_computelen(p, tlen);
    rx_SetDataSize(p, tlen);    /* this is the size of the user data area */
    tlen += RX_HEADER_SIZE;     /* now this is the size of the entire packet */
    rlen = rx_maxJumboRecvSize; /* this is what I am advertising.  Only check
				 * it once in order to avoid races.  */
    tlen = rlen - tlen;
    if (tlen > 0) {
	tlen = rxi_AllocDataBuf(p, tlen, RX_PACKET_CLASS_RECV_CBUF);
	if (tlen > 0) {
	    tlen = rlen - tlen;
	} else
	    tlen = rlen;
    } else
	tlen = rlen;
    /* add some padding to the last iovec, it's just to make sure that the
     * read doesn't return more data than we expect, and is done to get around
     * our problems caused by the lack of a length field in the rx header. */
    savelen = p->wirevec[p->niovecs - 1].iov_len;
    p->wirevec[p->niovecs - 1].iov_len = savelen + RX_EXTRABUFFERSIZE;

    resid = nbytes = tlen + sizeof(afs_int32);

    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &ss;
    msg.msg_namelen = sizeof(struct sockaddr_storage);
    sa =(struct sockaddr *) &ss;

    do {
	m = NULL;
	error = sock_receivembuf(so, &msg, &m, MSG_DONTWAIT, &nbytes);
	if (!error) {
	    size_t sz, offset = 0;
	    noffset = 0;
	    resid = nbytes;
	    for (i=0;i<p->niovecs && resid;i++) {
		sz=MIN(resid, p->wirevec[i].iov_len);
		error = mbuf_copydata(m, offset, sz, p->wirevec[i].iov_base);
		if (error)
		    break;
		resid-=sz;
		offset+=sz;
		noffset += sz;
	    }
	}
    } while (0);

    mbuf_freem(m);

    /* restore the vec to its correct state */
    p->wirevec[p->niovecs - 1].iov_len = savelen;

    if (error == EWOULDBLOCK && noffset > 0)
	error = 0;

    if (!error) {
	int host, port;

	nbytes -= resid;

	if (sa->sa_family == AF_INET)
	    from = *(struct sockaddr_in *)sa;

	p->length = nbytes - RX_HEADER_SIZE;;
	if ((nbytes > tlen) || (p->length & 0x8000)) {  /* Bogus packet */
	    if (nbytes <= 0) {
		if (rx_stats_active) {
		    MUTEX_ENTER(&rx_stats_mutex);
		    rx_stats.bogusPacketOnRead++;
		    rx_stats.bogusHost = from.sin_addr.s_addr;
		    MUTEX_EXIT(&rx_stats_mutex);
		}
		dpf(("B: bogus packet from [%x,%d] nb=%d",
		     from.sin_addr.s_addr, from.sin_port, nbytes));
	    }
	    return;
	} else {
	    /* Extract packet header. */
	    rxi_DecodePacketHeader(p);

	    host = from.sin_addr.s_addr;
	    port = from.sin_port;
	    if (p->header.type > 0 && p->header.type < RX_N_PACKET_TYPES) {
		if (rx_stats_active) {
		    MUTEX_ENTER(&rx_stats_mutex);
		    rx_stats.packetsRead[p->header.type - 1]++;
		    MUTEX_EXIT(&rx_stats_mutex);
		}
	    }

#ifdef RX_TRIMDATABUFS
	    /* Free any empty packet buffers at the end of this packet */
	    rxi_TrimDataBufs(p, 1);
#endif
	    /* receive pcket */
	    p = rxi_ReceivePacket(p, so, host, port, 0, 0);
	}
    }
    /* free packet? */
    if (p)
	rxi_FreePacket(p);

    return;
}

/* in listener env, the listener shutdown does this. we have no listener */
void
osi_StopNetIfPoller(void)
{
    shutdown_rx();
    soclose(rx_socket);
    if (afs_termState == AFSOP_STOP_NETIF) {
	afs_termState = AFSOP_STOP_COMPLETE;
	osi_rxWakeup(&afs_termState);
    }
}
#elif defined(RXK_LISTENER_ENV)
int
osi_NetReceive(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	       int nvecs, int *alength)
{
    int i;
    struct iovec iov[RX_MAXIOVECS];
    struct sockaddr *sa = NULL;
    int code;
    size_t resid;

    int haveGlock = ISAFS_GLOCK();

#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    struct msghdr msg;
    struct sockaddr_storage ss;
    int rlen;
    mbuf_t m;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
    memset(&u, 0, sizeof(u));
#endif
    memset(&iov, 0, sizeof(iov));
    /*AFS_STATCNT(osi_NetReceive); */

    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetReceive: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    if ((afs_termState == AFSOP_STOP_RXK_LISTENER) ||
	(afs_termState == AFSOP_STOP_COMPLETE))
	return -1;

    if (haveGlock)
	AFS_GUNLOCK();
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
#ifdef AFS_DARWIN80_ENV
    resid = *alength;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &ss;
    msg.msg_namelen = sizeof(struct sockaddr_storage);
    sa =(struct sockaddr *) &ss;
    code = sock_receivembuf(asocket, &msg, &m, 0, alength);
    if (!code) {
        size_t offset=0,sz;
        resid = *alength;
        for (i=0;i<nvecs && resid;i++) {
            sz=MIN(resid, iov[i].iov_len);
            code = mbuf_copydata(m, offset, sz, iov[i].iov_base);
            if (code)
                break;
            resid-=sz;
            offset+=sz;
        }
    }
    mbuf_freem(m);
#else

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = *alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_READ;
    u.uio_procp = NULL;
    code = soreceive(asocket, &sa, &u, NULL, NULL, NULL);
    resid = u.uio_resid;
#endif

#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#endif
    if (haveGlock)
	AFS_GLOCK();

    if (code)
	return code;
    *alength -= resid;
    if (sa) {
	if (sa->sa_family == AF_INET) {
	    if (addr)
		*addr = *(struct sockaddr_in *)sa;
	} else
	    printf("Unknown socket family %d in NetReceive\n", sa->sa_family);
#ifndef AFS_DARWIN80_ENV
	FREE(sa, M_SONAME);
#endif
    }
    return code;
}

extern int rxk_ListenerPid;
void
osi_StopListener(void)
{
    struct proc *p;

#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
    soclose(rx_socket);
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#endif
#ifndef AFS_DARWIN80_ENV
    p = pfind(rxk_ListenerPid);
    if (p)
	psignal(p, SIGUSR1);
#endif
}
#else
#error need upcall or listener
#endif

int
osi_NetSend(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	    int nvecs, afs_int32 alength, int istack)
{
    afs_int32 code;
    int i;
    struct iovec iov[RX_MAXIOVECS];
    int haveGlock = ISAFS_GLOCK();
#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    struct msghdr msg;
    size_t slen;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
    memset(&u, 0, sizeof(u));
#endif
    memset(&iov, 0, sizeof(iov));

    AFS_STATCNT(osi_NetSend);
    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    addr->sin_len = sizeof(struct sockaddr_in);

    if ((afs_termState == AFSOP_STOP_RXK_LISTENER) ||
	(afs_termState == AFSOP_STOP_COMPLETE))
	return -1;

    if (haveGlock)
	AFS_GUNLOCK();

#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
#ifdef AFS_DARWIN80_ENV
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = addr;
    msg.msg_namelen = ((struct sockaddr *)addr)->sa_len;
    msg.msg_iov = &iov[0];
    msg.msg_iovlen = nvecs;
    code = sock_send(asocket, &msg, 0, &slen);
#else
    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_WRITE;
    u.uio_procp = NULL;
    code = sosend(asocket, (struct sockaddr *)addr, &u, NULL, NULL, 0);
#endif

#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#endif
    if (haveGlock)
	AFS_GLOCK();
    return code;
}
