/* Copyright Transarc Corporation 1998 - All Rights Reserved.
 *
 * rx_kcommon.c - Common kernel RX code for all system types.
 */

#include "../afs/param.h"
#include "../rx/rx_kcommon.h"

#ifdef AFS_HPUX110_ENV
#include "../h/tihdr.h"
#include <xti.h>
#include "../afs/hpux_110.h"
#endif
#include "../afsint/afsint.h"

afs_int32 rxi_Findcbi();
extern  struct interfaceAddr afs_cb_interface;

#ifndef RXK_LISTENER_ENV
int (*rxk_GetPacketProc)(); /* set to packet allocation procedure */
int (*rxk_PacketArrivalProc)();
#endif

rxk_ports_t rxk_ports;
rxk_portRocks_t rxk_portRocks;

int rxk_initDone=0;

/* add a port to the monitored list, port # is in network order */
static int rxk_AddPort(u_short aport, char * arock)
{
    int i;
    unsigned short *tsp, ts;
    int zslot;

    zslot = -1;	    /* look for an empty slot simultaneously */
    for(i=0,tsp=rxk_ports;i<MAXRXPORTS;i++,tsp++) {
	if (((ts = *tsp) == 0) && (zslot == -1))
	    zslot = i;
	if (ts == aport) {
	    return 0;
	}
    }
    /* otherwise allocate a new port slot */
    if (zslot < 0) return E2BIG; /* all full */
    rxk_ports[zslot] = aport;
    rxk_portRocks[zslot] = arock;
    return 0;
}

/* remove as port from the monitored list, port # is in network order */
rxk_DelPort(aport)
u_short aport; {
    register int i;
    register unsigned short *tsp;

    for(i=0,tsp=rxk_ports;i<MAXRXPORTS;i++,tsp++) {
	if (*tsp == aport) {
	    /* found it, adjust ref count and free the port reference if all gone */
	    *tsp = 0;
	    return 0;
	}
    }
    /* otherwise port not found */
    return ENOENT;
}

void rxk_shutdownPorts(void)
{
    int i;
    for (i=0; i<MAXRXPORTS;i++) {
	if (rxk_ports[i]) {
	    rxk_ports[i] = 0;
#if ! defined(AFS_SUN5_ENV) && ! defined(UKERNEL) && ! defined(RXK_LISTENER_ENV)
	    soclose((struct socket *)rxk_portRocks[i]);
#endif
	    rxk_portRocks[i] = (char *)0;
	}
    }
}

osi_socket rxi_GetUDPSocket(port)
    u_short port;
{
    struct osi_socket *sockp;
    sockp = (struct osi_socket *) rxk_NewSocket(port);
    if (sockp == (struct osi_socket *) 0) return OSI_NULLSOCKET;
    rxk_AddPort(port, (char *) sockp);
    return (osi_socket)sockp;
}


void osi_Panic(msg, a1, a2, a3)
char *msg;
{
    if (!msg)
	msg = "Unknown AFS panic";

    printf(msg, a1, a2, a3);
#ifdef AFS_LINUX20_ENV
    *((char*)0xffffffff) = 42;
#else
    panic(msg);
#endif
}

/*
 * osi_utoa() - write the NUL-terminated ASCII decimal form of the given
 * unsigned long value into the given buffer.  Returns 0 on success,
 * and a value less than 0 on failure.  The contents of the buffer is
 * defined only on success.
 */

int
osi_utoa(char *buf, size_t len, unsigned long val)
{
	long k;	/* index of first byte of string value */

	/* we definitely need room for at least one digit and NUL */

	if (len < 2) {
		return -1;
	}

	/* compute the string form from the high end of the buffer */

	buf[len - 1] = '\0';
	for (k = len - 2; k >= 0; k--) {
		buf[k] = val % 10 + '0';
		val /= 10;

		if (val == 0)
			break;
	}

	/* did we finish converting val to string form? */

	if (val != 0) {
		return -2;
	}

	/* this should never happen */

	if (k < 0) {
		return -3;
	}

	/* this should never happen */

	if (k >= len) {
		return -4;
	}

	/* if necessary, relocate string to beginning of buf[] */

	if (k > 0) {

		/*
		 * We need to achieve the effect of calling
		 *
		 * memmove(buf, &buf[k], len - k);
		 *
		 * However, since memmove() is not available in all
		 * kernels, we explicitly do an appropriate copy.
		 */

		char *dst = buf;
		char *src = buf+k;

		while((*dst++ = *src++) != '\0')
			continue;
	}

	return 0;
}

/*
 * osi_AssertFailK() -- used by the osi_Assert() macro.
 *
 * It essentially does
 *
 * osi_Panic("assertion failed: %s, file: %s, line: %d", expr, file, line);
 *
 * Since the kernel version of osi_Panic() only passes its first
 * argument to the native panic(), we construct a single string and hand
 * that to osi_Panic().
 */
void
osi_AssertFailK(const char *expr, const char *file, int line)
{
	static const char msg0[] = "assertion failed: ";
	static const char msg1[] = ", file: ";
	static const char msg2[] = ", line: ";
	static const char msg3[] = "\n";

	/*
	 * These buffers add up to 1K, which is a pleasantly nice round
	 * value, but probably not vital.
	 */
	char buf[1008];
	char linebuf[16];

	/* check line number conversion */

	if (osi_utoa(linebuf, sizeof linebuf, line) < 0) {
		osi_Panic("osi_AssertFailK: error in osi_utoa()\n");
	}

	/* okay, panic */

#define ADDBUF(BUF, STR)					\
	if (strlen(BUF) + strlen((char *)(STR)) + 1 <= sizeof BUF) {	\
		strcat(BUF, (char *)(STR));				\
	}

	buf[0] = '\0';
	ADDBUF(buf, msg0);
	ADDBUF(buf, expr);
	ADDBUF(buf, msg1);
	ADDBUF(buf, file);
	ADDBUF(buf, msg2);
	ADDBUF(buf, linebuf);
	ADDBUF(buf, msg3);

#undef ADDBUF

	osi_Panic(buf);
}

#ifndef UKERNEL
/* This is the server process request loop. Kernel server
 * processes never become listener threads */
void rx_ServerProc()
{
    int threadID;

    rxi_MorePackets(rx_maxReceiveWindow+2); /* alloc more packets */
    rxi_dataQuota += rx_initSendWindow;	/* Reserve some pkts for hard times */
    /* threadID is used for making decisions in GetCall.  Get it by bumping
     * number of threads handling incoming calls */
    threadID = rxi_availProcs++;

#ifdef RX_ENABLE_LOCKS
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
    rxi_ServerProc(threadID, NULL, NULL);
#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
}
#endif /* !UKERNEL */

#ifndef RXK_LISTENER_ENV
static int MyPacketProc(ahandle, asize)
int asize;   /* this includes the Rx header */
char **ahandle;
{
    register struct rx_packet *tp;

    /* If this is larger than we expected, increase rx_maxReceiveDataSize */
    /* If we can't scrounge enough cbufs, then we have to drop the packet,
     * but we should set a flag so we magic up some more at our leisure.
     */

    if ((asize >= 0) && (asize <= RX_MAX_PACKET_SIZE)) {
      tp = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE);
      if (tp && (tp->length + RX_HEADER_SIZE) < asize ) {
	if (0 < rxi_AllocDataBuf(tp, asize - (tp->length + RX_HEADER_SIZE),
				 RX_PACKET_CLASS_RECV_CBUF)) {
	  rxi_FreePacket(tp);
	  tp = NULL;
	  MUTEX_ENTER(&rx_stats_mutex);
	  rx_stats.noPacketBuffersOnRead++;
	  MUTEX_EXIT(&rx_stats_mutex);
	}
      }
    } else {
      /*
       * XXX if packet is too long for our buffer,
       * should do this at a higher layer and let other
       * end know we're losing.
       */
      MUTEX_ENTER(&rx_stats_mutex);
      rx_stats.bogusPacketOnRead++;
      MUTEX_EXIT(&rx_stats_mutex);
      /* I DON"T LIKE THIS PRINTF -- PRINTFS MAKE THINGS VERY VERY SLOOWWW */
      printf("rx: packet dropped: bad ulen=%d\n", asize);
      tp = NULL;
    }

    if (!tp) return -1;
    /* otherwise we have a packet, set appropriate values */
    *ahandle = (char *) tp;
    return 0;
}

static int MyArrivalProc(ahandle, afrom, arock, asize)
register struct rx_packet *ahandle;
register struct sockaddr_in *afrom;
char *arock;
afs_int32 asize; {
    /* handle basic rx packet */
    ahandle->length = asize - RX_HEADER_SIZE;
    rxi_DecodePacketHeader(ahandle);
    ahandle = rxi_ReceivePacket(ahandle, (struct socket *) arock,
				afrom->sin_addr.s_addr, afrom->sin_port,
				NULL, NULL);

    /* free the packet if it has been returned */
    if (ahandle) rxi_FreePacket(ahandle);
    return 0;
}
#endif /* !RXK_LISTENER_ENV */

void
rxi_StartListener() {
    /* if kernel, give name of appropriate procedures */
#ifndef RXK_LISTENER_ENV
    rxk_GetPacketProc = MyPacketProc;
    rxk_PacketArrivalProc = MyArrivalProc;
    rxk_init();
#endif
}

/* Called from rxi_FindPeer, when initializing a clear rx_peer structure,
  to get interesting information. */
void rxi_InitPeerParams(pp)
register struct rx_peer *pp;
{
#ifdef	ADAPT_MTU
    u_short rxmtu;
    afs_int32 i, mtu;

#ifdef AFS_USERSPACE_IP_ADDR	
    i = rxi_Findcbi(pp->host);
    if (i == -1) {
       pp->timeout.sec = 3;
       /* pp->timeout.usec = 0; */
       pp->ifMTU = RX_REMOTE_PACKET_SIZE;
    } else {
       pp->timeout.sec = 2;
       /* pp->timeout.usec = 0; */
       pp->ifMTU = MIN(RX_MAX_PACKET_SIZE, rx_MyMaxSendSize);
    }
    if (i != -1) {
        mtu = ntohl(afs_cb_interface.mtu[i]);
	/* Diminish the packet size to one based on the MTU given by
         * the interface. */
	if (mtu > (RX_IPUDP_SIZE + RX_HEADER_SIZE)) {
	    rxmtu = mtu - RX_IPUDP_SIZE;
	    if (rxmtu < pp->ifMTU) pp->ifMTU = rxmtu;
	}
    }
    else {   /* couldn't find the interface, so assume the worst */
      pp->ifMTU = RX_REMOTE_PACKET_SIZE;
    }
#else /* AFS_USERSPACE_IP_ADDR */
    struct in_ifaddr *ifad = (struct in_ifaddr *) 0;
    struct ifnet *ifn;

    /* At some time we need to iterate through rxi_FindIfnet() to find the
     * global maximum.
     */
    ifn = rxi_FindIfnet(pp->host, &ifad);
    if (ifn == NULL) {	/* not local */
	pp->timeout.sec = 3;
	/* pp->timeout.usec = 0; */
	pp->ifMTU = RX_REMOTE_PACKET_SIZE;
    } else {
	pp->timeout.sec = 2;
	/* pp->timeout.usec = 0; */
	pp->ifMTU = MIN(RX_MAX_PACKET_SIZE, rx_MyMaxSendSize);
    }
    if (ifn) {
#ifdef IFF_POINTOPOINT
	if (ifn->if_flags & IFF_POINTOPOINT) {
	    /* wish we knew the bit rate and the chunk size, sigh. */
	    pp->timeout.sec = 4;
	    pp->ifMTU = RX_PP_PACKET_SIZE;
	}
#endif /* IFF_POINTOPOINT */
	/* Diminish the packet size to one based on the MTU given by
         * the interface. */
	if (ifn->if_mtu > (RX_IPUDP_SIZE + RX_HEADER_SIZE)) {
	    rxmtu = ifn->if_mtu - RX_IPUDP_SIZE;
	    if (rxmtu < pp->ifMTU) pp->ifMTU = rxmtu;
	}
    }
    else {   /* couldn't find the interface, so assume the worst */
      pp->ifMTU = RX_REMOTE_PACKET_SIZE;
    }
#endif/* else AFS_USERSPACE_IP_ADDR */
#else /* ADAPT_MTU */
    pp->rateFlag = 2;   /* start timing after two full packets */
    pp->timeout.sec = 2;
    pp->ifMTU = OLD_MAX_PACKET_SIZE;
#endif /* else ADAPT_MTU */
    pp->ifMTU = rxi_AdjustIfMTU(pp->ifMTU);
    pp->maxMTU = OLD_MAX_PACKET_SIZE;  /* for compatibility with old guys */
    pp->natMTU = MIN(pp->ifMTU, OLD_MAX_PACKET_SIZE); 
    pp->ifDgramPackets = MIN(rxi_nDgramPackets,
			     rxi_AdjustDgramPackets(RX_MAX_FRAGS, pp->ifMTU));
    pp->maxDgramPackets = 1;

    /* Initialize slow start parameters */
    pp->MTU = MIN(pp->natMTU, pp->maxMTU);
    pp->cwind = 1;
    pp->nDgramPackets = 1;
    pp->congestSeq = 0;
}


/* The following code is common to several system types, but not all. The
 * separate ones are found in the system specific subdirectories.
 */


#if ! defined(AFS_AIX_ENV) && ! defined(AFS_SUN5_ENV) && ! defined(UKERNEL) && ! defined(AFS_LINUX20_ENV)
/* Routine called during the afsd "-shutdown" process to put things back to
 * the initial state.
 */
static struct protosw parent_proto;	/* udp proto switch */

void shutdown_rxkernel(void)
{
    register struct protosw *tpro, *last;
    last = inetdomain.dom_protoswNPROTOSW;
    for (tpro = inetdomain.dom_protosw; tpro < last; tpro++)
	if (tpro->pr_protocol == IPPROTO_UDP) {
	    /* restore original udp protocol switch */
	    bcopy((void *)&parent_proto, (void *)tpro, sizeof(parent_proto));
	    bzero((void *)&parent_proto, sizeof(parent_proto));
	    rxk_initDone = 0;
	    rxk_shutdownPorts();
	    return;
	}    
    printf("shutdown_rxkernel: no udp proto");
}
#endif /* !AIX && !SUN && !NCR  && !UKERNEL */

#if !defined(AFS_SUN5_ENV) && !defined(AFS_SGI62_ENV)
/* Determine what the network interfaces are for this machine. */

#define ADDRSPERSITE 16
static afs_uint32 myNetAddrs[ADDRSPERSITE];
static int myNetMTUs[ADDRSPERSITE];
static int myNetFlags[ADDRSPERSITE];
static int numMyNetAddrs = 0;

#ifdef AFS_USERSPACE_IP_ADDR	
int rxi_GetcbiInfo()
{
   int     i, j, different = 0;
   int     rxmtu, maxmtu;
   afs_uint32 ifinaddr;
   afs_uint32 addrs[ADDRSPERSITE];
   int     mtus[ADDRSPERSITE];

   bzero((void *)addrs, sizeof(addrs));
   bzero((void *)mtus,  sizeof(mtus));

   for (i=0; i<afs_cb_interface.numberOfInterfaces; i++) {
      rxmtu    = (ntohl(afs_cb_interface.mtu[i]) - RX_IPUDP_SIZE);
      ifinaddr = ntohl(afs_cb_interface.addr_in[i]);
      if (myNetAddrs[i] != ifinaddr) different++;

      mtus[i]    = rxmtu;
      rxmtu      = rxi_AdjustIfMTU(rxmtu);
      maxmtu     = rxmtu * rxi_nRecvFrags + ((rxi_nRecvFrags-1) * UDP_HDR_SIZE);
      maxmtu     = rxi_AdjustMaxMTU(rxmtu, maxmtu);
      addrs[i++] = ifinaddr;
      if ( ( ifinaddr != 0x7f000001 ) && (maxmtu > rx_maxReceiveSize) ) {
	 rx_maxReceiveSize = MIN( RX_MAX_PACKET_SIZE, maxmtu);
	 rx_maxReceiveSize = MIN( rx_maxReceiveSize, rx_maxReceiveSizeUser);
      }
   }

   rx_maxJumboRecvSize = RX_HEADER_SIZE +
                         ( rxi_nDgramPackets    * RX_JUMBOBUFFERSIZE) +
			 ((rxi_nDgramPackets-1) * RX_JUMBOHEADERSIZE);
   rx_maxJumboRecvSize = MAX(rx_maxJumboRecvSize, rx_maxReceiveSize);

   if (different) {
      for (j=0; j<i; j++) {
	 myNetMTUs[j]  = mtus[j];
	 myNetAddrs[j] = addrs[j];
      }
   }
   return different;
}


/* Returns the afs_cb_interface inxex which best matches address.
 * If none is found, we return -1.
 */
afs_int32 rxi_Findcbi(addr)
     afs_uint32 addr;
{
   int j;
   afs_uint32 myAddr, thisAddr, netMask, subnetMask;
   afs_int32 rvalue = -1;
   int match_value = 0;

  if (numMyNetAddrs == 0)
    (void) rxi_GetcbiInfo();

   myAddr = ntohl(addr);

   if      ( IN_CLASSA(myAddr) ) netMask = IN_CLASSA_NET;
   else if ( IN_CLASSB(myAddr) ) netMask = IN_CLASSB_NET;
   else if ( IN_CLASSC(myAddr) ) netMask = IN_CLASSC_NET;
   else                          netMask = 0;

   for (j=0; j<afs_cb_interface.numberOfInterfaces; j++) {
      thisAddr   = ntohl(afs_cb_interface.addr_in[j]);
      subnetMask = ntohl(afs_cb_interface.subnetmask[j]);
      if ((myAddr & netMask) == (thisAddr & netMask)) {
	 if ((myAddr & subnetMask) == (thisAddr & subnetMask)) {
	    if (myAddr == thisAddr) {
	       match_value = 4;
	       rvalue = j;
	       break;
	    }
	    if (match_value < 3) {
	       match_value = 3;
	       rvalue = j;
	    }
	 } else {
	    if (match_value < 2) {
	       match_value = 2;
	       rvalue = j;
	    }
	 }
      }
   }

 done:
   return(rvalue);
}

#else /* AFS_USERSPACE_IP_ADDR */

#if !defined(AFS_AIX41_ENV) && !defined(AFS_DUX40_ENV)
#define IFADDR2SA(f) (&((f)->ifa_addr))
#else /* AFS_AIX41_ENV */
#define IFADDR2SA(f) ((f)->ifa_addr)
#endif

int rxi_GetIFInfo()
{
    int i = 0;
    int different = 0;

    register struct ifnet *ifn;
    register int rxmtu, maxmtu;
    afs_uint32 addrs[ADDRSPERSITE];
    int mtus[ADDRSPERSITE];
    struct ifaddr *ifad;  /* ifnet points to a if_addrlist of ifaddrs */
    afs_uint32 ifinaddr;

    bzero(addrs, sizeof(addrs));
    bzero(mtus, sizeof(mtus));

    for (ifn = ifnet; ifn != NULL && i < ADDRSPERSITE; ifn = ifn->if_next) {
      rxmtu = (ifn->if_mtu - RX_IPUDP_SIZE);
      for (ifad = ifn->if_addrlist; ifad != NULL && i < ADDRSPERSITE;
	   ifad = ifad->ifa_next){
	if (IFADDR2SA(ifad)->sa_family == AF_INET) {
	  ifinaddr = ntohl(((struct sockaddr_in *) IFADDR2SA(ifad))->sin_addr.s_addr);
	  if (myNetAddrs[i] != ifinaddr) { 
	    different++;
	  }
	  mtus[i] = rxmtu;
	  rxmtu = rxi_AdjustIfMTU(rxmtu);
	  maxmtu = rxmtu * rxi_nRecvFrags + ((rxi_nRecvFrags-1) * UDP_HDR_SIZE);
	  maxmtu = rxi_AdjustMaxMTU(rxmtu, maxmtu);
	  addrs[i++] = ifinaddr;
	  if ( ( ifinaddr != 0x7f000001 ) &&
	      (maxmtu > rx_maxReceiveSize) ) {
	    rx_maxReceiveSize = MIN( RX_MAX_PACKET_SIZE, maxmtu);
	    rx_maxReceiveSize = MIN( rx_maxReceiveSize, rx_maxReceiveSizeUser);
	  }
	}
      }
    }

    rx_maxJumboRecvSize = RX_HEADER_SIZE
			  + rxi_nDgramPackets * RX_JUMBOBUFFERSIZE
			  + (rxi_nDgramPackets-1) * RX_JUMBOHEADERSIZE;
    rx_maxJumboRecvSize = MAX(rx_maxJumboRecvSize, rx_maxReceiveSize);

    if (different) {
      int j;
      for (j=0; j< i; j++) {
	myNetMTUs[j] = mtus[j];
	myNetAddrs[j] = addrs[j];
      }
    }
   return different;
}

/* Returns ifnet which best matches address */
struct ifnet *
rxi_FindIfnet(addr, pifad) 
     afs_uint32 addr;
     struct in_ifaddr **pifad;
{
  afs_uint32 ppaddr;
  int match_value = 0;
  extern struct in_ifaddr *in_ifaddr;
  struct in_ifaddr *ifa;
  struct sockaddr_in *sin;
  
  if (numMyNetAddrs == 0)
    (void) rxi_GetIFInfo();

  ppaddr = ntohl(addr);

  /* if we're given an address, skip everything until we find it */
  if (!*pifad)
    *pifad = in_ifaddr;
  else {
    if (((ppaddr & (*pifad)->ia_subnetmask) == (*pifad)->ia_subnet))
      match_value = 2; /* don't find matching nets, just subnets */
    *pifad = (*pifad)->ia_next;
  }
    
  for (ifa = *pifad; ifa; ifa = ifa->ia_next ) {
    if ((ppaddr & ifa->ia_netmask) == ifa->ia_net) {
      if ((ppaddr & ifa->ia_subnetmask) == ifa->ia_subnet) {
	sin=IA_SIN(ifa);
	if ( sin->sin_addr.s_addr == ppaddr) {   /* ie, ME!!!  */
	  match_value = 4;
	  *pifad = ifa;
	  goto done;
	}
	if (match_value < 3) {
	  *pifad = ifa;
	  match_value = 3;
	}
      }
      else {
	if (match_value < 2) {
	  *pifad = ifa;
	  match_value = 2;
	}
      }
    } /* if net matches */
  } /* for all in_ifaddrs */

 done:
  return (*pifad ?  (*pifad)->ia_ifp : NULL );
}
#endif /* else AFS_USERSPACE_IP_ADDR */
#endif /* !SUN5 && !SGI62 */


/* rxk_NewSocket, rxk_FreeSocket and osi_NetSend are from the now defunct
 * afs_osinet.c. One could argue that rxi_NewSocket could go into the
 * system specific subdirectories for all systems. But for the moment,
 * most of it is simple to follow common code.
 */
#if !defined(UKERNEL)
#if !defined(AFS_SUN5_ENV) && !defined(AFS_LINUX20_ENV)
/* rxk_NewSocket creates a new socket on the specified port. The port is
 * in network byte order.
 */
struct osi_socket *rxk_NewSocket(short aport)
{
    register afs_int32 code;
    struct socket *newSocket;
    register struct mbuf *nam;
    struct sockaddr_in myaddr;
    int wow;
#ifdef AFS_HPUX110_ENV
    /* prototype copied from kernel source file streams/str_proto.h */
    extern MBLKP allocb_wait(int, int);
    MBLKP bindnam;
    int addrsize = sizeof(struct sockaddr_in);
#endif
#ifdef AFS_SGI65_ENV
    bhv_desc_t bhv;
#endif

    AFS_STATCNT(osi_NewSocket);
#if	defined(AFS_HPUX102_ENV)
#if     defined(AFS_HPUX110_ENV)
    /* blocking socket */
    code = socreate(AF_INET, &newSocket, SOCK_DGRAM, 0, 0);
#else      /* AFS_HPUX110_ENV */
    code = socreate(AF_INET, &newSocket, SOCK_DGRAM, 0, SS_NOWAIT);
#endif     /* else AFS_HPUX110_ENV */
#else
#ifdef AFS_SGI65_ENV
    code = socreate(AF_INET, &newSocket, SOCK_DGRAM,IPPROTO_UDP);
#else
    code = socreate(AF_INET, &newSocket, SOCK_DGRAM, 0);
#endif /* AFS_SGI65_ENV */
#endif /* AFS_HPUX102_ENV */
    if (code) return (struct osi_socket *) 0;

    myaddr.sin_family = AF_INET;
    myaddr.sin_port = aport;
    myaddr.sin_addr.s_addr = 0;

#ifdef AFS_HPUX110_ENV
    bindnam = allocb_wait((addrsize+SO_MSGOFFSET+1), BPRI_MED);
    if (!bindnam) {
       setuerror(ENOBUFS);
       return(struct osi_socket *) 0;
    }
    bcopy((caddr_t)&myaddr, (caddr_t)bindnam->b_rptr+SO_MSGOFFSET, addrsize);
    bindnam->b_wptr = bindnam->b_rptr + (addrsize+SO_MSGOFFSET+1);

    code = sobind(newSocket, bindnam, addrsize);
    if (code) {
       soclose(newSocket);
       m_freem(nam);
       return(struct osi_socket *) 0;
    }

    freeb(bindnam);
#else /* AFS_HPUX110_ENV */
    code = soreserve(newSocket, 50000, 50000);
    if (code) {
	code = soreserve(newSocket, 32766, 32766);
	if (code)
	    osi_Panic("osi_NewSocket: last attempt to reserve 32K failed!\n");
    }
#ifdef  AFS_OSF_ENV
    nam = m_getclr(M_WAIT, MT_SONAME);
#else   /* AFS_OSF_ENV */
    nam = m_get(M_WAIT, MT_SONAME);
#endif
    if (nam == NULL) {
#if !defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_SGI64_ENV)
 	setuerror(ENOBUFS);
#endif
	return (struct osi_socket *) 0;
    }
    nam->m_len = sizeof(myaddr);
#ifdef  AFS_OSF_ENV
    myaddr.sin_len = nam->m_len;
#endif  /* AFS_OSF_ENV */
    bcopy(&myaddr, mtod(nam, caddr_t), sizeof(myaddr));
#ifdef AFS_SGI65_ENV
    BHV_PDATA(&bhv) = (void*)newSocket;
    code = sobind(&bhv, nam);
    m_freem(nam);
#else
    code = sobind(newSocket, nam);
#endif
    if (code) {
	soclose(newSocket);
#ifndef AFS_SGI65_ENV
	m_freem(nam);
#endif
	return (struct osi_socket *) 0;
    }
#endif /* else AFS_HPUX110_ENV */

    return (struct osi_socket *) newSocket;
}


/* free socket allocated by rxk_NewSocket */
int rxk_FreeSocket(asocket)
    register struct socket *asocket;
{
    AFS_STATCNT(osi_FreeSocket);
    soclose(asocket);
    return 0;
}
#endif /* !SUN5 && !LINUX20 */

#if defined(RXK_LISTENER_ENV) || defined(AFS_SUN5_ENV)
/*
 * Run RX event daemon every second (5 times faster than rest of systems)
 */
afs_rxevent_daemon() 
{
    int s, code;
    struct clock temp;
    SPLVAR;

    while (1) {
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	NETPRI;
	AFS_RXGLOCK();
	rxevent_RaiseEvents(&temp);
	AFS_RXGUNLOCK();
	USERPRI;
#ifdef RX_ENABLE_LOCKS
	AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	afs_osi_Wait(500, (char *)0, 0);
	if (afs_termState == AFSOP_STOP_RXEVENT )
	{
#ifdef RXK_LISTENER_ENV
		afs_termState = AFSOP_STOP_RXK_LISTENER;
#else
		afs_termState = AFSOP_STOP_COMPLETE;
#endif
		afs_osi_Wakeup(&afs_termState);
		return;
	}
    }
}
#endif

#ifdef RXK_LISTENER_ENV 

/* rxk_ReadPacket returns 1 if valid packet, 0 on error. */
int rxk_ReadPacket(osi_socket so, struct rx_packet *p, int *host, int *port)
{
    int code;
    struct sockaddr_in from;
    int nbytes;
    afs_int32 rlen;
    register afs_int32 tlen;
    afs_int32 savelen;            /* was using rlen but had aliasing problems */
    rx_computelen(p, tlen);
    rx_SetDataSize(p, tlen);  /* this is the size of the user data area */

    tlen += RX_HEADER_SIZE;   /* now this is the size of the entire packet */
    rlen = rx_maxJumboRecvSize; /* this is what I am advertising.  Only check
			         * it once in order to avoid races.  */
    tlen = rlen - tlen;
    if (tlen > 0) {
      tlen = rxi_AllocDataBuf(p, tlen, RX_PACKET_CLASS_RECV_CBUF);
      if (tlen >0) {
	tlen = rlen - tlen;
      }
      else tlen = rlen;
    }
    else tlen = rlen;

   /* add some padding to the last iovec, it's just to make sure that the 
    * read doesn't return more data than we expect, and is done to get around
    * our problems caused by the lack of a length field in the rx header. */
    savelen = p->wirevec[p->niovecs-1].iov_len;
    p->wirevec[p->niovecs-1].iov_len = savelen + RX_EXTRABUFFERSIZE;

    nbytes = tlen + sizeof(afs_int32);
    code = osi_NetReceive(rx_socket, &from, p->wirevec, p->niovecs,
			    &nbytes);

   /* restore the vec to its correct state */
    p->wirevec[p->niovecs-1].iov_len = savelen;

    if (!code) {
	p->length = nbytes - RX_HEADER_SIZE;;
	if ((nbytes > tlen) || (p->length & 0x8000)) {  /* Bogus packet */
	    if (nbytes > 0)
		rxi_MorePackets(rx_initSendWindow);
	    else {
	        MUTEX_ENTER(&rx_stats_mutex);
		rx_stats.bogusPacketOnRead++;
		rx_stats.bogusHost = from.sin_addr.s_addr;
	        MUTEX_EXIT(&rx_stats_mutex);
		dpf(("B: bogus packet from [%x,%d] nb=%d", from.sin_addr.s_addr,
		     from.sin_port,nbytes));
	    }
	    return  -1;
	}
	else {
	    /* Extract packet header. */
	    rxi_DecodePacketHeader(p);
      
	    *host = from.sin_addr.s_addr;
	    *port = from.sin_port;
	    if (p->header.type > 0 && p->header.type < RX_N_PACKET_TYPES) {
	        MUTEX_ENTER(&rx_stats_mutex);
		rx_stats.packetsRead[p->header.type-1]++;
	        MUTEX_EXIT(&rx_stats_mutex);
	    }

	    /* Free any empty packet buffers at the end of this packet */
	    rxi_TrimDataBufs(p, 1);

	    return  0;
	}
    }
    else
	return code;
}

/* rxk_Listener() 
 *
 * Listen for packets on socket. This thread is typically started after
 * rx_Init has called rxi_StartListener(), but nevertheless, ensures that
 * the start state is set before proceeding.
 *
 * Note that this thread is outside the AFS global lock for much of
 * it's existence.
 *
 * In many OS's, the socket receive code sleeps interruptibly. That's not what
 * we want here. So we need to either block all signals (including SIGKILL
 * and SIGSTOP) or reset the thread's signal state to unsignalled when the
 * OS's socket receive routine returns as a result of a signal.
 */
int rxk_ListenerPid; /* Used to signal process to wakeup at shutdown */

#ifdef AFS_SUN5_ENV
/*
 * Run the listener as a kernel process.
 */
void rxk_Listener(void)
{
    extern id_t syscid;
    void rxk_ListenerProc(void);
    if (newproc(rxk_ListenerProc, syscid, 59))
	osi_Panic("rxk_Listener: failed to fork listener process!\n");
}

void rxk_ListenerProc(void)
#else /* AFS_SUN5_ENV */
void rxk_Listener(void)
#endif /* AFS_SUN5_ENV */
{
    struct rx_packet *rxp = NULL;
    int code;
    int host, port;

#ifdef AFS_LINUX20_ENV
    rxk_ListenerPid = current->pid;
#endif
#ifdef AFS_SUN5_ENV
    rxk_ListenerPid = ttoproc(curthread)->p_pidp->pid_id;
#endif /* AFS_SUN5_ENV */
#if defined(RX_ENABLE_LOCKS) && !defined(AFS_SUN5_ENV)
    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS && !AFS_SUN5_ENV */

    while (afs_termState != AFSOP_STOP_RXK_LISTENER) {
	if (rxp) {
	    rxi_RestoreDataBufs(rxp);
	}
	else {
	    rxp = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE);
	    if (!rxp)
		osi_Panic("rxk_Listener: No more Rx buffers!\n");
	}
	if (!(code = rxk_ReadPacket(rx_socket, rxp, &host, &port))) {
	    AFS_RXGLOCK();
	    rxp = rxi_ReceivePacket(rxp, rx_socket, host, port);
	    AFS_RXGUNLOCK();
	}
	if (afs_termState == AFSOP_STOP_RXK_LISTENER)
	    break;

    }

#ifdef RX_ENABLE_LOCKS
    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
    if (afs_termState == AFSOP_STOP_RXK_LISTENER) {
	afs_termState = AFSOP_STOP_COMPLETE;
	afs_osi_Wakeup(&afs_termState);
    }
    rxk_ListenerPid = 0;
#ifdef AFS_SUN5_ENV
    AFS_GUNLOCK();
#endif /* AFS_SUN5_ENV */
}

#if !defined(AFS_LINUX20_ENV) && !defined(AFS_SUN5_ENV)
/* The manner of stopping the rx listener thread may vary. Most unix's should
 * be able to call soclose.
 */
void osi_StopListener(void)
{
    soclose(rx_socket);
}
#endif
#endif /* RXK_LISTENER_ENV */

#endif /* !NCR && !UKERNEL */
