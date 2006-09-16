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

#include "rx/rx_kcommon.h"

#ifdef AFS_DARWIN80_ENV
#define soclose sock_close
#endif
 
int
osi_NetReceive(osi_socket so, struct sockaddr_storage *saddr, int *slen,
	       struct iovec *dvec, int nvecs, int *alength)
{
#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    struct msghdr msg;
    int rlen;
    mbuf_t m;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
#endif
    int i;
    struct iovec iov[RX_MAXIOVECS];
    struct sockaddr *sa = NULL;
    int code;
    size_t resid;

    int haveGlock = ISAFS_GLOCK();
    /*AFS_STATCNT(osi_NetReceive); */

    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetReceive: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];
    if (haveGlock)
	AFS_GUNLOCK();
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
#ifdef AFS_DARWIN80_ENV
#if 1
    resid = *alength;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = saddr;
    msg.msg_namelen = sizeof(struct sockaddr_storage);
    sa =(struct sockaddr *) saddr;
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
    resid = *alength;
    printf("Want to read %d bytes...", resid);
    for (i=0; i < nvecs && resid; i++) {
       if (resid < iov[i].iov_len)
          iov[0].iov_len = resid;
       resid -= iov[i].iov_len;
    }
    printf("Using %d/%d iovs\n", i, nvecs);
    nvecs = i;
    rlen = 0;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &ss;
    msg.msg_namelen = sizeof(struct sockaddr_storage);
    msg.msg_iov = &iov[0];
    msg.msg_iovlen = nvecs;
    sa =(struct sockaddr_in *) &ss;
    code = sock_receive(asocket, &msg, 0, &rlen);
    resid = *alength;
    if (resid != rlen)
    printf("recieved %d bytes\n", rlen);
    if (resid > rlen)
       resid -= rlen;
    else
       resid = 0;
#endif
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
	*slen = sa->sa_len;
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

int
osi_NetSend(osi_socket so, struct sockaddr_storage *saddr, int salen,
	    struct iovec *dvec, int nvecs, afs_int32 alength, int istack)
{
#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    struct msghdr msg;
    size_t slen;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
#endif
    register afs_int32 code;
    int i;
    struct iovec iov[RX_MAXIOVECS];
    int haveGlock = ISAFS_GLOCK();

    AFS_STATCNT(osi_NetSend);
    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    saddr->ss_len = saddr->ss_family == AF_INET6 ?
		sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    if (haveGlock)
	AFS_GUNLOCK();
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
#ifdef AFS_DARWIN80_ENV
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = saddr;
    msg.msg_namelen = saddr->ss_len;
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
