/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "../afs/param.h"


#include "../rx/rx_kcommon.h"

int
osi_NetReceive(osi_socket asocket, struct sockaddr_in *addr,
	       struct iovec *dvec, int nvecs, int *alength)
{
    struct uio u;
    int i, code;
    struct iovec iov[RX_MAXIOVECS];
    struct mbuf *nam = NULL;

    int glocked = ISAFS_GLOCK();

    memset(&u, 0, sizeof(u));
    memset(&iov, 0, sizeof(iov));

    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetReceive: %d: too many iovecs\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = *alength;
    UIO_SETUP_SYSSPACE(&u);
    u.uio_rw = UIO_READ;
#if 0
    u.uio_procp = NULL;
#endif
    if (glocked)
	AFS_GUNLOCK();
    code = soreceive(asocket, (addr ? &nam : NULL), &u, NULL, NULL, NULL);
    if (glocked)
	AFS_GLOCK();

    if (code) {
#ifdef RXKNET_DEBUG
	printf("rx code %d termState %d\n", code, afs_termState);
#endif
	while (afs_termState == AFSOP_STOP_RXEVENT)
	    afs_osi_Sleep(&afs_termState);
	return code;
    }

    *alength -= u.uio_resid;
    if (addr && nam) {
	memcpy(addr, mtod(nam, caddr_t), nam->m_len);
	m_freem(nam);
    }

    return code;
}

extern int rxk_ListenerPid;
void
osi_StopListener(void)
{
    struct proc *p;

    soclose(rx_socket);
    p = pfind(rxk_ListenerPid);
    if (p)
	psignal(p, SIGUSR1);
}

/*
 * rx_NetSend - send asize bytes at adata from asocket to host at addr.
 */

int
osi_NetSend(osi_socket asocket, struct sockaddr_in *addr, struct iovec *dvec,
	    int nvecs, afs_int32 alength, int istack)
{
    int i, code;
    struct iovec iov[RX_MAXIOVECS];
    struct uio u;
    struct mbuf *nam;
    int glocked = ISAFS_GLOCK();

    memset(&u, 0, sizeof(u));
    memset(&iov, 0, sizeof(iov));

    AFS_STATCNT(osi_NetSend);
    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = alength;
    UIO_SETUP_SYSSPACE(&u);
    u.uio_rw = UIO_WRITE;
#if 0
    u.uio_procp = NULL;
#endif
    nam = m_get(M_DONTWAIT, MT_SONAME);
    if (!nam)
	return ENOBUFS;
    nam->m_len = addr->sin_len = sizeof(struct sockaddr_in);
    memcpy(mtod(nam, caddr_t), addr, addr->sin_len);

    if (glocked)
	AFS_GUNLOCK();
    code = sosend(asocket, nam, &u, NULL, NULL, 0, osi_curproc());
    if (glocked)
	AFS_GLOCK();
    m_freem(nam);

    return code;
}
