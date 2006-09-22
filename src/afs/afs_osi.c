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

static char memZero;		/* address of 0 bytes for kmem_alloc */

struct osimem {
    struct osimem *next;
};

/* osi_Init -- do once per kernel installation initialization.
 *     -- On Solaris this is called from modload initialization.
 *     -- On AIX called from afs_config.
 *     -- On HP called from afsc_link.
 *     -- On SGI called from afs_init. */

#ifdef AFS_SGI53_ENV
lock_t afs_event_lock;
#endif

#ifdef AFS_SGI64_ENV
flid_t osi_flid;
#endif

struct AFS_UCRED *afs_osi_credp;

void
osi_Init(void)
{
    static int once = 0;
    if (once++ > 0)		/* just in case */
	return;
#if	defined(AFS_HPUX_ENV)
    osi_InitGlock();
#else /* AFS_HPUX_ENV */
#if defined(AFS_GLOBAL_SUNLOCK)
#if defined(AFS_SGI62_ENV)
    mutex_init(&afs_global_lock, MUTEX_DEFAULT, "afs_global_lock");
#elif defined(AFS_OSF_ENV)
    usimple_lock_init(&afs_global_lock);
    afs_global_owner = (thread_t) 0;
#elif defined(AFS_FBSD50_ENV)
    mtx_init(&afs_global_mtx, "AFS global lock", NULL, MTX_DEF);
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#if !defined(AFS_DARWIN80_ENV)
    lockinit(&afs_global_lock, PLOCK, "afs global lock", 0, 0);
#endif
    afs_global_owner = 0;
#elif defined(AFS_AIX41_ENV)
    lock_alloc((void *)&afs_global_lock, LOCK_ALLOC_PIN, 1, 1);
    simple_lock_init((void *)&afs_global_lock);
#elif !defined(AFS_LINUX22_ENV)
    /* Linux initialization in osi directory. Should move the others. */
    mutex_init(&afs_global_lock, "afs_global_lock", MUTEX_DEFAULT, NULL);
#endif
#endif /* AFS_GLOBAL_SUNLOCK */
#endif /* AFS_HPUX_ENV */

    if (!afs_osicred_initialized) {
#if defined(AFS_DARWIN80_ENV)
        afs_osi_ctxtp_initialized = 0;
        afs_osi_ctxtp = NULL; /* initialized in afs_Daemon since it has
                                  a proc reference that cannot be changed */
#endif
#if defined(AFS_XBSD_ENV)
	/* Can't just invent one, must use crget() because of mutex */
	afs_osi_credp = crdup(osi_curcred());
#else
	memset(&afs_osi_cred, 0, sizeof(struct AFS_UCRED));
#if defined(AFS_LINUX26_ENV)
        afs_osi_cred.cr_group_info = groups_alloc(0);
#endif
#if defined(AFS_DARWIN80_ENV)
        afs_osi_cred.cr_ref = 1; /* kauth_cred_get_ref needs 1 existing ref */
#else
	crhold(&afs_osi_cred);	/* don't let it evaporate */
#endif

	afs_osi_credp = &afs_osi_cred;
#endif
	afs_osicred_initialized = 1;
    }
#ifdef AFS_SGI64_ENV
    osi_flid.fl_pid = osi_flid.fl_sysid = 0;
#endif

    init_et_to_sys_error();
}

int
osi_Active(register struct vcache *avc)
{
    AFS_STATCNT(osi_Active);
#if defined(AFS_AIX_ENV) || defined(AFS_OSF_ENV) || defined(AFS_SUN5_ENV) || (AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if ((avc->opens > 0) || (avc->states & CMAPPED))
	return 1;		/* XXX: Warning, verify this XXX  */
#elif defined(AFS_SGI_ENV)
    if ((avc->opens > 0) || AFS_VN_MAPPED(AFSTOV(avc)))
	return 1;
#else
    if (avc->opens > 0 || (AFSTOV(avc)->v_flag & VTEXT))
	return (1);
#endif
    return 0;
}

/* this call, unlike osi_FlushText, is supposed to discard caches that may
   contain invalid information if a file is written remotely, but that may
   contain valid information that needs to be written back if the file is
   being written locally.  It doesn't subsume osi_FlushText, since the latter
   function may be needed to flush caches that are invalidated by local writes.

   avc->pvnLock is already held, avc->lock is guaranteed not to be held (by
   us, of course).
*/
void
osi_FlushPages(register struct vcache *avc, struct AFS_UCRED *credp)
{
    afs_hyper_t origDV;
    ObtainReadLock(&avc->lock);
    /* If we've already purged this version, or if we're the ones
     * writing this version, don't flush it (could lose the
     * data we're writing). */
    if ((hcmp((avc->m.DataVersion), (avc->mapDV)) <= 0)
	|| ((avc->execsOrWriters > 0) && afs_DirtyPages(avc))) {
	ReleaseReadLock(&avc->lock);
	return;
    }
    ReleaseReadLock(&avc->lock);
    ObtainWriteLock(&avc->lock, 10);
    /* Check again */
    if ((hcmp((avc->m.DataVersion), (avc->mapDV)) <= 0)
	|| ((avc->execsOrWriters > 0) && afs_DirtyPages(avc))) {
	ReleaseWriteLock(&avc->lock);
	return;
    }
    if (hiszero(avc->mapDV)) {
	hset(avc->mapDV, avc->m.DataVersion);
	ReleaseWriteLock(&avc->lock);
	return;
    }

    AFS_STATCNT(osi_FlushPages);
    hset(origDV, avc->m.DataVersion);
    afs_Trace3(afs_iclSetp, CM_TRACE_FLUSHPAGES, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, origDV.low, ICL_TYPE_INT32, avc->m.Length);

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    osi_VM_FlushPages(avc, credp);
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 88);

    /* do this last, and to original version, since stores may occur
     * while executing above PUTPAGE call */
    hset(avc->mapDV, origDV);
    ReleaseWriteLock(&avc->lock);
}

afs_lock_t afs_ftf;		/* flush text lock */

#ifdef	AFS_TEXT_ENV

/* This call is supposed to flush all caches that might be invalidated
 * by either a local write operation or a write operation done on
 * another client.  This call may be called repeatedly on the same
 * version of a file, even while a file is being written, so it
 * shouldn't do anything that would discard newly written data before
 * it is written to the file system. */

void
osi_FlushText_really(register struct vcache *vp)
{
    afs_hyper_t fdv;		/* version before which we'll flush */

    AFS_STATCNT(osi_FlushText);
    /* see if we've already flushed this data version */
    if (hcmp(vp->m.DataVersion, vp->flushDV) <= 0)
	return;

    MObtainWriteLock(&afs_ftf, 317);
    hset(fdv, vp->m.DataVersion);

    /* why this disgusting code below?
     *    xuntext, called by xrele, doesn't notice when it is called
     * with a freed text object.  Sun continually calls xrele or xuntext
     * without any locking, as long as VTEXT is set on the
     * corresponding vnode.
     *    But, if the text object is locked when you check the VTEXT
     * flag, several processes can wait in xuntext, waiting for the
     * text lock; when the second one finally enters xuntext's
     * critical region, the text object is already free, but the check
     * was already done by xuntext's caller.
     *    Even worse, it turns out that xalloc locks the text object
     * before reading or stating a file via the vnode layer.  Thus, we
     * could end up in getdcache, being asked to bring in a new
     * version of a file, but the corresponding text object could be
     * locked.  We can't flush the text object without causing
     * deadlock, so now we just don't try to lock the text object
     * unless it is guaranteed to work.  And we try to flush the text
     * when we need to a bit more often at the vnode layer.  Sun
     * really blew the vm-cache flushing interface.
     */

#if defined (AFS_HPUX_ENV)
    if (vp->v.v_flag & VTEXT) {
	xrele(vp);

	if (vp->v.v_flag & VTEXT) {	/* still has a text object? */
	    MReleaseWriteLock(&afs_ftf);
	    return;
	}
    }
#endif

    /* next do the stuff that need not check for deadlock problems */
    mpurge(vp);

    /* finally, record that we've done it */
    hset(vp->flushDV, fdv);
    MReleaseWriteLock(&afs_ftf);

}
#endif /* AFS_TEXT_ENV */

/* mask signals in afsds */
void
afs_osi_MaskSignals(void)
{
#ifdef AFS_LINUX22_ENV
    osi_linux_mask();
#endif
}

/* unmask signals in rxk listener */
void
afs_osi_UnmaskRxkSignals(void)
{
}

/* Two hacks to try and fix afsdb */
void 
afs_osi_MaskUserLoop()
{
#ifdef AFS_DARWIN_ENV
    afs_osi_Invisible();
    afs_osi_fullSigMask();
#else
    afs_osi_MaskSignals();
#endif
}

void 
afs_osi_UnmaskUserLoop()
{
#ifdef AFS_DARWIN_ENV
    afs_osi_fullSigRestore();
#endif
}

/* register rxk listener proc info */
void
afs_osi_RxkRegister(void)
{
#ifdef AFS_LINUX22_ENV
    osi_linux_rxkreg();
#endif
}

/* procedure for making our processes as invisible as we can */
void
afs_osi_Invisible(void)
{
#ifdef AFS_LINUX22_ENV
    afs_osi_MaskSignals();
#elif defined(AFS_SUN5_ENV)
    curproc->p_flag |= SSYS;
#elif defined(AFS_HPUX101_ENV) && !defined(AFS_HPUX1123_ENV)
    set_system_proc(u.u_procp);
#elif defined(AFS_DARWIN80_ENV)
#elif defined(AFS_DARWIN_ENV)
    /* maybe call init_process instead? */
    current_proc()->p_flag |= P_SYSTEM;
#elif defined(AFS_XBSD_ENV)
    curproc->p_flag |= P_SYSTEM;
#elif defined(AFS_SGI_ENV)
    vrelvm();
#endif

    AFS_STATCNT(osi_Invisible);
}


#if !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV)
/* set the real time */
void
afs_osi_SetTime(osi_timeval_t * atv)
{
#if defined(AFS_AIX32_ENV)
    struct timestruc_t t;

    t.tv_sec = atv->tv_sec;
    t.tv_nsec = atv->tv_usec * 1000;
    ksettimer(&t);		/*  Was -> settimer(TIMEOFDAY, &t); */
#elif defined(AFS_SUN55_ENV)
    stime(atv->tv_sec);
#elif defined(AFS_SUN5_ENV)
    /*
     * To get more than second resolution we can use adjtime. The problem
     * is that the usecs from the server are wrong (by now) so it isn't
     * worth complicating the following code.
     */
    struct stimea {
	time_t time;
    } sta;

    sta.time = atv->tv_sec;

    stime(&sta, NULL);
#elif defined(AFS_SGI_ENV)
    struct stimea {
	sysarg_t time;
    } sta;

    AFS_GUNLOCK();
    sta.time = atv->tv_sec;
    stime(&sta);
    AFS_GLOCK();
#elif defined(AFS_DARWIN_ENV)
#ifndef AFS_DARWIN80_ENV
    AFS_GUNLOCK();
    setthetime(atv);
    AFS_GLOCK();
#endif
#else
    /* stolen from kern_time.c */
#ifndef	AFS_AUX_ENV
    boottime.tv_sec += atv->tv_sec - time.tv_sec;
#endif
#ifdef AFS_HPUX_ENV
    {
#if !defined(AFS_HPUX1122_ENV)
	/* drop the setting of the clock for now. spl7 is not
	 * known on hpux11.22
	 */
	register ulong_t s;
	struct timeval t;
	t.tv_sec = atv->tv_sec;
	t.tv_usec = atv->tv_usec;
	s = spl7();
	time = t;
	(void)splx(s);
	resettodr(atv);
#endif
    }
#else
    {
	register int s;
	s = splclock();
	time = *atv;
	(void)splx(s);
    }
    resettodr();
#endif
#ifdef	AFS_AUX_ENV
    logtchg(atv->tv_sec);
#endif
#endif /* AFS_DARWIN_ENV */
    AFS_STATCNT(osi_SetTime);
}
#endif /* AFS_LINUX20_ENV */


void *
afs_osi_Alloc(size_t x)
{
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_FBSD_ENV)
    register struct osimem *tm = NULL;
    register int size;
#endif

    AFS_STATCNT(osi_Alloc);
    /* 0-length allocs may return NULL ptr from AFS_KALLOC, so we special-case
     * things so that NULL returned iff an error occurred */
    if (x == 0)
	return &memZero;

    AFS_STATS(afs_stats_cmperf.OutStandingAllocs++);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage += x);
#ifdef AFS_LINUX20_ENV
    return osi_linux_alloc(x, 1);
#elif defined(AFS_FBSD_ENV)
    return osi_fbsd_alloc(x, 1);
#else
    size = x;
    tm = (struct osimem *)AFS_KALLOC(size);
#ifdef	AFS_SUN5_ENV
    if (!tm)
	osi_Panic("osi_Alloc: Couldn't allocate %d bytes; out of memory!\n",
		  size);
#endif
    return (void *)tm;
#endif
}

#if	defined(AFS_SUN5_ENV) || defined(AFS_SGI_ENV)

void *
afs_osi_Alloc_NoSleep(size_t x)
{
    register struct osimem *tm;
    register int size;

    AFS_STATCNT(osi_Alloc);
    /* 0-length allocs may return NULL ptr from AFS_KALLOC, so we special-case
     * things so that NULL returned iff an error occurred */
    if (x == 0)
	return &memZero;

    size = x;
    AFS_STATS(afs_stats_cmperf.OutStandingAllocs++);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage += x);
    tm = (struct osimem *)AFS_KALLOC_NOSLEEP(size);
    return (void *)tm;
}

#endif /* SUN || SGI */

void
afs_osi_Free(void *x, size_t asize)
{
    AFS_STATCNT(osi_Free);
    if (x == &memZero)
	return;			/* check for putting memZero back */

    AFS_STATS(afs_stats_cmperf.OutStandingAllocs--);
    AFS_STATS(afs_stats_cmperf.OutStandingMemUsage -= asize);
#if defined(AFS_LINUX20_ENV)
    osi_linux_free(x);
#elif defined(AFS_FBSD_ENV)
    osi_fbsd_free(x);
#else
    AFS_KFREE((struct osimem *)x, asize);
#endif
}

void
afs_osi_FreeStr(char *x)
{
    afs_osi_Free(x, strlen(x) + 1);
}

/* ? is it moderately likely that there are dirty VM pages associated with
 * this vnode?
 *
 *  Prereqs:  avc must be write-locked
 *
 *  System Dependencies:  - *must* support each type of system for which
 *                          memory mapped files are supported, even if all
 *                          it does is return TRUE;
 *
 * NB:  this routine should err on the side of caution for ProcessFS to work
 *      correctly (or at least, not to introduce worse bugs than already exist)
 */
#ifdef	notdef
int
osi_VMDirty_p(struct vcache *avc)
{
    int dirtyPages;

    if (avc->execsOrWriters <= 0)
	return 0;		/* can't be many dirty pages here, I guess */

#if defined (AFS_AIX32_ENV)
#ifdef	notdef
    /* because of the level of hardware involvment with VM and all the
     * warnings about "This routine must be called at VMM interrupt
     * level", I thought it would be safest to disable interrupts while
     * looking at the software page fault table.  */

    /* convert vm handle into index into array:  I think that stoinio is
     * always zero...  Look into this XXX  */
#define VMHASH(handle) ( \
			( ((handle) & ~vmker.stoinio)  \
			 ^ ((((handle) & ~vmker.stoinio) & vmker.stoimask) << vmker.stoihash) \
			 ) & 0x000fffff)

    if (avc->segid) {
	unsigned int pagef, pri, index, next;

	index = VMHASH(avc->segid);
	if (scb_valid(index)) {	/* could almost be an ASSERT */

	    pri = disable_ints();
	    for (pagef = scb_sidlist(index); pagef >= 0; pagef = next) {
		next = pft_sidfwd(pagef);
		if (pft_modbit(pagef)) {	/* has page frame been modified? */
		    enable_ints(pri);
		    return 1;
		}
	    }
	    enable_ints(pri);
	}
    }
#undef VMHASH
#endif
#endif /* AFS_AIX32_ENV */

#if defined (AFS_SUN5_ENV)
    if (avc->states & CMAPPED) {
	struct page *pg;
	for (pg = avc->v.v_s.v_Pages; pg; pg = pg->p_vpnext) {
	    if (pg->p_mod) {
		return 1;
	    }
	}
    }
#endif
    return 0;
}
#endif /* notdef */


/*
 * Solaris osi_ReleaseVM should not drop and re-obtain the vcache entry lock.
 * This leads to bad races when osi_ReleaseVM() is called from
 * afs_InvalidateAllSegments().

 * We can do this because Solaris osi_VM_Truncate() doesn't care whether the
 * vcache entry lock is held or not.
 *
 * For other platforms, in some cases osi_VM_Truncate() doesn't care, but
 * there may be cases where it does care.  If so, it would be good to fix
 * them so they don't care.  Until then, we assume the worst.
 *
 * Locking:  the vcache entry lock is held.  It is dropped and re-obtained.
 */
void
osi_ReleaseVM(struct vcache *avc, struct AFS_UCRED *acred)
{
#ifdef	AFS_SUN5_ENV
    AFS_GUNLOCK();
    osi_VM_Truncate(avc, 0, acred);
    AFS_GLOCK();
#else
    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    osi_VM_Truncate(avc, 0, acred);
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 80);
#endif
}


void
shutdown_osi(void)
{
    AFS_STATCNT(shutdown_osi);
#ifdef AFS_DARWIN80_ENV
    if (afs_osi_ctxtp_initialized && afs_osi_ctxtp) {
       vfs_context_rele(afs_osi_ctxtp);
       afs_osi_ctxtp = NULL;
       afs_osi_ctxtp_initialized = 0;
    }
    shutdown_osisleep();
#endif
    if (afs_cold_shutdown) {
	LOCK_INIT(&afs_ftf, "afs_ftf");
    }
}

#ifndef AFS_OBSD_ENV
int
afs_osi_suser(void *credp)
{
#if defined(AFS_SUN5_ENV)
    return afs_suser(credp);
#else
    return afs_suser(NULL);
#endif
}
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
extern rwlock_t tasklist_lock __attribute__((weak));
void
afs_osi_TraverseProcTable()
{
    struct task_struct *p;
    if (&tasklist_lock)
       read_lock(&tasklist_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    else
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
    if (&tasklist_lock)
       read_unlock(&tasklist_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    else
	rcu_read_unlock();
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
