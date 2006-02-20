/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/stds.h"
#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"

/*
 * Chunk module.
 */

/* Place the defaults in afsd instead of all around the code, so
 * AFS_SETCHUNKSIZE() needs to be called before doing anything */
afs_int32 afs_FirstCSize = AFS_DEFAULTCSIZE;
afs_int32 afs_OtherCSize = AFS_DEFAULTCSIZE;
afs_int32 afs_LogChunk = AFS_DEFAULTLSIZE;
