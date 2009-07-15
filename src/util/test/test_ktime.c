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


#include "ktime.h"

static struct testTime {
    char *time;
    long code;
    long sec;
} testTimes[] = {
    "now", 1 /* lookup current time */ , 0,
	"never", 0, 0xffffffff, "12/3/89", 0, 628664400, "1/1/1", 0,
	978325200, "1/0/80", -2, 0, "13/1/80", -2, 0, "0/1/80", -2, 0,
	"1/32/80", -2, 0, "2/30/80", 0, 320734799,
	/* Oh, well: note that 2/30 is bigger than any real date in February, and
	 * since this algorithm approaches the correct value from below, this is
	 * the closest it can come. */
	"3/1/80", 0, 320734800, "3/1/80 0:00", 0, 320734800, "2/30/80 24:00", -2, 0, "2/30/80 23:60", -2, 0, "22/22/22", -2, 0, "12/31/69 19:07", 0, 420, "12/31/99 23:59", 0, 946702740, "12/31/99 23:59:59", 0, 946702740,	/* ignores seconds */
	"23:12", -1, 0, "22/12", -1, 0, "22/22/22 12", -1, 0, "12/31/199 23:59:59", -1, 0, "12/31/1888", -1, 0, "-13/-44/22 -15:77", -1, 0, "4/14/24", 0, 1713067200, "4/14/2024", 0, 1713067200, "4/14/68", 0, 0x7fffffff,	/* sadly, legal but w/ sign bit on */
"4/14/69", 0, 0, 0, 0};

main(argc, argv)
     int argc;
     char *argv[];
{
    long code, temp;
    int errors;
    time_t t;
    struct testTime *tt;

    /* should do timezone and daylight savings time correction so this program
     * work in other than EST */

    if (argc > 1) {
	code = util_GetLong(argv[1], &temp);
	t = temp;
	if (code) {		/* assume its a date string */
	    code = ktime_DateToLong(argv[1], &temp);
	    t = temp;
	    printf("The string %s gives %d; ctime yields %s", argv[1], temp,
		   ctime(&t));
	} else {
	    printf("The value %d is %s", temp, ctime(&t));
	}
	exit(0);
    }
    errors = 0;
    for (tt = testTimes; tt->time; tt++) {
	code = ktime_DateToLong(tt->time, &t);
	if (code) {
	    if (tt->code < 0)
		continue;	/* got error as we should have */
	    printf("ktime_DateToLong returned %d given '%s'\n", code,
		   tt->time);
	    errors++;
	    continue;
	} else if (tt->code < 0) {
	    printf("ktime_DateToLong didn't failed on %s, instead got %d\n",
		   tt->time, t);
	    errors++;
	    continue;
	}
	if (tt->code == 1) {
	    if (time(0) - t <= 1)
		continue;
	} else if (t == tt->sec)
	    continue;
	printf("Error converting %s to long: got %d should be %d\n", tt->time,
	       t, tt->sec);
	errors++;
	continue;
    }
    exit(errors);
}
