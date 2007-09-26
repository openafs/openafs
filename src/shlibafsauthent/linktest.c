/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

#include <stdio.h>
#include <afsconfig.h>
#include <afs/stds.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/ptclient.h>

	/* just enough smarts to exercise library linkage a bit. */

int
main(int ac, char **av)
{
    int code;

    code = pr_Initialize(1, AFSDIR_CLIENT_ETC_DIRPATH, 0);
    if (code) {
	afs_com_err("linktest", code, "can't talk to prot service.");
	exit(1);
    }
    exit(0);
}
