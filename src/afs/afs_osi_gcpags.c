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

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#ifdef AFS_AIX_ENV
#include <sys/adspace.h>	/* for vm_att(), vm_det() */
#endif

#if AFS_GCPAGS

/* afs_osi_TraverseProcTable() - Walk through the systems process
 * table, calling afs_GCPAGs_perproc_func() for each process.
 */

#if defined(AFS_SUN5_ENV)
void
afs_osi_TraverseProcTable(void)
{
    struct proc *prp;
    for (prp = practive; prp != NULL; prp = prp->p_next) {
	afs_GCPAGs_perproc_func(prp);
    }
}
#endif

#if defined(AFS_HPUX_ENV)

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
    register proc_t *p;
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
#endif

#if defined(AFS_SGI_ENV)

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
    afs_int32(*perproc_func) (struct proc *) = arg;
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
#endif /* AFS_SGI_ENV */

#if defined(AFS_AIX_ENV)
#ifdef AFS_AIX51_ENV
#define max_proc v.ve_proc
#endif
void
afs_osi_TraverseProcTable(void)
{
    struct proc *p;
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
    for (p = (struct proc *)v.vb_proc, i = 0; p < max_proc;
	 p = (struct proc *)((char *)p + afs_gcpags_procsize), i++) {

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
#endif

#if defined(AFS_OSF_ENV)

#ifdef AFS_DUX50_ENV
extern struct pid_entry *pidtab;
extern int npid; 
#endif

void
afs_osi_TraverseProcTable(void)
{
    struct pid_entry *pe;
#ifdef AFS_DUX50_ENV
#define pidNPID (pidtab + npid)
#define PID_LOCK()
#define PID_UNLOCK()
#endif
    PID_LOCK();
    for (pe = pidtab; pe < pidNPID; ++pe) {
	if (pe->pe_proc != PROC_NULL)
	    afs_GCPAGs_perproc_func(pe->pe_proc);
    }
    PID_UNLOCK();
}
#endif

#if (defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN80_ENV)) || defined(AFS_FBSD_ENV)
void
afs_osi_TraverseProcTable(void)
{
    struct proc *p;
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

#if defined(AFS_LINUX22_ENV)
void
afs_osi_TraverseProcTable()
{
#if !defined(LINUX_KEYRING_SUPPORT)
    struct task_struct *p;
#ifdef EXPORTED_TASKLIST_LOCK
    extern rwlock_t tasklist_lock __attribute__((weak));

    if (&tasklist_lock)
       read_lock(&tasklist_lock);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
#ifdef EXPORTED_TASKLIST_LOCK
    else
#endif
	rcu_read_lock();
#endif

#ifdef DEFINED_FOR_EACH_PROCESS
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
#ifdef EXPORTED_TASKLIST_LOCK
    if (&tasklist_lock)
       read_unlock(&tasklist_lock);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
#ifdef EXPORTED_TASKLIST_LOCK
    else
#endif
	rcu_read_unlock();
#endif
#endif
}
#endif

/* return a pointer (sometimes a static copy ) to the cred for a
 * given AFS_PROC.
 * subsequent calls may overwrite the previously returned value.
 */

#if defined(AFS_SGI65_ENV)
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * p)
{
    return NULL;
}
#elif defined(AFS_HPUX_ENV)
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * p)
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
#elif defined(AFS_AIX_ENV)

/* GLOBAL DECLARATIONS */

/*
 * LOCKS: the caller must do
 *  simple_lock(&proc_tbl_lock);
 *  simple_unlock(&proc_tbl_lock);
 * around calls to this function.
 */

const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * pproc)
{
    struct AFS_UCRED *pcred = 0;

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

	static struct AFS_UCRED cred;

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

#elif defined(AFS_OSF_ENV)
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * pr)
{
    struct AFS_UCRED *rv = NULL;

    if (pr == NULL) {
	return NULL;
    }

    if ((pr->p_stat == SSLEEP) || (pr->p_stat == SRUN)
	|| (pr->p_stat == SSTOP))
	rv = pr->p_rcred;

    return rv;
}
#elif defined(AFS_DARWIN80_ENV) 
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * pr)
{
    struct AFS_UCRED *rv = NULL;
    static struct AFS_UCRED cr;
    struct ucred *pcred;

    if (pr == NULL) {
	return NULL;
    }
    pcred = proc_ucred(pr);
    cr.cr_ref = 1;
    cr.cr_uid = pcred->cr_uid;
    cr.cr_ngroups = pcred->cr_ngroups;
    memcpy(cr.cr_groups, pcred->cr_groups,
           NGROUPS * sizeof(gid_t));
    return &cr;
}
#elif defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * pr)
{
    struct AFS_UCRED *rv = NULL;
    static struct AFS_UCRED cr;

    if (pr == NULL) {
	return NULL;
    }

    if ((pr->p_stat == SSLEEP) || (pr->p_stat == SRUN)
	|| (pr->p_stat == SSTOP)) {
	pcred_readlock(pr);
	cr.cr_ref = 1;
	cr.cr_uid = pr->p_cred->pc_ucred->cr_uid;
	cr.cr_ngroups = pr->p_cred->pc_ucred->cr_ngroups;
	memcpy(cr.cr_groups, pr->p_cred->pc_ucred->cr_groups,
	       NGROUPS * sizeof(gid_t));
	pcred_unlock(pr);
	rv = &cr;
    }

    return rv;
}
#elif defined(AFS_LINUX22_ENV)
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * pr)
{
    struct AFS_UCRED *rv = NULL;
    static struct AFS_UCRED cr;

    if (pr == NULL) {
	return NULL;
    }

    if ((pr->state == TASK_RUNNING) || (pr->state == TASK_INTERRUPTIBLE)
	|| (pr->state == TASK_UNINTERRUPTIBLE)
	|| (pr->state == TASK_STOPPED)) {
	cr.cr_ref = 1;
	cr.cr_uid = pr->uid;
#if defined(AFS_LINUX26_ENV)
	get_group_info(pr->group_info);
	cr.cr_group_info = pr->group_info;
#else
	cr.cr_ngroups = pr->ngroups;
	memcpy(cr.cr_groups, pr->groups, NGROUPS * sizeof(gid_t));
#endif
	rv = &cr;
    }

    return rv;
}
#else
const struct AFS_UCRED *
afs_osi_proc2cred(AFS_PROC * pr)
{
    struct AFS_UCRED *rv = NULL;

    if (pr == NULL) {
	return NULL;
    }
    rv = pr->p_cred;

    return rv;
}
#endif

#endif /* AFS_GCPAGS */
