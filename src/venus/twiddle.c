/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

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
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#undef VIRTUE
#undef VICE
#include "afs/prs_fs.h"
#include <afs/afsint.h>
#include <errno.h
#include <afs/cellconfig.h>
#include <afs/cmd.h>
#include <strings.h>
#include <afs/com_err.h>

#define	MAXSIZE	2048
#define MAXINSIZE 1300		/* pioctl complains if data is larger than this */
#define VMSGSIZE 128		/* size of msg buf in volume hdr */

static char space[MAXSIZE];
static char tspace[1024];
static struct ubik_client *uclient;


extern struct cmd_syndesc *cmd_CreateSyntax();
static char pn[] = "fs";
static int rxInitDone = 0;

static
Twiddle(as)
     struct cmd_syndesc *as;
{
    afs_int32 code;
    struct ViceIoctl blob;
    struct cmd_item *ti;
    struct rxparams rxp;
    int tmp;

    ti = as->parms[0].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rx_initReceiveWindow = tmp;
    ti = as->parms[1].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rx_maxReceiveWindow = tmp;
    ti = as->parms[2].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rx_initSendWindow = tmp;
    ti = as->parms[3].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rx_maxSendWindow = tmp;
    ti = as->parms[4].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rxi_nSendFrags = tmp;
    ti = as->parms[5].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rxi_nRecvFrags = tmp;
    ti = as->parms[6].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rxi_OrphanFragSize = tmp;
    ti = as->parms[7].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rx_maxReceiveSize = tmp;
    ti = as->parms[8].items;
    if (ti && ti->data) {
	tmp = atoi(ti->data);
    } else
	tmp = 0;
    rxp.rx_MyMaxSendSize = tmp;

    blob.in = (char *)&rxp;
    blob.out = (char *)&rxp;
    blob.in_size = sizeof(rxp);
    blob.out_size = sizeof(rxp);
    code = pioctl(0, VIOC_TWIDDLE, &blob, 1);

    if (code) {
	Die(code, 0);
    }
    return code;
}

#include "AFS_component_version_number.c"

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
    /* try to find volume location information */


    ts = cmd_CreateSyntax(NULL, Twiddle, 0, "adjust rx parms");
    cmd_AddParm(ts, "-initReceiveWindow ", CMD_SINGLE, CMD_OPTIONAL, "16");
    cmd_AddParm(ts, "-maxReceiveWindow ", CMD_SINGLE, CMD_OPTIONAL, "16");
    cmd_AddParm(ts, "-initSendWindow ", CMD_SINGLE, CMD_OPTIONAL, "8");
    cmd_AddParm(ts, "-maxSendWindow ", CMD_SINGLE, CMD_OPTIONAL, "16");
    cmd_AddParm(ts, "-nSendFrags ", CMD_SINGLE, CMD_OPTIONAL, "4");
    cmd_AddParm(ts, "-nRecvFrags ", CMD_SINGLE, CMD_OPTIONAL, "4");
    cmd_AddParm(ts, "-OrphanFragSize ", CMD_SINGLE, CMD_OPTIONAL, "512");
    cmd_AddParm(ts, "-maxReceiveSize ", CMD_SINGLE, CMD_OPTIONAL, "");
    cmd_AddParm(ts, "-MyMaxSendSize ", CMD_SINGLE, CMD_OPTIONAL, "");

    code = cmd_Dispatch(argc, argv);
    if (rxInitDone)
	rx_Finalize();

    exit(code);
}

Die(code, filename)
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
