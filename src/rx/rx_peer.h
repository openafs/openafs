/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_RX_PEER_H
#define OPENAFS_RX_PEER_H

/* A peer refers to a peer process, specified by a (host,port) pair.  There may
 * be more than one peer on a given host. */

struct rx_peer {
    struct rx_peer *next;	/* Next in hash conflict or free list */
#ifdef RX_ENABLE_LOCKS
    afs_kmutex_t peer_lock;	/* Lock peer */
#endif				/* RX_ENABLE_LOCKS */
    afs_uint32 host;		/* Remote IP address, in net byte order */
    u_short port;		/* Remote UDP port, in net byte order */

    /* interface mtu probably used for this host  -  includes RX Header */
    u_short ifMTU;		/* doesn't include IP/UDP header */

    /* For garbage collection */
    afs_uint32 idleWhen;	/* When the refcountwent to zero */
    afs_int32 refCount;	        /* Reference count for this structure (rx_peerHashTable_lock) */

    int rtt;			/* Smoothed round trip time, measured in milliseconds/8 */
    int rtt_dev;		/* Smoothed rtt mean difference, in milliseconds/4 */
    int nSent;			/* Total number of distinct data packets sent, not including retransmissions */
    int reSends;		/* Total number of retransmissions for this peer, since this structure was created */

    /* the "natural" MTU, excluding IP,UDP headers, is negotiated by the endpoints */
    u_short natMTU;		/* includes rx header */
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
    afs_uint64 bytesSent;	/* Number of bytes sent to this peer */
    afs_uint64 bytesReceived;	/* Number of bytes received from this peer */
    struct opr_queue rpcStats;	/* rpc statistic list */
    int lastReachTime;		/* Last time we verified reachability */
    afs_int32 maxPacketSize;    /* Max size we sent that got acked (w/o hdrs) */
#ifdef AFS_RXERRQ_ENV
    rx_atomic_t neterrs;

    /* protected by peer_lock */
    int last_err_origin; /* origin of the last network error received */
    int last_err_type; /* last error 'type' received */
    int last_err_code; /* last error 'code' received */
#endif
};

#endif
