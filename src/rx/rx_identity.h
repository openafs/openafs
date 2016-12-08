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

#ifndef OPENAFS_RX_IDENTITY_H
#define OPENAFS_RX_IDENTITY_H

#include <rx/rx_opaque.h>

typedef enum rx_identity_kind {
    RX_ID_SUPERUSER = -1,
    RX_ID_KRB4 = 0,
    RX_ID_GSS =  1,
} rx_identity_kind;

struct rx_identity {
    rx_identity_kind kind;
    char *displayName;
    struct rx_opaque exportedName;
};

struct rx_identity *rx_identity_new(rx_identity_kind, char *, void *,
				    size_t);
int rx_identity_match(struct rx_identity *a, struct rx_identity *b);
void rx_identity_populate(struct rx_identity *, rx_identity_kind, char *,
			  void *, size_t);
struct rx_identity *rx_identity_copy(struct rx_identity *from);
void rx_identity_copyContents(struct rx_identity *to, struct rx_identity *from);
void rx_identity_freeContents(struct rx_identity *identity);
void rx_identity_free(struct rx_identity **identity);

#endif
