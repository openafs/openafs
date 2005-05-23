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

int
osi_NetReceive(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	       int nvecs, int *alength)
{
#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    uio_t up;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
    struct uio *up = &u;
#endif
    int i;
    struct iovec iov[RX_MAXIOVECS];
    struct sockaddr *sa = NULL;
    int code;

    int haveGlock = ISAFS_GLOCK();
    /*AFS_STATCNT(osi_NetReceive); */

    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetReceive: %d: Too many iovecs.\n", nvecs);

#ifdef AFS_DARWIN80_ENV
    up = uio_create(nvecs, 0, UIO_SYSSPACE, UIO_READ);
    for (i = 0; i < nvecs; i++) {
	/* is (user_addr_t) needed? */
	code = uio_addiov(up, (user_addr_t) dvec[i].iov_base, dvec[i].iov_len);
	if (code)
	    return code;
    }

#else
    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = *alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_READ;
    u.uio_procp = NULL;
#endif

    if (haveGlock)
	AFS_GUNLOCK();
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
    code = soreceive(asocket, &sa, up, NULL, NULL, NULL);
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#endif
    if (haveGlock)
	AFS_GLOCK();

    if (code)
	return code;
    *alength -= AFS_UIO_RESID(up);
    if (sa) {
	if (sa->sa_family == AF_INET) {
	    if (addr)
		*addr = *(struct sockaddr_in *)sa;
	} else
	    printf("Unknown socket family %d in NetReceive\n", sa->sa_family);
	FREE(sa, M_SONAME);
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
    p = pfind(rxk_ListenerPid);
    if (p)
	psignal(p, SIGUSR1);
}

int
osi_NetSend(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	    int nvecs, afs_int32 alength, int istack)
{
#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    uio_t up;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
    struct uio *up = &u;
#endif
    register afs_int32 code;
    int i;
    struct iovec iov[RX_MAXIOVECS];
    int haveGlock = ISAFS_GLOCK();

    AFS_STATCNT(osi_NetSend);
    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);

#ifdef AFS_DARWIN80_ENV
    up = uio_create(nvecs, 0, UIO_SYSSPACE, UIO_WRITE);
    for (i = 0; i < nvecs; i++) {
	/* is (user_addr_t) needed? */
	code = uio_addiov(up, (user_addr_t) dvec[i].iov_base, dvec[i].iov_len);
	if (code)
	    return code;
    }
#else
    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_WRITE;
    u.uio_procp = NULL;
#endif

    addr->sin_len = sizeof(struct sockaddr_in);

    if (haveGlock)
	AFS_GUNLOCK();
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
    code = sosend(asocket, (struct sockaddr *)addr, up, NULL, NULL, 0);
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
#endif
    if (haveGlock)
	AFS_GLOCK();
    return code;
}
