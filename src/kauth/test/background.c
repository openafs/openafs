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

RCSID
    ("$Header: /cvs/openafs/src/kauth/test/background.c,v 1.5 2003/07/15 23:15:17 shadow Exp $");

main(argc, argv)
     int argc;
     char *argv[];
{
    int pid;

    if (argc == 1)
	exit(-1);
    if (pid = fork()) {		/* parent */
	printf("%d", pid);
	exit(0);
    }
    execve(argv[1], argv + 1, 0);
    perror("execve returned");
}
