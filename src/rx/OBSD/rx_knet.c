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

RCSID("$Header$");

#include "../rx/rx_kcommon.h"

int osi_NetReceive(osi_socket asocket, struct sockaddr_in *addr, struct iovec *dvec,
		   int nvecs, int *alength)
{
    struct uio u;
    int i, code;
    struct iovec iov[RX_MAXIOVECS];
    struct mbuf *nam = NULL;

    int haveGlock = ISAFS_GLOCK();

    if (nvecs > RX_MAXIOVECS)
        osi_Panic("osi_NetReceive: %d: too many iovecs\n", nvecs);

    for (i = 0 ; i < nvecs ; i++) {
        iov[i].iov_base = dvec[i].iov_base;
        iov[i].iov_len = dvec[i].iov_len;
    }

    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = *alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_READ;
    u.uio_procp = NULL;

    if (haveGlock)
        AFS_GUNLOCK();
    code = soreceive(asocket, (addr ? &nam : NULL), &u, NULL, NULL, NULL);
    if (haveGlock)
        AFS_GLOCK();

    if (code)
	return code;

    *alength -= u.uio_resid;
    if (addr && nam) {
	memcpy(addr, mtod(nam, caddr_t), nam->m_len);
	m_freem(nam);
    }

    return code;
}

extern int rxk_ListenerPid;
void osi_StopListener(void)
{
   struct proc *p;

   soclose(rx_socket);
   p = pfind(rxk_ListenerPid);
   if (p)
       psignal(p, SIGUSR1);
}

/* rx_NetSend - send asize bytes at adata from asocket to host at addr.
 *
 * Now, why do we allocate a new buffer when we could theoretically use the one
 * pointed to by adata?  Because PRU_SEND returns after queueing the message,
 * not after sending it.  If the sender changes the data after queueing it,
 * we'd see the already-queued data change.  One attempt to fix this without
 * adding a copy would be to have this function wait until the datagram is
 * sent; however this doesn't work well.  In particular, if a host is down, and
 * an ARP fails to that host, this packet will be queued until the ARP request
 * comes back, which could be hours later.  We can't block in this routine that
 * long, since it prevents RPC timeouts from happening.
 */
/* XXX In the brave new world, steal the data bufs out of the rx_packet iovec,
 * and just queue those.  XXX
 */


int osi_NetSend(osi_socket asocket, struct sockaddr_in *addr,
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

    for (i = 0; i < nvecs; i++) {
        iov[i].iov_base = dvec[i].iov_base;
        iov[i].iov_len = dvec[i].iov_len;
    }

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
    nam->m_len = addr->sin_len = sizeof(struct sockaddr_in);
    memcpy(mtod(nam, caddr_t), addr, addr->sin_len);

    if (haveGlock)
	AFS_GUNLOCK();
    code = sosend(asocket, nam, &u, NULL, NULL, 0);
    if (haveGlock)
	AFS_GLOCK();
    m_freem(nam);

    return code;
}
