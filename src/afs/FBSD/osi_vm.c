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
#include <sys/param.h>
#include <sys/vnode.h>


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <sys/limits.h>
#if __FreeBSD_version >= 1000030
#include <sys/rwlock.h>
#endif

/*
 * FreeBSD implementation notes:
 * Most of these operations require us to frob vm_objects.  Most
 * functions require that the object be locked (with VM_OBJECT_*LOCK)
 * on entry and leave it locked on exit.  The locking protocol
 * requires that we access vp->v_object with the heavy vnode lock
 * held and the vnode interlock unlocked.
 *
 * The locking protocol for vnodes is defined in
 * kern/vnode_if.src and sys/vnode.h; unfortunately, it is not *quite*
 * constant from version to version so to be properly correct we must
 * check the VCS history of those files.
 */

#define	lock_vnode(v, f)	vn_lock((v), (f))
#define ilock_vnode(v)	vn_lock((v), LK_INTERLOCK|LK_EXCLUSIVE|LK_RETRY)
#define unlock_vnode(v)	VOP_UNLOCK((v), 0)
#define islocked_vnode(v)	VOP_ISLOCKED((v))

#if __FreeBSD_version >= 1000030
#define AFS_VM_OBJECT_WLOCK(o)	VM_OBJECT_WLOCK(o)
#define AFS_VM_OBJECT_WUNLOCK(o)	VM_OBJECT_WUNLOCK(o)
#else
#define AFS_VM_OBJECT_WLOCK(o)	VM_OBJECT_LOCK(o)
#define AFS_VM_OBJECT_WUNLOCK(o)	VM_OBJECT_UNLOCK(o)
#endif

/* Try to discard pages, in order to recycle a vcache entry.
 *
 * We also make some sanity checks:  ref count, open count, held locks.
 *
 * We also do some non-VM-related chores, such as releasing the cred pointer
 * (for AIX and Solaris) and releasing the gnode (for AIX).
 *
 * Locking:  afs_xvcache lock is held. It must not be dropped.
 *
 */
int
osi_VM_FlushVCache(struct vcache *avc)
{
    struct vnode *vp;
    int code;

    vp = AFSTOV(avc);

    VI_LOCK(vp);
    code = osi_fbsd_checkinuse(avc);
    if (code) {
	VI_UNLOCK(vp);
	return code;
    }

    /* must hold the vnode before calling cache_purge()
     * This code largely copied from vfs_subr.c:vlrureclaim() */
    vholdl(vp);
    VI_UNLOCK(vp);

    AFS_GUNLOCK();
    cache_purge(vp);
    AFS_GLOCK();

    vdrop(vp);

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
    struct vm_object *obj;

    vp = AFSTOV(avc);
    /*
     * VOP_ISLOCKED may return LK_EXCLOTHER here, since we may be running in a
     * BOP_STORE background operation, and so we're running in a different
     * thread than the actual syscall that has the vnode locked. So we cannot
     * just call ASSERT_VOP_LOCKED (since that will fail if VOP_ISLOCKED
     * returns LK_EXCLOTHER), and instead we just have our own assert here.
     */
    osi_Assert(VOP_ISLOCKED(vp) != 0);

    obj = vp->v_object;

    if (obj != NULL && (obj->flags & OBJ_MIGHTBEDIRTY) != 0) {
	ReleaseWriteLock(&avc->lock);
	AFS_GUNLOCK();

	AFS_VM_OBJECT_WLOCK(obj);
	vm_object_page_clean(obj, 0, 0, OBJPC_SYNC);
	AFS_VM_OBJECT_WUNLOCK(obj);

	AFS_GLOCK();
	ObtainWriteLock(&avc->lock, 94);
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
    struct vnode *vp;
    int tries, code;
    int islocked;

    vp = AFSTOV(avc);

    VI_LOCK(vp);
    if (vp->v_iflag & VI_DOOMED) {
	VI_UNLOCK(vp);
	return;
    }
    VI_UNLOCK(vp);

    AFS_GUNLOCK();

    islocked = islocked_vnode(vp);
    if (islocked == LK_EXCLOTHER)
	panic("Trying to Smush over someone else's lock");
    else if (islocked == LK_SHARED) {
	afs_warn("Trying to Smush with a shared lock");
	lock_vnode(vp, LK_UPGRADE);
    } else if (!islocked)
	lock_vnode(vp, LK_EXCLUSIVE);

    if (vp->v_bufobj.bo_object != NULL) {
	AFS_VM_OBJECT_WLOCK(vp->v_bufobj.bo_object);
	/*
	 * Do we really want OBJPC_SYNC?  OBJPC_INVAL would be
	 * faster, if invalidation is really what we are being
	 * asked to do.  (It would make more sense, too, since
	 * otherwise this function is practically identical to
	 * osi_VM_StoreAllSegments().)  -GAW
	 */

	/*
	 * Dunno.  We no longer resemble osi_VM_StoreAllSegments,
	 * though maybe that's wrong, now.  And OBJPC_SYNC is the
	 * common thing in 70 file systems, it seems.  Matt.
	 */

	vm_object_page_clean(vp->v_bufobj.bo_object, 0, 0, OBJPC_SYNC);
	AFS_VM_OBJECT_WUNLOCK(vp->v_bufobj.bo_object);
    }

    tries = 5;
    code = osi_vinvalbuf(vp, V_SAVE, PCATCH, 0);
    while (code && (tries > 0)) {
	afs_warn("TryToSmush retrying vinvalbuf");
	code = osi_vinvalbuf(vp, V_SAVE, PCATCH, 0);
	--tries;
    }
    if (islocked == LK_SHARED)
	lock_vnode(vp, LK_DOWNGRADE);
    else if (!islocked)
	unlock_vnode(vp);

    AFS_GLOCK();
}

/* Purge VM for a file when its callback is revoked.
 *
 * Locking:  No lock is held, not even the global lock.
 */
void
osi_VM_FlushPages(struct vcache *avc, afs_ucred_t *credp)
{
    struct vnode *vp;
    struct vm_object *obj;

    vp = AFSTOV(avc);
    ASSERT_VOP_LOCKED(vp, __func__);
    obj = vp->v_object;
    if (obj != NULL) {
	AFS_VM_OBJECT_WLOCK(obj);
	vm_object_page_remove(obj, 0, 0, FALSE);
	AFS_VM_OBJECT_WUNLOCK(obj);
    }
    osi_vinvalbuf(vp, 0, 0, 0);
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
    vnode_pager_setsize(AFSTOV(avc), alen);
}
