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
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep)
{
    struct vnode *vp;
    int code;

    vp = AFSTOV(avc);

    if (!VI_TRYLOCK(vp))
	return 0;
    code = osi_fbsd_checkinuse(avc);
    if (code != 0) {
	VI_UNLOCK(vp);
	return 0;
    }

    if ((vp->v_iflag & VI_DOOMED) != 0) {
	VI_UNLOCK(vp);
	return 1;
    }

    /* must hold the vnode before calling vgone()
     * This code largely copied from vfs_subr.c:vlrureclaim() */
    vholdl(vp);

    ReleaseWriteLock(&afs_xvcache);
    AFS_GUNLOCK();

    *slept = 1;
    /* use the interlock while locking, so no one else can DOOM this */
    vn_lock(vp, LK_INTERLOCK|LK_EXCLUSIVE|LK_RETRY);
    vgone(vp);
    VOP_UNLOCK(vp, 0);
    vdrop(vp);

    AFS_GLOCK();
    ObtainWriteLock(&afs_xvcache, 340);
    return 1;
}

struct vcache *
osi_NewVnode(void) {
    struct vcache *tvc;

    tvc = afs_osi_Alloc(sizeof(struct vcache));
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
    if (getnewvnode(MOUNT_AFS, afs_globalVFS, &afs_vnodeops, &vp))
	panic("afs getnewvnode");	/* can't happen */
    /* XXX verified on 80--TODO check on 7x */
    if (!vp->v_mount) {
        vn_lock(vp, LK_EXCLUSIVE | LK_RETRY); /* !glocked */
        insmntque(vp, afs_globalVFS);
        VOP_UNLOCK(vp, 0);
    }
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

int
osi_vnhold(struct vcache *avc)
{
    vref(AFSTOV(avc));
    return 0;
}
