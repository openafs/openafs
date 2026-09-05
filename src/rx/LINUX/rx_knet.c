/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_knet.c - RX kernel send, receive and timer routines.
 *
 * Linux implementation.
 */
#include <afsconfig.h>
#include "afs/param.h"


#include <linux/version.h>
#include "rx/rx_kcommon.h"
#include "rx.h"
#include "rx_atomic.h"
#include "rx_globals.h"
#include "rx_stats.h"
#include "rx_peer.h"
#include "rx_packet.h"
#include "rx_internal.h"
#if defined(HAVE_LINUX_UACCESS_H)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif
#ifdef AFS_RXERRQ_ENV
#include <linux/errqueue.h>
#include <linux/icmp.h>
#endif
#include "osi_compat.h"

#if defined(HAVE_LINUX_SOCKET_BIND_SOCKADDR_UNSIZED)
# define BIND_SOCKADDR(sa) ((struct sockaddr_unsized *)(sa))
#else
# define BIND_SOCKADDR(sa) ((struct sockaddr *)(sa))
#endif

/* rxk_NewSocket
 * open and bind RX socket
 */
osi_socket *
rxk_NewSocketHost(afs_uint32 ahost, short aport)
{
    struct socket *sockp;
    struct sockaddr_in myaddr;
    int code;
#ifdef AFS_ADAPT_PMTU
    int pmtu = IP_PMTUDISC_WANT;
#else
    int pmtu = IP_PMTUDISC_DONT;
#endif

#ifdef HAVE_LINUX_SOCK_CREATE_KERN_NS
    code = sock_create_kern(&init_net, AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sockp);
#elif defined(HAVE_LINUX_SOCK_CREATE_KERN)
    code = sock_create_kern(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sockp);
#elif defined(LINUX_KERNEL_SOCK_CREATE_V)
    code = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sockp, 0);
#else
    code = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sockp);
#endif
    if (code < 0)
	return NULL;

    /* Bind socket */
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = ahost;
    myaddr.sin_port = aport;
    code = sockp->ops->bind(sockp, BIND_SOCKADDR(&myaddr), sizeof(myaddr));

    if (code < 0) {
	printk("sock_release(rx_socket) FIXME\n");
	return NULL;
    }

    afs_linux_sock_set_mtu_discover(sockp, pmtu);

#ifdef AFS_RXERRQ_ENV
    afs_linux_sock_set_recverr(sockp);
#endif
    return (osi_socket *)sockp;
}

osi_socket *
rxk_NewSocket(short aport)
{
    return rxk_NewSocketHost(htonl(INADDR_ANY), aport);
}

#if defined(HAVE_IPV6)
/*
 * rxk_NewSocketSA - open and bind an RX socket for either address family.
 *
 * Kept as a separate function from rxk_NewSocketHost() above (rather than
 * widening that one in place) so every existing caller of
 * rxk_NewSocketHost()/rxk_NewSocket() - the well-tested IPv4 rx_socket path
 * - keeps working completely unchanged. Only the new IPv6 rx_socket6 path
 * (rx_InitHost2(), src/rx/rx.c) calls this.
 */
osi_socket *
rxk_NewSocketSA(struct rx_sockaddr *sa)
{
    struct socket *sockp;
    int code;
    int family = (sa->rxsa_family == AF_INET6) ? AF_INET6 : AF_INET;
#ifdef AFS_ADAPT_PMTU
    int pmtu = IP_PMTUDISC_WANT;
#else
    int pmtu = IP_PMTUDISC_DONT;
#endif

    if (family != AF_INET6) {
	/* Not what this function is for - rxk_NewSocketHost() already
	 * covers plain IPv4. */
	return NULL;
    }

#ifdef HAVE_LINUX_SOCK_CREATE_KERN_NS
    code = sock_create_kern(&init_net, AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &sockp);
#elif defined(HAVE_LINUX_SOCK_CREATE_KERN)
    code = sock_create_kern(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &sockp);
#elif defined(LINUX_KERNEL_SOCK_CREATE_V)
    code = sock_create(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &sockp, 0);
#else
    code = sock_create(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &sockp);
#endif
    if (code < 0)
	return NULL;

    /* Never fall back to a v4-mapped address on this socket - a v4 peer
     * is always reached via the separate, real IPv4 rx_socket instead
     * (see the plan's D2). */
    afs_linux_sock_set_v6only(sockp);

    code = sockp->ops->bind(sockp, BIND_SOCKADDR(&sa->addr), sa->addrlen);
    if (code < 0) {
	sock_release(sockp);
	return NULL;
    }

    afs_linux_sock_set_mtu_discover(sockp, pmtu);

#ifdef AFS_RXERRQ_ENV
    afs_linux_sock_set_recverr6(sockp);
#endif
    return (osi_socket *)sockp;
}
#endif /* HAVE_IPV6 */

/* free socket allocated by osi_NetSocket */
int
rxk_FreeSocket(struct socket *asocket)
{
    AFS_STATCNT(osi_FreeSocket);
    return 0;
}

#ifdef AFS_RXERRQ_ENV
int
osi_HandleSocketError(osi_socket so, void *cmsgbuf, size_t cmsgbuf_len)
{
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct sock_extended_err *err;
    struct sockaddr_in addr;
    int code;
    struct socket *sop = (struct socket *)so;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = cmsgbuf_len;

    code = kernel_recvmsg(sop, &msg, NULL, 0, 0,
			  MSG_ERRQUEUE|MSG_DONTWAIT|MSG_TRUNC);

    if (code < 0 || !(msg.msg_flags & MSG_ERRQUEUE))
	return 0;

    /* kernel_recvmsg changes msg_control to point at the _end_ of the buffer,
     * and msg_controllen is set to the number of bytes remaining */
    msg.msg_controllen = ((char*)msg.msg_control - (char*)cmsgbuf);
    msg.msg_control = cmsgbuf;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg && CMSG_OK(&msg, cmsg);
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {

	if (cmsg->cmsg_level != SOL_IP || cmsg->cmsg_type != IP_RECVERR) {
	    continue;
	}

	{
	    struct rx_sockaddr sa;
	    err = CMSG_DATA(cmsg);
	    rx_ipv4_to_sockaddr(addr.sin_addr.s_addr, addr.sin_port, 0, &sa);
	    rxi_ProcessNetError(err, &sa);
	}
    }

    return 1;
}
#endif

/* osi_NetSend
 *
 * Return codes:
 * 0 = success
 * non-zero = failure
 */
int
osi_NetSend(osi_socket sop, struct sockaddr_in *to, struct iovec *iovec,
	    int iovcnt, afs_int32 size, int istack)
{
    struct msghdr msg;
    int code;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = to;
    msg.msg_namelen = sizeof(*to);

    code = kernel_sendmsg(sop, &msg, (struct kvec *) iovec, iovcnt, size);

    return (code < 0) ? code : 0;
}


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
 * error code (such as EINTER) if not
 *
 * Environment
 *	Note that the maximum number of iovecs is 2 + RX_MAXWVECS. This is
 *	so we have a little space to look for packets larger than 
 *	rx_maxReceiveSize.
 */
int rxk_lastSocketError;
int rxk_nSocketErrors;
int
osi_NetReceive(osi_socket so, struct sockaddr_in *from, struct iovec *iov,
	       int iovcnt, int *lengthp)
{
    struct msghdr msg;
    int code;
    struct iovec tmpvec[RX_MAXWVECS + 2];
    struct socket *sop = (struct socket *)so;

    if (iovcnt > RX_MAXWVECS + 2) {
	osi_Panic("Too many (%d) iovecs passed to osi_NetReceive\n", iovcnt);
    }

    memcpy(tmpvec, iov, iovcnt * sizeof(struct iovec));
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = from;

    code = kernel_recvmsg(sop, &msg, (struct kvec *)tmpvec, iovcnt,
			  *lengthp, 0);
    if (code < 0) {
	afs_try_to_freeze();

	/* Clear the error before using the socket again.
	 * Oh joy, Linux has hidden header files as well. It appears we can
	 * simply call again and have it clear itself via sock_error().
	 */
	flush_signals(current);	/* We don't want no stinkin' signals. */
	rxk_lastSocketError = code;
	rxk_nSocketErrors++;

	rxi_HandleSocketErrors(so);
    } else {
	*lengthp = code;
	code = 0;
    }

    return code;
}

#if defined(HAVE_IPV6)
/*
 * osi_NetReceiveSA - osi_NetReceive()'s IPv6-capable sibling, used only by
 * rxk_Listener6()/rx_socket6 (see rxk_ReadPacketSA() in rx_kcommon.c). Kept
 * separate rather than widening osi_NetReceive() in place, matching
 * rxk_NewSocketSA()'s own reasoning: the well-tested rx_socket/IPv4 path is
 * completely unaffected.
 *
 * Unlike osi_NetReceive(), this sets msg_namelen going in (the size of the
 * union member big enough for a sockaddr_in6) - not required for the v4
 * path (whose caller never inspects it), but IPv6 addresses genuinely need
 * the kernel to be told how much room it has to write sin6_addr/sin6_port
 * into.
 */
int
osi_NetReceiveSA(osi_socket so, struct rx_sockaddr *from, struct iovec *iov,
		 int iovcnt, int *lengthp)
{
    struct msghdr msg;
    int code;
    struct iovec tmpvec[RX_MAXWVECS + 2];
    struct socket *sop = (struct socket *)so;

    if (iovcnt > RX_MAXWVECS + 2) {
	osi_Panic("Too many (%d) iovecs passed to osi_NetReceiveSA\n", iovcnt);
    }

    memcpy(tmpvec, iov, iovcnt * sizeof(struct iovec));
    memset(&msg, 0, sizeof(msg));
    memset(from, 0, sizeof(*from));
    msg.msg_name = &from->addr;
    msg.msg_namelen = sizeof(from->addr);

    code = kernel_recvmsg(sop, &msg, (struct kvec *)tmpvec, iovcnt,
			  *lengthp, 0);
    if (code < 0) {
	afs_try_to_freeze();
	flush_signals(current);	/* We don't want no stinkin' signals. */
	rxk_lastSocketError = code;
	rxk_nSocketErrors++;

	rxi_HandleSocketErrors(so);
    } else {
	from->addrlen = msg.msg_namelen;
	from->socktype = SOCK_DGRAM;
	from->rxsa_in6_family = AF_INET6;
	*lengthp = code;
	code = 0;
    }

    return code;
}
#endif /* HAVE_IPV6 */

void
osi_StopListener(void)
{
    extern struct task_struct *rxk_ListenerTask;

    while (rxk_ListenerTask) {
        if (rxk_ListenerTask) {
	    flush_signals(rxk_ListenerTask);
	    send_sig(SIGKILL, rxk_ListenerTask, 1);
	}
	if (!rxk_ListenerTask)
	    break;
	afs_osi_Sleep(&rxk_ListenerTask);
    }
    sock_release(rx_socket);
    rx_socket = NULL;

#if defined(HAVE_IPV6)
    /*
     * rxk_Listener6()'s own task handle - a genuinely separate wait, not
     * folded into the loop above, since rxk_ListenerTask6 always gets
     * cleared (rxk_Listener6() sets it unconditionally, even when
     * rx_socket6 was never opened - see that function's own comment), so
     * this is safe to always run rather than needing an
     * "was IPv6 ever actually used" check first. Missing this originally
     * meant `rmmod` could hang forever (or leave a live kernel thread
     * pinning the module resident) any time a v6-capable client had
     * actually run this thread - confirmed live: an early version of
     * this patch reproduced exactly that hang against a real kernel.
     */
    {
	extern struct task_struct *rxk_ListenerTask6;

	while (rxk_ListenerTask6) {
	    flush_signals(rxk_ListenerTask6);
	    send_sig(SIGKILL, rxk_ListenerTask6, 1);
	    if (!rxk_ListenerTask6)
		break;
	    afs_osi_Sleep(&rxk_ListenerTask6);
	}
	if (rx_socket6 != OSI_NULLSOCKET) {
	    sock_release(rx_socket6);
	    rx_socket6 = OSI_NULLSOCKET;
	}
    }
#endif /* HAVE_IPV6 */
}

