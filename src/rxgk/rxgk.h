/* rxgk.h - External interfaces for RXGK */
/*
 * Copyright (C) 2013, 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * External interfaces for RXGK.
 */

#ifndef OPENAFS_RXGK_H
#define OPENAFS_RXGK_H

/* Pull in the com_err table */
#include <rx/rxgk_errs.h>

/* Pull in the protocol description */
#include <rx/rxgk_int.h>

/* RX-internal headers we depend on. */
#include <rx/rx_opaque.h>

/* rxgkTime is defined in rxgk_int.xg. rxgkTime values are unix timestamps, but
 * in 100-nanosecond units. */

/* Helpers to avoid having to count zeros. */
static_inline rxgkTime secondsToRxgkTime(afs_uint64 seconds) {
    return seconds * (rxgkTime)10000000;
}
static_inline afs_uint64 rxgkTimeToSeconds(rxgkTime atime) {
    return (afs_uint64)atime / 10000000;
}

/** Get the current timestamp as an rxgkTime. */
static_inline rxgkTime RXGK_NOW(void)
{
    struct timeval tv;
    osi_GetTime(&tv);
    return secondsToRxgkTime(tv.tv_sec) + (rxgkTime)tv.tv_usec * 10;
}

/* rxgk_key is an opaque type to wrap our RFC3961 implementation's concept
 * of a key.  It has (at least) the keyblock and length, and enctype. */
typedef struct rxgk_key_s * rxgk_key;

typedef afs_int32 (*rxgk_getkey_func)(void *rock, afs_int32 *kvno,
				      afs_int32 *enctype, rxgk_key *key);
/* rxgk_server.c */
struct rx_securityClass * rxgk_NewServerSecurityObject(void *getkey_rock,
						       rxgk_getkey_func getkey);
/* rxgk_client.c */
struct rx_securityClass *rxgk_NewClientSecurityObject(RXGK_Level level,
						      afs_int32 enctype,
						      rxgk_key k0,
						      RXGK_Data *token,
						      afsUUID *uuid);

/* rxgk_crypto_IMPL.c (currently rfc3961 is the only IMPL) */
afs_int32 rxgk_make_key(rxgk_key *key_out, void *raw_key, afs_uint32 length,
			afs_int32 enctype) AFS_NONNULL();
afs_int32 rxgk_copy_key(rxgk_key key_in, rxgk_key *key_out) AFS_NONNULL();
afs_int32 rxgk_random_key(afs_int32 *enctype, rxgk_key *key_out) AFS_NONNULL();
void rxgk_release_key(rxgk_key *key) AFS_NONNULL();
afs_int32 rxgk_mic_length(rxgk_key key, size_t *out) AFS_NONNULL();
afs_int32 rxgk_mic_in_key(rxgk_key key, afs_int32 usage, RXGK_Data *in,
			  RXGK_Data *out) AFS_NONNULL();
afs_int32 rxgk_check_mic_in_key(rxgk_key key, afs_int32 usage, RXGK_Data *in,
				RXGK_Data *mic) AFS_NONNULL();
afs_int32 rxgk_encrypt_in_key(rxgk_key key, afs_int32 usage, RXGK_Data *in,
			      RXGK_Data *out) AFS_NONNULL();
afs_int32 rxgk_decrypt_in_key(rxgk_key key, afs_int32 usage, RXGK_Data *in,
			      RXGK_Data *out) AFS_NONNULL();
afs_int32 rxgk_derive_tk(rxgk_key *tk, rxgk_key k0, afs_uint32 epoch,
			 afs_uint32 cid, rxgkTime start_time,
			 afs_uint32 key_number) AFS_NONNULL();
afs_int32 rxgk_cipher_expansion(rxgk_key k0, afs_uint32 *len_out) AFS_NONNULL();
afs_int32 rxgk_nonce(RXGK_Data *nonce, afs_uint32 len) AFS_NONNULL();

/* rxgk_token.c */
afs_int32 rxgk_make_token(struct rx_opaque *out, RXGK_TokenInfo *info,
			  struct rx_opaque *k0, PrAuthName *identities,
			  int nids, rxgk_key key, afs_int32 kvno,
			  afs_int32 enctype) AFS_NONNULL((1,2,3,6));
afs_int32 rxgk_print_token(struct rx_opaque *out, RXGK_TokenInfo *input_info,
			   struct rx_opaque *k0, rxgk_key key, afs_int32 kvno,
			   afs_int32 enctype) AFS_NONNULL();
afs_int32 rxgk_print_token_and_key(struct rx_opaque *out,
                                   RXGK_TokenInfo *input_info, rxgk_key key,
                                   afs_int32 kvno, afs_int32 enctype,
                                   rxgk_key *k0_out) AFS_NONNULL();

#endif /* OPENAFS_RXGK_H */
