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

RCSID
    ("$Header$");

#include "../rx/rx_kcommon.h"

int
osi_NetReceive(osi_socket asocket, struct sockaddr_storage *saddr, int *slen,
	       struct iovec *dvec, int nvecs, int *alength)
{
    struct uio u;
    int i, code;
    struct iovec iov[RX_MAXIOVECS];
    struct mbuf *nam = NULL;

    int haveGlock = ISAFS_GLOCK();

    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetReceive: %d: too many iovecs\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = *alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_READ;
    u.uio_procp = NULL;

    if (haveGlock)
	AFS_GUNLOCK();
    code = soreceive(asocket, (saddr ? &nam : NULL), &u, NULL, NULL, NULL);
    if (haveGlock)
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
    if (saddr && nam) {
	memcpy(saddr, mtod(nam, caddr_t), nam->m_len);
	*slen = nam->m_len;
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
osi_NetSend(osi_socket asocket, struct sockaddr_storage *addr, int slen,
	    struct iovec *dvec, int nvecs, afs_int32 alength, int istack)
{
    int i, code;
    struct iovec iov[RX_MAXIOVECS];
    struct uio u;
    struct mbuf *nam;
    int haveGlock = ISAFS_GLOCK();

    AFS_STATCNT(osi_NetSend);
    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_WRITE;
    u.uio_procp = NULL;

    nam = m_get(M_DONTWAIT, MT_SONAME);
    if (!nam)
	return ENOBUFS;
    nam->m_len = slen;
    memcpy(mtod(nam, caddr_t), addr, slen);

    if (haveGlock)
	AFS_GUNLOCK();
    code = sosend(asocket, nam, &u, NULL, NULL, 0);
    if (haveGlock)
	AFS_GLOCK();
    m_freem(nam);

    return code;
}
