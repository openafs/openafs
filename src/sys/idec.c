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


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <afs/afssyscalls.h>
#ifdef AFS_HPUX_ENV
#include <sys/mknod.h>
#endif

main(argc, argv)
     char **argv;
{

    int fd;
    struct stat status;

    if (argc < 3) {
	printf(" Usage is %s <partition> <inode>\n", argv[0]);
	exit(0);
    }

    if (stat(argv[1], &status) == -1) {
	perror("stat");
	exit(1);
    }
    printf("About to idec(dev=(%d), inode=%d)\n", status.st_dev,
	   atoi(argv[2]));
    fflush(stdout);
    fd = IDEC(status.st_dev, atoi(argv[2]), 17);
    if (fd == -1) {
	perror("iopen");
	exit(1);
    }
    exit(0);
}
