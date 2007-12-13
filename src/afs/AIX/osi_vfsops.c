/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_vfsops.c for AIX
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */


#ifdef AFS_AIX_IAUTH_ENV
#include "afs/nfsclient.h"
#include "afs/exporter.h"
extern struct afs_exporter *afs_nfsexporter;
#endif

#define AFS_VFSLOCK_DECL register int glockOwner = ISAFS_GLOCK()
#define AFS_VFSLOCK()	if (!glockOwner) AFS_GLOCK()
#define AFS_VFSUNLOCK() if (!glockOwner) AFS_GUNLOCK()

struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;

static int afs_root_nolock(struct vfs *afsp, struct vnode **avpp);

static int
afs_mount(afsp, path, data)
     char *path;
     caddr_t data;
     struct vfs *afsp;
{
    struct vnode *afsrootvp = NULL;
    int error;
    AFS_VFSLOCK_DECL;
    AFS_VFSLOCK();
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {
	/* Don't allow remounts since some system (like AIX) don't handle
	 * it well.
	 */
	AFS_VFSUNLOCK();
	return (setuerror(EBUSY));
    }


    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_count = 0;
#ifdef AFS_64BIT_CLIENT
    afsp->vfs_flag |= VFS_DEVMOUNT;
#endif /* AFS_64BIT_CLIENT */

    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    afsp->vfs_fsid.val[1] = AFS_VFSFSID;

    /* For AFS, we don't allow file over file mounts. */
    if (afsp->vfs_mntdover->v_type != VDIR)
	return (ENOTDIR);
    /* try to get the root vnode, but don't worry if you don't.  The actual
     * setting of the root vnode (vfs_mntd) is done in afs_root, so that it
     * get re-eval'd at the right time if things aren't working when we
     * first try the mount.
     */
    afs_root_nolock(afsp, &afsrootvp);

    afsp->vfs_mntdover->v_mvfsp = afsp;
    afsp->vfs_mdata->vmt_flags |= MNT_REMOTE;

#ifdef AFS_AIX51_ENV
    afsp->vfs_count = 1;
    afsp->vfs_mntd->v_count = 1;
#endif
#ifdef AFS_AIX_IAUTH_ENV
    if (afs_iauth_register() < 0)
	afs_warn("Can't register AFS iauth interface.\n");
#endif
    AFS_VFSUNLOCK();
    return 0;
}

static int
afs_unmount(struct vfs *afsp, int flag)
{
    AFS_VFSLOCK_DECL;
    AFS_VFSLOCK();
    AFS_STATCNT(afs_unmount);

    afs_globalVFS = 0;
    afs_cold_shutdown = 1;
    afs_shutdown();

    AFS_VFSUNLOCK();
    return 0;
}

static int
afs_root_nolock(struct vfs *afsp, struct vnode **avpp)
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp = 0;

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	struct ucred *credp;
	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}
	credp = crref();
	if (!(code = afs_InitReq(&treq, credp)) && !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	    /* we really want this to stay around */
	    if (tvp) {
		afs_globalVp = tvp;
	    } else
		code = ENOENT;
	}
	crfree(credp);
    }
    if (tvp) {
	VN_HOLD(AFSTOV(tvp));

	VN_LOCK(AFSTOV(tvp));
	AFSTOV(tvp)->v_flag |= VROOT;	/* No-op on Ultrix 2.2 */
	VN_UNLOCK(AFSTOV(tvp));

	afs_globalVFS = afsp;
	*avpp = AFSTOV(tvp);
	afsp->vfs_mntd = *avpp;
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, code);
    return code;
}


static int
afs_root(struct vfs *afsp, struct vnode **avpp)
{
    int code;
    AFS_VFSLOCK_DECL;
    AFS_VFSLOCK();
    code = afs_root_nolock(afsp, avpp);
    AFS_VFSUNLOCK();
    return code;
}

static int
afs_statfs(struct vfs *afsp, struct statfs *abp, struct ucred *credp)
{
    AFS_VFSLOCK_DECL;
    AFS_VFSLOCK();
    AFS_STATCNT(afs_statfs);

    abp->f_version = 0;
    abp->f_type = 0;
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = 9000000;
    abp->f_vfstype = AFS_VFSFSID;
    abp->f_vfsnumber = afsp->vfs_number;
    abp->f_vfsoff = abp->f_vfslen = abp->f_vfsvers = -1;
    abp->f_fsize = 4096;	/* fundamental filesystem block size */
    abp->f_fsid = afsp->vfs_fsid;
    (void)strcpy(abp->f_fname, "/afs");
    (void)strcpy(abp->f_fpack, "AFS");
    abp->f_name_max = 256;

    AFS_VFSUNLOCK();
    return 0;
}

static int
afs_sync()
{
    AFS_VFSLOCK_DECL;
    AFS_VFSLOCK();
    AFS_STATCNT(afs_sync);

    AFS_VFSUNLOCK();
    return 0;
}


/* Note that the cred is only for AIX 4.1.5+ and AIX 4.2+ */
static int
afs_vget(struct vfs *vfsp, struct vnode **avcp, struct fileid *fidp,
	 struct ucred *credp)
{
    int code;
    struct vrequest treq;
    AFS_VFSLOCK_DECL;
    AFS_VFSLOCK();
    AFS_STATCNT(afs_vget);
    *avcp = NULL;

#ifdef AFS_AIX_IAUTH_ENV
    /* If the exporter is off and this is an nfsd, fail immediately. */
    if (AFS_NFSXLATORREQ(credp)
	&& !(afs_nfsexporter->exp_states & EXP_EXPORTED)) {
	return EACCES;
    }
#endif

    if ((code = afs_InitReq(&treq, credp)) == 0) {
	code = afs_osi_vget((struct vcache **)avcp, fidp, &treq);
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_VGET, ICL_TYPE_POINTER, *avcp,
	       ICL_TYPE_INT32, treq.uid, ICL_TYPE_FID, fidp);
    code = afs_CheckCode(code, &treq, 42);

    AFS_VFSUNLOCK();
    return code;
}

static int
afs_badop()
{
    return EOPNOTSUPP;
}


struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statfs,
    afs_sync,
    afs_vget,
    afs_badop,			/* vfs_cntl */
    afs_badop			/* vfs_quotactl */
#ifdef AFS_AIX51_ENV
	, afs_badop		/* vfs_syncvfs */
#endif
};

/*
 * VFS is initialized in osi_config.c
 */
