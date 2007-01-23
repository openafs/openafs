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

#include "k5s_des.h"
#include "k5s_desint.h"

#ifdef KERNEL
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#endif

#define DES_BLOCK_SIZE 8

#ifndef NID_des_cbc
#define NID_des_ede_cbc	NID_undef
#define NID_des_ede_cfb64   NID_undef
#define NID_des_ede_ofb64   NID_undef
#define NID_des_ede_ecb	NID_undef
#define NID_des_ede3_cbc	NID_undef
#define NID_des_ede3_cfb64   NID_undef
#define NID_des_ede3_ofb64   NID_undef
#define NID_des_ede3_ecb	NID_undef
#endif

#define data(ctx)   ((EVP_DES_EDE_KEY*)ctx->cipher_data)

typedef struct {
    des_key ks[3];
} EVP_DES_EDE_KEY;

static int
des_ede_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	  const unsigned char *iv, int enc)
{
    des_setkey(data(ctx)->ks, key, 8);
    des_setkey(data(ctx)->ks+1, key+8, 8);
    memcpy(data(ctx)->ks+2, data(ctx)->ks+1, sizeof (*data(ctx)->ks));
    return 1;
}

static int
des_ede3_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	  const unsigned char *iv, int enc)
{
    des_setkey(data(ctx)->ks, key, 8);
    des_setkey(data(ctx)->ks+1, key+8, 8);
    des_setkey(data(ctx)->ks+2, key+16, 8);
    return 1;
}

static int
des_ede_cbc_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    des3_cbc_encrypt(in, out, (long) inl,
	data(ctx)->ks, ctx->iv,
	ctx->encrypt);
    return 1;
}

static int
des_ede_cfb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    des3_cfb64_encrypt(in, out, (long) inl,
	data(ctx)->ks,
	ctx->iv, &ctx->num, ctx->encrypt);
    return 1;
}

static int
des_ede_ecb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    unsigned int i, bl;
    bl = ctx->cipher->block_size;
    if (inl < bl)
	return 1;
    inl -= bl;
    for (i = 0; i <= inl; i += bl)
	des3_ecb_encrypt(in + i, out + i,
	    data(ctx)->ks,
	    ctx->encrypt);
    return 1;
}

static int
des_ede_ofb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    des3_ofb64_encrypt(in, out, (long) inl,
	       data(ctx)->ks,
	       ctx->iv, &ctx->num);
    return 1;
}

static const EVP_CIPHER des_ede_cbc[1] = {{
    NID_des_ede_cbc, DES_BLOCK_SIZE, 16, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    des_ede_init_key, des_ede_cbc_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
    EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede_cbc(void)
{
    return des_ede_cbc;
}

static const EVP_CIPHER des_ede_cfb[1] = {{
    NID_des_ede_cfb64, 1, 16, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    des_ede_init_key, des_ede_cfb_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede_cfb(void)
{
    return des_ede_cfb;
}

static const EVP_CIPHER des_ede_ofb[1] = {{
    NID_des_ede_ofb64, 1, 16, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    des_ede_init_key, des_ede_ofb_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede_ofb(void)
{
    return des_ede_ofb;
}

static const EVP_CIPHER des_ede_ecb[1] =
    {{ NID_des_ede_ecb, DES_BLOCK_SIZE, 16, DES_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, des_ede_init_key, des_ede_ecb_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede_ecb(void)
{
    return des_ede_ecb;
}

static const EVP_CIPHER des_ede3_cbc[1] = {{
    NID_des_ede3_cbc, DES_BLOCK_SIZE, 24, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    des_ede3_init_key, des_ede_cbc_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede3_cbc(void)
{
    return des_ede3_cbc;
}

static const EVP_CIPHER des_ede3_cfb[1] = {{
    NID_des_ede3_cfb64, 1, 24, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    des_ede3_init_key, des_ede_cfb_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede3_cfb(void)
{
    return des_ede3_cfb;
}

static const EVP_CIPHER des_ede3_ofb[1] = {{
    NID_des_ede3_ofb64, 1, 24, DES_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    des_ede3_init_key, des_ede_ofb_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede3_ofb(void)
{
    return des_ede3_ofb;
}

static const EVP_CIPHER des_ede3_ecb[1] =
    {{ NID_des_ede3_ecb, DES_BLOCK_SIZE, 24, DES_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, des_ede3_init_key, des_ede_ecb_cipher,
    NULL,
    sizeof(EVP_DES_EDE_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL,  (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_des_ede3_ecb(void)
{
    return des_ede3_ecb;
}
