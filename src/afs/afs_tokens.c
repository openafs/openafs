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

#include <afsconfig.h>
#include "afs/param.h"
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "token.h"

/* A jar for storing tokens in */

/*!
 * Return a token of the specified type from the selected tokenjar.
 *
 * @param[in] tokens
 * 	The tokenjar in which to search
 * @param[in] type
 * 	The type of token to return
 *
 * @return
 * 	A tokenUnion structure, from which the desired token can be
 * 	accessed using the appropriate element of the union.
 */
union tokenUnion *
afs_FindToken(struct tokenJar *tokens, rx_securityIndex type)
{
    while (tokens != NULL) {
	if (tokens->type == type) {
	    return &tokens->content;
	}
	tokens = tokens->next;
    }
    return NULL;
}

/*!
 * Unlink and free a single token
 *
 * This will unlink the first token in the given tokenJar, and free that token.
 * This attempts to perform a secure free, setting all token information to 0
 * before returning allocated data blocks to the kernel.  (Optimizing compilers
 * may eliminate such a "dead store", though.)
 *
 * Intended primarily for internal use.
 *
 * @param[inout] tokenPtr
 * 	The token to unlink and free
 */
static void
afs_FreeFirstToken(struct tokenJar **tokenPtr)
{
    struct tokenJar *token = *tokenPtr;
    if (token == NULL) {
	return;
    }

    /* Unlink the token. */
    *tokenPtr = token->next;
    token->next = NULL;

    switch (token->type) {
      case RX_SECIDX_KAD:
	if (token->content.rxkad.ticket != NULL) {
	    memset(token->content.rxkad.ticket, 0, token->content.rxkad.ticketLen);
	    afs_osi_Free(token->content.rxkad.ticket,
			 token->content.rxkad.ticketLen);
	}
	break;
      default:
	break;
    }
    memset(token, 0, sizeof(*token));
    afs_osi_Free(token, sizeof(*token));
}

/*!
 * Free a token jar
 *
 * Free all of the tokens in a given token jar. This will also set the
 * pointer to the jar to NULL, to indicate that it has been freed.
 *
 * @param[in] tokenPtr
 * 	A pointer to the address of the tokenjar to free.
 */
void
afs_FreeTokens(struct tokenJar **tokenPtr)
{
    while (*tokenPtr != NULL) {
	afs_FreeFirstToken(tokenPtr);
    }
}

/*!
 * Add a token to a token jar
 *
 * Add a new token to a token jar. If the jar already exists,
 * then this token becomes the first in the jar. If it doesn't
 * exist, then a new jar is created. The contents of the new
 * token are initialised to 0 upon creation.
 *
 * @param[in] tokens
 * 	A pointer to the address of the token jar to populate
 * @param[in] type
 * 	The type of token to create
 *
 * @return
 * 	A pointer to the tokenUnion of the newly created token,
 * 	which may then be used to populate the token.
 */
union tokenUnion *
afs_AddToken(struct tokenJar **tokens, rx_securityIndex type)
{
    struct tokenJar *newToken;

    newToken = afs_osi_Alloc(sizeof(*newToken));
    osi_Assert(newToken != NULL);
    memset(newToken, 0, sizeof(*newToken));

    newToken->type = type;
    newToken->next = *tokens;
    *tokens = newToken;

    return &newToken->content;
}

/*!
 * Indicate if a single token is expired
 *
 * @param[in] token
 * 	The token to check
 * @param[in] now
 * 	The time to check against for expiry (typically the results of
 * 	calling osi_Time())
 *
 * @returns
 * 	True if the token has expired, false otherwise
 */
static int
afs_IsTokenExpired(struct tokenJar *token, afs_int32 now)
{
    switch (token->type) {
      case RX_SECIDX_KAD:
	if (token->content.rxkad.clearToken.EndTimestamp < now - NOTOKTIMEOUT)
	    return 1;
	break;
      default:
	return 0;
    }
    return 0;
}

/*!
 * Indicate if a token is usable by the kernel module
 *
 * This determines whether a token is usable. A usable token is one that
 * has not expired, and which is otherwise suitable for use.
 *
 * @param[in] token
 * 	The token to check
 * @param[in] now
 * 	The time to use for the expiry check
 *
 * @returns
 * 	True if the token is usable, false otherwise
 */
static int
afs_IsTokenUsable(struct tokenJar *token, afs_int32 now)
{

    if (afs_IsTokenExpired(token, now))
	return 0;

    switch (token->type) {
      case RX_SECIDX_KAD:
	/* We assume that all non-expired rxkad tokens are usable by us */
	return 1;
      default :
	return 0;
    }
}

/*!
 * Discard all expired tokens from a token jar
 *
 * This permanently removes all tokens which have expired from the token
 * jar. Note that tokens which are not usable, but which have not expired,
 * will not be deleted.
 *
 * @param[in] tokenPtr
 * 	A pointer to the address of the token jar to check
 * @param[in] now
 * 	The time to use for the expiry check
 */

void
afs_DiscardExpiredTokens(struct tokenJar **tokenPtr, afs_int32 now)
{
    while (*tokenPtr != NULL) {
	if (afs_IsTokenExpired(*tokenPtr, now)) {
	    afs_FreeFirstToken(tokenPtr);
	} else {
	    tokenPtr = &(*tokenPtr)->next;
        }
    }
}

/*!
 * Indicate whether a token jar contains one, or more usable tokens
 *
 * @param[in] token
 * 	The token jar to check
 * @param[in] now
 * 	The cime to use for the expiry check
 *
 * @returns
 * 	True if the jar contains usable tokens, otherwise false
 */
int
afs_HasUsableTokens(struct tokenJar *token, afs_int32 now)
{
    while (token != NULL) {
        if (afs_IsTokenUsable(token, now))
	    return 1;
	token = token->next;
    }
    return 0;
}

/*!
 * Indicate whether a token jar contains a valid (non-expired) token
 *
 * @param[in] token
 * 	The token jar to check
 * @param[in] now
 * 	The time to use for the expiry check
 *
 * @returns
 * 	True if the jar contains valid tokens, otherwise false
 *
 */
int
afs_HasValidTokens(struct tokenJar *token, afs_int32 now)
{
    while (token != NULL) {
        if (!afs_IsTokenExpired(token, now))
	    return 1;
	token = token->next;
    }
    return 0;
}

/*!
 * Count the number of valid tokens in a jar. A valid token is
 * one which is not expired - note that valid tokens may not be
 * usable by the kernel.
 *
 * @param[in] token
 * 	The token jar to check
 * @param[in] now
 * 	The time to use for the expiry check
 *
 * @returns
 * 	The number of valid tokens in the jar
 */
static int
countValidTokens(struct tokenJar *token, time_t now)
{
    int count = 0;

    while (token != NULL) {
        if (!afs_IsTokenExpired(token, now))
	    count ++;
	token = token->next;
    }
    return count;
}

/*!
 * Add an rxkad token to the token jar
 *
 * @param[in] tokens
 * 	A pointer to the address of the jar to add the token to
 * @param[in] ticket
 * 	A data block containing the token's opaque ticket
 * @param[in] ticketLen
 * 	The length of the ticket data block
 * @param[in] clearToken
 * 	The cleartext token information
 */
void
afs_AddRxkadToken(struct tokenJar **tokens, char *ticket, int ticketLen,
		  struct ClearToken *clearToken)
{
    union tokenUnion *tokenU;
    struct rxkadToken *rxkad;

    tokenU = afs_AddToken(tokens, RX_SECIDX_KAD);
    rxkad = &tokenU->rxkad;

    rxkad->ticket = afs_osi_Alloc(ticketLen);
    osi_Assert(rxkad->ticket != NULL);
    rxkad->ticketLen = ticketLen;
    memcpy(rxkad->ticket, ticket, ticketLen);
    rxkad->clearToken = *clearToken;
}

static int
afs_AddRxkadTokenFromPioctl(struct tokenJar **tokens,
			    struct ktc_tokenUnion *pioctlToken)
{
    struct ClearToken clear;

    clear.AuthHandle = pioctlToken->ktc_tokenUnion_u.at_kad.rk_kvno;
    clear.ViceId = pioctlToken->ktc_tokenUnion_u.at_kad.rk_viceid;
    clear.BeginTimestamp = pioctlToken->ktc_tokenUnion_u.at_kad.rk_begintime;
    clear.EndTimestamp = pioctlToken->ktc_tokenUnion_u.at_kad.rk_endtime;
    memcpy(clear.HandShakeKey, pioctlToken->ktc_tokenUnion_u.at_kad.rk_key, 8);
    afs_AddRxkadToken(tokens,
		      pioctlToken->ktc_tokenUnion_u.at_kad.rk_ticket.rk_ticket_val,
		      pioctlToken->ktc_tokenUnion_u.at_kad.rk_ticket.rk_ticket_len,
		      &clear);

    /* Security means never having to say you're sorry */
    memset(clear.HandShakeKey, 0, 8);

    return 0;
}

static int
rxkad_extractTokenForPioctl(struct tokenJar *token,
			       struct ktc_tokenUnion *pioctlToken)
{

    struct token_rxkad *rxkadPioctl;
    struct rxkadToken *rxkadInternal;

    rxkadPioctl = &pioctlToken->ktc_tokenUnion_u.at_kad;
    rxkadInternal = &token->content.rxkad;

    rxkadPioctl->rk_kvno = rxkadInternal->clearToken.AuthHandle;
    rxkadPioctl->rk_viceid = rxkadInternal->clearToken.ViceId;
    rxkadPioctl->rk_begintime = rxkadInternal->clearToken.BeginTimestamp;
    rxkadPioctl->rk_endtime = rxkadInternal->clearToken.EndTimestamp;
    memcpy(rxkadPioctl->rk_key, rxkadInternal->clearToken.HandShakeKey, 8);

    rxkadPioctl->rk_ticket.rk_ticket_val = xdr_alloc(rxkadInternal->ticketLen);
    if (rxkadPioctl->rk_ticket.rk_ticket_val == NULL)
	return ENOMEM;
    rxkadPioctl->rk_ticket.rk_ticket_len = rxkadInternal->ticketLen;
    memcpy(rxkadPioctl->rk_ticket.rk_ticket_val,
	   rxkadInternal->ticket, rxkadInternal->ticketLen);

    return 0;
}

/*!
 * Add a token to a token jar based on the input from a new-style
 * SetToken pioctl
 *
 * @param[in] tokens
 * 	Pointer to the address of a token jar
 * @param[in] pioctlToken
 *	The token structure obtained through the pioctl (note this
 *	is a single, XDR decoded, token)
 *
 * @returns
 * 	0 on success, an error code on failure
 */
int
afs_AddTokenFromPioctl(struct tokenJar **tokens,
		       struct ktc_tokenUnion *pioctlToken)
{

    switch (pioctlToken->at_type) {
      case RX_SECIDX_KAD:
	return afs_AddRxkadTokenFromPioctl(tokens, pioctlToken);
    }

    return EINVAL;
}

static int
extractPioctlToken(struct tokenJar *token,
		   struct token_opaque *opaque)
{
    XDR xdrs;
    struct ktc_tokenUnion *pioctlToken;
    int code;

    memset(opaque, 0, sizeof(token_opaque));

    pioctlToken = osi_Alloc(sizeof(*pioctlToken));
    if (pioctlToken == NULL)
	return ENOMEM;

    pioctlToken->at_type = token->type;

    switch (token->type) {
      case RX_SECIDX_KAD:
	code = rxkad_extractTokenForPioctl(token, pioctlToken);
	break;
      default:
	code = EINVAL;;
    }

    if (code)
	goto out;

    xdrlen_create(&xdrs);
    if (!xdr_ktc_tokenUnion(&xdrs, pioctlToken)) {
	code = EINVAL;
	xdr_destroy(&xdrs);
	goto out;
    }

    opaque->token_opaque_len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    opaque->token_opaque_val = osi_Alloc(opaque->token_opaque_len);
    if (opaque->token_opaque_val == NULL) {
	code = ENOMEM;
	goto out;
    }

    xdrmem_create(&xdrs,
		  opaque->token_opaque_val,
		  opaque->token_opaque_len,
		  XDR_ENCODE);
    if (!xdr_ktc_tokenUnion(&xdrs, pioctlToken)) {
	code = EINVAL;
	xdr_destroy(&xdrs);
	goto out;
    }
    xdr_destroy(&xdrs);

 out:
    xdr_free((xdrproc_t) xdr_ktc_tokenUnion, pioctlToken);
    osi_Free(pioctlToken, sizeof(*pioctlToken));

    if (code != 0) {
	if (opaque->token_opaque_val != NULL)
	    osi_Free(opaque->token_opaque_val, opaque->token_opaque_len);
	opaque->token_opaque_val = NULL;
	opaque->token_opaque_len = 0;
    }
    return code;
}

int
afs_ExtractTokensForPioctl(struct tokenJar *token,
			   time_t now,
			   struct ktc_setTokenData *tokenSet)
{
    int numTokens, pos;
    int code = 0;

    numTokens = countValidTokens(token, now);

    tokenSet->tokens.tokens_len = numTokens;
    tokenSet->tokens.tokens_val
	= xdr_alloc(sizeof(tokenSet->tokens.tokens_val[0]) * numTokens);

    if (tokenSet->tokens.tokens_val == NULL)
	return ENOMEM;

    pos = 0;
    while (token != NULL && pos < numTokens) {
	code = extractPioctlToken(token, &tokenSet->tokens.tokens_val[pos]);
	if (code)
	    goto out;
	token = token->next;
	pos++;
    }

 out:
    if (code)
	xdr_free((xdrproc_t) xdr_ktc_setTokenData, tokenSet);

    return code;
}
