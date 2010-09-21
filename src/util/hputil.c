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


#ifdef AFS_HPUX_ENV
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>

/* insque/remque moved to timer.c where they are used. */

#ifndef AFS_HPUX102_ENV
int
utimes(char *file, struct timeval tvp[2])
{
    struct utimbuf times;

    times.actime = tvp[0].tv_sec;
    times.modtime = tvp[1].tv_sec;
    return (utime(file, &times));
}
#endif

int
random(void)
{
    return rand();
}

void
srandom(int seed)
{
    srand(seed);
}

int
getdtablesize(void)
{
    return (20);
}

void
setlinebuf(FILE * file)
{
    setbuf(file, NULL);
}

void
psignal(unsigned int sig, char *s)
{
    fprintf(stderr, "%s: signal %d\n", s, sig);
}
#endif /* AFS_HPUX_ENV */
