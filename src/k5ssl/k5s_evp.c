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

#include <sys/types.h>
#include "k5s_evp.h"
#include "k5s_rc4.h"
#ifdef KERNEL
#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afsincludes.h"        /*AFS-based standard headers */
#else
#include <errno.h>
#endif

int
EVP_DigestInit_ex(EVP_MD_CTX *c, const EVP_MD *m, void *e)
{
    if (!c->md_data && m->ctx_size) {
	if (!(c->md_data = osi_Alloc(m->ctx_size)))
	    return ENOMEM;
    }
    c->md = m;
    return m->init(c);
}

int EVP_MD_CTX_cleanup(EVP_MD_CTX *c)
{
    if (c->md_data) {
	memset(c->md_data, 0, c->md->ctx_size);
	afs_osi_Free(c->md_data, c->md->ctx_size);
    }
    memset(c,0,sizeof *c);
    return 1;
}

int EVP_DigestFinal_ex(EVP_MD_CTX *c, unsigned char *d, unsigned int *l)
{
    int r;

    r = c->md->final(c, d);
    if (l)
	*l = c->md->digest_len;
    memset(c->md_data, 0, c->md->ctx_size);
    return r;
}

int
EVP_CipherInit_ex(EVP_CIPHER_CTX *c, const EVP_CIPHER *m, void *x,
    const unsigned char *k, const unsigned char *i, int e)
{
    EVP_CIPHER_CTX_cleanup(c);
    c->encrypt = e;
    if (!c->cipher_data && m->ctx_size) {
	if (!(c->cipher_data = osi_Alloc(m->ctx_size)))
	    return ENOMEM;
    }
    c->cipher = m;
    c->ctx_len = m->ctx_size;
    c->key_len = m->key_size;
    c->nopadding = m->block_size == 1;
    switch(EVP_CIPHER_CTX_flags(c)) {
    case EVP_CIPH_STREAM_CIPHER:
    case EVP_CIPH_ECB_MODE:
	break;
    case EVP_CIPH_CFB_MODE:
    case EVP_CIPH_OFB_MODE:
	c->num = 0;
	/* fall through */
    case EVP_CIPH_CBC_MODE:
	if (i)
	    memcpy(c->iv, i, EVP_CIPHER_CTX_iv_length(c));
	break;
    default:
	    /* XXX no error? */
	return 0;
    }
    return c->cipher->init_key(c,k,i,e);
}

int
EVP_EncryptUpdate(EVP_CIPHER_CTX *c, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    int i, bs = c->cipher->block_size;
    if (c->used) {
	i = bs - c->used;
	if (i > inl) i = inl;
	memcpy(c->buf+c->used, in, i);
	in += i; inl -= i; c->used += i;
	if (c->used < bs) {
		*outl = 0;
		return 1;
	}
	*outl = bs;
	if (!c->cipher->update(c,out,c->buf,bs)) return 0;
	out += bs;
	c->used = 0;
    } else *outl = 0;
    i = inl & -bs;
    if (i) {
	if (!c->cipher->update(c,out,in,i)) return 0;
	*outl += i;
	inl -= i;
    }
    if ((c->used = inl))
	memcpy(c->buf, in + i, inl);
    return 1;
}

int
EVP_EncryptFinal_ex(EVP_CIPHER_CTX *c, unsigned char *out, int *outl)
{
    int bs = c->cipher->block_size, r;
    if (c->nopadding) {
	if (c->used)
	    return 0;
	*outl = 0;
	return 1;
    }
    memset(c->buf+c->used, bs-c->used, bs-c->used);
    r = c->cipher->update(c,out,c->buf,bs);
    if (r) *outl = bs;
    return r;
}

int
EVP_DecryptUpdate(EVP_CIPHER_CTX *c, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
    int r;
    if (c->nopadding) return EVP_EncryptUpdate(c,out,outl,in,inl);
    if (!inl) {
	*outl = 0;
	return 1;
    }
    r = EVP_EncryptUpdate(c,out,outl,in,--inl);
    if (!r) return 0;
    c->buf[c->used++] = in[inl];
    return r;
}

int
EVP_DecryptFinal_ex(EVP_CIPHER_CTX *c, unsigned char *out, int *outl)
{
    int i, n;
    if (c->nopadding) {
	if (c->used) {
	    /* EVP_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH */
	    return 0;
	}
	*outl = 0;
	return 1;
    }
    if (c->used != c->cipher->block_size) {
	/* EVP_R_WRONG_FINAL_BLOCK_LENGTH */
	return 0;
    }
    if (!c->cipher->update(c,c->buf,c->buf,c->used)) return 0;
    n = c->buf[c->used-1];
    if ((unsigned)(n-1) >= c->used) {
    }
    if (n < c->used) {
	for (i = c->used - n; i < c->used; ++i)
	    if (c->buf[i] != n) {
		/* EVP_R_BAD_DECRYPT */
		return 0;
	    }
	memcpy(out, c->buf, c->used - n);
    }
    *outl = c->used - n;
    return 1;
}

int
EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *c)
{
    if (c->cipher_data)
      afs_osi_Free(c->cipher_data, c->ctx_len);
    memset(c, 0, sizeof *c);
    return 1;
}

#if 0
void
ERR_print_errors_fp(void *fp)
{
    fprintf(fp, "Something went wrong\n");
}
#endif

static struct {
    RC4_KEY ks[1];
    int init;
} rstate[1];

int
RAND_bytes(unsigned char *b, int c)
{
    RC4(rstate->ks, c, b, b);
    return 1;
}

int
RAND_seed(const void *b, int c)
{
    RC4_set_key_ex(rstate->ks, c, b, rstate->init);
    return rstate->init = 1;
}

int
RAND_status(void)
{
	/* XXX should check entropy */
    return rstate->init;
}
