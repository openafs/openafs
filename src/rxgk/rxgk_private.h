/* src/rxgk/rxgk_private.h - Declarations of RXGK-internal routines */
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
 * Prototypes for routines internal to RXGK.
 */

#ifndef RXGK_PRIVATE_H
#define RXGK_PRIVATE_H

/** Statistics about a connection.  Bytes and packets sent/received. */
struct rxgkStats {
    afs_uint32 brecv;
    afs_uint32 bsent;
    afs_uint32 precv;
    afs_uint32 psent;
};

/* The packet pseudoheader used for auth and crypt connections. */
struct rxgk_header {
    afs_uint32 epoch;
    afs_uint32 cid;
    afs_uint32 callNumber;
    afs_uint32 seq;
    afs_uint32 index;
    afs_uint32 length;
} __attribute__((packed));

/*
 * rgxk_server.c
 */

/**
 * Security Object private data for the server.
 *
 * Per-connection flags, and a way to get a decryption key for what the client
 * sends us.
 */
struct rxgk_sprivate {
    void *rock;
    rxgk_getkey_func getkey;
};
/**
 * Per-connection security data for the server.
 *
 * Security level, authentication state, expiration, the current challenge
 * nonce, status, the connection start time and current key derivation key
 * number.  Cache both the user identity and callback identity presented
 * in the token, for later use.
 */
struct rxgk_sconn {
    RXGK_Level level;
    unsigned char auth;
    unsigned char challenge_valid;
    rxgkTime expiration;
    unsigned char challenge[RXGK_CHALLENGE_NONCE_LEN];
    struct rxgkStats stats;
    rxgkTime start_time;
    struct rx_identity *client;
    afs_uint32 key_number;
    rxgk_key k0;
};

/*
 * rxgk_client.c
 */

/**
 * Security Object private data for client.
 *
 * The session key ("token master key"), plust the enctype of the
 * token and the token itself.
 * UUIDs for both the client (cache manager) and target server.  This is
 * doable because the token is either a db server (the target has no UUID)
 * or tied to a particular file server (which does have a UUID).
 */
struct rxgk_cprivate {
    rxgk_key k0;
    afs_int32 enctype;
    RXGK_Level level;
    RXGK_Data token;
};
/**
 * Per-connection security data for client.
 *
 * The start time of the connection and connection key number are used
 * for key derivation, information about the callback key to be presented in
 * the authenticator for the connection, and the requisite connection
 * statistics.
 */
struct rxgk_cconn {
    rxgkTime start_time;
    afs_uint32 key_number;
    struct rxgkStats stats;
};

/* rxgk_crypto_IMPL.c (currently rfc3961 is the only IMPL) */
ssize_t rxgk_etype_to_len(int etype);

/* rxgk_token.c */
afs_int32 rxgk_extract_token(RXGK_Data *tc, RXGK_Token *out,
			     rxgk_getkey_func getkey, void *rock)
			    AFS_NONNULL((1,2,3));

/* rxgk_util.c */
afs_int32 rxgk_security_overhead(struct rx_connection *aconn, RXGK_Level level,
				 rxgk_key k0);
afs_int32 rxgk_key_number(afs_uint16 wire, afs_uint32 local, afs_uint32 *real);

/* rxgk_packet.c */
int rxgk_mic_packet(rxgk_key tk, afs_int32 keyusage,
		    struct rx_connection *aconn, struct rx_packet *apacket);
int rxgk_enc_packet(rxgk_key tk, afs_int32 keyusage,
		    struct rx_connection *aconn, struct rx_packet *apacket);
int rxgk_check_packet(int server, struct rx_connection *aconn,
                      struct rx_packet *apacket, RXGK_Level level,
                      rxgkTime start_time, afs_uint32 *a_kvno, rxgk_key k0);

#endif /* RXGK_PRIVATE_H */
