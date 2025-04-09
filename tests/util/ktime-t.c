/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <time.h>
#include <errno.h>

#include <afs/opr.h>
#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/afsutil_prototypes.h>
#include <tests/tap/basic.h>
#include "common.h"

/* Arbitrary fixed time for testing. */
static const time_t TEST_NOW = 1743998400L;  /* Sun Apr  6 23:00:00 2025 */

/* Common struct ktime_date masks for tests. */
static const int MASK_DATE = KTIMEDATE_YEAR | KTIMEDATE_MONTH | KTIMEDATE_DAY;
static const int MASK_DATETIME = KTIMEDATE_YEAR | KTIMEDATE_MONTH | KTIMEDATE_DAY |
				 KTIMEDATE_HOUR | KTIMEDATE_MIN | KTIMEDATE_SEC;

/*
 * Buffer for formatted struct ktime and struct ktime_date values.
 */
struct ktbuf {
    char buffer[128];
};

/*
 * Helper function to format struct ktime values.
 */
static char *
format_ktime(struct ktime *kt, struct ktbuf *buf)
{
    snprintf(buf->buffer, sizeof(buf->buffer),
	     "{0x%02x, %d, %d, %d, %d}",
	     kt->mask, kt->hour, kt->min, kt->sec, kt->day);
    return buf->buffer;
}

/*
 * Helper function to format struct ktime_date values.
 */
static char *
format_ktime_date(struct ktime_date *ktd, struct ktbuf *buf)
{
    snprintf(buf->buffer, sizeof(buf->buffer),
	     "{0x%02x, %hd, %hd, %hd, %hd, %hd, %hd}",
	     ktd->mask,
	     ktd->year, ktd->month, ktd->day,
	     ktd->hour, ktd->min, ktd->sec);
    return buf->buffer;
}

/*
 * Helper function to format time_t values.
 */
static char *
format_timestamp(time_t seconds)
{
    char *timestamp;

    timestamp = ctime(&seconds);
    opr_Assert(timestamp != NULL);
    timestamp[24] = '\0'; /* Trim the trailing newline. */
    return timestamp;
}

/*
 * ktime_DateOf() tests.
 */
static void
test_ktime_DateOf(void)
{
    int tc_i;
    struct {
	afs_int32 seconds_input;
	const char *expected;
    } *tc, test_cases[] = {
	{      -1000,  "Wed Dec 31 18:43:20 1969"},
	{         -1,  "Wed Dec 31 18:59:59 1969"},
	{          0,  "Wed Dec 31 19:00:00 1969"},
	{        420,  "Wed Dec 31 19:07:00 1969"},
	{  320734799,  "Fri Feb 29 23:59:59 1980"},
	{  320734800,  "Sat Mar  1 00:00:00 1980"},
	{  628664400,  "Sun Dec  3 00:00:00 1989"},
	{  946702740,  "Fri Dec 31 23:59:00 1999"},
	{  946702799,  "Fri Dec 31 23:59:59 1999"},
	{  978325200,  "Mon Jan  1 00:00:00 2001"},
	{ 1713070800,  "Sun Apr 14 00:00:00 2024"},
	{ 2147483647,  "Mon Jan 18 22:14:07 2038"},
	{-2147483648,  "Fri Dec 13 15:45:52 1901"},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	char *got = ktime_DateOf(tc->seconds_input);
	is_string(got, tc->expected, "ktime_DateOf(%d) returns '%s'",
		  tc->seconds_input, tc->expected);
    }
}

/*
 * ktime_Str2int32() tests.
 */
static void
test_ktime_Str2int32(void)
{
    int tc_i;
    struct {
	char *input;
	afs_int32 expected;
    } *tc, test_cases[] = {
	{             "",      0},
	{            "0",      0},
	{          "000",      0},
	{     "00:00:01",      1},
	{          "::1",      1},
	{        "0:0:1",      1},
	{           ":5",    300},
	{            "1",   3600},
	{          "001",   3600},
	{           "11",  39600},
	{           "12",  43200},
	{           "13",  46800},
	{         "1:00",   3600},
	{        "01:12",   4320},
	{         "5:30",  19800},
	{        "12:33",  45180},
	{     "12:33:12",  45192},
	{        "12:59",  46740},
	{        "12:60",     -1},
	{        "12:61",     -1},
	{           "13",  46800},
	{        "13:00",  46800},
	{        "13:59",  50340},
	{        "17:00",  61200},
	{        "17:99",     -1},
	{           "23",  82800},
	{        "23:59",  86340},
	{     "23:59:59",  86399},
	{           "24",     -1},
	{        "24:01",     -1},
	{"15:16:17 junk",     -1},
	{     "15:xx:yy",     -1},
	{        " 1:12",     -1},
	{           "-1",     -1},
	{        "bogus",     -1},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 got = ktime_Str2int32(tc->input);
	is_int(got, tc->expected, "ktime_Str2int32('%s') returns %d",
	    tc->input, tc->expected);
    }
}

/*
 * ktime_ParsePeriodic() tests.
 */
static void
test_ktime_ParsePeriodic(void)
{
    int tc_i;
    struct {
	char *input;
	int code;
	struct ktime expected;
    } *tc, test_cases[] = {
	/*          input   code          mask  hour  min  sec day */
	{              "",    0,         {0x00,   0,   0,   0,  0}},
	{           "now",    0,    {KTIME_NOW,   0,   0,   0,  0}},
	{         "never",    0,  {KTIME_NEVER,   0,   0,   0,  0}},
	{             "0",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{             "1",    0,   {KTIME_TIME,   1,   0,   0,  0}},
	{            "00",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{            "01",    0,   {KTIME_TIME,   1,   0,   0,  0}},
	{            "23",    0,   {KTIME_TIME,  23,   0,   0,  0}},
	{            "24",   -1},
	{         "00:00",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{      "00:00:00",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{         "00:01",    0,   {KTIME_TIME,   0,   1,   0,  0}},
	{         "00:59",    0,   {KTIME_TIME,   0,  59,   0,  0}},
	{         "00:60",  -1},
	{      "00:00:60",  -1},
	{         "01:17",    0,   {KTIME_TIME,   1,  17,   0,  0}},
	{         "11:59",    0,   {KTIME_TIME,  11,  59,   0,  0}},
	{         "12:00",    0,   {KTIME_TIME,  12,   0,   0,  0}},
	{         "12:01",    0,   {KTIME_TIME,  12,   1,   0,  0}},
	{      "12:59:59",    0,   {KTIME_TIME,  12,  59,  59,  0}},
	{         "13:00",    0,   {KTIME_TIME,  13,   0,   0,  0}},
	{          "5:00",    0,   {KTIME_TIME,   5,   0,   0,  0}},
	{         "23:59",    0,   {KTIME_TIME,  23,  59,   0,  0}},
	{      "23:59:59",    0,   {KTIME_TIME,  23,  59,  59,  0}},
	{      "24:00:00",  -1},
	{   "11:59:59 am",    0,   {KTIME_TIME,  11,  59,  59,  0}},
	{   "12:00:00 am",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{   "12:00:01 am",    0,   {KTIME_TIME,   0,   0,   1,  0}},
	{      "12:00 am",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{       "12:00am",   -1},
	{    "12:00 a.m.",    0,   {KTIME_TIME,   0,   0,   0,  0}},
	{     "12:00a.m.",  -1},
	{   "11:59:59 pm",    0,   {KTIME_TIME,  23,  59,  59,  0}},
	{   "12:00:00 pm",    0,   {KTIME_TIME,  12,   0,   0,  0}},
	{   "12:00:01 pm",    0,   {KTIME_TIME,  12,   0,   1,  0}},
	{      "12:00 pm",    0,   {KTIME_TIME,  12,   0,   0,  0}},
	{       "12:00pm",  -1},
	{    "12:00 p.m.",    0,   {KTIME_TIME,  12,   0,   0,  0}},
	{     "12:00p.m.",  -1},
	{           "sun",    0,    {KTIME_DAY,   0,   0,   0,  0}},
	{           "mon",    0,    {KTIME_DAY,   0,   0,   0,  1}},
	{           "tue",    0,    {KTIME_DAY,   0,   0,   0,  2}},
	{           "wed",    0,    {KTIME_DAY,   0,   0,   0,  3}},
	{           "thu",    0,    {KTIME_DAY,   0,   0,   0,  4}},
	{           "fri",    0,    {KTIME_DAY,   0,   0,   0,  5}},
	{           "sat",    0,    {KTIME_DAY,   0,   0,   0,  6}},
	{        "sunday",    0,    {KTIME_DAY,   0,   0,   0,  0}},
	{        "monday",    0,    {KTIME_DAY,   0,   0,   0,  1}},
	{       "tuesday",    0,    {KTIME_DAY,   0,   0,   0,  2}},
	{     "wednesday",    0,    {KTIME_DAY,   0,   0,   0,  3}},
	{      "thursday",    0,    {KTIME_DAY,   0,   0,   0,  4}},
	{          "thur",    0,    {KTIME_DAY,   0,   0,   0,  4}},
	{        "friday",    0,    {KTIME_DAY,   0,   0,   0,  5}},
	{      "saturday",    0,    {KTIME_DAY,   0,   0,   0,  6}},
	{"saturday night",  -1},
	/*                   input  code                    mask  hour min sec day */
	{              "mon 12:00",  0,  {KTIME_TIME | KTIME_DAY,  12,  0,  0,  1}},
	{           "mon 12:00 am",  0,  {KTIME_TIME | KTIME_DAY,   0,  0,  0,  1}},
	{        "monday 12:00 am",  0,  {KTIME_TIME | KTIME_DAY,   0,  0,  0,  1}},
	{               "at 12:00",  0,              {KTIME_TIME,  12,  0,  0,  0}},
	{            "at 12:00 am",  0,              {KTIME_TIME,   0,  0,  0,  0}},
	{        "at mon 12:00 am",  0,  {KTIME_TIME | KTIME_DAY,   0,  0,  0,  1}},
	{     "at monday 12:00 am",  0,  {KTIME_TIME | KTIME_DAY,   0,  0,  0,  1}},
	{            "every 12:00",  0,              {KTIME_TIME,  12,  0,  0,  0}},
	{         "every 12:00 am",  0,              {KTIME_TIME,   0,  0,  0,  0}},
	{     "every tue 12:00 am",  0,  {KTIME_TIME | KTIME_DAY,   0,  0,  0,  2}},
	{"every friday at 5:00 pm",  0,  {KTIME_TIME | KTIME_DAY,  17,  0,  0,  5}},
	{                     "at",  0,                    {0x00,   0,  0,  0,  0}},
	{                  "every",  0,                    {0x00,   0,  0,  0,  0}},
	/* input  code */
	{"-1",    -1},
	{"bogus", -1},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktime got;
	int code;

	memset(&got, 0, sizeof(got));
	code = ktime_ParsePeriodic(tc->input, &got);
	is_int(code, tc->code, "ktime_ParsePeriodic('%s') returns %d", tc->input, tc->code);
	if (tc->code == 0) {
	    is_int(got.mask, tc->expected.mask, "  mask is 0x%02x", tc->expected.mask);
	    is_int(got.hour, tc->expected.hour, "  hour is %d", tc->expected.hour);
	    is_int(got.min, tc->expected.min, "  min is %d", tc->expected.min);
	    is_int(got.sec, tc->expected.sec, "  sec is %d", tc->expected.sec);
	    is_int(got.day, tc->expected.day, "  day is %d", tc->expected.day);
	}
    }
}

/*
 * ktime_DisplayString() tests.
 */
static void
test_ktime_DisplayString(void)
{
    int tc_i;
    struct {
	struct ktime input;
	int code;
	char *expected;
    } *tc, test_cases[] = {
	/*                              mask   hour min  sec day code  expected */
	{{                       KTIME_NEVER,   0,   0,   0,  0},  0,  "never"},
	{{                         KTIME_NOW,   0,   0,   0,  0},  0,  "now"},
	{{                        KTIME_TIME,   0,   0,   0,  0},  0,  "at 12:00 am"},
	{{                        KTIME_TIME,   0,   1,   0,  0},  0,  "at 12:01 am"},
	{{                        KTIME_TIME,   0,   1,   2,  0},  0,  "at 12:01:02 am"},
	{{                        KTIME_TIME,   0,  59,  59,  0},  0,  "at 12:59:59 am"},
	{{                        KTIME_TIME,   1,   0,   0,  0},  0,  "at 1:00 am"},
	{{                        KTIME_TIME,   1,   0,   1,  0},  0,  "at 1:00:01 am"},
	{{                        KTIME_TIME,   6,   1,   2,  0},  0,  "at 6:01:02 am"},
	{{                        KTIME_TIME,  11,  59,  59,  0},  0,  "at 11:59:59 am"},
	{{                        KTIME_TIME,  12,   0,   0,  0},  0,  "at 12:00 pm"},
	{{                        KTIME_TIME,  12,   0,   1,  0},  0,  "at 12:00:01 pm"},
	{{                        KTIME_TIME,  14,   1,   2,  0},  0,  "at 2:01:02 pm"},
	{{                        KTIME_TIME,  23,  59,  59,  0},  0,  "at 11:59:59 pm"},
	{{                              0x00,   1,   2,   3,  4},  0,  "at"},
	{{                        KTIME_HOUR,   1,   2,   3,  4},  0,  "at 1 am"},
	{{                         KTIME_MIN,   1,   2,   3,  4},  0,  "at:02"},
	{{            KTIME_HOUR | KTIME_MIN,   1,   2,   3,  4},  0,  "at 1:02 am"},
	{{                         KTIME_SEC,   1,   2,   3,  4},  0,  "at:03"},
	{{            KTIME_HOUR | KTIME_SEC,   1,   2,   3,  4},  0,  "at 1:03 am"},
	{{             KTIME_MIN | KTIME_SEC,   1,   2,   3,  4},  0,  "at:02:03"},
	{{                        KTIME_TIME,   1,   2,   3,  4},  0,  "at 1:02:03 am"},
	{{                         KTIME_DAY,   1,   2,   3,  4},  0,  "at thu"},
	{{            KTIME_HOUR | KTIME_DAY,   1,   2,   3,  4},  0,  "at thu 1 am"},
	{{             KTIME_MIN | KTIME_DAY,   1,   2,   3,  4},  0,  "at thu:02"},
	{{KTIME_HOUR | KTIME_MIN | KTIME_DAY,   1,   2,   3,  4},  0,  "at thu 1:02 am"},
	{{             KTIME_SEC | KTIME_DAY,   1,   2,   3,  4},  0,  "at thu:03"},
	{{KTIME_HOUR | KTIME_SEC | KTIME_DAY,   1,   2,   3,  4},  0,  "at thu 1:03 am"},
	{{  KTIME_MIN | KTIME_SEC |KTIME_DAY,   1,   2,   3,  4},  0,  "at thu:02:03"},
	{{            KTIME_TIME | KTIME_DAY,   1,   2,   3,  4},  0,  "at thu 1:02:03 am"},
	{{                         KTIME_DAY,   0,   0,   0, -1},  EINVAL},
	{{                         KTIME_DAY,   0,   0,   0,  0},  0,  "at sun"},
	{{                         KTIME_DAY,   0,   0,   0,  1},  0,  "at mon"},
	{{                         KTIME_DAY,   0,   0,   0,  2},  0,  "at tue"},
	{{                         KTIME_DAY,   0,   0,   0,  3},  0,  "at wed"},
	{{                         KTIME_DAY,   0,   0,   0,  4},  0,  "at thu"},
	{{                         KTIME_DAY,   0,   0,   0,  5},  0,  "at fri"},
	{{                         KTIME_DAY,   0,   0,   0,  6},  0,  "at sat"},
	{{                         KTIME_DAY,   0,   0,   0,  7},  EINVAL},
	{{            KTIME_TIME | KTIME_DAY,   0,   0,   0,  1},  0,  "at mon 12:00 am"},
	{{            KTIME_TIME | KTIME_DAY,  17,   0,   0,  5},  0,  "at fri 5:00 pm"},
	{{            KTIME_TIME | KTIME_DAY,  17,   0,   1,  5},  0,  "at fri 5:00:01 pm"},
	{{                        KTIME_TIME,   0,   0,  -1,  0},  0,  "at 12:00:-1 am"},
	{{                        KTIME_TIME,   0,   0,  60,  0},  0,  "at 12:00:60 am"},
	{{                        KTIME_TIME,   0,   0,  61,  0},  0,  "at 12:00:61 am"},
	{{                        KTIME_TIME,   0,  -1,   0,  0},  0,  "at 12:-1 am"},
	{{                        KTIME_TIME,   0,  60,   0,  0},  0,  "at 12:60 am"},
	{{                        KTIME_TIME,   0,  61,   0,  0},  0,  "at 12:61 am"},
	{{                        KTIME_TIME,  -1,   0,   0,  0},  0,  "at -1:00 am"},
	{{                        KTIME_TIME,  24,   0,   0,  0},  0,  "at 12:00 pm"},
	{{                        KTIME_TIME,  25,   0,   0,  0},  0,  "at 13:00 pm"},
	{{                        KTIME_TIME,  99,   0,   0,  0},  0,  "at 87:00 pm"},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktbuf buf;
	int code;
	char got[512]; /* Note: ktime_DisplayString() can overflow the input buffer. */

	memset(got, 0, sizeof(got));
	code = ktime_DisplayString(&tc->input, got);
	is_int(code, tc->code, "ktime_DisplayString(%s) returns %d",
		  format_ktime(&tc->input, &buf), tc->code);
	if (tc->code == 0) {
	    is_string(got, tc->expected, "ktime_DisplayString('%s') output is '%s'",
		      format_ktime(&tc->input, &buf), tc->expected);
	}
    }
}

/*
 * ktime_next() tests.
 */
static void
test_ktime_next(void)
{
    int tc_i;
    struct {
	struct ktime input;
	afs_int32 afrom_input;
	afs_int32 expected;
    } *tc, test_cases[] = {
	/*                   mask  hour min  sec  day     from    expected */
	{{            KTIME_NEVER,   0,   0,   0,  0},      0,  2147483647},
	{{              KTIME_NOW,   0,   0,   0,  0},      0,           0},
	{{                   0x00,   0,   0,   0,  0},      0,  1744002000},
	{{             KTIME_HOUR,   0,   0,  30,  0},      0,  1744002030},
	{{              KTIME_MIN,   0,  30,   0,  0},      0,  1744003800},
	{{              KTIME_SEC,   1,   0,   0,  0},      0,  1744005600},
	{{             KTIME_TIME,   0,   1,   0,  0},      0,  1744002060},
	{{             KTIME_TIME,   0,   1,   2,  0},      0,  1744002062},
	{{             KTIME_TIME,   0,  59,  59,  0},      0,  1744005599},
	{{             KTIME_TIME,   1,   0,   0,  0},      0,  1744005600},
	{{             KTIME_TIME,   1,   0,   1,  0},      0,  1744005601},
	{{             KTIME_TIME,   6,   1,   2,  0},      0,  1744023662},
	{{             KTIME_TIME,  11,  59,  59,  0},      0,  1744045199},
	{{             KTIME_TIME,  12,   0,   0,  0},      0,  1744045200},
	{{             KTIME_TIME,  12,   0,   1,  0},      0,  1744045201},
	{{             KTIME_TIME,  14,   1,   2,  0},      0,  1744052462},
	{{             KTIME_TIME,  23,  59,  59,  0},      0,  1744001999},
	{{ KTIME_TIME | KTIME_DAY,   0,   0,   0,  1},      0,  1744002000},
	{{ KTIME_TIME | KTIME_DAY,  17,   0,   0,  5},      0,  1744408800},
	{{ KTIME_TIME | KTIME_DAY,  17,   0,   1,  5},      0,  1744408801},
	{{             KTIME_TIME,   0,   2,   3,  0},      0,  1744002123},
	{{             KTIME_TIME,   1,   2,   3,  0},      0,  1744005723},
	{{             KTIME_TIME,   1,   2,   3,  0},   3600,  1744005723},
	{{             KTIME_TIME,   1,   2,   3,  0},   7200,  1744005723},
	{{             KTIME_TIME,   1,   2,   3,  0},  10800,  1744092123},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktbuf buf;

	afs_int32 got = ktime_next(&tc->input, tc->afrom_input);
	is_int(got, tc->expected, "ktime_next(%s, %d) returned %d [%s]",
	       format_ktime(&tc->input, &buf), tc->afrom_input,
	       tc->expected, format_timestamp(tc->expected));
    }
}

/*
 * ktime_DateToInt32() tests.
 *
 * ktime_DateToInt32() will return an approximate result when the input date is
 * invalid. For example, "2/30/1980" is invalid since there are only 29 days in
 * February.
 */
static void
test_ktime_DateToInt32(void)
{
    int tc_i;
    struct {
	char *input;
	afs_int32 code;
	afs_int32 expected;
    } *tc, test_cases[] = {
	{               "now",  0,   TEST_NOW},
	{             "never",  0,         -1},
	{           "12/3/89",  0,  628664400},
	{             "1/1/1",  0,  978325200},
	{            "1/0/80", -2,          0},
	{           "13/1/80", -2,          0},
	{            "0/1/80", -2,          0},
	{           "1/32/80", -2,          0},
	{           "2/30/80",  0,  320734799}, /* Not a valid date. */
	{            "3/1/80",  0,  320734800},
	{       "3/1/80 0:00",  0,  320734800},
	{     "2/30/80 24:00", -2,          0},
	{     "2/30/80 23:60", -2,          0},
	{          "22/22/22", -2,          0},
	{    "12/31/69 19:07",  0,        420},
	{    "12/31/99 23:59",  0,  946702740},
	{ "12/31/99 23:59:59",  0,  946702799},
	{             "23:12", -1,          0},
	{             "22/12", -1,          0},
	{       "22/22/22 12", -1,          0},
	{"12/31/199 23:59:59", -2,          0},
	{        "12/31/1888", -2,          0},
	{ "-13/-44/22 -15:77", -2,          0},
	{           "4/14/24",  0, 1713070800},
	{         "4/14/2024",  0, 1713070800},
	{           "4/14/68",  0, 0x7fffffff},
	{           "4/14/69",  0,          0},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 code;
	afs_int32 got = 0;

	code = ktime_DateToInt32(tc->input, &got);
	is_int(code, tc->code, "ktime_DateToInt32('%s') returned %d",
	    tc->input, tc->code);
	is_int(got, tc->expected, "ktime_DateToInt32('%s') output is %d [%s]",
	    tc->input, tc->expected, format_timestamp(tc->expected));
    }
}

/*
 * ktime_GetDateUsage() tests.
 *
 * The usage string is subject to change, so just check it returns
 * a non-empty string.
 */
static void
test_ktime_GetDateUsage(void)
{
    char *got = ktime_GetDateUsage();
    ok((got != NULL), "ktime_GetDateUsage() returned a non-NULL pointer");
    ok((strlen(got) != 0), "ktime_GetDateUsage() returned a non-empty string");
}

/*
 * ktime_InterpretDate() tests.
 *
 * Note: This function expects the ktime_date year to be the year since 1900.
 */
static void
test_ktime_InterpretDate(void)
{
    int tc_i;
    struct {
	struct ktime_date input;
	afs_int32 expected;
    } *tc, test_cases[] = {
	/*             mask  year  mon  day hour  min  sec     expected */
	{{   KTIMEDATE_NOW,    0,   0,   0,   0,   0,   0},    TEST_NOW},
	{{ KTIMEDATE_NEVER,    0,   0,   0,   0,   0,   0},          -1},
	{{   MASK_DATETIME,    0,   0,   0,   0,   0,   0},           0},
	{{   MASK_DATETIME,    0,   0,   0,   0,   0,   1},           0},
	{{   MASK_DATETIME,   72,   0,   0,   0,   0,   0},    63089999},
	{{   MASK_DATETIME,   83,   1,   2,   3,   4,   5},   410342645},
	{{   MASK_DATETIME,   99,  12,  31,  23,  59,  59},   946702799},
	{{   MASK_DATETIME,  100,   1,   1,   0,   0,   0},   946702800},
	{{   MASK_DATETIME,  125,   4,   5,   0,   0,   0},  1743829200},
	{{   MASK_DATETIME,  138,   1,  18,  22,  14,   6},  2147483646},
	{{   MASK_DATETIME,  138,   1,  18,  22,  14,   7},  2147483647},
	{{   MASK_DATETIME,  138,   1,  18,  22,  14,   8},  2147483647},
	{{  KTIMEDATE_YEAR,   83,   1,   2,   3,   4,   5},   410417999},
	{{       MASK_DATE,   83,   1,   2,   3,   4,   5},   410417999},
	{{  KTIMEDATE_HOUR,   83,   1,   2,   3,   4,   5},   410345999},
	{{   MASK_DATETIME,   83,   1,   2,   3,   4,   5},   410342645},
	/*                                     mask   year  mon  day hour  min  sec     expected */
	{{           KTIMEDATE_YEAR | KTIMEDATE_MONTH,  83,   1,   2,   3,   4,   5},  410417999},
	{{                 MASK_DATE | KTIMEDATE_HOUR,  83,   1,   2,   3,   4,   5},  410345999},
	{{ MASK_DATE | KTIMEDATE_HOUR | KTIMEDATE_MIN,  83,   1,   2,   3,   4,   5},  410342699},
	{{ MASK_DATE | KTIMEDATE_HOUR | KTIMEDATE_SEC,  83,   1,   2,   3,   4,   5},  410343425},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktbuf buf;
	afs_int32 got;

	got = ktime_InterpretDate(&tc->input);
	is_int(got, tc->expected, "ktime_InterpretDate(%s) returned %d [%s]",
	       format_ktime_date(&tc->input, &buf),
	       tc->expected, format_timestamp(tc->expected));
    }
}

/*
 * ktimeRelDate_ToInt32() tests.
 *
 * The ktimeRelDate_ToInt32() function encodes a relative time (duration) as a
 * combination of a number of years, months, days into an integer
 * representation in days resolution (not seconds).
 *
 * The mask, hour, min, and sec fields of the struct ktime_date are ignored
 * by ktimeRelDate_ToInt32().
 *
 * ktimeRelDate_ToInt32() does not validate the input value ranges.
 */
static void
test_ktimeRelDate_ToInt32(void)
{
    int tc_i;
    struct {
	struct ktime_date input;
	int out_of_range;
	afs_int32 expected;
    } *tc, test_cases[] = {
	/*          mask     year    mon     day  hr  min sec  oor   expected */
	{{          0x00,      0,      0,      0,  0,  0,  0},  0,         0},
	{{          0x00,      0,      0,      1,  0,  0,  0},  0,         1},
	{{          0x00,      0,      0,     30,  0,  0,  0},  0,        30},
	{{          0x00,      0,      0,     31,  0,  0,  0},  1,        31},
	{{          0x00,      0,      1,      0,  0,  0,  0},  0,        31},
	{{          0x00,      0,      1,      1,  0,  0,  0},  0,        32},
	{{          0x00,      0,      1,     30,  0,  0,  0},  0,        61},
	{{          0x00,      0,      2,      0,  0,  0,  0},  0,        62},
	{{          0x00,      0,     11,      0,  0,  0,  0},  0,       341},
	{{          0x00,      0,     12,      0,  0,  0,  0},  1,       372},
	{{          0x00,      1,      0,      0,  0,  0,  0},  0,       372},
	{{          0x00,  32767,      0,      0,  0,  0,  0},  0,  12189324},
	{{          0x00,     -1,      0,      0,  0,  0,  0},  1,      -372},
	{{          0x00,      1,      1,      1,  0,  0,  0},  0,       404},
	{{          0x00,  32767,     11,     30,  0,  0,  0},  0,  12189695},
	{{          0x00,  32767,  32767,  32767,  0,  0,  0},  1,  13237868},
	{{          0x00,     -1,     -1,     -1,  0,  0,  0},  1,      -404},
	{{ MASK_DATETIME,      1,      2,      3,  0,  0,  0},  0,       437},
	{{ MASK_DATETIME,      1,      2,      3,  4,  5,  6},  0,       437},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktbuf buf;
	afs_int32 got;
	int code;

	got = ktimeRelDate_ToInt32(&tc->input);
	is_int(got, tc->expected, "ktimeRelDate_ToInt32(%s) returned %d",
	       format_ktime_date(&tc->input, &buf),
	       tc->expected);

	/* Verify the inverse for cases the input is in range. */
	if (!tc->out_of_range) {
	    struct ktime_date unpacked;
	    memset(&unpacked, 0, sizeof(unpacked));

	    code = Int32To_ktimeRelDate(got, &unpacked);
	    is_int(code, 0, "  Int32To_ktimeRelDate(%d) returned 0", got);
	    is_int(unpacked.year, tc->input.year, "    year is %d", tc->input.year);
	    is_int(unpacked.month, tc->input.month, "    month is %d", tc->input.month);
	    is_int(unpacked.day, tc->input.day, "    day is %d", tc->input.day);
	}
    }
}

/*
 * Int32To_ktimeRelDate tests
 *
 * The Int32To_ktimeRelDate function is the inverse of ktimeRelDate_ToInt32().
 * It unpacks an encoded relative time integer into a struct ktime_date.
 *
 * The mask field is set by this function. The hour, min, sec are always zero
 * in the output.
 *
 * Currently, this function always returns zero, even when the input value
 * is out of range.
 */
static void
test_Int32To_ktimeRelDate(void)
{
    int tc_i;
    struct {
	afs_int32 input;
	int code;
	int out_of_range;
	struct ktime_date expected;
    } *tc, test_cases[] = {
	/*   input rc oor mask                                  year  mon  day */
	{        0, 0, 0, {0x00,                                  0,   0,   0}},
	{        1, 0, 0, {KTIMEDATE_DAY,                         0,   0,   1}},
	{       30, 0, 0, {KTIMEDATE_DAY,                         0,   0,  30}},
	{       31, 0, 0, {KTIMEDATE_MONTH,                       0,   1,   0}},
	{       32, 0, 0, {KTIMEDATE_MONTH | KTIMEDATE_DAY,       0,   1,   1}},
	{       61, 0, 0, {KTIMEDATE_MONTH | KTIMEDATE_DAY,       0,   1,  30}},
	{       62, 0, 0, {KTIMEDATE_MONTH,                       0,   2,   0}},
	{      341, 0, 0, {KTIMEDATE_MONTH,                       0,  11,   0}},
	{      342, 0, 0, {KTIMEDATE_MONTH | KTIMEDATE_DAY,       0,  11,   1}},
	{      371, 0, 0, {KTIMEDATE_MONTH | KTIMEDATE_DAY,       0,  11,  30}},
	{      372, 0, 0, {KTIMEDATE_YEAR,                        1,   0,   0}},
	{      373, 0, 0, {KTIMEDATE_YEAR | KTIMEDATE_DAY,        1,   0,   1}},
	{      404, 0, 0, {MASK_DATE,                             1,   1,   1}},
	{ 12189324, 0, 0, {KTIMEDATE_YEAR,                    32767,   0,   0}},
	{ 12189695, 0, 0, {MASK_DATE,                         32767,  11,  30}},
	{ 12189696, 0, 1, {KTIMEDATE_YEAR,                   -32768,   0,   0}},
	{-12190068, 0, 1, {KTIMEDATE_YEAR,                    32767,   0,   0}},
	{-12190067, 0, 0, {MASK_DATE,                        -32768, -11, -30}},
	{-12190066, 0, 0, {MASK_DATE,                        -32768, -11, -29}},
	{       -1, 0, 0, {KTIMEDATE_DAY,                         0,   0,  -1}},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	int code;
	struct ktime_date got;

	memset(&got, 0, sizeof(got));
	code = Int32To_ktimeRelDate(tc->input, &got);
	is_int(code, tc->code, "Int32To_ktimeRelDate(%d) returned %d", tc->input, tc->code);
	is_int(got.mask, tc->expected.mask, "  mask is 0x%02x", tc->expected.mask);
	is_int(got.year, tc->expected.year, "  year is %d", tc->expected.year);
	is_int(got.month, tc->expected.month, "  month is %d", tc->expected.month);
	is_int(got.day, tc->expected.day, "  day is %d", tc->expected.day);

	/* Verify the inverse if the input was in range. */
	if (!tc->out_of_range) {
	    struct ktbuf buf;
	    afs_int32 encoded = ktimeRelDate_ToInt32(&got);
	    is_int(encoded, tc->input, "  ktimeRelDate_ToInt32(%s) is %d",
		   format_ktime_date(&got, &buf), tc->input);
	}
    }
}

/*
 * ktimeDate_FromInt32() tests.
 *
 * The ktimeDate_FromInt32() converts an absolute time in seconds into
 * a struct ktime_date.
 *
 * Note: ktimeDate_FromInt32() is not the inverse of Int32To_ktimeRelDate().
 */
static void
test_ktimeDate_FromInt32(void)
{
    int tc_i;
    struct {
	afs_int32 seconds_input;
	int code;
	struct ktime_date expected;
    } *tc, test_cases[] = {
	/* intput    code   mask            year mon  day  hour min  sec */
	{         -1,  0,  {MASK_DATETIME,   69,  12,  31,  18,  59,  59}},
	{          0,  0,  {MASK_DATETIME,   69,  12,  31,  19,   0,   0}},
	{          1,  0,  {MASK_DATETIME,   69,  12,  31,  19,   0,   1}},
	{        100,  0,  {MASK_DATETIME,   69,  12,  31,  19,   1,  40}},
	{  320734799,  0,  {MASK_DATETIME,   80,   2,  29,  23,  59,  59}},
	{  320734800,  0,  {MASK_DATETIME,   80,   3,   1,   0,   0,   0}},
	{  628664400,  0,  {MASK_DATETIME,   89,  12,   3,   0,   0,   0}},
	{  946702740,  0,  {MASK_DATETIME,   99,  12,  31,  23,  59,   0}},
	{  946702799,  0,  {MASK_DATETIME,   99,  12,  31,  23,  59,  59}},
	{  978325200,  0,  {MASK_DATETIME,  101,   1,   1,   0,   0,   0}},
	{ 1713070800,  0,  {MASK_DATETIME,  124,   4,  14,   0,   0,   0}},
	{ 2147483646,  0,  {MASK_DATETIME,  138,   1,  18,  22,  14,   6}},
	{ 2147483647,  0,  {MASK_DATETIME,  138,   1,  18,  22,  14,   7}},
	{-2147483648,  0,  {MASK_DATETIME,    1,  12,  13,  15,  45,  52}},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	int code;
	struct ktime_date got;

	memset(&got, 0, sizeof(got));
	code = ktimeDate_FromInt32(tc->seconds_input, &got);
	is_int(code, tc->code, "ktimeDate_FromInt32(%d [%s]) returned %d",
	       tc->seconds_input, format_timestamp(tc->seconds_input), tc->code);
	if (tc->code == 0) {
	    is_int(got.mask, tc->expected.mask, "  mask is 0x%02x", tc->expected.mask);
	    is_int(got.year, tc->expected.year, "  year is %d", tc->expected.year);
	    is_int(got.month, tc->expected.month, "  month is %d", tc->expected.month);
	    is_int(got.day, tc->expected.day, "  day is %d", tc->expected.day);
	    is_int(got.hour, tc->expected.hour, "  hour is %d", tc->expected.hour);
	    is_int(got.min, tc->expected.min, "  min is %d", tc->expected.min);
	    is_int(got.sec, tc->expected.sec, "  sec is %d", tc->expected.sec);
	}
    }
}

/*
 * ParseRelDate() tests.
 *
 * The ParseRelDate() function parses a string repesentation of a relative
 * date (duration) of years, months, and days into a struct ktime_date.
 */
static void
test_ParseRelDate(void)
{
    int tc_i;
    struct {
	char *input;
	afs_int32 code;
	struct ktime_date expected;
    } *tc, test_cases[] = {
	/* input        code  mask                            year mon day */
	{           "3d", 0, {KTIMEDATE_DAY,                    0,  0,  3}},
	{           "2m", 0, {KTIMEDATE_MONTH,                  0,  2,  0}},
	{        "2m 3d", 0, {KTIMEDATE_MONTH | KTIMEDATE_DAY,  0,  2,  3}},
	{     "1y 2m 3d", 0, {MASK_DATE,                        1,  2,  3}},
	{        "1y 3d", 0, {KTIMEDATE_YEAR | KTIMEDATE_DAY,   1,  0,  3}},
	{        "1y 2m", 0, {KTIMEDATE_YEAR | KTIMEDATE_MONTH, 1,  2,  0}},
	{         "1y3d", 0, {KTIMEDATE_YEAR | KTIMEDATE_DAY,   1,  0,  3}},
	{         "1y2m", 0, {KTIMEDATE_YEAR | KTIMEDATE_MONTH, 1,  2,  0}},
	{       "1y2m3d", 0, {MASK_DATE,                        1,  2,  3}},
	{     "  1y2m3d", 0, {MASK_DATE,                        1,  2,  3}},
	{"9999y 11m 30d", 0, {MASK_DATE,                     9999, 11, 30}},
	{"9999y 12m 31d", 1},
	{             "", 1},
	{          "32d", 1},
	{          "13m", 1},
	{       "10000y", 1},
	{     "1y2m3d  ", 1},
	{        "bogus", 1},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_int32 code;
	struct ktime_date got;

	memset(&got, 0, sizeof(got));
	code = ParseRelDate(tc->input, &got);
	is_int(code, tc->code, "ParseRelDate('%s') returned %d", tc->input, tc->code);
	if (tc->code == 0) {
	    is_int(got.mask, tc->expected.mask, "  mask is 0x%02x", tc->expected.mask);
	    is_int(got.year, tc->expected.year, "  year is %d", tc->expected.year);
	    is_int(got.month, tc->expected.month, "  month is %d", tc->expected.month);
	    is_int(got.day, tc->expected.day, "  day is %d", tc->expected.day);
	    is_int(got.hour, tc->expected.hour, "  hour is %d", tc->expected.hour);
	    is_int(got.min, tc->expected.min, "  min is %d", tc->expected.min);
	    is_int(got.sec, tc->expected.sec, "  sec is %d", tc->expected.sec);
	}
    }
}

/*
 * RelDatetoString() tests.
 *
 * The RelDatetoString() function formats a relative time of years, months, and
 * days stored in a struct ktime_date to a string represention.  This is the
 * inverse of ParseRelDate().
 *
 * RelDatetoString() ignores the hour, min, sec bit masks and fields of
 * the struct ktime_date.
 */
static void
test_RelDatetoString(void)
{
    int tc_i;
    struct {
	struct ktime_date input;
	char *expected;
    } *tc, test_cases[] = {
	/* mask                            year  mon  day  hr min sec   expected */
	{{0x00,                              0,   0,   0,  0,  0,  0},  ""},
	{{KTIMEDATE_YEAR,                    1,   0,   0,  0,  0,  0},  "1y"},
	{{KTIMEDATE_MONTH,                   0,   2,   0,  0,  0,  0},  " 2m"},
	{{KTIMEDATE_YEAR | KTIMEDATE_MONTH,  1,   2,   0,  0,  0,  0},  "1y 2m"},
	{{KTIMEDATE_DAY,                     0,   0,   3,  0,  0,  0},  " 3d"},
	{{KTIMEDATE_YEAR | KTIMEDATE_DAY,    1,   0,   3,  0,  0,  0},  "1y 3d"},
	{{KTIMEDATE_MONTH | KTIMEDATE_DAY,   0,   2,   3,  0,  0,  0},  " 2m 3d"},
	{{MASK_DATE,                         1,   2,   3,  0,  0,  0},  "1y 2m 3d"},
	{{MASK_DATE,                        -1,  -2,  -3,  0,  0,  0},  "-1y -2m -3d"},
	{{KTIMEDATE_HOUR,                    1,   2,   3,  4,  5,  6},  ""},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktbuf buf;
	char *got = RelDatetoString(&tc->input);
	is_string(got, tc->expected, "RelDatetoString(%s) returned '%s'",
		  format_ktime_date(&tc->input, &buf), tc->expected);
    }
}

/*
 * Add_RelDate_to_Time() tests.
 *
 * The Add_RelDate_to_Time() function adds a relative time (duration)
 * of years, months, and days to an absolute time in seconds and returns
 * the result as an absolute time in seconds.
 */
static void
test_Add_RelDate_to_Time(void)
{
    int tc_i;
    struct {
	struct ktime_date duration;
	afs_int32 time;  /* Absolute time in seconds. */
	afs_int32 expected;
    } *tc, test_cases[] = {
	/* mask                            year mon day  hr min sec          time     expected */
	{{0x00,                              0,  0,  0,  0,  0,  0},           0,           0},
	{{KTIMEDATE_YEAR,                    1,  2,  3,  4,  5,  6},           0,    31536000},
	{{KTIMEDATE_MONTH,                   1,  2,  3,  4,  5,  6},           0,     5115599},
	{{KTIMEDATE_YEAR | KTIMEDATE_MONTH,  1,  2,  3,  4,  5,  6},           0,    36651599},
	{{KTIMEDATE_DAY,                     1,  2,  3,  4,  5,  6},           0,      259200},
	{{KTIMEDATE_YEAR | KTIMEDATE_DAY,    1,  2,  3,  4,  5,  6},           0,    31795200},
	{{KTIMEDATE_MONTH | KTIMEDATE_DAY,   1,  2,  3,  4,  5,  6},           0,     5374799},
	{{MASK_DATE,                         1,  2,  3,  4,  5,  6},           0,    36910799},
	{{KTIMEDATE_YEAR,                    1,  2,  3,  4,  5,  6},  1713070800,  1744606800},
	{{KTIMEDATE_MONTH,                   1,  2,  3,  4,  5,  6},  1713070800,  1718341200},
	{{KTIMEDATE_YEAR | KTIMEDATE_MONTH,  1,  2,  3,  4,  5,  6},  1713070800,  1749877200},
	{{KTIMEDATE_DAY,                     1,  2,  3,  4,  5,  6},  1713070800,  1713330000},
	{{KTIMEDATE_YEAR | KTIMEDATE_DAY,    1,  2,  3,  4,  5,  6},  1713070800,  1744866000},
	{{KTIMEDATE_MONTH | KTIMEDATE_DAY,   1,  2,  3,  4,  5,  6},  1713070800,  1718600400},
	{{MASK_DATE,                         1,  2,  3,  4,  5,  6},  1713070800,  1750136400},
	{{KTIMEDATE_YEAR,                    1,  2,  3,  4,  5,  6},  1713070800,  1744606800},
	{{KTIMEDATE_MONTH,                   1,  2,  3,  4,  5,  6},  1713070800,  1718341200},
	{{KTIMEDATE_YEAR | KTIMEDATE_MONTH,  1,  2,  3,  4,  5,  6},  1713070800,  1749877200},
	{{KTIMEDATE_DAY,                     1,  2,  3,  4,  5,  6},  1713070800,  1713330000},
	{{KTIMEDATE_YEAR | KTIMEDATE_DAY,    1,  2,  3,  4,  5,  6},  1713070800,  1744866000},
	{{KTIMEDATE_MONTH | KTIMEDATE_DAY,   1,  2,  3,  4,  5,  6},  1713070800,  1718600400},
	{{MASK_DATE,                         1,  2,  3,  4,  5,  6},  1713070800,  1750136400},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct ktbuf buf;
	afs_int32 got;

	got = Add_RelDate_to_Time(&tc->duration, tc->time);
	is_int(got, tc->expected, "Add_RelDate_to_Time(%s, %d) returned %d [%s]",
	       format_ktime_date(&tc->duration, &buf),
	       tc->time, tc->expected, format_timestamp(tc->expected));
    }
}

int
main(void)
{
    plan(1048);

    /* Test times are in EST timezone. */
    putenv("TZ=EST+5");

    /* Use a fixed system clock time for testing. */
    ktime_SetTestTime(TEST_NOW);

    /* ktime.c tests */
    test_ktime_DateOf();
    test_ktime_Str2int32();
    test_ktime_ParsePeriodic();
    test_ktime_DisplayString();
    test_ktime_next();
    test_ktime_DateToInt32();
    test_ktime_GetDateUsage();
    test_ktime_InterpretDate();

    /* kreltime.c tests */
    test_ktimeRelDate_ToInt32();
    test_Int32To_ktimeRelDate();
    test_ktimeDate_FromInt32();
    test_ParseRelDate();
    test_RelDatetoString();
    test_Add_RelDate_to_Time();

    return 0;
}
