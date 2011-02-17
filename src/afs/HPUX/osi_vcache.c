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

int
osi_TryEvictVCache(struct vcache *avc, int *slept, int defersleep) {
    int code;

    /* we can't control whether we sleep */
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
    return afs_osi_Alloc(sizeof(struct vcache));
}

void
osi_PrePopulateVCache(struct vcache *avc) {
    memset(avc, 0, sizeof(struct vcache));

    avc->flushDV.low = avc->flushDV.high = AFS_MAXDV;
}

void
osi_AttachVnode(struct vcache *avc, int seq) {
}

void
osi_PostPopulateVCache(struct vcache *avc) {
    AFSTOV(avc)->v_op = afs_ops;
    avc->v.v_vfsp = afs_globalVFS;
    vSetType(avc, VREG);
}

