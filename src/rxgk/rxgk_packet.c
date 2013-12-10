/* rxgk/rxgk_packet.c - packet-manipulating routines for rxgk */
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
 * Routines to encrypt or checksum packets, and perform the reverse
 * decryption and checksum verification operations.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>

#include <rx/rx.h>
#include <rx/rx_packet.h>
#include <rx/rxgk.h>
#ifdef KERNEL
# include "afs/sysincludes.h"
# include "afsincludes.h"
#else
# include <errno.h>
#endif

#include "rxgk_private.h"

/**
 * Fill in an rxgk_header structure from a packet
 *
 * Fill in the elements of the rxgk_header structure, in network byte order,
 * using information from the packet structure and the supplied values for
 * the security index and data length.
 *
 * @param[out] header	The header structure to be populated.
 * @param[in] apacket	The packet from which to pull connection information.
 * @param[in] index	The security index of the connection.
 * @param[in] length	The (plaintext) data length of the packet.
 */
static void
populate_header(struct rxgk_header *header, struct rx_packet *apacket,
		afs_int32 index, afs_uint32 length)
{
    header->epoch = htonl(apacket->header.epoch);
    header->cid = htonl(apacket->header.cid);
    header->callNumber = htonl(apacket->header.callNumber);
    header->seq = htonl(apacket->header.seq);
    header->index = htonl(index);
    header->length = htonl(length);
}

/**
 * Verify the MIC on a packet
 *
 * Take a packet, extract the MIC and data payload, prefix the data with the
 * rxgk pseudoheader, and verify the mic of that assembly.  The plaintext
 * data remains at its present location in the packet.
 *
 * @param[in] tk	The transport key to be used.
 * @param[in] keyusage	The key usage value used to generate the MIC.
 * @param[in] aconn	The rx connection on which the packet was received.
 * @param[in,out] apacket	The packet to be processed.
 * @return rxgk error codes.  An error is returned if the MIC is invalid.
 */
int
rxgk_check_mic_packet(rxgk_key tk, afs_int32 keyusage,
		      struct rx_connection *aconn, struct rx_packet *apacket)
{
    struct rx_opaque plain = RX_EMPTY_OPAQUE, mic = RX_EMPTY_OPAQUE;
    struct rxgk_header *header;
    afs_int32 ret;
    afs_uint32 len;
    size_t miclen;

    miclen = rx_GetSecurityHeaderSize(aconn);
    if (miclen == 0) {
        ret = RXGK_INCONSISTENCY;
        goto done;
    }
    if (rx_GetDataSize(apacket) < miclen) {
	ret = RXGK_PACKETSHORT;
        goto done;
    }
    len = rx_GetDataSize(apacket) - miclen;
    ret = rx_opaque_alloc(&plain, sizeof(*header) + len);
    if (ret != 0)
	goto done;
    header = plain.val;
    ret = rx_opaque_alloc(&mic, miclen);
    if (ret != 0)
	goto done;
    populate_header(header, apacket, rx_SecurityClassOf(aconn), len);
    rx_packetread(apacket, 0, miclen, mic.val);
    rx_packetread(apacket, miclen, len,
		  (unsigned char *)plain.val + sizeof(*header));

    /* The actual crypto call */
    ret = rxgk_check_mic_in_key(tk, keyusage, &plain, &mic);

    /*
     * We need to tell Rx how many bytes of payload there are in the packet
     * before returning. We haven't touched the payload, so the amount of
     * payload is just the original data size of packet, minus our overhead for
     * the mic. This is just 'len', calculated above.
     */
    rx_SetDataSize(apacket, len);

 done:
    rx_opaque_freeContents(&plain);
    rx_opaque_freeContents(&mic);
    return ret;
}

/**
 * Decrypt a packet to plaintext
 *
 * Take an encrypted packet and decrypt it with the specified key and
 * key usage.  Put the plaintext back in the packet.
 *
 * @param[in] tk	The transport key to use.
 * @param[in] keyusage	The key usage used to encrypt the packet.
 * @param[in] aconn	The rx connection on which the packet was received.
 * @param[in,out] apacket	The packet being decrypted.
 * @return rxgk and system error codes.
 */
int
rxgk_decrypt_packet(rxgk_key tk, afs_int32 keyusage,
		    struct rx_connection *aconn, struct rx_packet *apacket)
{
    struct rx_opaque plain = RX_EMPTY_OPAQUE, crypt = RX_EMPTY_OPAQUE;
    struct rxgk_header *header = NULL, *cryptheader;
    afs_int32 ret;
    afs_uint32 len;

    len = rx_GetDataSize(apacket);

    if (rx_GetSecurityHeaderSize(aconn) != sizeof(*header)) {
        ret = RXGK_INCONSISTENCY;
        goto done;
    }

    header = rxi_Alloc(sizeof(*header));
    if (header == NULL) {
	ret = ENOMEM;
        goto done;
    }
    ret = rx_opaque_alloc(&crypt, len);
    if (ret != 0)
	goto done;
    rx_packetread(apacket, 0, len, crypt.val);

    /* The actual decryption */
    ret = rxgk_decrypt_in_key(tk, keyusage, &crypt, &plain);
    if (ret != 0)
	goto done;

    if (plain.len < sizeof(*cryptheader)) {
        /*
	 * Our decrypted contents must have an rxgk_header at the start. If we
         * don't even have enough bytes for an rxgk_header, something is
         * clearly wrong.
	 */
        ret = RXGK_DATA_LEN;
        goto done;
    }
    cryptheader = plain.val;

    /*
     * cryptheader->length indicates the length of the decrypted plaintext of
     * this packet. We must rely on this value, and we cannot calculate it
     * ourselves from e.g. plain.len, since the encryption routines may have
     * padded the plaintext. However, we also must not blindly trust the
     * cryptheader->length value, since if it is set to larger than the actual
     * plaintext length, we could read beyond the end of the buffer in
     * plain.val. So check that cryptheader->length is not larger than the max
     * size of the plaintext in our decrypted buffer.
     */
    if (ntohl(cryptheader->length) > plain.len - sizeof(*cryptheader)) {
        ret = RXGK_DATA_LEN;
        goto done;
    }

    populate_header(header, apacket, rx_SecurityClassOf(aconn),
                    ntohl(cryptheader->length));

    /* Verify the encrypted header. Note the constant-time memcmp here, since
     * this is a security-sensitive comparison. */
    ret = ct_memcmp(header, cryptheader, sizeof(*header));
    if (ret != 0) {
	ret = RXGK_SEALED_INCON;
	goto done;
    }

    len = ntohl(cryptheader->length) + sizeof(*header);
    if (len > 0xffffu) {
	ret = RXGK_DATA_LEN;
	goto done;
    }

    /*
     * Now, write the plain data back to the packet. Note that we write back
     * the decrypted packet payload, and the rxgk_header contents. We don't
     * really need the rxgk_header in there anymore, but the Rx API does not
     * let us skip it (Rx will skip the first N bytes when reading the packet
     * payload, where N is our security header size).
     */
    rx_packetwrite(apacket, 0, len, plain.val);
    rx_SetDataSize(apacket, ntohl(cryptheader->length));

 done:
    rx_opaque_freeContents(&plain);
    rx_opaque_freeContents(&crypt);
    rxi_Free(header, sizeof(*header));
    return ret;
}

/**
 * Compute the MIC of a packet using a given key and key usage
 *
 * Take a packet, prefix it with the rxgk pseudoheader, MIC the whole
 * thing with specified key and key usage, then insert the mic into the
 * packet payload before the actual data.
 *
 * @param[in] tk	The transport key to use.
 * @param[in] keyusage	The key usage to use for the MIC.
 * @param[in] aconn	The rx connection on which the packet will be sent.
 * @param[in,out] apacket	The packet whose MIC is being calculated.
 * @return rxgk error codes.
 */
int
rxgk_mic_packet(rxgk_key tk, afs_int32 keyusage, struct rx_connection *aconn,
		struct rx_packet *apacket)
{
    struct rx_opaque plain = RX_EMPTY_OPAQUE, mic = RX_EMPTY_OPAQUE;
    struct rxgk_header *header;
    afs_uint32 len, miclen;
    int ret;

    /* This 'len' does NOT include our overhead for the mic */
    len = rx_GetDataSize(apacket);
    miclen = rx_GetSecurityHeaderSize(aconn);
    ret = rx_opaque_alloc(&plain, sizeof(*header) + len);
    if (ret != 0)
	goto done;
    header = plain.val;
    populate_header(header, apacket, rx_SecurityClassOf(aconn), len);
    rx_packetread(apacket, miclen, len,
		  (unsigned char *)plain.val + sizeof(*header));

    /* The actual mic */
    ret = rxgk_mic_in_key(tk, keyusage, &plain, &mic);
    if (ret != 0)
	goto done;

    if (mic.len != miclen) {
	ret = RXGK_INCONSISTENCY;
	goto done;
    }

    rx_packetwrite(apacket, 0, mic.len, mic.val);
    rx_SetDataSize(apacket, mic.len + len);

 done:
    rx_opaque_freeContents(&plain);
    rx_opaque_freeContents(&mic);
    return ret;
}

/**
 * Encrypt a packet using a given key and key usage
 *
 * Take a packet, prefix it with the rxgk pseudoheader, encrypt the whole
 * thing with specified key and key usage, then rewrite the packet payload
 * to be the encrypted version.
 *
 * @param[in] tk	The transport key to use.
 * @param[in] keyusage	The key usage for the encryption.
 * @param[in] aconn	The rx connection on which the packet will be sent.
 * @param[in,out] apacket	The packet being encrypted.
 * @return rxgk error codes.
 */
int
rxgk_enc_packet(rxgk_key tk, afs_int32 keyusage, struct rx_connection *aconn,
		struct rx_packet *apacket)
{
    struct rx_opaque plain = RX_EMPTY_OPAQUE, crypt = RX_EMPTY_OPAQUE;
    struct rxgk_header *header;
    afs_int32 ret;
    afs_uint32 len;

    len = rx_GetDataSize(apacket);
    if (rx_GetSecurityHeaderSize(aconn) != sizeof(*header)) {
        ret = RXGK_INCONSISTENCY;
        goto done;
    }
    ret = rx_opaque_alloc(&plain, sizeof(*header) + len);
    if (ret != 0)
	goto done;
    header = plain.val;
    populate_header(header, apacket, rx_SecurityClassOf(aconn), len);
    rx_packetread(apacket, sizeof(*header), len,
		  (unsigned char *)plain.val + sizeof(*header));

    /* The actual encryption */
    ret = rxgk_encrypt_in_key(tk, keyusage, &plain, &crypt);
    if (ret != 0)
	goto done;
    if (crypt.len > 0xffffu) {
	ret = RXGK_DATA_LEN;
	goto done;
    }

    /* Now, put the data back. */
    if (crypt.len > plain.len)
        rxi_RoundUpPacket(apacket, crypt.len - plain.len);
    rx_packetwrite(apacket, 0, crypt.len, crypt.val);
    rx_SetDataSize(apacket, crypt.len);

 done:
    rx_opaque_freeContents(&plain);
    rx_opaque_freeContents(&crypt);
    return ret;
}
