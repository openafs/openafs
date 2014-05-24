/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* rx_user.c contains routines specific to the user space UNIX implementation of rx */

/* rxi_syscall is currently not prototyped */

#include <afsconfig.h>
#include <afs/param.h>


# include <sys/types.h>
# include <errno.h>
# include <signal.h>
# include <string.h>
#ifdef AFS_NT40_ENV
# include <WINNT/syscfg.h>
#else
# include <sys/socket.h>
# include <sys/file.h>
# include <netdb.h>
# include <sys/stat.h>
# include <netinet/in.h>
# include <sys/time.h>
# include <net/if.h>
# include <sys/ioctl.h>
# include <unistd.h>
#endif
# include <fcntl.h>
#if !defined(AFS_AIX_ENV) && !defined(AFS_NT40_ENV)
# include <sys/syscall.h>
#endif
#include <afs/afs_args.h>
#include <afs/afsutil.h>

#ifndef	IPPORT_USERRESERVED
/* If in.h doesn't define this, define it anyway.  Unfortunately, defining
   this doesn't put the code into the kernel to restrict kernel assigned
   port numbers to numbers below IPPORT_USERRESERVED...  */
#define IPPORT_USERRESERVED 5000
# endif

#if defined(HAVE_LINUX_ERRQUEUE_H) && defined(ADAPT_PMTU)
#include <linux/types.h>
#include <linux/errqueue.h>
#ifndef IP_MTU
#define IP_MTU 14
#endif
#endif

#ifndef AFS_NT40_ENV
# include <sys/time.h>
#endif
# include "rx.h"
# include "rx_globals.h"

#ifdef AFS_PTHREAD_ENV

/*
 * The rx_if_init_mutex mutex protects the following global variables:
 * Inited
 */

afs_kmutex_t rx_if_init_mutex;
#define LOCK_IF_INIT MUTEX_ENTER(&rx_if_init_mutex)
#define UNLOCK_IF_INIT MUTEX_EXIT(&rx_if_init_mutex)

/*
 * The rx_if_mutex mutex protects the following global variables:
 * myNetFlags
 * myNetMTUs
 * myNetMasks
 */

afs_kmutex_t rx_if_mutex;
#define LOCK_IF MUTEX_ENTER(&rx_if_mutex)
#define UNLOCK_IF MUTEX_EXIT(&rx_if_mutex)
#else
#define LOCK_IF_INIT
#define UNLOCK_IF_INIT
#define LOCK_IF
#define UNLOCK_IF
#endif /* AFS_PTHREAD_ENV */


/*
 * Make a socket for receiving/sending IP packets.  Set it into non-blocking
 * and large buffering modes.  If port isn't specified, the kernel will pick
 * one.  Returns the socket (>= 0) on success.  Returns OSI_NULLSOCKET on
 * failure. Port must be in network byte order.
 */
osi_socket
rxi_GetHostUDPSocket(u_int ahost, u_short port)
{
    int binds, code = 0;
    osi_socket socketFd = OSI_NULLSOCKET;
    struct sockaddr_in taddr;
    char *name = "rxi_GetUDPSocket: ";
#ifdef AFS_LINUX22_ENV
#if defined(ADAPT_PMTU)
    int pmtu=IP_PMTUDISC_WANT;
    int recverr=1;
#else
    int pmtu=IP_PMTUDISC_DONT;
#endif
#endif

#if !defined(AFS_NT40_ENV)
    if (ntohs(port) >= IPPORT_RESERVED && ntohs(port) < IPPORT_USERRESERVED) {
/*	(osi_Msg "%s*WARNING* port number %d is not a reserved port number.  Use port numbers above %d\n", name, port, IPPORT_USERRESERVED);
*/ ;
    }
    if (ntohs(port) > 0 && ntohs(port) < IPPORT_RESERVED && geteuid() != 0) {
	(osi_Msg
	 "%sport number %d is a reserved port number which may only be used by root.  Use port numbers above %d\n",
	 name, ntohs(port), IPPORT_USERRESERVED);
	goto error;
    }
#endif
    socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (socketFd == OSI_NULLSOCKET) {
#ifdef AFS_NT40_ENV
        fprintf(stderr, "socket() failed with error %u\n", WSAGetLastError());
#else
	perror("socket");
#endif
	goto error;
    }

#ifdef AFS_NT40_ENV
    rxi_xmit_init(socketFd);
#endif /* AFS_NT40_ENV */

    taddr.sin_addr.s_addr = ahost;
    taddr.sin_family = AF_INET;
    taddr.sin_port = (u_short) port;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    taddr.sin_len = sizeof(struct sockaddr_in);
#endif
#define MAX_RX_BINDS 10
    for (binds = 0; binds < MAX_RX_BINDS; binds++) {
	if (binds)
	    rxi_Delay(10);
	code = bind(socketFd, (struct sockaddr *)&taddr, sizeof(taddr));
        break;
    }
    if (code) {
	(osi_Msg "%sbind failed\n", name);
	goto error;
    }
#if !defined(AFS_NT40_ENV)
    /*
     * Set close-on-exec on rx socket
     */
    fcntl(socketFd, F_SETFD, 1);
#endif

    /* Use one of three different ways of getting a socket buffer expanded to
     * a reasonable size.
     */
    {
	int greedy = 0;
	int len1, len2;

	len1 = 32766;
	len2 = rx_UdpBufSize;

        /* find the size closest to rx_UdpBufSize that will be accepted */
        while (!greedy && len2 > len1) {
            greedy =
                (setsockopt
                  (socketFd, SOL_SOCKET, SO_RCVBUF, (char *)&len2,
                   sizeof(len2)) >= 0);
            if (!greedy)
                len2 /= 2;
        }

        /* but do not let it get smaller than 32K */
        if (len2 < len1)
            len2 = len1;

        if (len1 < len2)
            len1 = len2;


        greedy =
	    (setsockopt
	     (socketFd, SOL_SOCKET, SO_SNDBUF, (char *)&len1,
	      sizeof(len1)) >= 0)
	    &&
	    (setsockopt
	     (socketFd, SOL_SOCKET, SO_RCVBUF, (char *)&len2,
	      sizeof(len2)) >= 0);
	if (!greedy)
	    (osi_Msg "%s*WARNING* Unable to increase buffering on socket\n",
	     name);
        if (rx_stats_active) {
            MUTEX_ENTER(&rx_stats_mutex);
            rx_stats.socketGreedy = greedy;
            MUTEX_EXIT(&rx_stats_mutex);
        }
    }

#ifdef AFS_LINUX22_ENV
    setsockopt(socketFd, SOL_IP, IP_MTU_DISCOVER, &pmtu, sizeof(pmtu));
#if defined(ADAPT_PMTU)
    setsockopt(socketFd, SOL_IP, IP_RECVERR, &recverr, sizeof(recverr));
#endif
#endif
    if (rxi_Listen(socketFd) < 0) {
	goto error;
    }

    return socketFd;

  error:
#ifdef AFS_NT40_ENV
    if (socketFd != OSI_NULLSOCKET)
	closesocket(socketFd);
#else
    if (socketFd != OSI_NULLSOCKET)
	close(socketFd);
#endif

    return OSI_NULLSOCKET;
}

osi_socket
rxi_GetUDPSocket(u_short port)
{
    return rxi_GetHostUDPSocket(htonl(INADDR_ANY), port);
}

void
osi_Panic(char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    (osi_Msg "Fatal Rx error: ");
    (osi_VMsg msg, ap);
    va_end(ap);
    fflush(stderr);
    fflush(stdout);
    afs_abort();
}

/*
 * osi_AssertFailU() -- used by the osi_Assert() macro.
 */

void
osi_AssertFailU(const char *expr, const char *file, int line)
{
    osi_Panic("assertion failed: %s, file: %s, line: %d\n", expr,
	      file, line);
}

#if defined(AFS_AIX32_ENV) && !defined(KERNEL)
#ifndef osi_Alloc
static const char memZero;
void *
osi_Alloc(afs_int32 x)
{
    /*
     * 0-length allocs may return NULL ptr from malloc, so we special-case
     * things so that NULL returned iff an error occurred
     */
    if (x == 0)
	return (void *)&memZero;
    return(malloc(x));
}

void
osi_Free(void *x, afs_int32 size)
{
    if (x == &memZero)
	return;
    free(x);
}
#endif
#endif /* defined(AFS_AIX32_ENV) && !defined(KERNEL) */

#define	ADDRSPERSITE	16


static afs_uint32 rxi_NetAddrs[ADDRSPERSITE];	/* host order */
static int myNetMTUs[ADDRSPERSITE];
static int myNetMasks[ADDRSPERSITE];
static int myNetFlags[ADDRSPERSITE];
static u_int rxi_numNetAddrs;
static int Inited = 0;

#if defined(AFS_NT40_ENV)
int
rxi_getaddr(void)
{
    /* The IP address list can change so we must query for it */
    rx_GetIFInfo();

    /* we don't want to use the loopback adapter which is first */
    /* this is a bad bad hack */
    if (rxi_numNetAddrs > 1)
	return htonl(rxi_NetAddrs[1]);
    else if (rxi_numNetAddrs > 0)
	return htonl(rxi_NetAddrs[0]);
    else
	return 0;
}

/*
** return number of addresses
** and the addresses themselves in the buffer
** maxSize - max number of interfaces to return.
*/
int
rx_getAllAddr(afs_uint32 * buffer, int maxSize)
{
    int count = 0, offset = 0;

    /* The IP address list can change so we must query for it */
    rx_GetIFInfo();

    for (count = 0; offset < rxi_numNetAddrs && maxSize > 0;
	 count++, offset++, maxSize--)
	buffer[count] = htonl(rxi_NetAddrs[offset]);

    return count;
}

/* this function returns the total number of interface addresses
 * the buffer has to be passed in by the caller. It also returns
 * the matching interface mask and mtu.  All values are returned
 * in network byte order.
 */
int
rx_getAllAddrMaskMtu(afs_uint32 addrBuffer[], afs_uint32 maskBuffer[],
                     afs_uint32 mtuBuffer[], int maxSize)
{
    int count = 0, offset = 0;

    /* The IP address list can change so we must query for it */
    rx_GetIFInfo();

    for (count = 0;
         offset < rxi_numNetAddrs && maxSize > 0;
         count++, offset++, maxSize--) {
	addrBuffer[count] = htonl(rxi_NetAddrs[offset]);
	maskBuffer[count] = htonl(myNetMasks[offset]);
	mtuBuffer[count]  = htonl(myNetMTUs[offset]);
    }
    return count;
}
#endif

#ifdef AFS_NT40_ENV
extern int rxinit_status;
void
rxi_InitMorePackets(void) {
    int npackets, ncbufs;

    ncbufs = (rx_maxJumboRecvSize - RX_FIRSTBUFFERSIZE);
    if (ncbufs > 0) {
        ncbufs = ncbufs / RX_CBUFFERSIZE;
        npackets = rx_initSendWindow - 1;
        rxi_MorePackets(npackets * (ncbufs + 1));
    }
}
void
rx_GetIFInfo(void)
{
    u_int maxsize;
    u_int rxsize;
    afs_uint32 i;

    LOCK_IF_INIT;
    if (Inited) {
        if (Inited < 2 && rxinit_status == 0) {
            /* We couldn't initialize more packets earlier.
             * Do it now. */
            rxi_InitMorePackets();
            Inited = 2;
        }
        UNLOCK_IF_INIT;
	return;
    }
    Inited = 1;
    UNLOCK_IF_INIT;

    LOCK_IF;
    rxi_numNetAddrs = ADDRSPERSITE;
    (void)syscfg_GetIFInfo(&rxi_numNetAddrs, rxi_NetAddrs,
                           myNetMasks, myNetMTUs, myNetFlags);

    for (i = 0; i < rxi_numNetAddrs; i++) {
        rxsize = rxi_AdjustIfMTU(myNetMTUs[i] - RX_IPUDP_SIZE);
        maxsize =
            rxi_nRecvFrags * rxsize + (rxi_nRecvFrags - 1) * UDP_HDR_SIZE;
        maxsize = rxi_AdjustMaxMTU(rxsize, maxsize);
        if (rx_maxReceiveSize > maxsize) {
            rx_maxReceiveSize = MIN(RX_MAX_PACKET_SIZE, maxsize);
            rx_maxReceiveSize =
                MIN(rx_maxReceiveSize, rx_maxReceiveSizeUser);
        }
        if (rx_MyMaxSendSize > maxsize) {
            rx_MyMaxSendSize = MIN(RX_MAX_PACKET_SIZE, maxsize);
        }
    }
    UNLOCK_IF;

    /*
     * If rxinit_status is still set, rx_InitHost() has yet to be called
     * and we therefore do not have any mutex locks initialized.  As a
     * result we cannot call rxi_MorePackets() without crashing.
     */
    if (rxinit_status)
        return;

    rxi_InitMorePackets();
}
#endif

static afs_uint32
fudge_netmask(afs_uint32 addr)
{
    afs_uint32 msk;

    if (IN_CLASSA(addr))
	msk = IN_CLASSA_NET;
    else if (IN_CLASSB(addr))
	msk = IN_CLASSB_NET;
    else if (IN_CLASSC(addr))
	msk = IN_CLASSC_NET;
    else
	msk = 0;

    return msk;
}



#if !defined(AFS_AIX_ENV) && !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
int
rxi_syscall(afs_uint32 a3, afs_uint32 a4, void *a5)
{
    afs_uint32 rcode;
    void (*old) (int);

    old = signal(SIGSYS, SIG_IGN);

#if defined(AFS_SGI_ENV)
    rcode = afs_syscall(AFS_SYSCALL, 28, a3, a4, a5);
#else
    rcode = syscall(AFS_SYSCALL, 28 /* AFSCALL_CALL */ , a3, a4, a5);
#endif /* AFS_SGI_ENV */

    signal(SIGSYS, old);

    return rcode;
}
#endif /* AFS_AIX_ENV */

#ifndef AFS_NT40_ENV
void
rx_GetIFInfo(void)
{
    int s;
    int i, j, len, res;
    struct ifconf ifc;
    struct ifreq ifs[ADDRSPERSITE];
    struct ifreq *ifr;
#ifdef	AFS_AIX41_ENV
    char buf[BUFSIZ], *cp, *cplim;
#endif
    struct sockaddr_in *a;

    LOCK_IF_INIT;
    if (Inited) {
	UNLOCK_IF_INIT;
	return;
    }
    Inited = 1;
    UNLOCK_IF_INIT;
    LOCK_IF;
    rxi_numNetAddrs = 0;
    memset(rxi_NetAddrs, 0, sizeof(rxi_NetAddrs));
    memset(myNetFlags, 0, sizeof(myNetFlags));
    memset(myNetMTUs, 0, sizeof(myNetMTUs));
    memset(myNetMasks, 0, sizeof(myNetMasks));
    UNLOCK_IF;
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == OSI_NULLSOCKET)
	return;
#ifdef	AFS_AIX41_ENV
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    ifr = ifc.ifc_req;
#else
    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_buf = (caddr_t) & ifs[0];
    memset(&ifs[0], 0, sizeof(ifs));
#endif
    res = ioctl(s, SIOCGIFCONF, &ifc);
    if (res < 0) {
	/* fputs(stderr, "ioctl error IFCONF\n"); */
	close(s);
	return;
    }

    LOCK_IF;
#ifdef	AFS_AIX41_ENV
#define size(p) MAX((p).sa_len, sizeof(p))
    cplim = buf + ifc.ifc_len;	/*skip over if's with big ifr_addr's */
    for (cp = buf; cp < cplim;
	 cp += sizeof(ifr->ifr_name) + MAX(a->sin_len, sizeof(*a))) {
	if (rxi_numNetAddrs >= ADDRSPERSITE)
	    break;

	ifr = (struct ifreq *)cp;
#else
    len = ifc.ifc_len / sizeof(struct ifreq);
    if (len > ADDRSPERSITE)
	len = ADDRSPERSITE;

    for (i = 0; i < len; ++i) {
	ifr = &ifs[i];
	res = ioctl(s, SIOCGIFADDR, ifr);
#endif
	if (res < 0) {
	    /* fputs(stderr, "ioctl error IFADDR\n");
	     * perror(ifr->ifr_name);   */
	    continue;
	}
	a = (struct sockaddr_in *)&ifr->ifr_addr;
	if (a->sin_family != AF_INET)
	    continue;
	rxi_NetAddrs[rxi_numNetAddrs] = ntohl(a->sin_addr.s_addr);
	if (rx_IsLoopbackAddr(rxi_NetAddrs[rxi_numNetAddrs])) {
	    /* we don't really care about "localhost" */
	    continue;
	}
	for (j = 0; j < rxi_numNetAddrs; j++) {
	    if (rxi_NetAddrs[j] == rxi_NetAddrs[rxi_numNetAddrs])
		break;
	}
	if (j < rxi_numNetAddrs)
	    continue;

	/* fprintf(stderr, "if %s addr=%x\n", ifr->ifr_name,
	 * rxi_NetAddrs[rxi_numNetAddrs]); */

#ifdef SIOCGIFFLAGS
	res = ioctl(s, SIOCGIFFLAGS, ifr);
	if (res == 0) {
	    myNetFlags[rxi_numNetAddrs] = ifr->ifr_flags;
#ifdef IFF_LOOPBACK
	    /* Handle aliased loopbacks as well. */
	    if (ifr->ifr_flags & IFF_LOOPBACK)
		continue;
#endif
	    /* fprintf(stderr, "if %s flags=%x\n",
	     * ifr->ifr_name, ifr->ifr_flags); */
	} else {		/*
				 * fputs(stderr, "ioctl error IFFLAGS\n");
				 * perror(ifr->ifr_name); */
	}
#endif /* SIOCGIFFLAGS */

#if !defined(AFS_AIX_ENV)  && !defined(AFS_LINUX20_ENV)
	/* this won't run on an AIX system w/o a cache manager */
	rxi_syscallp = rxi_syscall;
#endif

	/* If I refer to kernel extensions that aren't loaded on AIX, the
	 * program refuses to load and run, so I simply can't include the
	 * following code.  Fortunately, AIX is the one operating system in
	 * which the subsequent ioctl works reliably. */
	if (rxi_syscallp) {
	    if ((*rxi_syscallp) (20 /*AFSOP_GETMTU */ ,
				 htonl(rxi_NetAddrs[rxi_numNetAddrs]),
				 &(myNetMTUs[rxi_numNetAddrs]))) {
		/* fputs(stderr, "syscall error GETMTU\n");
		 * perror(ifr->ifr_name); */
		myNetMTUs[rxi_numNetAddrs] = 0;
	    }
	    if ((*rxi_syscallp) (42 /*AFSOP_GETMASK */ ,
				 htonl(rxi_NetAddrs[rxi_numNetAddrs]),
				 &(myNetMasks[rxi_numNetAddrs]))) {
		/* fputs(stderr, "syscall error GETMASK\n");
		 * perror(ifr->ifr_name); */
		myNetMasks[rxi_numNetAddrs] = 0;
	    } else
		myNetMasks[rxi_numNetAddrs] =
		    ntohl(myNetMasks[rxi_numNetAddrs]);
	    /* fprintf(stderr, "if %s mask=0x%x\n",
	     * ifr->ifr_name, myNetMasks[rxi_numNetAddrs]); */
	}

	if (myNetMTUs[rxi_numNetAddrs] == 0) {
	    myNetMTUs[rxi_numNetAddrs] = OLD_MAX_PACKET_SIZE + RX_IPUDP_SIZE;
#ifdef SIOCGIFMTU
	    res = ioctl(s, SIOCGIFMTU, ifr);
	    if ((res == 0) && (ifr->ifr_metric > 128)) {	/* sanity check */
		myNetMTUs[rxi_numNetAddrs] = ifr->ifr_metric;
		/* fprintf(stderr, "if %s mtu=%d\n",
		 * ifr->ifr_name, ifr->ifr_metric); */
	    } else {
		/* fputs(stderr, "ioctl error IFMTU\n");
		 * perror(ifr->ifr_name); */
	    }
#endif
	}

	if (myNetMasks[rxi_numNetAddrs] == 0) {
	    myNetMasks[rxi_numNetAddrs] =
		fudge_netmask(rxi_NetAddrs[rxi_numNetAddrs]);
#ifdef SIOCGIFNETMASK
	    res = ioctl(s, SIOCGIFNETMASK, ifr);
	    if (res == 0) {
		a = (struct sockaddr_in *)&ifr->ifr_addr;
		myNetMasks[rxi_numNetAddrs] = ntohl(a->sin_addr.s_addr);
		/* fprintf(stderr, "if %s subnetmask=0x%x\n",
		 * ifr->ifr_name, myNetMasks[rxi_numNetAddrs]); */
	    } else {
		/* fputs(stderr, "ioctl error IFMASK\n");
		 * perror(ifr->ifr_name); */
	    }
#endif
	}

	if (!rx_IsLoopbackAddr(rxi_NetAddrs[rxi_numNetAddrs])) {	/* ignore lo0 */
	    int maxsize;
	    maxsize =
		rxi_nRecvFrags * (myNetMTUs[rxi_numNetAddrs] - RX_IP_SIZE);
	    maxsize -= UDP_HDR_SIZE;	/* only the first frag has a UDP hdr */
	    if (rx_maxReceiveSize < maxsize)
		rx_maxReceiveSize = MIN(RX_MAX_PACKET_SIZE, maxsize);
	    ++rxi_numNetAddrs;
	}
    }
    UNLOCK_IF;
    close(s);

    /* have to allocate at least enough to allow a single packet to reach its
     * maximum size, so ReadPacket will work.  Allocate enough for a couple
     * of packets to do so, for good measure */
    {
	int npackets, ncbufs;

	rx_maxJumboRecvSize =
	    RX_HEADER_SIZE + rxi_nDgramPackets * RX_JUMBOBUFFERSIZE +
	    (rxi_nDgramPackets - 1) * RX_JUMBOHEADERSIZE;
	rx_maxJumboRecvSize = MAX(rx_maxJumboRecvSize, rx_maxReceiveSize);
	ncbufs = (rx_maxJumboRecvSize - RX_FIRSTBUFFERSIZE);
	if (ncbufs > 0) {
	    ncbufs = ncbufs / RX_CBUFFERSIZE;
	    npackets = rx_initSendWindow - 1;
	    rxi_MorePackets(npackets * (ncbufs + 1));
	}
    }
}
#endif /* AFS_NT40_ENV */

/* Called from rxi_FindPeer, when initializing a clear rx_peer structure,
 * to get interesting information.
 * Curiously enough, the rx_peerHashTable_lock currently protects the
 * Inited variable (and hence rx_GetIFInfo). When the fs suite uses
 * pthreads, this issue will need to be revisited.
 */

void
rxi_InitPeerParams(struct rx_peer *pp)
{
    afs_uint32 ppaddr;
    u_short rxmtu;
    int ix;
#if defined(ADAPT_PMTU) && defined(IP_MTU)
    int sock;
    struct sockaddr_in addr;
#endif

    LOCK_IF_INIT;
    if (!Inited) {
	UNLOCK_IF_INIT;
	/*
	 * there's a race here since more than one thread could call
	 * rx_GetIFInfo.  The race stops in rx_GetIFInfo.
	 */
	rx_GetIFInfo();
    } else {
	UNLOCK_IF_INIT;
    }

#ifdef ADAPT_MTU
    /* try to second-guess IP, and identify which link is most likely to
     * be used for traffic to/from this host. */
    ppaddr = ntohl(pp->host);

    pp->ifMTU = 0;
    rx_rto_setPeerTimeoutSecs(pp, 2);
    pp->rateFlag = 2;		/* start timing after two full packets */
    /* I don't initialize these, because I presume they are bzero'd...
     * pp->burstSize pp->burst pp->burstWait.sec pp->burstWait.usec
     */

    LOCK_IF;
    for (ix = 0; ix < rxi_numNetAddrs; ++ix) {
	if ((rxi_NetAddrs[ix] & myNetMasks[ix]) == (ppaddr & myNetMasks[ix])) {
#ifdef IFF_POINTOPOINT
	    if (myNetFlags[ix] & IFF_POINTOPOINT)
		rx_rto_setPeerTimeoutSecs(pp, 4);
#endif /* IFF_POINTOPOINT */

	    rxmtu = myNetMTUs[ix] - RX_IPUDP_SIZE;
	    if (rxmtu < RX_MIN_PACKET_SIZE)
		rxmtu = RX_MIN_PACKET_SIZE;
	    if (pp->ifMTU < rxmtu)
		pp->ifMTU = MIN(rx_MyMaxSendSize, rxmtu);
	}
    }
    UNLOCK_IF;
    if (!pp->ifMTU) {		/* not local */
	rx_rto_setPeerTimeoutSecs(pp, 3);
	pp->ifMTU = MIN(rx_MyMaxSendSize, RX_REMOTE_PACKET_SIZE);
    }
#else /* ADAPT_MTU */
    pp->rateFlag = 2;		/* start timing after two full packets */
    rx_rto_setPeerTimeoutSecs(pp, 2);
    pp->ifMTU = MIN(rx_MyMaxSendSize, OLD_MAX_PACKET_SIZE);
#endif /* ADAPT_MTU */
#if defined(ADAPT_PMTU) && defined(IP_MTU)
    sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock != OSI_NULLSOCKET) {
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = pp->host;
        addr.sin_port = pp->port;
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            int mtu=0;
            socklen_t s = sizeof(mtu);
            if (getsockopt(sock, SOL_IP, IP_MTU, &mtu, &s)== 0) {
                pp->ifMTU = MIN(mtu - RX_IPUDP_SIZE, pp->ifMTU);
            }
        }
#ifdef AFS_NT40_ENV
        closesocket(sock);
#else
        close(sock);
#endif
    }
#endif
    pp->ifMTU = rxi_AdjustIfMTU(pp->ifMTU);
    pp->maxMTU = OLD_MAX_PACKET_SIZE;	/* for compatibility with old guys */
    pp->natMTU = MIN((int)pp->ifMTU, OLD_MAX_PACKET_SIZE);
    pp->maxDgramPackets =
	MIN(rxi_nDgramPackets,
	    rxi_AdjustDgramPackets(rxi_nSendFrags, pp->ifMTU));
    pp->ifDgramPackets =
	MIN(rxi_nDgramPackets,
	    rxi_AdjustDgramPackets(rxi_nSendFrags, pp->ifMTU));
    pp->maxDgramPackets = 1;
    /* Initialize slow start parameters */
    pp->MTU = MIN(pp->natMTU, pp->maxMTU);
    pp->cwind = 1;
    pp->nDgramPackets = 1;
    pp->congestSeq = 0;
}

/* Don't expose jumobgram internals. */
void
rx_SetNoJumbo(void)
{
    rx_maxReceiveSize = OLD_MAX_PACKET_SIZE;
    rxi_nSendFrags = rxi_nRecvFrags = 1;
}

/* Override max MTU.  If rx_SetNoJumbo is called, it must be
   called before calling rx_SetMaxMTU since SetNoJumbo clobbers rx_maxReceiveSize */
void
rx_SetMaxMTU(int mtu)
{
    rx_MyMaxSendSize = rx_maxReceiveSizeUser = rx_maxReceiveSize = mtu;
}

#if defined(ADAPT_PMTU)
int
rxi_HandleSocketError(int socket)
{
    int ret=0;
#if defined(HAVE_LINUX_ERRQUEUE_H)
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct sock_extended_err *err;
    struct sockaddr_in addr;
    char controlmsgbuf[256];
    int code;

    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov = NULL;
    msg.msg_iovlen = 0;
    msg.msg_control = controlmsgbuf;
    msg.msg_controllen = 256;
    msg.msg_flags = 0;
    code = recvmsg(socket, &msg, MSG_ERRQUEUE|MSG_DONTWAIT|MSG_TRUNC);

    if (code < 0 || !(msg.msg_flags & MSG_ERRQUEUE))
        goto out;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
       if ((char *)cmsg - controlmsgbuf > msg.msg_controllen - CMSG_SPACE(0) ||
           (char *)cmsg - controlmsgbuf > msg.msg_controllen - CMSG_SPACE(cmsg->cmsg_len) ||
	   cmsg->cmsg_len == 0) {
	   cmsg = 0;
           break;
	}
        if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR)
            break;
    }
    if (!cmsg)
        goto out;
    ret=1;
    err =(struct sock_extended_err *) CMSG_DATA(cmsg);

    if (err->ee_errno == EMSGSIZE && err->ee_info >= 68) {
	rxi_SetPeerMtu(NULL, addr.sin_addr.s_addr, addr.sin_port,
                       err->ee_info - RX_IPUDP_SIZE);
    }
    /* other DEST_UNREACH's and TIME_EXCEEDED should be dealt with too */

out:
#endif
    return ret;
}
#endif
