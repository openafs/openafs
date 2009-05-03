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

#include "k5s_config.h"
#include <sys/types.h>
#ifdef USING_SSL
#include <openssl/evp.h>
#endif
#ifdef USING_FAKESSL
#include "k5s_evp.h"
#endif

#include "k5s_des.h"
#include "k5s_desint.h"

#ifndef NID_des_cbc
#define NID_des_cbc	NID_undef
#define NID_des_cfb64   NID_undef
#define NID_des_ofb64   NID_undef
#define NID_des_ecb	NID_undef
#endif

#define data(ctx)   ((EVP_DES_KEY*)ctx->cipher_data)

typedef struct {
    des_key ks[1];
} EVP_DES_KEY;

static int
des_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	  const unsigned char *iv, int enc)
{
    des_setkey(data(ctx)->ks, key, 8);
    return 1;
}

static int
des_cbc_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    des_cbc_encrypt(in, out, (long) inl,
	data(ctx)->ks, ctx->iv,
	ctx->encrypt);
    return 1;
}

static int
des_cfb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    des_cfb64_encrypt(in, out, (long) inl,
	data(ctx)->ks,
	ctx->iv, &ctx->num, ctx->encrypt);
    return 1;
}

static int
des_ecb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    unsigned int i, bl;
    bl = ctx->cipher->block_size;
    if (inl < bl)
	return 1;
    inl -= bl;
    for (i = 0; i <= inl; i += bl)
	des_ecb_encrypt(in + i, out + i,
	    data(ctx)->ks,
	    ctx->encrypt);
    return 1;
}

static int
des_ofb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    des_ofb64_encrypt(in, out, (long) inl,
	       data(ctx)->ks,
	       ctx->iv, &ctx->num);
    return 1;
}

static const EVP_CIPHER des_cbc[1] = {{
    NID_des_cbc, DES_BLOCK_SIZE, 8, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    des_init_key, des_cbc_cipher,
    NULL,
    sizeof(EVP_DES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_cbc(void)
{
    return des_cbc;
}

static const EVP_CIPHER des_cfb[1] = {{
    NID_des_cfb64, 1, 8, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    des_init_key, des_cfb_cipher,
    NULL,
    sizeof(EVP_DES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_cfb(void)
{
    return des_cfb;
}

static const EVP_CIPHER des_ofb[1] = {{
    NID_des_ofb64, 1, 8, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    des_init_key, des_ofb_cipher,
    NULL,
    sizeof(EVP_DES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ofb(void)
{
    return des_ofb;
}

static const EVP_CIPHER des_ecb[1] =
    {{ NID_des_ecb, DES_BLOCK_SIZE, 8, DES_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, des_init_key, des_ecb_cipher,
    NULL,
    sizeof(EVP_DES_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ecb(void)
{
    return des_ecb;
}
