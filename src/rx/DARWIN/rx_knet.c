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
#include "afs/opr.h"

#ifdef AFS_SOCKPROXY_ENV
# include <afs/afs_args.h>
#endif

#include "rx/rx_kcommon.h"
#include "rx/rx_atomic.h"
#include "rx/rx_internal.h"
#include "rx/rx_packet.h"
#include "rx/rx_stats.h"

#ifdef AFS_DARWIN80_ENV
#define soclose sock_close
#endif

#ifdef RXK_UPCALL_ENV

struct afs_pkt_hdr;
static void rx_upcall_common(socket_t so, struct afs_pkt_hdr *pkt);

# ifdef AFS_SOCKPROXY_ENV

/*
 * Here is the kernel portion of our implementation of a userspace "proxy" for
 * socket operations (aka "sockproxy"). This is required on macOS after 10.15,
 * because DARWIN no longer provides an in-kernel socket API, so we need to get
 * userspace to handle our networking. Here's how it generally works:
 *
 * During startup, afsd forks a process that invokes the
 * AFSOP_SOCKPROXY_HANDLER syscall, which takes a struct afs_uspc_param, just
 * like with AFSOP_BKG_HANDLER. Each sockproxy afsd process has an "index"
 * associated with it, which ties the process to the corresponding entry in the
 * rx_sockproxy_ch.proc array. We have a few afsd sockproxy processes:
 *
 * - The receiver process, which calls recvmsg() to receive packets from the
 * net. This always uses index 0.
 *
 * - The sender processes, which calls sendmsg() to send packets to the net.
 * These run on indices 1 through (AFS_SOCKPROXY_NPROCS - 1).
 *
 * During startup, we only have one afsd sockproxy process, which waits for a
 * AFS_USPC_SOCKPROXY_START message. When afsd gets that message, it creates
 * the socket for sending and receiving packets from the net, and then fork()s
 * to create the other sockproxy procs, which then send and receive packets.
 *
 * When we need to send a packet, our osi_NetSend goes through
 * SockProxyRequest, which finds an idle afsd sockproxy process (or waits for
 * one to exist), and populates the relevant arguments, wakes up the afsd
 * process, and waits for a response. The afsd process sends the packet, then
 * responds with an error code, going through rxk_SockProxyReply which looks up
 * the corresponding request and wakes up the SockProxyRequest caller.
 *
 * When receiving packets, there is no request/reply mechanism. Instead, the
 * afsd process just receives whatever packets it can get, and submits them via
 * AFS_USPC_SOCKPROXY_RECV. The packets are processed via rx_upcall_sockproxy,
 * similar to the rx_upcall mechanism that existed for previous versions of
 * DARWIN.
 *
 * When the client has started shutting down, all calls to
 * AFSOP_SOCKPROXY_HANDLER will respond with AFS_USPC_SHUTDOWN, which tells the
 * afsd process to exit. We wait for all non-receiver sockproxy procs to get
 * the AFS_USPC_SHUTDOWN message before continuing with the shutdown process.
 *
 * For the receiver process, we can't just wait for it to get the
 * AFS_USPC_SHUTDOWN message, since it may be blocked in recvmsg(). Instead, we
 * rely on one of the other afsd sockproxy procs to kill the receiver process
 * with a signal. If the receiver process is inside an afs syscall, it'll get
 * a AFS_USPC_SHUTDOWN respond anyway, but if it's in userspace, it'll just get
 * killed by the signal. We know that someone must be handling killing the
 * receiver process, since we wait for all of the other sockproxy procs to be
 * notified of the shutdown.
 */

struct rx_sockproxy_proc {
    struct opr_queue entry;	/* chain of processes available for use */
    int inited;			/* process successfully initialized */
    int  op;			/* operation to be performed (AFS_USPC_SOCKPROXY_*) */
    char pending;		/* waiting for a reply from userspace */
    char complete;		/* response received from userspace */
    int ret;			/* value returned by op executed on userspace */
    afs_uint32 addr;		/* ipv4 address for socket */
    afs_uint32 port;		/* port for socket */
    struct afs_pkt_hdr *pkts;	/* packets to be sent */
    int npkts;			/* number of packets to be sent */
    afs_kcondvar_t cv_op;	/* request / reply received (lock: rx_sockproxy_channel.lock) */
};

struct rx_sockproxy_channel {
    int shutdown;
    /*
     * processes running on userspace, each with a specific role:
     * proc[0]: recvmsg.
     * proc[1]: socket, setsockopt, bind, and sendmsg.
     * ...
     * proc[AFS_SOCKPROXY_NPROCS-2]: sendmsg.
     * proc[AFS_SOCKPROXY_NPROCS-1]: sendmsg.
     */
    struct rx_sockproxy_proc proc[AFS_SOCKPROXY_NPROCS];
    struct opr_queue procq;
    afs_kcondvar_t cv_procq;
    afs_kmutex_t lock;
};
/* communication channel between SockProxyRequest and rxk_SockProxyReply */
static struct rx_sockproxy_channel rx_sockproxy_ch;

/* osi_socket returned by rxk_NewSocketHost on success */
static osi_socket *SockProxySocket;

/* number of afsd sockproxy processes running (not counting the receive
 * process).  Protected by GLOCK. */
static int afs_sockproxy_procs;

/**
 * Return process that provides the given operation.
 *
 * @param[in]  a_op  operation
 *
 * @return process on success; NULL otherwise.
 *
 * @pre rx_sockproxy_ch.lock held
 */
static struct rx_sockproxy_proc *
SockProxyGetProc(int a_op)
{
    struct rx_sockproxy_proc *proc = NULL;
    struct rx_sockproxy_channel *ch = &rx_sockproxy_ch;

    switch (a_op) {
    case AFS_USPC_SHUTDOWN:
    case AFS_USPC_SOCKPROXY_START:
    case AFS_USPC_SOCKPROXY_SEND:
	/* These are normal operations to process a request for. */
	break;
    default:
	/*
	 * Any other request shouldn't be going through the SockProxyRequest
	 * framework, so something weird is going on.
	 */
	printf("afs: SockProxyGetProc internal error: op %d not found.\n", a_op);
	return NULL;
    }

    while (!ch->shutdown && opr_queue_IsEmpty(&ch->procq)) {
	/* no process available */
	CV_WAIT(&ch->cv_procq, &ch->lock);
    }
    if (ch->shutdown) {
	return NULL;
    }
    proc = opr_queue_First(&ch->procq, struct rx_sockproxy_proc, entry);
    opr_queue_Remove(&proc->entry);

    return proc;
}

/**
 * Delegate given operation to user-space process.
 *
 * @param[in]  a_op    operation
 * @param[in]  a_sin   address assigned to socket
 * @param[in]  a_pkts  packets to be sent by process
 * @param[in]  a_npkts number of pkts
 *
 * @return value returned by the requested operation.
 */
static int
SockProxyRequest(int a_op, struct sockaddr_in *a_sin,
		 struct afs_pkt_hdr *a_pkts, int a_npkts)
{
    int ret = -1;
    struct rx_sockproxy_channel *ch = &rx_sockproxy_ch;
    struct rx_sockproxy_proc *proc;

    MUTEX_ENTER(&ch->lock);

    proc = SockProxyGetProc(a_op);
    if (proc == NULL) {
	/* proc not found or shutting down */
	goto done;
    }

    if (a_op == AFS_USPC_SOCKPROXY_START) {
	if (a_sin == NULL) {
	    printf("SockProxyRequest: _SOCKPROXY_START given NULL sin\n");
	    goto done;
	}
	proc->addr = a_sin->sin_addr.s_addr;
	proc->port = a_sin->sin_port;
    }
    if (a_op == AFS_USPC_SOCKPROXY_SEND) {
	if (a_pkts == NULL) {
	    printf("SockProxyRequest: _SOCKPROXY_SEND given NULL pkts\n");
	    goto done;
	}
	proc->pkts = a_pkts;
	proc->npkts = a_npkts;
    }

    proc->op = a_op;
    proc->pending = 1;
    proc->complete = 0;

    CV_BROADCAST(&proc->cv_op);
    /* if shutting down, there is no need to wait for the response from the
     * userspace process since it will exit and never return. */
    if (proc->op == AFS_USPC_SHUTDOWN) {
	struct rx_sockproxy_proc *p;

	while (!opr_queue_IsEmpty(&ch->procq)) {
	    /* wake up procs waiting for a request so they can shutdown */
	    p = opr_queue_First(&ch->procq, struct rx_sockproxy_proc, entry);
	    opr_queue_Remove(&p->entry);

	    p->op = AFS_USPC_SHUTDOWN;
	    CV_BROADCAST(&p->cv_op);
	}
	ch->shutdown = 1;
	/* wake up other threads waiting for a proc so they can return */
	CV_BROADCAST(&ch->cv_procq);

	ret = 0;
	goto done;
    }

    /* wait for response from userspace process */
    while (!proc->complete) {
	CV_WAIT(&proc->cv_op, &ch->lock);
    }
    ret = proc->ret;

    /* add proc to the queue of procs available for use */
    opr_queue_Append(&ch->procq, &proc->entry);
    CV_BROADCAST(&ch->cv_procq);
 done:
    MUTEX_EXIT(&ch->lock);
    return ret;
}

/**
 * Send packets to the given address.
 *
 * @param[in]  so       not used
 * @param[in]  addr     destination address
 * @param[in]  dvec     vector holding data to be sent
 * @param[in]  nvecs    number of dvec entries
 * @param[in]  alength  packet size
 * @param[in]  istack   not used
 *
 * @return 0 on success.
 */
int
osi_NetSend(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	    int nvecs, afs_int32 alength, int istack)
{
    int iov_i, code;
    int haveGlock;

    struct afs_pkt_hdr pkt;
    int npkts, nbytes, n_sent;
    char *payloadp;

    npkts = 1; /* for now, send one packet at a time */
    nbytes = 0;

    AFS_STATCNT(osi_NetSend);

    memset(&pkt, 0, sizeof(pkt));
    haveGlock = ISAFS_GLOCK();

    if (nvecs > RX_MAXIOVECS) {
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);
    }
    if (alength > AFS_SOCKPROXY_PAYLOAD_MAX) {
	osi_Panic("osi_NetSend: %d: Payload is too big.\n", alength);
    }
    if ((afs_termState == AFSOP_STOP_RXK_LISTENER) ||
	(afs_termState == AFSOP_STOP_COMPLETE)) {
	return -1;
    }

    if (haveGlock) {
	AFS_GUNLOCK();
    }

    pkt.addr = addr->sin_addr.s_addr;
    pkt.port = addr->sin_port;
    pkt.size = alength;
    pkt.payload = rxi_Alloc(alength);

    payloadp = pkt.payload;
    for (iov_i = 0; iov_i < nvecs; iov_i++) {
	nbytes += dvec[iov_i].iov_len;
	osi_Assert(nbytes <= alength);

	memcpy(payloadp, dvec[iov_i].iov_base, dvec[iov_i].iov_len);
	payloadp += dvec[iov_i].iov_len;
    }

    /* returns the number of packets sent */
    n_sent = SockProxyRequest(AFS_USPC_SOCKPROXY_SEND, NULL, &pkt, npkts);
    if (n_sent > 0) {
	/* success */
	code = 0;
    } else {
	code = EIO;
    }

    rxi_Free(pkt.payload, alength);
    if (haveGlock) {
	AFS_GLOCK();
    }
    return code;
}

/**
 * Check if index is valid for a given op.
 *
 * @param[in]  a_op   operation
 * @param[in]  a_idx  index
 *
 * @return 1 if valid; 0 otherwise.
 */
static int
IsSockProxyIndexValid(int a_op, int a_idx)
{
    switch (a_op) {
    case AFS_USPC_SOCKPROXY_RECV:
	/* index 0 is reserved for the receiver */
	if (a_idx == 0)
	    return 1;
	break;

    case AFS_USPC_SHUTDOWN:
    case AFS_USPC_SOCKPROXY_START:
    case AFS_USPC_SOCKPROXY_SEND:
	/* non-receiver procs can use any index besides 0 */
	if (a_idx > 0 && a_idx < AFS_SOCKPROXY_NPROCS)
	    return 1;
	break;
    }
    return 0;
}

/**
 * Receive packet from user-space.
 *
 * @param[in]  a_pkt  packet to be received
 */
static void
rx_upcall_sockproxy(struct afs_pkt_hdr *a_pkt)
{
    /*
     * Although SockProxySocket is not used as an endpoint for communication,
     * rxi_FindService uses this information to find the correct service
     * structure.
     */
    rx_upcall_common(SockProxySocket, a_pkt);
}

/**
 * Receive response from user-space process.
 *
 * @param[in]   uspc        control information exchanged between rx and procs
 * @param[in]	pkts_recv   packets to be received
 * @param[out]	pkts_send   packets to be sent (do not free; the memory is
 *			    owned by the SockProxyRequest caller)
 *
 * @return 0 on success; -1 otherwise.
 */
int
rxk_SockProxyReply(struct afs_uspc_param *uspc,
		   struct afs_pkt_hdr *pkts_recv,
		   struct afs_pkt_hdr **pkts_send)
{
    struct rx_sockproxy_channel *ch = &rx_sockproxy_ch;
    struct rx_sockproxy_proc *proc;
    int procidx, shutdown;

    procidx = uspc->req.usp.idx;
    shutdown = 0;

    if (!IsSockProxyIndexValid(uspc->reqtype, procidx)) {
	printf("rxk_SockProxyReply: bad proc %d idx %d\n",
		uspc->reqtype, procidx);
	return -1;
    }

    MUTEX_ENTER(&ch->lock);
    proc = &ch->proc[procidx];

    /* process successfully initialized */
    if (proc->inited == 0) {
	proc->inited = 1;
	if (uspc->reqtype != AFS_USPC_SOCKPROXY_RECV) {
	    /*
	     * Don't count the _RECV process in our count of
	     * afs_sockproxy_procs. The _RECV process will be killed by one of
	     * the other sockproxy procs during shutdown, to make sure it
	     * doesn't get stuck waiting for packets.
	     */
	    MUTEX_EXIT(&ch->lock);
	    AFS_GLOCK();
	    afs_sockproxy_procs++;
	    AFS_GUNLOCK();
	    MUTEX_ENTER(&ch->lock);

	    /*
	     * Add proc to the queue of procs waiting for a request. Skip
	     * AFS_USPC_SOCKPROXY_RECV since there is no request/reply
	     * mechanism for this type of request.
	     */
	    opr_queue_Append(&ch->procq, &proc->entry);
	    CV_BROADCAST(&ch->cv_procq);
	}
    }

    /* response received from userspace process */
    if (proc->pending) {
	proc->op = -1;
	proc->pending = 0;
	proc->complete = 1;
	proc->ret = uspc->retval;

	CV_BROADCAST(&proc->cv_op);
    }

    if (ch->shutdown) {
	shutdown = 1;
	goto done;
    }

    if (uspc->reqtype == AFS_USPC_SOCKPROXY_RECV) {
	int pkt_i;

	MUTEX_EXIT(&ch->lock);
	for (pkt_i = 0; pkt_i < uspc->req.usp.npkts; pkt_i++) {
	    rx_upcall_sockproxy(&pkts_recv[pkt_i]);
	}
	return 0;
    }

    while (!proc->pending && !ch->shutdown) {
	/* wait for requests */
	CV_WAIT(&proc->cv_op, &ch->lock);
    }
    if (!IsSockProxyIndexValid(proc->op, procidx)) {
	printf("rxk_SockProxyReply: bad proc %d idx %d\n", proc->op, procidx);
	MUTEX_EXIT(&ch->lock);
	return -1;
    }
    /* request received */
    uspc->reqtype = proc->op;

    if (ch->shutdown) {
	shutdown = 1;
	goto done;
    }

    if (uspc->reqtype == AFS_USPC_SOCKPROXY_START) {
	uspc->req.usp.addr = proc->addr;
	uspc->req.usp.port = proc->port;
    }
    if (uspc->reqtype == AFS_USPC_SOCKPROXY_SEND) {
	*pkts_send = proc->pkts;
	uspc->req.usp.npkts = proc->npkts;
    }

 done:
    MUTEX_EXIT(&ch->lock);
    if (shutdown) {
	AFS_GLOCK();
	if (uspc->reqtype != AFS_USPC_SOCKPROXY_RECV &&
	    afs_sockproxy_procs > 0) {
	     /*
	      * wait for all non-recv sockproxy procs before continuing the
	      * shutdown process.
	      */
	    afs_sockproxy_procs--;
	    if (afs_sockproxy_procs == 0) {
		afs_termState = AFSOP_STOP_NETIF;
		afs_osi_Wakeup(&afs_termState);
	    }
	}
	AFS_GUNLOCK();
	/* tell the afsd process to exit */
	uspc->reqtype = AFS_USPC_SHUTDOWN;
    }
    return 0;
}

/**
 * Shutdown socket proxy.
 */
void
osi_StopNetIfPoller(void)
{
    AFS_GUNLOCK();
    SockProxyRequest(AFS_USPC_SHUTDOWN, NULL, NULL, 0);
    AFS_GLOCK();

    while (afs_termState == AFSOP_STOP_SOCKPROXY) {
	afs_osi_Sleep(&afs_termState);
    }
    if (SockProxySocket != NULL) {
	rxi_Free(SockProxySocket, sizeof(*SockProxySocket));
	SockProxySocket = NULL;
    }

    if (afs_termState == AFSOP_STOP_NETIF) {
	afs_termState = AFSOP_STOP_COMPLETE;
	osi_rxWakeup(&afs_termState);
    }
}

/**
 * Open and bind RX socket.
 *
 * @param[in]  ahost  ip address
 * @param[in]  aport  port number
 *
 * @return non-NULL on success; NULL otherwise.
 */
osi_socket *
rxk_NewSocketHost(afs_uint32 ahost, short aport)
{
    int code;
    struct sockaddr_in addr;

    AFS_STATCNT(osi_NewSocket);

    if (SockProxySocket != NULL) {
	/* Just make sure we don't init twice. */
	return SockProxySocket;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = aport;
    addr.sin_addr.s_addr = ahost;

    AFS_GUNLOCK();

    code = SockProxyRequest(AFS_USPC_SOCKPROXY_START, &addr, NULL, 0);
    if (code != 0) {
	afs_warn("rxk_NewSocketHost: Couldn't initialize socket (%d).\n", code);
	AFS_GLOCK();
	return NULL;
    }

    AFS_GLOCK();

    /*
     * success. notice that the rxk_NewSocketHost interface forces us to return
     * an osi_socket address on success. however, if AFS_SOCKPROXY_ENV is
     * defined, the socket returned by this function is not used. since the
     * caller is expecting an osi_socket, return one to represent success.
     */
    if (SockProxySocket == NULL) {
	SockProxySocket = rxi_Alloc(sizeof(*SockProxySocket));
    } else {
	/* should not happen */
	afs_warn("rxk_NewSocketHost: SockProxySocket already initialized "
		 "(this should not happen, but continuing regardless).\n");
    }
    return SockProxySocket;
}

/**
 * Open and bind RX socket to all local interfaces.
 *
 * @param[in]  aport  port number
 *
 * @return non-NULL on success; NULL otherwise.
 */
osi_socket *
rxk_NewSocket(short aport)
{
    return rxk_NewSocketHost(0, aport);
}

/**
 * Init communication channel used by SockProxyRequest and rxk_SockProxyReply.
 */
void
rxk_SockProxySetup(void)
{
    int proc_i;

    opr_queue_Init(&rx_sockproxy_ch.procq);
    CV_INIT(&rx_sockproxy_ch.cv_procq, "rx_sockproxy_cv_procq", CV_DEFAULT, 0);
    MUTEX_INIT(&rx_sockproxy_ch.lock, "rx_sockproxy_mutex", MUTEX_DEFAULT, 0);

    for (proc_i = 0; proc_i < AFS_SOCKPROXY_NPROCS; proc_i++) {
	struct rx_sockproxy_proc *proc = &rx_sockproxy_ch.proc[proc_i];

	proc->op = -1;
	CV_INIT(&proc->cv_op, "rx_sockproxy_cv_op", CV_DEFAULT, 0);
    }
}

/**
 * Destroy communication channel.
 */
void
rxk_SockProxyFinish(void)
{
    int proc_i;

    for (proc_i = 0; proc_i < AFS_SOCKPROXY_NPROCS; proc_i++) {
	struct rx_sockproxy_proc *proc = &rx_sockproxy_ch.proc[proc_i];

	CV_DESTROY(&proc->cv_op);
    }
    MUTEX_DESTROY(&rx_sockproxy_ch.lock);
    CV_DESTROY(&rx_sockproxy_ch.cv_procq);

    memset(&rx_sockproxy_ch, 0, sizeof(rx_sockproxy_ch));
}

# else	/* AFS_SOCKPROXY_ENV */

/* in listener env, the listener shutdown does this. we have no listener */
void
osi_StopNetIfPoller(void)
{
    soclose(rx_socket);
    if (afs_termState == AFSOP_STOP_NETIF) {
	afs_termState = AFSOP_STOP_COMPLETE;
	osi_rxWakeup(&afs_termState);
    }
}

void
rx_upcall(socket_t so, void *arg, __unused int waitflag)
{
    rx_upcall_common(so, NULL);
}

# endif	/* AFS_SOCKPROXY_ENV */

static void
rx_upcall_common(socket_t so, struct afs_pkt_hdr *pkt)
{
    int error = 0;
    int i, flags = 0;
    struct msghdr msg;
    struct sockaddr_storage ss;
    struct sockaddr *sa = NULL;
    struct sockaddr_in from;
    struct rx_packet *p;
    afs_int32 rlen;
    afs_int32 tlen;
    afs_int32 savelen;          /* was using rlen but had aliasing problems */
    size_t nbytes, resid, noffset;

    /* we stopped rx but the socket isn't closed yet */
    if (!rxi_IsRunning())
	return;

    /* See if a check for additional packets was issued */
    rx_CheckPackets();

    p = rxi_AllocPacket(RX_PACKET_CLASS_RECEIVE);
    rx_computelen(p, tlen);
    rx_SetDataSize(p, tlen);    /* this is the size of the user data area */
    tlen += RX_HEADER_SIZE;     /* now this is the size of the entire packet */
    rlen = rx_maxJumboRecvSize; /* this is what I am advertising.  Only check
				 * it once in order to avoid races.  */
    tlen = rlen - tlen;
    if (tlen > 0) {
	tlen = rxi_AllocDataBuf(p, tlen, RX_PACKET_CLASS_RECV_CBUF);
	if (tlen > 0) {
	    tlen = rlen - tlen;
	} else
	    tlen = rlen;
    } else
	tlen = rlen;
    /* add some padding to the last iovec, it's just to make sure that the
     * read doesn't return more data than we expect, and is done to get around
     * our problems caused by the lack of a length field in the rx header. */
    savelen = p->wirevec[p->niovecs - 1].iov_len;
    p->wirevec[p->niovecs - 1].iov_len = savelen + RX_EXTRABUFFERSIZE;

    resid = nbytes = tlen + sizeof(afs_int32);

    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &ss;
    msg.msg_namelen = sizeof(struct sockaddr_storage);
    sa =(struct sockaddr *) &ss;

# ifdef AFS_SOCKPROXY_ENV
    {
	char *payload;
	size_t sz;

	osi_Assert(pkt != NULL);

	sa->sa_family = AF_INET;
	((struct sockaddr_in*)sa)->sin_addr.s_addr = pkt->addr;
	((struct sockaddr_in*)sa)->sin_port = pkt->port;

	payload = pkt->payload;
	nbytes = pkt->size;
	resid = nbytes;
	noffset = 0;

	for (i = 0; i < p->niovecs && resid > 0; i++) {
	    sz = opr_min(resid, p->wirevec[i].iov_len);
	    memcpy(p->wirevec[i].iov_base, payload, sz);
	    resid -= sz;
	    noffset += sz;
	    payload += sz;
	}
    }
# else
    {
	mbuf_t m = NULL;
	error = sock_receivembuf(so, &msg, &m, MSG_DONTWAIT, &nbytes);
	if (!error) {
	    size_t sz, offset = 0;
	    noffset = 0;
	    resid = nbytes;
	    for (i=0;i<p->niovecs && resid;i++) {
		sz=opr_min(resid, p->wirevec[i].iov_len);
		error = mbuf_copydata(m, offset, sz, p->wirevec[i].iov_base);
		if (error)
		    break;
		resid-=sz;
		offset+=sz;
		noffset += sz;
	    }
	}

	mbuf_freem(m);
    }
# endif /* AFS_SOCKPROXY_ENV */

    /* restore the vec to its correct state */
    p->wirevec[p->niovecs - 1].iov_len = savelen;

    if (error == EWOULDBLOCK && noffset > 0)
	error = 0;

    if (!error) {
	int host, port;

	nbytes -= resid;

	if (sa->sa_family == AF_INET)
	    from = *(struct sockaddr_in *)sa;

	p->length = nbytes - RX_HEADER_SIZE;;
	if ((nbytes > tlen) || (p->length & 0x8000)) {  /* Bogus packet */
	    if (nbytes <= 0) {
		if (rx_stats_active) {
		    MUTEX_ENTER(&rx_stats_mutex);
		    rx_atomic_inc(&rx_stats.bogusPacketOnRead);
		    rx_stats.bogusHost = from.sin_addr.s_addr;
		    MUTEX_EXIT(&rx_stats_mutex);
		}
		dpf(("B: bogus packet from [%x,%d] nb=%d",
		     from.sin_addr.s_addr, from.sin_port, nbytes));
	    }
	    return;
	} else {
	    /* Extract packet header. */
	    rxi_DecodePacketHeader(p);

	    host = from.sin_addr.s_addr;
	    port = from.sin_port;
	    if (p->header.type > 0 && p->header.type <= RX_N_PACKET_TYPES) {
		if (rx_stats_active) {
		    rx_atomic_inc(&rx_stats.packetsRead[p->header.type - 1]);
		}
	    }

#ifdef RX_TRIMDATABUFS
	    /* Free any empty packet buffers at the end of this packet */
	    rxi_TrimDataBufs(p, 1);
#endif
	    /* receive pcket */
	    p = rxi_ReceivePacket(p, so, host, port, 0, 0);
	}
    }
    /* free packet? */
    if (p)
	rxi_FreePacket(p);

    return;
}

#elif defined(RXK_LISTENER_ENV)
int
osi_NetReceive(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	       int nvecs, int *alength)
{
    int i;
    struct iovec iov[RX_MAXIOVECS];
    struct sockaddr *sa = NULL;
    int code;
    size_t resid;

    int haveGlock = ISAFS_GLOCK();

#ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    struct msghdr msg;
    struct sockaddr_storage ss;
    int rlen;
    mbuf_t m;
#else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
    memset(&u, 0, sizeof(u));
#endif
    memset(&iov, 0, sizeof(iov));
    /*AFS_STATCNT(osi_NetReceive); */

    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetReceive: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    if ((afs_termState == AFSOP_STOP_RXK_LISTENER) ||
	(afs_termState == AFSOP_STOP_COMPLETE))
	return -1;

    if (haveGlock)
	AFS_GUNLOCK();
#if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
#endif
#ifdef AFS_DARWIN80_ENV
    resid = *alength;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &ss;
    msg.msg_namelen = sizeof(struct sockaddr_storage);
    sa =(struct sockaddr *) &ss;
    code = sock_receivembuf(asocket, &msg, &m, 0, alength);
    if (!code) {
        size_t offset=0,sz;
        resid = *alength;
        for (i=0;i<nvecs && resid;i++) {
            sz=opr_min(resid, iov[i].iov_len);
            code = mbuf_copydata(m, offset, sz, iov[i].iov_base);
            if (code)
                break;
            resid-=sz;
            offset+=sz;
        }
    }
    mbuf_freem(m);
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
	if (sa->sa_family == AF_INET) {
	    if (addr)
		*addr = *(struct sockaddr_in *)sa;
	} else
	    printf("Unknown socket family %d in NetReceive\n", sa->sa_family);
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
#else
#error need upcall or listener
#endif

#ifndef AFS_SOCKPROXY_ENV
int
osi_NetSend(osi_socket so, struct sockaddr_in *addr, struct iovec *dvec,
	    int nvecs, afs_int32 alength, int istack)
{
    afs_int32 code;
    int i;
    struct iovec iov[RX_MAXIOVECS];
    int haveGlock = ISAFS_GLOCK();
# ifdef AFS_DARWIN80_ENV
    socket_t asocket = (socket_t)so;
    struct msghdr msg;
    size_t slen;
# else
    struct socket *asocket = (struct socket *)so;
    struct uio u;
    memset(&u, 0, sizeof(u));
# endif
    memset(&iov, 0, sizeof(iov));

    AFS_STATCNT(osi_NetSend);
    if (nvecs > RX_MAXIOVECS)
	osi_Panic("osi_NetSend: %d: Too many iovecs.\n", nvecs);

    for (i = 0; i < nvecs; i++)
	iov[i] = dvec[i];

    addr->sin_len = sizeof(struct sockaddr_in);

    if ((afs_termState == AFSOP_STOP_RXK_LISTENER) ||
	(afs_termState == AFSOP_STOP_COMPLETE))
	return -1;

    if (haveGlock)
	AFS_GUNLOCK();

# if defined(KERNEL_FUNNEL)
    thread_funnel_switch(KERNEL_FUNNEL, NETWORK_FUNNEL);
# endif
# ifdef AFS_DARWIN80_ENV
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = addr;
    msg.msg_namelen = ((struct sockaddr *)addr)->sa_len;
    msg.msg_iov = &iov[0];
    msg.msg_iovlen = nvecs;
    code = sock_send(asocket, &msg, 0, &slen);
# else
    u.uio_iov = &iov[0];
    u.uio_iovcnt = nvecs;
    u.uio_offset = 0;
    u.uio_resid = alength;
    u.uio_segflg = UIO_SYSSPACE;
    u.uio_rw = UIO_WRITE;
    u.uio_procp = NULL;
    code = sosend(asocket, (struct sockaddr *)addr, &u, NULL, NULL, 0);
# endif

# if defined(KERNEL_FUNNEL)
    thread_funnel_switch(NETWORK_FUNNEL, KERNEL_FUNNEL);
# endif
    if (haveGlock)
	AFS_GLOCK();
    return code;
}
#endif /* !AFS_SOCKPROXY_ENV */
