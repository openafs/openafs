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


/* This is the salvage test program. */

main(argc, argv)
     int argc;
     char **argv;
{
    long ofid, nfid, code, myFid[3], parentFid[3];
    DInit(20);
    if (argc == 2) {
	ofid = atoi(argv[1]);
	nfid = 0;
    } else if (argc == 3) {
	ofid = atoi(argv[1]);
	nfid = atoi(argv[2]);
    } else {
	printf("usage is: test <ofid> <optional new fid>\n");
	exit(1);
    }
    code = DirOK(&ofid);
    printf("DirOK returned %d.\n");
    if (nfid) {
	printf("Salvaging from fid %d into fid %d.\n", ofid, nfid);
	if (Lookup(&ofid, ".", myFid) || Lookup(&ofid, "..", parentFid)) {
	    printf("Lookup of \".\" and/or \"..\" failed: ");
	    printf("%d %d %d %d\n", myFid[1], myFid[2], parentFid[1],
		   parentFid[2]);
	    printf("Directory cannot be salvaged\n");
	} else {
	    code =
		DirSalvage(&ofid, &nfid, myFid[1], myFid[2], parentFid[1],
			   parentFid[2]);
	    printf("DirSalvage returned %d.\n", code);
	}
    }
    DFlush();
}

Log(a, b, c, d, e, f, g, h, i, j, k, l, m, n)
{
    printf(a, b, c, d, e, f, g, h, i, j, k, l, m, n);
}

/* the end */
