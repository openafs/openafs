/*
 * Copyright (c) 1995 - 2004, 2007 Kungliga Tekniska Högskolan
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

RCSID("$Id$");

/* Security object specific client data */
typedef struct rxgk_clnt_class {
    struct rx_securityClass klass;
    rxgk_level level;
    struct rxgk_keyblock k0;
    int32_t kvno;
    RXGK_Token ticket;
    uint32_t serviceId;
} rxgk_clnt_class;

/* Per connection specific client data */
typedef struct clnt_con_data {
    RXGK_Token auth_token;
    int32_t auth_token_kvno;
    int64_t start_time;
    struct rxgk_keyblock tk;
    key_stuff k;
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
    cdat->auth_token.len = obj->ticket.len;
    cdat->auth_token.val = obj->ticket.val;
    cdat->start_time = time(NULL);

    ret = rxgk_derive_transport_key(&obj->k0, &cdat->tk,
				    con->epoch, con->cid, cdat->start_time);
    if (ret) {
	return ret;
    }

    ret = rxgk_crypto_init(&cdat->tk, &cdat->k);

    if (ret) {
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
    struct RXGK_Challenge c;
    struct RXGK_Response r;
    clnt_con_data *cdat = (clnt_con_data *)con->securityData;
    size_t len;
    char bufr[RXGK_RESPONSE_MAX_SIZE];
    char *p;
    struct RXGK_Response_Crypt rc;
    RXGK_Token rc_clear, rc_crypt;
    int ret;
    int i;

    memset(&r, 0, sizeof(r));

    /* Get challenge */
    if (rx_SlowReadPacket(pkt, 0, sizeof(c), &c) != sizeof(c))
	return RXGKPACKETSHORT;
    
    if (ntohl(c.rc_version) != RXGK_VERSION)
	return RXGKINCONSISTENCY;

    memset(&rc, 0, sizeof(rc));
    memcpy(rc.nonce, c.rc_nonce, sizeof(rc.nonce));
    rc.epoch = con->epoch;
    rc.cid = con->cid & RX_CIDMASK;

    rxi_GetCallNumberVector(con, rc.call_numbers);

    for (i = 0; i < RX_MAXCALLS; i++) {
	if (rc.call_numbers[i] < 0)
	    return RXGKINCONSISTENCY;
    }

    len = RXGK_RESPONSE_CRYPT_SIZE;
    rc_clear.val = malloc(len);
    p = ydr_encode_RXGK_Response_Crypt(&rc, rc_clear.val, &len);
    if (p == NULL)
	return RXGKINCONSISTENCY;
    rc_clear.len = RXGK_RESPONSE_CRYPT_SIZE - len;

    ret = rxgk_encrypt_buffer(&rc_clear, &rc_crypt,
			      &cdat->tk, RXGK_CLIENT_ENC_RESPONSE);
    if (ret) {
	free(rc_clear.val);
	return RXGKINCONSISTENCY;
    }

    r.rr_version = RXGK_VERSION;
    r.rr_authenticator.val = cdat->auth_token.val;
    r.rr_authenticator.len = cdat->auth_token.len;
    r.rr_ctext.val = rc_crypt.val;
    r.rr_ctext.len = rc_crypt.len;
    r.start_time = cdat->start_time;

    len = sizeof(bufr);
    p = ydr_encode_RXGK_Response(&r, bufr, &len);
    len = sizeof(bufr) - len;

    if (p == NULL)
	return RXGKINCONSISTENCY;

    if (rx_SlowWritePacket(pkt, 0, len, bufr) != len)
	return RXGKPACKETSHORT;
    rx_SetDataSize(pkt, len);

    free(rc_crypt.val);
    free(rc_clear.val);

#if 0
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    key_stuff *k = &obj->k;
    krb5_data data;
    int ret;
    
    
    memcpy(rc.nonce, c.rc_nonce, sizeof(rc.nonce));


    ret = krb5_encrypt(rxgk_krb5_context, k->ks_crypto, 
		       RXGK_CLIENT_ENC_CHALLENGE, bufrc, len, &data);
    if (ret)
	return ret;

    krb5_data_free(&data);

#endif
    return 0;
}

/*
 * Checksum and/or encrypt packet.
 */
static int
client_PreparePacket(struct rx_securityClass *obj_,
		     struct rx_call *call,
		     struct rx_packet *pkt)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    struct rx_connection *con = rx_ConnectionOf(call);
    clnt_con_data *cdat = (clnt_con_data *) con->securityData;
        
    return rxgk_prepare_packet(pkt, con, obj->level, &cdat->k, &cdat->e,
			       RXGK_CLIENT_ENC_PACKET,
			       RXGK_CLIENT_MIC_PACKET);
}

/*
 * Verify checksums and/or decrypt packet.
 */
static int
client_CheckPacket(struct rx_securityClass *obj_,
		   struct rx_call *call,
		   struct rx_packet *pkt)
{
    rxgk_clnt_class *obj = (rxgk_clnt_class *) obj_;
    struct rx_connection *con = rx_ConnectionOf(call);
    clnt_con_data *cdat = (clnt_con_data *) con->securityData;

    return rxgk_check_packet(pkt, con, obj->level, &cdat->k, &cdat->e,
			     RXGK_SERVER_ENC_PACKET,
			     RXGK_SERVER_MIC_PACKET);
}

static int
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

static struct rx_securityOps client_ops = {
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

struct rx_securityClass *
rxgk_NewClientSecurityObject (rxgk_level level,
			      RXGK_Ticket_Crypt *token, 
			      struct rxgk_keyblock *key)
{
    rxgk_clnt_class *obj;
    static int inited = 0;
    
    if (rxgk_krb5_context == NULL) {
	krb5_init_context(&rxgk_krb5_context);
    }

    if (!inited) {
	rx_SetEpoch(17);
	inited = 1;
    }
    
    obj = (rxgk_clnt_class *) osi_Alloc(sizeof(rxgk_clnt_class));
    obj->klass.refCount = 1;
    obj->klass.ops = &client_ops;
    
    obj->klass.privateData = (char *)obj;
    
    obj->level = level;
    
    obj->k0.enctype = key->enctype;
    obj->k0.length = key->length;
    obj->k0.data = malloc(key->length);
    if (obj->k0.data == NULL) {
	osi_Free(obj, sizeof(rxgk_clnt_class));
	return NULL;
    }
    memcpy(obj->k0.data, key->data, key->length);

    obj->ticket.len = token->len;
    obj->ticket.val = osi_Alloc(token->len);
    memcpy(obj->ticket.val, token->val, obj->ticket.len);
    
    return &obj->klass;
}
