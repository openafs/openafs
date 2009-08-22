/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "AFS_component_version_number.c"

/* prototypes */
int mc_copy(FILE *, FILE *, char **);

int
main(int argc, char **argv)
{
    register FILE *infile;
    register FILE *outfile;
    char *alist[5];
    register int code;
    char *sysname;

    if (argc != 4) {
	printf
	    ("config: usage is 'config <from file> <to file> <system name>'\n");
	exit(1);
    }
    infile = fopen(argv[1], "r");
    if (!infile) {
	printf("config: input file %s not found.\n", argv[1]);
	exit(1);
    }
    outfile = fopen(argv[2], "w+");
    if (!outfile) {
	printf("config: output file %s not found.\n", argv[2]);
	exit(1);
    }
    memset (alist, 0, sizeof (alist));
    alist[0] = argv[3];
    alist[1] = "all";

    /* This allows JUST arch or JUST OS/version,
     * Linux 2.6 uses the in-kernel build system, so 
     * just 'linux26' is enough. */
    sysname = strdup (alist[0]);
    alist[2] = strchr (sysname, '_');
    if (alist[2]) {
        alist[3] = sysname;
        *alist[2] = 0;
        alist[2]++;
    }
    code = mc_copy(infile, outfile, alist);
    if (code) {
	printf("config: failed to correctly write makefile '%s', code %d\n",
	       argv[2], code);
	exit(1);
    } else {
	printf("Wrote new makefile '%s'.\n", argv[2]);
	exit(0);
    }
}
