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
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <string.h>


#include <stdio.h>
#include "afsutil.h"

#ifdef AFS_NT40_ENV
void
afs_NTAbort(void)
{
    DebugBreak();
}
#endif

#define TIMESTAMP_BUFFER_SIZE 26  /* including the null */
#define TIMESTAMP_NEWLINE_POS 24  /* offset to the newline placed by ctime */

void
AssertionFailed(char *file, int line)
{
    char tdate[TIMESTAMP_BUFFER_SIZE];
    time_t when;

    time(&when);
    (void)afs_ctime(&when, tdate, sizeof(tdate));
    tdate[TIMESTAMP_NEWLINE_POS] = ' ';
    fprintf(stderr, "%sAssertion failed! file %s, line %d.\n", tdate, file,
	    line);
    fflush(stderr);
#ifdef AFS_NT40_ENV
    afs_NTAbort();
#else
    abort();
#endif
}
