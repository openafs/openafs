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

/* $Id$ */

#ifndef __RXGK_LOCL_H
#define __RXGK_LOCL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_AFSCONFIG_H
#include <afsconfig.h>
#endif
#ifdef HAVE_AFS_PARAM_H
#include <afs/param.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>
#include <netinet/in.h>

#ifdef SHISHI_KRB5
#include <shishi.h>
#elif defined(KSSL_KRB5) /* XXX to be removed */
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#undef u
#include <krb5.h>
#endif

#include <errno.h>
#include "rxgk_err.h"

#include "rxgk_proto.h"

#ifdef NDEBUG
#ifndef assert
#define assert(e) ((void)0)
#endif
#else
#ifndef assert
#define assert(e) ((e) ? (void)0 : (void)osi_Panic("assert(%s) failed: file %s, line %d\n", #e, __FILE__, __LINE__, #e))
#endif
#endif

#undef RCSID
#include <rx/rx.h>
#include <rx/rx_null.h>
#undef RCSID
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

extern int rx_epoch, rx_nextCid;

#include "rxgk.h"

#define rxgk_disipline 3

#define rxgk_unallocated 1
#define rxgk_authenticated 2
#define rxgk_expired 4
#define rxgk_checksummed 8

krb5_context rxgk_krb5_context;

typedef struct key_stuff {
//    krb5_keyblock	ks_key;
//    uint32_t		ks_recv_seqnum;
//    krb5_keyblock	ks_skey;
//    krb5_crypto		ks_crypto;	/* rx session key */
    krb5_crypto		ks_scrypto;	/* rx stream key */
} key_stuff;

typedef struct end_stuff {
    /* need 64 bit counters */
    uint32_t bytesReceived, packetsReceived, bytesSent, packetsSent;
} end_stuff;

extern int rxgk_key_contrib_size;

int
rxgk_prepare_packet(struct rx_packet *pkt, struct rx_connection *con,
		    int level, key_stuff *k, end_stuff *e,
		    int keyusage_enc, int keyusage_mic);

int
rxgk_check_packet(struct rx_packet *pkt, struct rx_connection *con,
		  int level, key_stuff *k, end_stuff *e,
		  int keyusage_enc, int keyusage_mic);

int
rxgk_crypto_init(struct rxgk_keyblock *tk, key_stuff *k);

/* rxgk */

#if 0
int
rxgk5_get_auth_token(krb5_context context, uint32_t addr, int port, 
		     uint32_t serviceId,
		     RXGK_Token *token,
		     RXGK_Token *auth_token, krb5_keyblock *key,
		     krb5_keyblock *skey,
		     int32_t *kvno);
#endif

int
rxk5_mutual_auth_client_generate(krb5_context context, krb5_keyblock *key,
				 uint32_t number,
				 RXGK_Token *challage_token);
int
rxk5_mutual_auth_client_check(krb5_context context, krb5_keyblock *key,
			      uint32_t number,
			      const RXGK_Token *reply_token,
			      krb5_keyblock *rxsession_key);
int
rxk5_mutual_auth_server(krb5_context context, krb5_keyblock *key,
			const RXGK_Token *challage_token,
			int *enctype, 
			void **session_key, size_t *session_key_size,
			RXGK_Token *reply_token);

int
rxgk_set_conn(struct rx_connection *, int, int);

void
rxgk_getheader(struct rx_packet *pkt, struct rxgk_header_data *h);

int
rxgk_decode_auth_token(void *data, size_t len, struct rxgk_ticket *ticket);

int
rxgk_server_init(void);

#if 0
int
rxgk_derive_transport_key(krb5_context context,
			  krb5_keyblock *rx_conn_key,
			  RXGK_rxtransport_key *keycontrib,
			  krb5_keyblock *rkey);
#endif

int
rxgk_random_to_key(int, void *, int, krb5_keyblock *);

int
rxgk_make_ticket(struct rxgk_server_params *params,
		 gss_ctx_id_t ctx,
		 const void *snonce, size_t slength,
		 const void *cnonce, size_t clength,
		 RXGK_Ticket_Crypt *token,
		 int32_t enctype);

int
rxgk_encrypt_ticket(struct rxgk_ticket *ticket, RXGK_Ticket_Crypt *opaque);

int
rxgk_decrypt_ticket(RXGK_Ticket_Crypt *opaque, struct rxgk_ticket *ticket);

void
_rxgk_gssapi_err(OM_uint32, OM_uint32, gss_OID);

int
rxgk_derive_k0(gss_ctx_id_t ctx,
	       const void *snonce, size_t slength,
	       const void *cnonce, size_t clength,
	       int32_t enctype, struct rxgk_keyblock *key);

void
print_chararray(char *val, unsigned len);

int
rxgk_encrypt_buffer(RXGK_Token *in, RXGK_Token *out,
		    struct rxgk_keyblock *key, int keyusage);

int
rxgk_decrypt_buffer(RXGK_Token *in, RXGK_Token *out,
		    struct rxgk_keyblock *key, int keyusage);

int
rxgk_get_server_ticket_key(struct rxgk_keyblock *key);

int
rxgk_derive_transport_key(struct rxgk_keyblock *k0,
			  struct rxgk_keyblock *tk,
			  uint32_t epoch, uint32_t cid, int64_t start_time);

#endif /* __RXGK_LOCL_H */
