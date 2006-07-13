/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Declarations of data structures associated with rxkad security objects. */

#ifndef RXKAD_PRIVATE_DATA_H
#define RXKAD_PRIVATE_DATA_H

#include "rxkad.h"


#include "fcrypt.h"

struct connStats {
    afs_uint32 bytesReceived, bytesSent, packetsReceived, packetsSent;
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
    afs_int32 cuid[2];		/* being used for connection routing */
    afs_uint32 cksum;		/* cksum of challenge response */
    afs_int32 securityIndex;	/* security index */
};

/* structure used for generating connection IDs; must be encrypted in network
 * byte order.  Also must be a multiple of 8 bytes for encryption to work
 * properly.
 */
struct rxkad_cidgen {
    struct clock time;		/* time now */
    afs_int32 random1;		/* some implementation-specific random info */
    afs_int32 random2;		/* more random info */
    afs_int32 counter;		/* a counter */
    afs_int32 ipAddr;		/* or an approximation to it */
};

#define PDATA_SIZE(l) (sizeof(struct rxkad_cprivate) - MAXKTCTICKETLEN + (l))

/* private data in client-side security object */
/* type and level offsets should match sprivate */
struct rxkad_cprivate {
    rxkad_type type;		/* always client */
    rxkad_level level;		/* minimum security level of client */
    afs_int32 kvno;		/* key version of ticket */
    afs_int16 ticketLen;	/* length of ticket */
    fc_KeySchedule keysched;	/* the session key */
    fc_InitializationVector ivec;	/* initialization vector for cbc */
    char ticket[MAXKTCTICKETLEN];	/* the ticket for the server */
};

/* Per connection client-side info */
struct rxkad_cconn {
    fc_InitializationVector preSeq;	/* used in computing checksum */
    struct connStats stats;
    char cksumSeen;		/* rx: header.spare is a checksum */
};

/* private data in server-side security object */
/* type and level offsets should match cprivate */
struct rxkad_sprivate {
    rxkad_type type;		/* always server */
    rxkad_level level;		/* minimum security level of server */
    char *get_key_rock;		/* rock for get_key function */
    int (*get_key) ();		/* func. of kvno and server key ptr */
    int (*user_ok) ();		/* func called with new client name */
};

/* private data in server-side connection */
struct rxkad_sconn {
    rxkad_level level;		/* security level of connection */
    char tried;			/* did we ever try to auth this conn */
    char authenticated;		/* connection is good */
    char cksumSeen;		/* rx: header.spare is a checksum */
    afs_uint32 expirationTime;	/* when the ticket expires */
    afs_int32 challengeID;	/* unique challenge */
    struct connStats stats;	/* per connection stats */
    fc_KeySchedule keysched;	/* session key */
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
    afs_int32 level;		/* minimum security level */
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
    struct {			/* encrypted with session key */
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
    struct {			/* encrypted with session key */
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
The above structure requires
that(RX_MAXCALLS == 4).
#endif
#endif /* RXKAD_PRIVATE_DATA_H */
