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
#include <afs/stds.h>

#include <roken.h>
#include <afs/sys_prototypes.h>
#include <afs/vioc.h>

#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{
    afs_int32 code;
    struct ViceIoctl blob;

    blob.in = 0;
    blob.out = 0;
    blob.in_size = 0;
    blob.out_size = 0;
    code = pioctl(0, VIOC_GCPAGS, &blob, 1);
    if (code != 0) {
	perror("disable gcpags failed");
    }

    return code;
}
