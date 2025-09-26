/*
 * Copyright 2004, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/cmd.h>

#include <rx/xdr.h>
#include <rx/rx_queue.h>
#include <afs/afsint.h>
#include "nfs.h"
#include <afs/afs_lock.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"

int VolumeChanged; /* to keep physio happy */

static int
handleit(struct cmd_syndesc *as, void *arock)
{
    Volume *vp;
    Error ec;
    int bless, unbless, nofssync;
    int volumeId;
    VolumePackageOptions opts;
    ProgramType pt;

    volumeId = atoi(as->parms[0].items->data);
    bless    = !!(as->parms[1].items);
    unbless  = !!(as->parms[2].items);
    nofssync = !!(as->parms[3].items);

    if (bless && unbless) {
	fprintf(stderr,"Cannot use both -bless and -unbless\n");
	exit(1);
    }

    if (nofssync) {
	pt = salvager;
    } else {
	pt = volumeUtility;
    }

    VOptDefaults(pt, &opts);
    opts.canUseFSSYNC = !nofssync;

    if (VInitVolumePackage2(pt, &opts)) {
	fprintf(stderr,"Unable to initialize volume package\n");
	exit(1);
    }

    vp = VAttachVolume(&ec, volumeId, V_VOLUPD);
    if (ec) {
	fprintf(stderr,"VAttachVolume failed: %d\n", ec);
	exit(1);
    }
    if (bless) V_blessed(vp) = 1;
    if (unbless) V_blessed(vp) = 0;
    VUpdateVolume(&ec, vp);
    if (ec) {
	fprintf(stderr,"VUpdateVolume failed: %d\n", ec);
	VDetachVolume(&ec, vp);
	exit(1);
    }
    VDetachVolume(&ec, vp);
    return 0;
}

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    afs_int32 code;

    ts = cmd_CreateSyntax(NULL, handleit, NULL, 0, "Manipulate volume blessed bit");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_REQUIRED, "Volume id");
    cmd_AddParm(ts, "-bless", CMD_FLAG, CMD_OPTIONAL, "Set blessed bit");
    cmd_AddParm(ts, "-unbless", CMD_FLAG, CMD_OPTIONAL, "Clear blessed bit");
    cmd_AddParm(ts, "-nofssync", CMD_FLAG, CMD_OPTIONAL,
		"Don't communicate with running fileserver");
    code = cmd_Dispatch(argc, argv);
    return code;
}


