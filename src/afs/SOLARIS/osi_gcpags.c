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

void
afs_osi_TraverseProcTable(void)
{
	afs_proc_t *prp;
	for (prp = practive; prp != NULL; prp = prp->p_next) {
		afs_GCPAGs_perproc_func(prp);
	}
}

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */
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

#endif /* AFS_GCPAGS */
