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
#include "rxgk_proto.ss.h"

/* XXX need to pthread lock these */
krb5_context gk_context;
static krb5_keyblock gk_key;
krb5_crypto gk_crypto;
static int gk_kvno;

static int
get_key(const char *keytab_string, const char *p, int enctype, int kvno,
	krb5_keyblock *key)
{
    krb5_error_code ret;
    krb5_keytab keytab;
    krb5_principal princ;
    char keytab_buf[256];
    krb5_keytab_entry ktentry;

    ret = krb5_parse_name(gk_context, p, &princ);
    if (ret)
	return ret;

    if (keytab_string == NULL) {
	ret = krb5_kt_default_name (gk_context, keytab_buf,sizeof(keytab_buf));
	if (ret)
	    krb5_err(gk_context, 1, ret, "resolving keytab %s", keytab_string);
	keytab_string = keytab_buf;
    }
    ret = krb5_kt_resolve(gk_context, keytab_string, &keytab);
    if (ret)
	krb5_err(gk_context, 1, ret, "resolving keytab %s", keytab_string);

    ret = krb5_kt_get_entry (gk_context, keytab, princ, kvno,
			     enctype, &ktentry);
    if (ret)
	krb5_err(gk_context, 1, ret, "krb5_kt_get_entry %s", p);

    krb5_copy_keyblock_contents(gk_context, &ktentry.keyblock, key);
    /* ktentry.vno */

    krb5_kt_free_entry(gk_context, &ktentry);
	
    krb5_kt_close(gk_context, keytab);

    krb5_free_principal(gk_context, princ);

    return ret;
}

int
rxgk_default_get_key(void *data, const char *p, int enctype, int kvno, 
		     krb5_keyblock *key)
{
    int ret;

    ret = rxgk_server_init();
    if (ret)
	return ret;

    return get_key(NULL, p, enctype, kvno, key);
}


int
rxgk_server_init(void)
{
    static int inited = 0;
    int ret;

    if (inited)
	return 0;

    if (krb5_init_context(&gk_context))
	return EINVAL;

    ret = get_key(NULL, "gkkey@L.NXS.SE", 0, 0, &gk_key); /* XXX */
    if (ret) {
	krb5_free_context(gk_context);
	gk_context = NULL;
	return ret;
    }

    ret = krb5_crypto_init(gk_context, &gk_key, gk_key.keytype,
			   &gk_crypto);
    if (ret) {
	krb5_free_keyblock_contents(gk_context, &gk_key);
	krb5_free_context(gk_context);
	gk_context = NULL;
	return ret;
    }


    inited = 1;

    return 0;
}

static int
build_auth_token(krb5_context context, const char *princ, 
		 int32_t start, int32_t end,
		 krb5_keyblock *key,
		 int session_enctype, 
		 void *session_key, size_t session_key_size,
		 int32_t *auth_token_kvno, RXGK_Token *auth_token)
{
    struct RXGK_AUTH_CRED cred;
    krb5_data data;
    void *ptr;
    int sz, ret;

    sz = RXGK_AUTH_CRED_MAX_SIZE;
    ptr = malloc(sz);
    if (ptr == NULL)
	return ENOMEM;

    cred.ac_version = RXGK_KEY_VERSION;
    strlcpy(cred.ac_principal, princ, sizeof(cred.ac_principal));
    cred.ac_starttime = start;
    cred.ac_endtime = end;
    cred.ac_enctype = session_enctype;
    cred.ac_key.len = session_key_size;
    cred.ac_key.val = session_key;

    if (ydr_encode_RXGK_AUTH_CRED(&cred, ptr, &sz) == NULL) {
	free(ptr);
	return EINVAL;
    }
    sz = RXGK_AUTH_CRED_MAX_SIZE - sz;

    ret = krb5_encrypt(context, gk_crypto, 0, ptr, sz, &data);
    if (ret) {
	free(ptr);
	return ret;
    }
    
    if (data.length > RXGK_AUTH_CRED_MAX_SIZE) {
	free(ptr);
	return EINVAL;
    }
    
    memcpy(ptr, data.data, data.length);

    auth_token->len = data.length;
    auth_token->val = ptr;
    *auth_token_kvno = gk_kvno;
    
    krb5_data_free(&data);

    return 0;
}

int
rxgk_decode_auth_token(void *val, size_t len, RXGK_AUTH_CRED *c)
{
    krb5_data data;
    size_t sz;
    int ret;

    memset(c, 0, sizeof(*c));

    ret = krb5_decrypt(gk_context, gk_crypto, 0, val, len, &data);
    if (ret)
	return ret;

    sz = data.length;
    if (ydr_decode_RXGK_AUTH_CRED(c, data.data, &sz) == NULL) {
	if (c->ac_key.val)
	    free(c->ac_key.val);
	memset(c, 0, sizeof(*c));
	ret = RXGKBADTICKET;
    }

    krb5_data_free(&data);

    return ret;
}

/* XXX share */

static int
decode_v5(krb5_context context,
	  int (*get_key)(void *appl_data, const char *p, int enctype,
			 int kvno, krb5_keyblock *key),
	  void *appl_data,
	  const char *princ,
	  char *ticket,
	  int32_t ticket_len,
	  /* OUT parms */
	  krb5_principal *p,
	  krb5_keyblock *key,
	  int32_t *starts,
	  int32_t *expires)
{
    krb5_keyblock serv_key;
    int code;
    size_t siz;

    Ticket t5;			/* Must free */
    EncTicketPart decr_part;	/* Must free */
    krb5_data plain;		/* Must free */
    krb5_crypto crypto;		/* Must free */

    memset(&t5, 0x0, sizeof(t5));
    memset(&decr_part, 0x0, sizeof(decr_part));
    krb5_data_zero(&plain);
    memset(&serv_key, 0, sizeof(serv_key));
    crypto = NULL;

    code = decode_Ticket(ticket, ticket_len, &t5, &siz);
    if (code != 0)
	goto bad_ticket;

    code = (*get_key)(appl_data, princ, 
		      t5.enc_part.etype, t5.tkt_vno, &serv_key);
    if (code)
	goto unknown_key;
    
    code = krb5_crypto_init (context, &serv_key, t5.enc_part.etype,
			     &crypto);
    krb5_free_keyblock_contents(context, &serv_key);
    if (code)
	goto bad_ticket;

    /* Decrypt ticket */
    code = krb5_decrypt(context,
			crypto,
			0,
			t5.enc_part.cipher.data,
			t5.enc_part.cipher.length,
			&plain);

    if (code)
	goto bad_ticket;
    
    /* Decode ticket */
    code = decode_EncTicketPart(plain.data, plain.length, &decr_part, &siz);
    if (code != 0)
	goto bad_ticket;
    
    /* principal */
    code = principalname2krb5_principal(p, decr_part.cname, decr_part.crealm);
    if (code)
	goto bad_ticket;
    
    /* Extract session key */
    code = krb5_copy_keyblock_contents(context, &decr_part.key, key);
    if (code)
	goto bad_ticket;

    /* Check lifetimes and host addresses, flags etc */
    {
	time_t now = time(0);	/* Use fast time package instead??? */
	time_t start = decr_part.authtime;
	if (decr_part.starttime)
	    start = *decr_part.starttime;
	if (start - now > context->max_skew || decr_part.flags.invalid)
	    goto no_auth;
	if (now > decr_part.endtime)
	    goto tkt_expired;
	*starts = start;
	*expires = decr_part.endtime;
    }
    
#if 0
    /* Check host addresses */
#endif
    
 cleanup:
    free_Ticket(&t5);
    free_EncTicketPart(&decr_part);
    krb5_data_free(&plain);
    if (crypto)
	krb5_crypto_destroy(context, crypto);
    if (code) {
	krb5_free_principal(context, *p);
	*p = NULL;
    }
    return code;
    
 unknown_key:
    code = RXGKUNKNOWNKEY;
    goto cleanup;
 no_auth:
    code = RXGKNOAUTH;
    goto cleanup;
 tkt_expired:
    code = RXGKEXPIRED;
    goto cleanup;
 bad_ticket:
    code = RXGKBADTICKET;
    goto cleanup;
}

int
SRXGK_EstablishKrb5Context(struct rx_call *call,
			   const RXGK_Token *token,
			   const RXGK_Token *challage_token,
			   RXGK_Token *reply_token,
			   int32_t *auth_token_kvno,
			   RXGK_Token *auth_token)
{
    krb5_principal principal;
    krb5_keyblock key;
    int32_t starts, expires;
    char *cprinc, *sprinc;
    void *session_key;
    size_t session_key_size;
    int session_enctype;
    int ret;

    ret = rxgk_server_init();
    if (ret)
	return ret;

    if ((sprinc = rx_getServiceRock(call->conn->service)) == NULL)
	return EINVAL;

    session_key = NULL;

    key.keyvalue.data = NULL;

    auth_token->len = reply_token->len = 0;
    auth_token->val = reply_token->val = NULL;

    ret = decode_v5(gk_context, rxgk_default_get_key, NULL, sprinc, 
		    token->val, token->len,
		    &principal, &key, &starts, &expires);
    if (ret)
	goto out;

    ret = rxk5_mutual_auth_server(gk_context,
				  &key,
				  challage_token,
				  &session_enctype,
				  &session_key, 
				  &session_key_size,
				  reply_token);
    if (ret)
	goto out;

    ret = krb5_unparse_name(gk_context, principal, &cprinc);
    if (ret)
	goto out;

    ret = build_auth_token(gk_context, cprinc, starts, expires, &key,
			   session_enctype, session_key, session_key_size,
			   auth_token_kvno, auth_token);

    free(cprinc);

 out:
    if (session_key) {
	memset(session_key, 0, session_key_size);
	free(session_key);
    }
    if (key.keyvalue.data != NULL)
	krb5_free_keyblock_contents(gk_context, &key);

    if (ret) {
	if (reply_token->val)
	    free(reply_token->val);
	reply_token->len = 0;
	reply_token->val = NULL;

	if (auth_token->val)
	    free(auth_token->val);
	auth_token->len = 0;
	auth_token->val = NULL;
    }

    return ret;
}
