/*
 * Copyright 2004, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>

RCSID
    ("$Header$");

#include <stdio.h>

#include <afs/cmd.h>

#include <rx/xdr.h>
#include <afs/afsint.h>
#include "nfs.h"
#include "lock.h"
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"

int VolumeChanged; /* to keep physio happy */

static int
handleit(struct cmd_syndesc *as, char *arock)
{
    Volume *vp;
    Error ec;
    int bless, unbless, nofssync;
    int volumeId;

    volumeId = atoi(as->parms[0].items->data);
    bless    = !!(as->parms[1].items);
    unbless  = !!(as->parms[2].items);
    nofssync = !!(as->parms[3].items);

    if (bless && unbless) {
	fprintf(stderr,"Cannot use both -bless and -unbless\n");
	exit(1);
    }

    if (VInitVolumePackage(nofssync ? salvager : volumeUtility, 5, 5, 1, 0)) {
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
	VPutVolume(vp);
	exit(1);
    }
    VPutVolume(vp);
    return 0;
}

int
main(int argc, char **argv)
{
    register struct cmd_syndesc *ts;
    afs_int32 code;

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_EphemeralUtility, osi_NULL)));

    ts = cmd_CreateSyntax(NULL, handleit, 0, "Manipulate volume blessed bit");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_REQUIRED, "Volume id");
    cmd_AddParm(ts, "-bless", CMD_FLAG, CMD_OPTIONAL, "Set blessed bit");
    cmd_AddParm(ts, "-unbless", CMD_FLAG, CMD_OPTIONAL, "Clear blessed bit");
    cmd_AddParm(ts, "-nofssync", CMD_FLAG, CMD_OPTIONAL,
		"Don't communicate with running fileserver");
    code = cmd_Dispatch(argc, argv);
    return code;
}


