/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* The rxkad security object.  Routines used by both client and servers. */

#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

#ifdef AFS_SUN59_ENV
#include <sys/time_impl.h>
#endif

#define INCLUDE_RXKAD_PRIVATE_DECLS

RCSID
    ("$Header$");

#ifdef KERNEL
#ifndef UKERNEL
#include "afs/stds.h"
#include "afs/afs_osi.h"
#if defined(AFS_AIX_ENV) || defined(AFS_AUX_ENV) || defined(AFS_SUN5_ENV) 
#include "h/systm.h"
#endif
#if defined(AFS_DARWIN60_ENV) || defined(AFS_OBSD_ENV)
#include "h/kernel.h"
#endif
#include "h/types.h"
#include "h/time.h"
#ifndef AFS_LINUX22_ENV
#include "rpc/types.h"
#include "rx/xdr.h"
#endif /* AFS_LINUX22_ENV */
#else /* !UKERNEL */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#endif /* !UKERNEL */
#include "rx/rx.h"

#else /* KERNEL */
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
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <afs/afsutil.h>
#endif /* KERNEL */

#include <des/stats.h>
#include "private_data.h"
#define XPRT_RXKAD_COMMON

#ifndef afs_max
#define	afs_max(a,b)    ((a) < (b)? (b) : (a))
#endif /* afs_max */

#ifndef KERNEL
#define osi_Time() time(0)
#endif
/* variable initialization for the benefit of darwin compiler; if it causes
   problems elsewhere, conditionalize for darwin or fc_test compile breaks */
#ifdef AFS_PTHREAD_ENV
struct rxkad_global_stats rxkad_global_stats = { 0 };
pthread_mutex_t rxkad_global_stats_lock;
pthread_key_t rxkad_stats_key;
#else /* AFS_PTHREAD_ENV */
#if defined(KERNEL) && !defined(UKERNEL)
struct rxkad_stats rxkad_stats = { { 0 } }; 
#else
/* Move delaration of this to des/key_sched.c */
#endif
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_PTHREAD_ENV
/* rxkad_stats related stuff */

/*
 * Macro to insert an element at the tail of a doubly linked list
 */
#define DLL_INSERT_TAIL(ptr,head,tail,next,prev) \
    do {					 \
	(ptr)->next = NULL;			 \
        (ptr)->prev = (tail);			 \
	(tail) = (ptr);				 \
	if ((ptr)->prev) 			 \
	    (ptr)->prev->next = (ptr);		 \
	else					 \
	    (head) = (ptr);			 \
	assert((head) && ((head)->prev == NULL)); \
    } while(0)

void rxkad_global_stats_init() {
    assert(pthread_mutex_init(&rxkad_global_stats_lock, (const pthread_mutexattr_t *)0) == 0);
    assert(pthread_key_create(&rxkad_stats_key, NULL) == 0);
    memset(&rxkad_global_stats, 0, sizeof(rxkad_global_stats));
}

rxkad_stats_t * 
rxkad_thr_stats_init() {
    rxkad_stats_t * rxkad_stats;
    rxkad_stats = (rxkad_stats_t *)malloc(sizeof(rxkad_stats_t));
    assert(rxkad_stats != NULL && pthread_setspecific(rxkad_stats_key,rxkad_stats) == 0);
    memset(rxkad_stats,0,sizeof(rxkad_stats_t));
    RXKAD_GLOBAL_STATS_LOCK;
    DLL_INSERT_TAIL(rxkad_stats, rxkad_global_stats.first, rxkad_global_stats.last, next, prev);
    RXKAD_GLOBAL_STATS_UNLOCK;
    return rxkad_stats;
}

int rxkad_stats_agg(rxkad_stats_t * rxkad_stats) {
    rxkad_stats_t * thr_stats;
    assert(rxkad_stats != NULL);
    memset(rxkad_stats, 0, sizeof(rxkad_stats_t));
    RXKAD_GLOBAL_STATS_LOCK;
    for (thr_stats = rxkad_global_stats.first; thr_stats != NULL; thr_stats = thr_stats->next) {
        rxkad_stats->connections[0] += thr_stats->connections[0];
	rxkad_stats->connections[1] += thr_stats->connections[1];
	rxkad_stats->connections[2] += thr_stats->connections[2];
	rxkad_stats->destroyObject += thr_stats->destroyObject;
	rxkad_stats->destroyClient += thr_stats->destroyClient;
	rxkad_stats->destroyUnused += thr_stats->destroyUnused;
	rxkad_stats->destroyUnauth += thr_stats->destroyUnauth;
	rxkad_stats->destroyConn[0] += thr_stats->destroyConn[0];
	rxkad_stats->destroyConn[1] += thr_stats->destroyConn[1];
	rxkad_stats->destroyConn[2] += thr_stats->destroyConn[2];
	rxkad_stats->expired += thr_stats->expired;
	rxkad_stats->challengesSent += thr_stats->challengesSent;
	rxkad_stats->challenges[0] += thr_stats->challenges[0];
	rxkad_stats->challenges[1] += thr_stats->challenges[1];
	rxkad_stats->challenges[2] += thr_stats->challenges[2];
	rxkad_stats->responses[0] += thr_stats->responses[0];
	rxkad_stats->responses[1] += thr_stats->responses[1];
	rxkad_stats->responses[2] += thr_stats->responses[2];
	rxkad_stats->preparePackets[0] += thr_stats->preparePackets[0];
	rxkad_stats->preparePackets[1] += thr_stats->preparePackets[1];
	rxkad_stats->preparePackets[2] += thr_stats->preparePackets[2];
	rxkad_stats->preparePackets[3] += thr_stats->preparePackets[3];
	rxkad_stats->preparePackets[4] += thr_stats->preparePackets[4];
	rxkad_stats->preparePackets[5] += thr_stats->preparePackets[5];
	rxkad_stats->checkPackets[0] += thr_stats->checkPackets[0];
	rxkad_stats->checkPackets[1] += thr_stats->checkPackets[1];
	rxkad_stats->checkPackets[2] += thr_stats->checkPackets[2];
	rxkad_stats->checkPackets[3] += thr_stats->checkPackets[3];
	rxkad_stats->checkPackets[4] += thr_stats->checkPackets[4];
	rxkad_stats->checkPackets[5] += thr_stats->checkPackets[5];
	rxkad_stats->bytesEncrypted[0] += thr_stats->bytesEncrypted[0];
	rxkad_stats->bytesEncrypted[1] += thr_stats->bytesEncrypted[1];
	rxkad_stats->bytesDecrypted[0] += thr_stats->bytesDecrypted[0];
	rxkad_stats->bytesDecrypted[1] += thr_stats->bytesDecrypted[1];
	rxkad_stats->fc_encrypts[0] += thr_stats->fc_encrypts[0];
	rxkad_stats->fc_encrypts[1] += thr_stats->fc_encrypts[1];
	rxkad_stats->fc_key_scheds += thr_stats->fc_key_scheds;
	rxkad_stats->des_encrypts[0] += thr_stats->des_encrypts[0];
	rxkad_stats->des_encrypts[1] += thr_stats->des_encrypts[1];
	rxkad_stats->des_key_scheds += thr_stats->des_key_scheds;
	rxkad_stats->des_randoms += thr_stats->des_randoms;
	rxkad_stats->spares[0] += thr_stats->spares[0];
	rxkad_stats->spares[1] += thr_stats->spares[1];
	rxkad_stats->spares[2] += thr_stats->spares[2];
	rxkad_stats->spares[3] += thr_stats->spares[3];
	rxkad_stats->spares[4] += thr_stats->spares[4];
	rxkad_stats->spares[5] += thr_stats->spares[5];
	rxkad_stats->spares[6] += thr_stats->spares[6];
	rxkad_stats->spares[7] += thr_stats->spares[7];
	rxkad_stats->spares[8] += thr_stats->spares[8];
	rxkad_stats->spares[9] += thr_stats->spares[9];
    }
    RXKAD_GLOBAL_STATS_UNLOCK;
    return 0;
}
#endif /* AFS_PTHREAD_ENV */

/* static prototypes */
static afs_int32 ComputeSum(struct rx_packet *apacket,
			    fc_KeySchedule * aschedule, afs_int32 * aivec);
static afs_int32 FreeObject(struct rx_securityClass *aobj);

/* this call sets up an endpoint structure, leaving it in *network* byte
 * order so that it can be used quickly for encryption.
 */
int
rxkad_SetupEndpoint(struct rx_connection *aconnp,
		    struct rxkad_endpoint *aendpointp)
{
    register afs_int32 i;

    aendpointp->cuid[0] = htonl(aconnp->epoch);
    i = aconnp->cid & RX_CIDMASK;
    aendpointp->cuid[1] = htonl(i);
    aendpointp->cksum = 0;	/* used as cksum only in chal resp. */
    aendpointp->securityIndex = htonl(aconnp->securityIndex);
    return 0;
}

/* setup xor information based on session key */
int
rxkad_DeriveXORInfo(struct rx_connection *aconnp, fc_KeySchedule * aschedule,
		    char *aivec, char *aresult)
{
    struct rxkad_endpoint tendpoint;
    afs_uint32 xor[2];

    rxkad_SetupEndpoint(aconnp, &tendpoint);
    memcpy((void *)xor, aivec, 2 * sizeof(afs_int32));
    fc_cbc_encrypt(&tendpoint, &tendpoint, sizeof(tendpoint), aschedule, xor,
		   ENCRYPT);
    memcpy(aresult,
	   ((char *)&tendpoint) + sizeof(tendpoint) - ENCRYPTIONBLOCKSIZE,
	   ENCRYPTIONBLOCKSIZE);
    return 0;
}

/* rxkad_CksumChallengeResponse - computes a checksum of the components of a
 * challenge response packet (which must be unencrypted and in network order).
 * The endpoint.cksum field is omitted and treated as zero.  The cksum is
 * returned in network order. */

afs_uint32
rxkad_CksumChallengeResponse(struct rxkad_v2ChallengeResponse * v2r)
{
    int i;
    afs_uint32 cksum;
    u_char *cp = (u_char *) v2r;
    afs_uint32 savedCksum = v2r->encrypted.endpoint.cksum;

    v2r->encrypted.endpoint.cksum = 0;

    /* this function captured from budb/db_hash.c */
    cksum = 1000003;
    for (i = 0; i < sizeof(*v2r); i++)
	cksum = (*cp++) + cksum * 0x10204081;

    v2r->encrypted.endpoint.cksum = savedCksum;
    return htonl(cksum);
}

void
rxkad_SetLevel(struct rx_connection *conn, rxkad_level level)
{
    if (level == rxkad_auth) {
	rx_SetSecurityHeaderSize(conn, 4);
	rx_SetSecurityMaxTrailerSize(conn, 4);
    } else if (level == rxkad_crypt) {
	rx_SetSecurityHeaderSize(conn, 8);
	rx_SetSecurityMaxTrailerSize(conn, 8);	/* XXX was 7, but why screw with 
						 * unaligned accesses? */
    }
}

/* returns a short integer in host byte order representing a good checksum of
 * the packet header.
 */
static afs_int32
ComputeSum(struct rx_packet *apacket, fc_KeySchedule * aschedule,
	   afs_int32 * aivec)
{
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
    if (t == 0)
	t = 1;			/* so that 0 means don't care */
    return t;
}


static afs_int32
FreeObject(struct rx_securityClass *aobj)
{
    struct rxkad_cprivate *tcp;	/* both structs start w/ type field */

    if (aobj->refCount > 0)
	return 0;		/* still in use */
    tcp = (struct rxkad_cprivate *)aobj->privateData;
    rxi_Free(aobj, sizeof(struct rx_securityClass));
    if (tcp->type & rxkad_client) {
	afs_int32 psize = PDATA_SIZE(tcp->ticketLen);
	rxi_Free(tcp, psize);
    } else if (tcp->type & rxkad_server) {
	rxi_Free(tcp, sizeof(struct rxkad_sprivate));
    } else {
	return RXKADINCONSISTENCY;
    }				/* unknown type */
    INC_RXKAD_STATS(destroyObject);
    return 0;
}

/* rxkad_Close - called by rx with the security class object as a parameter
 * when a security object is to be discarded */

int
rxkad_Close(struct rx_securityClass *aobj)
{
    afs_int32 code;
    aobj->refCount--;
    code = FreeObject(aobj);
    return code;
}

/* either: called to (re)create a new connection. */

int
rxkad_NewConnection(struct rx_securityClass *aobj,
		    struct rx_connection *aconn)
{
    if (aconn->securityData)
	return RXKADINCONSISTENCY;	/* already allocated??? */

    if (rx_IsServerConn(aconn)) {
	int size = sizeof(struct rxkad_sconn);
	aconn->securityData = (char *)rxi_Alloc(size);
	memset(aconn->securityData, 0, size);	/* initialize it conveniently */
    } else {			/* client */
	struct rxkad_cprivate *tcp;
	struct rxkad_cconn *tccp;
	int size = sizeof(struct rxkad_cconn);
	tccp = (struct rxkad_cconn *)rxi_Alloc(size);
	aconn->securityData = (char *)tccp;
	memset(aconn->securityData, 0, size);	/* initialize it conveniently */
	tcp = (struct rxkad_cprivate *)aobj->privateData;
	if (!(tcp->type & rxkad_client))
	    return RXKADINCONSISTENCY;
	rxkad_SetLevel(aconn, tcp->level);	/* set header and trailer sizes */
	rxkad_AllocCID(aobj, aconn);	/* CHANGES cid AND epoch!!!! */
	rxkad_DeriveXORInfo(aconn, tcp->keysched, tcp->ivec, tccp->preSeq);
	INC_RXKAD_STATS(connections[rxkad_LevelIndex(tcp->level)]);
    }

    aobj->refCount++;		/* attached connection */
    return 0;
}

/* either: called to destroy a connection. */

int
rxkad_DestroyConnection(struct rx_securityClass *aobj,
			struct rx_connection *aconn)
{
    if (rx_IsServerConn(aconn)) {
	struct rxkad_sconn *sconn;
	struct rxkad_serverinfo *rock;
	sconn = (struct rxkad_sconn *)aconn->securityData;
	if (sconn) {
	    aconn->securityData = 0;
	    if (sconn->authenticated)
		INC_RXKAD_STATS(destroyConn[rxkad_LevelIndex(sconn->level)]);
	    else
		INC_RXKAD_STATS(destroyUnauth);
	    rock = sconn->rock;
	    if (rock)
		rxi_Free(rock, sizeof(struct rxkad_serverinfo));
	    rxi_Free(sconn, sizeof(struct rxkad_sconn));
	} else {
	    INC_RXKAD_STATS(destroyUnused);
	}
    } else {			/* client */
	struct rxkad_cconn *cconn;
	struct rxkad_cprivate *tcp;
	cconn = (struct rxkad_cconn *)aconn->securityData;
	tcp = (struct rxkad_cprivate *)aobj->privateData;
	if (!(tcp->type & rxkad_client))
	    return RXKADINCONSISTENCY;
	if (cconn) {
	    aconn->securityData = 0;
	    rxi_Free(cconn, sizeof(struct rxkad_cconn));
	}
	INC_RXKAD_STATS(destroyClient);
    }
    aobj->refCount--;		/* decrement connection counter */
    if (aobj->refCount <= 0) {
	afs_int32 code;
	code = FreeObject(aobj);
	if (code)
	    return code;
    }
    return 0;
}

/* either: decode packet */

int
rxkad_CheckPacket(struct rx_securityClass *aobj, struct rx_call *acall,
		  struct rx_packet *apacket)
{
    struct rx_connection *tconn;
    rxkad_level level;
    fc_KeySchedule *schedule;
    fc_InitializationVector *ivec;
    int len;
    int nlen = 0;
    u_int word;			/* so we get unsigned right-shift */
    int checkCksum;
    afs_int32 *preSeq;
    afs_int32 code;

    tconn = rx_ConnectionOf(acall);
    len = rx_GetDataSize(apacket);
    checkCksum = 0;		/* init */
    if (rx_IsServerConn(tconn)) {
	struct rxkad_sconn *sconn;
	sconn = (struct rxkad_sconn *)tconn->securityData;
	if (rx_GetPacketCksum(apacket) != 0)
	    sconn->cksumSeen = 1;
	checkCksum = sconn->cksumSeen;
	if (sconn && sconn->authenticated
	    && (osi_Time() < sconn->expirationTime)) {
	    level = sconn->level;
	    INC_RXKAD_STATS(checkPackets[rxkad_StatIndex(rxkad_server, level)]);
	    sconn->stats.packetsReceived++;
	    sconn->stats.bytesReceived += len;
	    schedule = (fc_KeySchedule *) sconn->keysched;
	    ivec = (fc_InitializationVector *) sconn->ivec;
	} else {
	    INC_RXKAD_STATS(expired);
	    return RXKADEXPIRED;
	}
	preSeq = sconn->preSeq;
    } else {			/* client connection */
	struct rxkad_cconn *cconn;
	struct rxkad_cprivate *tcp;
	cconn = (struct rxkad_cconn *)tconn->securityData;
	if (rx_GetPacketCksum(apacket) != 0)
	    cconn->cksumSeen = 1;
	checkCksum = cconn->cksumSeen;
	tcp = (struct rxkad_cprivate *)aobj->privateData;
	if (!(tcp->type & rxkad_client))
	    return RXKADINCONSISTENCY;
	level = tcp->level;
	INC_RXKAD_STATS(checkPackets[rxkad_StatIndex(rxkad_client, level)]);
	cconn->stats.packetsReceived++;
	cconn->stats.bytesReceived += len;
	preSeq = cconn->preSeq;
	schedule = (fc_KeySchedule *) tcp->keysched;
	ivec = (fc_InitializationVector *) tcp->ivec;
    }

    if (checkCksum) {
	code = ComputeSum(apacket, schedule, preSeq);
	if (code != rx_GetPacketCksum(apacket))
	    return RXKADSEALEDINCON;
    }

    switch (level) {
    case rxkad_clear:
	return 0;		/* shouldn't happen */
    case rxkad_auth:
	rx_Pullup(apacket, 8);	/* the following encrypts 8 bytes only */
	fc_ecb_encrypt(rx_DataOf(apacket), rx_DataOf(apacket), schedule,
		       DECRYPT);
	break;
    case rxkad_crypt:
	code = rxkad_DecryptPacket(tconn, schedule, ivec, len, apacket);
	if (code)
	    return code;
	break;
    }
    word = ntohl(rx_GetInt32(apacket, 0));	/* get first sealed word */
    if ((word >> 16) !=
	((apacket->header.seq ^ apacket->header.callNumber) & 0xffff))
	return RXKADSEALEDINCON;
    nlen = word & 0xffff;	/* get real user data length */

    /* The sealed length should be no larger than the initial length, since the  
     * reverse (round-up) occurs in ...PreparePacket */
    if (nlen > len)
	return RXKADDATALEN;
    rx_SetDataSize(apacket, nlen);
    return 0;
}

/* either: encode packet */

int
rxkad_PreparePacket(struct rx_securityClass *aobj, struct rx_call *acall,
		    struct rx_packet *apacket)
{
    struct rx_connection *tconn;
    rxkad_level level;
    fc_KeySchedule *schedule;
    fc_InitializationVector *ivec;
    int len;
    int nlen = 0;
    int word;
    afs_int32 code;
    afs_int32 *preSeq;

    tconn = rx_ConnectionOf(acall);
    len = rx_GetDataSize(apacket);
    if (rx_IsServerConn(tconn)) {
	struct rxkad_sconn *sconn;
	sconn = (struct rxkad_sconn *)tconn->securityData;
	if (sconn && sconn->authenticated
	    && (osi_Time() < sconn->expirationTime)) {
	    level = sconn->level;
	    INC_RXKAD_STATS(preparePackets[rxkad_StatIndex(rxkad_server, level)]);
	    sconn->stats.packetsSent++;
	    sconn->stats.bytesSent += len;
	    schedule = (fc_KeySchedule *) sconn->keysched;
	    ivec = (fc_InitializationVector *) sconn->ivec;
	} else {
	    INC_RXKAD_STATS(expired);	/* this is a pretty unlikely path... */
	    return RXKADEXPIRED;
	}
	preSeq = sconn->preSeq;
    } else {			/* client connection */
	struct rxkad_cconn *cconn;
	struct rxkad_cprivate *tcp;
	cconn = (struct rxkad_cconn *)tconn->securityData;
	tcp = (struct rxkad_cprivate *)aobj->privateData;
	if (!(tcp->type & rxkad_client))
	    return RXKADINCONSISTENCY;
	level = tcp->level;
	INC_RXKAD_STATS(preparePackets[rxkad_StatIndex(rxkad_client, level)]);
	cconn->stats.packetsSent++;
	cconn->stats.bytesSent += len;
	preSeq = cconn->preSeq;
	schedule = (fc_KeySchedule *) tcp->keysched;
	ivec = (fc_InitializationVector *) tcp->ivec;
    }

    /* compute upward compatible checksum */
    rx_SetPacketCksum(apacket, ComputeSum(apacket, schedule, preSeq));
    if (level == rxkad_clear)
	return 0;

    len = rx_GetDataSize(apacket);
    word = (((apacket->header.seq ^ apacket->header.callNumber)
	     & 0xffff) << 16) | (len & 0xffff);
    rx_PutInt32(apacket, 0, htonl(word));

    switch (level) {
    case rxkad_clear:
	return 0;		/* shouldn't happen */
    case rxkad_auth:
	nlen =
	    afs_max(ENCRYPTIONBLOCKSIZE,
		    len + rx_GetSecurityHeaderSize(tconn));
	if (nlen > (len + rx_GetSecurityHeaderSize(tconn))) {
	    rxi_RoundUpPacket(apacket,
			      nlen - (len + rx_GetSecurityHeaderSize(tconn)));
	}
	rx_Pullup(apacket, 8);	/* the following encrypts 8 bytes only */
	fc_ecb_encrypt(rx_DataOf(apacket), rx_DataOf(apacket), schedule,
		       ENCRYPT);
	break;
    case rxkad_crypt:
	nlen = round_up_to_ebs(len + rx_GetSecurityHeaderSize(tconn));
	if (nlen > (len + rx_GetSecurityHeaderSize(tconn))) {
	    rxi_RoundUpPacket(apacket,
			      nlen - (len + rx_GetSecurityHeaderSize(tconn)));
	}
	code = rxkad_EncryptPacket(tconn, schedule, ivec, nlen, apacket);
	if (code)
	    return code;
	break;
    }
    rx_SetDataSize(apacket, nlen);
    return 0;
}

/* either: return connection stats */

int
rxkad_GetStats(struct rx_securityClass *aobj, struct rx_connection *aconn,
	       struct rx_securityObjectStats *astats)
{
    astats->type = 3;
    astats->level = ((struct rxkad_cprivate *)aobj->privateData)->level;
    if (!aconn->securityData) {
	astats->flags |= 1;
	return 0;
    }
    if (rx_IsServerConn(aconn)) {
	struct rxkad_sconn *sconn;
	sconn = (struct rxkad_sconn *)aconn->securityData;
	astats->level = sconn->level;
	if (sconn->authenticated)
	    astats->flags |= 2;
	if (sconn->cksumSeen)
	    astats->flags |= 8;
	astats->expires = sconn->expirationTime;
	astats->bytesReceived = sconn->stats.bytesReceived;
	astats->packetsReceived = sconn->stats.packetsReceived;
	astats->bytesSent = sconn->stats.bytesSent;
	astats->packetsSent = sconn->stats.packetsSent;
    } else {			/* client connection */
	struct rxkad_cconn *cconn;
	cconn = (struct rxkad_cconn *)aconn->securityData;
	if (cconn->cksumSeen)
	    astats->flags |= 8;
	astats->bytesReceived = cconn->stats.bytesReceived;
	astats->packetsReceived = cconn->stats.packetsReceived;
	astats->bytesSent = cconn->stats.bytesSent;
	astats->packetsSent = cconn->stats.packetsSent;
    }
    return 0;
}

rxkad_level
rxkad_StringToLevel(char *name)
{
    if (strcmp(name, "clear") == 0)
	return rxkad_clear;
    if (strcmp(name, "auth") == 0)
	return rxkad_auth;
    if (strcmp(name, "crypt") == 0)
	return rxkad_crypt;
    return -1;
}

char *
rxkad_LevelToString(rxkad_level level)
{
    if (level == rxkad_clear)
	return "clear";
    if (level == rxkad_auth)
	return "auth";
    if (level == rxkad_crypt)
	return "crypt";
    return "unknown";
}
