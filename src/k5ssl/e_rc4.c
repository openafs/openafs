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

#include "k5s_rc4.h"

#define NID_rc4_cbc	NID_undef
#define NID_rc4_cfb128	NID_undef
#define NID_rc4_ofb128	NID_undef
#define NID_rc4_ecb	NID_undef

#define data(ctx)	((EVP_RC4_KEY*)ctx->cipher_data)

typedef struct {
	RC4_KEY ks[1];
} EVP_RC4_KEY;

static int
rc4_init_key(EVP_CIPHER_CTX * ctx, const unsigned char *key,
	      const unsigned char *iv, int enc)
{
	RC4_set_key_ex(data(ctx)->ks,
		     EVP_CIPHER_CTX_key_length(ctx), key, 0);
	return 1;
}

static int
rc4_crypt(EVP_CIPHER_CTX * ctx, unsigned char *out,
	const unsigned char *in, unsigned int inl)
{
	RC4(data(ctx)->ks,inl,in,out);
	return 1;
}

#ifndef NID_rc4
#define NID_rc4 4
#endif

static const EVP_CIPHER rc4_cipher[1] = {{ 
    NID_rc4, 1, 16, 0, 
    EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH,
    rc4_init_key, rc4_crypt,
    NULL,
    sizeof(EVP_RC4_KEY), (int) /* !! */ NULL,
              (int) /* !! */ NULL,
    NULL, (int) /* !! */ NULL
}};
const EVP_CIPHER *EVP_rc4(void)
{
	return rc4_cipher;
}
