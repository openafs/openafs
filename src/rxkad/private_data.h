/* Declarations of data structures associated with rxkad security objects. */

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/* Copyright (C) 1991, 1990 Transarc Corporation - All rights reserved */


#ifndef TRANSARC_RXKAD_PRIVATE_DATA_H
#define TRANSARC_RXKAD_PRIVATE_DATA_H

#include "rxkad.h"


#include "fcrypt.h"

struct connStats {
    afs_uint32
	bytesReceived, bytesSent, packetsReceived, packetsSent;
};
	
/* Private data structure representing an RX server end point for rxkad.
 * This structure is encrypted in network byte order and transmitted as
 * part of a challenge response.  It is also used as part of the per-packet
 * checksum sent on every packet, to ensure that the per-packet checksum
 * is not used in the context of another end point.
 *
 * THIS STRUCTURE MUST BE A MULTIPLE OF 8 BYTES LONG SINCE IT IS
 * ENCRYPTED IN PLACE!
 */
struct rxkad_endpoint {
    afs_int32 cuid[2];			/* being used for connection routing */
    afs_uint32 cksum;			/* cksum of challenge response */
    afs_int32 securityIndex;			/* security index */
};

/* structure used for generating connection IDs; must be encrypted in network
 * byte order.  Also must be a multiple of 8 bytes for encryption to work
 * properly.
 */
struct rxkad_cidgen {
    struct clock time;	/* time now */
    afs_int32 random1;	/* some implementation-specific random info */
    afs_int32 random2;	/* more random info */
    afs_int32 counter;	/* a counter */
    afs_int32 ipAddr;	/* or an approximation to it */
};

/* private data in client-side security object */
struct rxkad_cprivate {
    afs_int32	   kvno;		/* key version of ticket */
    afs_int32	   ticketLen;		/* length of ticket */
    fc_KeySchedule keysched;		/* the session key */
    fc_InitializationVector ivec;	/* initialization vector for cbc */
    char	   ticket[MAXKTCTICKETLEN]; /* the ticket for the server */
    rxkad_type	   type;		/* always client */
    rxkad_level	   level;		/* minimum security level of client */
};

/* Per connection client-side info */
struct rxkad_cconn {
    fc_InitializationVector preSeq;	/* used in computing checksum */
    struct connStats stats;
    char	     cksumSeen;		/* rx: header.spare is a checksum */
};

/* private data in server-side security object */
struct rxkad_sprivate {
    char       *get_key_rock;		/* rock for get_key function */
    int	      (*get_key)();		/* func. of kvno and server key ptr */
    int	      (*user_ok)();		/* func called with new client name */
    rxkad_type  type;			/* always server */
    rxkad_level level;			/* minimum security level of server */
};

/* private data in server-side connection */
struct rxkad_sconn {
    rxkad_level	     level;		/* security level of connection */
    char	     tried;		/* did we ever try to auth this conn */
    char	     authenticated;	/* connection is good */
    char	     cksumSeen;		/* rx: header.spare is a checksum */
    afs_uint32    expirationTime;	/* when the ticket expires */
    afs_int32	     challengeID;	/* unique challenge */
    struct connStats stats;		/* per connection stats */
    fc_KeySchedule   keysched;		/* session key */
    fc_InitializationVector ivec;	/* initialization vector for cbc */
    fc_InitializationVector preSeq;	/* used in computing checksum */
    struct rxkad_serverinfo *rock;	/* info about client if saved */
};

struct rxkad_serverinfo {
    int kvno;
    struct ktc_principal client;
};

#define RXKAD_CHALLENGE_PROTOCOL_VERSION 2

/* An old style (any version predating 2) challenge packet */
struct rxkad_oldChallenge {
    afs_int32 challengeID;
    afs_int32 level;				/* minimum security level */
};

/* A version 2 challenge */
struct rxkad_v2Challenge {
    afs_int32 version;
    afs_int32 challengeID;
    afs_int32 level;
    afs_int32 spare;
};

/* An old challenge response packet */
struct rxkad_oldChallengeResponse {
    struct { /* encrypted with session key */
	afs_int32 incChallengeID;
	afs_int32 level;
    } encrypted;
    afs_int32 kvno;
    afs_int32 ticketLen;
};
/*  <ticketLen> bytes of ticket follow here */

/* A version 2 challenge response also includes connection routing (Rx server
 * end point) and client call number state as well as version and spare fields.
 * The encrypted part probably doesn't need to start on an 8 byte boundary, but
 * just in case we put in a spare. */
struct rxkad_v2ChallengeResponse {
    afs_int32 version;
    afs_int32 spare;
    struct { /* encrypted with session key */
	struct rxkad_endpoint endpoint;	/* for connection routing */
	afs_int32 callNumbers[RX_MAXCALLS];	/* client call # state */
	afs_int32 incChallengeID;
	afs_int32 level;
    } encrypted;
    afs_int32 kvno;
    afs_int32 ticketLen;
};
/*  <ticketLen> bytes of ticket follow here */
#if RX_MAXCALLS != 4
    The above structure requires that (RX_MAXCALLS == 4).
#endif

/* This should be afs_int32, but the RXS ops are defined as int returning  */
#define rxs_return_t int
extern rxs_return_t
    rxkad_Close(),
    rxkad_NewConnection(),
    rxkad_CheckAuthentication(),
    rxkad_CreateChallenge(),
    rxkad_GetChallenge(),
    rxkad_GetResponse(),
    rxkad_CheckPacket(),
    rxkad_PreparePacket(),
    rxkad_CheckResponse(),
    rxkad_DestroyConnection(),
    rxkad_AllocCID(),
    rxkad_GetStats();

#endif /* TRANSARC_RXKAD_PRIVATE_DATA_H */
