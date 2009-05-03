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
#ifdef USING_SSL
#include <openssl/evp.h>
#endif
#ifdef USING_FAKESSL
#include "k5s_evp.h"
#endif

#include "k5s_rc6.h"

#define NID_rc6_cbc NID_undef
#define NID_rc6_cfb128	NID_undef
#define NID_rc6_ofb128	NID_undef
#define NID_rc6_ecb NID_undef

#define data(ctx)   ((EVP_RC6_KEY*)ctx->cipher_data)

typedef struct {
    RC6_KEY ks[1];
} EVP_RC6_KEY;

static int
rc6_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	  const unsigned char *iv, int enc)
{
    RC6_set_key(data(ctx)->ks,
	     EVP_CIPHER_CTX_key_length(ctx), key);
    return 1;
}

static int
rc6_cbc_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    RC6_cbc_encrypt(in, out, (long) inl,
	 data(ctx)->ks, ctx->iv,
	 ctx->encrypt);
    return 1;
}

static int
rc6_cfb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    RC6_cfb128_encrypt(in, out, (long) inl,
	   data(ctx)->ks,
	   ctx->iv, &ctx->num, ctx->encrypt);
    return 1;
}

static int
rc6_ecb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    unsigned int i, bl;
    bl = ctx->cipher->block_size;
    if (inl < bl)
	return 1;
    inl -= bl;
    for (i = 0; i <= inl; i += bl)
	RC6_ecb_encrypt(in + i, out + i,
		 data(ctx)->ks,
		 ctx->encrypt);
    return 1;
}

static int
rc6_ofb_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
    RC6_ofb128_encrypt(in, out, (long) inl,
	       data(ctx)->ks,
	       ctx->iv, &ctx->num);
    return 1;
}

static const EVP_CIPHER rc6_cbc_128[1] = {{
    NID_rc6_cbc, RC6_BLOCK_SIZE, 16, RC6_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CBC_MODE,
    rc6_init_key, rc6_cbc_cipher,
    NULL,
    sizeof(EVP_RC6_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_rc6_128_cbc(void)
{
    return rc6_cbc_128;
}

static const EVP_CIPHER rc6_cfb_128[1] = {{
    NID_rc6_cfb128, 1, 16, RC6_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_CFB_MODE,
    rc6_init_key, rc6_cfb_cipher,
    NULL,
    sizeof(EVP_RC6_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_rc6_128_cfb(void)
{
    return rc6_cfb_128;
}

static const EVP_CIPHER rc6_ofb_128[1] = {{
    NID_rc6_ofb128, 1, 16, RC6_BLOCK_SIZE,
    EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_OFB_MODE,
    rc6_init_key, rc6_ofb_cipher,
    NULL,
    sizeof(EVP_RC6_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_rc6_128_ofb(void)
{
    return rc6_ofb_128;
}

static const EVP_CIPHER rc6_ecb_128[1] =
    {{ NID_rc6_ecb, RC6_BLOCK_SIZE, 16, RC6_BLOCK_SIZE, EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ECB_MODE, rc6_init_key, rc6_ecb_cipher,
    NULL,
    sizeof(EVP_RC6_KEY), EVP_CIPHER_set_asn1_iv,
	EVP_CIPHER_get_asn1_iv,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_rc6_128_ecb(void)
{
    return rc6_ecb_128;
}
