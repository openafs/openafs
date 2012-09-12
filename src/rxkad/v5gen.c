/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_krb5int32(unsigned char *p, size_t len, const krb5int32 * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_integer(p, len, data, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_krb5int32(const unsigned char *p, size_t len, krb5int32 * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Integer, &Top_datalen, &l);
	if (e == 0 && Top_type != PRIM) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	e = der_get_integer(p, len, data, &l);
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_krb5int32(data);
    return e;
}

void
free_krb5int32(krb5int32 * data)
{
}

size_t
length_krb5int32(const krb5int32 * data)
{
    size_t ret = 0;
    ret += der_length_integer(data);
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_krb5int32(const krb5int32 * from, krb5int32 * to)
{
    memset(to, 0, sizeof(*to));
    *(to) = *(from);
    return 0;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_Ticket(unsigned char *p, size_t len, const Ticket * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* enc-part */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_EncryptedData(p, len, &(data)->enc_part, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* sname */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_PrincipalName(p, len, &(data)->sname, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* realm */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_Realm(p, len, &(data)->realm, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* tkt-vno */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_krb5int32(p, len, &(data)->tkt_vno, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    e = der_put_length_and_tag(p, len, ret, ASN1_C_APPL, CONS, 1, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_Ticket(const unsigned char *p, size_t len, Ticket * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_APPL, &Top_type, 1, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t Top_Tag_datalen, Top_Tag_oldlen;
	    Der_type Top_Tag_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_Tag_type, UT_Sequence, &Top_Tag_datalen, &l);
	    if (e == 0 && Top_Tag_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    Top_Tag_oldlen = len;
	    if (Top_Tag_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = Top_Tag_datalen;
	    {
		size_t tkt_vno_datalen, tkt_vno_oldlen;
		Der_type tkt_vno_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &tkt_vno_type, 0, &tkt_vno_datalen, &l);
		if (e == 0 && tkt_vno_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		tkt_vno_oldlen = len;
		if (tkt_vno_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = tkt_vno_datalen;
		e = decode_krb5int32(p, len, &(data)->tkt_vno, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = tkt_vno_oldlen - tkt_vno_datalen;
	    }
	    {
		size_t realm_datalen, realm_oldlen;
		Der_type realm_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &realm_type, 1, &realm_datalen, &l);
		if (e == 0 && realm_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		realm_oldlen = len;
		if (realm_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = realm_datalen;
		e = decode_Realm(p, len, &(data)->realm, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = realm_oldlen - realm_datalen;
	    }
	    {
		size_t sname_datalen, sname_oldlen;
		Der_type sname_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &sname_type, 2, &sname_datalen, &l);
		if (e == 0 && sname_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		sname_oldlen = len;
		if (sname_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = sname_datalen;
		e = decode_PrincipalName(p, len, &(data)->sname, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = sname_oldlen - sname_datalen;
	    }
	    {
		size_t enc_part_datalen, enc_part_oldlen;
		Der_type enc_part_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &enc_part_type, 3, &enc_part_datalen, &l);
		if (e == 0 && enc_part_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		enc_part_oldlen = len;
		if (enc_part_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = enc_part_datalen;
		e = decode_EncryptedData(p, len, &(data)->enc_part, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = enc_part_oldlen - enc_part_datalen;
	    }
	    len = Top_Tag_oldlen - Top_Tag_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_Ticket(data);
    return e;
}

void
free_Ticket(Ticket * data)
{
    free_krb5int32(&(data)->tkt_vno);
    free_Realm(&(data)->realm);
    free_PrincipalName(&(data)->sname);
    free_EncryptedData(&(data)->enc_part);
}

size_t
length_Ticket(const Ticket * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_krb5int32(&(data)->tkt_vno);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_Realm(&(data)->realm);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_PrincipalName(&(data)->sname);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_EncryptedData(&(data)->enc_part);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_Ticket(const Ticket * from, Ticket * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_krb5int32(&(from)->tkt_vno, &(to)->tkt_vno))
	goto fail;
    if (copy_Realm(&(from)->realm, &(to)->realm))
	goto fail;
    if (copy_PrincipalName(&(from)->sname, &(to)->sname))
	goto fail;
    if (copy_EncryptedData(&(from)->enc_part, &(to)->enc_part))
	goto fail;
    return 0;
fail:
    free_Ticket(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_AuthorizationDataElement(unsigned char *p, size_t len, const AuthorizationDataElement * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* ad-data */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = der_put_octet_string(p, len, &(data)->ad_data, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_OctetString, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* ad-type */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_krb5int32(p, len, &(data)->ad_type, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_AuthorizationDataElement(const unsigned char *p, size_t len, AuthorizationDataElement * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t ad_type_datalen, ad_type_oldlen;
	    Der_type ad_type_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &ad_type_type, 0, &ad_type_datalen, &l);
	    if (e == 0 && ad_type_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    ad_type_oldlen = len;
	    if (ad_type_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = ad_type_datalen;
	    e = decode_krb5int32(p, len, &(data)->ad_type, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    len = ad_type_oldlen - ad_type_datalen;
	}
	{
	    size_t ad_data_datalen, ad_data_oldlen;
	    Der_type ad_data_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &ad_data_type, 1, &ad_data_datalen, &l);
	    if (e == 0 && ad_data_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    ad_data_oldlen = len;
	    if (ad_data_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = ad_data_datalen;
	    {
		size_t ad_data_Tag_datalen, ad_data_Tag_oldlen;
		Der_type ad_data_Tag_type;
		e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &ad_data_Tag_type, UT_OctetString, &ad_data_Tag_datalen, &l);
		if (e == 0 && ad_data_Tag_type != PRIM) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		ad_data_Tag_oldlen = len;
		if (ad_data_Tag_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = ad_data_Tag_datalen;
		e = der_get_octet_string(p, len, &(data)->ad_data, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = ad_data_Tag_oldlen - ad_data_Tag_datalen;
	    }
	    len = ad_data_oldlen - ad_data_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_AuthorizationDataElement(data);
    return e;
}

void
free_AuthorizationDataElement(AuthorizationDataElement * data)
{
    free_krb5int32(&(data)->ad_type);
    der_free_octet_string(&(data)->ad_data);
}

size_t
length_AuthorizationDataElement(const AuthorizationDataElement * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_krb5int32(&(data)->ad_type);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += der_length_octet_string(&(data)->ad_data);
	ret += 1 + der_length_len(ret);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_AuthorizationDataElement(const AuthorizationDataElement * from, AuthorizationDataElement * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_krb5int32(&(from)->ad_type, &(to)->ad_type))
	goto fail;
    if (der_copy_octet_string(&(from)->ad_data, &(to)->ad_data))
	goto fail;
    return 0;
fail:
    free_AuthorizationDataElement(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_EncryptedData(unsigned char *p, size_t len, const EncryptedData * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* cipher */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = der_put_octet_string(p, len, &(data)->cipher, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_OctetString, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* kvno */
    if ((data)->kvno) {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_krb5int32(p, len, (data)->kvno, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* etype */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_ENCTYPE(p, len, &(data)->etype, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_EncryptedData(const unsigned char *p, size_t len, EncryptedData * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t etype_datalen, etype_oldlen;
	    Der_type etype_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &etype_type, 0, &etype_datalen, &l);
	    if (e == 0 && etype_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    etype_oldlen = len;
	    if (etype_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = etype_datalen;
	    e = decode_ENCTYPE(p, len, &(data)->etype, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    len = etype_oldlen - etype_datalen;
	}
	{
	    size_t kvno_datalen, kvno_oldlen;
	    Der_type kvno_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &kvno_type, 1, &kvno_datalen, &l);
	    if (e == 0 && kvno_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e) {
		(data)->kvno = NULL;
	    } else {
		(data)->kvno = calloc(1, sizeof(*(data)->kvno));
		if ((data)->kvno == NULL) {
		    e = ENOMEM;
		    goto fail;
		}
		p += l;
		len -= l;
		ret += l;
		kvno_oldlen = len;
		if (kvno_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = kvno_datalen;
		e = decode_krb5int32(p, len, (data)->kvno, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = kvno_oldlen - kvno_datalen;
	    }
	}
	{
	    size_t cipher_datalen, cipher_oldlen;
	    Der_type cipher_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &cipher_type, 2, &cipher_datalen, &l);
	    if (e == 0 && cipher_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    cipher_oldlen = len;
	    if (cipher_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = cipher_datalen;
	    {
		size_t cipher_Tag_datalen, cipher_Tag_oldlen;
		Der_type cipher_Tag_type;
		e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &cipher_Tag_type, UT_OctetString, &cipher_Tag_datalen, &l);
		if (e == 0 && cipher_Tag_type != PRIM) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		cipher_Tag_oldlen = len;
		if (cipher_Tag_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = cipher_Tag_datalen;
		e = der_get_octet_string(p, len, &(data)->cipher, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = cipher_Tag_oldlen - cipher_Tag_datalen;
	    }
	    len = cipher_oldlen - cipher_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_EncryptedData(data);
    return e;
}

void
free_EncryptedData(EncryptedData * data)
{
    free_ENCTYPE(&(data)->etype);
    if ((data)->kvno) {
	free_krb5int32((data)->kvno);
	free((data)->kvno);
	(data)->kvno = NULL;
    }
    der_free_octet_string(&(data)->cipher);
}

size_t
length_EncryptedData(const EncryptedData * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_ENCTYPE(&(data)->etype);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    if ((data)->kvno) {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_krb5int32((data)->kvno);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    } {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += der_length_octet_string(&(data)->cipher);
	ret += 1 + der_length_len(ret);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_EncryptedData(const EncryptedData * from, EncryptedData * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_ENCTYPE(&(from)->etype, &(to)->etype))
	goto fail;
    if ((from)->kvno) {
	(to)->kvno = malloc(sizeof(*(to)->kvno));
	if ((to)->kvno == NULL)
	    goto fail;
	if (copy_krb5int32((from)->kvno, (to)->kvno))
	    goto fail;
    } else
	(to)->kvno = NULL;
    if (der_copy_octet_string(&(from)->cipher, &(to)->cipher))
	goto fail;
    return 0;
fail:
    free_EncryptedData(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_PrincipalName(unsigned char *p, size_t len, const PrincipalName * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
/* name-string */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	for (i = (&(data)->name_string)->len - 1; i >= 0; --i) {
	    size_t name_string_tag_tag_for_oldret = ret;
	    ret = 0;
	    e = der_put_general_string(p, len, &(&(data)->name_string)->val[i], &l);
	    if (e)
		return e;
	    p -= l;
	    len -= l;
	    ret += l;

	    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_GeneralString, &l);
	    if (e)
		return e;
	    p -= l;
	    len -= l;
	    ret += l;

	    ret += name_string_tag_tag_for_oldret;
	}
	e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* name-type */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_NAME_TYPE(p, len, &(data)->name_type, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_PrincipalName(const unsigned char *p, size_t len, PrincipalName * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t name_type_datalen, name_type_oldlen;
	    Der_type name_type_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &name_type_type, 0, &name_type_datalen, &l);
	    if (e == 0 && name_type_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    name_type_oldlen = len;
	    if (name_type_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = name_type_datalen;
	    e = decode_NAME_TYPE(p, len, &(data)->name_type, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    len = name_type_oldlen - name_type_datalen;
	}
	{
	    size_t name_string_datalen, name_string_oldlen;
	    Der_type name_string_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &name_string_type, 1, &name_string_datalen, &l);
	    if (e == 0 && name_string_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    name_string_oldlen = len;
	    if (name_string_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = name_string_datalen;
	    {
		size_t name_string_Tag_datalen, name_string_Tag_oldlen;
		Der_type name_string_Tag_type;
		e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &name_string_Tag_type, UT_Sequence, &name_string_Tag_datalen, &l);
		if (e == 0 && name_string_Tag_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		name_string_Tag_oldlen = len;
		if (name_string_Tag_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = name_string_Tag_datalen;
		{
		    size_t name_string_Tag_Tag_origlen = len;
		    size_t name_string_Tag_Tag_oldret = ret;
		    size_t name_string_Tag_Tag_olen = 0;
		    void *name_string_Tag_Tag_tmp;
		    ret = 0;
		    (&(data)->name_string)->len = 0;
		    (&(data)->name_string)->val = NULL;
		    while (ret < name_string_Tag_Tag_origlen) {
			size_t name_string_Tag_Tag_nlen = name_string_Tag_Tag_olen + sizeof(*((&(data)->name_string)->val));
			if (name_string_Tag_Tag_olen > name_string_Tag_Tag_nlen) {
			    e = ASN1_OVERFLOW;
			    goto fail;
			}
			name_string_Tag_Tag_olen = name_string_Tag_Tag_nlen;
			name_string_Tag_Tag_tmp = realloc((&(data)->name_string)->val, name_string_Tag_Tag_olen);
			if (name_string_Tag_Tag_tmp == NULL) {
			    e = ENOMEM;
			    goto fail;
			}
			(&(data)->name_string)->val = name_string_Tag_Tag_tmp;
			{
			    size_t name_string_Tag_Tag_s_of_datalen, name_string_Tag_Tag_s_of_oldlen;
			    Der_type name_string_Tag_Tag_s_of_type;
			    e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &name_string_Tag_Tag_s_of_type, UT_GeneralString, &name_string_Tag_Tag_s_of_datalen, &l);
			    if (e == 0 && name_string_Tag_Tag_s_of_type != PRIM) {
				e = ASN1_BAD_ID;
			    }
			    if (e)
				goto fail;
			    p += l;
			    len -= l;
			    ret += l;
			    name_string_Tag_Tag_s_of_oldlen = len;
			    if (name_string_Tag_Tag_s_of_datalen > len) {
				e = ASN1_OVERRUN;
				goto fail;
			    }
			    len = name_string_Tag_Tag_s_of_datalen;
			    e = der_get_general_string(p, len, &(&(data)->name_string)->val[(&(data)->name_string)->len], &l);
			    if (e)
				goto fail;
			    p += l;
			    len -= l;
			    ret += l;
			    len = name_string_Tag_Tag_s_of_oldlen - name_string_Tag_Tag_s_of_datalen;
			}
			(&(data)->name_string)->len++;
			len = name_string_Tag_Tag_origlen - ret;
		    }
		    ret += name_string_Tag_Tag_oldret;
		}
		len = name_string_Tag_oldlen - name_string_Tag_datalen;
	    }
	    len = name_string_oldlen - name_string_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_PrincipalName(data);
    return e;
}

void
free_PrincipalName(PrincipalName * data)
{
    free_NAME_TYPE(&(data)->name_type);
    while ((&(data)->name_string)->len) {
	der_free_general_string(&(&(data)->name_string)->val[(&(data)->name_string)->len - 1]);
	(&(data)->name_string)->len--;
    }
    free((&(data)->name_string)->val);
    (&(data)->name_string)->val = NULL;
}

size_t
length_PrincipalName(const PrincipalName * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_NAME_TYPE(&(data)->name_type);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	{
	    int name_string_tag_tag_oldret = ret;
	    int i;
	    ret = 0;
	    for (i = (&(data)->name_string)->len - 1; i >= 0; --i) {
		int name_string_tag_tag_for_oldret = ret;
		ret = 0;
		ret += der_length_general_string(&(&(data)->name_string)->val[i]);
		ret += 1 + der_length_len(ret);
		ret += name_string_tag_tag_for_oldret;
	    }
	    ret += name_string_tag_tag_oldret;
	}
	ret += 1 + der_length_len(ret);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_PrincipalName(const PrincipalName * from, PrincipalName * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_NAME_TYPE(&(from)->name_type, &(to)->name_type))
	goto fail;
    if (((&(to)->name_string)->val = malloc((&(from)->name_string)->len * sizeof(*(&(to)->name_string)->val))) == NULL && (&(from)->name_string)->len != 0)
	goto fail;
    for ((&(to)->name_string)->len = 0; (&(to)->name_string)->len < (&(from)->name_string)->len; (&(to)->name_string)->len++) {
	if (der_copy_general_string(&(&(from)->name_string)->val[(&(to)->name_string)->len], &(&(to)->name_string)->val[(&(to)->name_string)->len]))
	    goto fail;
    }
    return 0;
fail:
    free_PrincipalName(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_HostAddresses(unsigned char *p, size_t len, const HostAddresses * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    for (i = (data)->len - 1; i >= 0; --i) {
	size_t Top_tag_for_oldret = ret;
	ret = 0;
	e = encode_HostAddress(p, len, &(data)->val[i], &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_for_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_HostAddresses(const unsigned char *p, size_t len, HostAddresses * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t Top_Tag_origlen = len;
	    size_t Top_Tag_oldret = ret;
	    size_t Top_Tag_olen = 0;
	    void *Top_Tag_tmp;
	    ret = 0;
	    (data)->len = 0;
	    (data)->val = NULL;
	    while (ret < Top_Tag_origlen) {
		size_t Top_Tag_nlen = Top_Tag_olen + sizeof(*((data)->val));
		if (Top_Tag_olen > Top_Tag_nlen) {
		    e = ASN1_OVERFLOW;
		    goto fail;
		}
		Top_Tag_olen = Top_Tag_nlen;
		Top_Tag_tmp = realloc((data)->val, Top_Tag_olen);
		if (Top_Tag_tmp == NULL) {
		    e = ENOMEM;
		    goto fail;
		}
		(data)->val = Top_Tag_tmp;
		e = decode_HostAddress(p, len, &(data)->val[(data)->len], &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		(data)->len++;
		len = Top_Tag_origlen - ret;
	    }
	    ret += Top_Tag_oldret;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_HostAddresses(data);
    return e;
}

void
free_HostAddresses(HostAddresses * data)
{
    while ((data)->len) {
	free_HostAddress(&(data)->val[(data)->len - 1]);
	(data)->len--;
    }
    free((data)->val);
    (data)->val = NULL;
}

size_t
length_HostAddresses(const HostAddresses * data)
{
    size_t ret = 0;
    {
	int Top_tag_oldret = ret;
	int i;
	ret = 0;
	for (i = (data)->len - 1; i >= 0; --i) {
	    int Top_tag_for_oldret = ret;
	    ret = 0;
	    ret += length_HostAddress(&(data)->val[i]);
	    ret += Top_tag_for_oldret;
	}
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_HostAddresses(const HostAddresses * from, HostAddresses * to)
{
    memset(to, 0, sizeof(*to));
    if (((to)->val = malloc((from)->len * sizeof(*(to)->val))) == NULL && (from)->len != 0)
	goto fail;
    for ((to)->len = 0; (to)->len < (from)->len; (to)->len++) {
	if (copy_HostAddress(&(from)->val[(to)->len], &(to)->val[(to)->len]))
	    goto fail;
    }
    return 0;
fail:
    free_HostAddresses(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_HostAddress(unsigned char *p, size_t len, const HostAddress * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* address */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = der_put_octet_string(p, len, &(data)->address, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_OctetString, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* addr-type */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_krb5int32(p, len, &(data)->addr_type, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_HostAddress(const unsigned char *p, size_t len, HostAddress * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t addr_type_datalen, addr_type_oldlen;
	    Der_type addr_type_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &addr_type_type, 0, &addr_type_datalen, &l);
	    if (e == 0 && addr_type_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    addr_type_oldlen = len;
	    if (addr_type_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = addr_type_datalen;
	    e = decode_krb5int32(p, len, &(data)->addr_type, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    len = addr_type_oldlen - addr_type_datalen;
	}
	{
	    size_t address_datalen, address_oldlen;
	    Der_type address_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &address_type, 1, &address_datalen, &l);
	    if (e == 0 && address_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    address_oldlen = len;
	    if (address_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = address_datalen;
	    {
		size_t address_Tag_datalen, address_Tag_oldlen;
		Der_type address_Tag_type;
		e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &address_Tag_type, UT_OctetString, &address_Tag_datalen, &l);
		if (e == 0 && address_Tag_type != PRIM) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		address_Tag_oldlen = len;
		if (address_Tag_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = address_Tag_datalen;
		e = der_get_octet_string(p, len, &(data)->address, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = address_Tag_oldlen - address_Tag_datalen;
	    }
	    len = address_oldlen - address_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_HostAddress(data);
    return e;
}

void
free_HostAddress(HostAddress * data)
{
    free_krb5int32(&(data)->addr_type);
    der_free_octet_string(&(data)->address);
}

size_t
length_HostAddress(const HostAddress * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_krb5int32(&(data)->addr_type);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += der_length_octet_string(&(data)->address);
	ret += 1 + der_length_len(ret);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_HostAddress(const HostAddress * from, HostAddress * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_krb5int32(&(from)->addr_type, &(to)->addr_type))
	goto fail;
    if (der_copy_octet_string(&(from)->address, &(to)->address))
	goto fail;
    return 0;
fail:
    free_HostAddress(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_AuthorizationData(unsigned char *p, size_t len, const AuthorizationData * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    for (i = (data)->len - 1; i >= 0; --i) {
	size_t Top_tag_for_oldret = ret;
	ret = 0;
	e = encode_AuthorizationDataElement(p, len, &(data)->val[i], &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_for_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_AuthorizationData(const unsigned char *p, size_t len, AuthorizationData * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t Top_Tag_origlen = len;
	    size_t Top_Tag_oldret = ret;
	    size_t Top_Tag_olen = 0;
	    void *Top_Tag_tmp;
	    ret = 0;
	    (data)->len = 0;
	    (data)->val = NULL;
	    while (ret < Top_Tag_origlen) {
		size_t Top_Tag_nlen = Top_Tag_olen + sizeof(*((data)->val));
		if (Top_Tag_olen > Top_Tag_nlen) {
		    e = ASN1_OVERFLOW;
		    goto fail;
		}
		Top_Tag_olen = Top_Tag_nlen;
		Top_Tag_tmp = realloc((data)->val, Top_Tag_olen);
		if (Top_Tag_tmp == NULL) {
		    e = ENOMEM;
		    goto fail;
		}
		(data)->val = Top_Tag_tmp;
		e = decode_AuthorizationDataElement(p, len, &(data)->val[(data)->len], &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		(data)->len++;
		len = Top_Tag_origlen - ret;
	    }
	    ret += Top_Tag_oldret;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_AuthorizationData(data);
    return e;
}

void
free_AuthorizationData(AuthorizationData * data)
{
    while ((data)->len) {
	free_AuthorizationDataElement(&(data)->val[(data)->len - 1]);
	(data)->len--;
    }
    free((data)->val);
    (data)->val = NULL;
}

size_t
length_AuthorizationData(const AuthorizationData * data)
{
    size_t ret = 0;
    {
	int Top_tag_oldret = ret;
	int i;
	ret = 0;
	for (i = (data)->len - 1; i >= 0; --i) {
	    int Top_tag_for_oldret = ret;
	    ret = 0;
	    ret += length_AuthorizationDataElement(&(data)->val[i]);
	    ret += Top_tag_for_oldret;
	}
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_AuthorizationData(const AuthorizationData * from, AuthorizationData * to)
{
    memset(to, 0, sizeof(*to));
    if (((to)->val = malloc((from)->len * sizeof(*(to)->val))) == NULL && (from)->len != 0)
	goto fail;
    for ((to)->len = 0; (to)->len < (from)->len; (to)->len++) {
	if (copy_AuthorizationDataElement(&(from)->val[(to)->len], &(to)->val[(to)->len]))
	    goto fail;
    }
    return 0;
fail:
    free_AuthorizationData(to);
    return ENOMEM;
}

int
add_AuthorizationData(AuthorizationData * data, const AuthorizationDataElement * element)
{
    int ret;
    void *ptr;

    ptr = realloc(data->val,
		  (data->len + 1) * sizeof(data->val[0]));
    if (ptr == NULL)
	return ENOMEM;
    data->val = ptr;

    ret = copy_AuthorizationDataElement(element, &data->val[data->len]);
    if (ret)
	return ret;
    data->len++;
    return 0;
}

int
remove_AuthorizationData(AuthorizationData * data, unsigned int element)
{
    void *ptr;

    if (data->len == 0 || element >= data->len)
	return ASN1_OVERRUN;
    free_AuthorizationDataElement(&data->val[element]);
    data->len--;
    if (element < data->len)
	memmove(&data->val[element], &data->val[element + 1],
		sizeof(data->val[0]) * data->len);
    ptr = realloc(data->val, data->len * sizeof(data->val[0]));
    if (ptr != NULL || data->len == 0)
	data->val = ptr;
    return 0;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_EncTicketPart(unsigned char *p, size_t len, const EncTicketPart * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* authorization-data */
    if ((data)->authorization_data) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_AuthorizationData(p, len, (data)->authorization_data, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 10, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* caddr */
    if ((data)->caddr) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_HostAddresses(p, len, (data)->caddr, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 9, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* renew-till */
    if ((data)->renew_till) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, (data)->renew_till, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 8, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* endtime */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, &(data)->endtime, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 7, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* starttime */
    if ((data)->starttime) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, (data)->starttime, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 6, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* authtime */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, &(data)->authtime, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 5, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* transited */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_TransitedEncoding(p, len, &(data)->transited, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 4, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* cname */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_PrincipalName(p, len, &(data)->cname, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 3, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* crealm */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_Realm(p, len, &(data)->crealm, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 2, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* key */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_EncryptionKey(p, len, &(data)->key, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
/* flags */
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	e = encode_TicketFlags(p, len, &(data)->flags, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    e = der_put_length_and_tag(p, len, ret, ASN1_C_APPL, CONS, 3, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_EncTicketPart(const unsigned char *p, size_t len, EncTicketPart * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_APPL, &Top_type, 3, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t Top_Tag_datalen, Top_Tag_oldlen;
	    Der_type Top_Tag_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_Tag_type, UT_Sequence, &Top_Tag_datalen, &l);
	    if (e == 0 && Top_Tag_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    Top_Tag_oldlen = len;
	    if (Top_Tag_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = Top_Tag_datalen;
	    {
		size_t flags_datalen, flags_oldlen;
		Der_type flags_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &flags_type, 0, &flags_datalen, &l);
		if (e == 0 && flags_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		flags_oldlen = len;
		if (flags_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = flags_datalen;
		e = decode_TicketFlags(p, len, &(data)->flags, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = flags_oldlen - flags_datalen;
	    }
	    {
		size_t key_datalen, key_oldlen;
		Der_type key_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &key_type, 1, &key_datalen, &l);
		if (e == 0 && key_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		key_oldlen = len;
		if (key_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = key_datalen;
		e = decode_EncryptionKey(p, len, &(data)->key, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = key_oldlen - key_datalen;
	    }
	    {
		size_t crealm_datalen, crealm_oldlen;
		Der_type crealm_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &crealm_type, 2, &crealm_datalen, &l);
		if (e == 0 && crealm_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		crealm_oldlen = len;
		if (crealm_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = crealm_datalen;
		e = decode_Realm(p, len, &(data)->crealm, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = crealm_oldlen - crealm_datalen;
	    }
	    {
		size_t cname_datalen, cname_oldlen;
		Der_type cname_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &cname_type, 3, &cname_datalen, &l);
		if (e == 0 && cname_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		cname_oldlen = len;
		if (cname_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = cname_datalen;
		e = decode_PrincipalName(p, len, &(data)->cname, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = cname_oldlen - cname_datalen;
	    }
	    {
		size_t transited_datalen, transited_oldlen;
		Der_type transited_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &transited_type, 4, &transited_datalen, &l);
		if (e == 0 && transited_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		transited_oldlen = len;
		if (transited_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = transited_datalen;
		e = decode_TransitedEncoding(p, len, &(data)->transited, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = transited_oldlen - transited_datalen;
	    }
	    {
		size_t authtime_datalen, authtime_oldlen;
		Der_type authtime_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &authtime_type, 5, &authtime_datalen, &l);
		if (e == 0 && authtime_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		authtime_oldlen = len;
		if (authtime_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = authtime_datalen;
		e = decode_KerberosTime(p, len, &(data)->authtime, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = authtime_oldlen - authtime_datalen;
	    }
	    {
		size_t starttime_datalen, starttime_oldlen;
		Der_type starttime_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &starttime_type, 6, &starttime_datalen, &l);
		if (e == 0 && starttime_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e) {
		    (data)->starttime = NULL;
		} else {
		    (data)->starttime = calloc(1, sizeof(*(data)->starttime));
		    if ((data)->starttime == NULL) {
			e = ENOMEM;
			goto fail;
		    }
		    p += l;
		    len -= l;
		    ret += l;
		    starttime_oldlen = len;
		    if (starttime_datalen > len) {
			e = ASN1_OVERRUN;
			goto fail;
		    }
		    len = starttime_datalen;
		    e = decode_KerberosTime(p, len, (data)->starttime, &l);
		    if (e)
			goto fail;
		    p += l;
		    len -= l;
		    ret += l;
		    len = starttime_oldlen - starttime_datalen;
		}
	    }
	    {
		size_t endtime_datalen, endtime_oldlen;
		Der_type endtime_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &endtime_type, 7, &endtime_datalen, &l);
		if (e == 0 && endtime_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		endtime_oldlen = len;
		if (endtime_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = endtime_datalen;
		e = decode_KerberosTime(p, len, &(data)->endtime, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = endtime_oldlen - endtime_datalen;
	    }
	    {
		size_t renew_till_datalen, renew_till_oldlen;
		Der_type renew_till_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &renew_till_type, 8, &renew_till_datalen, &l);
		if (e == 0 && renew_till_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e) {
		    (data)->renew_till = NULL;
		} else {
		    (data)->renew_till = calloc(1, sizeof(*(data)->renew_till));
		    if ((data)->renew_till == NULL) {
			e = ENOMEM;
			goto fail;
		    }
		    p += l;
		    len -= l;
		    ret += l;
		    renew_till_oldlen = len;
		    if (renew_till_datalen > len) {
			e = ASN1_OVERRUN;
			goto fail;
		    }
		    len = renew_till_datalen;
		    e = decode_KerberosTime(p, len, (data)->renew_till, &l);
		    if (e)
			goto fail;
		    p += l;
		    len -= l;
		    ret += l;
		    len = renew_till_oldlen - renew_till_datalen;
		}
	    }
	    {
		size_t caddr_datalen, caddr_oldlen;
		Der_type caddr_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &caddr_type, 9, &caddr_datalen, &l);
		if (e == 0 && caddr_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e) {
		    (data)->caddr = NULL;
		} else {
		    (data)->caddr = calloc(1, sizeof(*(data)->caddr));
		    if ((data)->caddr == NULL) {
			e = ENOMEM;
			goto fail;
		    }
		    p += l;
		    len -= l;
		    ret += l;
		    caddr_oldlen = len;
		    if (caddr_datalen > len) {
			e = ASN1_OVERRUN;
			goto fail;
		    }
		    len = caddr_datalen;
		    e = decode_HostAddresses(p, len, (data)->caddr, &l);
		    if (e)
			goto fail;
		    p += l;
		    len -= l;
		    ret += l;
		    len = caddr_oldlen - caddr_datalen;
		}
	    }
	    {
		size_t authorization_data_datalen, authorization_data_oldlen;
		Der_type authorization_data_type;
		e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &authorization_data_type, 10, &authorization_data_datalen, &l);
		if (e == 0 && authorization_data_type != CONS) {
		    e = ASN1_BAD_ID;
		}
		if (e) {
		    (data)->authorization_data = NULL;
		} else {
		    (data)->authorization_data = calloc(1, sizeof(*(data)->authorization_data));
		    if ((data)->authorization_data == NULL) {
			e = ENOMEM;
			goto fail;
		    }
		    p += l;
		    len -= l;
		    ret += l;
		    authorization_data_oldlen = len;
		    if (authorization_data_datalen > len) {
			e = ASN1_OVERRUN;
			goto fail;
		    }
		    len = authorization_data_datalen;
		    e = decode_AuthorizationData(p, len, (data)->authorization_data, &l);
		    if (e)
			goto fail;
		    p += l;
		    len -= l;
		    ret += l;
		    len = authorization_data_oldlen - authorization_data_datalen;
		}
	    }
	    len = Top_Tag_oldlen - Top_Tag_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_EncTicketPart(data);
    return e;
}

void
free_EncTicketPart(EncTicketPart * data)
{
    free_TicketFlags(&(data)->flags);
    free_EncryptionKey(&(data)->key);
    free_Realm(&(data)->crealm);
    free_PrincipalName(&(data)->cname);
    free_TransitedEncoding(&(data)->transited);
    free_KerberosTime(&(data)->authtime);
    if ((data)->starttime) {
	free_KerberosTime((data)->starttime);
	free((data)->starttime);
	(data)->starttime = NULL;
    }
    free_KerberosTime(&(data)->endtime);
    if ((data)->renew_till) {
	free_KerberosTime((data)->renew_till);
	free((data)->renew_till);
	(data)->renew_till = NULL;
    }
    if ((data)->caddr) {
	free_HostAddresses((data)->caddr);
	free((data)->caddr);
	(data)->caddr = NULL;
    }
    if ((data)->authorization_data) {
	free_AuthorizationData((data)->authorization_data);
	free((data)->authorization_data);
	(data)->authorization_data = NULL;
    }
}

size_t
length_EncTicketPart(const EncTicketPart * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_TicketFlags(&(data)->flags);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_EncryptionKey(&(data)->key);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_Realm(&(data)->crealm);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_PrincipalName(&(data)->cname);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_TransitedEncoding(&(data)->transited);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_KerberosTime(&(data)->authtime);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    if ((data)->starttime) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_KerberosTime((data)->starttime);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    } {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_KerberosTime(&(data)->endtime);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    if ((data)->renew_till) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_KerberosTime((data)->renew_till);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    if ((data)->caddr) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_HostAddresses((data)->caddr);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    if ((data)->authorization_data) {
	size_t Top_tag_tag_oldret = ret;
	ret = 0;
	ret += length_AuthorizationData((data)->authorization_data);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_EncTicketPart(const EncTicketPart * from, EncTicketPart * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_TicketFlags(&(from)->flags, &(to)->flags))
	goto fail;
    if (copy_EncryptionKey(&(from)->key, &(to)->key))
	goto fail;
    if (copy_Realm(&(from)->crealm, &(to)->crealm))
	goto fail;
    if (copy_PrincipalName(&(from)->cname, &(to)->cname))
	goto fail;
    if (copy_TransitedEncoding(&(from)->transited, &(to)->transited))
	goto fail;
    if (copy_KerberosTime(&(from)->authtime, &(to)->authtime))
	goto fail;
    if ((from)->starttime) {
	(to)->starttime = malloc(sizeof(*(to)->starttime));
	if ((to)->starttime == NULL)
	    goto fail;
	if (copy_KerberosTime((from)->starttime, (to)->starttime))
	    goto fail;
    } else
	(to)->starttime = NULL;
    if (copy_KerberosTime(&(from)->endtime, &(to)->endtime))
	goto fail;
    if ((from)->renew_till) {
	(to)->renew_till = malloc(sizeof(*(to)->renew_till));
	if ((to)->renew_till == NULL)
	    goto fail;
	if (copy_KerberosTime((from)->renew_till, (to)->renew_till))
	    goto fail;
    } else
	(to)->renew_till = NULL;
    if ((from)->caddr) {
	(to)->caddr = malloc(sizeof(*(to)->caddr));
	if ((to)->caddr == NULL)
	    goto fail;
	if (copy_HostAddresses((from)->caddr, (to)->caddr))
	    goto fail;
    } else
	(to)->caddr = NULL;
    if ((from)->authorization_data) {
	(to)->authorization_data = malloc(sizeof(*(to)->authorization_data));
	if ((to)->authorization_data == NULL)
	    goto fail;
	if (copy_AuthorizationData((from)->authorization_data, (to)->authorization_data))
	    goto fail;
    } else
	(to)->authorization_data = NULL;
    return 0;
fail:
    free_EncTicketPart(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_KerberosTime(unsigned char *p, size_t len, const KerberosTime * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_generalized_time(p, len, data, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_GeneralizedTime, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_KerberosTime(const unsigned char *p, size_t len, KerberosTime * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_GeneralizedTime, &Top_datalen, &l);
	if (e == 0 && Top_type != PRIM) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	e = der_get_generalized_time(p, len, data, &l);
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_KerberosTime(data);
    return e;
}

void
free_KerberosTime(KerberosTime * data)
{
}

size_t
length_KerberosTime(const KerberosTime * data)
{
    size_t ret = 0;
    ret += der_length_generalized_time(data);
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_KerberosTime(const KerberosTime * from, KerberosTime * to)
{
    memset(to, 0, sizeof(*to));
    *(to) = *(from);
    return 0;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_TransitedEncoding(unsigned char *p, size_t len, const TransitedEncoding * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* contents */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = der_put_octet_string(p, len, &(data)->contents, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_OctetString, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* tr-type */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_krb5int32(p, len, &(data)->tr_type, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_TransitedEncoding(const unsigned char *p, size_t len, TransitedEncoding * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t tr_type_datalen, tr_type_oldlen;
	    Der_type tr_type_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &tr_type_type, 0, &tr_type_datalen, &l);
	    if (e == 0 && tr_type_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    tr_type_oldlen = len;
	    if (tr_type_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = tr_type_datalen;
	    e = decode_krb5int32(p, len, &(data)->tr_type, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    len = tr_type_oldlen - tr_type_datalen;
	}
	{
	    size_t contents_datalen, contents_oldlen;
	    Der_type contents_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &contents_type, 1, &contents_datalen, &l);
	    if (e == 0 && contents_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    contents_oldlen = len;
	    if (contents_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = contents_datalen;
	    {
		size_t contents_Tag_datalen, contents_Tag_oldlen;
		Der_type contents_Tag_type;
		e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &contents_Tag_type, UT_OctetString, &contents_Tag_datalen, &l);
		if (e == 0 && contents_Tag_type != PRIM) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		contents_Tag_oldlen = len;
		if (contents_Tag_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = contents_Tag_datalen;
		e = der_get_octet_string(p, len, &(data)->contents, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = contents_Tag_oldlen - contents_Tag_datalen;
	    }
	    len = contents_oldlen - contents_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_TransitedEncoding(data);
    return e;
}

void
free_TransitedEncoding(TransitedEncoding * data)
{
    free_krb5int32(&(data)->tr_type);
    der_free_octet_string(&(data)->contents);
}

size_t
length_TransitedEncoding(const TransitedEncoding * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_krb5int32(&(data)->tr_type);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += der_length_octet_string(&(data)->contents);
	ret += 1 + der_length_len(ret);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_TransitedEncoding(const TransitedEncoding * from, TransitedEncoding * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_krb5int32(&(from)->tr_type, &(to)->tr_type))
	goto fail;
    if (der_copy_octet_string(&(from)->contents, &(to)->contents))
	goto fail;
    return 0;
fail:
    free_TransitedEncoding(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_EncryptionKey(unsigned char *p, size_t len, const EncryptionKey * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

/* keyvalue */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = der_put_octet_string(p, len, &(data)->keyvalue, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_OctetString, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 1, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
/* keytype */
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	e = encode_krb5int32(p, len, &(data)->keytype, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	e = der_put_length_and_tag(p, len, ret, ASN1_C_CONTEXT, CONS, 0, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

	ret += Top_tag_oldret;
    }
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, CONS, UT_Sequence, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_EncryptionKey(const unsigned char *p, size_t len, EncryptionKey * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Sequence, &Top_datalen, &l);
	if (e == 0 && Top_type != CONS) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    size_t keytype_datalen, keytype_oldlen;
	    Der_type keytype_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &keytype_type, 0, &keytype_datalen, &l);
	    if (e == 0 && keytype_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    keytype_oldlen = len;
	    if (keytype_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = keytype_datalen;
	    e = decode_krb5int32(p, len, &(data)->keytype, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    len = keytype_oldlen - keytype_datalen;
	}
	{
	    size_t keyvalue_datalen, keyvalue_oldlen;
	    Der_type keyvalue_type;
	    e = der_match_tag_and_length(p, len, ASN1_C_CONTEXT, &keyvalue_type, 1, &keyvalue_datalen, &l);
	    if (e == 0 && keyvalue_type != CONS) {
		e = ASN1_BAD_ID;
	    }
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    keyvalue_oldlen = len;
	    if (keyvalue_datalen > len) {
		e = ASN1_OVERRUN;
		goto fail;
	    }
	    len = keyvalue_datalen;
	    {
		size_t keyvalue_Tag_datalen, keyvalue_Tag_oldlen;
		Der_type keyvalue_Tag_type;
		e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &keyvalue_Tag_type, UT_OctetString, &keyvalue_Tag_datalen, &l);
		if (e == 0 && keyvalue_Tag_type != PRIM) {
		    e = ASN1_BAD_ID;
		}
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		keyvalue_Tag_oldlen = len;
		if (keyvalue_Tag_datalen > len) {
		    e = ASN1_OVERRUN;
		    goto fail;
		}
		len = keyvalue_Tag_datalen;
		e = der_get_octet_string(p, len, &(data)->keyvalue, &l);
		if (e)
		    goto fail;
		p += l;
		len -= l;
		ret += l;
		len = keyvalue_Tag_oldlen - keyvalue_Tag_datalen;
	    }
	    len = keyvalue_oldlen - keyvalue_datalen;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_EncryptionKey(data);
    return e;
}

void
free_EncryptionKey(EncryptionKey * data)
{
    free_krb5int32(&(data)->keytype);
    der_free_octet_string(&(data)->keyvalue);
}

size_t
length_EncryptionKey(const EncryptionKey * data)
{
    size_t ret = 0;
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += length_krb5int32(&(data)->keytype);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    {
	size_t Top_tag_oldret = ret;
	ret = 0;
	ret += der_length_octet_string(&(data)->keyvalue);
	ret += 1 + der_length_len(ret);
	ret += 1 + der_length_len(ret);
	ret += Top_tag_oldret;
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_EncryptionKey(const EncryptionKey * from, EncryptionKey * to)
{
    memset(to, 0, sizeof(*to));
    if (copy_krb5int32(&(from)->keytype, &(to)->keytype))
	goto fail;
    if (der_copy_octet_string(&(from)->keyvalue, &(to)->keyvalue))
	goto fail;
    return 0;
fail:
    free_EncryptionKey(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_TicketFlags(unsigned char *p, size_t len, const TicketFlags * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    {
	unsigned char c = 0;
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = c;
	len--;
	ret++;
	c = 0;
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = c;
	len--;
	ret++;
	c = 0;
	if ((data)->anonymous) {
	    c |= 1 << 1;
	}
	if ((data)->ok_as_delegate) {
	    c |= 1 << 2;
	}
	if ((data)->transited_policy_checked) {
	    c |= 1 << 3;
	}
	if ((data)->hw_authent) {
	    c |= 1 << 4;
	}
	if ((data)->pre_authent) {
	    c |= 1 << 5;
	}
	if ((data)->initial) {
	    c |= 1 << 6;
	}
	if ((data)->renewable) {
	    c |= 1 << 7;
	}
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = c;
	len--;
	ret++;
	c = 0;
	if ((data)->invalid) {
	    c |= 1 << 0;
	}
	if ((data)->postdated) {
	    c |= 1 << 1;
	}
	if ((data)->may_postdate) {
	    c |= 1 << 2;
	}
	if ((data)->proxy) {
	    c |= 1 << 3;
	}
	if ((data)->proxiable) {
	    c |= 1 << 4;
	}
	if ((data)->forwarded) {
	    c |= 1 << 5;
	}
	if ((data)->forwardable) {
	    c |= 1 << 6;
	}
	if ((data)->reserved) {
	    c |= 1 << 7;
	}
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = c;
	len--;
	ret++;
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = 0;
	len -= 1;
	ret += 1;
    }

    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_BitString, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_TicketFlags(const unsigned char *p, size_t len, TicketFlags * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_BitString, &Top_datalen, &l);
	if (e == 0 && Top_type != PRIM) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	if (len < 1)
	    return ASN1_OVERRUN;
	p++;
	len--;
	ret++;
	do {
	    if (len < 1)
		break;
	    (data)->reserved = (*p >> 7) & 1;
	    (data)->forwardable = (*p >> 6) & 1;
	    (data)->forwarded = (*p >> 5) & 1;
	    (data)->proxiable = (*p >> 4) & 1;
	    (data)->proxy = (*p >> 3) & 1;
	    (data)->may_postdate = (*p >> 2) & 1;
	    (data)->postdated = (*p >> 1) & 1;
	    (data)->invalid = (*p >> 0) & 1;
	    p++;
	    len--;
	    ret++;
	    if (len < 1)
		break;
	    (data)->renewable = (*p >> 7) & 1;
	    (data)->initial = (*p >> 6) & 1;
	    (data)->pre_authent = (*p >> 5) & 1;
	    (data)->hw_authent = (*p >> 4) & 1;
	    (data)->transited_policy_checked = (*p >> 3) & 1;
	    (data)->ok_as_delegate = (*p >> 2) & 1;
	    (data)->anonymous = (*p >> 1) & 1;
	} while (0);
	p += len;
	ret += len;
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_TicketFlags(data);
    return e;
}

void
free_TicketFlags(TicketFlags * data)
{
}

size_t
length_TicketFlags(const TicketFlags * data)
{
    size_t ret = 0;
    ret += 5;
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_TicketFlags(const TicketFlags * from, TicketFlags * to)
{
    memset(to, 0, sizeof(*to));
    *(to) = *(from);
    return 0;
}

unsigned
TicketFlags2int(TicketFlags f)
{
    unsigned r = 0;
    if (f.reserved)
	r |= (1U << 0);
    if (f.forwardable)
	r |= (1U << 1);
    if (f.forwarded)
	r |= (1U << 2);
    if (f.proxiable)
	r |= (1U << 3);
    if (f.proxy)
	r |= (1U << 4);
    if (f.may_postdate)
	r |= (1U << 5);
    if (f.postdated)
	r |= (1U << 6);
    if (f.invalid)
	r |= (1U << 7);
    if (f.renewable)
	r |= (1U << 8);
    if (f.initial)
	r |= (1U << 9);
    if (f.pre_authent)
	r |= (1U << 10);
    if (f.hw_authent)
	r |= (1U << 11);
    if (f.transited_policy_checked)
	r |= (1U << 12);
    if (f.ok_as_delegate)
	r |= (1U << 13);
    if (f.anonymous)
	r |= (1U << 14);
    return r;
}

TicketFlags
int2TicketFlags(unsigned n)
{
    TicketFlags flags;

    memset(&flags, 0, sizeof(flags));

    flags.reserved = (n >> 0) & 1;
    flags.forwardable = (n >> 1) & 1;
    flags.forwarded = (n >> 2) & 1;
    flags.proxiable = (n >> 3) & 1;
    flags.proxy = (n >> 4) & 1;
    flags.may_postdate = (n >> 5) & 1;
    flags.postdated = (n >> 6) & 1;
    flags.invalid = (n >> 7) & 1;
    flags.renewable = (n >> 8) & 1;
    flags.initial = (n >> 9) & 1;
    flags.pre_authent = (n >> 10) & 1;
    flags.hw_authent = (n >> 11) & 1;
    flags.transited_policy_checked = (n >> 12) & 1;
    flags.ok_as_delegate = (n >> 13) & 1;
    flags.anonymous = (n >> 14) & 1;
    return flags;
}





/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_Realm(unsigned char *p, size_t len, const Realm * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_general_string(p, len, data, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_GeneralString, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_Realm(const unsigned char *p, size_t len, Realm * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_GeneralString, &Top_datalen, &l);
	if (e == 0 && Top_type != PRIM) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	e = der_get_general_string(p, len, data, &l);
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_Realm(data);
    return e;
}

void
free_Realm(Realm * data)
{
    der_free_general_string(data);
}

size_t
length_Realm(const Realm * data)
{
    size_t ret = 0;
    ret += der_length_general_string(data);
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_Realm(const Realm * from, Realm * to)
{
    memset(to, 0, sizeof(*to));
    if (der_copy_general_string(from, to))
	goto fail;
    return 0;
fail:
    free_Realm(to);
    return ENOMEM;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_ENCTYPE(unsigned char *p, size_t len, const ENCTYPE * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    {
	int enumint = (int) *data;
	e = der_put_integer(p, len, &enumint, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

    }
    ;
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_ENCTYPE(const unsigned char *p, size_t len, ENCTYPE * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Integer, &Top_datalen, &l);
	if (e == 0 && Top_type != PRIM) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    int enumint;
	    e = der_get_integer(p, len, &enumint, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    *data = enumint;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_ENCTYPE(data);
    return e;
}

void
free_ENCTYPE(ENCTYPE * data)
{
}

size_t
length_ENCTYPE(const ENCTYPE * data)
{
    size_t ret = 0;
    {
	int enumint = *data;
	ret += der_length_integer(&enumint);
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_ENCTYPE(const ENCTYPE * from, ENCTYPE * to)
{
    memset(to, 0, sizeof(*to));
    *(to) = *(from);
    return 0;
}



/* Generated from ./krb5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <asn1_err.h>

int
encode_NAME_TYPE(unsigned char *p, size_t len, const NAME_TYPE * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    {
	int enumint = (int) *data;
	e = der_put_integer(p, len, &enumint, &l);
	if (e)
	    return e;
	p -= l;
	len -= l;
	ret += l;

    }
    ;
    e = der_put_length_and_tag(p, len, ret, ASN1_C_UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;

    *size = ret;
    return 0;
}

int
decode_NAME_TYPE(const unsigned char *p, size_t len, NAME_TYPE * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    {
	size_t Top_datalen, Top_oldlen;
	Der_type Top_type;
	e = der_match_tag_and_length(p, len, ASN1_C_UNIV, &Top_type, UT_Integer, &Top_datalen, &l);
	if (e == 0 && Top_type != PRIM) {
	    e = ASN1_BAD_ID;
	}
	if (e)
	    goto fail;
	p += l;
	len -= l;
	ret += l;
	Top_oldlen = len;
	if (Top_datalen > len) {
	    e = ASN1_OVERRUN;
	    goto fail;
	}
	len = Top_datalen;
	{
	    int enumint;
	    e = der_get_integer(p, len, &enumint, &l);
	    if (e)
		goto fail;
	    p += l;
	    len -= l;
	    ret += l;
	    *data = enumint;
	}
	len = Top_oldlen - Top_datalen;
    }
    if (size)
	*size = ret;
    return 0;
fail:
    free_NAME_TYPE(data);
    return e;
}

void
free_NAME_TYPE(NAME_TYPE * data)
{
}

size_t
length_NAME_TYPE(const NAME_TYPE * data)
{
    size_t ret = 0;
    {
	int enumint = *data;
	ret += der_length_integer(&enumint);
    }
    ret += 1 + der_length_len(ret);
    return ret;
}

int
copy_NAME_TYPE(const NAME_TYPE * from, NAME_TYPE * to)
{
    memset(to, 0, sizeof(*to));
    *(to) = *(from);
    return 0;
}
