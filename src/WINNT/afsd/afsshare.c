/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
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
