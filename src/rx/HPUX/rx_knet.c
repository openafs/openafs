/* This file is bootstrapped from the RX release cleared by Transarc legal
   in 1994, available from ftp.dementia.org/pub/rx 
 */

#include "../afs/param.h"
#include "../h/types.h"
#include "../h/param.h"
#include "../rx/rx_kcommon.h"
#include "../h/user.h"
#include "../h/tihdr.h"
#include <xti.h>

/* Define this here, used externally */
#ifdef RX_ENABLE_LOCKS
lock_t *rx_sleepLock;
#endif

/* rx_kern.c */
/*
 * XXX The following is needed since the include file that contains them (<net/cko.h>)
 * isn't part of a binary distribution!! XXXX
 */

/* CKO_PSEUDO : Account for pseudo_header in the checksum 		*/
#ifndef	CKO_SUM_MASK
#define	CKO_SUM_MASK	    0xffff	/* sum is in lower 16 bits 	*/
#endif

#ifndef	CKO_PSEUDO
#define CKO_PSEUDO(sum, m, hdr) {					\
	(sum) = 0xffff ^ in_3sum( ((struct ipovly *)(hdr))->ih_src.s_addr, \
		((struct ipovly *)(hdr))->ih_dst.s_addr, 		\
		((struct ipovly *)(hdr))->ih_len 			\
		+ (((struct ipovly *)(hdr))->ih_pr<<16) 		\
		+ ((m)->m_quad[MQ_CKO_IN] & CKO_SUM_MASK));		\
}
#endif

static struct protosw parent_proto;	/* udp proto switch */

/* rx_kern.c */
static int rxk_fasttimo (void)
{
    int code;
    int (*tproc)();
    struct clock temp;

    SPLVAR;
    NETPRI;
    /* do rx fasttimo processing here */
    rxevent_RaiseEvents(&temp);
    USERPRI; /* should this be after the call to tproc? */
    if (tproc = parent_proto.pr_fasttimo) code = (*tproc)();
    else code = 0;
    return code;
}

/* hybrid of IRIX/knet.c and rx_kern.c */
/* XXX should this be listener or !listener? */
#if !defined(RXK_LISTENER_ENV)
/* start intercepting basic calls */
int
rxk_init() {
    register struct protosw *tpro, *last;
    if (rxk_initDone) return 0;

    last = inetdomain.dom_protoswNPROTOSW;
    for (tpro = inetdomain.dom_protosw; tpro < last; tpro++) {
	if (tpro->pr_protocol == IPPROTO_UDP) {
	    /* force UDP checksumming on for AFS        */
	    int udpcksum;
	    udpcksum = 1;
	    bcopy(tpro, &parent_proto, sizeof(parent_proto));
	    tpro->pr_input = rxk_input;
	    tpro->pr_fasttimo = rxk_fasttimo;
	    rxk_initDone = 1;
	    return 0;
	}
    }
    osi_Panic("inet:no udp");
}

/* basic packet handling routine called at splnet from softnet loop */
static struct mbuf *rxk_input (register struct mbuf *am, struct ifnet *aif)
{
    int (*tproc)();
    register unsigned short *tsp;
    int hdr;
    struct udphdr *tu;
    register struct ip *ti;
    struct udpiphdr *tvu;
    register int i;
    char *phandle;
    afs_int32 code;
    struct sockaddr_in taddr;
    int tlen;
    short port;
    int data_len;
    unsigned int comp_sum;

    SPLVAR;
    NETPRI;
    /* make sure we have base ip and udp headers in first mbuf */
    if (am->m_off > MMAXOFF || am->m_len < 28) {
	am = m_pullup(am, 28);
	USERPRI;
	if (!am) return (struct mbuf *)0;
    }
    hdr = (mtod(am, struct ip *))->ip_hl;
    if (hdr > 5) {
	/* pull up more, the IP hdr is bigger than usual */
	if (am->m_len < (8 + (hdr<<2))) {
	    am = m_pullup(am, 8+(hdr<<2));
	    USERPRI;
	    if (!am) return (struct mbuf *)0;
	}
	ti = mtod(am, struct ip *); /* recompute, since m_pullup allocates new mbuf */
	tu = (struct udphdr *)(((char *)ti) + (hdr<<2)); /* skip ip hdr */
    }
    else {
	ti = mtod(am, struct ip *);
	tu = (struct udphdr *)(((char *)ti) + 20);      /* skip basic ip hdr */
    }
    /* now read the port out */
    port = tu->uh_dport;
    
    if (port) {
	for(tsp=ports, i=0; i<MAXRXPORTS;i++) {
	    if (*tsp++ == port) {
		/* checksum the packet */
		ip_stripoptions(ti,     (struct mbuf *) 0); /* get rid of anything we don't need */
		/* deliver packet to rx */
		taddr.sin_family = AF_INET;         /* compute source address */
		taddr.sin_port = tu->uh_sport;
		taddr.sin_addr.s_addr = ti->ip_src.s_addr;
		
		/* handle the checksum.  Note that this code damages the actual ip
		   header (replacing it with the virtual one, which is the same 
		   size), so we must ensure we get everything out we need, first */
		tvu = (struct udpiphdr *) ti;   /* virtual udp structure, for ck
						   sum */
		if ( tu->uh_sum != 0) {
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
		    /* HP: checksum assist for cksum offloading drivers */
		    if (am->m_flags & MF_CKO_IN) {
			CKO_PSEUDO(comp_sum, am, tvu);
		    } else {
			struct mbuf *m1;
			
			comp_sum = in_cksum(am, sizeof(struct ip) + tlen);
			for (m1 = am; m1; m1 = m1->m_next)
			    m1->m_flags &= ~MF_NOACC;
		    }
		    if (comp_sum) {
			/* checksum, including cksum field, doesn't come out 0, so
			   this packet is bad */
			m_freem(am);
			USERPRI;
			return((struct mbuf *)0);
		    }
		}
		/*
		 * 28 is IP (20) + UDP (8) header.  ulen includes
		 * udp header, and we *don't* tell RX about udp
		 * header either.  So, we remove those 8 as well.
		 */
		data_len = ntohs(tu->uh_ulen);
		data_len -= 8;
                  
		if (!(*rxk_GetPacketProc)(&phandle, data_len)) {
		    if (rx_mb_to_packet(am, m_freem, 28, data_len, phandle)) {
			/* XXX should just increment counter here.. */
			printf("rx: truncated UDP packet\n");
			rxi_FreePacket(phandle);
		    }
		    else 
			(*rxk_PacketArrivalProc)(phandle, &taddr,
						 portRocks[i], data_len);
		}
		else m_freem(am);
		USERPRI;
		return((struct mbuf *)0);
	    }
	}
    }

    /* if we get here, try to deliver packet to udp */
    if (tproc = parent_proto.pr_input) {
      code = (*tproc)(am,aif);
      USERPRI;
      return code;
    }
    USERPRI;
    return((struct mbuf *)0);
}
#endif /* ! RXK_LISTENER_ENV */

/* steal decl from sgi_65 */
int
osi_NetSend(asocket, addr, dvec, nvec, asize, istack)
     register struct socket *asocket;
     struct iovec *dvec;
     int nvec;
     register afs_int32 asize;
     struct sockaddr_in *addr;
     int istack;
{
    struct uio uio;
    MBLKP bp;
    struct iovec temp[RX_MAXWVECS];
    int code;
    int size = sizeof(struct sockaddr_in);

    /* Guess based on rxk_NewSocket */
    bp = allocb((size+SO_MSGOFFSET+1), BPRI_MED);
    if (!bp) return ENOBUFS;        
    bcopy((caddr_t)addr, (caddr_t)bp->b_rptr+SO_MSGOFFSET, size);
    bp->b_wptr = bp->b_rptr + (size+SO_MSGOFFSET+1);

    bcopy((caddr_t)dvec, (caddr_t)temp, nvec * sizeof(struct iovec));
    
    /* devresource.hp.com/Drivers/Docs/Refs/11i/ddRef/Chap02R.pdf has details
       on use of uio */
    bzero((caddr_t)&uio, sizeof(uio));
    uio.uio_resid   = asize;
    uio.uio_iov     = temp;         
    uio.uio_iovcnt  = nvec;
    uio.uio_seg     = UIOSEG_KERNEL;

    code = sosend(asocket, bp, &uio, 0, 0, 0, size);
    return code;
}

/* pattern from IRIX */
#if defined(RXK_LISTENER_ENV)
int osi_NetReceive(struct socket *so, struct sockaddr_in *from, 
                   struct iovec *iov, int iovcnt, int *lengthp)
{
    int          code;
    struct uio   tuio;
    struct iovec tmpvec[RX_MAXWVECS+2];
    int          flags = 0;
    MBLKP bp, sp;

    if (iovcnt > RX_MAXWVECS+2) {
        osi_Panic("Too many (%d) iovecs passed to osi_NetReceive\n", iovcnt);
    }
    bcopy((char*)iov, tmpvec, iovcnt/*(RX_MAXWVECS+1)*/ * sizeof(struct iovec));
    tuio.uio_iov     = tmpvec;
    tuio.uio_iovcnt  = iovcnt;
    tuio.uio_fpflags = 0;
    tuio.uio_offset  = 0;
    tuio.uio_seg     = UIOSEG_KERNEL;
    tuio.uio_resid   = *lengthp;

    code = soreceive(so, &bp, &tuio, &flags, &sp, (MBLKPP)NULL);
    if (!code) {
        *lengthp = *lengthp - tuio.uio_resid;
        if (bp) {
            bcopy((char*)bp->b_rptr, (char*)from, sizeof(struct sockaddr_in));
        } else {
            code = -1;
        }
    } else {
        so->so_error = 0;
    }

    if (bp) freeb(bp); 
    if (sp) freeb(sp);
    return code;
}
#endif
