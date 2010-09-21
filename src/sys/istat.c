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

#include "AFS_component_version_number.c"

main(argc, argv)
     char **argv;
{

    int fd;
    int ino;
    struct stat status;

    if (stat(argv[1], &status) == -1) {
	perror("stat");
	exit(1);
    }
    ino = atoi(argv[2]);
    fd = xiopen(status.st_dev, ino, O_RDONLY);
    if (fd == -1) {
	perror("iopen");
	exit(1);
    }
    if (fstat(fd, &status) == -1) {
	perror("fstat");
	exit(1);
    }
    printf("links=%d, size=%d\n", status.st_nlink, status.st_size);
    exit(0);
}
