/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifdef KDUMP_RX_LOCK
/* kdump for SGI needs MP and SP versions of rx_serverQueueEntry,
 * rx_peer, rx_connection and rx_call structs. rx.h gets included a
 * second time to pick up mp_ versions of those structs. Currently
 * the affected struct's have #ifdef's in them for the second pass.
 * This should change once we start using only ANSI compilers.
 * Actually, kdump does not use rx_serverQueueEntry, but I'm including
 * it for completeness.
 */
#undef _RX_
#endif

#ifndef	_RX_
#define _RX_

#ifndef KDUMP_RX_LOCK
/* Substitute VOID (char) for void, because some compilers are confused by void
 * in some situations */
#ifndef AFS_NT40_ENV
#define	VOID	char
#endif

#ifdef	KERNEL
#include "rx_kmutex.h"
#include "rx_kernel.h"
#include "rx_clock.h"
#include "rx_event.h"
#include "rx_queue.h"
#include "rx_packet.h"
#include "rx_misc.h"
#include "rx_multi.h"
#if defined (AFS_OBSD_ENV) && !defined (MLEN)
#include "sys/mbuf.h"
#endif
#include "netinet/in.h"
#include "sys/socket.h"
#else /* KERNEL */
# include <sys/types.h>
# include <stdio.h>
# include <string.h>
#ifdef AFS_PTHREAD_ENV
# include "rx_pthread.h"
#else
# include "rx_lwp.h"
#endif
#ifdef AFS_NT40_ENV
#include <malloc.h>
#endif
# include "rx_user.h"
# include "rx_clock.h"
# include "rx_event.h"
# include "rx_packet.h"
# include "rx_misc.h"
# include "rx_null.h"
# include "rx_multi.h"
#ifndef AFS_NT40_ENV
# include <netinet/in.h>
# include <sys/socket.h>
#endif
#endif /* KERNEL */


/* Configurable parameters */
#define	RX_IDLE_DEAD_TIME	60	/* default idle dead time */
#define	RX_MAX_SERVICES		20	/* Maximum number of services that may be installed */
#if defined(KERNEL) && defined(AFS_AIX51_ENV) && defined(__64__)
#define RX_DEFAULT_STACK_SIZE   24000
#else
#define	RX_DEFAULT_STACK_SIZE	16000	/* Default process stack size; overriden by rx_SetStackSize */
#endif

/* This parameter should not normally be changed */
#define	RX_PROCESS_PRIORITY	LWP_NORMAL_PRIORITY

/* backoff is fixed point binary.  Ie, units of 1/4 seconds */
#define MAXBACKOFF 0x1F

#define ADDRSPERSITE 16

#ifndef KDUMP_RX_LOCK
/* Bottom n-bits of the Call Identifier give the call number */
#define	RX_MAXCALLS 4		/* Power of 2; max async calls per connection */
#define	RX_CIDSHIFT 2		/* Log2(RX_MAXCALLS) */
#define	RX_CHANNELMASK (RX_MAXCALLS-1)
#define	RX_CIDMASK  (~RX_CHANNELMASK)
#endif /* !KDUMP_RX_LOCK */

#ifndef KERNEL
typedef void (*rx_destructor_t) (void *);
int rx_KeyCreate(rx_destructor_t);
osi_socket rxi_GetHostUDPSocket(u_int host, u_short port);
osi_socket rxi_GetUDPSocket(u_short port);
#endif /* KERNEL */


int ntoh_syserr_conv(int error);

#define	RX_WAIT	    1
#define	RX_DONTWAIT 0

#define	rx_ConnectionOf(call)		((call)->conn)
#define	rx_PeerOf(conn)			((conn)->peer)
#define	rx_HostOf(peer)			((peer)->host)
#define	rx_PortOf(peer)			((peer)->port)
#define	rx_SetLocalStatus(call, status)	((call)->localStatus = (status))
#define rx_GetLocalStatus(call, status) ((call)->localStatus)
#define	rx_GetRemoteStatus(call)	((call)->remoteStatus)
#define	rx_Error(call)			((call)->error)
#define	rx_ConnError(conn)		((conn)->error)
#define	rx_IsServerConn(conn)		((conn)->type == RX_SERVER_CONNECTION)
#define	rx_IsClientConn(conn)		((conn)->type == RX_CLIENT_CONNECTION)
/* Don't use these; use the IsServerConn style */
#define	rx_ServerConn(conn)		((conn)->type == RX_SERVER_CONNECTION)
#define	rx_ClientConn(conn)		((conn)->type == RX_CLIENT_CONNECTION)
#define rx_IsUsingPktCksum(conn)	((conn)->flags & RX_CONN_USING_PACKET_CKSUM)
#define rx_ServiceIdOf(conn)		((conn)->serviceId)
#define	rx_SecurityClassOf(conn)	((conn)->securityIndex)
#define rx_SecurityObjectOf(conn)	((conn)->securityObject)

/*******************
 * Macros callable by the user to further define attributes of a
 * service.  Must be called before rx_StartServer
 */

/* Set the service stack size.  This currently just sets the stack
 * size for all processes to be the maximum seen, so far */
#define rx_SetStackSize(service, stackSize) \
  rx_stackSize = (((stackSize) > rx_stackSize)? stackSize: rx_stackSize)

/* Set minimum number of processes guaranteed to be available for this
 * service at all times */
#define rx_SetMinProcs(service, min) ((service)->minProcs = (min))

/* Set maximum number of processes that will be made available to this
 * service (also a guarantee that this number will be made available
 * if there is no competition) */
#define rx_SetMaxProcs(service, max) ((service)->maxProcs = (max))

/* Define a procedure to be called just before a server connection is destroyed */
#define rx_SetDestroyConnProc(service,proc) ((service)->destroyConnProc = (proc))

/* Define procedure to set service dead time */
#define rx_SetIdleDeadTime(service,time) ((service)->idleDeadTime = (time))

/* Define procedures for getting and setting before and after execute-request procs */
#define rx_SetAfterProc(service,proc) ((service)->afterProc = (proc))
#define rx_SetBeforeProc(service,proc) ((service)->beforeProc = (proc))
#define rx_GetAfterProc(service) ((service)->afterProc)
#define rx_GetBeforeProc(service) ((service)->beforeProc)

/* Define a procedure to be called when a server connection is created */
#define rx_SetNewConnProc(service, proc) ((service)->newConnProc = (proc))

/* NOTE:  We'll probably redefine the following three routines, again, sometime. */

/* Set the connection dead time for any connections created for this service (server only) */
#define rx_SetServiceDeadTime(service, seconds) ((service)->secondsUntilDead = (seconds))

/* Enable or disable asymmetric client checking for a service */
#define rx_SetCheckReach(service, x) ((service)->checkReach = (x))

/* Set connection hard and idle timeouts for a connection */
#define rx_SetConnHardDeadTime(conn, seconds) ((conn)->hardDeadTime = (seconds))
#define rx_SetConnIdleDeadTime(conn, seconds) ((conn)->idleDeadTime = (seconds))

/* Set the overload threshold and the overload error */
#define rx_SetBusyThreshold(threshold, code) (rx_BusyThreshold=(threshold),rx_BusyError=(code))

/* If this flag is set,no new requests are processed by rx, all new requests are
returned with an error code of RX_CALL_DEAD ( transient error ) */
#define	rx_SetRxTranquil()   		(rx_tranquil = 1)
#define	rx_ClearRxTranquil()   		(rx_tranquil = 0)

/* Set the threshold and time to delay aborts for consecutive errors */
#define rx_SetCallAbortThreshold(A) (rxi_callAbortThreshhold = (A))
#define rx_SetCallAbortDelay(A) (rxi_callAbortDelay = (A))
#define rx_SetConnAbortThreshold(A) (rxi_connAbortThreshhold = (A))
#define rx_SetConnAbortDelay(A) (rxi_connAbortDelay = (A))

#define rx_GetCallAbortCode(call) ((call)->abortCode)
#define rx_SetCallAbortCode(call, code) ((call)->abortCode = (code))

#define cpspace(call) ((call)->curlen)
#define cppos(call) ((call)->curpos)

#define	rx_Read(call, buf, nbytes)   rx_ReadProc(call, buf, nbytes)
#define	rx_Read32(call, value)   rx_ReadProc32(call, value)
#define	rx_Readv(call, iov, nio, maxio, nbytes) \
   rx_ReadvProc(call, iov, nio, maxio, nbytes)
#define	rx_Write(call, buf, nbytes) rx_WriteProc(call, buf, nbytes)
#define	rx_Write32(call, value) rx_WriteProc32(call, value)
#define	rx_Writev(call, iov, nio, nbytes) \
   rx_WritevProc(call, iov, nio, nbytes)

/* This is the maximum size data packet that can be sent on this connection, accounting for security module-specific overheads. */
#define	rx_MaxUserDataSize(call)		((call)->MTU - RX_HEADER_SIZE - (call)->conn->securityHeaderSize - (call)->conn->securityMaxTrailerSize)

/* Macros to turn the hot thread feature on and off. Enabling hot threads
 * allows the listener thread to trade places with an idle worker thread,
 * which moves the context switch from listener to worker out of the
 * critical path.
 */
#define rx_EnableHotThread()		(rx_enable_hot_thread = 1)
#define rx_DisableHotThread()		(rx_enable_hot_thread = 0)

#define rx_PutConnection(conn) rx_DestroyConnection(conn)

/* A connection is an authenticated communication path, allowing 
   limited multiple asynchronous conversations. */
#ifdef KDUMP_RX_LOCK
struct rx_connection_rx_lock {
    struct rx_connection_rx_lock *next;	/*  on hash chain _or_ free list */
    struct rx_peer_rx_lock *peer;
#else
struct rx_connection {
    struct rx_connection *next;	/*  on hash chain _or_ free list */
    struct rx_peer *peer;
#endif
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t conn_call_lock;	/* locks conn_call_cv */
    afs_kcondvar_t conn_call_cv;
    afs_kmutex_t conn_data_lock;	/* locks packet data */
#endif
    afs_uint32 epoch;		/* Process start time of client side of connection */
    afs_uint32 cid;		/* Connection id (call channel is bottom bits) */
    afs_int32 error;		/* If this connection is in error, this is it */
#ifdef KDUMP_RX_LOCK
    struct rx_call_rx_lock *call[RX_MAXCALLS];
#else
    struct rx_call *call[RX_MAXCALLS];
#endif
    afs_uint32 callNumber[RX_MAXCALLS];	/* Current call numbers */
    afs_uint32 serial;		/* Next outgoing packet serial number */
    afs_uint32 lastSerial;	/* # of last packet received, for computing skew */
    afs_int32 maxSerial;	/* largest serial number seen on incoming packets */
/*    afs_int32 maxPacketSize;    max packet size should be per-connection since */
    /* peer process could be restarted on us. Includes RX Header.       */
    struct rxevent *challengeEvent;	/* Scheduled when the server is challenging a     */
    struct rxevent *delayedAbortEvent;	/* Scheduled to throttle looping client */
    struct rxevent *checkReachEvent;	/* Scheduled when checking reachability */
    int abortCount;		/* count of abort messages sent */
    /* client-- to retransmit the challenge */
    struct rx_service *service;	/* used by servers only */
    u_short serviceId;		/* To stamp on requests (clients only) */
    afs_uint32 refCount;		/* Reference count */
    u_char flags;		/* Defined below */
    u_char type;		/* Type of connection, defined below */
    u_char secondsUntilPing;	/* how often to ping for each active call */
    u_char securityIndex;	/* corresponds to the security class of the */
    /* securityObject for this conn */
    struct rx_securityClass *securityObject;	/* Security object for this connection */
    VOID *securityData;		/* Private data for this conn's security class */
    u_short securityHeaderSize;	/* Length of security module's packet header data */
    u_short securityMaxTrailerSize;	/* Length of security module's packet trailer data */

    int timeout;		/* Overall timeout per call (seconds) for this conn */
    int lastSendTime;		/* Last send time for this connection */
    u_short secondsUntilDead;	/* Maximum silence from peer before RX_CALL_DEAD */
    u_short hardDeadTime;	/* hard max for call execution */
    u_short idleDeadTime;	/* max time a call can be idle (no data) */
    u_char ackRate;		/* how many packets between ack requests */
    u_char makeCallWaiters;	/* how many rx_NewCalls are waiting */
    int nSpecific;		/* number entries in specific data */
    void **specific;		/* pointer to connection specific data */
};


/* A service is installed by rx_NewService, and specifies a service type that
 * is exported by this process.  Incoming calls are stamped with the service
 * type, and must match an installed service for the call to be accepted.
 * Each service exported has a (port,serviceId) pair to uniquely identify it.
 * It is also named:  this is intended to allow a remote statistics gathering
 * program to retrieve per service statistics without having to know the local
 * service id's.  Each service has a number of
 */

/* security objects (instances of security classes) which implement
 * various types of end-to-end security protocols for connections made
 * to this service.  Finally, there are two parameters controlling the
 * number of requests which may be executed in parallel by this
 * service: minProcs is the number of requests to this service which
 * are guaranteed to be able to run in parallel at any time; maxProcs
 * has two meanings: it limits the total number of requests which may
 * execute in parallel and it also guarantees that that many requests
 * may be handled in parallel if no other service is handling any
 * requests. */

struct rx_service {
    u_short serviceId;		/* Service number */
    afs_uint32 serviceHost;	/* IP address for this service */
    u_short servicePort;	/* UDP port for this service */
    char *serviceName;		/* Name of the service */
    osi_socket socket;		/* socket structure or file descriptor */
    u_short nRequestsRunning;	/* Number of requests currently in progress */
    u_short nSecurityObjects;	/* Number of entries in security objects array */
    struct rx_securityClass **securityObjects;	/* Array of security class objects */
      afs_int32(*executeRequestProc) (struct rx_call * acall);	/* Routine to call when an rpc request is received */
    void (*destroyConnProc) (struct rx_connection * tcon);	/* Routine to call when a server connection is destroyed */
    void (*newConnProc) (struct rx_connection * tcon);	/* Routine to call when a server connection is created */
    void (*beforeProc) (struct rx_call * acall);	/* routine to call before a call is executed */
    void (*afterProc) (struct rx_call * acall, afs_int32 code);	/* routine to call after a call is executed */
    u_short maxProcs;		/* Maximum procs to be used for this service */
    u_short minProcs;		/* Minimum # of requests guaranteed executable simultaneously */
    u_short connDeadTime;	/* Seconds until a client of this service will be declared dead, if it is not responding */
    u_short idleDeadTime;	/* Time a server will wait for I/O to start up again */
    u_char checkReach;		/* Check for asymmetric clients? */
};

#endif /* KDUMP_RX_LOCK */

/* A server puts itself on an idle queue for a service using an
 * instance of the following structure.  When a call arrives, the call
 * structure pointer is placed in "newcall", the routine to execute to
 * service the request is placed in executeRequestProc, and the
 * process is woken up.  The queue entry's address is used for the
 * sleep/wakeup. If socketp is non-null, then this thread is willing
 * to become a listener thread. A thread sets *socketp to -1 before
 * sleeping. If *socketp is not -1 when the thread awakes, it is now
 * the listener thread for *socketp. When socketp is non-null, tno
 * contains the server's threadID, which is used to make decitions in GetCall.
 */
#ifdef KDUMP_RX_LOCK
struct rx_serverQueueEntry_rx_lock {
#else
struct rx_serverQueueEntry {
#endif
    struct rx_queue queueItemHeader;
#ifdef KDUMP_RX_LOCK
    struct rx_call_rx_lock *newcall;
#else
    struct rx_call *newcall;
#endif
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t lock;
    afs_kcondvar_t cv;
#endif
    int tno;
    osi_socket *socketp;
};


/* A peer refers to a peer process, specified by a (host,port) pair.  There may be more than one peer on a given host. */
#ifdef KDUMP_RX_LOCK
struct rx_peer_rx_lock {
    struct rx_peer_rx_lock *next;	/* Next in hash conflict or free list */
#else
struct rx_peer {
    struct rx_peer *next;	/* Next in hash conflict or free list */
#endif
#ifdef RX_ENABLE_LOCKS
    afs_kmutex_t peer_lock;	/* Lock peer */
#endif				/* RX_ENABLE_LOCKS */
    afs_uint32 host;		/* Remote IP address, in net byte order */
    u_short port;		/* Remote UDP port, in net byte order */

    /* interface mtu probably used for this host  -  includes RX Header */
    u_short ifMTU;		/* doesn't include IP header */

    /* For garbage collection */
    afs_uint32 idleWhen;	/* When the refcountwent to zero */
    afs_uint32 refCount;		/* Reference count for this structure */

    /* Congestion control parameters */
    u_char burstSize;		/* Reinitialization size for the burst parameter */
    u_char burst;		/* Number of packets that can be transmitted right now, without pausing */
    struct clock burstWait;	/* Delay until new burst is allowed */
    struct rx_queue congestionQueue;	/* Calls that are waiting for non-zero burst value */
    int rtt;			/* Round trip time, measured in milliseconds/8 */
    int rtt_dev;		/* rtt smoothed error, in milliseconds/4 */
    struct clock timeout;	/* Current retransmission delay */
    int nSent;			/* Total number of distinct data packets sent, not including retransmissions */
    int reSends;		/* Total number of retransmissions for this peer, since this structure was created */

/* Skew: if a packet is received N packets later than expected (based
 * on packet serial numbers), then we define it to have a skew of N.
 * The maximum skew values allow us to decide when a packet hasn't
 * been received yet because it is out-of-order, as opposed to when it
 * is likely to have been dropped. */
    afs_uint32 inPacketSkew;	/* Maximum skew on incoming packets */
    afs_uint32 outPacketSkew;	/* Peer-reported max skew on our sent packets */
    int rateFlag;		/* Flag for rate testing (-no 0yes +decrement) */

    /* the "natural" MTU, excluding IP,UDP headers, is negotiated by the endpoints */
    u_short natMTU;
    u_short maxMTU;
    /* negotiated maximum number of packets to send in a single datagram. */
    u_short maxDgramPackets;
    /* local maximum number of packets to send in a single datagram. */
    u_short ifDgramPackets;
    /*
     * MTU, cwind, and nDgramPackets are used to initialize
     * slow start parameters for new calls. These values are set whenever a
     * call sends a retransmission and at the end of each call.
     * congestSeq is incremented each time the congestion parameters are
     * changed by a call recovering from a dropped packet. A call used
     * MAX when updating congestion parameters if it started with the
     * current congestion sequence number, otherwise it uses MIN.
     */
    u_short MTU;		/* MTU for AFS 3.4a jumboGrams */
    u_short cwind;		/* congestion window */
    u_short nDgramPackets;	/* number packets per AFS 3.5 jumbogram */
    u_short congestSeq;		/* Changed when a call retransmits */
    afs_hyper_t bytesSent;	/* Number of bytes sent to this peer */
    afs_hyper_t bytesReceived;	/* Number of bytes received from this peer */
    struct rx_queue rpcStats;	/* rpc statistic list */
    int lastReachTime;		/* Last time we verified reachability */
};


#ifndef KDUMP_RX_LOCK
/* Flag bits for connection structure */
#define	RX_CONN_MAKECALL_WAITING    1	/* rx_MakeCall is waiting for a channel */
#define	RX_CONN_DESTROY_ME	    2	/* Destroy *client* connection after last call */
#define RX_CONN_USING_PACKET_CKSUM  4	/* non-zero header.spare field seen */
#define RX_CONN_KNOW_WINDOW         8	/* window size negotiation works */
#define RX_CONN_RESET		   16	/* connection is reset, remove */
#define RX_CONN_BUSY               32	/* connection is busy; don't delete */
#define RX_CONN_ATTACHWAIT	   64	/* attach waiting for peer->lastReach */

/* Type of connection, client or server */
#define	RX_CLIENT_CONNECTION	0
#define	RX_SERVER_CONNECTION	1
#endif /* !KDUMP_RX_LOCK */

/* Call structure:  only instantiated for active calls and dallying server calls.  The permanent call state (i.e. the call number as well as state shared with other calls associated with this connection) is maintained in the connection structure. */
#ifdef KDUMP_RX_LOCK
struct rx_call_rx_lock {
#else
struct rx_call {
#endif
    struct rx_queue queue_item_header;	/* Call can be on various queues (one-at-a-time) */
    struct rx_queue tq;		/* Transmit packet queue */
    struct rx_queue rq;		/* Receive packet queue */
    /*
     * The following fields are accessed while the call is unlocked.
     * These fields are used by the caller/server thread to marshall
     * and unmarshall RPC data. The only time they may be changed by
     * other threads is when the RX_CALL_IOVEC_WAIT flag is set. 
     * 
     * NOTE: Be sure that these fields start and end on a double
     *       word boundary. Otherwise threads that are changing
     *       adjacent fields will cause problems.
     */
    struct rx_queue iovq;	/* readv/writev packet queue */
    u_short nLeft;		/* Number bytes left in first receive packet */
    u_short curvec;		/* current iovec in currentPacket */
    u_short curlen;		/* bytes remaining in curvec */
    u_short nFree;		/* Number bytes free in last send packet */
    struct rx_packet *currentPacket;	/* Current packet being assembled or being read */
    char *curpos;		/* current position in curvec */
    /*
     * End of fields accessed with call unlocked
     */
    u_char channel;		/* Index of call, within connection */
    u_char state;		/* Current call state as defined below */
    u_char mode;		/* Current mode of a call in ACTIVE state */
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t lock;		/* lock covers data as well as mutexes. */
    afs_kmutex_t *call_queue_lock;	/* points to lock for queue we're on,
					 * if any. */
    afs_kcondvar_t cv_twind;
    afs_kcondvar_t cv_rq;
    afs_kcondvar_t cv_tq;
#endif
#ifdef KDUMP_RX_LOCK
    struct rx_connection_rx_lock *conn;	/* Parent connection for call */
#else
    struct rx_connection *conn;	/* Parent connection for this call */
#endif
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
    u_short twind;		/* The transmit window:  we cannot assign a sequence number to a packet >= tfirst + twind */
    u_short cwind;		/* The congestion window */
    u_short nSoftAcked;		/* Number soft acked transmit packets */
    u_short nextCwind;		/* The congestion window after recovery */
    u_short nCwindAcks;		/* Number acks received at current cwind */
    u_short ssthresh;		/* The slow start threshold */
    u_short nDgramPackets;	/* Packets per AFS 3.5 jumbogram */
    u_short nAcks;		/* The number of consecttive acks */
    u_short nNacks;		/* Number packets acked that follow the
				 * first negatively acked packet */
    u_short nSoftAcks;		/* The number of delayed soft acks */
    u_short nHardAcks;		/* The number of delayed hard acks */
    u_short congestSeq;		/* Peer's congestion sequence counter */
    struct rxevent *resendEvent;	/* If this is non-Null, there is a retransmission event pending */
    struct rxevent *timeoutEvent;	/* If this is non-Null, then there is an overall timeout for this call */
    struct rxevent *keepAliveEvent;	/* Scheduled periodically in active calls to keep call alive */
    struct rxevent *delayedAckEvent;	/* Scheduled after all packets are received to send an ack if a reply or new call is not generated soon */
    struct rxevent *delayedAbortEvent;	/* Scheduled to throttle looping client */
    int abortCode;		/* error code from last RPC */
    int abortCount;		/* number of times last error was sent */
    u_int lastSendTime;		/* Last time a packet was sent on this call */
    u_int lastReceiveTime;	/* Last time a packet was received for this call */
    void (*arrivalProc) (register struct rx_call * call, register VOID * mh, register int index);	/* Procedure to call when reply is received */
    VOID *arrivalProcHandle;	/* Handle to pass to replyFunc */
    int arrivalProcArg;         /* Additional arg to pass to reply Proc */
    afs_uint32 lastAcked;	/* last packet "hard" acked by receiver */
    afs_uint32 startWait;	/* time server began waiting for input data/send quota */
    struct clock traceWait;	/* time server began waiting for input data/send quota */
    struct clock traceStart;	/* time the call started running */
    u_short MTU;		/* size of packets currently sending */
#ifdef RX_ENABLE_LOCKS
    short refCount;		/* Used to keep calls from disappearring
				 * when we get them from a queue. */
#endif				/* RX_ENABLE_LOCKS */
/* Call refcount modifiers */
#define RX_CALL_REFCOUNT_BEGIN  0	/* GetCall/NewCall/EndCall */
#define RX_CALL_REFCOUNT_RESEND 1	/* resend event */
#define RX_CALL_REFCOUNT_DELAY  2	/* delayed ack */
#define RX_CALL_REFCOUNT_ALIVE  3	/* keep alive event */
#define RX_CALL_REFCOUNT_PACKET 4	/* waiting for packets. */
#define RX_CALL_REFCOUNT_SEND   5	/* rxi_Send */
#define RX_CALL_REFCOUNT_ACKALL 6	/* rxi_AckAll */
#define RX_CALL_REFCOUNT_ABORT  7	/* delayed abort */
#define RX_CALL_REFCOUNT_MAX    8	/* array size. */
#ifdef RX_REFCOUNT_CHECK
    short refCDebug[RX_CALL_REFCOUNT_MAX];
#endif				/* RX_REFCOUNT_CHECK */
    int iovNBytes;		/* byte count for current iovec */
    int iovMax;			/* number elements in current iovec */
    int iovNext;		/* next entry in current iovec */
    struct iovec *iov;		/* current iovec */
    struct clock queueTime;	/* time call was queued */
    struct clock startTime;	/* time call was started */
    afs_hyper_t bytesSent;	/* Number bytes sent */
    afs_hyper_t bytesRcvd;	/* Number bytes received */
    u_short tqWaiters;
};

#ifndef KDUMP_RX_LOCK
/* Major call states */
#define	RX_STATE_NOTINIT  0	/* Call structure has never been initialized */
#define	RX_STATE_PRECALL  1	/* Server-only:  call is not in progress, but packets have arrived */
#define	RX_STATE_ACTIVE	  2	/* An active call; a process is dealing with this call */
#define	RX_STATE_DALLY	  3	/* Dallying after process is done with call */
#define	RX_STATE_HOLD	  4	/* Waiting for acks on reply data packets */

/* Call modes:  the modes of a call in RX_STATE_ACTIVE state (process attached) */
#define	RX_MODE_SENDING	  1	/* Sending or ready to send */
#define	RX_MODE_RECEIVING 2	/* Receiving or ready to receive */
#define	RX_MODE_ERROR	  3	/* Something in error for current conversation */
#define	RX_MODE_EOF	  4	/* Server has flushed (or client has read) last reply packet */

/* Flags */
#define	RX_CALL_READER_WAIT	   1	/* Reader is waiting for next packet */
#define	RX_CALL_WAIT_WINDOW_ALLOC  2	/* Sender is waiting for window to allocate buffers */
#define	RX_CALL_WAIT_WINDOW_SEND   4	/* Sender is waiting for window to send buffers */
#define	RX_CALL_WAIT_PACKETS	   8	/* Sender is waiting for packet buffers */
#define	RX_CALL_WAIT_PROC	  16	/* Waiting for a process to be assigned */
#define	RX_CALL_RECEIVE_DONE	  32	/* All packets received on this call */
#define	RX_CALL_CLEARED		  64	/* Receive queue cleared in precall state */
#define	RX_CALL_TQ_BUSY		 128	/* Call's Xmit Queue is busy; don't modify */
#define	RX_CALL_TQ_CLEARME	 256	/* Need to clear this call's TQ later */
#define RX_CALL_TQ_SOME_ACKED    512	/* rxi_Start needs to discard ack'd packets. */
#define RX_CALL_TQ_WAIT		1024	/* Reader is waiting for TQ_BUSY to be reset */
#define RX_CALL_FAST_RECOVER    2048	/* call is doing congestion recovery */
#define RX_CALL_FAST_RECOVER_WAIT 4096	/* thread is waiting to start recovery */
#define RX_CALL_SLOW_START_OK   8192	/* receiver acks every other packet */
#define RX_CALL_IOVEC_WAIT	16384	/* waiting thread is using an iovec */
#define RX_CALL_HAVE_LAST	32768	/* Last packet has been received */
#define RX_CALL_NEED_START	0x10000	/* tells rxi_Start to start again */

/* Maximum number of acknowledgements in an acknowledge packet */
#define	RX_MAXACKS	    255

/* The structure of the data portion of an acknowledge packet: An acknowledge
 * packet is in network byte order at all times.  An acknowledgement is always
 * prompted for a specific reason by a specific incoming packet.  This reason
 * is reported in "reason" and the packet's sequence number in the packet
 * header.seq.  In addition to this information, all of the current
 * acknowledgement information about this call is placed in the packet.
 * "FirstPacket" is the sequence number of the first packet represented in an
 * array of bytes, "acks", containing acknowledgement information for a number
 * of consecutive packets.  All packets prior to FirstPacket are implicitly
 * acknowledged: the sender need no longer be concerned about them.  Packets
 * from firstPacket+nAcks and on are not acknowledged.  Packets in the range
 * [firstPacket,firstPacket+nAcks) are each acknowledged explicitly.  The
 * acknowledgement may be RX_NACK if the packet is not (currently) at the
 * receiver (it may have never been received, or received and then later
 * dropped), or it may be RX_ACK if the packet is queued up waiting to be read
 * by the upper level software.  RX_ACK does not imply that the packet may not
 * be dropped before it is read; it does imply that the sender should stop
 * retransmitting the packet until notified otherwise.  The field
 * previousPacket identifies the previous packet received by the peer.  This
 * was used in a previous version of this software, and could be used in the
 * future.  The serial number in the data part of the ack packet corresponds to
 * the serial number oof the packet which prompted the acknowledge.  Any
 * packets which are explicitly not acknowledged, and which were last
 * transmitted with a serial number less than the provided serial number,
 * should be retransmitted immediately.  Actually, this is slightly inaccurate:
 * packets are not necessarily received in order.  When packets are habitually
 * transmitted out of order, this is allowed for in the retransmission
 * algorithm by introducing the notion of maximum packet skew: the degree of
 * out-of-orderness of the packets received on the wire.  This number is
 * communicated from the receiver to the sender in ack packets. */

struct rx_ackPacket {
    u_short bufferSpace;	/* Number of packet buffers available.  That is:  the number of buffers that the sender of the ack packet is willing to provide for data, on this or subsequent calls.  Lying is permissable. */
    u_short maxSkew;		/* Maximum difference between serial# of packet acknowledged and highest packet yet received */
    afs_uint32 firstPacket;	/* The first packet in the list of acknowledged packets */
    afs_uint32 previousPacket;	/* The previous packet number received (obsolete?) */
    afs_uint32 serial;		/* Serial number of the packet which prompted the acknowledge */
    u_char reason;		/* Reason for the acknowledge of ackPacket, defined below */
    u_char nAcks;		/* Number of acknowledgements */
    u_char acks[RX_MAXACKS];	/* Up to RX_MAXACKS packet acknowledgements, defined below */
    /* Packets <firstPacket are implicitly acknowledged and may be discarded by the sender.  Packets >= firstPacket+nAcks are implicitly NOT acknowledged.  No packets with sequence numbers >= firstPacket should be discarded by the sender (they may thrown out at any time by the receiver) */
};

#define FIRSTACKOFFSET 4

/* Reason for acknowledge message */
#define	RX_ACK_REQUESTED	1	/* Peer requested an ack on this packet */
#define	RX_ACK_DUPLICATE	2	/* Duplicate packet */
#define	RX_ACK_OUT_OF_SEQUENCE	3	/* Packet out of sequence */
#define	RX_ACK_EXCEEDS_WINDOW	4	/* Packet sequence number higher than window; discarded */
#define	RX_ACK_NOSPACE		5	/* No buffer space at all */
#define	RX_ACK_PING		6	/* This is a keep-alive ack */
#define	RX_ACK_PING_RESPONSE	7	/* Ack'ing because we were pinged */
#define	RX_ACK_DELAY		8	/* Ack generated since nothing has happened since receiving packet */
#define RX_ACK_IDLE             9	/* Similar to RX_ACK_DELAY, but can 
					 * be used to compute RTT */

/* Packet acknowledgement type */
#define	RX_ACK_TYPE_NACK	0	/* I Don't have this packet */
#define	RX_ACK_TYPE_ACK		1	/* I have this packet, although I may discard it later */

/* The packet size transmitted for an acknowledge is adjusted to reflect the actual size of the acks array.  This macro defines the size */
#define rx_AckDataSize(nAcks) (3 + nAcks + offsetof(struct rx_ackPacket, acks[0]))

#define	RX_CHALLENGE_TIMEOUT	2	/* Number of seconds before another authentication request packet is generated */
#define RX_CHALLENGE_MAXTRIES	50	/* Max # of times we resend challenge */
#define	RX_CHECKREACH_TIMEOUT	2	/* Number of seconds before another ping is generated */
#define	RX_CHECKREACH_TTL	60	/* Re-check reachability this often */

/* RX error codes.  RX uses error codes from -1 to -64.  Rxgen may use other error codes < -64; user programs are expected to return positive error codes */

/* Something bad happened to the connection; temporary loss of communication */
#define	RX_CALL_DEAD		    (-1)

/* An invalid operation, such as a client attempting to send data after having received the beginning of a reply from the server */
#define	RX_INVALID_OPERATION	    (-2)

/* An optional timeout per call may be specified */
#define	RX_CALL_TIMEOUT		    (-3)

/* End of data on a read */
#define	RX_EOF			    (-4)

/* Some sort of low-level protocol error */
#define	RX_PROTOCOL_ERROR	    (-5)

/* Generic user abort code; used when no more specific error code needs to be communicated.  For example, multi rx clients use this code to abort a multi rx call */
#define	RX_USER_ABORT		    (-6)

/* Port already in use (from rx_Init) */
#define RX_ADDRINUSE		    (-7)

/* EMSGSIZE returned from network.  Packet too big, must fragment */
#define RX_MSGSIZE		    (-8)

/* transient failure detected ( possibly the server is restarting ) */
/* this shud be equal to VRESTARTING ( util/errors.h ) for old clients to work */
#define RX_RESTARTING		    (-100)

struct rx_securityObjectStats {
    char type;			/* 0:unk 1:null,2:vab 3:kad */
    char level;
    char sparec[10];		/* force correct alignment */
    afs_int32 flags;		/* 1=>unalloc, 2=>auth, 4=>expired */
    afs_uint32 expires;
    afs_uint32 packetsReceived;
    afs_uint32 packetsSent;
    afs_uint32 bytesReceived;
    afs_uint32 bytesSent;
    short spares[4];
    afs_int32 sparel[8];
};

/* Configuration settings */

/* Enum for storing configuration variables which can be set via the 
 * SetConfiguration method in the rx_securityClass, below
 */

typedef enum {
     RXS_CONFIG_FLAGS /* afs_uint32 set of bitwise flags */
} rx_securityConfigVariables;

/* For the RXS_CONFIG_FLAGS, the following bit values are defined */

/* Disable the principal name contains dot check in rxkad */
#define RXS_CONFIG_FLAGS_DISABLE_DOTCHECK	0x01

/* XXXX (rewrite this description) A security class object contains a set of
 * procedures and some private data to implement a security model for rx
 * connections.  These routines are called by rx as appropriate.  Rx knows
 * nothing about the internal details of any particular security model, or
 * about security state.  Rx does maintain state per connection on behalf of
 * the security class.  Each security class implementation is also expected to
 * provide routines to create these objects.  Rx provides a basic routine to
 * allocate one of these objects; this routine must be called by the class. */
struct rx_securityClass {
    struct rx_securityOps {
	int (*op_Close) (struct rx_securityClass * aobj);
	int (*op_NewConnection) (struct rx_securityClass * aobj,
				 struct rx_connection * aconn);
	int (*op_PreparePacket) (struct rx_securityClass * aobj,
				 struct rx_call * acall,
				 struct rx_packet * apacket);
	int (*op_SendPacket) (struct rx_securityClass * aobj,
			      struct rx_call * acall,
			      struct rx_packet * apacket);
	int (*op_CheckAuthentication) (struct rx_securityClass * aobj,
				       struct rx_connection * aconn);
	int (*op_CreateChallenge) (struct rx_securityClass * aobj,
				   struct rx_connection * aconn);
	int (*op_GetChallenge) (struct rx_securityClass * aobj,
				struct rx_connection * aconn,
				struct rx_packet * apacket);
	int (*op_GetResponse) (struct rx_securityClass * aobj,
			       struct rx_connection * aconn,
			       struct rx_packet * apacket);
	int (*op_CheckResponse) (struct rx_securityClass * aobj,
				 struct rx_connection * aconn,
				 struct rx_packet * apacket);
	int (*op_CheckPacket) (struct rx_securityClass * aobj,
			       struct rx_call * acall,
			       struct rx_packet * apacket);
	int (*op_DestroyConnection) (struct rx_securityClass * aobj,
				     struct rx_connection * aconn);
	int (*op_GetStats) (struct rx_securityClass * aobj,
			    struct rx_connection * aconn,
			    struct rx_securityObjectStats * astats);
	int (*op_SetConfiguration) (struct rx_securityClass * aobj,
				    struct rx_connection * aconn,
				    rx_securityConfigVariables atype,
				    void * avalue,
				    void ** acurrentValue);
	int (*op_Spare2) (void);
	int (*op_Spare3) (void);
    } *ops;
    VOID *privateData;
    int refCount;
};

#define RXS_OP(obj,op,args) ((obj && (obj->ops->op_ ## op)) ? (*(obj)->ops->op_ ## op)args : 0)

#define RXS_Close(obj) RXS_OP(obj,Close,(obj))
#define RXS_NewConnection(obj,conn) RXS_OP(obj,NewConnection,(obj,conn))
#define RXS_PreparePacket(obj,call,packet) RXS_OP(obj,PreparePacket,(obj,call,packet))
#define RXS_SendPacket(obj,call,packet) RXS_OP(obj,SendPacket,(obj,call,packet))
#define RXS_CheckAuthentication(obj,conn) RXS_OP(obj,CheckAuthentication,(obj,conn))
#define RXS_CreateChallenge(obj,conn) RXS_OP(obj,CreateChallenge,(obj,conn))
#define RXS_GetChallenge(obj,conn,packet) RXS_OP(obj,GetChallenge,(obj,conn,packet))
#define RXS_GetResponse(obj,conn,packet) RXS_OP(obj,GetResponse,(obj,conn,packet))
#define RXS_CheckResponse(obj,conn,packet) RXS_OP(obj,CheckResponse,(obj,conn,packet))
#define RXS_CheckPacket(obj,call,packet) RXS_OP(obj,CheckPacket,(obj,call,packet))
#define RXS_DestroyConnection(obj,conn) RXS_OP(obj,DestroyConnection,(obj,conn))
#define RXS_GetStats(obj,conn,stats) RXS_OP(obj,GetStats,(obj,conn,stats))
#define RXS_SetConfiguration(obj, conn, type, value, currentValue) RXS_OP(obj, SetConfiguration,(obj,conn,type,value,currentValue))


/* Structure for keeping rx statistics.  Note that this structure is returned
 * by rxdebug, so, for compatibility reasons, new fields should be appended (or
 * spares used), the rxdebug protocol checked, if necessary, and the PrintStats
 * code should be updated as well.
 *
 * Clearly we assume that ntohl will work on these structures so sizeof(int)
 * must equal sizeof(afs_int32). */

struct rx_stats {		/* General rx statistics */
    int packetRequests;		/* Number of packet allocation requests */
    int receivePktAllocFailures;
    int sendPktAllocFailures;
    int specialPktAllocFailures;
    int socketGreedy;		/* Whether SO_GREEDY succeeded */
    int bogusPacketOnRead;	/* Number of inappropriately short packets received */
    int bogusHost;		/* Host address from bogus packets */
    int noPacketOnRead;		/* Number of read packets attempted when there was actually no packet to read off the wire */
    int noPacketBuffersOnRead;	/* Number of dropped data packets due to lack of packet buffers */
    int selects;		/* Number of selects waiting for packet or timeout */
    int sendSelects;		/* Number of selects forced when sending packet */
    int packetsRead[RX_N_PACKET_TYPES];	/* Total number of packets read, per type */
    int dataPacketsRead;	/* Number of unique data packets read off the wire */
    int ackPacketsRead;		/* Number of ack packets read */
    int dupPacketsRead;		/* Number of duplicate data packets read */
    int spuriousPacketsRead;	/* Number of inappropriate data packets */
    int packetsSent[RX_N_PACKET_TYPES];	/* Number of rxi_Sends: packets sent over the wire, per type */
    int ackPacketsSent;		/* Number of acks sent */
    int pingPacketsSent;	/* Total number of ping packets sent */
    int abortPacketsSent;	/* Total number of aborts */
    int busyPacketsSent;	/* Total number of busies sent received */
    int dataPacketsSent;	/* Number of unique data packets sent */
    int dataPacketsReSent;	/* Number of retransmissions */
    int dataPacketsPushed;	/* Number of retransmissions pushed early by a NACK */
    int ignoreAckedPacket;	/* Number of packets with acked flag, on rxi_Start */
    struct clock totalRtt;	/* Total round trip time measured (use to compute average) */
    struct clock minRtt;	/* Minimum round trip time measured */
    struct clock maxRtt;	/* Maximum round trip time measured */
    int nRttSamples;		/* Total number of round trip samples */
    int nServerConns;		/* Total number of server connections */
    int nClientConns;		/* Total number of client connections */
    int nPeerStructs;		/* Total number of peer structures */
    int nCallStructs;		/* Total number of call structures allocated */
    int nFreeCallStructs;	/* Total number of previously allocated free call structures */
    int netSendFailures;
    afs_int32 fatalErrors;
    int ignorePacketDally;	/* packets dropped because call is in dally state */
    int receiveCbufPktAllocFailures;
    int sendCbufPktAllocFailures;
    int nBusies;
    int spares[4];
};

/* structures for debug input and output packets */

/* debug input types */
struct rx_debugIn {
    afs_int32 type;
    afs_int32 index;
};

/* Invalid rx debug package type */
#define RX_DEBUGI_BADTYPE     (-8)

#define RX_DEBUGI_VERSION_MINIMUM ('L')	/* earliest real version */
#define RX_DEBUGI_VERSION     ('R')	/* Latest version */
    /* first version w/ secStats */
#define RX_DEBUGI_VERSION_W_SECSTATS ('L')
    /* version M is first supporting GETALLCONN and RXSTATS type */
#define RX_DEBUGI_VERSION_W_GETALLCONN ('M')
#define RX_DEBUGI_VERSION_W_RXSTATS ('M')
    /* last version with unaligned debugConn */
#define RX_DEBUGI_VERSION_W_UNALIGNED_CONN ('L')
#define RX_DEBUGI_VERSION_W_WAITERS ('N')
#define RX_DEBUGI_VERSION_W_IDLETHREADS ('O')
#define RX_DEBUGI_VERSION_W_NEWPACKETTYPES ('P')
#define RX_DEBUGI_VERSION_W_GETPEER ('Q')
#define RX_DEBUGI_VERSION_W_WAITED ('R')

#define	RX_DEBUGI_GETSTATS	1	/* get basic rx stats */
#define	RX_DEBUGI_GETCONN	2	/* get connection info */
#define	RX_DEBUGI_GETALLCONN	3	/* get even uninteresting conns */
#define	RX_DEBUGI_RXSTATS	4	/* get all rx stats */
#define	RX_DEBUGI_GETPEER	5	/* get all peer structs */

struct rx_debugStats {
    afs_int32 nFreePackets;
    afs_int32 packetReclaims;
    afs_int32 callsExecuted;
    char waitingForPackets;
    char usedFDs;
    char version;
    char spare1;
    afs_int32 nWaiting;
    afs_int32 idleThreads;	/* Number of server threads that are idle */
    afs_int32 nWaited;
    afs_int32 spare2[7];
};

struct rx_debugConn_vL {
    afs_int32 host;
    afs_int32 cid;
    afs_int32 serial;
    afs_int32 callNumber[RX_MAXCALLS];
    afs_int32 error;
    short port;
    char flags;
    char type;
    char securityIndex;
    char callState[RX_MAXCALLS];
    char callMode[RX_MAXCALLS];
    char callFlags[RX_MAXCALLS];
    char callOther[RX_MAXCALLS];
    /* old style getconn stops here */
    struct rx_securityObjectStats secStats;
    afs_int32 sparel[10];
};

struct rx_debugConn {
    afs_int32 host;
    afs_int32 cid;
    afs_int32 serial;
    afs_int32 callNumber[RX_MAXCALLS];
    afs_int32 error;
    short port;
    char flags;
    char type;
    char securityIndex;
    char sparec[3];		/* force correct alignment */
    char callState[RX_MAXCALLS];
    char callMode[RX_MAXCALLS];
    char callFlags[RX_MAXCALLS];
    char callOther[RX_MAXCALLS];
    /* old style getconn stops here */
    struct rx_securityObjectStats secStats;
    afs_int32 epoch;
    afs_int32 natMTU;
    afs_int32 sparel[9];
};

struct rx_debugPeer {
    afs_uint32 host;
    u_short port;
    u_short ifMTU;
    afs_uint32 idleWhen;
    short refCount;
    u_char burstSize;
    u_char burst;
    struct clock burstWait;
    afs_int32 rtt;
    afs_int32 rtt_dev;
    struct clock timeout;
    afs_int32 nSent;
    afs_int32 reSends;
    afs_int32 inPacketSkew;
    afs_int32 outPacketSkew;
    afs_int32 rateFlag;
    u_short natMTU;
    u_short maxMTU;
    u_short maxDgramPackets;
    u_short ifDgramPackets;
    u_short MTU;
    u_short cwind;
    u_short nDgramPackets;
    u_short congestSeq;
    afs_hyper_t bytesSent;
    afs_hyper_t bytesReceived;
    afs_int32 sparel[10];
};

#define	RX_OTHER_IN	1	/* packets avail in in queue */
#define	RX_OTHER_OUT	2	/* packets avail in out queue */



/* Only include this once, even when re-loading for kdump. */
#ifndef _CALL_REF_DEFINED_
#define _CALL_REF_DEFINED_

#ifdef RX_ENABLE_LOCKS
#ifdef RX_REFCOUNT_CHECK
/* RX_REFCOUNT_CHECK is used to test for call refcount leaks by event
 * type.
 */
extern int rx_callHoldType;
#define CALL_HOLD(call, type) do { \
				 call->refCount++; \
				 call->refCDebug[type]++; \
				 if (call->refCDebug[type] > 50)  {\
				     rx_callHoldType = type; \
				     osi_Panic("Huge call refCount"); \
							       } \
			     } while (0)
#define CALL_RELE(call, type) do { \
				 call->refCount--; \
				 call->refCDebug[type]--; \
				 if (call->refCDebug[type] > 50) {\
				     rx_callHoldType = type; \
				     osi_Panic("Negative call refCount"); \
							      } \
			     } while (0)
#else /* RX_REFCOUNT_CHECK */
#define CALL_HOLD(call, type) 	 call->refCount++
#define CALL_RELE(call, type)	 call->refCount--
#endif /* RX_REFCOUNT_CHECK */

#else /* RX_ENABLE_LOCKS */
#define CALL_HOLD(call, type)
#define CALL_RELE(call, type)
#endif /* RX_ENABLE_LOCKS */

#endif /* _CALL_REF_DEFINED_ */

#define RX_SERVER_DEBUG_SEC_STATS		0x1
#define RX_SERVER_DEBUG_ALL_CONN		0x2
#define RX_SERVER_DEBUG_RX_STATS		0x4
#define RX_SERVER_DEBUG_WAITER_CNT		0x8
#define RX_SERVER_DEBUG_IDLE_THREADS		0x10
#define RX_SERVER_DEBUG_OLD_CONN		0x20
#define RX_SERVER_DEBUG_NEW_PACKETS		0x40
#define RX_SERVER_DEBUG_ALL_PEER		0x80
#define RX_SERVER_DEBUG_WAITED_CNT              0x100

#define AFS_RX_STATS_CLEAR_ALL			0xffffffff
#define AFS_RX_STATS_CLEAR_INVOCATIONS		0x1
#define AFS_RX_STATS_CLEAR_BYTES_SENT		0x2
#define AFS_RX_STATS_CLEAR_BYTES_RCVD		0x4
#define AFS_RX_STATS_CLEAR_QUEUE_TIME_SUM	0x8
#define AFS_RX_STATS_CLEAR_QUEUE_TIME_SQUARE	0x10
#define AFS_RX_STATS_CLEAR_QUEUE_TIME_MIN	0x20
#define AFS_RX_STATS_CLEAR_QUEUE_TIME_MAX	0x40
#define AFS_RX_STATS_CLEAR_EXEC_TIME_SUM	0x80
#define AFS_RX_STATS_CLEAR_EXEC_TIME_SQUARE	0x100
#define AFS_RX_STATS_CLEAR_EXEC_TIME_MIN	0x200
#define AFS_RX_STATS_CLEAR_EXEC_TIME_MAX	0x400

typedef struct rx_function_entry_v1 {
    afs_uint32 remote_peer;
    afs_uint32 remote_port;
    afs_uint32 remote_is_server;
    afs_uint32 interfaceId;
    afs_uint32 func_total;
    afs_uint32 func_index;
    afs_hyper_t invocations;
    afs_hyper_t bytes_sent;
    afs_hyper_t bytes_rcvd;
    struct clock queue_time_sum;
    struct clock queue_time_sum_sqr;
    struct clock queue_time_min;
    struct clock queue_time_max;
    struct clock execution_time_sum;
    struct clock execution_time_sum_sqr;
    struct clock execution_time_min;
    struct clock execution_time_max;
} rx_function_entry_v1_t, *rx_function_entry_v1_p;

/*
 * If you need to change rx_function_entry, you should probably create a brand
 * new structure.  Keeping the old structure will allow backwards compatibility
 * with old clients (even if it is only used to calculate allocation size).
 * If you do change the size or the format, you'll need to bump
 * RX_STATS_RETRIEVAL_VERSION.  This allows some primitive form
 * of versioning a la rxdebug.
 */

#define RX_STATS_RETRIEVAL_VERSION 1	/* latest version */
#define RX_STATS_RETRIEVAL_FIRST_EDITION 1	/* first implementation */

typedef struct rx_interface_stat {
    struct rx_queue queue_header;
    struct rx_queue all_peers;
    rx_function_entry_v1_t stats[1];	/* make sure this is aligned correctly */
} rx_interface_stat_t, *rx_interface_stat_p;

#define RX_STATS_SERVICE_ID 409



#endif /* _RX_   End of rx.h */

#ifdef	KERNEL
#include "rx/rx_prototypes.h"
#else
#include "rx_prototypes.h"
#endif

#endif /* !KDUMP_RX_LOCK */
