/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* The rxkad security object.  Authentication using a DES-encrypted
 * Kerberos-style ticket.  These are the server-only routines. */


#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>

#if (defined(AFS_AIX_ENV) && defined(KERNEL) && !defined(UKERNEL)) || defined(AFS_AUX_ENV) || defined(AFS_SUN5_ENV)
#include <sys/systm.h>
#endif

#include <afs/opr.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rx_packet.h>
#include <afs/afsutil.h>

#include "stats.h"
#include "private_data.h"
#define XPRT_RXKAD_SERVER

/*
 * This can be set to allow alternate ticket decoding.
 * Currently only used by the AFS/DFS protocol translator to recognize
 * Kerberos V5 tickets. The actual code to do that is provided externally.
 */
afs_int32(*rxkad_AlternateTicketDecoder) (afs_int32, char *, afs_int32,
		                          char *, char *, char *,
					  struct ktc_encryptionKey *,
					  afs_int32 *, afs_uint32 *,
					  afs_uint32 *);

static struct rx_securityOps rxkad_server_ops = {
    AFS_STRUCT_INIT(.op_Close,		rxkad_Close),
    AFS_STRUCT_INIT(.op_NewConnection,	rxkad_NewConnection),
    AFS_STRUCT_INIT(.op_PreparePacket,	rxkad_PreparePacket),	/* once per packet creation */
    AFS_STRUCT_INIT(.op_SendPacket,	NULL),			/* send packet (once per retrans) */
    AFS_STRUCT_INIT(.op_CheckAuthentication, rxkad_CheckAuthentication),
    AFS_STRUCT_INIT(.op_CreateChallenge, rxkad_CreateChallenge),
    AFS_STRUCT_INIT(.op_GetChallenge,	rxkad_GetChallenge),
    AFS_STRUCT_INIT(.op_GetResponse,	NULL),
    AFS_STRUCT_INIT(.op_CheckResponse,	rxkad_CheckResponse),
    AFS_STRUCT_INIT(.op_CheckPacket,	rxkad_CheckPacket),	/* check data packet */
    AFS_STRUCT_INIT(.op_DestroyConnection, rxkad_DestroyConnection),
    AFS_STRUCT_INIT(.op_GetStats,	rxkad_GetStats),
    AFS_STRUCT_INIT(.op_SetConfiguration, rxkad_SetConfiguration),
    AFS_STRUCT_INIT(.op_Spare2,		NULL),			/* spare 2 */
    AFS_STRUCT_INIT(.op_Spare3,		NULL),			/* spare 3 */
};
extern afs_uint32 rx_MyMaxSendSize;

/* Miscellaneous random number routines that use the fcrypt module and the
 * timeofday. */

static fc_KeySchedule random_int32_schedule;

#ifdef AFS_PTHREAD_ENV
/*
 * This mutex protects the following global variables:
 * random_int32_schedule
 * seed
 */

pthread_mutex_t rxkad_random_mutex
#ifdef PTHREAD_MUTEX_INITIALIZER
= PTHREAD_MUTEX_INITIALIZER
#endif
;
#define LOCK_RM opr_Verify(pthread_mutex_lock(&rxkad_random_mutex)==0)
#define UNLOCK_RM opr_Verify(pthread_mutex_unlock(&rxkad_random_mutex)==0)
#else
#define LOCK_RM
#define UNLOCK_RM
#endif /* AFS_PTHREAD_ENV */

static void
init_random_int32(void)
{
    struct timeval key;

    gettimeofday(&key, NULL);
    LOCK_RM;
    fc_keysched((struct ktc_encryptionKey*)&key, random_int32_schedule);
    UNLOCK_RM;
}

static afs_int32
get_random_int32(void)
{
    static struct timeval seed;
    afs_int32 rc;

    LOCK_RM;
    fc_ecb_encrypt(&seed, &seed, random_int32_schedule, ENCRYPT);
    rc = seed.tv_sec;
    UNLOCK_RM;
    return rc;
}

/* Called with four parameters.  The first is the level of encryption, as
   defined in the rxkad.h file.  The second and third are a rock and a
   procedure that is called with the key version number that accompanies the
   ticket and returns a pointer to the server's decryption key.  The fourth
   argument, if not NULL, is a pointer to a function that will be called for
   every new connection with the name, instance and cell of the client.  The
   routine should return zero if the user is NOT acceptible to the server.  If
   this routine is not supplied, the server can call rxkad_GetServerInfo with
   the rx connection pointer passed to the RPC routine to obtain information
   about the client. */

/*
  rxkad_level      level;		* minimum level *
  char		  *get_key_rock;	* rock for get_key implementor *
  int		 (*get_key)();		* passed kvno & addr(key) to fill *
  int		 (*user_ok)();		* passed name, inst, cell => bool *
*/

struct rx_securityClass *
rxkad_NewServerSecurityObject(rxkad_level level, void *get_key_rock,
			      int (*get_key) (void *get_key_rock, int kvno,
					      struct ktc_encryptionKey *
					      serverKey),
			      int (*user_ok) (char *name, char *instance,
					      char *cell, afs_int32 kvno))
{
    struct rx_securityClass *tsc;
    struct rxkad_sprivate *tsp;
    int size;

    rxkad_Init();

    if (!get_key)
	return 0;

    size = sizeof(struct rx_securityClass);
    tsc = rxi_Alloc(size);
    memset(tsc, 0, size);
    rxs_SetRefs(tsc, 1);		/* caller has one reference */
    tsc->ops = &rxkad_server_ops;
    size = sizeof(struct rxkad_sprivate);
    tsp = rxi_Alloc(size);
    memset(tsp, 0, size);
    tsc->privateData = (char *)tsp;

    tsp->type |= rxkad_server;	/* so can identify later */
    tsp->level = level;		/* level of encryption */
    tsp->get_key_rock = get_key_rock;
    tsp->get_key = get_key;	/* to get server ticket */
    tsp->user_ok = user_ok;	/* to inform server of client id. */
    init_random_int32();

    INC_RXKAD_STATS(serverObjects);
    return tsc;
}

struct rx_securityClass *
rxkad_NewKrb5ServerSecurityObject(rxkad_level level, void *get_key_rock,
				  int (*get_key) (void *get_key_rock, int kvno,
						  struct ktc_encryptionKey *
						  serverKey),
				  rxkad_get_key_enctype_func get_key_enctype,
				  int (*user_ok) (char *name, char *instance,
						  char *cell, afs_int32 kvno)
) {
    struct rx_securityClass *tsc;
    struct rxkad_sprivate *tsp;
    tsc = rxkad_NewServerSecurityObject(level, get_key_rock, get_key, user_ok);
    tsp = (struct rxkad_sprivate *)tsc->privateData;
    tsp->get_key_enctype = get_key_enctype;
    return tsc;
}

/* server: called to tell if a connection authenticated properly */

int
rxkad_CheckAuthentication(struct rx_securityClass *aobj,
			  struct rx_connection *aconn)
{
    struct rxkad_sconn *sconn = rx_GetSecurityData(aconn);

    /* first make sure the object exists */
    if (!sconn)
	return RXKADINCONSISTENCY;

    return !sconn->authenticated;
}

/* server: put the current challenge in the connection structure for later use
   by packet sender */

int
rxkad_CreateChallenge(struct rx_securityClass *aobj,
		      struct rx_connection *aconn)
{
    struct rxkad_sconn *sconn = rx_GetSecurityData(aconn);
    struct rxkad_sprivate *tsp;

    sconn->challengeID = get_random_int32();
    sconn->authenticated = 0;	/* conn unauth. 'til we hear back */
    /* initialize level from object's minimum acceptable level */
    tsp = (struct rxkad_sprivate *)aobj->privateData;
    sconn->level = tsp->level;
    return 0;
}

/* server: fill in a challenge in the packet */

int
rxkad_GetChallenge(struct rx_securityClass *aobj, struct rx_connection *aconn,
		   struct rx_packet *apacket)
{
    struct rxkad_sconn *sconn = rx_GetSecurityData(aconn);
    char *challenge;
    int challengeSize;
    struct rxkad_v2Challenge c_v2;	/* version 2 */
    struct rxkad_oldChallenge c_old;	/* old style */

    if (rx_IsUsingPktCksum(aconn))
	sconn->cksumSeen = 1;

    if (sconn->cksumSeen) {
	memset(&c_v2, 0, sizeof(c_v2));
	c_v2.version = htonl(RXKAD_CHALLENGE_PROTOCOL_VERSION);
	c_v2.challengeID = htonl(sconn->challengeID);
	c_v2.level = htonl((afs_int32) sconn->level);
	c_v2.spare = 0;
	challenge = (char *)&c_v2;
	challengeSize = sizeof(c_v2);
    } else {
	memset(&c_old, 0, sizeof(c_old));
	c_old.challengeID = htonl(sconn->challengeID);
	c_old.level = htonl((afs_int32) sconn->level);
	challenge = (char *)&c_old;
	challengeSize = sizeof(c_old);
    }
    if (rx_MyMaxSendSize < challengeSize)
	return RXKADPACKETSHORT;	/* not enough space */

    rx_packetwrite(apacket, 0, challengeSize, challenge);
    rx_SetDataSize(apacket, challengeSize);
    sconn->tried = 1;
    INC_RXKAD_STATS(challengesSent);
    return 0;
}

/* server: process a response to a challenge packet */
/* XXX this does some copying of data in and out of the packet, but I'll bet it
 * could just do it in place, especially if I used rx_Pullup...
 */
int
rxkad_CheckResponse(struct rx_securityClass *aobj,
		    struct rx_connection *aconn, struct rx_packet *apacket)
{
    struct rxkad_sconn *sconn;
    struct rxkad_sprivate *tsp;
    struct ktc_encryptionKey serverKey;
    struct rxkad_oldChallengeResponse oldr;	/* response format */
    struct rxkad_v2ChallengeResponse v2r;
    afs_int32 tlen;		/* ticket len */
    afs_int32 kvno;		/* key version of ticket */
    char tix[MAXKTCTICKETLEN];
    afs_int32 incChallengeID;
    rxkad_level level;
    int code;
    /* ticket contents */
    struct ktc_principal client;
    struct ktc_encryptionKey sessionkey;
    afs_int32 host;
    afs_uint32 start;
    afs_uint32 end;
    unsigned int pos;
    struct rxkad_serverinfo *rock;

    sconn = rx_GetSecurityData(aconn);
    tsp = (struct rxkad_sprivate *)aobj->privateData;

    if (sconn->cksumSeen) {
	/* expect v2 response, leave fields in v2r in network order for cksum
	 * computation which follows decryption. */
	if (rx_GetDataSize(apacket) < sizeof(v2r))
	    return RXKADPACKETSHORT;
	rx_packetread(apacket, 0, sizeof(v2r), &v2r);
	pos = sizeof(v2r);
	/* version == 2 */
	/* ignore spare */
	kvno = ntohl(v2r.kvno);
	tlen = ntohl(v2r.ticketLen);
	if (rx_GetDataSize(apacket) < sizeof(v2r) + tlen)
	    return RXKADPACKETSHORT;
    } else {
	/* expect old format response */
	if (rx_GetDataSize(apacket) < sizeof(oldr))
	    return RXKADPACKETSHORT;
	rx_packetread(apacket, 0, sizeof(oldr), &oldr);
	pos = sizeof(oldr);

	kvno = ntohl(oldr.kvno);
	tlen = ntohl(oldr.ticketLen);
	if (rx_GetDataSize(apacket) != sizeof(oldr) + tlen)
	    return RXKADPACKETSHORT;
    }
    if ((tlen < MINKTCTICKETLEN) || (tlen > MAXKTCTICKETLEN))
	return RXKADTICKETLEN;

    rx_packetread(apacket, pos, tlen, tix);	/* get ticket */

    /*
     * We allow the ticket to be optionally decoded by an alternate
     * ticket decoder, if the function variable
     * rxkad_AlternateTicketDecoder is set. That function should
     * return a code of -1 if it wants the ticket to be decoded by
     * the standard decoder.
     */
    if (rxkad_AlternateTicketDecoder) {
	code =
	    rxkad_AlternateTicketDecoder(kvno, tix, tlen, client.name,
					 client.instance, client.cell,
					 &sessionkey, &host, &start, &end);
	if (code && code != -1) {
	    return code;
	}
    } else {
	code = -1;		/* No alternate ticket decoder present */
    }

    /*
     * If the alternate decoder is not present, or returns -1, then
     * assume the ticket is of the default style.
     */
    if (code == -1 && ((kvno == RXKAD_TKT_TYPE_KERBEROS_V5)
	|| (kvno == RXKAD_TKT_TYPE_KERBEROS_V5_ENCPART_ONLY))) {
	code =
	    tkt_DecodeTicket5(tix, tlen, tsp->get_key, tsp->get_key_enctype,
			      tsp->get_key_rock, kvno, client.name,
			      client.instance, client.cell,
			      &sessionkey, &host, &start, &end,
			      tsp->flags & RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
	if (code)
	    return code;
    }

    /*
     * If the alternate decoder/kerberos 5 decoder is not present, or
     * returns -1, then assume the ticket is of the default style.
     */
    if (code == -1) {
	/* get ticket's key */
	code = (*tsp->get_key) (tsp->get_key_rock, kvno, &serverKey);
	if (code)
	    return RXKADUNKNOWNKEY;	/* invalid kvno */
	code =
	    tkt_DecodeTicket(tix, tlen, &serverKey, client.name,
			     client.instance, client.cell, &sessionkey, &host,
			     &start, &end);
	if (code)
	    return code;
    }
    code = tkt_CheckTimes(start, end, time(0));
    if (code == 0)
	return RXKADNOAUTH;
    else if (code == -1)
	return RXKADEXPIRED;
    else if (code < -1)
	return RXKADBADTICKET;

    code = fc_keysched(&sessionkey, sconn->keysched);
    if (code)
	return RXKADBADKEY;
    memcpy(sconn->ivec, &sessionkey, sizeof(sconn->ivec));

    if (sconn->cksumSeen) {
	/* using v2 response */
	afs_uint32 cksum;	/* observed cksum */
	struct rxkad_endpoint endpoint;	/* connections endpoint */
	int i;
	afs_uint32 xor[2];

	memcpy(xor, sconn->ivec, 2 * sizeof(afs_int32));
	fc_cbc_encrypt(&v2r.encrypted, &v2r.encrypted, sizeof(v2r.encrypted),
		       sconn->keysched, xor, DECRYPT);
	cksum = rxkad_CksumChallengeResponse(&v2r);
	if (cksum != v2r.encrypted.endpoint.cksum)
	    return RXKADSEALEDINCON;
	(void)rxkad_SetupEndpoint(aconn, &endpoint);
	v2r.encrypted.endpoint.cksum = 0;
	if (memcmp(&endpoint, &v2r.encrypted.endpoint, sizeof(endpoint)) != 0)
	    return RXKADSEALEDINCON;
	for (i = 0; i < RX_MAXCALLS; i++) {
	    v2r.encrypted.callNumbers[i] =
		ntohl(v2r.encrypted.callNumbers[i]);
	    if (v2r.encrypted.callNumbers[i] < 0)
		return RXKADSEALEDINCON;
	}

	(void)rxi_SetCallNumberVector(aconn, v2r.encrypted.callNumbers);
	incChallengeID = ntohl(v2r.encrypted.incChallengeID);
	level = ntohl(v2r.encrypted.level);
    } else {
	/* expect old format response */
	fc_ecb_encrypt(&oldr.encrypted, &oldr.encrypted, sconn->keysched,
		       DECRYPT);
	incChallengeID = ntohl(oldr.encrypted.incChallengeID);
	level = ntohl(oldr.encrypted.level);
    }
    if (incChallengeID != sconn->challengeID + 1)
	return RXKADOUTOFSEQUENCE;	/* replay attempt */
    if ((level < sconn->level) || (level > rxkad_crypt))
	return RXKADLEVELFAIL;
    sconn->level = level;
    rxkad_SetLevel(aconn, sconn->level);
    INC_RXKAD_STATS(responses[rxkad_LevelIndex(sconn->level)]);
    /* now compute endpoint-specific info used for computing 16 bit checksum */
    rxkad_DeriveXORInfo(aconn, &sconn->keysched, (char *)sconn->ivec, (char *)sconn->preSeq);

    /* otherwise things are ok */
    sconn->expirationTime = end;
    sconn->authenticated = 1;

    if (tsp->user_ok) {
	code = tsp->user_ok(client.name, client.instance, client.cell, kvno);
	if (code)
	    return RXKADNOAUTH;
    } else {			/* save the info for later retreival */
	int size = sizeof(struct rxkad_serverinfo);
	rock = rxi_Alloc(size);
	memset(rock, 0, size);
	rock->kvno = kvno;
	memcpy(&rock->client, &client, sizeof(rock->client));
	sconn->rock = rock;
    }
    return 0;
}

/* return useful authentication info about a server-side connection */

afs_int32
rxkad_GetServerInfo(struct rx_connection * aconn, rxkad_level * level,
		    afs_uint32 * expiration, char *name, char *instance,
		    char *cell, afs_int32 * kvno)
{
    struct rxkad_sconn *sconn;

    sconn = rx_GetSecurityData(aconn);
    if (sconn && sconn->authenticated && sconn->rock
	&& (time(0) < sconn->expirationTime)) {
	if (level)
	    *level = sconn->level;
	if (expiration)
	    *expiration = sconn->expirationTime;
	if (name)
	    strcpy(name, sconn->rock->client.name);
	if (instance)
	    strcpy(instance, sconn->rock->client.instance);
	if (cell)
	    strcpy(cell, sconn->rock->client.cell);
	if (kvno)
	    *kvno = sconn->rock->kvno;
	return 0;
    } else
	return RXKADNOAUTH;
}

/* Set security object configuration variables */
afs_int32 rxkad_SetConfiguration(struct rx_securityClass *aobj,
                                 struct rx_connection *aconn,
                                 rx_securityConfigVariables atype,
		                         void * avalue, void **currentValue)
{
    struct rxkad_sprivate *private =
    (struct rxkad_sprivate *) aobj->privateData;

    switch (atype) {
    case RXS_CONFIG_FLAGS:
        if (currentValue) {
            *((afs_uint32 *)currentValue) = private->flags;
        } else {
            private->flags = (intptr_t)avalue;
        }
        break;
    default:
        break;
    }
    return 0;
}
