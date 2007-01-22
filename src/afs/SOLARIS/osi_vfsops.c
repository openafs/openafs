/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_vfsops.c for SOLARIS
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */
#include "h/modctl.h"
#include "h/syscall.h"
#include <sys/kobj.h>



struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;

int afsfstype = 0;

int
afs_mount(struct vfs *afsp, struct vnode *amvp, struct mounta *uap,
	  struct AFS_UCRED *credp)
{

    AFS_GLOCK();

    AFS_STATCNT(afs_mount);

#if defined(AFS_SUN510_ENV)
    if (secpolicy_fs_mount(credp, amvp, afsp) != 0) {
#else
    if (!afs_osi_suser(credp)) {
#endif
	AFS_GUNLOCK();
	return (EPERM);
    }
    afsp->vfs_fstype = afsfstype;

    if (afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    afsp->vfs_fsid.val[1] = AFS_VFSFSID;
    afsp->vfs_dev = AFS_VFSMAGIC;

    AFS_GUNLOCK();
    return 0;
}

#if defined(AFS_SUN58_ENV)
int
afs_unmount(struct vfs *afsp, int flag, struct AFS_UCRED *credp)
#else
int
afs_unmount(struct vfs *afsp, struct AFS_UCRED *credp)
#endif
{
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

#if defined(AFS_SUN510_ENV)
    if (secpolicy_fs_unmount(credp, afsp) != 0) {
#else
    if (!afs_osi_suser(credp)) {
#endif
        AFS_GUNLOCK();
        return (EPERM);
    }
    afs_globalVFS = 0;
    afs_shutdown();

    AFS_GUNLOCK();
    return 0;
}

int
afs_root(struct vfs *afsp, struct vnode **avpp)
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp = 0;
    struct proc *proc = ttoproc(curthread);
    struct vnode *vp = afsp->vfs_vnodecovered;
    int locked = 0;

    /* Potential deadlock:
     * afs_root is called with the Vnode's v_lock locked. Set VVFSLOCK
     * and drop the v_lock if we need to make an RPC to complete this
     * request. There used to be a deadlock on the global lock until
     * we stopped calling iget while holding the global lock.
     */

    AFS_GLOCK();

    AFS_STATCNT(afs_root);

    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (MUTEX_HELD(&vp->v_lock)) {
	    vp->v_flag |= VVFSLOCK;
	    locked = 1;
	    mutex_exit(&vp->v_lock);
	}

	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(code = afs_InitReq(&treq, proc->p_cred))
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
	mutex_enter(&AFSTOV(tvp)->v_lock);
	AFSTOV(tvp)->v_flag |= VROOT;
	mutex_exit(&AFSTOV(tvp)->v_lock);

	afs_globalVFS = afsp;
	*avpp = AFSTOV(tvp);
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, code);

    AFS_GUNLOCK();
    if (locked) {
	mutex_enter(&vp->v_lock);
	vp->v_flag &= ~VVFSLOCK;
	if (vp->v_flag & VVFSWAIT) {
	    vp->v_flag &= ~VVFSWAIT;
	    cv_broadcast(&vp->v_cv);
	}
    }

    return code;
}

#ifdef AFS_SUN56_ENV
int
afs_statvfs(struct vfs *afsp, struct statvfs64 *abp)
#else
int
afs_statvfs(struct vfs *afsp, struct statvfs *abp)
#endif
{
    AFS_GLOCK();

    AFS_STATCNT(afs_statfs);

    abp->f_frsize = 1024;
    abp->f_favail = 9000000;
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = 9000000;
    abp->f_fsid = (AFS_VFSMAGIC << 16) || AFS_VFSFSID;

    AFS_GUNLOCK();
    return 0;
}

int
afs_sync(struct vfs *afsp, short flags, struct AFS_UCRED *credp)
{
    return 0;
}

int
afs_vget(struct vfs *afsp, struct vnode **avcp, struct fid *fidp)
{
    cred_t *credp = CRED();
    struct vrequest treq;
    int code;

    AFS_GLOCK();

    AFS_STATCNT(afs_vget);

    *avcp = NULL;
    if (!(code = afs_InitReq(&treq, credp))) {
	code = afs_osi_vget((struct vcache **)avcp, fidp, &treq);
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_VGET, ICL_TYPE_POINTER, *avcp,
	       ICL_TYPE_INT32, treq.uid, ICL_TYPE_FID, fidp);
    code = afs_CheckCode(code, &treq, 42);

    AFS_GUNLOCK();
    return code;
}

/* This is only called by vfs_mount when afs is going to be mounted as root.
 * Since we don't support diskless clients we shouldn't come here.
 */
int afsmountroot = 0;
afs_mountroot(struct vfs *afsp, whymountroot_t why)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_mountroot);
    afsmountroot++;
    AFS_GUNLOCK();
    return EINVAL;
}

/* afs_swapvp is called to setup swapping over the net for diskless clients.
 * Again not for us.
 */
int afsswapvp = 0;
afs_swapvp(struct vfs *afsp, struct vnode **avpp, char *nm)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_swapvp);
    afsswapvp++;
    AFS_GUNLOCK();
    return EINVAL;
}


static void *
do_mod_lookup(const char * mod, const char * sym)
{
    void * ptr;

    ptr = modlookup(mod, sym);
    if (ptr == NULL) {
        afs_warn("modlookup failed for symbol '%s' in module '%s'\n",
		 sym, mod);
    }

    return ptr;
}

#ifdef AFS_SUN510_ENV

/* The following list must always be NULL-terminated */
const fs_operation_def_t afs_vfsops_template[] = {
    { VFSNAME_MOUNT,		afs_mount },
    { VFSNAME_UNMOUNT,		afs_unmount },
    { VFSNAME_ROOT,		afs_root },
    { VFSNAME_STATVFS,		afs_statvfs },
    { VFSNAME_SYNC,		afs_sync },
    { VFSNAME_VGET,		afs_vget },
    { VFSNAME_MOUNTROOT,	afs_mountroot },
    { VFSNAME_FREEVFS,		fs_freevfs },
    { NULL,			NULL},
};
struct vfsops *afs_vfsopsp;

#else /* !AFS_SUN510_ENV */

struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statvfs,
    afs_sync,
    afs_vget,
    afs_mountroot,
    afs_swapvp,
#if defined(AFS_SUN58_ENV)
    fs_freevfs,
#endif
};

#endif /* !AFS_SUN510_ENV */
