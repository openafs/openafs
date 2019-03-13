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
#ifdef	KERNEL
#include "rx_kmutex.h"
#include "rx_kernel.h"
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
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
# include "rx_user.h"
#ifndef AFS_NT40_ENV
# include <netinet/in.h>
# include <sys/socket.h>
#endif
#endif /* KERNEL */

#include <opr/queue.h>

#include "rx_clock.h"
#include "rx_event.h"
#include "rx_misc.h"
#include "rx_null.h"
#include "rx_multi.h"

/* These items are part of the new RX API. They're living in this section
 * for now, to keep them separate from everything else... */

struct rx_connection;
struct rx_call;
struct rx_packet;

/* Connection management */

extern afs_uint32  rx_GetConnectionEpoch(struct rx_connection *conn);
extern afs_uint32  rx_GetConnectionId(struct rx_connection *conn);
extern void *rx_GetSecurityData(struct rx_connection *conn);
extern void  rx_SetSecurityData(struct rx_connection *conn, void *data);
extern int  rx_IsUsingPktCksum(struct rx_connection *conn);
extern void rx_SetSecurityHeaderSize(struct rx_connection *conn, afs_uint32 size);
extern afs_uint32  rx_GetSecurityHeaderSize(struct rx_connection *conn);
extern void rx_SetSecurityMaxTrailerSize(struct rx_connection *conn, afs_uint32 size);
extern afs_uint32  rx_GetSecurityMaxTrailerSize(struct rx_connection *conn);
extern void rx_SetMsgsizeRetryErr(struct rx_connection *conn, int err);
extern int  rx_IsServerConn(struct rx_connection *conn);
extern int  rx_IsClientConn(struct rx_connection *conn);
extern struct rx_securityClass *rx_SecurityObjectOf(const struct rx_connection *);
extern struct rx_peer *rx_PeerOf(struct rx_connection *);
extern u_short rx_ServiceIdOf(struct rx_connection *);
extern int rx_SecurityClassOf(struct rx_connection *);
extern struct rx_service *rx_ServiceOf(struct rx_connection *);
extern int rx_ConnError(struct rx_connection *);

/* Call management */
extern struct rx_connection *rx_ConnectionOf(struct rx_call *call);
extern int rx_Error(struct rx_call *call);
extern int rx_GetRemoteStatus(struct rx_call *call);
extern void rx_SetLocalStatus(struct rx_call *call, int status);
extern int rx_GetCallAbortCode(struct rx_call *call);
extern void rx_SetCallAbortCode(struct rx_call *call, int code);

extern void rx_RecordCallStatistics(struct rx_call *call,
				    unsigned int rxInterface,
				    unsigned int currentFunc,
				    unsigned int totalFunc,
				    int isServer);

extern void rx_GetCallStatus(struct rx_call *call,
			     afs_int32 *readNext,
			     afs_int32 *transmitNext,
			     int *lastSendTime,
			     int *lastReceiveTime);
/* Peer management */
extern afs_uint32 rx_HostOf(struct rx_peer *peer);
extern u_short rx_PortOf(struct rx_peer *peer);

/* Packets */

/* Packet classes, for rx_AllocPacket and rx_packetQuota */
#define	RX_PACKET_CLASS_RECEIVE	    0
#define	RX_PACKET_CLASS_SEND	    1
#define	RX_PACKET_CLASS_SPECIAL	    2
#define	RX_PACKET_CLASS_RECV_CBUF   3
#define	RX_PACKET_CLASS_SEND_CBUF   4

#define	RX_N_PACKET_CLASSES	    5	/* Must agree with above list */

#define	RX_PACKET_TYPES	    {"data", "ack", "busy", "abort", "ackall", "challenge", "response", "debug", "params", "unused", "unused", "unused", "version"}
#define	RX_N_PACKET_TYPES	    13	/* Must agree with above list;
					 * counts 0
					 * WARNING: if this number ever
					 * grows past 13, rxdebug packets
					 * will need to be modified */


/* For most Unixes, maximum elements in an iovec is 16 */
#define RX_MAXIOVECS 16		        /* limit for ReadvProc/WritevProc */
#define RX_MAXWVECS (RX_MAXIOVECS-1)	/* need one iovec for packet header */

/* Debugging */

/* Call flags, states and modes are exposed by the debug interface */
#ifndef KDUMP_RX_LOCK
/* Major call states */
#define	RX_STATE_NOTINIT  0	/* Call structure has never been initialized */
#define	RX_STATE_PRECALL  1	/* Server-only:  call is not in progress, but packets have arrived */
#define	RX_STATE_ACTIVE	  2	/* An active call; a process is dealing with this call */
#define	RX_STATE_DALLY	  3	/* Dallying after process is done with call */
#define	RX_STATE_HOLD	  4	/* Waiting for acks on reply data packets */
#define RX_STATE_RESET    5     /* Call is being reset */

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
/* 4096 was RX_CALL_FAST_RECOVER_WAIT */
#define RX_CALL_SLOW_START_OK   8192	/* receiver acks every other packet */
#define RX_CALL_IOVEC_WAIT	16384	/* waiting thread is using an iovec */
#define RX_CALL_HAVE_LAST	32768	/* Last packet has been received */
#define RX_CALL_NEED_START	0x10000	/* tells rxi_Start to start again */
/* 0x20000 was RX_CALL_PEER_BUSY */
#define RX_CALL_ACKALL_SENT     0x40000 /* ACKALL has been sent on the call */
#define RX_CALL_FLUSH		0x80000 /* Transmit queue should be flushed to peer */
#endif


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

#define rx_GetLocalStatus(call, status) ((call)->localStatus)


static_inline int
rx_IsLoopbackAddr(afs_uint32 addr)
{
    return ((addr & 0xffff0000) == 0x7f000000);
}

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
#define rx_SetPostProc(service,proc) ((service)->postProc = (proc))
#define rx_GetPostProc(service) ((service)->postProc)

/* Define a procedure to be called when a server connection is created */
#define rx_SetNewConnProc(service, proc) ((service)->newConnProc = (proc))

/* NOTE:  We'll probably redefine the following three routines, again, sometime. */

/* Set the connection dead time for any connections created for this service (server only) */
#define rx_SetServiceDeadTime(service, seconds) ((service)->secondsUntilDead = (seconds))

/* Enable or disable asymmetric client checking for a service */
#define rx_SetCheckReach(service, x) ((service)->checkReach = (x))

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
    void (*postProc) (afs_int32 code);	/* routine to call after the call has ended */
    u_short maxProcs;		/* Maximum procs to be used for this service */
    u_short minProcs;		/* Minimum # of requests guaranteed executable simultaneously */
    u_short connDeadTime;	/* Seconds until a client of this service will be declared dead, if it is not responding */
    u_short idleDeadTime;	/* Time a server will wait for I/O to start up again */
    u_char checkReach;		/* Check for asymmetric clients? */
    int nSpecific;		/* number entries in specific data */
    void **specific;		/* pointer to connection specific data */
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t svc_data_lock;	/* protect specific data */
#endif

};

#endif /* KDUMP_RX_LOCK */

#ifndef KDUMP_RX_LOCK
/* Flag bits for connection structure */
#define	RX_CONN_MAKECALL_WAITING    1	/* rx_NewCall is waiting for a channel */
#define	RX_CONN_DESTROY_ME	    2	/* Destroy *client* connection after last call */
#define RX_CONN_USING_PACKET_CKSUM  4	/* non-zero header.spare field seen */
#define RX_CONN_KNOW_WINDOW         8	/* window size negotiation works */
#define RX_CONN_RESET		   16	/* connection is reset, remove */
#define RX_CONN_BUSY               32	/* connection is busy; don't delete */
#define RX_CONN_ATTACHWAIT	   64	/* attach waiting for peer->lastReach */
#define RX_CONN_MAKECALL_ACTIVE   128   /* a thread is actively in rx_NewCall */
#define RX_CONN_NAT_PING          256   /* NAT ping requested but deferred during attachWait */
#define RX_CONN_CACHED		  512   /* connection is managed by rxi_connectionCache */

/* Type of connection, client or server */
#define	RX_CLIENT_CONNECTION	0
#define	RX_SERVER_CONNECTION	1
#endif /* !KDUMP_RX_LOCK */

/* Maximum number of acknowledgements in an acknowledge packet */
#define	RX_MAXACKS	    255

#ifndef KDUMP_RX_LOCK

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
    u_char nAcks;		/* Number of acknowledgements (saturates at 255) */
    u_char acks[RX_MAXACKS];	/* Packet acknowledgements, one bit per packet.
				 * The first (up to) RX_MAXACKS packets'
				 * acknowledgment state is indicated by bit-0
				 * of the corresponding byte of acks[].  The
				 * additional bits are reserved for future use. */
    /*
     * DATA packets whose sequence number is less than firstPacket are
     * implicitly acknowledged and may be discarded by the sender.
     * DATA packets whose sequence number is greater than or equal to
     * (firstPacket + nAcks) are implicitly NOT acknowledged.
     * No DATA packets with sequence numbers greater than or equal to
     * firstPacket should be discarded by the sender (they may be thrown
     * out by the receiver and listed as NOT acknowledged in a subsequent
     * ACK packet.)
     */
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
#define RX_ACK_MTU             -1       /* will be rewritten to ACK_PING */

/* Packet acknowledgement type (for maximum window size 255) */
#define RX_ACK_TYPE_NACK        0x0     /* I Don't have this packet */
#define RX_ACK_TYPE_ACK         0x1     /* I have this packet, although I may discard it later */

/* The packet size transmitted for an acknowledge is adjusted to reflect the actual size of the acks array.  This macro defines the size */
#define rx_AckDataSize(nAcks) (3 + nAcks + offsetof(struct rx_ackPacket, acks[0]))

#define	RX_CHALLENGE_TIMEOUT	2	/* Number of seconds before another authentication request packet is generated */
#define RX_CHALLENGE_MAXTRIES	50	/* Max # of times we resend challenge */
#define	RX_CHECKREACH_TIMEOUT	2	/* Number of seconds before another ping is generated */
#define	RX_CHECKREACH_TTL	60	/* Re-check reachability this often */

/*
 * rx_GetNetworkError 'origin' constants. These define the meaning of the
 * 'type' and 'code' values returned by rx_GetNetworkError.
 */

/* Used for ICMP errors; the type and code are the ICMP type and code,
 * respectively */
#define RX_NETWORK_ERROR_ORIGIN_ICMP (0)

/*
 * RX error codes.  RX uses error codes from -1 to -64 and -100.
 * Rxgen uses other error codes < -64 (see src/rxgen/rpc_errors.h);
 * user programs are expected to return positive error codes
 */

/* Something bad happened to the connection; temporary loss of communication */
#define	RX_CALL_DEAD		    (-1)

/*
 * An invalid operation, such as a client attempting to send data
 * after having received the beginning of a reply from the server.
 */
#define	RX_INVALID_OPERATION	    (-2)

/* An optional timeout per call may be specified */
#define	RX_CALL_TIMEOUT		    (-3)

/* End of data on a read.  Not currently in use. */
#define	RX_EOF			    (-4)

/* Some sort of low-level protocol error. */
#define	RX_PROTOCOL_ERROR	    (-5)

/*
 * Generic user abort code; used when no more specific error code needs to be
 * communicated.  For example, multi rx clients use this code to abort a multi-
 * rx call.
 */
#define	RX_USER_ABORT		    (-6)

/* Port already in use (from rx_Init).  This error is never sent on the wire. */
#define RX_ADDRINUSE		    (-7)

/* EMSGSIZE returned from network.  Packet too big, must fragment */
#define RX_MSGSIZE		    (-8)

/* The value -9 was previously used for RX_CALL_IDLE but is now free for
 * reuse. */

/* The value -10 was previously used for RX_CALL_BUSY but is now free for
 * reuse. */

/* transient failure detected ( possibly the server is restarting ) */
/* this should be equal to VRESTARTING ( util/errors.h ) for old clients to work */
#define RX_RESTARTING		    (-100)

typedef enum {
    RX_SECIDX_NULL = 0,		/** rxnull, no security. */
    RX_SECIDX_VAB  = 1,		/** vice tokens with bcrypt.  Unused. */
    RX_SECIDX_KAD  = 2,		/** kerberos/DES. */
    RX_SECIDX_KAE  = 3,		/** rxkad, but always encrypt. */
    RX_SECIDX_GK   = 4,		/** rxgk, RFC 3961 crypto. */
    RX_SECIDX_K5   = 5,		/** kerberos 5 tickets as tokens. */
} rx_securityIndex;

/*
 * We use an enum for the symbol definitions but have no need for a typedef
 * because the enum is at least as wide as 'int' and these have to fit into
 * a field of type 'char'.  Direct assigment will do the right thing if the
 * enum value fits into that type.
 */
enum {
    RX_SECTYPE_UNK = 0,
    RX_SECTYPE_NULL = 1,
    RX_SECTYPE_VAB = 2,
    RX_SECTYPE_KAD = 3,
};
struct rx_securityObjectStats {
    char type;			/* An RX_SECTYPE_* value */
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
	void (*op_DestroyConnection) (struct rx_securityClass * aobj,
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
    void *privateData;
    int refCount;
};

#define RXS_OP(obj,op,args) ((obj && (obj->ops->op_ ## op)) ? (*(obj)->ops->op_ ## op)args : 0)
#define RXS_OP_VOID(obj,op,args) do { \
    if (obj && (obj->ops->op_ ## op)) \
	(*(obj)->ops->op_ ## op)args; \
} while (0)

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
#define RXS_DestroyConnection(obj,conn) RXS_OP_VOID(obj,DestroyConnection,(obj,conn))
#define RXS_GetStats(obj,conn,stats) RXS_OP(obj,GetStats,(obj,conn,stats))
#define RXS_SetConfiguration(obj, conn, type, value, currentValue) RXS_OP(obj, SetConfiguration,(obj,conn,type,value,currentValue))



/* Structure for keeping rx statistics.  Note that this structure is returned
 * by rxdebug, so, for compatibility reasons, new fields should be appended (or
 * spares used), the rxdebug protocol checked, if necessary, and the PrintStats
 * code should be updated as well.
 *
 * Clearly we assume that ntohl will work on these structures so sizeof(int)
 * must equal sizeof(afs_int32). */

struct rx_statistics {		/* General rx statistics */
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
#define RX_DEBUGI_VERSION     ('S')    /* Latest version */
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
#define RX_DEBUGI_VERSION_W_PACKETS ('S')

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
    afs_int32 nPackets;
    afs_int32 spare2[6];
};

struct rx_debugConn_vL {
    afs_uint32 host;
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
    afs_uint32 host;
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

#define RX_SERVER_DEBUG_SEC_STATS		0x1
#define RX_SERVER_DEBUG_ALL_CONN		0x2
#define RX_SERVER_DEBUG_RX_STATS		0x4
#define RX_SERVER_DEBUG_WAITER_CNT		0x8
#define RX_SERVER_DEBUG_IDLE_THREADS		0x10
#define RX_SERVER_DEBUG_OLD_CONN		0x20
#define RX_SERVER_DEBUG_NEW_PACKETS		0x40
#define RX_SERVER_DEBUG_ALL_PEER		0x80
#define RX_SERVER_DEBUG_WAITED_CNT              0x100
#define RX_SERVER_DEBUG_PACKETS_CNT              0x200

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
    afs_uint64 invocations;
    afs_uint64 bytes_sent;
    afs_uint64 bytes_rcvd;
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
    struct opr_queue entry;
    struct opr_queue entryPeers;
    rx_function_entry_v1_t stats[1];	/* make sure this is aligned correctly */
} rx_interface_stat_t, *rx_interface_stat_p;

#define RX_STATS_SERVICE_ID 409

#ifdef AFS_NT40_ENV
extern int rx_DumpCalls(FILE *outputFile, char *cookie);
#endif

#endif /* _RX_   End of rx.h */

#ifdef	KERNEL
#include "rx/rx_prototypes.h"
#else
#include "rx_prototypes.h"
#endif

static_inline afs_uint32
RPCOpStat_Peer(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->remote_peer;
}

static_inline afs_uint32
RPCOpStat_Port(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->remote_port;
}

static_inline afs_uint32
RPCOpStat_IsServer(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->remote_is_server;
}

static_inline afs_uint32
RPCOpStat_InterfaceId(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->interfaceId;
}

static_inline afs_uint32
RPCOpStat_NumFuncs(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->func_total;
}

static_inline afs_uint32
RPCOpStat_CurFunc(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->func_index;
}

static_inline struct clock *
RPCOpStat_QTimeSum(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->queue_time_sum);
}

static_inline struct clock *
RPCOpStat_QTimeSumSqr(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->queue_time_sum_sqr);
}

static_inline struct clock *
RPCOpStat_QTimeSumMin(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->queue_time_min);
}

static_inline struct clock *
RPCOpStat_QTimeSumMax(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->queue_time_max);
}

static_inline struct clock *
RPCOpStat_ExecTimeSum(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->execution_time_sum);
}

static_inline struct clock *
RPCOpStat_ExecTimeSumSqr(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->execution_time_sum_sqr);
}

static_inline struct clock *
RPCOpStat_ExecTimeSumMin(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->execution_time_min);
}

static_inline struct clock *
RPCOpStat_ExecTimeSumMax(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return &(rpcop_stat->execution_time_max);
}

static_inline afs_uint64
RPCOpStat_NumCalls(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->invocations;
}

static_inline afs_uint64
RPCOpStat_BytesSent(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->bytes_sent;
}

static_inline afs_uint64
RPCOpStat_BytesRcvd(void *blob) {
    rx_function_entry_v1_p rpcop_stat = (rx_function_entry_v1_p)blob;
    return rpcop_stat->bytes_rcvd;
}
#endif /* !KDUMP_RX_LOCK */
