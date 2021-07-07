/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_RX_CONN_H
#define OPENAFS_RX_CONN_H 1

/* A connection is an authenticated communication path, allowing limited
 * multiple asynchronous conversations. */

struct rx_connection {
    struct rx_connection *next;	/*  on hash chain _or_ free list */
    struct rx_peer *peer;
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t conn_call_lock;	/* locks conn_call_cv */
    afs_kcondvar_t conn_call_cv;
    afs_kmutex_t conn_data_lock;	/* locks packet data */
#endif
    afs_uint32 epoch;		/* Process start time of client side of connection */
    afs_uint32 cid;		/* Connection id (call channel is bottom bits) */
    afs_int32 error;		/* If this connection is in error, this is it */
    struct rx_call *call[RX_MAXCALLS];
    afs_uint32 callNumber[RX_MAXCALLS];	/* Current call numbers */
    afs_uint32 rwind[RX_MAXCALLS];
    u_short twind[RX_MAXCALLS];
    afs_uint32 lastBusy[RX_MAXCALLS]; /* timestamp of the last time we got an
                                       * RX_PACKET_TYPE_BUSY packet for this
                                       * call slot, or 0 if the slot is not busy */
    afs_uint32 serial;		/* Next outgoing packet serial number */
    afs_int32 lastPacketSize; /* size of last >max attempt, excludes headers */
    afs_int32 lastPacketSizeSeq; /* seq number of attempt */
    afs_int32 lastPingSize; /* size of last MTU ping attempt, w/o headers */
    afs_int32 lastPingSizeSer; /* serial of last MTU ping attempt */
    struct rxevent *challengeEvent;	/* Scheduled when the server is challenging a     */
    struct rxevent *delayedAbortEvent;	/* Scheduled to throttle looping client */
    struct rxevent *checkReachEvent;	/* Scheduled when checking reachability */
    int abortCount;		/* count of abort messages sent */
    /* client-- to retransmit the challenge */
    struct rx_service *service;	/* used by servers only */
    u_short serviceId;		/* To stamp on requests (clients only) */
    afs_int32 refCount;	        /* Reference count (rx_refcnt_mutex) */
    u_char spare;		/* was flags - placeholder for alignment */
    u_char type;		/* Type of connection, defined below */
    u_char secondsUntilPing;	/* how often to ping for each active call */
    u_char securityIndex;	/* corresponds to the security class of the */
    afs_uint32 flags;		/* Defined in rx.h RX_CONN_* */
    /* securityObject for this conn */
    struct rx_securityClass *securityObject;	/* Security object for this connection */
    void *securityData;		/* Private data for this conn's security class */
    u_short securityHeaderSize;	/* Length of security module's packet header data */
    u_short securityMaxTrailerSize;	/* Length of security module's packet trailer data */
    int securityChallengeSent;	/* Have we ever sent a challenge? */

    int timeout;		/* Overall timeout per call (seconds) for this conn */
    int lastSendTime;		/* Last send time for this connection */
    u_short secondsUntilDead;	/* Maximum silence from peer before RX_CALL_DEAD */
    u_short hardDeadTime;	/* hard max for call execution */
    u_short idleDeadTime;	/* max time a call can be idle (no data) */
    u_char ackRate;		/* how many packets between ack requests */
    u_char makeCallWaiters;	/* how many rx_NewCalls are waiting */
    afs_int32 secondsUntilNatPing;	/* how often to ping conn */
    struct rxevent *natKeepAliveEvent; /* Scheduled to keep connection open */
    afs_int32 msgsizeRetryErr;
    int nSpecific;		/* number entries in specific data */
    void **specific;		/* pointer to connection specific data */
};

#endif

