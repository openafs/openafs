/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_Ticket(unsigned char *p, size_t len, const Ticket * data,
	      size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	int oldret = ret;
	ret = 0;
	e = encode_EncryptedData(p, len, &(data)->enc_part, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 3, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_PrincipalName(p, len, &(data)->sname, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 2, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_Realm(p, len, &(data)->realm, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_integer(p, len, &(data)->tkt_vno, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    e = der_put_length_and_tag(p, len, ret, APPL, CONS, 1, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_Ticket(const unsigned char *p, size_t len, Ticket * data,
	      size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, APPL, CONS, 1, &reallen, &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence,
				     &reallen, &l);
	FORW;
	{
	    int dce_fix;
	    if ((dce_fix = fix_dce(reallen, &len)) < 0)
		return ASN1_BAD_FORMAT;
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_integer(p, len, &(data)->tkt_vno, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_Realm(p, len, &(data)->realm, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 2, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_PrincipalName(p, len, &(data)->sname, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 3, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_EncryptedData(p, len, &(data)->enc_part,
						 &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    if (dce_fix) {
		e = der_match_tag_and_length(p, len, (Der_class) 0,
					     (Der_type) 0, 0, &reallen, &l);
		FORW;
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
    free_Realm(&(data)->realm);
    free_PrincipalName(&(data)->sname);
    free_EncryptedData(&(data)->enc_part);
}

size_t
length_Ticket(const Ticket * data)
{
    size_t ret = 0;
    {
	int oldret = ret;
	ret = 0;
	ret += length_integer(&(data)->tkt_vno);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_Realm(&(data)->realm);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_PrincipalName(&(data)->sname);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_EncryptedData(&(data)->enc_part);
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_Ticket(const Ticket * from, Ticket * to)
{
    *(&(to)->tkt_vno) = *(&(from)->tkt_vno);
    if (copy_Realm(&(from)->realm, &(to)->realm))
	return ENOMEM;
    if (copy_PrincipalName(&(from)->sname, &(to)->sname))
	return ENOMEM;
    if (copy_EncryptedData(&(from)->enc_part, &(to)->enc_part))
	return ENOMEM;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_EncryptedData(unsigned char *p, size_t len, const EncryptedData * data,
		     size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	int oldret = ret;
	ret = 0;
	e = encode_octet_string(p, len, &(data)->cipher, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 2, &l);
	BACK;
	ret += oldret;
    }
    if ((data)->kvno) {
	int oldret = ret;
	ret = 0;
	e = encode_integer(p, len, (data)->kvno, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_ENCTYPE(p, len, &(data)->etype, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_EncryptedData(const unsigned char *p, size_t len, EncryptedData * data,
		     size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_ENCTYPE(p, len, &(data)->etype, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
	    if (e)
		(data)->kvno = NULL;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    (data)->kvno = malloc(sizeof(*(data)->kvno));
		    if ((data)->kvno == NULL)
			return ENOMEM;
		    e = decode_integer(p, len, (data)->kvno, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 2, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_octet_string(p, len, &(data)->cipher, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
	free((data)->kvno);
	(data)->kvno = NULL;
    }
    free_octet_string(&(data)->cipher);
}

size_t
length_EncryptedData(const EncryptedData * data)
{
    size_t ret = 0;
    {
	int oldret = ret;
	ret = 0;
	ret += length_ENCTYPE(&(data)->etype);
	ret += 1 + length_len(ret) + oldret;
    }
    if ((data)->kvno) {
	int oldret = ret;
	ret = 0;
	ret += length_integer((data)->kvno);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_octet_string(&(data)->cipher);
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_EncryptedData(const EncryptedData * from, EncryptedData * to)
{
    if (copy_ENCTYPE(&(from)->etype, &(to)->etype))
	return ENOMEM;
    if ((from)->kvno) {
	(to)->kvno = malloc(sizeof(*(to)->kvno));
	if ((to)->kvno == NULL)
	    return ENOMEM;
	*((to)->kvno) = *((from)->kvno);
    } else
	(to)->kvno = NULL;
    if (copy_octet_string(&(from)->cipher, &(to)->cipher))
	return ENOMEM;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_PrincipalName(unsigned char *p, size_t len, const PrincipalName * data,
		     size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	int oldret = ret;
	ret = 0;
	for (i = (&(data)->name_string)->len - 1; i >= 0; --i) {
	    int oldret = ret;
	    ret = 0;
	    e = encode_general_string(p, len, &(&(data)->name_string)->val[i],
				      &l);
	    BACK;
	    ret += oldret;
	}
	e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_NAME_TYPE(p, len, &(data)->name_type, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_PrincipalName(const unsigned char *p, size_t len, PrincipalName * data,
		     size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_NAME_TYPE(p, len, &(data)->name_type, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = der_match_tag_and_length(p, len, UNIV, CONS,
						 UT_Sequence, &reallen, &l);
		    FORW;
		    if (len < reallen)
			return ASN1_OVERRUN;
		    len = reallen;
		    {
			size_t origlen = len;
			int oldret = ret;
			ret = 0;
			(&(data)->name_string)->len = 0;
			(&(data)->name_string)->val = NULL;
			while (ret < origlen) {
			    (&(data)->name_string)->len++;
			    (&(data)->name_string)->val =
				realloc((&(data)->name_string)->val,
					sizeof(*((&(data)->name_string)->val))
					* (&(data)->name_string)->len);
			    e = decode_general_string(p, len,
						      &(&(data)->
							name_string)->
						      val[(&(data)->
							   name_string)->len -
							  1], &l);
			    FORW;
			    len = origlen - ret;
			}
			ret += oldret;
		    }
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
	free_general_string(&(&(data)->name_string)->
			    val[(&(data)->name_string)->len - 1]);
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
	int oldret = ret;
	ret = 0;
	ret += length_NAME_TYPE(&(data)->name_type);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	{
	    int oldret = ret;
	    int i;
	    ret = 0;
	    for (i = (&(data)->name_string)->len - 1; i >= 0; --i) {
		ret += length_general_string(&(&(data)->name_string)->val[i]);
	    }
	    ret += 1 + length_len(ret) + oldret;
	}
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_PrincipalName(const PrincipalName * from, PrincipalName * to)
{
    if (copy_NAME_TYPE(&(from)->name_type, &(to)->name_type))
	return ENOMEM;
    if (((&(to)->name_string)->val =
	 malloc((&(from)->name_string)->len *
		sizeof(*(&(to)->name_string)->val))) == NULL
	&& (&(from)->name_string)->len != 0)
	return ENOMEM;
    for ((&(to)->name_string)->len = 0;
	 (&(to)->name_string)->len < (&(from)->name_string)->len;
	 (&(to)->name_string)->len++) {
	if (copy_general_string
	    (&(&(from)->name_string)->val[(&(to)->name_string)->len],
	     &(&(to)->name_string)->val[(&(to)->name_string)->len]))
	    return ENOMEM;
    }
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_HostAddresses(unsigned char *p, size_t len, const HostAddresses * data,
		     size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    for (i = (data)->len - 1; i >= 0; --i) {
	int oldret = ret;
	ret = 0;
	e = encode_HostAddress(p, len, &(data)->val[i], &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_HostAddresses(const unsigned char *p, size_t len, HostAddresses * data,
		     size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    if (len < reallen)
	return ASN1_OVERRUN;
    len = reallen;
    {
	size_t origlen = len;
	int oldret = ret;
	ret = 0;
	(data)->len = 0;
	(data)->val = NULL;
	while (ret < origlen) {
	    (data)->len++;
	    (data)->val =
		realloc((data)->val, sizeof(*((data)->val)) * (data)->len);
	    e = decode_HostAddress(p, len, &(data)->val[(data)->len - 1], &l);
	    FORW;
	    len = origlen - ret;
	}
	ret += oldret;
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
	int oldret = ret;
	int i;
	ret = 0;
	for (i = (data)->len - 1; i >= 0; --i) {
	    ret += length_HostAddress(&(data)->val[i]);
	}
	ret += 1 + length_len(ret) + oldret;
    }
    return ret;
}

int
copy_HostAddresses(const HostAddresses * from, HostAddresses * to)
{
    if (((to)->val = malloc((from)->len * sizeof(*(to)->val))) == NULL
	&& (from)->len != 0)
	return ENOMEM;
    for ((to)->len = 0; (to)->len < (from)->len; (to)->len++) {
	if (copy_HostAddress(&(from)->val[(to)->len], &(to)->val[(to)->len]))
	    return ENOMEM;
    }
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_HostAddress(unsigned char *p, size_t len, const HostAddress * data,
		   size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	int oldret = ret;
	ret = 0;
	e = encode_octet_string(p, len, &(data)->address, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_integer(p, len, &(data)->addr_type, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_HostAddress(const unsigned char *p, size_t len, HostAddress * data,
		   size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_integer(p, len, &(data)->addr_type, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_octet_string(p, len, &(data)->address, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
    free_octet_string(&(data)->address);
}

size_t
length_HostAddress(const HostAddress * data)
{
    size_t ret = 0;
    {
	int oldret = ret;
	ret = 0;
	ret += length_integer(&(data)->addr_type);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_octet_string(&(data)->address);
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_HostAddress(const HostAddress * from, HostAddress * to)
{
    *(&(to)->addr_type) = *(&(from)->addr_type);
    if (copy_octet_string(&(from)->address, &(to)->address))
	return ENOMEM;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_AuthorizationData(unsigned char *p, size_t len,
			 const AuthorizationData * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    for (i = (data)->len - 1; i >= 0; --i) {
	int oldret = ret;
	ret = 0;
	{
	    int oldret = ret;
	    ret = 0;
	    e = encode_octet_string(p, len, &(&(data)->val[i])->ad_data, &l);
	    BACK;
	    e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	    BACK;
	    ret += oldret;
	}
	{
	    int oldret = ret;
	    ret = 0;
	    e = encode_integer(p, len, &(&(data)->val[i])->ad_type, &l);
	    BACK;
	    e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	    BACK;
	    ret += oldret;
	}
	e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_AuthorizationData(const unsigned char *p, size_t len,
			 AuthorizationData * data, size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    if (len < reallen)
	return ASN1_OVERRUN;
    len = reallen;
    {
	size_t origlen = len;
	int oldret = ret;
	ret = 0;
	(data)->len = 0;
	(data)->val = NULL;
	while (ret < origlen) {
	    (data)->len++;
	    (data)->val =
		realloc((data)->val, sizeof(*((data)->val)) * (data)->len);
	    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence,
					 &reallen, &l);
	    FORW;
	    {
		int dce_fix;
		if ((dce_fix = fix_dce(reallen, &len)) < 0)
		    return ASN1_BAD_FORMAT;
		{
		    size_t newlen, oldlen;

		    e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
		    if (e)
			return e;
		    else {
			p += l;
			len -= l;
			ret += l;
			e = der_get_length(p, len, &newlen, &l);
			FORW;
			{
			    int dce_fix;
			    oldlen = len;
			    if ((dce_fix = fix_dce(newlen, &len)) < 0)
				return ASN1_BAD_FORMAT;
			    e = decode_integer(p, len,
					       &(&(data)->
						 val[(data)->len -
						     1])->ad_type, &l);
			    FORW;
			    if (dce_fix) {
				e = der_match_tag_and_length(p, len,
							     (Der_class) 0,
							     (Der_type) 0, 0,
							     &reallen, &l);
				FORW;
			    } else
				len = oldlen - newlen;
			}
		    }
		}
		{
		    size_t newlen, oldlen;

		    e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
		    if (e)
			return e;
		    else {
			p += l;
			len -= l;
			ret += l;
			e = der_get_length(p, len, &newlen, &l);
			FORW;
			{
			    int dce_fix;
			    oldlen = len;
			    if ((dce_fix = fix_dce(newlen, &len)) < 0)
				return ASN1_BAD_FORMAT;
			    e = decode_octet_string(p, len,
						    &(&(data)->
						      val[(data)->len -
							  1])->ad_data, &l);
			    FORW;
			    if (dce_fix) {
				e = der_match_tag_and_length(p, len,
							     (Der_class) 0,
							     (Der_type) 0, 0,
							     &reallen, &l);
				FORW;
			    } else
				len = oldlen - newlen;
			}
		    }
		}
		if (dce_fix) {
		    e = der_match_tag_and_length(p, len, (Der_class) 0,
						 (Der_type) 0, 0, &reallen,
						 &l);
		    FORW;
		}
	    }
	    len = origlen - ret;
	}
	ret += oldret;
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
	free_octet_string(&(&(data)->val[(data)->len - 1])->ad_data);
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
	int oldret = ret;
	int i;
	ret = 0;
	for (i = (data)->len - 1; i >= 0; --i) {
	    {
		int oldret = ret;
		ret = 0;
		ret += length_integer(&(&(data)->val[i])->ad_type);
		ret += 1 + length_len(ret) + oldret;
	    }
	    {
		int oldret = ret;
		ret = 0;
		ret += length_octet_string(&(&(data)->val[i])->ad_data);
		ret += 1 + length_len(ret) + oldret;
	    }
	    ret += 1 + length_len(ret);
	}
	ret += 1 + length_len(ret) + oldret;
    }
    return ret;
}

int
copy_AuthorizationData(const AuthorizationData * from, AuthorizationData * to)
{
    if (((to)->val = malloc((from)->len * sizeof(*(to)->val))) == NULL
	&& (from)->len != 0)
	return ENOMEM;
    for ((to)->len = 0; (to)->len < (from)->len; (to)->len++) {
	*(&(&(to)->val[(to)->len])->ad_type) =
	    *(&(&(from)->val[(to)->len])->ad_type);
	if (copy_octet_string
	    (&(&(from)->val[(to)->len])->ad_data,
	     &(&(to)->val[(to)->len])->ad_data))
	    return ENOMEM;
    }
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_EncTicketPart(unsigned char *p, size_t len, const EncTicketPart * data,
		     size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    if ((data)->authorization_data) {
	int oldret = ret;
	ret = 0;
	e = encode_AuthorizationData(p, len, (data)->authorization_data, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 10, &l);
	BACK;
	ret += oldret;
    }
    if ((data)->caddr) {
	int oldret = ret;
	ret = 0;
	e = encode_HostAddresses(p, len, (data)->caddr, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 9, &l);
	BACK;
	ret += oldret;
    }
    if ((data)->renew_till) {
	int oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, (data)->renew_till, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 8, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, &(data)->endtime, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 7, &l);
	BACK;
	ret += oldret;
    }
    if ((data)->starttime) {
	int oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, (data)->starttime, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 6, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_KerberosTime(p, len, &(data)->authtime, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 5, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_TransitedEncoding(p, len, &(data)->transited, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 4, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_PrincipalName(p, len, &(data)->cname, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 3, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_Realm(p, len, &(data)->crealm, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 2, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_EncryptionKey(p, len, &(data)->key, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_TicketFlags(p, len, &(data)->flags, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    e = der_put_length_and_tag(p, len, ret, APPL, CONS, 3, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_EncTicketPart(const unsigned char *p, size_t len, EncTicketPart * data,
		     size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, APPL, CONS, 3, &reallen, &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence,
				     &reallen, &l);
	FORW;
	{
	    int dce_fix;
	    if ((dce_fix = fix_dce(reallen, &len)) < 0)
		return ASN1_BAD_FORMAT;
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_TicketFlags(p, len, &(data)->flags, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_EncryptionKey(p, len, &(data)->key, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 2, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_Realm(p, len, &(data)->crealm, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 3, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_PrincipalName(p, len, &(data)->cname, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 4, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_TransitedEncoding(p, len,
						     &(data)->transited, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 5, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_KerberosTime(p, len, &(data)->authtime,
						&l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 6, &l);
		if (e)
		    (data)->starttime = NULL;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			(data)->starttime =
			    malloc(sizeof(*(data)->starttime));
			if ((data)->starttime == NULL)
			    return ENOMEM;
			e = decode_KerberosTime(p, len, (data)->starttime,
						&l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 7, &l);
		if (e)
		    return e;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			e = decode_KerberosTime(p, len, &(data)->endtime, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 8, &l);
		if (e)
		    (data)->renew_till = NULL;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			(data)->renew_till =
			    malloc(sizeof(*(data)->renew_till));
			if ((data)->renew_till == NULL)
			    return ENOMEM;
			e = decode_KerberosTime(p, len, (data)->renew_till,
						&l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 9, &l);
		if (e)
		    (data)->caddr = NULL;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			(data)->caddr = malloc(sizeof(*(data)->caddr));
			if ((data)->caddr == NULL)
			    return ENOMEM;
			e = decode_HostAddresses(p, len, (data)->caddr, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    {
		size_t newlen, oldlen;

		e = der_match_tag(p, len, Der_CONTEXT, CONS, 10, &l);
		if (e)
		    (data)->authorization_data = NULL;
		else {
		    p += l;
		    len -= l;
		    ret += l;
		    e = der_get_length(p, len, &newlen, &l);
		    FORW;
		    {
			int dce_fix;
			oldlen = len;
			if ((dce_fix = fix_dce(newlen, &len)) < 0)
			    return ASN1_BAD_FORMAT;
			(data)->authorization_data =
			    malloc(sizeof(*(data)->authorization_data));
			if ((data)->authorization_data == NULL)
			    return ENOMEM;
			e = decode_AuthorizationData(p, len,
						     (data)->
						     authorization_data, &l);
			FORW;
			if (dce_fix) {
			    e = der_match_tag_and_length(p, len,
							 (Der_class) 0,
							 (Der_type) 0, 0,
							 &reallen, &l);
			    FORW;
			} else
			    len = oldlen - newlen;
		    }
		}
	    }
	    if (dce_fix) {
		e = der_match_tag_and_length(p, len, (Der_class) 0,
					     (Der_type) 0, 0, &reallen, &l);
		FORW;
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
	int oldret = ret;
	ret = 0;
	ret += length_TicketFlags(&(data)->flags);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_EncryptionKey(&(data)->key);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_Realm(&(data)->crealm);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_PrincipalName(&(data)->cname);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_TransitedEncoding(&(data)->transited);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_KerberosTime(&(data)->authtime);
	ret += 1 + length_len(ret) + oldret;
    }
    if ((data)->starttime) {
	int oldret = ret;
	ret = 0;
	ret += length_KerberosTime((data)->starttime);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_KerberosTime(&(data)->endtime);
	ret += 1 + length_len(ret) + oldret;
    }
    if ((data)->renew_till) {
	int oldret = ret;
	ret = 0;
	ret += length_KerberosTime((data)->renew_till);
	ret += 1 + length_len(ret) + oldret;
    }
    if ((data)->caddr) {
	int oldret = ret;
	ret = 0;
	ret += length_HostAddresses((data)->caddr);
	ret += 1 + length_len(ret) + oldret;
    }
    if ((data)->authorization_data) {
	int oldret = ret;
	ret = 0;
	ret += length_AuthorizationData((data)->authorization_data);
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_EncTicketPart(const EncTicketPart * from, EncTicketPart * to)
{
    if (copy_TicketFlags(&(from)->flags, &(to)->flags))
	return ENOMEM;
    if (copy_EncryptionKey(&(from)->key, &(to)->key))
	return ENOMEM;
    if (copy_Realm(&(from)->crealm, &(to)->crealm))
	return ENOMEM;
    if (copy_PrincipalName(&(from)->cname, &(to)->cname))
	return ENOMEM;
    if (copy_TransitedEncoding(&(from)->transited, &(to)->transited))
	return ENOMEM;
    if (copy_KerberosTime(&(from)->authtime, &(to)->authtime))
	return ENOMEM;
    if ((from)->starttime) {
	(to)->starttime = malloc(sizeof(*(to)->starttime));
	if ((to)->starttime == NULL)
	    return ENOMEM;
	if (copy_KerberosTime((from)->starttime, (to)->starttime))
	    return ENOMEM;
    } else
	(to)->starttime = NULL;
    if (copy_KerberosTime(&(from)->endtime, &(to)->endtime))
	return ENOMEM;
    if ((from)->renew_till) {
	(to)->renew_till = malloc(sizeof(*(to)->renew_till));
	if ((to)->renew_till == NULL)
	    return ENOMEM;
	if (copy_KerberosTime((from)->renew_till, (to)->renew_till))
	    return ENOMEM;
    } else
	(to)->renew_till = NULL;
    if ((from)->caddr) {
	(to)->caddr = malloc(sizeof(*(to)->caddr));
	if ((to)->caddr == NULL)
	    return ENOMEM;
	if (copy_HostAddresses((from)->caddr, (to)->caddr))
	    return ENOMEM;
    } else
	(to)->caddr = NULL;
    if ((from)->authorization_data) {
	(to)->authorization_data = malloc(sizeof(*(to)->authorization_data));
	if ((to)->authorization_data == NULL)
	    return ENOMEM;
	if (copy_AuthorizationData
	    ((from)->authorization_data, (to)->authorization_data))
	    return ENOMEM;
    } else
	(to)->authorization_data = NULL;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_KerberosTime(unsigned char *p, size_t len, const KerberosTime * data,
		    size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    e = encode_generalized_time(p, len, data, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_KerberosTime(const unsigned char *p, size_t len, KerberosTime * data,
		    size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = decode_generalized_time(p, len, data, &l);
    FORW;
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
    ret += length_generalized_time(data);
    return ret;
}

int
copy_KerberosTime(const KerberosTime * from, KerberosTime * to)
{
    *(to) = *(from);
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_TransitedEncoding(unsigned char *p, size_t len,
			 const TransitedEncoding * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	int oldret = ret;
	ret = 0;
	e = encode_octet_string(p, len, &(data)->contents, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_integer(p, len, &(data)->tr_type, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_TransitedEncoding(const unsigned char *p, size_t len,
			 TransitedEncoding * data, size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_integer(p, len, &(data)->tr_type, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_octet_string(p, len, &(data)->contents, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
    free_octet_string(&(data)->contents);
}

size_t
length_TransitedEncoding(const TransitedEncoding * data)
{
    size_t ret = 0;
    {
	int oldret = ret;
	ret = 0;
	ret += length_integer(&(data)->tr_type);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_octet_string(&(data)->contents);
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_TransitedEncoding(const TransitedEncoding * from, TransitedEncoding * to)
{
    *(&(to)->tr_type) = *(&(from)->tr_type);
    if (copy_octet_string(&(from)->contents, &(to)->contents))
	return ENOMEM;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_EncryptionKey(unsigned char *p, size_t len, const EncryptionKey * data,
		     size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	int oldret = ret;
	ret = 0;
	e = encode_octet_string(p, len, &(data)->keyvalue, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 1, &l);
	BACK;
	ret += oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	e = encode_integer(p, len, &(data)->keytype, &l);
	BACK;
	e = der_put_length_and_tag(p, len, ret, Der_CONTEXT, CONS, 0, &l);
	BACK;
	ret += oldret;
    }
    e = der_put_length_and_tag(p, len, ret, UNIV, CONS, UT_Sequence, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_EncryptionKey(const unsigned char *p, size_t len, EncryptionKey * data,
		     size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, CONS, UT_Sequence, &reallen,
				 &l);
    FORW;
    {
	int dce_fix;
	if ((dce_fix = fix_dce(reallen, &len)) < 0)
	    return ASN1_BAD_FORMAT;
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 0, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_integer(p, len, &(data)->keytype, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	{
	    size_t newlen, oldlen;

	    e = der_match_tag(p, len, Der_CONTEXT, CONS, 1, &l);
	    if (e)
		return e;
	    else {
		p += l;
		len -= l;
		ret += l;
		e = der_get_length(p, len, &newlen, &l);
		FORW;
		{
		    int dce_fix;
		    oldlen = len;
		    if ((dce_fix = fix_dce(newlen, &len)) < 0)
			return ASN1_BAD_FORMAT;
		    e = decode_octet_string(p, len, &(data)->keyvalue, &l);
		    FORW;
		    if (dce_fix) {
			e = der_match_tag_and_length(p, len, (Der_class) 0,
						     (Der_type) 0, 0,
						     &reallen, &l);
			FORW;
		    } else
			len = oldlen - newlen;
		}
	    }
	}
	if (dce_fix) {
	    e = der_match_tag_and_length(p, len, (Der_class) 0, (Der_type) 0,
					 0, &reallen, &l);
	    FORW;
	}
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
    free_octet_string(&(data)->keyvalue);
}

size_t
length_EncryptionKey(const EncryptionKey * data)
{
    size_t ret = 0;
    {
	int oldret = ret;
	ret = 0;
	ret += length_integer(&(data)->keytype);
	ret += 1 + length_len(ret) + oldret;
    }
    {
	int oldret = ret;
	ret = 0;
	ret += length_octet_string(&(data)->keyvalue);
	ret += 1 + length_len(ret) + oldret;
    }
    ret += 1 + length_len(ret);
    return ret;
}

int
copy_EncryptionKey(const EncryptionKey * from, EncryptionKey * to)
{
    *(&(to)->keytype) = *(&(from)->keytype);
    if (copy_octet_string(&(from)->keyvalue, &(to)->keyvalue))
	return ENOMEM;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_TicketFlags(unsigned char *p, size_t len, const TicketFlags * data,
		   size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    {
	unsigned char c = 0;
	*p-- = c;
	len--;
	ret++;
	c = 0;
	*p-- = c;
	len--;
	ret++;
	c = 0;
	if (data->anonymous)
	    c |= 1 << 1;
	if (data->ok_as_delegate)
	    c |= 1 << 2;
	if (data->transited_policy_checked)
	    c |= 1 << 3;
	if (data->hw_authent)
	    c |= 1 << 4;
	if (data->pre_authent)
	    c |= 1 << 5;
	if (data->initial)
	    c |= 1 << 6;
	if (data->renewable)
	    c |= 1 << 7;
	*p-- = c;
	len--;
	ret++;
	c = 0;
	if (data->invalid)
	    c |= 1 << 0;
	if (data->postdated)
	    c |= 1 << 1;
	if (data->may_postdate)
	    c |= 1 << 2;
	if (data->proxy)
	    c |= 1 << 3;
	if (data->proxiable)
	    c |= 1 << 4;
	if (data->forwarded)
	    c |= 1 << 5;
	if (data->forwardable)
	    c |= 1 << 6;
	if (data->reserved)
	    c |= 1 << 7;
	*p-- = c;
	*p-- = 0;
	len -= 2;
	ret += 2;
    }

    e = der_put_length_and_tag(p, len, ret, UNIV, PRIM, UT_BitString, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_TicketFlags(const unsigned char *p, size_t len, TicketFlags * data,
		   size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = der_match_tag_and_length(p, len, UNIV, PRIM, UT_BitString, &reallen,
				 &l);
    FORW;
    if (len < reallen)
	return ASN1_OVERRUN;
    p++;
    len--;
    reallen--;
    ret++;
    data->reserved = (*p >> 7) & 1;
    data->forwardable = (*p >> 6) & 1;
    data->forwarded = (*p >> 5) & 1;
    data->proxiable = (*p >> 4) & 1;
    data->proxy = (*p >> 3) & 1;
    data->may_postdate = (*p >> 2) & 1;
    data->postdated = (*p >> 1) & 1;
    data->invalid = (*p >> 0) & 1;
    p++;
    len--;
    reallen--;
    ret++;
    data->renewable = (*p >> 7) & 1;
    data->initial = (*p >> 6) & 1;
    data->pre_authent = (*p >> 5) & 1;
    data->hw_authent = (*p >> 4) & 1;
    data->transited_policy_checked = (*p >> 3) & 1;
    data->ok_as_delegate = (*p >> 2) & 1;
    data->anonymous = (*p >> 1) & 1;
    p += reallen;
    len -= reallen;
    ret += reallen;
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
    ret += 7;
    return ret;
}

int
copy_TicketFlags(const TicketFlags * from, TicketFlags * to)
{
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


/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_Realm(unsigned char *p, size_t len, const Realm * data, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    e = encode_general_string(p, len, data, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_Realm(const unsigned char *p, size_t len, Realm * data, size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = decode_general_string(p, len, data, &l);
    FORW;
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
    free_general_string(data);
}

size_t
length_Realm(const Realm * data)
{
    size_t ret = 0;
    ret += length_general_string(data);
    return ret;
}

int
copy_Realm(const Realm * from, Realm * to)
{
    if (copy_general_string(from, to))
	return ENOMEM;
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_ENCTYPE(unsigned char *p, size_t len, const ENCTYPE * data,
	       size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    e = encode_integer(p, len, (const int *)data, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_ENCTYPE(const unsigned char *p, size_t len, ENCTYPE * data,
	       size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = decode_integer(p, len, (int *)data, &l);
    FORW;
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
    ret += length_integer((const int *)data);
    return ret;
}

int
copy_ENCTYPE(const ENCTYPE * from, ENCTYPE * to)
{
    *(to) = *(from);
    return 0;
}

/* Generated from /home/lha/src/cvs/heimdal/lib/asn1/k5.asn1 */
/* Do not edit */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <asn1_err.h>

#define BACK if (e) return e; p -= l; len -= l; ret += l

int
encode_NAME_TYPE(unsigned char *p, size_t len, const NAME_TYPE * data,
		 size_t * size)
{
    size_t ret = 0;
    size_t l;
    int i, e;

    i = 0;
    e = encode_integer(p, len, (const int *)data, &l);
    BACK;
    *size = ret;
    return 0;
}

#define FORW if(e) goto fail; p += l; len -= l; ret += l

int
decode_NAME_TYPE(const unsigned char *p, size_t len, NAME_TYPE * data,
		 size_t * size)
{
    size_t ret = 0, reallen;
    size_t l;
    int e;

    memset(data, 0, sizeof(*data));
    reallen = 0;
    e = decode_integer(p, len, (int *)data, &l);
    FORW;
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
    ret += length_integer((const int *)data);
    return ret;
}

int
copy_NAME_TYPE(const NAME_TYPE * from, NAME_TYPE * to)
{
    *(to) = *(from);
    return 0;
}
