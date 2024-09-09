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
#include "afs/opr.h"

#include "rx/rx_kcommon.h"
#include "rx/rx_packet.h"
#include "h/tcp-param.h"
/* This must be loaded after proc.h to avoid macro collision with a variable*/
#include "netinet/udp_var.h"




#ifdef RXK_LISTENER_ENV
/* osi_NetReceive
 * OS dependent part of kernel RX listener thread.
 *
 * Arguments:
 *	so      socket to receive on, typically rx_socket
 *	from    pointer to a sockaddr_in. 
 *	iov     array of iovecs to fill in.
 *	iovcnt  how many iovecs there are.
 *	lengthp IN/OUT in: total space available in iovecs. out: size of read.
 *
 * Return
 * 0 if successful
 * error code (such as EINTR) if not
 *
 * Environment
 *	Note that the maximum number of iovecs is 2 + RX_MAXWVECS. This is
 *	so we have a little space to look for packets larger than 
 *	rx_maxReceiveSize.
 */
int rxk_lastSocketError = 0;
int rxk_nSocketErrors = 0;
int rxk_nSignalsCleared = 0;

int
osi_NetReceive(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	       int nvecs, int *alength)
{
    struct uio tuio;
    int code;
    struct mbuf *maddr = NULL;
    struct sockaddr_in *taddr;
    struct iovec tmpvec[RX_MAXWVECS + 2];
    bhv_desc_t bhv;
    BHV_PDATA(&bhv) = (void *)so;

    memset(&tuio, 0, sizeof(tuio));
    memset(&tmpvec, 0, sizeof(tmpvec));

    tuio.uio_iov = tmpvec;
    tuio.uio_iovcnt = nvecs;
    tuio.uio_offset = 0;
    tuio.uio_segflg = AFS_UIOSYS;
    tuio.uio_fmode = 0;
    tuio.uio_resid = *alength;
    tuio.uio_pio = 0;
    tuio.uio_pbuf = 0;

    if (nvecs > RX_MAXWVECS + 2) {
	osi_Panic("Too many (%d) iovecs passed to osi_NetReceive\n", nvecs);
    }
    memcpy(tmpvec, (char *)dvec, (RX_MAXWVECS + 1) * sizeof(struct iovec));
    code = soreceive(&bhv, &maddr, &tuio, NULL, NULL);

    if (code) {
	/* Clear the error before using the socket again. I've tried being nice
	 * and blocking SIGKILL and SIGSTOP from the kernel, but they get
	 * delivered anyway. So, time to be crude and just clear the signals
	 * pending on this thread.
	 */
	if (code == EINTR) {
	    uthread_t *ut = curuthread;
	    int s;
	    s = ut_lock(ut);
	    sigemptyset(&ut->ut_sig);
	    ut->ut_cursig = 0;
	    thread_interrupt_clear(UT_TO_KT(ut), 1);
	    ut_unlock(ut, s);
	    rxk_nSignalsCleared++;
	}
	/* Clear the error before using the socket again. */
	so->so_error = 0;
	rxk_lastSocketError = code;
	rxk_nSocketErrors++;
	if (maddr)
	    m_freem(maddr);
    } else {
	*alength = *alength - tuio.uio_resid;
	if (maddr) {
	    memcpy((char *)addr, (char *)mtod(maddr, struct sockaddr_in *),
		   sizeof(struct sockaddr_in));
	    m_freem(maddr);
	} else {
	    return -1;
	}
    }
    return code;
}
#else /* RXK_LISTENER_ENV */

static struct protosw parent_proto;	/* udp proto switch */

/*
 * RX input, fast timer and initialization routines. 
 */

static void
rxk_input(struct mbuf *am, struct ifnet *aif, struct ipsec *spec)
{
    void (*tproc) ();
    unsigned short *tsp;
    int hdr;
    struct udphdr *tu;
    struct ip *ti;
    struct udpiphdr *tvu;
    int i;
    char *phandle;
    struct sockaddr_in taddr;
    int tlen;
    short port;
    int data_len;

    /* make sure we have base ip and udp headers in first mbuf */
    if (am->m_off > MMAXOFF || am->m_len < 28) {
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
		/* checksum the packet */
		if (hdr > 5) {
		    ip_stripoptions(am, (struct mbuf *)0);	/* get rid of anything we don't need */
		    tu = (struct udphdr *)(((char *)ti) + 20);
		}
		/*
		 * Make mbuf data length reflect UDP length.
		 * If not enough data to reflect UDP length, drop.
		 */
		tvu = (struct udpiphdr *)ti;
		tlen = ntohs((u_short) tvu->ui_ulen);
		if ((int)ti->ip_len != tlen) {
		    if (tlen > (int)ti->ip_len) {
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
		    tvu->ui_next = 0;
		    tvu->ui_prev = 0;
		    tvu->ui_x1 = 0;
		    tvu->ui_len = tvu->ui_ulen;
		    tlen = ntohs((unsigned short)(tvu->ui_ulen));
		    if ((!(am->m_flags & M_CKSUMMED))
			&& in_cksum(am, sizeof(struct ip) + tlen)) {
			/* checksum, including cksum field, doesn't come out 0, so
			 * this packet is bad */
			m_freem(am);
			return;
		    }
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
			(*rxk_PacketArrivalProc) (phandle, &taddr,
						  rxk_portRocks[i], data_len);
		} else
		    m_freem(am);
		return;
	    }
	}
    }

    /* if we get here, try to deliver packet to udp */
    if (tproc = parent_proto.pr_input)
	(*tproc) (am, aif);
    return;
}

/* 
 * UDP fast timer to raise events for all but Solaris and NCR. 
 * Called about 5 times per second (at unknown priority?).  Must go to
 * splnet or obtain global lock before touching anything significant.
 */
static void
rxk_fasttimo(void)
{
    int (*tproc) ();
    struct clock temp;

    /* do rx fasttimo processing here */
    rxevent_RaiseEvents(&temp);
    if (tproc = parent_proto.pr_fasttimo)
	(*tproc) ();
}


/* start intercepting basic calls */
void
rxk_init(void)
{
    struct protosw *tpro, *last;
    if (rxk_initDone)
	return;

    last = inetdomain.dom_protoswNPROTOSW;
    for (tpro = inetdomain.dom_protosw; tpro < last; tpro++) {
	if (tpro->pr_protocol == IPPROTO_UDP) {
	    memcpy(&parent_proto, tpro, sizeof(parent_proto));
	    tpro->pr_input = rxk_input;
	    tpro->pr_fasttimo = rxk_fasttimo;
	    rxk_initDone = 1;
	    return;
	}
    }
    osi_Panic("inet:no udp");
}
#endif /* RXK_LISTENER_ENV */

/*
 * RX IP address routines.
 */

static afs_uint32 myNetAddrs[ADDRSPERSITE];
static int myNetMTUs[ADDRSPERSITE];
static int myNetFlags[ADDRSPERSITE];
static int numMyNetAddrs = 0;

/* This version doesn't even begin to handle iterative requests, but then
 * we don't yet use them anyway. Fix this when rxi_InitPeerParams is changed
 * to find a true maximum.
 */
static int
rxi_MatchIfnet(struct hashbucket *h, caddr_t key, caddr_t arg1, caddr_t arg2)
{
    afs_uint32 ppaddr = *(afs_uint32 *) key;
    int match_value = *(int *)arg1;
    struct in_ifaddr *ifa = (struct in_ifaddr *)h;
    struct sockaddr_in *sin;

    if ((ppaddr & ifa->ia_netmask) == ifa->ia_net) {
	if ((ppaddr & ifa->ia_subnetmask) == ifa->ia_subnet) {
	    sin = IA_SIN(ifa);
	    if (sin->sin_addr.s_addr == ppaddr) {	/* ie, ME!!!  */
		match_value = 4;
		*(struct in_ifaddr **)arg2 = ifa;
	    }
	    if (match_value < 3) {
		*(struct in_ifaddr **)arg2 = ifa;
		match_value = 3;
	    }
	} else {
	    if (match_value < 2) {
		*(struct in_ifaddr **)arg2 = ifa;
		match_value = 2;
	    }
	}
    }
    *(int *)arg1 = match_value;
    return 0;
}


struct ifnet *
rxi_FindIfnet(afs_uint32 addr, afs_uint32 * maskp)
{
    afs_uint32 ppaddr;
    int match_value = 0;
    struct in_ifaddr *ifad;

    if (numMyNetAddrs == 0)
	(void)rxi_GetIFInfo();

    ppaddr = ntohl(addr);
    ifad = (struct in_ifaddr *)&hashinfo_inaddr;

    (void)hash_enum(&hashinfo_inaddr, rxi_MatchIfnet, HTF_INET,
		    (caddr_t) & ppaddr, (caddr_t) & match_value,
		    (caddr_t) & ifad);

    if (match_value) {
	if (maskp)
	    *maskp = ifad->ia_subnetmask;
	return ifad->ia_ifp;
    } else
	return NULL;
}

static int
rxi_EnumGetIfInfo(struct hashbucket *h, caddr_t key, caddr_t arg1,
		  caddr_t arg2)
{
    int different = *(int *)arg1;
    int i = *(int *)arg2;
    struct in_ifaddr *iap = (struct in_ifaddr *)h;
    struct ifnet *ifnp;
    afs_uint32 ifinaddr;
    afs_uint32 rxmtu;

    if (i >= ADDRSPERSITE)
	return 0;

    ifnp = iap->ia_ifp;
    rxmtu = (ifnp->if_mtu - RX_IPUDP_SIZE);
    ifinaddr = ntohl(iap->ia_addr.sin_addr.s_addr);
    if (myNetAddrs[i] != ifinaddr) {
	myNetAddrs[i] = ifinaddr;
	myNetMTUs[i] = rxmtu;
	different++;
	*(int *)arg1 = different;
    }
    rxmtu = rxmtu * rxi_nRecvFrags + ((rxi_nRecvFrags - 1) * UDP_HDR_SIZE);
    if (!rx_IsLoopbackAddr(ifinaddr) && (rxmtu > rx_maxReceiveSize)) {
	rx_maxReceiveSize = opr_min(RX_MAX_PACKET_SIZE, rxmtu);
	rx_maxReceiveSize = opr_min(rx_maxReceiveSize, rx_maxReceiveSizeUser);
    }

    *(int *)arg2 = i + 1;
    return 0;
}

int
rxi_GetIFInfo()
{
    int i = 0;
    int different = 0;

    /* SGI 6.2 does not have a pointer from the ifnet to the list of
     * of addresses (if_addrlist). So it's more efficient to run the
     * in_ifaddr list and use the back pointers to the ifnet struct's.
     */
    (void)hash_enum(&hashinfo_inaddr, rxi_EnumGetIfInfo, HTF_INET, NULL,
		    (caddr_t) & different, (caddr_t) & i);

    rx_maxJumboRecvSize =
	RX_HEADER_SIZE + rxi_nDgramPackets * RX_JUMBOBUFFERSIZE +
	(rxi_nDgramPackets - 1) * RX_JUMBOHEADERSIZE;
    rx_maxJumboRecvSize = opr_max(rx_maxJumboRecvSize, rx_maxReceiveSize);

    return different;
}

/* osi_NetSend - from the now defunct afs_osinet.c */
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef MP
#define _MP_NETLOCKS
#endif

osi_NetSend(asocket, addr, dvec, nvec, asize, istack)
     osi_socket *asocket;
     struct iovec *dvec;
     int nvec;
     afs_int32 asize;
     struct sockaddr_in *addr;
     int istack;
{
    int code;
    struct iovec tvecs[RX_MAXWVECS + 1];
    struct iovec *iovp;
    struct uio tuio;
    struct mbuf *to;
    int i;
    bhv_desc_t bhv;

    memset(&tuio, 0, sizeof(tuio));
    memset(&tvecs, 0, sizeof(tvecs));

    if (nvec > RX_MAXWVECS + 1) {
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvec);
    }
    memcpy((char *)tvecs, (char *)dvec, nvec * sizeof(struct iovec));

    tuio.uio_iov = tvecs;
    tuio.uio_iovcnt = nvec;
    tuio.uio_segflg = UIO_SYSSPACE;
    tuio.uio_offset = 0;
    tuio.uio_sigpipe = 0;
    tuio.uio_pio = 0;
    tuio.uio_pbuf = 0;

    tuio.uio_resid = 0;
    for (i = 0, iovp = tvecs; i < nvec; i++, iovp++)
	tuio.uio_resid += iovp->iov_len;


    to = m_get(M_WAIT, MT_SONAME);
    to->m_len = sizeof(struct sockaddr_in);
    memcpy(mtod(to, caddr_t), (char *)addr, to->m_len);

    BHV_PDATA(&bhv) = (void *)asocket;
    code = sosend(&bhv, to, &tuio, 0, NULL);

    m_freem(to);
    return code;
}
