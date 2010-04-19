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


#ifdef AFS_SGI65_ENV
/* TODO: Fix this later. */
static int
SGI_ProcScanFunc(void *p, void *arg, int mode)
{
    return 0;
}
#else /* AFS_SGI65_ENV */
static int
SGI_ProcScanFunc(proc_t * p, void *arg, int mode)
{
    afs_int32(*perproc_func) (afs_proc_t *) = arg;
    int code = 0;
    /* we pass in the function pointer for arg,
     * mode ==0 for startup call, ==1 for each valid proc,
     * and ==2 for terminate call.
     */
    if (mode == 1) {
	code = perproc_func(p);
    }
    return code;
}
#endif /* AFS_SGI65_ENV */

void
afs_osi_TraverseProcTable(void)
{
    procscan(SGI_ProcScanFunc, afs_GCPAGs_perproc_func);
}

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */

#if defined(AFS_SGI65_ENV)
const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * p)
{
    return NULL;
}

#else
const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * pr)
{
    afs_ucred_t *rv = NULL;

    if (pr == NULL) {
	return NULL;
    }
    rv = pr->p_cred;

    return rv;
}
#endif

#endif /* AFS_GCPAGS */
