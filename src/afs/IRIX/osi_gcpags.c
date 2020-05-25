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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if AFS_GCPAGS

/* afs_osi_TraverseProcTable() - Walk through the systems process
 * table, calling afs_GCPAGs_perproc_func() for each process.
 */


/* TODO: Fix this later. */
static int
SGI_ProcScanFunc(void *p, void *arg, int mode)
{
    return 0;
}

void
afs_osi_TraverseProcTable(void)
{
    procscan(SGI_ProcScanFunc, afs_GCPAGs_perproc_func);
}

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */

const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * p)
{
    return NULL;
}

#endif /* AFS_GCPAGS */
