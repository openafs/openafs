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
#ifdef AFS_AIX_ENV
#include <sys/adspace.h>	/* for vm_att(), vm_det() */
#endif

int
osi_Active(struct vcache *avc)
{
    AFS_STATCNT(osi_Active);
#if defined(AFS_AIX_ENV) || defined(AFS_SUN5_ENV) || (AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if ((avc->opens > 0) || (avc->f.states & CMAPPED))
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
osi_FlushPages(struct vcache *avc, afs_ucred_t *credp)
{
    afs_hyper_t origDV;
#if defined(AFS_CACHE_BYPASS)
    /* The optimization to check DV under read lock below is identical a
     * change in CITI cache bypass work.  The problem CITI found in 1999
     * was that this code and background daemon doing prefetching competed
     * for the vcache entry shared lock.  It's not clear to me from the
     * tech report, but it looks like CITI fixed the general prefetch code
     * path as a bonus when experimenting on prefetch for cache bypass, see
     * citi-tr-01-3.
     */
#endif
    if (vType(avc) == VDIR) {
	/* not applicable to directories; they're never mapped or stored in
	 * pages */
	return;
    }
    ObtainReadLock(&avc->lock);
    /* If we've already purged this version, or if we're the ones
     * writing this version, don't flush it (could lose the
     * data we're writing). */
    if ((hcmp((avc->f.m.DataVersion), (avc->mapDV)) <= 0)
	|| ((avc->execsOrWriters > 0) && afs_DirtyPages(avc))) {
	ReleaseReadLock(&avc->lock);
	return;
    }
    ReleaseReadLock(&avc->lock);
    ObtainWriteLock(&avc->lock, 10);
    /* Check again */
    if ((hcmp((avc->f.m.DataVersion), (avc->mapDV)) <= 0)
	|| ((avc->execsOrWriters > 0) && afs_DirtyPages(avc))) {
	ReleaseWriteLock(&avc->lock);
	return;
    }

    /* At this point, you might think that we can skip trying to flush pages
     * if mapDV is zero, since a file with a zero DV will not have any data in
     * it. However, some platforms (notably Linux 2.6.22+) will keep a page
     * full of zeroes around for an empty file. So play it safe and always
     * flush pages. */

    AFS_STATCNT(osi_FlushPages);
    hset(origDV, avc->f.m.DataVersion);
    afs_Trace3(afs_iclSetp, CM_TRACE_FLUSHPAGES, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, origDV.low, ICL_TYPE_INT32, avc->f.m.Length);

    ReleaseWriteLock(&avc->lock);
#ifndef AFS_FBSD_ENV
    AFS_GUNLOCK();
#endif
    osi_VM_FlushPages(avc, credp);
#ifndef AFS_FBSD_ENV
    AFS_GLOCK();
#endif
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
osi_FlushText_really(struct vcache *vp)
{
    afs_hyper_t fdv;		/* version before which we'll flush */

    AFS_STATCNT(osi_FlushText);
    /* see if we've already flushed this data version */
    if (hcmp(vp->f.m.DataVersion, vp->flushDV) <= 0)
	return;

    ObtainWriteLock(&afs_ftf, 317);
    hset(fdv, vp->f.m.DataVersion);

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
	    ReleaseWriteLock(&afs_ftf);
	    return;
	}
    }
#endif

    /* next do the stuff that need not check for deadlock problems */
    mpurge(vp);

    /* finally, record that we've done it */
    hset(vp->flushDV, fdv);
    ReleaseWriteLock(&afs_ftf);

}
#endif /* AFS_TEXT_ENV */

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
osi_ReleaseVM(struct vcache *avc, afs_ucred_t *acred)
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
