/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include "afs/param.h"
#include "afs/opr.h"

#include "afs/stds.h"
#include "afs/sysincludes.h"
#include "afs/afsincludes.h"
#include "afs/afs_prototypes.h"

#if defined(assert)
#undef assert
#endif
#define assert opr_Assert

/* hcrypto uses "static inline", which isn't supported by some of our
 * compilers */
#if !defined(inline) && !defined(__GNUC__)
#define inline
#endif

/* We need wrappers for the various memory management functions */
#define calloc _afscrypto_calloc
void * _afscrypto_calloc(int, size_t);

#define malloc _afscrypto_malloc
void * _afscrypto_malloc(size_t);

#define free _afscrypto_free
void _afscrypto_free(void *);

#define strdup _afscrypto_strdup
char * _afscrypto_strdup(const char *);

#define realloc _afscrypto_realloc
void * _afscrypto_realloc(void *, size_t);

/* we may not have strcasecmp in the kernel */
#define strcasecmp afs_strcasecmp

/* osi_readRandom is also prototyped in afs_prototypes.h, but pulling that in
 * here creates loads of additional dependencies */
extern int osi_readRandom(void *, afs_size_t);
