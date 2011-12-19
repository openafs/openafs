/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#include <afsconfig.h>
#include <sys/param.h>


#include <stdio.h>
#include <sys/time.h>
#include <sys/file.h>
#include <errno.h>
#include "kautils.h"

static char *usage = "usage: %s [-n] [-u] [yymmddhhmm[.ss]]\n";
main(argc, argv)
     int argc;
     char *argv[];
{
    char *progname = argv[0];
    long time;
    char bob[30];

    if (argc > 2) {
	fprintf(stderr, usage, progname);
	exit(1);
    }

    if (ktime_DateToLong(argv[1], &time)) {
	fprintf(stderr, usage, progname);
	exit(1);
    } else {
	ka_timestr(time, bob, KA_TIMESTR_LEN);
	printf("time is %s\n", bob);
    }

    exit(0);
}
