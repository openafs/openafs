/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef lint
#endif
/* ReallyAbort:  called from assert. May/85 */
#include <afs/param.h>
#include <stdio.h>

#ifdef AFS_NT40_ENV
void afs_NTAbort(void)
{
    _asm int 3h; /* always trap. */
}
#endif


void AssertionFailed(char *file, int line)
{
    fprintf(stderr, "Assertion failed! file %s, line %d.\n", file, line);
    fflush(stderr);
#ifdef AFS_NT40_ENV
    afs_NTAbort();
#else
    abort();
#endif
}

