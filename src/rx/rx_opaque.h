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

#ifndef OPENAFS_RX_OPAQUE_H
#define OPENAFS_RX_OPAQUE_H

struct rx_opaque {
    size_t len;
    void *val;
};

#define RX_EMPTY_OPAQUE {0, NULL}

struct rx_opaque *rx_opaque_new(void *data, size_t datalen);
int rx_opaque_alloc(struct rx_opaque *buf, size_t length);
int rx_opaque_populate(struct rx_opaque *to, const void *data, size_t datalen);
int rx_opaque_copy(struct rx_opaque *to, const struct rx_opaque *from);
void rx_opaque_freeContents(struct rx_opaque *buf);
void rx_opaque_zeroFreeContents(struct rx_opaque *buf);
void rx_opaque_free(struct rx_opaque **buf);
void rx_opaque_zeroFree(struct rx_opaque **buf);

#endif
