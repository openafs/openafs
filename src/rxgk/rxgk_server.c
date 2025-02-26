/* rxgk/rxgk_server.c - server-specific security object routines */
/*
 * Copyright (C) 2013, 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Server-specific security object routines.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>

#include <afs/opr.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rx_packet.h>
#include <rx/rxgk.h>

#include "rxgk_private.h"

/*
 * Decrement the reference count on the security object secobj.
 * If the reference count falls to zero, release the underlying storage.
 */
static void
obj_rele(struct rx_securityClass *secobj)
{
    struct rxgk_sprivate *sp;

    if (rxs_DecRef(secobj) > 0) {
	/* still in use */
	return;
    }

    sp = secobj->privateData;
    rxi_Free(secobj, sizeof(*secobj));
    rxi_Free(sp, sizeof(*sp));
    return;
}

/* Release a server security object. */
static int
rxgk_ServerClose(struct rx_securityClass *aobj)
{
    obj_rele(aobj);
    return 0;
}

/* Set fields in 'sc' to invalid/uninitialized values, so we don't accidentally
 * use blank/zeroed values later. */
static void
sconn_set_noauth(struct rxgk_sconn *sc)
{
    rxgk_release_key(&sc->k0);
    if (sc->client != NULL)
        rx_identity_free(&sc->client);
    sc->start_time = 0;
    sc->auth = 0;

    /*
     * The values here should never be seen; set some bogus values. For
     * 'expiration' and 'level', values of 0 are not bogus, so we explicitly
     * set some nonzero values that are sure to be invalid, just in case they
     * get used.
     */
    sc->expiration = 1;
    sc->level = RXGK_LEVEL_BOGUS;
}

/*
 * Create a new rx connection on this given server security object.
 */
static int
rxgk_NewServerConnection(struct rx_securityClass *aobj,
			 struct rx_connection *aconn)
{
    struct rxgk_sconn *sc;

    if (rx_GetSecurityData(aconn) != NULL)
	goto error;

    sc = rxi_Alloc(sizeof(*sc));
    if (sc == NULL)
	goto error;

    sconn_set_noauth(sc);
    rx_SetSecurityData(aconn, sc);
    rxs_Ref(aobj);
    return 0;

 error:
    return RXGK_INCONSISTENCY;
}

/*
 * Server-specific packet preparation routine. All the interesting bits are in
 * rxgk_packet.c; all we have to do here is extract data from the security data
 * on the connection and use the proper key usage.
 */
static int
rxgk_ServerPreparePacket(struct rx_securityClass *aobj, struct rx_call *acall,
			 struct rx_packet *apacket)
{
    struct rxgk_sconn *sc;
    struct rx_connection *aconn;
    rxgk_key tk;
    afs_uint32 lkvno;
    afs_uint16 wkvno, len;
    int ret;

    aconn = rx_ConnectionOf(acall);
    sc = rx_GetSecurityData(aconn);

    if (sc->expiration < RXGK_NOW() && sc->expiration != RXGK_NEVERDATE)
	return RXGK_EXPIRED;

    len = rx_GetDataSize(apacket);
    lkvno = sc->key_number;
    sc->stats.psent++;
    sc->stats.bsent += len;
    wkvno = (afs_uint16)lkvno;
    rx_SetPacketCksum(apacket, wkvno);

    if (sc->level == RXGK_LEVEL_CLEAR)
	return 0;

    ret = rxgk_derive_tk(&tk, sc->k0, rx_GetConnectionEpoch(aconn),
			 rx_GetConnectionId(aconn), sc->start_time, lkvno);
    if (ret != 0)
	return ret;

    switch(sc->level) {
	case RXGK_LEVEL_AUTH:
	    ret = rxgk_mic_packet(tk, RXGK_SERVER_MIC_PACKET, aconn, apacket);
	    break;
	case RXGK_LEVEL_CRYPT:
	    ret = rxgk_enc_packet(tk, RXGK_SERVER_ENC_PACKET, aconn, apacket);
	    break;
	default:
	    ret = RXGK_INCONSISTENCY;
	    break;
    }

    rxgk_release_key(&tk);
    return ret;
}

/* Did a connection properly authenticate? */
static int
rxgk_CheckAuthentication(struct rx_securityClass *aobj,
			 struct rx_connection *aconn)
{
    struct rxgk_sconn *sc;

    sc = rx_GetSecurityData(aconn);
    if (sc == NULL)
	return RXGK_INCONSISTENCY;

    if (sc->auth == 0)
	return RXGK_NOTAUTH;

    return 0;
}

/* Generate a challenge to be used later. */
static int
rxgk_CreateChallenge(struct rx_securityClass *aobj,
		     struct rx_connection *aconn)
{
    struct rxgk_sconn *sc;
    struct rx_opaque buf = RX_EMPTY_OPAQUE;
    opr_StaticAssert(sizeof(sc->challenge) == RXGK_CHALLENGE_NONCE_LEN);

    sc = rx_GetSecurityData(aconn);
    if (sc == NULL)
	return RXGK_INCONSISTENCY;
    sc->auth = 0;

    /* The challenge is a 20-byte random nonce. */
    if (rxgk_nonce(&buf, RXGK_CHALLENGE_NONCE_LEN) != 0)
	return RXGK_INCONSISTENCY;

    opr_Assert(buf.len == RXGK_CHALLENGE_NONCE_LEN);
    memcpy(&sc->challenge, buf.val, RXGK_CHALLENGE_NONCE_LEN);
    rx_opaque_freeContents(&buf);
    sc->challenge_valid = 1;
    return 0;
}

/*
 * Read the challenge stored in 'sc', performing some sanity checks. Always go
 * through this function to access the challenge, and never read from
 * sc->challenge directly.
 */
static int
read_challenge(struct rxgk_sconn *sc, void *buf, int len)
{
    opr_StaticAssert(sizeof(sc->challenge) == RXGK_CHALLENGE_NONCE_LEN);

    if (len != RXGK_CHALLENGE_NONCE_LEN) {
	return RXGK_INCONSISTENCY;
    }
    if (!sc->challenge_valid) {
	return RXGK_INCONSISTENCY;
    }
    memcpy(buf, sc->challenge, RXGK_CHALLENGE_NONCE_LEN);
    return 0;
}

/* Incorporate a challenge into a packet */
static int
rxgk_GetChallenge(struct rx_securityClass *aobj, struct rx_connection *aconn,
		  struct rx_packet *apacket)
{
    XDR xdrs;
    struct rxgk_sconn *sc;
    void *data = NULL;
    RXGK_Challenge challenge;
    int ret;
    u_int len = 0;
    opr_StaticAssert(sizeof(challenge.nonce) == RXGK_CHALLENGE_NONCE_LEN);

    memset(&xdrs, 0, sizeof(xdrs));
    memset(&challenge, 0, sizeof(challenge));

    sc = rx_GetSecurityData(aconn);
    if (sc == NULL) {
	ret = RXGK_INCONSISTENCY;
	goto done;
    }
    ret = read_challenge(sc, challenge.nonce, sizeof(challenge.nonce));
    if (ret)
	goto done;

    xdrlen_create(&xdrs);
    if (!xdr_RXGK_Challenge(&xdrs, &challenge)) {
	ret = RXGEN_SS_MARSHAL;
	goto done;
    }
    len = xdr_getpos(&xdrs);

    data = rxi_Alloc(len);
    if (data == NULL) {
	ret = RXGK_INCONSISTENCY;
	goto done;
    }
    xdr_destroy(&xdrs);
    xdrmem_create(&xdrs, data, len, XDR_ENCODE);
    if (!xdr_RXGK_Challenge(&xdrs, &challenge)) {
	ret = RXGEN_SS_MARSHAL;
	goto done;
    }
    opr_Assert(len <= 0xffffu);
    rx_packetwrite(apacket, 0, len, data);
    rx_SetDataSize(apacket, len);

    /* Nothing should really pay attention to the checksum of a challenge
     * packet, but just set it to 0 so it's always set to _something_. */
    rx_SetPacketCksum(apacket, 0);

    ret = 0;

 done:
    rxi_Free(data, len);
    if (xdrs.x_ops)
	xdr_destroy(&xdrs);
    return ret;
}

/*
 * Helper functions for CheckResponse.
 */

/**
 * The XDR token format uses the XDR PrAuthName type to store identities.
 * However, there is an existing rx_identity type used in libauth, so
 * we convert from the wire type to the internal type as soon as possible
 * in order to be able to use the most library code. 'a_identity' will contain
 * a single identity on success, not an array.
 *
 * @return rxgk error codes
 */
static int
prnames_to_identity(struct rx_identity **a_identity, PrAuthName *namelist,
		    size_t nnames)
{
    rx_identity_kind kind;
    size_t len;
    char *display;

    *a_identity = NULL;

    /* Could grab the acceptor identity from ServiceSpecific if wanted. */
    if (nnames == 0) {
	*a_identity = rx_identity_new(RX_ID_SUPERUSER, "<printed token>", "",
				      0);
	return 0;

    } else if (nnames > 1) {
	/* Compound identities are not supported yet. */
	return RXGK_INCONSISTENCY;
    }

    if (namelist[0].kind == PRAUTHTYPE_KRB4)
	kind = RX_ID_KRB4;
    else if (namelist[0].kind == PRAUTHTYPE_GSS)
	kind = RX_ID_GSS;
    else
	return RXGK_INCONSISTENCY;
    len = namelist[0].display.len;
    display = rxi_Alloc(len + 1);
    if (display == NULL)
	return RXGK_INCONSISTENCY;
    memcpy(display, namelist[0].display.val, len);
    display[len] = '\0';
    *a_identity = rx_identity_new(kind, display, namelist[0].data.val,
				  namelist[0].data.len);
    rxi_Free(display, len + 1);
    return 0;
}

/*
 * Unpack, decrypt, and extract information from a token.
 * Store the relevant bits in the connection security data.
 */
static int
process_token(RXGK_Data *tc, struct rxgk_sprivate *sp, struct rxgk_sconn *sc)
{
    RXGK_Token token;
    int ret;

    memset(&token, 0, sizeof(token));

    ret = rxgk_extract_token(tc, &token, sp->getkey, sp->rock);
    if (ret != 0)
	goto done;

    /* Stash the token master key in the per-connection data. */
    rxgk_release_key(&sc->k0);
    ret = rxgk_make_key(&sc->k0, token.K0.val, token.K0.len, token.enctype);
    if (ret != 0)
	goto done;

    sc->level = token.level;
    sc->expiration = token.expirationtime;
    /*
     * TODO: note that we currently ignore the bytelife and lifetime in
     * 'token'. In the future, we should of course actually remember these and
     * potentially alter our rekeying frequency according to them.
     */

    if (sc->client != NULL)
	rx_identity_free(&sc->client);
    ret = prnames_to_identity(&sc->client, token.identities.val,
			      token.identities.len);
    if (ret != 0)
	goto done;

 done:
    xdr_free((xdrproc_t)xdr_RXGK_Token, &token);
    return ret;
}

static void
update_kvno(struct rxgk_sconn *sc, afs_uint32 kvno)
{
    sc->key_number = kvno;

    /* XXX Our statistics for tracking when to re-key the conn should be reset
     * here. */
}

/* Caller is responsible for freeing 'out'. */
static int
decrypt_authenticator(RXGK_Authenticator *out, struct rx_opaque *in,
		      struct rx_connection *aconn, struct rxgk_sconn *sc,
		      afs_uint16 wkvno)
{
    XDR xdrs;
    struct rx_opaque packauth = RX_EMPTY_OPAQUE;
    rxgk_key tk = NULL;
    afs_uint32 lkvno, kvno = 0;
    int ret;

    memset(&xdrs, 0, sizeof(xdrs));

    lkvno = sc->key_number;
    ret = rxgk_key_number(wkvno, lkvno, &kvno);
    if (ret != 0)
	goto done;
    ret = rxgk_derive_tk(&tk, sc->k0, rx_GetConnectionEpoch(aconn),
			 rx_GetConnectionId(aconn), sc->start_time, kvno);
    if (ret != 0)
	goto done;
    ret = rxgk_decrypt_in_key(tk, RXGK_CLIENT_ENC_RESPONSE, in, &packauth);
    if (ret != 0) {
	goto done;
    }
    if (kvno > lkvno)
	update_kvno(sc, kvno);

    xdrmem_create(&xdrs, packauth.val, packauth.len, XDR_DECODE);
    if (!xdr_RXGK_Authenticator(&xdrs, out)) {
	ret = RXGEN_SS_UNMARSHAL;
	goto done;
    }
    ret = 0;

 done:
    rx_opaque_freeContents(&packauth);
    rxgk_release_key(&tk);
    if (xdrs.x_ops)
	xdr_destroy(&xdrs);
    return ret;
}

/*
 * Make the authenticator do its job with channel binding and nonce
 * verification.
 */
static int
check_authenticator(RXGK_Authenticator *authenticator,
		    struct rx_connection *aconn, struct rxgk_sconn *sc)
{
    /*
     * To check the data in the authenticator, we could simply check
     * if (got_value == expected_value) for each field we care about. But since
     * this is a security-sensitive check, we should try to do this check in
     * constant time to avoid timing-based attacks. So to do that, we construct
     * a small structure of the values we got and the expected values, and run
     * ct_memcmp on the whole thing at the end.
     */

    int code;
    struct {
	unsigned char challenge[RXGK_CHALLENGE_NONCE_LEN];
	RXGK_Level level;
	afs_uint32 epoch;
	afs_uint32 cid;
	int calls_len;
    } auth_got, auth_exp;

    opr_StaticAssert(sizeof(auth_got.challenge) == RXGK_CHALLENGE_NONCE_LEN);
    opr_StaticAssert(sizeof(auth_exp.challenge) == RXGK_CHALLENGE_NONCE_LEN);
    opr_StaticAssert(sizeof(authenticator->nonce) == RXGK_CHALLENGE_NONCE_LEN);

    memset(&auth_got, 0, sizeof(auth_got));
    memset(&auth_exp, 0, sizeof(auth_exp));

    memcpy(auth_got.challenge, authenticator->nonce, RXGK_CHALLENGE_NONCE_LEN);
    code = read_challenge(sc, auth_exp.challenge, sizeof(auth_exp.challenge));
    if (code)
	return code;

    auth_got.level = authenticator->level;
    auth_exp.level = sc->level;

    auth_got.epoch = authenticator->epoch;
    auth_exp.epoch = rx_GetConnectionEpoch(aconn);

    auth_got.cid = authenticator->cid;
    auth_exp.cid = rx_GetConnectionId(aconn);

    auth_got.calls_len = authenticator->call_numbers.len;
    auth_exp.calls_len = RX_MAXCALLS;

    /* XXX We do nothing with the appdata for now. */

    if (ct_memcmp(&auth_got, &auth_exp, sizeof(auth_got)) != 0) {
	return RXGK_BADCHALLENGE;
    }
    return 0;
}

/* Process the response packet to a challenge */
static int
rxgk_CheckResponse(struct rx_securityClass *aobj,
		   struct rx_connection *aconn, struct rx_packet *apacket)
{
    struct rxgk_sprivate *sp;
    struct rxgk_sconn *sc;
    XDR xdrs;
    RXGK_Response response;
    RXGK_Authenticator authenticator;
    int ret;

    memset(&xdrs, 0, sizeof(xdrs));
    memset(&response, 0, sizeof(response));
    memset(&authenticator, 0, sizeof(authenticator));

    sp = aobj->privateData;
    sc = rx_GetSecurityData(aconn);

    /*
     * This assumes that the entire response is in a contiguous data block in
     * the packet. rx in general can store packet data in multiple different
     * buffers (pointed to by apacket->wirevec[N]), but the payload when
     * receiving a Response packet should all be in one buffer (so we can just
     * reference it directly via rx_DataOf()). If this assumption turns out to
     * be wrong, then we'll just see a truncated response blob and this
     * function will likely return an error; there should be no danger of
     * buffer overrun or anything scary like that.
     */
    xdrmem_create(&xdrs, rx_DataOf(apacket), rx_Contiguous(apacket),
		  XDR_DECODE);
    if (!xdr_RXGK_Response(&xdrs, &response)) {
	ret = RXGEN_SS_UNMARSHAL;
	goto done;
    }

    /* Stash useful bits from the token in sc. */
    ret = process_token(&response.token, sp, sc);
    if (ret != 0)
	goto done;
    if (sc->expiration < RXGK_NOW() && sc->expiration != RXGK_NEVERDATE) {
	ret = RXGK_EXPIRED;
	goto done;
    }

    /*
     * Cache the client-provided start_time. If this is wrong, we cannot derive
     * the correct transport key and the authenticator decryption will fail.
     */
    sc->start_time = response.start_time;

    /* Try to decrypt the authenticator. */
    ret = decrypt_authenticator(&authenticator, &response.authenticator, aconn,
				sc, rx_GetPacketCksum(apacket));
    if (ret != 0)
	goto done;
    ret = check_authenticator(&authenticator, aconn, sc);
    if (ret != 0)
	goto done;
    ret = rxgk_security_overhead(aconn, sc->level, sc->k0);
    if (ret != 0)
	goto done;
    if (rxi_SetCallNumberVector(aconn, (afs_int32 *)authenticator.call_numbers.val) != 0) {
	ret = RXGK_INCONSISTENCY;
	goto done;
    }
    /* Success! */
    sc->auth = 1;
    sc->challenge_valid = 0;

 done:
    if (ret != 0)
	sconn_set_noauth(sc);
    if (xdrs.x_ops)
	xdr_destroy(&xdrs);
    xdr_free((xdrproc_t)xdr_RXGK_Response, &response);
    xdr_free((xdrproc_t)xdr_RXGK_Authenticator, &authenticator);
    return ret;
}

/*
 * Server-specific packet receipt routine.
 * The interesting bits are in rxgk_packet.c, we just extract data from the
 * connection security data.
 */
static int
rxgk_ServerCheckPacket(struct rx_securityClass *aobj, struct rx_call *acall,
		       struct rx_packet *apacket)
{
    struct rxgk_sconn *sc;
    struct rx_connection *aconn;
    afs_uint32 lkvno, kvno;
    afs_uint16 len;
    int ret;

    aconn = rx_ConnectionOf(acall);
    sc = rx_GetSecurityData(aconn);
    if (sc == NULL)
	return RXGK_INCONSISTENCY;

    len = rx_GetDataSize(apacket);
    sc->stats.precv++;
    sc->stats.brecv += len;
    if (sc->expiration < RXGK_NOW() && sc->expiration != RXGK_NEVERDATE)
	return RXGK_EXPIRED;

    lkvno = kvno = sc->key_number;
    ret = rxgk_check_packet(1, aconn, apacket, sc->level, sc->start_time,
			    &kvno, sc->k0);
    if (ret != 0)
	return ret;

    if (kvno > lkvno)
	update_kvno(sc, kvno);

    return ret;
}

/*
 * Perform server-side connection-specific teardown.
 */
static void
rxgk_DestroyServerConnection(struct rx_securityClass *aobj,
			     struct rx_connection *aconn)
{
    struct rxgk_sconn *sc;

    sc = rx_GetSecurityData(aconn);
    if (sc == NULL) {
	return;
    }
    rx_SetSecurityData(aconn, NULL);

    rxgk_release_key(&sc->k0);
    if (sc->client != NULL)
	rx_identity_free(&sc->client);
    rxi_Free(sc, sizeof(*sc));
    obj_rele(aobj);
}

/*
 * Get statistics about this connection.
 */
static int
rxgk_ServerGetStats(struct rx_securityClass *aobj, struct rx_connection *aconn,
		    struct rx_securityObjectStats *astats)
{
    struct rxgkStats *stats;
    struct rxgk_sconn *sc;

    astats->type = RX_SECTYPE_GK;
    sc = rx_GetSecurityData(aconn);
    if (sc == NULL) {
	astats->flags |= RXGK_STATS_UNALLOC;
	return 0;
    }

    stats = &sc->stats;
    astats->level = sc->level;
    if (sc->auth)
	astats->flags |= RXGK_STATS_AUTH;
    astats->expires = (afs_uint32)rxgkTimeToSeconds(sc->expiration);

    astats->packetsReceived = stats->precv;
    astats->packetsSent = stats->psent;
    astats->bytesReceived = stats->brecv;
    astats->bytesSent = stats->bsent;

    return 0;
}

/*
 * Get some information about this connection, in particular the security
 * level, expiry time, and the remote user's identity.
 */
afs_int32
rxgk_GetServerInfo(struct rx_connection *conn, RXGK_Level *level,
		   rxgkTime *expiry, struct rx_identity **identity)
{
    struct rxgk_sconn *sconn;

    if (rx_SecurityClassOf(conn) != RX_SECIDX_GK) {
	return EINVAL;
    }

    sconn = rx_GetSecurityData(conn);
    if (sconn == NULL)
	return RXGK_INCONSISTENCY;
    if (identity != NULL) {
	*identity = rx_identity_copy(sconn->client);
	if (*identity == NULL)
	    return RXGK_INCONSISTENCY;
    }
    if (level != NULL)
	*level = sconn->level;
    if (expiry != NULL)
	*expiry = sconn->expiration;
    return 0;
}

static struct rx_securityOps rxgk_server_ops = {
    AFS_STRUCT_INIT(.op_Close,		rxgk_ServerClose),
    AFS_STRUCT_INIT(.op_NewConnection,	rxgk_NewServerConnection),
    AFS_STRUCT_INIT(.op_PreparePacket,	rxgk_ServerPreparePacket), /* once per packet creation */
    AFS_STRUCT_INIT(.op_SendPacket,	NULL),			   /* send packet (once per retrans) */
    AFS_STRUCT_INIT(.op_CheckAuthentication, rxgk_CheckAuthentication),
    AFS_STRUCT_INIT(.op_CreateChallenge, rxgk_CreateChallenge),
    AFS_STRUCT_INIT(.op_GetChallenge,	rxgk_GetChallenge),
    AFS_STRUCT_INIT(.op_GetResponse,	NULL),
    AFS_STRUCT_INIT(.op_CheckResponse,	rxgk_CheckResponse),
    AFS_STRUCT_INIT(.op_CheckPacket,	rxgk_ServerCheckPacket),   /* check data packet */
    AFS_STRUCT_INIT(.op_DestroyConnection, rxgk_DestroyServerConnection),
    AFS_STRUCT_INIT(.op_GetStats,	rxgk_ServerGetStats),
    AFS_STRUCT_INIT(.op_SetConfiguration, NULL),
    AFS_STRUCT_INIT(.op_Spare2,		NULL),			   /* spare 2 */
    AFS_STRUCT_INIT(.op_Spare3,		NULL),			   /* spare 3 */
};

/**
 * The low-level routine to generate a new server security object.
 *
 * Takes a getkey function and its rock.
 *
 * It is not expected that most callers will use this function, as
 * we provide helpers that do other setup, setting service-specific
 * data and such.
 */
struct rx_securityClass *
rxgk_NewServerSecurityObject(void *getkey_rock, rxgk_getkey_func getkey)
{
    struct rx_securityClass *sc;
    struct rxgk_sprivate *sp;

    sc = rxi_Alloc(sizeof(*sc));
    if (sc == NULL)
	return NULL;
    sp = rxi_Alloc(sizeof(*sp));
    if (sp == NULL) {
	rxi_Free(sc, sizeof(*sc));
	return NULL;
    }
    sc->ops = &rxgk_server_ops;
    rxs_SetRefs(sc, 1);
    sc->privateData = sp;

    /* Now set the server-private data. */
    sp->rock = getkey_rock;
    sp->getkey = getkey;

    return sc;
}
