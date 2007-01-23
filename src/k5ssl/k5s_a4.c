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
#include "k5ssl.h"

#if OPENSSL_VERSION_NUMBER <= 0x009070afL
/* for old ssl? (to test...) */
typedef char *d2i_of_void();
#endif

int krb5i_i2d_ENC_DATA(krb5_enc_data const *, unsigned char **);

krb5_etype_info_entry *
krb5i_EINFO_new()
{
    krb5_etype_info_entry * ret;
    ASN1_CTX c;
    M_ASN1_New_Malloc(ret,krb5_etype_info_entry);
    memset(ret, 0, sizeof *ret);
    return ret;
    M_ASN1_New_Error(902);	/* XXX */
}

void
krb5i_EINFO_free(krb5_etype_info_entry *a)
{
    if (!a) return;
    if (a->salt) free(a->salt);
    if (a->s2kparams.data) free(a->s2kparams.data);
    free(a);
}

krb5_etype_info_entry *
krb5i_d2i_EINFO (krb5_etype_info_entry **a, unsigned char *const *pp, long length)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    ASN1_OCTET_STRING *a1os = 0;
    M_ASN1_D2I_vars(a, krb5_etype_info_entry *, krb5i_EINFO_new);

    a1int = ASN1_INTEGER_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1os = ASN1_OCTET_STRING_new();
    if (!a1int || !a1gs || !a1os) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }

    M_ASN1_D2I_Init();
    M_ASN1_D2I_start_sequence();
    M_ASN1_D2I_get_EXP_opt(a1int, d2i_ASN1_INTEGER, 0);
    M_ASN1_D2I_get_EXP_opt(a1gs, d2i_ASN1_GENERALSTRING, 1);
    M_ASN1_D2I_get_EXP_opt(a1os, d2i_ASN1_OCTET_STRING, 2);
    ret->etype = ASN1_INTEGER_get(a1int);
    ret->length= ASN1_STRING_length(a1gs);
    ret->salt = ASN1_STRING_data(a1gs);
    ret->s2kparams.length = ASN1_STRING_length(a1os);
    ret->s2kparams.data = ASN1_STRING_data(a1os);
	
    free(a1gs); a1gs = 0;	/* in place of ASN1_GENERALSTRING_free(a1gs); */
    free(a1os); a1os = 0;	/* in place of ASN1_OCTET_STRING_fre(a1os); */
    ASN1_INTEGER_free(a1int); a1int = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (a1int) ASN1_INTEGER_free(a1int);
    if (a1gs) ASN1_GENERALSTRING_free(a1gs);
    if (a1os) ASN1_OCTET_STRING_free(a1os);
    if (ret && (!a || *a != ret)) krb5i_EINFO_free(ret);
    if (a) *a = 0;
    return NULL;
}

STACK *
krb5i_get_einfo_s(STACK  **nlp, const unsigned char **pp, long length)
{
    STACK *nl;

    nl = ASN1_seq_unpack(*pp, length,
	(d2i_of_void *) krb5i_d2i_EINFO,
	(void(*)(void*))krb5i_EINFO_free);
    if (!nl) {
	return 0;
    }
    *pp += length;
    *nlp = nl;
    return nl;
}

krb5_etype_info_entry **
krb5i_SEQ_EINFO_dummy()
{
    static krb5_etype_info_entry *foo[1];
    return foo;
}

void
krb5i_SEQ_EINFO_free(krb5_etype_info_entry ** a)
{
    int i;
    krb5_etype_info_entry *ka;
    if (!a) return;
    for (i = 0; (ka = a[i]); ++i)
	krb5i_EINFO_free(ka);
    free(a);
}

krb5_etype_info_entry **
krb5i_d2i_SEQ_EINFO (krb5_etype_info_entry***a, const unsigned char **pp, long length)
{
    STACK *nl = 0;
    krb5_etype_info_entry *ka = 0;
    int count, i;
    M_ASN1_D2I_vars(a, krb5_etype_info_entry **, krb5i_SEQ_EINFO_dummy);
    ret = 0;

    M_ASN1_D2I_Init();
    M_ASN1_D2I_begin();

    M_ASN1_D2I_get(nl, krb5i_get_einfo_s);
    
    count = sk_num(nl);
    if (count < 0) count = 0;
    ret = (krb5_etype_info_entry **) malloc(sizeof *ret * (count+1));
    if (!ret) {
	c.line = __LINE__; c.error = ERR_R_MALLOC_FAILURE;
	goto err;
    }
    memset(ret, 0, sizeof *ret * (count+1));
    i = 0;
    while ((ka = (krb5_etype_info_entry *)sk_shift(nl))) {
	ret[i] = ka;
	++i;
    }
    ret[i] = 0;

    sk_free(nl); nl = 0;
    M_ASN1_D2I_Finish_2(a);
err:
    ASN1_MAC_H_err(902, c.error, c.line);
    if (nl) sk_pop_free(nl, (void(*)(void*))krb5i_EINFO_free);
    if (ret && *a != ret) krb5i_SEQ_EINFO_free(ret);
    *a = 0;
    return NULL;
}

/*
PA-ENC-TS-ENC ::= SEQUENCE {
	patimestamp[0]		KerberosTime,	-- client's time
	pausec[1]		INTEGER OPTIONAL
}
*/

int
krb5i_i2d_PAENCTS (krb5_pa_enc_ts *a, unsigned char **pp)
{
    ASN1_INTEGER *a1int = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
    int v0 = 0, v1 = 0;
    M_ASN1_I2D_vars(a);
    a1int = ASN1_INTEGER_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    if (!a1int || !a1gt) {
	if (a1int) ASN1_INTEGER_free(a1int);
	if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
	return 0;
    }
    ASN1_GENERALIZEDTIME_set(a1gt, a->patimestamp);
    M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 0, v0);
    if (a->pausec) {
	ASN1_INTEGER_set(a1int, a->pausec);
	M_ASN1_I2D_len_EXP_opt(a1int, i2d_ASN1_INTEGER, 1, v1);
    }
#if 1
    r=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    if (!pp || !*pp) {
	ASN1_GENERALIZEDTIME_free(a1gt);
	ASN1_INTEGER_free(a1int);
	return r;
    }
    p = *pp;
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
    M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 0, v0); /* req */
    if (a->pausec)
	M_ASN1_I2D_put_EXP_opt(a1int, i2d_ASN1_INTEGER, 1, v1);

    ASN1_GENERALIZEDTIME_free(a1gt);
    ASN1_INTEGER_free(a1int);
    M_ASN1_I2D_finish();
}

krb5_error_code
decode_krb5_padata_sequence(krb5_data *data,
    krb5_pa_data ***out)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_pa_data **a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_SEQ_PADATA(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
decode_krb5_etype_info(krb5_data *data,
    krb5_etype_info_entry ***out)
{
    long length = data->length;
    const unsigned char *p = data->data;
    krb5_etype_info_entry **a = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    *out = 0;
    if (!krb5i_d2i_SEQ_EINFO(&a, &p, length)) {
	unsigned long er = ERR_peek_error();
	int code = ASN1_PARSE_ERROR;
	return code;
    }
    *out = a;
    return 0;
}

krb5_error_code
encode_krb5_enc_data(const krb5_enc_data *ed, krb5_data **datap)
{
    int len;
    int code;
    unsigned char *p, *q = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;
    for (;;) {
	p = q;
	len = krb5i_i2d_ENC_DATA((krb5_enc_data *) ed, &p);
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
    return code;
}

krb5_error_code
encode_krb5_pa_enc_ts(const krb5_pa_enc_ts *ts, krb5_data **datap)
{
    int len;
    int code;
    unsigned char *p, *q = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;
    for (;;) {
	p = q;
	len = krb5i_i2d_PAENCTS((krb5_pa_enc_ts *) ts, &p);
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
    return code;
}
