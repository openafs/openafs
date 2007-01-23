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
typedef char *d2i_of_void(/* void **, const unsigned char **, long */);
typedef int i2d_of_void(/* void *, unsigned char ** */);
#endif

void
krb5_free_pa_data(krb5_context context, krb5_pa_data **pa)
{
    int i;
    if (!pa) return;
    for (i = 0; pa[i]; ++i) {
	if (pa[i]->contents) free(pa[i]->contents);
	free(pa[i]);
    }
    free(pa);
}

void
krb5_free_last_req(krb5_context context, krb5_last_req_entry **lr)
{
    int i;
    if (!lr) return;
    for (i = 0; lr[i]; ++i) free(lr[i]);
    free(lr);
}

void
krb5i_LAST_REQ_SEQ_free(krb5_last_req_entry ** a)
{
    krb5_free_last_req(0, a);
}

void
krb5_free_enc_kdc_rep_part(krb5_context context, krb5_enc_kdc_rep_part *ep)
{
    if (!ep) return;
    krb5_free_keyblock(context, ep->session);
    krb5_free_last_req(context, ep->last_req);
    krb5_free_principal(context, ep->server);
    krb5_free_addresses(context, ep->caddrs);
    free(ep);
}

void
krb5_free_error(krb5_context context, krb5_error *e)
{
    if (!e) return;
    krb5_free_principal(context, e->client);
    krb5_free_principal(context, e->server);
    if (e->text.data)
	free (e->text.data);
    if (e->e_data.data)
	free (e->e_data.data);
    free(e);
}

void krb5_free_kdc_rep(krb5_context context, krb5_kdc_rep *krp)
{
    if (!krp) return;
    krb5_free_pa_data(context, krp->padata);
    krb5_free_principal(context, krp->client);
    krb5_free_ticket(context, krp->ticket);
    if (krp->enc_part.ciphertext.data)
	free(krp->enc_part.ciphertext.data);
    krb5_free_enc_kdc_rep_part(context, krp->enc_part2);
    free(krp);
}

krb5_kdc_rep *
krb5i_KDC_REP_new(void)
{
    krb5_kdc_rep *ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_kdc_rep);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_KDC_REP_free(krb5_kdc_rep *krp)
{
    krb5_free_kdc_rep(0, krp);
}

krb5_enc_kdc_rep_part *
krb5i_KDC_ENC_REP_PART_new(void)
{
    krb5_enc_kdc_rep_part *ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_enc_kdc_rep_part);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_KDC_ENC_REP_PART_free (krb5_enc_kdc_rep_part * kerp)
{
    krb5_free_enc_kdc_rep_part(0, kerp);
}

krb5_last_req_entry *
krb5i_LAST_REQ_new(void)
{
    krb5_last_req_entry *ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_last_req_entry);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

krb5_error *
krb5i_ERROR_new(void)
{
    krb5_error *ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_error);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_ERROR_free(krb5_error *a)
{
    krb5_free_error(0, a);
}

int
krb5i_put_name_list(STACK *nl, unsigned char **pp)
{
    return i2d_ASN1_SET(nl, pp, (i2d_of_void *) i2d_ASN1_GENERALSTRING,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE);
}

/*
NAME-TYPE ::= INTEGER	-- constraints (not checked here)
Realm ::= GeneralString
PrincipalName ::= SEQUENCE {
	name-type[0]		NAME-TYPE,
	name-string[1]		SEQUENCE OF GeneralString
}
*/

int
krb5i_i2d_PRINCIPAL_NAME(krb5_principal a, unsigned char **pp)
{
    ASN1_INTEGER *bs = 0;
    ASN1_GENERALSTRING *gs = 0;
    STACK *nl = 0;
    int i, v0 = 0, v1;

    M_ASN1_I2D_vars(a);
    bs = ASN1_INTEGER_new();
    if (!bs) return 0;
    nl = sk_new_null();
    if (!nl) {
	ASN1_INTEGER_free(bs);
	return 0;
    }
    for (i = 0; i < a->length; ++i) {
	gs = ASN1_GENERALSTRING_new();
	ASN1_STRING_set(gs, a->data[i].data, a->data[i].length);
	sk_push(nl, (char *) gs);
    }
    if (!nl) {
	ASN1_INTEGER_free(bs);
	return 0;
    }
    ASN1_INTEGER_set(bs, a->type);
    M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_len_EXP_opt(nl, krb5i_put_name_list, 1, v1);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	if (nl) sk_pop_free(nl, (void(*)(void*))ASN1_GENERALSTRING_free);
	ASN1_INTEGER_free(bs);		/* not in M_ASN1_I2D_seq_total */
	return(r);
    }
    p= *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(nl, krb5i_put_name_list, 1, v1);
    if (nl) sk_pop_free(nl, (void(*)(void*))ASN1_GENERALSTRING_free);
    ASN1_INTEGER_free(bs); bs = 0;
    M_ASN1_I2D_finish();
}

/*
EncryptedData ::= SEQUENCE {
	etype[0]		ENCTYPE, -- EncryptionType
	kvno[1]			INTEGER OPTIONAL,
	cipher[2]		OCTET STRING -- ciphertext
}
*/

int
krb5i_i2d_ENC_DATA(krb5_enc_data const *a, unsigned char **pp)
{
    ASN1_INTEGER *bs = 0;
    ASN1_OCTET_STRING *os = 0;
    int v0, v1 = 0, v2;
    M_ASN1_I2D_vars(a);
    bs = ASN1_INTEGER_new();
    if (!bs) return 0;
    os = ASN1_OCTET_STRING_new();
    if (!os) {
	ASN1_INTEGER_free(bs);
	return 0;
    }
    ASN1_INTEGER_set(bs, a->enctype);
    M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    if (a->kvno) {
	ASN1_INTEGER_set(bs, a->kvno);
	M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 1, v1);
    }
    ASN1_STRING_set(os, a->ciphertext.data, a->ciphertext.length);
    M_ASN1_I2D_len_EXP_opt(os, i2d_ASN1_OCTET_STRING, 2, v2);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_OCTET_STRING_free(os);	/* not in M_ASN1_I2D_seq_total */
	ASN1_INTEGER_free(bs);		/* not in M_ASN1_I2D_seq_total */
	return(r);
    }
    p= *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    ASN1_INTEGER_set(bs, a->enctype);
    M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    if (a->kvno) {
	ASN1_INTEGER_set(bs, a->kvno);
	M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 1, v1);
    }
    M_ASN1_I2D_put_EXP_opt(os, i2d_ASN1_OCTET_STRING, 2, v2);

    ASN1_OCTET_STRING_free(os);
    ASN1_INTEGER_free(bs);
    M_ASN1_I2D_finish();
}

void
krb5i_put_application(unsigned char **pp, int ret, int n)
{
    unsigned char *q = *pp;
    ASN1_put_object(pp, 1, ret, V_ASN1_SEQUENCE, n);
    *q = V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + n;
}

/*
Ticket ::= [APPLICATION 1] SEQUENCE {
	tkt-vno[0]		INTEGER,
	realm[1]		Realm,
	sname[2]		PrincipalName,
	enc-part[3]		EncryptedData
};
*/

int
krb5i_i2d_TICKET(const krb5_ticket *a, unsigned char **pp)
{
    ASN1_INTEGER *bs = 0;
    ASN1_GENERALSTRING *gs = 0;
    int v0, v1, v2 = 0, v3 = 0;
    int r2;
    M_ASN1_I2D_vars(a);
    bs = ASN1_INTEGER_new();
    if (!bs) return 0;
    gs = ASN1_GENERALSTRING_new();
    if (!gs) {
	ASN1_INTEGER_free(bs);
	return 0;
    }
    ASN1_INTEGER_set(bs, 5);
    ASN1_STRING_set(gs, a->server->realm.data, a->server->realm.length);
    M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_len_EXP_opt(gs, i2d_ASN1_GENERALSTRING, 1, v1);
    M_ASN1_I2D_len_EXP_opt(a->server, krb5i_i2d_PRINCIPAL_NAME, 2, v2);
    M_ASN1_I2D_len_EXP_opt(&a->enc_part, krb5i_i2d_ENC_DATA, 3, v3);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r2=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    r=ASN1_object_size(0, r2, V_ASN1_SEQUENCE);	/* V_ASN1_APPLICATION ! */
    if (!pp || !*pp) {
	ASN1_GENERALSTRING_free(gs);	/* not in M_ASN1_I2D_seq_total */
	ASN1_INTEGER_free(bs);		/* not in M_ASN1_I2D_seq_total */
	return(r);
    }
    p= *pp;
    krb5i_put_application(&p, r2, 1);
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(gs, i2d_ASN1_GENERALSTRING, 1, v1);
    M_ASN1_I2D_put_EXP_opt(a->server, krb5i_i2d_PRINCIPAL_NAME, 2, v2);
    M_ASN1_I2D_put_EXP_opt(&a->enc_part, krb5i_i2d_ENC_DATA, 3, v3);

    ASN1_INTEGER_free(bs);
    ASN1_GENERALSTRING_free(gs);
    M_ASN1_I2D_finish();
}

/*

MESSAGE-TYPE ::= INTEGER {
	krb-tgs-req(12), -- Request for authentication based on TGT
	krb-tgs-rep(13), -- Response to KRB_TGS_REQ request
}

krb5i_d2i_ENC_DATA
krb5i_d2i_PRINCIPAL_NAME
krb5i_d2i_TICKET
*/

int lr_type;
krb5_timestamp value;

void
krb5i_LAST_REQ_free(krb5_last_req_entry *a)
{
    free(a);
}

krb5_last_req_entry *
krb5i_d2i_LAST_REQ(krb5_last_req_entry **a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    M_ASN1_D2I_vars(a, krb5_last_req_entry *, krb5i_LAST_REQ_new);

    a1gt = ASN1_GENERALIZEDTIME_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1gt) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    ret->lr_type = ASN1_INTEGER_get(a1int);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 1); /* req */
    ret->value = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);

    ASN1_INTEGER_free(a1int); a1int = 0;
    ASN1_GENERALIZEDTIME_free(a1gt); a1gt = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (ret && *a != ret) krb5i_LAST_REQ_free(ret);
    *a = 0;
    return NULL;
}

STACK *
krb5i_get_last_requests(STACK  **nlp, const unsigned char **pp, long length)
{
    STACK *nl;

    nl = ASN1_seq_unpack(*pp, length,
	(d2i_of_void *) krb5i_d2i_LAST_REQ,
	(void(*)(void*))krb5i_LAST_REQ_free);
    if (!nl) {
	return 0;
    }
    *pp += length;
    *nlp = nl;
    return nl;
}

krb5_last_req_entry **
krb5i_LAST_REQ_dummy()
{
    static krb5_last_req_entry *foo[1];
    return foo;
}

krb5_last_req_entry **
krb5i_d2i_LAST_REQ_SEQ (krb5_last_req_entry ***a, const unsigned char **pp, long length)
{
    STACK *nl = 0;
    krb5_last_req_entry *ka = 0;
    int count, i;
    M_ASN1_D2I_vars(a, krb5_last_req_entry **, krb5i_LAST_REQ_dummy);
    ret = 0;

    M_ASN1_D2I_Init();
    M_ASN1_D2I_begin();

    M_ASN1_D2I_get(nl, krb5i_get_last_requests);

    count = sk_num(nl);
    if (count < 0) count = 0;
    ret = (krb5_last_req_entry **) malloc(sizeof *ret * (count+1));
    if (!ret) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }
    memset(ret, 0, sizeof *ret * (count+1));
    i = 0;
    while ((ka = (krb5_last_req_entry *)sk_shift(nl))) {
	ret[i] = ka;
	++i;
    }
    ret[i] = 0;

    sk_free(nl); nl = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (nl) sk_pop_free(nl, (void(*)(void*))krb5i_LAST_REQ_free);
    if (ret && *a != ret) krb5i_LAST_REQ_SEQ_free(ret);
    *a = 0;
    return NULL;
}

/*
EncKDCRepPart ::= SEQUENCE {
	key[0]			EncryptionKey,
	last-req[1]		LastReq,
	nonce[2]		INTEGER,
	key-expiration[3]	KerberosTime OPTIONAL,
	flags[4]		TicketFlags,
	authtime[5]		KerberosTime,
	starttime[6]		KerberosTime OPTIONAL,
	endtime[7]		KerberosTime,
	renew-till[8]		KerberosTime OPTIONAL,
	srealm[9]		Realm,
	sname[10]		PrincipalName,
	caddr[11]		HostAddresses OPTIONAL
}
*/

krb5_enc_kdc_rep_part *
krb5i_d2i_KDC_ENC_REP_PART (krb5_enc_kdc_rep_part**a, unsigned char **pp,
    long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    ASN1_BIT_STRING *a1bs = 0;
    void *foo;
    int func = 902;
    M_ASN1_D2I_vars(a, krb5_enc_kdc_rep_part *, krb5i_KDC_ENC_REP_PART_new);

    a1int = ASN1_INTEGER_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    a1bs = ASN1_BIT_STRING_new();
    if (!a1int || !a1gs || !a1gt || !a1bs) goto err;

    M_ASN1_D2I_Init();

    M_ASN1_D2I_begin();
    if (!c.slen) goto Bad;
    switch (M_ASN1_next) {
    default:
    Bad:
	c.line = __LINE__; c.error = ERR_R_BAD_ASN1_OBJECT_HEADER;
	goto err;
    case V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + 25:	/* AS REP */
	ret->msg_type = KRB5_AS_REP;
	break;
    case V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + 26:	/* KDC REP */
	ret->msg_type = KRB5_TGS_REP;
	break;
    }
    M_ASN1_D2I_get(foo, krb5i_skip_tag);
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(ret->session, krb5i_d2i_ENCRYPTION_KEY, 0);
    M_ASN1_D2I_get_EXP_opt(ret->last_req, krb5i_d2i_LAST_REQ_SEQ, 1);
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 2);
    ret->nonce = ASN1_INTEGER_get(a1int);
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 3);
    ret->key_exp = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1bs, d2i_ASN1_BIT_STRING, 4);
    ret->flags = krb5i_BIT_STRING_get(a1bs);
    ASN1_BIT_STRING_free(a1bs); a1bs = 0;
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 5);
    ret->times.authtime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 6);
    if (a1gt->length)
	ret->times.starttime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    else
	ret->times.starttime = ret->times.authtime;
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 7);
    ret->times.endtime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 8);
    ret->times.renew_till = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    ASN1_GENERALIZEDTIME_free(a1gt); a1gt = 0;
    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_GENERALSTRING, 9);
    M_ASN1_D2I_get_EXP_opt(ret->server, krb5i_d2i_PRINCIPAL_NAME, 10);
    if (ret->server->realm.data) free(ret->server->realm.data);
    ret->server->realm.data = ASN1_STRING_data(a1gs);
    ret->server->realm.length = ASN1_STRING_length(a1gs);
    free(a1gs); a1gs = 0;
    M_ASN1_D2I_get_EXP_opt(ret->caddrs, krb5i_d2i_SEQ_HOSTADDRESS, 11);
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(func, c.error, c.line);
    if (a1bs) ASN1_BIT_STRING_free(a1bs);
    if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1gs) ASN1_GENERALSTRING_free(a1gs);
    if (ret && *a != ret) krb5i_KDC_ENC_REP_PART_free(ret);
    *a = 0;
    return NULL;
}

/*
KDC-REP ::= SEQUENCE {
	pvno[0]			INTEGER,
	msg-type[1]		MESSAGE-TYPE,
	padata[2]		METHOD-DATA OPTIONAL,
	crealm[3]		Realm,
	cname[4]		PrincipalName,
	ticket[5]		Ticket,
	enc-part[6]		EncryptedData
}
*/

krb5_kdc_rep *
krb5i_d2i_KDC_REP (krb5_kdc_rep**a, const unsigned char **pp, long length, int tag)
{
    ASN1_INTEGER *bs = 0;
    ASN1_GENERALSTRING *gs = 0;
    krb5_enc_data * enc_data;
    krb5_pa_data ** padata = 0;
    void *foo;
    int func = 902;
    M_ASN1_D2I_vars(a, krb5_kdc_rep *, krb5i_KDC_REP_new);

    bs = ASN1_INTEGER_new();
    gs = ASN1_GENERALSTRING_new();
    if (!bs || !gs) goto err;

    M_ASN1_D2I_Init();

    M_ASN1_D2I_begin();
    if (!c.slen || M_ASN1_next != V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + tag) {
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
    M_ASN1_D2I_get_EXP_opt(bs, d2i_ASN1_INTEGER, 1);
    ret->msg_type = ASN1_INTEGER_get(bs);
    M_ASN1_D2I_get_EXP_opt(padata, krb5i_d2i_SEQ_PADATA, 2);
    ret->padata = padata;
    M_ASN1_D2I_get_EXP_opt(gs, d2i_ASN1_GENERALSTRING, 3);
    M_ASN1_D2I_get_EXP_opt(ret->client, krb5i_d2i_PRINCIPAL_NAME, 4);
    if (ret->client->realm.data) free(ret->client->realm.data);
    ret->client->realm.data = ASN1_STRING_data(gs);
    ret->client->realm.length = ASN1_STRING_length(gs);
    free(gs); gs = 0;
    ASN1_INTEGER_free(bs); bs = 0;
    M_ASN1_D2I_get_EXP_opt(ret->ticket, krb5i_d2i_TICKET, 5);
    enc_data = &ret->enc_part;
    M_ASN1_D2I_get_EXP_opt(enc_data, krb5i_d2i_ENC_DATA, 6);
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(func, c.error, c.line);
    if (bs) ASN1_INTEGER_free(bs);
    if (gs) ASN1_GENERALSTRING_free(gs);
    krb5i_SEQ_PADATA_free(padata);
    if (ret && *a != ret) krb5i_KDC_REP_free(ret);
    *a = 0;
    return NULL;
}

/*
krb5i_d2i_ENC_DATA
krb5i_d2i_PRINCIPAL_NAME
krb5i_d2i_TICKET

krb5_ticket *
krb5i_d2i_TICKET (krb5_ticket **a, const unsigned char **pp, long length)
*/

/*
KRB-ERROR ::= [APPLICATION 30] SEQUENCE {
	pvno[0]			INTEGER,
	msg-type[1]		MESSAGE-TYPE,
	ctime[2]		KerberosTime OPTIONAL,
	cusec[3]		INTEGER OPTIONAL,
	stime[4]		KerberosTime,
	susec[5]		INTEGER,
	error-code[6]		INTEGER,
	crealm[7]		Realm OPTIONAL,
	cname[8]		PrincipalName OPTIONAL,
	realm[9]		Realm, -- Correct realm
	sname[10]		PrincipalName, -- Correct name
	e-text[11]		GeneralString OPTIONAL,
	e-data[12]		OCTET STRING OPTIONAL
}
*/

krb5_error *
krb5i_d2i_ERROR (krb5_error **a, const unsigned char **pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    void *foo;
    int func = 902;
    M_ASN1_D2I_vars(a, krb5_error *, krb5i_ERROR_new);

    a1int = ASN1_INTEGER_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    if (!a1int || !a1gs || !a1gt) goto err;

    M_ASN1_D2I_Init();

    M_ASN1_D2I_begin();
    if (!c.slen || M_ASN1_next != V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + 30) {
	c.line = __LINE__; c.error = ERR_R_BAD_ASN1_OBJECT_HEADER;
	goto err;
    }
    M_ASN1_D2I_get(foo, krb5i_skip_tag);

    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    if (ASN1_INTEGER_get(a1int) != 5) {
	c.line = __LINE__; c.error = ERR_R_BAD_ASN1_OBJECT_HEADER;
	func = 903;	/* trick to return KRB5KDC_ERR_BAD_PVNO */
	goto err;
    }
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 1);
#if 0
    ret->msg_type = ASN1_INTEGER_get(a1int);
if (ret->msg_type != 30) and KRB5_MSGTYPE_STRICT, need trick to return KRB5_BADMSGTYPE;
#endif
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 2);
    ret->ctime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 3);
    ret->cusec = ASN1_INTEGER_get(a1int);
    ASN1_STRING_set((ASN1_STRING*)a1gt, "", 0);
    M_ASN1_D2I_get_EXP_opt(a1gt, d2i_ASN1_GENERALIZEDTIME, 4);
    ret->stime = krb5i_ASN1_KERBEROS_TIME_get(a1gt);
    ASN1_GENERALIZEDTIME_free(a1gt); a1gt = 0;
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 5);
    ret->susec = ASN1_INTEGER_get(a1int);
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 6);
    ret->error = ASN1_INTEGER_get(a1int);

    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_GENERALSTRING, 7);
    M_ASN1_D2I_get_EXP_opt(ret->client, krb5i_d2i_PRINCIPAL_NAME, 8);
    if (ret->client) {
	if (ret->client->realm.data) free(ret->client->realm.data);
	ret->client->realm.data = ASN1_STRING_data(a1gs);
	ret->client->realm.length = ASN1_STRING_length(a1gs);
	M_ASN1_STRING_data(a1gs) = 0;
    }

    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_GENERALSTRING, 9);
    M_ASN1_D2I_get_EXP_opt(ret->server, krb5i_d2i_PRINCIPAL_NAME, 10);
    if (ret->server) {
	if (ret->server->realm.data) free(ret->server->realm.data);
	ret->server->realm.data = ASN1_STRING_data(a1gs);
	ret->server->realm.length = ASN1_STRING_length(a1gs);
	M_ASN1_STRING_data(a1gs) = 0;
    }

    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_GENERALSTRING, 11);
    ret->text.data = ASN1_STRING_data(a1gs);
    ret->text.length = ASN1_STRING_length(a1gs);
    M_ASN1_STRING_data(a1gs) = 0;
    M_ASN1_STRING_length(a1gs) = 0;

    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_OCTET_STRING, 12);
    ret->e_data.data = ASN1_STRING_data(a1gs);
    ret->e_data.length = ASN1_STRING_length(a1gs);
    free(a1gs); a1gs = 0;

    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(func, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1gs) ASN1_GENERALSTRING_free(a1gs);
    if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
    if (ret && *a != ret) krb5i_ERROR_free(ret);
    *a = 0;
    return NULL;
}

/*
AuthorizationData ::= SEQUENCE OF SEQUENCE {
	ad-type[0]		INTEGER,
	ad-data[1]		OCTET STRING
}
*/

int
krb5i_i2d_AUTHDATA(const krb5_authdata *a, unsigned char **pp)
{
    ASN1_INTEGER *bs = 0;
    ASN1_OCTET_STRING *os = 0;
    int v0 = 0, v1 = 0;

    M_ASN1_I2D_vars(a);
    bs = ASN1_INTEGER_new();
    if (!bs) return 0;
    os = ASN1_OCTET_STRING_new();
    ASN1_STRING_set(os, a->contents, a->length);
    ASN1_INTEGER_set(bs, a->ad_type);
    M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_len_EXP_opt(os, i2d_ASN1_OCTET_STRING, 1, v1);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_INTEGER_free(bs);		/* not in M_ASN1_I2D_seq_total */
	ASN1_OCTET_STRING_free(os);	/* not in M_ASN1_I2D_seq_total */
	return(r);
    }
    p= *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(os, i2d_ASN1_OCTET_STRING, 1, v1);
    ASN1_INTEGER_free(bs);
    ASN1_OCTET_STRING_free(os);
    M_ASN1_I2D_finish();
}

/*
HostAddress ::= SEQUENCE  {
	addr-type[0]		INTEGER,
	address[1]		OCTET STRING
}
*/
/* XXX combine??? */
int
krb5i_i2d_HOSTADDRESS(const krb5_address *a, unsigned char **pp)
{
    ASN1_INTEGER *bs = 0;
    ASN1_OCTET_STRING *os = 0;
    int v0 = 0, v1 = 0;

    M_ASN1_I2D_vars(a);
    bs = ASN1_INTEGER_new();
    if (!bs) return 0;
    os = ASN1_OCTET_STRING_new();
    ASN1_STRING_set(os, a->contents, a->length);
    ASN1_INTEGER_set(bs, a->addrtype);
    M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_len_EXP_opt(os, i2d_ASN1_OCTET_STRING, 1, v1);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_INTEGER_free(bs);		/* not in M_ASN1_I2D_seq_total */
	ASN1_OCTET_STRING_free(os);	/* not in M_ASN1_I2D_seq_total */
	return(r);
    }
    p= *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(os, i2d_ASN1_OCTET_STRING, 1, v1);
    ASN1_INTEGER_free(bs);
    ASN1_OCTET_STRING_free(os);
    M_ASN1_I2D_finish();
}

/*
PADATA-TYPE ::= INTEGER {
	KRB5-PADATA-TGS-REQ(1),
};
PA-DATA ::= SEQUENCE {
	-- might be encoded AP-REQ
	padata-type[1]		PADATA-TYPE,
	padata-value[2]		OCTET STRING
}
*/

int
krb5i_i2d_PADATA(const krb5_pa_data *a, unsigned char **pp)
{
/* Note: this is [1] [2] not [0] [1] like padata et al. */
    ASN1_INTEGER *bs = 0;
    ASN1_OCTET_STRING *os = 0;
    int v1 = 0, v2 = 0;

    M_ASN1_I2D_vars(a);
    bs = ASN1_INTEGER_new();
    if (!bs) return 0;
    os = ASN1_OCTET_STRING_new();
    ASN1_STRING_set(os, a->contents, a->length);
    ASN1_INTEGER_set(bs, a->pa_type);
    M_ASN1_I2D_len_EXP_opt(bs, i2d_ASN1_INTEGER, 1, v1);
    M_ASN1_I2D_len_EXP_opt(os, i2d_ASN1_OCTET_STRING, 2, v2);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_INTEGER_free(bs);		/* not in M_ASN1_I2D_seq_total */
	ASN1_OCTET_STRING_free(os);	/* not in M_ASN1_I2D_seq_total */
	return(r);
    }
    p= *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(bs, i2d_ASN1_INTEGER, 1, v1);
    M_ASN1_I2D_put_EXP_opt(os, i2d_ASN1_OCTET_STRING, 2, v2);
    ASN1_INTEGER_free(bs);
    ASN1_OCTET_STRING_free(os);
    M_ASN1_I2D_finish();
}

int
krb5i_i2d_SEQ_AUTHDATA(const krb5_authdata **a, unsigned char **pp)
{
    int safelen;
    unsigned char *safe, *p;
    STACK *safes;
    int i;

    safes = sk_new_null();
    if (!safes) {
	return 0;
    }
    for (i = 0; a[i]; ++i) {
	sk_push(safes, (char *) a[i]);
    }
    if (!(safelen = i2d_ASN1_SET(safes, pp, (i2d_of_void*) krb5i_i2d_AUTHDATA,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ASN1_R_ENCODE_ERROR);
	sk_free(safes);
	return 0;
    }
    if (!pp || *pp) {
	sk_free(safes);
	return safelen;
    }
    if (!(safe = OPENSSL_malloc(safelen))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ERR_R_MALLOC_FAILURE);
	sk_free(safes);
	return 0;
    }
    p = safe;
    safelen = i2d_ASN1_SET(safes, &p, (i2d_of_void *) krb5i_i2d_AUTHDATA,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE);
    if (safelen) {
	*pp = safe;
    } else {
	free(safe);
    }
    sk_free(safes);
    return safelen;
}

/*
HostAddresses ::= SEQUENCE OF HostAddress
*/

int
krb5i_i2d_SEQ_HOSTADDRESS(krb5_address **a, unsigned char **pp)
{
    int safelen;
    unsigned char *safe, *p;
    STACK *safes;
    int i;

    safes = sk_new_null();
    if (!safes) {
	return 0;
    }
    for (i = 0; a[i]; ++i) {
	sk_push(safes, (char *) a[i]);
    }
    if (!(safelen = i2d_ASN1_SET(safes, pp, (i2d_of_void *) krb5i_i2d_HOSTADDRESS,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ASN1_R_ENCODE_ERROR);
	sk_free(safes);
	return 0;
    }
    if (!pp || *pp) {
	sk_free(safes);
	return safelen;
    }
    if (!(safe = OPENSSL_malloc(safelen))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ERR_R_MALLOC_FAILURE);
	sk_free(safes);
	return 0;
    }
    p = safe;
    safelen = i2d_ASN1_SET(safes, &p, (i2d_of_void *) krb5i_i2d_HOSTADDRESS,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE);
    if (safelen) {
	*pp = safe;
    } else {
	free(safe);
    }
    sk_free(safes);
    return safelen;
}

/*
METHOD-DATA ::= SEQUENCE OF PA-DATA
*/

int
krb5i_i2d_SEQ_PADATA(krb5_pa_data *const*a, unsigned char **pp)
{
    int safelen;
    unsigned char *safe, *p;
    STACK *safes;
    int i;

    safes = sk_new_null();
    if (!safes) {
	return 0;
    }
    for (i = 0; a[i]; ++i) {
	sk_push(safes, (char *) a[i]);
    }
    if (!(safelen = i2d_ASN1_SET(safes, pp, (i2d_of_void *) krb5i_i2d_PADATA,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ASN1_R_ENCODE_ERROR);
	sk_free(safes);
	return 0;
    }
    if (!pp || *pp) {
	sk_free(safes);
	return safelen;
    }
    if (!(safe = OPENSSL_malloc(safelen))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ERR_R_MALLOC_FAILURE);
	sk_free(safes);
	return 0;
    }
    p = safe;
    safelen = i2d_ASN1_SET(safes, &p, (i2d_of_void *) krb5i_i2d_PADATA,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE);
    if (safelen) {
	*pp = safe;
    } else {
	free(safe);
    }
    sk_free(safes);
    return safelen;
}

struct ktypelist {
    int nktypes;
    krb5_enctype *ktype;
};

/*
SEQUENCE OF ENCTYPE
*/

int
krb5i_i2d_SEQ_ENCTYPE(struct ktypelist *kt, unsigned char **pp)
{
    int safelen;
    unsigned char *safe, *p;
    STACK *safes;
    int i;
    ASN1_INTEGER *a1int = 0;

    safes = sk_new_null();
    if (!safes) {
	return 0;
    }
    for (i = 0; i < kt->nktypes; ++i) {
	a1int = ASN1_INTEGER_new();
	ASN1_INTEGER_set(a1int, kt->ktype[i]);
	sk_push(safes, (char *) a1int);
    }
    if (!(safelen = i2d_ASN1_SET(safes, pp, (i2d_of_void *) i2d_ASN1_INTEGER,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ASN1_R_ENCODE_ERROR);
	sk_pop_free(safes, (void(*)(void*))ASN1_INTEGER_free);
	return 0;
    }
    if (!pp || *pp) {
	sk_pop_free(safes, (void(*)(void*))ASN1_INTEGER_free);
	return safelen;
    }
    if (!(safe = OPENSSL_malloc(safelen))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ERR_R_MALLOC_FAILURE);
	sk_pop_free(safes, (void(*)(void*))ASN1_INTEGER_free);
	return 0;
    }
    p = safe;
    safelen = i2d_ASN1_SET(safes, &p, (i2d_of_void *) i2d_ASN1_INTEGER,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE);
    if (safelen) {
	*pp = safe;
    } else {
	free(safe);
    }
    sk_pop_free(safes, (void(*)(void*))ASN1_INTEGER_free);
    return safelen;
}

/*
SEQUENCE OF Ticket
*/

int
krb5i_i2d_SEQ_TICKET(krb5_ticket **a, unsigned char **pp)
{
    int safelen;
    unsigned char *safe, *p;
    STACK *safes;
    int i;

    safes = sk_new_null();
    if (!safes) {
	return 0;
    }
    for (i = 0; a[i]; ++i) {
	sk_push(safes, (char *) a[i]);
    }
    if (!(safelen = i2d_ASN1_SET(safes, pp, (i2d_of_void *) krb5i_i2d_TICKET,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ASN1_R_ENCODE_ERROR);
	sk_free(safes);
	return 0;
    }
    if (!pp || *pp) {
	sk_free(safes);
	return safelen;
    }
    *pp = 0;
    if (!(safe = OPENSSL_malloc(safelen))) {
	ASN1err(ASN1_F_ASN1_SEQ_PACK,ERR_R_MALLOC_FAILURE);
	sk_free(safes);
	return 0;
    }
    p = safe;
    safelen = i2d_ASN1_SET(safes, &p, (i2d_of_void *) krb5i_i2d_TICKET,
	V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL, IS_SEQUENCE);
    if (safelen) {
	*pp = safe;
    } else {
	free(safe);
    }
    sk_free(safes);
    return safelen;
}

int
krb5i_BIT_STRING_set(const ASN1_BIT_STRING *bs, int value)
{
    int i;
static char foo[sizeof value];

    for (i = sizeof foo;; ) {
#if 0
	if (!value) break;	/* XXX don't think mit will like this */
#endif
	foo[--i] = value;
	value >>= 8;
	if (!i) break;
    }
#if 0
printf ("bit_string_set: value was %#x\n", value);
bin_dump(foo+i, sizeof foo-i, 0);
#endif
    return M_ASN1_BIT_STRING_set(bs, foo+i, sizeof foo-i);
}

int krb5i_i2d_ASN1_BIT_STRING(ASN1_BIT_STRING *a, unsigned char **out)
{
    ASN1_BIT_STRING foo[1];
    int r;
    unsigned char *x = 0;

    *foo = *a;
    if (foo->length && (x = malloc(foo->length))) {
	memcpy(x, a->data, a->length);
	foo->data = x;
	x[a->length-1] |= 1;
    }
    r = i2d_ASN1_BIT_STRING(foo, out);
    if (x && out && *out) {
	(*out)[-1] = a->data[a->length-1];
    }
    if (x) free(x);
    return r;
}

/*
EncryptionKey ::= SEQUENCE {
	keytype[0]		INTEGER,
	keyvalue[1]		OCTET STRING
}
*/

int
krb5i_i2d_ENCRYPTION_KEY(krb5_keyblock const *a, unsigned char **pp)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    int v0 = 0, v1 = 0;
    M_ASN1_I2D_vars(a);
    a1int = ASN1_INTEGER_new();
    a1os = ASN1_OCTET_STRING_new();
    if (!a1int || !a1os) {
	if (a1int) ASN1_INTEGER_free(a1int);
	if (a1os) ASN1_OCTET_STRING_free(a1os);
	return 0;
    }
    ASN1_INTEGER_set(a1int, a->enctype);
    ASN1_STRING_set(a1os, a->contents, a->length);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_len_EXP_opt(a1os, i2d_ASN1_OCTET_STRING, 1, v1);
#if 1
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_INTEGER_free(a1int);
	ASN1_OCTET_STRING_free(a1os);
	return r;
    }
    p = *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(a1os, i2d_ASN1_OCTET_STRING, 1, v1);
    ASN1_INTEGER_free(a1int);
    ASN1_OCTET_STRING_free(a1os);
    M_ASN1_I2D_finish();
}

/*
CKSUMTYPE ::= INTEGER	-- no constraint checking here.
Checksum ::= SEQUENCE {
	cksumtype[0]		CKSUMTYPE,
	checksum[1]		OCTET STRING
}
*/

int
krb5i_i2d_CHECKSUM(krb5_checksum const *a, unsigned char **pp)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_OCTET_STRING *a1os = 0;
    int v0 = 0, v1 = 0;
    M_ASN1_I2D_vars(a);
    a1int = ASN1_INTEGER_new();
    a1os = ASN1_OCTET_STRING_new();
    if (!a1int || !a1os) {
	if (a1int) ASN1_INTEGER_free(a1int);
	if (a1os) ASN1_OCTET_STRING_free(a1os);
	return 0;
    }
    ASN1_INTEGER_set(a1int, a->checksum_type);
    ASN1_STRING_set(a1os, a->contents, a->length);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_len_EXP_opt(a1os, i2d_ASN1_OCTET_STRING, 1, v1);
#if 1
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_INTEGER_free(a1int);
	ASN1_OCTET_STRING_free(a1os);
	return r;
    }
    p = *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(a1os, i2d_ASN1_OCTET_STRING, 1, v1);
    ASN1_INTEGER_free(a1int);
    ASN1_OCTET_STRING_free(a1os);
    M_ASN1_I2D_finish();
}

/*
Authenticator ::= [APPLICATION 2] SEQUENCE {
	authenticator-vno[0]	krb5int32,
	crealm[1]		Realm,
	cname[2]		PrincipalName,
	cksum[3]		Checksum OPTIONAL,
	cusec[4]		krb5int32,
	ctime[5]		KerberosTime,
	subkey[6]		EncryptionKey OPTIONAL,
	seq-number[7]		krb5uint32 OPTIONAL,
	authorization-data[8]	AuthorizationData OPTIONAL
	}
*/

int
krb5i_i2d_AUTHENTICATOR (krb5_authenticator *a, unsigned char **pp)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    int v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5, v6 = 0, v7 = 0, v8 = 0;
    int r2;
    M_ASN1_I2D_vars(a);
    a1int = ASN1_INTEGER_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    if (!a1int || !a1gs || !a1gt) {
	if (a1int) ASN1_INTEGER_free(a1int);
	if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
	if (a1gs) ASN1_GENERALSTRING_free(a1gs);
	return 0;
    }
    ASN1_INTEGER_set(a1int, 5);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    ASN1_STRING_set(a1gs, a->client->realm.data, a->client->realm.length);
    M_ASN1_I2D_len_EXP_opt(a1gs, i2d_ASN1_GENERALSTRING, 1, v1);
    M_ASN1_I2D_len_EXP_opt(a->client, krb5i_i2d_PRINCIPAL_NAME, 2, v2);
    M_ASN1_I2D_len_EXP_opt(a->checksum, krb5i_i2d_CHECKSUM, 3, v3);
    ASN1_INTEGER_set(a1int, a->cusec);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 4, v4);
    ASN1_GENERALIZEDTIME_set(a1gt, a->ctime);
    M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 5, v5); /* req */
    M_ASN1_I2D_len_EXP_opt(a->subkey, krb5i_i2d_ENCRYPTION_KEY, 6, v6);
    ASN1_INTEGER_set(a1int, a->seq_number);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 7, v7);
    if (a->authorization_data && *a->authorization_data) {
	M_ASN1_I2D_len_EXP_opt((const krb5_authdata **)a->authorization_data,
		krb5i_i2d_SEQ_AUTHDATA, 8, v8);
    }
#if 1
    r2=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    r=ASN1_object_size(0, r2, V_ASN1_SEQUENCE);	/* V_ASN1_APPLICATION ! */
    if (!pp || !*pp) {
	ASN1_INTEGER_free(a1int);
	ASN1_GENERALIZEDTIME_free(a1gt);
	ASN1_GENERALSTRING_free(a1gs);
	return r;
    }
    p = *pp;
    krb5i_put_application(&p, r2, 2);
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    ASN1_INTEGER_set(a1int, 5);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    M_ASN1_I2D_put_EXP_opt(a1gs, i2d_ASN1_GENERALSTRING, 1, v1);
    M_ASN1_I2D_put_EXP_opt(a->client, krb5i_i2d_PRINCIPAL_NAME, 2, v2);
    M_ASN1_I2D_put_EXP_opt(a->checksum, krb5i_i2d_CHECKSUM, 3, v3);
    ASN1_INTEGER_set(a1int, a->cusec);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 4, v4);
    M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 5, v5); /* req */
    M_ASN1_I2D_put_EXP_opt(a->subkey, krb5i_i2d_ENCRYPTION_KEY, 6, v6);
    ASN1_INTEGER_set(a1int, a->seq_number);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 7, v7);
    if (a->authorization_data && *a->authorization_data) {
	M_ASN1_I2D_put_EXP_opt((const krb5_authdata **)a->authorization_data,
		krb5i_i2d_SEQ_AUTHDATA, 8, v8);
    }

    ASN1_INTEGER_free(a1int);
    ASN1_GENERALIZEDTIME_free(a1gt);
    ASN1_GENERALSTRING_free(a1gs);
    M_ASN1_I2D_finish();
}

/*
MESSAGE-TYPE ::= INTEGER {
krb-ap-req(14), -- application request to server
};
AP-REQ ::= [APPLICATION 14] SEQUENCE {
        pvno[0]                 krb5int32,
        msg-type[1]             MESSAGE-TYPE,
        ap-options[2]           APOptions,
        ticket[3]               Ticket,
        authenticator[4]        EncryptedData
}
*/

#define ErrorIfZ(v,x) if (!v) {ASN1err(904,x); goto Bad;}

int
krb5i_i2d_APREQ (krb5_ap_req *a, unsigned char **pp)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_BIT_STRING *a1bs = 0;
    int v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0;
    int r2;
    M_ASN1_I2D_vars(a);
    a1bs = ASN1_BIT_STRING_new();
    a1int = ASN1_INTEGER_new();
    if (!a1int || !a1bs) {
    Bad:
	if (a1int) ASN1_INTEGER_free(a1int);
	if (a1bs) ASN1_BIT_STRING_free(a1bs);
	return 0;
    }
    krb5i_BIT_STRING_set(a1bs, a->ap_options);
    ASN1_INTEGER_set(a1int, 5);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    ErrorIfZ(v0,ERR_R_MALLOC_FAILURE);
    ASN1_INTEGER_set(a1int, 14);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 1, v1);
    ErrorIfZ(v1,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_len_EXP_opt(a1bs, krb5i_i2d_ASN1_BIT_STRING, 2, v2);
    ErrorIfZ(v2,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_len_EXP_opt(a->ticket, krb5i_i2d_TICKET, 3, v3);
    if (a->ticket && !v3) goto Bad;
    M_ASN1_I2D_len_EXP_opt(&a->authenticator, krb5i_i2d_ENC_DATA, 4, v4);
    ErrorIfZ(v4,ERR_R_MALLOC_FAILURE);
#if 1
    r2=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    r=ASN1_object_size(0, r2, V_ASN1_SEQUENCE);	/* V_ASN1_APPLICATION ! */
    if (!pp || !*pp) {
	ASN1_INTEGER_free(a1int);
	ASN1_BIT_STRING_free(a1bs);
	return r;
    }
    p = *pp;
    krb5i_put_application(&p, r2, 14);
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    ASN1_INTEGER_set(a1int, 5);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 0, v0);
    ErrorIfZ(v0,ERR_R_MALLOC_FAILURE);
    ASN1_INTEGER_set(a1int, 14);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 1, v1);
    ErrorIfZ(v1,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_put_EXP_opt(a1bs, krb5i_i2d_ASN1_BIT_STRING, 2, v2);
    ErrorIfZ(v2,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_put_EXP_opt(a->ticket, krb5i_i2d_TICKET, 3, v3);
    ErrorIfZ(v3,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_put_EXP_opt(&a->authenticator, krb5i_i2d_ENC_DATA, 4, v4);
    ErrorIfZ(v4,ERR_R_MALLOC_FAILURE);

    ASN1_INTEGER_free(a1int);
    ASN1_BIT_STRING_free(a1bs);
    M_ASN1_I2D_finish();
}

/*
KDC-REQ-BODY ::= SEQUENCE {
	kdc-options[0]		KDCOptions,
	cname[1]		PrincipalName OPTIONAL, -- Used only in AS-REQ
	realm[2]		Realm,	-- Server's realm
					-- Also client's in AS-REQ
	sname[3]		PrincipalName OPTIONAL,
	from[4]			KerberosTime OPTIONAL,
	till[5]			KerberosTime OPTIONAL,
	rtime[6]		KerberosTime OPTIONAL,
	nonce[7]		INTEGER,
	etype[8]		SEQUENCE OF ENCTYPE, -- EncryptionType,
					-- in preference order
	addresses[9]		HostAddresses OPTIONAL,
	enc-authorization-data[10] EncryptedData OPTIONAL,
					-- Encrypted AuthorizationData encoding
	additional-tickets[11]	SEQUENCE OF Ticket OPTIONAL
}
*/

int
krb5i_i2d_KDCREQBODY(const krb5_kdc_req *a, unsigned char **pp)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    ASN1_BIT_STRING *a1bs = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    int v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5 = 0, v6 = 0, v7 = 0, v8, v9 = 0, v10 = 0, v11 = 0;
    struct ktypelist kt[1];
    krb5_principal server;

    M_ASN1_I2D_vars(a);
    a1int = ASN1_INTEGER_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1bs = ASN1_BIT_STRING_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    if (!a1int || !a1gs || !a1bs || !a1gt) {
    Bad:
	if (!a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
	if (!a1bs) ASN1_BIT_STRING_free(a1bs);
	if (!a1gs) ASN1_OCTET_STRING_free(a1gs);
	if (!a1int) ASN1_INTEGER_free(a1int);
	return 0;
    }
    server = a->server;
    if ((a->kdc_options & KDC_OPT_ENC_TKT_IN_SKEY)
	    && a->second_ticket && a->second_ticket[0]) {
	server = a->second_ticket[0]->server;
    }
    krb5i_BIT_STRING_set(a1bs, a->kdc_options);
    M_ASN1_I2D_len_EXP_opt(a1bs, krb5i_i2d_ASN1_BIT_STRING, 0, v0);
    if (a->client) {
	M_ASN1_I2D_len_EXP_opt(a->client, krb5i_i2d_PRINCIPAL_NAME, 1, v1);
	ErrorIfZ(v1,ERR_R_MALLOC_FAILURE);
    }
    ASN1_STRING_set(a1gs, server->realm.data, server->realm.length);
    M_ASN1_I2D_len_EXP_opt(a1gs, i2d_ASN1_GENERALSTRING, 2, v2);
    ErrorIfZ(v2,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_len_EXP_opt(a->server, krb5i_i2d_PRINCIPAL_NAME, 3, v3);
    if (a->server) {ErrorIfZ(v3,ERR_R_MALLOC_FAILURE);}
    if (a->from) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->from);
	M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 4, v4);
	ErrorIfZ(v4,ERR_R_MALLOC_FAILURE);
    }
    ASN1_GENERALIZEDTIME_set(a1gt, a->till);
    M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 5, v5);
    ErrorIfZ(v5,ERR_R_MALLOC_FAILURE);
    if (a->rtime) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->rtime);
	M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 6, v6);
	ErrorIfZ(v6,ERR_R_MALLOC_FAILURE);
    }
    ASN1_INTEGER_set(a1int, a->nonce);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 7, v7);
    ErrorIfZ(v7,ERR_R_MALLOC_FAILURE);
    kt->nktypes = a->nktypes;
    kt->ktype = a->ktype;
    M_ASN1_I2D_len_EXP_opt(kt, krb5i_i2d_SEQ_ENCTYPE, 8, v8);
    ErrorIfZ(v8,ERR_R_MALLOC_FAILURE);
    if (a->addresses && a->addresses[0]) {
	M_ASN1_I2D_len_EXP_opt(a->addresses, krb5i_i2d_SEQ_HOSTADDRESS, 9, v9);
	ErrorIfZ(v9,ERR_R_MALLOC_FAILURE);
    }
    if (a->authorization_data.ciphertext.data) {
	M_ASN1_I2D_len_EXP_opt(&a->authorization_data, krb5i_i2d_ENC_DATA, 10, v10);
	ErrorIfZ(v10,ERR_R_MALLOC_FAILURE);
    }
    if (a->second_ticket && *a->second_ticket) {
	M_ASN1_I2D_len_EXP_opt(a->second_ticket, krb5i_i2d_SEQ_TICKET, 11, v11);
	ErrorIfZ(v11,ERR_R_MALLOC_FAILURE);
    }
#if 0
    M_ASN1_I2D_seq_total();
#else
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_GENERALIZEDTIME_free(a1gt);
	ASN1_BIT_STRING_free(a1bs);
	ASN1_OCTET_STRING_free(a1gs);
	ASN1_INTEGER_free(a1int);
	return(r);
    }
    p = *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(a1bs, krb5i_i2d_ASN1_BIT_STRING, 0, v0);
    ErrorIfZ(v0,ERR_R_MALLOC_FAILURE);
    if (a->client) {
	M_ASN1_I2D_put_EXP_opt(a->client, krb5i_i2d_PRINCIPAL_NAME, 1, v1);
	ErrorIfZ(v1,ERR_R_MALLOC_FAILURE);
    }
    M_ASN1_I2D_put_EXP_opt(a1gs, i2d_ASN1_GENERALSTRING, 2, v2);
    ErrorIfZ(v2,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_put_EXP_opt(a->server, krb5i_i2d_PRINCIPAL_NAME, 3, v3);
    if (a->server) {ErrorIfZ(v3,ERR_R_MALLOC_FAILURE);}
    if (a->from) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->from);
	M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 4, v4);
	ErrorIfZ(v4,ERR_R_MALLOC_FAILURE);
    }
    ASN1_GENERALIZEDTIME_set(a1gt, a->till);
    M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 5, v5);
    ErrorIfZ(v5,ERR_R_MALLOC_FAILURE);
    if (a->rtime) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->rtime);
	M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 6, v6);
	ErrorIfZ(v6,ERR_R_MALLOC_FAILURE);
    }
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 7, v7);
    ErrorIfZ(v7,ERR_R_MALLOC_FAILURE);
    M_ASN1_I2D_put_EXP_opt(kt, krb5i_i2d_SEQ_ENCTYPE, 8, v8);
    ErrorIfZ(v8,ERR_R_MALLOC_FAILURE);
    if (a->addresses && a->addresses[0]) {
	M_ASN1_I2D_put_EXP_opt(a->addresses, krb5i_i2d_SEQ_HOSTADDRESS, 9, v9);
	ErrorIfZ(v9,ERR_R_MALLOC_FAILURE);
    }
    if (a->authorization_data.ciphertext.data) {
	M_ASN1_I2D_put_EXP_opt(&a->authorization_data, krb5i_i2d_ENC_DATA, 10, v10);
	ErrorIfZ(v10,ERR_R_MALLOC_FAILURE);
    }
    if (a->second_ticket && *a->second_ticket) {
	M_ASN1_I2D_put_EXP_opt(a->second_ticket, krb5i_i2d_SEQ_TICKET, 11, v11);
	ErrorIfZ(v11,ERR_R_MALLOC_FAILURE);
    }
    ASN1_GENERALIZEDTIME_free(a1gt);
    ASN1_BIT_STRING_free(a1bs);
    ASN1_OCTET_STRING_free(a1gs);
    ASN1_INTEGER_free(a1int);
    M_ASN1_I2D_finish();
}

/*
KDC-REQ ::= SEQUENCE {
	pvno[1]			INTEGER,
	msg-type[2]		MESSAGE-TYPE,
	padata[3]		METHOD-DATA OPTIONAL,
	req-body[4]		KDC-REQ-BODY
}
TGS-REQ ::= [APPLICATION 12] KDC-REQ
*/

int
krb5i_i2d_KDCREQ(const krb5_kdc_req *a, unsigned char **pp, int n)
{
    ASN1_INTEGER *a1int = 0;
    int v1, v2, v3 = 0, v4 = 0;
    int r2;

    M_ASN1_I2D_vars(a);
    a1int = ASN1_INTEGER_new();
    if (!a1int) {
    Bad:
	if (!a1int) ASN1_INTEGER_free(a1int);
	return 0;
    }
    ASN1_INTEGER_set(a1int, 5);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 1, v1);
    ErrorIfZ(v1,ERR_R_MALLOC_FAILURE);
    ASN1_INTEGER_set(a1int, n);
    M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 2, v2);
    ErrorIfZ(v2,ERR_R_MALLOC_FAILURE);
    if (a->padata && *a->padata) {
	M_ASN1_I2D_len_EXP_opt(a->padata, krb5i_i2d_SEQ_PADATA, 3, v3);
	ErrorIfZ(v3,ERR_R_MALLOC_FAILURE);
    }
    M_ASN1_I2D_len_EXP_opt(a, krb5i_i2d_KDCREQBODY, 4, v4);
    ErrorIfZ(v4,ERR_R_MALLOC_FAILURE);
#if 0
    M_ASN1_I2D_seq_total();
#else
    r2=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    r = ASN1_object_size(0, r2, V_ASN1_SEQUENCE);	/* V_ASN1_APPLICATION ! */
    if (!pp || !*pp) {
	ASN1_INTEGER_free(a1int);
	return(r);
    }
    p = *pp;
    krb5i_put_application(&p, r2, n);
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    ASN1_INTEGER_set(a1int, 5);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 1, v1);
    ErrorIfZ(v1,ERR_R_MALLOC_FAILURE);
    ASN1_INTEGER_set(a1int, n);
    M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 2, v2);
    ErrorIfZ(v2,ERR_R_MALLOC_FAILURE);
    if (a->padata && *a->padata) {
	M_ASN1_I2D_put_EXP_opt(a->padata, krb5i_i2d_SEQ_PADATA, 3, v3);
	ErrorIfZ(v3,ERR_R_MALLOC_FAILURE);
    }
    M_ASN1_I2D_put_EXP_opt(a, krb5i_i2d_KDCREQBODY, 4, v4);
    ErrorIfZ(v4,ERR_R_MALLOC_FAILURE);
    ASN1_INTEGER_free(a1int);
    M_ASN1_I2D_finish();
}

krb5_error_code
encode_krb5_ticket(const krb5_ticket *tkt, krb5_data **datap)
{
    int len;
    int code;
    unsigned char *p, *q = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;	/* XXX */
    for (;;) {
	p = q;
	len = krb5i_i2d_TICKET(tkt, &p);
	if (q || !len) break;
	code = ENOMEM;
	if (!(q = malloc(len))) break;
    }
    if (q && (*datap = malloc(sizeof **datap))) {
	(*datap)->data = q;
	(*datap)->length = len;
	q = 0;
	code = 0;
    }
    if (q) free(q);
if (code == 999) { fprintf (stderr,"encode_krb5_ticket; something broke in krb5i_i2d_TICKET.\n"); ERR_print_errors_fp(stderr);}
    return code;
}

krb5_error_code
encode_krb5_authdata(C krb5_authdata **auth, krb5_data **out)
{
    unsigned char *p, *q = 0;
    int code;
    int len;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;	/* XXX */
    for (;;) {
	p = q;
	len = krb5i_i2d_SEQ_AUTHDATA((const krb5_authdata **) auth, &p);
	if (q || !len) break;
	code = ENOMEM;
	if (!(q = malloc(len))) break;
    }
    if (q && (*out = malloc(sizeof **out))) {
	(*out)->data = q;
	(*out)->length = len;
	q = 0;
	code = 0;
    }
    if (q) free(q);
if (code == 999) { fprintf (stderr,"encode_krb5_authdata; something broke in krb5i_i2d_SEQ_AUTHDATA.\n"); ERR_print_errors_fp(stderr);}
    return code;
}

krb5_error_code
encode_krb5_kdc_req_body(krb5_kdc_req *req, krb5_data **out)
{
    unsigned char *p, *q = 0;
    int code;
    int len;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;	/* XXX */
    for (;;) {
	p = q;
	len = krb5i_i2d_KDCREQBODY(req, &p);
	if (q || !len) break;
	code = ENOMEM;
	if (!(q = malloc(len))) break;
    }
    if (q && (*out = malloc(sizeof **out))) {
	(*out)->data = q;
	(*out)->length = len;
	q = 0;
	code = 0;
    }
    if (q) free(q);
if (code == 999) { fprintf (stderr,"encode_krb5_kdc_req_body; something broke in krb5i_i2d_KDCREQBODY.\n"); ERR_print_errors_fp(stderr);}
    return code;
}

krb5_error_code
encode_krb5_x_req(krb5_kdc_req *req, krb5_data **out, int type)
{
    unsigned char *p, *q = 0;
    int code;
    int len;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;	/* XXX */
    for (;;) {
	p = q;
	len = krb5i_i2d_KDCREQ(req, &p, type);
	if (q || !len) break;
	code = ENOMEM;
	if (!(q = malloc(len))) break;
    }
    if (q && (*out = malloc(sizeof **out))) {
	(*out)->data = q;
	(*out)->length = len;
	q = 0;
	code = 0;
    }
    if (q) free(q);
if (code == 999) { fprintf (stderr,"encode_krb5_tgs_req; something broke in krb5i_i2d_KDCREQ.\n"); ERR_print_errors_fp(stderr);}
    return code;
}

krb5_error_code
encode_krb5_as_req(krb5_kdc_req *req, krb5_data **out)
{
    return encode_krb5_x_req(req, out, 10);
}

krb5_error_code
encode_krb5_tgs_req(krb5_kdc_req *req, krb5_data **out)
{
    return encode_krb5_x_req(req, out, 12);
}

krb5_error_code
krb5i_decode_tgs_rep(krb5_data *data, krb5_kdc_rep ** out)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_kdc_rep *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_KDC_REP(&a, &p, length, KRB5_TGS_REP)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	if (ERR_GET_FUNC(er) == 903) code = KRB5KDC_ERR_BAD_PVNO;
	if (ERR_GET_FUNC(er) == 904) code = ASN1_BAD_TIMEFORMAT;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
krb5i_decode_as_rep(krb5_data *data, krb5_kdc_rep ** out)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_kdc_rep *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_KDC_REP(&a, &p, length, KRB5_AS_REP)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	if (ERR_GET_FUNC(er) == 903) code = KRB5KDC_ERR_BAD_PVNO;
	if (ERR_GET_FUNC(er) == 904) code = ASN1_BAD_TIMEFORMAT;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
decode_krb5_enc_kdc_rep_part(krb5_data *data, krb5_enc_kdc_rep_part ** out)
{
    long length = data->length;
    unsigned char *p = data->data;
    krb5_enc_kdc_rep_part *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_KDC_ENC_REP_PART(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	if (ERR_GET_FUNC(er) == 903) code = KRB5KDC_ERR_BAD_PVNO;
	if (ERR_GET_FUNC(er) == 904) code = ASN1_BAD_TIMEFORMAT;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
krb5_kdc_rep_decrypt_proc(krb5_context context,
const krb5_keyblock *key, const krb5_keyusage *usagep, krb5_kdc_rep *krp)
{
    krb5_error_code code;
    krb5_keyusage usage = usagep ? *usagep : KRB5_KEYUSAGE_TGS_REP_ENCPART_SESSKEY;
    krb5_data temp[1];

    krp->enc_part2 = 0;
    temp->data = malloc(temp->length = krp->enc_part.ciphertext.length);
    if (!temp->data) return ENOMEM;
    code = krb5_c_decrypt(context, key, usage, 0, &krp->enc_part, temp);
    if (code) goto Done;
    code = decode_krb5_enc_kdc_rep_part(temp, &krp->enc_part2);
Done:
    if (temp->data)
    {
	memset(temp->data, 0, temp->length);
	free(temp->data);
    }
    return code;
}

krb5_error_code
krb5_decode_kdc_rep(krb5_context context,krb5_data *data,
    const krb5_keyblock *key,krb5_kdc_rep ** krpp)
{
    krb5_error_code code;
    krb5_keyusage usage;
    usage = KRB5_KEYUSAGE_TGS_REP_ENCPART_SESSKEY;

    *krpp = 0;
    code = krb5i_decode_tgs_rep(data, krpp);
    if (code) goto Done;

    code = krb5_kdc_rep_decrypt_proc(context, key, &usage, *krpp);
Done:
    if (code) {
	krb5_free_kdc_rep(context, *krpp);
	*krpp = 0;
    }
    return code;
}

krb5_error_code
decode_krb5_error(const krb5_data *data, krb5_error **out)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_error *a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_ERROR(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	if (ERR_GET_FUNC(er) == 903) code = KRB5KDC_ERR_BAD_PVNO;
	if (ERR_GET_FUNC(er) == 904) code = ASN1_BAD_TIMEFORMAT;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
encode_krb5_ap_req(const krb5_ap_req *apreq, krb5_data **datap)
{
    int len;
    int code;
    unsigned char *p, *q = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;
    for (;;) {
	p = q;
	len = krb5i_i2d_APREQ((krb5_ap_req *) apreq, &p);
	if (q || !len) break;
	code = ENOMEM;
	if (!(q = malloc(len))) break;
    }
    if (q && (*datap = malloc(sizeof **datap))) {
	(*datap)->data = q;
	(*datap)->length = len;
	q = 0;
	code = 0;
    }
    if (q) free(q);
if (code == 999) { fprintf (stderr,"encode_krb5_ap_req; something broke in krb5i_i2d_APREQ.\n"); ERR_print_errors_fp(stderr);}
    return code;
}

krb5_error_code
encode_krb5_authenticator(const krb5_authenticator *auth, krb5_data **datap)
{
    int len;
    int code;
    unsigned char *p, *q = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;
    for (;;) {
	p = q;
	len = krb5i_i2d_AUTHENTICATOR((krb5_authenticator *) auth, &p);
	if (q || !len) break;
	code = ENOMEM;
	if (!(q = malloc(len))) break;
    }
    if (q && (*datap = malloc(sizeof **datap))) {
	(*datap)->data = q;
	(*datap)->length = len;
	q = 0;
	code = 0;
    }
    if (q) free(q);
if (code == 999) { fprintf (stderr,"encode_krb5_authenticator; something broke in krb5i_i2d_AUTHENTICATOR.\n"); ERR_print_errors_fp(stderr);}
    return code;
}

int
krb5i_is_application(krb5_data *data, int n)
{
    return data && data->data && data->length &&
	(data->data[0]|V_ASN1_CONSTRUCTED) ==
	V_ASN1_APPLICATION + V_ASN1_CONSTRUCTED + n;
}

int
krb5_is_as_rep(krb5_data *data)
{
    return krb5i_is_application(data, 11);
}

int
krb5_is_tgs_rep(krb5_data *data)
{
    return krb5i_is_application(data, 13);
}

int
krb5_is_krb_error(krb5_data *data)
{
    return krb5i_is_application(data, 30);
}
