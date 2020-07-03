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
    code =
	sockp->ops->bind(sockp, (struct sockaddr *)&myaddr, sizeof(myaddr));

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

/* free socket allocated by osi_NetSocket */
int
rxk_FreeSocket(struct socket *asocket)
{
    AFS_STATCNT(osi_FreeSocket);
    return 0;
}

#ifdef AFS_RXERRQ_ENV
static int
osi_HandleSocketError(osi_socket so, char *cmsgbuf, size_t cmsgbuf_len)
{
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct sock_extended_err *err;
    struct sockaddr_in addr;
    int code;
    struct socket *sop = (struct socket *)so;

    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = cmsgbuf_len;
    msg.msg_flags = 0;

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

	err = CMSG_DATA(cmsg);
	rxi_ProcessNetError(err, addr.sin_addr.s_addr, addr.sin_port);
    }

    return 1;
}
#endif

static void
do_handlesocketerror(osi_socket so)
{
#ifdef AFS_RXERRQ_ENV
    char *cmsgbuf;
    size_t cmsgbuf_len;

    cmsgbuf_len = 256;
    cmsgbuf = rxi_Alloc(cmsgbuf_len);
    if (!cmsgbuf) {
	return;
    }

    while (osi_HandleSocketError(so, cmsgbuf, cmsgbuf_len))
	;

    rxi_Free(cmsgbuf, cmsgbuf_len);
#endif
}

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


    msg.msg_name = to;
    msg.msg_namelen = sizeof(*to);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    code = kernel_sendmsg(sop, &msg, (struct kvec *) iovec, iovcnt, size);

    if (code < 0) {
	do_handlesocketerror(sop);
    }

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
    msg.msg_name = from;
#if defined(STRUCT_MSGHDR_HAS_MSG_ITER)
    msg.msg_iter.iov = tmpvec;
    msg.msg_iter.nr_segs = iovcnt;
#else
    msg.msg_iov = tmpvec;
    msg.msg_iovlen = iovcnt;
#endif
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

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

	do_handlesocketerror(so);
    } else {
	*lengthp = code;
	code = 0;
    }

    return code;
}

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
}

