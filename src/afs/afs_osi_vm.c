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
