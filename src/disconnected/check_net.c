#ifdef __OpenBSD__
#define __NetBSD__
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#ifndef __NetBSD__
#include <sys/inode.h>
#endif
#include <netinet/in.h>
/*
*/
#include <afs/vice.h>
#include <afs/venus.h>
#include <afs/stds.h>
#undef VIRTUE
#undef VICE
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <rx/rxkad.h>
#include <afs/vldbint.h>
#include <afs/volser.h>
#include <afs/volser.h>
#include <afs/vlserver.h>
#include <afs/cmd.h>
#include <strings.h>
#include <pwd.h>

#ifdef __NetBSD__
#include "../libafs/afs/lock.h"
#else
#include "lock.h"
#endif
#include "afsint.h" 

#define KERNEL
#include "afs.h"
#undef KERNEL
#include "discon.h"
#include "discon_log.h"
#include "playlog.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01

long root_server(int);

#define	DEFAULT_INTERVAL 5
#define REALLY_BIG 1024

extern char    *optarg;


int
network_up(verbose)
	int verbose;
{
	int code = 1;
	long host;

	host = 	root_server(verbose);
	code = ping_server(host);

	if (verbose)
		printf("network: %s; ", code ? "up" : "down");

	return code;
}



#define MAXSIZE 100
char space[MAXSIZE];

long
root_server(verbose)
	int verbose;
{
	long code;
	struct ViceIoctl blob;
	long *hosts;

	blob.out_size = MAXSIZE;
	blob.in_size = 0;
	blob.out = space;
	bzero(space, sizeof(space));
	code = pioctl("/afs", VIOCWHEREIS, &blob, 1);
	if (code) {
		printf("error %d \n", code);
	}
	hosts = (long *) space;

	return hosts[0];
}


int
ping_server(server)
	long server;
{
	long code;
	struct ViceIoctl blob;
	dis_setop_info_t info;
	
	long *hosts;

	info.op = PING_SERVER;
	 *((long *) info.data) = server;

	blob.in_size = (short) sizeof(dis_setop_info_t);
	blob.in = (char *) &info;
	blob.out_size = MAXSIZE;
	blob.out = space;

	bzero(space, sizeof(space));

	code = pioctl(0, VIOCSETDOPS, &blob, 1);

	if (code) {
		printf("error %d \n", code);
		return 0;
	} else
		return 1;
}


/*
 * if $USER has no tokens, prompt for exit.
 *
 * TODO: take a more fine-grained look at the tokens,
 * e.g., are they the right ones for the log, are they
 * still valid, etc.
 */




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
