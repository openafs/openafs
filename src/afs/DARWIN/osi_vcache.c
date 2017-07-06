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

#include "afs/sysincludes.h"    /*Standard vendor system headers */
#include "afsincludes.h"        /*AFS-based standard headers */

struct vcache *
osi_NewVnode(void) {
    struct vcache *tvc;

    tvc = afs_osi_Alloc(sizeof(struct vcache));
    osi_Assert(tvc != NULL);
    tvc->v = NULL; /* important to clean this, or use memset 0 */

    return tvc;
}


#if defined(AFS_DARWIN80_ENV)
int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
    *slept = 0;

    /* we ignore defersleep, as we *always* need to sleep */
    if (!VREFCOUNT_GT(avc, 0) && avc->opens == 0 &&
	(avc->f.states & CUnlinkedDel) == 0) {

	vnode_t tvp = AFSTOV(avc);
	/* VREFCOUNT_GT only sees usecounts, not iocounts */
	/* so this may fail to actually recycle the vnode now */
	/* must call vnode_get to avoid races. */
	if (vnode_get(tvp) == 0) {
	    *slept=1;
	    /* must release lock, since vnode_put will immediately
	       reclaim if there are no other users */
	    ReleaseWriteLock(&afs_xvcache);
	    AFS_GUNLOCK();
	    vnode_recycle(tvp);
	    vnode_put(tvp);
	    AFS_GLOCK();
	    ObtainWriteLock(&afs_xvcache, 336);
	}
	/* we can't use the vnode_recycle return value to figure
	 * this out, since the iocount we have to hold makes it
	 * always "fail" */
	if (AFSTOV(avc) == tvp) {
	    /* Caller will move this vcache to the head of the VLRU. */
	    return 0;
	} else
	    return 1;
    }
    return 0;
}
#else
int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
    if (!VREFCOUNT_GT(avc,0)
        || ((VREFCOUNT(avc) == 1) && (UBCINFOEXISTS(AFSTOV(avc))))
        && avc->opens == 0 && (avc->f.states & CUnlinkedDel) == 0)
    {
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
#endif /* AFS_DARWIN80_ENV */

void
osi_PrePopulateVCache(struct vcache *avc) {
    memset(avc, 0, sizeof(struct vcache));

    /* PPC Darwin 80 seems to be a BOZONLOCK environment, so we need this
     * here ... */
#if defined(AFS_BOZONLOCK_ENV)
    afs_BozonInit(&avc->pvnLock, avc);
#endif
}

void
osi_AttachVnode(struct vcache *avc, int seq) {
    ReleaseWriteLock(&afs_xvcache);
    AFS_GUNLOCK();
    afs_darwin_getnewvnode(avc);  /* includes one refcount */
    AFS_GLOCK();
    ObtainWriteLock(&afs_xvcache,338);
#ifdef AFS_DARWIN80_ENV
    LOCKINIT(avc->rwlock);
#else
    lockinit(&avc->rwlock, PINOD, "vcache", 0, 0);
#endif
}

void
osi_PostPopulateVCache(struct vcache *avc) {
#if !defined(AFS_DARWIN80_ENV)
   avc->v->v_mount = afs_globalVFS;
   vSetType(avc, VREG);
#else
   vSetType(avc, VNON);
#endif
}
