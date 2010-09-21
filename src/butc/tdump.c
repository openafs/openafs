/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h
#include <sys/file.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#define BLKSIZE (4096+24)	/* actual block size on our backup tapes */

afs_int32
glong(cp, index)
     int index;
     char *cp;
{
    afs_int32 temp;
    memcpy(&temp, cp + index * 4, sizeof(afs_int32));
    return temp;
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    char tbuffer[10000];
    int fd;
    afs_int32 code;
    char *lp;
    afs_int32 count;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    fd = open(argv[1], O_RDONLY, 0);
    if (fd < 0) {
	perror("tape open");
	exit(1);
    }
    for (count = 0;; count++) {
	code = read(fd, tbuffer, BLKSIZE);
	if (code == 0)
	    printf("***EOF***\n");
	else if (code != BLKSIZE) {
	    printf("failed to read correct number of bytes, read %d\n", code);
	    if (code < 0)
		perror("read");
	    exit(1);
	} else {
	    printf("Block %d is:\n", count);
	    lp = tbuffer;
	    printf("%08x %08x %08x %08x %08x %08x %08x %08x\n", glong(lp, 0),
		   glong(lp, 1), glong(lp, 2), glong(lp, 3), glong(lp, 4),
		   glong(lp, 5), glong(lp, 6), glong(lp, 7));
	    printf("%08x %08x %08x %08x %08x %08x %08x %08x\n", glong(lp, 8),
		   glong(lp, 9), glong(lp, 10), glong(lp, 11), glong(lp, 12),
		   glong(lp, 13), glong(lp, 14), glong(lp, 15));
	}
    }
}
