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

#ifndef K5S_EVP_H
#define K5S_EVP_H

typedef struct _md_ctx {
    const struct _md *md;
    void *md_data;
} EVP_MD_CTX;

typedef struct _md {
    int nid1, nid2;
    int digest_len;
    int dummy1;
    int (*init)(EVP_MD_CTX *);
    int (*update)(EVP_MD_CTX *, const void *, size_t);
    int (*final)(EVP_MD_CTX *, unsigned char *);
    void *dummy2, *dummy3;
    int pkey;
    int block_size;
    int ctx_size;
} EVP_MD;

#define NID_undef 0
#define EVP_PKEY_NULL_method 0

#define EVP_MD_size(m)	((m)->digest_len)
#define EVP_MD_block_size(m)	((m)->block_size)

#define EVP_MD_CTX_init(c)	(memset((c),0,sizeof *c))
int EVP_DigestInit_ex(EVP_MD_CTX *, const EVP_MD *, void *);
#define EVP_DigestUpdate(c,b,l)	((c)->md->update((c),(b),(l)))
int EVP_MD_CTX_cleanup(EVP_MD_CTX *);
int EVP_DigestFinal_ex(EVP_MD_CTX *, unsigned char *, unsigned int *);

#define MAX_BLOCK_SIZE 16

typedef struct _cipher_ctx {
    const struct _cipher *cipher;
    void *cipher_data;
    int key_len;
    int ctx_len;
    unsigned char iv[MAX_BLOCK_SIZE];
    int num;
    int encrypt;
    int nopadding;
    unsigned char buf[MAX_BLOCK_SIZE];
    int used;
} EVP_CIPHER_CTX;

#define EVP_CIPHER_CTX_key_length(c)	((c)->key_len)
#define EVP_CIPHER_CTX_iv_length(c)	((c)->cipher->ivec_size)
#define EVP_CIPHER_block_size(m)	((m)->block_size)
#define EVP_CIPHER_key_length(m)	((m)->key_size)
#define EVP_CIPHER_iv_length(m)		((m)->ivec_size)

typedef struct _cipher {
    int nid;
    int block_size;
    int key_size;
    int ivec_size;
    int flags;
    int (*init_key)(EVP_CIPHER_CTX *, const unsigned char *, const unsigned char *, int);
    int (*update)(EVP_CIPHER_CTX *, unsigned char *out, const unsigned char *, unsigned int);
    int (*cleanup)(EVP_CIPHER_CTX *);
    int ctx_size;
    void * set_asn1, * get_asn1;
    int (*ctrl)(EVP_CIPHER_CTX *, int, int, void *);
    void * dummy6;
} EVP_CIPHER;

#define EVP_CIPH_STREAM_CIPHER 0
#define EVP_CIPH_VARIABLE_LENGTH 1
#define EVP_CIPH_CBC_MODE 2
#define EVP_CIPH_CFB_MODE 4
#define EVP_CIPH_OFB_MODE 8
#define EVP_CIPH_ECB_MODE 16
#define EVP_CIPHER_CTX_flags(c)	((c)->cipher->flags & ~(EVP_CIPH_VARIABLE_LENGTH))
#define EVP_CIPHER_set_asn1_iv 0
#define EVP_CIPHER_get_asn1_iv 0

#define EVP_CIPHER_CTX_init(c)	(memset((c),0,sizeof *c))
int EVP_CipherInit_ex(EVP_CIPHER_CTX *,
const EVP_CIPHER *, void *, const unsigned char *,
    const unsigned char *, int);
#define EVP_DecryptInit_ex(c,m,e,k,i)	EVP_CipherInit_ex(c,m,e,k,i,0)
#define EVP_CIPHER_CTX_set_padding(c,p)	((c)->nopadding=!(p),1)
int EVP_DecryptUpdate(EVP_CIPHER_CTX *, unsigned char *, int *,
    const unsigned char *, int);
int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *, unsigned char *, int *);
int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *);
#define EVP_EncryptInit_ex(c,m,e,k,i)	EVP_CipherInit_ex(c,m,e,k,i,1)
int EVP_EncryptUpdate(EVP_CIPHER_CTX *, unsigned char *, int *,
    const unsigned char *, int);
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *, unsigned char *, int *);

const EVP_MD *EVP_md4();
const EVP_MD *EVP_md5();
const EVP_MD *EVP_sha1();

const EVP_CIPHER *EVP_des_cbc();
const EVP_CIPHER *EVP_cast5_cbc();
const EVP_CIPHER *EVP_des_ede3_cbc();
const EVP_CIPHER *EVP_aes_128_cbc();
const EVP_CIPHER *EVP_aes_256_cbc();
const EVP_CIPHER *EVP_rc4();
void ERR_print_errors_fp(void *);

const EVP_CIPHER *EVP_aes_128_cfb();
const EVP_CIPHER *EVP_aes_128_ofb();
const EVP_CIPHER *EVP_aes_128_ecb();
const EVP_CIPHER *EVP_aes_192_cbc();
const EVP_CIPHER *EVP_aes_192_cfb();
const EVP_CIPHER *EVP_aes_192_ofb();
const EVP_CIPHER *EVP_aes_192_ecb();
const EVP_CIPHER *EVP_aes_256_cfb();
const EVP_CIPHER *EVP_aes_256_ofb();
const EVP_CIPHER *EVP_aes_256_ecb();
const EVP_CIPHER *EVP_cast5_cfb();
const EVP_CIPHER *EVP_cast5_ofb();
const EVP_CIPHER *EVP_cast5_ecb();
const EVP_CIPHER *EVP_des_cfb();
const EVP_CIPHER *EVP_des_ofb();
const EVP_CIPHER *EVP_des_ecb();
const EVP_CIPHER *EVP_des_ede_cbc();
const EVP_CIPHER *EVP_des_ede_cfb();
const EVP_CIPHER *EVP_des_ede_ofb();
const EVP_CIPHER *EVP_des_ede_ecb();
const EVP_CIPHER *EVP_des_ede3_cfb();
const EVP_CIPHER *EVP_des_ede3_ofb();
const EVP_CIPHER *EVP_des_ede3_ecb();
const EVP_CIPHER *EVP_rc6_128_cbc();
const EVP_CIPHER *EVP_rc6_128_cfb();
const EVP_CIPHER *EVP_rc6_128_ofb();
const EVP_CIPHER *EVP_rc6_128_ecb();

int RAND_bytes(unsigned char *, int);
int RAND_seed(const void *, int);
int RAND_status(void);

#ifndef NULL
#define NULL	0U
#endif

#endif /* K5S_EVP_H */
