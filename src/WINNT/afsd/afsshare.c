/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stdio.h>

main(int argc, char **argv) {
    BOOL res;
    if (argc < 2 || argc > 3) {
	fprintf(stderr, "Incorrect arguments\n");
	exit(1);
    }
    res = WritePrivateProfileString("AFS Submounts",
				    argv[1],
				    (argc == 3) ? argv[2] : NULL,
				    "afsdsbmt.ini");
    if (res == FALSE)
	fprintf(stderr, "Failed, error code %d\n", GetLastError());

    return 0;
}
