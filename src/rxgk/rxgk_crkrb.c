/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Heimdal implementation of the rxgk wire encryption
 */

#include "rxgk_locl.h"
#include <errno.h>

struct _rxg_key_type {
    char *name;
    int enctype;
    int blocklen;
    int checksumlen;
    int confounderlen;
};

static struct _rxg_key_type ktypes[] = {
    { "des-cbc-crc", RXGK_CRYPTO_DES_CBC_CRC,
      8, 4, 8,
    },
    { "des-cbc-md5", RXGK_CRYPTO_DES_CBC_MD5,
      8, 24, 8,
    }
};

static struct _rxg_key_type *
_rxg_find_enctype(int enctype)
{
    struct _rxg_key_type *key;

    for (key = ktypes; key->name != NULL; key++)
	if (key->enctype == enctype)
	    return key;
    return NULL;
}

int
rxgk_set_conn(struct rx_connection *con, int enctype, int enc)
{
    struct _rxg_key_type *key;

    key = _rxg_find_enctype(enctype);
    if (key == NULL)
	return ENOENT;

    if (enc) {
	rx_SetSecurityHeaderSize(con, key->checksumlen + key->confounderlen +
				 RXGK_HEADER_DATA_SIZE);

	rx_SetSecurityMaxTrailerSize(con, key->blocklen);
    } else {
	rx_SetSecurityHeaderSize(con, 
				 key->checksumlen + RXGK_HEADER_DATA_SIZE);
	rx_SetSecurityMaxTrailerSize(con, 0);
    }
    return 0;
}

/*
 *
 */

static int
uiomove_to(struct rx_packet *pkt, u_int pre, u_int off, void **p, u_int *rlen)
{
    u_int len;
    void *ptr;

    len = rx_GetDataSize(pkt);
    *rlen = len + pre;

    ptr = malloc(*rlen);
    if (ptr == NULL)
	return ENOMEM;

    *p = ptr;

    ptr = (char *)ptr + pre;

    if (rx_SlowReadPacket(pkt, off, len, ptr) != len) {
	free(p);
	*p = NULL;
	return RXGKPACKETSHORT;
    }
    
    return 0;
}

/*
 *
 */

static int
uiomove_from(struct rx_packet *pkt, u_int off, void *ptr, u_int len)
{
    if (rx_SlowWritePacket(pkt, off, len, ptr) != len)
	return RXGKPACKETSHORT;
    rx_SetDataSize(pkt, len + off);

    return 0;
}

/*
 *
 */
int
rxgk_prepare_packet(struct rx_packet *pkt, struct rx_connection *con,
		    int level, key_stuff *k, end_stuff *e)
{
    int ret, keyusage;
    
    

    if (k->ks_scrypto == NULL)
	return RXGKSEALEDINCON;

    if (level == rxgk_crypt) {
	krb5_data data;
	struct rxgk_header_data hdr;
	u_int len;
	void *p;

	if (rx_IsClientConn(con))
	    keyusage = RXGK_CLIENT_ENC_PACKETS;
	else
	    keyusage = RXGK_SERVER_ENC_PACKETS;

	ret = uiomove_to(pkt, RXGK_HEADER_DATA_SIZE, 
			 rx_GetSecurityHeaderSize(con),
			 &p, &len);
	if (ret)
	    return ret;

	rxgk_getheader(pkt, &hdr);
	memcpy(p, &hdr, sizeof(hdr));

	ret = krb5_encrypt(k->ks_context, k->ks_scrypto, 
			   keyusage, p, len, &data);
	if (ret) {
	    free(p);
	    return ret;
	}

	ret = uiomove_from(pkt, 0, data.data, data.length);

	krb5_data_free(&data);
	free(p);

    } else if (level == rxgk_auth) {
	if (rx_IsClientConn(con))
	    keyusage = RXGK_CLIENT_CKSUM_PACKETS;
	else
	    keyusage = RXGK_SERVER_CKSUM_PACKETS;

	abort();
    } else
	abort();

    return ret;
}

/*
 *
 */
int
rxgk_check_packet(struct rx_packet *pkt, struct rx_connection *con,
		  int level, key_stuff *k, end_stuff *e)
{
    int ret, keyusage;
    
    if (k->ks_scrypto == NULL)
	return RXGKSEALEDINCON;

    ret = 0;

    if (level == rxgk_crypt) {
	krb5_data data;
	struct rxgk_header_data hdr;
	u_int len;
	void *p;

	if (rx_IsClientConn(con))
	    keyusage = RXGK_SERVER_ENC_PACKETS;
	else
	    keyusage = RXGK_CLIENT_ENC_PACKETS;

	ret = uiomove_to(pkt, 0, 0, &p, &len);
	if (ret)
	    return ret;

	ret = krb5_decrypt(k->ks_context, k->ks_scrypto, 
			   keyusage, p, len, &data);
	if (ret) {
	    free(p);
	    return ret;
	}

	ret = uiomove_from(pkt, rx_GetSecurityHeaderSize(con) - 
			   RXGK_HEADER_DATA_SIZE,
			   data.data, data.length);
	if (ret == 0) {
	    rxgk_getheader(pkt, &hdr);
	    if (memcmp(&hdr, data.data, sizeof(hdr)) != 0)
		ret = RXGKSEALEDINCON;
	}

	krb5_data_free(&data);
	free(p);
    } else if (level == rxgk_auth) {
	if (rx_IsClientConn(con))
	    keyusage = RXGK_SERVER_CKSUM_PACKETS;
	else
	    keyusage = RXGK_CLIENT_CKSUM_PACKETS;


	abort();
    } else
	abort();

    return ret;
}
