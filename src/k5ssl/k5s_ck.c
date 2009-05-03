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

#include "k5s_config.h"

#if defined(KERNEL)
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs_stats.h"
#else
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strcasecmp(p,q)	strcasecmp(p,q)
#include <errno.h>
#endif
#include <sys/types.h>
#ifdef USING_SSL
#include <openssl/evp.h>
#endif
#ifdef USING_FAKESSL
#include "k5s_evp.h"
#endif
#include "k5ssl.h"
#include "k5s_im.h"

#if KRB5_DES_SUPPORT
extern const EVP_MD *EVP_crc32();
#endif
#if KRB5_RC6_SUPPORT
extern const EVP_CIPHER *EVP_rc6_128_cbc(void);
#endif
static const struct krb5_enctypes * find_enctype(krb5_enctype enctype);
krb5_error_code krb5i_derive_key(const struct krb5_enc_provider *,
    const krb5_keyblock *, krb5_keyblock *, const krb5_data *);

int krb5i_maperr(int code)
{
    if (code == 1) {
	code = KRB5_CRYPTO_INTERNAL;
#if 0
	ERR_print_errors_fp(stderr);
#endif
    }
    return code;
}

#if KRB5_DES_SUPPORT
int krb5i_crc32_hashsize(unsigned int *output)
{
    const EVP_MD *md = EVP_crc32();

    *output = EVP_MD_size(md);
    return 0;
}

int krb5i_crc32_blocksize(unsigned int *output)
{
    const EVP_MD *md = EVP_crc32();

    *output = EVP_MD_block_size(md);
    return 0;
}

int krb5i_crc32_hashinit(void **state)
{
    EVP_MD_CTX *ctx;
    int code;

    ctx = afs_osi_Alloc(sizeof *ctx);
    EVP_MD_CTX_init(ctx);
    if ((code = !EVP_DigestInit_ex(ctx, EVP_crc32(), NULL))) goto Out;
    if ((code = !EVP_DigestUpdate(ctx, "uysonue", 7))) goto Out;
Out:
    if ((code = krb5i_maperr(code))) {
	if (ctx) {
	    EVP_MD_CTX_cleanup(ctx);
	    afs_osi_Free(ctx, sizeof(*ctx));
	}
	ctx = 0;
    }
    *state = (void *) ctx;
    return code;
}

int krb5i_crc32_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;

    code = !EVP_DigestUpdate(ctx, buf, len);
    return krb5i_maperr(code);
}

int krb5i_crc32_digest(void *state, unsigned char *digest)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;
    unsigned digestlen;
    int i, t;

    if ((code = !EVP_DigestFinal_ex(ctx, digest, &digestlen))) goto Out;
    for (i = 0; i < digestlen/2; ++i) {
	t = digest[i];
	digest[i] = ~digest[digestlen + ~i];
	digest[digestlen + ~i] = ~t;
    }
Out:
    code = krb5i_maperr(code);
    return code;
}

int krb5i_crc32_freestate(void *state)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;

    if (ctx) {
	EVP_MD_CTX_cleanup(ctx);
	afs_osi_Free(ctx, sizeof(*ctx));
    }
    return 0;
}

const struct krb5_hash_provider krb5i_hash_crc32[1] = {{
    krb5i_crc32_hashsize,
    krb5i_crc32_blocksize,
    krb5i_crc32_hashinit,
    NULL,
    krb5i_crc32_hashupdate,
    krb5i_crc32_digest,
    krb5i_crc32_freestate,
}};
#endif	/* KRB5_DES_SUPPORT */

int krb5i_md4_hashsize(unsigned int *output)
{
    const EVP_MD *md = EVP_md4();

    *output = EVP_MD_size(md);
    return 0;
}

int krb5i_md4_blocksize(unsigned int *output)
{
    const EVP_MD *md = EVP_md4();

    *output = EVP_MD_block_size(md);
    return 0;
}

int krb5i_md4_hashinit(void **state)
{
    EVP_MD_CTX *ctx;
    int code;

    ctx = afs_osi_Alloc(sizeof *ctx);
    EVP_MD_CTX_init(ctx);
    code = !EVP_DigestInit_ex(ctx, EVP_md4(), NULL);
    if ((code = krb5i_maperr(code))) {
	if (ctx) {
	    EVP_MD_CTX_cleanup(ctx);
	    afs_osi_Free(ctx, sizeof(*ctx));
	}
	ctx = 0;
    }
    *state = (void *) ctx;
    return code;
}

int krb5i_md4_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;

    code = !EVP_DigestUpdate(ctx, buf, len);
    return krb5i_maperr(code);
}

int krb5i_md4_digest(void *state, unsigned char *digest)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;
    unsigned digestlen;

    code = !EVP_DigestFinal_ex(ctx, digest, &digestlen);
    return krb5i_maperr(code);
}

int krb5i_md4_freestate(void *state)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    if (ctx) {
	EVP_MD_CTX_cleanup(ctx);
	afs_osi_Free(ctx, sizeof(*ctx));
    }
    return 0;
}

const struct krb5_hash_provider krb5i_hash_md4[1] = {{
    krb5i_md4_hashsize,
    krb5i_md4_blocksize,
    krb5i_md4_hashinit,
    NULL,
    krb5i_md4_hashupdate,
    krb5i_md4_digest,
    krb5i_md4_freestate,
}};

int krb5i_md5_hashsize(unsigned int *output)
{
    const EVP_MD *md = EVP_md5();

    *output = EVP_MD_size(md);
    return 0;
}

int krb5i_md5_blocksize(unsigned int *output)
{
    const EVP_MD *md = EVP_md5();

    *output = EVP_MD_block_size(md);
    return 0;
}

int krb5i_md5_hashinit(void **state)
{
    EVP_MD_CTX *ctx;
    int code;

    ctx = afs_osi_Alloc(sizeof *ctx);
    EVP_MD_CTX_init(ctx);
    code = !EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    if ((code = krb5i_maperr(code))) {
	if (ctx) {
	    EVP_MD_CTX_cleanup(ctx);
	    afs_osi_Free(ctx, sizeof(*ctx));
	}
	ctx = 0;
    }
    *state = (void *) ctx;
    return code;
}

int krb5i_md5_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;

    code = !EVP_DigestUpdate(ctx, buf, len);
    return krb5i_maperr(code);
}

int krb5i_md5_digest(void *state, unsigned char *digest)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;
    unsigned digestlen;

    code = !EVP_DigestFinal_ex(ctx, digest, &digestlen);
    return krb5i_maperr(code);
}

int krb5i_md5_freestate(void *state)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    if (ctx) {
	EVP_MD_CTX_cleanup(ctx);
	afs_osi_Free(ctx, sizeof(*ctx));
    }
    return 0;
}

const struct krb5_hash_provider krb5i_hash_md5[1] = {{
    krb5i_md5_hashsize,
    krb5i_md5_blocksize,
    krb5i_md5_hashinit,
    NULL,
    krb5i_md5_hashupdate,
    krb5i_md5_digest,
    krb5i_md5_freestate,
}};

#if KRB5_DES_SUPPORT
struct state_md_des {
    EVP_MD_CTX mdctx[1];
    unsigned int keysize, confsize, digestsize, totalsize;
    unsigned char *confounder;
    unsigned char *ivec;
    unsigned char *digest;
    unsigned char key[4];
};

int krb5i_md_des_hashsize(const EVP_MD *md, unsigned int *output)
{
    const EVP_CIPHER *cipher = EVP_des_cbc();

    *output = EVP_MD_size(md) + EVP_CIPHER_block_size(cipher);
    return 0;
}

int krb5i_md_des_blocksize(const EVP_MD *md, unsigned int *output)
{
    *output = EVP_MD_block_size(md);
    return 0;
}

int krb5i_md_des_hashinit(const EVP_MD *md, void **state)
{
    struct state_md_des *mdstate;
    int code;
    const EVP_CIPHER *cipher = EVP_des_cbc();
    unsigned int confsize = EVP_CIPHER_block_size(cipher);
    unsigned int digestsize = EVP_MD_size(md);
    unsigned int keysize = EVP_CIPHER_key_length(cipher);
    unsigned int totalsize = keysize + 2*confsize + digestsize;

    code = ENOMEM;
    mdstate = afs_osi_Alloc(sizeof *mdstate + totalsize);
    if (!mdstate) goto Out;
    mdstate->totalsize = totalsize;
    mdstate->keysize = keysize;
    mdstate->confsize = confsize;
    mdstate->digestsize = digestsize;
    mdstate->ivec = mdstate->key + confsize;
    mdstate->confounder = mdstate->ivec + keysize;
    mdstate->digest = mdstate->confounder + confsize;
    EVP_MD_CTX_init(mdstate->mdctx);
    if ((code = !EVP_DigestInit_ex(mdstate->mdctx, md, NULL))) goto Out;
Out:
    if ((code = krb5i_maperr(code))) {
	EVP_MD_CTX_cleanup(mdstate->mdctx);
	afs_osi_Free(mdstate, sizeof *mdstate + totalsize);
	mdstate = 0;
    }
    *state = (void *) mdstate;
    return code;
}

int krb5i_md_des_setkey(void * state,
    krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    unsigned char *expected)
{
    struct state_md_des *mdstate = (struct state_md_des *) state;
    krb5_data data[1];
    int code;
    const EVP_CIPHER *cipher;
    EVP_CIPHER_CTX ctx[1];
    unsigned char *t;
    int length;
    int i;

    if (mdstate->keysize != key->length)
	return KRB5_BAD_KEYSIZE;
    memcpy(mdstate->key, key->contents, mdstate->keysize);
    for (i = 0; i < mdstate->keysize; ++i)
	mdstate->key[i] ^= 0xf0;
    EVP_CIPHER_CTX_init(ctx);
    if (expected) {
	cipher = EVP_des_cbc();
	memset(mdstate->ivec, 0, mdstate->confsize);
	if ((code = !EVP_DecryptInit_ex(ctx, cipher, 0,
	    mdstate->key, mdstate->ivec)))
	    goto Out;
	if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0)))
	    goto Out;
	t = mdstate->confounder;
	if ((code = !EVP_DecryptUpdate(ctx, t, &length,
	    expected, mdstate->confsize)))
	    goto Out;
	t += length;
	if ((code = !EVP_DecryptFinal_ex(ctx, t, &length)))
	    goto Out;
	t += length;
	code = KRB5_CRYPTO_INTERNAL;
	if (t-mdstate->confounder != mdstate->confsize)
	    goto Out;
    } else {
	data->data = mdstate->confounder;
	data->length = mdstate->confsize;
	if ((code = krb5_c_random_make_octets(context, data)))
	    goto Out;
    }

    code = !EVP_DigestUpdate(mdstate->mdctx, mdstate->confounder,
	mdstate->confsize);
Out:
    EVP_CIPHER_CTX_cleanup(ctx);

    return krb5i_maperr(code);
}

int krb5i_md_des_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    struct state_md_des *mdstate = (struct state_md_des *) state;
    int code;

    code = !EVP_DigestUpdate(mdstate->mdctx, buf, len);
    return krb5i_maperr(code);
}

int krb5i_md_des_digest(void *state, unsigned char *digest)
{
    struct state_md_des *mdstate = (struct state_md_des *) state;
    int code;
    unsigned digestlen;
    int length;
    unsigned char *t;
    const EVP_CIPHER *cipher = EVP_des_cbc();
    EVP_CIPHER_CTX ctx[1];

    EVP_CIPHER_CTX_init(ctx);
    if ((code = !EVP_DigestFinal_ex(mdstate->mdctx, mdstate->digest,
	    &digestlen)))
	goto Out;
    code = KRB5_CRYPTO_INTERNAL;
    if (digestlen != mdstate->digestsize) {
#if 0
fprintf(stderr,"krb5i_md_des_digest: length=%d mdstate->digestsize=%d\n",
digestlen ,mdstate->digestsize);
#endif
	goto Out;
    }
    memset(mdstate->ivec, 0, mdstate->confsize);
    if ((code = !EVP_EncryptInit_ex(ctx, cipher, 0,
	    mdstate->key, mdstate->ivec)))
	goto Out;
    if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0)))
	goto Out;
    t = digest;
    if ((code = !EVP_EncryptUpdate(ctx, t, &length,
	mdstate->confounder, mdstate->confsize + mdstate->digestsize)))
	goto Out;
    t += length;
    if ((code = !EVP_EncryptFinal_ex(ctx, t, &length)))
	goto Out;
    t += length;
    if (t-digest != mdstate->confsize + mdstate->digestsize)
	code = KRB5_CRYPTO_INTERNAL;
Out:
    EVP_CIPHER_CTX_cleanup(ctx);
    return krb5i_maperr(code);
}

int krb5i_md_des_freestate(void *state)
{
    struct state_md_des *mdstate = (struct state_md_des *) state;

    if (mdstate) {
	EVP_MD_CTX_cleanup(mdstate->mdctx);
	memset(mdstate, 0, sizeof *mdstate + mdstate->totalsize);
	afs_osi_Free(mdstate, sizeof *mdstate + mdstate->totalsize);
    }
    return 0;
}

int krb5i_md4des_hashsize(unsigned int *output)
{
    return krb5i_md_des_hashsize(EVP_md4(), output);
}

int krb5i_md4des_blocksize(unsigned int *output)
{
    return krb5i_md_des_blocksize(EVP_md4(), output);
}

int krb5i_md4des_hashinit(void **state)
{
    return krb5i_md_des_hashinit(EVP_md4(), state);
}

const struct krb5_hash_provider krb5i_keyhash_md4des[1] = {{
    krb5i_md4des_hashsize,
    krb5i_md4des_blocksize,
    krb5i_md4des_hashinit,
    krb5i_md_des_setkey,
    krb5i_md_des_hashupdate,
    krb5i_md_des_digest,
    krb5i_md_des_freestate,
}};

int krb5i_md5des_hashsize(unsigned int *output)
{
    return krb5i_md_des_hashsize(EVP_md5(), output);
}

int krb5i_md5des_blocksize(unsigned int *output)
{
    return krb5i_md_des_blocksize(EVP_md5(), output);
}

int krb5i_md5des_hashinit(void **state)
{
    return krb5i_md_des_hashinit(EVP_md5(), state);
}

const struct krb5_hash_provider krb5i_keyhash_md5des[1] = {{
    krb5i_md5des_hashsize,
    krb5i_md5des_blocksize,
    krb5i_md5des_hashinit,
    krb5i_md_des_setkey,
    krb5i_md_des_hashupdate,
    krb5i_md_des_digest,
    krb5i_md_des_freestate,
}};
#endif /* KRB5_DES_SUPPORT */

#if KRB5_DES_SUPPORT || KRB5_CAST_SUPPORT || KRB5_RC6_SUPPORT
struct state_xcbc {
    EVP_CIPHER_CTX ctx[1];
    const EVP_CIPHER *cipher;
    unsigned int ivecsize;
    unsigned char *lastcb;
    unsigned char junk[512];
};

int krb5i_xcbc_hashsize(const EVP_CIPHER *cipher, unsigned int *output)
{
    *output = EVP_CIPHER_block_size(cipher);
    return 0;
}

int krb5i_xcbc_blocksize(const EVP_CIPHER *cipher, unsigned int *out)
{
    *out = EVP_CIPHER_block_size(cipher);
    return 0;
}

int krb5i_xcbc_hashinit(const EVP_CIPHER *cipher, void **state)
{
    struct state_xcbc *mdstate;
    int code;
    unsigned int ivecsize;

    ivecsize = EVP_CIPHER_block_size(cipher);
    if (ivecsize > sizeof mdstate->junk)
	return KRB5_CRYPTO_INTERNAL;
    mdstate = afs_osi_Alloc(sizeof *mdstate);
    if (!mdstate) return ENOMEM;
    EVP_CIPHER_CTX_init(mdstate->ctx);
    mdstate->cipher = cipher;
    mdstate->ivecsize = ivecsize;
    memset(mdstate->junk, 0, mdstate->ivecsize);
    code = 0;
#if 0	/* can't happen - code always 0 here */
Out:
    if ((code = krb5i_maperr(code))) {
	EVP_CIPHER_CTX_cleanup(mdstate->ctx);
	afs_osi_Free(mdstate, sizeof *mdstate);
	mdstate = 0;
    }
#endif
    *state = (void *) mdstate;
    return code;
}

int krb5i_xcbc_setkey(void * state,
    krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    unsigned char *expected)
{
    struct state_xcbc *mdstate = (struct state_xcbc *) state;
    int code;

    if (EVP_CIPHER_key_length(mdstate->cipher) != key->length)
	return KRB5_BAD_KEYSIZE;

    mdstate->lastcb = mdstate->junk;
    if ((code = !EVP_EncryptInit_ex(mdstate->ctx, mdstate->cipher, 0,
	    key->contents, mdstate->junk)))
	goto Out;
    if ((code = !EVP_CIPHER_CTX_set_padding(mdstate->ctx, 0)))
	goto Out;
Out:
    return krb5i_maperr(code);
}

int krb5i_xcbc_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    struct state_xcbc *mdstate = (struct state_xcbc *) state;
    int code;
    int length;
    unsigned int cs, left;
    const unsigned char *t;

    code = 0;
    for (t = buf, left = len; left; t += cs, left -= cs) {
	cs = sizeof(mdstate->junk);
	if (cs > left)
	    cs = left;
	if ((code = !EVP_EncryptUpdate(mdstate->ctx, mdstate->junk, &length,
	    t, cs)))
	    goto Out;
	if (length)
	    mdstate->lastcb = mdstate->junk + length - mdstate->ivecsize;
    }
Out:
    return krb5i_maperr(code);
}

int krb5i_xcbc_digest(void *state, unsigned char *digest)
{
    struct state_xcbc *mdstate = (struct state_xcbc *) state;
    int code;
    int length;

    if ((code = !EVP_EncryptFinal_ex(mdstate->ctx, mdstate->junk, &length)))
	goto Out;
    if (length)
	mdstate->lastcb = mdstate->junk + length - mdstate->ivecsize;
    memcpy(digest, mdstate->lastcb, mdstate->ivecsize);
Out:
    return krb5i_maperr(code);
}

int krb5i_xcbc_freestate(void *state)
{
    struct state_xcbc *mdstate = (struct state_xcbc *) state;

    if (mdstate) {
	EVP_CIPHER_CTX_cleanup(mdstate->ctx);
	memset(mdstate, 0, sizeof *mdstate);
	afs_osi_Free(mdstate, sizeof *mdstate);
    }
    return 0;
}

int krb5i_xcbc_setivec(void *state, const unsigned char *buf,
    unsigned int len)
{
    struct state_xcbc *mdstate = (struct state_xcbc *) state;
    int code;
    int length;
    unsigned int cs, left;
    const unsigned char *t;

    if (len > sizeof mdstate->junk) return KRB5_BAD_KEYSIZE;
    memcpy(mdstate->junk, buf, len);
    return 0;
}
#endif	/* KRB5_DES_SUPPORT || KRB5_CAST_SUPPORT || KRB5_RC6_SUPPORT */

#if KRB5_DES_SUPPORT
int krb5i_descbc_hashsize(unsigned int *output)
{
    return krb5i_xcbc_hashsize(EVP_des_cbc(), output);
}

int krb5i_descbc_blocksize(unsigned int *out)
{
    return krb5i_xcbc_blocksize(EVP_des_cbc(), out);
}

int krb5i_descbc_hashinit(void **state)
{
    return krb5i_xcbc_hashinit(EVP_des_cbc(), state);
}

const struct krb5_hash_provider krb5i_keyhash_descbc[1] = {{
    krb5i_descbc_hashsize,
    krb5i_descbc_blocksize,
    krb5i_descbc_hashinit,
    krb5i_xcbc_setkey,
    krb5i_xcbc_hashupdate,
    krb5i_xcbc_digest,
    krb5i_xcbc_freestate,
    krb5i_xcbc_setivec,
}};
#endif	/* KRB5_DES_SUPPORT */

#if KRB5_CAST_SUPPORT
int krb5i_castcbc_hashsize(unsigned int *output)
{
    return krb5i_xcbc_hashsize(EVP_cast5_cbc(), output);
}

int krb5i_castcbc_blocksize(unsigned int *out)
{
    return krb5i_xcbc_blocksize(EVP_cast5_cbc(), out);
}

int krb5i_castcbc_hashinit(void **state)
{
    return krb5i_xcbc_hashinit(EVP_cast5_cbc(), state);
}

const struct krb5_hash_provider krb5i_keyhash_castcbc[1] = {{
    krb5i_castcbc_hashsize,
    krb5i_castcbc_blocksize,
    krb5i_castcbc_hashinit,
    krb5i_xcbc_setkey,
    krb5i_xcbc_hashupdate,
    krb5i_xcbc_digest,
    krb5i_xcbc_freestate,
    krb5i_xcbc_setivec,
}};
#endif /* KRB5_CAST_SUPPORT */

#if KRB5_RC6_SUPPORT
int krb5i_rc6cbc_hashsize(unsigned int *output)
{
    return krb5i_xcbc_hashsize(EVP_rc6_128_cbc(), output);
}

int krb5i_rc6cbc_blocksize(unsigned int *out)
{
    return krb5i_xcbc_blocksize(EVP_rc6_128_cbc(), out);
}

int krb5i_rc6cbc_hashinit(void **state)
{
    return krb5i_xcbc_hashinit(EVP_rc6_128_cbc(), state);
}

const struct krb5_hash_provider krb5i_keyhash_rc6cbc[1] = {{
    krb5i_rc6cbc_hashsize,
    krb5i_rc6cbc_blocksize,
    krb5i_rc6cbc_hashinit,
    krb5i_xcbc_setkey,
    krb5i_xcbc_hashupdate,
    krb5i_xcbc_digest,
    krb5i_xcbc_freestate,
    krb5i_xcbc_setivec,
}};
#endif /* KRB5_RC6_SUPPORT */

int krb5i_sha1_hashsize(unsigned int *output)
{
    const EVP_MD *md = EVP_sha1();

    *output = EVP_MD_size(md);
    return 0;
}

int krb5i_sha1_blocksize(unsigned int *output)
{
    const EVP_MD *md = EVP_sha1();

    *output = EVP_MD_block_size(md);
    return 0;
}

int krb5i_sha1_hashinit(void **state)
{
    EVP_MD_CTX *ctx;
    int code;

    ctx = afs_osi_Alloc(sizeof *ctx);
    EVP_MD_CTX_init(ctx);
    code = !EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
    if ((code = krb5i_maperr(code))) {
	if (ctx) {
	    EVP_MD_CTX_cleanup(ctx);
	    afs_osi_Free(ctx, sizeof *ctx);
	}
	ctx = 0;
    }
    *state = (void *) ctx;
    return code;
}

int krb5i_sha1_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;

    code = !EVP_DigestUpdate(ctx, buf, len);
    return krb5i_maperr(code);
}

int krb5i_sha1_digest(void *state, unsigned char *digest)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    int code;
    unsigned digestlen;

    code = !EVP_DigestFinal_ex(ctx, digest, &digestlen);
    return krb5i_maperr(code);
}

int krb5i_sha1_freestate(void *state)
{
    EVP_MD_CTX *ctx = (EVP_MD_CTX *) state;
    if (ctx) {
	EVP_MD_CTX_cleanup(ctx);
	afs_osi_Free(ctx, sizeof *ctx);
    }
    return 0;
}

const struct krb5_hash_provider krb5i_hash_sha1[1] = {{
    krb5i_sha1_hashsize,
    krb5i_sha1_blocksize,
    krb5i_sha1_hashinit,
    NULL,
    krb5i_sha1_hashupdate,
    krb5i_sha1_digest,
    krb5i_sha1_freestate,
}};

struct state_hmac {
    const struct krb5_hash_provider *hash;
    void *ihash, *ohash;
    unsigned int blocksize;
    unsigned int hashsize;
};

int krb5i_hmac_x_hashsize(const struct krb5_hash_provider *hash,
    unsigned int *output)
{
    return hash->hash_size(output);
}

int krb5i_hmac_x_blocksize(const struct krb5_hash_provider *hash,
    unsigned int *output)
{
    return hash->block_size(output);
}

#define KRB5I_MAXDIGEST 128
int krb5i_hmac_x_hashinit(const struct krb5_hash_provider *hash, void **state)
{
    struct state_hmac *mdstate;
    int code;
    unsigned int blocksize;

    if ((code = hash->block_size(&blocksize))) return code;
    if (blocksize > KRB5I_MAXDIGEST) return KRB5_CRYPTO_INTERNAL;
    mdstate = afs_osi_Alloc(sizeof *mdstate);
    if (!mdstate) return ENOMEM;
    memset(mdstate, 0, sizeof *mdstate);
    mdstate->hash = hash;
    mdstate->blocksize = blocksize;
    if ((code = hash->hash_init(&mdstate->ihash))) goto Out;
    if ((code = hash->hash_init(&mdstate->ohash))) goto Out;
    if ((code = hash->hash_size(&mdstate->hashsize))) goto Out;
Out:
    if (code) {
	if (mdstate->ihash)
	    hash->free_state(mdstate->ihash);
	if (mdstate->ohash)
	    hash->free_state(mdstate->ohash);
	afs_osi_Free(mdstate, sizeof *mdstate);
	mdstate = 0;
    }
    *state = (void *) mdstate;
    return code;
}

int krb5i_hmac_setkey(void * state,
    krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    unsigned char *expected)
{
    struct state_hmac *mdstate = (struct state_hmac *) state;
    int code;
    int i;
    unsigned char pad[KRB5I_MAXDIGEST];

    if (key->length > mdstate->blocksize)
	return KRB5_CRYPTO_INTERNAL;

    memcpy(pad, key->contents, key->length);
    if (key->length != mdstate->blocksize)
	memset(pad+key->length, 0,
	    mdstate->blocksize - key->length);
    for (i=0; i < mdstate->blocksize; ++i)
	pad[i] ^= 0x36;
    code = mdstate->hash->hash_update(mdstate->ihash, pad, mdstate->blocksize);
    if (code) goto Out;
    for (i=0; i < mdstate->blocksize; ++i)
	pad[i] ^= (0x36 ^ 0x5c);
    code = mdstate->hash->hash_update(mdstate->ohash, pad, mdstate->blocksize);
Out:
    return code;
}

int krb5i_hmac_x_setkey(void * state,
    krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    unsigned char *expected,
    int usage2)
{
    int code;
    krb5_keyblock checkkey[1];
    krb5_data constant[1];
    unsigned char cd[5];
    const struct krb5_enctypes * et = find_enctype(key->enctype);

    if (!et) return KRB5_BAD_ENCTYPE;

    memset(checkkey, 0, sizeof *checkkey);
    checkkey->contents = afs_osi_Alloc(checkkey->length = key->length);
    if (!checkkey->contents) return ENOMEM;
    cd[0] = usage>>24; cd[1] = usage>>16; cd[2] = usage>>8; cd[3] = usage;
    cd[4] = usage2;
    constant->data = cd; constant->length = 5;
    if ((code = krb5i_derive_key(et->enc, key, checkkey, constant)))
	goto Out;

    code = krb5i_hmac_setkey(state, context, checkkey, usage, expected);
Out:
    if (checkkey->contents) {
	memset(checkkey->contents, 0, checkkey->length);
	afs_osi_Free(checkkey->contents, checkkey->length);
    }
    return code;
}

int krb5i_hmac_x99_setkey(void * state,
    krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    unsigned char *expected)
{
    return krb5i_hmac_x_setkey(state, context, key, usage, expected, 0x99);
}

int krb5i_hmac_x_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    struct state_hmac *mdstate = (struct state_hmac *) state;
    int code;

    code = mdstate->hash->hash_update(mdstate->ihash, buf, len);
    return code;
}

int krb5i_hmac_x_digest(void *state, unsigned char *digest)
{
    struct state_hmac *mdstate = (struct state_hmac *) state;
    int code;

    code = mdstate->hash->hash_digest(mdstate->ihash, digest);
    if (code) goto Out;
    code = mdstate->hash->hash_update(mdstate->ohash, digest,
	mdstate->hashsize);
    if (code) goto Out;
    code = mdstate->hash->hash_digest(mdstate->ohash, digest);
Out:
    return code;
}

int krb5i_hmac_x_freestate(void *state)
{
    struct state_hmac *mdstate = (struct state_hmac *) state;
    if (mdstate) {
	mdstate->hash->free_state(mdstate->ihash);
	mdstate->hash->free_state(mdstate->ohash);
	afs_osi_Free(mdstate, sizeof *mdstate);
    }
    return 0;
}

krb5_error_code
krb5i_hmac(const struct krb5_hash_provider *hash,
    const krb5_keyblock *key,
    int icount,
    const krb5_data *input,
    krb5_data *output)
{
    int code;
    unsigned int hs;
    void *state = 0;
    int i;

    if (!icount) return KRB5_CRYPTO_INTERNAL;
    if ((code = hash->hash_size(&hs))) goto Done;
    if (output->length < hs) {
      return KRB5_BAD_MSIZE;
    }
    if ((code = krb5i_hmac_x_hashinit(hash, &state))) goto Done;
    if ((code = krb5i_hmac_setkey(state, /* XXX */0, key, 0, 0))) goto Done;
    for (i = 0; i < icount; ++i, ++input) {
	code = krb5i_hmac_x_hashupdate(state, input->data, input->length);
	if (code) goto Done;
    }
    code = krb5i_hmac_x_digest(state, output->data);
    output->length = hs;
Done:
    if (state)
	krb5i_hmac_x_freestate(state);
    return code;
}

int krb5i_hmac_sha1_hashsize(unsigned int *output)
{
    return krb5i_hmac_x_hashsize(krb5i_hash_sha1, output);
}

int krb5i_hmac_sha1_blocksize(unsigned int *output)
{
    return krb5i_hmac_x_blocksize(krb5i_hash_sha1, output);
}

#define KRB5I_MAXDIGEST 128
int krb5i_hmac_sha1_hashinit(void **state)
{
    return krb5i_hmac_x_hashinit(krb5i_hash_sha1, state);
}

const struct krb5_hash_provider krb5i_hash_hmac_sha1[1] = {{
    krb5i_hmac_sha1_hashsize,
    krb5i_hmac_sha1_blocksize,
    krb5i_hmac_sha1_hashinit,
    krb5i_hmac_x99_setkey,
    krb5i_hmac_x_hashupdate,
    krb5i_hmac_x_digest,
    krb5i_hmac_x_freestate,
}};

#if KRB5_ECEKE_SUPPORT
const struct krb5_hash_provider krb5i_keyhash_old_hmac_sha[1] = {{
    krb5i_hmac_sha1_hashsize,
    krb5i_hmac_sha1_blocksize,
    krb5i_hmac_sha1_hashinit,
    krb5i_hmac_setkey,
    krb5i_hmac_x_hashupdate,
    krb5i_hmac_x_digest,
    krb5i_hmac_x_freestate,
}};
#endif /* KRB5_ECEKE_SUPPORT */

#if KRB5_RC4_SUPPORT
int
krb5i_ms_usage(krb5_keyusage usage)
{
    switch(usage) {
    case 3: /* as-rep encrypted part */
    case 9: /* tgs-rep encrypted with subkey */
	return 8;
    case 23:	/* sign wrap token */
	return 13;
    /* case 1:	AS-REQ PA-ENC-TIMESTAMP padata timestamp, */
    /* case 2:	ticket from kdc */
    /* case 4:	tgs-req authz data */
    /* case 5:	tgs-req authz data in subkey */
    /* case 6:	tgs-req authenticator cksum */
    /* case 7:	tgs-req authenticator */
    /* case 8:	??? */
    /* case 10: ap-rep authentication cksum */
    /* case 11: app-req authenticator */
    /* case 12: app-rep encrypted part */
    /* default: ??? */
    }
    return usage;
}

struct state_hmac_md5_ms {
    const struct krb5_hash_provider *hash;
    void *ihash;
    unsigned int hashsize;
    int enctype;
    unsigned int kslen;
    unsigned char ks[512];
};

int krb5i_hmac_md5_ms_hashsize(unsigned int *output)
{
    return krb5i_hash_md5->hash_size(output);
}

int krb5i_hmac_md5_ms_blocksize(unsigned int *out)
{
    return krb5i_hash_md5->block_size(out);
}

int krb5i_hmac_md5_ms_hashinit(void **state)
{
    struct state_hmac_md5_ms *mdstate;
    int code;
    const struct krb5_hash_provider *hash = krb5i_hash_md5;
    unsigned int hs;

    mdstate = afs_osi_Alloc(sizeof *mdstate);
    if (!mdstate) return ENOMEM;
    if ((code = hash->hash_size(&hs))) goto Out;
    if ((code = hash->hash_init(&mdstate->ihash))) goto Out;
    mdstate->hash = hash;
    mdstate->hashsize = hs;
    mdstate->kslen = 0;
Out:
    if (code) {
	if (mdstate->ihash)
	    hash->free_state(mdstate->ihash);
	afs_osi_Free(mdstate, sizeof *mdstate);
	mdstate = 0;
    }
    *state = (void *) mdstate;
    return code;
}

int krb5i_hmac_md5_ms_setkey(void * state,
    krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    unsigned char *expected)
{
    struct state_hmac_md5_ms *mdstate = (struct state_hmac_md5_ms *) state;
    int code;
    krb5_data datain[1], dataout[1];
    static const unsigned char signaturekey[] = "signaturekey";
    unsigned char salt[4];

    if (key->length > sizeof mdstate->ks)
	return KRB5_BAD_KEYSIZE;
    mdstate->enctype = key->enctype;
    datain->data = (void *)signaturekey;
    datain->length = sizeof signaturekey;
    dataout->data = mdstate->ks;
    dataout->length = mdstate->kslen = key->length;
    code = krb5i_hmac(mdstate->hash, key, 1, datain, dataout);
    if (code) goto Out;
    usage = krb5i_ms_usage(usage);
    salt[3] = usage>>24; salt[2] = usage>>16;
    salt[1] = usage>>8;	 salt[0] = usage;
    code = mdstate->hash->hash_update(mdstate->ihash, salt, 4);
Out:
    return code;
}

int krb5i_hmac_md5_ms_hashupdate(void *state, const unsigned char *buf,
    unsigned int len)
{
    struct state_hmac_md5_ms *mdstate = (struct state_hmac_md5_ms *) state;
    int code;

    code = mdstate->hash->hash_update(mdstate->ihash, buf, len);
    return code;
}

int krb5i_hmac_md5_ms_digest(void *state, unsigned char *digest)
{
    struct state_hmac_md5_ms *mdstate = (struct state_hmac_md5_ms *) state;
    int code;
    krb5_keyblock ks[1];
    krb5_data data[1];

    code = mdstate->hash->hash_digest(mdstate->ihash, digest);
    if (code) goto Out;
    ks->enctype = mdstate->enctype;
    ks->length = mdstate->kslen;
    ks->contents = mdstate->ks;
    data->data = digest;
    data->length = mdstate->hashsize;
    code = krb5i_hmac(mdstate->hash, ks, 1, data, data);
Out:
    return krb5i_maperr(code);
}

int krb5i_hmac_md5_ms_freestate(void *state)
{
    struct state_hmac_md5_ms *mdstate = (struct state_hmac_md5_ms *) state;

    if (mdstate) {
	if (mdstate->ihash)
	    mdstate->hash->free_state(mdstate->ihash);
	memset(mdstate->ks, 0, sizeof mdstate->ks);
	afs_osi_Free(mdstate, sizeof *mdstate);
    }
    return 0;
}

const struct krb5_hash_provider krb5i_keyhash_hmac_md5_ms[1] = {{
    krb5i_hmac_md5_ms_hashsize,
    krb5i_hmac_md5_ms_blocksize,
    krb5i_hmac_md5_ms_hashinit,
    krb5i_hmac_md5_ms_setkey,
    krb5i_hmac_md5_ms_hashupdate,
    krb5i_hmac_md5_ms_digest,
    krb5i_hmac_md5_ms_freestate,
}};
#endif /* KRB5_RC4_SUPPORT */

const struct krb5_cksumtypes krb5i_cksumtypes_list[] = {
#if KRB5_DES_SUPPORT
    { CKSUMTYPE_CRC32, KRB5_CKSUMFLAG_NOT_COLL_PROOF, 0,
    krb5i_hash_crc32, 0, {"crc-32", NULL}},
#endif
    { CKSUMTYPE_RSA_MD4, 0, 0,
    krb5i_hash_md4, 0, {"md4",NULL}},
    { CKSUMTYPE_RSA_MD5, 0, 0,
    krb5i_hash_md5, 0, {"md5",NULL}},
    { CKSUMTYPE_NIST_SHA, 0, 0,
    krb5i_hash_sha1, 0, {"sha",NULL}},
#if KRB5_DES_SUPPORT
    { CKSUMTYPE_RSA_MD4_DES, KRB5_CKSUMFLAG_KEYED, ENCTYPE_DES_CBC_CRC, 
    krb5i_keyhash_md4des, 0, {"md4-des"}},
    { CKSUMTYPE_RSA_MD5_DES, KRB5_CKSUMFLAG_KEYED, ENCTYPE_DES_CBC_CRC, 
    krb5i_keyhash_md5des, 0, {"md5-des"}},
    { CKSUMTYPE_DESCBC, KRB5_CKSUMFLAG_KEYED, ENCTYPE_DES_CBC_CRC,
    krb5i_keyhash_descbc, 0, {"des-cbc", NULL}},
#endif
#if 1 || KRB5_DES3_SUPPORT
    { CKSUMTYPE_HMAC_SHA1_DES3,
    KRB5_CKSUMFLAG_KEYED | KRB5_CKSUMFLAG_DERIVE, 0,
    krb5i_hash_hmac_sha1, 0, {"hmac-sha1-des3", NULL}},
#endif
#if KRB5_AES_SUPPORT
    { CKSUMTYPE_HMAC_SHA1_96_AES128,
    KRB5_CKSUMFLAG_KEYED | KRB5_CKSUMFLAG_DERIVE, 0,
    krb5i_hash_hmac_sha1, 12, {"hmac-sha1-96-aes128", NULL}},
    { CKSUMTYPE_HMAC_SHA1_96_AES256,
    KRB5_CKSUMFLAG_KEYED | KRB5_CKSUMFLAG_DERIVE, 0,
    krb5i_hash_hmac_sha1, 12, {"hmac-sha1-96-aes256", NULL}},
#endif
#if KRB5_RC4_SUPPORT
    { CKSUMTYPE_HMAC_MD5_ARCFOUR, KRB5_CKSUMFLAG_KEYED, ENCTYPE_ARCFOUR_HMAC,
    krb5i_keyhash_hmac_md5_ms, 0,
    {"hmac-md5-rc4", "hmac-md5-enc", "hmac-md5-earcfour", NULL}},
#endif

#if KRB5_ECEKE_SUPPORT
    /* obselete */
    { CKSUMTYPE_OLD_HMAC_SHA, 0, 0,
    krb5i_keyhash_old_hmac_sha, 0, {"old-hmac-sha", NULL}},
#endif

#if KRB5_CAST_SUPPORT
    /* old mdw experimental */
    { CKSUMTYPE_CASTCBC, KRB5_CKSUMFLAG_KEYED, ENCTYPE_CAST_CBC_SHA,
    krb5i_keyhash_castcbc, 0, {"cast-cbc", NULL}},
#endif
#if KRB5_RC6_SUPPORT
    { CKSUMTYPE_RC6CBC, KRB5_CKSUMFLAG_KEYED, ENCTYPE_RC6_CBC_SHA,
    krb5i_keyhash_rc6cbc, 0, {"rc6-cbc", NULL}},
#endif
};

static const struct krb5_cksumtypes *
find_cksumtype(krb5_cksumtype cksumtype)
{
    int i;

    for (i = 0;
		i < sizeof krb5i_cksumtypes_list/sizeof *krb5i_cksumtypes_list;
		++i) {

	if (krb5i_cksumtypes_list[i].cksumtype == cksumtype)
	    return krb5i_cksumtypes_list + i;
    }
    return NULL;
}

krb5_error_code krb5_c_checksum_length(krb5_context context,
    krb5_cksumtype cksumtype,
    size_t *out)
{
    const struct krb5_cksumtypes *ct = find_cksumtype(cksumtype);
    unsigned int x;
    int r;
    if (!ct) return KRB5_BAD_ENCTYPE;
    if (ct->hash_trunc_size) {
	*out = ct->hash_trunc_size;
	return 0;
    }
    r = ct->hash->hash_size(&x);
    *out = x;
    return r;
}

krb5_boolean krb5_c_valid_cksumtype(krb5_cksumtype cksumtype)
{
    return !!find_cksumtype(cksumtype);
}

krb5_boolean krb5_c_is_keyed_cksum(krb5_cksumtype cksumtype)
{
    const struct krb5_cksumtypes *ct = find_cksumtype(cksumtype);
    if (!ct) return 0;
    return !!(ct->flags & KRB5_CKSUMFLAG_KEYED);
}

krb5_boolean krb5_c_is_coll_proof_cksum(krb5_cksumtype cksumtype)
{
    const struct krb5_cksumtypes *ct = find_cksumtype(cksumtype);
    if (!ct) return 0;
    return !(ct->flags & KRB5_CKSUMFLAG_NOT_COLL_PROOF);
}

krb5_error_code krb5_c_make_checksum(krb5_context context,
    krb5_cksumtype cksumtype,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    const krb5_data *input,
    krb5_checksum *cksum)
{
    int code;
    void *state;
    const struct krb5_cksumtypes *ct = find_cksumtype(cksumtype);

    if (!ct) return KRB5_BAD_ENCTYPE;
    if ((code = ct->hash->hash_size(&cksum->length)))
	return code;
    if (!(cksum->contents = afs_osi_Alloc(cksum->length)))
	return ENOMEM;
    cksum->checksum_type = cksumtype;
    if ((code = ct->hash->hash_init(&state))) goto Done;
    if (ct->hash->hash_setkey && (code = ct->hash->hash_setkey(state,
	context, key, usage, NULL))) goto Done;
    if ((code = ct->hash->hash_update(state, input->data,
	    input->length)))
	goto Done;
    code = ct->hash->hash_digest(state, cksum->contents);
#if defined(KERNEL)
    if (ct->hash_trunc_size) {
	void *x = 0;
	if (!(x = afs_osi_Alloc(ct->hash_trunc_size))) {
	    code = ENOMEM;
	    goto Done;
	}
	memcpy(x, cksum->contents, ct->hash_trunc_size);
	afs_osi_Free(cksum->contents, cksum->length);
	cksum->contents = x;
	cksum->length = ct->hash_trunc_size;
	x = 0;
    }
#else
    if (ct->hash_trunc_size) cksum->length = ct->hash_trunc_size;
#endif
Done:
    if (code) {
	afs_osi_Free(cksum->contents, cksum->length);
	memset(cksum, 0, sizeof *cksum);
    }
    ct->hash->free_state(state);
    return code;
}

krb5_error_code krb5_c_verify_checksum(krb5_context context,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    const krb5_data *input,
    krb5_checksum *cksum,
    krb5_boolean *valid)
{
    int code;
    void *state;
    krb5_checksum test[1];
    const struct krb5_cksumtypes *ct = find_cksumtype(cksum->checksum_type);

    memset(test, 0, sizeof *test);
    code = KRB5_BAD_ENCTYPE; if (!ct) goto Done;
    if ((code = ct->hash->hash_size(&test->length))) goto Done;
    code = KRB5_BAD_MSIZE; if (cksum->length !=
	    (ct->hash_trunc_size
		? ct->hash_trunc_size : test->length))
	{
	    goto Done;
	}
    if ((code = ct->hash->hash_init(&state))) goto Done;
    if (ct->hash->hash_setkey && (code = ct->hash->hash_setkey(state,
	context, key, usage, cksum->contents))) goto Done;
    if ((code = ct->hash->hash_update(state, input->data,
	    input->length)))
	goto Done;
    if (!(test->contents = afs_osi_Alloc(test->length)))
	code = ENOMEM;
    else code = ct->hash->hash_digest(state, test->contents);
Done:
    if (code || memcmp(test->contents, cksum->contents, cksum->length))
	*valid = 0;
    else *valid = 1;
    if (test->contents) afs_osi_Free(test->contents, test->length);
    ct->hash->free_state(state);
    return code;
}

krb5_error_code krb5_c_keyed_checksum_types(krb5_context context,
    krb5_enctype enctype,
    unsigned int *count, krb5_cksumtype **out)
{
    unsigned int i, c;
    krb5_cksumtype *result;
    const struct krb5_enctypes * et1, * et2;
    const struct krb5_cksumtypes *ct;

    et1 = find_enctype(enctype);
    for (result = 0;;) {
	c = 0;
	for (ct = krb5i_cksumtypes_list;
		ct<krb5i_cksumtypes_list+sizeof krb5i_cksumtypes_list
		    /sizeof *krb5i_cksumtypes_list;
		++ct) {
	    if (ct->flags & KRB5_CKSUMFLAG_DERIVE)
		;
	    else if (!(ct->flags & KRB5_CKSUMFLAG_KEYED))
		continue;
	    else if (!et1
		    || !(et2 = find_enctype(ct->keyed_etype))
		    || et1->enc != et2->enc) {
		continue;
	    }
	    if (!result) {
		++c;
		continue;
	    }
	    for (i = 0; ; ++i) {
		if (i >= c || result[i] > ct->cksumtype) {
		    if (i < c)
			memmove(result+i+1,
			    result+i,
			    (c-i)*sizeof *result);
		    result[i] = ct->cksumtype;
		    ++c;
		    break;
		}
		else if (result[i] == ct->cksumtype)
		    break;
	    }
	}
	if (!c || result) break;
	result = (krb5_cksumtype *) afs_osi_Alloc(c*sizeof *result);
	if (!result) return ENOMEM;
    }
    *count = c;
    *out = result;
    return 0;
}

void krb5_free_cksumtypes(krb5_context context, krb5_cksumtype *val)
{
    if (val) afs_osi_Free(val, sizeof *val);
}

/* the n-fold algorithm is described in RFC 3961 */

static int
compute_lcm(a, b)
{
    int p = a*b;

    while (b) {
	int c = b;
	b = a % b;
	a = c;
    }
    return p/a;
}

unsigned char *
krb5i_nfold(unsigned char *in, int inlen, unsigned char *out, int outlen)
{
    int lcm;
    int i;
    int w;
    int wi, sc;
    int accum;
    int msbit;
    int inbits;
    unsigned char *op;

    memset(out, 0, outlen);
    lcm = compute_lcm(inlen, outlen);
/* printf ("in=%d out=%d lcm=%d\n", inlen, outlen, lcm); */
    accum = 0;
    inbits = inlen<<3;
    op = (unsigned char*) (out + outlen);
    for (i = lcm; --i >= 0; ) {
	if (op == (unsigned char *) out)
	    op = (unsigned char*) (out + outlen);
	--op;
	msbit = (
	     ((inbits)-1)
	    +(((inbits)+13)*(i/inlen))
	     +((inlen-(i%inlen))<<3)
	     )%(inbits);
	wi = (((inlen)-((msbit>>3))) % inlen);
	sc = 1+(msbit&7);

	w = (unsigned char) in[wi];
	--wi; if (wi < 0) wi = inlen-1;
	w += in[wi]<<8;
	accum += (unsigned char) (w>>sc);
	accum += *op;
	*op = accum;
	accum >>= 8;
    }
    if (accum) {
	for (i = outlen; --i >= 0; ) {
	    accum += (unsigned char) (out[i]);
	    out[i] = accum;
	    accum >>= 8;
	    if (!accum) break;
	}
    }
    return out;
}

#if KRB5_DES_SUPPORT || KRB5_CAST_SUPPORT || KRB5_DES3_SUPPORT
int krb5i_x_cbc_encrypt(const EVP_CIPHER *cipher,
    const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    int code;
    unsigned char *t;
    int length;
    EVP_CIPHER_CTX ctx[1];

    if (key->length != EVP_CIPHER_key_length(cipher))
	return KRB5_BAD_KEYSIZE;
    if (input->length % EVP_CIPHER_block_size(cipher)) {
      return KRB5_BAD_MSIZE;
    }
    if (ivec && ivec->length != EVP_CIPHER_iv_length(cipher)) {
      return KRB5_BAD_MSIZE;
    }
    if (input->length != output->length) {
      return KRB5_BAD_MSIZE;
    }
    EVP_CIPHER_CTX_init(ctx);
    if ((code = !EVP_EncryptInit_ex(ctx, cipher, 0,
	key->contents, ivec ? ivec->data : 0)))
	goto Out;
    if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0)))
	goto Out;
    t = output->data;
    if ((code = !EVP_EncryptUpdate(ctx, t, &length,
	input->data, input->length)))
	goto Out;
    t += length;
    if ((code = !EVP_EncryptFinal_ex(ctx, t, &length)))
	goto Out;
    t += length;
    if (t != output->data + output->length)
	code = KRB5_CRYPTO_INTERNAL;
Out:
    EVP_CIPHER_CTX_cleanup(ctx);
    return krb5i_maperr(code);
}

int krb5i_x_cbc_decrypt(const EVP_CIPHER *cipher,
    const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    int code;
    unsigned char *t;
    int length;
    EVP_CIPHER_CTX ctx[1];

    if (key->length != EVP_CIPHER_key_length(cipher))
	return KRB5_BAD_KEYSIZE;
    if (input->length % EVP_CIPHER_block_size(cipher)) {
      return KRB5_BAD_MSIZE;
    }
    if (ivec && ivec->length != EVP_CIPHER_iv_length(cipher)) {
      return KRB5_BAD_MSIZE;
    }
    if (input->length != output->length) {
      return KRB5_BAD_MSIZE;
    }
    EVP_CIPHER_CTX_init(ctx);
    if ((code = !EVP_DecryptInit_ex(ctx, cipher, 0,
	key->contents, ivec ? ivec->data : 0)))
	goto Out;
    if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0)))
	goto Out;
    t = output->data;
    if ((code = !EVP_DecryptUpdate(ctx, t, &length,
	input->data, input->length)))
	goto Out;
    t += length;
    if ((code = !EVP_DecryptFinal_ex(ctx, t, &length)))
	goto Out;
    t += length;
    if (t != output->data + output->length)
	code = KRB5_CRYPTO_INTERNAL;
Out:
    EVP_CIPHER_CTX_cleanup(ctx);
    return krb5i_maperr(code);
}
#endif	/* KRB5_DES_SUPPORT || KRB5_CAST_SUPPORT || KRB5_DES3_SUPPORT */

#if KRB5_AES_SUPPORT || KRB5_RC6_SUPPORT
int krb5i_x_cts_encrypt(const EVP_CIPHER *cipher,
    const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output, int cbc_compat)
{
    unsigned char *o, *work, *t;
    unsigned int bs, c, lbs, worksize;
    int uc, tc;
    EVP_CIPHER_CTX ctx[1];
    int code;

    bs = EVP_CIPHER_block_size(cipher);
    if (input->length < bs) {
	return KRB5_BAD_MSIZE;
    }
    if (ivec && ivec->length != EVP_CIPHER_iv_length(cipher)) {
      return KRB5_BAD_MSIZE;
    }
    EVP_CIPHER_CTX_init(ctx);
    work = afs_osi_Alloc(worksize = 6*bs);
    code = ENOMEM; if (!work) goto Out;

    lbs = -((-input->length) | -bs);

    o = output->data;
    if ((code = !EVP_EncryptInit_ex(ctx, cipher, 0, key->contents,
	ivec ? ivec->data : 0))) goto Out;
    if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0))) goto Out;
    if ((code = !EVP_EncryptUpdate(ctx, o, &uc,
	input->data, input->length))) goto Out;
    o += uc;
    if (input->length-uc > bs) {
	code = KRB5_CRYPTO_INTERNAL;
	goto Out;
    }
    t = work+bs;
    tc = (uc > 2*bs) ? 2*bs : uc;
    if (tc) {
	memcpy(t, o-tc, tc);
	o -= tc;
	t += tc;
    }
    if (input->length & (bs-1)) {
	memset(work, 0, bs);
	if ((code = !EVP_EncryptUpdate(ctx, t, &tc, work,
	    bs-lbs))) goto Out;
	t += tc;
    }
    if ((code = !EVP_EncryptFinal_ex(ctx, t, &tc))) goto Out;
    t += tc;
    c = t-(work+bs);
    output->length = lbs + c + o - output->data - bs;
    if (c >= 2*bs && (!cbc_compat || (input->length & (bs-1)))) {
	c -= 2*bs;
	memcpy(o+c+bs,
	    t-(2*bs), lbs);
	memcpy(o+c, t-bs, bs);
    }
    if (c)
	memcpy(o, work+bs, c);
Out:
    EVP_CIPHER_CTX_cleanup(ctx);
    if (work) {
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    }
    return krb5i_maperr(code);
}

int krb5i_x_cts_decrypt(const EVP_CIPHER *cipher,
    const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output, int cbc_compat)
{
    unsigned char *o, *work, *t;
    unsigned int lbs, bs, used, worksize;
    int c, tc;
    EVP_CIPHER_CTX ctx[1];
    int code;

    bs = EVP_CIPHER_block_size(cipher);
    if (input->length < bs) {
	return KRB5_BAD_MSIZE;
    }
    if (ivec && ivec->length != bs) {
	return KRB5_BAD_MSIZE;
    }
    EVP_CIPHER_CTX_init(ctx);
    work = afs_osi_Alloc(worksize = 6*bs);
    code = ENOMEM; if (!work) goto Out;
    lbs = -((-input->length) | -bs);
    if (ivec) memcpy(work, ivec->data, bs);
    else memset(work, 0, bs);

    o = output->data;
    if ((code = !EVP_DecryptInit_ex(ctx, cipher, 0, key->contents,
	work))) goto Out;
    if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0))) goto Out;
    t = (work+bs);
    if (cbc_compat && !(input->length & (bs-1))) {
	if ((code = !EVP_DecryptUpdate(ctx, o, &c, input->data ,
	    input->length))) goto Out;
	goto Skip;
    }
    if (input->length > 2*bs) {
	tc = input->length-(bs+lbs);
	if ((code = !EVP_DecryptUpdate(ctx, o, &c, input->data ,
	    tc))) goto Out;
	o += c;
	if (tc-c > bs) {
	    code = KRB5_CRYPTO_INTERNAL;
	    goto Out;
	}
    }
    if ((code = !EVP_DecryptUpdate(ctx, t, &tc, input->data +input->length-lbs,
	lbs))) goto Out;
    t += tc;
    if (input->length & (bs-1)) {
	memset(work, 0, bs);
	if ((code = !EVP_DecryptUpdate(ctx, t, &tc, work, bs-lbs))) goto Out;
	t += tc;
    }
    if (input->length > bs) {
	if ((code = !EVP_DecryptUpdate(ctx, t, &tc,
	    input->data +input->length-(bs+lbs), bs))) goto Out;
	t += tc;
    }
Skip:
    if ((code = !EVP_DecryptFinal_ex(ctx, t, &tc))) goto Out;
    t += tc;
    if (input->length & (bs-1)) {
	used = t-(work+bs);
	memcpy(work,
	    (input->length > 2*bs)
		? input->data +input->length-(2*bs+lbs)
		: work,
	    bs);
	if ((code = !EVP_DecryptInit_ex(ctx, cipher, 0,
	    key->contents, work))) goto Out;
	if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0))) goto Out;
	if ((code = !EVP_DecryptUpdate(ctx, t, &tc,
		input->data +input->length-lbs, lbs)))
	    goto Out;
	t += tc;
	if ((code = !EVP_DecryptUpdate(ctx, t, &tc,
	    work+used+lbs,
	    (bs-lbs)))) goto Out;
	t += tc;
	if ((code = !EVP_DecryptFinal_ex(ctx, t, &tc))) goto Out;
	t += tc;
	memcpy(t-(3*bs),
	    t-bs,
	    bs);
	t -= bs;
    }
    c = t-(work+bs);
    memcpy(o, (work+bs), c);
    output->length = input->length;
Out:
    EVP_CIPHER_CTX_cleanup(ctx);
    if (work) {
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    }
    return krb5i_maperr(code);
}
#endif	/* KRB5_AES_SUPPORT || KRB5_RC6_SUPPORT */

#if KRB5_DES_SUPPORT || KRB5_DES3_SUPPORT
const static unsigned char weak_keys[][8]={
	/* weak keys */
	{ 1,1,1,1,1,1,1,1 },
	{ 254,254,254,254,254,254,254,254 },
	{ 31,31,31,31,14,14,14,14 },
	{ 224,224,224,224,241,241,241,241 },
	/* semi-weak keys */
	{ 1,254,1,254,1,254,1,254 },
	{ 254,1,254,1,254,1,254,1 },
	{ 31,224,31,224,14,241,14,241 },
	{ 224,31,224,31,241,14,241,14 },
	{ 1,224,1,224,1,241,1,241 },
	{ 224,1,224,1,241,1,241,1 },
	{ 31,254,31,254,14,254,14,254 },
	{ 254,31,254,31,254,14,254,14 },
	{ 1,31,1,31,1,14,1,14 },
	{ 31,1,31,1,14,1,14,1 },
	{ 224,254,224,254,241,254,241,254 },
	{ 254,224,254,224,254,241,254,241 },
};
#endif	/* KRB5_DES_SUPPORT || KRB5_DES3_SUPPORT */

#if KRB5_DES_SUPPORT
void krb5i_des_cbc_blocksize(unsigned int *minbs, unsigned int *bs)
{
    *minbs = *bs = 8;
}

void krb5i_des_cbc_keysize(unsigned int *bytes,
    unsigned int *length)
{
    *bytes = 7;
    *length = 8;
}

int krb5i_des_cbc_makekey(const krb5_data *data,
    krb5_keyblock *key, int f)
{
    int i;

    if (key->length != 8) return KRB5_CRYPTO_INTERNAL;
    if (data->length != 7) return KRB5_CRYPTO_INTERNAL;
    memcpy(key->contents, data->data, 7);
    key->contents[7] = 0;
    for (i = 0; i < 7; ++i)
	key->contents[7] ^= ((1&key->contents[i]) << (i+1));
    i = 8;
#define maskbits(n) ((1<<n)-1)
#define hibits(x,n)	(((x)>>n)&maskbits(n))
#define lobits(x,n) ((x)&maskbits(n))
#define xorbits(x,n)	(hibits(x,n)^lobits(x,n))
#define oddparity(x)	xorbits(xorbits(xorbits(x,4),2),1)
    while (i--) /* force odd parity */
	key->contents[i] ^= !oddparity(key->contents[i]);
#undef oddparity
#undef xorbits
#undef lobits
#undef hibits
#undef maskbits
    for (i = 0; i < sizeof(weak_keys)/sizeof(*weak_keys);++i)
	if (!memcmp(key->contents, weak_keys[i], 8)) {
	    if (f) return KRB5DES_WEAK_KEY;
	    key->contents[7] ^= 0xf0;
	    break;
	}
    return 0;
}

int krb5i_des_cbc_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cbc_encrypt(EVP_des_cbc(),
	key,ivec,input,output);
}

int krb5i_des_cbc_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cbc_decrypt(EVP_des_cbc(),
	key,ivec,input,output);
}

const struct krb5_enc_provider krb5i_des_cbc[1] = {{
    krb5i_des_cbc_blocksize,
    krb5i_des_cbc_keysize,
    krb5i_des_cbc_makekey,
    krb5i_des_cbc_encrypt,
    krb5i_des_cbc_decrypt,
}};
#endif	/* KRB5_DES_SUPPORT */

#if KRB5_DES3_SUPPORT
void krb5i_des3_cbc_blocksize(unsigned int *minbs, unsigned int *bs)
{
    *minbs = *bs = 8;
}

void krb5i_des3_cbc_keysize(unsigned int *bytes,
    unsigned int *length)
{
    *bytes = 21;
    *length = 24;
}

int krb5i_des3_cbc_makekey(const krb5_data *data,
    krb5_keyblock *key, int f)
{
    int i, j;
    unsigned char *cp, *dp;
    int code;

    if (key->length != 24) return KRB5_CRYPTO_INTERNAL;
    if (data->length != 21) return KRB5_CRYPTO_INTERNAL;
    cp = key->contents;
    dp = data->data;
    code = 0;
    for (j = 0; j < 3; ++j) {
	memcpy(cp, dp, 7);
	cp[7] = 0;
	for (i = 0; i < 7; ++i)
	    cp[7] ^= ((1&cp[i]) << (i+1));
	i = 8;
#define maskbits(n) ((1<<n)-1)
#define hibits(x,n)	(((x)>>n)&maskbits(n))
#define lobits(x,n) ((x)&maskbits(n))
#define xorbits(x,n)	(hibits(x,n)^lobits(x,n))
#define oddparity(x)	xorbits(xorbits(xorbits(x,4),2),1)
	while (i--) /* force odd parity */
	    cp[i] ^= !oddparity(cp[i]);
#undef oddparity
#undef xorbits
#undef lobits
#undef hibits
#undef maskbits
	for (i = 0; i < sizeof(weak_keys)/sizeof(*weak_keys);++i)
	    if (!memcmp(cp, weak_keys[i], 8)) {
		if (f) code = KRB5DES_WEAK_KEY;
		else cp[7] ^= 0xf0;
		break;
	    }
	cp += 8;
	dp += 7;
    }
    return code;
}

int krb5i_des3_cbc_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cbc_encrypt(EVP_des_ede3_cbc(),
	key,ivec,input,output);
}

int krb5i_des3_cbc_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cbc_decrypt(EVP_des_ede3_cbc(),
	key,ivec,input,output);
}

const struct krb5_enc_provider krb5i_des3_cbc[1] = {{
    krb5i_des3_cbc_blocksize,
    krb5i_des3_cbc_keysize,
    krb5i_des3_cbc_makekey,
    krb5i_des3_cbc_encrypt,
    krb5i_des3_cbc_decrypt,
}};
#endif	/* KRB5_DES3_SUPPORT */

#if KRB5_CAST_SUPPORT
void krb5i_cast_cbc_blocksize(unsigned int *minbs, unsigned int *bs)
{
    *minbs = *bs = 8;
}

void krb5i_cast_cbc_keysize(unsigned int *bytes,
    unsigned int *length)
{
    *bytes = 16;
    *length = 16;
}

int krb5i_cast_cbc_makekey(const krb5_data *data,
    krb5_keyblock *key, int f)
{
    if (key->length != 16) return KRB5_CRYPTO_INTERNAL;
    if (data->length != 16) return KRB5_CRYPTO_INTERNAL;
    memcpy(key->contents, data->data, 16);
    return 0;
}

int krb5i_cast_cbc_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cbc_encrypt(EVP_cast5_cbc(),
	key,ivec,input,output);
}

int krb5i_cast_cbc_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cbc_decrypt(EVP_cast5_cbc(),
	key,ivec,input,output);
}

const struct krb5_enc_provider krb5i_cast_cbc[1] = {{
    krb5i_cast_cbc_blocksize,
    krb5i_cast_cbc_keysize,
    krb5i_cast_cbc_makekey,
    krb5i_cast_cbc_encrypt,
    krb5i_cast_cbc_decrypt,
}};
#endif	/* KRB5_CAST_SUPPORT */

#if KRB5_AES_SUPPORT
void krb5i_aes_cts_blocksize(unsigned int *minbs, unsigned int *bs)
{
    *minbs = 16;
    *bs = 1;
}

void krb5i_aes128_cts_keysize(unsigned int *bytes,
    unsigned int *length)
{
    *length = *bytes = 16;
}

int krb5i_aes128_cts_makekey(const krb5_data *data,
    krb5_keyblock *key, int f)
{
    if (key->length != 16) return KRB5_CRYPTO_INTERNAL;
    if (data->length != 16) return KRB5_CRYPTO_INTERNAL;
    memcpy(key->contents, data->data, 16);
    return 0;
}

int krb5i_aes128_cts_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cts_encrypt(EVP_aes_128_cbc(),
	key,ivec,input,output, 0);
}

int krb5i_aes128_cts_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cts_decrypt(EVP_aes_128_cbc(),
	key,ivec,input,output, 0);
}

const struct krb5_enc_provider krb5i_aes128_cts[1] = {{
    krb5i_aes_cts_blocksize,
    krb5i_aes128_cts_keysize,
    krb5i_aes128_cts_makekey,
    krb5i_aes128_cts_encrypt,
    krb5i_aes128_cts_decrypt,
}};

void krb5i_aes256_cts_keysize(unsigned int *bytes,
    unsigned int *length)
{
    *length = *bytes = 32;
}

int krb5i_aes256_cts_makekey(const krb5_data *data,
    krb5_keyblock *key, int f)
{
    if (key->length != 32) return KRB5_CRYPTO_INTERNAL;
    if (data->length != 32) return KRB5_CRYPTO_INTERNAL;
    memcpy(key->contents, data->data, 32);
    return 0;
}

int krb5i_aes256_cts_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cts_encrypt(EVP_aes_256_cbc(),
	key,ivec,input,output, 0);
}

int krb5i_aes256_cts_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cts_decrypt(EVP_aes_256_cbc(),
	key,ivec,input,output,0);
}

const struct krb5_enc_provider krb5i_aes256_cts[1] = {{
    krb5i_aes_cts_blocksize,
    krb5i_aes256_cts_keysize,
    krb5i_aes256_cts_makekey,
    krb5i_aes256_cts_encrypt,
    krb5i_aes256_cts_decrypt,
}};
#endif	/* KRB5_AES_SUPPORT */

#if KRB5_RC6_SUPPORT
int krb5i_rc6_cts_encrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cts_encrypt(EVP_rc6_128_cbc(),
	key,ivec,input,output,1);
}

int krb5i_rc6_cts_decrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    return krb5i_x_cts_decrypt(EVP_rc6_128_cbc(),
	key,ivec,input,output,1);
}

const struct krb5_enc_provider krb5i_rc6_cts[1] = {{
    krb5i_aes_cts_blocksize,
    krb5i_aes128_cts_keysize,
    krb5i_aes128_cts_makekey,
    krb5i_rc6_cts_encrypt,
    krb5i_rc6_cts_decrypt,
}};
#endif	/* KRB5_RC6_SUPPORT */

#if KRB5_RC4_SUPPORT
void krb5i_arcfour_blocksize(unsigned int *minbs, unsigned int *bs)
{
    *minbs = *bs = 1;
}

void krb5i_arcfour_keysize(unsigned int *bytes,
    unsigned int *length)
{
    *bytes = 16;
    *length = 16;
}

int krb5i_arcfour_makekey(const krb5_data *data,
    krb5_keyblock *key, int f)
{
    if (key->length != 16) return KRB5_CRYPTO_INTERNAL;
    if (data->length != 16) return KRB5_CRYPTO_INTERNAL;
    memcpy(key->contents, data->data, 16);
    return 0;
}

int krb5i_arcfour_docrypt(const krb5_keyblock *key, const krb5_data *ivec,
    const krb5_data *input, krb5_data *output)
{
    EVP_CIPHER_CTX ctx[1];
    const EVP_CIPHER *cipher = EVP_rc4();
    int code;
    unsigned char *t;
    int length;

    if (key->length != 16)
	return KRB5_BAD_KEYSIZE;
    if (ivec) {
	return KRB5_BAD_MSIZE;	/* XXX */
    }
    if (input->length != output->length) {
	return KRB5_BAD_MSIZE;
    }
    EVP_CIPHER_CTX_init(ctx);
    if ((code = !EVP_EncryptInit_ex(ctx, cipher, 0, key->contents, 0)))
	goto Out;
#if 0
    if ((code = !EVP_CIPHER_CTX_set_padding(ctx, 0)))
	goto Out;
#endif
    t = output->data;
    if ((code = !EVP_EncryptUpdate(ctx, t, &length,
	input->data, input->length)))
	goto Out;
    t += length;
    if ((code = !EVP_EncryptFinal_ex(ctx, t, &length)))
	goto Out;
    t += length;
    if (t != output->data + output->length)
	code = KRB5_CRYPTO_INTERNAL;
Out:
    EVP_CIPHER_CTX_cleanup(ctx);
    return krb5i_maperr(code);
}

const struct krb5_enc_provider krb5i_arcfour[1] = {{
    krb5i_arcfour_blocksize,
    krb5i_arcfour_keysize,
    krb5i_arcfour_makekey,
    krb5i_arcfour_docrypt,
    krb5i_arcfour_docrypt,
}};
#endif	/* KRB5_RC4_SUPPORT */

#if KRB5_DES_SUPPORT || KRB5_CAST_SUPPORT || KRB5_DES3_SUPPORT
int
krb5i_old_encrypt_len(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hash_trunc_size,
    size_t inlen, size_t *outlen)
{
    unsigned int mbs, bs, hs, frag;
    size_t out;

    enc->block_size(&mbs, &bs);
    hash->hash_size(&hs);
    out = mbs + hs + inlen;
    if ((frag = out % bs))
	out += bs - frag;
    if (out < mbs) out = mbs;
    *outlen = out;
    return 0;
}

int
krb5i_old_encrypt(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hash_trunc_size,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_data * output)
{
    unsigned int mbs, bs, hs, frag;
    size_t out;
    int code;
    krb5_data fakeivec[1], temp[1];
    void *state;
    unsigned char *work;

    state = 0;
    enc->block_size(&mbs, &bs);
    hash->hash_size(&hs);
    out = mbs + hs + input->length;
    if ((frag = out % bs))
	out += bs - frag;
    if (out < mbs) out = mbs;
    if (output->length < out) {
      return KRB5_BAD_MSIZE;
    }
    if (!(work = afs_osi_Alloc(out)))
	return ENOMEM;
    if (frag)
	memset(work + out+frag-bs, 0, bs-frag);
    memset(work + mbs, 0, hs);
    temp->data = work;
    temp->length = mbs;
    if ((code = krb5_c_random_make_octets(0, temp)))
	goto Out;
    memcpy(work + mbs + hs, input->data, input->length);
    temp->length = out;

    if ((code = hash->hash_init(&state))) goto Out;
    if ((code = hash->hash_update(state, work, out)))
	goto Out;
    if ((code = hash->hash_digest(state, work + mbs)))
	goto Out;
    if (key->enctype == ENCTYPE_DES_CBC_CRC && !ivec) {
	fakeivec->length = key->length;
	fakeivec->data = key->contents;
	ivec = fakeivec;
    }
    output->length = out;
    code = enc->encrypt(key,ivec,temp,output);
Out:
    if (work) {
	memset(work, 0, out);
	afs_osi_Free(work, out);
    }
    if (state) hash->free_state(state);
    return code;
}

int
krb5i_old_decrypt(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hash_trunc_size,
    const krb5_keyblock * key,
    krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_data * output)
{
    unsigned int mbs, bs, hs;
    size_t ptlen;
    int code;
    krb5_data fakeivec[1], temp[1];
    void *state;
    unsigned char *work;
    unsigned int worksize;

    state = 0;
    work = 0;
    enc->block_size(&mbs, &bs);
    hash->hash_size(&hs);
    worksize = input->length + hs;
    ptlen = input->length - mbs - hs;
    if (input->length < mbs || input->length % bs) {
      return KRB5_BAD_MSIZE;
    }
    if (output->length >= worksize)
	;
    else if (output->length < ptlen) {
      return KRB5_BAD_MSIZE;
    }
    else if (!(work = afs_osi_Alloc(worksize)))
	return ENOMEM;

    temp->length = input->length;
    temp->data = work ? work : output->data;

    if (key->enctype == ENCTYPE_DES_CBC_CRC && !ivec) {
	fakeivec->length = key->length;
	fakeivec->data = key->contents;
	ivec = fakeivec;
    }
    
    code = enc->decrypt(key,ivec,input,temp);
    memcpy(temp->data + temp->length, temp->data + mbs, hs);
    memset(temp->data + mbs, 0, hs);
    if ((code = hash->hash_init(&state))) goto Out;
    if ((code = hash->hash_update(state, temp->data, temp->length)))
	goto Out;
    if ((code = hash->hash_digest(state, temp->data)))
	goto Out;
    code = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    if (memcmp(temp->data, temp->data + temp->length, hs))
	goto Out;
    if (work)
	memcpy(output->data, temp->data + mbs + hs, ptlen);
    else
	memmove(output->data, temp->data + mbs + hs, ptlen);
    output->length = ptlen;
    code = 0;
Out:
    if (state) hash->free_state(state);
    if (work) {
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    } else {
	memset(output->data + ptlen, 0, worksize-ptlen);
    }
    return code;
}
#endif	/* KRB5_DES_SUPPORT || KRB5_CAST_SUPPORT || KRB5_DES3_SUPPORT */

#if KRB5_DES_SUPPORT || KRB5_AES_SUPPORT || KRB5_DES3_SUPPORT
int
krb5i_dk_encrypt_len(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hts,
    size_t inlen, size_t *outlen)
{
    unsigned int mbs, bs, hs, frag;
    size_t out;

    enc->block_size(&mbs, &bs);
    if (!(hs = hts))
	hash->hash_size(&hs);
    out = mbs + inlen;
    if ((frag = out % bs))
	out += bs - frag;
    if (hs + out < mbs)
	out = mbs - hs;
    *outlen = hs + out;
    return 0;
}

int
krb5i_dk_encrypt(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hts,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_data * output)
{
    unsigned int mbs, bs, hs, frag, worksize;
    unsigned int out, keybytes, keylen;
    int code;
    krb5_data datain[1], dataout[1];
    unsigned char cd[5];
    krb5_keyblock enckey[1], checkkey[1];
    unsigned char *work;

    enc->block_size(&mbs, &bs);
    hash->hash_size(&hs);
    if (!hts) hts = hs;
    enc->key_size(&keybytes, &keylen);
    out = mbs + input->length;
    if ((frag = out % bs))
	out += bs - frag;
    out += hs;
    if (out < mbs) out = mbs;
    worksize = out + 2*keylen;

    if (output->length >= worksize)
	work = output->data;
    else if (output->length < out + (hts-hs)) {
#if 0
printf ("krb5i_dk_encrypt: output would be %d not %d bytes\n",
out, output->length);
printf (" input=%d hts=%d mbs=%d\n", input->length, hts, mbs);
#endif
      return KRB5_BAD_MSIZE;
    }
    else if (!(work = afs_osi_Alloc(worksize)))
	return ENOMEM;
    enckey->length = checkkey->length = keylen;
    enckey->contents = (checkkey->contents = work + out) + keylen;
    enckey->enctype = checkkey->enctype = key->enctype;
    cd[0] = usage>>24; cd[1] = usage>>16; cd[2] = usage>>8; cd[3] = usage;
    cd[4] = 0x55;
    datain->data = cd; datain->length = 5;
    if ((code = krb5i_derive_key(enc, key, checkkey, datain)))
	goto Out;
    cd[4] = 0xaa;
    if ((code = krb5i_derive_key(enc, key, enckey, datain)))
	goto Out;
    dataout->data = work;
    dataout->length = mbs;
    if ((code = krb5_c_random_make_octets(0, dataout))) goto Out;
    memcpy(work+mbs, input->data, input->length);
    memset(work+mbs+input->length, 0, out-(hs+mbs+input->length));
    datain->length = out-hs;
    datain->data = work;
    dataout->length = hs;
    dataout->data = work + out - hs;
    if ((code = krb5i_hmac(hash, checkkey, 1, datain, dataout)))
	goto Out;
    output->length = out-hs;
    if ((code = enc->encrypt(enckey, ivec, datain, output))) goto Out;
    output->length = out + (hts-hs);
Out:
    if (code) output->length = 0;
    if (!work)
	;
    else if (work != output->data) {
	memcpy(output->data + out - hs, dataout->data, hts);
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    } else {
	memset(output->data + output->length, 0, worksize-output->length);
    }
    return code;
}

int
krb5i_dk_decrypt(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hts,
    const krb5_keyblock * key,
    krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_data * output)
{
    unsigned int mbs, bs, hs, worksize;
    unsigned int ptlen, keybytes, keylen;
    int code;
    krb5_data datain[1], dataout[1];
    unsigned char cd[5];
    krb5_keyblock enckey[1], checkkey[1];
    unsigned char *work;

    enc->block_size(&mbs, &bs);
    hash->hash_size(&hs);
    if (!hts) hts = hs;
    enc->key_size(&keybytes, &keylen);

    ptlen = input->length - hts - mbs;
    worksize = input->length + hs + 2*keylen;

    if (input->length < mbs) {
      return KRB5_BAD_MSIZE;
    }
    if (output->length >= worksize)
	work = output->data;
    else if (output->length < ptlen) {
#if 0
      printf ("krb5i_dk_decrypt: output would be %d not %d bytes\n",
	      ptlen, output->length);
      printf (" input=%d hts=%d mbs=%d\n", input->length, hts, mbs);
#endif
      return KRB5_BAD_MSIZE;
    }
    else if (!(work = afs_osi_Alloc(worksize)))
	return ENOMEM;
    enckey->length = checkkey->length = keylen;
    enckey->contents = (checkkey->contents = work + worksize-keylen) - keylen;
    enckey->enctype = checkkey->enctype = key->enctype;
    cd[0] = usage>>24; cd[1] = usage>>16; cd[2] = usage>>8; cd[3] = usage;
    cd[4] = 0x55;
    datain->data = cd; datain->length = 5;
    if ((code = krb5i_derive_key(enc, key, checkkey, datain)))
	goto Out;
    cd[4] = 0xaa;
    if ((code = krb5i_derive_key(enc, key, enckey, datain)))
	goto Out;
    datain->length = input->length - hts;
    datain->data = input->data;
    dataout->length = input->length - hts;
    dataout->data = work;
    if ((code = enc->decrypt(enckey, ivec, datain, dataout))) goto Out;
    datain->data = enckey->contents - hs;
    datain->length = hs;
    if ((code = krb5i_hmac(hash, checkkey, 1, dataout, datain)))
	goto Out;
    code = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    if (memcmp(enckey->contents-hs, input->data+input->length-hts, hts))
    goto Out;
    if (work == output->data)
	memmove(work, work+mbs, ptlen);
    else
	memcpy(output->data, work+mbs, ptlen);
    output->length = ptlen;
    code = 0;
Out:
    if (code) output->length = 0;
    if (!work)
	;
    else if (work != output->data) {
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    } else {
	memset(output->data + output->length, 0, worksize-output->length);
    }
    return code;
}
#endif	/* KRB5_DES_SUPPORT || KRB5_AES_SUPPORT || KRB5_DES3_SUPPORT */

#if KRB5_RC4_SUPPORT
int
krb5i_arcfour_encrypt_len(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hash_trunc_size,
    size_t inlen, size_t *outlen)
{
    unsigned int hs;

    hash->hash_size(&hs);
    *outlen = hs + 8 + inlen;

    return 0;
}

const char krb5i_arcfour_l40[] = "fortybits";

int
krb5i_arcfour_encrypt(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hash_trunc_size,
    const krb5_keyblock *key,
    krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_data * output)
{
    unsigned int mbs, bs, hs, worksize;
    unsigned int enclen, keybytes, keylen;
    int code;
    krb5_data datain[1], dataout[1];
    unsigned char salt[14];
    krb5_keyblock tempkey[1];
    unsigned char *work;

    enc->block_size(&mbs, &bs);
    if (bs != 1) return KRB5_CRYPTO_INTERNAL;
    hash->hash_size(&hs);
    if (mbs > hs) return KRB5_CRYPTO_INTERNAL;
    enc->key_size(&keybytes, &keylen);
    enclen = 8 + input->length;
    worksize = enclen + keylen;

    if (output->length < enclen + hs) {
      return KRB5_BAD_MSIZE;
    }
    else if (!(work = afs_osi_Alloc(worksize)))
	return ENOMEM;
    tempkey->length = keybytes;
    tempkey->contents = work + enclen;
    tempkey->enctype = key->enctype;
    usage = krb5i_ms_usage(usage);
    datain->data = salt;
#if KRB5_RC4_SUPPORT > 1
    if (key->enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
/* if (export){*((DWORD *)(L40+10)) = T;HMAC(K,L40,10+4,K1);}*/
	memcpy(salt, krb5i_arcfour_l40, sizeof(krb5i_arcfour_l40));
	salt[13] = usage>>24; salt[12] = usage>>16;
	salt[11] = usage>>8;  salt[10] = usage;
	datain->length = 14;
    } else {
#endif
/* else HMAC(K,&T,4,K1); */
	salt[3] = usage>>24; salt[2] = usage>>16;
	salt[1] = usage>>8;  salt[0] = usage;
	datain->length = 4;
#if KRB5_RC4_SUPPORT > 1
    }
#endif
    dataout->length = tempkey->length;
    dataout->data = tempkey->contents;
    if ((code = krb5i_hmac(hash, key, 1, datain, dataout))) goto Out;
/* memcpy(K2, K1, 16); */
/* nonce (edata.Confounder, 8); */
    datain->data = work;
    datain->length = 8;
    if ((code = krb5_c_random_make_octets(0, datain))) goto Out;
/* memcpy (edata.Data, data); */
    memcpy(work+8, input->data, input->length);
/* edata.Checksum = HMAC (K2, edata); */
    datain->length += input->length;
    dataout->data = output->data;
    dataout->length = hs;
    if ((code = krb5i_hmac(hash, tempkey, 1, datain, dataout))) goto Out;
#if KRB5_RC4_SUPPORT > 1
/* if (export) memset (K1+7, 0xAB, 9); */
    if (key->enctype == ENCTYPE_ARCFOUR_HMAC_EXP)
	memset(tempkey->contents+7, 0xab, 9);
#endif
/* K3 = HMAC (K1, edata.Checksum); */
    datain->data = output->data;
    datain->length = hs;
    dataout->data = tempkey->contents;
    dataout->length = tempkey->length;
    if ((code = krb5i_hmac(hash, tempkey, 1, datain, dataout))) goto Out;
/* RC4 (K3, edata.Confounder); */
/* RC4 (K3, data.Data); */
    dataout->length = datain->length = enclen;
    datain->data = work;
    dataout->data = output->data + hs;
    if ((code = enc->encrypt(tempkey, ivec, datain, dataout))) goto Out;
    output->length = hs + enclen;
Out:
    if (code) output->length = 0;
    if (work) {
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    }
    return code;
}

int
krb5i_arcfour_decrypt(const struct krb5_enc_provider * enc,
    const struct krb5_hash_provider * hash,
    int hash_trunc_size,
    const krb5_keyblock * key,
    krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_data * output)
{
    unsigned int mbs, bs, hs, worksize;
    unsigned int ptlen, keybytes, keylen;
    int code;
    krb5_data datain[1], dataout[1];
    unsigned char salt[14];
    krb5_keyblock k1[1], k2[1], k3[1];
    unsigned char *work;

    enc->block_size(&mbs, &bs);
    hash->hash_size(&hs);
    enc->key_size(&keybytes, &keylen);

    ptlen = input->length - hs - 8;
    worksize = input->length + 3*keybytes;

    if (ptlen > input->length) {
      return KRB5_BAD_MSIZE;
    }
    if (output->length < ptlen) {
      return KRB5_BAD_MSIZE;
    }
    else if (!(work = afs_osi_Alloc(worksize)))
	return ENOMEM;
    k3->length = k2->length = k1->length = keybytes;
    k3->contents =
	(k2->contents =
	    (k1->contents = work + input->length)
	    + keybytes)
	+ keybytes;
    k3->enctype = k2->enctype = k1->enctype = key->enctype;

    usage = krb5i_ms_usage(usage);
    datain->data = salt;
#if KRB5_RC4_SUPPORT > 1
    if (key->enctype == ENCTYPE_ARCFOUR_HMAC_EXP) {
/* if (export){*((DWORD *)(L40+10)) = T;HMAC(K,L40,10+4,K1);}*/
	memcpy(salt, krb5i_arcfour_l40, sizeof(krb5i_arcfour_l40));
	salt[13] = usage>>24; salt[12] = usage>>16;
	salt[11] = usage>>8;  salt[10] = usage;
	datain->length = 14;
    } else {
#endif
/* else HMAC(K,&T,4,K1); */
	salt[3] = usage>>24; salt[2] = usage>>16;
	salt[1] = usage>>8;  salt[0] = usage;
	datain->length = 4;
#if KRB5_RC4_SUPPORT > 1
    }
#endif
    dataout->length = k1->length;
    dataout->data = k1->contents;
    if ((code = krb5i_hmac(hash, key, 1, datain, dataout))) goto Out;

/* memcpy(K2,K1,16); */
    memcpy(k2->contents, k1->contents, k1->length);
#if KRB5_RC4_SUPPORT > 1
/* if (export) memset(K1+7,0xAB,9); */
    if (key->enctype == ENCTYPE_ARCFOUR_HMAC_EXP)
	memset(k1->contents+7, 0xab, 9);
#endif
/* K3 = HMAC(K1, edata.Checksum); */
    datain->data = input->data;
    datain->length = hs;
    dataout->data = k3->contents;
    dataout->length = k3->length;
    if ((code = krb5i_hmac(hash, k1, 1, datain, dataout)))
	goto Out;

/* RC4(K3,edata.ConFounder); */
/* RC4(K3,edata.Data); */
    datain->length = input->length - hs;
    datain->data = input->data+hs;
    dataout->length = input->length - hs;
    dataout->data = work+hs;
    if ((code = enc->decrypt(k3, ivec, datain, dataout))) goto Out;
/* checksum = HMAC(K2,concat(edata.Confounder,edata.Data)); */
    datain->data = work;
    datain->length = hs;
    if ((code = krb5i_hmac(hash, k2, 1, dataout, datain)))
	goto Out;
/* if (checksum != edata.Checksum) error; */
    code = KRB5KRB_AP_ERR_BAD_INTEGRITY;
    if (memcmp(work, input->data, hs))
	goto Out;
    memcpy(output->data, work+hs+8, ptlen);
    output->length = ptlen;
    code = 0;
Out:
    if (code) output->length = 0;
    if (work) {
	memset(work, 0, worksize);
	afs_osi_Free(work, worksize);
    }
    return code;
}
#endif	/* KRB5_RC4_SUPPORT */

const struct krb5_enctypes krb5i_enctypes_list[] = {
#if KRB5_DES_SUPPORT
    {ENCTYPE_DES_CBC_CRC,krb5i_des_cbc,krb5i_hash_crc32,0,
    krb5i_old_encrypt_len,krb5i_old_encrypt,krb5i_old_decrypt,
    {"des-cbc-crc", NULL}},
    {ENCTYPE_DES_CBC_MD4,krb5i_des_cbc,krb5i_hash_md4,0,
    krb5i_old_encrypt_len,krb5i_old_encrypt,krb5i_old_decrypt,
    {"des-cbc-md4",NULL}},
    {ENCTYPE_DES_CBC_MD5,krb5i_des_cbc,krb5i_hash_md5,0,
    krb5i_old_encrypt_len,krb5i_old_encrypt,krb5i_old_decrypt,
    {"des-cbc-md5","des",NULL}},
    {ENCTYPE_DES_HMAC_SHA1,krb5i_des_cbc,krb5i_hash_sha1,0,
    krb5i_dk_encrypt_len,krb5i_dk_encrypt,krb5i_dk_decrypt,
    {"des-hmac-sha1",NULL}},
#endif	/* KRB5_DES_SUPPORT */
#if KRB5_DES3_SUPPORT
    {ENCTYPE_DES3_CBC_SHA1,krb5i_des3_cbc,krb5i_hash_sha1,0,
    krb5i_dk_encrypt_len,krb5i_dk_encrypt,krb5i_dk_decrypt,
    {"des3-cbc-sha1","des3-hmac-sha1","des3-cbc-sha1-kd",NULL}},
#endif	/* KRB5_DES3_SUPPORT */
#if KRB5_RC4_SUPPORT
    {ENCTYPE_ARCFOUR_HMAC,krb5i_arcfour,krb5i_hash_md5,0,
	krb5i_arcfour_encrypt_len,krb5i_arcfour_encrypt,krb5i_arcfour_decrypt,
    {"rc4-hmac","arcfour-hmac","arcfour-hmac-md5",NULL}},
#endif
#if KRB5_RC4_SUPPORT > 1
    {ENCTYPE_ARCFOUR_HMAC_EXP,krb5i_arcfour,krb5i_hash_md5,0,
	krb5i_arcfour_encrypt_len,krb5i_arcfour_encrypt,krb5i_arcfour_decrypt,
    {"rc4-hmac-exp","arcfour-hmac-exp","arcfour-hmac-md5-exp",NULL}},
#endif	/* KRB5_RC4_SUPPORT */
#if KRB5_AES_SUPPORT
    {ENCTYPE_AES128_CTS_HMAC_SHA1_96,krb5i_aes128_cts,krb5i_hash_sha1,12,
	krb5i_dk_encrypt_len,krb5i_dk_encrypt,krb5i_dk_decrypt,
    {"aes128-cts","aes128-cts-hmac-sha1-96",NULL}},
    {ENCTYPE_AES256_CTS_HMAC_SHA1_96,krb5i_aes256_cts,krb5i_hash_sha1,12,
	krb5i_dk_encrypt_len,krb5i_dk_encrypt,krb5i_dk_decrypt,
    {"aes256-cts","aes256-cts-hmac-sha1-96",NULL}},
#endif	/* KRB5_AES_SUPPORT */

    /* old mdw experimental */
#if KRB5_CAST_SUPPORT
    {ENCTYPE_CAST_CBC_SHA,krb5i_cast_cbc,krb5i_hash_sha1,0,
    krb5i_old_encrypt_len,krb5i_old_encrypt,krb5i_old_decrypt,
    {"cast-cbc-sha",NULL}},
#endif	/* KRB5_CAST_SUPPORT */
#if KRB5_RC6_SUPPORT
    {ENCTYPE_RC6_CBC_SHA,krb5i_rc6_cts,krb5i_hash_sha1,0,
    krb5i_old_encrypt_len,krb5i_old_encrypt,krb5i_old_decrypt,
    {"rc6-cbc-sha",NULL}},
#endif	/* KRB5_RC6_SUPPORT */
};

int
krb5i_iterate_enctypes(int (*f)(void *, krb5_enctype, char *const *,
    void (*)(unsigned int *, unsigned int *),
    void (*)(unsigned int *, unsigned int *)), void *a)
{
    int i, r;
    for (i = 0;
	    i < sizeof krb5i_enctypes_list/sizeof *krb5i_enctypes_list;
	    ++i) {
	r = f(a,krb5i_enctypes_list[i].enctype,
		krb5i_enctypes_list[i].names,
		krb5i_enctypes_list[i].enc->block_size,
		krb5i_enctypes_list[i].enc->key_size);
	if (r != -1) return r;
    }
    return -1;
}

static const struct krb5_enctypes * find_enctype(krb5_enctype enctype)
{
    int i;

    for (i = 0;
	    i < sizeof krb5i_enctypes_list/sizeof *krb5i_enctypes_list;
	    ++i) {

	if (krb5i_enctypes_list[i].enctype == enctype)
	    return krb5i_enctypes_list + i;
    }
    return NULL;
}

krb5_error_code krb5_c_make_random_key(krb5_context ctx,
    krb5_enctype enctype,
    krb5_keyblock *kb)
{
    krb5_data data[1];
    int code;
    const struct krb5_enctypes * et = find_enctype(enctype);

    if (!et) return KRB5_BAD_ENCTYPE;
    kb->contents = 0;
    memset(data, 0, sizeof *data);
    et->enc->key_size(&data->length, &kb->length);
    code = ENOMEM;
    if (!(data->data = afs_osi_Alloc(data->length)))
	goto Out;
    if (!(kb->contents = afs_osi_Alloc(kb->length)))
	goto Out;
    kb->enctype = enctype;
    do {
	if ((code = krb5_c_random_make_octets(ctx, data)))
	    break;
	code = et->enc->make_key(data, kb, 1);
    } while (code == KRB5DES_WEAK_KEY);
Out:
    if (data->data)
	afs_osi_Free(data->data, data->length);
    if (code) {
	if (kb->contents)
	    afs_osi_Free(kb->contents, kb->length);
	kb->contents = 0;
    }
    return code;
}

krb5_error_code krb5i_derive_key(const struct krb5_enc_provider *enc,
    const krb5_keyblock *in,
    krb5_keyblock *out,
    const krb5_data *constant)
{
    unsigned int mbs, blocksize, keybytes, keylen;
    int code;
    unsigned int cs;
    unsigned char *work, *t;
    krb5_data inblock[1], outblock[1];
    int i;

    enc->block_size(&mbs, &blocksize);
    enc->key_size(&keybytes, &keylen);

    if (blocksize < mbs) blocksize = mbs;
    if (in->length != keylen || out->length != keylen)
	return KRB5_CRYPTO_INTERNAL;
    if (!(work = afs_osi_Alloc(keybytes + 2*blocksize)))
	return ENOMEM;
    inblock->length = outblock->length = blocksize;
    outblock->data = (inblock->data = work + keybytes) + blocksize;
    if (constant->length == blocksize)
	memcpy(inblock->data, constant->data, blocksize);
    else krb5i_nfold(constant->data, constant->length,
	inblock->data, inblock->length);

    for (i = 0; ; )
    {
	cs = keybytes - i;
	if (cs > blocksize) cs = blocksize;
	code = enc->encrypt(in, 0, inblock, outblock);
	if (code) goto Out;
	memcpy(work + i, outblock->data, cs);
	i += cs;
	if (i == keybytes) break;
	t = inblock->data;
	inblock->data = outblock->data;
	outblock->data = t;
    }

    inblock->data = work;
    inblock->length = keybytes;

    code = enc->make_key(inblock, out, 1);
    if (code == KRB5DES_WEAK_KEY) code = 0;
    if (code) goto Out;
Out:
    memset(work, 0, keybytes + blocksize*2);
    afs_osi_Free(work, keybytes + blocksize*2);
    return code;
}

krb5_boolean krb5_c_valid_enctype(krb5_enctype enctype)
{
    return !!find_enctype(enctype);
}

krb5_error_code
krb5_c_block_size(krb5_context context,
    krb5_enctype enctype, size_t *blocksize)
{
    const struct krb5_enctypes * et = find_enctype(enctype);
    unsigned int mbs, bs;

    if (!et) return KRB5_BAD_ENCTYPE;
    et->enc->block_size(&mbs, &bs);
    *blocksize = mbs;
    return 0;
}

krb5_error_code
krb5_c_encrypt_length(krb5_context context,
    krb5_enctype enctype,
	size_t inlen, size_t *outlen)
{
    const struct krb5_enctypes * et = find_enctype(enctype);

    if (!et) return KRB5_BAD_ENCTYPE;
    return et->encrypt_len(et->enc, et->hash,
	et->hash_trunc_size,
	inlen, outlen);
}

krb5_error_code
krb5_c_encrypt(krb5_context context,
    const krb5_keyblock * key,
	krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_data * input,
    krb5_enc_data* output)
{
    const struct krb5_enctypes * et = find_enctype(key->enctype);

    if (!et) return KRB5_BAD_ENCTYPE;
    output->kvno = 0;
    output->enctype = key->enctype;
    return et->encrypt(et->enc, et->hash,
	et->hash_trunc_size,
	key, usage, ivec, input, &output->ciphertext);
}

krb5_error_code
krb5_c_decrypt(krb5_context context,
    const krb5_keyblock * key,
	krb5_keyusage usage,
    const krb5_data * ivec,
    const krb5_enc_data *input,
    krb5_data*output)
{
    const struct krb5_enctypes * et = find_enctype(key->enctype);

    if (!et) return KRB5_BAD_ENCTYPE;
    if (input->enctype != ENCTYPE_UNKNOWN &&
	key->enctype != input->enctype)
	return KRB5_BAD_ENCTYPE;
    return et->decrypt(et->enc, et->hash,
	et->hash_trunc_size,
	key, usage, ivec, &input->ciphertext, output);
}

krb5_error_code
krb5_string_to_enctype(char *name, krb5_enctype *enctype)
{
    int i, j;
    char *cp;

    for (i = 0;
	    i < sizeof krb5i_enctypes_list/sizeof *krb5i_enctypes_list;
	    ++i) {
	for (j = 0; (cp = krb5i_enctypes_list[i].names[j]); ++j)
	    if (!afs_strcasecmp(cp, name)) {
		*enctype = krb5i_enctypes_list[i].enctype;
		return 0;
	    }
    }
    return EINVAL;
}

krb5_error_code
krb5_string_to_cksumtype(char *name, krb5_cksumtype *cksumtype)
{
    int i, j;
    char *cp;

    for (i = 0;
	    i < sizeof krb5i_cksumtypes_list/sizeof *krb5i_cksumtypes_list;
	    ++i) {
	for (j = 0; (cp = krb5i_cksumtypes_list[i].names[j]); ++j)
	    if (!afs_strcasecmp(cp, name)) {
		*cksumtype = krb5i_cksumtypes_list[i].cksumtype;
		return 0;
	    }
    }
    return EINVAL;
}
