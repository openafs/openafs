/*
 * Copyright (c) 1995, 1996, 1997, 2002 Kungliga Tekniska Högskolan
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

#include <afsconfig.h>
#if defined(UKERNEL)
#include "../afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID("$Header$");

#if defined(UKERNEL)
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../afs/stds.h"
#include "../rx/xdr.h"
#include "../rx/rx.h"
#include "../des/des.h"
#include "../afs/lifetimes.h"
#include "../afs/rxkad.h"
#else /* defined(UKERNEL) */
#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <rx/xdr.h>
#include <rx/rx.h>
#include <des.h>
#include "lifetimes.h"
#include "rxkad.h"
#endif /* defined(UKERNEL) */

#include "v5gen-rewrite.h"
#include "asn1-common.h"
#include "der.h"
#include "v5gen.h"
#include "v5der.c"
#include "v5gen.c"

static int
krb5_des_decrypt(struct ktc_encryptionKey *, 
		 int, void *, size_t, void *, size_t *);




int tkt_DecodeTicket5(char *ticket, afs_int32 ticket_len, 
		      int (*get_key)(char *, int, struct ktc_encryptionKey *),
		      char *get_key_rock,
		      int serv_kvno,
		      char *name, 
		      char *inst, 
		      char *cell, 
		      char *session_key, 
		      afs_int32 *host, 
		      afs_int32 *start, 
		      afs_int32 *end)
{
    char plain[MAXKRB5TICKETLEN];
    struct ktc_encryptionKey serv_key;
    Ticket t5;	/* Must free */
    EncTicketPart decr_part;	/* Must free */
    int code;
    size_t siz, plainsiz;
    int v5_serv_kvno;

    memset(&t5, 0, sizeof(t5));
    memset(&decr_part, 0, sizeof(decr_part));

    *host = 0;

    if (ticket_len == 0)
	return RXKADBADTICKET; /* no ticket */

    if (serv_kvno == RXKAD_TKT_TYPE_KERBEROS_V5) {
	code = decode_Ticket(ticket, ticket_len, &t5, &siz);
	if (code != 0)
	    goto cleanup;

	if (t5.tkt_vno != 5)
	    goto bad_ticket;
    } else {
	code = decode_EncryptedData(ticket, ticket_len, &t5.enc_part, &siz);
	if (code != 0)
	    goto cleanup;
    }

    /* Find the real service key version number */
    if (t5.enc_part.kvno == NULL)
	goto bad_ticket;
    v5_serv_kvno = *t5.enc_part.kvno;
    

    code = (*get_key)(get_key_rock, v5_serv_kvno, &serv_key);
    if (code)
	goto unknown_key;

    /* Check that the key type really fit into 8 bytes */
    switch (t5.enc_part.etype) {
    case ETYPE_DES_CBC_CRC:
    case ETYPE_DES_CBC_MD4:
    case ETYPE_DES_CBC_MD5:
	break;
    default:
	goto unknown_key;
    }

    /* check ticket */
    if (t5.enc_part.cipher.length > sizeof(plain) ||
	t5.enc_part.cipher.length % 8 != 0)
	goto bad_ticket;

    /* Decrypt data here, save in plain, assume it will shrink */
    code = krb5_des_decrypt(&serv_key,
			    t5.enc_part.etype,
			    t5.enc_part.cipher.data,
			    t5.enc_part.cipher.length,
			    plain,
			    &plainsiz);
    if (code != 0)
	goto bad_ticket;

    /* Decode ticket */
    code = decode_EncTicketPart(plain, plainsiz, &decr_part, &siz);
    if (code != 0)
	goto bad_ticket;
    
    /* Extract realm and principal */  
    strncpy(cell, decr_part.crealm, MAXKTCNAMELEN);
    cell[MAXKTCNAMELEN - 1] = '\0';
    inst[0] = '\0';
    switch (decr_part.cname.name_string.len) {
    case 2:
	strncpy(inst, decr_part.cname.name_string.val[1], MAXKTCNAMELEN);
	inst[MAXKTCNAMELEN - 1] = '\0';
    case 1:
	strncpy(name, decr_part.cname.name_string.val[0], MAXKTCNAMELEN);
	name[MAXKTCNAMELEN - 1] = '\0';
	break;
    default:
	goto bad_ticket;
    }
    
    /* Verify that decr_part.key is of right type */
    switch (decr_part.key.keytype) {
    case ETYPE_DES_CBC_CRC:
    case ETYPE_DES_CBC_MD4:
    case ETYPE_DES_CBC_MD5:
	break;
    default:
	goto bad_ticket;
    }

    if (decr_part.key.keyvalue.length != 8)
	goto bad_ticket;

    /* Extract session key */
    memcpy(session_key, decr_part.key.keyvalue.data, 8);
    
    /* Check lifetimes and host addresses, flags etc */
    {
	time_t now = time(0);	/* Use fast time package instead??? */
	*start = decr_part.authtime;
	if (decr_part.starttime)
	    *start = *decr_part.starttime;
#if 0
	if (*start - now > CLOCK_SKEW || decr_part.flags.invalid)
	    goto no_auth;
#else
	if (decr_part.flags.invalid)
	    goto no_auth;
#endif
	if (now > decr_part.endtime)
	    goto tkt_expired;
	*end = decr_part.endtime;
    }

 cleanup:
    if (serv_kvno == RXKAD_TKT_TYPE_KERBEROS_V5)
	free_Ticket(&t5);
    else
	free_EncryptedData(&t5.enc_part);
    free_EncTicketPart(&decr_part);
    memset(&serv_key, 0, sizeof(serv_key));
    return code;

 unknown_key:
    code = RXKADUNKNOWNKEY;
    goto cleanup;
 no_auth:
    code = RXKADNOAUTH;
    goto cleanup;
 tkt_expired:
    code = RXKADEXPIRED;
    goto cleanup;
 bad_ticket:
    code = RXKADBADTICKET;
    goto cleanup;

}

static int
verify_checksum_crc(void *data, size_t len,
		    void *cksum, size_t cksumsz, 
		    struct ktc_encryptionKey *key)
{
    afs_uint32 crc;
    char r[4];

    _rxkad_crc_init_table ();
    crc = _rxkad_crc_update (data, len, 0);
    r[0] = crc & 0xff;
    r[1] = (crc >> 8)  & 0xff;
    r[2] = (crc >> 16) & 0xff;
    r[3] = (crc >> 24) & 0xff;
    
    if (memcmp(cksum, r, 4) != 0)
	return 1;
    return 0;
}


static int
krb5_des_decrypt(struct ktc_encryptionKey *key,
		 int etype,
		 void *in, size_t insz,
		 void *out, size_t *outsz)
{
    int (*cksum_func)(void *,size_t,void *,size_t,struct ktc_encryptionKey *);
    des_cblock ivec;
    des_key_schedule s;
    char cksum[24];
    size_t cksumsz;
    int ret;

    cksum_func = NULL;

    des_key_sched(key, &s);

#define CONFOUNDERSZ 8

    switch (etype) {
    case ETYPE_DES_CBC_CRC:
	memcpy(&ivec, key, sizeof(ivec));
	cksumsz = 4;
	cksum_func = verify_checksum_crc;
	break;
    case ETYPE_DES_CBC_MD4:
	memset(&ivec, 0, sizeof(ivec));
	cksumsz = 16;
	/* FIXME: cksum_func = verify_checksum_md4 */;
	break;
    case ETYPE_DES_CBC_MD5:
	memset(&ivec, 0, sizeof(ivec));
	cksumsz = 16;
	/* FIXME: cksum_func = verify_checksum_md5 */;
	break;
    default:
	abort();
    }

    ret = des_cbc_encrypt(in, out, insz, s, &ivec, 0);
    if (ret)
	return ret;

    memcpy(cksum, (char *)out + CONFOUNDERSZ, cksumsz);
    memset((char *)out + CONFOUNDERSZ, 0, cksumsz);

    if (cksum_func)
	ret = (*cksum_func)(out, insz, cksum, cksumsz, key);

    *outsz = insz - CONFOUNDERSZ - cksumsz;
    memmove(out, (char *)out + CONFOUNDERSZ + cksumsz, *outsz);

    return ret;
}
