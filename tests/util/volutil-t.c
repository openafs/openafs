/*
 * Copyright 2018, Sine Nomine Associates and others.
 * All Rights Reserved.
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

#include <afs/opr.h>
#include <afs/afsutil.h>
#include <tests/tap/basic.h>
#include "common.h"

char *out_of_range_names[] = {
    "/vicepiv",
    "/vicepzz",
    "vicepiv",
    "vicepzz",
    "iv",
    "zz",
    "255",
    "256",
    NULL,
};

char *invalid_names[] = {
    "",
    "/vice",
    "/vicep",
    "/vicepbogus",
    "/vicep0",
    "vice",
    "vicep",
    "vicepbogus",
    "vicep0",
    "foo",
    "!?",
    "-1",
    NULL,
};

void
test_partition_name_to_id(void)
{
    char x, y;
    int id;
    char part[32];
    char **t;

    /*
     * Test conversion of XX to partition id,
     * where XX is "a".."z", "aa"..."iu"
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "%c", x);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "%c%c", x, y);
            is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /*
     * Test conversion of /vicepXX to partition id,
     * where XX is "a".."z", "aa"..."iu".
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "/vicep%c", x);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "/vicep%c%c", x,y );
            is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /*
     * Test conversion of vicepXX to partition id,
     * where XX is "a".."z", "aa"..."iu".
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "vicep%c", x);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "vicep%c%c", x,y );
            is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /*
     * Test numeric names in the range "0".."254" are passed through.
     */
    for (id = 0; id <= 254; id++) {
        sprintf(part, "%d", id);
        is_int(id, volutil_GetPartitionID(part), "\"%s\" is %d", part, id);
    }

    /*
     * Test out of range names.
     */
    for (t = out_of_range_names; *t != NULL; t++) {
        is_int(-1, volutil_GetPartitionID(*t), "\"%s\" is out of range", *t);
    }

    /*
     * Test invalid names.
     */
    for (t = invalid_names; *t != NULL; t++) {
        is_int(-1, volutil_GetPartitionID(*t), "\"%s\" is invalid", *t);
    }
}

void
test_partition_id_to_name(void)
{
    char x, y;
    int id;
    char part[32];

    /*
     * Test conversion of ids to partition names,
     * for ids of 0 to 254.
     */
    id = 0;
    for (x = 'a'; x <= 'z'; x++, id++) {
        sprintf(part, "/vicep%c", x);
        is_string(part, volutil_PartitionName(id), "%d is \"%s\"", id, part);
    }
    for (x = 'a'; x <= 'i'; x++) {
        for (y = 'a'; y <= 'z'; y++, id++) {
            sprintf(part, "/vicep%c%c", x,y );
            is_string(part, volutil_PartitionName(id), "%d is \"%s\"", id, part);
            if (x == 'i' && y == 'u')
                break;
        }
    }

    /* Test out of range values. */
    is_string("BAD VOLUME ID", volutil_PartitionName(-1), "-1 is invalid");
    is_string("BAD VOLUME ID", volutil_PartitionName(255), "255 is invalid");
    is_string("BAD VOLUME ID", volutil_PartitionName(256), "256 is invalid");

    /* Test buffer too short (thread safe variant of volutil_PartitionName). */
    is_string("SPC", volutil_PartitionName_r(0, part, 4), "buffer too short");
}

static void
test_GetInt32(void)
{
    int tc_i;
    struct {
	char *input;
	afs_int32 expected;
	afs_int32 code;
	const char *display;
    } *tc, test_cases[] = {
	/* Decimal */
	{	      "0",		0,  0 },
	{	      "1",		1,  0 },
	{	  "12345",	    12345,  0 },
	{    "2147483646",     2147483646,  0 },
	{    "2147483647",  MAX_AFS_INT32,  0 },
	{    "2147483648",    -2147483648,  0 }, /* bug: overflow */
	{   "11111111111",    -1773790777,  0 }, /* bug: overflow */
	{	     "-1",	       -1,  0 },
	{	 "-54321",	   -54321,  0 },
	{   "-2147483647",    -2147483647,  0 },
	{   "-2147483648",  MIN_AFS_INT32,  0 },
	{   "-2147483649",     2147483647,  0 }, /* bug: underflow */
	{  "-11111111111",     1773790777,  0 }, /* bug: underflow */
	/* Octal */
	{	     "00",		0,  0 },
	{	     "01",		1,  0 },
	{	   "0123",	       83,  0 },
	{  "017777777776",     2147483646,  0 },
	{  "017777777777",  MAX_AFS_INT32,  0 },
	{  "020000000000",    -2147483648,  0 }, /* bug: overflow */
	{	    "-01",	       -1,  0 },
	{	   "-077",	      -63,  0 },
	{ "-017777777777",    -2147483647,  0 },
	{ "-020000000000",  MIN_AFS_INT32,  0 },
	{ "-020000000001",     2147483647,  0 }, /* bug: underflow */
	/* Hexadecimal */
	{	    "0x0",		0,  0 },
	{	    "0x1",		1,  0 },
	{	 "0xAbCd",	   0xabcd,  0 },
	{    "0x7FFFFFFE",     2147483646,  0 },
	{    "0x7FFFFFFF",  MAX_AFS_INT32,  0 },
	{    "0x80000000",    -2147483648,  0 }, /* bug: overflow */
	{	 "-0xDeF",	   -0xdef,  0 },
	{   "-0x80000000",  MIN_AFS_INT32,  0 },
	{   "-0x80000001",     2147483647,  0 }, /* bug: underflow */
	/* Empty value */
	{	      "",  0,  0, "<empty>" },
	{	     "-",  0,  0 },
	{	    "0x",  0,  0 },
	{	   "-0x",  0,  0 },
	/* Whitespace */
	{	   "   ",    0,  0, "<sp><sp><sp>" },
	{	 "  123",  123,  0, "<sp><sp>123" },
	{      "\t\t123",  123,  0, "<tab><tab>123" },
	{       "\t 123",  123,  0, "<tab><sp>123", },
	{	 "123  ",    0, -1, "123<sp><sp>" },  /* Trailing whitespace is invalid. */
	{	 "\n123",    0, -1, "<newline>123" }, /* Newlines are invalid. */
	{	 "\r123",    0, -1, "<return>123" },  /* Returns are invalid. */
	/* Invalid syntax */
	{	     "+",  0, -1 },
	{	    "+1",  0, -1 },
	{	   "+01",  0, -1 },
	{	   "+0x",  0, -1 },
	{	  "+0x1",  0, -1 },
	{	  "123a",  0, -1 },
	{	   "0xG",  0, -1 },
	{	    "08",  0, -1 },
	{	 "hello",  0, -1 },
	{ "\x01\x02\x03",  0, -1, "<x01><x02><x03>" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 code;
	afs_int32 got = 99999;

	if (tc->display == NULL) {
	    tc->display = tc->input;
	}

	opr_Assert(got != tc->expected);
	code = util_GetInt32(tc->input, &got);
	is_int(code, tc->code, "util_GetInt32('%s') returns %d",
	       tc->display, tc->code);
	if (tc->code == 0) {
	    is_int(got, tc->expected, "util_GetInt32('%s') output is %d",
		   tc->display, tc->expected);
	}
    }
}

static void
test_GetUInt32(void)
{
    int tc_i;
    struct {
	char *input;
	afs_uint32 expected;
	afs_int32 code;
	const char *display;
    } *tc, test_cases[] = {
	/* Decimal */
	{	     "0",	       0U,  0 },
	{	     "1",	       1U,  0 },
	{	 "12345",	   12345U,  0 },
	{   "4294967295",  MAX_AFS_UINT32,  0 },
	{   "4294967296",	       0U,  0 }, /* bug: overflow */
	{  "11111111111",     2521176519U,  0 }, /* bug: overflow */
	/* Octal */
	{	    "00",	       0U,  0 },
	{	    "01",	       1U,  0 },
	{	  "0001",	       1U,  0 },
	{	  "0123",	    0123U,  0 },
	{ "037777777777",  MAX_AFS_UINT32,  0 },
	{ "040000000000",	       0U,  0 }, /* bug: overflow */
	/* Hexadecimal */
	{	   "0x0",	       0U,  0 },
	{	   "0x1",	       1U,  0 },
	{       "0xABCD",	  0xabcdU,  0 },
	{   "0xFFFFFFFF",  MAX_AFS_UINT32,  0 },
	{  "0x100000000",	       0U,  0 }, /* bug: overflow */
	/* Empty value */
	{	      "",  0,  0, "<empty>" },
	{	    "0x",  0,  0 },
	/* Whitespace */
	{	   "   ",   0,  0, "<sp><sp><sp>" },
	{	 "  123", 123,  0, "<sp><sp>123" },
	{      "\t\t123", 123,  0, "<tab><tab>123" },
	{       "\t 123", 123,  0, "<tab><sp>123", },
	{	 "123  ",   0, -1, "123<sp><sp>" },  /* Trailing whitespace is invalid. */
	{	 "\n123",   0, -1, "<newline>123" }, /* Newlines are invalid. */
	{	 "\r123",   0, -1, "<return>123" },  /* Returns are invalid. */
	/* Invalid syntax */
	{	     "+",  0, -1 },
	{	    "+1",  0, -1 },
	{	   "+01",  0, -1 },
	{	   "+0x",  0, -1 },
	{	  "+0x1",  0, -1 },
	{	     "-",  0, -1 },
	{	    "-1",  0, -1 },
	{	   "-01",  0, -1 },
	{	   "-0x",  0, -1 },
	{	  "-0x1",  0, -1 },
	{	  "123a",  0, -1 },
	{	   "0xG",  0, -1 },
	{	    "08",  0, -1 },
	{	 "hello",  0, -1 },
	{ "\x01\x02\x03",  0, -1, "<x01><x02><x03>" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 code;
	afs_uint32 got = 99999;

	if (tc->display == NULL) {
	    tc->display = tc->input;
	}

	opr_Assert(got != tc->expected);
	code = util_GetUInt32(tc->input, &got);
	is_int(code, tc->code, "util_GetUInt32('%s') returns %d",
	       tc->display, tc->code);
	if (tc->code == 0) {
	    is_int(got, tc->expected, "util_GetUInt32('%s') output is %u",
		   tc->display, tc->expected);
	}
    }
}

static void
test_GetInt64(void)
{
    int tc_i;
    struct {
	char *input;
	afs_int64 expected;
	afs_int32 code;
	const char *display;
    } *tc, test_cases[] = {
	/* Decimal */
	{		      "0",			0LL,   0 },
	{	  "1234567890123",	    1234567890123LL,   0 },
	{   "9223372036854775806",    9223372036854775806LL,   0 },
	{   "9223372036854775807",	      MAX_AFS_INT64,   0 },
	{   "9223372036854775808",  -9223372036854775808ULL,   0 }, /* bug: overflow */
	{  "11111111111111111111",   -7335632962598440505LL,   0 }, /* bug: overflow */
	{		     "-1",		       -1LL,   0 },
	{	 "-1234567890123",	   -1234567890123LL,   0 },
	{  "-9223372036854775807",   -9223372036854775807LL,   0 },
	{  "-9223372036854775808",	      MIN_AFS_INT64,   0 },
	{  "-9223372036854775809",    9223372036854775807LL,   0 }, /* bug: underflow */
	{ "-11111111111111111111",    7335632962598440505LL,   0 }, /* bug: underflow */
	/* Octal */
	{			"01",			    1LL,  0 },
	{		  "01234567",		     01234567LL,  0 },
	{   "0777777777777777777776",  0777777777777777777776LL,  0 },
	{   "0777777777777777777777",		  MAX_AFS_INT64,  0 },
	{  "01000000000000000000000",   -9223372036854775808ULL,  0 }, /* bug: overflow */
	{  "01000000000000000000001",   -9223372036854775807ULL,  0 }, /* bug: overflow */
	{		       "-01",			   -1LL,  0 },
	{		 "-07654321",		    -07654321LL,  0 },
	{  "-0777777777777777777777", -0777777777777777777777LL,  0 },
	{ "-01000000000000000000000",		  MIN_AFS_INT64,  0 },
	{ "-01000000000000000000001",     9223372036854775807LL,  0 }, /* bug: underflow */
	/* Hexadecimal */
	{		  "0x0",			0LL,  0 },
	{		  "0x1",			1LL,  0 },
	{	  "0xABCDEF123",	      0xabcdef123LL,  0 },
	{  "0x7FFFFFFFFFFFFFFE",	 0x7FFFFFFFFFFFFFFE,  0 },
	{  "0x7FFFFFFFFFFFFFFF",	      MAX_AFS_INT64,  0 },
	{  "0x8000000000000000",    -9223372036854775808ULL,  0 }, /* bug: overflow */
	{  "0x8000000000000001",    -9223372036854775807ULL,  0 }, /* bug: overflow */
	{	 "-0x321FEDCBA",	     -0x321fedcbaLL,  0 },
	{ "-0x8000000000000000",	      MIN_AFS_INT64,  0 },
	{ "-0x8000000000000001",      9223372036854775807LL,  0 }, /* bug: underflow */
	/* Empty value */
	{	      "",  0,  0, "<empty>" },
	{	     "-",  0,  0 },
	{	    "0x",  0,  0 },
	{	   "-0x",  0,  0 },
	/* Whitespace */
#if 0
	{	   "   ",    0,  0, "<sp><sp><sp>" },  /* bug: infinite loop */
	{	 "  123",  123,  0, "<sp><sp>123" },   /* bug: infinite loop */
	{      "\t\t123",  123,  0, "<tab><tab>123" }, /* bug: infinite loop */
	{       "\t 123",  123,  0, "<tab><sp>123", }, /* bug: infinite loop */
#endif
	{	 "123  ",    0, -1, "123<sp><sp>" },   /* Trailing whitespace is invalid. */
	{	 "\n123",    0, -1, "<newline>123" },  /* Newlines are invalid. */
	{	 "\r123",    0, -1, "<return>123" },   /* Returns are invalid. */
	/* Invalid syntax */
	{	     "+",  0, -1 },
	{	    "+1",  0, -1 },
	{	   "+01",  0, -1 },
	{	   "+0x",  0, -1 },
	{	  "+0x1",  0, -1 },
	{	  "123a",  0, -1 },
	{	   "0xG",  0, -1 },
	{	    "08",  0, -1 },
	{	 "hello",  0, -1 },
	{ "\x01\x02\x03",  0, -1, "<x01><x02><x03>" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 code;
	afs_int64 got = 99999;

	if (tc->display == NULL) {
	    tc->display = tc->input;
	}

	opr_Assert(got != tc->expected);
	code = util_GetInt64(tc->input, &got);
	is_int(code, tc->code, "util_GetInt64('%s') returns %d",
	       tc->display, tc->code);
	if (tc->code == 0) {
	    is_int64(got, tc->expected, "util_GetInt64('%s') output is %lld",
		     tc->display, tc->expected);
	}
    }
}

static void
test_GetUInt64(void)
{
    int tc_i;
    struct {
	char *input;
	afs_uint64 expected;
	afs_int32 code;
	const char *display;
    } *tc, test_cases[] = {
	/* Decimal */
	{		      "0",		      0ULL,  0 },
	{		      "1",		      1ULL,  0 },
	{	  "1234567890123",	  1234567890123ULL,  0 },
	{  "18446744073709551614", 18446744073709551614ULL,  0 },
	{  "18446744073709551615",	    MAX_AFS_UINT64,  0 },
	{  "18446744073709551616",		      0ULL,  0 }, /* bug: overflow */
	{  "18446744073709551617",		      1ULL,  0 }, /* bug: overflow */
	{ "111111111111111111111",   430646668853801415ULL,  0 }, /* bug: overflow */
	/* Octal */
	{		       "00",			    0ULL,  0 },
	{		       "01",			   01ULL,  0 },
	{		 "01234567",		     01234567ULL,  0 },
	{ "01777777777777777777776",  01777777777777777777776ULL,  0 },
	{ "01777777777777777777777",		  MAX_AFS_UINT64,  0 },
	{ "02000000000000000000000",			    0ULL,  0 }, /* bug: overflow */
	/* Hexadecimal */
	{		  "0x0",		     0ULL,  0 },
	{		  "0x1",		     1ULL,  0 },
	{	  "0xABCDEF123",	   0xABCDEF123ULL,  0 },
	{  "0xFFFFFFFFFFFFFFFE",    0xFFFFFFFFFFFFFFFEULL,  0 },
	{  "0xFFFFFFFFFFFFFFFF",	   MAX_AFS_UINT64,  0 },
	{ "0x10000000000000000",		     0ULL,  0 }, /* bug: overflow */
	/* Empty value */
	{	      "",  0,  0, "<empty>" },
	{	    "0x",  0,  0 },
	/* Whitespace */
#if 0
	{	   "   ",   0,  0, "<sp><sp><sp>" },   /* bug: infinite loop */
	{	 "  123", 123,  0, "<sp><sp>123" },    /* bug: infinite loop */
	{      "\t\t123", 123,  0, "<tab><tab>123" },  /* bug: infinite loop */
	{       "\t 123", 123,  0, "<tab><sp>123" },   /* bug: infinite loop */
#endif
	{	 "123  ",   0, -1, "123<sp><sp>" },  /* Trailing whitespace is invalid. */
	{	 "\n123",   0, -1, "<newline>123" }, /* Newlines are invalid. */
	{	 "\r123",   0, -1, "<return>123" },  /* Returns are invalid. */
	/* Invalid syntax */
	{	     "+",  0, -1 },
	{	    "+1",  0, -1 },
	{	   "+01",  0, -1 },
	{	   "+0x",  0, -1 },
	{	  "+0x1",  0, -1 },
	{	     "-",  0, -1 },
	{	    "-1",  0, -1 },
	{	   "-01",  0, -1 },
	{	   "-0x",  0, -1 },
	{	  "-0x1",  0, -1 },
	{	  "123a",  0, -1 },
	{	   "0xG",  0, -1 },
	{	    "08",  0, -1 },
	{	 "hello",  0, -1 },
	{ "\x01\x02\x03",  0, -1, "<x01><x02><x03>" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 code;
	afs_uint64 got = 99999;

	if (tc->display == NULL) {
	    tc->display = tc->input;
	}

	opr_Assert(got != tc->expected);
	code = util_GetUInt64(tc->input, &got);
	is_int(code, tc->code, "util_GetUInt64('%s') returns %d",
	       tc->display, tc->code);
	if (tc->code == 0) {
	    is_uint64(got, tc->expected, "util_GetUInt64('%s') output is %llu",
		      tc->display, tc->expected);
	}
    }
}

int
main(int argc, char **argv)
{
    plan(1607);

    test_partition_name_to_id();
    test_partition_id_to_name();
    test_GetInt32();
    test_GetUInt32();
    test_GetInt64();
    test_GetUInt64();

    return 0;
}
