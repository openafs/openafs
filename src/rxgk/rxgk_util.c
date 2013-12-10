/* rxgk/rxgk_util.c - utility functions for RXGK use */
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
 * Utility functions for RXGK use. Compute the security overhead for a
 * connection at a given security level, and helpers for maintaining key
 * version numbers for connections.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <rx/rx.h>
#include <rx/rx_identity.h>
#include <rx/rxgk.h>
#include <rx/rx_packet.h>
#include <afs/rfc3961.h>
#ifdef KERNEL
# include "afs/sysincludes.h"
# include "afsincludes.h"
#else
# include <errno.h>
#endif

#include "rxgk_private.h"

/**
 * Set the security header and trailer sizes on a connection
 *
 * Set the security header and trailer sizes on aconn to be consistent
 * with the space needed for packet handling at the given security level
 * using the given key (only its enctype/checksum type are relevant).
 *
 * @param[out] aconn	The connection being modified.
 * @param[in] level	The security level of the connection.
 * @param[in] k0	The master key for the connection.
 * @return rxgk error codes.
 */
afs_int32
rxgk_security_overhead(struct rx_connection *aconn, RXGK_Level level,
		       rxgk_key k0)
{
    afs_int32 ret;
    size_t mlen;
    afs_uint32 elen;

    switch (level) {
	case RXGK_LEVEL_CLEAR:
	    return 0;
	case RXGK_LEVEL_AUTH:
	    ret = rxgk_mic_length(k0, &mlen);
	    if (ret != 0)
		return ret;
	    rx_SetSecurityHeaderSize(aconn, mlen);
	    rx_SetSecurityMaxTrailerSize(aconn, 0);
	    return 0;
	case RXGK_LEVEL_CRYPT:
	    ret = rxgk_cipher_expansion(k0, &elen);
	    if (ret != 0)
		return ret;
	    rx_SetSecurityHeaderSize(aconn, sizeof(struct rxgk_header));
	    rx_SetSecurityMaxTrailerSize(aconn, elen);
	    return 0;
	default:
	    return RXGK_INCONSISTENCY;
    }
}

/**
 * Compute the full 32-bit kvno of a connection
 *
 * Given the 16-bit wire kvno and the local state, return the actual kvno which
 * should be used for key derivation. All values are in host byte order.
 * Understanding how we derive a 32-bit kvno from a 16-bit value requires some
 * explanation:
 *
 * After an rxgk conn is set up, our peer informs us of kvno changes by sending
 * the lowest 16 bits of its kvno. The real kvno being used is a 32-bit value,
 * but the peer cannot change the kvno arbitrarily; the protocol spec only
 * allows a peer to change the kvno by incrementing or decrementing it by 1. So
 * if we know the current 32-bit kvno ('local'), and we know the advertised
 * lower 16 bits of the new kvno ('wire'), we can calculate the new 32-bit kvno
 * ('*real').
 *
 * @param[in] wire	The 16-bit kvno from the received packet.
 * @param[in] local	The 32-bit kvno from the local connection state.
 * @param[out] real	The kvno to be used to process this packet.
 * @return rxgk error codes.
 */
afs_int32
rxgk_key_number(afs_uint16 wire, afs_uint32 local, afs_uint32 *real)
{
    afs_uint16 lres, diff;

    lres = local % (1u << 16);
    diff = (afs_uint16)(wire - lres);

    if (diff == 0) {
	*real = local;
    } else if (diff == 1) {
	/* Our peer is using a kvno 1 higher than 'local' */
	if (local == MAX_AFS_UINT32)
	    return RXGK_INCONSISTENCY;
	*real = local + 1;

    } else if (diff == (afs_uint16)0xffffu) {
	/* Our peer is using a kvno 1 lower than 'local' */
	if (local == 0)
	    return RXGK_INCONSISTENCY;
	*real = local - 1;

    } else {
	return RXGK_BADKEYNO;
    }
    return 0;
}
