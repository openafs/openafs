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

#ifndef INIT
#define INIT(x)
#if defined(AFS_NT40_ENV) && defined(AFS_PTHREAD_ENV)
#define EXT __declspec(dllimport) extern
#else
#define	EXT extern
#endif
#endif /* !INIT */

/* Basic socket for client requests; other sockets (for receiving server requests) are in the service structures */
EXT osi_socket rx_socket;

/* The array of installed services.  Null terminated. */
EXT struct rx_service *rx_services[RX_MAX_SERVICES + 1];
#ifdef RX_ENABLE_LOCKS
/* Protects nRequestsRunning as well as pool allocation variables. */
EXT afs_kmutex_t rx_serverPool_lock;
#endif /* RX_ENABLE_LOCKS */

/* Incoming calls wait on this queue when there are no available server processes */
EXT struct rx_queue rx_incomingCallQueue;

/* Server processes wait on this queue when there are no appropriate calls to process */
EXT struct rx_queue rx_idleServerQueue;

/* Constant delay time before sending an acknowledge of the last packet received.  This is to avoid sending an extra acknowledge when the client is about to make another call, anyway, or the server is about to respond. */
EXT struct clock rx_lastAckDelay;

/* Constant delay time before sending a hard ack if the receiver consumes
 * a packet while no delayed ack event is scheduled. Ensures that the
 * sender is able to advance its window when the receiver consumes a packet
 * after the sender has exhausted its transmit window.
 */
EXT struct clock rx_hardAckDelay;

/* Constant delay time before sending a soft ack when none was requested.
 * This is to make sure we send soft acks before the sender times out,
 * Normally we wait and send a hard ack when the receiver consumes the packet */
EXT struct clock rx_softAckDelay;

/* Variable to allow introduction of network unreliability */
#ifdef RXDEBUG
EXT int rx_intentionallyDroppedPacketsPer100 INIT(0);	/* Dropped on Send */
EXT int rx_intentionallyDroppedOnReadPer100  INIT(0);	/* Dropped on Read */
#endif

/* extra packets to add to the quota */
EXT int rx_extraQuota INIT(0);
/* extra packets to alloc (2 windows by deflt) */
EXT int rx_extraPackets INIT(32);

EXT int rx_stackSize INIT(RX_DEFAULT_STACK_SIZE);

/* Time until an unresponsive connection is declared dead */
EXT int rx_connDeadTime INIT(12);
/* Set rx default connection dead time; set on both services and connections at creation time */
#define rx_SetRxDeadTime(seconds)   (rx_connDeadTime = (seconds))

/* Time until we toss an idle connection */
EXT int rx_idleConnectionTime INIT(700);
/* Time until we toss a peer structure, after all connections using are gone */
EXT int rx_idlePeerTime INIT(60);

/* The file server is temporarily salvaging */
EXT int rx_tranquil INIT(0);

/* UDP rcv buffer size */
EXT int rx_UdpBufSize INIT(64 * 1024);
#define rx_GetMinUdpBufSize()   (64*1024)
#define rx_SetUdpBufSize(x)     (((x)>rx_GetMinUdpBufSize()) ? (rx_UdpBufSize = (x)):0)

/*
 * Variables to control RX overload management. When the number of calls
 * waiting for a thread exceed the threshold, new calls are aborted
 * with the busy error. 
 */
EXT int rx_BusyThreshold INIT(-1);	/* default is disabled */
EXT int rx_BusyError INIT(-1);

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

EXT int rx_minWindow INIT(1);
EXT int rx_initReceiveWindow INIT(16);	/* how much to accept */
EXT int rx_maxReceiveWindow INIT(32);	/* how much to accept */
EXT int rx_initSendWindow INIT(8);
EXT int rx_maxSendWindow INIT(32);
EXT int rx_nackThreshold INIT(3);	/* Number NACKS to trigger congestion recovery */
EXT int rx_nDgramThreshold INIT(4);	/* Number of packets before increasing
					 * packets per datagram */
#define RX_MAX_FRAGS 4
EXT int rxi_nSendFrags INIT(RX_MAX_FRAGS);	/* max fragments in a datagram */
EXT int rxi_nRecvFrags INIT(RX_MAX_FRAGS);
EXT int rxi_OrphanFragSize INIT(512);

#define RX_MAX_DGRAM_PACKETS 6	/* max packets per jumbogram */

EXT int rxi_nDgramPackets INIT(RX_MAX_DGRAM_PACKETS);
/* allow n packets between soft acks - must be power of 2 -1, else change
 * macro below */
EXT int rxi_SoftAckRate INIT(RX_FAST_ACK_RATE);
/* consume n packets before sending hard ack, should be larger than above,
   but not absolutely necessary.  If it's smaller, than fast receivers will
   send a soft ack, immediately followed by a hard ack. */
EXT int rxi_HardAckRate INIT(RX_FAST_ACK_RATE + 1);

/* EXT int rx_maxWindow INIT(15);   Temporary HACK:  transmit/receive window */

/* If window sizes become very variable (in terms of #packets), be
 * sure that the sender can get back a hard acks without having to wait for
 * some kind of timer event first (like a keep-alive, for instance).
 * It might be kind of tricky, so it might be better to shrink the
 * window size by reducing the packet size below the "natural" MTU. */

#define	ACKHACK(p,r) { if (((p)->header.seq & (rxi_SoftAckRate))==0) (p)->header.flags |= RX_REQUEST_ACK; }

EXT int rx_nPackets INIT(100);	/* obsolete; use rx_extraPackets now */

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
        struct rx_queue queue;
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
        int alloc_ops;
        int alloc_xfer;
    } _FPQ;
    struct rx_packet * local_special_packet;
} rx_ts_info_t;
EXT struct rx_ts_info_t * rx_ts_info_init();   /* init function for thread-specific data struct */
#define RX_TS_INFO_GET(ts_info_p) \
    do { \
        ts_info_p = (struct rx_ts_info_t*)pthread_getspecific(rx_ts_info_key); \
        if (ts_info_p == NULL) { \
            assert((ts_info_p = rx_ts_info_init()) != NULL); \
        } \
    } while(0)
#endif /* AFS_PTHREAD_ENV */


/* List of free packets */
/* in pthreads rx, free packet queue is now a two-tiered queueing system
 * in which the first tier is thread-specific, and the second tier is
 * a global free packet queue */
EXT struct rx_queue rx_freePacketQueue;
#define RX_FPQ_MARK_FREE(p) \
    do { \
        if ((p)->flags & RX_PKTFLAG_FREE) \
            osi_Panic("rx packet already free\n"); \
        (p)->flags |= RX_PKTFLAG_FREE; \
    } while(0)
#define RX_FPQ_MARK_USED(p) \
    do { \
        if (!((p)->flags & RX_PKTFLAG_FREE)) \
            osi_Panic("rx packet not free\n"); \
        (p)->flags = 0;		/* clear RX_PKTFLAG_FREE, initialize the rest */ \
        (p)->header.flags = 0; \
    } while(0)
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

#if defined(AFS_PTHREAD_ENV)
#define RX_ENABLE_TSFPQ
EXT int rx_TSFPQGlobSize INIT(3); /* number of packets to transfer between global and local queues in one op */
EXT int rx_TSFPQLocalMax INIT(15); /* max number of packets on local FPQ before returning a glob to the global pool */
EXT int rx_TSFPQMaxProcs INIT(0); /* max number of threads expected */
EXT void rxi_MorePacketsTSFPQ(int apackets, int flush_global, int num_keep_local); /* more flexible packet alloc function */
EXT void rxi_AdjustLocalPacketsTSFPQ(int num_keep_local, int allow_overcommit); /* adjust thread-local queue length, for places where we know how many packets we will need a priori */
EXT void rxi_FlushLocalPacketsTSFPQ(void); /* flush all thread-local packets to global queue */
#define RX_TS_FPQ_FLUSH_GLOBAL 1
#define RX_TS_FPQ_PULL_GLOBAL 1
#define RX_TS_FPQ_ALLOW_OVERCOMMIT 1
/* compute the localmax and globsize values from rx_TSFPQMaxProcs and rx_nPackets.
   arbitarily set local max so that all threads consume 90% of packets, if all local queues are full.
   arbitarily set transfer glob size to 20% of max local packet queue length.
   also set minimum values of 15 and 3. */
#define RX_TS_FPQ_COMPUTE_LIMITS \
    do { \
        register int newmax, newglob; \
        newmax = (rx_nPackets * 9) / (10 * rx_TSFPQMaxProcs); \
        newmax = (newmax >= 15) ? newmax : 15; \
        newglob = newmax / 5; \
        newglob = (newglob >= 3) ? newglob : 3; \
        rx_TSFPQLocalMax = newmax; \
        rx_TSFPQGlobSize = newglob; \
    } while(0)
/* move packets from local (thread-specific) to global free packet queue.
   rx_freePktQ_lock must be held. default is to move the difference between the current lenght, and the 
   allowed max plus one extra glob. */
#define RX_TS_FPQ_LTOG(rx_ts_info_p) \
    do { \
        register int i; \
        register struct rx_packet * p; \
        register int tsize = (rx_ts_info_p)->_FPQ.len - rx_TSFPQLocalMax + rx_TSFPQGlobSize; \
        for (i=0,p=queue_Last(&((rx_ts_info_p)->_FPQ), rx_packet); \
             i < tsize; i++,p=queue_Prev(p, rx_packet)); \
        queue_SplitAfterPrepend(&((rx_ts_info_p)->_FPQ),&rx_freePacketQueue,p); \
        (rx_ts_info_p)->_FPQ.len -= tsize; \
        rx_nFreePackets += tsize; \
        (rx_ts_info_p)->_FPQ.ltog_ops++; \
        (rx_ts_info_p)->_FPQ.ltog_xfer += tsize; \
        if ((rx_ts_info_p)->_FPQ.delta) { \
            (rx_ts_info_p)->_FPQ.alloc_ops++; \
            (rx_ts_info_p)->_FPQ.alloc_xfer += (rx_ts_info_p)->_FPQ.delta; \
            MUTEX_ENTER(&rx_stats_mutex); \
            rx_nPackets += (rx_ts_info_p)->_FPQ.delta; \
            RX_TS_FPQ_COMPUTE_LIMITS; \
            MUTEX_EXIT(&rx_stats_mutex); \
           (rx_ts_info_p)->_FPQ.delta = 0; \
        } \
    } while(0)
/* same as above, except user has direct control over number to transfer */
#define RX_TS_FPQ_LTOG2(rx_ts_info_p,num_transfer) \
    do { \
        register int i; \
        register struct rx_packet * p; \
        for (i=0,p=queue_Last(&((rx_ts_info_p)->_FPQ), rx_packet); \
	     i < (num_transfer); i++,p=queue_Prev(p, rx_packet)); \
        queue_SplitAfterPrepend(&((rx_ts_info_p)->_FPQ),&rx_freePacketQueue,p); \
        (rx_ts_info_p)->_FPQ.len -= (num_transfer); \
        rx_nFreePackets += (num_transfer); \
        (rx_ts_info_p)->_FPQ.ltog_ops++; \
        (rx_ts_info_p)->_FPQ.ltog_xfer += (num_transfer); \
        if ((rx_ts_info_p)->_FPQ.delta) { \
            (rx_ts_info_p)->_FPQ.alloc_ops++; \
            (rx_ts_info_p)->_FPQ.alloc_xfer += (rx_ts_info_p)->_FPQ.delta; \
            MUTEX_ENTER(&rx_stats_mutex); \
            rx_nPackets += (rx_ts_info_p)->_FPQ.delta; \
            RX_TS_FPQ_COMPUTE_LIMITS; \
            MUTEX_EXIT(&rx_stats_mutex); \
            (rx_ts_info_p)->_FPQ.delta = 0; \
        } \
    } while(0)
/* move packets from global to local (thread-specific) free packet queue.
   rx_freePktQ_lock must be held. */
#define RX_TS_FPQ_GTOL(rx_ts_info_p) \
    do { \
        register int i, tsize; \
        register struct rx_packet * p; \
        tsize = (rx_TSFPQGlobSize <= rx_nFreePackets) ? \
                 rx_TSFPQGlobSize : rx_nFreePackets; \
        for (i=0,p=queue_First(&rx_freePacketQueue, rx_packet); \
             i < tsize; i++,p=queue_Next(p, rx_packet)); \
        queue_SplitBeforeAppend(&rx_freePacketQueue,&((rx_ts_info_p)->_FPQ),p); \
        (rx_ts_info_p)->_FPQ.len += i; \
        rx_nFreePackets -= i; \
        (rx_ts_info_p)->_FPQ.gtol_ops++; \
        (rx_ts_info_p)->_FPQ.gtol_xfer += i; \
    } while(0)
/* same as above, except user has direct control over number to transfer */
#define RX_TS_FPQ_GTOL2(rx_ts_info_p,num_transfer) \
    do { \
        register int i; \
        register struct rx_packet * p; \
        for (i=0,p=queue_First(&rx_freePacketQueue, rx_packet); \
             i < (num_transfer); i++,p=queue_Next(p, rx_packet)); \
        queue_SplitBeforeAppend(&rx_freePacketQueue,&((rx_ts_info_p)->_FPQ),p); \
        (rx_ts_info_p)->_FPQ.len += i; \
        rx_nFreePackets -= i; \
        (rx_ts_info_p)->_FPQ.gtol_ops++; \
        (rx_ts_info_p)->_FPQ.gtol_xfer += i; \
    } while(0)
/* checkout a packet from the thread-specific free packet queue */
#define RX_TS_FPQ_CHECKOUT(rx_ts_info_p,p) \
    do { \
        (p) = queue_First(&((rx_ts_info_p)->_FPQ), rx_packet); \
        queue_Remove(p); \
        RX_FPQ_MARK_USED(p); \
        (rx_ts_info_p)->_FPQ.len--; \
        (rx_ts_info_p)->_FPQ.checkout_ops++; \
        (rx_ts_info_p)->_FPQ.checkout_xfer++; \
    } while(0)
/* checkout multiple packets from the thread-specific free packet queue */
#define RX_TS_FPQ_CHECKOUT2(rx_ts_info_p,num_transfer,q) \
    do { \
        register int i; \
        register struct rx_packet *p; \
        for (i=0, p=queue_First(&((rx_ts_info_p)->_FPQ), rx_packet); \
             i < (num_transfer); \
             i++, p=queue_Next(p, rx_packet)) { \
            RX_FPQ_MARK_USED(p); \
        } \
        queue_SplitBeforeAppend(&((rx_ts_info_p)->_FPQ),(q),p); \
        (rx_ts_info_p)->_FPQ.len -= (num_transfer); \
        (rx_ts_info_p)->_FPQ.checkout_ops++; \
        (rx_ts_info_p)->_FPQ.checkout_xfer += (num_transfer); \
    } while(0)
/* check a packet into the thread-specific free packet queue */
#define RX_TS_FPQ_CHECKIN(rx_ts_info_p,p) \
    do { \
        queue_Prepend(&((rx_ts_info_p)->_FPQ), (p)); \
        RX_FPQ_MARK_FREE(p); \
        (rx_ts_info_p)->_FPQ.len++; \
        (rx_ts_info_p)->_FPQ.checkin_ops++; \
        (rx_ts_info_p)->_FPQ.checkin_xfer++; \
    } while(0)
/* check multiple packets into the thread-specific free packet queue */
/* num_transfer must equal length of (q); it is not a means of checking 
 * in part of (q).  passing num_transfer just saves us instructions 
 * since caller already knows length of (q) for other reasons */
#define RX_TS_FPQ_CHECKIN2(rx_ts_info_p,num_transfer,q) \
    do { \
        register struct rx_packet *p, *np; \
        for (queue_Scan((q), p, np, rx_packet)) { \
            RX_FPQ_MARK_FREE(p); \
        } \
        queue_SplicePrepend(&((rx_ts_info_p)->_FPQ),(q)); \
        (rx_ts_info_p)->_FPQ.len += (num_transfer); \
        (rx_ts_info_p)->_FPQ.checkin_ops++; \
        (rx_ts_info_p)->_FPQ.checkin_xfer += (num_transfer); \
    } while(0)
#endif /* AFS_PTHREAD_ENV */

/* Number of free packets */
EXT int rx_nFreePackets INIT(0);
EXT int rxi_NeedMorePackets INIT(0);
EXT int rx_nWaiting INIT(0);
EXT int rx_nWaited INIT(0);
EXT int rx_packetReclaims INIT(0);

/* largest packet which we can safely receive, initialized to AFS 3.2 value
 * This is provided for backward compatibility with peers which may be unable
 * to swallow anything larger. THIS MUST NEVER DECREASE WHILE AN APPLICATION
 * IS RUNNING! */
EXT afs_uint32 rx_maxReceiveSize INIT(OLD_MAX_PACKET_SIZE * RX_MAX_FRAGS +
				      UDP_HDR_SIZE * (RX_MAX_FRAGS - 1));

/* this is the maximum packet size that the user wants us to receive */
/* this is set by rxTune if required */
EXT afs_uint32 rx_maxReceiveSizeUser INIT(0xffffffff);

/* rx_MyMaxSendSize is the size of the largest packet we will send,
 * including the RX header. Just as rx_maxReceiveSize is the
 * max we will receive, including the rx header.
 */
EXT afs_uint32 rx_MyMaxSendSize INIT(8588);

/* Maximum size of a jumbo datagram we can receive */
EXT afs_uint32 rx_maxJumboRecvSize INIT(RX_MAX_PACKET_SIZE);

/* need this to permit progs to run on AIX systems */
EXT int (*rxi_syscallp) (afs_uint32 a3, afs_uint32 a4, void *a5)INIT(0);

/* List of free queue entries */
EXT struct rx_serverQueueEntry *rx_FreeSQEList INIT(0);
#ifdef	RX_ENABLE_LOCKS
EXT afs_kmutex_t freeSQEList_lock;
#endif

/* List of free call structures */
EXT struct rx_queue rx_freeCallQueue;
#ifdef	RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_freeCallQueue_lock;
#endif
EXT afs_int32 rxi_nCalls INIT(0);

/* Port requested at rx_Init.  If this is zero, the actual port used will be different--but it will only be used for client operations.  If non-zero, server provided services may use the same port. */
EXT u_short rx_port;

#if !defined(KERNEL) && !defined(AFS_PTHREAD_ENV)
/* 32-bit select Mask for rx_Listener. */
EXT fd_set rx_selectMask;
EXT osi_socket rx_maxSocketNumber;	/* Maximum socket number in the select mask. */
/* Minumum socket number in the select mask. */
EXT osi_socket rx_minSocketNumber INIT(0x7fffffff);
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
EXT int rx_packetQuota[RX_N_PACKET_CLASSES] INIT(RX_PACKET_QUOTAS);
EXT int meltdown_1pkt INIT(1);	/* prefer to schedule single-packet calls */
EXT int rxi_doreclaim INIT(1);	/* if discard one packet, discard all */
EXT int rxi_md2cnt INIT(0);	/* counter of skipped calls */
EXT int rxi_2dchoice INIT(1);	/* keep track of another call to schedule */

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
EXT int rxi_dataQuota INIT(RX_MAX_QUOTA);	/* packets to reserve for active threads */

EXT afs_int32 rxi_availProcs INIT(0);	/* number of threads in the pool */
EXT afs_int32 rxi_totalMin INIT(0);	/* Sum(minProcs) forall services */
EXT afs_int32 rxi_minDeficit INIT(0);	/* number of procs needed to handle all minProcs */

EXT int rx_nextCid;		/* Next connection call id */
EXT int rx_epoch;		/* Initialization time of rx */
#ifdef	RX_ENABLE_LOCKS
EXT afs_kcondvar_t rx_waitingForPackets_cv;
#endif
EXT char rx_waitingForPackets;	/* Processes set and wait on this variable when waiting for packet buffers */

EXT struct rx_stats rx_stats;

EXT struct rx_peer **rx_peerHashTable;
EXT struct rx_connection **rx_connHashTable;
EXT struct rx_connection *rx_connCleanup_list INIT(0);
EXT afs_uint32 rx_hashTableSize INIT(257);	/* Prime number */
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_peerHashTable_lock;
EXT afs_kmutex_t rx_connHashTable_lock;
#endif /* RX_ENABLE_LOCKS */

#define CONN_HASH(host, port, cid, epoch, type) ((((cid)>>RX_CIDSHIFT)%rx_hashTableSize))

#define PEER_HASH(host, port)  ((host ^ port) % rx_hashTableSize)

/* Forward definitions of internal procedures */
#define	rxi_ChallengeOff(conn)	rxevent_Cancel((conn)->challengeEvent, (struct rx_call*)0, 0);
#define rxi_KeepAliveOff(call) rxevent_Cancel((call)->keepAliveEvent, call, RX_CALL_REFCOUNT_ALIVE)

#define rxi_AllocSecurityObject() (struct rx_securityClass *) rxi_Alloc(sizeof(struct rx_securityClass))
#define	rxi_FreeSecurityObject(obj) rxi_Free(obj, sizeof(struct rx_securityClass))
#define	rxi_AllocService()	(struct rx_service *) rxi_Alloc(sizeof(struct rx_service))
#define	rxi_FreeService(obj)	rxi_Free(obj, sizeof(struct rx_service))
#define	rxi_AllocPeer()		(struct rx_peer *) rxi_Alloc(sizeof(struct rx_peer))
#define	rxi_FreePeer(peer)	rxi_Free(peer, sizeof(struct rx_peer))
#define	rxi_AllocConnection()	(struct rx_connection *) rxi_Alloc(sizeof(struct rx_connection))
#define rxi_FreeConnection(conn) (rxi_Free(conn, sizeof(struct rx_connection)))

#ifdef RXDEBUG
/* Some debugging stuff */
EXT FILE *rx_debugFile;		/* Set by the user to a stdio file for debugging output */
EXT FILE *rxevent_debugFile;	/* Set to an stdio descriptor for event logging to that file */

#define rx_Log rx_debugFile
#ifdef AFS_NT40_ENV
EXT int rxdebug_active;
#if !defined(_WIN64)
#define dpf(args) if (rxdebug_active) rxi_DebugPrint args;
#else
#define dpf(args)
#endif
#else
#define dpf(args) if (rx_debugFile) rxi_DebugPrint args; else
#endif
#define rx_Log_event rxevent_debugFile

EXT char *rx_packetTypes[RX_N_PACKET_TYPES] INIT(RX_PACKET_TYPES);	/* Strings defined in rx.h */

#ifndef KERNEL
/*
 * Counter used to implement connection specific data
 */
EXT int rxi_keyCreate_counter INIT(0);
/*
 * Array of function pointers used to destory connection specific data
 */
EXT rx_destructor_t *rxi_keyCreate_destructor INIT(NULL);
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rxi_keyCreate_lock;
#endif /* RX_ENABLE_LOCKS */
#endif /* !KERNEL */

#else
#define dpf(args)
#endif /* RXDEBUG */

/*
 * SERVER ONLY: Threshholds used to throttle error replies to looping
 * clients. When consecutive calls are aborting with the same error, the
 * server throttles the client by waiting before sending error messages.
 * Disabled if abort thresholds are zero.
 */
EXT int rxi_connAbortThreshhold INIT(0);
EXT int rxi_connAbortDelay INIT(3000);
EXT int rxi_callAbortThreshhold INIT(0);
EXT int rxi_callAbortDelay INIT(3000);

/*
 * Thread specific thread ID used to implement LWP_Index().
 */

#if defined(AFS_PTHREAD_ENV)
EXT int rxi_fcfs_thread_num INIT(0);
EXT pthread_key_t rx_thread_id_key;
/* keep track of pthread numbers - protected by rx_stats_mutex, 
   except in rx_Init() before mutex exists! */
EXT int rxi_pthread_hinum INIT(0);
#else
#define rxi_fcfs_thread_num (0)
#endif

#if defined(RX_ENABLE_LOCKS)
EXT afs_kmutex_t rx_stats_mutex;	/* used to activate stats gathering */
#endif

EXT int rx_enable_stats INIT(0);

/*
 * Set this flag to enable the listener thread to trade places with an idle
 * worker thread to move the context switch from listener to worker out of
 * the request path.
 */
EXT int rx_enable_hot_thread INIT(0);

#endif /* AFS_RX_GLOBALS_H */
