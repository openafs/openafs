/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef KERNEL
# if defined(UKERNEL)
#  include "afs/sysincludes.h"
#  include "afsincludes.h"
#  include "rx_kcommon.h"
# else /* defined(UKERNEL) */
#  ifdef RX_KERNEL_TRACE
#   include "rx_kcommon.h"
#  endif
#  include "h/types.h"
#  ifndef AFS_LINUX_ENV
#   include "h/systm.h"
#  endif
#  if defined(AFS_SGI_ENV) || defined(AFS_HPUX110_ENV) || defined(AFS_NBSD50_ENV)
#   include "afs/sysincludes.h"
#  endif
#  if defined(AFS_OBSD_ENV)
#   include "h/proc.h"
#  endif
#  include "h/socket.h"
#  if !defined(AFS_SUN5_ENV) &&  !defined(AFS_LINUX_ENV) && !defined(AFS_HPUX110_ENV)
#   if	!defined(AFS_AIX41_ENV)
#    include "sys/mount.h"		/* it gets pulled in by something later anyway */
#   endif
#   include "h/mbuf.h"
#  endif
#  include "netinet/in.h"
#  include "afs/afs_osi.h"
#  include "rx_kmutex.h"
# endif /* defined(UKERNEL) */
#else /* KERNEL */
# include <roken.h>
# include <assert.h>
# include <afs/opr.h>
# if defined(AFS_NT40_ENV)
#  ifndef EWOULDBLOCK
#   define EWOULDBLOCK WSAEWOULDBLOCK
#  endif
#  include "rx_user.h"
#  include "rx_xmit_nt.h"
# endif
# include <lwp.h>
#endif /* KERNEL */

#ifdef	AFS_SUN5_ENV
# include <sys/sysmacros.h>
#endif

#include <opr/queue.h>

#include "rx.h"
#include "rx_clock.h"
#include "rx_packet.h"
#include "rx_atomic.h"
#include "rx_globals.h"
#include "rx_internal.h"
#include "rx_stats.h"

#include "rx_peer.h"
#include "rx_conn.h"
#include "rx_call.h"

/*!
 * \brief structure used to keep track of allocated packets
 */
struct rx_mallocedPacket {
    struct opr_queue entry;	/*!< chained using opr_queue */
    struct rx_packet *addr;	/*!< address of the first element */
    afs_uint32 size;		/*!< array size in bytes */
};

#ifdef RX_LOCKS_DB
/* rxdb_fileID is used to identify the lock location, along with line#. */
static int rxdb_fileID = RXDB_FILE_RX_PACKET;
#endif /* RX_LOCKS_DB */
static struct rx_packet *rx_mallocedP = 0;
#ifdef RXDEBUG_PACKET
static afs_uint32       rx_packet_id = 0;
#endif

extern char cml_version_number[];

static int AllocPacketBufs(int class, int num_pkts, struct opr_queue *q);

static void rxi_SendDebugPacket(struct rx_packet *apacket, osi_socket asocket,
				afs_uint32 ahost, short aport,
				afs_int32 istack);
static struct rx_packet *rxi_AllocPacketNoLock(int class);

#ifndef KERNEL
static void rxi_MorePacketsNoLock(int apackets);
#endif

#ifdef RX_ENABLE_TSFPQ
static int rxi_FreeDataBufsTSFPQ(struct rx_packet *p, afs_uint32 first,
				 int flush_global);
static void rxi_AdjustLocalPacketsTSFPQ(int num_keep_local,
					int allow_overcommit);
#else
static void rxi_FreePacketNoLock(struct rx_packet *p);
static int rxi_FreeDataBufsNoLock(struct rx_packet *p, afs_uint32 first);
static int rxi_FreeDataBufsToQueue(struct rx_packet *p, afs_uint32 first,
				   struct opr_queue * q);
#endif

extern struct opr_queue rx_idleServerQueue;

/* some rules about packets:
 * 1.  When a packet is allocated, the final iov_buf contains room for
 * a security trailer, but iov_len masks that fact.  If the security
 * package wants to add the trailer, it may do so, and then extend
 * iov_len appropriately.  For this reason, packet's niovecs and
 * iov_len fields should be accurate before calling PreparePacket.
*/

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of
 *	  the word size.
 *        offset is an integral multiple of the word size.
 */
afs_int32
rx_SlowGetInt32(struct rx_packet *packet, size_t offset)
{
    unsigned int i;
    size_t l;
    for (l = 0, i = 1; i < packet->niovecs; i++) {
	if (l + packet->wirevec[i].iov_len > offset) {
	    return
		*((afs_int32 *) ((char *)(packet->wirevec[i].iov_base) +
				 (offset - l)));
	}
	l += packet->wirevec[i].iov_len;
    }

    return 0;
}

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the word size.
 *        offset is an integral multiple of the word size.
 */
afs_int32
rx_SlowPutInt32(struct rx_packet * packet, size_t offset, afs_int32 data)
{
    unsigned int i;
    size_t l;
    for (l = 0, i = 1; i < packet->niovecs; i++) {
	if (l + packet->wirevec[i].iov_len > offset) {
	    *((afs_int32 *) ((char *)(packet->wirevec[i].iov_base) +
			     (offset - l))) = data;
	    return 0;
	}
	l += packet->wirevec[i].iov_len;
    }

    return 0;
}

/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the
 *        word size.
 *        offset is an integral multiple of the word size.
 * Packet Invariants:
 *         all buffers are contiguously arrayed in the iovec from 0..niovecs-1
 */
afs_int32
rx_SlowReadPacket(struct rx_packet * packet, unsigned int offset, int resid,
		  char *out)
{
    unsigned int i, j, l, r;
    for (l = 0, i = 1; i < packet->niovecs; i++) {
	if (l + packet->wirevec[i].iov_len > offset) {
	    break;
	}
	l += packet->wirevec[i].iov_len;
    }

    /* i is the iovec which contains the first little bit of data in which we
     * are interested.  l is the total length of everything prior to this iovec.
     * j is the number of bytes we can safely copy out of this iovec.
     * offset only applies to the first iovec.
     */
    r = resid;
    while ((r > 0) && (i < packet->niovecs)) {
	j = MIN(r, packet->wirevec[i].iov_len - (offset - l));
	memcpy(out, (char *)(packet->wirevec[i].iov_base) + (offset - l), j);
	r -= j;
        out += j;
  	l += packet->wirevec[i].iov_len;
	offset = l;
	i++;
    }

    return (r ? (resid - r) : resid);
}


/* Preconditions:
 *        all packet buffers (iov_base) are integral multiples of the
 *        word size.
 *        offset is an integral multiple of the word size.
 */
afs_int32
rx_SlowWritePacket(struct rx_packet * packet, int offset, int resid, char *in)
{
    unsigned int i, j, l, o, r;
    char *b;

    for (l = 0, i = 1, o = offset; i < packet->niovecs; i++) {
	if (l + packet->wirevec[i].iov_len > o) {
	    break;
	}
	l += packet->wirevec[i].iov_len;
    }

    /* i is the iovec which contains the first little bit of data in which we
     * are interested.  l is the total length of everything prior to this iovec.
     * j is the number of bytes we can safely copy out of this iovec.
     * offset only applies to the first iovec.
     */
    r = resid;
    while ((r > 0) && (i <= RX_MAXWVECS)) {
	if (i >= packet->niovecs)
	    if (rxi_AllocDataBuf(packet, r, RX_PACKET_CLASS_SEND_CBUF) > 0)	/* ++niovecs as a side-effect */
		break;

	b = (char *)(packet->wirevec[i].iov_base) + (offset - l);
	j = MIN(r, packet->wirevec[i].iov_len - (offset - l));
	memcpy(b, in, j);
	r -= j;
        in += j;
	l += packet->wirevec[i].iov_len;
	offset = l;
	i++;
    }

    return (r ? (resid - r) : resid);
}

int
rxi_AllocPackets(int class, int num_pkts, struct opr_queue * q)
{
    struct opr_queue *c;

    num_pkts = AllocPacketBufs(class, num_pkts, q);

    for (opr_queue_Scan(q, c)) {
        RX_PACKET_IOV_FULLINIT(opr_queue_Entry(c, struct rx_packet, entry));
    }

    return num_pkts;
}

#ifdef RX_ENABLE_TSFPQ
static int
AllocPacketBufs(int class, int num_pkts, struct opr_queue * q)
{
    struct rx_ts_info_t * rx_ts_info;
    int transfer;
    SPLVAR;

    RX_TS_INFO_GET(rx_ts_info);

    transfer = num_pkts - rx_ts_info->_FPQ.len;
    if (transfer > 0) {
        NETPRI;
        MUTEX_ENTER(&rx_freePktQ_lock);
	transfer = MAX(transfer, rx_TSFPQGlobSize);
	if (transfer > rx_nFreePackets) {
	    /* alloc enough for us, plus a few globs for other threads */
	    rxi_MorePacketsNoLock(transfer + 4 * rx_initSendWindow);
	}

	RX_TS_FPQ_GTOL2(rx_ts_info, transfer);

	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }

    RX_TS_FPQ_QCHECKOUT(rx_ts_info, num_pkts, q);

    return num_pkts;
}
#else /* RX_ENABLE_TSFPQ */
static int
AllocPacketBufs(int class, int num_pkts, struct opr_queue * q)
{
    struct rx_packet *c;
    int i;
#ifdef KERNEL
    int overq = 0;
#endif
    SPLVAR;

    NETPRI;

    MUTEX_ENTER(&rx_freePktQ_lock);

#ifdef KERNEL
    for (; (num_pkts > 0) && (rxi_OverQuota2(class,num_pkts));
	 num_pkts--, overq++);

    if (overq) {
	rxi_NeedMorePackets = TRUE;
        if (rx_stats_active) {
            switch (class) {
            case RX_PACKET_CLASS_RECEIVE:
                rx_atomic_inc(&rx_stats.receivePktAllocFailures);
                break;
            case RX_PACKET_CLASS_SEND:
                rx_atomic_inc(&rx_stats.sendPktAllocFailures);
                break;
            case RX_PACKET_CLASS_SPECIAL:
                rx_atomic_inc(&rx_stats.specialPktAllocFailures);
                break;
            case RX_PACKET_CLASS_RECV_CBUF:
                rx_atomic_inc(&rx_stats.receiveCbufPktAllocFailures);
                break;
            case RX_PACKET_CLASS_SEND_CBUF:
                rx_atomic_inc(&rx_stats.sendCbufPktAllocFailures);
                break;
            }
	}
    }

    if (rx_nFreePackets < num_pkts)
        num_pkts = rx_nFreePackets;

    if (!num_pkts) {
	rxi_NeedMorePackets = TRUE;
	goto done;
    }
#else /* KERNEL */
    if (rx_nFreePackets < num_pkts) {
	rxi_MorePacketsNoLock(MAX((num_pkts-rx_nFreePackets), 4 * rx_initSendWindow));
    }
#endif /* KERNEL */

    for (i=0, c=opr_queue_First(&rx_freePacketQueue, struct rx_packet, entry);
	 i < num_pkts;
	 i++, c=opr_queue_Next(&c->entry, struct rx_packet, entry)) {
        RX_FPQ_MARK_USED(c);
    }

    opr_queue_SplitBeforeAppend(&rx_freePacketQueue, q, &c->entry);

    rx_nFreePackets -= num_pkts;

#ifdef KERNEL
  done:
#endif
    MUTEX_EXIT(&rx_freePktQ_lock);

    USERPRI;
    return num_pkts;
}
#endif /* RX_ENABLE_TSFPQ */

/*
 * Free a packet currently used as a continuation buffer
 */
#ifdef RX_ENABLE_TSFPQ
/* num_pkts=0 means queue length is unknown */
int
rxi_FreePackets(int num_pkts, struct opr_queue * q)
{
    struct rx_ts_info_t * rx_ts_info;
    struct opr_queue *cursor, *store;
    SPLVAR;

    osi_Assert(num_pkts >= 0);
    RX_TS_INFO_GET(rx_ts_info);

    if (!num_pkts) {
	for (opr_queue_ScanSafe(q, cursor, store)) {
	    num_pkts++;
	    rxi_FreeDataBufsTSFPQ(opr_queue_Entry(cursor, struct rx_packet, 
				    		 entry), 2, 0);
	}
    } else {
	for (opr_queue_ScanSafe(q, cursor, store)) {
	    rxi_FreeDataBufsTSFPQ(opr_queue_Entry(cursor, struct rx_packet, 
				    		 entry), 2, 0);
	}
    }

    if (num_pkts) {
	RX_TS_FPQ_QCHECKIN(rx_ts_info, num_pkts, q);
    }

    if (rx_ts_info->_FPQ.len > rx_TSFPQLocalMax) {
        NETPRI;
	MUTEX_ENTER(&rx_freePktQ_lock);

	RX_TS_FPQ_LTOG(rx_ts_info);

	/* Wakeup anyone waiting for packets */
	rxi_PacketsUnWait();

	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }

    return num_pkts;
}
#else /* RX_ENABLE_TSFPQ */
/* num_pkts=0 means queue length is unknown */
int
rxi_FreePackets(int num_pkts, struct opr_queue *q)
{
    struct opr_queue cbs;
    struct opr_queue *cursor, *store;
    int qlen = 0;
    SPLVAR;

    osi_Assert(num_pkts >= 0);
    opr_queue_Init(&cbs);

    if (!num_pkts) {
        for (opr_queue_ScanSafe(q, cursor, store)) {
	    struct rx_packet *p
		= opr_queue_Entry(cursor, struct rx_packet, entry);
	    if (p->niovecs > 2) {
		qlen += rxi_FreeDataBufsToQueue(p, 2, &cbs);
	    }
            RX_FPQ_MARK_FREE(p);
	    num_pkts++;
	}
	if (!num_pkts)
	    return 0;
    } else {
        for (opr_queue_ScanSafe(q, cursor, store)) {
	    struct rx_packet *p
		= opr_queue_Entry(cursor, struct rx_packet, entry);

	    if (p->niovecs > 2) {
		qlen += rxi_FreeDataBufsToQueue(p, 2, &cbs);
	    }
            RX_FPQ_MARK_FREE(p);
	}
    }

    if (qlen) {
	opr_queue_SpliceAppend(q, &cbs);
	qlen += num_pkts;
    } else
	qlen = num_pkts;

    NETPRI;
    MUTEX_ENTER(&rx_freePktQ_lock);

    opr_queue_SpliceAppend(&rx_freePacketQueue, q);
    rx_nFreePackets += qlen;

    /* Wakeup anyone waiting for packets */
    rxi_PacketsUnWait();

    MUTEX_EXIT(&rx_freePktQ_lock);
    USERPRI;

    return num_pkts;
}
#endif /* RX_ENABLE_TSFPQ */

/* this one is kind of awful.
 * In rxkad, the packet has been all shortened, and everything, ready for
 * sending.  All of a sudden, we discover we need some of that space back.
 * This isn't terribly general, because it knows that the packets are only
 * rounded up to the EBS (userdata + security header).
 */
int
rxi_RoundUpPacket(struct rx_packet *p, unsigned int nb)
{
    int i;
    i = p->niovecs - 1;
    if (p->wirevec[i].iov_base == (caddr_t) p->localdata) {
	if (p->wirevec[i].iov_len <= RX_FIRSTBUFFERSIZE - nb) {
	    p->wirevec[i].iov_len += nb;
	    return 0;
	}
    } else {
	if (p->wirevec[i].iov_len <= RX_CBUFFERSIZE - nb) {
	    p->wirevec[i].iov_len += nb;
	    return 0;
	}
    }

    return 0;
}

/* get sufficient space to store nb bytes of data (or more), and hook
 * it into the supplied packet.  Return nbytes<=0 if successful, otherwise
 * returns the number of bytes >0 which it failed to come up with.
 * Don't need to worry about locking on packet, since only
 * one thread can manipulate one at a time. Locking on continution
 * packets is handled by AllocPacketBufs */
/* MTUXXX don't need to go throught the for loop if we can trust niovecs */
int
rxi_AllocDataBuf(struct rx_packet *p, int nb, int class)
{
    int i, nv;
    struct opr_queue q, *cursor, *store;

    /* compute the number of cbuf's we need */
    nv = nb / RX_CBUFFERSIZE;
    if ((nv * RX_CBUFFERSIZE) < nb)
        nv++;
    if ((nv + p->niovecs) > RX_MAXWVECS)
        nv = RX_MAXWVECS - p->niovecs;
    if (nv < 1)
        return nb;

    /* allocate buffers */
    opr_queue_Init(&q);
    nv = AllocPacketBufs(class, nv, &q);

    /* setup packet iovs */
    i = p ->niovecs;
    for (opr_queue_ScanSafe(&q, cursor, store)) {
	struct rx_packet *cb
	    = opr_queue_Entry(cursor, struct rx_packet, entry);

        opr_queue_Remove(&cb->entry);
        p->wirevec[i].iov_base = (caddr_t) cb->localdata;
        p->wirevec[i].iov_len = RX_CBUFFERSIZE;
	i++;
    }

    nb -= (nv * RX_CBUFFERSIZE);
    p->length += (nv * RX_CBUFFERSIZE);
    p->niovecs += nv;

    return nb;
}

/**
 * Register allocated packets.
 *
 * @param[in] addr array of packets
 * @param[in] npkt number of packets
 *
 * @return none
 */
static void
registerPackets(struct rx_packet *addr, afs_uint32 npkt)
{
    struct rx_mallocedPacket *mp;

    mp = osi_Alloc(sizeof(*mp));

    osi_Assert(mp != NULL);
    memset(mp, 0, sizeof(*mp));

    mp->addr = addr;
    mp->size = npkt * sizeof(struct rx_packet);
    osi_Assert(npkt <= MAX_AFS_UINT32 / sizeof(struct rx_packet));

    MUTEX_ENTER(&rx_mallocedPktQ_lock);
    opr_queue_Append(&rx_mallocedPacketQueue, &mp->entry);
    MUTEX_EXIT(&rx_mallocedPktQ_lock);
}

/* Add more packet buffers */
#ifdef RX_ENABLE_TSFPQ
void
rxi_MorePackets(int apackets)
{
    struct rx_packet *p, *e;
    struct rx_ts_info_t * rx_ts_info;
    int getme;
    SPLVAR;

    getme = apackets * sizeof(struct rx_packet);
    p = osi_Alloc(getme);
    osi_Assert(p);
    registerPackets(p, apackets);

    PIN(p, getme);		/* XXXXX */
    memset(p, 0, getme);
    RX_TS_INFO_GET(rx_ts_info);

    RX_TS_FPQ_LOCAL_ALLOC(rx_ts_info,apackets);
    /* TSFPQ patch also needs to keep track of total packets */

    MUTEX_ENTER(&rx_packets_mutex);
    rx_nPackets += apackets;
    RX_TS_FPQ_COMPUTE_LIMITS;
    MUTEX_EXIT(&rx_packets_mutex);

    for (e = p + apackets; p < e; p++) {
        RX_PACKET_IOV_INIT(p);
	p->niovecs = 2;

	RX_TS_FPQ_CHECKIN(rx_ts_info,p);

        NETPRI;
        MUTEX_ENTER(&rx_freePktQ_lock);
#ifdef RXDEBUG_PACKET
        p->packetId = rx_packet_id++;
        p->allNextp = rx_mallocedP;
#endif /* RXDEBUG_PACKET */
        rx_mallocedP = p;
        MUTEX_EXIT(&rx_freePktQ_lock);
        USERPRI;
    }
    rx_ts_info->_FPQ.delta += apackets;

    if (rx_ts_info->_FPQ.len > rx_TSFPQLocalMax) {
        NETPRI;
	MUTEX_ENTER(&rx_freePktQ_lock);

	RX_TS_FPQ_LTOG(rx_ts_info);
	rxi_NeedMorePackets = FALSE;
	rxi_PacketsUnWait();

	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }
}
#else /* RX_ENABLE_TSFPQ */
void
rxi_MorePackets(int apackets)
{
    struct rx_packet *p, *e;
    int getme;
    SPLVAR;

    getme = apackets * sizeof(struct rx_packet);
    p = osi_Alloc(getme);
    osi_Assert(p);
    registerPackets(p, apackets);

    PIN(p, getme);		/* XXXXX */
    memset(p, 0, getme);
    NETPRI;
    MUTEX_ENTER(&rx_freePktQ_lock);

    for (e = p + apackets; p < e; p++) {
        RX_PACKET_IOV_INIT(p);
#ifdef RX_TRACK_PACKETS
	p->flags |= RX_PKTFLAG_FREE;
#endif
	p->niovecs = 2;

	opr_queue_Append(&rx_freePacketQueue, &p->entry);
#ifdef RXDEBUG_PACKET
        p->packetId = rx_packet_id++;
        p->allNextp = rx_mallocedP;
#endif /* RXDEBUG_PACKET */
	rx_mallocedP = p;
    }

    rx_nPackets += apackets;
    rx_nFreePackets += apackets;
    rxi_NeedMorePackets = FALSE;
    rxi_PacketsUnWait();

    MUTEX_EXIT(&rx_freePktQ_lock);
    USERPRI;
}
#endif /* RX_ENABLE_TSFPQ */

#ifdef RX_ENABLE_TSFPQ
void
rxi_MorePacketsTSFPQ(int apackets, int flush_global, int num_keep_local)
{
    struct rx_packet *p, *e;
    struct rx_ts_info_t * rx_ts_info;
    int getme;
    SPLVAR;

    getme = apackets * sizeof(struct rx_packet);
    p = osi_Alloc(getme);
    registerPackets(p, apackets);

    PIN(p, getme);		/* XXXXX */
    memset(p, 0, getme);
    RX_TS_INFO_GET(rx_ts_info);

    RX_TS_FPQ_LOCAL_ALLOC(rx_ts_info,apackets);
    /* TSFPQ patch also needs to keep track of total packets */
    MUTEX_ENTER(&rx_packets_mutex);
    rx_nPackets += apackets;
    RX_TS_FPQ_COMPUTE_LIMITS;
    MUTEX_EXIT(&rx_packets_mutex);

    for (e = p + apackets; p < e; p++) {
        RX_PACKET_IOV_INIT(p);
	p->niovecs = 2;
	RX_TS_FPQ_CHECKIN(rx_ts_info,p);

        NETPRI;
        MUTEX_ENTER(&rx_freePktQ_lock);
#ifdef RXDEBUG_PACKET
        p->packetId = rx_packet_id++;
        p->allNextp = rx_mallocedP;
#endif /* RXDEBUG_PACKET */
        rx_mallocedP = p;
        MUTEX_EXIT(&rx_freePktQ_lock);
        USERPRI;
    }
    rx_ts_info->_FPQ.delta += apackets;

    if (flush_global &&
        (num_keep_local < apackets)) {
        NETPRI;
	MUTEX_ENTER(&rx_freePktQ_lock);

	RX_TS_FPQ_LTOG2(rx_ts_info, (apackets - num_keep_local));
	rxi_NeedMorePackets = FALSE;
	rxi_PacketsUnWait();

	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }
}
#endif /* RX_ENABLE_TSFPQ */

#ifndef KERNEL
/* Add more packet buffers */
static void
rxi_MorePacketsNoLock(int apackets)
{
#ifdef RX_ENABLE_TSFPQ
    struct rx_ts_info_t * rx_ts_info;
#endif /* RX_ENABLE_TSFPQ */
    struct rx_packet *p, *e;
    int getme;

    /* allocate enough packets that 1/4 of the packets will be able
     * to hold maximal amounts of data */
    apackets += (apackets / 4)
	* ((rx_maxJumboRecvSize - RX_FIRSTBUFFERSIZE) / RX_CBUFFERSIZE);
    do {
        getme = apackets * sizeof(struct rx_packet);
        p = osi_Alloc(getme);
	if (p == NULL) {
            apackets -= apackets / 4;
            osi_Assert(apackets > 0);
        }
    } while(p == NULL);
    memset(p, 0, getme);
    registerPackets(p, apackets);

#ifdef RX_ENABLE_TSFPQ
    RX_TS_INFO_GET(rx_ts_info);
    RX_TS_FPQ_GLOBAL_ALLOC(rx_ts_info,apackets);
#endif /* RX_ENABLE_TSFPQ */

    for (e = p + apackets; p < e; p++) {
        RX_PACKET_IOV_INIT(p);
#ifdef RX_TRACK_PACKETS
	p->flags |= RX_PKTFLAG_FREE;
#endif
	p->niovecs = 2;

	opr_queue_Append(&rx_freePacketQueue, &p->entry);
#ifdef RXDEBUG_PACKET
        p->packetId = rx_packet_id++;
        p->allNextp = rx_mallocedP;
#endif /* RXDEBUG_PACKET */
	rx_mallocedP = p;
    }

    rx_nFreePackets += apackets;
    MUTEX_ENTER(&rx_packets_mutex);
    rx_nPackets += apackets;
#ifdef RX_ENABLE_TSFPQ
    RX_TS_FPQ_COMPUTE_LIMITS;
#endif /* RX_ENABLE_TSFPQ */
    MUTEX_EXIT(&rx_packets_mutex);
    rxi_NeedMorePackets = FALSE;
    rxi_PacketsUnWait();
}
#endif /* !KERNEL */

void
rxi_FreeAllPackets(void)
{
    struct rx_mallocedPacket *mp;

    MUTEX_ENTER(&rx_mallocedPktQ_lock);

    while (!opr_queue_IsEmpty(&rx_mallocedPacketQueue)) {
	mp = opr_queue_First(&rx_mallocedPacketQueue,
			     struct rx_mallocedPacket, entry);
	opr_queue_Remove(&mp->entry);
	osi_Free(mp->addr, mp->size);
	UNPIN(mp->addr, mp->size);
	osi_Free(mp, sizeof(*mp));
    }
    MUTEX_EXIT(&rx_mallocedPktQ_lock);
}

#ifdef RX_ENABLE_TSFPQ
static void
rxi_AdjustLocalPacketsTSFPQ(int num_keep_local, int allow_overcommit)
{
    struct rx_ts_info_t * rx_ts_info;
    int xfer;
    SPLVAR;

    RX_TS_INFO_GET(rx_ts_info);

    if (num_keep_local != rx_ts_info->_FPQ.len) {
        NETPRI;
	MUTEX_ENTER(&rx_freePktQ_lock);
        if (num_keep_local < rx_ts_info->_FPQ.len) {
            xfer = rx_ts_info->_FPQ.len - num_keep_local;
	    RX_TS_FPQ_LTOG2(rx_ts_info, xfer);
	    rxi_PacketsUnWait();
        } else {
            xfer = num_keep_local - rx_ts_info->_FPQ.len;
            if ((num_keep_local > rx_TSFPQLocalMax) && !allow_overcommit)
                xfer = rx_TSFPQLocalMax - rx_ts_info->_FPQ.len;
            if (rx_nFreePackets < xfer) {
		rxi_MorePacketsNoLock(MAX(xfer - rx_nFreePackets, 4 * rx_initSendWindow));
            }
            RX_TS_FPQ_GTOL2(rx_ts_info, xfer);
        }
	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }
}

void
rxi_FlushLocalPacketsTSFPQ(void)
{
    rxi_AdjustLocalPacketsTSFPQ(0, 0);
}
#endif /* RX_ENABLE_TSFPQ */

/* Allocate more packets iff we need more continuation buffers */
/* In kernel, can't page in memory with interrupts disabled, so we
 * don't use the event mechanism. */
void
rx_CheckPackets(void)
{
    if (rxi_NeedMorePackets) {
	rxi_MorePackets(rx_maxSendWindow);
    }
}

/* In the packet freeing routine below, the assumption is that
   we want all of the packets to be used equally frequently, so that we
   don't get packet buffers paging out.  It would be just as valid to
   assume that we DO want them to page out if not many are being used.
   In any event, we assume the former, and append the packets to the end
   of the free list.  */
/* This explanation is bogus.  The free list doesn't remain in any kind of
   useful order for afs_int32: the packets in use get pretty much randomly scattered
   across all the pages.  In order to permit unused {packets,bufs} to page out, they
   must be stored so that packets which are adjacent in memory are adjacent in the
   free list.  An array springs rapidly to mind.
   */

/* Actually free the packet p. */
#ifndef RX_ENABLE_TSFPQ
static void
rxi_FreePacketNoLock(struct rx_packet *p)
{
    dpf(("Free %"AFS_PTR_FMT"\n", p));

    RX_FPQ_MARK_FREE(p);
    rx_nFreePackets++;
    opr_queue_Append(&rx_freePacketQueue, &p->entry);
}
#endif /* RX_ENABLE_TSFPQ */

#ifdef RX_ENABLE_TSFPQ
static void
rxi_FreePacketTSFPQ(struct rx_packet *p, int flush_global)
{
    struct rx_ts_info_t * rx_ts_info;
    dpf(("Free %"AFS_PTR_FMT"\n", p));

    RX_TS_INFO_GET(rx_ts_info);
    RX_TS_FPQ_CHECKIN(rx_ts_info,p);

    if (flush_global && (rx_ts_info->_FPQ.len > rx_TSFPQLocalMax)) {
        NETPRI;
	MUTEX_ENTER(&rx_freePktQ_lock);

	RX_TS_FPQ_LTOG(rx_ts_info);

	/* Wakeup anyone waiting for packets */
	rxi_PacketsUnWait();

	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }
}
#endif /* RX_ENABLE_TSFPQ */

/*
 * free continuation buffers off a packet into a queue
 *
 * [IN] p      -- packet from which continuation buffers will be freed
 * [IN] first  -- iovec offset of first continuation buffer to free
 * [IN] q      -- queue into which continuation buffers will be chained
 *
 * returns:
 *   number of continuation buffers freed
 */
#ifndef RX_ENABLE_TSFPQ
static int
rxi_FreeDataBufsToQueue(struct rx_packet *p, afs_uint32 first, struct opr_queue * q)
{
    struct iovec *iov;
    struct rx_packet * cb;
    int count = 0;

    for (first = MAX(2, first); first < p->niovecs; first++, count++) {
	iov = &p->wirevec[first];
	if (!iov->iov_base)
	    osi_Panic("rxi_FreeDataBufsToQueue: unexpected NULL iov");
	cb = RX_CBUF_TO_PACKET(iov->iov_base, p);
	RX_FPQ_MARK_FREE(cb);
	opr_queue_Append(q, &cb->entry);
    }
    p->length = 0;
    p->niovecs = 0;

    return count;
}

/*
 * free packet continuation buffers into the global free packet pool
 *
 * [IN] p      -- packet from which to free continuation buffers
 * [IN] first  -- iovec offset of first continuation buffer to free
 *
 * returns:
 *   zero always
 */
static int
rxi_FreeDataBufsNoLock(struct rx_packet *p, afs_uint32 first)
{
    struct iovec *iov;

    for (first = MAX(2, first); first < p->niovecs; first++) {
	iov = &p->wirevec[first];
	if (!iov->iov_base)
	    osi_Panic("rxi_FreeDataBufsNoLock: unexpected NULL iov");
	rxi_FreePacketNoLock(RX_CBUF_TO_PACKET(iov->iov_base, p));
    }
    p->length = 0;
    p->niovecs = 0;

    return 0;
}

#else

/*
 * free packet continuation buffers into the thread-local free pool
 *
 * [IN] p             -- packet from which continuation buffers will be freed
 * [IN] first         -- iovec offset of first continuation buffer to free
 *                       any value less than 2, the min number of iovecs,
 *                       is treated as if it is 2.
 * [IN] flush_global  -- if nonzero, we will flush overquota packets to the
 *                       global free pool before returning
 *
 * returns:
 *   zero always
 */
static int
rxi_FreeDataBufsTSFPQ(struct rx_packet *p, afs_uint32 first, int flush_global)
{
    struct iovec *iov;
    struct rx_ts_info_t * rx_ts_info;

    RX_TS_INFO_GET(rx_ts_info);

    for (first = MAX(2, first); first < p->niovecs; first++) {
	iov = &p->wirevec[first];
	if (!iov->iov_base)
	    osi_Panic("rxi_FreeDataBufsTSFPQ: unexpected NULL iov");
	RX_TS_FPQ_CHECKIN(rx_ts_info,RX_CBUF_TO_PACKET(iov->iov_base, p));
    }
    p->length = 0;
    p->niovecs = 0;

    if (flush_global && (rx_ts_info->_FPQ.len > rx_TSFPQLocalMax)) {
        NETPRI;
	MUTEX_ENTER(&rx_freePktQ_lock);

	RX_TS_FPQ_LTOG(rx_ts_info);

	/* Wakeup anyone waiting for packets */
	rxi_PacketsUnWait();

	MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }
    return 0;
}
#endif /* RX_ENABLE_TSFPQ */

int rxi_nBadIovecs = 0;

/* rxi_RestoreDataBufs
 *
 * Restore the correct sizes to the iovecs. Called when reusing a packet
 * for reading off the wire.
 */
void
rxi_RestoreDataBufs(struct rx_packet *p)
{
    unsigned int i;
    struct iovec *iov;

    RX_PACKET_IOV_INIT(p);

    for (i = 2, iov = &p->wirevec[2]; i < p->niovecs; i++, iov++) {
	if (!iov->iov_base) {
	    rxi_nBadIovecs++;
	    p->niovecs = i;
	    break;
	}
	iov->iov_len = RX_CBUFFERSIZE;
    }
}

#ifdef RX_ENABLE_TSFPQ
int
rxi_TrimDataBufs(struct rx_packet *p, int first)
{
    int length;
    struct iovec *iov, *end;
    struct rx_ts_info_t * rx_ts_info;
    SPLVAR;

    if (first != 1)
	osi_Panic("TrimDataBufs 1: first must be 1");

    /* Skip over continuation buffers containing message data */
    iov = &p->wirevec[2];
    end = iov + (p->niovecs - 2);
    length = p->length - p->wirevec[1].iov_len;
    for (; iov < end && length > 0; iov++) {
	if (!iov->iov_base)
	    osi_Panic("TrimDataBufs 3: vecs 1-niovecs must not be NULL");
	length -= iov->iov_len;
    }

    /* iov now points to the first empty data buffer. */
    if (iov >= end)
	return 0;

    RX_TS_INFO_GET(rx_ts_info);
    for (; iov < end; iov++) {
	if (!iov->iov_base)
	    osi_Panic("TrimDataBufs 4: vecs 2-niovecs must not be NULL");
	RX_TS_FPQ_CHECKIN(rx_ts_info,RX_CBUF_TO_PACKET(iov->iov_base, p));
	p->niovecs--;
    }
    if (rx_ts_info->_FPQ.len > rx_TSFPQLocalMax) {
        NETPRI;
        MUTEX_ENTER(&rx_freePktQ_lock);

	RX_TS_FPQ_LTOG(rx_ts_info);
        rxi_PacketsUnWait();

        MUTEX_EXIT(&rx_freePktQ_lock);
	USERPRI;
    }

    return 0;
}
#else /* RX_ENABLE_TSFPQ */
int
rxi_TrimDataBufs(struct rx_packet *p, int first)
{
    int length;
    struct iovec *iov, *end;
    SPLVAR;

    if (first != 1)
	osi_Panic("TrimDataBufs 1: first must be 1");

    /* Skip over continuation buffers containing message data */
    iov = &p->wirevec[2];
    end = iov + (p->niovecs - 2);
    length = p->length - p->wirevec[1].iov_len;
    for (; iov < end && length > 0; iov++) {
	if (!iov->iov_base)
	    osi_Panic("TrimDataBufs 3: vecs 1-niovecs must not be NULL");
	length -= iov->iov_len;
    }

    /* iov now points to the first empty data buffer. */
    if (iov >= end)
	return 0;

    NETPRI;
    MUTEX_ENTER(&rx_freePktQ_lock);

    for (; iov < end; iov++) {
	if (!iov->iov_base)
	    osi_Panic("TrimDataBufs 4: vecs 2-niovecs must not be NULL");
	rxi_FreePacketNoLock(RX_CBUF_TO_PACKET(iov->iov_base, p));
	p->niovecs--;
    }
    rxi_PacketsUnWait();

    MUTEX_EXIT(&rx_freePktQ_lock);
    USERPRI;

    return 0;
}
#endif /* RX_ENABLE_TSFPQ */

/* Free the packet p.  P is assumed not to be on any queue, i.e.
 * remove it yourself first if you call this routine. */
#ifdef RX_ENABLE_TSFPQ
void
rxi_FreePacket(struct rx_packet *p)
{
    rxi_FreeDataBufsTSFPQ(p, 2, 0);
    rxi_FreePacketTSFPQ(p, RX_TS_FPQ_FLUSH_GLOBAL);
}
#else /* RX_ENABLE_TSFPQ */
void
rxi_FreePacket(struct rx_packet *p)
{
    SPLVAR;

    NETPRI;
    MUTEX_ENTER(&rx_freePktQ_lock);

    rxi_FreeDataBufsNoLock(p, 2);
    rxi_FreePacketNoLock(p);
    /* Wakeup anyone waiting for packets */
    rxi_PacketsUnWait();

    MUTEX_EXIT(&rx_freePktQ_lock);
    USERPRI;
}
#endif /* RX_ENABLE_TSFPQ */

/* rxi_AllocPacket sets up p->length so it reflects the number of
 * bytes in the packet at this point, **not including** the header.
 * The header is absolutely necessary, besides, this is the way the
 * length field is usually used */
#ifdef RX_ENABLE_TSFPQ
static struct rx_packet *
rxi_AllocPacketNoLock(int class)
{
    struct rx_packet *p;
    struct rx_ts_info_t * rx_ts_info;

    RX_TS_INFO_GET(rx_ts_info);

    if (rx_stats_active)
        rx_atomic_inc(&rx_stats.packetRequests);
    if (opr_queue_IsEmpty(&rx_ts_info->_FPQ.queue)) {

#ifdef KERNEL
        if (opr_queue_IsEmpty(&rx_freePacketQueue))
	    osi_Panic("rxi_AllocPacket error");
#else /* KERNEL */
        if (opr_queue_IsEmpty(&rx_freePacketQueue))
	    rxi_MorePacketsNoLock(rx_maxSendWindow);
#endif /* KERNEL */


	RX_TS_FPQ_GTOL(rx_ts_info);
    }

    RX_TS_FPQ_CHECKOUT(rx_ts_info,p);

    dpf(("Alloc %"AFS_PTR_FMT", class %d\n", p, class));


    /* have to do this here because rx_FlushWrite fiddles with the iovs in
     * order to truncate outbound packets.  In the near future, may need
     * to allocate bufs from a static pool here, and/or in AllocSendPacket
     */
    RX_PACKET_IOV_FULLINIT(p);
    return p;
}
#else /* RX_ENABLE_TSFPQ */
static struct rx_packet *
rxi_AllocPacketNoLock(int class)
{
    struct rx_packet *p;

#ifdef KERNEL
    if (rxi_OverQuota(class)) {
	rxi_NeedMorePackets = TRUE;
        if (rx_stats_active) {
            switch (class) {
            case RX_PACKET_CLASS_RECEIVE:
                rx_atomic_inc(&rx_stats.receivePktAllocFailures);
                break;
            case RX_PACKET_CLASS_SEND:
                rx_atomic_inc(&rx_stats.sendPktAllocFailures);
                break;
            case RX_PACKET_CLASS_SPECIAL:
                rx_atomic_inc(&rx_stats.specialPktAllocFailures);
                break;
            case RX_PACKET_CLASS_RECV_CBUF:
                rx_atomic_inc(&rx_stats.receiveCbufPktAllocFailures);
                break;
            case RX_PACKET_CLASS_SEND_CBUF:
                rx_atomic_inc(&rx_stats.sendCbufPktAllocFailures);
                break;
            }
        }
	return (struct rx_packet *)0;
    }
#endif /* KERNEL */

    if (rx_stats_active)
        rx_atomic_inc(&rx_stats.packetRequests);

#ifdef KERNEL
    if (opr_queue_IsEmpty(&rx_freePacketQueue))
	osi_Panic("rxi_AllocPacket error");
#else /* KERNEL */
    if (opr_queue_IsEmpty(&rx_freePacketQueue))
	rxi_MorePacketsNoLock(rx_maxSendWindow);
#endif /* KERNEL */

    rx_nFreePackets--;
    p = opr_queue_First(&rx_freePacketQueue, struct rx_packet, entry);
    opr_queue_Remove(&p->entry);
    RX_FPQ_MARK_USED(p);

    dpf(("Alloc %"AFS_PTR_FMT", class %d\n", p, class));


    /* have to do this here because rx_FlushWrite fiddles with the iovs in
     * order to truncate outbound packets.  In the near future, may need
     * to allocate bufs from a static pool here, and/or in AllocSendPacket
     */
    RX_PACKET_IOV_FULLINIT(p);
    return p;
}
#endif /* RX_ENABLE_TSFPQ */

#ifdef RX_ENABLE_TSFPQ
static struct rx_packet *
rxi_AllocPacketTSFPQ(int class, int pull_global)
{
    struct rx_packet *p;
    struct rx_ts_info_t * rx_ts_info;

    RX_TS_INFO_GET(rx_ts_info);

    if (rx_stats_active)
        rx_atomic_inc(&rx_stats.packetRequests);
    if (pull_global && opr_queue_IsEmpty(&rx_ts_info->_FPQ.queue)) {
        MUTEX_ENTER(&rx_freePktQ_lock);

        if (opr_queue_IsEmpty(&rx_freePacketQueue))
	    rxi_MorePacketsNoLock(rx_maxSendWindow);

	RX_TS_FPQ_GTOL(rx_ts_info);

        MUTEX_EXIT(&rx_freePktQ_lock);
    } else if (opr_queue_IsEmpty(&rx_ts_info->_FPQ.queue)) {
        return NULL;
    }

    RX_TS_FPQ_CHECKOUT(rx_ts_info,p);

    dpf(("Alloc %"AFS_PTR_FMT", class %d\n", p, class));

    /* have to do this here because rx_FlushWrite fiddles with the iovs in
     * order to truncate outbound packets.  In the near future, may need
     * to allocate bufs from a static pool here, and/or in AllocSendPacket
     */
    RX_PACKET_IOV_FULLINIT(p);
    return p;
}
#endif /* RX_ENABLE_TSFPQ */

#ifdef RX_ENABLE_TSFPQ
struct rx_packet *
rxi_AllocPacket(int class)
{
    struct rx_packet *p;

    p = rxi_AllocPacketTSFPQ(class, RX_TS_FPQ_PULL_GLOBAL);
    return p;
}
#else /* RX_ENABLE_TSFPQ */
struct rx_packet *
rxi_AllocPacket(int class)
{
    struct rx_packet *p;

    MUTEX_ENTER(&rx_freePktQ_lock);
    p = rxi_AllocPacketNoLock(class);
    MUTEX_EXIT(&rx_freePktQ_lock);
    return p;
}
#endif /* RX_ENABLE_TSFPQ */

/* This guy comes up with as many buffers as it {takes,can get} given
 * the MTU for this call. It also sets the packet length before
 * returning.  caution: this is often called at NETPRI
 * Called with call locked.
 */
struct rx_packet *
rxi_AllocSendPacket(struct rx_call *call, int want)
{
    struct rx_packet *p = (struct rx_packet *)0;
    int mud;
    unsigned delta;

    SPLVAR;
    mud = call->MTU - RX_HEADER_SIZE;
    delta =
	rx_GetSecurityHeaderSize(rx_ConnectionOf(call)) +
	rx_GetSecurityMaxTrailerSize(rx_ConnectionOf(call));

#ifdef RX_ENABLE_TSFPQ
    if ((p = rxi_AllocPacketTSFPQ(RX_PACKET_CLASS_SEND, 0))) {
        want += delta;
	want = MIN(want, mud);

	if ((unsigned)want > p->length)
	    (void)rxi_AllocDataBuf(p, (want - p->length),
				   RX_PACKET_CLASS_SEND_CBUF);

	if (p->length > mud)
            p->length = mud;

	if (delta >= p->length) {
	    rxi_FreePacket(p);
	    p = NULL;
	} else {
	    p->length -= delta;
	}
	return p;
    }
#endif /* RX_ENABLE_TSFPQ */

    while (!(call->error)) {
	MUTEX_ENTER(&rx_freePktQ_lock);
	/* if an error occurred, or we get the packet we want, we're done */
	if ((p = rxi_AllocPacketNoLock(RX_PACKET_CLASS_SEND))) {
	    MUTEX_EXIT(&rx_freePktQ_lock);

	    want += delta;
	    want = MIN(want, mud);

	    if ((unsigned)want > p->length)
		(void)rxi_AllocDataBuf(p, (want - p->length),
				       RX_PACKET_CLASS_SEND_CBUF);

	    if (p->length > mud)
		p->length = mud;

	    if (delta >= p->length) {
		rxi_FreePacket(p);
		p = NULL;
	    } else {
		p->length -= delta;
	    }
	    break;
	}

	/* no error occurred, and we didn't get a packet, so we sleep.
	 * At this point, we assume that packets will be returned
	 * sooner or later, as packets are acknowledged, and so we
	 * just wait.  */
	NETPRI;
	call->flags |= RX_CALL_WAIT_PACKETS;
	CALL_HOLD(call, RX_CALL_REFCOUNT_PACKET);
	MUTEX_EXIT(&call->lock);
	rx_waitingForPackets = 1;

#ifdef	RX_ENABLE_LOCKS
	CV_WAIT(&rx_waitingForPackets_cv, &rx_freePktQ_lock);
#else
	osi_rxSleep(&rx_waitingForPackets);
#endif
	MUTEX_EXIT(&rx_freePktQ_lock);
	MUTEX_ENTER(&call->lock);
	CALL_RELE(call, RX_CALL_REFCOUNT_PACKET);
	call->flags &= ~RX_CALL_WAIT_PACKETS;
	USERPRI;
    }

    return p;
}

#ifndef KERNEL
#ifdef AFS_NT40_ENV
/* Windows does not use file descriptors. */
#define CountFDs(amax) 0
#else
/* count the number of used FDs */
static int
CountFDs(int amax)
{
    struct stat tstat;
    int i, code;
    int count;

    count = 0;
    for (i = 0; i < amax; i++) {
	code = fstat(i, &tstat);
	if (code == 0)
	    count++;
    }
    return count;
}
#endif /* AFS_NT40_ENV */
#else /* KERNEL */

#define CountFDs(amax) amax

#endif /* KERNEL */

#if !defined(KERNEL) || defined(UKERNEL)

/* This function reads a single packet from the interface into the
 * supplied packet buffer (*p).  Return 0 if the packet is bogus.  The
 * (host,port) of the sender are stored in the supplied variables, and
 * the data length of the packet is stored in the packet structure.
 * The header is decoded. */
int
rxi_ReadPacket(osi_socket socket, struct rx_packet *p, afs_uint32 * host,
	       u_short * port)
{
    struct sockaddr_in from;
    int nbytes;
    afs_int32 rlen;
    afs_uint32 tlen, savelen;
    struct msghdr msg;
    rx_computelen(p, tlen);
    rx_SetDataSize(p, tlen);	/* this is the size of the user data area */

    tlen += RX_HEADER_SIZE;	/* now this is the size of the entire packet */
    rlen = rx_maxJumboRecvSize;	/* this is what I am advertising.  Only check
				 * it once in order to avoid races.  */
    tlen = rlen - tlen;
    if (tlen > 0) {
	tlen = rxi_AllocDataBuf(p, tlen, RX_PACKET_CLASS_SEND_CBUF);
	if (tlen > 0) {
	    tlen = rlen - tlen;
	} else
	    tlen = rlen;
    } else
	tlen = rlen;

    /* Extend the last iovec for padding, it's just to make sure that the
     * read doesn't return more data than we expect, and is done to get around
     * our problems caused by the lack of a length field in the rx header.
     * Use the extra buffer that follows the localdata in each packet
     * structure. */
    savelen = p->wirevec[p->niovecs - 1].iov_len;
    p->wirevec[p->niovecs - 1].iov_len += RX_EXTRABUFFERSIZE;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (char *)&from;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_iov = p->wirevec;
    msg.msg_iovlen = p->niovecs;
    nbytes = rxi_Recvmsg(socket, &msg, 0);

    /* restore the vec to its correct state */
    p->wirevec[p->niovecs - 1].iov_len = savelen;

    p->length = (u_short)(nbytes - RX_HEADER_SIZE);
    if (nbytes < 0 || (nbytes > tlen) || (p->length & 0x8000)) { /* Bogus packet */
	if (nbytes < 0 && errno == EWOULDBLOCK) {
            if (rx_stats_active)
                rx_atomic_inc(&rx_stats.noPacketOnRead);
	} else if (nbytes <= 0) {
            if (rx_stats_active) {
                rx_atomic_inc(&rx_stats.bogusPacketOnRead);
                rx_stats.bogusHost = from.sin_addr.s_addr;
            }
	    dpf(("B: bogus packet from [%x,%d] nb=%d\n", ntohl(from.sin_addr.s_addr),
		 ntohs(from.sin_port), nbytes));
	}
	return 0;
    }
#ifdef RXDEBUG
    else if ((rx_intentionallyDroppedOnReadPer100 > 0)
		&& (random() % 100 < rx_intentionallyDroppedOnReadPer100)) {
	rxi_DecodePacketHeader(p);

	*host = from.sin_addr.s_addr;
	*port = from.sin_port;

	dpf(("Dropped %d %s: %x.%u.%u.%u.%u.%u.%u flags %d len %d\n",
	      p->header.serial, rx_packetTypes[p->header.type - 1], ntohl(*host), ntohs(*port), p->header.serial,
	      p->header.epoch, p->header.cid, p->header.callNumber, p->header.seq, p->header.flags,
	      p->length));
#ifdef RX_TRIMDATABUFS
	rxi_TrimDataBufs(p, 1);
#endif
	return 0;
    }
#endif
    else {
	/* Extract packet header. */
	rxi_DecodePacketHeader(p);

	*host = from.sin_addr.s_addr;
	*port = from.sin_port;
	if (rx_stats_active
	    && p->header.type > 0 && p->header.type < RX_N_PACKET_TYPES) {

		rx_atomic_inc(&rx_stats.packetsRead[p->header.type - 1]);
	}

#ifdef RX_TRIMDATABUFS
	/* Free any empty packet buffers at the end of this packet */
	rxi_TrimDataBufs(p, 1);
#endif
	return 1;
    }
}

#endif /* !KERNEL || UKERNEL */

/* This function splits off the first packet in a jumbo packet.
 * As of AFS 3.5, jumbograms contain more than one fixed size
 * packet, and the RX_JUMBO_PACKET flag is set in all but the
 * last packet header. All packets (except the last) are padded to
 * fall on RX_CBUFFERSIZE boundaries.
 * HACK: We store the length of the first n-1 packets in the
 * last two pad bytes. */

struct rx_packet *
rxi_SplitJumboPacket(struct rx_packet *p, afs_uint32 host, short port,
		     int first)
{
    struct rx_packet *np;
    struct rx_jumboHeader *jp;
    int niov, i;
    struct iovec *iov;
    int length;
    afs_uint32 temp;

    /* All but the last packet in each jumbogram are RX_JUMBOBUFFERSIZE
     * bytes in length. All but the first packet are preceded by
     * an abbreviated four byte header. The length of the last packet
     * is calculated from the size of the jumbogram. */
    length = RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;

    if ((int)p->length < length) {
	dpf(("rxi_SplitJumboPacket: bogus length %d\n", p->length));
	return NULL;
    }
    niov = p->niovecs - 2;
    if (niov < 1) {
	dpf(("rxi_SplitJumboPacket: bogus niovecs %d\n", p->niovecs));
	return NULL;
    }
    iov = &p->wirevec[2];
    np = RX_CBUF_TO_PACKET(iov->iov_base, p);

    /* Get a pointer to the abbreviated packet header */
    jp = (struct rx_jumboHeader *)
	((char *)(p->wirevec[1].iov_base) + RX_JUMBOBUFFERSIZE);

    /* Set up the iovecs for the next packet */
    np->wirevec[0].iov_base = (char *)(&np->wirehead[0]);
    np->wirevec[0].iov_len = sizeof(struct rx_header);
    np->wirevec[1].iov_base = (char *)(&np->localdata[0]);
    np->wirevec[1].iov_len = length - RX_JUMBOHEADERSIZE;
    np->niovecs = niov + 1;
    for (i = 2, iov++; i <= niov; i++, iov++) {
	np->wirevec[i] = *iov;
    }
    np->length = p->length - length;
    p->length = RX_JUMBOBUFFERSIZE;
    p->niovecs = 2;

    /* Convert the jumbo packet header to host byte order */
    temp = ntohl(*(afs_uint32 *) jp);
    jp->flags = (u_char) (temp >> 24);
    jp->cksum = (u_short) (temp);

    /* Fill in the packet header */
    np->header = p->header;
    np->header.serial = p->header.serial + 1;
    np->header.seq = p->header.seq + 1;
    np->header.userStatus = 0;
    np->header.flags = jp->flags;
    np->header.spare = jp->cksum;

    return np;
}

#ifndef KERNEL
/* Send a udp datagram */
int
osi_NetSend(osi_socket socket, void *addr, struct iovec *dvec, int nvecs,
	    int length, int istack)
{
    struct msghdr msg;
	int ret;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = dvec;
    msg.msg_iovlen = nvecs;
    msg.msg_name = addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);

    ret = rxi_Sendmsg(socket, &msg, 0);

    return ret;
}
#elif !defined(UKERNEL)
/*
 * message receipt is done in rxk_input or rx_put.
 */

#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX110_ENV)
/*
 * Copy an mblock to the contiguous area pointed to by cp.
 * MTUXXX Supposed to skip <off> bytes and copy <len> bytes,
 * but it doesn't really.
 * Returns the number of bytes not transferred.
 * The message is NOT changed.
 */
static int
cpytoc(mblk_t * mp, int off, int len, char *cp)
{
    int n;

    for (; mp && len > 0; mp = mp->b_cont) {
	if (mp->b_datap->db_type != M_DATA) {
	    return -1;
	}
	n = MIN(len, (mp->b_wptr - mp->b_rptr));
	memcpy(cp, (char *)mp->b_rptr, n);
	cp += n;
	len -= n;
	mp->b_rptr += n;
    }
    return (len);
}

/* MTUXXX Supposed to skip <off> bytes and copy <len> bytes,
 * but it doesn't really.
 * This sucks, anyway, do it like m_cpy.... below
 */
static int
cpytoiovec(mblk_t * mp, int off, int len, struct iovec *iovs,
	   int niovs)
{
    int m, n, o, t, i;

    for (i = -1, t = 0; i < niovs && mp && len > 0; mp = mp->b_cont) {
	if (mp->b_datap->db_type != M_DATA) {
	    return -1;
	}
	n = MIN(len, (mp->b_wptr - mp->b_rptr));
	len -= n;
	while (n) {
	    if (!t) {
		o = 0;
		i++;
		t = iovs[i].iov_len;
	    }
	    m = MIN(n, t);
	    memcpy(iovs[i].iov_base + o, (char *)mp->b_rptr, m);
	    mp->b_rptr += m;
	    o += m;
	    t -= m;
	    n -= m;
	}
    }
    return (len);
}

#define m_cpytoc(a, b, c, d)  cpytoc(a, b, c, d)
#define m_cpytoiovec(a, b, c, d, e) cpytoiovec(a, b, c, d, e)
#else
#if !defined(AFS_LINUX_ENV) && !defined(AFS_DARWIN80_ENV)
static int
m_cpytoiovec(struct mbuf *m, int off, int len, struct iovec iovs[], int niovs)
{
    caddr_t p1, p2;
    unsigned int l1, l2, i, t;

    if (m == NULL || off < 0 || len < 0 || iovs == NULL)
	osi_Panic("m_cpytoiovec");	/* MTUXXX probably don't need this check */

    while (off && m)
	if (m->m_len <= off) {
	    off -= m->m_len;
	    m = m->m_next;
	    continue;
	} else
	    break;

    if (m == NULL)
	return len;

    p1 = mtod(m, caddr_t) + off;
    l1 = m->m_len - off;
    i = 0;
    p2 = iovs[0].iov_base;
    l2 = iovs[0].iov_len;

    while (len) {
	t = MIN(l1, MIN(l2, (unsigned int)len));
	memcpy(p2, p1, t);
	p1 += t;
	p2 += t;
	l1 -= t;
	l2 -= t;
	len -= t;
	if (!l1) {
	    m = m->m_next;
	    if (!m)
		break;
	    p1 = mtod(m, caddr_t);
	    l1 = m->m_len;
	}
	if (!l2) {
	    if (++i >= niovs)
		break;
	    p2 = iovs[i].iov_base;
	    l2 = iovs[i].iov_len;
	}

    }

    return len;
}
#endif /* !AFS_LINUX_ENV && !AFS_DARWIN80_ENV */
#endif /* AFS_SUN5_ENV */

#if !defined(AFS_LINUX_ENV) && !defined(AFS_DARWIN80_ENV)
#if defined(AFS_NBSD_ENV)
int
rx_mb_to_packet(struct mbuf *amb, void (*free) (struct mbuf *), int hdr_len, int data_len, struct rx_packet *phandle)
#else
int
rx_mb_to_packet(amb, free, hdr_len, data_len, phandle)
#if defined(AFS_SUN5_ENV) || defined(AFS_HPUX110_ENV)
     mblk_t *amb;
#else
     struct mbuf *amb;
#endif
     void (*free) ();
     struct rx_packet *phandle;
     int hdr_len, data_len;
#endif /* AFS_NBSD_ENV */
{
    int code;

    code =
	m_cpytoiovec(amb, hdr_len, data_len, phandle->wirevec,
		     phandle->niovecs);
    (*free) (amb);

    return code;
}
#endif /* LINUX */
#endif /*KERNEL && !UKERNEL */


/* send a response to a debug packet */

struct rx_packet *
rxi_ReceiveDebugPacket(struct rx_packet *ap, osi_socket asocket,
		       afs_uint32 ahost, short aport, int istack)
{
    struct rx_debugIn tin;
    afs_int32 tl;

    /*
     * Only respond to client-initiated Rx debug packets,
     * and clear the client flag in the response.
     */
    if (ap->header.flags & RX_CLIENT_INITIATED) {
	ap->header.flags = ap->header.flags & ~RX_CLIENT_INITIATED;
	rxi_EncodePacketHeader(ap);
    } else {
	return ap;
    }

    rx_packetread(ap, 0, sizeof(struct rx_debugIn), (char *)&tin);
    /* all done with packet, now set length to the truth, so we can
     * reuse this packet */
    rx_computelen(ap, ap->length);

    tin.type = ntohl(tin.type);
    tin.index = ntohl(tin.index);
    switch (tin.type) {
    case RX_DEBUGI_GETSTATS:{
	    struct rx_debugStats tstat;

	    /* get basic stats */
	    memset(&tstat, 0, sizeof(tstat));	/* make sure spares are zero */
	    tstat.version = RX_DEBUGI_VERSION;
#ifndef	RX_ENABLE_LOCKS
	    tstat.waitingForPackets = rx_waitingForPackets;
#endif
	    MUTEX_ENTER(&rx_serverPool_lock);
	    tstat.nFreePackets = htonl(rx_nFreePackets);
	    tstat.nPackets = htonl(rx_nPackets);
	    tstat.callsExecuted = htonl(rxi_nCalls);
	    tstat.packetReclaims = htonl(rx_packetReclaims);
	    tstat.usedFDs = CountFDs(64);
	    tstat.nWaiting = htonl(rx_atomic_read(&rx_nWaiting));
	    tstat.nWaited = htonl(rx_atomic_read(&rx_nWaited));
	    tstat.idleThreads = opr_queue_Count(&rx_idleServerQueue);
	    MUTEX_EXIT(&rx_serverPool_lock);
	    tstat.idleThreads = htonl(tstat.idleThreads);
	    tl = sizeof(struct rx_debugStats) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);

	    if (tl <= 0) {
		rx_packetwrite(ap, 0, sizeof(struct rx_debugStats),
			       (char *)&tstat);
		ap->length = sizeof(struct rx_debugStats);
		rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
		rx_computelen(ap, ap->length);
	    }
	    break;
	}

    case RX_DEBUGI_GETALLCONN:
    case RX_DEBUGI_GETCONN:{
            unsigned int i, j;
	    struct rx_connection *tc;
	    struct rx_call *tcall;
	    struct rx_debugConn tconn;
	    int all = (tin.type == RX_DEBUGI_GETALLCONN);


	    tl = sizeof(struct rx_debugConn) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);
	    if (tl > 0)
		return ap;

	    memset(&tconn, 0, sizeof(tconn));	/* make sure spares are zero */
	    /* get N'th (maybe) "interesting" connection info */
	    for (i = 0; i < rx_hashTableSize; i++) {
#if !defined(KERNEL)
		/* the time complexity of the algorithm used here
		 * exponentially increses with the number of connections.
		 */
#ifdef AFS_PTHREAD_ENV
		pthread_yield();
#else
		(void)IOMGR_Poll();
#endif
#endif
		MUTEX_ENTER(&rx_connHashTable_lock);
		/* We might be slightly out of step since we are not
		 * locking each call, but this is only debugging output.
		 */
		for (tc = rx_connHashTable[i]; tc; tc = tc->next) {
		    if ((all || rxi_IsConnInteresting(tc))
			&& tin.index-- <= 0) {
			tconn.host = tc->peer->host;
			tconn.port = tc->peer->port;
			tconn.cid = htonl(tc->cid);
			tconn.epoch = htonl(tc->epoch);
			tconn.serial = htonl(tc->serial);
			for (j = 0; j < RX_MAXCALLS; j++) {
			    tconn.callNumber[j] = htonl(tc->callNumber[j]);
			    if ((tcall = tc->call[j])) {
				tconn.callState[j] = tcall->state;
				tconn.callMode[j] = tcall->app.mode;
				tconn.callFlags[j] = tcall->flags;
				if (!opr_queue_IsEmpty(&tcall->rq))
				    tconn.callOther[j] |= RX_OTHER_IN;
				if (!opr_queue_IsEmpty(&tcall->tq))
				    tconn.callOther[j] |= RX_OTHER_OUT;
			    } else
				tconn.callState[j] = RX_STATE_NOTINIT;
			}

			tconn.natMTU = htonl(tc->peer->natMTU);
			tconn.error = htonl(tc->error);
			tconn.flags = (u_char) (tc->flags & 0xff);  /* compat. */
			tconn.type = tc->type;
			tconn.securityIndex = tc->securityIndex;
			if (tc->securityObject) {
			    RXS_GetStats(tc->securityObject, tc,
					 &tconn.secStats);
#define DOHTONL(a) (tconn.secStats.a = htonl(tconn.secStats.a))
#define DOHTONS(a) (tconn.secStats.a = htons(tconn.secStats.a))
			    DOHTONL(flags);
			    DOHTONL(expires);
			    DOHTONL(packetsReceived);
			    DOHTONL(packetsSent);
			    DOHTONL(bytesReceived);
			    DOHTONL(bytesSent);
			    for (i = 0;
				 i <
				 sizeof(tconn.secStats.spares) /
				 sizeof(short); i++)
				DOHTONS(spares[i]);
			    for (i = 0;
				 i <
				 sizeof(tconn.secStats.sparel) /
				 sizeof(afs_int32); i++)
				DOHTONL(sparel[i]);
			}

			MUTEX_EXIT(&rx_connHashTable_lock);
			rx_packetwrite(ap, 0, sizeof(struct rx_debugConn),
				       (char *)&tconn);
			tl = ap->length;
			ap->length = sizeof(struct rx_debugConn);
			rxi_SendDebugPacket(ap, asocket, ahost, aport,
					    istack);
			ap->length = tl;
			return ap;
		    }
		}
		MUTEX_EXIT(&rx_connHashTable_lock);
	    }
	    /* if we make it here, there are no interesting packets */
	    tconn.cid = htonl(0xffffffff);	/* means end */
	    rx_packetwrite(ap, 0, sizeof(struct rx_debugConn),
			   (char *)&tconn);
	    tl = ap->length;
	    ap->length = sizeof(struct rx_debugConn);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
	}

	/*
	 * Pass back all the peer structures we have available
	 */

    case RX_DEBUGI_GETPEER:{
	    unsigned int i;
	    struct rx_peer *tp;
	    struct rx_debugPeer tpeer;


	    tl = sizeof(struct rx_debugPeer) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);
	    if (tl > 0)
		return ap;

	    memset(&tpeer, 0, sizeof(tpeer));
	    for (i = 0; i < rx_hashTableSize; i++) {
#if !defined(KERNEL)
		/* the time complexity of the algorithm used here
		 * exponentially increses with the number of peers.
		 *
		 * Yielding after processing each hash table entry
		 * and dropping rx_peerHashTable_lock.
		 * also increases the risk that we will miss a new
		 * entry - but we are willing to live with this
		 * limitation since this is meant for debugging only
		 */
#ifdef AFS_PTHREAD_ENV
		pthread_yield();
#else
		(void)IOMGR_Poll();
#endif
#endif
		MUTEX_ENTER(&rx_peerHashTable_lock);
		for (tp = rx_peerHashTable[i]; tp; tp = tp->next) {
		    if (tin.index-- <= 0) {
                        tp->refCount++;
                        MUTEX_EXIT(&rx_peerHashTable_lock);

                        MUTEX_ENTER(&tp->peer_lock);
			tpeer.host = tp->host;
			tpeer.port = tp->port;
			tpeer.ifMTU = htons(tp->ifMTU);
			tpeer.idleWhen = htonl(tp->idleWhen);
			tpeer.refCount = htons(tp->refCount);
			tpeer.burstSize = 0;
			tpeer.burst = 0;
			tpeer.burstWait.sec = 0;
			tpeer.burstWait.usec = 0;
			tpeer.rtt = htonl(tp->rtt);
			tpeer.rtt_dev = htonl(tp->rtt_dev);
			tpeer.nSent = htonl(tp->nSent);
			tpeer.reSends = htonl(tp->reSends);
			tpeer.natMTU = htons(tp->natMTU);
			tpeer.maxMTU = htons(tp->maxMTU);
			tpeer.maxDgramPackets = htons(tp->maxDgramPackets);
			tpeer.ifDgramPackets = htons(tp->ifDgramPackets);
			tpeer.MTU = htons(tp->MTU);
			tpeer.cwind = htons(tp->cwind);
			tpeer.nDgramPackets = htons(tp->nDgramPackets);
			tpeer.congestSeq = htons(tp->congestSeq);
			tpeer.bytesSent.high =
			    htonl(tp->bytesSent >> 32);
			tpeer.bytesSent.low =
			    htonl(tp->bytesSent & MAX_AFS_UINT32);
			tpeer.bytesReceived.high =
			    htonl(tp->bytesReceived >> 32);
			tpeer.bytesReceived.low =
			    htonl(tp->bytesReceived & MAX_AFS_UINT32);
                        MUTEX_EXIT(&tp->peer_lock);

                        MUTEX_ENTER(&rx_peerHashTable_lock);
                        tp->refCount--;
			MUTEX_EXIT(&rx_peerHashTable_lock);

			rx_packetwrite(ap, 0, sizeof(struct rx_debugPeer),
				       (char *)&tpeer);
			tl = ap->length;
			ap->length = sizeof(struct rx_debugPeer);
			rxi_SendDebugPacket(ap, asocket, ahost, aport,
					    istack);
			ap->length = tl;
			return ap;
		    }
		}
		MUTEX_EXIT(&rx_peerHashTable_lock);
	    }
	    /* if we make it here, there are no interesting packets */
	    tpeer.host = htonl(0xffffffff);	/* means end */
	    rx_packetwrite(ap, 0, sizeof(struct rx_debugPeer),
			   (char *)&tpeer);
	    tl = ap->length;
	    ap->length = sizeof(struct rx_debugPeer);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
	}

    case RX_DEBUGI_RXSTATS:{
	    int i;
	    afs_int32 *s;

	    tl = sizeof(rx_stats) - ap->length;
	    if (tl > 0)
		tl = rxi_AllocDataBuf(ap, tl, RX_PACKET_CLASS_SEND_CBUF);
	    if (tl > 0)
		return ap;

	    /* Since its all int32s convert to network order with a loop. */
	    if (rx_stats_active)
		MUTEX_ENTER(&rx_stats_mutex);
	    s = (afs_int32 *) & rx_stats;
	    for (i = 0; i < sizeof(rx_stats) / sizeof(afs_int32); i++, s++)
		rx_PutInt32(ap, i * sizeof(afs_int32), htonl(*s));

	    tl = ap->length;
	    ap->length = sizeof(rx_stats);
	    if (rx_stats_active)
		MUTEX_EXIT(&rx_stats_mutex);
	    rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	    ap->length = tl;
	    break;
	}

    default:
	/* error response packet */
	tin.type = htonl(RX_DEBUGI_BADTYPE);
	tin.index = tin.type;
	rx_packetwrite(ap, 0, sizeof(struct rx_debugIn), (char *)&tin);
	tl = ap->length;
	ap->length = sizeof(struct rx_debugIn);
	rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	ap->length = tl;
	break;
    }
    return ap;
}

struct rx_packet *
rxi_ReceiveVersionPacket(struct rx_packet *ap, osi_socket asocket,
			 afs_uint32 ahost, short aport, int istack)
{
    afs_int32 tl;

    /*
     * Only respond to client-initiated version requests, and
     * clear that flag in the response.
     */
    if (ap->header.flags & RX_CLIENT_INITIATED) {
	char buf[66];

	ap->header.flags = ap->header.flags & ~RX_CLIENT_INITIATED;
	rxi_EncodePacketHeader(ap);
	memset(buf, 0, sizeof(buf));
	strncpy(buf, cml_version_number + 4, sizeof(buf) - 1);
	rx_packetwrite(ap, 0, 65, buf);
	tl = ap->length;
	ap->length = 65;
	rxi_SendDebugPacket(ap, asocket, ahost, aport, istack);
	ap->length = tl;
    }

    return ap;
}


/* send a debug packet back to the sender */
static void
rxi_SendDebugPacket(struct rx_packet *apacket, osi_socket asocket,
		    afs_uint32 ahost, short aport, afs_int32 istack)
{
    struct sockaddr_in taddr;
    unsigned int i, nbytes, savelen = 0;
    int saven = 0;
#ifdef KERNEL
    int waslocked = ISAFS_GLOCK();
#endif

    taddr.sin_family = AF_INET;
    taddr.sin_port = aport;
    taddr.sin_addr.s_addr = ahost;
    memset(&taddr.sin_zero, 0, sizeof(taddr.sin_zero));
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    taddr.sin_len = sizeof(struct sockaddr_in);
#endif

    /* We need to trim the niovecs. */
    nbytes = apacket->length;
    for (i = 1; i < apacket->niovecs; i++) {
	if (nbytes <= apacket->wirevec[i].iov_len) {
	    savelen = apacket->wirevec[i].iov_len;
	    saven = apacket->niovecs;
	    apacket->wirevec[i].iov_len = nbytes;
	    apacket->niovecs = i + 1;	/* so condition fails because i == niovecs */
	} else
	    nbytes -= apacket->wirevec[i].iov_len;
    }
#ifdef KERNEL
#ifdef RX_KERNEL_TRACE
    if (ICL_SETACTIVE(afs_iclSetp)) {
	if (!waslocked)
	    AFS_GLOCK();
	afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		   "before rxi_NetSend()");
	AFS_GUNLOCK();
    }
#else
    if (waslocked)
	AFS_GUNLOCK();
#endif
#endif
    /* debug packets are not reliably delivered, hence the cast below. */
    (void)rxi_NetSend(asocket, &taddr, apacket->wirevec, apacket->niovecs,
		      apacket->length + RX_HEADER_SIZE, istack);
#ifdef KERNEL
#ifdef RX_KERNEL_TRACE
    if (ICL_SETACTIVE(afs_iclSetp)) {
	AFS_GLOCK();
	afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		   "after rxi_NetSend()");
	if (!waslocked)
	    AFS_GUNLOCK();
    }
#else
    if (waslocked)
	AFS_GLOCK();
#endif
#endif
    if (saven) {		/* means we truncated the packet above. */
	apacket->wirevec[i - 1].iov_len = savelen;
	apacket->niovecs = saven;
    }

}

static void
rxi_NetSendError(struct rx_call *call, int code)
{
    int down = 0;
#ifdef AFS_NT40_ENV
    if (code == -1 && WSAGetLastError() == WSAEHOSTUNREACH) {
	down = 1;
    }
    if (code == -WSAEHOSTUNREACH) {
	down = 1;
    }
#elif defined(AFS_LINUX_ENV)
    if (code == -ENETUNREACH) {
	down = 1;
    }
#elif defined(AFS_DARWIN_ENV)
    if (code == EHOSTUNREACH) {
	down = 1;
    }
#endif
    if (down) {
	call->lastReceiveTime = 0;
    }
}

/* Send the packet to appropriate destination for the specified
 * call.  The header is first encoded and placed in the packet.
 */
void
rxi_SendPacket(struct rx_call *call, struct rx_connection *conn,
	       struct rx_packet *p, int istack)
{
#if defined(KERNEL)
    int waslocked;
#endif
    int code;
    struct sockaddr_in addr;
    struct rx_peer *peer = conn->peer;
    osi_socket socket;
#ifdef RXDEBUG
    char deliveryType = 'S';
#endif
    /* The address we're sending the packet to */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = peer->port;
    addr.sin_addr.s_addr = peer->host;
    memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

    /* This stuff should be revamped, I think, so that most, if not
     * all, of the header stuff is always added here.  We could
     * probably do away with the encode/decode routines. XXXXX */

    /* Stamp each packet with a unique serial number.  The serial
     * number is maintained on a connection basis because some types
     * of security may be based on the serial number of the packet,
     * and security is handled on a per authenticated-connection
     * basis. */
    /* Pre-increment, to guarantee no zero serial number; a zero
     * serial number means the packet was never sent. */
    MUTEX_ENTER(&conn->conn_data_lock);
    p->header.serial = ++conn->serial;
    if (p->length > conn->peer->maxPacketSize) {
	if ((p->header.type == RX_PACKET_TYPE_ACK) &&
	    (p->header.flags & RX_REQUEST_ACK)) {
	    conn->lastPingSize = p->length;
	    conn->lastPingSizeSer = p->header.serial;
	} else if (p->header.seq != 0) {
	    conn->lastPacketSize = p->length;
	    conn->lastPacketSizeSeq = p->header.seq;
	}
    }
    MUTEX_EXIT(&conn->conn_data_lock);
    /* This is so we can adjust retransmit time-outs better in the face of
     * rapidly changing round-trip times.  RTO estimation is not a la Karn.
     */
    if (p->firstSerial == 0) {
	p->firstSerial = p->header.serial;
    }
#ifdef RXDEBUG
    /* If an output tracer function is defined, call it with the packet and
     * network address.  Note this function may modify its arguments. */
    if (rx_almostSent) {
	int drop = (*rx_almostSent) (p, &addr);
	/* drop packet if return value is non-zero? */
	if (drop)
	    deliveryType = 'D';	/* Drop the packet */
    }
#endif

    /* Get network byte order header */
    rxi_EncodePacketHeader(p);	/* XXX in the event of rexmit, etc, don't need to
				 * touch ALL the fields */

    /* Send the packet out on the same socket that related packets are being
     * received on */
    socket =
	(conn->type ==
	 RX_CLIENT_CONNECTION ? rx_socket : conn->service->socket);

#ifdef RXDEBUG
    /* Possibly drop this packet,  for testing purposes */
    if ((deliveryType == 'D')
	|| ((rx_intentionallyDroppedPacketsPer100 > 0)
	    && (random() % 100 < rx_intentionallyDroppedPacketsPer100))) {
	deliveryType = 'D';	/* Drop the packet */
    } else {
	deliveryType = 'S';	/* Send the packet */
#endif /* RXDEBUG */

	/* Loop until the packet is sent.  We'd prefer just to use a
	 * blocking socket, but unfortunately the interface doesn't
	 * allow us to have the socket block in send mode, and not
	 * block in receive mode */
#ifdef KERNEL
	waslocked = ISAFS_GLOCK();
#ifdef RX_KERNEL_TRACE
	if (ICL_SETACTIVE(afs_iclSetp)) {
	    if (!waslocked)
		AFS_GLOCK();
	    afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		       "before rxi_NetSend()");
	    AFS_GUNLOCK();
	}
#else
	if (waslocked)
	    AFS_GUNLOCK();
#endif
#endif
	if ((code =
	     rxi_NetSend(socket, &addr, p->wirevec, p->niovecs,
			 p->length + RX_HEADER_SIZE, istack)) != 0) {
	    /* send failed, so let's hurry up the resend, eh? */
            if (rx_stats_active)
                rx_atomic_inc(&rx_stats.netSendFailures);
	    p->flags &= ~RX_PKTFLAG_SENT; /* resend it very soon */

	    /* Some systems are nice and tell us right away that we cannot
	     * reach this recipient by returning an error code.
	     * So, when this happens let's "down" the host NOW so
	     * we don't sit around waiting for this host to timeout later.
	     */
	    if (call) {
		rxi_NetSendError(call, code);
	    }
	}
#ifdef KERNEL
#ifdef RX_KERNEL_TRACE
	if (ICL_SETACTIVE(afs_iclSetp)) {
	    AFS_GLOCK();
	    afs_Trace1(afs_iclSetp, CM_TRACE_TIMESTAMP, ICL_TYPE_STRING,
		       "after rxi_NetSend()");
	    if (!waslocked)
		AFS_GUNLOCK();
	}
#else
	if (waslocked)
	    AFS_GLOCK();
#endif
#endif
#ifdef RXDEBUG
    }
    dpf(("%c %d %s: %x.%u.%u.%u.%u.%u.%u flags %d, packet %"AFS_PTR_FMT" len %d\n",
          deliveryType, p->header.serial, rx_packetTypes[p->header.type - 1], ntohl(peer->host),
          ntohs(peer->port), p->header.serial, p->header.epoch, p->header.cid, p->header.callNumber,
          p->header.seq, p->header.flags, p, p->length));
#endif
    if (rx_stats_active) {
        rx_atomic_inc(&rx_stats.packetsSent[p->header.type - 1]);
        MUTEX_ENTER(&peer->peer_lock);
        peer->bytesSent += p->length;
        MUTEX_EXIT(&peer->peer_lock);
    }
}

/* Send a list of packets to appropriate destination for the specified
 * connection.  The headers are first encoded and placed in the packets.
 */
void
rxi_SendPacketList(struct rx_call *call, struct rx_connection *conn,
		   struct rx_packet **list, int len, int istack)
{
#if     defined(AFS_SUN5_ENV) && defined(KERNEL)
    int waslocked;
#endif
    struct sockaddr_in addr;
    struct rx_peer *peer = conn->peer;
    osi_socket socket;
    struct rx_packet *p = NULL;
    struct iovec wirevec[RX_MAXIOVECS];
    int i, length, code;
    afs_uint32 serial;
    afs_uint32 temp;
    struct rx_jumboHeader *jp;
#ifdef RXDEBUG
    char deliveryType = 'S';
#endif
    /* The address we're sending the packet to */
    addr.sin_family = AF_INET;
    addr.sin_port = peer->port;
    addr.sin_addr.s_addr = peer->host;
    memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

    if (len + 1 > RX_MAXIOVECS) {
	osi_Panic("rxi_SendPacketList, len > RX_MAXIOVECS\n");
    }

    /*
     * Stamp the packets in this jumbogram with consecutive serial numbers
     */
    MUTEX_ENTER(&conn->conn_data_lock);
    serial = conn->serial;
    conn->serial += len;
    for (i = 0; i < len; i++) {
	p = list[i];
	/* a ping *or* a sequenced packet can count */
	if (p->length > conn->peer->maxPacketSize) {
	    if (((p->header.type == RX_PACKET_TYPE_ACK) &&
		 (p->header.flags & RX_REQUEST_ACK)) &&
		((i == 0) || (p->length >= conn->lastPingSize))) {
		conn->lastPingSize = p->length;
		conn->lastPingSizeSer = serial + i;
	    } else if ((p->header.seq != 0) &&
		       ((i == 0) || (p->length >= conn->lastPacketSize))) {
		conn->lastPacketSize = p->length;
		conn->lastPacketSizeSeq = p->header.seq;
	    }
	}
    }
    MUTEX_EXIT(&conn->conn_data_lock);


    /* This stuff should be revamped, I think, so that most, if not
     * all, of the header stuff is always added here.  We could
     * probably do away with the encode/decode routines. XXXXX */

    jp = NULL;
    length = RX_HEADER_SIZE;
    wirevec[0].iov_base = (char *)(&list[0]->wirehead[0]);
    wirevec[0].iov_len = RX_HEADER_SIZE;
    for (i = 0; i < len; i++) {
	p = list[i];

	/* The whole 3.5 jumbogram scheme relies on packets fitting
	 * in a single packet buffer. */
	if (p->niovecs > 2) {
	    osi_Panic("rxi_SendPacketList, niovecs > 2\n");
	}

	/* Set the RX_JUMBO_PACKET flags in all but the last packets
	 * in this chunk.  */
	if (i < len - 1) {
	    if (p->length != RX_JUMBOBUFFERSIZE) {
		osi_Panic("rxi_SendPacketList, length != jumbo size\n");
	    }
	    p->header.flags |= RX_JUMBO_PACKET;
	    length += RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;
	    wirevec[i + 1].iov_len = RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;
	} else {
	    wirevec[i + 1].iov_len = p->length;
	    length += p->length;
	}
	wirevec[i + 1].iov_base = (char *)(&p->localdata[0]);
	if (jp != NULL) {
	    /* Convert jumbo packet header to network byte order */
	    temp = (afs_uint32) (p->header.flags) << 24;
	    temp |= (afs_uint32) (p->header.spare);
	    *(afs_uint32 *) jp = htonl(temp);
	}
	jp = (struct rx_jumboHeader *)
	    ((char *)(&p->localdata[0]) + RX_JUMBOBUFFERSIZE);

	/* Stamp each packet with a unique serial number.  The serial
	 * number is maintained on a connection basis because some types
	 * of security may be based on the serial number of the packet,
	 * and security is handled on a per authenticated-connection
	 * basis. */
	/* Pre-increment, to guarantee no zero serial number; a zero
	 * serial number means the packet was never sent. */
	p->header.serial = ++serial;
	/* This is so we can adjust retransmit time-outs better in the face of
	 * rapidly changing round-trip times.  RTO estimation is not a la Karn.
	 */
	if (p->firstSerial == 0) {
	    p->firstSerial = p->header.serial;
	}
#ifdef RXDEBUG
	/* If an output tracer function is defined, call it with the packet and
	 * network address.  Note this function may modify its arguments. */
	if (rx_almostSent) {
	    int drop = (*rx_almostSent) (p, &addr);
	    /* drop packet if return value is non-zero? */
	    if (drop)
		deliveryType = 'D';	/* Drop the packet */
	}
#endif

	/* Get network byte order header */
	rxi_EncodePacketHeader(p);	/* XXX in the event of rexmit, etc, don't need to
					 * touch ALL the fields */
    }

    /* Send the packet out on the same socket that related packets are being
     * received on */
    socket =
	(conn->type ==
	 RX_CLIENT_CONNECTION ? rx_socket : conn->service->socket);

#ifdef RXDEBUG
    /* Possibly drop this packet,  for testing purposes */
    if ((deliveryType == 'D')
	|| ((rx_intentionallyDroppedPacketsPer100 > 0)
	    && (random() % 100 < rx_intentionallyDroppedPacketsPer100))) {
	deliveryType = 'D';	/* Drop the packet */
    } else {
	deliveryType = 'S';	/* Send the packet */
#endif /* RXDEBUG */

	/* Loop until the packet is sent.  We'd prefer just to use a
	 * blocking socket, but unfortunately the interface doesn't
	 * allow us to have the socket block in send mode, and not
	 * block in receive mode */
#if	defined(AFS_SUN5_ENV) && defined(KERNEL)
	waslocked = ISAFS_GLOCK();
	if (!istack && waslocked)
	    AFS_GUNLOCK();
#endif
	if ((code =
	     rxi_NetSend(socket, &addr, &wirevec[0], len + 1, length,
			 istack)) != 0) {
	    /* send failed, so let's hurry up the resend, eh? */
            if (rx_stats_active)
                rx_atomic_inc(&rx_stats.netSendFailures);
	    for (i = 0; i < len; i++) {
		p = list[i];
		p->flags &= ~RX_PKTFLAG_SENT;  /* resend it very soon */
	    }
	    /* Some systems are nice and tell us right away that we cannot
	     * reach this recipient by returning an error code.
	     * So, when this happens let's "down" the host NOW so
	     * we don't sit around waiting for this host to timeout later.
	     */
	    if (call) {
		rxi_NetSendError(call, code);
	    }
	}
#if	defined(AFS_SUN5_ENV) && defined(KERNEL)
	if (!istack && waslocked)
	    AFS_GLOCK();
#endif
#ifdef RXDEBUG
    }

    osi_Assert(p != NULL);

    dpf(("%c %d %s: %x.%u.%u.%u.%u.%u.%u flags %d, packet %"AFS_PTR_FMT" len %d\n",
          deliveryType, p->header.serial, rx_packetTypes[p->header.type - 1], ntohl(peer->host),
          ntohs(peer->port), p->header.serial, p->header.epoch, p->header.cid, p->header.callNumber,
          p->header.seq, p->header.flags, p, p->length));

#endif
    if (rx_stats_active) {
        rx_atomic_inc(&rx_stats.packetsSent[p->header.type - 1]);
        MUTEX_ENTER(&peer->peer_lock);
        peer->bytesSent += p->length;
        MUTEX_EXIT(&peer->peer_lock);
    }
}

/* Send a raw abort packet, without any call or connection structures */
void
rxi_SendRawAbort(osi_socket socket, afs_uint32 host, u_short port,
		 afs_uint32 serial, afs_int32 error,
		 struct rx_packet *source, int istack)
{
    struct rx_header theader;
    struct sockaddr_in addr;
    struct iovec iov[2];

    memset(&theader, 0, sizeof(theader));
    theader.epoch = htonl(source->header.epoch);
    theader.callNumber = htonl(source->header.callNumber);
    theader.serial = htonl(serial);
    theader.type = RX_PACKET_TYPE_ABORT;
    theader.serviceId = htons(source->header.serviceId);
    theader.securityIndex = source->header.securityIndex;
    theader.cid = htonl(source->header.cid);

    /*
     * If the abort is being sent in response to a server initiated packet,
     * set client_initiated in the abort to ensure it is not associated by
     * the receiver with a connection in the opposite direction.
     */
    if ((source->header.flags & RX_CLIENT_INITIATED) != RX_CLIENT_INITIATED)
        theader.flags |= RX_CLIENT_INITIATED;

    error = htonl(error);

    iov[0].iov_base = &theader;
    iov[0].iov_len = sizeof(struct rx_header);
    iov[1].iov_base = &error;
    iov[1].iov_len = sizeof(error);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = host;
    addr.sin_port = port;
    memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr.sin_len = sizeof(struct sockaddr_in);
#endif

    rxi_NetSend(socket, &addr, iov, 2,
		sizeof(struct rx_header) + sizeof(error), istack);
}

/* Send a "special" packet to the peer connection.  If call is
 * specified, then the packet is directed to a specific call channel
 * associated with the connection, otherwise it is directed to the
 * connection only. Uses optionalPacket if it is supplied, rather than
 * allocating a new packet buffer.  Nbytes is the length of the data
 * portion of the packet.  If data is non-null, nbytes of data are
 * copied into the packet.  Type is the type of the packet, as defined
 * in rx.h.  Bug: there's a lot of duplication between this and other
 * routines.  This needs to be cleaned up. */
struct rx_packet *
rxi_SendSpecial(struct rx_call *call,
		struct rx_connection *conn,
		struct rx_packet *optionalPacket, int type, char *data,
		int nbytes, int istack)
{
    /* Some of the following stuff should be common code for all
     * packet sends (it's repeated elsewhere) */
    struct rx_packet *p;
    unsigned int i = 0;
    int savelen = 0, saven = 0;
    int channel, callNumber;
    if (call) {
	channel = call->channel;
	callNumber = *call->callNumber;
	/* BUSY packets refer to the next call on this connection */
	if (type == RX_PACKET_TYPE_BUSY) {
	    callNumber++;
	}
    } else {
	channel = 0;
	callNumber = 0;
    }
    p = optionalPacket;
    if (!p) {
	p = rxi_AllocPacket(RX_PACKET_CLASS_SPECIAL);
	if (!p)
	    osi_Panic("rxi_SendSpecial failure");
    }

    if (nbytes != -1)
	p->length = nbytes;
    else
	nbytes = p->length;
    p->header.serviceId = conn->serviceId;
    p->header.securityIndex = conn->securityIndex;
    p->header.cid = (conn->cid | channel);
    p->header.callNumber = callNumber;
    p->header.seq = 0;
    p->header.epoch = conn->epoch;
    p->header.type = type;
    p->header.userStatus = 0;
    p->header.flags = 0;
    if (conn->type == RX_CLIENT_CONNECTION)
	p->header.flags |= RX_CLIENT_INITIATED;
    if (data)
	rx_packetwrite(p, 0, nbytes, data);

    for (i = 1; i < p->niovecs; i++) {
	if (nbytes <= p->wirevec[i].iov_len) {
	    savelen = p->wirevec[i].iov_len;
	    saven = p->niovecs;
	    p->wirevec[i].iov_len = nbytes;
	    p->niovecs = i + 1;	/* so condition fails because i == niovecs */
	} else
	    nbytes -= p->wirevec[i].iov_len;
    }

    if (call)
	rxi_Send(call, p, istack);
    else
	rxi_SendPacket((struct rx_call *)0, conn, p, istack);
    if (saven) {		/* means we truncated the packet above.  We probably don't  */
	/* really need to do this, but it seems safer this way, given that  */
	/* sneaky optionalPacket... */
	p->wirevec[i - 1].iov_len = savelen;
	p->niovecs = saven;
    }
    if (!optionalPacket)
	rxi_FreePacket(p);
    return optionalPacket;
}


/* Encode the packet's header (from the struct header in the packet to
 * the net byte order representation in the wire representation of the
 * packet, which is what is actually sent out on the wire) */
void
rxi_EncodePacketHeader(struct rx_packet *p)
{
    afs_uint32 *buf = (afs_uint32 *) (p->wirevec[0].iov_base);	/* MTUXXX */

    memset(buf, 0, RX_HEADER_SIZE);
    *buf++ = htonl(p->header.epoch);
    *buf++ = htonl(p->header.cid);
    *buf++ = htonl(p->header.callNumber);
    *buf++ = htonl(p->header.seq);
    *buf++ = htonl(p->header.serial);
    *buf++ = htonl((((afs_uint32) p->header.type) << 24)
		   | (((afs_uint32) p->header.flags) << 16)
		   | (p->header.userStatus << 8) | p->header.securityIndex);
    /* Note: top 16 bits of this next word were reserved */
    *buf++ = htonl((p->header.spare << 16) | (p->header.serviceId & 0xffff));
}

/* Decode the packet's header (from net byte order to a struct header) */
void
rxi_DecodePacketHeader(struct rx_packet *p)
{
    afs_uint32 *buf = (afs_uint32 *) (p->wirevec[0].iov_base);	/* MTUXXX */
    afs_uint32 temp;

    p->header.epoch = ntohl(*buf);
    buf++;
    p->header.cid = ntohl(*buf);
    buf++;
    p->header.callNumber = ntohl(*buf);
    buf++;
    p->header.seq = ntohl(*buf);
    buf++;
    p->header.serial = ntohl(*buf);
    buf++;

    temp = ntohl(*buf);
    buf++;

    /* C will truncate byte fields to bytes for me */
    p->header.type = temp >> 24;
    p->header.flags = temp >> 16;
    p->header.userStatus = temp >> 8;
    p->header.securityIndex = temp >> 0;

    temp = ntohl(*buf);
    buf++;

    p->header.serviceId = (temp & 0xffff);
    p->header.spare = temp >> 16;
    /* Note: top 16 bits of this last word are the security checksum */
}

/*
 * LOCKS HELD: called with call->lock held.
 *
 * PrepareSendPacket is the only place in the code that
 * can increment call->tnext.  This could become an atomic
 * in the future.  Beyond that there is nothing in this
 * function that requires the call being locked.  This
 * function can only be called by the application thread.
 */
void
rxi_PrepareSendPacket(struct rx_call *call,
		      struct rx_packet *p, int last)
{
    struct rx_connection *conn = call->conn;
    afs_uint32 seq = call->tnext++;
    unsigned int i;
    afs_int32 len;		/* len must be a signed type; it can go negative */
    int code;

    /* No data packets on call 0. Where do these come from? */
    if (*call->callNumber == 0)
	*call->callNumber = 1;

    MUTEX_EXIT(&call->lock);
    p->flags &= ~(RX_PKTFLAG_ACKED | RX_PKTFLAG_SENT);

    p->header.cid = (conn->cid | call->channel);
    p->header.serviceId = conn->serviceId;
    p->header.securityIndex = conn->securityIndex;

    p->header.callNumber = *call->callNumber;
    p->header.seq = seq;
    p->header.epoch = conn->epoch;
    p->header.type = RX_PACKET_TYPE_DATA;
    p->header.userStatus = 0;
    p->header.flags = 0;
    p->header.spare = 0;
    if (conn->type == RX_CLIENT_CONNECTION)
	p->header.flags |= RX_CLIENT_INITIATED;

    if (last)
	p->header.flags |= RX_LAST_PACKET;

    clock_Zero(&p->firstSent);	/* Never yet transmitted */
    p->header.serial = 0;	/* Another way of saying never transmitted... */

    /* Now that we're sure this is the last data on the call, make sure
     * that the "length" and the sum of the iov_lens matches. */
    len = p->length + call->conn->securityHeaderSize;

    for (i = 1; i < p->niovecs && len > 0; i++) {
	len -= p->wirevec[i].iov_len;
    }
    if (len > 0) {
	osi_Panic("PrepareSendPacket 1\n");	/* MTUXXX */
    } else if (i < p->niovecs) {
	/* Free any extra elements in the wirevec */
#if defined(RX_ENABLE_TSFPQ)
	rxi_FreeDataBufsTSFPQ(p, i, 1 /* allow global pool flush if overquota */);
#else /* !RX_ENABLE_TSFPQ */
        MUTEX_ENTER(&rx_freePktQ_lock);
	rxi_FreeDataBufsNoLock(p, i);
        MUTEX_EXIT(&rx_freePktQ_lock);
#endif /* !RX_ENABLE_TSFPQ */

        p->niovecs = i;
    }
    if (len)
        p->wirevec[i - 1].iov_len += len;
    MUTEX_ENTER(&call->lock);
    code = RXS_PreparePacket(conn->securityObject, call, p);
    if (code) {
	MUTEX_EXIT(&call->lock);
	rxi_ConnectionError(conn, code);
	MUTEX_ENTER(&conn->conn_data_lock);
	p = rxi_SendConnectionAbort(conn, p, 0, 0);
	MUTEX_EXIT(&conn->conn_data_lock);
	MUTEX_ENTER(&call->lock);
	/* setting a connection error means all calls for that conn are also
	 * error'd. if this call does not have an error by now, something is
	 * very wrong, and we risk sending data in the clear that is supposed
	 * to be encrypted. */
	osi_Assert(call->error);
    }
}

/* Given an interface MTU size, calculate an adjusted MTU size that
 * will make efficient use of the RX buffers when the peer is sending
 * either AFS 3.4a jumbograms or AFS 3.5 jumbograms.  */
int
rxi_AdjustIfMTU(int mtu)
{
    int adjMTU;
    int frags;

    if (rxi_nRecvFrags == 1 && rxi_nSendFrags == 1)
        return mtu;
    adjMTU = RX_HEADER_SIZE + RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE;
    if (mtu <= adjMTU) {
	return mtu;
    }
    mtu -= adjMTU;
    if (mtu <= 0) {
	return adjMTU;
    }
    frags = mtu / (RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE);
    return (adjMTU + (frags * (RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE)));
}

/* Given an interface MTU size, and the peer's advertised max receive
 * size, calculate an adjisted maxMTU size that makes efficient use
 * of our packet buffers when we are sending AFS 3.4a jumbograms. */
int
rxi_AdjustMaxMTU(int mtu, int peerMaxMTU)
{
    int maxMTU = mtu * rxi_nSendFrags;
    maxMTU = MIN(maxMTU, peerMaxMTU);
    return rxi_AdjustIfMTU(maxMTU);
}

/* Given a packet size, figure out how many datagram packet will fit.
 * The first buffer always contains RX_HEADER_SIZE+RX_JUMBOBUFFERSIZE+
 * RX_JUMBOHEADERSIZE, the middle buffers contain RX_JUMBOBUFFERSIZE+
 * RX_JUMBOHEADERSIZE, and the last buffer contains RX_JUMBOBUFFERSIZE */
int
rxi_AdjustDgramPackets(int frags, int mtu)
{
    int maxMTU;
    if (mtu + IPv6_FRAG_HDR_SIZE < RX_JUMBOBUFFERSIZE + RX_HEADER_SIZE) {
	return 1;
    }
    maxMTU = (frags * (mtu + UDP_HDR_SIZE)) - UDP_HDR_SIZE;
    maxMTU = MIN(maxMTU, RX_MAX_PACKET_SIZE);
    /* subtract the size of the first and last packets */
    maxMTU -= RX_HEADER_SIZE + (2 * RX_JUMBOBUFFERSIZE) + RX_JUMBOHEADERSIZE;
    if (maxMTU < 0) {
	return 1;
    }
    return (2 + (maxMTU / (RX_JUMBOBUFFERSIZE + RX_JUMBOHEADERSIZE)));
}

#ifndef KERNEL
/*
 * This function can be used by the Windows Cache Manager
 * to dump the list of all rx packets so that we can determine
 * where the packet leakage is.
 */
int rx_DumpPackets(FILE *outputFile, char *cookie)
{
#ifdef RXDEBUG_PACKET
    struct rx_packet *p;
#ifdef AFS_NT40_ENV
    int zilch;
    char output[2048];
#define RXDPRINTF sprintf
#define RXDPRINTOUT output
#else
#define RXDPRINTF fprintf
#define RXDPRINTOUT outputFile
#endif

    NETPRI;
    MUTEX_ENTER(&rx_freePktQ_lock);
    RXDPRINTF(RXDPRINTOUT, "%s - Start dumping all Rx Packets - count=%u\r\n", cookie, rx_packet_id);
#ifdef AFS_NT40_ENV
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
#endif

    for (p = rx_mallocedP; p; p = p->allNextp) {
        RXDPRINTF(RXDPRINTOUT, "%s - packet=0x%p, id=%u, firstSent=%u.%08u, timeSent=%u.%08u, firstSerial=%u, niovecs=%u, flags=0x%x, length=%u  header: epoch=%u, cid=%u, callNum=%u, seq=%u, serial=%u, type=%u, flags=0x%x, userStatus=%u, securityIndex=%u, serviceId=%u\r\n",
                cookie, p, p->packetId, p->firstSent.sec, p->firstSent.usec, p->timeSent.sec, p->timeSent.usec,
                p->firstSerial, p->niovecs, (afs_uint32)p->flags, (afs_uint32)p->length,
                p->header.epoch, p->header.cid, p->header.callNumber, p->header.seq, p->header.serial,
                (afs_uint32)p->header.type, (afs_uint32)p->header.flags, (afs_uint32)p->header.userStatus,
                (afs_uint32)p->header.securityIndex, (afs_uint32)p->header.serviceId);
#ifdef AFS_NT40_ENV
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
#endif
    }

    RXDPRINTF(RXDPRINTOUT, "%s - End dumping all Rx Packets\r\n", cookie);
#ifdef AFS_NT40_ENV
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
#endif

    MUTEX_EXIT(&rx_freePktQ_lock);
    USERPRI;
#endif /* RXDEBUG_PACKET */
    return 0;
}
#endif
