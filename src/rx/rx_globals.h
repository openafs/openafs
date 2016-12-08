/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX:  Globals for internal use, basically */

#ifndef AFS_RX_GLOBALS_H
#define AFS_RX_GLOBALS_H


#ifdef	KERNEL
#include "rx/rx.h"
#else /* KERNEL */
# include "rx.h"
#endif /* KERNEL */

#ifndef GLOBALSINIT
#define GLOBALSINIT(x)
#define POSTAMBLE
#if defined(AFS_NT40_ENV)
#define RX_STATS_INTERLOCKED 1
#if defined(AFS_PTHREAD_ENV)
#define EXT __declspec(dllimport) extern
#else /* AFS_PTHREAD_ENV */
#define	EXT extern
#endif /* AFS_PTHREAD_ENV */
#else /* AFS_NT40_ENV */
#define EXT extern
#endif /* AFS_NT40_ENV */
#endif /* !GLOBALSINIT */

/* Basic socket for client requests; other sockets (for receiving server requests) are in the service structures */
EXT osi_socket rx_socket;

/* The array of installed services.  Null terminated. */
EXT struct rx_service *rx_services[RX_MAX_SERVICES + 1];
#ifdef RX_ENABLE_LOCKS
/* Protects nRequestsRunning as well as pool allocation variables. */
EXT afs_kmutex_t rx_serverPool_lock;
#endif /* RX_ENABLE_LOCKS */

/* Constant delay time before sending a hard ack if the receiver consumes
 * a packet while no delayed ack event is scheduled. Ensures that the
 * sender is able to advance its window when the receiver consumes a packet
 * after the sender has exhausted its transmit window.
 */
EXT struct clock rx_hardAckDelay;

#if defined(RXDEBUG) || defined(AFS_NT40_ENV)
/* Variable to allow introduction of network unreliability; exported from libafsrpc */
EXT int rx_intentionallyDroppedPacketsPer100 GLOBALSINIT(0);	/* Dropped on Send */
EXT int rx_intentionallyDroppedOnReadPer100  GLOBALSINIT(0);	/* Dropped on Read */
#endif

/* extra packets to add to the quota */
EXT int rx_extraQuota GLOBALSINIT(0);
/* extra packets to alloc (2 * maxWindowSize by default) */
EXT int rx_extraPackets GLOBALSINIT(256);

EXT int rx_stackSize GLOBALSINIT(RX_DEFAULT_STACK_SIZE);

/* Time until an unresponsive connection is declared dead */
EXT int rx_connDeadTime GLOBALSINIT(12);

/* Set rx default connection dead time; set on both services and connections at creation time */
#ifdef AFS_NT40_ENV
void rx_SetRxDeadTime(int seconds);
#else
#define rx_SetRxDeadTime(seconds)   (rx_connDeadTime = (seconds))
#endif

/* Time until we toss an idle connection */
EXT int rx_idleConnectionTime GLOBALSINIT(700);
/* Time until we toss a peer structure, after all connections using are gone */
EXT int rx_idlePeerTime GLOBALSINIT(60);

/* The file server is temporarily salvaging */
EXT int rx_tranquil GLOBALSINIT(0);

/* UDP rcv buffer size */
EXT int rx_UdpBufSize GLOBALSINIT(64 * 1024);
#ifdef AFS_NT40_ENV
int   rx_GetMinUdpBufSize(void);
void  rx_SetUdpBufSize(int x);
#else
#define rx_GetMinUdpBufSize()   (64*1024)
#define rx_SetUdpBufSize(x)     (((x)>rx_GetMinUdpBufSize()) ? (rx_UdpBufSize = (x)):0)
#endif
/*
 * Variables to control RX overload management. When the number of calls
 * waiting for a thread exceed the threshold, new calls are aborted
 * with the busy error.
 */
EXT int rx_BusyThreshold GLOBALSINIT(-1);	/* default is disabled */
EXT int rx_BusyError GLOBALSINIT(-1);

/* These definitions should be in one place */
#ifdef	AFS_SUN5_ENV
#define	RX_CBUF_TIME	180	/* Check for packet deficit */
#define	RX_REAP_TIME	90	/* Check for tossable connections every 90 seconds */
#else
#define	RX_CBUF_TIME	120	/* Check for packet deficit */
#define	RX_REAP_TIME	60	/* Check for tossable connections every 60 seconds */
#endif

#define RX_FAST_ACK_RATE 1	/* as of 3.4, ask for an ack every
				 * other packet. */

EXT int rx_minPeerTimeout GLOBALSINIT(20);      /* in milliseconds */
EXT int rx_minWindow GLOBALSINIT(1);
EXT int rx_maxWindow GLOBALSINIT(RX_MAXACKS);   /* must ack what we receive */
EXT int rx_initReceiveWindow GLOBALSINIT(16);	/* how much to accept */
EXT int rx_maxReceiveWindow GLOBALSINIT(32);	/* how much to accept */
EXT int rx_initSendWindow GLOBALSINIT(16);
EXT int rx_maxSendWindow GLOBALSINIT(32);
EXT int rx_nackThreshold GLOBALSINIT(3);	/* Number NACKS to trigger congestion recovery */
EXT int rx_nDgramThreshold GLOBALSINIT(4);	/* Number of packets before increasing
                                                 * packets per datagram */
#define RX_MAX_FRAGS 4
EXT int rxi_nSendFrags GLOBALSINIT(RX_MAX_FRAGS);	/* max fragments in a datagram */
EXT int rxi_nRecvFrags GLOBALSINIT(RX_MAX_FRAGS);
EXT int rxi_OrphanFragSize GLOBALSINIT(512);

#define RX_MAX_DGRAM_PACKETS 6	/* max packets per jumbogram */

EXT int rxi_nDgramPackets GLOBALSINIT(RX_MAX_DGRAM_PACKETS);
/* allow n packets between soft acks */
EXT int rxi_SoftAckRate GLOBALSINIT(RX_FAST_ACK_RATE);
/* consume n packets before sending hard ack, should be larger than above,
   but not absolutely necessary.  If it's smaller, than fast receivers will
   send a soft ack, immediately followed by a hard ack. */
EXT int rxi_HardAckRate GLOBALSINIT(RX_FAST_ACK_RATE + 1);

EXT int rx_nPackets GLOBALSINIT(0);	/* preallocate packets with rx_extraPackets */

/*
 * pthreads thread-specific rx info support
 * the rx_ts_info_t struct is meant to support all kinds of
 * thread-specific rx data:
 *
 *  _FPQ member contains a thread-specific free packet queue
 */
#ifdef AFS_PTHREAD_ENV
EXT pthread_key_t rx_ts_info_key;
typedef struct rx_ts_info_t {
    struct {
        struct opr_queue queue;
        int len;                /* local queue length */
        int delta;              /* number of new packets alloc'd locally since last sync w/ global queue */

        /* FPQ stats */
        int checkin_ops;
        int checkin_xfer;
        int checkout_ops;
        int checkout_xfer;
        int gtol_ops;
        int gtol_xfer;
        int ltog_ops;
        int ltog_xfer;
        int lalloc_ops;
        int lalloc_xfer;
        int galloc_ops;
        int galloc_xfer;
    } _FPQ;
    struct rx_packet * local_special_packet;
} rx_ts_info_t;
EXT struct rx_ts_info_t * rx_ts_info_init(void);   /* init function for thread-specific data struct */
#define RX_TS_INFO_GET(ts_info_p) \
    do { \
        ts_info_p = (struct rx_ts_info_t*)pthread_getspecific(rx_ts_info_key); \
        if (ts_info_p == NULL) { \
            opr_Verify((ts_info_p = rx_ts_info_init()) != NULL); \
        } \
    } while(0)
#endif /* AFS_PTHREAD_ENV */


/* List of free packets */
/* in pthreads rx, free packet queue is now a two-tiered queueing system
 * in which the first tier is thread-specific, and the second tier is
 * a global free packet queue */
EXT struct opr_queue rx_freePacketQueue;
#ifdef RX_TRACK_PACKETS
#define RX_FPQ_MARK_FREE(p) \
    do { \
        if ((p)->flags & RX_PKTFLAG_FREE) \
            osi_Panic("rx packet already free\n"); \
        (p)->flags |= RX_PKTFLAG_FREE; \
        (p)->flags &= ~(RX_PKTFLAG_TQ|RX_PKTFLAG_IOVQ|RX_PKTFLAG_RQ|RX_PKTFLAG_CP); \
        (p)->length = 0; \
        (p)->niovecs = 0; \
    } while(0)
#define RX_FPQ_MARK_USED(p) \
    do { \
        if (!((p)->flags & RX_PKTFLAG_FREE)) \
            osi_Panic("rx packet not free\n"); \
        (p)->flags = 0;		/* clear RX_PKTFLAG_FREE, initialize the rest */ \
        (p)->header.flags = 0; \
    } while(0)
#else
#define RX_FPQ_MARK_FREE(p) \
    do { \
        (p)->length = 0; \
        (p)->niovecs = 0; \
    } while(0)
#define RX_FPQ_MARK_USED(p) \
    do { \
        (p)->flags = 0;		/* clear RX_PKTFLAG_FREE, initialize the rest */ \
        (p)->header.flags = 0; \
    } while(0)
#endif
#define RX_PACKET_IOV_INIT(p) \
    do { \
	(p)->wirevec[0].iov_base = (char *)((p)->wirehead); \
	(p)->wirevec[0].iov_len = RX_HEADER_SIZE; \
	(p)->wirevec[1].iov_base = (char *)((p)->localdata); \
	(p)->wirevec[1].iov_len = RX_FIRSTBUFFERSIZE; \
    } while(0)
#define RX_PACKET_IOV_FULLINIT(p) \
    do { \
	(p)->wirevec[0].iov_base = (char *)((p)->wirehead); \
	(p)->wirevec[0].iov_len = RX_HEADER_SIZE; \
	(p)->wirevec[1].iov_base = (char *)((p)->localdata); \
	(p)->wirevec[1].iov_len = RX_FIRSTBUFFERSIZE; \
	(p)->niovecs = 2; \
	(p)->length = RX_FIRSTBUFFERSIZE; \
    } while(0)

#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_freePktQ_lock;
#endif /* RX_ENABLE_LOCKS */

/*!
 * \brief Queue of allocated packets.
 *
 * This queue is used to keep track of the blocks of allocated packets.
 * This information is used when afs is being unmounted and the memory
 * used by those packets needs to be released.
 */
EXT struct opr_queue rx_mallocedPacketQueue;
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_mallocedPktQ_lock;
#endif /* RX_ENABLE_LOCKS */

#if defined(AFS_PTHREAD_ENV) && !defined(KERNEL)
#define RX_ENABLE_TSFPQ
EXT int rx_TSFPQGlobSize GLOBALSINIT(3); /* number of packets to transfer between global and local queues in one op */
EXT int rx_TSFPQLocalMax GLOBALSINIT(15); /* max number of packets on local FPQ before returning a glob to the global pool */
EXT int rx_TSFPQMaxProcs GLOBALSINIT(0); /* max number of threads expected */
#define RX_TS_FPQ_FLUSH_GLOBAL 1
#define RX_TS_FPQ_PULL_GLOBAL 1
#define RX_TS_FPQ_ALLOW_OVERCOMMIT 1
/*
 * compute the localmax and globsize values from rx_TSFPQMaxProcs and rx_nPackets.
 * arbitarily set local max so that all threads consume 90% of packets, if all local queues are full.
 * arbitarily set transfer glob size to 20% of max local packet queue length.
 * also set minimum values of 15 and 3.  Given the algorithms, the number of buffers allocated
 * by each call to AllocPacketBufs() will increase indefinitely without a cap on the transfer
 * glob size.  A cap of 64 is selected because that will produce an allocation of greater than
 * three times that amount which is greater than half of ncalls * maxReceiveWindow.
 * Must be called under rx_packets_mutex.
 */
#define RX_TS_FPQ_COMPUTE_LIMITS \
    do { \
        int newmax, newglob; \
        newmax = (rx_nPackets * 9) / (10 * rx_TSFPQMaxProcs); \
        newmax = (newmax >= 15) ? newmax : 15; \
        newglob = newmax / 5; \
        newglob = (newglob >= 3) ? (newglob < 64 ? newglob : 64) : 3; \
        rx_TSFPQLocalMax = newmax; \
        rx_TSFPQGlobSize = newglob; \
    } while(0)
/* record the number of packets allocated by this thread
 * and stored in the thread local queue */
#define RX_TS_FPQ_LOCAL_ALLOC(rx_ts_info_p,num_alloc) \
    do { \
        (rx_ts_info_p)->_FPQ.lalloc_ops++; \
        (rx_ts_info_p)->_FPQ.lalloc_xfer += num_alloc; \
    } while (0)
/* record the number of packets allocated by this thread
 * and stored in the global queue */
#define RX_TS_FPQ_GLOBAL_ALLOC(rx_ts_info_p,num_alloc) \
    do { \
        (rx_ts_info_p)->_FPQ.galloc_ops++; \
        (rx_ts_info_p)->_FPQ.galloc_xfer += num_alloc; \
    } while (0)
/* move packets from local (thread-specific) to global free packet queue.
   rx_freePktQ_lock must be held. default is to reduce the queue size to 40% ofmax */
#define RX_TS_FPQ_LTOG(rx_ts_info_p) \
    do { \
        int i; \
        struct rx_packet * p; \
        int tsize = MIN((rx_ts_info_p)->_FPQ.len, (rx_ts_info_p)->_FPQ.len - rx_TSFPQLocalMax + 3 *  rx_TSFPQGlobSize); \
	if (tsize <= 0) break; \
        for (i=0,p=opr_queue_Last(&((rx_ts_info_p)->_FPQ.queue), \
				 struct rx_packet, entry); \
             i < tsize; i++,p=opr_queue_Prev(&p->entry, \
		     			    struct rx_packet, entry )); \
        opr_queue_SplitAfterPrepend(&((rx_ts_info_p)->_FPQ.queue), \
				   &rx_freePacketQueue, &p->entry); \
        (rx_ts_info_p)->_FPQ.len -= tsize; \
        rx_nFreePackets += tsize; \
        (rx_ts_info_p)->_FPQ.ltog_ops++; \
        (rx_ts_info_p)->_FPQ.ltog_xfer += tsize; \
        if ((rx_ts_info_p)->_FPQ.delta) { \
            MUTEX_ENTER(&rx_packets_mutex); \
            RX_TS_FPQ_COMPUTE_LIMITS; \
            MUTEX_EXIT(&rx_packets_mutex); \
           (rx_ts_info_p)->_FPQ.delta = 0; \
        } \
    } while(0)
/* same as above, except user has direct control over number to transfer */
#define RX_TS_FPQ_LTOG2(rx_ts_info_p,num_transfer) \
    do { \
        int i; \
        struct rx_packet * p; \
        if (num_transfer <= 0) break; \
        for (i=0,p=opr_queue_Last(&((rx_ts_info_p)->_FPQ.queue), \
				 struct rx_packet, entry ); \
	     i < (num_transfer); \
	     i++,p=opr_queue_Prev(&p->entry, struct rx_packet, entry )); \
        opr_queue_SplitAfterPrepend(&((rx_ts_info_p)->_FPQ.queue), \
				   &rx_freePacketQueue, &p->entry); \
        (rx_ts_info_p)->_FPQ.len -= (num_transfer); \
        rx_nFreePackets += (num_transfer); \
        (rx_ts_info_p)->_FPQ.ltog_ops++; \
        (rx_ts_info_p)->_FPQ.ltog_xfer += (num_transfer); \
        if ((rx_ts_info_p)->_FPQ.delta) { \
            MUTEX_ENTER(&rx_packets_mutex); \
            RX_TS_FPQ_COMPUTE_LIMITS; \
            MUTEX_EXIT(&rx_packets_mutex); \
            (rx_ts_info_p)->_FPQ.delta = 0; \
        } \
    } while(0)
/* move packets from global to local (thread-specific) free packet queue.
   rx_freePktQ_lock must be held. */
#define RX_TS_FPQ_GTOL(rx_ts_info_p) \
    do { \
        int i, tsize; \
        struct rx_packet * p; \
        tsize = (rx_TSFPQGlobSize <= rx_nFreePackets) ? \
                 rx_TSFPQGlobSize : rx_nFreePackets; \
        for (i=0, \
	       p=opr_queue_First(&rx_freePacketQueue, struct rx_packet, entry); \
             i < tsize; \
	     i++,p=opr_queue_Next(&p->entry, struct rx_packet, entry)); \
        opr_queue_SplitBeforeAppend(&rx_freePacketQueue, \
				   &((rx_ts_info_p)->_FPQ.queue), &p->entry); \
        (rx_ts_info_p)->_FPQ.len += i; \
        rx_nFreePackets -= i; \
        (rx_ts_info_p)->_FPQ.gtol_ops++; \
        (rx_ts_info_p)->_FPQ.gtol_xfer += i; \
    } while(0)
/* same as above, except user has direct control over number to transfer */
#define RX_TS_FPQ_GTOL2(rx_ts_info_p,num_transfer) \
    do { \
        int i, tsize; \
        struct rx_packet * p; \
        tsize = (num_transfer); \
        if (tsize > rx_nFreePackets) tsize = rx_nFreePackets; \
        for (i=0, \
	       p=opr_queue_First(&rx_freePacketQueue, struct rx_packet, entry); \
             i < tsize; \
	     i++, p=opr_queue_Next(&p->entry, struct rx_packet, entry)); \
        opr_queue_SplitBeforeAppend(&rx_freePacketQueue, \
				   &((rx_ts_info_p)->_FPQ.queue), &p->entry); \
        (rx_ts_info_p)->_FPQ.len += i; \
        rx_nFreePackets -= i; \
        (rx_ts_info_p)->_FPQ.gtol_ops++; \
        (rx_ts_info_p)->_FPQ.gtol_xfer += i; \
    } while(0)
/* checkout a packet from the thread-specific free packet queue */
#define RX_TS_FPQ_CHECKOUT(rx_ts_info_p,p) \
    do { \
        (p) = opr_queue_First(&((rx_ts_info_p)->_FPQ.queue), \
			     struct rx_packet, entry); \
        opr_queue_Remove(&p->entry); \
        RX_FPQ_MARK_USED(p); \
        (rx_ts_info_p)->_FPQ.len--; \
        (rx_ts_info_p)->_FPQ.checkout_ops++; \
        (rx_ts_info_p)->_FPQ.checkout_xfer++; \
    } while(0)
/* checkout multiple packets from the thread-specific free packet queue.
 * num_transfer must be a variable.
 */
#define RX_TS_FPQ_QCHECKOUT(rx_ts_info_p,num_transfer,q) \
    do { \
        int i; \
        struct rx_packet *p; \
        if (num_transfer > (rx_ts_info_p)->_FPQ.len) num_transfer = (rx_ts_info_p)->_FPQ.len; \
        for (i=0, p=opr_queue_First(&((rx_ts_info_p)->_FPQ.queue), \
				   struct rx_packet, entry); \
             i < num_transfer; \
             i++, p=opr_queue_Next(&p->entry, struct rx_packet, entry)) { \
            RX_FPQ_MARK_USED(p); \
        } \
        opr_queue_SplitBeforeAppend(&((rx_ts_info_p)->_FPQ.queue),(q), \
				   &((p)->entry)); \
        (rx_ts_info_p)->_FPQ.len -= num_transfer; \
        (rx_ts_info_p)->_FPQ.checkout_ops++; \
        (rx_ts_info_p)->_FPQ.checkout_xfer += num_transfer; \
    } while(0)
/* check a packet into the thread-specific free packet queue */
#define RX_TS_FPQ_CHECKIN(rx_ts_info_p,p) \
    do { \
        opr_queue_Prepend(&((rx_ts_info_p)->_FPQ.queue), &((p)->entry)); \
        RX_FPQ_MARK_FREE(p); \
        (rx_ts_info_p)->_FPQ.len++; \
        (rx_ts_info_p)->_FPQ.checkin_ops++; \
        (rx_ts_info_p)->_FPQ.checkin_xfer++; \
    } while(0)
/* check multiple packets into the thread-specific free packet queue */
/* num_transfer must equal length of (q); it is not a means of checking
 * in part of (q).  passing num_transfer just saves us instructions
 * since caller already knows length of (q) for other reasons */
#define RX_TS_FPQ_QCHECKIN(rx_ts_info_p,num_transfer,q) \
    do { \
	struct opr_queue *cur; \
        for (opr_queue_Scan((q), cur)) { \
            RX_FPQ_MARK_FREE(opr_queue_Entry(cur, struct rx_packet, entry)); \
        } \
        opr_queue_SplicePrepend(&((rx_ts_info_p)->_FPQ.queue),(q)); \
        (rx_ts_info_p)->_FPQ.len += (num_transfer); \
        (rx_ts_info_p)->_FPQ.checkin_ops++; \
        (rx_ts_info_p)->_FPQ.checkin_xfer += (num_transfer); \
    } while(0)
#endif /* AFS_PTHREAD_ENV && !KERNEL */

/* Number of free packets */
EXT int rx_nFreePackets GLOBALSINIT(0);
EXT int rxi_NeedMorePackets GLOBALSINIT(0);
EXT int rx_packetReclaims GLOBALSINIT(0);

/* largest packet which we can safely receive, initialized to AFS 3.2 value
 * This is provided for backward compatibility with peers which may be unable
 * to swallow anything larger. THIS MUST NEVER DECREASE WHILE AN APPLICATION
 * IS RUNNING! */
EXT afs_uint32 rx_maxReceiveSize GLOBALSINIT(_OLD_MAX_PACKET_SIZE * RX_MAX_FRAGS +
				      UDP_HDR_SIZE * (RX_MAX_FRAGS - 1));

/* this is the maximum packet size that the user wants us to receive */
/* this is set by rxTune if required */
EXT afs_uint32 rx_maxReceiveSizeUser GLOBALSINIT(0xffffffff);

/* rx_MyMaxSendSize is the size of the largest packet we will send,
 * including the RX header. Just as rx_maxReceiveSize is the
 * max we will receive, including the rx header.
 */
EXT afs_uint32 rx_MyMaxSendSize GLOBALSINIT(8588);

/* Maximum size of a jumbo datagram we can receive */
EXT afs_uint32 rx_maxJumboRecvSize GLOBALSINIT(RX_MAX_PACKET_SIZE);

/* need this to permit progs to run on AIX systems */
EXT int (*rxi_syscallp) (afs_uint32 a3, afs_uint32 a4, void *a5)GLOBALSINIT(0);

/* List of free queue entries */
EXT struct rx_serverQueueEntry *rx_FreeSQEList GLOBALSINIT(0);
#ifdef	RX_ENABLE_LOCKS
EXT afs_kmutex_t freeSQEList_lock;
#endif

/* List of free call structures */
EXT struct opr_queue rx_freeCallQueue;
#ifdef	RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_freeCallQueue_lock;
#endif
EXT afs_int32 rxi_nCalls GLOBALSINIT(0);

/* Port requested at rx_Init.  If this is zero, the actual port used will be different--but it will only be used for client operations.  If non-zero, server provided services may use the same port. */
EXT u_short rx_port;

#if !defined(KERNEL) && !defined(AFS_PTHREAD_ENV)
/* 32-bit select Mask for rx_Listener. */
EXT fd_set rx_selectMask;
EXT osi_socket rx_maxSocketNumber;	/* Maximum socket number in the select mask. */
/* Minumum socket number in the select mask. */
EXT osi_socket rx_minSocketNumber GLOBALSINIT(0x7fffffff);
#endif

/* This is actually the minimum number of packets that must remain free,
    overall, immediately after a packet of the requested class has been
    allocated.  *WARNING* These must be assigned with a great deal of care.
    In order, these are receive quota, send quota, special quota, receive
    continuation quota, and send continuation quota. */
#define	RX_PACKET_QUOTAS {1, 10, 0, 1, 10}
/* value large enough to guarantee that no allocation fails due to RX_PACKET_QUOTAS.
   Make it a little bigger, just for fun */
#define	RX_MAX_QUOTA	15	/* part of min packet computation */
EXT int rx_packetQuota[RX_N_PACKET_CLASSES] GLOBALSINIT(RX_PACKET_QUOTAS);
EXT int meltdown_1pkt GLOBALSINIT(1);	/* prefer to schedule single-packet calls */
EXT int rxi_md2cnt GLOBALSINIT(0);	/* counter of skipped calls */
EXT int rxi_2dchoice GLOBALSINIT(1);	/* keep track of another call to schedule */

/* quota system: each attached server process must be able to make
    progress to avoid system deadlock, so we ensure that we can always
    handle the arrival of the next unacknowledged data packet for an
    attached call.  rxi_dataQuota gives the max # of packets that must be
    reserved for active calls for them to be able to make progress, which is
    essentially enough to queue up a window-full of packets (the first packet
    may be missing, so these may not get read) + the # of packets the thread
    may use before reading all of its input (# free must be one more than send
    packet quota).  Thus, each thread allocates rx_maxReceiveWindow+1 (max
    queued packets) + an extra for sending data.  The system also reserves
    RX_MAX_QUOTA (must be more than RX_PACKET_QUOTA[i], which is 10), so that
    the extra packet can be sent (must be under the system-wide send packet
    quota to send any packets) */
/* # to reserve so that thread with input can still make calls (send packets)
   without blocking */
EXT int rxi_dataQuota GLOBALSINIT(RX_MAX_QUOTA);	/* packets to reserve for active threads */

EXT afs_int32 rxi_availProcs GLOBALSINIT(0);	/* number of threads in the pool */
EXT afs_int32 rxi_totalMin GLOBALSINIT(0);	/* Sum(minProcs) forall services */
EXT afs_int32 rxi_minDeficit GLOBALSINIT(0);	/* number of procs needed to handle all minProcs */

EXT afs_uint32 rx_nextCid;		/* Next connection call id */
EXT afs_uint32 rx_epoch;		/* Initialization time of rx */
#ifdef	RX_ENABLE_LOCKS
EXT afs_kcondvar_t rx_waitingForPackets_cv;
#endif
EXT char rx_waitingForPackets;	/* Processes set and wait on this variable when waiting for packet buffers */

EXT struct rx_peer **rx_peerHashTable;
EXT struct rx_connection **rx_connHashTable;
EXT struct rx_connection *rx_connCleanup_list GLOBALSINIT(0);
EXT afs_uint32 rx_hashTableSize GLOBALSINIT(257);	/* Prime number */
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_peerHashTable_lock;
EXT afs_kmutex_t rx_connHashTable_lock;
#endif /* RX_ENABLE_LOCKS */

#define CONN_HASH(host, port, cid, epoch, type) ((((cid)>>RX_CIDSHIFT)%rx_hashTableSize))

#define PEER_HASH(host, port)  ((host ^ port) % rx_hashTableSize)

/* Forward definitions of internal procedures */
#define	rxi_ChallengeOff(conn)	\
	rxevent_Cancel(&(conn)->challengeEvent)
#define rxi_NatKeepAliveOff(conn) \
	rxevent_Cancel(&(conn)->natKeepAliveEvent)

#define rxi_AllocSecurityObject() rxi_Alloc(sizeof(struct rx_securityClass))
#define	rxi_FreeSecurityObject(obj) rxi_Free(obj, sizeof(struct rx_securityClass))
#define	rxi_AllocService()	rxi_Alloc(sizeof(struct rx_service))
#define	rxi_FreeService(obj) \
do { \
    MUTEX_DESTROY(&(obj)->svc_data_lock);  \
    rxi_Free((obj), sizeof(struct rx_service)); \
} while (0)
#define	rxi_AllocPeer()		rxi_Alloc(sizeof(struct rx_peer))
#define	rxi_FreePeer(peer)	rxi_Free(peer, sizeof(struct rx_peer))
#define	rxi_AllocConnection()	rxi_Alloc(sizeof(struct rx_connection))
#define rxi_FreeConnection(conn) (rxi_Free(conn, sizeof(struct rx_connection)))

EXT afs_int32 rx_stats_active GLOBALSINIT(1);	/* boolean - rx statistics gathering */

#ifndef KERNEL
/* Some debugging stuff */
EXT FILE *rx_debugFile;		/* Set by the user to a stdio file for debugging output */
EXT FILE *rxevent_debugFile;	/* Set to an stdio descriptor for event logging to that file */
#endif

#ifdef RXDEBUG
# define rx_Log rx_debugFile
# ifdef AFS_NT40_ENV
EXT int rxdebug_active;
#  define dpf(args) do { if (rxdebug_active) rxi_DebugPrint args; } while (0)
# else
#  ifdef DPF_FSLOG
#   include <afs/afsutil.h>
#   define dpf(args) FSLog args
#  else
#   define dpf(args) do { if (rx_debugFile) rxi_DebugPrint args; } while (0)
#  endif
# endif
# define rx_Log_event rxevent_debugFile
#else
# define dpf(args)
#endif /* RXDEBUG */

EXT char *rx_packetTypes[RX_N_PACKET_TYPES] GLOBALSINIT(RX_PACKET_TYPES);	/* Strings defined in rx.h */

#ifndef KERNEL
/*
 * Counter used to implement connection specific data
 */
EXT int rxi_keyCreate_counter GLOBALSINIT(0);
/*
 * Array of function pointers used to destory connection specific data
 */
EXT rx_destructor_t *rxi_keyCreate_destructor GLOBALSINIT(NULL);
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rxi_keyCreate_lock;
#endif /* RX_ENABLE_LOCKS */
#endif /* !KERNEL */

/*
 * SERVER ONLY: Threshholds used to throttle error replies to looping
 * clients. When consecutive calls are aborting with the same error, the
 * server throttles the client by waiting before sending error messages.
 * Disabled if abort thresholds are zero.
 */
EXT int rxi_connAbortThreshhold GLOBALSINIT(0);
EXT int rxi_connAbortDelay GLOBALSINIT(3000);
EXT int rxi_callAbortThreshhold GLOBALSINIT(0);
EXT int rxi_callAbortDelay GLOBALSINIT(3000);

/*
 * Thread specific thread ID used to implement LWP_Index().
 */

#if defined(AFS_PTHREAD_ENV)
EXT int rxi_fcfs_thread_num GLOBALSINIT(0);
EXT pthread_key_t rx_thread_id_key;
#else
#define rxi_fcfs_thread_num (0)
#endif

#if defined(RX_ENABLE_LOCKS)
EXT afs_kmutex_t rx_waiting_mutex POSTAMBLE;	/* used to protect waiting counters */
EXT afs_kmutex_t rx_quota_mutex POSTAMBLE;	/* used to protect quota counters */
EXT afs_kmutex_t rx_pthread_mutex POSTAMBLE;	/* used to protect pthread counters */
EXT afs_kmutex_t rx_packets_mutex POSTAMBLE;	/* used to protect packet counters */
EXT afs_kmutex_t rx_refcnt_mutex POSTAMBLE;       /* used to protect conn/call ref counts */
#endif

EXT int rx_enable_stats GLOBALSINIT(0);

/*
 * Set this flag to enable the listener thread to trade places with an idle
 * worker thread to move the context switch from listener to worker out of
 * the request path.
 */
EXT int rx_enable_hot_thread GLOBALSINIT(0);

EXT int RX_IPUDP_SIZE GLOBALSINIT(_RX_IPUDP_SIZE);
#endif /* AFS_RX_GLOBALS_H */
