/*
 * Copyright (c) 1995 - 1998, 2002 Kungliga Tekniska Högskolan
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

#include <errno.h>

#include "rxgk_proto.ss.h"

/* Security object specific server data */
typedef struct rxgk_serv_class {
    struct rx_securityClass klass;
    rxgk_level min_level;
    char *principal;
    char *service_principal;
    void *appl_data;
    int (*get_key)(void *, const char *, int, int, krb5_keyblock *);
    int (*user_ok)(const char *name, const char *realm, int kvno);
    uint32_t serviceId;
} rxgk_serv_class;

extern krb5_context gk_context;
extern krb5_crypto gk_crypto;

static int
server_NewConnection(struct rx_securityClass *obj, struct rx_connection *con)
{
    serv_con_data *cdat;
    assert(con->securityData == 0);
    assert(gk_context != NULL);
    obj->refCount++;
    con->securityData = (char *) osi_Alloc(sizeof(serv_con_data));
    memset(con->securityData, 0x0, sizeof(serv_con_data));
    cdat = (serv_con_data *)con->securityData;
    cdat->k.ks_context = gk_context;
    return 0;
}

static int
server_Close(struct rx_securityClass *obj)
{
    obj->refCount--;
    if (obj->refCount <= 0)
	osi_Free(obj, sizeof(rxgk_serv_class));
    return 0;
}

static
int
server_DestroyConnection(struct rx_securityClass *obj,
			 struct rx_connection *con)
{
  serv_con_data *cdat = (serv_con_data *)con->securityData;

  if (cdat)
      osi_Free(cdat, sizeof(serv_con_data));
  return server_Close(obj);
}

/*
 * Check whether a connection authenticated properly.
 * Zero is good (authentication succeeded).
 */
static int
server_CheckAuthentication(struct rx_securityClass *obj,
			   struct rx_connection *con)
{
    serv_con_data *cdat = (serv_con_data *) con->securityData;

    if (cdat)
	return !cdat->authenticated;
    else
	return RXGKNOAUTH;
}

/*
 * Select a nonce for later use.
 */
static
int
server_CreateChallenge(struct rx_securityClass *obj_,
		       struct rx_connection *con)
{
    serv_con_data *cdat = (serv_con_data *) con->securityData;

    cdat->nonce = arc4random();
    cdat->authenticated = 0;
    return 0;
}

/*
 * Wrap the nonce in a challenge packet.
 */
static int
server_GetChallenge(const struct rx_securityClass *obj_,
		    const struct rx_connection *con,
		    struct rx_packet *pkt)
{
    rxgk_serv_class *obj = (rxgk_serv_class *) obj_;
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    struct RXGK_Challenge c;
    int initial_challage = 1;

    c.rc_version = htonl(RXGK_VERSION);
    c.rc_nonce = htonl(cdat->nonce);
    c.rc_max_seq_skew = htonl(200); /* XXX */
    c.rc_min_level = htonl(obj->min_level);

    if (initial_challage) {
	/* Make challenge */
	c.rc_opcode = htonl(RXKG_OPCODE_CHALLENGE);
    
	/* Stuff into packet */
	if (rx_SlowWritePacket(pkt, 0, sizeof(c), &c) != sizeof(c))
	    return RXGKPACKETSHORT;
	rx_SetDataSize(pkt, sizeof(c));
#if 0
    } else {
	RXGK_ReKey rk;
	RXGK_ReKey_Crypt rkc;
	char bufrk[RXGK_REKEY_MAX_SIZE];
	char bufrkc[RXGK_REKEY_CRYPT_MAX_SIZE];
	krb5_data data;
	size_t sz;
	int ret;
	key_stuff *k = &cdat->k;

	memset(&rk, 0, sizeof(rk));
	memset(&rkc, 0, sizeof(rkc));

	c.rc_opcode = htonl(RXKG_OPCODE_REKEY);

	if (rx_SlowWritePacket(pkt, 0, sizeof(c), &c) != sizeof(c))
	    return RXGKPACKETSHORT;
	
	rkc.rkc_version = RXGK_VERSION;
	rkc.rkc_max_seq_num = 200; /* XXX */
	rkc.rkc_kvno = /* current_key + 1 */ 1;
	rkc.rkc_keycontribution.len = 0; /* XXX */
	rkc.rkc_keycontribution.val = NULL;

	sz = RXGK_REKEY_CRYPT_MAX_SIZE;
	if (ydr_encode_RXGK_ReKey_Crypt(&rkc, bufrkc, &sz) == NULL)
	    return RXGKPACKETSHORT;
	sz = RXGK_REKEY_CRYPT_MAX_SIZE - sz;
	
	ret = krb5_encrypt(k->ks_context, k->ks_crypto, 
			   RXGK_SERVER_ENC_REKEY, bufrkc, sz, &data);
	if (ret)
	    return ret;

	rk.rk_ctext.val = data.data;
	rk.rk_ctext.len = data.length;

	sz = RXGK_REKEY_MAX_SIZE;
	if (ydr_encode_RXGK_ReKey(&rk, bufrk, &sz) == NULL) {
	    krb5_data_free(&data);
	    return RXGKPACKETSHORT;
	}
	sz = RXGK_REKEY_MAX_SIZE - sz;

	krb5_data_free(&data);
	
	if (rx_SlowWritePacket(pkt, sizeof(c), sz, bufrk) != sz)
	    return RXGKPACKETSHORT;

	rx_SetDataSize(pkt, sizeof(c) + sz);
#endif
    }
    return 0;
}

/*
 * Process a response to a challange.
 */
static int
server_CheckResponse(struct rx_securityClass *obj_,
		     struct rx_connection *con,
		     struct rx_packet *pkt)
{
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    
    struct RXGK_Response r;
    struct RXGK_Response_Crypt rc;
    struct RXGK_AUTH_CRED c;
    char response[RXGK_RESPONSE_MAX_SIZE];
    size_t len, len2;
    int ret;
    krb5_context context = cdat->k.ks_context;
    krb5_data data;
    
    memset(&r, 0, sizeof(r));
    memset(&c, 0, sizeof(r));
    
    len = rx_SlowReadPacket(pkt, 0, sizeof(response), response);
    if (len <= 0)
	return RXGKPACKETSHORT;
    
    len2 = len;
    if (ydr_decode_RXGK_Response(&r, response, &len2) == NULL) {
	ret = RXGKPACKETSHORT;
	goto out;
    }
    
    ret = rxgk_decode_auth_token(r.rr_auth_token.val, r.rr_auth_token.len, &c);
    if (ret)
	goto out;
    
    ret = rxgk_random_to_key(c.ac_enctype, c.ac_key.val, c.ac_key.len,
			     &cdat->k.ks_key);
    if (ret)
	goto out;

    cdat->k.ks_crypto = NULL; /* XXX */
    ret = krb5_crypto_init(context, &cdat->k.ks_key, cdat->k.ks_key.keytype, 
			   &cdat->k.ks_crypto);
    if (ret)
	goto out2;


    ret = krb5_decrypt(context, cdat->k.ks_crypto, RXGK_CLIENT_ENC_CHALLENGE, 
		       r.rr_ctext.val, r.rr_ctext.len, &data);
    if (ret)
	goto out2;

    len = data.length;
    if (ydr_decode_RXGK_Response_Crypt(&rc, data.data, &len) == NULL) {
	krb5_data_free(&data);
	goto out2;
    }

    krb5_data_free(&data);

    if (rc.epoch != con->epoch
	|| rc.cid != (con->cid & RX_CIDMASK)
#if 0
	|| rc.security_index != con->securityIndex
#endif
	) {
	ret = RXGKSEALEDINCON;
	goto out2;
    }

    {
	int i;
	for (i = 0; i < RX_MAXCALLS; i++)
	{
	    if (rc.call_numbers[i] < 0) {
		ret = RXGKSEALEDINCON;
		goto out2;
	    }
	}

    }

    if (rc.nonce != cdat->nonce + 1) {
	ret = RXGKOUTOFSEQUENCE;
	goto out2;
    }

    /* XXX */
    if (rc.level != rxgk_crypt) {
	ret = RXGKLEVELFAIL;
	goto out2;
    }

    if ((rc.level != rxgk_auth && rc.level != rxgk_crypt) ||
	rc.level < cdat->cur_level) 
    {
	ret = RXGKLEVELFAIL;
	goto out2;
    }

#if 0
    ret = rxgk_derive_transport_key(context, &cdat->k.ks_key,
				    &rc.contrib, &cdat->k.ks_skey);
    if (ret)
	goto out2;
#endif

    ret = krb5_crypto_init (context, &cdat->k.ks_skey,
			    cdat->k.ks_skey.keytype,
			    &cdat->k.ks_scrypto);
    if (ret)
	goto out2;

    rxi_SetCallNumberVector(con, rc.call_numbers);

    cdat->authenticated = 1;
    cdat->expires = c.ac_endtime;
    cdat->cur_level = rc.level;

    rxgk_set_conn(con, cdat->k.ks_key.keytype,
		  rc.level == rxgk_crypt ? 1 : 0);

 out2:
    if (ret) {
	krb5_free_keyblock_contents(context, &cdat->k.ks_key);
	if (cdat->k.ks_crypto)
	    krb5_crypto_destroy(context, cdat->k.ks_crypto);
	cdat->k.ks_crypto = NULL;
    }

 out:  

    ydr_free_RXGK_AUTH_CRED(&c);
    ydr_free_RXGK_Response(&r);
    
    return ret;
}

/*
 * Checksum and/or encrypt packet
 */
static int
server_PreparePacket(struct rx_securityClass *obj_,
		     struct rx_call *call,
		     struct rx_packet *pkt)
{
    struct rx_connection *con = rx_ConnectionOf(call);
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    key_stuff *k = &cdat->k;
    end_stuff *e = &cdat->e;
    
    return rxgk_prepare_packet(pkt, con, cdat->cur_level, k, e);
}

/*
 * Verify checksum and/or decrypt packet.
 */
static int
server_CheckPacket(struct rx_securityClass *obj_,
		   struct rx_call *call,
		   struct rx_packet *pkt)
{
    struct rx_connection *con = rx_ConnectionOf(call);
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    key_stuff *k = &cdat->k;
    end_stuff *e = &cdat->e;

    if (time(0) > cdat->expires)	/* Use fast time package instead??? */
	return RXGKEXPIRED;

    return rxgk_check_packet(pkt, con, cdat->cur_level, k, e);
}

static int
server_GetStats(const struct rx_securityClass *obj_,
		const struct rx_connection *con,
		struct rx_securityObjectStats *st)
{
    rxgk_serv_class *obj = (rxgk_serv_class *) obj_;
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    
    st->type = rxgk_disipline;
    st->level = obj->min_level;
    st->flags = rxgk_checksummed;
    if (cdat == 0)
	st->flags |= rxgk_unallocated;
    {
	st->bytesReceived = cdat->e.bytesReceived;
	st->packetsReceived = cdat->e.packetsReceived;
	st->bytesSent = cdat->e.bytesSent;
	st->packetsSent = cdat->e.packetsSent;
	st->expires = cdat->expires;
	st->level = cdat->cur_level;
	if (cdat->authenticated)
	    st->flags |= rxgk_authenticated;
    }
    return 0;
}

static
void
free_context(void)
{
    return;
}

static
int
server_NewService(const struct rx_securityClass *obj_,
		  struct rx_service *service,
		  int reuse)
{
    rxgk_serv_class *obj = (rxgk_serv_class *) obj_;

    if (service->serviceId == obj->serviceId)
	return 0;
    
    if (!reuse) {
	struct rx_securityClass *sec[2];
	struct rx_service *secservice;
	
	sec[0] = rxnull_NewServerSecurityObject();
	sec[1] = NULL;
	
	secservice = rx_NewService (service->servicePort,
				    obj->serviceId,
				    "rxgk", 
				    sec, 1, 
				    RXGK_ExecuteRequest);
	
	secservice->destroyConnProc = free_context;
	rx_setServiceRock(secservice, obj->principal);
    }
    return 0;
}


static struct rx_securityOps server_ops = {
    server_Close,
    server_NewConnection,
    server_PreparePacket,
    NULL,
    server_CheckAuthentication,
    server_CreateChallenge,
    server_GetChallenge,
    NULL,
    server_CheckResponse,
    server_CheckPacket,
    server_DestroyConnection,
    server_GetStats,
    server_NewService,
};

struct rx_securityClass *
rxgk_NewServerSecurityObject(/*rxgk_level*/ int min_level,
			     const char *principal,
			     void *appl_data,
			     int (*get_key)(void *data, const char *principal,
					    int enctype, int kvno,
					    krb5_keyblock *key),
			     int (*user_ok)(const char *name,
					    const char *realm,
					    int kvno),
			     uint32_t serviceId)
{
    rxgk_serv_class *obj;
    int ret;

    if (get_key == NULL || principal == NULL)
	return NULL;

    ret = rxgk_server_init();
    if (ret)
	return NULL;

    obj = (rxgk_serv_class *) osi_Alloc(sizeof(rxgk_serv_class));
    obj->klass.refCount = 1;
    obj->klass.ops = &server_ops;
    obj->klass.privateData = (char *) obj;
    
    obj->min_level = min_level;
    obj->appl_data = appl_data;
    obj->get_key = get_key;
    obj->user_ok = user_ok;
    obj->principal = strdup(principal);
    if (obj->principal == NULL) {
	osi_Free(obj, sizeof(rxgk_serv_class));
	return NULL;
    }
    obj->serviceId = serviceId;
    
    return &obj->klass;
}
