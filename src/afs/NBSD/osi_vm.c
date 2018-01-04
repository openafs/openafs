/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */


/* osi_vm.c implements:
 *
 * osi_VM_FlushVCache(avc)
 * osi_ubc_flush_dirty_and_wait(vp, flags)
 * osi_VM_StoreAllSegments(avc)
 * osi_VM_TryToSmush(avc, acred, sync)
 * osi_VM_FlushPages(avc, credp)
 * osi_VM_Truncate(avc, alen, acred)
 */

#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */

/* Try to discard pages, in order to recycle a vcache entry.
 *
 * We also make some sanity checks:  ref count, open count, held locks.
 *
 * We also do some non-VM-related chores, such as releasing the cred pointer
 * (for AIX and Solaris) and releasing the gnode (for AIX).
 *
 * Locking:  afs_xvcache lock is held. It must not be dropped.
 *
 * OSF/1 Locking:  VN_LOCK has been called.
 */
int
osi_VM_FlushVCache(struct vcache *avc)
{
    struct vnode *vp = AFSTOV(avc);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s enter\n", __func__);
    }

    if (vp == NULL) {
	printf("%s NULL vp\n", __func__);
	return 0;
    }

    AFS_GUNLOCK();
    cache_purge(vp);
    vflushbuf(vp, 1);
    AFS_GLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s exit\n", __func__);
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
    struct vnode *vp;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s enter\n", __func__);
    }

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    vp = AFSTOV(avc);
    mutex_enter(&vp->v_interlock);
    VOP_PUTPAGES(vp, 0, 0, PGO_ALLPAGES|PGO_CLEANIT|PGO_SYNCIO);
    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 94);
    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s exit\n", __func__);
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
    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s enter\n", __func__);
    }

    ReleaseWriteLock(&avc->lock);
    osi_VM_FlushVCache(avc);
    ObtainWriteLock(&avc->lock, 59);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s exit\n", __func__);
    }
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(struct vcache *avc, afs_ucred_t *credp)
{
    struct vnode *vp = AFSTOV(avc);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s enter\n", __func__);
    }

    if (!vp) {
	printf("%s NULL vp\n", __func__);
	return;
    }

    cache_purge(vp);
    vinvalbuf(vp, 0, credp, curlwp, false, 1);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s exit\n", __func__);
    }
}

/* Purge pages beyond end-of-file, when truncating a file.
 *
 * Locking:  no lock is held, not even the global lock.
 * activeV is raised.  This is supposed to block pageins, but at present
 * it only works on Solaris.
 */
void
osi_VM_Truncate(struct vcache *avc, voff_t alen, afs_ucred_t *acred)
{
    struct vnode *vp = AFSTOV(avc);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s enter\n", __func__);
    }

    vtruncbuf(vp, alen, false, 0);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("%s exit\n", __func__);
    }
}
