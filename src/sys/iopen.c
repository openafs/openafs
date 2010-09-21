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
#include <stdlib.h>
#include "afssyscalls.h"
#ifdef AFS_HPUX_ENV
#include <sys/mknod.h>
#endif

#include "AFS_component_version_number.c"

void
Usage(void)
{
    printf("Usage: iopen <partition> <inode>\n");
    printf
	("iopen opens file by inode, then tries to read it, printing it to stdout.\n");
    exit(1);
}

main(argc, argv)
     char **argv;
{
    char *part;
    char buf[5];
    int fd, n;
    struct stat status;
    Inode ino;

    if (argc != 3)
	Usage();

    part = argv[1];
#ifdef AFS_64BIT_IOPS_ENV
    ino = strtoull(argv[2], NULL, 10);
#else
    ino = atoi(argv[2]);
#endif

    if (stat(part, &status) == -1) {
	perror("stat");
	exit(1);
    }
    printf("ino=%" AFS_INT64_FMT "\n", ino);
    printf("About to iopen(dev=(%d,%d), inode=%s, mode=%d\n",
	   major(status.st_dev), minor(status.st_dev), PrintInode(NULL, ino),
	   O_RDONLY);
    fflush(stdout);
    fd = IOPEN(status.st_dev, ino, O_RDONLY);
    if (fd == -1) {
	perror("iopen");
	exit(1);
    }
    printf("iopen successful, fd=%d\n", fd);
    while ((n = read(fd, buf, 5)) > 0)
	write(1, buf, n);
    if (n < 0)
	perror("read");
    exit(0);
}
