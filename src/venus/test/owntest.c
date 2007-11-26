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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

extern int errno;

main(argc, argv)
     int argc;
     char **argv;
{
    struct timeval tv[2];
    struct stat tstat;
    register long code;
    register char *pn;		/* path name we're dealing with */

    if (argc != 2) {
	printf
	    ("usage: owntest <file owned by somoneelse, but still writable>\n");
	exit(1);
    }

    pn = argv[1];
    printf("Starting tests on %s.\n", pn);
    code = chmod(pn, 0444);
    if (code < 0) {
	perror("chmod to RO");
	exit(errno);
    }
    code = chmod(pn, 0666);
    if (code < 0) {
	perror("chmod back to RW");
	exit(errno);
    }
    gettimeofday(&tv[0], NULL);
    gettimeofday(&tv[1], NULL);
    tv[0].tv_sec -= 10000;
    tv[0].tv_usec = 0;
    tv[1].tv_sec -= 20000;
    tv[1].tv_usec = 0;
    code = utimes(pn, tv);
    if (code < 0) {
	perror("utimes");
	exit(errno);
    }
    code = stat(pn, &tstat);
    if (code < 0) {
	perror("stat");
	exit(errno);
    }
    if (tstat.st_mtime != tv[1].tv_sec) {
	printf("modtime didn't stick\n");
	exit(1);
    }
    printf("Done.\n");
    exit(0);
}
