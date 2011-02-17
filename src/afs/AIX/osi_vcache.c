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

extern struct vnodeops *afs_ops;

int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
     int code;
     if (!VREFCOUNT_GT(avc,0)
         && avc->opens == 0 && (avc->f.states & CUnlinkedDel) == 0) {
        code = afs_FlushVCache(avc, slept);
	if (code == 0)
	    return 1;
     }
     return 0;
}

struct vcache *
osi_NewVnode(void) {
    struct vcache *tvc;

    tvc = afs_osi_Alloc(sizeof(struct vcache));
    osi_Assert(tvc != NULL);

#ifdef	KERNEL_HAVE_PIN
    pin((char *)tvc, sizeof(struct vcache));	/* XXX */
#endif

    return tvc;
}

void
osi_PrePopulateVCache(struct vcache *avc) {
    memset(avc, 0, sizeof(struct vcache));

#ifdef	AFS_AIX32_ENV
    LOCK_INIT(&avc->pvmlock, "vcache pvmlock");
    avc->vmh = avc->segid = NULL;
    avc->credp = NULL;
#endif

    /* Don't forget to free the gnode space */
    avc->v.v_gnode = osi_AllocSmallSpace(sizeof(struct gnode));
    memset(avc->v.v_gnode, 0, sizeof(struct gnode));
}

void
osi_AttachVnode(struct vcache *avc, int seq) { }

void
osi_PostPopulateVCache(struct vcache *avc) {
    avc->v.v_op = afs_ops;

    avc->v.v_vfsp = afs_globalVFS;
    avc->v.v_type = VREG;

    avc->v.v_vfsnext = afs_globalVFS->vfs_vnodes;	/* link off vfs */
    avc->v.v_vfsprev = NULL;
    afs_globalVFS->vfs_vnodes = &avc->v;
    if (avc->v.v_vfsnext != NULL)
	avc->v.v_vfsnext->v_vfsprev = &avc->v;
    avc->v.v_next = avc->v.v_gnode->gn_vnode;	/*Single vnode per gnode for us! */
    avc->v.v_gnode->gn_vnode = &avc->v;
}

