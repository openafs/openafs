/*
 * afsck -- check and fix contents of vcache
 *
 * -t t check only objects of type 't' (File, Symlink, Directory, All)
 * -f flush old data
 * -r refresh old data
 * default is to keep old data
 *
 * Jim Rees, University of Michigan, December 1997
 */

#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <netinet/in.h>
#include "../libafs/afs/lock.h"
#include "afsint.h"
#include "venus.h"
#include "stds.h"
#define KERNEL
#include "afs.h"
#undef KERNEL

#include "discon.h"

extern discon_modes_t get_current_mode();

extern char *optarg;

int nok, nbad, nwrong, nnouse, nnull, nnotlink, nflushed;

main(ac, av)
int ac;
char *av[];
{
    int i, c, type = 's', arg = 'k';
    discon_modes_t mode;

    while ((c = getopt(ac, av, "t:fr")) != EOF) {
	switch (c) {
	case 't':
	    type = optarg[0];
	    break;
	case 'f':
	    arg = 'f';
	    break;
	case 'r':
	    arg = 'r';
	    break;
	default:
	    printf("usage: afsck [-t [fsd]] [-r] [-f]\n");
	    exit(0);
	}
    }

    mode = get_current_mode();

    if ((int) mode < 0) {
	perror("get_current_mode");
	exit(1);
    }
    if (mode != CONNECTED) {
	fprintf(stderr, "not connected\n");
	exit(2);
    }

    for (i = 0; ; i++) {
	if (i % 1000 == 0)
	    printf("%d\n", i);
	if (verify_vcache(i, type, arg) < 0) {
	    if (errno == EINVAL)
		break;
	    else
		perror("verify_vcache");
	}
    }
    printf("%d checked, %d ok, %d bad, %d wrong, %d flushed\n", i, nok, nbad, nwrong, nflushed);
    printf("%d not in use, %d null, %d wrong type\n", nnouse, nnull, nnotlink);
    exit(0);
}

verify_vcache(index, type, arg)
int index, type, arg;
{
    struct ViceIoctl blob;
    dis_setop_info_t info;
    long data = index;
    char msg[MAX_NAME];

    info.op = VERIFY_VCACHE;
    bcopy(&data, info.data, sizeof data);
    info.data[8] = type;
    info.data[9] = arg;

    blob.in_size = sizeof(dis_setop_info_t);
    blob.in = (char *) &info;
    blob.out_size = MAX_NAME;
    blob.out = (char *) msg;

    if (pioctl(0, VIOCSETDOPS, &blob, 1) < 0)
	return -1;

    if (msg[0] != 'n')
	printf("%d: %s\n", index, msg);

    if (!strncmp(msg, "ok", 2))
	nok++;
    else if (!strncmp(msg, "BAD", 3))
	nbad++;
    else if (!strncmp(msg, "WRONG", 5))
	nwrong++;
    else if (!strncmp(msg, "not in use", 10))
	nnouse++;
    else if (!strncmp(msg, "null slot", 9))
	nnull++;
    else if (!strncmp(msg, "flushed", 7))
	nflushed++;
    else
	nnotlink++;
    return 0;
}

/*
****************************************************************************
*Copyright 1993 Regents of The University of Michigan - All Rights Reserved*
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appears in all copies and that  *
* both that copyright notice and this permission notice appear in          *
* supporting documentation, and that the name of The University of Michigan*
* not be used in advertising or publicity pertaining to distribution of    *
* the software without specific, written prior permission. This software   *
* is supplied as is without expressed or implied warranties of any kind.   *
*                                                                          *
*            Center for Information Technology Integration                 *
*                  Information Technology Division                         *
*                     The University of Michigan                           *
*                       535 W. William Street                              *
*                        Ann Arbor, Michigan                               *
*                            313-763-4403                                  *
*                        info@citi.umich.edu                               *
*                                                                          *
****************************************************************************
*/
