/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <ctype.h>
#include <stdio.h>
#include <memory.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/asn1_mac.h>
#include <openssl/opensslv.h>
#include "k5ssl.h"

#if OPENSSL_VERSION_NUMBER <= 0x009070afL
/* for old ssl? (to test...) */
typedef char *d2i_of_void();
#endif

void
krb5_free_enc_tkt_part(krb5_context context, krb5_enc_tkt_part *enc_part)
{
    if (!enc_part) return;
    if (enc_part->session)
	krb5_free_keyblock(context, enc_part->session);
    if (enc_part->client)
	krb5_free_principal(context, enc_part->client);
    if (enc_part->transited.tr_contents.data)
	free(enc_part->transited.tr_contents.data);
    if (enc_part->caddrs)
	krb5_free_addresses(context, enc_part->caddrs);
    if (enc_part->authorization_data)
	krb5_free_authdata(context, enc_part->authorization_data);
    free(enc_part);
}

void
krb5_free_ticket(krb5_context context, krb5_ticket *tkt)
{
    if (!tkt) return;
    if (tkt->server) krb5_free_principal(context, tkt->server);
    if (tkt->enc_part.ciphertext.data)
	free(tkt->enc_part.ciphertext.data);
    if (tkt->enc_part2)
	krb5_free_enc_tkt_part(context, tkt->enc_part2);
    free(tkt);
}

krb5_enc_data *
krb5i_ENC_DATA_new()
{
    krb5_enc_data * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_enc_data);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_ENC_DATA_free(krb5_enc_data *a)
{
    if (a) {
	if (a->ciphertext.data)
	    free(a->ciphertext.data);
	free(a);
    }
}

krb5_ticket *
krb5i_TICKET_new()
{
    krb5_ticket * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_ticket);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_TICKET_free(krb5_ticket *a)
{
    if (a)
	krb5_free_ticket(0, a);
}

krb5_principal
krb5i_PRINCIPAL_NAME_new()
{
    krb5_principal ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_principal_data);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_PRINCIPAL_NAME_free(krb5_principal a)
{
    krb5_free_principal(0, a);
}

/*
Realm ::= GeneralString
ENCTYPE ::= INTEGER -- {
-- }
NAME-TYPE ::= INTEGER -- {
-- }
PrincipalName ::= SEQUENCE {
        name-type[0]            NAME-TYPE,
        name-string[1]          SEQUENCE OF GeneralString
}
EncryptedData ::= SEQUENCE {
        etype[0]                ENCTYPE, -- EncryptionType
        kvno[1]                 INTEGER OPTIONAL,
        cipher[2]               OCTET STRING -- ciphertext
}
Ticket ::= [APPLICATION 1] SEQUENCE {
        tkt-vno[0]              INTEGER,
        realm[1]                Realm,
        sname[2]                PrincipalName,
        enc-part[3]             EncryptedData
}
*/

krb5_enc_data *
krb5i_d2i_ENC_DATA (krb5_enc_data **a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *bs = 0;
    ASN1_OCTET_STRING *os = 0;
    M_ASN1_D2I_vars(a, krb5_enc_data *, krb5i_ENC_DATA_new);

    bs = ASN1_INTEGER_new();
    os = ASN1_OCTET_STRING_new();
    if (!bs || !os) goto err;
    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(bs, d2i_ASN1_INTEGER, 0);
    ret->enctype = ASN1_INTEGER_get(bs);
    M_ASN1_D2I_get_EXP_opt(bs, d2i_ASN1_INTEGER, 1);
    ret->kvno = ASN1_INTEGER_get(bs);
    M_ASN1_D2I_get_EXP_opt(os, d2i_ASN1_OCTET_STRING, 2);
    ret->ciphertext.data = ASN1_STRING_data(os);
    ret->ciphertext.length = ASN1_STRING_length(os);
    free(os); os = 0;	/* in place of ASN1_OCTET_STRING_free(os); */
    ASN1_INTEGER_free(bs); bs = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (bs) ASN1_INTEGER_free(bs);
    if (os) ASN1_OCTET_STRING_free(os);
    if (ret && *a != ret) krb5i_ENC_DATA_free(ret);
    *a = 0;
    return NULL;
}

STACK *
krb5i_get_name_list(STACK  **nlp, const unsigned char **pp, long length)
{
    STACK *nl;

    nl = ASN1_seq_unpack(*pp, length,
	(d2i_of_void *) d2i_ASN1_GENERALSTRING,
	(void(*)(void*))ASN1_GENERALSTRING_free);
    if (!nl) {
	return 0;
    }
    *pp += length;
    *nlp = nl;
    return nl;
}

krb5_principal
krb5i_d2i_PRINCIPAL_NAME (krb5_principal*a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *bs = 0;
    STACK *nl = 0;
    ASN1_GENERALSTRING *gs = 0;
    int count;
    krb5_data *data;
    M_ASN1_D2I_vars(a, krb5_principal, krb5i_PRINCIPAL_NAME_new);

    bs = ASN1_INTEGER_new();
    if (!bs) goto err;
    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(bs, d2i_ASN1_INTEGER, 0);
    ret->type = ASN1_INTEGER_get(bs);

    M_ASN1_D2I_get_EXP_opt(nl, krb5i_get_name_list, 1);
    count = sk_num(nl);
    if (count < 0) count = 0;
    ret->length = count;
    data = ret->data = (krb5_data *) malloc(ret->length * sizeof *ret->data);
    if (!ret->data) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }
    memset(data, 0, ret->length * sizeof *ret->data);
    while ((gs = (ASN1_GENERALSTRING *)sk_shift(nl))) {
	data->length = ASN1_STRING_length(gs);
	data->data = ASN1_STRING_data(gs);
	free(gs); /* in place of ASN1_GENERALSTRING_free(gs); */
	++data;
    }

    sk_free(nl); nl = 0;
    ASN1_INTEGER_free(bs); bs = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (bs) ASN1_INTEGER_free(bs);
    if (nl) sk_pop_free(nl, (void(*)(void*))ASN1_GENERALSTRING_free);
    if (ret && *a != ret) krb5i_PRINCIPAL_NAME_free(ret);
    *a = 0;
    return NULL;
}

void *
krb5i_skip_tag(void **foo, const unsigned char **pp, long len)
{
    const unsigned char *q;
    int tinf, tag, class;
    long tlen;

    q = *pp;
    tinf = ASN1_get_object(&q, &tlen, &tag, &class, len);
    if (tinf & 0x80) return 0;
    *pp = q;
    return foo;
}

krb5_ticket *
krb5i_d2i_TICKET (krb5_ticket **a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *bs = 0;
    ASN1_GENERALSTRING *gs = 0;
    krb5_enc_data * enc_data;
    void *foo;
    int func = 902;
    M_ASN1_D2I_vars(a, krb5_ticket *, krb5i_TICKET_new);

    bs = ASN1_INTEGER_new();
    gs = ASN1_GENERALSTRING_new();
    if (!bs || !gs) goto err;

    M_ASN1_D2I_Init();

    M_ASN1_D2I_begin();
    if (!c.slen || M_ASN1_next != V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + 1) {
	c.line = __LINE__; c.error = ERR_R_BAD_ASN1_OBJECT_HEADER;
	goto err;
    }
    M_ASN1_D2I_get(foo, krb5i_skip_tag);

    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(bs, d2i_ASN1_INTEGER, 0);
    if (ASN1_INTEGER_get(bs) != 5) {
	c.line = __LINE__; c.error = ERR_R_BAD_ASN1_OBJECT_HEADER;
	func = 903;	/* trick to return KRB5KDC_ERR_BAD_PVNO */
	goto err;
    }
    M_ASN1_D2I_get_EXP_opt(gs, d2i_ASN1_GENERALSTRING, 1);
    M_ASN1_D2I_get_EXP_opt(ret->server, krb5i_d2i_PRINCIPAL_NAME, 2);
    if (ret->server->realm.data) free(ret->server->realm.data);
    ret->server->realm.data = ASN1_STRING_data(gs);
    ret->server->realm.length = ASN1_STRING_length(gs);
    free(gs); gs = 0;
    ASN1_INTEGER_free(bs); bs = 0;
    enc_data = &ret->enc_part;
    M_ASN1_D2I_get_EXP_opt(enc_data, krb5i_d2i_ENC_DATA, 3);
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(func, c.error, c.line);
    if (bs) ASN1_INTEGER_free(bs);
    if (gs) ASN1_GENERALSTRING_free(gs);
    if (ret && *a != ret) krb5i_TICKET_free(ret);
    *a = 0;
    return NULL;
}

krb5_error_code
krb5_decode_ticket(const krb5_data *data, krb5_ticket **tkt)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_ticket *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *tkt = 0;
    if (!krb5i_d2i_TICKET(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	if (ERR_GET_FUNC(er) == 903) code = KRB5KDC_ERR_BAD_PVNO;
	return code;
    }
    *tkt = a;
    return 0;
}

/*
TicketFlags ::= BIT STRING -- {
-- }
EncryptionKey ::= SEQUENCE {
        keytype[0]              INTEGER,
        keyvalue[1]             OCTET STRING
}
TransitedEncoding ::= SEQUENCE {
        tr-type[0]              INTEGER, -- must be registered
        contents[1]             OCTET STRING
}
KerberosTime ::= GeneralizedTime -- Specifying UTC time zone (Z)
HostAddress ::= SEQUENCE  {
        addr-type[0]            INTEGER,
        address[1]              OCTET STRING
}
HostAddresses ::= SEQUENCE OF HostAddress
AuthorizationData ::= SEQUENCE OF SEQUENCE {
        ad-type[0]              INTEGER,
        ad-data[1]              OCTET STRING
}        
EncTicketPart ::= [APPLICATION 3] SEQUENCE {
        flags[0]                TicketFlags,
        key[1]                  EncryptionKey,
        crealm[2]               Realm,
        cname[3]                PrincipalName,
        transited[4]            TransitedEncoding,
        authtime[5]             KerberosTime,
        starttime[6]            KerberosTime OPTIONAL,
        endtime[7]              KerberosTime,
        renew-till[8]           KerberosTime OPTIONAL,
        caddr[9]                HostAddresses OPTIONAL,
        authorization-data[10]  AuthorizationData OPTIONAL
}
*/

krb5_enc_tkt_part *
krb5i_ENCTICKETPART_new()
{
    krb5_enc_tkt_part * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_enc_tkt_part);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_ENCTICKETPART_free(krb5_enc_tkt_part *a)
{
    krb5_free_enc_tkt_part(0, a);
}

krb5_keyblock *
krb5i_ENCRYPTION_KEY_new()
{
    krb5_keyblock * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_keyblock);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_ENCRYPTION_KEY_free(krb5_keyblock *a)
{
    krb5_free_keyblock(0, a);
}

krb5_transited *
krb5i_TRANSITED_ENCODING_new()
{
    krb5_transited * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_transited);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_TRANSITED_ENCODING_free(krb5_transited *a)
{
    if (!a) return;
    if (a->tr_contents.data) free(a->tr_contents.data);
    free(a);
}

krb5_address *
krb5i_HOSTADDRESS_new()
{
    krb5_address * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_address);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_HOSTADDRESS_free(krb5_address *a)
{
    if (!a) return;
    if (a->contents) free(a->contents);
    free(a);
}

krb5_authdata *
krb5i_AUTHDATA_new()
{
    krb5_authdata * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_authdata);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_AUTHDATA_free(krb5_authdata *a)
{
    if (!a) return;
    if (a->contents) free(a->contents);
    free(a);
}

krb5_pa_data *
krb5i_PADATA_new()
{
    krb5_pa_data * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_pa_data);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_PADATA_free(krb5_pa_data *a)
{
    if (!a) return;
    if (a->contents) free(a->contents);
    free(a);
}

krb5_keyblock *
krb5i_d2i_ENCRYPTION_KEY (krb5_keyblock **a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    M_ASN1_D2I_vars(a, krb5_keyblock *, krb5i_ENCRYPTION_KEY_new);

    a1os = ASN1_OCTET_STRING_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1os) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    M_ASN1_D2I_get_EXP_opt(a1os, d2i_ASN1_OCTET_STRING, 1);
    ret->enctype = ASN1_INTEGER_get(a1int);
    ret->length= ASN1_STRING_length(a1os);
    ret->contents = ASN1_STRING_data(a1os);
	
    free(a1os); a1os = 0;	/* in place of ASN1_OCTET_STRING_free(a1os); */
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1os) ASN1_OCTET_STRING_free(a1os);
    if (ret && *a != ret) krb5i_ENCRYPTION_KEY_free(ret);
    *a = 0;
    return NULL;
}

krb5_transited *
krb5i_d2i_TRANSITED_ENCODING (krb5_transited **a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    M_ASN1_D2I_vars(a, krb5_transited *, krb5i_TRANSITED_ENCODING_new);

    a1os = ASN1_OCTET_STRING_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1os) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    M_ASN1_D2I_get_EXP_opt(a1os, d2i_ASN1_OCTET_STRING, 1);
    ret->tr_type = ASN1_INTEGER_get(a1int);
    ret->tr_contents.length= ASN1_STRING_length(a1os);
    ret->tr_contents.data = ASN1_STRING_data(a1os);
	
    free(a1os); a1os = 0;	/* in place of ASN1_OCTET_STRING_free(a1os); */
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1os) ASN1_OCTET_STRING_free(a1os);
    if (ret && *a != ret) krb5i_TRANSITED_ENCODING_free(ret);
    *a = 0;
    return NULL;
}

krb5_address *
krb5i_d2i_HOSTADDRESS (krb5_address **a, unsigned char *const *pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    M_ASN1_D2I_vars(a, krb5_address *, krb5i_HOSTADDRESS_new);

    a1os = ASN1_OCTET_STRING_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1os) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    M_ASN1_D2I_get_EXP_opt(a1os, d2i_ASN1_OCTET_STRING, 1);
    ret->addrtype = ASN1_INTEGER_get(a1int);
    ret->length= ASN1_STRING_length(a1os);
    ret->contents = ASN1_STRING_data(a1os);
	
    free(a1os); a1os = 0;	/* in place of ASN1_OCTET_STRING_free(a1os); */
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1os) ASN1_OCTET_STRING_free(a1os);
    if (ret && *a != ret) krb5i_HOSTADDRESS_free(ret);
    *a = 0;
    return NULL;
}

STACK *
krb5i_get_host_addresses(STACK  **nlp, const unsigned char **pp, long length)
{
    STACK *nl;

    nl = ASN1_seq_unpack(*pp, length,
	(d2i_of_void *) krb5i_d2i_HOSTADDRESS,
	(void(*)(void*))krb5i_HOSTADDRESS_free);
    if (!nl) {
	return 0;
    }
    *pp += length;
    *nlp = nl;
    return nl;
}

krb5_address **
krb5i_SEQ_HOSTADDRESS_dummy()
{
    static krb5_address *foo[1];
    return foo;
}

void
krb5i_SEQ_HOSTADDRESS_free(krb5_address ** a)
{
    int i;
    krb5_address *ka;
    if (!a) return;
    for (i = 0; (ka = a[i]); ++i)
	krb5i_HOSTADDRESS_free(ka);
    free(a);
}

krb5_address **
krb5i_d2i_SEQ_HOSTADDRESS (krb5_address***a, const unsigned char **pp, long length)
{
    STACK *nl = 0;
    krb5_address *ka = 0;
    int count, i;
    M_ASN1_D2I_vars(a, krb5_address **, krb5i_SEQ_HOSTADDRESS_dummy);
    ret = 0;

    M_ASN1_D2I_Init();
    M_ASN1_D2I_begin();

    M_ASN1_D2I_get(nl, krb5i_get_host_addresses);
    
    count = sk_num(nl);
    if (count < 0) count = 0;
    ret = (krb5_address **) malloc(sizeof *ret * (count+1));
    if (!ret) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }
    memset(ret, 0, sizeof *ret * (count+1));
    i = 0;
    while ((ka = (krb5_address *)sk_shift(nl))) {
	ret[i] = ka;
	++i;
    }
    ret[i] = 0;

    sk_free(nl); nl = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (nl) sk_pop_free(nl, (void(*)(void*))krb5i_HOSTADDRESS_free);
    if (ret && *a != ret) krb5i_SEQ_HOSTADDRESS_free(ret);
    *a = 0;
    return NULL;
}

krb5_authdata *
krb5i_d2i_AUTHDATA (krb5_authdata **a, unsigned char *const *pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    M_ASN1_D2I_vars(a, krb5_authdata *, krb5i_AUTHDATA_new);

    a1os = ASN1_OCTET_STRING_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1os) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    M_ASN1_D2I_get_EXP_opt(a1os, d2i_ASN1_OCTET_STRING, 1);
    ret->ad_type = ASN1_INTEGER_get(a1int);
    ret->length= ASN1_STRING_length(a1os);
    ret->contents = ASN1_STRING_data(a1os);
	
    free(a1os); a1os = 0;	/* in place of ASN1_OCTET_STRING_free(a1os); */
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1os) ASN1_OCTET_STRING_free(a1os);
    if (ret && *a != ret) krb5i_AUTHDATA_free(ret);
    *a = 0;
    return NULL;
}

STACK *
krb5i_get_authorizations(STACK  **nlp, const unsigned char **pp, long length)
{
    STACK *nl;

    nl = ASN1_seq_unpack(*pp, length,
	(d2i_of_void *) krb5i_d2i_AUTHDATA,
	(void(*)(void*))krb5i_AUTHDATA_free);
    if (!nl) {
	return 0;
    }
    *pp += length;
    *nlp = nl;
    return nl;
}

krb5_authdata **
krb5i_SEQ_AUTHDATA_dummy()
{
    static krb5_authdata *foo[1];
    return foo;
}

void
krb5i_SEQ_AUTHDATA_free(krb5_authdata ** a)
{
    int i;
    krb5_authdata *ka;
    if (!a) return;
    for (i = 0; (ka = a[i]); ++i)
	krb5i_AUTHDATA_free(ka);
    free(a);
}

krb5_authdata **
krb5i_d2i_SEQ_AUTHDATA (krb5_authdata***a, const unsigned char **pp, long length)
{
    STACK *nl = 0;
    krb5_authdata *ka = 0;
    int count, i;
    M_ASN1_D2I_vars(a, krb5_authdata **, krb5i_SEQ_AUTHDATA_dummy);
    ret = 0;

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();

    M_ASN1_D2I_get_EXP_opt(nl, krb5i_get_authorizations, 1);
    
    count = sk_num(nl);
    if (count < 0) count = 0;
    ret = (krb5_authdata **) malloc(sizeof *ret * (count+1));
    if (!ret) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }
    memset(ret, 0, sizeof *ret * (count+1));
    i = 0;
    while ((ka = (krb5_authdata *)sk_shift(nl))) {
	ret[i] = ka;
	++i;
    }
    ret[i] = 0;

    sk_free(nl); nl = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (nl) sk_pop_free(nl, (void(*)(void*))krb5i_AUTHDATA_free);
    if (ret && *a != ret) krb5i_SEQ_AUTHDATA_free(ret);
    *a = 0;
    return NULL;
}

krb5_pa_data *
krb5i_d2i_PADATA (krb5_pa_data **a, unsigned char *const *pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    M_ASN1_D2I_vars(a, krb5_pa_data *, krb5i_PADATA_new);

    a1os = ASN1_OCTET_STRING_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1os) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 1);
    M_ASN1_D2I_get_EXP_opt(a1os, d2i_ASN1_OCTET_STRING, 2);
    ret->pa_type = ASN1_INTEGER_get(a1int);
    ret->length= ASN1_STRING_length(a1os);
    ret->contents = ASN1_STRING_data(a1os);
	
    free(a1os); a1os = 0;	/* in place of ASN1_OCTET_STRING_free(a1os); */
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1os) ASN1_OCTET_STRING_free(a1os);
    if (ret && (!a || *a != ret)) krb5i_PADATA_free(ret);
    if (a) *a = 0;
    return NULL;
}

STACK *
krb5i_get_pa_data_s(STACK  **nlp, const unsigned char **pp, long length)
{
    STACK *nl;

    nl = ASN1_seq_unpack(*pp, length,
	(d2i_of_void *) krb5i_d2i_PADATA,
	(void(*)(void*))krb5i_PADATA_free);
    if (!nl) {
	return 0;
    }
    *pp += length;
    *nlp = nl;
    return nl;
}

krb5_pa_data **
krb5i_SEQ_PADATA_dummy()
{
    static krb5_pa_data *foo[1];
    return foo;
}

void
krb5i_SEQ_PADATA_free(krb5_pa_data ** a)
{
    int i;
    krb5_pa_data *ka;
    if (!a) return;
    for (i = 0; (ka = a[i]); ++i)
	krb5i_PADATA_free(ka);
    free(a);
}

krb5_pa_data **
krb5i_d2i_SEQ_PADATA (krb5_pa_data***a, const unsigned char **pp, long length)
{
    STACK *nl = 0;
    krb5_pa_data *ka = 0;
    int count, i;
    M_ASN1_D2I_vars(a, krb5_pa_data **, krb5i_SEQ_PADATA_dummy);
    ret = 0;

    M_ASN1_D2I_Init();
    M_ASN1_D2I_begin();

    M_ASN1_D2I_get(nl, krb5i_get_pa_data_s);
    
    count = sk_num(nl);
    if (count < 0) count = 0;
    ret = (krb5_pa_data **) malloc(sizeof *ret * (count+1));
    if (!ret) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }
    memset(ret, 0, sizeof *ret * (count+1));
    i = 0;
    while ((ka = (krb5_pa_data *)sk_shift(nl))) {
	ret[i] = ka;
	++i;
    }
    ret[i] = 0;

    sk_free(nl); nl = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (nl) sk_pop_free(nl, (void(*)(void*))krb5i_PADATA_free);
    if (ret && *a != ret) krb5i_SEQ_PADATA_free(ret);
    *a = 0;
    return NULL;
}

time_t
krb5i_timegm(const struct tm *tm)
{
    time_t r;
    int year, i;
    static const unsigned char mtable[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    year = tm->tm_year - 70;
    r = (year * 365) + ((year+1)/4);
/* XXX should check for range errors */
    if (tm->tm_mon >= 12U) return -1;
    if (tm->tm_mday-1 < mtable[tm->tm_mon])
	;
    else if (tm->tm_mday != 29 || tm->tm_mon != 1 || !((year+2) & 3))
	return -1;
    if (tm->tm_hour >= 24U) return -1;
    if (tm->tm_min >= 60U) return -1;
    if (tm->tm_sec >= 60U) return -1;

    for (i = 0; i < tm->tm_mon; ++i)
	r += mtable[i];
    r += tm->tm_mon >= 2 && !((year+2) & 3);
    r += tm->tm_mday-1;
    r *= 24;
    r += tm->tm_hour;
    r *= 60;
    r += tm->tm_min;
    r *= 60;
    r += tm->tm_sec;
    return r;
}

time_t
krb5i_ASN1_KERBEROS_TIME_get(const ASN1_GENERALIZEDTIME *gt)
{
    struct tm tm[1];
    int year, i;
    unsigned char * cp = gt->data;

    if (!gt->length) return 0;

    /* [yy]yymmddhhmmssZ */
    for (i = 0; i < gt->length; ++i)
	if (!isdigit(cp[i]))
	    break;
    memset(tm, 0, sizeof *tm);
    year = 0;
#define c2i(p)	((0[p]-'0')*10+1[p]-'0')
    cp += i;
    if (gt->length - i != 1 || *cp != 'Z')
	return -1;
    if (i >= 2) tm->tm_sec = c2i(cp-2);
    if (i >= 4) tm->tm_min = c2i(cp-4);
    if (i >= 6) tm->tm_hour = c2i(cp-6);
    if (i >= 8) tm->tm_mday = c2i(cp-8);
    if (i >= 10) tm->tm_mon = c2i(cp-10) - 1;
    if (i >= 12) year = c2i(cp-12);
    if (i >= 14) year += (c2i(cp-14) * 100 - 1900);
    else if (year < 50) year += 100; /* yuck */
    tm->tm_year = year;
    return krb5i_timegm(tm);
}

int
krb5i_BIT_STRING_get(const ASN1_BIT_STRING *bs)
{
    int i, r;
/* XXX what if bs->length > 4 ???.  Mit says ignore. */
/* XXX for that matter; what if the bit string is < 4, hmm???? */
    r = 0;
    for (i = 0; i < sizeof r; ++i) {
	r <<= 8;
	if (i < bs->length)
	    r += i[(unsigned char*)bs->data];
    }
    return r;
}

krb5_enc_tkt_part *
krb5i_d2i_ENCTICKETPART (krb5_enc_tkt_part **a, unsigned char *const *pp, long length)
{
    ASN1_BIT_STRING *a1bs = 0;
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    krb5_transited *tr = 0;
    void *foo;
    int func = 902;
    M_ASN1_D2I_vars(a, krb5_enc_tkt_part *, krb5i_ENCTICKETPART_new);

    a1int = ASN1_INTEGER_new();
    a1bs = ASN1_BIT_STRING_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    if (!a1int || !a1bs || !a1gs || !a1gt) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();

    M_ASN1_D2I_begin();
    if (!c.slen || M_ASN1_next != V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + 3) {
	c.line = __LINE__; c.error = ERR_R_BAD_ASN1_OBJECT_HEADER;
	goto err;
    }
    M_ASN1_D2I_get(foo, krb5i_skip_tag);

    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1bs, d2i_ASN1_BIT_STRING, 0);
    ret->flags = krb5i_BIT_STRING_get(a1bs);
    M_ASN1_D2I_get_EXP_opt(ret->session, krb5i_d2i_ENCRYPTION_KEY, 1);
    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_GENERALSTRING, 2);
    M_ASN1_D2I_get_EXP_opt(ret->client, krb5i_d2i_PRINCIPAL_NAME, 3);
    if (!ret->client) {
	func = 904;
	c.line = __LINE__; c.error = ERR_R_NESTED_ASN1_ERROR;
	goto err;
    }
    if (ret->client->realm.data) free(ret->client->realm.data);
    ret->client->realm.data = ASN1_STRING_data(a1gs);
    ret->client->realm.length = ASN1_STRING_length(a1gs);
    free(a1gs); a1gs = 0;	/* in place of ASN1_GENERALSTRING_free(a1gs); */
    M_ASN1_D2I_get_EXP_opt(tr, krb5i_d2i_TRANSITED_ENCODING, 4); /* req */
    ret->transited = *tr;
    free(tr); tr = 0;
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 5); /* req */
    ret->times.authtime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 6);
    if (a1gt->length)
	ret->times.starttime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    else
	ret->times.starttime = ret->times.authtime;
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 7); /* req */
    ret->times.endtime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 8);
    ret->times.renew_till = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
/* XXX should check for errors from krb5i_ASN1_KERBEROS_TIME_get */
    M_ASN1_D2I_get_EXP_opt(ret->caddrs, krb5i_d2i_SEQ_HOSTADDRESS, 9);
#if 0
printf ("About to get authorization data\n");
bin_dump(c.p, c.slen, c.p-*pp);
#endif
    M_ASN1_D2I_get_EXP_opt(ret->authorization_data, krb5i_d2i_SEQ_AUTHDATA, 10);
    ASN1_INTEGER_free(a1int); a1int = 0;
    ASN1_GENERALIZEDTIME_free(a1gt); a1gt = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(func, c.error, c.line);
    if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1bs) ASN1_BIT_STRING_free(a1bs);
    if (a1gs) ASN1_GENERALSTRING_free(a1gs);
    if (tr) krb5i_TRANSITED_ENCODING_free(tr);
    if (ret && *a != ret) krb5i_ENCTICKETPART_free(ret);
    *a = 0;
    return NULL;
}

krb5_error_code
decode_krb5_enc_tkt_part(krb5_data * data, krb5_enc_tkt_part **out)
{
    long length = data->length;
    unsigned char *p = data->data;
    krb5_enc_tkt_part *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_ENCTICKETPART(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	if (ERR_GET_FUNC(er) == 904) code = ASN1_BAD_TIMEFORMAT;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
decode_krb5_encryption_key(const krb5_data *data,
    krb5_keyblock **out)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_keyblock *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_ENCRYPTION_KEY(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	return code;
    }
    *out = a;
    return 0;
}
