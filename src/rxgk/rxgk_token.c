/* rxgk/rxgk_token.c - Token generation/manuipluation routines for RXGK */
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

/**
 * @file
 * Routines to generate, encode, encrypt, decode, and decrypt rxgk tokens.
 */


#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <rx/rx.h>
#include <rx/xdr.h>
#include <rx/rx_opaque.h>
#include <rx/rxgk.h>
#include <errno.h>

#include "rxgk_private.h"

/*
 * Copy the fields from a TokenInfo to a Token.
 */
static void
tokeninfo_to_token(RXGK_TokenInfo *info, RXGK_Token *token)
{

    token->enctype = info->enctype;
    token->level = info->level;
    token->lifetime = info->lifetime;
    token->bytelife = info->bytelife;
    token->expirationtime = info->expiration;
    return;
}

/*
 * Take the input RXGK_Token and XDR-encode it, returning the result in 'out'.
 * The caller is responsible for freeing the memory contained in 'out'.
 *
 * Returns RX errors.
 */
static afs_int32
pack_token(RXGK_Token *token, struct rx_opaque *out)
{
    XDR xdrs;
    afs_int32 ret;
    u_int len;

    memset(&xdrs, 0, sizeof(xdrs));
    memset(out, 0, sizeof(*out));
    xdrlen_create(&xdrs);
    if (!xdr_RXGK_Token(&xdrs, token)) {
	ret = RXGEN_SS_MARSHAL;
	goto done;
    }
    len = xdr_getpos(&xdrs);

    ret = rx_opaque_alloc(out, len);
    if (ret != 0)
	goto done;

    xdr_destroy(&xdrs);
    xdrmem_create(&xdrs, out->val, len, XDR_ENCODE);
    if (!xdr_RXGK_Token(&xdrs, token)) {
	rx_opaque_freeContents(out);
	ret = RXGEN_SS_MARSHAL;
	goto done;
    }
    ret = 0;

 done:
    if (xdrs.x_ops) {
        xdr_destroy(&xdrs);
    }
    return ret;
}

/*
 * Take the input TokenContainer and XDR-encode it, returning the result
 * in 'out'.  The caller is responsible for freeing the memory contained
 * in 'out'.
 *
 * Returns RX errors.
 */
static afs_int32
pack_container(RXGK_TokenContainer *container, struct rx_opaque *out)
{
    XDR xdrs;
    afs_int32 ret;
    u_int len;

    memset(&xdrs, 0, sizeof(xdrs));
    xdrlen_create(&xdrs);
    if (!xdr_RXGK_TokenContainer(&xdrs, container)) {
	ret = RXGEN_SS_MARSHAL;
	goto done;
    }
    len = xdr_getpos(&xdrs);

    ret = rx_opaque_alloc(out, len);
    if (ret != 0)
	goto done;

    xdr_destroy(&xdrs);
    xdrmem_create(&xdrs, out->val, len, XDR_ENCODE);
    if (!xdr_RXGK_TokenContainer(&xdrs, container)) {
	rx_opaque_freeContents(out);
	ret = RXGEN_SS_MARSHAL;
	goto done;
    }
    ret = 0;

 done:
    if (xdrs.x_ops) {
        xdr_destroy(&xdrs);
    }
    return ret;
}

/*
 * Take the input token, encode it, encrypt that blob, populate a
 * TokenContainer with the encrypted token, kvno, and enctype, and encode
 * the resulting TokenContainer into 'out'.
 *
 * Returns RX errors.
 */
static afs_int32
pack_wrap_token(rxgk_key server_key, afs_int32 kvno, afs_int32 enctype,
		RXGK_Token *token, struct rx_opaque *out)
{
    struct rx_opaque packed_token = RX_EMPTY_OPAQUE;
    struct rx_opaque encrypted_token = RX_EMPTY_OPAQUE;
    RXGK_TokenContainer container;
    afs_int32 ret;

    memset(&container, 0, sizeof(container));

    /* XDR-encode the token in to packed_token. */
    ret = pack_token(token, &packed_token);
    if (ret != 0)
	goto done;

    ret = rxgk_encrypt_in_key(server_key, RXGK_SERVER_ENC_TOKEN, &packed_token,
			      &encrypted_token);
    if (ret != 0)
	goto done;
    ret = rx_opaque_populate(&container.encrypted_token, encrypted_token.val,
			     encrypted_token.len);
    if (ret != 0)
	goto done;
    container.kvno = kvno;
    container.enctype = enctype;

    /* Now the token container is populated; time to encode it into 'out'. */
    ret = pack_container(&container, out);

 done:
    rx_opaque_freeContents(&packed_token);
    rx_opaque_freeContents(&encrypted_token);
    rx_opaque_freeContents(&container.encrypted_token);
    return ret;
}

/**
 * Print an rxgk token with random key, returning key and token
 *
 * Print a token (with empty identity list) with a random master key,
 * and encrypt it in the specified key/kvno/enctype.  Return the master
 * key as well as the token, so that the token is usable.
 *
 * The caller must free k0 with release_key().
 *
 * @param[out] out	The printed token (RXGK_TokenContainer).
 * @param[in] input_info	Parameters describing the token to be printed.
 * @param[in] key	The token-encrypting key.
 * @param[in] kvno	The kvno of key.
 * @param[in] enctype	The enctype of key.
 * @param[out] k0_out	The token master key.
 * @return rxgk error codes.
 */
afs_int32
rxgk_print_token_and_key(struct rx_opaque *out, RXGK_TokenInfo *input_info,
                         rxgk_key key, afs_int32 kvno, afs_int32 enctype,
                         rxgk_key *k0_out)
{
    struct rx_opaque k0_data = RX_EMPTY_OPAQUE;
    rxgk_key k0 = NULL;
    ssize_t len;
    afs_int32 ret;

    *k0_out = NULL;

    len = rxgk_etype_to_len(input_info->enctype);
    if (len < 0) {
	ret = RXGK_BADETYPE;
        goto done;
    }

    ret = rxgk_nonce(&k0_data, len);
    if (ret != 0)
	goto done;

    ret = rxgk_make_key(&k0, k0_data.val, k0_data.len, input_info->enctype);
    if (ret != 0)
	goto done;

    ret = rxgk_print_token(out, input_info, &k0_data, key, kvno, enctype);
    if (ret != 0)
	goto done;

    *k0_out = k0;
    k0 = NULL;

 done:
    rx_opaque_freeContents(&k0_data);
    rxgk_release_key(&k0);
    return ret;
}

/*
 * Helper functions for rxgk_extract_token.
 */
static int
unpack_container(RXGK_Data *in, RXGK_TokenContainer *container)
{
    XDR xdrs;

    memset(&xdrs, 0, sizeof(xdrs));

    xdrmem_create(&xdrs, in->val, in->len, XDR_DECODE);
    if (!xdr_RXGK_TokenContainer(&xdrs, container)) {
	xdr_destroy(&xdrs);
	return RXGEN_SS_UNMARSHAL;
    }
    xdr_destroy(&xdrs);
    return 0;
}

static int
decrypt_token(struct rx_opaque *enctoken, afs_int32 kvno, afs_int32 enctype,
              rxgk_getkey_func getkey, void *rock, RXGK_Data *out)
{
    rxgk_key service_key = NULL;
    afs_int32 ret;

    if (kvno <= 0 || enctype <= 0) {
	ret = RXGK_BAD_TOKEN;
        goto done;
    }

    ret = getkey(rock, &kvno, &enctype, &service_key);
    if (ret != 0)
	goto done;
    ret = rxgk_decrypt_in_key(service_key, RXGK_SERVER_ENC_TOKEN, enctoken,
			      out);

 done:
    rxgk_release_key(&service_key);
    return ret;
}

static int
unpack_token(RXGK_Data *in, RXGK_Token *token)
{
    XDR xdrs;

    memset(&xdrs, 0, sizeof(xdrs));

    xdrmem_create(&xdrs, in->val, in->len, XDR_DECODE);
    if (!xdr_RXGK_Token(&xdrs, token)) {
	xdr_destroy(&xdrs);
	return RXGEN_SS_UNMARSHAL;
    }
    xdr_destroy(&xdrs);
    return 0;
}

/**
 * Extract a cleartext RXGK_Token from a packed RXGK_TokenContainer
 *
 * Given an XDR-encoded RXGK_TokenContainer, extract/decrypt the contents
 * into an RXGK_Token.
 *
 * The caller must free the returned token with xdr_free.
 *
 * @param[in] tc	The RXGK_TokenContainer to unpack.
 * @param[out] out	The extracted RXGK_Token.
 * @param[in] getkey	The getkey function used to decrypt the token.
 * @param[in] rock	Data to pass to getkey.
 * @return rxgk error codes.
 */
afs_int32
rxgk_extract_token(RXGK_Data *tc, RXGK_Token *out, rxgk_getkey_func getkey,
		   void *rock)
{
    RXGK_TokenContainer container;
    struct rx_opaque packed_token = RX_EMPTY_OPAQUE;
    afs_int32 ret;

    memset(&container, 0, sizeof(container));

    ret = unpack_container(tc, &container);
    if (ret != 0)
	goto done;
    ret = decrypt_token(&container.encrypted_token, container.kvno,
                        container.enctype, getkey, rock, &packed_token);
    if (ret != 0)
	goto done;
    ret = unpack_token(&packed_token, out);

 done:
    xdr_free((xdrproc_t)xdr_RXGK_TokenContainer, &container);
    xdr_free((xdrproc_t)xdr_RXGK_Data, &packed_token);
    return ret;
}

/* NEVER call this function directly (except from rxgk_make_token or
 * rxgk_print_token). Call rxgk_make_token or rxgk_print_token instead. See
 * rxgk_make_token for info about our arguments. */
static afs_int32
make_token(struct rx_opaque *out, RXGK_TokenInfo *info,
	   struct rx_opaque *k0, PrAuthName *identities,
	   int nids, rxgk_key key, afs_int32 kvno, afs_int32 enctype)
{
    RXGK_Token token;
    afs_int32 ret;

    memset(&token, 0, sizeof(token));
    memset(out, 0, sizeof(*out));

    if (nids < 0) {
        ret = RXGK_INCONSISTENCY;
        goto done;
    }

    /* Get the tokeninfo values from the authoritative source. */
    tokeninfo_to_token(info, &token);

    /* Create the rest of the token. */
    ret = rx_opaque_populate(&token.K0, k0->val, k0->len);
    if (ret != 0)
	goto done;
    token.identities.len = (afs_uint32)nids;
    token.identities.val = identities;
    ret = pack_wrap_token(key, kvno, enctype, &token, out);
    if (ret != 0)
	goto done;

 done:
    /*
     * We need to free the contents in 'token', but don't free
     * token.identities. The pointer for that was given to us by our caller;
     * they'll manage the memory for it.
     */
    memset(&token.identities, 0, sizeof(token.identities));
    xdr_free((xdrproc_t)xdr_RXGK_Token, &token);
    return ret;
}

/**
 * Create an rxgk token
 *
 * Create a token from the specified TokenInfo, key, start time, and lists
 * of identities.  Encrypts the token and stores it as an rx_opaque.
 *
 * Note that you cannot make printed tokens with this function ('nids' must be
 * greater than 0). This is a deliberate restriction to try to avoid
 * accidentally creating printed tokens.  Use rxgk_print_token() instead to
 * make printed tokens.
 *
 * @param[out] out	The encoded rxgk token (RXGK_TokenContainer).
 * @param[in] info	RXGK_Tokeninfo describing the token to be produced.
 * @param[in] k0	The token master key.
 * @param[in] identities The list of identities to be included in the token.
 * @param[in] nids	The number of identities in the identities list (must
 * 			be positive).
 * @param[in] key	The token-encrypting key to use.
 * @param[in] kvno	The kvno of key.
 * @param[in] enctype	The enctype of key.
 * @return rxgk error codes.
 */
afs_int32
rxgk_make_token(struct rx_opaque *out, RXGK_TokenInfo *info,
		struct rx_opaque *k0, PrAuthName *identities,
		int nids, rxgk_key key, afs_int32 kvno, afs_int32 enctype)
{
    if (nids == 0 || identities == NULL) {
	/* You cannot make printed tokens with this function; use
	 * rxgk_print_token instead. */
	memset(out, 0, sizeof(*out));
	return RXGK_INCONSISTENCY;
    }
    return make_token(out, info, k0, identities, nids, key, kvno, enctype);
}

/* This lifetime is in seconds. */
#define DEFAULT_LIFETIME	(60 * 60 * 10)
/* The bytelife is log_2(bytes). */
#define DEFAULT_BYTELIFE	30
/**
 * Create a printed rxgk token
 *
 * Print a token (with empty identity list) where the master key (k0)
 * already exists, and encrypt it in the specified key/kvno/enctype.
 *
 * @param[out] out	The printed token (RXGK_TokenContainer).
 * @param[in] input_info	Parameters describing the token to be printed.
 * @param[in] k0	The master key to use for the token.
 * @param[in] key	The token-encrypting key.
 * @param[in] kvno	The kvno of key.
 * @param[in] enctype	The enctype of key.
 * @return rxgk error codes.
 */
afs_int32
rxgk_print_token(struct rx_opaque *out, RXGK_TokenInfo *input_info,
		 struct rx_opaque *k0, rxgk_key key, afs_int32 kvno,
		 afs_int32 enctype)
{
    RXGK_TokenInfo info;

    memset(&info, 0, sizeof(info));

    info.enctype = input_info->enctype;
    info.level = input_info->level;
    info.lifetime = DEFAULT_LIFETIME;
    info.bytelife = DEFAULT_BYTELIFE;
    info.expiration = RXGK_NEVERDATE;

    return make_token(out, &info, k0, NULL, 0, key, kvno, enctype);
}

