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

#include "config.h"

void *
_afscrypto_calloc(int num, size_t len)
{
    void *ptr;
    size_t total;

    total = num * len;
    ptr = afs_osi_Alloc(total);
    /* In practice, callers assume the afs_osi_Alloc() will not fail. */
    if (ptr != NULL)
	memset(ptr, 0, total);

    return ptr;
}

void *
_afscrypto_malloc(size_t len)
{
    void *ptr;

    ptr = afs_osi_Alloc(len);

    return ptr;
}

void
_afscrypto_free(void *ptr)
{
    if (ptr != NULL)
	afs_osi_Free(ptr, 0);
}

char*
_afscrypto_strdup(const char *str) {
    char *ptr;

    ptr = malloc(strlen(str) + 1);
    if (ptr == NULL)
       return ptr;
    memcpy(ptr, str, strlen(str) + 1);

    return ptr;
}

/* This is a horrible, horrible bodge, but the crypto code uses realloc,
 * so we need to handle it too.
 *
 * There are two different call sites for realloc. Firstly, it's used
 * in the decrypt case to shrink the size of the allotted buffer. In
 * this case, we can just ignore the realloc and return the original
 * pointer.
 *
 * Secondly, it's used when computing derived keys. In this case, the
 * first call will be with a NULL input, and the size of a single
 * derived key. So, we just give back space for 20 keys, and pray.
 */

void *
_afscrypto_realloc(void *ptr, size_t len) {
   if (ptr == NULL)
	return calloc(20, len);
   return ptr;
}
