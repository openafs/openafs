/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* ReallyAbort:  called from assert. May/85 */
#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <afs/opr.h>

#include "afsutil.h"

#define TIMESTAMP_BUFFER_SIZE 26  /* including the null */

void
AssertionFailed(char *file, int line)
{
    char tdate[TIMESTAMP_BUFFER_SIZE];
    time_t when;
    struct tm tm;

    when = time(NULL);
    strftime(tdate, sizeof(tdate), "%a %b %d %H:%M:%S %Y",
	     localtime_r(&when, &tm));
    fprintf(stderr, "%s Assertion failed! file %s, line %d.\n", tdate, file,
	    line);
    fflush(stderr);
    opr_abort();
}
