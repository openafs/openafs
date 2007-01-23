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
    char **alist, **ap, *cp;
    register int code;
    int i;
    char *sysname;

    if (argc < 4) {
	fprintf (stderr,
"config: usage is 'config <from file> <to file> <system name> [<more things>]'\n");
	exit(1);
    }
    alist = calloc(2*(argc+5), sizeof *alist);
    if (!alist) {
	perror("config: calloc failed");
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
    ap = alist;
    *ap++ = "all";
    /* copy in extra keywords, this allows arbitrary variant sections, 
       alongside sysname  */
    for (i = 3; argv[i]; ++i)
    {
	*ap++ = argv[i];
	/* This allows JUST arch or JUST OS/version,
	 * Linux 2.6 uses the in-kernel build system, so 
	 * just 'linux26' is enough. */
	if (cp = strchr(argv[i], '_'))
	    *ap++ = (cp+1);
    }
    *ap = 0;
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
