/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* ltlist - a standalone program to dump the link count table. */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <windows.h>
#include <io.h>
#endif

main(int ac, char **av)
{
    FILE *fp;
    unsigned short row;
    int i;
    int count;
    int stamp[2];

    if (ac != 2) {
	printf("Usage ltlist <filename>\n");
	exit(1);
    }

    fp = fopen(av[1], "r");
    if (!fp) {
	printf("Can't open %s for reading.\n");
	exit(1);
    }

    /* Print the magic and version numbers in hex. */
    count = fread((void *)stamp, 1, 8, fp);
    if (count != 8) {
	if (feof(fp)) {
	    printf("Only read %d bytes of %s, wanted 8 for stamp.\n", count,
		   av[1]);
	} else {
#ifdef AFS_NT40_ENV
	    printf("NT Error %d reading 8 bytes from %s\n", GetLastError(),
		   av[1]);
#else
	    perror("fread");
#endif
	}
	exit(1);
    }

    printf("magic=0x%x, version=0x%x\n", stamp[0], stamp[1]);

    printf("%10s %2s %2s %2s %2s %2s\n", "Vnode", "F1", "F2", "F3", "F4",
	   "F5");
    i = 0;
    while (fread((void *)&row, 1, 2, fp)) {
	printf("%10d %2d %2d %2d %2d %2d\n", i, (int)(row & 0x7),
	       (int)((row >> 3) & 0x7), (int)((row >> 6) & 0x7),
	       (int)((row >> 9) & 0x7), (int)((row >> 12) & 0x7));
	i++;
    }
}
