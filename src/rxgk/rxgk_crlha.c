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

#include "rxgk_locl.h"

#include <openssl/md5.h>
#include <openssl/des.h>

#include <errno.h>

/*
 *	krb5 non-des encrypting:
 *
 * 	+------------+----------+-------+---------+-----+
 *	| confounder | checksum | rxhdr | msg-seq | pad |
 *	+------------+----------+-------+---------+-----+
 *
 *	krb5 non-des checksuming only:
 *
 * 	+----------+-------+---------+
 *	| checksum | rxhdr | msg-seq |
 *	+----------+-------+---------+
 *
 *	XXX THIS SHOULD BE FIXED
 *	so, the checksuming only case includes unnessery data right
 *	now but I don't care since that makes it easier for me to
 *	share code between the two cases.
 *
 */

struct rxg_key_type;

struct rxg_des_keystuff {
    des_cblock key;
    des_key_schedule sched;
    des_key_schedule chksum;
    des_cblock iv[RX_MAXCALLS];
};

struct rxg_key {
    struct rxg_key_type *type;
    rxgk_level level;
    union {
	struct rxg_des_keystuff des;
    } key;
};

#define RXG_MAX_CHECKSUM_SIZE	128

struct rxg_con {
    struct rxg_key key;
    struct rxg_key_type *type;
};

int
rxg_PacketCheckSum(struct rxg_key_type *, struct rx_packet *, 
		   struct rxg_key *, void *, size_t, int);
int
rxg_check_packet(struct rx_packet *pkt,
		 struct rx_connection *con,
		 int clear,
		 struct rxg_con *kc);
int
rxg_prepare_packet(struct rx_packet *pkt,
		   struct rx_connection *con,
		   int clear,
		   struct rxg_con *kc);

static void rxg_des_enc(void *, size_t, struct rxg_key *, void *, int);
static void des_setup_iv(struct rx_packet *, struct rxg_key *, void *);
static void des_prepare_key(struct rxg_key *, void *);
static int checksum_pkt_md5_des(struct rx_packet *, struct rxg_key *, 
				void *, size_t, int);
struct rxg_key_type * rxg_find_enctype(int);

struct rxg_key_type {
    char *name;
    int enctype;
    int keylen;
    int blocklen;
    int checksumlen;
    int confounderlen;
    int ivsize;
    void (*prepare_key)(struct rxg_key *, void *key);
    void (*setup_iv)(struct rx_packet *, struct rxg_key *, void *iv);
    void (*encrypt)(void *, size_t, struct rxg_key *, void *, int);
    int (*cksum_pkt)(struct rx_packet *, struct rxg_key *,void *,size_t,int);
};

static struct rxg_key_type ktypes[] = {
    { "des-cbc-crc", RXGK_CRYPTO_DES_CBC_MD5,
      8, 8, 24, 8, 8,
      des_prepare_key, des_setup_iv, rxg_des_enc, checksum_pkt_md5_des
    }
};

struct rxg_key_type *
rxg_find_enctype(int enctype)
{
    struct rxg_key_type *key;

    for (key = ktypes; key->name != NULL; key++)
	if (key->enctype == enctype)
	    return key;
    return NULL;
}

static void
rxg_des_enc(void *io, size_t sz, struct rxg_key *key, void *iv, int enc)
{
    struct rxg_des_keystuff *ks = &key->key.des;

    assert((sz % 8) == 0);
    des_cbc_encrypt(io, io, sz, ks->sched, iv, enc);
}

static void
des_prepare_key(struct rxg_key *key, void *keym)
{
    struct rxg_des_keystuff *ks;
    des_cblock cksumkey;
    int i;

    ks = &key->key.des;

    memset(ks, 0, sizeof(*ks));

    memcpy(ks->key, keym, sizeof(des_cblock));
    des_set_key(&ks->key, ks->sched);
    memset(ks->iv, 0, sizeof(ks->iv));

    for (i = 0; i < 8; i++)
	cksumkey[i] = ((char *)keym)[i] ^ 0xF0;

    des_set_key(&cksumkey, ks->chksum);
}

static void
des_setup_iv(struct rx_packet *pkt, struct rxg_key *key, void *iv)
{
    memset(iv, 0, sizeof(des_cblock));
}

static void
rxg_random_data(void *ptr, size_t sz)
{
    memset(ptr, 0, sz);
    abort();
}


static int
encrypt_pkt(struct rxg_key_type *kt, struct rx_packet *pkt, 
	    struct rxg_key *key, int encrypt)
{
    u_int len = rx_GetDataSize(pkt);
    struct iovec *frag;
    void *iv;

    if ((iv = malloc(kt->ivsize)) == NULL)
	return ENOMEM;

    (kt->setup_iv)(pkt, key, iv);

    assert((len % kt->blocklen) == 0);

    for (frag = &pkt->wirevec[1]; len; frag++)
    {
	int      iov_len = frag->iov_len;
	uint32_t *iov_bas = (uint32_t *) frag->iov_base;
	if (iov_len == 0) {
	    memset(iv, 0, kt->ivsize);
	    free(iv);
	    return RXGKPACKETSHORT;	/* Length mismatch */
	}
	if (len < iov_len)
	    iov_len = len;		/* Don't process to much data */

	assert((iov_len % kt->blocklen) == 0);

	(*kt->encrypt)(iov_bas, iov_len, key, iv, encrypt);
	len -= iov_len;
    }
    memset(iv, 0, kt->ivsize);
    free(iv);
    return 0;
}

#define MAXCONFOUNDER		50

struct variable_header_data {
    /* Data that changes per packet */
    uint32_t call_number;
    uint32_t channel_and_seq;
};

static void
getheader(struct rx_packet *pkt, struct variable_header_data *h)
{
  uint32_t t;

  /* Collect selected packet fields */
  h->call_number = htonl(pkt->header.callNumber);
  t = ((pkt->header.cid & RX_CHANNELMASK) << (32 - RX_CIDSHIFT))
    | ((pkt->header.seq & 0x3fffffff));
  h->channel_and_seq = htonl(t);
}


/* des-cbc(key XOR 0xF0F0F0F0F0F0F0F0, conf | rsa-md5(conf | msg)) */

static int
checksum_pkt_md5_des(struct rx_packet *pkt, struct rxg_key *key, 
		     void *checksum, size_t checksumlen, int encrypt)
{
    struct rxg_des_keystuff *ks;
    u_int len = rx_GetDataSize(pkt);
    struct iovec *frag;
    des_cblock iv;
    MD5_CTX c;
    int cksumsz;

    ks = &key->key.des;
    cksumsz = key->type->checksumlen;

    assert(cksumsz == 24);

    memset(&iv, 0, sizeof(iv));

    MD5_Init(&c);

    for (frag = &pkt->wirevec[1]; len; frag++)
    {
	int   iov_len = frag->iov_len;
	char *iov_bas = (char *) frag->iov_base;

	if (iov_len == 0)
	    return RXGKPACKETSHORT;		/* Length mismatch */
	if (len < iov_len)
	    iov_len = len;		/* Don't process to much data */

	MD5_Update(&c, iov_bas, iov_len);
	len -= iov_len;
    }
    MD5_Final(checksum, &c);

    des_cbc_encrypt(checksum, checksum, cksumsz, ks->chksum, &iv, 1);

    return 0;
}


int
rxg_PacketCheckSum(struct rxg_key_type *kt, struct rx_packet *pkt, 
		   struct rxg_key *key, void *cksum, size_t cksumsz,
		   int encrypt)
{
    (*kt->cksum_pkt)(pkt, key, cksum, cksumsz, encrypt);
    return 0;
}

int
rxg_check_packet(struct rx_packet *pkt,
		 struct rx_connection *con,
		 int encrypt,
		 struct rxg_con *kc)
{
    struct variable_header_data hd;
    char sum[RXG_MAX_CHECKSUM_SIZE];
    char sum2[RXG_MAX_CHECKSUM_SIZE];
    char *base;
    int ret;

    if (rx_GetPacketCksum(pkt) != 0)
	return RXGKSEALEDINCON;

    if (encrypt) {
	ret = encrypt_pkt(kc->type, pkt, &kc->key, 0);
	if (ret)
	    return ret;
    }
    
    base = pkt->wirevec[1].iov_base;
    if (encrypt)
	base += kc->type->confounderlen;
    memcpy(sum, base, kc->type->checksumlen);
    memset(base, 0, kc->type->checksumlen);

    ret = rxg_PacketCheckSum(kc->type, pkt, &kc->key, sum2, 
			      kc->type->checksumlen, 0);
    if (ret)
	return ret;

    if (memcmp(sum2, sum, kc->type->checksumlen) != 0)
	return RXGKSEALEDINCON;

    getheader(pkt, &hd);
    
    if (memcmp(base + kc->type->checksumlen, &hd, sizeof(hd)) != 0)
	return RXGKSEALEDINCON;

    return 0;
}

int
rxg_prepare_packet(struct rx_packet *pkt,
		   struct rx_connection *con,
		   int encrypt,
		   struct rxg_con *kc)
{
    char sum[RXG_MAX_CHECKSUM_SIZE];
    u_int len = rx_GetDataSize(pkt);
    int diff, ret;
    struct variable_header_data hd;
    char *base;

    /* checksum in rx header is defined to 0 in rxgss */
    rx_SetPacketCksum(pkt, 0);
    
    /* 
     * First we fixup the packet size, its assumed that the checksum
     * need to to the operation on blocklen too
     */

    len += rx_GetSecurityHeaderSize(con); /* Extended pkt len */
	
    if (encrypt) {
	if ((diff = (len % kc->type->blocklen)) != 0) {
	    rxi_RoundUpPacket(pkt, diff);
	    len += diff;
	}
    }
    rx_SetDataSize(pkt, len); /* Set extended packet length */

    diff = kc->type->checksumlen + RXGK_HEADER_DATA_SIZE;
    if (encrypt)
	diff += kc->type->confounderlen;

    if (pkt->wirevec[1].iov_len < diff)
	return RXGKPACKETSHORT;

    base = pkt->wirevec[1].iov_base;
    if (encrypt) {
	rxg_random_data(base, kc->type->confounderlen);
	base += kc->type->confounderlen;
    }
    memset(base, 0, kc->type->checksumlen);
    base += kc->type->checksumlen;
    getheader(pkt, &hd);
    memcpy(base, &hd, sizeof(hd));

    /* computer and store checksum of packet */
    ret = rxg_PacketCheckSum(kc->type, pkt, &kc->key, 
			     sum, kc->type->checksumlen, 1);
    if (ret)
	return ret;

    base = pkt->wirevec[1].iov_base;
    if (encrypt)
	base += kc->type->confounderlen;
    memcpy(base, sum, kc->type->checksumlen);

    if (!encrypt)
	return 0;

    return encrypt_pkt(kc->type, pkt, &kc->key, 1);
}

int
rxgk_set_conn(struct rx_connection *con, int enctype, int enc)
{
    struct rxg_key_type *key;

    key = rxg_find_enctype(enctype);
    if (key)
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


int
rxgk_prepare_packet(struct rx_packet *pkt, struct rx_connection *con,
		     int level, key_stuff *k, end_stuff *e)
{
    return 0;
}

int
rxgk_check_packet(struct rx_packet *pkt, struct rx_connection *con,
		   int level, key_stuff *k, end_stuff *e)
{
    return 0;
}


#if 0

int
main(int argc, char **argv)
{
    krb5_encrypt();
    return 0;
}

#endif
