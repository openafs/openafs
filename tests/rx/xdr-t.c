/*
 * Copyright (c) 2024 Sine Nomine Associates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <rx/rx_opaque.h>
#include <rx/xdr.h>
#include <afs/afsutil.h>

#include <tests/tap/basic.h>
#include <ctype.h>

#include "common.h"

/*
 * Tests for XDR encoding/decoding routines.
 *
 * The tests in this file roughly follow this convention, for example:
 *
 * struct {
 *     u_char val;
 *     char *hex;
 *     enum test_flags flags;
 * } *tc, test_cases[] = {
 *     { 5, "00000005" },
 *     { 5, "ffffff05", DECODE_ONLY },
 * }
 *
 * For each test case, we check that calling xdr_foo() will encode 'val' into
 * binary data represented by 'hex' (as a hexdump string). We also check that
 * calling xdr_foo() will decode the 'hex' data back into 'val'.
 *
 * By default, we check both encoding and decoding, and that the xdr_foo()
 * calls succeed. If ENCODE_ONLY is set in 'flags', then we only check the
 * encoding part of that test; if DECODE_ONLY is set, we only check decoding.
 * If ERROR is set, then xdr_foo() should return an error instead of
 * succeeding.
 *
 * The SMALLBUF3 and other SMALLBUFX flags limit the encoding buffer to only be
 * X bytes large. This lets us check that encoding routines throw an error if
 * the encoding buffer is too small.
 *
 * Each test case first calls test_init() to interpret flags and set various
 * globals (globals are used to try to make the test code more concise and
 * reduce clutter). Then we call functions to see what test modes/variants
 * we're running under. For example: encode_success() tells us whether
 * xdr_foo() encoding calls should return success or an error, and do_encode()
 * tells us whether we should run the encoding portion of tests at all. Calling
 * encode_xdr() and decode_xdr() gives us an XDR* that's been setup properly to
 * give to xdr_foo().
 */

enum test_flags {
    DECODE_ONLY = 0x01, /* Only test XDR decoding (not encoding). */
    ENCODE_ONLY = 0x02, /* Only test XDR encoding (not decoding). */

    ERROR	= 0x04, /* XDR encoding/decoding should fail with an error. */

    SMALLBUF0   = 0x08, /* Use 0 bytes of encoding space. */
    SMALLBUF3   = 0x10, /* Use only 3 bytes of encoding space. */
    SMALLBUF4   = 0x20, /* Use only 4 bytes of encoding space. */
    SMALLBUF7   = 0x40, /* Use only 7 bytes of encoding space. */
    SMALLBUF8   = 0x80, /* Use only 8 bytes of encoding space. */
};

struct test_globals {
    int do_decode;
    int do_encode;
    int decode_success;
    int encode_success;

    int decoding;
    int encoding;

    /* buffer for encoding XDR data. */
#define ENCODE_BUF_SIZE 4096
    char encode_buf[ENCODE_BUF_SIZE];
    size_t encode_buf_len;
    XDR encode_xdrs;

    /* buffer for decoding XDR data. */
    struct rx_opaque *decode_buf;
};
static struct test_globals globals;

/* '0' -> 0, 'f' -> 15, etc */
static unsigned char
hexchar(char hex)
{
    hex = tolower(hex);
    if (hex >= '0' && hex <= '9') {
	return hex - '0';
    }
    if (hex >= 'a' && hex <= 'f') {
	return hex - 'a' + 10;
    }
    bail("bad hex 0x%x", (unsigned int)hex);
    return 0;
}

/*
 * Given a hexdump string (e.g. "ab0f3d"), alloc and return an opaque buffer of
 * the actual data. Caller must free the returned opaque.
 */
static struct rx_opaque *
hex2opaque(const char *hex)
{
    struct rx_opaque *buf;
    size_t hex_len = strlen(hex);
    int cur_i;

    if (hex_len % 2 != 0) {
	bail("bad hex string: %s", hex);
    }

    buf = rx_opaque_new(NULL, 0);
    opr_Assert(buf != NULL);

    opr_Verify(rx_opaque_alloc(buf, hex_len / 2) == 0);

    for (cur_i = 0; cur_i < buf->len; cur_i++) {
	unsigned char *buf_cur = &((unsigned char *)buf->val)[cur_i];
	const char *hex_cur = &hex[cur_i * 2];

	*buf_cur = hexchar(hex_cur[0]) * 16 + hexchar(hex_cur[1]);
    }

    return buf;
}

/*
 * Set all of our globals according to the given flags for the current test
 * case.
 *
 * The data in 'hex' is used for decoding data via decode_xdr().
 */
static void
test_init(char *hex, enum test_flags flags)
{
    struct test_globals *g = &globals;

    rx_opaque_free(&g->decode_buf);

    memset(g, 0, sizeof(*g));

    g->do_decode = 1;
    g->do_encode = 1;
    g->decode_success = 1;
    g->encode_success = 1;

    if ((flags & DECODE_ONLY) != 0) {
	g->do_encode = 0;
    }
    if ((flags & ENCODE_ONLY) != 0) {
	g->do_decode = 0;
    }
    if ((flags & ERROR) != 0) {
	g->decode_success = 0;
	g->encode_success = 0;
    }

    g->encode_buf_len = sizeof(g->encode_buf);
    if ((flags & SMALLBUF0) != 0) {
	g->encode_buf_len = 0;
    }
    if ((flags & SMALLBUF3) != 0) {
	g->encode_buf_len = 3;
    }
    if ((flags & SMALLBUF4) != 0) {
	g->encode_buf_len = 4;
    }
    if ((flags & SMALLBUF7) != 0) {
	g->encode_buf_len = 7;
    }
    if ((flags & SMALLBUF8) != 0) {
	g->encode_buf_len = 8;
    }

    g->decode_buf = hex2opaque(hex);
}

/* Should we run XDR_DECODE tests for the current test case? */
static int
do_decode(void)
{
    globals.decoding = globals.do_decode;
    globals.encoding = 0;
    return globals.do_decode;
}

/* Should we run XDR_ENCODE tests for the current test case? */
static int
do_encode(void)
{
    globals.decoding = 0;
    globals.encoding = globals.do_encode;
    return globals.do_encode;
}

/* Should XDR_DECODE calls succeed for the current test case? */
static int
decode_success(void)
{
    opr_Assert(globals.decoding);
    return globals.decode_success;
}

/* Should XDR_ENCODE calls succeed for the current test case? */
static int
encode_success(void)
{
    opr_Assert(globals.encoding);
    return globals.encode_success;
}

/* Use the returned XDR to encode to our in-memory buffer. */
static XDR *
encode_xdr(void)
{
    struct test_globals *g = &globals;
    XDR *xdrs = &g->encode_xdrs;

    memset(xdrs, 0, sizeof(*xdrs));
    memset(g->encode_buf, 0, sizeof(g->encode_buf));
    xdrmem_create(xdrs, g->encode_buf, g->encode_buf_len, XDR_ENCODE);

    diag("encoding to %d-byte buffer", (int)g->encode_buf_len);

    return xdrs;
}

/*
 * After you are done encoding to the XDR from encode_xdr(), call this to get
 * an rx_opaque of the encoded data.
 */
static struct rx_opaque *
encode_buf(void)
{
    static struct rx_opaque buf;
    struct test_globals *g = &globals;
    XDR *xdrs = &g->encode_xdrs;

    buf.len = xdr_getpos(xdrs);
    buf.val = g->encode_buf;

    return &buf;
}

/* Return an XDR* to decode the data specified in test_init(). */
static XDR *
decode_xdr(void)
{
    static XDR xdrs;
    struct test_globals *g = &globals;

    memset(&xdrs, 0, sizeof(xdrs));
    xdrmem_create(&xdrs, g->decode_buf->val, g->decode_buf->len, XDR_DECODE);

    return &xdrs;
}

/*
 * Same as is_opaque(), but the second argument is specified as a hex string
 * instead of an rx_opaque, for convenience.
 */
static int
is_opaque_hex(struct rx_opaque *left, const char *right_hex,
	      const char *format, ...)
{
    va_list ap;
    struct rx_opaque *right = hex2opaque(right_hex);
    int success;

    va_start(ap, format);
    success = is_opaque_v(left, right, format, ap);
    va_end(ap);

    rx_opaque_free(&right);

    return success;
}

/*
 * Wrapper around xdr_bytes() that uses an rx_opaque instead of separate
 * buf/len args.
 */
static bool_t
xdr_bytes_opaque(XDR *xdrs, struct rx_opaque *buf, u_int maxsize)
{
    char **cpp = (char **)&buf->val;
    u_int len = buf->len;
    bool_t success = xdr_bytes(xdrs, cpp, &len, maxsize);
    buf->len = len;
    return success;
}

/*
 * This tests the following xdr functions together:
 *
 * - xdr_int64()
 * - xdr_afs_int64()
 * - xdr_uint64()
 * - xdr_afs_uint64()
 */
static void
test_xdr_int64(void)
{
    int tc_i;
    struct {
	afs_int64 val;
	afs_uint64 val_u;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	/*		  val		     val_u		  hex   flags */
	{		    0,			 0, "0000000000000000" },
	{		    1,			 1, "0000000000000001" },
	{		   -1, 0xffffffffffffffffL, "ffffffffffffffff" },
	{		   -1, 0xffffffffffffffffL, "ffffffffffffffff",	SMALLBUF8 },
	/* Error: encoding buffer too small */
	{		   -1, 0xffffffffffffffffL, "ffffffff",		ENCODE_ONLY | ERROR | SMALLBUF7 },
	/* Error: need 8 bytes to decode */
	{		    0,			 0, "",			DECODE_ONLY | ERROR },
	{		    0,			 0, "ffffffffffffff",	DECODE_ONLY | ERROR },
	{	 44444444444L,	      44444444444L, "0000000a5918671c" },
	{	-44444444444L, 0xfffffff5a6e798e4L, "fffffff5a6e798e4" },
	{ 0x7fffffffffffffffL, 0x7fffffffffffffffL, "7fffffffffffffff" },
	{-0x8000000000000000L, 0x8000000000000000L, "8000000000000000" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_int64(encode_xdr(), &tc->val), encode_success(),
		   "xdr_int64(%lld) == %d (encode)",
		   (long long)tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_afs_int64(encode_xdr(), &tc->val), encode_success(),
		   "xdr_afs_int64(%lld) == %d (encode)",
		   (long long)tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_uint64(encode_xdr(), &tc->val_u), encode_success(),
		   "xdr_uint64(%llu) == %d (encode)",
		   (long long unsigned)tc->val_u, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_afs_uint64(encode_xdr(), &tc->val_u), encode_success(),
		   "xdr_afs_uint64(%llu) == %d (encode)",
		   (long long unsigned)tc->val_u, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}
	if (do_decode()) {
	    afs_int64 got = 0;
	    afs_uint64 got_u = 0;

	    is_int(xdr_int64(decode_xdr(), &got), decode_success(),
		   "xdr_int64(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int64(got, tc->val, "... value matches");

	    is_int(xdr_afs_int64(decode_xdr(), &got), decode_success(),
		   "xdr_afs_int64(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int64(got, tc->val, "... value matches");

	    is_int(xdr_uint64(decode_xdr(), &got_u), decode_success(),
		   "xdr_uint64(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_uint64(got_u, tc->val_u, "... value matches");

	    is_int(xdr_afs_uint64(decode_xdr(), &got_u), decode_success(),
	       "xdr_afs_uint64(%s) == %d (decode)",
	       tc->hex, decode_success());
	    is_uint64(got_u, tc->val_u, "... value matches");
	}
    }
}

/*
 * This tests the following xdr functions together:
 *
 * - xdr_int()
 * - xdr_afs_int32()
 * - xdr_enum()
 * - xdr_u_int()
 * - xdr_afs_uint32()
 * - xdr_long()
 * - xdr_u_long()
 */
static void
test_xdr_int(void)
{
    int tc_i;
    struct {
	int val;
	u_int val_u;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	/*	  val	    val_u	  hex */
	{	    0,		0, "00000000" },
	{	    1,		1, "00000001" },
	{	   -1, 0xffffffff, "ffffffff" },

	{	   -1, 0xffffffff, "ffffffff", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{	   -1, 0xffffffff, "",	       ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: need 4 bytes to decode */
	{	    0,		0, "",	       DECODE_ONLY | ERROR },
	{	    0,		0, "ffffff",   DECODE_ONLY | ERROR },

	{    11259375,	 11259375, "00abcdef" },
	{   -11259375, 0xff543211, "ff543211" },
	{  2147483647, 2147483647, "7fffffff" },
	{ -2147483648, 2147483648, "80000000" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    long val_long = tc->val;
	    u_long val_ulong = tc->val_u;

	    is_int(xdr_int(encode_xdr(), &tc->val), encode_success(),
		   "xdr_int(%d) == %d (encode)",
		   tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_afs_int32(encode_xdr(), &tc->val), encode_success(),
		   "xdr_afs_int32(%d) == %d (encode)",
		   tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_enum(encode_xdr(), &tc->val), encode_success(),
		   "xdr_enum(%d) == %d (encode)",
		   tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_u_int(encode_xdr(), &tc->val_u), encode_success(),
		   "xdr_u_int(%u) == %d (encode)",
		   tc->val_u, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_afs_uint32(encode_xdr(), &tc->val_u), encode_success(),
		   "xdr_afs_uint32(%u) == %d (encode)",
		   tc->val_u, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_long(encode_xdr(), &val_long), encode_success(),
		   "xdr_long(%d) == %d (encode)",
		   tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");

	    is_int(xdr_u_long(encode_xdr(), &val_ulong), encode_success(),
		   "xdr_u_long(%u) == %d (encode)",
		   tc->val_u, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    int got = 0;
	    u_int got_u = 0;
	    long got_long = 0;
	    u_long got_ulong = 0;

	    is_int(xdr_int(decode_xdr(), &got), decode_success(),
		   "xdr_int(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");

	    is_int(xdr_enum(decode_xdr(), &got), decode_success(),
		   "xdr_enum(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");

	    is_int(xdr_u_int(decode_xdr(), &got_u), decode_success(),
		   "xdr_u_int(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got_u, tc->val_u, "... value matches");

	    is_int(xdr_long(decode_xdr(), &got_long), decode_success(),
		   "xdr_long(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got_long, tc->val, "... value matches");

	    is_int(xdr_u_long(decode_xdr(), &got_ulong), decode_success(),
		   "xdr_u_long(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got_ulong, tc->val_u, "... value matches");
	}
    }
}

static void
test_xdr_short(void)
{
    int tc_i;
    struct {
	short val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{      0, "00000000" },
	{      1, "00000001" },
	{      1, "abcd0001", DECODE_ONLY },
	{     -1, "ffffffff" },
	{     -1, "abcdffff", DECODE_ONLY },

	{     -1, "ffffffff", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{     -1, "",	      ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: need 4 bytes to decode */
	{      0, "",	      DECODE_ONLY | ERROR },
	{      0, "ffffff",   DECODE_ONLY | ERROR },

	{   2748, "00000abc" },
	{  -2748, "fffff544" },
	{  32767, "00007fff" },
	{ -32768, "ffff8000" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_short(encode_xdr(), &tc->val), encode_success(),
		   "xdr_short(%d) == %d (encode)",
		   (int)tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    short got = 0;

	    is_int(xdr_short(decode_xdr(), &got), decode_success(),
		   "xdr_short(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");
	}
    }
}

static void
test_xdr_u_short(void)
{
    int tc_i;
    struct {
	u_short val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{     0, "00000000" },
	{     1, "00000001" },
	{     1, "abcd0001", DECODE_ONLY },
	{  2748, "00000abc" },
	{ 65535, "0000ffff" },
	{ 65535, "abcdffff", DECODE_ONLY },

	{ 65535, "0000ffff", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{ 65535, "",	     ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: need 4 bytes to decode */
	{     0, "",	     DECODE_ONLY | ERROR },
	{     0, "ffffff",   DECODE_ONLY | ERROR },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_u_short(encode_xdr(), &tc->val), encode_success(),
		   "xdr_u_short(%u) == %d (encode)",
		   (unsigned)tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    u_short got = 0;

	    is_int(xdr_u_short(decode_xdr(), &got), decode_success(),
		   "xdr_u_short(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");
	}
    }
}

static void
test_xdr_char(void)
{
    int tc_i;
    struct {
	char val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{    0, "00000000" },
	{    1, "00000001" },
	{    1, "abcdef01", DECODE_ONLY },
	{   -1, "ffffffff" },
	{   -1, "abcdefff", DECODE_ONLY },

	{   -1, "ffffffff", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{   -1, "",	    ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: need 4 bytes to decode */
	{    0, "",	    DECODE_ONLY | ERROR },
	{    0, "ffffff",   DECODE_ONLY | ERROR },

	{   10, "0000000a" },
	{  -10, "fffffff6" },
	{  127, "0000007f" },
	{ -128, "ffffff80" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_char(encode_xdr(), &tc->val), encode_success(),
		   "xdr_char(%d) == %d (encode)",
		   (int)tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    char got = 0;

	    is_int(xdr_char(decode_xdr(), &got), decode_success(),
		   "xdr_char(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");
	}
    }
}

static void
test_xdr_u_char(void)
{
    int tc_i;
    struct {
	u_char val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{   0, "00000000" },
	{   1, "00000001" },
	{   1, "12345601", DECODE_ONLY },
	{  10, "0000000a" },
	{ 255, "000000ff" },
	{ 255, "123456ff", DECODE_ONLY },

	{ 255, "000000ff", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{ 255, "",	   ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: need 4 bytes to decode */
	{   0, "",	   DECODE_ONLY | ERROR },
	{   0, "ffffff",   DECODE_ONLY | ERROR },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_u_char(encode_xdr(), &tc->val), encode_success(),
		   "xdr_u_char(%u) == %d (encode)",
		   (unsigned)tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    u_char got = 0;

	    is_int(xdr_u_char(decode_xdr(), &got), decode_success(),
		   "xdr_u_char(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");
	}
    }
}

static void
test_xdr_bool(void)
{
    int tc_i;
    struct {
	bool_t val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{ 0, "00000000" },
	{ 1, "00000001" },

	{	   -1, "00000001", ENCODE_ONLY },
	{	    2, "00000001", ENCODE_ONLY },
	{  2147483647, "00000001", ENCODE_ONLY },
	{ -2147483648, "00000001", ENCODE_ONLY },

	{  1, "00000001", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{ -1, "",	  ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: need 4 bytes to decode */
	{  0, "ffffff",	  DECODE_ONLY | ERROR },

	{  1, "00000002", DECODE_ONLY },
	{  1, "ffffffff", DECODE_ONLY },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_bool(encode_xdr(), &tc->val), encode_success(),
		   "xdr_bool(%d) == %d (encode)",
		   tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    bool_t got = 0;

	    is_int(xdr_bool(decode_xdr(), &got), decode_success(),
		   "xdr_bool(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_int(got, tc->val, "... value matches");
	}
    }
}

static void
test_xdr_opaque(void)
{
    int tc_i;
    struct {
	struct rx_opaque val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{ { 0, "" }, "" },
	{ { 0, "" }, "", SMALLBUF0 },

	{ { 4, "a\0cd" },    "61006364" },
	{ { 4, "a\0cd" },    "61006364", SMALLBUF4 },
	/* Error: encoding buffer too small */
	{ { 4, "a\0cd" },    "",	 ENCODE_ONLY | ERROR | SMALLBUF3 },
	/* Error: missing padding */
	{ { 4, "\0\0\0\0" }, "610063",   DECODE_ONLY | ERROR },

	{ { 5, "a\0cde" },    "6100636465000000" },
	{ { 5, "a\0cde" },    "6100636465abcdef", DECODE_ONLY },
	{ { 5, "a\0cde" },    "6100636465",	  ENCODE_ONLY | ERROR | SMALLBUF7 },
	{ { 5, "a\0cde" },    "61006364650000",	  DECODE_ONLY | ERROR },

	{ { 6, "a\0cdef" },   "6100636465660000" },
	{ { 7, "a\0cdefg" },  "6100636465666700" },
	{ { 8, "a\0cdefgh" }, "6100636465666768" },
    };
    struct rx_opaque_stringbuf strbuf;

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_opaque(encode_xdr(), tc->val.val, tc->val.len), encode_success(),
	       "xdr_opaque(%s) == %d (encode)",
	       rx_opaque_stringify(&tc->val, &strbuf),
	       encode_success());

	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    struct rx_opaque got;

	    opr_Verify(rx_opaque_alloc(&got, tc->val.len) == 0);

	    is_int(xdr_opaque(decode_xdr(), got.val, got.len), decode_success(),
		   "xdr_opaque(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_opaque(&got, &tc->val, "... value matches");

	    rx_opaque_freeContents(&got);
	}
    }
}

static void
test_xdr_bytes(void)
{
    int tc_i;
    struct {
	u_int maxsize;
	struct rx_opaque val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{ 0, { 0, "" }, "00000000" },

	{ 1, { 1, "a" },     "0000000161000000" },
	/* Error: encoding buffer too small */
	{ 1, { 1, "a" },     "0000000161",	 ENCODE_ONLY | ERROR | SMALLBUF7 },
	/* Error: missing padding */
	{ 1, { 1, "a" },     "00000001610000",	 DECODE_ONLY | ERROR },

	{ 4, { 4, "a\0cd" }, "0000000461006364" },
	{ 4, { 4, "a\0cd" }, "0000000461006364", SMALLBUF8 },
	{ 5, { 4, "a\0cd" }, "0000000461006364" },
	/* Error: value larger than maxsize */
	{ 3, { 4, "a\0cd" }, "00000004",	 ENCODE_ONLY | ERROR },
	{ 3, { 4, NULL },    "0000000461006364", DECODE_ONLY | ERROR },

	{ 5, { 5, "a\0cde" },    "000000056100636465000000" },
	{ 5, { 5, "a\0cde" },    "000000056100636465abcdef", DECODE_ONLY },
	{ 6, { 6, "a\0cdef" },   "000000066100636465660000" },
	{ 7, { 7, "a\0cdefg" },  "000000076100636465666700" },
	{ 8, { 8, "a\0cdefgh" }, "000000086100636465666768" },
    };
    struct rx_opaque_stringbuf strbuf;

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    int success = xdr_bytes_opaque(encode_xdr(), &tc->val,
					   tc->maxsize);
	    is_int(success, encode_success(),
		   "xdr_bytes(%s, %u) == %d (encode)",
		   rx_opaque_stringify(&tc->val, &strbuf),
		   tc->maxsize,
		   encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    /* Decode */
	    struct rx_opaque got = RX_EMPTY_OPAQUE;
	    XDR xdrs = {0};
	    int success = xdr_bytes_opaque(decode_xdr(), &got, tc->maxsize);

	    is_int(success, decode_success(),
		   "xdr_bytes(%s, %u) == %d (decode)",
		   tc->hex, tc->maxsize, decode_success());
	    is_opaque(&got, &tc->val, "... value matches");

	    xdrs.x_op = XDR_FREE;
	    ok(xdr_bytes_opaque(&xdrs, &got, tc->maxsize),
	       "... XDR_FREE succeeds");
	}
    }
}

/*
 * Tests for xdr_bytes(), using a prealloc'd buffer.
 */
static void
test_xdr_bytes_prealloc(void)
{
    int tc_i;
    struct {
	u_int maxsize;
	int prealloc;
	struct rx_opaque val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{ 9, 4, { 4, "a\0cd" },	 "0000000461006364", DECODE_ONLY },
	{ 9, 5, { 4, "a\0cd" },	 "0000000461006364", DECODE_ONLY },
	/* Error: prealloc'd buffer too small */
	{ 9, 3, { 3, "\0\0\0" }, "0000000461006364", DECODE_ONLY | ERROR },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_decode()) {
	    /* Decode */
	    struct rx_opaque got;
	    int success;

	    opr_Verify(rx_opaque_alloc(&got, tc->prealloc) == 0);

	    success = xdr_bytes_opaque(decode_xdr(), &got, tc->maxsize);
	    is_int(success, decode_success(),
		   "xdr_bytes(%s, %u, prealloc %d) == %d (decode)",
		   tc->hex, tc->maxsize, tc->prealloc, decode_success());
	    is_opaque(&got, &tc->val, "... value matches");

	    rx_opaque_freeContents(&got);
	}
    }
}

static void
test_xdr_string(void)
{
    int tc_i;
    struct {
	u_int maxsize;
	char *val;
	char *hex;
	enum test_flags flags;
	int prealloc;
    } *tc, test_cases[] = {
	{ 0, "", "00000000" },

	{ 1, "a",	 "0000000161000000" },
	{ 1, "a",	 "0000000161000000", SMALLBUF8 },
	/* Error: encoding buffer too small */
	{ 1, "a",	 "0000000161",	     ENCODE_ONLY | ERROR | SMALLBUF7 },
	/* Error: missing padding */
	{ 1, "z",	 "00000001610000",   DECODE_ONLY | ERROR },

	{ 4, "abcd",     "0000000461626364" },
	/* Error: cannot use prealloc'd memory */
	{ 4, "",	 "0000000461626364", DECODE_ONLY | ERROR, 1 },
	/* Error: NUL byte in the string */
	{ 4, "zzzz",     "0000000461620064", DECODE_ONLY | ERROR },
	{ 4, "zzzz",     "0000000461626300", DECODE_ONLY | ERROR },

	/* Error: string is longer than maxsize */
	{ 3, "abcd",     "00000004",	     ENCODE_ONLY | ERROR },
	{ 3, NULL,	 "0000000461626364", DECODE_ONLY | ERROR},

	{ 5, "abcde",    "000000056162636465000000" },
	{ 6, "abcde",    "000000056162636465000000" },
	/* Padding ignored on decode */
	{ 6, "abcde",    "000000056162636465abcdef", DECODE_ONLY },
	{ 6, "abcdef",   "000000066162636465660000" },
	{ 7, "abcdefg",  "000000076162636465666700" },
	{ 8, "abcdefgh", "000000086162636465666768" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	if (do_encode()) {
	    is_int(xdr_string(encode_xdr(), &tc->val, tc->maxsize), encode_success(),
		   "xdr_string(%s, %u) == %d (encode)",
		   tc->val, tc->maxsize, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}

	if (do_decode()) {
	    char *got = NULL;

	    if (tc->prealloc) {
		static char scratch[32];
		got = scratch;
		diag("Using prealloc'd buffer");
	    }

	    is_int(xdr_string(decode_xdr(), &got, tc->maxsize), decode_success(),
		   "xdr_string(%s, %u) == %d (decode)",
		   tc->hex, tc->maxsize, decode_success());
	    is_string(got, tc->val, "... value matches");

	    if (!tc->prealloc) {
		xdrfree_string(&got);
	    }
	}
    }
}

static void
test_xdr_afsUUID(void)
{
    int tc_i;
    struct {
	char *val;
	char *hex;
	enum test_flags flags;
    } *tc, test_cases[] = {
	{ "00112233-4455-6677-88-99-aabbccddeeff",
	  "001122330000445500006677ffffff88ffffff99ffffffaaffffffbbffffffccffffffddffffffeeffffffff" },

	{ "00112233-4455-6677-88-99-aabbccddeeff",
	  "0011223311114455111166771111118811111199111111aa111111bb111111cc111111dd111111ee111111ff",
	  DECODE_ONLY },

	/* Error: encoding buffer too small */
	{ "00112233-4455-6677-88-99-aabbccddeeff",
	  "00112233",
	  ENCODE_ONLY | ERROR | SMALLBUF7 },

	/* Error: not enough bytes to decode */
	{ "00000000-0000-0000-00-00-000000000000",
	  "",
	  DECODE_ONLY | ERROR },
	{ "00112233-4455-6677-88-99-aabbccddee00",
	  "0011223300004455000066770000008800000099000000aa000000bb000000cc000000dd000000ee000000",
	  DECODE_ONLY | ERROR },
    };
    afsUUID val;

    for (afstest_Scan(test_cases, tc, tc_i)) {
	test_init(tc->hex, tc->flags);

	opr_Verify(afsUUID_from_string(tc->val, &val) == 0);

	if (do_encode()) {
	    is_int(xdr_afsUUID(encode_xdr(), &val), encode_success(),
		   "xdr_afsUUID(%s) == %d (encode)",
		   tc->val, encode_success());
	    is_opaque_hex(encode_buf(), tc->hex,
			  "... encoded buffer matches");
	}
	if (do_decode()) {
	    afsUUID got;
	    struct uuid_fmtbuf strbuf;

	    memset(&got, 0, sizeof(got));

	    is_int(xdr_afsUUID(decode_xdr(), &got), decode_success(),
		   "xdr_afsUUID(%s) == %d (decode)",
		   tc->hex, decode_success());
	    is_string(afsUUID_to_string(&got, &strbuf), tc->val,
		      "... value matches");
	}
    }
}

int
main(void)
{
    plan(721);

    is_int(xdr_void(), 1, "xdr_void() == 1");

    test_xdr_int64();
    test_xdr_int();
    test_xdr_short();
    test_xdr_u_short();
    test_xdr_char();
    test_xdr_u_char();
    test_xdr_bool();

    test_xdr_opaque();
    test_xdr_bytes();
    test_xdr_bytes_prealloc();
    test_xdr_string();

    test_xdr_afsUUID();

    /*
     * TODO: Tests for these xdr types are currently missing from this file:
     * - xdr_array()
     * - xdr_arrayN()
     * - xdr_float()
     * - xdr_double()
     * - xdr_union()
     * - xdr_reference()
     * - xdr_pointer()
     * - xdr_vector()
     * - xdr_wrapstring()
     */

    return 0;
}
