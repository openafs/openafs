/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <sys/types.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

#include "AFS_component_version_number.c"

/* prototypes */
int mc_copy(FILE *, FILE *, char **);

int main(int argc, char **argv) {
    register FILE *infile;
    register FILE *outfile;
    char *alist[3];
    register int code;

    if (argc != 4) {
	printf("config: usage is 'config <from file> <to file> <system name>'\n");
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
    alist[0] = argv[3];
    alist[1] = "all";
    alist[2] = NULL;
    code = mc_copy(infile, outfile, alist);
    if (code) {
	printf("config: failed to correctly write makefile '%s', code %d\n",
		argv[2], code);
	exit(1);
    }
    else {
	printf("Wrote new makefile '%s'.\n", argv[2]);
	exit(0);
    }
}
