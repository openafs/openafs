#include <afs/param.h>
#include <rx/xdr.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/signal.h>
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
#include "afs/afs.h"
#include "discon.h"

/*** BOGUS -- but how else to get the name?? */
#define VCACHEFILE      "/usr/vice/cache/DVCacheItems"

/* also bogus */
#ifndef AFS_FHMAGIC
#define AFS_FHMAGIC 0x7635faba
#endif

void disconnect_cm(long newmode);
extern discon_modes_t get_current_mode(), display_current_mode();
extern char *mode_to_string(discon_modes_t mode);

int vflag;

/* set
** the disconnected variable in the kernel
*/

main(argc,argv)
    int argc;
    char *argv[];
{
    long cmd = -1;
    discon_modes_t mode, newmode;
    int nflag = 0, c;

    while ((c = getopt(argc, argv, "rpfqnvh")) != EOF) {
	switch (c) {
	case 'r' :
	    if (cmd != -1)
		goto idiot;
	    cmd = AFS_DIS_RECON;
	    newmode = CONNECTED;
	    break;
	case 'p' :
	    if (cmd != -1)
		goto idiot;
	    cmd = AFS_DIS_PARTIAL;
	    newmode = PARTIALLY_CONNECTED;
	    break;
	case 'f' :
	    if (cmd != -1)
		goto idiot;
	    cmd = AFS_DIS_FETCHONLY;
	    newmode = FETCH_ONLY;
	    break;
	case 'q' :
	    if (cmd != -1)
		goto idiot;
	    cmd = AFS_DIS_QUERY;
	    break;
	case 'n' :
	    nflag = 1;
	    break;
	case 'v' :
	    vflag = 1;
	    break;
	case 'h' :
	    usage();
	    exit(0);
	default:
	idiot:
	    usage();
	    exit(1);
	}
    }

    if (cmd == -1) {
	cmd = AFS_DIS_DISCON;
	newmode = DISCONNECTED;
    }

    signal(SIGSYS, SIG_IGN);

    /* first see what mode we are in already */
    mode = get_current_mode();

    if ((int) mode < 0) {
	if (errno == ENOSYS) {
	    printf("AFS not running; mucking DVCacheItems\n");
	    flip_the_discon_bit(cmd, newmode);
	    exit(0);
	}
	perror("get_current_mode");
	exit(1);
    }

    if (cmd == AFS_DIS_QUERY) {
	print_current_mode(mode, stdout);
	exit(0);
    }

    if (mode == newmode) {
	printf("Already running in %s mode\n", mode_to_string(mode));
	exit(0);
    }

    /* Make sure we have tokens if we're reconnecting */
    if (cmd == AFS_DIS_RECON && !tokencheck(1))
	exit(2);

    if (nflag) {
	if (cmd == AFS_DIS_RECON)
	    cmd = AFS_DIS_RECONTOSS;
	else if (cmd == AFS_DIS_DISCON)
	    cmd = AFS_DIS_DISCONTOSS;
    }

    if (cmd == AFS_DIS_RECONTOSS) {
	fprintf(stderr, "This will discard your replay log");
	if (!askuser())
	    exit(3);
    }

    if (vflag)
	printf("Changing from %s to %s mode\n", mode_to_string(mode), mode_to_string(newmode));

    change_mode(cmd, newmode);

    exit(0);
}

flip_the_discon_bit(cmd, mode)
long cmd;
discon_modes_t mode;
{
    static char fname[] = VCACHEFILE;
    FILE *f;
    struct afs_dheader dh;

    f = fopen(fname, "r+");
    if (f == NULL) {
	fprintf(stderr, "can't open %s\n", fname);
	return -1;
    }
    if (fread(&dh, sizeof dh, 1, f) != 1) {
	fprintf(stderr, "can't read %s\n", fname);
	fclose(f);
	return -1;
    }
    if (dh.magic != AFS_FHMAGIC) {
	fprintf(stderr, "bad magic %x != %x\n", dh.magic, AFS_DHMAGIC);
	fclose(f);
	return -1;
    }
    if (cmd == AFS_DIS_QUERY) {
	print_current_mode(dh.mode, stdout);
    } else {
	dh.mode = mode;
	rewind(f);
	fwrite(&dh, sizeof dh, 1, f);
    }
    fclose(f);
    return 0;
}

usage()
{
    fputs("usage: discon [ -r | -p | -f | -q | -h ] [ -n ]\n", stderr);
    fputs("  -r reconnect\n", stderr);
    fputs("  -f fetch only\n", stderr);
    fputs("  -p partially connected\n", stderr);
    fputs("  -q query only\n", stderr);
    fputs("  -n don't discard callbacks (discon) or toss replay log (recon)\n", stderr);
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
