/*
 * Copyright (c) 2019 Sine Nomine Associates. All rights reserved.
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
#include <afs/param.h>
#include <afs/stds.h>

#include <errno.h>
#include <rx/rx.h>
#include <rx/rxgk.h>
#include <afs/rfc3961.h>
#include <afs/opr.h>

#include <tests/tap/basic.h>
#include <assert.h>

#include "common.h"

#define OPAQUE(str) { sizeof(str) - 1, (str) }

static void
key2data(rxgk_key key, struct rx_opaque *data)
{
    /* Same as the private struct rxgk_keyblock from rxgk_crypto_rfc3961.c. */
    struct key_impl {
	krb5_context ctx;
	krb5_keyblock key;
    };
    krb5_keyblock *keyblock = &((struct key_impl *)key)->key;

    data->len = keyblock->keyvalue.length;
    data->val = keyblock->keyvalue.data;
}

int
main(void)
{
    int tc_i;
    struct {
	char *descr;

	struct rx_opaque k0_raw;
	struct rx_opaque tk_raw;

	afs_uint32 enctype;

	afs_uint32 epoch;
	afs_uint32 cid;
	rxgkTime start_time;
	afs_uint32 key_number;

    } *tc, test_cases[] = {
	{
	    "key_number 1",
	    OPAQUE("1234567890123456"),
	    OPAQUE("\x61\x8b\xbb\xaa\x4c\xb8\xd9\x82\xb3\x09\x7c\x67\x95\x40\x40\x9f"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007429, 0x760a9c24, 15710085940000001LL, 1
	},
	{
	    "different k0",
	    OPAQUE("\xde\xad\xbe\xef\xba\xdd\xca\xfe\xd0\xd0\xca\xca\xde\xad\xc0\xde"),
	    OPAQUE("\x66\x45\x6f\xfc\x5d\x1b\xdd\x9e\x54\x62\x8c\xca\xd9\x8f\x23\x9f"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007429, 0x760a9c24, 15710085940000001LL, 1
	},
	{
	    "aes256",
	    OPAQUE("1234567890123456\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"),
	    OPAQUE("\x0b\xfd\xe1\x78\x01\x8d\x41\x70\xe8\x74\x52\x55\x7a\x7a\x2c\xaa"
		   "\x3b\x36\xe4\x19\x6b\x3c\x72\x6c\x8c\x2c\x03\xa8\x31\x38\x82\x54"),
	    ETYPE_AES256_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007429, 0x760a9c24, 15710085940000001LL, 1
	},
	{
	    "different epoch",
	    OPAQUE("1234567890123456"),
	    OPAQUE("\x3c\x69\xfc\xe0\xe2\xe1\x1c\xd4\xd0\x3b\xa1\xc2\x05\x35\x18\x71"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007430, 0x760a9c24, 15710085940000001LL, 1
	},
	{
	    "different cid",
	    OPAQUE("1234567890123456"),
	    OPAQUE("\x7c\xda\x40\x6f\xa0\xd6\x35\x86\xc1\xb4\x99\xa2\x41\xa6\xcc\xca"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007429, 0x760a9c25, 15710085940000001LL, 1
	},
	{
	    "different start_time",
	    OPAQUE("1234567890123456"),
	    OPAQUE("\x5b\xa5\x6b\xe9\x12\x2f\x74\x69\x1c\x18\x43\x22\x6b\x36\x72\x9d"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007429, 0x760a9c25, 15710085940000002LL, 1
	},
	{
	    "different key_number",
	    OPAQUE("1234567890123456"),
	    OPAQUE("\xb9\x3b\x23\x32\x16\x91\x0d\xd3\x7f\x2d\x8d\x67\xae\x07\xa8\x63"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	     start_time	 key_number */
	    1571007429, 0x760a9c24, 15710085940000001LL, 2
	},
	{
	    "start_time 0, key_number 9999",
	    OPAQUE("\xde\xad\xbe\xef\xba\xdd\xca\xfe\xd0\xd0\xca\xca\xde\xad\xc0\xde"),
	    OPAQUE("\x82\x6a\x16\xd5\x94\xf9\x2f\xca\x7c\x43\x4f\xf1\xe7\x35\xe2\x81"),
	    ETYPE_AES128_CTS_HMAC_SHA1_96,
	    /*   epoch	       cid	    start_time	key_number */
	    1571000000, 0xdeadb33f,		     0, 9999
	},
    };

    plan(24);

    for (afstest_Scan(test_cases, tc, tc_i)) {
	int code;
	rxgk_key k0 = NULL;
	rxgk_key tk = NULL;
	struct rx_opaque keydata;

	memset(&keydata, 0, sizeof(keydata));

	code = rxgk_make_key(&k0, tc->k0_raw.val, tc->k0_raw.len,
			     tc->enctype);
	is_int(code, 0, "[%s] rxgk_make_key() == 0", tc->descr);

	code = rxgk_derive_tk(&tk, k0, tc->epoch, tc->cid,
			      tc->start_time, tc->key_number);
	is_int(code, 0, "[%s] rxgk_derive_tk() == 0", tc->descr);

	key2data(tk, &keydata);
	is_opaque(&keydata, &tc->tk_raw, "[%s] ... key matches", tc->descr);

	rxgk_release_key(&k0);
	rxgk_release_key(&tk);
    }

    return 0;
}
