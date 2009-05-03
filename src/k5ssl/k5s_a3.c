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

int krb5i_i2d_PRINCIPAL_NAME(krb5_principal, unsigned char **);
int krb5i_i2d_SEQ_HOSTADDRESS(krb5_address **, unsigned char **);
int krb5i_i2d_SEQ_AUTHDATA(const krb5_authdata **, unsigned char **);
int krb5i_BIT_STRING_set(const ASN1_BIT_STRING *, int);
int krb5i_i2d_TICKET(const krb5_ticket *, unsigned char **);
int krb5i_i2d_ENC_DATA(krb5_enc_data const *, unsigned char **);
void krb5i_put_application(unsigned char **, int, int);
int krb5i_i2d_ENCRYPTION_KEY(krb5_keyblock const *, unsigned char **);

/*
TransitedEncoding ::= SEQUENCE {
	tr-type[0]		INTEGER, -- must be registered
	contents[1]		OCTET STRING
}
*/

int
krb5i_i2d_TRANSITED_ENCODING(krb5_transited *a, unsigned char **pp)
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
    ASN1_INTEGER_set(a1int, a->tr_type);
    ASN1_STRING_set(a1os, a->tr_contents.data, a->tr_contents.length);
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
EncTicketPart ::= [APPLICATION 3] SEQUENCE {
	flags[0]		TicketFlags,
	key[1]			EncryptionKey,
	crealm[2]		Realm,
	cname[3]		PrincipalName,
	transited[4]		TransitedEncoding,
	authtime[5]		KerberosTime,
	starttime[6]		KerberosTime OPTIONAL,
	endtime[7]		KerberosTime,
	renew-till[8]		KerberosTime OPTIONAL,
	caddr[9]		HostAddresses OPTIONAL,
	authorization-data[10]	AuthorizationData OPTIONAL
}
*/

int
krb5i_i2d_ENCTICKETPART (krb5_enc_tkt_part *a, unsigned char **pp)
{
    ASN1_BIT_STRING *a1bs = 0;
    ASN1_GENERALSTRING *a1gs = 0;
    ASN1_GENERALIZEDTIME *a1gt = 0;
unsigned char *x;
    int v0 = 0, v1 = 0, v2 = 0, v3 = 0, v4 = 0, v5, v6 = 0, v7, v8 = 0, v9 = 0, v10 = 0;
    int r2;
    M_ASN1_I2D_vars(a);
    a1bs = ASN1_BIT_STRING_new();
    a1gs = ASN1_GENERALSTRING_new();
    a1gt = ASN1_GENERALIZEDTIME_new();
    if (!a1bs || !a1gs || !a1gt) {
	if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
	if (a1bs) ASN1_BIT_STRING_free(a1bs);
	if (a1gs) ASN1_GENERALSTRING_free(a1gs);
	return 0;
    }
    krb5i_BIT_STRING_set(a1bs, a->flags);
    M_ASN1_I2D_len_EXP_opt(a1bs, krb5i_i2d_ASN1_BIT_STRING, 0, v0);
    M_ASN1_I2D_len_EXP_opt(a->session, krb5i_i2d_ENCRYPTION_KEY, 1, v1);
    ASN1_STRING_set(a1gs, a->client->realm.data, a->client->realm.length);
    M_ASN1_I2D_len_EXP_opt(a1gs, i2d_ASN1_GENERALSTRING, 2, v2);
    M_ASN1_I2D_len_EXP_opt(a->client, krb5i_i2d_PRINCIPAL_NAME, 3, v3);
    M_ASN1_I2D_len_EXP_opt(&a->transited, krb5i_i2d_TRANSITED_ENCODING, 4, v4); /* req */
    ASN1_GENERALIZEDTIME_set(a1gt, a->times.authtime);
    M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 5, v5); /* req */
    if (a->times.starttime) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->times.starttime);
	M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 6, v6); /* req */
    }
    ASN1_GENERALIZEDTIME_set(a1gt, a->times.endtime);
    M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 7, v7); /* req */
    if (a->times.renew_till) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->times.renew_till);
	M_ASN1_I2D_len_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 8, v8);
    }
    if (a->caddrs) {
	M_ASN1_I2D_len_EXP_opt(a->caddrs, krb5i_i2d_SEQ_HOSTADDRESS, 9, v9);
    }
    if (a->authorization_data && *a->authorization_data) {
	M_ASN1_I2D_len_EXP_opt((const krb5_authdata **)a->authorization_data,
	    krb5i_i2d_SEQ_AUTHDATA, 10, v10);
    }
#if 1
    r2=ASN1_object_size(1, ret, V_ASN1_SEQUENCE);
    r=ASN1_object_size(0, r2, V_ASN1_SEQUENCE);	/* V_ASN1_APPLICATION ! */
    if (!pp || !*pp) {
	ASN1_GENERALIZEDTIME_free(a1gt);
	ASN1_BIT_STRING_free(a1bs);
	ASN1_GENERALSTRING_free(a1gs);
	return r;
    }
    p = *pp;
    krb5i_put_application(&p, r2, 3);
    ASN1_put_object(&p, 1, ret, V_ASN1_SEQUENCE, V_ASN1_UNIVERSAL);
#endif
x = p;
    M_ASN1_I2D_put_EXP_opt(a1bs, krb5i_i2d_ASN1_BIT_STRING, 0, v0);
#if 0
printf ("encoded form of this bit string was:\n");
bin_dump(x, p-x, 0);
#endif
    M_ASN1_I2D_put_EXP_opt(a->session, krb5i_i2d_ENCRYPTION_KEY, 1, v1);
    M_ASN1_I2D_put_EXP_opt(a1gs, i2d_ASN1_GENERALSTRING, 2, v2);
    M_ASN1_I2D_put_EXP_opt(a->client, krb5i_i2d_PRINCIPAL_NAME, 3, v3);
    M_ASN1_I2D_put_EXP_opt(&a->transited, krb5i_i2d_TRANSITED_ENCODING, 4, v4); /* req */
    ASN1_GENERALIZEDTIME_set(a1gt, a->times.authtime);
    M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 5, v5); /* req */
    if (a->times.starttime) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->times.starttime);
	M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 6, v6); /* req */
    }
    ASN1_GENERALIZEDTIME_set(a1gt, a->times.endtime);
    M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 7, v7); /* req */
    if (a->times.renew_till) {
	ASN1_GENERALIZEDTIME_set(a1gt, a->times.renew_till);
	M_ASN1_I2D_put_EXP_opt(a1gt, i2d_ASN1_GENERALIZEDTIME, 8, v8);
    }
    if (a->caddrs) {
	M_ASN1_I2D_put_EXP_opt(a->caddrs, krb5i_i2d_SEQ_HOSTADDRESS, 9, v9);
    }
    if (a->authorization_data && *a->authorization_data) {
	M_ASN1_I2D_put_EXP_opt((const krb5_authdata **)a->authorization_data,
	    krb5i_i2d_SEQ_AUTHDATA, 10, v10);
    }

    if (a1gt) ASN1_GENERALIZEDTIME_free(a1gt);
    if (a1bs) ASN1_BIT_STRING_free(a1bs);
    if (a1gs) ASN1_GENERALSTRING_free(a1gs);
    M_ASN1_I2D_finish();
}

krb5_error_code
encode_krb5_enc_tkt_part(const krb5_enc_tkt_part *tkt, krb5_data **datap)
{
    int len;
    int code;
    unsigned char *p, *q = 0;

    ERR_clear_error();	/* XXX ok to do this here? */
    code = 999;
    for (;;) {
	p = q;
	len = krb5i_i2d_ENCTICKETPART((krb5_enc_tkt_part *) tkt, &p);
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
