/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX:  Globals for internal use, basically */

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
#endif

/* Basic socket for client requests; other sockets (for receiving server requests) are in the service structures */
EXT osi_socket rx_socket;

/* The array of installed services.  Null terminated. */
EXT struct rx_service *rx_services[RX_MAX_SERVICES+1];
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
#endif

/* extra packets to add to the quota */
EXT int rx_extraQuota INIT(0);
/* extra packets to alloc (2 windows by deflt) */
EXT int rx_extraPackets INIT(32);

EXT int rx_stackSize INIT(RX_DEFAULT_STACK_SIZE);

/* Time until an unresponsive connection is declared dead */
EXT int	rx_connDeadTime	INIT(12);
/* Set rx default connection dead time; set on both services and connections at creation time */
#define rx_SetRxDeadTime(seconds)   (rx_connDeadTime = (seconds))

/* Time until we toss an idle connection */
EXT int rx_idleConnectionTime INIT(700);
/* Time until we toss a peer structure, after all connections using are gone */
EXT int	rx_idlePeerTime	INIT(60);

/* The file server is temporarily salvaging */
EXT int	rx_tranquil	INIT(0);

/* UDP rcv buffer size */
EXT int rx_UdpBufSize   INIT(64*1024);
#define rx_GetMinUdpBufSize()   (64*1024)
#define rx_SetUdpBufSize(x)     (((x)>rx_GetMinUdpBufSize()) ? (rx_UdpBufSize = (x)):0)

/*
 * Variables to control RX overload management. When the number of calls
 * waiting for a thread exceed the threshold, new calls are aborted
 * with the busy error. 
 */
EXT int rx_BusyThreshold INIT(-1);      /* default is disabled */
EXT int rx_BusyError INIT(-1);

/* These definitions should be in one place */
#ifdef	AFS_SUN5_ENV
#define	RX_CBUF_TIME	180	/* Check for packet deficit */
#define	RX_REAP_TIME	90	    /* Check for tossable connections every 90 seconds */
#else
#define	RX_CBUF_TIME	120	/* Check for packet deficit */
#define	RX_REAP_TIME	60	    /* Check for tossable connections every 60 seconds */
#endif

#define RX_FAST_ACK_RATE 1      /* as of 3.4, ask for an ack every 
				   other packet. */

EXT int rx_minWindow INIT(1); 
EXT int rx_initReceiveWindow INIT(16); /* how much to accept */
EXT int rx_maxReceiveWindow INIT(32);  /* how much to accept */
EXT int rx_initSendWindow INIT(8); 
EXT int rx_maxSendWindow INIT(32); 
EXT int rx_nackThreshold INIT(3);      /* Number NACKS to trigger congestion recovery */
EXT int rx_nDgramThreshold INIT(4);    /* Number of packets before increasing
					  packets per datagram */
#define RX_MAX_FRAGS 4
EXT int rxi_nSendFrags INIT(RX_MAX_FRAGS);  /* max fragments in a datagram */
EXT int rxi_nRecvFrags INIT(RX_MAX_FRAGS);
EXT int rxi_OrphanFragSize INIT(512);

#define RX_MAX_DGRAM_PACKETS 6 /* max packets per jumbogram */

EXT int rxi_nDgramPackets INIT(RX_MAX_DGRAM_PACKETS);
/* allow n packets between soft acks - must be power of 2 -1, else change
 * macro below */
EXT int rxi_SoftAckRate INIT(RX_FAST_ACK_RATE);  
/* consume n packets before sending hard ack, should be larger than above,
   but not absolutely necessary.  If it's smaller, than fast receivers will
   send a soft ack, immediately followed by a hard ack. */
EXT int rxi_HardAckRate INIT(RX_FAST_ACK_RATE+1);

/* EXT int rx_maxWindow INIT(15);   Temporary HACK:  transmit/receive window */

/* If window sizes become very variable (in terms of #packets), be
 * sure that the sender can get back a hard acks without having to wait for
 * some kind of timer event first (like a keep-alive, for instance).
 * It might be kind of tricky, so it might be better to shrink the
 * window size by reducing the packet size below the "natural" MTU. */

#define	ACKHACK(p,r) { if (((p)->header.seq & (rxi_SoftAckRate))==0) (p)->header.flags |= RX_REQUEST_ACK; } 

EXT int rx_nPackets INIT(100);	/* obsolete; use rx_extraPackets now */

/* List of free packets */
EXT struct rx_queue rx_freePacketQueue;
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_freePktQ_lock;
#endif

/* Number of free packets */
EXT int rx_nFreePackets INIT(0);
EXT int rxi_NeedMorePackets INIT(0);
EXT int rx_nWaiting INIT(0);
EXT int rx_packetReclaims INIT(0);

/* largest packet which we can safely receive, initialized to AFS 3.2 value
 * This is provided for backward compatibility with peers which may be unable
 * to swallow anything larger. THIS MUST NEVER DECREASE WHILE AN APPLICATION
 * IS RUNNING! */
EXT afs_uint32 rx_maxReceiveSize INIT(OLD_MAX_PACKET_SIZE*RX_MAX_FRAGS + UDP_HDR_SIZE*(RX_MAX_FRAGS-1));

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
EXT int (*rxi_syscallp) (afs_uint32 a3, afs_uint32 a4, void *a5) INIT(0); 

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
EXT int rx_maxSocketNumber; /* Maximum socket number in the select mask. */
/* Minumum socket number in the select mask. */
EXT int rx_minSocketNumber INIT(0x7fffffff);
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
EXT int meltdown_1pkt INIT(1); /* prefer to schedule single-packet calls */
EXT int rxi_doreclaim INIT(1);    /* if discard one packet, discard all */
EXT int rxi_md2cnt INIT(0);    /* counter of skipped calls */
EXT int rxi_2dchoice INIT(1);    /* keep track of another call to schedule */

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
EXT int rxi_dataQuota INIT(RX_MAX_QUOTA); /* packets to reserve for active threads */

EXT afs_int32 rxi_availProcs INIT(0);	/* number of threads in the pool */
EXT afs_int32 rxi_totalMin INIT(0);		/* Sum(minProcs) forall services */
EXT afs_int32 rxi_minDeficit INIT(0);	/* number of procs needed to handle all minProcs */

EXT int rx_nextCid;	    /* Next connection call id */
EXT int rx_epoch;	    /* Initialization time of rx */
#ifdef	RX_ENABLE_LOCKS
EXT afs_kcondvar_t rx_waitingForPackets_cv;
#endif
EXT char rx_waitingForPackets; /* Processes set and wait on this variable when waiting for packet buffers */

EXT struct rx_stats rx_stats;

EXT struct rx_peer **rx_peerHashTable;
EXT struct rx_connection **rx_connHashTable;
EXT struct rx_connection *rx_connCleanup_list INIT(0);
EXT afs_uint32 rx_hashTableSize INIT(256);	/* Power of 2 */
EXT afs_uint32 rx_hashTableMask INIT(255);	/* One less than rx_hashTableSize */
#ifdef RX_ENABLE_LOCKS
EXT afs_kmutex_t rx_peerHashTable_lock;
EXT afs_kmutex_t rx_connHashTable_lock;
#endif /* RX_ENABLE_LOCKS */

#define CONN_HASH(host, port, cid, epoch, type) ((((cid)>>RX_CIDSHIFT)&rx_hashTableMask))

#define PEER_HASH(host, port)  ((host ^ port) & rx_hashTableMask)

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
EXT FILE *rx_debugFile;	/* Set by the user to a stdio file for debugging output */
EXT FILE *rxevent_debugFile;	/* Set to an stdio descriptor for event logging to that file */

#define rx_Log rx_debugFile
#define dpf(args) if (rx_debugFile) rxi_DebugPrint args; else
#define rx_Log_event rxevent_debugFile

EXT char *rx_packetTypes[RX_N_PACKET_TYPES] INIT(RX_PACKET_TYPES); /* Strings defined in rx.h */

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
EXT pthread_key_t rx_thread_id_key;
/* keep track of pthread numbers - protected by rx_stats_mutex, 
   except in rx_Init() before mutex exists! */
EXT int rxi_pthread_hinum INIT(0);
#endif

#if defined(RX_ENABLE_LOCKS)
EXT afs_kmutex_t rx_stats_mutex; /* used to activate stats gathering */
#endif

EXT int rx_enable_stats INIT(0);

/*
 * Set this flag to enable the listener thread to trade places with an idle
 * worker thread to move the context switch from listener to worker out of
 * the request path.
 */
EXT int rx_enable_hot_thread INIT(0);
