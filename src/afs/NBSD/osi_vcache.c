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
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
    *slept = 0;

    if (!VREFCOUNT_GT(avc,0)
        && avc->opens == 0 && (avc->f.states & CUnlinkedDel) == 0) {
	/*
         * vgone() reclaims the vnode, which calls afs_FlushVCache(),
         * then it puts the vnode on the free list.
         * If we don't do this we end up with a cleaned vnode that's
         * not on the free list.
         */
        AFS_GUNLOCK();
        vgone(AFSTOV(avc));
        AFS_GLOCK();
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
    ReleaseWriteLock(&afs_xvcache);
    AFS_GUNLOCK();
    afs_obsd_getnewvnode(avc);	/* includes one refcount */
    AFS_GLOCK();
    ObtainWriteLock(&afs_xvcache,337);
    lockinit(&avc->rwlock, PINOD, "vcache", 0, 0);
}

void
osi_PostPopulateVCache(struct vcache *avc) {
    AFSTOV(avc)->v_mount = afs_globalVFS;
    vSetType(vc, VREG);
}

