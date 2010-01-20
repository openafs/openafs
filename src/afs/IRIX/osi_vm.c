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
#include "afs/afs_stats.h"	/* statistics */
#include "sys/flock.h"		/* for IGN_PID */

extern struct vnodeops Afs_vnodeops;

/* Try to discard pages, in order to recycle a vcache entry.
 *
 * We also make some sanity checks:  ref count, open count, held locks.
 *
 * We also do some non-VM-related chores, such as releasing the cred pointer
 * (for AIX and Solaris) and releasing the gnode (for AIX).
 *
 * Locking:  afs_xvcache lock is held.  If it is dropped and re-acquired,
 *   *slept should be set to warn the caller.
 *
 * Formerly, afs_xvcache was dropped and re-acquired for Solaris, but now it
 * is not dropped and re-acquired for any platform.  It may be that *slept is
 * therefore obsolescent.
 */
int
osi_VM_FlushVCache(struct vcache *avc, int *slept)
{
    int s, code;
    vnode_t *vp = &avc->v;

    if (avc->vrefCount != 0)
	return EBUSY;

    if (avc->opens != 0)
	return EBUSY;

    /*
     * Just in case someone is still referring to the vnode we give up
     * trying to get rid of this guy.
     */
    if (CheckLock(&avc->lock) || LockWaiters(&avc->lock))
	return EBUSY;

    s = VN_LOCK(vp);

    /*
     * we just need to avoid the race
     * in vn_rele between the ref count going to 0 and VOP_INACTIVE
     * finishing up.
     * Note that although we checked vcount above, we didn't have the lock
     */
    if (vp->v_count > 0 || (vp->v_flag & VINACT)) {
	VN_UNLOCK(vp, s);
	return EBUSY;
    }
    VN_UNLOCK(vp, s);

    /*
     * Since we store on last close and on VOP_INACTIVE
     * there should be NO dirty pages
     * Note that we hold the xvcache lock the entire time.
     */
    AFS_GUNLOCK();
    PTOSSVP(vp, (off_t) 0, (off_t) MAXLONG);
    AFS_GLOCK();

    /* afs_chkpgoob will drop and re-acquire the global lock. */
    afs_chkpgoob(vp, 0);
    osi_Assert(!VN_GET_PGCNT(vp));
    osi_Assert(!AFS_VN_MAPPED(vp));
    osi_Assert(!AFS_VN_DIRTY(&avc->v));

#if defined(AFS_SGI65_ENV)
    if (vp->v_filocks)
	cleanlocks(vp, IGN_PID, 0);
    mutex_destroy(&vp->v_filocksem);
#else /* AFS_SGI65_ENV */
    if (vp->v_filocksem) {
	if (vp->v_filocks)
#ifdef AFS_SGI64_ENV
	    cleanlocks(vp, &curprocp->p_flid);
#else
	    cleanlocks(vp, IGN_PID, 0);
#endif
	osi_Assert(vp->v_filocks == NULL);
	mutex_destroy(vp->v_filocksem);
	kmem_free(vp->v_filocksem, sizeof *vp->v_filocksem);
	vp->v_filocksem = NULL;
    }
#endif /* AFS_SGI65_ENV */

    if (avc->vrefCount)
	osi_Panic("flushVcache: vm race");
#ifdef AFS_SGI64_ENV
    AFS_GUNLOCK();
    vnode_pcache_reclaim(vp);	/* this can sleep */
    vnode_pcache_free(vp);
    if (vp->v_op != &Afs_vnodeops) {
	VOP_RECLAIM(vp, FSYNC_WAIT, code);
    }
    AFS_GLOCK();
#ifdef AFS_SGI65_ENV
#ifdef VNODE_TRACING
    ktrace_free(vp->v_trace);
#endif /* VNODE_TRACING */
    vn_bhv_remove(VN_BHV_HEAD(vp), &(avc->vc_bhv_desc));
    vn_bhv_head_destroy(&(vp->v_bh));
    destroy_bitlock(&vp->v_pcacheflag);
    mutex_destroy(&vp->v_buf_lock);
#else
    bhv_remove(VN_BHV_HEAD(vp), &(avc->vc_bhv_desc));
    bhv_head_destroy(&(vp->v_bh));
#endif
    vp->v_flag = 0;		/* debug */
#if defined(DEBUG) && defined(VNODE_INIT_BITLOCK)
    destroy_bitlock(&vp->v_flag);
#endif
#ifdef INTR_KTHREADS
    AFS_VN_DESTROY_BUF_LOCK(vp);
#endif
#endif /* AFS_SGI64_ENV */

    return 0;
}

/* Try to invalidate pages, for "fs flush" or "fs flushv"; or
 * try to free pages, when deleting a file.
 *
 * Locking:  the vcache entry's lock is held.  It may be dropped and 
 * re-obtained.
 *
 * Since we drop and re-obtain the lock, we can't guarantee that there won't
 * be some pages around when we return, newly created by concurrent activity.
 */
void
osi_VM_TryToSmush(struct vcache *avc, afs_ucred_t *acred, int sync)
{
    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    /* current remapf restriction - cannot have VOP_RWLOCK */
    osi_Assert(OSI_GET_LOCKID() != avc->vc_rwlockid);
    if (((vnode_t *) avc)->v_type == VREG && AFS_VN_MAPPED(((vnode_t *) avc)))
	remapf(((vnode_t *) avc), 0, 0);
    PTOSSVP(AFSTOV(avc), (off_t) 0, (off_t) MAXLONG);
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 62);
}

/* Flush and invalidate pages, for fsync() with INVAL flag
 *
 * Locking:  only the global lock is held.
 */
void
osi_VM_FSyncInval(struct vcache *avc)
{
    AFS_GUNLOCK();
    PFLUSHINVALVP((vnode_t *) avc, (off_t) 0, (off_t) avc->f.m.Length);
    AFS_GLOCK();
}

/* Try to store pages to cache, in order to store a file back to the server.
 *
 * Locking:  the vcache entry's lock is held.  It will usually be dropped and
 * re-obtained.
 */
void
osi_VM_StoreAllSegments(struct vcache *avc)
{
    int error;
    osi_Assert(valusema(&avc->vc_rwlock) <= 0);
    osi_Assert(OSI_GET_LOCKID() == avc->vc_rwlockid);
    osi_Assert(avc->vrefCount > 0);
    ReleaseWriteLock(&avc->lock);
    /* We may call back into AFS via:
     * pflushvp->chunkpush->do_pdflush->mp_afs_bmap
     */
    AFS_GUNLOCK();

    /* Write out dirty pages list to avoid B_DELWRI buffers. */
    while (VN_GET_DPAGES((vnode_t *) avc)) {
	pdflush(AFSTOV(avc), 0);
    }

    PFLUSHVP(AFSTOV(avc), (off_t) avc->f.m.Length, (off_t) 0, error);
    AFS_GLOCK();
    if (error) {
	/*
	 * If this fails (due to quota overage, etc.)
	 * what can we do?? we need to sure that
	 * that the VM cache is cleared of dirty pages
	 * We note that pinvalfree ignores write errors & otherwise
	 * does what we want (we don't use this normally since
	 * it also unhashes pages ..)
	 */
	PINVALFREE((vnode_t *) avc, avc->f.m.Length);
    }
    ObtainWriteLock(&avc->lock, 121);
    if (error && avc->f.m.LinkCount)
	cmn_err(CE_WARN,
		"AFS:Failed to push back pages for vnode 0x%x error %d (from afs_StoreOnLastReference)",
		avc, error);
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(struct vcache *avc, afs_ucred_t *credp)
{
    vnode_t *vp = (vnode_t *) avc;

    remapf(vp, /*avc->f.m.Length */ 0, 0);

    /* Used to grab locks and recheck avc->f.m.DataVersion and
     * avc->execsOrWriters here, but we have to drop locks before calling
     * ptossvp() anyway, so why bother.
     */

    /*
     * ptossvp tosses all pages associated with this vnode
     * All in-use pages are marked BAD
     */
    PTOSSVP(vp, (off_t) 0, (off_t) MAXLONG);
}

/* Purge pages beyond end-of-file, when truncating a file.
 *
 * Locking:  no lock is held, not even the global lock.
 * activeV is raised.  This is supposed to block pageins, but at present
 * it only works on Solaris.
 */
void
osi_VM_Truncate(struct vcache *avc, int alen, afs_ucred_t *acred)
{
    PTOSSVP(&avc->v, (off_t) alen, (off_t) MAXLONG);
}
