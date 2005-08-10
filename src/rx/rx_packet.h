/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _RX_PACKET_
#define _RX_PACKET_
#ifndef UKERNEL
#if defined(AFS_NT40_ENV) || defined(AFS_DJGPP_ENV)
#include "rx_xmit_nt.h"
#endif
#ifndef AFS_NT40_ENV
#include <sys/uio.h>
#endif /* !AFS_NT40_ENV */
#endif /* !UKERNEL */
/* this file includes the macros and decls which depend on packet
 * format, and related packet manipulation macros.  Note that code
 * which runs at NETPRI should not sleep, or AIX will panic */
/* There are some assumptions that various code makes -- I'll try to 
 * express them all here: 
 * 1.  rx_ReceiveAckPacket assumes that it can get an entire ack
 * contiguous in the first iovec.  As a result, the iovec buffers must
 * be >= sizeof (struct rx_ackpacket)
 * 2. All callers of rx_Pullup besides rx_ReceiveAckPacket try to pull
 * up less data than rx_ReceiveAckPacket does.
 * 3. rx_GetInt32 and rx_PutInt32 (and the slow versions of same) assume
 * that the iovec buffers are all integral multiples of the word size,
 * and that the offsets are as well.
 */


#if defined(AFS_NT40_ENV) || defined(AFS_DJGPP_ENV)
#ifndef MIN
#define MIN(a,b)  ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)  ((a)>(b)?(a):(b))
#endif
#else /* AFS_NT40_ENV */
#if !defined(AFS_DARWIN_ENV) && !defined(AFS_USR_DARWIN_ENV) && !defined(AFS_XBSD_ENV) && !defined(AFS_USR_FBSD_ENV) && !defined(AFS_LINUX20_ENV)
#include <sys/sysmacros.h>	/* MIN, MAX on Solaris */
#endif
#include <sys/param.h>		/* MIN, MAX elsewhere */
#endif /* AFS_NT40_ENV */

#define	IPv6_HDR_SIZE		40	/* IPv6 Header */
#define IPv6_FRAG_HDR_SIZE	 8	/* IPv6 Fragment Header */
#define UDP_HDR_SIZE             8	/* UDP Header */
#define	RX_IP_SIZE		(IPv6_HDR_SIZE + IPv6_FRAG_HDR_SIZE)
#define	RX_IPUDP_SIZE		(RX_IP_SIZE + UDP_HDR_SIZE)

/* REMOTE_PACKET_SIZE is currently the same as local.  This is because REMOTE
 * is defined much too generally for my tastes, and includes the case of 
 * multiple class C nets connected with a router within one campus or MAN. 
 * I don't want to make local performance suffer just because of some
 * out-dated protocol that used to be in use on the NSFANET that's
 * practically unused anymore.  Any modern IP implementation will be
 * using MTU discovery, and even old routers shouldn't frag packets
 * when sending from one connected network directly to another.  Maybe
 * the next release of RX will do MTU discovery. */

/* MTUXXX the various "MAX" params here must be rationalized.  From now on,
 * the MAX packet size will be the maximum receive size, but the maximum send
 * size will be larger than that. */

#ifdef notdef
/*  some sample MTUs 
           4352   what FDDI(RFC1188) uses... Larger? 
           4096   VJ's recommendation for FDDI 
          17914   what IBM 16MB TR  uses   
           8166   IEEE 802.4 
           4464   IEEE 802.5 MAX
           2002   IEEE 802.5 Recommended 
	   1500   what Ethernet uses 
	   1492   what 802.3 uses ( 8 bytes for 802.2 SAP )
	   9180   Classical IP over ATM (RFC2225)
*/

/* * * * these are the old defines
*/
#define	RX_MAX_PACKET_SIZE	(RX_MAX_DL_MTU -RX_IPUDP_SIZE)

#define	RX_MAX_PACKET_DATA_SIZE	(RX_MAX_PACKET_SIZE-RX_HEADER_SIZE)
#ifdef AFS_HPUX_ENV
/* HPUX by default uses an 802.3 size, and it's not evident from SIOCGIFCONF */
#define	RX_LOCAL_PACKET_SIZE	(1492 - RX_IPUDP_SIZE)
#define	RX_REMOTE_PACKET_SIZE	(1492 - RX_IPUDP_SIZE)
#else
#define	RX_LOCAL_PACKET_SIZE	RX_MAX_PACKET_SIZE	/* For hosts on same net */
#define	RX_REMOTE_PACKET_SIZE	RX_MAX_PACKET_SIZE	/* see note above */
#endif
#endif /* notdef */

/* These are the new, streamlined ones.
 */
#define	RX_HEADER_SIZE		sizeof (struct rx_header)

/* The minimum MTU for an IP network is 576 bytes including headers */
#define RX_MIN_PACKET_SIZE      (576 - RX_IPUDP_SIZE)
#define	RX_PP_PACKET_SIZE	RX_MIN_PACKET_SIZE

#define	OLD_MAX_PACKET_SIZE	(1500 - RX_IPUDP_SIZE)

/* if the other guy is not on the local net, use this size */
#define	RX_REMOTE_PACKET_SIZE	(1500 - RX_IPUDP_SIZE)

/* for now, never send more data than this */
#define	RX_MAX_PACKET_SIZE	16384
#define	RX_MAX_PACKET_DATA_SIZE	(16384 - RX_HEADER_SIZE)

/* Packet types, for rx_packet.type */
#define	RX_PACKET_TYPE_DATA	    1	/* A vanilla data packet */
#define	RX_PACKET_TYPE_ACK	    2	/* Acknowledge packet */
#define	RX_PACKET_TYPE_BUSY	    3	/* Busy: can't accept call immediately; try later */
#define	RX_PACKET_TYPE_ABORT	    4	/* Abort packet.  No response needed. */
#define	RX_PACKET_TYPE_ACKALL	    5	/* Acknowledges receipt of all packets */
#define	RX_PACKET_TYPE_CHALLENGE    6	/* Challenge client's identity: request credentials */
#define	RX_PACKET_TYPE_RESPONSE	    7	/* Respond to challenge packet */
#define	RX_PACKET_TYPE_DEBUG	    8	/* Get debug information */

#define RX_PACKET_TYPE_PARAMS       9	/* exchange size params (showUmine) */
#define RX_PACKET_TYPE_VERSION	   13	/* get AFS version */


#define	RX_PACKET_TYPES	    {"data", "ack", "busy", "abort", "ackall", "challenge", "response", "debug", "params", "unused", "unused", "unused", "version"}
#define	RX_N_PACKET_TYPES	    13	/* Must agree with above list;
					 * counts 0
					 * WARNING: if this number ever
					 * grows past 13, rxdebug packets
					 * will need to be modified */

/* Packet classes, for rx_AllocPacket */
#define	RX_PACKET_CLASS_RECEIVE	    0
#define	RX_PACKET_CLASS_SEND	    1
#define	RX_PACKET_CLASS_SPECIAL	    2
#define	RX_PACKET_CLASS_RECV_CBUF   3
#define	RX_PACKET_CLASS_SEND_CBUF   4

#define	RX_N_PACKET_CLASSES	    5	/* Must agree with above list */

/* Flags for rx_header flags field */
#define	RX_CLIENT_INITIATED	1	/* Packet is sent/received from client side of call */
#define	RX_REQUEST_ACK		2	/* Peer requests acknowledgement */
#define	RX_LAST_PACKET		4	/* This is the last packet from this side of the call */
#define	RX_MORE_PACKETS		8	/* There are more packets following this,
					 * i.e. the next sequence number seen by
					 * the receiver should be greater than
					 * this one, rather than a resend of an
					 * earlier sequence number */
#define RX_SLOW_START_OK	32	/* Set this flag in an ack packet to
					 * inform the sender that slow start is
					 * supported by the receiver. */
#define RX_JUMBO_PACKET         32	/* Set this flag in a data packet to
					 * indicate that more packets follow
					 * this packet in the datagram */

/* The following flags are preset per packet, i.e. they don't change
 * on retransmission of the packet */
#define	RX_PRESET_FLAGS		(RX_CLIENT_INITIATED | RX_LAST_PACKET)


/*
 * Flags for the packet structure itself, housekeeping for the
 * most part.  These live in rx_packet->flags.
 */
#define	RX_PKTFLAG_ACKED	0x01
#define	RX_PKTFLAG_FREE		0x02


/* The rx part of the header of a packet, in host form */
struct rx_header {
    afs_uint32 epoch;		/* Start time of client process */
    afs_uint32 cid;		/* Connection id (defined by client) */
    afs_uint32 callNumber;	/* Current call number */
    afs_uint32 seq;		/* Sequence number of this packet, within this call */
    afs_uint32 serial;		/* Serial number of this packet: a new serial
				 * number is stamped on each packet sent out */
    u_char type;		/* RX packet type */
    u_char flags;		/* Flags, defined below */
    u_char userStatus;		/* User defined status information,
				 * returned/set by macros
				 * rx_Get/SetLocal/RemoteStatus */
    u_char securityIndex;	/* Which service-defined security method to use */
    u_short serviceId;		/* service this packet is directed _to_ */
    /* This spare is now used for packet header checkksum.  see
     * rxi_ReceiveDataPacket and packet cksum macros above for details. */
    u_short spare;
};

/* The abbreviated header for jumbo packets. Most fields in the
 * jumbo packet headers are either the same as or can be quickly
 * derived from their counterparts in the main packet header.
 */
struct rx_jumboHeader {
    u_char flags;		/* Flags, defined below */
    u_char spare1;
    u_short cksum;		/* packet header checksum */
};

/* For most Unixes, maximum elements in an iovec is 16 */
#define RX_MAXIOVECS 16		/* limit for ReadvProc/WritevProc */
#define RX_MAXWVECS RX_MAXIOVECS-1	/* need one iovec for packet header */

/*
 * The values for the RX buffer sizes are calculated to ensure efficient
 * use of network resources when sending AFS 3.5 jumbograms over Ethernet,
 * 802.3, FDDI, and ATM networks running IPv4 or IPv6. Changing these
 * values may affect interoperability with AFS 3.5 clients.
 */

/*
 * We always transmit jumbo grams so that each packet starts at the
 * beginning of a packet buffer. Because of the requirement that all
 * segments of a 3.4a jumbogram contain multiples of eight bytes, the
 * receivers iovec has RX_HEADERSIZE bytes in the first element,
 * RX_FIRSTBUFFERSIZE bytes in the second element, and RX_CBUFFERSIZE
 * bytes in each successive entry.  All packets in a jumbogram
 * except for the last must contain RX_JUMBOBUFFERSIZE bytes of data
 * so the receiver can split the AFS 3.5 jumbograms back into packets
 * without having to copy any of the data.
 */
#define RX_JUMBOBUFFERSIZE 1412
#define RX_JUMBOHEADERSIZE 4
/*
 * RX_FIRSTBUFFERSIZE must be larger than the largest ack packet, 
 * the largest possible challenge or response packet. 
 * Both Firstbuffersize and cbuffersize must be integral multiples of 8,
 * so the security header and trailer stuff works for rxkad_crypt.  yuck.
 */
#define RX_FIRSTBUFFERSIZE (RX_JUMBOBUFFERSIZE+RX_JUMBOHEADERSIZE)
/*
 * The size of a continuation buffer is buffer is the same as the
 * size of the first buffer, which must also the size of a jumbo packet
 * buffer plus the size of a jumbo packet header. */
#define RX_CBUFFERSIZE (RX_JUMBOBUFFERSIZE+RX_JUMBOHEADERSIZE)
/*
 * Add an extra four bytes of slop at the end of each buffer.
 */
#define RX_EXTRABUFFERSIZE 4

struct rx_packet {
    struct rx_queue queueItemHeader;	/* Packets are chained using the queue.h package */
    struct clock retryTime;	/* When this packet should NEXT be re-transmitted */
    struct clock timeSent;	/* When this packet was transmitted last */
    afs_uint32 firstSerial;	/* Original serial number of this packet */
    struct clock firstSent;	/* When this packet was transmitted first */
    struct rx_header header;	/* The internal packet header */
    unsigned int niovecs;
    struct iovec wirevec[RX_MAXWVECS + 1];	/* the new form of the packet */

    u_char flags;		/* Flags for local state of this packet */
    u_char backoff;		/* for multiple re-sends */
    u_short length;		/* Data length */
    /* NT port relies on the fact that the next two are physically adjacent.
     * If that assumption changes change sendmsg and recvmsg in rx_xmit_nt.c .
     * The jumbo datagram code also relies on the next two being
     * physically adjacent.
     * The Linux port uses this knowledge as well in osi_NetSend.
     */
    afs_uint32 wirehead[RX_HEADER_SIZE / sizeof(afs_int32)];
    afs_uint32 localdata[RX_CBUFFERSIZE / sizeof(afs_int32)];
    afs_uint32 extradata[RX_EXTRABUFFERSIZE / sizeof(afs_int32)];
};

/* Macro to convert continuation buffer pointers to packet pointers */
#define RX_CBUF_TO_PACKET(CP, PP) \
    ((struct rx_packet *) \
     ((char *)(CP) - ((char *)(&(PP)->localdata[0])-(char *)(PP))))

/* Macros callable by security modules, to set header/trailer lengths,
 * set actual packet size, and find the beginning of the security
 * header (or data) */
#define rx_SetSecurityHeaderSize(conn, length) ((conn)->securityHeaderSize = (length))
#define rx_SetSecurityMaxTrailerSize(conn, length) ((conn)->securityMaxTrailerSize = (length))
#define rx_GetSecurityHeaderSize(conn) ((conn)->securityHeaderSize)
#define rx_GetSecurityMaxTrailerSize(conn) ((conn)->securityMaxTrailerSize)

/* This is the address of the data portion of the packet.  Any encryption
 * headers will be at this address, the actual data, for a data packet, will
 * start at this address + the connection's security header size. */
#define	rx_DataOf(packet)		((char *) (packet)->wirevec[1].iov_base)
#define	rx_GetDataSize(packet)		((packet)->length)
#define	rx_SetDataSize(packet, size)	((packet)->length = (size))

/* These macros used in conjunction with reuse of packet header spare as a
 * packet cksum for rxkad security module. */
#define rx_GetPacketCksum(packet)	 ((packet)->header.spare)
#define rx_SetPacketCksum(packet, cksum) ((packet)->header.spare = (cksum))

#ifdef KERNEL
#define rxi_OverQuota(packetclass) (rx_nFreePackets - 1 < rx_packetQuota[packetclass])
#define rxi_OverQuota2(packetclass,num_alloc) (rx_nFreePackets - (num_alloc) < rx_packetQuota[packetclass])
#endif /* KERNEL */

/* this returns an afs_int32 from byte offset o in packet p.  offset must
 * always be aligned properly for an afs_int32, I'm leaving this up to the
 * caller. */
#define rx_GetInt32(p,off) (( (off) >= (p)->wirevec[1].iov_len) ? \
   rx_SlowGetInt32((p), (off)) :  \
  *((afs_int32 *)((char *)((p)->wirevec[1].iov_base) + (off))))

#define rx_PutInt32(p,off,b) { \
       if ((off) >= (p)->wirevec[1].iov_len) \
	  rx_SlowPutInt32((p), (off), (b));   \
       else *((afs_int32 *)((char *)((p)->wirevec[1].iov_base) + (off))) = b; }

#define rx_data(p, o, l) ((l=((struct rx_packet*)(p))->wirevec[(o+1)].iov_len),\
  (((struct rx_packet*)(p))->wirevec[(o+1)].iov_base))


/* copy data into an RX packet */
#define rx_packetwrite(p, off, len, in)               \
  ( (off) + (len) > (p)->wirevec[1].iov_len ?         \
    rx_SlowWritePacket(p, off, len, (char*)(in)) :             \
    ((memcpy((char*)((p)->wirevec[1].iov_base)+(off), (char *)(in), (len))),0))

/* copy data from an RX packet */
#define rx_packetread(p, off, len, out)               \
  ( (off) + (len) > (p)->wirevec[1].iov_len ?         \
    rx_SlowReadPacket(p, off, len, (char*)(out)) :             \
    ((memcpy((char *)(out), (char*)((p)->wirevec[1].iov_base)+(off), (len))),0))

#define rx_computelen(p,l) { register int i; \
   for (l=0, i=1; i < p->niovecs; i++ ) l += p->wirevec[i].iov_len; }

/* return what the actual contiguous space is: should be min(length,size) */
/* The things that call this really want something like ...pullup MTUXXX  */
#define rx_Contiguous(p) \
    MIN((unsigned) (p)->length, (unsigned) ((p)->wirevec[1].iov_len))

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* === packet-ized down to here, the following macros work temporarily */
/* Unfortunately, they know that the cbuf stuff isn't there. */

/* try to ensure that rx_DataOf will return a contiguous space at
 * least size bytes long */
/* return what the actual contiguous space is: should be min(length,size) */
#define rx_Pullup(p,size)	/* this idea here is that this will make a guarantee */


/* The offset of the actual user's data in the packet, skipping any
 * security header */
/* DEPRECATED */
#define	rx_UserDataOf(conn, packet)	(((char *) (packet)->wirevec[1].iov_base) + (conn)->securityHeaderSize)

#endif /* _RX_PACKET_ */
