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


/*
 * NOTE: h/proc_private.h gives the process table locking rules
 * It indicates that access to p_cred must be protected by
 * mp_mtproc_lock(p);
 * mp_mtproc_unlock(p);
 *
 * The code in sys/pm_prot.c uses pcred_lock() to protect access to
 * the process creds, and uses mp_mtproc_lock() only for audit-related
 * changes.  To be safe, we use both.
 */

void
afs_osi_TraverseProcTable(void)
{
    proc_t *p;
    int endchain = 0;

    MP_SPINLOCK(activeproc_lock);
    MP_SPINLOCK(sched_lock);
    pcred_lock();

    /*
     * Instead of iterating through all of proc[], traverse only
     * the list of active processes.  As an example of this,
     * see foreach_process() in sys/vm_sched.c.
     *
     * We hold the locks for the entire scan in order to get a
     * consistent view of the current set of creds.
     */

    for (p = proc; endchain == 0; p = &proc[p->p_fandx]) {
	if (p->p_fandx == 0) {
	    endchain = 1;
	}

	if (system_proc(p))
	    continue;

	mp_mtproc_lock(p);
	afs_GCPAGs_perproc_func(p);
	mp_mtproc_unlock(p);
    }

    pcred_unlock();
    MP_SPINUNLOCK(sched_lock);
    MP_SPINUNLOCK(activeproc_lock);
}

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */
const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * p)
{
    if (!p)
	return;

    /*
     * Cannot use afs_warnuser() here, as the code path
     * eventually wants to grab sched_lock, which is
     * already held here
     */

    return p_cred(p);
}

#endif /* AFS_GCPAGS */
