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

#include "k5s_cast.h"

#define CAST_BLOCK_SIZE 8

#ifndef NID_cast5_cbc
#define NID_cast5_cbc	NID_undef
#define NID_cast5_cfb64   NID_undef
#define NID_cast5_ofb64   NID_undef
#define NID_cast5_ecb	NID_undef
#endif

#define data(ctx)   ((EVP_CAST_KEY*)ctx->cipher_data)

typedef struct {
    cast_key ks[1];
} EVP_CAST_KEY;

static int
cast5_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	  const unsigned char *iv, int enc)
{
    cast_setkey(data(ctx)->ks, key,
	EVP_CIPHER_CTX_key_length(ctx));
    return 1;
}

static int
cast5_cbc_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    cast_cbc_encrypt(in, out, (long) inl,
	data(ctx)->ks, ctx->iv,
	ctx->encrypt);
    return 1;
}

static int
cast5_cfb64_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    cast_cfb64_encrypt(in, out, (long) inl,
	data(ctx)->ks,
	ctx->iv, &ctx->num, ctx->encrypt);
    return 1;
}

static int
cast5_ecb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    unsigned int i, bl;
    bl = ctx->cipher->block_size;
    if (inl < bl)
	return 1;
    inl -= bl;
    for (i = 0; i <= inl; i += bl)
	cast_ecb_encrypt(in + i, out + i,
	    data(ctx)->ks,
	    ctx->encrypt);
    return 1;
}

static int
cast5_ofb64_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    cast_ofb64_encrypt(in, out, (long) inl,
	       data(ctx)->ks,
	       ctx->iv, &ctx->num);
    return 1;
}

static const EVP_CIPHER cast5_cbc[1] = {{
    NID_cast5_cbc, CAST_BLOCK_SIZE, 16, CAST_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    cast5_init_key, cast5_cbc_cipher,
    NULL,
    sizeof(EVP_CAST_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_cast5_cbc(void)
{
    return cast5_cbc;
}

static const EVP_CIPHER cast5_cfb[1] = {{
    NID_cast5_cfb64, 1, 16, CAST_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    cast5_init_key, cast5_cfb64_cipher,
    NULL,
    sizeof(EVP_CAST_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_cast5_cfb(void)
{
    return cast5_cfb;
}

static const EVP_CIPHER cast5_ofb[1] = {{
    NID_cast5_ofb64, 1, 16, CAST_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    cast5_init_key, cast5_ofb64_cipher,
    NULL,
    sizeof(EVP_CAST_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_cast5_ofb(void)
{
    return cast5_ofb;
}

static const EVP_CIPHER cast5_ecb[1] =
    {{ NID_cast5_ecb, CAST_BLOCK_SIZE, 16, CAST_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, cast5_init_key, cast5_ecb_cipher,
    NULL,
    sizeof(EVP_CAST_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_cast5_ecb(void)
{
    return cast5_ecb;
}
