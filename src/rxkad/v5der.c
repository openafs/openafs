#include "asn1_err.h"
#include <errno.h>
#include <limits.h>
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


/* RCSID("$Id$"); */

static int
is_leap(unsigned y)
{
    y += 1900;
    return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}

/*
 * This is a simplifed version of timegm(3) that doesn't accept out of
 * bound values that timegm(3) normally accepts but those are not
 * valid in asn1 encodings.
 */

time_t
_der_timegm (struct tm *tm)
{
  static const unsigned ndays[2][12] ={
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
  time_t res = 0;
  unsigned i;

  if (tm->tm_year < 0)
      return -1;
  if (tm->tm_mon < 0 || tm->tm_mon > 11)
      return -1;
  if (tm->tm_mday < 1 || tm->tm_mday > ndays[is_leap(tm->tm_year)][tm->tm_mon])
      return -1;
  if (tm->tm_hour < 0 || tm->tm_hour > 23)
      return -1;
  if (tm->tm_min < 0 || tm->tm_min > 59)
      return -1;
  if (tm->tm_sec < 0 || tm->tm_sec > 59)
      return -1;

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
/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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


/*
 * All decoding functions take a pointer `p' to first position in
 * which to read, from the left, `len' which means the maximum number
 * of characters we are able to read, `ret' were the value will be
 * returned and `size' where the number of used bytes is stored.
 * Either 0 or an error code is returned.
 */

int
der_get_unsigned (const unsigned char *p, size_t len,
		  unsigned *ret, size_t *size)
{
    unsigned val = 0;
    size_t oldlen = len;

    if (len == sizeof(unsigned) + 1 && p[0] == 0)
	;
    else if (len > sizeof(unsigned))
	return ASN1_OVERRUN;

    while (len--)
	val = val * 256 + *p++;
    *ret = val;
    if(size) *size = oldlen;
    return 0;
}

int
der_get_integer (const unsigned char *p, size_t len,
		 int *ret, size_t *size)
{
    int val = 0;
    size_t oldlen = len;

    if (len > sizeof(int))
	return ASN1_OVERRUN;

    if (len > 0) {
	val = (signed char)*p++;
	while (--len)
	    val = val * 256 + *p++;
    }
    *ret = val;
    if(size) *size = oldlen;
    return 0;
}

int
der_get_length (const unsigned char *p, size_t len,
		size_t *val, size_t *size)
{
    size_t v;

    if (len <= 0)
	return ASN1_OVERRUN;
    --len;
    v = *p++;
    if (v < 128) {
	*val = v;
	if(size) *size = 1;
    } else {
	int e;
	size_t l;
	unsigned tmp;

	if(v == 0x80){
	    *val = ASN1_INDEFINITE;
	    if(size) *size = 1;
	    return 0;
	}
	v &= 0x7F;
	if (len < v)
	    return ASN1_OVERRUN;
	e = der_get_unsigned (p, v, &tmp, &l);
	if(e) return e;
	*val = tmp;
	if(size) *size = l + 1;
    }
    return 0;
}

int
der_get_boolean(const unsigned char *p, size_t len, int *data, size_t *size)
{
    if(len < 1)
	return ASN1_OVERRUN;
    if(*p != 0)
	*data = 1;
    else
	*data = 0;
    *size = 1;
    return 0;
}

int
der_get_general_string (const unsigned char *p, size_t len,
			heim_general_string *str, size_t *size)
{
    const unsigned char *p1;
    char *s;

    p1 = memchr(p, 0, len);
    if (p1 != NULL) {
	/*
	 * Allow trailing NULs. We allow this since MIT Kerberos sends
	 * an strings in the NEED_PREAUTH case that includes a
	 * trailing NUL.
	 */
	while (p1 - p < len && *p1 == '\0')
	    p1++;
       if (p1 - p != len)
	    return ASN1_BAD_CHARACTER;
    }
    if (len > len + 1)
	return ASN1_BAD_LENGTH;

    s = malloc (len + 1);
    if (s == NULL)
	return ENOMEM;
    memcpy (s, p, len);
    s[len] = '\0';
    *str = s;
    if(size) *size = len;
    return 0;
}

int
der_get_utf8string (const unsigned char *p, size_t len,
		    heim_utf8_string *str, size_t *size)
{
    return der_get_general_string(p, len, str, size);
}

int
der_get_printable_string (const unsigned char *p, size_t len,
			  heim_printable_string *str, size_t *size)
{
    return der_get_general_string(p, len, str, size);
}

int
der_get_ia5_string (const unsigned char *p, size_t len,
		    heim_ia5_string *str, size_t *size)
{
    return der_get_general_string(p, len, str, size);
}

int
der_get_bmp_string (const unsigned char *p, size_t len,
		    heim_bmp_string *data, size_t *size)
{
    size_t i;

    if (len & 1)
	return ASN1_BAD_FORMAT;
    data->length = len / 2;
    if (data->length > UINT_MAX/sizeof(data->data[0]))
	return ERANGE;
    data->data = malloc(data->length * sizeof(data->data[0]));
    if (data->data == NULL && data->length != 0)
	return ENOMEM;

    for (i = 0; i < data->length; i++) {
	data->data[i] = (p[0] << 8) | p[1];
	p += 2;
	/* check for NUL in the middle of the string */
	if (data->data[i] == 0 && i != (data->length - 1)) {
	    free(data->data);
	    data->data = NULL;
	    data->length = 0;
	    return ASN1_BAD_CHARACTER;
	}
    }
    if (size) *size = len;

    return 0;
}

int
der_get_universal_string (const unsigned char *p, size_t len,
			  heim_universal_string *data, size_t *size)
{
    size_t i;

    if (len & 3)
	return ASN1_BAD_FORMAT;
    data->length = len / 4;
    if (data->length > UINT_MAX/sizeof(data->data[0]))
	return ERANGE;
    data->data = malloc(data->length * sizeof(data->data[0]));
    if (data->data == NULL && data->length != 0)
	return ENOMEM;

    for (i = 0; i < data->length; i++) {
	data->data[i] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	p += 4;
	/* check for NUL in the middle of the string */
	if (data->data[i] == 0 && i != (data->length - 1)) {
	    free(data->data);
	    data->data = NULL;
	    data->length = 0;
	    return ASN1_BAD_CHARACTER;
	}
    }
    if (size) *size = len;
    return 0;
}

int
der_get_visible_string (const unsigned char *p, size_t len,
			heim_visible_string *str, size_t *size)
{
    return der_get_general_string(p, len, str, size);
}

int
der_get_octet_string (const unsigned char *p, size_t len,
		      heim_octet_string *data, size_t *size)
{
    data->length = len;
    data->data = malloc(len);
    if (data->data == NULL && data->length != 0)
	return ENOMEM;
    memcpy (data->data, p, len);
    if(size) *size = len;
    return 0;
}

int
der_get_octet_string_ber (const unsigned char *p, size_t len,
			  heim_octet_string *data, size_t *size)
{
    int e;
    Der_type type;
    Der_class class;
    unsigned int tag, depth = 0;
    size_t l, datalen, oldlen = len;

    data->length = 0;
    data->data = NULL;

    while (len) {
	e = der_get_tag (p, len, &class, &type, &tag, &l);
	if (e) goto out;
	if (class != ASN1_C_UNIV) {
	    e = ASN1_BAD_ID;
	    goto out;
	}
	if (type == PRIM && tag == UT_EndOfContent) {
	    if (depth == 0)
		break;
	    depth--;
	}
	if (tag != UT_OctetString) {
	    e = ASN1_BAD_ID;
	    goto out;
	}

	p += l;
	len -= l;
	e = der_get_length (p, len, &datalen, &l);
	if (e) goto out;
	p += l;
	len -= l;

	if (datalen > len)
	    return ASN1_OVERRUN;

	if (type == PRIM) {
	    void *ptr;

	    ptr = realloc(data->data, data->length + datalen);
	    if (ptr == NULL) {
		e = ENOMEM;
		goto out;
	    }
	    data->data = ptr;
	    memcpy(((unsigned char *)data->data) + data->length, p, datalen);
	    data->length += datalen;
	} else
	    depth++;

	p += datalen;
	len -= datalen;
    }
    if (depth != 0)
	return ASN1_INDEF_OVERRUN;
    if(size) *size = oldlen - len;
    return 0;
 out:
    free(data->data);
    data->data = NULL;
    data->length = 0;
    return e;
}


int
der_get_heim_integer (const unsigned char *p, size_t len,
		      heim_integer *data, size_t *size)
{
    data->length = 0;
    data->negative = 0;
    data->data = NULL;

    if (len == 0) {
	if (size)
	    *size = 0;
	return 0;
    }
    if (p[0] & 0x80) {
	unsigned char *q;
	int carry = 1;
	data->negative = 1;

	data->length = len;

	if (p[0] == 0xff) {
	    p++;
	    data->length--;
	}
	data->data = malloc(data->length);
	if (data->data == NULL) {
	    data->length = 0;
	    if (size)
		*size = 0;
	    return ENOMEM;
	}
	q = &((unsigned char*)data->data)[data->length - 1];
	p += data->length - 1;
	while (q >= (unsigned char*)data->data) {
	    *q = *p ^ 0xff;
	    if (carry)
		carry = !++*q;
	    p--;
	    q--;
	}
    } else {
	data->negative = 0;
	data->length = len;

	if (p[0] == 0) {
	    p++;
	    data->length--;
	}
	data->data = malloc(data->length);
	if (data->data == NULL && data->length != 0) {
	    data->length = 0;
	    if (size)
		*size = 0;
	    return ENOMEM;
	}
	memcpy(data->data, p, data->length);
    }
    if (size)
	*size = len;
    return 0;
}

static int
generalizedtime2time (const char *s, time_t *t)
{
    struct tm tm;

    memset(&tm, 0, sizeof(tm));
    if (sscanf (s, "%04d%02d%02d%02d%02d%02dZ",
		&tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour,
		&tm.tm_min, &tm.tm_sec) != 6) {
	if (sscanf (s, "%02d%02d%02d%02d%02d%02dZ",
		    &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour,
		    &tm.tm_min, &tm.tm_sec) != 6)
	    return ASN1_BAD_TIMEFORMAT;
	if (tm.tm_year < 50)
	    tm.tm_year += 2000;
	else
	    tm.tm_year += 1900;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    *t = _der_timegm (&tm);
    return 0;
}

static int
der_get_time (const unsigned char *p, size_t len,
	      time_t *data, size_t *size)
{
    char *times;
    int e;

    if (len > len + 1 || len == 0)
	return ASN1_BAD_LENGTH;

    times = malloc(len + 1);
    if (times == NULL)
	return ENOMEM;
    memcpy(times, p, len);
    times[len] = '\0';
    e = generalizedtime2time(times, data);
    free (times);
    if(size) *size = len;
    return e;
}

int
der_get_generalized_time (const unsigned char *p, size_t len,
			  time_t *data, size_t *size)
{
    return der_get_time(p, len, data, size);
}

int
der_get_utctime (const unsigned char *p, size_t len,
			  time_t *data, size_t *size)
{
    return der_get_time(p, len, data, size);
}

int
der_get_oid (const unsigned char *p, size_t len,
	     heim_oid *data, size_t *size)
{
    size_t n;
    size_t oldlen = len;

    if (len < 1)
	return ASN1_OVERRUN;

    if (len > len + 1)
	return ASN1_BAD_LENGTH;

    if (len + 1 > UINT_MAX/sizeof(data->components[0]))
	return ERANGE;

    data->components = malloc((len + 1) * sizeof(data->components[0]));
    if (data->components == NULL)
	return ENOMEM;
    data->components[0] = (*p) / 40;
    data->components[1] = (*p) % 40;
    --len;
    ++p;
    for (n = 2; len > 0; ++n) {
	unsigned u = 0, u1;

	do {
	    --len;
	    u1 = u * 128 + (*p++ % 128);
	    /* check that we don't overflow the element */
	    if (u1 < u) {
		der_free_oid(data);
		return ASN1_OVERRUN;
	    }
	    u = u1;
	} while (len > 0 && p[-1] & 0x80);
	data->components[n] = u;
    }
    if (n > 2 && p[-1] & 0x80) {
	der_free_oid (data);
	return ASN1_OVERRUN;
    }
    data->length = n;
    if (size)
	*size = oldlen;
    return 0;
}

int
der_get_tag (const unsigned char *p, size_t len,
	     Der_class *class, Der_type *type,
	     unsigned int *tag, size_t *size)
{
    size_t ret = 0;
    if (len < 1)
	return ASN1_OVERRUN;
    *class = (Der_class)(((*p) >> 6) & 0x03);
    *type = (Der_type)(((*p) >> 5) & 0x01);
    *tag = (*p) & 0x1f;
    p++; len--; ret++;
    if(*tag == 0x1f) {
	unsigned int continuation;
	unsigned int tag1;
	*tag = 0;
	do {
	    if(len < 1)
		return ASN1_OVERRUN;
	    continuation = *p & 128;
	    tag1 = *tag * 128 + (*p % 128);
	    /* check that we don't overflow the tag */
	    if (tag1 < *tag)
		return ASN1_OVERFLOW;
	    *tag = tag1;
	    p++; len--; ret++;
	} while(continuation);
    }
    if(size) *size = ret;
    return 0;
}

int
der_match_tag (const unsigned char *p, size_t len,
	       Der_class class, Der_type type,
	       unsigned int tag, size_t *size)
{
    Der_type thistype;
    int e;

    e = der_match_tag2(p, len, class, &thistype, tag, size);
    if (e) return e;
    if (thistype != type) return ASN1_BAD_ID;
    return 0;
}

int
der_match_tag2 (const unsigned char *p, size_t len,
		Der_class class, Der_type *type,
		unsigned int tag, size_t *size)
{
    size_t l;
    Der_class thisclass;
    unsigned int thistag;
    int e;

    e = der_get_tag (p, len, &thisclass, type, &thistag, &l);
    if (e) return e;
    if (class != thisclass)
	return ASN1_BAD_ID;
    if(tag > thistag)
	return ASN1_MISPLACED_FIELD;
    if(tag < thistag)
	return ASN1_MISSING_FIELD;
    if(size) *size = l;
    return 0;
}

int
der_match_tag_and_length (const unsigned char *p, size_t len,
			  Der_class class, Der_type *type, unsigned int tag,
			  size_t *length_ret, size_t *size)
{
    size_t l, ret = 0;
    int e;

    e = der_match_tag2 (p, len, class, type, tag, &l);
    if (e) return e;
    p += l;
    len -= l;
    ret += l;
    e = der_get_length (p, len, length_ret, &l);
    if (e) return e;
    if(size) *size = ret + l;
    return 0;
}



/*
 * Old versions of DCE was based on a very early beta of the MIT code,
 * which used MAVROS for ASN.1 encoding. MAVROS had the interesting
 * feature that it encoded data in the forward direction, which has
 * it's problems, since you have no idea how long the data will be
 * until after you're done. MAVROS solved this by reserving one byte
 * for length, and later, if the actual length was longer, it reverted
 * to indefinite, BER style, lengths. The version of MAVROS used by
 * the DCE people could apparently generate correct X.509 DER encodings, and
 * did this by making space for the length after encoding, but
 * unfortunately this feature wasn't used with Kerberos.
 */

int
_heim_fix_dce(size_t reallen, size_t *len)
{
    if(reallen == ASN1_INDEFINITE)
	return 1;
    if(*len < reallen)
	return -1;
    *len = reallen;
    return 0;
}

int
der_get_bit_string (const unsigned char *p, size_t len,
		    heim_bit_string *data, size_t *size)
{
    if (len < 1)
	return ASN1_OVERRUN;
    if (p[0] > 7)
	return ASN1_BAD_FORMAT;
    if (len - 1 == 0 && p[0] != 0)
	return ASN1_BAD_FORMAT;
    /* check if any of the three upper bits are set
     * any of them will cause a interger overrun */
    if ((len - 1) >> (sizeof(len) * 8 - 3))
	return ASN1_OVERRUN;
    data->length = (len - 1) * 8;
    data->data = malloc(len - 1);
    if (data->data == NULL && (len - 1) != 0)
	return ENOMEM;
    /* copy data is there is data to copy */
    if (len - 1 != 0) {
      memcpy (data->data, p + 1, len - 1);
      data->length -= p[0];
    }
    if(size) *size = len;
    return 0;
}
/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
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


/* RCSID("$Id$"); */

/*
 * All encoding functions take a pointer `p' to first position in
 * which to write, from the right, `len' which means the maximum
 * number of characters we are able to write.  The function returns
 * the number of characters written in `size' (if non-NULL).
 * The return value is 0 or an error.
 */

int
der_put_unsigned (unsigned char *p, size_t len, const unsigned *v, size_t *size)
{
    unsigned char *base = p;
    unsigned val = *v;

    if (val) {
	while (len > 0 && val) {
	    *p-- = val % 256;
	    val /= 256;
	    --len;
	}
	if (val != 0)
	    return ASN1_OVERFLOW;
	else {
	    if(p[1] >= 128) {
		if(len < 1)
		    return ASN1_OVERFLOW;
		*p-- = 0;
	    }
	    *size = base - p;
	    return 0;
	}
    } else if (len < 1)
	return ASN1_OVERFLOW;
    else {
	*p    = 0;
	*size = 1;
	return 0;
    }
}

int
der_put_integer (unsigned char *p, size_t len, const int *v, size_t *size)
{
    unsigned char *base = p;
    int val = *v;

    if(val >= 0) {
	do {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = val % 256;
	    len--;
	    val /= 256;
	} while(val);
	if(p[1] >= 128) {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0;
	    len--;
	}
    } else {
	val = ~val;
	do {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = ~(val % 256);
	    len--;
	    val /= 256;
	} while(val);
	if(p[1] < 128) {
	    if(len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0xff;
	    len--;
	}
    }
    *size = base - p;
    return 0;
}


int
der_put_length (unsigned char *p, size_t len, size_t val, size_t *size)
{
    if (len < 1)
	return ASN1_OVERFLOW;

    if (val < 128) {
	*p = val;
	*size = 1;
    } else {
	size_t l = 0;

	while(val > 0) {
	    if(len < 2)
		return ASN1_OVERFLOW;
	    *p-- = val % 256;
	    val /= 256;
	    len--;
	    l++;
	}
	*p = 0x80 | l;
	if(size)
	    *size = l + 1;
    }
    return 0;
}

int
der_put_boolean(unsigned char *p, size_t len, const int *data, size_t *size)
{
    if(len < 1)
	return ASN1_OVERFLOW;
    if(*data != 0)
	*p = 0xff;
    else
	*p = 0;
    *size = 1;
    return 0;
}

int
der_put_general_string (unsigned char *p, size_t len,
			const heim_general_string *str, size_t *size)
{
    size_t slen = strlen(*str);

    if (len < slen)
	return ASN1_OVERFLOW;
    p -= slen;
    memcpy (p+1, *str, slen);
    *size = slen;
    return 0;
}

int
der_put_utf8string (unsigned char *p, size_t len,
		    const heim_utf8_string *str, size_t *size)
{
    return der_put_general_string(p, len, str, size);
}

int
der_put_printable_string (unsigned char *p, size_t len,
			  const heim_printable_string *str, size_t *size)
{
    return der_put_general_string(p, len, str, size);
}

int
der_put_ia5_string (unsigned char *p, size_t len,
		    const heim_ia5_string *str, size_t *size)
{
    return der_put_general_string(p, len, str, size);
}

int
der_put_bmp_string (unsigned char *p, size_t len,
		    const heim_bmp_string *data, size_t *size)
{
    size_t i;
    if (len / 2 < data->length)
	return ASN1_OVERFLOW;
    p -= data->length * 2;
    for (i = 0; i < data->length; i++) {
	p[1] = (data->data[i] >> 8) & 0xff;
	p[2] = data->data[i] & 0xff;
	p += 2;
    }
    if (size) *size = data->length * 2;
    return 0;
}

int
der_put_universal_string (unsigned char *p, size_t len,
			  const heim_universal_string *data, size_t *size)
{
    size_t i;
    if (len / 4 < data->length)
	return ASN1_OVERFLOW;
    p -= data->length * 4;
    for (i = 0; i < data->length; i++) {
	p[1] = (data->data[i] >> 24) & 0xff;
	p[2] = (data->data[i] >> 16) & 0xff;
	p[3] = (data->data[i] >> 8) & 0xff;
	p[4] = data->data[i] & 0xff;
	p += 4;
    }
    if (size) *size = data->length * 4;
    return 0;
}

int
der_put_visible_string (unsigned char *p, size_t len,
			 const heim_visible_string *str, size_t *size)
{
    return der_put_general_string(p, len, str, size);
}

int
der_put_octet_string (unsigned char *p, size_t len,
		      const heim_octet_string *data, size_t *size)
{
    if (len < data->length)
	return ASN1_OVERFLOW;
    p -= data->length;
    memcpy (p+1, data->data, data->length);
    *size = data->length;
    return 0;
}

int
der_put_heim_integer (unsigned char *p, size_t len,
		     const heim_integer *data, size_t *size)
{
    unsigned char *buf = data->data;
    int hibitset = 0;

    if (data->length == 0) {
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = 0;
	if (size)
	    *size = 1;
	return 0;
    }
    if (len < data->length)
	return ASN1_OVERFLOW;

    len -= data->length;

    if (data->negative) {
	int i, carry;
	for (i = data->length - 1, carry = 1; i >= 0; i--) {
	    *p = buf[i] ^ 0xff;
	    if (carry)
		carry = !++*p;
	    p--;
	}
	if (p[1] < 128) {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = 0xff;
	    len--;
	    hibitset = 1;
	}
    } else {
	p -= data->length;
	memcpy(p + 1, buf, data->length);

	if (p[1] >= 128) {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    p[0] = 0;
	    len--;
	    hibitset = 1;
	}
    }
    if (size)
	*size = data->length + hibitset;
    return 0;
}

int
der_put_generalized_time (unsigned char *p, size_t len,
			  const time_t *data, size_t *size)
{
    heim_octet_string k;
    size_t l;
    int e;

    e = _heim_time2generalizedtime (*data, &k, 1);
    if (e)
	return e;
    e = der_put_octet_string(p, len, &k, &l);
    free(k.data);
    if(e)
	return e;
    if(size)
	*size = l;
    return 0;
}

int
der_put_utctime (unsigned char *p, size_t len,
		 const time_t *data, size_t *size)
{
    heim_octet_string k;
    size_t l;
    int e;

    e = _heim_time2generalizedtime (*data, &k, 0);
    if (e)
	return e;
    e = der_put_octet_string(p, len, &k, &l);
    free(k.data);
    if(e)
	return e;
    if(size)
	*size = l;
    return 0;
}

int
der_put_oid (unsigned char *p, size_t len,
	     const heim_oid *data, size_t *size)
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
der_put_tag (unsigned char *p, size_t len, Der_class class, Der_type type,
	     unsigned int tag, size_t *size)
{
    if (tag <= 30) {
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p = MAKE_TAG(class, type, tag);
	*size = 1;
    } else {
	size_t ret = 0;
	unsigned int continuation = 0;

	do {
	    if (len < 1)
		return ASN1_OVERFLOW;
	    *p-- = tag % 128 | continuation;
	    len--;
	    ret++;
	    tag /= 128;
	    continuation = 0x80;
	} while(tag > 0);
	if (len < 1)
	    return ASN1_OVERFLOW;
	*p-- = MAKE_TAG(class, type, 0x1f);
	ret++;
	*size = ret;
    }
    return 0;
}

int
der_put_length_and_tag (unsigned char *p, size_t len, size_t len_val,
			Der_class class, Der_type type,
			unsigned int tag, size_t *size)
{
    size_t ret = 0;
    size_t l;
    int e;

    e = der_put_length (p, len, len_val, &l);
    if(e)
	return e;
    p -= l;
    len -= l;
    ret += l;
    e = der_put_tag (p, len, class, type, tag, &l);
    if(e)
	return e;

    ret += l;
    *size = ret;
    return 0;
}

int
_heim_time2generalizedtime (time_t t, heim_octet_string *s, int gtimep)
{
     struct tm *tm;
     const size_t len = gtimep ? 15 : 13;

     s->data = malloc(len + 1);
     if (s->data == NULL)
	 return ENOMEM;
     s->length = len;
     tm = gmtime (&t);
     if (gtimep)
	 snprintf (s->data, len + 1, "%04d%02d%02d%02d%02d%02dZ",
		   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		   tm->tm_hour, tm->tm_min, tm->tm_sec);
     else
	 snprintf (s->data, len + 1, "%02d%02d%02d%02d%02d%02dZ",
		   tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday,
		   tm->tm_hour, tm->tm_min, tm->tm_sec);

     return 0;
}

int
der_put_bit_string (unsigned char *p, size_t len,
		    const heim_bit_string *data, size_t *size)
{
    size_t data_size = (data->length + 7) / 8;
    if (len < data_size + 1)
	return ASN1_OVERFLOW;
    p -= data_size + 1;

    memcpy (p+2, data->data, data_size);
    if (data->length && (data->length % 8) != 0)
	p[1] = 8 - (data->length % 8);
    else
	p[1] = 0;
    *size = data_size + 1;
    return 0;
}

int
_heim_der_set_sort(const void *a1, const void *a2)
{
    const struct heim_octet_string *s1 = a1, *s2 = a2;
    int ret;

    ret = memcmp(s1->data, s2->data,
		 s1->length < s2->length ? s1->length : s2->length);
    if(ret)
	return ret;
    return s1->length - s2->length;
}
/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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


/* RCSID("$Id$"); */

void
der_free_general_string (heim_general_string *str)
{
    free(*str);
    *str = NULL;
}

void
der_free_integer (int *i)
{
    *i = 0;
}

void
der_free_unsigned (unsigned *u)
{
    *u = 0;
}

void
der_free_generalized_time(time_t *t)
{
    *t = 0;
}

void
der_free_utctime(time_t *t)
{
    *t = 0;
}


void
der_free_utf8string (heim_utf8_string *str)
{
    free(*str);
    *str = NULL;
}

void
der_free_printable_string (heim_printable_string *str)
{
    free(*str);
    *str = NULL;
}

void
der_free_ia5_string (heim_ia5_string *str)
{
    free(*str);
    *str = NULL;
}

void
der_free_bmp_string (heim_bmp_string *k)
{
    free(k->data);
    k->data = NULL;
    k->length = 0;
}

void
der_free_universal_string (heim_universal_string *k)
{
    free(k->data);
    k->data = NULL;
    k->length = 0;
}

void
der_free_visible_string (heim_visible_string *str)
{
    free(*str);
    *str = NULL;
}

void
der_free_octet_string (heim_octet_string *k)
{
    free(k->data);
    k->data = NULL;
    k->length = 0;
}

void
der_free_heim_integer (heim_integer *k)
{
    free(k->data);
    k->data = NULL;
    k->length = 0;
}

void
der_free_oid (heim_oid *k)
{
    free(k->components);
    k->components = NULL;
    k->length = 0;
}

void
der_free_bit_string (heim_bit_string *k)
{
    free(k->data);
    k->data = NULL;
    k->length = 0;
}
/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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


/* RCSID("$Id$"); */

size_t
_heim_len_unsigned (unsigned val)
{
    size_t ret = 0;
    int last_val_gt_128;

    do {
	++ret;
	last_val_gt_128 = (val >= 128);
	val /= 256;
    } while (val);

    if(last_val_gt_128)
	ret++;

    return ret;
}

size_t
_heim_len_int (int val)
{
    unsigned char q;
    size_t ret = 0;

    if (val >= 0) {
	do {
	    q = val % 256;
	    ret++;
	    val /= 256;
	} while(val);
	if(q >= 128)
	    ret++;
    } else {
	val = ~val;
	do {
	    q = ~(val % 256);
	    ret++;
	    val /= 256;
	} while(val);
	if(q < 128)
	    ret++;
    }
    return ret;
}

static size_t
len_oid (const heim_oid *oid)
{
    size_t ret = 1;
    int n;

    for (n = 2; n < oid->length; ++n) {
	unsigned u = oid->components[n];

	do {
	    ++ret;
	    u /= 128;
	} while(u > 0);
    }
    return ret;
}

size_t
der_length_len (size_t len)
{
    if (len < 128)
	return 1;
    else {
	int ret = 0;
	do {
	    ++ret;
	    len /= 256;
	} while (len);
	return ret + 1;
    }
}

size_t
der_length_tag(unsigned int tag)
{
    size_t len = 0;

    if(tag <= 30)
	return 1;
    while(tag) {
	tag /= 128;
	len++;
    }
    return len + 1;
}

size_t
der_length_integer (const int *data)
{
    return _heim_len_int (*data);
}

size_t
der_length_unsigned (const unsigned *data)
{
    return _heim_len_unsigned(*data);
}

size_t
der_length_enumerated (const unsigned *data)
{
  return _heim_len_int (*data);
}

size_t
der_length_general_string (const heim_general_string *data)
{
    return strlen(*data);
}

size_t
der_length_utf8string (const heim_utf8_string *data)
{
    return strlen(*data);
}

size_t
der_length_printable_string (const heim_printable_string *data)
{
    return strlen(*data);
}

size_t
der_length_ia5_string (const heim_ia5_string *data)
{
    return strlen(*data);
}

size_t
der_length_bmp_string (const heim_bmp_string *data)
{
    return data->length * 2;
}

size_t
der_length_universal_string (const heim_universal_string *data)
{
    return data->length * 4;
}

size_t
der_length_visible_string (const heim_visible_string *data)
{
    return strlen(*data);
}

size_t
der_length_octet_string (const heim_octet_string *k)
{
    return k->length;
}

size_t
der_length_heim_integer (const heim_integer *k)
{
    if (k->length == 0)
	return 1;
    if (k->negative)
	return k->length + (((~(((unsigned char *)k->data)[0])) & 0x80) ? 0 : 1);
    else
	return k->length + ((((unsigned char *)k->data)[0] & 0x80) ? 1 : 0);
}

size_t
der_length_oid (const heim_oid *k)
{
    return len_oid (k);
}

size_t
der_length_generalized_time (const time_t *t)
{
    heim_octet_string k;
    size_t ret;

    _heim_time2generalizedtime (*t, &k, 1);
    ret = k.length;
    free(k.data);
    return ret;
}

size_t
der_length_utctime (const time_t *t)
{
    heim_octet_string k;
    size_t ret;

    _heim_time2generalizedtime (*t, &k, 0);
    ret = k.length;
    free(k.data);
    return ret;
}

size_t
der_length_boolean (const int *k)
{
    return 1;
}

size_t
der_length_bit_string (const heim_bit_string *k)
{
    return (k->length + 7) / 8 + 1;
}
/*
 * Copyright (c) 1997 - 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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


/* RCSID("$Id$"); */

int
der_copy_general_string (const heim_general_string *from,
			 heim_general_string *to)
{
    *to = strdup(*from);
    if(*to == NULL)
	return ENOMEM;
    return 0;
}

int
der_copy_integer (const int *from, int *to)
{
    *to = *from;
    return 0;
}

int
der_copy_unsigned (const unsigned *from, unsigned *to)
{
    *to = *from;
    return 0;
}

int
der_copy_generalized_time (const time_t *from, time_t *to)
{
    *to = *from;
    return 0;
}

int
der_copy_utctime (const time_t *from, time_t *to)
{
    *to = *from;
    return 0;
}

int
der_copy_utf8string (const heim_utf8_string *from, heim_utf8_string *to)
{
    return der_copy_general_string(from, to);
}

int
der_copy_printable_string (const heim_printable_string *from,
		       heim_printable_string *to)
{
    return der_copy_general_string(from, to);
}

int
der_copy_ia5_string (const heim_printable_string *from,
		     heim_printable_string *to)
{
    return der_copy_general_string(from, to);
}

int
der_copy_bmp_string (const heim_bmp_string *from, heim_bmp_string *to)
{
    to->length = from->length;
    to->data   = malloc(to->length * sizeof(to->data[0]));
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length * sizeof(to->data[0]));
    return 0;
}

int
der_copy_universal_string (const heim_universal_string *from,
			   heim_universal_string *to)
{
    to->length = from->length;
    to->data   = malloc(to->length * sizeof(to->data[0]));
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length * sizeof(to->data[0]));
    return 0;
}

int
der_copy_visible_string (const heim_visible_string *from,
			 heim_visible_string *to)
{
    return der_copy_general_string(from, to);
}

int
der_copy_octet_string (const heim_octet_string *from, heim_octet_string *to)
{
    to->length = from->length;
    to->data   = malloc(to->length);
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length);
    return 0;
}

int
der_copy_heim_integer (const heim_integer *from, heim_integer *to)
{
    to->length = from->length;
    to->data   = malloc(to->length);
    if(to->length != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, to->length);
    to->negative = from->negative;
    return 0;
}

int
der_copy_oid (const heim_oid *from, heim_oid *to)
{
    to->length     = from->length;
    to->components = malloc(to->length * sizeof(*to->components));
    if (to->length != 0 && to->components == NULL)
	return ENOMEM;
    memcpy(to->components, from->components,
	   to->length * sizeof(*to->components));
    return 0;
}

int
der_copy_bit_string (const heim_bit_string *from, heim_bit_string *to)
{
    size_t len;

    len = (from->length + 7) / 8;
    to->length = from->length;
    to->data   = malloc(len);
    if(len != 0 && to->data == NULL)
	return ENOMEM;
    memcpy(to->data, from->data, len);
    return 0;
}
