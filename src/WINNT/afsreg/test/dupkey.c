/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Test of RegDupKeyAlt() */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <WINNT/afsreg.h>

int main(int argc, char *argv[])
{
    long status;

    if (argc != 3) {
	printf("Usage: %s key1 key2\n", argv[0]);
	return 1;
    }

    status = RegDupKeyAlt(argv[1], argv[2]);

    if (status == ERROR_SUCCESS) {
	printf("Registry key duplication succeeded\n");
    } else {
	printf("Registry key duplication failed (%d)\n", status);
    }
}
