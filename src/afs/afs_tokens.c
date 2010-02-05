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
afs_FindToken(struct tokenJar *tokens, rx_securityIndex type) {
    while (tokens != NULL) {
	if (tokens->type == type) {
	    return &tokens->u;
	}
	tokens = tokens->next;
    }
    return NULL;
}

/*!
 * Free a single token
 *
 * This will free the given token. No attempt is made to unlink
 * the token from its container, and it is an error to attempt to
 * free a token which is still linked.
 *
 * This performs a secure free, setting all token information to 0
 * before returning allocated data blocks to the kernel.
 *
 * Intended primarily for internal use.
 *
 * @param[in] token
 * 	The token to free
 */

void
afs_FreeOneToken(struct tokenJar *token) {
    if (token->next != NULL)
	osi_Panic("Freeing linked token");

    switch (token->type) {
      case RX_SECIDX_KAD:
	if (token->u.rxkad.ticket != NULL) {
		memset(token->u.rxkad.ticket, 0, token->u.rxkad.ticketLen);
		afs_osi_Free(token->u.rxkad.ticket,
			     token->u.rxkad.ticketLen);
	}
	break;
      default:
	break;
    }
    memset(token, 0, sizeof(struct tokenJar));
    afs_osi_Free(token, sizeof(struct tokenJar));
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
afs_FreeTokens(struct tokenJar **tokenPtr) {
    struct tokenJar *next, *tokens;

    tokens = *tokenPtr;
    *tokenPtr = NULL;
    while(tokens != NULL) {
	next = tokens->next;
	tokens->next = NULL; /* Unlink from chain */
	afs_FreeOneToken(tokens);
	tokens = next;
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
afs_AddToken(struct tokenJar **tokens, rx_securityIndex type) {
    struct tokenJar *newToken;

    newToken = afs_osi_Alloc(sizeof(struct tokenJar));
    memset(newToken, 0, sizeof(*newToken));

    newToken->type = type;
    newToken->next = *tokens;
    *tokens = newToken;

    return &newToken->u;
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
int
afs_IsTokenExpired(struct tokenJar *token, afs_int32 now) {
    switch (token->type) {
      case RX_SECIDX_KAD:
	if (token->u.rxkad.clearToken.EndTimestamp < now - NOTOKTIMEOUT)
	    return 1;
	break;
      default:
	return 0;
    }
    return 0; /* not reached, but keep gcc happy */
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
int
afs_IsTokenUsable(struct tokenJar *token, afs_int32 now) {

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
afs_DiscardExpiredTokens(struct tokenJar **tokenPtr, afs_int32 now) {
    struct tokenJar *next;

    while (*tokenPtr != NULL) {
	if (afs_IsTokenExpired(*tokenPtr, now)) {
	    next = (*tokenPtr)->next;
	    (*tokenPtr)->next = NULL;
	    afs_FreeOneToken(*tokenPtr);
	    *tokenPtr = next;
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
afs_HasUsableTokens(struct tokenJar *token, afs_int32 now) {
    while (token != NULL) {
        if (afs_IsTokenUsable(token, now))
	    return 1;
	token = token->next;
    }
    return 0;
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
		  struct ClearToken *clearToken) {
    union tokenUnion *tokenU;
    struct rxkadToken *rxkad;

    tokenU = afs_AddToken(tokens, RX_SECIDX_KAD);
    rxkad = &tokenU->rxkad;

    rxkad->ticket = afs_osi_Alloc(ticketLen);
    rxkad->ticketLen = ticketLen;
    memcpy(rxkad->ticket, ticket, ticketLen);
    rxkad->clearToken = *clearToken;
}

