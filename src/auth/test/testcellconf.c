/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
testcellconfig.c:

    Test of the routines used by the FileServer to manipulate the cell/server database and
    determine the local cell name:
        1) Reading in the local cell name from file.
        2) Reading in the cell/server database from disk.
        3) Reporting the set of servers associated with a given cell name.
        4) Printing out the contents of the cell/server database.
        5) Reclaiming the space used by an in-memory database.

Creation date:
    17 August 1987

--------------------------------------------------------------------------------------------------------------*/
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <afs/afsutil.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <afs/cellconfig.h>

int 
PrintOneCell(struct afsconf_cell *ainfo, void *arock, struct afsconf_dir *adir)
{
    register int i;
    long temp;

    printf("Cell %s:\n", ainfo->name);
    for (i = 0; i < ainfo->numServers; i++) {
	memcpy(&temp, &ainfo->hostAddr[i].sin_addr, sizeof(long));
	printf("    host %s at %x.%x\n", ainfo->hostName[i], temp,
	       ainfo->hostAddr[i].sin_port);
    }
    return 0;
}

/*Main for testcellconfig*/
main(argc, argv)
     int argc;
     char *argv[];
{
    struct afsconf_dir *theDir;
    char tbuffer[1024];
    struct afsconf_cell theCell;
    long i;
    register long code;
    char *dirName;

    if (argc < 2) {
	printf
	    ("usage: testcellconfig <conf-dir-name> [<cell-to-display>]*\n");
	exit(1);
    }

    dirName = argv[1];
    theDir = afsconf_Open(dirName);
    if (!theDir) {
	printf("could not open configuration files in '%s'\n", dirName);
	exit(1);
    }

    /* get the cell */
    code = afsconf_GetLocalCell(theDir, tbuffer, sizeof(tbuffer));
    if (code != 0) {
	printf("get local cell failed, code %d\n", code);
	exit(1);
    }
    printf("Local cell is '%s'\n\n", tbuffer);

    if (argc == 2) {
	printf("About to print cell database contents:\n");
	afsconf_CellApply(theDir, PrintOneCell, 0);
	printf("Done.\n\n");
	/* do this junk once */
	printf("start of special test\n");
	code = afsconf_GetCellInfo(theDir, NULL, "afsprot", &theCell);
	if (code)
	    printf("failed to find afsprot service (%d)\n", code);
	else {
	    printf("AFSPROT service:\n");
	    PrintOneCell(&theCell, NULL, theDir);
	}
	code = afsconf_GetCellInfo(theDir, 0, "bozotheclown", &theCell);
	if (code == 0)
	    printf("unexpectedly found service 'bozotheclown'\n");
	code = afsconf_GetCellInfo(theDir, NULL, "telnet", &theCell);
	printf("Here's the telnet service:\n");
	PrintOneCell(&theCell, NULL, theDir);
	printf("done with special test\n");
    } else {
	/* now print out specified cell info */
	for (i = 2; i < argc; i++) {
	    code = afsconf_GetCellInfo(theDir, argv[i], 0, &theCell);
	    if (code) {
		printf("Could not find info for cell '%s', code %d\n",
		       argv[i], code);
	    } else
		PrintOneCell(&theCell, NULL, theDir);
	}
    }

    /* all done */
    exit(0);
}
