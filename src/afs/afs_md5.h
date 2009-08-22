/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id$ */


struct afs_md5 {
    unsigned int sz[2];
    afs_uint32 counter[4];
    unsigned char save[64];
};

void AFS_MD5_Init(struct afs_md5 *m);
void AFS_MD5_Update(struct afs_md5 *m, const void *p, size_t len);
void AFS_MD5_Final(void *res, struct afs_md5 *m);	/* afs_uint32 res[4] */
void AFS_MD5_String(void *res, const void *v, size_t len);

/* stuff in common between md4, md5, and sha1 */

#ifndef min
#define min(a,b) (((a)>(b))?(b):(a))
#endif

/* Vector Crays doesn't have a good 32-bit type, or more precisely,
   int32_t as defined by <bind/bitypes.h> isn't 32 bits, and we don't
   want to depend in being able to redefine this type.  To cope with
   this we have to clamp the result in some places to [0,2^32); no
   need to do this on other machines.  Did I say this was a mess?
   */

#ifdef _CRAY
#define CRAYFIX(X) ((X) & 0xffffffff)
#else
#define CRAYFIX(X) (X)
#endif

#if !defined(inline) && !defined(__GNUC__)
#define inline
#endif

static inline afs_uint32
cshift(afs_uint32 x, unsigned int n)
{
    x = CRAYFIX(x);
    return CRAYFIX((x << n) | (x >> (32 - n)));
}
