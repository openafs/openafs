/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <afs/afs_args.h>

#include "AFS_component_version_number.c"

main (argc, argv)
    int argc;
    char **argv; {
    afs_int32 code;
    int i, counter;
    afs_int32 parms[6];
    int numberFlag;

    if (argc < 2) {printf("use: vsys <call number> <parms>\n"); exit(1); }
    numberFlag = 1;
    counter = 0;
    for(i=1;i<argc;i++) {
	if (numberFlag && argv[i][0] == '-') {
	    if (strcmp(argv[i], "-s")==0) numberFlag = 0;
	    else {
		printf("bad switch %s\n", argv[i]);
		exit(1);
	    }
	}
	else if (numberFlag) {
	    parms[counter++] = atoi(argv[i]);
	    numberFlag = 1;
	}
	else {
	    parms[counter++] = (afs_int32) argv[i];
	    numberFlag = 1;
	}
    }
    code = syscall(AFS_SYSCALL, parms[0], parms[1], parms[2], parms[3], parms[4], parms[5]);
    printf("code %d\n", code);
}
