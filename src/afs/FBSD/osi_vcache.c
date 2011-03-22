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

#if defined(AFS_FBSD80_ENV)
#define ma_vn_lock(vp, flags, p) (vn_lock(vp, flags))
#define MA_VOP_LOCK(vp, flags, p) (VOP_LOCK(vp, flags))
#define MA_VOP_UNLOCK(vp, flags, p) (VOP_UNLOCK(vp, flags))
#else
#define ma_vn_lock(vp, flags, p) (vn_lock(vp, flags, p))
#define MA_VOP_LOCK(vp, flags, p) (VOP_LOCK(vp, flags, p))
#define MA_VOP_UNLOCK(vp, flags, p) (VOP_UNLOCK(vp, flags, p))
#endif

int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {

    /*
     * essentially all we want to do here is check that the
     * vcache is not in use, then call vgone() (which will call
     * inactive and reclaim as needed).  This requires some
     * kind of complicated locking, which we already need to implement
     * for FlushVCache, so just call that routine here and check
     * its return value for whether the vcache was evict-able.
     */
    if (osi_VM_FlushVCache(avc, slept) != 0)
	return 0;
    else
	return 1;
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
    struct thread *p = curthread;

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
        ma_vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p); /* !glocked */
        insmntque(vp, afs_globalVFS);
        MA_VOP_UNLOCK(vp, 0, p);
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

