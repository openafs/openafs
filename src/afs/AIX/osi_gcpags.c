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
#include <sys/adspace.h>	/* for vm_att(), vm_det() */

#if AFS_GCPAGS

/* afs_osi_TraverseProcTable() - Walk through the systems process
 * table, calling afs_GCPAGs_perproc_func() for each process.
 */

#ifdef AFS_AIX51_ENV
#define max_proc v.ve_proc
#endif
void
afs_osi_TraverseProcTable(void)
{
    afs_proc_t *p;
    int i;

    /*
     * For binary compatibility, on AIX we need to be careful to use the
     * proper size of a struct proc, even if it is different from what
     * we were compiled with.
     */
    if (!afs_gcpags_procsize)
	return;

#ifndef AFS_AIX51_ENV
    simple_lock(&proc_tbl_lock);
#endif
    for (p = (afs_proc_t *)v.vb_proc, i = 0; p < max_proc;
	 p = (afs_proc_t *)((char *)p + afs_gcpags_procsize), i++) {

#ifdef AFS_AIX51_ENV
	if (p->p_pvprocp->pv_stat == SNONE)
	    continue;
	if (p->p_pvprocp->pv_stat == SIDL)
	    continue;
	if (p->p_pvprocp->pv_stat == SEXIT)
	    continue;
#else
	if (p->p_stat == SNONE)
	    continue;
	if (p->p_stat == SIDL)
	    continue;
	if (p->p_stat == SEXIT)
	    continue;
#endif

	/* sanity check */

	if (PROCMASK(p->p_pid) != i) {
	    afs_gcpags = AFS_GCPAGS_EPIDCHECK;
	    break;
	}

	/* sanity check */

	if ((p->p_nice < P_NICE_MIN) || (P_NICE_MAX < p->p_nice)) {
	    afs_gcpags = AFS_GCPAGS_ENICECHECK;
	    break;
	}

	afs_GCPAGs_perproc_func(p);
    }
#ifndef AFS_AIX51_ENV
    simple_unlock(&proc_tbl_lock);
#endif
}

/* return a pointer (sometimes a static copy ) to the cred for a
 * given afs_proc_t.
 * subsequent calls may overwrite the previously returned value.
 */

/* GLOBAL DECLARATIONS */

/*
 * LOCKS: the caller must do
 *  simple_lock(&proc_tbl_lock);
 *  simple_unlock(&proc_tbl_lock);
 * around calls to this function.
 */

const afs_ucred_t *
afs_osi_proc2cred(afs_proc_t * pproc)
{
    afs_ucred_t *pcred = 0;

    /*
     * pointer to process user structure valid in *our*
     * address space
     *
     * The user structure for a process is stored in the user
     * address space (as distinct from the kernel address
     * space), and so to refer to the user structure of a
     * different process we must employ special measures.
     *
     * I followed the example used in the AIX getproc() system
     * call in bos/kernel/proc/getproc.c
     */
    struct user *xmem_userp;

    struct xmem dp;		/* ptr to xmem descriptor */
    int xm;			/* xmem result */

    if (!pproc) {
	return pcred;
    }

    /*
     * The process private segment in which the user
     * area is located may disappear. We need to increment
     * its use count. Therefore we
     *    - get the proc_tbl_lock to hold the segment.
     *    - get the p_lock to lockout vm_cleardata.
     *    - vm_att to load the segment register (no check)
     *    - xmattach to bump its use count.
     *    - release the p_lock.
     *    - release the proc_tbl_lock.
     *    - do whatever we need.
     *    - xmdetach to decrement the use count.
     *    - vm_det to free the segment register (no check)
     */

    xmem_userp = NULL;
    xm = XMEM_FAIL;
    /* simple_lock(&proc_tbl_lock); */
#ifdef __64BIT__
    if (pproc->p_adspace != vm_handle(NULLSEGID, (int32long64_t) 0)) {
#else
    if (pproc->p_adspace != NULLSEGVAL) {
#endif

#ifdef AFS_AIX51_ENV
	simple_lock(&pproc->p_pvprocp->pv_lock);
#else
	simple_lock(&pproc->p_lock);
#endif

	if (pproc->p_threadcount &&
#ifdef AFS_AIX51_ENV
	    pproc->p_pvprocp->pv_threadlist) {
#else
	    pproc->p_threadlist) {
#endif

	    /*
	     * arbitrarily pick the first thread in pproc
	     */
	    struct thread *pproc_thread =
#ifdef AFS_AIX51_ENV
		pproc->p_pvprocp->pv_threadlist;
#else
		pproc->p_threadlist;
#endif

	    /*
	     * location of 'struct user' in pproc's
	     * address space
	     */
	    struct user *pproc_userp = pproc_thread->t_userp;

	    /*
	     * create a pointer valid in my own address space
	     */

	    xmem_userp = (struct user *)vm_att(pproc->p_adspace, pproc_userp);

	    dp.aspace_id = XMEM_INVAL;
	    xm = xmattach(xmem_userp, sizeof(*xmem_userp), &dp, SYS_ADSPACE);
	}

#ifdef AFS_AIX51_ENV
	simple_unlock(&pproc->p_pvprocp->pv_lock);
#else
	simple_unlock(&pproc->p_lock);
#endif
    }
    /* simple_unlock(&proc_tbl_lock); */
    if (xm == XMEM_SUCC) {

	static afs_ucred_t cred;

	/*
	 * What locking should we use to protect access to the user
	 * area?  If needed also change the code in AIX/osi_groups.c.
	 */

	/* copy cred to local address space */
	cred = *xmem_userp->U_cred;
	pcred = &cred;

	xmdetach(&dp);
    }
    if (xmem_userp) {
	vm_det((void *)xmem_userp);
    }

    return pcred;
}


#endif /* AFS_GCPAGS */
