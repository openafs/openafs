/*
 * Copyright (c) 2006
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

#include <sys/types.h>
#ifdef USE_SSL
#include <openssl/evp.h>
#endif
#ifdef USE_FAKESSL
#include "k5s_evp.h"
#endif

#include "k5s_rijndael.h"
#include "k5s_aesint.h"

#ifndef NID_aes_128_cbc
#define NID_aes_128_cbc NID_undef
#define NID_aes_128_cfb128	NID_undef
#define NID_aes_128_ofb128	NID_undef
#define NID_aes_128_ecb NID_undef
#define NID_aes_192_cbc NID_undef
#define NID_aes_192_cfb128	NID_undef
#define NID_aes_192_ofb128	NID_undef
#define NID_aes_192_ecb NID_undef
#define NID_aes_256_cbc NID_undef
#define NID_aes_256_cfb128	NID_undef
#define NID_aes_256_ofb128	NID_undef
#define NID_aes_256_ecb NID_undef
#endif

#define data(ctx)   ((EVP_AES_KEY*)ctx->cipher_data)

typedef struct {
    RIJNDAEL_KEY ks[1];
} EVP_AES_KEY;

static int
aes_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	  const unsigned char *iv, int enc)
{
    switch(EVP_CIPHER_CTX_flags(ctx)) {
    default:
	if (!ctx->encrypt) {
	    rijndael_setkey_decrypt(data(ctx)->ks, key,
		EVP_CIPHER_CTX_key_length(ctx));
	    break;
	}
	/* fall through */
    case EVP_CIPH_CFB_MODE:
    case EVP_CIPH_OFB_MODE:
	rijndael_setkey_encrypt(data(ctx)->ks, key,
	    EVP_CIPHER_CTX_key_length(ctx));
    }
    return 1;
}

static int
aes_cbc_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    AES_cbc_encrypt(in, out, (long) inl,
	 data(ctx)->ks, ctx->iv,
	 ctx->encrypt);
    return 1;
}

static int
aes_cfb128_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    AES_cfb128_encrypt(in, out, (long) inl,
	   data(ctx)->ks,
	   ctx->iv, &ctx->num, ctx->encrypt);
    return 1;
}

static int
aes_ecb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    unsigned int i, bl;
    bl = ctx->cipher->block_size;
    if (inl < bl)
	return 1;
    inl -= bl;
    for (i = 0; i <= inl; i += bl)
	AES_ecb_encrypt(in + i, out + i,
		 data(ctx)->ks,
		 ctx->encrypt);
    return 1;
}

static int
aes_ofb128_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    AES_ofb128_encrypt(in, out, (long) inl,
	       data(ctx)->ks,
	       ctx->iv, &ctx->num);
    return 1;
}

static const EVP_CIPHER aes_128_cbc[1] = {{
    NID_aes_128_cbc, RIJNDAEL_BLOCK_SIZE, 16, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    aes_init_key, aes_cbc_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_128_cbc(void)
{
    return aes_128_cbc;
}

static const EVP_CIPHER aes_128_cfb[1] = {{
    NID_aes_128_cfb128, 1, 16, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    aes_init_key, aes_cfb128_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_128_cfb(void)
{
    return aes_128_cfb;
}

static const EVP_CIPHER aes_128_ofb[1] = {{
    NID_aes_128_ofb128, 1, 16, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    aes_init_key, aes_ofb128_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_128_ofb(void)
{
    return aes_128_ofb;
}

static const EVP_CIPHER aes_128_ecb[1] =
    {{ NID_aes_128_ecb, RIJNDAEL_BLOCK_SIZE, 16, RIJNDAEL_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, aes_init_key, aes_ecb_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_128_ecb(void)
{
    return aes_128_ecb;
}

static const EVP_CIPHER aes_192_cbc[1] = {{
    NID_aes_192_cbc, RIJNDAEL_BLOCK_SIZE, 24, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    aes_init_key, aes_cbc_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_192_cbc(void)
{
    return aes_192_cbc;
}

static const EVP_CIPHER aes_192_cfb[1] = {{
    NID_aes_192_cfb128, 1, 24, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    aes_init_key, aes_cfb128_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_192_cfb(void)
{
    return aes_192_cfb;
}

static const EVP_CIPHER aes_192_ofb[1] = {{
    NID_aes_192_ofb128, 1, 24, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    aes_init_key, aes_ofb128_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_192_ofb(void)
{
    return aes_192_ofb;
}

static const EVP_CIPHER aes_192_ecb[1] =
    {{ NID_aes_192_ecb, RIJNDAEL_BLOCK_SIZE, 24, RIJNDAEL_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, aes_init_key, aes_ecb_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_192_ecb(void)
{
    return aes_192_ecb;
}

static const EVP_CIPHER aes_256_cbc[1] = {{
    NID_aes_256_cbc, RIJNDAEL_BLOCK_SIZE, 32, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    aes_init_key, aes_cbc_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_256_cbc(void)
{
    return aes_256_cbc;
}

static const EVP_CIPHER aes_256_cfb[1] = {{
    NID_aes_256_cfb128, 1, 32, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    aes_init_key, aes_cfb128_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_256_cfb(void)
{
    return aes_256_cfb;
}

static const EVP_CIPHER aes_256_ofb[1] = {{
    NID_aes_256_ofb128, 1, 32, RIJNDAEL_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    aes_init_key, aes_ofb128_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_256_ofb(void)
{
    return aes_256_ofb;
}

static const EVP_CIPHER aes_256_ecb[1] =
    {{ NID_aes_256_ecb, RIJNDAEL_BLOCK_SIZE, 32, RIJNDAEL_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, aes_init_key, aes_ecb_cipher,
    NULL,
    sizeof(EVP_AES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_aes_256_ecb(void)
{
    return aes_256_ecb;
}
