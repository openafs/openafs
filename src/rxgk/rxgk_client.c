/* rxgk/rxgk_client.c - Client-only security object routines */
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
 * Client-only security object routines.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <afs/opr.h>

/* OS-specific system headers go here */

#include <rx/rx.h>
#include <rx/rx_packet.h>
#include <rx/rx_identity.h>
#include <rx/rxgk.h>
#include <rx/xdr.h>

#include "rxgk_private.h"

static void
cprivate_destroy(struct rxgk_cprivate *cp)
{
    if (cp == NULL)
        return;
    rxgk_release_key(&cp->k0);
    rx_opaque_freeContents(&cp->token);
    rxi_Free(cp, sizeof(*cp));
}

/*
 * Increment the reference count on the security object secobj.
 */
static_inline void
obj_ref(struct rx_securityClass *secobj)
{
    secobj->refCount++;
}

/*
 * Decrement the reference count on the security object secobj.
 * If the reference count falls to zero, release the underlying storage.
 */
static void
obj_rele(struct rx_securityClass *secobj)
{
    struct rxgk_cprivate *cp;

    secobj->refCount--;
    if (secobj->refCount > 0) {
	/* still in use */
	return;
    }

    cp = secobj->privateData;
    cprivate_destroy(cp);
    rxi_Free(secobj, sizeof(*secobj));
    return;
}

static int
rxgk_ClientClose(struct rx_securityClass *aobj)
{
    obj_rele(aobj);
    return 0;
}

static int
rxgk_NewClientConnection(struct rx_securityClass *aobj,
			 struct rx_connection *aconn)
{
    struct rxgk_cconn *cc = NULL;
    struct rxgk_cprivate *cp;

    if (rx_GetSecurityData(aconn) != NULL)
	goto error;
    cp = aobj->privateData;

    cc = rxi_Alloc(sizeof(*cc));
    if (cc == NULL)
	goto error;
    cc->start_time = RXGK_NOW();
    /* Set the header and trailer size to be reserved for the security
     * class in each packet. */
    if (rxgk_security_overhead(aconn, cp->level, cp->k0) != 0)
	goto error;
    rx_SetSecurityData(aconn, cc);
    obj_ref(aobj);
    return 0;

 error:
    rxi_Free(cc, sizeof(*cc));
    return RXGK_INCONSISTENCY;
}

static int
rxgk_ClientPreparePacket(struct rx_securityClass *aobj, struct rx_call *acall,
			 struct rx_packet *apacket)
{
    struct rxgk_cconn *cc;
    struct rxgk_cprivate *cp;
    struct rx_connection *aconn;
    rxgk_key tk;
    afs_uint32 lkvno;
    afs_uint16 wkvno, len;
    int ret;

    aconn = rx_ConnectionOf(acall);
    cc = rx_GetSecurityData(aconn);
    cp = aobj->privateData;

    len = rx_GetDataSize(apacket);
    lkvno = cc->key_number;
    cc->stats.psent++;
    cc->stats.bsent += len;
    wkvno = (afs_uint16)lkvno;
    rx_SetPacketCksum(apacket, wkvno);

    if (cp->level == RXGK_LEVEL_CLEAR)
        return 0;

    ret = rxgk_derive_tk(&tk, cp->k0, rx_GetConnectionEpoch(aconn),
			 rx_GetConnectionId(aconn), cc->start_time, lkvno);
    if (ret != 0)
	return ret;

    switch(cp->level) {
	case RXGK_LEVEL_AUTH:
	    ret = rxgk_mic_packet(tk, RXGK_CLIENT_MIC_PACKET, aconn, apacket);
	    break;
	case RXGK_LEVEL_CRYPT:
	    ret = rxgk_enc_packet(tk, RXGK_CLIENT_ENC_PACKET, aconn, apacket);
	    break;
	default:
	    ret = RXGK_INCONSISTENCY;
	    break;
    }

    rxgk_release_key(&tk);
    return ret;
}

/*
 * Helpers for GetResponse.
 */

/*
 * Populate the RXGK_Authenticator structure.
 * The caller is responsible for pre-zeroing the structure and freeing
 * the resulting allocations, including partial allocations in the case
 * of failure.
 */
static int
fill_authenticator(RXGK_Authenticator *authenticator, RXGK_Challenge *challenge,
		   struct rxgk_cprivate *cp, struct rx_connection *aconn)
{
    afs_int32 call_numbers[RX_MAXCALLS];
    int ret, call_i;
    opr_StaticAssert(sizeof(authenticator->nonce) == RXGK_CHALLENGE_NONCE_LEN);
    opr_StaticAssert(sizeof(challenge->nonce) == RXGK_CHALLENGE_NONCE_LEN);

    memset(&call_numbers, 0, sizeof(call_numbers));

    memcpy(authenticator->nonce, challenge->nonce, sizeof(authenticator->nonce));

    authenticator->level = cp->level;
    authenticator->epoch = rx_GetConnectionEpoch(aconn);
    authenticator->cid = rx_GetConnectionId(aconn);
    /* Export the call numbers. */
    ret = rxi_GetCallNumberVector(aconn, call_numbers);
    if (ret != 0)
	return ret;
    authenticator->call_numbers.val = xdr_alloc(RX_MAXCALLS *
						sizeof(afs_int32));
    if (authenticator->call_numbers.val == NULL)
	return RXGEN_CC_MARSHAL;
    authenticator->call_numbers.len = RX_MAXCALLS;
    for (call_i = 0; call_i < RX_MAXCALLS; call_i++)
	authenticator->call_numbers.val[call_i] = (afs_uint32)call_numbers[call_i];
    return 0;
}

/* XDR-encode an authenticator and encrypt it. */
static int
pack_wrap_authenticator(RXGK_Data *encdata, RXGK_Authenticator *authenticator,
			struct rxgk_cprivate *cp, struct rxgk_cconn *cc)
{
    XDR xdrs;
    struct rx_opaque data = RX_EMPTY_OPAQUE;
    rxgk_key tk = NULL;
    int ret;
    u_int len;

    memset(&xdrs, 0, sizeof(xdrs));

    xdrlen_create(&xdrs);
    if (!xdr_RXGK_Authenticator(&xdrs, authenticator)) {
	ret = RXGEN_CC_MARSHAL;
	goto done;
    }
    len = xdr_getpos(&xdrs);
    ret = rx_opaque_alloc(&data, len);
    if (ret != 0)
	goto done;
    xdr_destroy(&xdrs);
    xdrmem_create(&xdrs, data.val, len, XDR_ENCODE);
    if (!xdr_RXGK_Authenticator(&xdrs, authenticator)) {
	ret = RXGEN_CC_MARSHAL;
	goto done;
    }
    ret = rxgk_derive_tk(&tk, cp->k0, authenticator->epoch, authenticator->cid,
		         cc->start_time, cc->key_number);
    if (ret != 0)
	goto done;
    ret = rxgk_encrypt_in_key(tk, RXGK_CLIENT_ENC_RESPONSE, &data, encdata);
    if (ret != 0)
	goto done;

 done:
    if (xdrs.x_ops) {
        xdr_destroy(&xdrs);
    }
    rx_opaque_freeContents(&data);
    rxgk_release_key(&tk);
    return ret;
}

/* XDR-encode an RXGK_Response structure to put it on the wire.
 * The caller must free the contents of the out parameter. */
static int
pack_response(RXGK_Data *out, RXGK_Response *response)
{
    XDR xdrs;
    int ret;
    u_int len;

    memset(&xdrs, 0, sizeof(xdrs));

    xdrlen_create(&xdrs);
    if (!xdr_RXGK_Response(&xdrs, response)) {
	ret = RXGEN_CC_MARSHAL;
	goto done;
    }
    len = xdr_getpos(&xdrs);
    ret = rx_opaque_alloc(out, len);
    if (ret != 0)
	goto done;
    xdr_destroy(&xdrs);
    xdrmem_create(&xdrs, out->val, len, XDR_ENCODE);
    if (!xdr_RXGK_Response(&xdrs, response)) {
	rx_opaque_freeContents(out);
	ret = RXGEN_CC_MARSHAL;
	goto done;
    }

 done:
    if (xdrs.x_ops) {
        xdr_destroy(&xdrs);
    }
    return ret;
}

/*
 * Respond to a challenge packet.
 * The data of the packet on entry is the XDR-encoded RXGK_Challenge.
 * We decode it and reuse the packet structure to prepare a response.
 */
static int
rxgk_GetResponse(struct rx_securityClass *aobj, struct rx_connection *aconn,
		 struct rx_packet *apacket)
{
    struct rxgk_cprivate *cp;
    struct rxgk_cconn *cc;
    XDR xdrs;
    RXGK_Challenge challenge;
    RXGK_Response response;
    RXGK_Authenticator authenticator;
    struct rx_opaque packed = RX_EMPTY_OPAQUE;
    int ret;

    memset(&xdrs, 0, sizeof(xdrs));
    memset(&challenge, 0, sizeof(challenge));
    memset(&response, 0, sizeof(response));
    memset(&authenticator, 0, sizeof(authenticator));

    cp = aobj->privateData;
    cc = rx_GetSecurityData(aconn);

    /*
     * Decode the challenge to get the nonce.
     * This assumes that the entire challenge is in a contiguous data block in
     * the packet. rx in general can store packet data in multiple different
     * buffers (pointed to by apacket->wirevec[N]), but the payload when
     * receiving a Challenge packet should all be in one buffer (so we can just
     * reference it directly via rx_DataOf()). If this assumption turns out to
     * be wrong, then we'll just see a truncated challenge blob and this
     * function will likely return an error; there should be no danger of
     * buffer overrun or anything scary like that.
     */
    if (rx_Contiguous(apacket) < RXGK_CHALLENGE_NONCE_LEN) {
        ret = RXGK_PACKETSHORT;
	goto done;
    }
    xdrmem_create(&xdrs, rx_DataOf(apacket), rx_Contiguous(apacket),
		  XDR_DECODE);
    if (!xdr_RXGK_Challenge(&xdrs, &challenge)) {
	ret = RXGEN_CC_UNMARSHAL;
	goto done;
    }

    /* Start filling the response. */
    response.start_time = cc->start_time;
    if (rx_opaque_copy(&response.token, &cp->token) != 0) {
	ret = RXGEN_CC_MARSHAL;
	goto done;
    }

    /* Fill up the authenticator */
    ret = fill_authenticator(&authenticator, &challenge, cp, aconn);
    if (ret != 0)
	goto done;
    /* Authenticator is full, now to pack and encrypt it. */
    ret = pack_wrap_authenticator(&response.authenticator, &authenticator, cp, cc);
    if (ret != 0)
	goto done;
    /* Put the kvno we used on the wire for the remote end. */
    rx_SetPacketCksum(apacket, (afs_uint16)cc->key_number);

    /* Response is ready, now to shove it in a packet. */
    ret = pack_response(&packed, &response);
    if (ret != 0)
	goto done;
    osi_Assert(packed.len <= 0xffffu);
    rx_packetwrite(apacket, 0, packed.len, packed.val);
    rx_SetDataSize(apacket, (afs_uint16)packed.len);

 done:
    if (xdrs.x_ops) {
        xdr_destroy(&xdrs);
    }
    xdr_free((xdrproc_t)xdr_RXGK_Challenge, &challenge);
    xdr_free((xdrproc_t)xdr_RXGK_Response, &response);
    xdr_free((xdrproc_t)xdr_RXGK_Authenticator, &authenticator);
    rx_opaque_freeContents(&packed);
    return ret;
}

static void
update_kvno(struct rxgk_cconn *cc, afs_uint32 kvno)
{
    cc->key_number = kvno;

    /* XXX Our statistics for tracking when to re-key the conn should be reset
     * here. */
}

static int
rxgk_ClientCheckPacket(struct rx_securityClass *aobj, struct rx_call *acall,
		       struct rx_packet *apacket)
{
    struct rxgk_cconn *cc;
    struct rxgk_cprivate *cp;
    struct rx_connection *aconn;
    afs_uint32 lkvno, kvno;
    afs_uint16 len;
    int ret;

    aconn = rx_ConnectionOf(acall);
    cc = rx_GetSecurityData(aconn);
    cp = aobj->privateData;

    len = rx_GetDataSize(apacket);
    cc->stats.precv++;
    cc->stats.brecv += len;

    lkvno = kvno = cc->key_number;
    ret = rxgk_check_packet(0, aconn, apacket, cp->level, cc->start_time,
                            &kvno, cp->k0);
    if (ret != 0)
	return ret;

    if (kvno > lkvno)
	update_kvno(cc, kvno);

    return ret;
}

static void
rxgk_DestroyClientConnection(struct rx_securityClass *aobj,
			     struct rx_connection *aconn)
{
    struct rxgk_cconn *cc;

    cc = rx_GetSecurityData(aconn);
    if (cc == NULL) {
        return;
    }
    rx_SetSecurityData(aconn, NULL);

    rxi_Free(cc, sizeof(*cc));
    obj_rele(aobj);
}

static int
rxgk_ClientGetStats(struct rx_securityClass *aobj, struct rx_connection *aconn,
		    struct rx_securityObjectStats *astats)
{
    struct rxgkStats *stats;
    struct rxgk_cprivate *cp;
    struct rxgk_cconn *cc;

    astats->type = RX_SECTYPE_GK;
    cc = rx_GetSecurityData(aconn);
    if (cc == NULL) {
	astats->flags |= RXGK_STATS_UNALLOC;
	return 0;
    }

    stats = &cc->stats;
    cp = aobj->privateData;
    astats->level = cp->level;

    astats->packetsReceived = stats->precv;
    astats->packetsSent = stats->psent;
    astats->bytesReceived = stats->brecv;
    astats->bytesSent = stats->bsent;

    return 0;
}

static struct rx_securityOps rxgk_client_ops = {
    rxgk_ClientClose,
    rxgk_NewClientConnection,		/* every new connection */
    rxgk_ClientPreparePacket,		/* once per packet creation */
    0,					/* send packet (once per retrans) */
    0,
    0,
    0,
    rxgk_GetResponse,			/* respond to challenge packet */
    0,
    rxgk_ClientCheckPacket,		/* check data packet */
    rxgk_DestroyClientConnection,
    rxgk_ClientGetStats,
    0,
    0,					/* spare 1 */
    0,					/* spare 2 */
};

/*
 * Create an rxgk client security object
 *
 * @param[in] level	The security level of the token; this must be the
 *			level used for all connections using this security
 *			object.
 * @param[in] enctype	The RFC 3961 enctype of k0.
 * @param[in] k0	The master key of the token.
 * @param[in] token	The rxgk token used to authenticate the connection.
 * @return NULL on failure, and a pointer to the security object on success.
 */
struct rx_securityClass *
rxgk_NewClientSecurityObject(RXGK_Level level, afs_int32 enctype, rxgk_key k0,
			     RXGK_Data *token)
{
    struct rx_securityClass *sc = NULL;
    struct rxgk_cprivate *cp = NULL;

    sc = rxi_Alloc(sizeof(*sc));
    if (sc == NULL)
	goto error;
    cp = rxi_Alloc(sizeof(*cp));
    if (cp == NULL)
	goto error;
    sc->ops = &rxgk_client_ops;
    sc->refCount = 1;
    sc->privateData = cp;

    /* Now get the client-private data. */
    if (rxgk_copy_key(k0, &cp->k0) != 0)
	goto error;
    cp->enctype = enctype;
    cp->level = level;
    if (rx_opaque_copy(&cp->token, token) != 0)
	goto error;

    return sc;

 error:
    rxi_Free(sc, sizeof(*sc));
    cprivate_destroy(cp);
    return NULL;
}
