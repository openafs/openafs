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

/* @(#)$Id$ */

#ifndef __RXKAD_LOCL_H
#define __RXKAD_LOCL_H

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>
#include <netinet/in.h>

#ifdef NDEBUG
#ifndef assert
#define assert(e) ((void)0)
#endif
#else
#ifndef assert
#define assert(e) ((e) ? (void)0 : (void)osi_Panic("assert(%s) failed: file %s, line %d\n", #e, __FILE__, __LINE__, #e))
#endif
#endif

#include <des.h>

#undef RCSID
#include <rx/rx.h>
#undef RCSID
#define RCSID(msg) \
static /**/const char *const rcsid[] = { (char *)rcsid, "\100(#)" msg }

extern int rx_epoch, rx_nextCid;

#include "rxkad.h"
#include "rxkad_errs.h"

#define rxkad_disipline 3

#define rxkad_unallocated 1
#define rxkad_authenticated 2
#define rxkad_expired 4
#define rxkad_checksummed 8

#define ROUNDS 16

#define ROUNDS 16
typedef afs_int32 fc_KeySchedule[ROUNDS];

int fc_keysched(const void *key_, afs_int32 sched[ROUNDS]);

/* In_ and out_ MUST be afs_uint32 aligned */
int fc_ecb_encrypt(const void *in_, void *out_,
		   const afs_int32 sched[ROUNDS], int encrypt);

/* In_ and out_ MUST be afs_uint32 aligned */
int fc_cbc_encrypt(const void *in_, void *out_, afs_int32 length,
		   const afs_int32 sched[ROUNDS], afs_uint32 iv[2],
		   int encrypt);

int rxkad_EncryptPacket(const void *rx_connection_not_used,
			const afs_int32 sched[ROUNDS], const afs_uint32 iv[2],
			int len, struct rx_packet *packet);

int rxkad_DecryptPacket(const void *rx_connection_not_used,
			const afs_int32 sched[ROUNDS], const afs_uint32 iv[2],
			int len, struct rx_packet *packet);

#ifdef __GNUC__
static inline
void
fc_cbc_enc2(const void *in, void *out, afs_int32 length, const afs_int32 sched[ROUNDS],
	    const afs_uint32 iv_[2], int encrypt)
{
  afs_uint32 iv[2];
  iv[0] = iv_[0];
  iv[1] = iv_[1];
  fc_cbc_encrypt(in, out, length, sched, iv, encrypt);
}
#else
#define fc_cbc_enc2(in, out, length, sched, iv_, encrypt) \
{ afs_uint32 _iv_[2]; afs_uint32 *_tmp_ = (iv_); \
  memcpy(_iv_, _tmp_, 8);  \
  fc_cbc_encrypt((in), (out), (length), (sched), (_iv_), (encrypt)); }
#endif

#define RXKAD_VERSION 2

/* Version 2 challenge format */
typedef struct rxkad_challenge {
  afs_int32 version;
  afs_int32 nonce;
  afs_int32 min_level;
  afs_int32 unused;
} rxkad_challenge;

/* To protect the client from being used as an oracle the response
 * contains connection specific information. */
typedef struct rxkad_response {
  afs_int32 version;
  afs_int32 unused;
  struct {
    afs_int32 epoch;
    afs_int32 cid;
    afs_uint32 cksum;		/* Cksum of this response */
    afs_int32 security_index;
    afs_int32 call_numbers[RX_MAXCALLS];
    afs_int32 inc_nonce;
    afs_int32 level;
  } encrypted;
  afs_int32 kvno;
  afs_int32 ticket_len;
  /* u_char the_ticket[ticket_len]; */
} rxkad_response;

typedef struct key_stuff {
  afs_int32 keysched[ROUNDS];
  struct ktc_encryptionKey key;
} key_stuff;

typedef struct end_stuff {
  afs_uint32 header_iv[2];
  afs_uint32 bytesReceived, packetsReceived, bytesSent, packetsSent;
} end_stuff;

afs_uint32
rxkad_cksum_response(rxkad_response *r);

void
rxkad_calc_header_iv(const struct rx_connection *conn,
		     const afs_int32 sched[ROUNDS],
		     const struct ktc_encryptionKey *in_iv,
		     afs_uint32 out_iv[2]);

int
rxkad_prepare_packet(struct rx_packet *pkt, struct rx_connection *con,
		     int level, key_stuff *k, end_stuff *e);

int
rxkad_check_packet(struct rx_packet *pkt, struct rx_connection *con,
		   int level, key_stuff *k, end_stuff *e);

/* Per connection specific server data */
typedef struct serv_con_data {
  end_stuff e;
  key_stuff k;
  afs_uint32 expires;
  afs_int32 nonce;
  struct ktc_principal *user;
  afs_int32 kvno;
  rxkad_level cur_level;	/* Starts at min_level and can only increase */
  char authenticated;
} serv_con_data;


int
decomp_ticket(char *data,
	      int length,
	      char *pname,
	      char *pinstance,
	      char *prealm,
	      afs_int32 *paddress,
	      unsigned char *session,
	      afs_int32 *life,
	      afs_int32 *time_sec,
	      struct ktc_encryptionKey *key,
	      des_key_schedule schedule);


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


void
_rxkad_crc_init_table(void);

afs_uint32
_rxkad_crc_update (const char *p, size_t len, afs_uint32 res);


#endif /* __RXKAD_LOCL_H */
