/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* file		whatfid.c */


#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <errno.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <afs/stds.h>
#include <afs/vice.h>
#include <afs/venus.h>
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <strings.h>


struct VenusFid {
    afs_int32 Cell;
    struct AFSFid Fid;
};


char *pn;
void PioctlError();

#include "AFS_component_version_number.c"

int WhatFidCmd_FileParm;
int WhatFidCmd_FollowLinkParm;
int
WhatFidCmd(as)
     register struct cmd_syndesc *as;
{
    register afs_int32 code;
    struct ViceIoctl blob;
    struct VenusFid vFid;
    register struct cmd_item *ti;
    struct VolumeStatus *status;
    char *name;
    int follow = 1;

    if (as->parms[1].items)
	follow = 0;
    for (ti = as->parms[0].items; ti; ti = ti->next) {
	/* once per file */
	blob.out_size = sizeof(struct VenusFid);
	blob.in_size = 0;
	blob.out = (char *)&vFid;
	code = pioctl(ti->data, VIOCGETFID, &blob, follow);
	if (code) {
	    PioctlError(code, ti->data);
	    continue;
	}
	printf("%s: %x:%d.%d.%d\n", ti->data, vFid.Cell, vFid.Fid.Volume,
	       vFid.Fid.Vnode, vFid.Fid.Unique);
    }
    return 0;
}



main(argc, argv)
     int argc;
     char **argv;
{
    register afs_int32 code;
    register struct cmd_syndesc *ts;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif

    pn = argv[0];

    ts = cmd_CreateSyntax("initcmd", WhatFidCmd, 0, "list fid for file(s)");
    WhatFidCmd_FileParm = cmd_AddParm(ts, "-path", CMD_LIST, 0, "pathnames");
    WhatFidCmd_FollowLinkParm =
	cmd_AddParm(ts, "-link", CMD_FLAG, CMD_OPTIONAL,
		    "do not follow symlinks");

    exit(cmd_Dispatch(argc, argv));
}

void
PioctlError(code, filename)
     int code;
     char *filename;
{				/*Die */

    if (errno == EINVAL) {
	if (filename)
	    fprintf(stderr,
		    "%s: Invalid argument; it is possible that %s is not in AFS.\n",
		    pn, filename);
	else
	    fprintf(stderr, "%s: Invalid argument.\n", pn);
    } else if (errno == ENOENT) {
	if (filename)
	    fprintf(stderr, "%s: File '%s' doesn't exist\n", pn, filename);
	else
	    fprintf(stderr, "%s: no such file returned\n", pn);
    } else if (errno == EROFS)
	fprintf(stderr,
		"%s: You can not change a backup or readonly volume\n", pn);
    else if (errno == EACCES || errno == EPERM) {
	if (filename)
	    fprintf(stderr,
		    "%s: You don't have the required access rights on '%s'\n",
		    pn, filename);
	else
	    fprintf(stderr,
		    "%s: You do not have the required rights to do this operation\n",
		    pn);
    } else {
	if (filename)
	    fprintf(stderr, "%s:'%s'", pn, filename);
	else
	    fprintf(stderr, "%s", pn);
	fprintf(stderr, ": %s\n", error_message(errno));
    }
}				/*Die */
