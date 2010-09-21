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

#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */

int
osi_TryEvictVCache(struct vcache *avc, int *slept) {
    struct vnode *vp = AFSTOV(avc);

    if (!VREFCOUNT_GT(avc,0)
        && avc->opens == 0 && (avc->f.states & CUnlinkedDel) == 0) {
	/*
         * vgone() reclaims the vnode, which calls afs_FlushVCache(),
         * then it puts the vnode on the free list.
         * If we don't do this we end up with a cleaned vnode that's
         * not on the free list.
         * XXX assume FreeBSD is the same for now.
         */
	/*
	 * We only have one caller (afs_ShakeLooseVCaches), which already
	 * holds the write lock.  vgonel() sometimes calls VOP_CLOSE(),
	 * so we must drop the write lock around our call to vgone().
	 */
	ReleaseWriteLock(&afs_xvcache);
        AFS_GUNLOCK();
	*slept = 1;

#if defined(AFS_FBSD80_ENV)
	/* vgone() is correct, but vgonel() panics if v_usecount is 0--
         * this is particularly confusing since vgonel() will trigger
         * vop_reclaim, in the call path of which we'll check v_usecount
         * and decide that the vnode is busy.  Splat. */
	if (vrefcnt(vp) < 1)
	    vref(vp);

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY); /* !glocked */
#endif
        vgone(vp);
#if defined(AFS_FBSD80_ENV)
	VOP_UNLOCK(vp, 0);
#endif

	AFS_GLOCK();
	ObtainWriteLock(&afs_xvcache, 340);
	return 1;
    }
    return 0;
}

struct vcache *
osi_NewVnode(void) {
    struct vcache *tvc;

    tvc = (struct vcache *)afs_osi_Alloc(sizeof(struct vcache));
    tvc->v = NULL; /* important to clean this, or use memset 0 */

    return tvc;
}

void
osi_PrePopulateVCache(struct vcache *avc) {
    memset(avc, 0, sizeof(struct vcache));
}

void
osi_AttachVnode(struct vcache *avc, int seq) {
    struct vnode *vp;

    ReleaseWriteLock(&afs_xvcache);
    AFS_GUNLOCK();
#if defined(AFS_FBSD60_ENV)
    if (getnewvnode(MOUNT_AFS, afs_globalVFS, &afs_vnodeops, &vp))
#else
    if (getnewvnode(MOUNT_AFS, afs_globalVFS, afs_vnodeop_p, &vp))
#endif
	panic("afs getnewvnode");	/* can't happen */
#ifdef AFS_FBSD70_ENV
    /* XXX verified on 80--TODO check on 7x */
    if (!vp->v_mount) {
        vn_lock(vp, LK_EXCLUSIVE | LK_RETRY); /* !glocked */
        insmntque(vp, afs_globalVFS);
        VOP_UNLOCK(vp, 0);
    }
#endif
    AFS_GLOCK();
    ObtainWriteLock(&afs_xvcache,339);
    if (avc->v != NULL) {
        /* I'd like to know if this ever happens...
         * We don't drop global for the rest of this function,
         * so if we do lose the race, the other thread should
         * have found the same vnode and finished initializing
         * the vcache entry.  Is it conceivable that this vcache
         * entry could be recycled during this interval?  If so,
         * then there probably needs to be some sort of additional
         * mutual exclusion (an Embryonic flag would suffice).
         * -GAW */
	afs_warn("afs_NewVCache: lost the race\n");
	return;
    }
    avc->v = vp;
    avc->v->v_data = avc;
    lockinit(&avc->rwlock, PINOD, "vcache", 0, 0);
}

void
osi_PostPopulateVCache(struct vcache *avc) {
    avc->v->v_mount = afs_globalVFS;
    vSetType(avc, VREG);
}

