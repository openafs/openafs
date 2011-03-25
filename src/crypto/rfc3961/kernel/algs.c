/*
 * Copyright (c) 2010 Your Filesystem Inc. All rights reserved.
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

/* Algorithm list for the in-kernel hcrypto implementation. We use a really cut
 * down list of algorithms, to reduce the code-footprint of our kernel module.
 */

#include "krb5_locl.h"

struct _krb5_checksum_type *_krb5_checksum_types[] = {
    &_krb5_checksum_sha1,
    &_krb5_checksum_hmac_sha1_aes128,
    &_krb5_checksum_hmac_sha1_aes256,
};

int _krb5_num_checksums
        = sizeof(_krb5_checksum_types) / sizeof(_krb5_checksum_types[0]);

struct _krb5_encryption_type *_krb5_etypes[] = {
    &_krb5_enctype_aes256_cts_hmac_sha1,
    &_krb5_enctype_aes128_cts_hmac_sha1,
};

int _krb5_num_etypes = sizeof(_krb5_etypes) / sizeof(_krb5_etypes[0]);
