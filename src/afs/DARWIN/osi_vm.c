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
    ("$Header: /cvs/openafs/src/afs/DARWIN/osi_vm.c,v 1.14.2.1 2005/02/21 01:14:18 shadow Exp $");

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
#ifdef AFS_DARWIN14_ENV
    if (UBCINFOEXISTS(vp))
	return EBUSY;
#endif
    if (avc->vrefCount > DARWIN_REFBASE)
	return EBUSY;

    if (avc->opens)
	return EBUSY;

    /* if a lock is held, give up */
    if (CheckLock(&avc->lock) || afs_CheckBozonLock(&avc->pvnLock))
	return EBUSY;

    AFS_GUNLOCK();
    cache_purge(vp);
#ifndef AFS_DARWIN14_ENV
    if (UBCINFOEXISTS(vp)) {
	ubc_clean(vp, 1);
	ubc_uncache(vp);
	ubc_release(vp);
	ubc_info_free(vp);
    }
#else
    /* This is literally clean_up_name_parent_ptrs() */
    /* Critical to clean up any state attached to the vnode here since it's
       being recycled, and we're not letting refcnt drop to 0 to trigger
       normal recycling. */
    if (VNAME(vp) || VPARENT(vp)) {
	char *tmp1;
	struct vnode *tmp2;

	/* do it this way so we don't block before clearing 
	   these fields. */
	tmp1 = VNAME(vp);
	tmp2 = VPARENT(vp);
	VNAME(vp) = NULL;
	VPARENT(vp) = NULL;
            
	if (tmp1) {
	    remove_name(tmp1);
	}
            
	if (tmp2) {
	    vrele(tmp2);
	}
    }
#endif

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
    if (UBCINFOEXISTS(vp)) {
	ubc_pushdirty(vp);
    }
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
    if (UBCINFOEXISTS(vp)) {
	size = ubc_getsize(vp);
	kret = ubc_invalidate(vp, 0, size);
	if (kret != 1)		/* should be KERN_SUCCESS */
	    printf("TryToSmush: invalidate failed (error = %d)\n", kret);
    }
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
    if (UBCINFOEXISTS(vp)) {
	size = ubc_getsize(vp);
	kret = ubc_invalidate(vp, 0, size);
	if (kret != 1)		/* Should be KERN_SUCCESS */
	    printf("VMFlushPages: invalidate failed (error = %d)\n", kret);
	/* XXX what about when not CStatd */
	if (avc->states & CStatd && size != avc->m.Length)
	    ubc_setsize(vp, avc->m.Length);
    }
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
    if (UBCINFOEXISTS(vp)) {
	ubc_setsize(vp, alen);
    }
}

/* vnreclaim and vinactive are probably not aggressive enough to keep
   enough afs vcaches free, so we try to do some of it ourselves */
/* XXX there's probably not nearly enough locking here */
void
osi_VM_TryReclaim(struct vcache *avc, int *slept)
{
    struct proc *p = current_proc();
    struct vnode *vp = AFSTOV(avc);
    void *obj;
#ifdef AFS_DARWIN14_ENV
    int didhold;
#endif

    if (slept)
	*slept = 0;
    VN_HOLD(vp);		/* remove from inactive list */
    if (!simple_lock_try(&vp->v_interlock)) {
	AFS_RELE(vp);
	return;
    }
    if (!UBCINFOEXISTS(vp) || vp->v_usecount != 2+DARWIN_REFBASE) {
	simple_unlock(&vp->v_interlock);
	AFS_RELE(vp);
	return;
    }
    if (ISSET(vp->v_flag, VUINACTIVE)) {
	simple_unlock(&vp->v_interlock);
        AFS_RELE(vp);
        printf("vnode %x still inactive!", vp);
        return;
    }
#ifdef AFS_DARWIN14_ENV
    if (vp->v_ubcinfo->ui_refcount > 1 || vp->v_ubcinfo->ui_mapped) {
	simple_unlock(&vp->v_interlock);
	AFS_RELE(vp);
	return;
    }
    if (ISSET(vp->v_flag, VORECLAIM)) {
        simple_unlock(&vp->v_interlock);
        AFS_RELE(vp);
        return;
    }
#else
    if (vp->v_ubcinfo->ui_holdcnt) {
	simple_unlock(&vp->v_interlock);
	AFS_RELE(vp);
	return;
    }
#endif
    if (slept && ubc_issetflags(vp, UI_WASMAPPED)) {
	/* We can't possibly release this in time for this NewVCache to get it */
	simple_unlock(&vp->v_interlock);
	AFS_RELE(vp);
	return;
    }

#ifndef AFS_DARWIN14_ENV
    vp->v_usecount--;		/* we want the usecount to be 1 */
#endif

    if (slept) {
	ReleaseWriteLock(&afs_xvcache);
	*slept = 1;
    } else
	ReleaseReadLock(&afs_xvcache);
    AFS_GUNLOCK();
    obj = 0;
    if (ubc_issetflags(vp, UI_WASMAPPED)) {
	simple_unlock(&vp->v_interlock);
#ifdef  AFS_DARWIN14_ENV
	ubc_release_named(vp);
#else
	ubc_release(vp);
#endif
	if (ubc_issetflags(vp, UI_HASOBJREF))
	    printf("ubc_release didn't release the reference?!\n");
    } else {
#ifdef AFS_DARWIN14_ENV
        SET(vp->v_flag, VORECLAIM);
#endif
	if (!vn_lock(vp, LK_EXCLUSIVE|LK_INTERLOCK,current_proc())) {
#ifdef AFS_DARWIN14_ENV
	    obj = ubc_getobject(vp,UBC_HOLDOBJECT);
	    if ((didhold = ubc_hold(vp)))
		(void)ubc_clean(vp, 0);
#else
#ifdef AFS_DARWIN13_ENV
	    obj = ubc_getobject(vp,(UBC_NOREACTIVATE|UBC_HOLDOBJECT));
#else
	    obj = ubc_getobject(vp);
#endif
	    (void)ubc_clean(vp, 1);
#endif
	    vinvalbuf(vp, V_SAVE, &afs_osi_cred, p, 0, 0);
	    if (vp->v_usecount ==
#ifdef AFS_DARWIN14_ENV
		2 + DARWIN_REFBASE
#else
		1
#endif
		)
	      VOP_UNLOCK(vp, 0, p); /* was VOP_INACTIVE(vp, p); */
	    else
		VOP_UNLOCK(vp, 0, p);
#ifdef AFS_DARWIN14_ENV
	    if (didhold)
		ubc_rele(vp);
#endif
	    if (obj) {
		if (ISSET(vp->v_flag, VTERMINATE))
		    panic("afs_vnreclaim: already terminating");
		SET(vp->v_flag, VTERMINATE);
		memory_object_destroy(obj, 0);
		while (ISSET(vp->v_flag, VTERMINATE)) {
		    SET(vp->v_flag, VTERMWANT);
		    tsleep((caddr_t)&vp->v_ubcinfo, PINOD, "afs_vnreclaim", 0);
		}
	    }
#ifdef AFS_DARWIN14_ENV
	    simple_lock(&vp->v_interlock);
	    CLR(vp->v_flag, VORECLAIM);
	    if (ISSET((vp)->v_flag, VXWANT)) {
		CLR((vp)->v_flag, VXWANT);
                wakeup((caddr_t)(vp));
	    }       
	    vp->v_usecount--;
	    simple_unlock(&vp->v_interlock);
#endif
	} else {
#ifdef AFS_DARWIN14_ENV
	    CLR(vp->v_flag, VORECLAIM);
#endif
	    if (simple_lock_try(&vp->v_interlock))
		panic("afs_vnreclaim: slept, but did no work :(");
	    if (UBCINFOEXISTS(vp) && vp->v_count == DARWIN_REFBASE +
#ifdef AFS_DARWIN14_ENV
		2
#else
		1
#endif
		) {
#ifndef AFS_DARWIN14_ENV
		/* We left the refcount high in 1.4 */
		vp->v_usecount++;
#endif
		simple_unlock(&vp->v_interlock);
		VN_RELE(vp);
	    } else {
#ifdef AFS_DARWIN14_ENV
		/* We left the refcount high in 1.4 */
		vp->v_usecount--;
#endif
		simple_unlock(&vp->v_interlock);
	    }
	}
    }
    AFS_GLOCK();
    if (slept)
        ObtainWriteLock(&afs_xvcache,175);
    else
        ObtainReadLock(&afs_xvcache);
}

void
osi_VM_NukePages(struct vnode *vp, off_t offset, off_t size)
{
    void *object;
    struct vcache *avc = VTOAFS(vp);

#ifdef AFS_DARWIN14_ENV
    offset = trunc_page(offset);
    size = round_page(size + 1);
    while (size) {
	ubc_page_op(vp, (vm_offset_t) offset,
		    UPL_POP_SET | UPL_POP_BUSY | UPL_POP_DUMP, 0, 0);
	size -= PAGE_SIZE;
	offset += PAGE_SIZE;
    }
#else
    object = NULL;
#ifdef AFS_DARWIN13_ENV
    if (UBCINFOEXISTS(vp))
	object = ubc_getobject(vp, UBC_NOREACTIVATE);
#else
    if (UBCINFOEXISTS(vp))
	object = ubc_getobject(vp);
#endif
    if (!object)
	return;

    offset = trunc_page(offset);
    size = round_page(size + 1);

#ifdef AFS_DARWIN13_ENV
    while (size) {
	memory_object_page_op(object, (vm_offset_t) offset,
			      UPL_POP_SET | UPL_POP_BUSY | UPL_POP_DUMP, 0,
			      0);
	size -= PAGE_SIZE;
	offset += PAGE_SIZE;
    }
#else
    /* This is all we can do, and it's not enough. sucks to be us */
    ubc_setsize(vp, offset);
    size = (offset + size > avc->m.Length) ? offset + size : avc->m.Length;
    ubc_setsize(vp, size);
#endif
#endif
}

int
osi_VM_Setup(struct vcache *avc, int force)
{
    int error;
    struct vnode *vp = AFSTOV(avc);

    if (UBCISVALID(vp) && ((avc->states & CStatd) || force)) {
	if (!UBCINFOEXISTS(vp) && !ISSET(vp->v_flag, VTERMINATE)) {
	    osi_vnhold(avc, 0);
	    avc->states |= CUBCinit;
	    AFS_GUNLOCK();
	    if ((error = ubc_info_init(&avc->v))) {
		AFS_GLOCK();
		avc->states &= ~CUBCinit;
		AFS_RELE(avc);
		return error;
	    }
#ifndef AFS_DARWIN14_ENV
	    simple_lock(&avc->v.v_interlock);
	    if (!ubc_issetflags(&avc->v, UI_HASOBJREF))
#ifdef AFS_DARWIN13_ENV
		if (ubc_getobject
		    (&avc->v, (UBC_NOREACTIVATE | UBC_HOLDOBJECT)))
		    panic("VM_Setup: null object");
#else
		(void)_ubc_getobject(&avc->v, 1);	/* return value not used */
#endif
	    simple_unlock(&avc->v.v_interlock);
#endif
	    AFS_GLOCK();
	    avc->states &= ~CUBCinit;
	    AFS_RELE(avc);
	}
	if (UBCINFOEXISTS(&avc->v))
	    ubc_setsize(&avc->v, avc->m.Length);
    }
    return 0;
}
