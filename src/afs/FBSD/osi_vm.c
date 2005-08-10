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
 * osi_VM_FlushVCache(avc, slept)
 * osi_ubc_flush_dirty_and_wait(vp, flags)
 * osi_VM_StoreAllSegments(avc)
 * osi_VM_TryToSmush(avc, acred, sync)
 * osi_VM_FlushPages(avc, credp)
 * osi_VM_Truncate(avc, alen, acred)
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/FBSD/osi_vm.c,v 1.11.2.2 2005/05/23 21:23:53 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <limits.h>
#include <float.h>

/*
 * FreeBSD implementation notes:
 * Most of these operations require us to frob vm_objects.  Most
 * functions require that the object be locked (with VM_OBJECT_LOCK)
 * on entry and leave it locked on exit.  In order to get the
 * vm_object itself we call VOP_GETVOBJECT on the vnode; the
 * locking protocol requires that we do so with the heavy vnode lock
 * held and the vnode interlock unlocked, and it returns the same
 * way.
 *
 * The locking protocol for vnodes is defined in
 * kern/vnode_if.src and sys/vnode.h; the locking is still a work in 
 * progress, so some fields are (as of 5.1) still protected by Giant
 * rather than an explicit lock.
 */

#ifdef AFS_FBSD60_ENV
#define VOP_GETVOBJECT(vp, objp) (*(objp) = (vp)->v_object)
#endif

#ifdef AFS_FBSD50_ENV
#define	lock_vnode(v)	vn_lock((v), LK_EXCLUSIVE | LK_RETRY, curthread)
#define unlock_vnode(v)	VOP_UNLOCK((v), 0, curthread)
#else
#define	lock_vnode(v)	vn_lock((v), LK_EXCLUSIVE | LK_RETRY, curproc)
#define unlock_vnode(v)	VOP_UNLOCK((v), 0, curproc)
/* need splvm() protection? */
#define	VM_OBJECT_LOCK(o)
#define VM_OBJECT_UNLOCK(o)
#endif

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
 * XXX - should FreeBSD have done this, too?  Certainly looks like it.
 * Maybe better to just call vnode_pager_setsize()?
 */
int
osi_VM_FlushVCache(struct vcache *avc, int *slept)
{
    struct vm_object *obj;
    struct vnode *vp;
    if (VREFCOUNT(avc) > 1)
	return EBUSY;

    if (avc->opens)
	return EBUSY;

    /* if a lock is held, give up */
    if (CheckLock(&avc->lock))
	return EBUSY;

    AFS_GUNLOCK();
    vp = AFSTOV(avc);
    lock_vnode(vp);
    if (VOP_GETVOBJECT(vp, &obj) == 0) {
	VM_OBJECT_LOCK(obj);
	vm_object_page_remove(obj, 0, 0, FALSE);
#if 0
	if (obj->ref_count == 0) {
	    vgonel(vp, curproc);
	    simple_lock(&vp->v_interlock);
	    vp->v_tag = VT_AFS;
	    SetAfsVnode(vp);
	}
#endif
	VM_OBJECT_UNLOCK(obj);
    }
    unlock_vnode(vp);
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
    struct vnode *vp;
    struct vm_object *obj;
    int anyio, tries;

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    tries = 5;
    vp = AFSTOV(avc);

    /*
     * I don't understand this.  Why not just call vm_object_page_clean()
     * and be done with it?  I particularly don't understand why we're calling
     * vget() here.  Is there some reason to believe that the vnode might
     * be being recycled at this point?  I don't think there's any need for
     * this loop, either -- if we keep the vnode locked all the time,
     * that and the object lock will prevent any new pages from appearing.
     * The loop is what causes the race condition.  -GAW
     */
    do {
	anyio = 0;
	lock_vnode(vp);
	if (VOP_GETVOBJECT(vp, &obj) == 0 && (obj->flags & OBJ_MIGHTBEDIRTY)) {
	    /* XXX - obj locking? */
	    unlock_vnode(vp);
#ifdef AFS_FBSD50_ENV
	    if (!vget(vp, LK_EXCLUSIVE | LK_RETRY, curthread)) {
#else
	    if (!vget(vp, LK_EXCLUSIVE | LK_RETRY | LK_NOOBJ, curproc)) {
#endif
		if (VOP_GETVOBJECT(vp, &obj) == 0) {
		    VM_OBJECT_LOCK(obj);
		    vm_object_page_clean(obj, 0, 0, OBJPC_SYNC);
		    VM_OBJECT_UNLOCK(obj);
		    anyio = 1;
		}
		vput(vp);
	    }
	} else
	    unlock_vnode(vp);
    } while (anyio && (--tries > 0));
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
    struct vnode *vp;
    struct vm_object *obj;
    int anyio, tries;

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();
    tries = 5;
    vp = AFSTOV(avc);
    do {
	anyio = 0;
	lock_vnode(vp);
	/* See the comments above. */
	if (VOP_GETVOBJECT(vp, &obj) == 0 && (obj->flags & OBJ_MIGHTBEDIRTY)) {
	    /* XXX - obj locking */
	    unlock_vnode(vp);
#ifdef AFS_FBSD50_ENV
	    if (!vget(vp, LK_EXCLUSIVE | LK_RETRY, curthread)) {
#else
	    if (!vget(vp, LK_EXCLUSIVE | LK_RETRY | LK_NOOBJ, curproc)) {
#endif
		if (VOP_GETVOBJECT(vp, &obj) == 0) {
		    VM_OBJECT_LOCK(obj);
		    /*
		     * Do we really want OBJPC_SYNC?  OBJPC_INVAL would be
		     * faster, if invalidation is really what we are being
		     * asked to do.  (It would make more sense, too, since
		     * otherwise this function is practically identical to
		     * osi_VM_StoreAllSegments().)  -GAW
		     */
		    vm_object_page_clean(obj, 0, 0, OBJPC_SYNC);
		    VM_OBJECT_UNLOCK(obj);
		    anyio = 1;
		}
		vput(vp);
	    }
	} else
	    unlock_vnode(vp);
    } while (anyio && (--tries > 0));
    lock_vnode(vp);
    if (VOP_GETVOBJECT(vp, &obj) == 0) {
	VM_OBJECT_LOCK(obj);
	vm_object_page_remove(obj, 0, 0, FALSE);
	VM_OBJECT_UNLOCK(obj);
    }
    unlock_vnode(vp);
    /*vinvalbuf(AFSTOV(avc),0, NOCRED, curproc, 0,0); */
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
    struct vnode *vp;
    struct vm_object *obj;

    vp = AFSTOV(avc);
    ASSERT_VOP_LOCKED(vp, __func__);
    if (VOP_GETVOBJECT(vp, &obj) == 0) {
	VM_OBJECT_LOCK(obj);
	vm_object_page_remove(obj, 0, 0, FALSE);
	VM_OBJECT_UNLOCK(obj);
    }
    /*vinvalbuf(AFSTOV(avc),0, NOCRED, curproc, 0,0); */
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
    vnode_pager_setsize(AFSTOV(avc), alen);
}
