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

    /* Just in case someone is still referring to the vnode we
     * give up trying to get rid of this guy */
    if (avc->vrefCount || CheckLock(&avc->lock) || LockWaiters(&avc->lock))
	return EBUSY;

    if (avc->segid) {
	AFS_GUNLOCK();
	vms_delete(avc->segid);
	AFS_GLOCK();
	avc->segid = avc->vmh = NULL;
    }

    if (avc->credp) {
	crfree(avc->credp);
	avc->credp = NULL;
    }

    /* Free the alloced gnode that was accompanying the vcache's vnode */
    aix_gnode_rele(AFSTOV(avc));

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
    if (avc->segid) {
	/*
	 * The execsOrWriters test is done so that we don't thrash on
	 * the vm_writep call below. We only initiate a pageout of the
	 * dirty vm pages on the last store...
	 * this is strictly a pragmatic decision, and _does_ break the 
	 * advertised AFS consistency semantics.  Without this hack,
	 * AIX systems panic under heavy load.  I consider the current
	 * behavior a bug introduced to hack around a worse bug. XXX
	 *
	 * Removed do_writep governing sync'ing behavior.
	 */
	ReleaseWriteLock(&avc->lock);	/* XXX */
	AFS_GUNLOCK();
	vm_writep(avc->segid, 0, MAXFSIZE / PAGESIZE - 1);
	vms_iowait(avc->segid);
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 93);	/* XXX */
	/*
	 * The following is necessary because of the following
	 * asynchronicity: We open a file, write to it and 
	 * close the file
	 * if CCore flag is set, we clear it and do the extra
	 * decrement ourselves now.
	 * If we're called by the CCore clearer, the CCore flag
	 * will already be clear, so we don't have to worry about
	 * clearing it twice.
	 * avc was "VN_HELD" and "crheld" when CCore was set in
	 * afs_FakeClose
	 */
	if (avc->f.states & CCore) {
	    avc->f.states &= ~CCore;
	    avc->opens--;
	    avc->execsOrWriters--;
	    AFS_RELE(AFSTOV(avc));
	    crfree((struct ucred *)avc->linkData);
	    avc->linkData = NULL;
	}
    }
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
    if (avc->segid) {
	ReleaseWriteLock(&avc->lock);
	AFS_GUNLOCK();
	vm_flushp(avc->segid, 0, MAXFSIZE / PAGESIZE - 1);
	vms_iowait(avc->segid);	/* XXX Wait?? XXX */
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 60);
    }
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(struct vcache *avc, afs_ucred_t *credp)
{
    if (avc->segid) {
	vm_flushp(avc->segid, 0, MAXFSIZE / PAGESIZE - 1);
	/*
	 * XXX We probably don't need to wait but better be safe XXX
	 */
	vms_iowait(avc->segid);
    }
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
    if (avc->segid) {
	int firstpage = (alen + PAGESIZE - 1) / PAGESIZE;
	vm_releasep(avc->segid, firstpage, MAXFSIZE / PAGESIZE - firstpage);
	vms_iowait(avc->segid);	/* Do we need this? */
    }
}
