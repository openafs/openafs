/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Portable implementation of rxgk wire encryption
 */

#include "rxgk_locl.h"
#include <errno.h>

RCSID("$Id$");

struct _rxg_key_type {
    char *name;
    int enctype;
    int blocklen;
    int checksumlen;
    int confounderlen;
};

static struct _rxg_key_type ktypes[] = {
    { "des-cbc-crc", RXGK_CRYPTO_DES_CBC_CRC,
      8, 4, 8,
    },
    { "des-cbc-md5", RXGK_CRYPTO_DES_CBC_MD5,
      8, 24, 8,
    }
};

static struct _rxg_key_type *
_rxg_find_enctype(int enctype)
{
    struct _rxg_key_type *key;

    for (key = ktypes; key->name != NULL; key++)
	if (key->enctype == enctype)
	    return key;
    return NULL;
}

int
rxgk_set_conn(struct rx_connection *con, int enctype, int enc)
{
    struct _rxg_key_type *key;

    key = _rxg_find_enctype(enctype);
    if (key == NULL)
	return ENOENT;

    if (enc) {
	rx_SetSecurityHeaderSize(con, key->checksumlen + key->confounderlen +
				 RXGK_HEADER_DATA_SIZE);

	rx_SetSecurityMaxTrailerSize(con, key->blocklen);
    } else {
	rx_SetSecurityHeaderSize(con, 
				 key->checksumlen + RXGK_HEADER_DATA_SIZE);
	rx_SetSecurityMaxTrailerSize(con, 0);
    }
    return 0;
}

/*
 * portability macros
 */
#ifdef MIT_KRB5
#define CIPHERTYPE krb5_enc_data
#define CKSUMTYPE krb5_checksum
#define CIPHTEXT cipher->ciphertext
#define CIPHTEXTLEN cipher->ciphertext.length
#define CIPHTEXTDAT cipher->ciphertext.data
#define CKSUMDAT cksum->contents
#define CKSUMLEN cksum->length
#elif defined(SHISHI_KRB5)
#define CIPHERTYPE krb5_data
#define CKSUMTYPE krb5_data
#define CIPHTEXT cipher
#define CIPHTEXTLEN cipher->length
#define CIPHTEXTDAT cipher->data
#define CKSUMDAT cksum->data
#define CKSUMLEN cksum->length
#else
#define CIPHERTYPE krb5_data
#define CKSUMTYPE krb5_checksum
#define CIPHTEXT cipher
#define CIPHTEXTLEN cipher->length
#define CIPHTEXTDAT cipher->data
#define CKSUMDAT cksum->checksum.data
#define CKSUMLEN cksum->checksum.length
#endif

#ifdef OPENAFS
#define ALLOCFUNC(x) osi_Alloc(x)
#define FREEFUNC(x,y) osi_Free(x,y)
#else
#define ALLOCFUNC(x) malloc(x)
#define FREEFUNC(x,y) free(x)
#endif

#define GETCONTEXT rxgk_krb5_context

int
rxgk_crypto_init(struct rxgk_keyblock *tk, key_stuff *k)
{
    int ret;

    krb5_keyblock tk_kb;

    ret = krb5_keyblock_init(GETCONTEXT, tk->enctype,
			     tk->data, tk->length,
			     &tk_kb);

    if (ret)
	return EINVAL;

    ret = krb5_crypto_init (GETCONTEXT, &tk_kb,
			    tk->enctype,
			    &k->ks_scrypto);

    krb5_free_keyblock_contents(GETCONTEXT, &tk_kb);

    if (ret)
	return EINVAL;

    return 0;
}

int
rxgk_prepare_packet(struct rx_packet *p, struct rx_connection *conn,
		    int level, key_stuff *k, end_stuff *e,
		    int keyusage_enc, int keyusage_mic)
{
    int len = rx_GetDataSize(p);
    int off = rx_GetSecurityHeaderSize(conn);
#if 0
    int padding = rx_GetSecurityMaxTrailerSize(conn);
#endif

    krb5_data plain[1];
    CIPHERTYPE cipher[1];
    CKSUMTYPE cksum[1];
    krb5_data scratch[1];
    krb5_keyblock key[1];

    krb5_data computed[1]; /* shishi tmp */
#if defined(MIT_KRB5)
    size_t enclen; /* mit tmp */
#endif

#if defined(MIT_KRB5)
    krb5_boolean valid;
#endif
    int cktype = 1;     /* CKSUMTYPE_CRC32 XXX */

    int cs, code;
    char *cp;

    if (level == RXGK_M_INTEGRITY) {
	memset((void*) scratch, 0, sizeof *scratch);
	memset((void*) cksum, 0, sizeof *cksum);
	memset((void*) key, 0, sizeof *key);
	memset((void*) computed, 0, sizeof *computed);
	
	scratch->length = len;
	scratch->data = ALLOCFUNC(scratch->length);
	if (!scratch->data) {
	    printf ("rxgk_preparepacket: NOALLOC %d\n", len);
	    code = RXGKINCONSISTENCY;
	    goto PrChDone;
	}
	rx_Pullup(p, off);
	rx_packetread(p, off, len-off, scratch->data);
	scratch->length = len - off;
	cp = rx_data(p, 0, cs);
	if (cs < off) {
	    printf ("rxgk_preparepacket: short packet %d < %d\n", cs, off);
	    code = RXGKINCONSISTENCY;
	    goto PrChDone;
	}
	CKSUMDAT = cp;
	CKSUMLEN = off;
#if defined(MIT_KRB5)
	cksum->checksum_type = cktype;      /* XXX */
#elif !defined(SHISHI_KRB5)
	cksum->cksumtype = cktype;  /* XXX */
#endif

#ifdef SHISHI_KRB5
	/* XXX confounders? */
	code = shishi_checksum(GETCONTEXT, key->sk, keyusage_mic, cktype,
			       scratch->data, scratch->length,
			       &computed->data, &computed->length);
	if (code) { 
	    code += ERROR_TABLE_BASE_SHI5; 
	    goto PrChDone; 
	}
	if (computed->length != CKSUMLEN
	    ||  memcmp(CKSUMDAT, computed->data, computed->length)) {
	    code = RXGKSEALEDINCON;
	}
#elif defined(MIT_KRB5)
	if ((code = krb5_c_verify_checksum(GETCONTEXT, key, keyusage_mic,
					   scratch, cksum, &valid)))
	    goto PrChDone;
	if (valid == 0) code = RXGKSEALEDINCON;
#else
	if ((code = krb5_verify_checksum(GETCONTEXT, k->ks_scrypto, keyusage_mic,
					 scratch->data, scratch->length, cksum)))
	    goto PrChDone;
#endif
	if(!code)
	    rx_SetDataSize(p, len-off);
 PrChDone:
	if (computed->data) FREEFUNC(computed->data, computed->length);
	krb5_free_keyblock_contents(GETCONTEXT, key);
	if (scratch->data) FREEFUNC(scratch->data, len);
    } else if (level == RXGK_M_CRYPT) {
	memset((void*) cipher, 0, sizeof *cipher);
	memset((void*) plain, 0, sizeof *plain);
	memset((void*) key, 0, sizeof *key);
	
	plain->length = len;
	plain->data = ALLOCFUNC(plain->length);
#if defined(MIT_KRB5)
	code = krb5_c_block_size(GETCONTEXT, akey->enctype, &enclen);
	if (code) 
	    goto PrEncPrChDone;
	code = krb5_c_encrypt_length(GETCONTEXT, akey->enctype, plain->length, &enclen);
	if (code) 
	    goto PrEncPrChDone;
	cipher->ciphertext.length = enclen;
	cipher->ciphertext.data = ALLOCFUNC(cipher->ciphertext.length);
	cipher->enctype = akey->enctype;
	if (!cipher->ciphertext.data || !plain->data) {
	    code = RXGKINCONSISTENCY;
	    goto PrEncPrChDone;
	}
#endif
	/* XXX pad here */
	cs = rx_packetread(p, off, len, plain->data);
	if (cs && cs != len) {
	    printf ("rxgk_preparepacket read failed: got r=%d; gave len=%d\n",
		    cs, len);
	}
#ifdef USING_SHISHI
	code = shishi_encrypt(GETCONTEXT,
			      key->sk, keyusage_enc,
			      plain->data, plain->length,
			      &cipher->data, &cipher->length);
	if (code) { 
	    code += ERROR_TABLE_BASE_SHI5; 
	    goto PrEncPrChDone; 
	}
	if (cipher->length != plain->length + off)
	{
	    printf ("rxgk_preparepacket: plain=%d off=%d; encrypted: predicted=%d actual=%d\n",
		    plain->length, off, plain->length+off, cipher->length);
	    code = RXGKINCONSISTENCY;
	    goto PrEncPrChDone;
	}
#elif defined(MIT_KRB5)
	code = krb5_c_encrypt(GETCONTEXT, key, keyusage_enc, 0, plain, cipher);
#else
	code = krb5_encrypt(GETCONTEXT, k->ks_scrypto, keyusage_enc,
			    plain->data, plain->length,
			    cipher);
#endif
	if (code) 
	    goto PrEncPrChDone;
	rxi_RoundUpPacket(p, plain->length-len);
	rx_packetwrite(p, 0, CIPHTEXTLEN, CIPHTEXTDAT);
	rx_SetDataSize(p, CIPHTEXTLEN);
	cs = rx_GetDataSize(p);

	if (cs && cs != CIPHTEXTLEN) {
	    printf("rxgk_preparepacket failed to write: "
		   "got r=%lu; gave len=%lu\n",
		   (unsigned long)cs, (unsigned long)CIPHTEXTLEN);
	    code = RXGKINCONSISTENCY;
	    goto PrEncPrChDone;
	}
    PrEncPrChDone:
	if (plain->data) 
	    FREEFUNC(plain->data, plain->length);
	if (CIPHTEXTDAT) 
	    FREEFUNC(CIPHTEXTDAT, CIPHTEXTLEN);
	krb5_free_keyblock_contents(GETCONTEXT, key);
    } else if (level == RXGK_M_AUTH_ONLY) {
	code = 0;
    } else 
	code = RXGKILLEGALLEVEL;
    
    if (code) 
	printf ("rxgk_preparepacket err=%d\n", code);
    return code;
}

/*
 *
 */
int
rxgk_check_packet(struct rx_packet *p, struct rx_connection *conn,
		  int level, key_stuff *k, end_stuff *e,
		  int keyusage_enc, int keyusage_mic)
{
    int len = rx_GetDataSize(p);
    int off = rx_GetSecurityHeaderSize(conn);
    int code;
    krb5_data plain[1];
    CIPHERTYPE cipher[1];
    krb5_data scratch[1];
    krb5_data computed[1];
    CKSUMTYPE cksum[1];
    krb5_keyblock key[1];
    int cs;
    char *cp;
#if defined(MIT_KRB5)
    krb5_boolean valid;
#endif
    int cktype = 1;     /* CKSUMTYPE_CRC32 XXX */

    if (level == RXGK_M_CRYPT) {
	memset((void*) cipher, 0, sizeof *cipher);
	memset((void*) plain, 0, sizeof *plain);
	memset((void*) key, 0, sizeof *key);
	CIPHTEXTLEN = len;
	CIPHTEXTDAT = ALLOCFUNC(CIPHTEXTLEN);
#if !defined(MIT_KRB5)
	if (!CIPHTEXTDAT) {
	    printf ("rxgk_checkpacket: NOALLOC %lu\n",
		    (unsigned long)cipher->length);
	    code = RXGKINCONSISTENCY;
	    goto ChEncPrChDone;
	}
#else
	plain->length = CIPHTEXTLEN;
	plain->data = ALLOCFUNC(plain->length);
	cipher->enctype = akey->enctype;
	if (!cipher->ciphertext.data || !plain->data) {
	    printf ("rxgk_checkpacket: failed %#x(%d) %#x(%d)\n",
		    (int) cipher->ciphertext.data, (int) cipher->ciphertext.length,
		    (int) plain->data, (int) plain->length);
	    code = RXGKINCONSISTENCY;
	    goto ChEncPrChDone;
	}
#endif
	rx_packetread(p, 0, CIPHTEXTLEN, CIPHTEXTDAT);
#ifdef SHISHI_KRB5
        code = shishi_decrypt(GETCONTEXT, key->sk, keyusage_enc,
			      cipher->data, cipher->length,
			      &plain->data, &plain->length);
	if (code) { 
	    code += ERROR_TABLE_BASE_SHI5; 
	    goto ChEncPrChDone;
	}
#elif MIT_KRB5
	if ((code = krb5_c_decrypt(GETCONTEXT, key, keyusage_enc, 0, cipher, plain)))
	    goto ChEncPrChDone;
#else
	code = krb5_decrypt(GETCONTEXT, k->ks_scrypto, keyusage_enc,
			    cipher->data, cipher->length,
			    plain);
	if (code) 
	    goto ChEncPrChDone;
#endif
	rx_packetwrite(p, off, plain->length, plain->data);
	rx_SetDataSize(p, plain->length);
    ChEncPrChDone:
	if (plain->data) 
	    FREEFUNC(plain->data, plain->length); /* really free CIPHTEXTLEN? */
	if (CIPHTEXTDAT)
	    FREEFUNC(CIPHTEXTDAT, CIPHTEXTLEN);
	krb5_free_keyblock_contents(GETCONTEXT, key);
    } else if (level == RXGK_M_INTEGRITY) {
	memset((void*) scratch, 0, sizeof *scratch);
	memset((void*) cksum, 0, sizeof *cksum);
	memset((void*) key, 0, sizeof *key);
	memset((void*) computed, 0, sizeof *computed);
	scratch->length = len;
	scratch->data = ALLOCFUNC(scratch->length);
	if (!scratch->data) {
	    printf ("rxgk_checkpacket: alloc failed %lu\n", 
		    (unsigned long)scratch->length);
	    code = RXGKINCONSISTENCY;
	    goto ChChDone;
	}
	rx_Pullup(p, off);
	rx_packetread(p, off, len-off, scratch->data);
	scratch->length = len - off;
	cp = rx_data(p, 0, cs);
	if (cs < off) {
	    printf ("rxgk_checkpacket: short packet %d < %d\n", cs, off);
	    code = RXGKINCONSISTENCY;
	    goto ChChDone;
	}
	CKSUMDAT = cp;
	CKSUMLEN = off;
#if defined(MIT_KRB5)
	cksum->checksum_type = cktype;      /* XXX */
#elif !defined(SHISHI_KRB5)
	cksum->cksumtype = cktype;  /* XXX */
#endif

#ifdef SHISHI_KRB5
	/* XXX confounders? */
	code = shishi_checksum(GETCONTEXT, key->sk, keyusage_mic, cktype,
			       scratch->data, scratch->length,
			       &computed->data, &computed->length);
	if (code) { 
	    code += ERROR_TABLE_BASE_SHI5; 
	    goto ChChDone; 
	}
	if (computed->length != CKSUMLEN
            ||  memcmp(CKSUMDAT, computed->data, computed->length)) {
	    code = RXGKSEALEDINCON;
	}
#elif defined(MIT_KRB5)
	if ((code = krb5_c_verify_checksum(GETCONTEXT, key, keyusage_mic,
					   scratch, cksum, &valid)))
	    goto ChChDone;
	if (valid == 0) code = RXGKSEALEDINCON;
#else
	if ((code = krb5_verify_checksum(GETCONTEXT, k->ks_scrypto, keyusage_mic,
					 scratch->data, scratch->length, 
					 cksum)))
	    goto ChChDone;
#endif
	if(!code)
	    rx_SetDataSize(p, len-off);
    ChChDone:
	if (computed->data) 
	    FREEFUNC(computed->data, computed->length);
	krb5_free_keyblock_contents(GETCONTEXT, key);
	if (scratch->data) 
	    FREEFUNC(scratch->data, len);
    } else if (level == RXGK_M_AUTH_ONLY) {
	code = 0;
    } else {
	code = RXGKILLEGALLEVEL;
    }
    if (code) 
	printf ("rxgk_checkpacket err=%d\n", code);
    return code;
}

int
rxgk_encrypt_buffer(RXGK_Token *in, RXGK_Token *out,
		    struct rxgk_keyblock *key, int keyusage)
{
    int i;
    int x;

    out->len = in->len + 4;
    out->val = malloc(out->len);

    out->val[0] = 1;
    out->val[1] = 2;
    out->val[2] = 3;
    out->val[3] = 4;

    /* XXX add real crypto */
    x = keyusage;
    for (i = 0; i < in->len; i++) {
	x += i * 3 + ((unsigned char *)key->data)[i%key->length];
	out->val[i+4] = in->val[i] ^ x;
    }
    
    return 0;
}

int
rxgk_decrypt_buffer(RXGK_Token *in, RXGK_Token *out,
		    struct rxgk_keyblock *key, int keyusage)
{
    int i;
    int x;

    if (in->len < 4) {
	return EINVAL;
    }

    if (in->val[0] != 1 ||
	in->val[0] != 1 ||
	in->val[0] != 1 ||
	in->val[0] != 1) {
	return EINVAL;
    }

    out->len = in->len - 4;
    out->val = malloc(out->len);

    /* XXX add real crypto */
    x = keyusage;
    for (i = 0; i < out->len; i++) {
	x += i * 3 + ((unsigned char *)key->data)[i%key->length];
	out->val[i] = in->val[i+4] ^ x;
    }

    return 0;
}

static void
print_key(char *name, struct rxgk_keyblock *key)
{
    int i;

    fprintf(stderr, "type: %s", name);
    for (i = 0; i < key->length; i++)
	fprintf(stderr, " %02x", ((unsigned char*)key->data)[i]);
    fprintf(stderr, "\n");    
}

int
rxgk_derive_transport_key(struct rxgk_keyblock *k0,
			  struct rxgk_keyblock *tk,
			  uint32_t epoch, uint32_t cid, int64_t start_time)
{
    int i;
    uint32_t x;
    /* XXX get real key */

    if (k0->enctype != RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96) {
	return EINVAL;
    }

    tk->length = 32;
    tk->enctype = RXGK_CRYPTO_AES256_CTS_HMAC_SHA1_96;
    
    tk->data = malloc(tk->length);
    if (tk->data == NULL)
	return ENOMEM;

    x = epoch * 4711 + cid * 33 + start_time;
    for (i = 0; i < tk->length; i++) {
	x += i * 3 + ((unsigned char *)k0->data)[i%k0->length];
	((unsigned char *)tk->data)[i] = 0x23 + x * 47;
    }

    print_key("k0: ", k0);
    print_key("tk: ", tk);

    return 0;
}

