/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

#include "rxkad_locl.h"

#if defined(KRB5)
#include <krb5.h>
#endif

RCSID("$Id$");

/* Security object specific server data */
typedef struct rxkad_serv_class {
  struct rx_securityClass klass;
  rxkad_level min_level;
  void *appl_data;
  int (*get_key)(void *appl_data, int kvno, struct ktc_encryptionKey *key);
  int (*user_ok)(char *name, char *inst, char *realm, int kvno);
} rxkad_serv_class;

static
int
server_NewConnection(struct rx_securityClass *obj, struct rx_connection *con)
{
  assert(con->securityData == 0);
  obj->refCount++;
  con->securityData = (char *) rxi_Alloc(sizeof(serv_con_data));
  memset(con->securityData, 0x0, sizeof(serv_con_data));
  return 0;
}

static
int
server_Close(struct rx_securityClass *obj)
{
  obj->refCount--;
  if (obj->refCount <= 0)
    rxi_Free(obj, sizeof(rxkad_serv_class));
  return 0;
}

static
int
server_DestroyConnection(struct rx_securityClass *obj,
			 struct rx_connection *con)
{
  serv_con_data *cdat = (serv_con_data *)con->securityData;

  if (cdat)
    {
      if (cdat->user)
	rxi_Free(cdat->user, sizeof(struct ktc_principal));
      rxi_Free(cdat, sizeof(serv_con_data));
    }
  return server_Close(obj);
}

/*
 * Check whether a connection authenticated properly.
 * Zero is good (authentication succeeded).
 */
static
int
server_CheckAuthentication(struct rx_securityClass *obj,
			   struct rx_connection *con)
{
  serv_con_data *cdat = (serv_con_data *) con->securityData;

  if (cdat)
    return !cdat->authenticated;
  else
    return RXKADNOAUTH;
}

/*
 * Select a nonce for later use.
 */
static
int
server_CreateChallenge(struct rx_securityClass *obj_,
		       struct rx_connection *con)
{
  rxkad_serv_class *obj = (rxkad_serv_class *) obj_;
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  union {
    afs_uint32 rnd[2];
    struct ktc_encryptionKey k;
  } u;

  /* Any good random numbers will do, no real need to use
   * cryptographic techniques here */
  /* XXX openafs !!! */
  des_random_key(&u.k);
  cdat->nonce = u.rnd[0] ^ u.rnd[1];
  cdat->authenticated = 0;
  cdat->cur_level = obj->min_level;
  return 0;
}

/*
 * Wrap the nonce in a challenge packet.
 */
static
int
server_GetChallenge(const struct rx_securityClass *obj,
		    const struct rx_connection *con,
		    struct rx_packet *pkt)
{
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  rxkad_challenge c;

  /* Make challenge */
  c.version = htonl(RXKAD_VERSION);
  c.nonce = htonl(cdat->nonce);
  c.min_level = htonl((afs_int32)cdat->cur_level);
  c.unused = 0; /* Use this to hint client we understand krb5 tickets??? */

  /* Stuff into packet */
  if (rx_SlowWritePacket(pkt, 0, sizeof(c), &c) != sizeof(c))
    return RXKADPACKETSHORT;
  rx_SetDataSize(pkt, sizeof(c));
  return 0;
}

int
rxkad_decode_krb5_ticket(int (*get_key)(void *appl_data,
					int kvno,
					struct ktc_encryptionKey *key),
			 void *appl_data,
			 int serv_kvno,
			 char *ticket,
			 afs_int32 ticket_len,
			 /* OUT parms */
			 struct ktc_encryptionKey *session_key,
			 afs_uint32 *expires,
			 struct ktc_principal *p,
			 afs_int32 *real_kvno);


static
int
decode_krb5_ticket(rxkad_serv_class *obj,
		   int serv_kvno,
		   char *ticket,
		   afs_int32 ticket_len,
		   /* OUT parms */
		   struct ktc_encryptionKey *session_key,
		   afs_uint32 *expires,
		   struct ktc_principal *p,
		   afs_int32 *real_kvno)
{
    return rxkad_decode_krb5_ticket(obj->get_key,
				    obj->appl_data,
				    serv_kvno,
				    ticket,
				    ticket_len,
				    session_key,
				    expires,
				    p,
				    real_kvno);
}

static
int
decode_krb4_ticket(rxkad_serv_class *obj,
		   int serv_kvno,
		   char *ticket,
		   afs_int32 ticket_len,
		   /* OUT parms */
		   struct ktc_encryptionKey *session_key,
		   afs_uint32 *expires,
		   struct ktc_principal *p)
{
  afs_uint32 start;
  afs_uint32 paddress;
  struct ktc_encryptionKey serv_key;		/* Service's secret key */
  des_key_schedule serv_sched;	/* Service's schedule */

  /* First get service key */
  int code = (*obj->get_key)(obj->appl_data, serv_kvno, &serv_key);
  if (code)
    return RXKADUNKNOWNKEY;

  code = decomp_ticket(ticket, ticket_len,
		       p->name, p->instance, p->cell, &paddress,
		       (char *)session_key, &start, expires,
		       &serv_key, serv_sched);
  if (code != 0)
    return RXKADBADTICKET;

#if 0
  if (paddress != ntohl(con->peer->host))
    return RXKADBADTICKET;
#endif

  if (tkt_CheckTimes (start, *expires, time(0)) < -1) 
    return RXKADBADTICKET;

  return 0;			/* Success */
}

/*
 * Process a response to a challange.
 */
static
int
server_CheckResponse(struct rx_securityClass *obj_,
		     struct rx_connection *con,
		     struct rx_packet *pkt)
{
  rxkad_serv_class *obj = (rxkad_serv_class *) obj_;
  serv_con_data *cdat = (serv_con_data *) con->securityData;

  int serv_kvno;		/* Service's kvno we used */
  afs_int32 ticket_len;
  char ticket[MAXKTCTICKETLEN];
  int code;
  rxkad_response r;
  struct ktc_principal p;
  afs_uint32 cksum;

  if (rx_SlowReadPacket(pkt, 0, sizeof(r), &r) != sizeof(r))
    return RXKADPACKETSHORT;
  
  serv_kvno = ntohl(r.kvno);
  ticket_len = ntohl(r.ticket_len);

  if (ticket_len > MAXKTCTICKETLEN)
    return RXKADTICKETLEN;

  if (rx_SlowReadPacket(pkt, sizeof(r), ticket_len, ticket) != ticket_len)
    return RXKADPACKETSHORT;

  /* Disassemble kerberos ticket */
  if (serv_kvno == RXKAD_TKT_TYPE_KERBEROS_V5 ||
      serv_kvno == RXKAD_TKT_TYPE_KERBEROS_V5_ENCPART_ONLY) {
    code = decode_krb5_ticket(obj, serv_kvno, ticket, ticket_len,
			      &cdat->k.key, &cdat->expires, &p, &cdat->kvno);
  } else {
    code = decode_krb4_ticket(obj, serv_kvno, ticket, ticket_len,
			      &cdat->k.key, &cdat->expires, &p);
    cdat->kvno = serv_kvno;
  }

  if (code != 0)
    return code;

  fc_keysched(&cdat->k.key, cdat->k.keysched);

  /* Unseal r.encrypted */
  fc_cbc_enc2(&r.encrypted, &r.encrypted, sizeof(r.encrypted),
	      cdat->k.keysched, (afs_uint32*)&cdat->k.key, DECRYPT);

  /* Verify response integrity */
  cksum = r.encrypted.cksum;
  r.encrypted.cksum = 0;
  if (r.encrypted.epoch != ntohl(con->epoch)
      || r.encrypted.cid != ntohl(con->cid & RX_CIDMASK)
      || r.encrypted.security_index != ntohl(con->securityIndex)
      || cksum != rxkad_cksum_response(&r))
    return RXKADSEALEDINCON;
  {
    int i;
    for (i = 0; i < RX_MAXCALLS; i++)
      {
	r.encrypted.call_numbers[i] = ntohl(r.encrypted.call_numbers[i]);
	if (r.encrypted.call_numbers[i] < 0)
	  return RXKADSEALEDINCON;
      }
  }

  if (ntohl(r.encrypted.inc_nonce) != cdat->nonce+1)
    return RXKADOUTOFSEQUENCE;

  {
    int level = ntohl(r.encrypted.level);
    if ((level < cdat->cur_level) || (level > rxkad_crypt))
      return RXKADLEVELFAIL;
    cdat->cur_level = level;
    /* We don't use trailers but the transarc implementation breaks if
     * we don't set the trailer size, packets get to large */
    if (level == rxkad_auth)
      {
	rx_SetSecurityHeaderSize(con, 4);
	rx_SetSecurityMaxTrailerSize(con, 4);
      }
    else if (level == rxkad_crypt)
      {
	rx_SetSecurityHeaderSize(con, 8);
	rx_SetSecurityMaxTrailerSize(con, 8);
      }
  }
  
  rxi_SetCallNumberVector(con, r.encrypted.call_numbers);

  rxkad_calc_header_iv(con, cdat->k.keysched,
		       (const struct ktc_encryptionKey *)&cdat->k.key,
		       cdat->e.header_iv);
  cdat->authenticated = 1;

  if (obj->user_ok)
    {
      code = obj->user_ok(p.name, p.instance, p.cell, serv_kvno);
      if (code)
	return RXKADNOAUTH;
    }
  else
    {
      struct ktc_principal *user = (struct ktc_principal *) rxi_Alloc(sizeof(struct ktc_principal));
      *user = p;
      cdat->user = user;
    }
  return 0;
}

/*
 * Checksum and/or encrypt packet
 */
static
int
server_PreparePacket(struct rx_securityClass *obj_,
		     struct rx_call *call,
		     struct rx_packet *pkt)
{
  struct rx_connection *con = rx_ConnectionOf(call);
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  key_stuff *k = &cdat->k;
  end_stuff *e = &cdat->e;

  return rxkad_prepare_packet(pkt, con, cdat->cur_level, k, e);
}

/*
 * Verify checksum and/or decrypt packet.
 */
static
int
server_CheckPacket(struct rx_securityClass *obj_,
		   struct rx_call *call,
		   struct rx_packet *pkt)
{
  struct rx_connection *con = rx_ConnectionOf(call);
  serv_con_data *cdat = (serv_con_data *) con->securityData;
  key_stuff *k = &cdat->k;
  end_stuff *e = &cdat->e;

  if (time(0) > cdat->expires)	/* Use fast time package instead??? */
    return RXKADEXPIRED;

  return rxkad_check_packet(pkt, con, cdat->cur_level, k, e);
}

static
int
server_GetStats(const struct rx_securityClass *obj_,
		const struct rx_connection *con,
		struct rx_securityObjectStats *st)
{
  rxkad_serv_class *obj = (rxkad_serv_class *) obj_;
  serv_con_data *cdat = (serv_con_data *) con->securityData;

  st->type = rxkad_disipline;
  st->level = obj->min_level;
  st->flags = rxkad_checksummed;
  if (cdat == 0)
    st->flags |= rxkad_unallocated;
  {
    st->bytesReceived = cdat->e.bytesReceived;
    st->packetsReceived = cdat->e.packetsReceived;
    st->bytesSent = cdat->e.bytesSent;
    st->packetsSent = cdat->e.packetsSent;
    st->expires = cdat->expires;
    st->level = cdat->cur_level;
    if (cdat->authenticated)
      st->flags |= rxkad_authenticated;
  }
  return 0;
}

static struct rx_securityOps server_ops = {
  server_Close,
  server_NewConnection,
  server_PreparePacket,
  0,
  server_CheckAuthentication,
  server_CreateChallenge,
  server_GetChallenge,
  0,
  server_CheckResponse,
  server_CheckPacket,
  server_DestroyConnection,
  server_GetStats,
};

struct rx_securityClass *
rxkad_NewServerSecurityObject(rxkad_level min_level,
			      void *appl_data,
			      int (*get_key)(void *appl_data,
					     int kvno,
					     struct ktc_encryptionKey *key),
			      int (*user_ok)(char *name,
					     char *inst,
					     char *realm,
					     int kvno))
{
  rxkad_serv_class *obj;

  if (!get_key)
    return 0;

  obj = (rxkad_serv_class *) rxi_Alloc(sizeof(rxkad_serv_class));
  obj->klass.refCount = 1;
  obj->klass.ops = &server_ops;
  obj->klass.privateData = (char *) obj;

  obj->min_level = min_level;
  obj->appl_data = appl_data;
  obj->get_key = get_key;
  obj->user_ok = user_ok;

  return &obj->klass;
}
