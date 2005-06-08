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
#include "afs/afs_stats.h"	/* statistics */
#include <sys/ubc.h>

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
 *
 * OSF/1 Locking:  VN_LOCK has been called.
 */
int
osi_VM_FlushVCache(struct vcache *avc, int *slept)
{
    struct vnode *vp = AFSTOV(avc);
    kern_return_t kret;
    off_t size;

    AFS_GUNLOCK();
#if 0
    if (!(UBCINFOMISSING(vp) || UBCINFORECLAIMED(vp))) {
      size=ubc_getsize(vp);
      kret=ubc_invalidate(vp,0,size); 
    }
#endif
    cache_purge(vp);
    AFS_GLOCK();

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
    struct vnode *vp = AFSTOV(avc);
    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
#ifdef AFS_DARWIN80_ENV
    ubc_sync_range(vp, 0, ubc_getsize(vp), UBC_SYNC|UBC_PUSHDIRTY);
#else
    if (UBCINFOEXISTS(vp)) {
	ubc_pushdirty(vp);
    }
#endif
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 94);
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
osi_VM_TryToSmush(struct vcache *avc, struct AFS_UCRED *acred, int sync)
{
    struct vnode *vp = AFSTOV(avc);
    void *object;
    kern_return_t kret;
    off_t size, lastpg;

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
#ifdef AFS_DARWIN80_ENV
    ubc_sync_range(vp, 0, ubc_getsize(vp), UBC_INVALIDATE);
#else
    if (UBCINFOEXISTS(vp)) {
	size = ubc_getsize(vp);
	kret = ubc_invalidate(vp, 0, size);
	if (kret != 1)		/* should be KERN_SUCCESS */
	    printf("TryToSmush: invalidate failed (error = %d)\n", kret);
    }
#endif
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 59);
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
/* XXX this seems to not be referenced anywhere. *somebody* ought to be calling
   this, and also making sure that ubc's idea of the filesize is right more
   often */
void
osi_VM_FlushPages(struct vcache *avc, struct AFS_UCRED *credp)
{
    struct vnode *vp = AFSTOV(avc);
    void *object;
    kern_return_t kret;
    off_t size;
#ifdef AFS_DARWIN80_ENV
    ubc_sync_range(vp, 0, ubc_getsize(vp), UBC_INVALIDATE);
	/* XXX what about when not CStatd */
    if (avc->states & CStatd && size != avc->m.Length)
       ubc_setsize(vp, avc->m.Length);
#else
    if (UBCINFOEXISTS(vp)) {
	size = ubc_getsize(vp);
	kret = ubc_invalidate(vp, 0, size);
	if (kret != 1)		/* Should be KERN_SUCCESS */
	    printf("VMFlushPages: invalidate failed (error = %d)\n", kret);
	/* XXX what about when not CStatd */
	if (avc->states & CStatd && size != avc->m.Length)
	  if (UBCISVALID(vp))
	    ubc_setsize(vp, avc->m.Length);
    }
#endif
}

/* Purge pages beyond end-of-file, when truncating a file.
 *
 * Locking:  no lock is held, not even the global lock.
 * activeV is raised.  This is supposed to block pageins, but at present
 * it only works on Solaris.
 */
void
osi_VM_Truncate(struct vcache *avc, int alen, struct AFS_UCRED *acred)
{
    struct vnode *vp = AFSTOV(avc);
#ifdef AFS_DARWIN80_ENV
    ubc_setsize(vp, alen);
#else
    if (UBCINFOEXISTS(vp) && UBCISVALID(vp)) {
	ubc_setsize(vp, alen);
    }
#endif
}

void
osi_VM_NukePages(struct vnode *vp, off_t offset, off_t size)
{
#if 0
    void *object;
    struct vcache *avc = VTOAFS(vp);

    offset = trunc_page(offset);
    size = round_page(size + 1);
    while (size) {
	ubc_page_op(vp, (vm_offset_t) offset,
		    UPL_POP_SET | UPL_POP_BUSY | UPL_POP_DUMP, 0, 0);
	size -= PAGE_SIZE;
	offset += PAGE_SIZE;
    }
#endif
}

int
osi_VM_Setup(struct vcache *avc, int force)
{
    int error;
    struct vnode *vp = AFSTOV(avc);

#ifndef AFS_DARWIN80_ENV
    if (UBCISVALID(vp) && ((avc->states & CStatd) || force)) {
	if (!UBCINFOEXISTS(vp)) {
	    osi_vnhold(avc, 0);
	    avc->states |= CUBCinit;
	    AFS_GUNLOCK();
	    if ((error = ubc_info_init(vp))) {
		AFS_GLOCK();
		avc->states &= ~CUBCinit;
		AFS_RELE(vp);
		return error;
	    }
	    AFS_GLOCK();
	    avc->states &= ~CUBCinit;
	    AFS_RELE(vp);
	}
	if (UBCINFOEXISTS(vp) && UBCISVALID(vp)) {
	    ubc_setsize(vp, avc->m.Length);
	}
    }
#endif
    return 0;
}
