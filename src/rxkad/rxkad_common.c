/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* The rxkad security object.  Routines used by both client and servers. */

#ifdef KERNEL
#include "../afs/param.h"
#ifndef UKERNEL
#include "../afs/stds.h"
#include "../afs/afs_osi.h"
#ifdef	AFS_AIX_ENV
#include "../h/systm.h"
#endif
#include "../h/types.h"
#include "../h/time.h"
#ifndef AFS_LINUX22_ENV
#include "../rpc/types.h"
#include "../rpc/xdr.h"
#endif /* AFS_LINUX22_ENV */
#else /* !UKERNEL */
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#endif /* !UKERNEL */
#include "../rx/rx.h"

#else /* KERNEL */
#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#include <time.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#ifdef AFS_PTHREAD_ENV
#define RXKAD_STATS_DECLSPEC __declspec(dllexport)
#endif
#else
#include <netinet/in.h>
#endif
#include <rx/rx.h>
#include <rx/xdr.h>

#endif /* KERNEL */

#include "private_data.h"
#define XPRT_RXKAD_COMMON

char *rxi_Alloc();

#ifndef max
#define	max(a,b)    ((a) < (b)? (b) : (a))
#endif /* max */

#ifndef KERNEL
#define osi_Time() time(0)
#endif
struct rxkad_stats rxkad_stats;

/* this call sets up an endpoint structure, leaving it in *network* byte
 * order so that it can be used quickly for encryption.
 */
rxkad_SetupEndpoint(aconnp, aendpointp)
  IN struct rx_connection *aconnp;
  OUT struct rxkad_endpoint *aendpointp;
{
    register afs_int32 i;

    aendpointp->cuid[0] = htonl(aconnp->epoch);
    i = aconnp->cid & RX_CIDMASK;
    aendpointp->cuid[1] = htonl(i);
    aendpointp->cksum = 0;		/* used as cksum only in chal resp. */
    aendpointp->securityIndex = htonl(aconnp->securityIndex);
    return 0;
}

/* setup xor information based on session key */
rxkad_DeriveXORInfo(aconnp, aschedule, aivec, aresult)
  IN struct rx_connection *aconnp;
  IN fc_KeySchedule *aschedule;
  IN char *aivec;
  OUT char *aresult;
{
    struct rxkad_endpoint tendpoint;
    afs_uint32 xor[2];

    rxkad_SetupEndpoint(aconnp, &tendpoint);
    bcopy(aivec, (void *)xor, 2*sizeof(afs_int32));
    fc_cbc_encrypt(&tendpoint, &tendpoint, sizeof(tendpoint),
		   aschedule, xor, ENCRYPT);
    bcopy(((char *)&tendpoint) + sizeof(tendpoint) - ENCRYPTIONBLOCKSIZE,
	  aresult, ENCRYPTIONBLOCKSIZE);
    return 0;
}

/* rxkad_CksumChallengeResponse - computes a checksum of the components of a
 * challenge response packet (which must be unencrypted and in network order).
 * The endpoint.cksum field is omitted and treated as zero.  The cksum is
 * returned in network order. */

afs_uint32 rxkad_CksumChallengeResponse (v2r)
  IN struct rxkad_v2ChallengeResponse *v2r;
{
    int i;
    afs_uint32 cksum;
    u_char *cp = (u_char *)v2r;
    afs_uint32 savedCksum = v2r->encrypted.endpoint.cksum;

    v2r->encrypted.endpoint.cksum = 0;

    /* this function captured from budb/db_hash.c */
    cksum = 1000003;
    for (i=0; i<sizeof(*v2r); i++)
	cksum = (*cp++) + cksum * 0x10204081;

    v2r->encrypted.endpoint.cksum = savedCksum;
    return htonl(cksum);
}

void rxkad_SetLevel(conn, level)
  struct rx_connection *conn;
  rxkad_level	        level;
{
    if (level == rxkad_auth) {
	rx_SetSecurityHeaderSize (conn, 4);
	rx_SetSecurityMaxTrailerSize (conn, 4);
    }
    else if (level == rxkad_crypt) {
	rx_SetSecurityHeaderSize (conn, 8);
	rx_SetSecurityMaxTrailerSize (conn, 8); /* XXX was 7, but why screw with 
						   unaligned accesses? */
    }
}

/* returns a short integer in host byte order representing a good checksum of
 * the packet header.
 */
static afs_int32 ComputeSum(apacket, aschedule, aivec)
struct rx_packet *apacket;
afs_int32 *aivec;
fc_KeySchedule *aschedule; {
    afs_uint32 word[2];
    register afs_uint32 t;

    t = apacket->header.callNumber;
    word[0] = htonl(t);
    /* note that word [1] includes the channel # */
    t = ((apacket->header.cid & 0x3) << 30)
	    | ((apacket->header.seq & 0x3fffffff));
    word[1] = htonl(t);
    /* XOR in the ivec from the per-endpoint encryption */
    word[0] ^= aivec[0];
    word[1] ^= aivec[1];
    /* encrypts word as if it were a character string */
    fc_ecb_encrypt(word, word, aschedule, ENCRYPT);
    t = ntohl(word[1]);
    t = (t >> 16) & 0xffff;
    if (t == 0) t = 1;	/* so that 0 means don't care */
    return t;
}


static afs_int32 FreeObject (aobj)
  IN struct rx_securityClass *aobj;
{   struct rxkad_cprivate *tcp;		/* both structs start w/ type field */

    if (aobj->refCount > 0) return 0;	/* still in use */
    tcp = (struct rxkad_cprivate *)aobj->privateData;
    rxi_Free(aobj, sizeof(struct rx_securityClass));
    if (tcp->type & rxkad_client) {
	rxi_Free(tcp, sizeof(struct rxkad_cprivate));
    }
    else if (tcp->type & rxkad_server) {
	rxi_Free(tcp, sizeof(struct rxkad_sprivate));
    }
    else { return RXKADINCONSISTENCY; }	/* unknown type */
    LOCK_RXKAD_STATS
    rxkad_stats.destroyObject++;
    UNLOCK_RXKAD_STATS
    return 0;
}

/* rxkad_Close - called by rx with the security class object as a parameter
 * when a security object is to be discarded */

rxs_return_t rxkad_Close (aobj)
  IN struct rx_securityClass *aobj;
{
    afs_int32 code;
    aobj->refCount--;
    code = FreeObject (aobj);
    return code;
}

/* either: called to (re)create a new connection. */

rxs_return_t rxkad_NewConnection (aobj, aconn)
  struct rx_securityClass *aobj;
  struct rx_connection	  *aconn;
{
    if (aconn->securityData)
	return RXKADINCONSISTENCY;	/* already allocated??? */

    if (rx_IsServerConn(aconn)) {
	int size = sizeof(struct rxkad_sconn);
	aconn->securityData = (char *) rxi_Alloc (size);
	bzero(aconn->securityData, size); /* initialize it conveniently */
    }
    else { /* client */
	struct rxkad_cprivate *tcp;
	struct rxkad_cconn *tccp;
	int size = sizeof(struct rxkad_cconn);
	tccp = (struct rxkad_cconn *) rxi_Alloc (size);
	aconn->securityData = (char *) tccp;
	bzero(aconn->securityData, size); /* initialize it conveniently */
	tcp = (struct rxkad_cprivate *) aobj->privateData;
	if (!(tcp->type & rxkad_client)) return RXKADINCONSISTENCY;
	rxkad_SetLevel(aconn, tcp->level); /* set header and trailer sizes */
	rxkad_AllocCID(aobj, aconn);	/* CHANGES cid AND epoch!!!! */
	rxkad_DeriveXORInfo(aconn, tcp->keysched, tcp->ivec, tccp->preSeq);
	LOCK_RXKAD_STATS
	rxkad_stats.connections[rxkad_LevelIndex(tcp->level)]++;
	UNLOCK_RXKAD_STATS
    }

    aobj->refCount++;			/* attached connection */
    return 0;
}

/* either: called to destroy a connection. */

rxs_return_t rxkad_DestroyConnection (aobj, aconn)
  struct rx_securityClass *aobj;
  struct rx_connection	  *aconn;
{
    if (rx_IsServerConn(aconn)) {
	struct rxkad_sconn *sconn;
	struct rxkad_serverinfo *rock;
	sconn = (struct rxkad_sconn *)aconn->securityData;
	if (sconn) {
	    aconn->securityData = 0;
	    LOCK_RXKAD_STATS
	    if (sconn->authenticated)
		rxkad_stats.destroyConn[rxkad_LevelIndex(sconn->level)]++;
	    else rxkad_stats.destroyUnauth++;
	    UNLOCK_RXKAD_STATS
	    rock = sconn->rock;
	    if (rock) rxi_Free (rock, sizeof(struct rxkad_serverinfo));
	    rxi_Free (sconn, sizeof(struct rxkad_sconn));
	}
	else {
	    LOCK_RXKAD_STATS
	    rxkad_stats.destroyUnused++;
	    UNLOCK_RXKAD_STATS
	}
    }
    else {				/* client */
	struct rxkad_cconn *cconn;
	struct rxkad_cprivate *tcp;
	cconn = (struct rxkad_cconn *)aconn->securityData;
	tcp = (struct rxkad_cprivate *) aobj->privateData;
	if (!(tcp->type & rxkad_client)) return RXKADINCONSISTENCY;
	if (cconn) {
	    aconn->securityData = 0;
	    rxi_Free (cconn, sizeof(struct rxkad_cconn));
	}
	LOCK_RXKAD_STATS
	rxkad_stats.destroyClient++;
	UNLOCK_RXKAD_STATS
    }
    aobj->refCount--;			/* decrement connection counter */
    if (aobj->refCount <= 0) {
	afs_int32 code;
	code = FreeObject (aobj);
	if (code) return code;
    }
    return 0;
}

/* either: decode packet */

rxs_return_t rxkad_CheckPacket (aobj, acall, apacket)
  struct rx_securityClass *aobj;
  struct rx_call	  *acall;
  struct rx_packet	  *apacket;
{   struct rx_connection  *tconn;
    rxkad_level	           level;
    fc_KeySchedule *schedule;
    fc_InitializationVector *ivec;
    int len;
    int nlen;
    u_int word;				/* so we get unsigned right-shift */
    int checkCksum;
    afs_int32 *preSeq;
    afs_int32 code;

    tconn = rx_ConnectionOf(acall);
    len = rx_GetDataSize (apacket);
    checkCksum = 0;			/* init */
    if (rx_IsServerConn(tconn)) {
	struct rxkad_sconn *sconn;
	sconn = (struct rxkad_sconn *) tconn->securityData;
	if (rx_GetPacketCksum(apacket) != 0) sconn->cksumSeen = 1;
	checkCksum = sconn->cksumSeen;
	if (sconn && sconn->authenticated &&
	    (osi_Time() < sconn->expirationTime)) {
	    level = sconn->level;
	    LOCK_RXKAD_STATS
	    rxkad_stats.checkPackets[rxkad_StatIndex(rxkad_server, level)]++;
	    UNLOCK_RXKAD_STATS
	    sconn->stats.packetsReceived++;
	    sconn->stats.bytesReceived += len;
	    schedule = (fc_KeySchedule *)sconn->keysched;
	    ivec = (fc_InitializationVector *)sconn->ivec;
	}
	else {
	    LOCK_RXKAD_STATS
	    rxkad_stats.expired++;
	    UNLOCK_RXKAD_STATS
	    return RXKADEXPIRED;
	}
	preSeq = sconn->preSeq;
    }
    else {				/* client connection */
	struct rxkad_cconn *cconn;
	struct rxkad_cprivate *tcp;
	cconn = (struct rxkad_cconn *) tconn->securityData;
	if (rx_GetPacketCksum(apacket) != 0) cconn->cksumSeen = 1;
	checkCksum = cconn->cksumSeen;
	tcp = (struct rxkad_cprivate *) aobj->privateData;
	if (!(tcp->type & rxkad_client)) return RXKADINCONSISTENCY;
	level = tcp->level;
	LOCK_RXKAD_STATS
	rxkad_stats.checkPackets[rxkad_StatIndex(rxkad_client, level)]++;
	UNLOCK_RXKAD_STATS
	cconn->stats.packetsReceived++;
	cconn->stats.bytesReceived += len;
	preSeq = cconn->preSeq;
	schedule = (fc_KeySchedule *)tcp->keysched;
	ivec = (fc_InitializationVector *)tcp->ivec;
    }
    
    if (checkCksum) {
	code = ComputeSum(apacket, schedule, preSeq);
	if (code != rx_GetPacketCksum(apacket))
	    return RXKADSEALEDINCON;
    }

    switch (level) {
      case rxkad_clear: return 0;	/* shouldn't happen */
      case rxkad_auth:
	rx_Pullup(apacket, 8);  /* the following encrypts 8 bytes only */
	fc_ecb_encrypt (rx_DataOf(apacket), rx_DataOf(apacket),
			schedule, DECRYPT);
	break;
      case rxkad_crypt:
	code = rxkad_DecryptPacket (tconn, schedule, ivec, len, apacket);
	if (code) return code;
	break;
    }
    word = ntohl(rx_GetInt32(apacket,0)); /* get first sealed word */
    if ((word >> 16) !=
	((apacket->header.seq ^ apacket->header.callNumber) & 0xffff))
	return RXKADSEALEDINCON;
    nlen = word & 0xffff;		/* get real user data length */

    /* The sealed length should be no larger than the initial length, since the  
     * reverse (round-up) occurs in ...PreparePacket */
    if (nlen > len)                     
      return RXKADDATALEN;              
    rx_SetDataSize (apacket, nlen);
    return 0;
}

/* either: encode packet */

rxs_return_t rxkad_PreparePacket (aobj, acall, apacket)
  struct rx_securityClass *aobj;
  struct rx_call *acall;
  struct rx_packet *apacket;
{
    struct rx_connection *tconn;
    rxkad_level	        level;
    fc_KeySchedule *schedule;
    fc_InitializationVector *ivec;
    int len;
    int nlen;
    int word;
    afs_int32 code;
    afs_int32 *preSeq;

    tconn = rx_ConnectionOf(acall);
    len = rx_GetDataSize (apacket);
    if (rx_IsServerConn(tconn)) {
	struct rxkad_sconn *sconn;
	sconn = (struct rxkad_sconn *) tconn->securityData;
	if (sconn && sconn->authenticated &&
	    (osi_Time() < sconn->expirationTime)) {
	    level = sconn->level;
	    LOCK_RXKAD_STATS
	    rxkad_stats.preparePackets[rxkad_StatIndex(rxkad_server, level)]++;
	    UNLOCK_RXKAD_STATS
	    sconn->stats.packetsSent++;
	    sconn->stats.bytesSent += len;
	    schedule = (fc_KeySchedule *)sconn->keysched;
	    ivec = (fc_InitializationVector *)sconn->ivec;
	}
	else {
	    LOCK_RXKAD_STATS
	    rxkad_stats.expired++;	/* this is a pretty unlikely path... */
	    UNLOCK_RXKAD_STATS
	    return RXKADEXPIRED;
	}
	preSeq = sconn->preSeq;
    }
    else {				/* client connection */
	struct rxkad_cconn *cconn;
	struct rxkad_cprivate *tcp;
	cconn = (struct rxkad_cconn *) tconn->securityData;
	tcp = (struct rxkad_cprivate *) aobj->privateData;
	if (!(tcp->type & rxkad_client)) return RXKADINCONSISTENCY;
	level = tcp->level;
	LOCK_RXKAD_STATS
	rxkad_stats.preparePackets[rxkad_StatIndex(rxkad_client, level)]++;
	UNLOCK_RXKAD_STATS
	cconn->stats.packetsSent++;
	cconn->stats.bytesSent += len;
	preSeq = cconn->preSeq;
	schedule = (fc_KeySchedule *)tcp->keysched;
	ivec = (fc_InitializationVector *)tcp->ivec;
    }

    /* compute upward compatible checksum */
    rx_SetPacketCksum(apacket, ComputeSum(apacket, schedule, preSeq));
    if (level == rxkad_clear) return 0;

    len = rx_GetDataSize (apacket);
    word = (((apacket->header.seq ^ apacket->header.callNumber)
	     & 0xffff) << 16) | (len & 0xffff);
    rx_PutInt32(apacket,0, htonl(word));   

    switch (level) {
      case rxkad_clear: return 0;	/* shouldn't happen */
      case rxkad_auth:
	nlen = max (ENCRYPTIONBLOCKSIZE,
		    len + rx_GetSecurityHeaderSize(tconn));
	if (nlen > (len + rx_GetSecurityHeaderSize(tconn))) {
	  rxi_RoundUpPacket(apacket, nlen - (len + rx_GetSecurityHeaderSize(tconn)));
	}
	rx_Pullup(apacket, 8);  /* the following encrypts 8 bytes only */
	fc_ecb_encrypt (rx_DataOf(apacket), rx_DataOf(apacket),
			schedule, ENCRYPT);
	break;
      case rxkad_crypt:
	nlen = round_up_to_ebs(len + rx_GetSecurityHeaderSize(tconn));
	if (nlen > (len + rx_GetSecurityHeaderSize(tconn))) {
	  rxi_RoundUpPacket(apacket, nlen - (len + rx_GetSecurityHeaderSize(tconn)));
	}
	code = rxkad_EncryptPacket (tconn, schedule, ivec, nlen, apacket);
	if (code) return code;
	break;
    }
    rx_SetDataSize (apacket, nlen);
    return 0;
}

/* either: return connection stats */

rxs_return_t rxkad_GetStats (aobj, aconn, astats)
  IN struct rx_securityClass *aobj;
  IN struct rx_connection *aconn;
  OUT struct rx_securityObjectStats *astats;
{
    astats->type = 3;
    astats->level = ((struct rxkad_cprivate *)aobj->privateData)->level;
    if (!aconn->securityData) {
	astats->flags |= 1;
	return 0;
    }
    if (rx_IsServerConn(aconn)) {
	struct rxkad_sconn *sconn;
	sconn = (struct rxkad_sconn *) aconn->securityData;
	astats->level = sconn->level;
	if (sconn->authenticated) astats->flags |= 2;
	if (sconn->cksumSeen) astats->flags |= 8;
	astats->expires = sconn->expirationTime;
	astats->bytesReceived = sconn->stats.bytesReceived;
	astats->packetsReceived = sconn->stats.packetsReceived;
	astats->bytesSent = sconn->stats.bytesSent;
	astats->packetsSent = sconn->stats.packetsSent;
    }
    else { /* client connection */
	struct rxkad_cconn *cconn;
	cconn = (struct rxkad_cconn *) aconn->securityData;
	if (cconn->cksumSeen) astats->flags |= 8;
	astats->bytesReceived = cconn->stats.bytesReceived;
	astats->packetsReceived = cconn->stats.packetsReceived;
	astats->bytesSent = cconn->stats.bytesSent;
	astats->packetsSent = cconn->stats.packetsSent;
    }
    return 0;
}
