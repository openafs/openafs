#include <afs/param.h>
#include <rx/xdr.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <afs/stds.h>
#include <afs/vice.h>
#include <afs/venus.h>
#undef VIRTUE
#undef VICE
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <ubik.h>
#include <rx/rxkad.h>
#include <afs/vldbint.h>
#include <afs/volser.h>
#include <afs/volser.h>
#include <afs/vlserver.h>
#include <afs/cmd.h>
#include <strings.h>
#include <pwd.h>
#include "discon.h"

#define MAX_IOCTL_SIZE	2048

/*
 * if $USER has no tokens, prompt for exit.
 *
 * TODO: take a more fine-grained look at the tokens,
 * e.g., are they the right ones for the log, are they
 * still valid, etc.
 */

int
tokencheck(int askme)
{
    int code;
    int index, newindex, numtokens=0;
    struct ktc_principal service;
    char home_cell[MAX_IOCTL_SIZE];
    char *home_user;
    struct ViceIoctl blob;
    struct passwd *pwd;

    /* Find the user's home cell */
    if (!(home_user = (char *) getenv("USER")) || !(pwd = getpwnam(home_user))) {
	if (askme) {
	    fprintf(stderr, "Don't know who you are");
	    return askuser();
	} else
	    return 0;
    }

    /* find the home cell name */
    blob.in_size = 0;                    
    blob.out_size = MAX_IOCTL_SIZE;
    blob.out = home_cell;
    code = pioctl(pwd->pw_dir, VIOC_FILE_CELL_NAME, &blob, 1);

    if (code) {
	if (askme) {
	    fprintf(stderr,"Can't determine your home cell, code %d",code);
	    return askuser();
	} else
	    return 0;
    }

    /* see if there is a token for the home cell */

    for (index = 0; ; index = newindex) {
	code = ktc_ListTokens(index, &newindex, &service);
	if (code)
	    break;
	if (!strcmp(service.cell, home_cell))
	    return 1; 
	numtokens++;
    }

    if (!askme)
	return 0;

    fprintf(stderr, "You have no afs tokens");
    if (numtokens) 
	fprintf(stderr, " for your home cell");

    return askuser();
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
