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
#if !defined(LINUX_KEYRING_SUPPORT) && (!defined(STRUCT_TASK_STRUCT_HAS_CRED) || defined(HAVE_LINUX_RCU_READ_LOCK))
    struct task_struct *p;

    rcu_read_lock();

#if defined(for_each_process)
    for_each_process(p) if (p->pid) {
#ifdef STRUCT_TASK_STRUCT_HAS_EXIT_STATE
	if (p->exit_state)
	    continue;
#else
	if (p->state & TASK_ZOMBIE)
	    continue;
#endif
	afs_GCPAGs_perproc_func(p);
    }
#else
    for_each_task(p) if (p->pid) {
#ifdef STRUCT_TASK_STRUCT_HAS_EXIT_STATE
	if (p->exit_state)
	    continue;
#else
	if (p->state & TASK_ZOMBIE)
	    continue;
#endif
	afs_GCPAGs_perproc_func(p);
    }
#endif

    rcu_read_unlock();
#endif
}

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */

#if !defined(LINUX_KEYRING_SUPPORT) && (!defined(STRUCT_TASK_STRUCT_HAS_CRED) || defined(HAVE_LINUX_RCU_READ_LOCK))
const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * pr)
{
    afs_ucred_t *rv = NULL;
    static afs_ucred_t cr;

    if (pr == NULL) {
	return NULL;
    }

    if ((pr->state == TASK_RUNNING) || (pr->state == TASK_INTERRUPTIBLE)
	|| (pr->state == TASK_UNINTERRUPTIBLE)
	|| (pr->state == TASK_STOPPED)) {
	/* This is dangerous. If anyone ever crfree's the cred that's
	 * returned from here, we'll go boom, because it's statically
	 * allocated. */
	atomic_set(&cr.cr_ref, 1);
	afs_set_cr_uid(&cr, task_uid(pr));
#if defined(STRUCT_TASK_STRUCT_HAS_CRED)
	get_group_info(pr->cred->group_info);
	afs_set_cr_group_info(&cr, pr->cred->group_info);
#else
	get_group_info(pr->group_info);
	afs_set_cr_group_info(&cr, pr->group_info);
#endif
	rv = &cr;
    }

    return rv;
}
#endif

#endif /* AFS_GCPAGS */
