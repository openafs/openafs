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
    int code;

    /* Perhaps this function should use vgone() or vrecycle() instead. */

    if ((afs_debug & AFSDEB_GENERAL) != 0) {
	printf("%s enter\n", __func__);
    }

    if (osi_VM_FlushVCache(avc) != 0) {
	code = 0;
    } else {
	code = 1;
    }

    if ((afs_debug & AFSDEB_GENERAL) != 0) {
        printf("%s exit %d\n", __func__, code);
    }

    return code;
}

struct vcache *
osi_NewVnode(void)
{
    struct vcache *tvc;

    tvc = afs_osi_Alloc(sizeof(struct vcache));
    if (tvc == NULL) {
	return NULL;
    }
    tvc->v = NULL; /* important to clean this, or use memset 0 */

    return tvc;
}

void
osi_PrePopulateVCache(struct vcache *avc)
{
    memset(avc, 0, sizeof(struct vcache));
}

void
osi_AttachVnode(struct vcache *avc, int seq)
{
    ReleaseWriteLock(&afs_xvcache);
    AFS_GUNLOCK();
    afs_nbsd_getnewvnode(avc);	/* includes one refcount */
    AFS_GLOCK();
    ObtainWriteLock(&afs_xvcache,337);
#ifndef AFS_NBSD50_ENV
    lockinit(&avc->rwlock, PINOD, "vcache", 0, 0);
#endif
}

void
osi_PostPopulateVCache(struct vcache *avc)
{
    AFSTOV(avc)->v_mount = afs_globalVFS;
    vSetType(avc, VREG);
}

int
osi_vnhold(struct vcache *avc)
{
    VN_HOLD(AFSTOV(avc));
    return 0;
}
