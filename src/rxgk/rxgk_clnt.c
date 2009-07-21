/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rxgk_locl.h"

/* Security object specific client data */
typedef struct rxgk_clnt_class {
    struct rx_securityClass klass;
    krb5_context context;
    rxgk_level level;
    krb5_keyblock krb_key;
    key_stuff k;
    int32_t kvno;
    RXGK_Token ticket;
    uint32_t serviceId;
#if 0
    RXGK_rxtransport_key contrib;
#endif
} rxgk_clnt_class;

/* Per connection specific client data */
typedef struct clnt_con_data {
    RXGK_Token auth_token;
    int32_t auth_token_kvno;
    end_stuff e;
} clnt_con_data;

static
int
client_NewConnection(struct rx_securityClass *obj_, struct rx_connection *con)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    clnt_con_data *cdat;
    int ret;
    
    assert(con->securityData == 0);
    obj->klass.refCount++;
    cdat = (clnt_con_data *) osi_Alloc(sizeof(clnt_con_data));
    cdat->e.bytesReceived = cdat->e.packetsReceived = 0;
    cdat->e.bytesSent = cdat->e.packetsSent = 0;
    
    con->securityData = (char *) cdat;
    rx_nextCid += RX_MAXCALLS;
    con->epoch = rx_epoch;
    con->cid = rx_nextCid;
    cdat->auth_token.len = 0;
    cdat->auth_token.val = NULL;

    ret = rxgk5_get_auth_token(obj->context,
			       rx_HostOf(con->peer), rx_PortOf(con->peer),
			       obj->serviceId,
			       &obj->ticket, &cdat->auth_token, 
			       &obj->krb_key, 
			       &obj->k.ks_key,
			       &cdat->auth_token_kvno);
    if (ret) {
	osi_Free(cdat, sizeof(clnt_con_data));
	return ret;
    }

    /* XXX derive crypto key */

    ret = krb5_crypto_init(obj->k.ks_context,
			   &obj->k.ks_key, obj->k.ks_key.keytype,
			   &obj->k.ks_crypto);
    if (ret)
	goto out;

#if 0
    obj->contrib.server_keycontribution.val = "";
    obj->contrib.server_keycontribution.len = 0;

    obj->contrib.client_keycontribution.len = rxgk_key_contrib_size;
    obj->contrib.client_keycontribution.val = malloc(rxgk_key_contrib_size);
    if (obj->contrib.client_keycontribution.val == NULL)
	goto out;

    {
	int i;

	for (i = 0; i < rxgk_key_contrib_size; i++)
	    obj->contrib.client_keycontribution.val[i] = arc4random(); /*XXX*/
    }

    ret = rxgk_derive_transport_key(obj->k.ks_context,
				    &obj->k.ks_key,
				    &obj->contrib,
				    &obj->k.ks_skey);
    if (ret)
	return ret;
#endif

    ret = krb5_crypto_init (obj->context, &obj->k.ks_skey,
			    obj->k.ks_skey.keytype,
			    &obj->k.ks_scrypto);
    if (ret)
	return ret;

    ret = rxgk_set_conn(con, obj->k.ks_key.keytype, 
			obj->level == rxgk_crypt ? 1 : 0);

 out:
    if (ret) {
	if (obj->k.ks_crypto)
	    krb5_crypto_destroy(obj->k.ks_context, obj->k.ks_crypto);
	obj->k.ks_crypto = NULL;
	krb5_free_keyblock_contents(obj->k.ks_context, &obj->k.ks_skey);
	memset(&obj->k.ks_skey, 0, sizeof(obj->k.ks_skey));
	osi_Free(cdat->auth_token.val, cdat->auth_token.len);
	osi_Free(cdat, sizeof(clnt_con_data));
	return ret;
    }
    
    return 0;
}

static
int
client_Close(struct rx_securityClass *obj_)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    obj->klass.refCount--;
    if (obj->klass.refCount <= 0)
    {
	osi_Free(obj->ticket.val, obj->ticket.len);
	osi_Free(obj, sizeof(rxgk_clnt_class));
    }
    return 0;
}

static
int
client_DestroyConnection(struct rx_securityClass *obj,
			 struct rx_connection *con)
{
    clnt_con_data *cdat = (clnt_con_data *)con->securityData;
  
    if (cdat)
      osi_Free(cdat, sizeof(clnt_con_data));
    return client_Close(obj);
}

/*
 * Receive a challange and respond.
 */
static
int
client_GetResponse(const struct rx_securityClass *obj_,
		   const struct rx_connection *con,
		   struct rx_packet *pkt)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    clnt_con_data *cdat = (clnt_con_data *)con->securityData;
    key_stuff *k = &obj->k;
    struct RXGK_Challenge c;
    struct RXGK_Response r;
    struct RXGK_Response_Crypt rc;
    char bufrc[RXGK_RESPONSE_CRYPT_SIZE];
    char bufr[RXGK_RESPONSE_MAX_SIZE];
    krb5_data data;
    size_t len;
    int ret;
    char *p;
    
    memset(&r, 0, sizeof(r));
    memset(&rc, 0, sizeof(rc));
    
    /* Get challenge */
    if (rx_SlowReadPacket(pkt, 0, sizeof(c), &c) != sizeof(c))
	return RXGKPACKETSHORT;
    
    if (ntohl(c.rc_version) != RXGK_VERSION)
	return RXGKINCONSISTENCY;
    
    if (ntohl(c.rc_min_level) > obj->level)
	return RXGKLEVELFAIL;
    
    if (c.rc_opcode == htonl(RXKG_OPCODE_CHALLENGE)) {
	;
    } else if (c.rc_opcode == htonl(RXKG_OPCODE_REKEY)) {
	/* XXX decode ydr_encode_RXGK_ReKey_Crypt info */
	return RXGKINCONSISTENCY;
#if 0
	ret = rxgk_derive_transport_key(obj->k.ks_context,
					&obj->k.ks_key,
					&obj->contrib,
					&obj->k.ks_skey);
	if (ret)
	    return ret;
	
	ret = krb5_crypto_init (obj->context, &obj->k.ks_skey,
				obj->k.ks_skey.keytype,
				&obj->k.ks_scrypto);
	if (ret)
	    return ret;
#endif
    } else
	return RXGKINCONSISTENCY;
    
    rc.nonce = ntohl(c.rc_nonce) + 1;
    rc.epoch = con->epoch;
    rc.cid = con->cid & RX_CIDMASK;
    rxi_GetCallNumberVector(con, rc.call_numbers);
#if 0
    rc.security_index = con->securityIndex;
#endif
    rc.level = obj->level;
#if 0
    rc.last_seq = 0; /* XXX */
#endif
    rc.key_version = 0;
#if 0
    rc.contrib = obj->contrib; /* XXX copy_RXGK_rxtransport_key */
#endif
    
    {
	int i;
	for (i = 0; i < RX_MAXCALLS; i++) {
	    if (rc.call_numbers[i] < 0)
		return RXGKINCONSISTENCY;
	}
    }
    len = sizeof(bufrc);
    p = ydr_encode_RXGK_Response_Crypt(&rc, bufrc, &len);
    if (p == NULL)
	return RXGKINCONSISTENCY;
    len = sizeof(bufrc) - len;

    ret = krb5_encrypt(obj->context, k->ks_crypto, 
		       RXGK_CLIENT_ENC_CHALLENGE, bufrc, len, &data);
    if (ret)
	return ret;

    r.rr_version = RXGK_VERSION;
    r.rr_auth_token_kvno = cdat->auth_token_kvno;
    r.rr_auth_token.val = cdat->auth_token.val;
    r.rr_auth_token.len = cdat->auth_token.len;
    r.rr_ctext.val = data.data;
    r.rr_ctext.len = data.length;

    len = sizeof(bufr);
    p = ydr_encode_RXGK_Response(&r, bufr, &len);
    len = sizeof(bufr) - len;
    krb5_data_free(&data);

    if (p == NULL)
	return RXGKINCONSISTENCY;

    if (rx_SlowWritePacket(pkt, 0, len, bufr) != len)
	return RXGKPACKETSHORT;
    rx_SetDataSize(pkt, len);

    return 0;
}

/*
 * Checksum and/or encrypt packet.
 */
static
int
client_PreparePacket(struct rx_securityClass *obj_,
		     struct rx_call *call,
		     struct rx_packet *pkt)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    key_stuff *k = &obj->k;
    struct rx_connection *con = rx_ConnectionOf(call);
    end_stuff *e = &((clnt_con_data *) con->securityData)->e;
    
    return rxgk_prepare_packet(pkt, con, obj->level, k, e);
}

/*
 * Verify checksums and/or decrypt packet.
 */
static
int
client_CheckPacket(struct rx_securityClass *obj_,
		   struct rx_call *call,
		   struct rx_packet *pkt)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    key_stuff *k = &obj->k;
    struct rx_connection *con = rx_ConnectionOf(call);
    end_stuff *e = &((clnt_con_data *) con->securityData)->e;
    
    return rxgk_check_packet(pkt, con, obj->level, k, e);
}

static
int
client_GetStats(const struct rx_securityClass *obj,
		const struct rx_connection *con,
		struct rx_securityObjectStats *st)
{
    clnt_con_data *cdat = (clnt_con_data *) con->securityData;
    
    st->type = rxgk_disipline;
    st->level = ((rxgk_clnt_class *)obj)->level;
    st->flags = rxgk_checksummed;
    if (cdat == 0)
	st->flags |= rxgk_unallocated;
    {
	st->bytesReceived = cdat->e.bytesReceived;
	st->packetsReceived = cdat->e.packetsReceived;
	st->bytesSent = cdat->e.bytesSent;
	st->packetsSent = cdat->e.packetsSent;
    }
    return 0;
}

static
struct rx_securityOps client_ops = {
    client_Close,
    client_NewConnection,
    client_PreparePacket,
    NULL,
    NULL,
    NULL,
    NULL,
    client_GetResponse,
    NULL,
    client_CheckPacket,
    client_DestroyConnection,
    client_GetStats,
    NULL,
    NULL,
    NULL,
};

int rxgk_EpochWasSet = 0;

int rxgk_min_level = rxgk_crypt; /* rxgk_{auth,crypt} */ /* XXX */

struct rx_securityClass *
rxgk_k5_NewClientSecurityObject(/*rxgk_level*/ int level,
				krb5_keyblock *key,
				int32_t kvno,
				int ticket_len,
				void *ticket,
				uint32_t serviceId,
				krb5_context context)
{
    rxgk_clnt_class *obj;
    static int inited = 0;
    int ret;
    
    if (level < rxgk_min_level)
	level = rxgk_min_level;	/* Boost security level */
    
    if (!inited) {
	rx_SetEpoch(arc4random());
	inited = 1;
    }
    
    obj = (rxgk_clnt_class *) osi_Alloc(sizeof(rxgk_clnt_class));
    obj->klass.refCount = 1;
    obj->klass.ops = &client_ops;
    
    obj->klass.privateData = (char *) obj;
    
    obj->context = context;
    obj->level = level;
    obj->kvno = kvno;
    obj->serviceId = serviceId;
    
    ret = krb5_copy_keyblock_contents(context, key, &obj->krb_key);
    if (ret) {
	osi_Free(obj, sizeof(rxgk_clnt_class));
	return NULL;
    }

    memset(&obj->k.ks_key, 0, sizeof(obj->k.ks_key));
    obj->k.ks_crypto = NULL;

    memset(&obj->k.ks_skey, 0, sizeof(obj->k.ks_skey));
    obj->k.ks_scrypto = NULL;
    obj->k.ks_context = context;

    obj->ticket.len = ticket_len;
    obj->ticket.val = osi_Alloc(ticket_len);
    memcpy(obj->ticket.val, ticket, obj->ticket.len);
    
    return &obj->klass;
}
