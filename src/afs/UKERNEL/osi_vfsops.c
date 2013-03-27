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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */

struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statfs,
    afs_sync,
};

struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;
int afs_rootCellIndex = 0;

#if !defined(AFS_USR_AIX_ENV)
#include "sys/syscall.h"
#endif

int
afs_mount(struct vfs *afsp, char *path, void *data)
{
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {
	/* Don't allow remounts since some system (like AIX) don't handle it well */
	return (setuerror(EBUSY));
    }
    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    afsp->vfs_fsid.val[1] = (intptr_t)AFS_VFSFSID;

    return 0;
}

int
afs_unmount(struct vfs *afsp)
{
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown();
    return 0;
}

int
afs_root(OSI_VFS_DECL(afsp), struct vnode **avpp)
{
    afs_int32 code = 0;
    struct vrequest treq;
    struct vcache *tvp = 0;
    OSI_VFS_CONVERT(afsp);

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->f.states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(code = afs_InitReq(&treq, get_user_struct()->u_cred))
	    && !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	    /* we really want this to stay around */
	    if (tvp) {
		afs_globalVp = tvp;
	    } else
		code = ENOENT;
	}
    }
    if (tvp) {
	VN_HOLD(AFSTOV(tvp));

	AFSTOV(tvp)->v_flag |= VROOT;	/* No-op on Ultrix 2.2 */
	afs_globalVFS = afsp;
	*avpp = AFSTOV(tvp);
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_GOPEN, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, 0, ICL_TYPE_INT32, code);
    return code;
}

int
afs_sync(struct vfs *afsp)
{
    AFS_STATCNT(afs_sync);
    return 0;
}

int
afs_statfs(struct vfs *afsp, struct statfs *abp)
{
    AFS_STATCNT(afs_statfs);
    abp->f_type = 0;
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    abp->f_fsid.val[1] = (intptr_t)AFS_VFSFSID;
    return 0;
}

int
afs_statvfs(struct vfs *afsp, struct statvfs *abp)
{
    AFS_STATCNT(afs_statfs);

    abp->f_frsize = 1024;
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
      abp->f_favail = abp->f_ffree = AFS_VFS_FAKEFREE;

#ifdef AFS_USR_AIX_ENV
    abp->f_fsid.val[0] = AFS_VFSMAGIC;
    abp->f_fsid.val[1] = AFS_VFSFSID;
#else
    abp->f_fsid = (AFS_VFSMAGIC << 16) || AFS_VFSFSID;
#endif

    return 0;
}

int
afs_mountroot(void)
{
    AFS_STATCNT(afs_mountroot);
    return (EINVAL);
}

int
afs_swapvp(void)
{
    AFS_STATCNT(afs_swapvp);
    return (EINVAL);
}
