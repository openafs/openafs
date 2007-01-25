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

RCSID("$Id$");

#include <errno.h>

#include "rxgk_proto.ss.h"

/* Security object specific server data */
typedef struct rxgk_serv_class {
    struct rx_securityClass klass;
    rxgk_level min_level;
    char *service_name;

    int (*get_key)(void *, const char *, int, int, krb5_keyblock *);
    int (*user_ok)(const char *name, const char *realm, int kvno);
    uint32_t serviceId;

    struct rxgk_server_params params;
} rxgk_serv_class;

/* Per connection specific server data */
typedef struct serv_con_data {
    end_stuff e;
    key_stuff k;
    uint32_t expires;
    char nonce[20];
    rxgk_level cur_level;	/* Starts at min_level and can only increase */
    char authenticated;
    struct rxgk_ticket ticket;
    struct rxgk_keyblock tk;
} serv_con_data;

extern krb5_crypto rxgk_crypto;

static int
server_NewConnection(struct rx_securityClass *obj, struct rx_connection *con)
{
    serv_con_data *cdat;
    assert(con->securityData == 0);
    obj->refCount++;
    con->securityData = (char *) osi_Alloc(sizeof(serv_con_data));
    memset(con->securityData, 0x0, sizeof(serv_con_data));
    cdat = (serv_con_data *)con->securityData;
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
    int i;

    for (i = 0; i < sizeof(cdat->nonce)/sizeof(cdat->nonce[0]); i++)
	cdat->nonce[i] = 17; /* XXX */
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
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    struct RXGK_Challenge c;

    c.rc_version = htonl(RXGK_VERSION);
    memcpy(c.rc_nonce, cdat->nonce, sizeof(c.rc_nonce));

    /* Stuff into packet */
    if (rx_SlowWritePacket(pkt, 0, sizeof(c), &c) != sizeof(c))
	return RXGKPACKETSHORT;
    rx_SetDataSize(pkt, sizeof(c));

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
    int ret;
    serv_con_data *cdat = (serv_con_data *) con->securityData;
    struct RXGK_Response_Crypt rc;
    struct RXGK_Response r;
    char response[RXGK_RESPONSE_MAX_SIZE];
    size_t len, len2;
    RXGK_Token rc_clear, rc_crypt;
    struct rxgk_keyblock k0;
    const char *p;
    int i;

    memset(&r, 0, sizeof(r));
    
    len = rx_SlowReadPacket(pkt, 0, sizeof(response), response);
    if (len <= 0)
	return RXGKPACKETSHORT;
    
    len2 = len;
    if (ydr_decode_RXGK_Response(&r, response, &len2) == NULL) {
	ret = RXGKPACKETSHORT;
	goto out;
    }

    ret = rxgk_decrypt_ticket(&r.rr_authenticator, &cdat->ticket);
    if (ret) {
	ret = RXGKPACKETSHORT;
	goto out;
    }

    k0.data = cdat->ticket.key.val;
    k0.length = cdat->ticket.key.len;
    k0.enctype = cdat->ticket.enctype;

    ret = rxgk_derive_transport_key(&k0, &cdat->tk,
				    con->epoch, con->cid, r.start_time);
    if (ret) {
	return ret;
    }

    rc_crypt.val = r.rr_ctext.val;
    rc_crypt.len = r.rr_ctext.len;

    ret = rxgk_decrypt_buffer(&rc_crypt, &rc_clear,
			      &cdat->tk, RXGK_CLIENT_ENC_RESPONSE);
    if (ret) {
	free(cdat->tk.data);
	return RXGKPACKETSHORT;
    }

    len = rc_clear.len;
    p = ydr_decode_RXGK_Response_Crypt(&rc, rc_clear.val, &len);
    if (p == NULL)
	return RXGKPACKETSHORT;

    if (rc.epoch != con->epoch ||
	rc.cid != (con->cid & RX_CIDMASK)) {
	return RXGKPACKETSHORT;
    }

    for (i = 0; i < RX_MAXCALLS; i++) {
	if (rc.call_numbers[i] < 0) {
	    return RXGKSEALEDINCON;
	}
    }

    rxi_SetCallNumberVector(con, rc.call_numbers);

    if (memcmp(rc.nonce, cdat->nonce, sizeof(rc.nonce)) != 0) {
	return RXGKSEALEDINCON;
    }

    ret = rxgk_crypto_init(&cdat->tk, &cdat->k);

    if (ret)
	return ret;

#if 0
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

#endif
 out:  

#if 0
    ydr_free_rxgk_ticket(&c);
    ydr_free_RXGK_Response(&r);
#endif
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

    return rxgk_prepare_packet(pkt, con, cdat->cur_level, &cdat->k, &cdat->e,
			       RXGK_SERVER_ENC_PACKET,
			       RXGK_SERVER_MIC_PACKET);
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

    if (time(0) > cdat->expires)	/* Use fast time package instead??? */
	return RXGKEXPIRED;

    return rxgk_check_packet(pkt, con, cdat->cur_level, &cdat->k, &cdat->e,
			     RXGK_CLIENT_ENC_PACKET,
			     RXGK_CLIENT_MIC_PACKET);
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
    struct rxgk_serv_class *serv_class = (struct rxgk_serv_class *)obj_->privateData;

    if (service->serviceId == RXGK_SERVICE_ID)
	return 0;
    
    if (!reuse) {
	struct rx_securityClass *sec[2];
	struct rx_service *secservice;
	
	sec[0] = rxnull_NewServerSecurityObject();
	sec[1] = NULL;
	
	secservice = rx_NewService (service->servicePort,
				    RXGK_SERVICE_ID,
				    "rxgk", 
				    sec, 1, 
				    RXGK_ExecuteRequest);
	
	secservice->destroyConnProc = free_context;
	rx_setServiceRock(secservice, &serv_class->params);
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
rxgk_NewServerSecurityObject(rxgk_level min_level,
			     const char *gss_service_name,
			     struct rxgk_server_params *params)
{
    rxgk_serv_class *obj;

    if (rxgk_krb5_context == NULL) {
	krb5_init_context(&rxgk_krb5_context);
    }

    if (gss_service_name == NULL)
	return NULL;

    obj = (rxgk_serv_class *) osi_Alloc(sizeof(rxgk_serv_class));
    obj->klass.refCount = 1;
    obj->klass.ops = &server_ops;
    obj->klass.privateData = (char *) obj;
    obj->params = *params;
    
    obj->min_level = min_level;
    obj->service_name = strdup(gss_service_name);
    if (obj->service_name == NULL) {
	osi_Free(obj, sizeof(*obj));
	return NULL;
    }
    
    return &obj->klass;
}
