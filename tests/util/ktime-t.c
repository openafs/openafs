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

#include <afs/opr.h>
#include <afs/ktime.h>
#include <afs/afsutil.h>
#include <afs/afsutil_prototypes.h>
#include <tests/tap/basic.h>
#include "common.h"

/* Arbitrary fixed time for testing. */
static const time_t TEST_NOW = 1743998400L;  /* Sun Apr  6 23:00:00 2025 */

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

int
main(void)
{
    plan(54);

    /* Test times are in EST timezone. */
    putenv("TZ=EST+5");

    /* Use a fixed system clock time for testing. */
    ktime_SetTestTime(TEST_NOW);

    /* ktime.c tests */
    test_ktime_DateToInt32();

    return 0;
}
