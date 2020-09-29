/*
 * Copyright (c) 2021 Sine Nomine Associates. All rights reserved.
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
#include "common.h"

static void
test_rx_opaque_cmp(void)
{
    int tc_i;
    struct {
	int expected;
	struct rx_opaque buf_a;
	struct rx_opaque buf_b;

    } *tc, test_cases[] = {
	{ 0, { 7, "foo\0bar" }, { 7, "foo\0bar" } },
	{-1, { 7, "foo\0bar" }, { 8, "foo\0bar" } },
	{ 1, { 7, "foo\0bar" }, { 6, "foo\0bar" } },
	{ 1, { 7, "foo\0bar" }, { 0, "foo\0bar" } },

	{-1, { 7, "foo\0bar" }, { 7, "zoo\0bar" } },
	{-1, { 7, "foo\0bar" }, { 6, "zoo\0bar" } },
	{-1, { 7, "foo\0bar" }, { 8, "zoo\0bar" } },
	{ 1, { 7, "foo\0bar" }, { 0, "zoo\0bar" } },

	{ 1, { 7, "foo\0bar" }, { 7, "boo\0bar" } },
	{ 1, { 7, "foo\0bar" }, { 6, "boo\0bar" } },
	{ 1, { 7, "foo\0bar" }, { 8, "boo\0bar" } },
	{ 1, { 7, "foo\0bar" }, { 0, "boo\0bar" } },

	{ 0, { 0, "foo\0bar" }, { 0, "abc\0def" } },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	int got;
	struct rx_opaque_stringbuf strbuf_a;
	struct rx_opaque_stringbuf strbuf_b;

	got = rx_opaque_cmp(&tc->buf_a, &tc->buf_b);
	is_int(got, tc->expected,
	       "rx_opaque_cmp(%s, %s) == %d",
	       rx_opaque_stringify(&tc->buf_a, &strbuf_a),
	       rx_opaque_stringify(&tc->buf_b, &strbuf_b),
	       tc->expected);
    }
}

static void
test_misc(void)
{
    struct rx_opaque *buf;
    struct rx_opaque buf_s = RX_EMPTY_OPAQUE;
    struct rx_opaque copy = RX_EMPTY_OPAQUE;
    struct rx_opaque exp = RX_EMPTY_OPAQUE;
    struct rx_opaque_stringbuf strbuf;
    int code;

    buf = rx_opaque_new("some\0data", 10);
    opr_Assert(buf != NULL);
    exp.val = "some\0data";
    exp.len = 10;
    is_opaque(buf, &exp, "rx_opaque_new()");

    rx_opaque_free(&buf);
    is_pointer(buf, NULL, "rx_opaque_free()");

    code = rx_opaque_alloc(&buf_s, 5);
    opr_Assert(code == 0);
    exp.val = "\0\0\0\0\0";
    exp.len = 5;
    is_opaque(&buf_s, &exp, "rx_opaque_alloc()");

    rx_opaque_freeContents(&buf_s);
    is_pointer(buf_s.val, NULL, "rx_opaque_freeContents() val");
    is_int(buf_s.len, 0, "rx_opaque_freeContents() len");

    code = rx_opaque_populate(&buf_s, "some\0data", 10);
    opr_Assert(code == 0);
    exp.val = "some\0data";
    exp.len = 10;
    is_opaque(&buf_s, &exp, "rx_opaque_populate()");

    rx_opaque_copy(&copy, &buf_s);
    rx_opaque_zeroFreeContents(&buf_s);
    is_opaque(&copy, &exp, "rx_opaque_copy()");
    is_pointer(buf_s.val, NULL, "rx_opaque_zeroFreeContents() val");
    is_int(buf_s.len, 0, "rx_opaque_zeroFreeContents() len");

    rx_opaque_freeContents(&copy);

    buf_s.len = 10;
    buf_s.val = "some\0data";
    is_string(rx_opaque_stringify(&buf_s, &strbuf), "10:736f6d65006461746100",
	      "rx_opaque_stringify()");

    {
	static unsigned char bigbuf[2000];

	bigbuf[2] = 0x12;
	bigbuf[102] = 0x34;

	buf_s.len = sizeof(bigbuf);
	buf_s.val = bigbuf;

	is_string(rx_opaque_stringify(&buf_s, &strbuf),
		  "2000:000012000000000"
		  "00000000000000000000"
		  "00000000000000000000"
		  "00000000000000000000"
		  "000000000000000000",
		  "rx_opaque_stringify() (bigbuf)");
    }
}

int
main(void)
{
    plan(24);

    test_rx_opaque_cmp();
    test_misc();

    return 0;
}
