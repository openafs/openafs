#include "asn1_err.h"
#include <errno.h>
/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


/* RCSID("Heimdal: der_get.c,v 1.33 2002/09/03 16:21:49 nectar Exp $"); */


/* 
 * All decoding functions take a pointer `p' to first position in
 * which to read, from the left, `len' which means the maximum number
 * of characters we are able to read, `ret' were the value will be
 * returned and `size' where the number of used bytes is stored.
 * Either 0 or an error code is returned.
 */

static int
der_get_unsigned(const unsigned char *p, size_t len, unsigned *ret,
		 size_t * size)
{
    unsigned val = 0;
    size_t oldlen = len;

    while (len--)
	val = val * 256 + *p++;
    *ret = val;
    if (size)
	*size = oldlen;
    return 0;
}

int
der_get_int(const unsigned char *p, size_t len, int *ret, size_t * size)
{
    int val = 0;
    size_t oldlen = len;

    if (len > 0) {
	val = (signed char)*p++;
	while (--len)
	    val = val * 256 + *p++;
    }
    *ret = val;
    if (size)
	*size = oldlen;
    return 0;
}

int
der_get_length(const unsigned char *p, size_t len, size_t * val,
	       size_t * size)
{
    size_t v;

    if (len <= 0)
	return ASN1_OVERRUN;
    --len;
    v = *p++;
    if (v < 128) {
	*val = v;
	if (size)
	    *size = 1;
    } else {
	int e;
	size_t l;
	unsigned tmp;

	if (v == 0x80) {
	    *val = ASN1_INDEFINITE;
	    if (size)
		*size = 1;
	    return 0;
	}
	v &= 0x7F;
	if (len < v)
	    return ASN1_OVERRUN;
	e = der_get_unsigned(p, v, &tmp, &l);
	if (e)
	    return e;
	*val = tmp;
	if (size)
	    *size = l + 1;
    }
    return 0;
}

int
der_get_general_string(const unsigned char *p, size_t len,
		       general_string * str, size_t * size)
{
    char *s;

    s = malloc(len + 1);
    if (s == NULL)
	return ENOMEM;
    memcpy(s, p, len);
    s[len] = '\0';
    *str = s;
    if (size)
	*size = len;
    return 0;
}

int
der_get_octet_string(const unsigned char *p, size_t len, octet_string * data,
		     size_t * size)
{
    data->length = len;
    data->data = malloc(len);
    if (data->data == NULL && data->length != 0)
	return ENOMEM;
    memcpy(data->data, p, len);
    if (size)
	*size = len;
    return 0;
}

int
der_get_oid(const unsigned char *p, size_t len, oid * data, size_t * size)
{
    int n;
    size_t oldlen = len;

    if (len < 1)
	return ASN1_OVERRUN;

    data->components = malloc(len * sizeof(*data->components));
    if (data->components == NULL && len != 0)
	return ENOMEM;
    data->components[0] = (*p) / 40;
    data->components[1] = (*p) % 40;
    --len;
    ++p;
    for (n = 2; len > 0; ++n) {
	unsigned u = 0;

	do {
	    --len;
	    u = u * 128 + (*p++ % 128);
	} while (len > 0 && p[-1] & 0x80);
	data->components[n] = u;
    }
    if (p[-1] & 0x80) {
	free_oid(data);
	return ASN1_OVERRUN;
    }
    data->length = n;
    if (size)
	*size = oldlen;
    return 0;
}

int
der_get_tag(const unsigned char *p, size_t len, Der_class * class,
	    Der_type * type, int *tag, size_t * size)
{
    if (len < 1)
	return ASN1_OVERRUN;
    *class = (Der_class) (((*p) >> 6) & 0x03);
    *type = (Der_type) (((*p) >> 5) & 0x01);
    *tag = (*p) & 0x1F;
    if (size)
	*size = 1;
    return 0;
}

int
der_match_tag(const unsigned char *p, size_t len, Der_class class,
	      Der_type type, int tag, size_t * size)
{
    size_t l;
    Der_class thisclass;
    Der_type thistype;
    int thistag;
    int e;

    e = der_get_tag(p, len, &thisclass, &thistype, &thistag, &l);
    if (e)
	return e;
    if (class != thisclass || type != thistype)
	return ASN1_BAD_ID;
    if (tag > thistag)
	return ASN1_MISPLACED_FIELD;
    if (tag < thistag)
	return ASN1_MISSING_FIELD;
    if (size)
	*size = l;
    return 0;
}

int
der_match_tag_and_length(const unsigned char *p, size_t len, Der_class class,
			 Der_type type, int tag, size_t * length_ret,
			 size_t * size)
{
    size_t l, ret = 0;
    int e;

    e = der_match_tag(p, len, class, type, tag, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length(p, len, length_ret, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

int
decode_integer(const unsigned char *p, size_t len, int *num, size_t * size)
{
    size_t ret = 0;
    size_t l, reallen;
    int e;

    e = der_match_tag(p, len, UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length(p, len, &reallen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (reallen > len)
	return ASN1_OVERRUN;
    e = der_get_int(p, reallen, num, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

int
decode_unsigned(const unsigned char *p, size_t len, unsigned *num,
		size_t * size)
{
    size_t ret = 0;
    size_t l, reallen;
    int e;

    e = der_match_tag(p, len, UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length(p, len, &reallen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (reallen > len)
	return ASN1_OVERRUN;
    e = der_get_unsigned(p, reallen, num, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

int
decode_enumerated(const unsigned char *p, size_t len, unsigned *num,
		  size_t * size)
{
    size_t ret = 0;
    size_t l, reallen;
    int e;

    e = der_match_tag(p, len, UNIV, PRIM, UT_Enumerated, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length(p, len, &reallen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_int(p, reallen, num, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

int
decode_general_string(const unsigned char *p, size_t len,
		      general_string * str, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag(p, len, UNIV, PRIM, UT_GeneralString, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length(p, len, &slen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;

    e = der_get_general_string(p, slen, str, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

int
decode_octet_string(const unsigned char *p, size_t len, octet_string * k,
		    size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag(p, len, UNIV, PRIM, UT_OctetString, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length(p, len, &slen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;

    e = der_get_octet_string(p, slen, k, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

int
decode_oid(const unsigned char *p, size_t len, oid * k, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag(p, len, UNIV, PRIM, UT_OID, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length(p, len, &slen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;

    e = der_get_oid(p, slen, k, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (size)
	*size = ret;
    return 0;
}

static void
generalizedtime2time(const char *s, time_t * t)
{
    struct tm tm;

    memset(&tm, 0, sizeof(tm));
    sscanf(s, "%04d%02d%02d%02d%02d%02dZ", &tm.tm_year, &tm.tm_mon,
	   &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    *t = timegm(&tm);
}

int
decode_generalized_time(const unsigned char *p, size_t len, time_t * t,
			size_t * size)
{
    octet_string k;
    char *times;
    size_t ret = 0;
    size_t l;
    int e;
    size_t slen;

    e = der_match_tag(p, len, UNIV, PRIM, UT_GeneralizedTime, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;

    e = der_get_length(p, len, &slen, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    if (len < slen)
	return ASN1_OVERRUN;
    e = der_get_octet_string(p, slen, &k, &l);
    if (e)
	return e;
    p += l;
    len -= l;
    ret += l;
    times = realloc(k.data, k.length + 1);
    if (times == NULL) {
	free(k.data);
	return ENOMEM;
    }
    times[k.length] = 0;
    generalizedtime2time(times, t);
    free(times);
    if (size)
	*size = ret;
    return 0;
}


int
fix_dce(size_t reallen, size_t * len)
{
    if (reallen == ASN1_INDEFINITE)
	return 1;
    if (*len < reallen)
	return -1;
    *len = reallen;
    return 0;
}

/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


/* RCSID("Heimdal: der_put.c,v 1.27 2001/09/25 23:37:25 assar Exp $"); */

/*
 * All encoding functions take a pointer `p' to first position in
 * which to write, from the right, `len' which means the maximum
 * number of characters we are able to write.  The function returns
 * the number of characters written in `size' (if non-NULL).
 * The return value is 0 or an error.
 */

static int
der_put_unsigned(unsigned char *p, size_t len, unsigned val, size_t * size)
{
    unsigned char *base = p;

    if (val) {
	while (len > 0 && val) {
	    *p-- = val % 256;
	    val /= 256;
	    --len;
	}
	if (val != 0)
	    return ASN1_OVERFLOW;
	else {
	    *size = base - p;
	    return 0;
	}
    } else if (len < 1)
	return ASN1_OVERFLOW;
    else {
	*p = 0;
	*size = 1;
	return 0;
    }
}

int
der_put_int(unsigned char *p, size_t len, int val, size_t * size)
{
    unsigned char *base = p;

    if (val >= 0) {
	do {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = val % 256;
	    len--;
	    val /= 256;
	} while (val);
	if (p[1] >= 128) {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0;
	    len--;
	}
    } else {
	val = ~val;
	do {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = ~(val % 256);
	    len--;
	    val /= 256;
	} while (val);
	if (p[1] < 128) {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0xff;
	    len--;
	}
    }
    *size = base - p;
    return 0;
}


int
der_put_length(unsigned char *p, size_t len, size_t val, size_t * size)
{
    if (len < 1)
	return ASN1_OVERFLOW;
    if (val < 128) {
	*p = val;
	*size = 1;
	return 0;
    } else {
	size_t l;
	int e;

	e = der_put_unsigned(p, len - 1, val, &l);
	if (e)
	    return e;
	p -= l;
	*p = 0x80 | l;
	*size = l + 1;
	return 0;
    }
}

int
der_put_general_string(unsigned char *p, size_t len,
		       const general_string * str, size_t * size)
{
    size_t slen = strlen(*str);

    if (len < slen)
	return ASN1_OVERFLOW;
    p -= slen;
    len -= slen;
    memcpy(p + 1, *str, slen);
    *size = slen;
    return 0;
}

int
der_put_octet_string(unsigned char *p, size_t len, const octet_string * data,
		     size_t * size)
{
    if (len < data->length)
	return ASN1_OVERFLOW;
    p -= data->length;
    len -= data->length;
    memcpy(p + 1, data->data, data->length);
    *size = data->length;
    return 0;
}

int
der_put_oid(unsigned char *p, size_t len, const oid * data, size_t * size)
{
    unsigned char *base = p;
    int n;

    for (n = data->length - 1; n >= 2; --n) {
	unsigned u = data->components[n];

	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = u % 128;
	u /= 128;
	--len;
	while (u > 0) {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 128 + u % 128;
	    u /= 128;
	    --len;
	}
    }
    if (len < 1)
	return ASN1_OVERFLOW;
    *p-- = 40 * data->components[0] + data->components[1];
    *size = base - p;
    return 0;
}

int
der_put_tag(unsigned char *p, size_t len, Der_class class, Der_type type,
	    int tag, size_t * size)
{
    if (len < 1)
	return ASN1_OVERFLOW;
    *p = (class << 6) | (type << 5) | tag;	/* XXX */
    *size = 1;
    return 0;
}

int
der_put_length_and_tag(unsigned char *p, size_t len, size_t len_val,
		       Der_class class, Der_type type, int tag, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_length(p, len, len_val, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_tag(p, len, class, type, tag, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_integer(unsigned char *p, size_t len, const int *data, size_t * size)
{
    int num = *data;
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_int(p, len, num, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag(p, len, l, UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_unsigned(unsigned char *p, size_t len, const unsigned *data,
		size_t * size)
{
    unsigned num = *data;
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_unsigned(p, len, num, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag(p, len, l, UNIV, PRIM, UT_Integer, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_enumerated(unsigned char *p, size_t len, const unsigned *data,
		  size_t * size)
{
    unsigned num = *data;
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_int(p, len, num, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag(p, len, l, UNIV, PRIM, UT_Enumerated, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_general_string(unsigned char *p, size_t len,
		      const general_string * data, size_t * size)
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
    e = der_put_length_and_tag(p, len, l, UNIV, PRIM, UT_GeneralString, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_octet_string(unsigned char *p, size_t len, const octet_string * k,
		    size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_octet_string(p, len, k, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag(p, len, l, UNIV, PRIM, UT_OctetString, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
encode_oid(unsigned char *p, size_t len, const oid * k, size_t * size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_oid(p, len, k, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag(p, len, l, UNIV, PRIM, UT_OID, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

int
time2generalizedtime(time_t t, octet_string * s)
{
    struct tm *tm;

    s->data = malloc(16);
    if (s->data == NULL)
	return ENOMEM;
    s->length = 15;
    tm = gmtime(&t);
    sprintf(s->data, "%04d%02d%02d%02d%02d%02dZ", tm->tm_year + 1900,
	    tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
    return 0;
}

int
encode_generalized_time(unsigned char *p, size_t len, const time_t * t,
			size_t * size)
{
    size_t ret = 0;
    size_t l;
    octet_string k;
    int e;

    e = time2generalizedtime(*t, &k);
    if (e)
	return e;
    e = der_put_octet_string(p, len, &k, &l);
    free(k.data);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_length_and_tag(p, len, k.length, UNIV, PRIM,
			       UT_GeneralizedTime, &l);
    if (e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    *size = ret;
    return 0;
}

/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


/* RCSID("Heimdal: der_free.c,v 1.8 2001/09/25 13:39:26 assar Exp $"); */

void
free_general_string(general_string * str)
{
    free(*str);
    *str = NULL;
}

void
free_octet_string(octet_string * k)
{
    free(k->data);
    k->data = NULL;
}

void
free_oid(oid * k)
{
    free(k->components);
    k->components = NULL;
}

/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


/* RCSID("Heimdal: der_length.c,v 1.12 2001/09/25 13:39:26 assar Exp $"); */

static size_t
len_unsigned(unsigned val)
{
    size_t ret = 0;

    do {
	++ret;
	val /= 256;
    } while (val);
    return ret;
}

static size_t
len_int(int val)
{
    size_t ret = 0;

    if (val == 0)
	return 1;
    while (val > 255 || val < -255) {
	++ret;
	val /= 256;
    }
    if (val != 0) {
	++ret;
	if ((signed char)val != val)
	    ++ret;
	val /= 256;
    }
    return ret;
}

static size_t
len_oid(const oid * oid)
{
    size_t ret = 1;
    int n;

    for (n = 2; n < oid->length; ++n) {
	unsigned u = oid->components[n];

	++ret;
	u /= 128;
	while (u > 0) {
	    ++ret;
	    u /= 128;
	}
    }
    return ret;
}

size_t
length_len(size_t len)
{
    if (len < 128)
	return 1;
    else
	return len_unsigned(len) + 1;
}

size_t
length_integer(const int *data)
{
    size_t len = len_int(*data);

    return 1 + length_len(len) + len;
}

size_t
length_unsigned(const unsigned *data)
{
    size_t len = len_unsigned(*data);

    return 1 + length_len(len) + len;
}

size_t
length_enumerated(const unsigned *data)
{
    size_t len = len_int(*data);

    return 1 + length_len(len) + len;
}

size_t
length_general_string(const general_string * data)
{
    char *str = *data;
    size_t len = strlen(str);
    return 1 + length_len(len) + len;
}

size_t
length_octet_string(const octet_string * k)
{
    return 1 + length_len(k->length) + k->length;
}

size_t
length_oid(const oid * k)
{
    size_t len = len_oid(k);

    return 1 + length_len(len) + len;
}

size_t
length_generalized_time(const time_t * t)
{
    octet_string k;
    size_t ret;

    time2generalizedtime(*t, &k);
    ret = 1 + length_len(k.length) + k.length;
    free(k.data);
    return ret;
}

/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


/* RCSID("Heimdal: der_copy.c,v 1.9 2001/09/25 13:39:25 assar Exp $"); */

int
copy_general_string(const general_string * from, general_string * to)
{
    *to = malloc(strlen(*from) + 1);
    if (*to == NULL)
	return ENOMEM;
    strcpy(*to, *from);
    return 0;
}

int
copy_octet_string(const octet_string * from, octet_string * to)
{
    to->length = from->length;
    to->data = malloc(to->length);
    if (to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length);
    return 0;
}

int
copy_oid(const oid * from, oid * to)
{
    to->length = from->length;
    to->components = malloc(to->length * sizeof(*to->components));
    if (to->length != 0 && to->components == NULL)
	return ENOMEM;
    memcpy(to->components, from->components, to->length);
    return 0;
}

/*
 * Copyright (c) 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
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
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */


/* RCSID("Heimdal: timegm.c,v 1.7 1999/12/02 17:05:02 joda Exp $"); */

#ifndef HAVE_TIMEGM

static int
is_leap(unsigned y)
{
    y += 1900;
    return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}

time_t
timegm(struct tm * tm)
{
    static const unsigned ndays[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    time_t res = 0;
    unsigned i;

    for (i = 70; i < tm->tm_year; ++i)
	res += is_leap(i) ? 366 : 365;

    for (i = 0; i < tm->tm_mon; ++i)
	res += ndays[is_leap(tm->tm_year)][i];
    res += tm->tm_mday - 1;
    res *= 24;
    res += tm->tm_hour;
    res *= 60;
    res += tm->tm_min;
    res *= 60;
    res += tm->tm_sec;
    return res;
}

#endif /* HAVE_TIMEGM */
