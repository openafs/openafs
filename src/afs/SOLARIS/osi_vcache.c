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
    struct vcache *avc;

    avc = afs_osi_Alloc(sizeof(struct vcache));
    osi_Assert(avc != NULL);
    return avc;
}

void
osi_PrePopulateVCache(struct vcache *avc) {
    memset(avc, 0, sizeof(struct vcache));

    QInit(&avc->multiPage);

    AFS_RWLOCK_INIT(&avc->vlock, "vcache vlock");

    rw_init(&avc->rwlock, "vcache rwlock", RW_DEFAULT, NULL);

#ifndef AFS_SUN511_ENV
    /* This is required if the kaio (kernel aynchronous io)
     ** module is installed. Inside the kernel, the function
     ** check_vp( common/os/aio.c) checks to see if the kernel has
     ** to provide asynchronous io for this vnode. This
     ** function extracts the device number by following the
     ** v_data field of the vnode. If we do not set this field
     ** then the system panics. The  value of the v_data field
     ** is not really important for AFS vnodes because the kernel
     ** does not do asynchronous io for regular files. Hence,
     ** for the time being, we fill up the v_data field with the
     ** vnode pointer itself. */
    avc->v.v_data = (char *)avc;
#endif /* !AFS_SUN511_ENV */
}

void
osi_AttachVnode(struct vcache *avc, int seq)
{
#ifdef AFS_SUN511_ENV
    struct vnode *vp;

    osi_Assert(AFSTOV(avc) == NULL);

    vp = vn_alloc(KM_SLEEP);
    osi_Assert(vp != NULL);

    vp->v_data = avc;
    AFSTOV(avc) = vp;
#endif
}

void
osi_PostPopulateVCache(struct vcache *avc) {
    AFSTOV(avc)->v_op = afs_ops;
    AFSTOV(avc)->v_vfsp = afs_globalVFS;
    vSetType(avc, VREG);

    /* Normally we do this in osi_vnhold when we notice the ref count went from
     * 0 -> 1. But if we just setup or reused a vcache, we set the refcount to
     * 1 directly. So, we must explicitly VFS_HOLD here. */
    VFS_HOLD(afs_globalVFS);
}

int
osi_vnhold(struct vcache *avc)
{
    struct vnode *vp = AFSTOV(avc);
    uint_t prevcount;

    mutex_enter(&vp->v_lock);
    prevcount = vp->v_count++;
    mutex_exit(&vp->v_lock);

    if (prevcount == 0) {
	VFS_HOLD(afs_globalVFS);
    }
    return 0;
}
