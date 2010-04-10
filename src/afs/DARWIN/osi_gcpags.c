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


#if (defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN80_ENV))
void
afs_osi_TraverseProcTable(void)
{
    afs_proc_t *p;
    LIST_FOREACH(p, &allproc, p_list) {
	if (p->p_stat == SIDL)
	    continue;
	if (p->p_stat == SZOMB)
	    continue;
	if (p->p_flag & P_SYSTEM)
	    continue;
	afs_GCPAGs_perproc_func(p);
    }
}
#endif

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */

#if defined(AFS_DARWIN80_ENV)

const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * pr)
{
    afs_ucred_t *rv = NULL;
    static afs_ucred_t cr;
    struct ucred *pcred;

    if (pr == NULL) {
	return NULL;
    }
    pcred = proc_ucred(pr);
    cr.cr_ref = 1;
    afs_set_cr_uid(&cr, afs_cr_uid(pcred));
    cr.cr_ngroups = pcred->cr_ngroups;
    memcpy(cr.cr_groups, pcred->cr_groups,
           NGROUPS * sizeof(gid_t));
    return &cr;
}
#else

const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * pr)
{
    afs_ucred_t *rv = NULL;
    static afs_ucred_t cr;

    if (pr == NULL) {
	return NULL;
    }

    if ((pr->p_stat == SSLEEP) || (pr->p_stat == SRUN)
	|| (pr->p_stat == SSTOP)) {
	pcred_readlock(pr);
	cr.cr_ref = 1;
	afs_set_cr_uid(&cr, afs_cr_uid(pr->p_cred->pc_ucred));
	cr.cr_ngroups = pr->p_cred->pc_ucred->cr_ngroups;
	memcpy(cr.cr_groups, pr->p_cred->pc_ucred->cr_groups,
	       NGROUPS * sizeof(gid_t));
	pcred_unlock(pr);
	rv = &cr;
    }

    return rv;
}

#endif

#endif /* AFS_GCPAGS */
