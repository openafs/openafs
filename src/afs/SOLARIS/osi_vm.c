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
#include "afs/nfsclient.h"

/* This file contains Solaris VM-related code for the cache manager. */

#include <sys/mman.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/rm.h>
#include <sys/modctl.h>
#include <sys/syscall.h>
#include <sys/debug.h>
#include <sys/fs_subr.h>

/* Try to invalidate pages, in order to recycle a dcache entry.
 *
 * This function only exists for Solaris.  For other platforms, it's OK to
 * recycle a dcache entry without invalidating pages, because the strategy
 * function can call afs_GetDCache().
 *
 * Locking:  only the global lock is held on entry.
 */
int
osi_VM_GetDownD(struct vcache *avc, struct dcache *adc)
{
    int code;

    AFS_GUNLOCK();
    code =
	afs_putpage(AFSTOV(avc), (offset_t) AFS_CHUNKTOBASE(adc->f.chunk),
		    AFS_CHUNKTOSIZE(adc->f.chunk), B_INVAL, CRED());
    AFS_GLOCK();

    return code;
}

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
    if (avc->vrefCount != 0)
	return EBUSY;

    if (avc->opens)
	return EBUSY;

    /* if a lock is held, give up */
    if (CheckLock(&avc->lock))
	return EBUSY;
    if (afs_CheckBozonLock(&avc->pvnLock))
	return EBUSY;

    AFS_GUNLOCK();
    pvn_vplist_dirty(AFSTOV(avc), 0, NULL, B_TRUNC | B_INVAL, CRED());
    AFS_GLOCK();

    /* Might as well make the obvious check */
    if (AFSTOV(avc)->v_pages)
	return EBUSY;		/* should be all gone still */

    rw_destroy(&avc->rwlock);
    if (avc->credp) {
	crfree(avc->credp);
	avc->credp = NULL;
    }


    return 0;
}

/* Try to store pages to cache, in order to store a file back to the server.
 *
 * Locking:  the vcache entry's lock is held.  It will usually be dropped and
 * re-obtained.
 */
void
osi_VM_StoreAllSegments(struct vcache *avc)
{
    AFS_GUNLOCK();
#if	defined(AFS_SUN56_ENV)
    (void)pvn_vplist_dirty(AFSTOV(avc), (u_offset_t) 0, afs_putapage, 0,
			   CRED());
#else
    (void)pvn_vplist_dirty(AFSTOV(avc), 0, afs_putapage, 0, CRED());
#endif
    AFS_GLOCK();
}

/* Try to invalidate pages, for "fs flush" or "fs flushv"; or
 * try to free pages, when deleting a file.
 *
 * Locking:  the vcache entry's lock is held.  It may be dropped and
 * re-obtained.
 */
void
osi_VM_TryToSmush(struct vcache *avc, struct AFS_UCRED *acred, int sync)
{
    AFS_GUNLOCK();
#if	defined(AFS_SUN56_ENV)
    (void)pvn_vplist_dirty(AFSTOV(avc), (u_offset_t) 0, afs_putapage,
			   (sync ? B_INVAL : B_FREE), acred);
#else
    (void)pvn_vplist_dirty(AFSTOV(avc), 0, afs_putapage,
			   (sync ? B_INVAL : B_FREE), acred);
#endif
    AFS_GLOCK();
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(struct vcache *avc, struct AFS_UCRED *credp)
{
    extern int afs_pvn_vptrunc;

    afs_pvn_vptrunc++;
    (void)afs_putpage(AFSTOV(avc), (offset_t) 0, 0, B_TRUNC | B_INVAL, credp);
}

/* Zero no-longer-used part of last page, when truncating a file
 *
 * This function only exists for Solaris.  Other platforms do not support it.
 *
 * Locking:  the vcache entry lock is held.  It is released and re-obtained.
 * The caller will raise activeV (to prevent pageins), but this function must
 * be called first, since it causes a pagein.
 */
void
osi_VM_PreTruncate(struct vcache *avc, int alen, struct AFS_UCRED *acred)
{
    page_t *pp;
    int pageOffset = (alen & PAGEOFFSET);

    if (pageOffset == 0) {
	return;
    }

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    pp = page_lookup(AFSTOV(avc), alen - pageOffset, SE_EXCL);
    if (pp) {
	pagezero(pp, pageOffset, PAGESIZE - pageOffset);
	page_unlock(pp);
    }
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 563);
}

/* Purge pages beyond end-of-file, when truncating a file.
 *
 * Locking:  no lock is held, not even the global lock.
 * Pageins are blocked (activeV is raised).
 */
void
osi_VM_Truncate(struct vcache *avc, int alen, struct AFS_UCRED *acred)
{
    /*
     * It's OK to specify afs_putapage here, even though we aren't holding
     * the vcache entry lock, because it isn't going to get called.
     */
    pvn_vplist_dirty(AFSTOV(avc), alen, afs_putapage, B_TRUNC | B_INVAL,
		     acred);
}
