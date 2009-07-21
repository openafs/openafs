/*
 * Copyright (c) 2002 - 2004, Stockholms universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 * 
 * Redistribution is not permitted
 */

#include "rxgk_locl.h"

#include <errno.h>

#include <rx/rx.h>
#include "rxgk_proto.h"

/*
 *
 */

int rxgk_key_contrib_size = 16;

/*
 *
 */

int
rxk5_mutual_auth_client_generate(krb5_context context, krb5_keyblock *key,
				 uint32_t number,
				 RXGK_Token *challage_token)
{
    krb5_crypto crypto;
    krb5_data data;
    RXGK_CHALLENGE_TOKEN ct;
    char buf[RXGK_CHALLENGE_TOKEN_MAX_SIZE];
    size_t sz;
    int ret;

    data.data = NULL;

    ret = krb5_crypto_init (context, key, key->keytype, &crypto);
    if (ret)
	return ret;
    
    ct.ct_version = RXGK_CR_TOKEN_VERSION;
    ct.ct_nonce = number;
    ct.ct_enctype.val = malloc(sizeof(ct.ct_enctype.val[0]));
    ct.ct_enctype.len = 1;
    if (ct.ct_enctype.val == NULL) {
	ret = ENOMEM;
	goto out;
    }
    ct.ct_enctype.val[0] = RXGK_CRYPTO_DES_CBC_CRC;
    
    sz = RXGK_CHALLENGE_TOKEN_MAX_SIZE;
    if (ydr_encode_RXGK_CHALLENGE_TOKEN(&ct, buf, &sz) == NULL) {
	ret = ENOMEM;
	goto out;
    }
    sz = RXGK_CHALLENGE_TOKEN_MAX_SIZE - sz;

    ret = krb5_encrypt(context, crypto, 0, buf, sz, &data);
    if (ret)
	goto out;
    
    challage_token->val = malloc(data.length);
    if (challage_token->val == NULL) {
	ret = ENOMEM;
	goto out;
    }

    challage_token->len = data.length;
    memcpy(challage_token->val, data.data, data.length);

 out:
    ydr_free_RXGK_CHALLENGE_TOKEN(&ct);
    if (data.data)
	krb5_data_free(&data);
    krb5_crypto_destroy(context, crypto);
    return ret;
}

/*
 *
 */

int
rxk5_mutual_auth_client_check(krb5_context context, krb5_keyblock *key,
			      uint32_t number,
			      const RXGK_Token *challage_token,
			      krb5_keyblock *rxsession_key)
{
    krb5_crypto crypto;
    krb5_data data;
    RXGK_REPLY_TOKEN rt;
    size_t sz;
    int ret;

    memset(&rt, 0, sizeof(rt));
    memset(rxsession_key, 0, sizeof(*rxsession_key));

    ret = krb5_crypto_init (context, key, key->keytype, &crypto);
    if (ret)
	return ret;
    
    /* Decrypt ticket */
    data.data = NULL;
    ret = krb5_decrypt(context, crypto, 0,
		       challage_token->val, challage_token->len,
		       &data);
    if (ret)
	goto out;

    sz = data.length;
    if (ydr_decode_RXGK_REPLY_TOKEN(&rt, data.data, &sz) == NULL) {
	ret = RXGKSEALEDINCON;
	goto out;
    }

    if (rt.rt_nonce != number + 1) {
	ret = RXGKSEALEDINCON;
	goto out2;
    }

    if (rt.rt_error != 0) {
	ret = rt.rt_error;
	goto out2;
    }

#if 1
    /* XXX check rt_enctype */
    ret = rxgk_random_to_key(rt.rt_enctype, 
			     rt.rt_key.val, rt.rt_key.len,
			     rxsession_key);
#else
    ret = krb5_copy_keyblock_contents(context, key, rxsession_key);
#endif

 out2:
    ydr_free_RXGK_REPLY_TOKEN(&rt);
 out:
    if (data.data)
	krb5_data_free(&data);
    krb5_crypto_destroy(context, crypto);

    return ret;
}

/*
 *
 */

int
rxk5_mutual_auth_server(krb5_context context, krb5_keyblock *key,
			const RXGK_Token *challage_token,
			int *session_enctype, 
			void **session_key, size_t *session_key_size,
			RXGK_Token *reply_token)
{
    krb5_crypto crypto;
    krb5_data data;
    krb5_keyblock keyblock;
    RXGK_CHALLENGE_TOKEN ct;
    RXGK_REPLY_TOKEN rt;
    char buf[RXGK_REPLY_TOKEN_MAX_SIZE];
    size_t sz;
    int ret;

    memset(&rt, 0, sizeof(rt));
    memset(&ct, 0, sizeof(ct));

    *session_enctype = 0;
    *session_key = NULL;
    *session_key_size = 0;

    keyblock.keyvalue.data = NULL;

    sz = RXGK_CHALLENGE_TOKEN_MAX_SIZE - sz;

    ret = krb5_crypto_init (context, key, key->keytype, &crypto);
    if (ret)
	return ret;
    
    /* Decrypt ticket */
    data.data = NULL;
    ret = krb5_decrypt(context, crypto, 0,
		       challage_token->val, challage_token->len,
		       &data);
    if (ret)
	goto out;

    sz = data.length;
    if (ydr_decode_RXGK_CHALLENGE_TOKEN(&ct, data.data, &sz) == NULL) {
	memset(&ct, 0, sizeof(ct));
	ret = ENOMEM;
	goto out;
    }
    sz = data.length - sz;

    krb5_data_free(&data);
    data.data = NULL;

    if (ct.ct_version < RXGK_CR_TOKEN_VERSION) {
	ret = RXGKSEALEDINCON;
	goto out;
    } else
	ret = 0;

    /* XXX choose best enctype, not just the one we use now */
    { 
	int i;

	for (i = 0; i < ct.ct_enctype.len ; i++) {
	    if (ct.ct_enctype.val[i] == key->keytype)
		break;
	}

	if (i == ct.ct_enctype.len)
	    ret = RXGKSEALEDINCON;
    }

    rt.rt_version = RXGK_CR_TOKEN_VERSION;
    rt.rt_nonce = ct.ct_nonce + 1;

    rt.rt_key.len = 0;
    rt.rt_key.val = NULL;
    rt.rt_error = ret;

    if (ret == 0) {
	ret = krb5_generate_random_keyblock(context, 
					    key->keytype,
					    &keyblock);
	if (ret == 0) {
	    rt.rt_enctype = keyblock.keytype;
	    rt.rt_key.len = keyblock.keyvalue.length;
	    rt.rt_key.val = keyblock.keyvalue.data;

	    *session_enctype = keyblock.keytype;
	    *session_key_size = keyblock.keyvalue.length;
	    *session_key = malloc(keyblock.keyvalue.length);
	    if (*session_key == NULL)
		abort();
	    memcpy(*session_key, keyblock.keyvalue.data, 
		   keyblock.keyvalue.length);
	} else {
	    rt.rt_error = ret;
	}
    }

    sz = RXGK_REPLY_TOKEN_MAX_SIZE;
    if (ydr_encode_RXGK_REPLY_TOKEN(&rt, buf, &sz) == 0) {
	ret = ENOMEM;
	goto out;
    }
    sz = RXGK_REPLY_TOKEN_MAX_SIZE - sz;

    memset(rt.rt_key.val, 0, rt.rt_key.len);

    data.data = NULL;
    ret = krb5_encrypt(context, crypto, 0, buf, sz, &data);
    if (ret)
	goto out;
    
    reply_token->val = malloc(data.length);
    if (reply_token->val == NULL) {
	ret = ENOMEM;
	goto out;
    }

    reply_token->len = data.length;
    memcpy(reply_token->val, data.data, data.length);

 out:
    ydr_free_RXGK_CHALLENGE_TOKEN(&ct);
    /* ydr_free_RXGK_REPLY_TOKEN(&rt); */

    if (data.data)
	krb5_data_free(&data);
    if (keyblock.keyvalue.data)
	krb5_free_keyblock_contents(context, &keyblock);
    krb5_crypto_destroy(context, crypto);
    return ret;
}

/*
 *
 */

void
rxgk_getheader(struct rx_packet *pkt, struct rxgk_header_data *h)
{
  uint32_t t;

  /* Collect selected packet fields */
  h->call_number = htonl(pkt->header.callNumber);
  t = ((pkt->header.cid & RX_CHANNELMASK) << (32 - RX_CIDSHIFT))
    | ((pkt->header.seq & 0x3fffffff));
  h->channel_and_seq = htonl(t);
}

/*
 *
 */

#if 0
int
rxgk_derive_transport_key(krb5_context context,
			  krb5_keyblock *rx_conn_key,
			  RXGK_rxtransport_key *keycontrib,
			  krb5_keyblock *rkey)
{
    krb5_error_code ret;

    /* XXX heimdal broken doesn't implement derive key for des encrypes */

    switch (rx_conn_key->keytype) {
    case RXGK_CRYPTO_DES_CBC_CRC:
    case RXGK_CRYPTO_DES_CBC_MD4:
    case RXGK_CRYPTO_DES_CBC_MD5:
	ret = krb5_copy_keyblock_contents(context, rx_conn_key, rkey);
	if (ret)
	    abort();

	break;
    default: {
	char rxk_enc[RXGK_RXTRANSPORT_KEY_MAX_SIZE];
	size_t sz;
	krb5_keyblock *key;
	
	sz = RXGK_RXTRANSPORT_KEY_MAX_SIZE;
	if (ydr_encode_RXGK_rxtransport_key(keycontrib, rxk_enc, &sz) == NULL)
	    return EINVAL;
	
	sz = RXGK_RXTRANSPORT_KEY_MAX_SIZE - sz;

	ret = krb5_derive_key (context,
			       rx_conn_key,
			       rx_conn_key->keytype,
			       rxk_enc,
			       sz,
			       &key);
	if (ret)
	    abort();
	
	ret = krb5_copy_keyblock_contents(context, key, rkey);
	if (ret)
	    abort();
	
	krb5_free_keyblock(context, key);
	break;
    }
    }

    return ret;
}
#endif

/*
 *
 */


/* XXX replace me */

int
rxgk_random_to_key(int enctype, 
		   void *random_data, int random_sz,
		   krb5_keyblock *key)
{
    memset(key, 0, sizeof(*key));

    switch (enctype) {
    case RXGK_CRYPTO_DES_CBC_CRC:
    case RXGK_CRYPTO_DES_CBC_MD4:
    case RXGK_CRYPTO_DES_CBC_MD5:
	if (random_sz != 8)
	    return RXGKINCONSISTENCY;
	break;
    default:
	    return RXGKINCONSISTENCY;
    }

    key->keyvalue.data = malloc(random_sz);
    if (key->keyvalue.data == NULL)
	return ENOMEM;
    memcpy(key->keyvalue.data, random_data, random_sz);
    key->keyvalue.length = random_sz;
    key->keytype = enctype;

    return 0;
}
