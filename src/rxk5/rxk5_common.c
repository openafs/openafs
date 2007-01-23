/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <afs/stds.h>
#include <afsconfig.h>
#if defined(KERNEL) && !defined(UKERNEL)
#include "afs/param.h"
#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afsincludes.h"        /*AFS-based standard headers */
#include "afs_stats.h"
#include <rx/rxk5.h>
#include <rx/rxk5errors.h>
#include <rx/rxk5c.h>
#include <k5ssl.h>
#include <rx/rxk5imp.h>
#define assert(p) do {} while(0)
#else
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <rx/rx.h>
#include <rx/xdr.h>
#ifdef USING_SHISHI
#include <shishi.h>
#else
#ifdef USING_SSL
#include "k5ssl.h"
#else
#if HAVE_PARSE_UNITS_H
#include "parse_units.h"
#endif
#undef u
#include <krb5.h>
#endif
#endif
#include <assert.h>
#include <errno.h>
#include <com_err.h>
#include <string.h>

#include "rxk5imp.h"
#include "rxk5c.h"
#include "rxk5errors.h"
#endif /* !kernel */

krb5_context rxk5_context;
#ifdef AFS_PTHREAD_ENV
/* this mostly just protects setting rxk5_context. */
pthread_mutex_t rxk5i_context_mutex[1] = {PTHREAD_MUTEX_INITIALIZER};
pthread_mutex_t rxk5_cuid_mutex[1] = {PTHREAD_MUTEX_INITIALIZER};
#else
#if defined(KERNEL) && !defined(UKERNEL)
afs_rwlock_t rxk5i_context_mutex;
afs_rwlock_t rxk5_cuid_mutex;
#endif /* kernel */
#endif

void rxk5_OnetimeInit()
{
    printf("rxk5_OnetimeInit called\n");

#if defined(KERNEL) && !defined(UKERNEL)

    /* Arrange for this to be called once, when the cache manager starts */
    RWLOCK_INIT(&rxk5i_context_mutex, "rxk5 context lock");
    RWLOCK_INIT(&rxk5_cuid_mutex, "rxk5 cuid lock");

    {
      /* set up random number generator */
      int code, r;
      krb5_context k5context;
      r = 0;
      k5context = rxk5_get_context(0);
      code = krb5_c_random_os_entropy(k5context,0,NULL);
      if(code) { 
	afs_warn("rxk5/k5ssl: problem adding entropy");
      }
    }
#endif
}

int rxk5i_random_integer(context)
    krb5_context context;
{
    int result;
#ifndef USING_HEIMDAL
    int code;
#endif
#ifndef USING_SHISHI
    krb5_data rd[1];
#endif

    LOCK_RXK5_CONTEXT;
#ifdef USING_SHISHI
    code = shishi_randomize(context, 0, (void*)&result, sizeof result);
    if (code) code += ERROR_TABLE_BASE_SHI5;
    assert(!code);
#else
    rd->data = (void*) &result;
    rd->length = sizeof result;
#ifdef USING_HEIMDAL
    krb5_generate_random_block(rd->data, rd->length);
#else
    code = krb5_c_random_make_octets(context, rd);
    assert(!code);
#endif
#endif
    UNLOCK_RXK5_CONTEXT;
    return result;
}

krb5_context
rxk5_get_context(k5_context)
    krb5_context k5_context;
{
    int code;
    if (k5_context || (k5_context = rxk5_context)) {
	return k5_context;
    }
    LOCK_RXK5_CONTEXT;
    if (!(k5_context = rxk5_context)) {
#ifdef USING_SHISHI
	code = shishi_init(&rxk5_context);
	if (code) code += ERROR_TABLE_BASE_SHI5;
#else
	code = krb5_init_context(&rxk5_context);
#endif
	assert(!code);
	k5_context = rxk5_context;
    }
    UNLOCK_RXK5_CONTEXT;
    return k5_context;
}

int
rxk5i_SetLevel(conn, level, enctype, cktype, context, padp)
    struct rx_connection *conn;
    int cktype;
    krb5_context context;
    int *padp;
{
    int code;
    int d, maxd, mind;
    size_t enclen, bsize, len;
    size_t length;
    int padding, x;
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
    krb5_keyblock key[1];
    int hm;
#endif
#ifdef USING_SHISHI
    char *enc;
    krb5_keyblock key[1];
    char junk[1];	/* XXX */
#endif

#ifdef USING_SHISHI
    memset(key, 0, sizeof *key);
#endif
#ifdef USING_HEIMDAL
    memset(key, 0, sizeof *key);
#endif
    padding = 0;
    x = ~0;
    switch(level) {
#if CONFIG_CLEAR
    case rxk5_clear:
	code = 0;
	break;
#endif
    case rxk5_auth:
#ifdef USING_SHISHI
/* XXX how to check if keyed?  collision-proof ?? */
	if (!shishi_checksum_supported_p(cktype))
{
printf ("rxk5i_SetLevel: CHECKSUM NO SUPPORT cktype = %d\n", cktype);
	    return RXK5INCONSISTENCY;
}
	length = shishi_checksum_cksumlen(cktype);
#else
#ifdef USING_HEIMDAL
    /* valid returns: 0, small power of 2, KRB5_PROG_SUMTYPE_NOSUPP */
	hm = krb5_checksum_is_keyed(context, cktype);
	if (!hm || (hm & (hm-1)) || (unsigned)hm >= 256)
		return hm ? hm : KRB5KRB_AP_ERR_INAPP_CKSUM;
	hm = krb5_checksum_is_collision_proof(context, cktype);
	if (!hm || (hm & (hm-1)) || (unsigned)hm >= 256)
		return hm ? hm : KRB5KRB_AP_ERR_INAPP_CKSUM;
	code = krb5_checksumsize(context, cktype, &length);
#else
	if (!krb5_c_valid_cksumtype(cktype)
		|| !krb5_c_is_coll_proof_cksum(cktype)
		|| !krb5_c_is_keyed_cksum(cktype)) {
	    return KRB5KRB_AP_ERR_INAPP_CKSUM;
	}
	code = krb5_c_checksum_length(context, cktype, &length);
#endif
#endif
	if (code) return code;
	rx_SetSecurityHeaderSize(conn, (u_short)length);
	rx_SetSecurityMaxTrailerSize(conn, (u_short)length);
	break;
    case rxk5_crypt:
#ifdef USING_SHISHI
	if (!shishi_cipher_supported_p(enctype))
{
printf ("rxk5i_SetLevel: ENCTYPE NO SUPPORT enctype = %d\n", enctype);
	    return RXK5INCONSISTENCY;	/* XXX need better return */
}
	bsize = shishi_cipher_blocksize(enctype);
	LOCK_RXK5_CONTEXT;
	code = shishi_key_random(context, enctype, &key->sk);
	UNLOCK_RXK5_CONTEXT;
	if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#else
#ifdef USING_HEIMDAL
	/* XXX gross */
	LOCK_RXK5_CONTEXT;
	code = krb5_generate_random_keyblock(context, enctype, key);
	UNLOCK_RXK5_CONTEXT;
	if (code) goto Done;
	code = krb5_crypto_init(context, key, enctype, &crypto);
	if (code) goto Done;
	code = krb5_crypto_getblocksize(context, crypto, &bsize);
	if (code) goto Done;
#else
	code = krb5_c_block_size(context, enctype, &bsize);
	if (code) return code;
#endif
#endif
	bsize |= (!bsize) << 5;
	maxd = 0;
	mind = 511;
	for (len = bsize*2; len; --len) {
#ifdef USING_SHISHI
	    enc = 0;
		/* wow; usage must not be zero here */
	    code = shishi_encrypt(context, key->sk, USAGE_ENCRYPT, junk, len, &enc, &enclen);
	    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
	    if (enc) osi_Free(enc, enclen);
#else
#ifdef USING_HEIMDAL
	    enclen = krb5_get_wrapped_length(context, crypto, len);
#else
	    code = krb5_c_encrypt_length(context, enctype, len, &enclen);
	    if (code) return code;
#endif
#endif
	    d = enclen - len;
	    if (d > maxd) maxd = d;
	    else if (d < mind) mind = d, x = len;
	}
	padding = mind != maxd;
	rx_SetSecurityHeaderSize(conn, (u_short)mind);
	rx_SetSecurityMaxTrailerSize(conn, (u_short)(maxd&-padding));
	padp[0] = padding;
	padp[1] = 1 + ((~x)&(maxd-mind));
	break;
    default:
printf ("rxk5i_SetLevel: BAD LEVEL %d\n", level);
	return RXK5ILLEGALLEVEL;
    }
#ifdef USING_SHISHI
Done:
    krb5_free_keyblock_contents(context, key);
    return code;
#else
#ifdef USING_HEIMDAL
Done:
    if (crypto)
	krb5_crypto_destroy(context, crypto);	/* YYY freeing zeroword */
    krb5_free_keyblock_contents(context, key);
    return code;
#else
    return 0;
#endif
#endif
}

int
rxk5i_NewConnection (so, conn, size)
    struct  rx_securityClass *so;
    struct  rx_connection *conn;
{
    conn->securityData = (char*) rxi_Alloc(size);
    memset(conn->securityData, 0, size);
    ++so->refCount;
    return 0;
}

int
rxk5i_MakePsdata(call, p, psdata)
    struct rx_call *call;
    struct rx_packet *p;
    krb5_data *psdata;
{
    rxk5c_pshead ps[1];
    XDR xdrs[1];
    char data[sizeof *ps];

    ps->epoch = p->header.epoch;
    ps->cid = p->header.cid;
    ps->callnumber = p->header.callNumber;
    ps->seq = p->header.seq;
    ps->securityindex = call->conn->securityIndex;
    xdrmem_create(xdrs, data, sizeof *ps, XDR_ENCODE);
    if (!xdr_rxk5c_pshead(xdrs, ps)) {
printf ("rxk5i_MakePsdata: PSEUDOHEADER serialization failed\n");
	return RXK5INCONSISTENCY;
    }
    psdata->length = xdr_getpos(xdrs);
    if (!(psdata->data = osi_Alloc(psdata->length)))
	return ENOMEM;
    memcpy(psdata->data, data, psdata->length);
    return 0;
}

int
rxk5i_PrepareEncrypt(so, call, p, akey, padp, stats, context)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
    krb5_keyblock *akey;
    int *padp;
    struct rxk5_connstats *stats;
    krb5_context context;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int len = rx_GetDataSize(p);
    int code;
    krb5_data plain[1];
    krb5_data psdata[1];
#ifdef USING_SHISHI
    krb5_data cipher[1];
#else
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
    krb5_data cipher[1];
#else
    krb5_enc_data cipher[1];
    size_t enclen;
#endif
#endif
    krb5_keyblock key[1];
    int off = rx_GetSecurityHeaderSize(conn);
    int padding = rx_GetSecurityMaxTrailerSize(conn);
    int c;

    memset((void*) cipher, 0, sizeof *cipher);
    memset((void*) plain, 0, sizeof *plain);
    memset((void*) psdata, 0, sizeof *psdata);
    memset((void*) key, 0, sizeof *key);
    stats->packetsSent++;
    stats->bytesSent += len;
    if (*padp)
	plain->length = ((len+padp[1]) & ~(padding-off)) + (padp[1] & (padding-off));
    else
	plain->length = len;
    plain->data = osi_Alloc(plain->length);
    if ((code = rxk5i_MakePsdata(call, p, psdata)))
	goto Done;
    if ((code = rxk5i_derive_key(context, akey, psdata, key)))
	goto Done;
#ifdef USING_SHISHI
#else
#ifdef USING_HEIMDAL
    code = krb5_crypto_init(context, key, key->keytype, &crypto);
    if (code) goto Done;
#else
    code = krb5_c_block_size(context, akey->enctype, &enclen);
    if (code) goto Done;
    code = krb5_c_encrypt_length(context, akey->enctype, plain->length, &enclen);
    if (code) goto Done;
    cipher->ciphertext.length = enclen;
    cipher->ciphertext.data = osi_Alloc(cipher->ciphertext.length);
    cipher->enctype = akey->enctype;
    if (!cipher->ciphertext.data || !plain->data) {
/* printf ("rxk5i_PrepareEncrypt: out of memory\n"); */
	code = RXK5INCONSISTENCY;
	goto Done;
    }
#endif
#endif
    if (*padp)
	memset(plain->data+len, plain->length-len, plain->length-len);
    c = rx_packetread(p, off, len, plain->data);
if (c && c != len) {
printf ("rxk5i_PrepareEncrypt: FAILED to read: got r=%d; gave len=%d\n",
c, len);
}
#ifdef USING_SHISHI
    LOCK_RXK5_CONTEXT;
    code = shishi_encrypt(context,
	key->sk, USAGE_ENCRYPT,
	plain->data, plain->length,
	&cipher->data, &cipher->length);
    UNLOCK_RXK5_CONTEXT;
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
if (cipher->length != plain->length + off)
{
printf ("rxk5i_PrepareEncrypt: plain=%d off=%d; encrypted: predicted=%d actual=%d\n",
plain->length, off, plain->length+off, cipher->length);
	code = RXK5INCONSISTENCY;
	goto Done;
}
#else
#ifdef USING_HEIMDAL
    LOCK_RXK5_CONTEXT;
    code = krb5_encrypt(context, crypto, USAGE_ENCRYPT,
	plain->data, plain->length,
	cipher);
    UNLOCK_RXK5_CONTEXT;
#else
    LOCK_RXK5_CONTEXT;
    code = krb5_c_encrypt(context, key, USAGE_ENCRYPT, 0, plain, cipher);
    UNLOCK_RXK5_CONTEXT;
#endif
#endif
    if (code) goto Done;
    rxi_RoundUpPacket(p, plain->length-len);
#if defined(USING_HEIMDAL) || defined(USING_SHISHI)
    rx_packetwrite(p, 0, cipher->length, cipher->data);
    rx_SetDataSize(p, cipher->length);
    c = rx_GetDataSize(p);
#else
    rx_packetwrite(p, 0, cipher->ciphertext.length, cipher->ciphertext.data);
    rx_SetDataSize(p, cipher->ciphertext.length);
    c = rx_GetDataSize(p);
#endif
#if defined(USING_HEIMDAL) || defined(USING_SHISHI)
    if (c && c != cipher->length)
#else
    if (c && c != cipher->ciphertext.length)
#endif
    {
printf ("rxk5i_PrepareEncrypt: FAILED to write: got r=%d; gave len=%d\n",
c,
#if defined( USING_HEIMDAL) || defined(USING_SHISHI)
cipher->length
#else
cipher->ciphertext.length
#endif
);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    /* rx_SetPacketCksum(p, 0); */
Done:
    if (psdata->data) osi_Free(psdata->data, psdata->length);
    if (plain->data) osi_Free(plain->data, plain->length);
#ifdef USING_SHISHI
    if (cipher->data) osi_Free(cipher->data, cipher->length);
#else
#ifdef USING_HEIMDAL
    if (crypto) krb5_crypto_destroy(context, crypto);
    if (cipher->data) osi_Free(cipher->data, cipher->length);
#else
    if (cipher->ciphertext.data) osi_Free(cipher->ciphertext.data, cipher->ciphertext.length);
#endif
#endif
    krb5_free_keyblock_contents(context, key);
if (code) printf ("rxk5i_PrepareEncrypt err=%d\n", code);
    return code;
}

int
rxk5i_CheckEncrypt(so, call, p, akey, padp, stats, context)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
    krb5_keyblock *akey;
    int *padp;
    struct rxk5_connstats *stats;
    krb5_context context;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int len = rx_GetDataSize(p);
    int code;
    krb5_data plain[1];
    krb5_data psdata[1];
#ifdef USING_SHISHI
    krb5_data cipher[1];
#else
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
    krb5_data cipher[1];
#else
    krb5_enc_data cipher[1];
#endif
#endif
    int off = rx_GetSecurityHeaderSize(conn);
    /* int pidx = rx_GetPacketCksum(p); */
    krb5_keyblock key[1];

    memset((void*) cipher, 0, sizeof *cipher);
    memset((void*) plain, 0, sizeof *plain);
    memset((void*) key, 0, sizeof *key);
    memset((void*) psdata, 0, sizeof *psdata);
    stats->packetsReceived++;
    stats->bytesReceived += len;
    stats->packetsSent++;
    stats->bytesSent += len;
#if defined(USING_HEIMDAL) || defined(USING_SHISHI)
    cipher->length = len;
    cipher->data = osi_Alloc(cipher->length);
    if (!cipher->data) {
printf ("rxk5i_CheckEncrypt: NOALLOC %d\n",cipher->length);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    rx_packetread(p, 0, cipher->length, cipher->data);
#else
    cipher->ciphertext.length = len;
    plain->length = cipher->ciphertext.length;
    plain->data = osi_Alloc(plain->length);
    cipher->ciphertext.data = osi_Alloc(cipher->ciphertext.length);
    cipher->enctype = akey->enctype;
    if (!cipher->ciphertext.data || !plain->data) {
printf ("rxk5i_CheckEncrypt: failed %#x(%d) %#x(%d)\n",
(int) cipher->ciphertext.data, (int) cipher->ciphertext.length,
(int) plain->data, (int) plain->length);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    rx_packetread(p, 0, cipher->ciphertext.length, cipher->ciphertext.data);
#endif
    if ((code = rxk5i_MakePsdata(call, p, psdata)))
	goto Done;
    if ((code = rxk5i_derive_key(context, akey, psdata, key)))
	goto Done;
#ifdef USING_SHISHI
	code = shishi_decrypt(context, key->sk, USAGE_ENCRYPT,
	    cipher->data, cipher->length,
	    &plain->data, &plain->length);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#else
#ifdef USING_HEIMDAL
    code = krb5_crypto_init(context, key, key->keytype, &crypto);
    if (code) goto Done;
    code = krb5_decrypt(context, crypto, USAGE_ENCRYPT,
	    cipher->data, cipher->length,
	    plain);
    if (code) goto Done;
#else
    if ((code = krb5_c_decrypt(context, key, USAGE_ENCRYPT, 0, cipher, plain)))
		goto Done;
#endif
#endif
    if (!*padp)
	;
    else if (!plain->length ||
	    ((unsigned char*)plain->data)[(plain->length-1)]
		> (unsigned) plain->length) {
	code = RXK5DATALEN;
	goto Done;
    }
    else
	plain->length -= ((unsigned char *)plain->data)[plain->length-1];

    rx_packetwrite(p, off, plain->length, plain->data);
    rx_SetDataSize(p, plain->length);
Done:
    if (psdata->data) osi_Free(psdata->data, psdata->length);
#ifdef USING_HEIMDAL
    if (plain->data) osi_Free(plain->data, cipher->length);
#else
    if (plain->data) osi_Free(plain->data, cipher->ciphertext.length);
#endif
#ifdef USING_SHISHI
    if (cipher->data)
	osi_Free(cipher->data, cipher->length);
#else
#ifdef USING_HEIMDAL
    if (crypto) krb5_crypto_destroy(context, crypto);
    if (cipher->data)
	osi_Free(cipher->data, cipher->length);
#else
    if (cipher->ciphertext.data)
	osi_Free(cipher->ciphertext.data, cipher->ciphertext.length);
#endif
#endif
    krb5_free_keyblock_contents(context, key);
    return code;
}

int
rxk5i_PrepareSum(so, call, p, akey, cktype, stats, context)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
    krb5_keyblock *akey;
    struct rxk5_connstats *stats;
    krb5_context context;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int len = rx_GetDataSize(p);
    int code;
    int off = rx_GetSecurityHeaderSize(conn);
#ifdef USING_SHISHI
    krb5_data cksum[1];
#else
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
#endif
    krb5_checksum cksum[1];
#endif
    krb5_keyblock key[1];
    krb5_data scratch[1];
    krb5_data psdata[1];

    memset((void*) scratch, 0, sizeof *scratch);
    memset((void*) cksum, 0, sizeof *cksum);
    memset((void*) key, 0, sizeof *key);
    memset((void*) psdata, 0, sizeof *psdata);
    stats->packetsSent++;
    stats->bytesSent += len;
    scratch->length = len;
    scratch->data = osi_Alloc(scratch->length);
    if (!scratch->data) {
printf ("rxk5i_PrepareSum: NOALLOC %d\n",scratch->length);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    rx_packetread(p, off, len, scratch->data);
    if ((code = rxk5i_MakePsdata(call, p, psdata)))
	goto Done;
    if ((code = rxk5i_derive_key(context, akey, psdata, key)))
	goto Done;
#ifdef USING_SHISHI
    LOCK_RXK5_CONTEXT;
    code = shishi_checksum(context, key->sk, USAGE_CHECK, cktype,
	scratch->data, scratch->length,
	&cksum->data, &cksum->length);
    UNLOCK_RXK5_CONTEXT;
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (cksum->length != off) {
printf ("rxk5i_PrepareSum: checksum length mismatch %d != %d\n",cksum->length, off);
	code = RXK5CKSUMLEN;
	goto Done;
    }
    rx_packetwrite(p, 0, off, cksum->data);
#else
#ifdef USING_HEIMDAL
    if ((code = krb5_crypto_init(context, key, key->keytype, &crypto)))
	goto Done;
    LOCK_RXK5_CONTEXT;
    code = krb5_create_checksum(context, crypto, USAGE_CHECK,
	    cktype, scratch->data, scratch->length, cksum);
    UNLOCK_RXK5_CONTEXT;
    if (code)
	goto Done;
    if (cksum->checksum.length != off) {
	code = RXK5CKSUMLEN;
	goto Done;
    }
    rx_packetwrite(p, 0, off, cksum->checksum.data);
#else
    LOCK_RXK5_CONTEXT;
    code = krb5_c_make_checksum(context, cktype, key, USAGE_CHECK,
	    scratch, cksum);
    UNLOCK_RXK5_CONTEXT;
    if (code)
	goto Done;
    if (cksum->length != off) {
	code = RXK5CKSUMLEN;
	goto Done;
    }
    rx_packetwrite(p, 0, off, cksum->contents);
#endif
#endif
    rx_SetDataSize(p, off + len);
Done:
#ifdef USING_SHISHI
    if (cksum->data) osi_Free(cksum->data, cksum->length);
#else
#ifdef USING_HEIMDAL
    if (crypto) krb5_crypto_destroy(context, crypto);
    if (cksum->checksum.data) osi_Free(cksum->checksum.data, cksum->checksum.length);
#else
    if (cksum->contents) osi_Free(cksum->contents, cksum->length);
#endif
#endif
    krb5_free_keyblock_contents(context, key);
    if (scratch->data) osi_Free(scratch->data, scratch->length);
    if (psdata->data) osi_Free(psdata->data, psdata->length);
    return code;
}

int
rxk5i_CheckSum(so, call, p, akey, cktype, stats, context)
    struct rx_securityClass *so;
    struct rx_call *call;
    struct rx_packet *p;
    krb5_keyblock *akey;
    struct rxk5_connstats *stats;
    krb5_context context;
{
    struct rx_connection *conn = rx_ConnectionOf(call);
    int len = rx_GetDataSize(p);
    int code;
    krb5_data scratch[1];
    krb5_data psdata[1];
    int off = rx_GetSecurityHeaderSize(conn);
    /* int pidx = rx_GetPacketCksum(p); */
#ifdef USING_SHISHI
    krb5_data cksum[1];
#else
    krb5_checksum cksum[1];
#endif
    krb5_keyblock key[1];
    int cs;
    char *cp;
#ifdef USING_SHISHI
    krb5_data computed[1];
#else
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
#else
    krb5_boolean valid;
#endif
#endif

    memset((void*) scratch, 0, sizeof *scratch);
    memset((void*) psdata, 0, sizeof *psdata);
    memset((void*) cksum, 0, sizeof *cksum);
    memset((void*) key, 0, sizeof *key);
#ifdef USING_SHISHI
    memset((void*) computed, 0, sizeof *computed);
#endif
    stats->packetsReceived++;
    stats->bytesReceived += len;
    stats->packetsSent++;
    stats->bytesSent += len;
    scratch->length = len;
    scratch->data = osi_Alloc(scratch->length);
    if (!scratch->data) {
printf ("rxk5i_CheckSum: NOALLOC %d\n", scratch->length);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
    rx_Pullup(p, off);
    rx_packetread(p, off, len-off, scratch->data);
    scratch->length = len - off;
    cp = rx_data(p, 0, cs);
    if (cs < off) {
printf ("rxk5i_CheckSum: short packet %d < %d\n", cs, off);
	code = RXK5INCONSISTENCY;
	goto Done;
    }
#ifdef USING_SHISHI
    cksum->data = cp;
    cksum->length = off;
#else
#ifdef USING_HEIMDAL
    cksum->checksum.data = cp;
    cksum->checksum.length = off;
    cksum->cksumtype = cktype;	/* XXX */
#else
    cksum->contents = cp;
    cksum->length = off;
    cksum->checksum_type = cktype;	/* XXX */
#endif
#endif
    if ((code = rxk5i_MakePsdata(call, p, psdata)))
	goto Done;
    if ((code = rxk5i_derive_key(context, akey, psdata, key)))
	goto Done;
#ifdef USING_SHISHI
    /* XXX confounders? */
    code = shishi_checksum(context, key->sk, USAGE_CHECK, cktype,
	scratch->data, scratch->length,
	&computed->data, &computed->length);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
    if (computed->length != cksum->length
	    ||  memcmp(cksum->data, computed->data, computed->length)) {
	code = RXK5SEALEDINCON;
    }
#else
#ifdef USING_HEIMDAL
    if ((code = krb5_crypto_init(context, key, key->keytype, &crypto)))
	goto Done;
    if ((code = krb5_verify_checksum(context, crypto, USAGE_CHECK,
	    scratch->data, scratch->length, cksum)))
	goto Done;
#else
    if ((code = krb5_c_verify_checksum(context, key, USAGE_CHECK,
	    scratch, cksum, &valid)))
	goto Done;
    if (valid == 0) code = RXK5SEALEDINCON;
#endif
#endif
    if(!code)
      rx_SetDataSize(p, len-off);
Done:
#ifdef USING_SHISHI
    if (computed->data) osi_Free(computed->data, computed->length);
#else
#ifdef USING_HEIMDAL
    if (crypto) krb5_crypto_destroy(context, crypto);
#endif
#endif
    krb5_free_keyblock_contents(context, key);
    if (psdata->data) osi_Free(psdata->data, psdata->length);
    if (scratch->data) osi_Free(scratch->data, len);
    return code;
}

int
rxk5i_GetStats(so, conn, stats, cstats)
    struct rx_securityClass *so;
    struct rx_connection *conn;
    struct rx_securityObjectStats *stats;
    struct rxk5_connstats *cstats;
{
    stats->bytesReceived = cstats->bytesReceived;
    stats->packetsReceived = cstats->packetsReceived;
    stats->bytesSent = cstats->bytesSent;
    stats->packetsSent = cstats->packetsSent;
    return 0;
}

/* this isn't particularly good, and ought to be
 * replaced by something better.  Unfortunately,
 * the distribution mit k5 release (1.4.1) isn't
 * very helpful.
 *
 * older:
 */
int
rxk5i_derive_key(context, from, how, to)
    krb5_context context;
    krb5_keyblock *from, *to;
    krb5_data *how;
{
    int i, code;
    krb5_data plain[1];
#ifdef USING_SHISHI
    char *cp;
    krb5_data hash[1];
    int keylen;
#else
    krb5_checksum hash[1];
#ifdef USING_HEIMDAL
    krb5_crypto crypto = 0;
#endif
#endif

    /* this is a 'derive key' logic */
    memset(hash, 0, sizeof *hash);
#ifdef USING_SHISHI
    keylen = shishi_key_length(from->sk);
    plain->length = keylen + how->length;
#else
#ifdef USING_HEIMDAL
    plain->length = from->keyvalue.length + how->length;
#else
    plain->length = from->length + how->length;
#endif
#endif
    plain->data = osi_Alloc(plain->length);
    if (!plain->data)
	return ENOMEM;
#ifdef USING_SHISHI
    memcpy(plain->data, shishi_key_value(from->sk), keylen);
    memcpy(plain->data+keylen, how->data, how->length);
    code = shishi_checksum(context, NULL, 0, SHISHI_RSA_MD5,
	plain->data, plain->length,
	&hash->data, &hash->length);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#else
#ifdef USING_HEIMDAL
    memcpy(plain->data, from->keyvalue.data, from->keyvalue.length);
    memcpy(plain->data+from->keyvalue.length,
	how->data, how->length);
    if ((code = krb5_crypto_init(context, from, from->keytype, &crypto)))
	goto Done;
    if ((code = krb5_create_checksum(context, crypto, 0,
	    CKSUMTYPE_RSA_MD5, plain->data, plain->length, hash)))
	goto Done;
#else
    memcpy(plain->data, from->contents, from->length);
    memcpy(plain->data+from->length, how->data, how->length);
    if ((code = krb5_c_make_checksum(context, CKSUMTYPE_RSA_MD5,
	    NULL, 0, plain, hash)))
	goto Done;
#endif
#endif
#ifdef USING_SHISHI
    code = shishi_key_from_value(context, shishi_key_type(from->sk),
	plain->data /* don't care */,
	&to->sk);
    if (code) { code += ERROR_TABLE_BASE_SHI5; goto Done; }
#else
    if ((code = krb5_copy_keyblock_contents(context, from, to)))
	goto Done;
#endif
#ifdef K5S_CK_H
#define rxk5i_nfold(a,b,c,d)	krb5i_nfold(a,b,c,d)
#endif
#ifdef USING_SHISHI
    rxk5i_nfold(hash->data, hash->length,
	shishi_key_value(to->sk), shishi_key_length(to->sk));
#else
#ifdef USING_HEIMDAL
    rxk5i_nfold(hash->checksum.data, hash->checksum.length,
	to->keyvalue.data, to->keyvalue.length);
#else
    rxk5i_nfold(hash->contents, hash->length, to->contents, to->length);
#endif
#endif

    /* this is "make_key".  Only des keys are a problem */
	/* ... shishi: static int des_set_odd_key_parity */
	/* XXX what about weak keys? == shishi des_key_correction... */
    switch(
#ifdef USING_SHISHI
		shishi_key_type(from->sk)
#else
#ifdef USING_HEIMDAL
		from->keytype
#else
		from->enctype
#endif
#endif
	    ) {
	case ENCTYPE_DES_CBC_CRC:
	case ENCTYPE_DES_CBC_MD4:
	case ENCTYPE_DES_CBC_MD5:
#ifdef ENCTYPE_DES_HMAC_SHA1
	case ENCTYPE_DES_HMAC_SHA1:
#endif
#ifdef ENCTYPE_DES3_OLD_MD5
	case ENCTYPE_DES3_OLD_MD5:
#endif
#ifdef ENCTYPE_DES3_OLD_SHA
	case ENCTYPE_DES3_OLD_SHA:
#endif
#ifdef ENCTYPE_DES3_CBC_SHA
	case ENCTYPE_DES3_CBC_SHA:
#endif
#ifdef ENCTYPE_DES3_HMAC_SHA1
	case ENCTYPE_DES3_HMAC_SHA1:
#endif
	case ENCTYPE_DES3_CBC_SHA1:
#ifdef USING_SHISHI
	i = shishi_key_length(to->sk);
#else
#ifdef USING_HEIMDAL
	i = to->keyvalue.length;
#else
	i = to->length;
#endif
#endif
	break;
    default:
	i = 0;
    }
#ifdef USING_SHISHI
    cp = shishi_key_value(to->sk);
#endif
#define maskbits(n)	((1<<n)-1)
#define hibits(x,n) 	(((x)>>n)&maskbits(n))
#define lobits(x,n)	((x)&maskbits(n))
#define xorbits(x,n)	(hibits(x,n)^lobits(x,n))
#define oddparity(x)	xorbits(xorbits(xorbits(x,4),2),1)
    while (i--) {	/* force odd parity */
#ifdef USING_SHISHI
	cp[i] ^= !oddparity(cp[i]);
#else
#ifdef USING_HEIMDAL
	i[(unsigned char*)to->keyvalue.data]
		^= !oddparity(i[(unsigned char*)to->keyvalue.data]);
#else
	to->contents[i] ^= !oddparity(to->contents[i]);
#endif
#endif
    }
#undef oddparity
#undef xorbits
#undef lobits
#undef hibits
#undef maskbits
Done:
#ifdef USING_SHISHI
    if (hash->data) {
	memset(hash->data, 0, hash->length);
	osi_Free(hash->data, hash->length);
    }
#else
#ifdef USING_HEIMDAL
    if (crypto) krb5_crypto_destroy(context, crypto);	/* YYY freeing 0word */
    if (hash->checksum.data) {
	memset(hash->checksum.data, 0, hash->checksum.length);
	krb5_data_free(&hash->checksum);
    }
#else
    if (hash->contents) {
	memset(hash->contents, 0, hash->length);
	krb5_free_checksum_contents(context, hash);
    }
#endif
#endif
    if (plain->data) {
	memset(plain->data, 0, plain->length);
	osi_Free(plain->data, plain->length);
    }
    return code;
}

#ifdef USING_SHISHI
void
krb5_free_keyblock_contents(context, key)
    krb5_context context;
    krb5_keyblock *key;
{
    if (key->sk)
	shishi_key_done(key->sk);
    memset(key, 0, sizeof *key);
}
#endif
