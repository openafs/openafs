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

#include <afsconfig.h>
#include <afs/param.h>
#include <sys/types.h>
#if USE_FAKESSL
#include "k5s_evp.h"
#else
#include <openssl/evp.h>
#endif

#include "k5s_md5.h"

static int init(EVP_MD_CTX *ctx)
{
    MD5Init(ctx->md_data);
    return 1;
}

static int update(EVP_MD_CTX *ctx,const void *data,size_t count)
{
    MD5Update(ctx->md_data, data, count);
    return 1;
}

static int final(EVP_MD_CTX *ctx,unsigned char *md)
{
    MD5Final(md, ctx->md_data);
    return 1;
}

#ifndef NID_md5
#define NID_md5 0
#define NID_md5WithRSAEncryption 0
#define EVP_PKEY_RSA_method 0
#endif

static const EVP_MD md5_md[1] = {{
    NID_md5,
    NID_md5WithRSAEncryption,
    16,
    0,
    init,
    update,
    final,
    (int) /* !! */ NULL,
    (int) /* !! */ NULL,
    EVP_PKEY_RSA_method,
    64,
    sizeof(EVP_MD*)+sizeof(MD5_CTX),
}};

const EVP_MD *EVP_md5(void)
{
    return md5_md;
}
