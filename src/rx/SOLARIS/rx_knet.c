/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*        Copyright Transarc Corporation 1989 - All Rights Reserved         *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/
#include "../afs/param.h"
#ifdef AFS_SUN5_ENV
#include "../rx/rx_kcommon.h"


#ifdef AFS_SUN56_ENV

#include "../inet/common.h"
#include "../sys/tiuser.h"
#include "../sys/t_kuser.h"
#include "../sys/stropts.h"
#include "../sys/stream.h"
#include "../sys/tihdr.h"
#include "../sys/fcntl.h"
#include "../inet/ip.h"
#include "../netinet/udp.h"

/*
 * Function pointers for kernel socket routines
 */
struct sonode *(*sockfs_socreate)
    (vnode_t *, int, int, int, int, struct sonode *, int *) = NULL;
struct vnode *(*sockfs_solookup)
    (int, int, int, char *, int *) = NULL;
int (*sockfs_sobind)
    (struct sonode *, struct sockaddr *, int, int, int) = NULL;
int (*sockfs_sorecvmsg)
    (struct sonode *, struct nmsghdr *, struct uio *) = NULL;
int (*sockfs_sosendmsg)
    (struct sonode *, struct nmsghdr *, struct uio *) = NULL;
int (*sockfs_sosetsockopt)
    (struct sonode *, int, int, void *, int) = NULL;

int rxi_GetIFInfo()
{
    return 0;
}

/* rxi_NewSocket, rxi_FreeSocket and osi_NetSend are from the now defunct
 * afs_osinet.c. 
 */

struct sockaddr_in rx_sockaddr;

/* Allocate a new socket at specified port in network byte order. */
struct osi_socket *rxk_NewSocket(short aport)
{
    vnode_t *accessvp;
    struct sonode *so;
    struct sockaddr_in addr;
    int error;
    int len;

    AFS_STATCNT(osi_NewSocket);

    if (sockfs_solookup == NULL) {
	sockfs_solookup = (struct vnode *(*)())modlookup("sockfs", "solookup");
	if (sockfs_solookup == NULL) {
	    return NULL;
	}
    }
    if (sockfs_socreate == NULL) {
	sockfs_socreate = (struct sonode *(*)())modlookup("sockfs", "socreate");
	if (sockfs_socreate == NULL) {
	    return NULL;
	}
    }
    if (sockfs_sobind == NULL) {
	sockfs_sobind = (int (*)())modlookup("sockfs", "sobind");
	if (sockfs_sobind == NULL) {
	    return NULL;
	}
    }
    if (sockfs_sosetsockopt == NULL) {
	sockfs_sosetsockopt = (int (*)())modlookup("sockfs", "sosetsockopt");
	if (sockfs_sosetsockopt == NULL) {
	    return NULL;
	}
    }
    if (sockfs_sosendmsg == NULL) {
	sockfs_sosendmsg = (int (*)())modlookup("sockfs", "sosendmsg");
	if (sockfs_sosendmsg == NULL) {
	    return NULL;
	}
    }
    if (sockfs_sorecvmsg == NULL) {
	sockfs_sorecvmsg = (int (*)())modlookup("sockfs", "sorecvmsg");
	if (sockfs_sorecvmsg == NULL) {
	    return NULL;
	}
    }

    accessvp = sockfs_solookup(AF_INET, SOCK_DGRAM, 0, "/dev/udp", &error);
    if (accessvp == NULL) {
	return NULL;
    }

    so = sockfs_socreate(accessvp, AF_INET, SOCK_DGRAM, 0,
			 SOV_STREAM, NULL, &error);
    if (so == NULL) {
	return NULL;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = aport;
    addr.sin_addr.s_addr = INADDR_ANY;
    error = sockfs_sobind(so, (struct sockaddr *)&addr, sizeof(addr), 0, 0);
    if (error != 0) {
	return NULL;
    }

    len = rx_UdpBufSize;
    error = sockfs_sosetsockopt(so, SOL_SOCKET, SO_SNDBUF, &len, sizeof(len));
    if (error != 0) {
	return NULL;
    }

    len = rx_UdpBufSize;
    error = sockfs_sosetsockopt(so, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len));
    if (error != 0) {
	return NULL;
    }

    return (struct osi_socket *)so;
}

int osi_FreeSocket(asocket)
    register struct osi_socket *asocket; 
{
    extern int rxk_ListenerPid;
    struct sonode *so = (struct sonode *)asocket;
    vnode_t *vp = SOTOV(so);

    AFS_STATCNT(osi_FreeSocket);
    if (rxk_ListenerPid)
	kill(rxk_ListenerPid, SIGUSR1);
    return 0;
}

int osi_NetSend(asocket, addr, dvec, nvecs, asize, istack) 
    struct osi_socket *asocket;
    struct sockaddr_in *addr; 
    struct iovec dvec[];
    int nvecs;
    afs_int32 asize;
    int istack;
{
    struct sonode *so = (struct sonode *)asocket;
    struct nmsghdr msg;
    struct uio uio;
    struct iovec iov[RX_MAXIOVECS];
    int error;
    int i;
 
    if (nvecs > RX_MAXIOVECS) {
        osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);
    }

    msg.msg_name = (struct sockaddr *)addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = dvec;
    msg.msg_iovlen = nvecs;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    for (i = 0 ; i < nvecs ; i++) {
	iov[i].iov_base = dvec[i].iov_base;
	iov[i].iov_len = dvec[i].iov_len;
    }
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = nvecs;
    uio.uio_loffset = 0;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_fmode = FREAD|FWRITE;
    uio.uio_limit = 0;
    uio.uio_resid = asize;

    error = sockfs_sosendmsg(so, &msg, &uio);

    return error;
}

int osi_NetReceive(asocket, addr, dvec, nvecs, alength)
    struct osi_socket *asocket;
    struct sockaddr_in *addr;
    struct iovec *dvec;
    int nvecs;
    int *alength;
{
    struct sonode *so = (struct sonode *)asocket;
    struct nmsghdr msg;
    struct uio uio;
    struct iovec iov[RX_MAXIOVECS];
    int error;
    int i;
 
    if (nvecs > RX_MAXIOVECS) {
        osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);
    }

    msg.msg_name = NULL;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = NULL;
    msg.msg_iovlen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = 0;

    for (i = 0 ; i < nvecs ; i++) {
	iov[i].iov_base = dvec[i].iov_base;
	iov[i].iov_len = dvec[i].iov_len;
    }
    uio.uio_iov = &iov[0];
    uio.uio_iovcnt = nvecs;
    uio.uio_loffset = 0;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_fmode = 0;
    uio.uio_limit = 0;
    uio.uio_resid = *alength;

    error = sockfs_sorecvmsg(so, &msg, &uio);
    if (error == 0) {
	if (msg.msg_name == NULL) {
	    error = -1;
	} else {
	    bcopy(msg.msg_name, addr, msg.msg_namelen);
	    kmem_free(msg.msg_name, msg.msg_namelen);
	    *alength = *alength - uio.uio_resid;
	}
    }

    if (error == EINTR && ISSIG(curthread, FORREAL)) {
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = ttoproc(curthread);
	int sig = lwp->lwp_cursig;

	if (sig == SIGKILL) {
	    mutex_enter(&p->p_lock);
	    p->p_flag &= ~SKILLED;
	    mutex_exit(&p->p_lock);
	}
	lwp->lwp_cursig = 0;
	if (lwp->lwp_curinfo) {
	    siginfofree(lwp->lwp_curinfo);
	    lwp->lwp_curinfo = NULL;
	}
    }

    return error;
}

void shutdown_rxkernel(void)
{
}

void osi_StopListener(void)
{
    osi_FreeSocket(rx_socket);
}

#else /* AFS_SUN56_ENV */

#include "../inet/common.h"
#include "../sys/tiuser.h"
#include "../sys/t_kuser.h"
#include "../sys/ioctl.h"
#include "../sys/stropts.h"
#include "../sys/stream.h"
#include "../sys/strsubr.h"
#include "../sys/vnode.h"
#include "../sys/stropts.h"
#include "../sys/tihdr.h"
#include "../sys/timod.h"
#include "../sys/fcntl.h"
#include "../sys/debug.h"
#include "../inet/common.h"
#include "../inet/mi.h"
#include "../netinet/udp.h"

extern dev_t afs_udp_rdev;


int rxi_GetIFInfo()
{
    return 0;
}


/* rxi_NewSocket, rxi_FreeSocket and osi_NetSend are from the now defunct
 * afs_osinet.c. 
 */

dev_t afs_udp_rdev = (dev_t)0;

/* Allocate a new socket at specified port in network byte order. */
struct osi_socket *rxk_NewSocket(short aport)
{
    TIUSER *udp_tiptr;
    struct t_bind *reqp, *rspp;
    afs_int32 code;
    struct sockaddr_in *myaddrp;
    struct stdata *stp;
    struct queue *q;

    AFS_STATCNT(osi_NewSocket);
    afs_udp_rdev = makedevice(11 /*CLONE*/, ddi_name_to_major("udp"));
    code = t_kopen(NULL, afs_udp_rdev, FREAD|FWRITE, &udp_tiptr, CRED());
    if (code) {
	return (struct osi_socket *)0;
    }

    code = t_kalloc(udp_tiptr, T_BIND, T_ADDR, (char **)&reqp);
    if (code) {
	t_kclose(udp_tiptr, 0);
    }
    code = t_kalloc(udp_tiptr, T_BIND, T_ADDR, (char **)&rspp);
    if (code) {
	t_kfree(udp_tiptr, (char *)reqp, T_BIND);
	t_kclose(udp_tiptr, 0);
	return (struct osi_socket *)0;
    }

    reqp->addr.len = sizeof(struct sockaddr_in);
    myaddrp = (struct sockaddr_in *) reqp->addr.buf;
    myaddrp->sin_family = AF_INET;
    myaddrp->sin_port = aport;
    myaddrp->sin_addr.s_addr = INADDR_ANY;	/* XXX Was 0 XXX */

    code = t_kbind(udp_tiptr, reqp, rspp);
    if (code) {
	t_kfree(udp_tiptr, (char *)reqp, T_BIND);
	t_kfree(udp_tiptr, (char *)rspp, T_BIND);
	t_kclose(udp_tiptr, 0);
	return (struct osi_socket *)0;
    }
    if (bcmp(reqp->addr.buf, rspp->addr.buf, rspp->addr.len)) {
	t_kfree(udp_tiptr, (char *)reqp, T_BIND);
	t_kfree(udp_tiptr, (char *)rspp, T_BIND);
	t_kclose(udp_tiptr, 0);
	return (struct osi_socket *)0;
    }
    t_kfree(udp_tiptr, (char *)reqp, T_BIND);
    t_kfree(udp_tiptr, (char *)rspp, T_BIND);

    /*
     * Set the send and receive buffer sizes.
     */
    stp = udp_tiptr->fp->f_vnode->v_stream;
    q = stp->sd_wrq;
    q->q_hiwat = rx_UdpBufSize;
    q->q_next->q_hiwat = rx_UdpBufSize;
    RD(q)->q_hiwat = rx_UdpBufSize;

    return (struct osi_socket *) udp_tiptr;
}


int osi_FreeSocket(asocket)
    register struct osi_socket *asocket; 
{
    extern int rxk_ListenerPid;
    TIUSER *udp_tiptr = (TIUSER *) asocket;    
    AFS_STATCNT(osi_FreeSocket);

    if (rxk_ListenerPid)
	kill(rxk_ListenerPid, SIGUSR1);
    return 0;
}


int osi_NetSend(asocket, addr, dvec, nvecs, asize, istack) 
    register struct osi_socket *asocket;
    struct iovec dvec[];
    int nvecs;
    register afs_int32 asize;
    struct sockaddr_in *addr; 
    int istack;
{
    int i;
    int code;
    TIUSER *udp_tiptr = (TIUSER *) asocket;    
    struct t_kunitdata *udreq;
    struct sockaddr_in sin;
    mblk_t *bp;
    mblk_t *dbp;

    /*
     * XXX We don't do any checking on the family since it's assumed to be
     * AF_INET XXX
     */
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = addr->sin_addr.s_addr;
    sin.sin_port = addr->sin_port;

    /*
     * Get a buffer for the RX header
     */
    if (nvecs < 1) {
	osi_Panic("osi_NetSend, nvecs=%d\n", nvecs);
    }
    while (!(bp = allocb(dvec[0].iov_len, BPRI_LO))) {
	if (strwaitbuf(dvec[i].iov_len, BPRI_LO)) {
	    return (ENOSR);
	}
    }

    /* Copy the data into the buffer */
    bcopy((char *)dvec[0].iov_base, (char *)bp->b_wptr, dvec[0].iov_len);
    bp->b_datap->db_type = M_DATA;
    bp->b_wptr += dvec[0].iov_len;

    /*
     * Append each element in the iovec to the buffer
     */
    for (i = 1 ; i < nvecs ; i++) {
	/* Get a buffer for the next chunk */
	while (!(dbp = allocb(dvec[i].iov_len, BPRI_LO))) {
	    if (strwaitbuf(dvec[i].iov_len, BPRI_LO)) {
		freeb(bp);
		return (ENOSR);
	    }
	}

	/* Copy the data into the buffer */
	bcopy((char *)dvec[i].iov_base, (char *)dbp->b_wptr, dvec[i].iov_len);
	dbp->b_datap->db_type = M_DATA;
	dbp->b_wptr += dvec[i].iov_len;

	/* Append it to the message buffer */
	linkb(bp, dbp);
    }

    /*
     * Allocate and format the unitdata structure.
     */
    code = t_kalloc(udp_tiptr, T_UNITDATA, T_UDATA, (char **)&udreq);
    if (code) {
	freeb(bp);
	printf("osi_NetSend: t_kalloc failed %d\n", code);
	return code;
    }
    udreq->addr.len = sizeof(struct sockaddr_in);
    udreq->addr.maxlen = sizeof(struct sockaddr_in);
    udreq->addr.buf = (char *)kmem_alloc(sizeof(struct sockaddr_in), KM_SLEEP);
    udreq->opt.len = 0;
    udreq->opt.maxlen = 0;
    bcopy((char *)&sin, udreq->addr.buf, sizeof(struct sockaddr_in));
    udreq->udata.udata_mp = bp;
    udreq->udata.len = asize;

    code = t_ksndudata(udp_tiptr, udreq, NULL);
    if (code) {
	printf("osi_NetSend: t_ksndudata failed %d\n", code);
    }

    t_kfree(udp_tiptr, (caddr_t)udreq, T_UNITDATA);
    return code;
}


int osi_NetReceive(asocket, addr, dvec, nvecs, alength)
    struct osi_socket *asocket;
    struct sockaddr_in *addr;
    struct iovec *dvec;
    int nvecs;
    int *alength;
{
    int i;
    TIUSER *udp_tiptr = (TIUSER *) asocket;    
    struct t_kunitdata *udreq;
    mblk_t *dbp;
    char *phandle;
    short sport;
    int code = 0;
    int length;
    int tlen;
    int blen;
    char *tbase;
    int type;
    int error;
    int events;

    /*
     * Allocate the unitdata structure.
     */
    code = t_kalloc(udp_tiptr, T_UNITDATA, T_UDATA, (char **)&udreq);
    if (code) {
	printf("osi_NetReceive: t_kalloc failed %d\n", code);
	return code;
    }
    udreq->addr.len = sizeof(struct sockaddr_in);
    udreq->addr.maxlen = sizeof(struct sockaddr_in);
    udreq->addr.buf = (char *)kmem_alloc(sizeof(struct sockaddr_in), KM_SLEEP);
    udreq->opt.len = 0;
    udreq->opt.maxlen = 0;

    /*
     * Loop until we get an error or receive some data.
     */
    while(1) {
	/*
	 * Wait until there is something to do
	 */
	code = t_kspoll(udp_tiptr, -1, READWAIT, &events);
	if (events == 0) {
	    osi_Panic("osi_NetReceive, infinite t_kspoll timed out\n");
	}
	/*
	 * If there is a message then read it in
	 */
	if (code == 0) {
	    code = t_krcvudata(udp_tiptr, udreq, &type, &error);
	}

	/*
	 * Block attempts to kill this thread
	 */
	if (code == EINTR && ISSIG(curthread, FORREAL)) {
	    klwp_t *lwp = ttolwp(curthread);
	    proc_t *p = ttoproc(curthread);
	    int sig = lwp->lwp_cursig;

	    if (sig == SIGKILL) {
		mutex_enter(&p->p_lock);
		p->p_flag &= ~SKILLED;
		mutex_exit(&p->p_lock);
	    }
	    lwp->lwp_cursig = 0;
	    if (lwp->lwp_curinfo) {
		kmem_free((caddr_t)lwp->lwp_curinfo, sizeof(*lwp->lwp_curinfo));
		lwp->lwp_curinfo = NULL;
	    }
	}

	if (code) {
	    break;
	}

	/*
	 * Ignore non-data message types
	 */
	if (type != T_DATA) {
	    continue;
	}

	/*
	 * Save the source address
	 */
	bcopy(udreq->addr.buf, (char *)addr, sizeof(struct sockaddr_in));

	/*
	 * Copy out the message buffers, take care not to overflow
	 * the I/O vector.
	 */
        dbp = udreq->udata.udata_mp;
	length = *alength;
        for (i = 0 ; dbp != NULL && length > 0 && i < nvecs ; i++) {
	    tlen = dvec[i].iov_len;
	    tbase = dvec[i].iov_base;
	    if (tlen > length) {
		tlen = length;
	    }
	    while (dbp != NULL && tlen > 0) {
		blen = dbp->b_wptr - dbp->b_rptr;
		if (blen > tlen) {
		    bcopy((char *)dbp->b_rptr, tbase, tlen);
		    length -= tlen;
		    dbp->b_rptr += tlen;
		    tlen = 0;
		} else {
		    bcopy((char *)dbp->b_rptr, tbase, blen);
		    length -= blen;
		    tlen -= blen;
		    tbase += blen;
		    dbp = dbp->b_cont;
		}
	    }
        }
	*alength = *alength - length;
	break;
    }

    t_kfree(udp_tiptr, (caddr_t)udreq, T_UNITDATA);
    return code;
}


void osi_StopListener(void)
{
    osi_FreeSocket(rx_socket);
}


void shutdown_rxkernel(void)
{
}


#endif /* AFS_SUN56_ENV */
#endif /* AFS_SUN5_ENV */
