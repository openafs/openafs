/* Copyright (C) 1999 Transarc Corporation - All rights reserved.
 *
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
