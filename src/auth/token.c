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

#include <sys/errno.h>

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/auth.h>
#include <rx/rxkad.h>
#include "ktc.h"
#include "token.h"


/* Routines for processing tokens in the new XDR format
 *
 * The code here is inspired by work done by Jeffrey Hutzelman et al
 * at the AFSSIG.se hackathon and further refined by Matt
 * Benjamin and Marcus Watts as part of rxk5. However, unless
 * otherwise noted, the implementation is new
 */

/* Given a token type, return the entry number of the first token of that
 * type */
static int
findTokenEntry(struct ktc_setTokenData *token,
	       int targetType)
{
    XDR xdrs;
    int i, type;

    for (i = 0; i < token->tokens.tokens_len; i++) {
	xdrmem_create(&xdrs,
		      token->tokens.tokens_val[i].token_opaque_val,
		      token->tokens.tokens_val[i].token_opaque_len,
		      XDR_DECODE);
	/* Take a peak at the discriminator. */
	if (!xdr_enum(&xdrs, &type)) {
	    type = -1;
	}
	xdr_destroy(&xdrs);

	if (type == targetType)
	    return i;
    }
    return -1;
}

/* XDR encode a token union structure, and return data and length information
 * suitable for stuffing into a token_opaque structure
 */
static int
encodeTokenUnion(struct ktc_tokenUnion *token,
		 char **dataPtr, size_t *lenPtr) {
    char *data = NULL;
    size_t len;
    XDR xdrs;
    int code = 0;

    *dataPtr = NULL;
    *lenPtr = 0;

    xdrlen_create(&xdrs);
    if (!xdr_ktc_tokenUnion(&xdrs, token)) {
	code = EINVAL;
	goto out;
    }

    len = xdr_getpos(&xdrs);
    data = malloc(len);
    if (data == NULL) {
	code = ENOMEM;
	goto out;
    }
    xdr_destroy(&xdrs);

    xdrmem_create(&xdrs, data, len, XDR_ENCODE);
    if (!xdr_ktc_tokenUnion(&xdrs, token)) {
	code = EINVAL;
        goto out;
    }

    *dataPtr = data;
    *lenPtr = len;

out:
    xdr_destroy(&xdrs);
    if (code) {
	if (data)
	    free(data);
    }

    return code;
}

static void
addOpaque(struct ktc_setTokenData *jar, char *data, size_t len)
{
    int entry;

    entry = jar->tokens.tokens_len;
    jar->tokens.tokens_val = realloc(jar->tokens.tokens_val,
				     entry + 1 * sizeof(token_opaque));
    jar->tokens.tokens_len++;
    jar->tokens.tokens_val[entry].token_opaque_val = data;
    jar->tokens.tokens_val[entry].token_opaque_len = len;
}

/*!
 * Extract a specific token element from a unified token structure
 *
 * This routine extracts an afsTokenUnion structure from the tokenData
 * structure used by the SetTokenEx and GetTokenEx pioctls
 *
 * @param[in] token
 * 	A ktc_setTokenData structure containing the token to extract from
 * @param[in] targetType
 * 	The securityClass index of the token to be extracted
 * @param[out] output
 * 	The decoded token. On entry, this must point to a block of memory
 * 	of sufficient size to contain an afsTokenUnion structure. Upon
 * 	completion, this block must be passed to xdr_free(), using the
 * 	xdr_afsTokenUnion xdrproc_t.
 */
int
token_findByType(struct ktc_setTokenData *token,
		 int targetType,
		 struct ktc_tokenUnion *output)
{
    XDR xdrs;
    int entry;
    int code = EINVAL;

    memset(output, 0, sizeof *output);
    entry = findTokenEntry(token, targetType);
    if (entry == -1)
	goto out;

    xdrmem_create(&xdrs,
		  token->tokens.tokens_val[entry].token_opaque_val,
		  token->tokens.tokens_val[entry].token_opaque_len,
		  XDR_DECODE);

    if (!xdr_ktc_tokenUnion(&xdrs, output)) {
	xdr_destroy(&xdrs);
	goto out;
    }

    xdr_destroy(&xdrs);
    if (output->at_type != targetType) {
	xdr_free((xdrproc_t)xdr_ktc_tokenUnion, output);
	goto out;
    }

    code = 0;

out:
    return code;
}

/*!
 * Given an unified token, populate an rxkad token from it
 *
 * This routine populates an rxkad token using information contained
 * in the tokenData structure used by the SetTokenEx and GetTokenEX
 * pioctls.
 *
 * @param[in] token
 * 	The new format token to extract information from.
 * @param[out] rxkadToken
 * 	The old-style rxkad token. This must be a pointer to an existing
 * 	data block of sufficient size
 * @param[out] flags
 * 	The set of token flags
 * @param[out] aclient
 * 	The client owning the token. This must be a pointer to an existing
 * 	data block of sufficient size, or NULL.
 */

int
token_extractRxkad(struct ktc_setTokenData *token,
		   struct ktc_token *rxkadToken,
		   int *flags,
		   struct ktc_principal *aclient)
{
    struct ktc_tokenUnion uToken;
    int code;

    memset(&uToken, 0, sizeof(uToken));
    if (aclient)
        memset(aclient, 0, sizeof(*aclient));

    code = token_findByType(token, AFSTOKEN_UNION_KAD, &uToken);
    if (code)
	return code;

    rxkadToken->kvno = uToken.ktc_tokenUnion_u.at_kad.rk_kvno;
    memcpy(rxkadToken->sessionKey.data,
	   uToken.ktc_tokenUnion_u.at_kad.rk_key, 8);
    rxkadToken->startTime = uToken.ktc_tokenUnion_u.at_kad.rk_begintime;
    rxkadToken->endTime   = uToken.ktc_tokenUnion_u.at_kad.rk_endtime;
    rxkadToken->ticketLen = uToken.ktc_tokenUnion_u.at_kad.rk_ticket.rk_ticket_len;

    if (rxkadToken->ticketLen > MAXKTCTICKETLEN) {
	code = E2BIG;
	goto out;
    }

    memcpy(rxkadToken->ticket,
	   uToken.ktc_tokenUnion_u.at_kad.rk_ticket.rk_ticket_val,
	   rxkadToken->ticketLen);

    if (flags)
	*flags = uToken.ktc_tokenUnion_u.at_kad.rk_primary_flag & ~0x8000;

    if (aclient) {
	strncpy(aclient->cell, token->cell, MAXKTCREALMLEN-1);
	aclient->cell[MAXKTCREALMLEN-1] = '\0';

        if ((rxkadToken->kvno == 999) ||        /* old style bcrypt ticket */
            (rxkadToken->startTime &&   /* new w/ prserver lookup */
             (((rxkadToken->endTime - rxkadToken->startTime) & 1) == 1))) {
	    sprintf(aclient->name, "AFS ID %d",
		    uToken.ktc_tokenUnion_u.at_kad.rk_viceid);
	} else {
	    sprintf(aclient->name, "Unix UID %d",
		    uToken.ktc_tokenUnion_u.at_kad.rk_viceid);
	}
    }

out:
    xdr_free((xdrproc_t) xdr_ktc_tokenUnion, &uToken);
    return code;
}

struct ktc_setTokenData *
token_buildTokenJar(char * cellname) {
    struct ktc_setTokenData *jar;

    jar = malloc(sizeof(struct ktc_setTokenData));
    if (jar == NULL)
	return NULL;

    memset(jar, 0, sizeof(struct ktc_setTokenData));

    jar->cell = strdup(cellname);

    return jar;
}

/*!
 * Add a token to an existing set of tokens. This will always add the token,
 * regardless of whether an entry for the security class already exists
 */
int
token_addToken(struct ktc_setTokenData *jar, struct ktc_tokenUnion *token) {
    int code;
    char *data;
    size_t len;

    code = encodeTokenUnion(token, &data, &len);
    if (code)
	goto out;

    addOpaque(jar, data, len);

out:
    return code;
}

/*!
 * Replace at token in an existing set of tokens. This replaces the first
 * token stored of a matching type. If no matching tokens are found, then
 * the new token is added at the end of the list
 */
int
token_replaceToken(struct ktc_setTokenData *jar,
		   struct ktc_tokenUnion *token) {
    int entry;
    char *data;
    size_t len;
    int code;

    entry = findTokenEntry(jar, token->at_type);
    if (entry == -1)
	return token_addToken(jar, token);

    code = encodeTokenUnion(token, &data, &len);
    if (code)
	goto out;

    free(jar->tokens.tokens_val[entry].token_opaque_val);
    jar->tokens.tokens_val[entry].token_opaque_val = data;
    jar->tokens.tokens_val[entry].token_opaque_len = len;

out:
    return code;
}

void
token_setPag(struct ktc_setTokenData *jar, int setpag) {
    if (setpag)
	jar->flags |= AFSTOKEN_EX_SETPAG;
    else
	jar->flags &= ~AFSTOKEN_EX_SETPAG;
}
