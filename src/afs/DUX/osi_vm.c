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
    ("$Header: /cvs/openafs/src/afs/DUX/Attic/osi_vm.c,v 1.8 2003/07/15 23:14:19 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include <vm/vm_ubc.h>
#include <values.h>

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
    if (avc->vrefCount > 1)
	return EBUSY;

    if (avc->opens)
	return EBUSY;

    /* if a lock is held, give up */
    if (CheckLock(&avc->lock) || afs_CheckBozonLock(&avc->pvnLock))
	return EBUSY;

    AFS_GUNLOCK();
    ubc_invalidate(AFSTOV(avc)->v_object, 0, 0, B_INVAL);
    AFS_GLOCK();

    return 0;
}

/*
 * osi_ubc_flush_dirty_and_wait -- ensure all dirty pages cleaned
 *
 * Alpha OSF/1 doesn't make it easy to wait for all dirty pages to be cleaned.
 * NFS tries to do this by calling waitforio(), which waits for v_numoutput
 * to go to zero.  But that isn't good enough, because afs_putpage() doesn't
 * increment v_numoutput until it has obtained the vcache entry lock.  Suppose
 * that Process A, trying to flush a page, is waiting for that lock, and
 * Process B tries to close the file.  Process B calls waitforio() which thinks
 * that everything is cool because v_numoutput is still zero.  Process B then
 * proceeds to call afs_StoreAllSegments().  Finally when B is finished, A gets
 * to proceed and flush its page.  But then it's too late because the file is
 * already closed.
 *
 * (I suspect that waitforio() is not adequate for NFS, just as it isn't
 * adequate for us.  But that's not my problem.)
 *
 * The only way we can be sure that there are no more dirty pages is if there
 * are no more pages with pg_busy set.  We look for them on the cleanpl.
 *
 * For some reason, ubc_flush_dirty() only looks at the dirtypl, not the
 * dirtywpl.  I don't know why this is good enough, but I assume it is.  By
 * the same token, I only look for busy pages on the cleanpl, not the cleanwpl.
 *
 * Called with the global lock NOT held.
 */
static void
osi_ubc_flush_dirty_and_wait(struct vnode *vp, int flags)
{
    int retry;
    vm_page_t pp;
    int first;

    do {
	struct vm_ubc_object *vop;
	vop = (struct vm_ubc_object *)(vp->v_object);
	ubc_flush_dirty(vop, flags);

	vm_object_lock(vop);
	if (vop->vu_dirtypl)
	    /* shouldn't happen, but who knows */
	    retry = 1;
	else {
	    retry = 0;
	    if (vop->vu_cleanpl) {
		for (first = 1, pp = vop->vu_cleanpl;
		     first || pp != vop->vu_cleanpl;
		     first = 0, pp = pp->pg_onext) {
		    if (pp->pg_busy) {
			retry = 1;
			pp->pg_wait = 1;
			assert_wait_mesg((vm_offset_t) pp, FALSE, "pg_wait");
			vm_object_unlock(vop);
			thread_block();
			break;
		    }
		}
	    }
	    if (retry)
		continue;
	}
	vm_object_unlock(vop);
    } while (retry);
}

/* Try to store pages to cache, in order to store a file back to the server.
 *
 * Locking:  the vcache entry's lock is held.  It will usually be dropped and
 * re-obtained.
 */
void
osi_VM_StoreAllSegments(struct vcache *avc)
{
    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    osi_ubc_flush_dirty_and_wait(AFSTOV(avc), 0);
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
    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    osi_ubc_flush_dirty_and_wait(AFSTOV(avc), 0);
    ubc_invalidate(AFSTOV(avc)->v_object, 0, 0, B_INVAL);
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 59);
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(struct vcache *avc, struct AFS_UCRED *credp)
{
    ubc_flush_dirty(AFSTOV(avc)->v_object, 0);
    ubc_invalidate(AFSTOV(avc)->v_object, 0, 0, B_INVAL);
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
    ubc_invalidate(AFSTOV(avc)->v_object, alen, MAXINT - alen, B_INVAL);
}
