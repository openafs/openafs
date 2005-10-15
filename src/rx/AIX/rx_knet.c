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
    ("$Header$");

#ifdef AFS_AIX41_ENV
#include "rx/rx_kcommon.h"

static struct protosw parent_proto;	/* udp proto switch */

static void
rxk_input(struct mbuf *am, int hlen)
{
    unsigned short *tsp;
    int hdr;
    struct udphdr *tu;
    struct ip *ti;
    struct udpiphdr *tvu;
    int i;
    char *phandle;
    long code;
    struct sockaddr_in taddr;
    int tlen;
    short port;
    int data_len, comp_sum;
    /* make sure we have base ip and udp headers in first mbuf */
    if (M_HASCL(am) || am->m_len < 28) {
	am = m_pullup(am, 28);
	if (!am)
	    return;
    }
    hdr = (mtod(am, struct ip *))->ip_hl;
    if (hdr > 5) {
	/* pull up more, the IP hdr is bigger than usual */
	if (am->m_len < (8 + (hdr << 2))) {
	    am = m_pullup(am, 8 + (hdr << 2));
	    if (!am)
		return;
	}
	ti = mtod(am, struct ip *);	/* recompute, since m_pullup allocates new mbuf */
	tu = (struct udphdr *)(((char *)ti) + (hdr << 2));	/* skip ip hdr */
    } else {
	ti = mtod(am, struct ip *);
	tu = (struct udphdr *)(((char *)ti) + 20);	/* skip basic ip hdr */
    }

    /* now read the port out */
    port = tu->uh_dport;
    if (port) {
	for (tsp = rxk_ports, i = 0; i < MAXRXPORTS; i++) {
	    if (*tsp++ == port) {
		rxk_kpork(am);
		return;
	    }
	}
    }
    /* if we get here, try to deliver packet to udp */
    if (parent_proto.pr_input)
	udp_input(am, hlen);
}

/*
 * the AIX version is complicated by the fact that the internet protocols
 * are in a separate kernel extension, and they are unwilling to export their
 * symbols to us.  We can get there indirectly, however.
 */
#include <net/netisr.h>
static struct ifqueue rxk_q;	/* RXKluge queue        */
static struct arpcom rxk_bogosity;

/* rxk_kpork -	send pkt over to netwerk kporc for processing */
rxk_kpork(struct mbuf *m)
{
    find_input_type(0xdead, m, &rxk_bogosity, 0);
}

/*
 * AIX 4.3.3 changed the type of the second argument to
 * ip_stripoptions().  The ip_stripoptions() prototype is in
 * <netinet/proto_inet.h>.  This header file also acquired a guard
 * macro, _PROTO_INET_H_, at the same time.  So we test for the guard
 * macro to see which type we need to use for the second argument to
 * ip_stripoptions().
 *
 * This way we don't have to introduce a port just to compile AFS on AIX
 * 4.3.3.
 */

#if defined(_PROTO_INET_H_)	/* AIX 4.3.3 and presumably later */
#define	STRIP_ARG2_TYPE	unsigned long
#else /* AIX 4.3.2 and earlier */
#define	STRIP_ARG2_TYPE	struct mbuf *
#endif

void
ip_stripoptions(struct mbuf *m, STRIP_ARG2_TYPE mopt)
{
    struct ip *ip = mtod(m, struct ip *);
    int i;
    caddr_t opts;
    int olen;

    olen = (ip->ip_hl << 2) - sizeof(struct ip);
    opts = (caddr_t) (ip + 1);
    i = m->m_len - (sizeof(struct ip) + olen);
    memcpy(opts, opts + olen, (unsigned)i);
    m->m_len -= olen;
    if (m->m_flags & M_PKTHDR)
	m->m_pkthdr.len -= olen;
    ip->ip_hl = sizeof(struct ip) >> 2;
}

/* rxk_RX_input -	RX pkt input process */
rxk_RX_input(struct mbuf *am)
{
    unsigned short *tsp;
    int hdr;
    struct udphdr *tu;
    struct ip *ti;
    struct udpiphdr *tvu;
    int i;
    struct rx_packet *phandle;
    long code;
    struct sockaddr_in taddr;
    int tlen;
    short port;
    int data_len, comp_sum;

    hdr = (ti = mtod(am, struct ip *))->ip_hl;
    if (hdr > 5) {
	ip_stripoptions(am, 0);	/* get rid of anything we don't need */
    }
    tu = (struct udphdr *)(((char *)ti) + 20);
    /*
     * Make mbuf data length reflect UDP length.
     * If not enough data to reflect UDP length, drop.
     */
    tvu = (struct udpiphdr *)ti;
    tlen = ntohs((u_short) tvu->ui_ulen);
    if ((int)ti->ip_len != tlen) {
	if (tlen > (int)ti->ip_len) {
#ifdef RX_KERNEL_TRACE
	    int glockOwner = ISAFS_GLOCK();
	    if (!glockOwner)
		AFS_GLOCK();
	    afs_Trace3(afs_iclSetp, CM_TRACE_WASHERE, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_INT32,
		       tlen);
	    if (!glockOwner)
		AFS_GUNLOCK();
#endif
	    m_free(am);
	    return;
	}
	m_adj(am, tlen - (int)ti->ip_len);
    }
    /* deliver packet to rx */
    taddr.sin_family = AF_INET;	/* compute source address */
    taddr.sin_port = tu->uh_sport;
    taddr.sin_addr.s_addr = ti->ip_src.s_addr;
    /* handle the checksum.  Note that this code damages the actual ip
     * header (replacing it with the virtual one, which is the same size),
     * so we must ensure we get everything out we need, first */
    if (tu->uh_sum != 0) {
	/* if the checksum is there, always check it. It's crazy not
	 * to, unless you can really be sure that your
	 * underlying network (and interfaces and drivers and
	 * DMA hardware, etc!) is error-free. First, fill
	 * in entire virtual ip header. */
#ifndef AFS_64BIT_KERNEL
	tvu->ui_next = 0;
	tvu->ui_prev = 0;
#endif
	tvu->ui_x1 = 0;
	tvu->ui_len = tvu->ui_ulen;
	am->m_flags |= M_PKTHDR;
	am->m_pkthdr.len = tlen;
#if !defined(AFS_AIX51_ENV) || !defined(AFS_64BIT_KERNEL)
	if (in_cksum(am, sizeof(struct ip) + tlen)) {
	    /* checksum, including cksum field, doesn't come out 0, so
	     * this packet is bad */
#ifdef RX_KERNEL_TRACE
	    int glockOwner = ISAFS_GLOCK();
	    if (!glockOwner)
		AFS_GLOCK();
	    afs_Trace3(afs_iclSetp, CM_TRACE_WASHERE, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_INT32,
		       tlen);
	    if (!glockOwner)
		AFS_GUNLOCK();
#endif
	    m_freem(am);
	    return;
	}
#else
#ifdef notdef
	{			/* in_cksum() doesn't work correctly or the length is wrong? */
	    int cksum;
	    int glockOwner = ISAFS_GLOCK();
	    cksum = in_cksum(am, sizeof(struct ip) + tlen);
	    if (!glockOwner)
		AFS_GLOCK();
	    afs_Trace3(afs_iclSetp, CM_TRACE_WASHERE, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_INT32,
		       cksum);
	    if (!glockOwner)
		AFS_GUNLOCK();
	}
#endif
#endif
    }

    /*
     * 28 is IP (20) + UDP (8) header.  ulen includes
     * udp header, and we *don't* tell RX about udp
     * header either.  So, we remove those 8 as well.
     */
    data_len = ntohs(tu->uh_ulen);
    data_len -= 8;
    if (!(*rxk_GetPacketProc) (&phandle, data_len)) {
	if (rx_mb_to_packet(am, m_freem, 28, data_len, phandle)) {
	    /* XXX should just increment counter here.. */
	    printf("rx: truncated UDP packet\n");
	    rxi_FreePacket(phandle);
	} else
	    (*rxk_PacketArrivalProc) (phandle, &taddr, rx_socket, data_len);
    } else
	m_freem(am);
}

/* rxk_isr - RX Kluge Input Service Routine */
static
rxk_isr()
{
    struct mbuf *m;
    IFQ_LOCK_DECL();		/* silly macro has trailing ';'.  Sigh. */
    while (1) {
	IF_DEQUEUE(&rxk_q, m);
	if (!m)
	    return;
	rxk_RX_input(m);
    }
}

/* 
 * UDP fast timer to raise events for all but Solaris and NCR. 
 * Called about 5 times per second (at unknown priority?).  Must go to
 * splnet or obtain global lock before touching anything significant.
 */
static void
rxk_fasttimo(void)
{
    void (*tproc) (void);
    struct clock temp;

    /* do rx fasttimo processing here */
    rxevent_RaiseEvents(&temp);
    if (tproc = parent_proto.pr_fasttimo)
	(*tproc) ();
}


void
rxk_init(void)
{
    struct protosw *pr;
    extern struct protosw *pffindproto();

    if (!rxk_initDone && (pr = pffindproto(AF_INET, IPPROTO_UDP, SOCK_DGRAM))) {
	parent_proto = *pr;

	pr->pr_input = rxk_input;
	pr->pr_fasttimo = rxk_fasttimo;


	/*
	 * don't bother with pr_drain and pr_ctlinput
	 * until we have something to do
	 */
	rxk_q.ifq_maxlen = 128;	/* obligatory XXX       */
	/* add pseudo pkt types as haque to get back onto net kproc */
	if (!add_input_type
	    (0xdead, NET_KPROC, rxk_isr, &rxk_q, NETISR_MAX - 1))
	    rxk_initDone = 1;
    }

    if (!rxk_initDone) {
	printf("\nAFS: no INTERNET protocol support found\n");
    }
}



void
shutdown_rxkernel(void)
{
    struct protosw *pr;
    int i;
    extern struct protosw *pffindproto();

    if (rxk_initDone && (pr = pffindproto(AF_INET, IPPROTO_UDP, SOCK_DGRAM))) {
	*pr = parent_proto;

	rxk_initDone = 0;
	for (i = 0; i < MAXRXPORTS; i++) {
	    if (rxk_ports[i]) {
		rxk_ports[i] = 0;
		soclose((struct socket *)rxk_portRocks[i]);
		rxk_portRocks[i] = NULL;
	    }
	}
	del_input_type(0xdead);
    }
}


/* osi_NetSend - send asize bytes at adata from asocket to host at addr.
 *
 * Now, why do we allocate a new buffer when we could theoretically use the one
 * pointed to by adata?  Because PRU_SEND returns after queueing the message,
 * not after sending it.  If the sender changes the data after queueing it,
 * we'd see the already-queued data change.  One attempt to fix this without
 * adding a copy would be to have this function wait until the datagram is
 * sent; however this doesn't work well.  In particular, if a host is down, and
 * an ARP fails to that host, this packet will be queued until the ARP request
 * comes back, which could be hours later.  We can't block in this routine that
 * long, since it prevents RPC timeouts from happening.
 */
/* XXX In the brave new world, steal the data bufs out of the rx_packet iovec,
 * and just queue those.  XXX
 */

int
osi_NetSend(osi_socket asocket, struct sockaddr_in *addr, struct iovec *dvec,
	    int nvec, afs_int32 asize, int istack)
{
    struct mbuf *tm, *um;
    afs_int32 code;
    struct mbuf *top = 0;
    struct mbuf *m, **mp;
    int len, mlen;
    char *tdata;
    caddr_t tpa;
    int i, tl, rlen;

    AFS_STATCNT(osi_NetSend);
#ifndef	AFS_AIX41_ENV
    /*
     * VRMIX has a version of sun's mclgetx() that works correctly with
     * respect to mcopy(), so we can just dummy up the entire packet as
     * an mbuf cluster, and pass it to the IP output routine (which will
     * most likely have to frag it, but since mclgetx() has been fixed,
     * will work ok).  The only problem is that we have to wait until
     * m_free() has been called on the cluster, to guarantee that we
     * do not muck with it until it has gone out.  We also must refrain
     * from inadvertantly touching a piece of data that falls within the
     * same cache line as any portion of the packet, if we have been lucky
     * enough to be DMA-ing directly out from it.
     * Certain IBM architects assure me that the rios is fast enough
     * that the cost of the extra copy, as opposed to trying to
     * DMA directly from the packet is barely worth my while,
     * but I have a hard time accepting this.
     *
     * We can only use this code once we are passed in an indication of
     * whether we are being called `process-synchronously' or not.
     *
     * of course, the packet must be pinned, which is currently true,
     * but in future may not be.
     */
#endif
    mp = &top;
    i = 0;
    tdata = dvec[0].iov_base;
    tl = dvec[0].iov_len;

    while (1) {
	if (!top) {
	    MGETHDR(m, M_DONTWAIT, MT_DATA);
	    mlen = MHLEN;
	} else {
	    MGET(m, M_DONTWAIT, MT_DATA);
	    mlen = MLEN;
	}
	if (!m) {
	    /* can't get an mbuf, give up */
	    if (top)
		m_freem(top);	/* free mbuf list we're building */
	    return 1;
	}
	if (!top) {
	    m->m_flags |= M_PKTHDR;	/* XXX - temp */
	    m->m_pkthdr.len = 0;
	    m->m_pkthdr.rcvif = NULL;
	}

	/*
	 * WARNING: the `4 * MLEN' is somewhat dubious.  It is better than
	 * `NBPG', which may have no relation to `CLBYTES'.  Also,
	 * `CLBYTES' may be so large that we never use clusters,
	 * resulting in far too many mbufs being used.  It is often
	 * better to briefly use a cluster, even if we are only using a
	 * portion of it.  Since we are on the xmit side, it shouldn't
	 * end up sitting on a queue for a potentially unbounded time
	 * (except perhaps if we are talking to ourself).
	 */
	if (asize >= (MHLEN + 3 * MLEN)) {
	    MCLGET(m, M_DONTWAIT);
	}
	/* now compute usable size */
	if (M_HASCL(m)) {
	    len = MIN(m->m_ext.ext_size, asize);
	} else {
	    len = MIN(mlen, asize);
	}

	tpa = mtod(m, caddr_t);
	*mp = m;
	mp = &m->m_next;
	m->m_len = 0;
	while (len) {
	    rlen = MIN(len, tl);
	    memcpy(tpa, tdata, rlen);
	    asize -= rlen;
	    len -= rlen;
	    tpa += rlen;
	    m->m_len += rlen;
	    top->m_pkthdr.len += rlen;
	    tdata += rlen;
	    tl -= rlen;
	    if (tl <= 0) {
		i++;
		if (i > nvec) {
		    /* shouldn't come here! */
		    asize = 0;	/* so we make progress toward completion */
		    break;
		}
		tdata = dvec[i].iov_base;
		tl = dvec[i].iov_len;
	    }
	}

	if (asize <= 0)
	    break;
    }
    tm = top;

    tm->m_act = NULL;

    /* setup mbuf corresponding to destination address */
    MGETHDR(um, M_DONTWAIT, MT_SONAME);
    if (!um) {
	if (top)
	    m_freem(top);	/* free mbuf chain */
	return 1;
    }
    memcpy(mtod(um, caddr_t), addr, sizeof(*addr));
    um->m_len = sizeof(*addr);
    um->m_pkthdr.len = sizeof(*addr);
    um->m_flags |= M_PKTHDR;

    SOCKET_LOCK(asocket);
    code = (*asocket->so_proto->pr_usrreq) (asocket, PRU_SEND, tm, um, 0);
    SOCKET_UNLOCK(asocket);
    m_free(um);

    return code;
}



#endif /* AFS_AIX41_ENV */
