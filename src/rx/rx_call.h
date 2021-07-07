/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_RX_CALL_H
#define OPENAFS_RX_CALL_H 1

/*
 * The following fields are accessed while the call is unlocked.
 * These fields are used by the application thread to marshall
 * and unmarshall RPC data. The only time they may be changed by
 * other threads is when the RX_CALL_IOVEC_WAIT flag is set
 *
 * NOTE: Ensure that this structure is padded to a double word boundary
 * to avoid problems with other threads accessing items stored beside it
 * in the call structure
 */
struct rx_call_appl {
    struct opr_queue iovq;	/* readv/writev packet queue */
    u_short nLeft;		/* Number bytes left in first receive packet */
    u_short curvec;		/* current iovec in currentPacket */
    u_short curlen;		/* bytes remaining in curvec */
    u_short nFree;		/* Number bytes free in last send packet */
    struct rx_packet *currentPacket;	/* Current packet being assembled or being read */
    char *curpos;		/* current position in curvec */
    int mode;			/* Mode of call */
    int padding;		/* Pad to double word */
    afs_uint64 bytesSent;	/* Number bytes sent */
    afs_uint64 bytesRcvd;	/* Number bytes received */
};

/* Call structure:  only instantiated for active calls and dallying
 * server calls.  The permanent call state (i.e. the call number as
 * well as state shared with other calls associated with this
 * connection) is maintained in the connection structure. */

struct rx_call {
    struct opr_queue entry;	/* Call can be on various queues (one-at-a-time) */
    struct opr_queue tq;	/* Transmit packet queue */
    struct opr_queue rq;	/* Receive packet queue */
    struct rx_call_appl app;	/* Data private to the application thread */
    u_char channel;		/* Index of call, within connection */
    u_char state;		/* Current call state as defined in rx.h */
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t lock;		/* lock covers data as well as mutexes. */
    afs_kmutex_t *call_queue_lock;	/* points to lock for queue we're on,
					 * if any. */
    afs_kcondvar_t cv_twind;
    afs_kcondvar_t cv_rq;
    afs_kcondvar_t cv_tq;
#endif
    struct rx_connection *conn;	/* Parent connection for this call */
    afs_uint32 *callNumber;	/* Pointer to call number field within connection */
    afs_uint32 flags;		/* Some random flags */
    u_char localStatus;		/* Local user status sent out of band */
    u_char remoteStatus;	/* Remote user status received out of band */
    afs_int32 error;		/* Error condition for this call */
    afs_uint32 timeout;		/* High level timeout for this call */
    afs_uint32 rnext;		/* Next sequence number expected to be read by rx_ReadData */
    afs_uint32 rprev;		/* Previous packet received; used for deciding what the next packet to be received should be, in order to decide whether a negative acknowledge should be sent */
    afs_uint32 rwind;		/* The receive window:  the peer must not send packets with sequence numbers >= rnext+rwind */
    afs_uint32 tfirst;		/* First unacknowledged transmit packet number */
    afs_uint32 tnext;		/* Next transmit sequence number to use */
    afs_uint32 tprev;		/* Last packet that we saw an ack for */
    u_short twind;		/* The transmit window:  we cannot assign a sequence number to a packet >= tfirst + twind */
    u_short cwind;		/* The congestion window */
    u_short nSoftAcked;		/* Number soft acked transmit packets */
    u_short nextCwind;		/* The congestion window after recovery */
    u_short nCwindAcks;		/* Number acks received at current cwind */
    u_short ssthresh;		/* The slow start threshold */
    u_short nDgramPackets;	/* Packets per AFS 3.5 jumbogram */
    u_short nAcks;		/* The number of consecutive acks */
    u_short nNacks;		/* Number packets acked that follow the
				 * first negatively acked packet */
    u_short nSoftAcks;		/* The number of delayed soft acks */
    u_short nHardAcks;		/* The number of delayed hard acks */
    u_short congestSeq;		/* Peer's congestion sequence counter */
    int rtt;
    int rtt_dev;
    struct clock rto;		/* The round trip timeout calculated for this call */
    struct rxevent *resendEvent;	/* If this is non-Null, there is a retransmission event pending */
    struct rxevent *keepAliveEvent;	/* Scheduled periodically in active calls to keep call alive */
    struct rxevent *growMTUEvent;      /* Scheduled periodically in active calls to discover true maximum MTU */
    struct rxevent *delayedAckEvent;	/* Scheduled after all packets are received to send an ack if a reply or new call is not generated soon */
    struct clock delayedAckTime;        /* Time that next delayed ack was scheduled  for */
    struct rxevent *delayedAbortEvent;	/* Scheduled to throttle looping client */
    int abortCode;		/* error code from last RPC */
    int abortCount;		/* number of times last error was sent */
    u_int lastSendTime;		/* Last time a packet was sent on this call */
    u_int lastReceiveTime;	/* Last time a packet was received for this call */
    void (*arrivalProc) (struct rx_call * call, void * mh, int index);	/* Procedure to call when reply is received */
    void *arrivalProcHandle;	/* Handle to pass to replyFunc */
    int arrivalProcArg;         /* Additional arg to pass to reply Proc */
    afs_uint32 lastAcked;	/* last packet "hard" acked by receiver */
    afs_uint32 startWait;	/* time server began waiting for input data/send quota */
    struct clock traceWait;	/* time server began waiting for input data/send quota */
    struct clock traceStart;	/* time the call started running */
    u_short MTU;		/* size of packets currently sending */
#ifdef RX_ENABLE_LOCKS
    short refCount;		/* Used to keep calls from disappearring
				 * when we get them from a queue. (rx_refcnt_lock) */
#endif                          /* RX_ENABLE_LOCKS */
/* Call refcount modifiers */
#define RX_CALL_REFCOUNT_BEGIN  0	/* GetCall/NewCall/EndCall */
#define RX_CALL_REFCOUNT_RESEND 1	/* resend event */
#define RX_CALL_REFCOUNT_DELAY  2	/* delayed ack */
#define RX_CALL_REFCOUNT_ALIVE  3	/* keep alive event */
#define RX_CALL_REFCOUNT_PACKET 4	/* waiting for packets. */
#define RX_CALL_REFCOUNT_SEND   5	/* rxi_Send */
#define RX_CALL_REFCOUNT_ABORT  7	/* delayed abort */
#define RX_CALL_REFCOUNT_MTU    8       /* grow mtu event */
#define RX_CALL_REFCOUNT_MAX    9	/* array size. */
#ifdef RX_REFCOUNT_CHECK
    short refCDebug[RX_CALL_REFCOUNT_MAX];
#endif				/* RX_REFCOUNT_CHECK */

    /*
     * iov, iovNBytes, iovMax, and iovNext are set in rxi_ReadvProc()
     * and adjusted by rxi_FillReadVec().  iov does not own the buffers
     * it refers to.  The buffers belong to the packets stored in iovq.
     * Only one call to rx_ReadvProc() can be active at a time.
     */

    int iovNBytes;		/* byte count for current iovec */
    int iovMax;			/* number elements in current iovec */
    int iovNext;		/* next entry in current iovec */
    struct iovec *iov;		/* current iovec */

    struct clock queueTime;	/* time call was queued */
    struct clock startTime;	/* time call was started */

    u_short tqWaiters;

    struct rx_packet *xmitList[RX_MAXACKS]; /* Can't xmit more than we ack */
                                /* Protected by setting RX_CALL_TQ_BUSY */
#ifdef RXDEBUG_PACKET
    u_short tqc;                /* packet count in tq */
    u_short rqc;                /* packet count in rq */
    u_short iovqc;              /* packet count in iovq */

    struct rx_call *allNextp;
    afs_uint32 call_id;
#endif
#ifdef AFS_RXERRQ_ENV
    int neterr_gen;
#endif
};

#ifdef RX_ENABLE_LOCKS

# define CALL_HOLD(call, type) do { \
				MUTEX_ENTER(&rx_refcnt_mutex); \
				CALL_HOLD_R(call, type); \
				MUTEX_EXIT(&rx_refcnt_mutex); \
			      } while(0)
# define CALL_RELE(call, type) do { \
				MUTEX_ENTER(&rx_refcnt_mutex); \
				CALL_RELE_R(call, type); \
				MUTEX_EXIT(&rx_refcnt_mutex); \
			      } while(0)

# ifdef RX_REFCOUNT_CHECK
/* RX_REFCOUNT_CHECK is used to test for call refcount leaks by event
 * type.
 */
extern int rx_callHoldType;
#  define CALL_HOLD_R(call, type) do { \
				 call->refCount++; \
				 call->refCDebug[type]++; \
				 if (call->refCDebug[type] > 50)  {\
				     rx_callHoldType = type; \
				     osi_Panic("Huge call refCount"); \
							       } \
			     } while (0)
#  define CALL_RELE_R(call, type) do { \
				 call->refCount--; \
				 call->refCDebug[type]--; \
				 if (call->refCDebug[type] > 50) {\
				     rx_callHoldType = type; \
				     osi_Panic("Negative call refCount"); \
							      } \
			     } while (0)
# else /* RX_REFCOUNT_CHECK */
#  define CALL_HOLD_R(call, type) 	 call->refCount++
#  define CALL_RELE_R(call, type)	 call->refCount--
# endif /* RX_REFCOUNT_CHECK */

#else /* RX_ENABLE_LOCKS */
# define CALL_HOLD(call, type)
# define CALL_RELE(call, type)
# define CALL_RELE_R(call, type)
#endif /* RX_ENABLE_LOCKS */

#endif
