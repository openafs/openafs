#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/afs/FBSD/osi_vfsops.c,v 1.15 2004/03/10 23:01:51 rees Exp $");

#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/syscall.h>

struct vcache *afs_globalVp = 0;
struct mount *afs_globalVFS = 0;
int afs_pbuf_freecnt = -1;

#ifdef AFS_FBSD50_ENV
#define	THREAD_OR_PROC struct thread *p
#else
#define	THREAD_OR_PROC struct proc *p
#endif

int
afs_start(struct mount *mp, int flags, THREAD_OR_PROC)
{
    afs_pbuf_freecnt = nswbuf / 2 + 1;
    return (0);			/* nothing to do. ? */
}

int
afs_mount(struct mount *mp, char *path, caddr_t data, struct nameidata *ndp,
	THREAD_OR_PROC)
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our proc struct. */
    size_t size;

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = mp;
    mp->vfs_bsize = 8192;
    vfs_getnewfsid(mp);
    mp->mnt_stat.f_iosize = 8192;

    (void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
    memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
    memset(mp->mnt_stat.f_mntfromname, 0, MNAMELEN);
    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
    /* null terminated string "AFS" will fit, just leave it be. */
    strcpy(mp->mnt_stat.f_fstypename, "afs");
    AFS_GUNLOCK();
    (void)afs_statfs(mp, &mp->mnt_stat, p);
    return 0;
}

int
afs_unmount(struct mount *mp, int flags, THREAD_OR_PROC)
{

    /*
     * Release any remaining vnodes on this mount point.
     * The `1' means that we hold one extra reference on
     * the root vnode (this is just a guess right now).
     * This has to be done outside the global lock.
     */
    vflush(mp, 1, (flags & MNT_FORCE) ? FORCECLOSE : 0);
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown();
    AFS_GUNLOCK();

    return 0;
}

int
afs_root(struct mount *mp, struct vnode **vpp)
{
    int error;
    struct vrequest treq;
    register struct vcache *tvp = 0;
#ifdef AFS_FBSD50_ENV
    struct thread *td = curthread;
    struct ucred *cr = td->td_ucred;
#else
    struct proc *p = curproc;
    struct ucred *cr = p->p_cred->pc_ucred;
#endif

    AFS_GLOCK();
    AFS_STATCNT(afs_root);
    crhold(cr);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
	error = 0;
    } else {
tryagain:
	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    /* vrele() needed here or not? */
	    afs_globalVp = NULL;
	}

	if (!(error = afs_InitReq(&treq, cr)) && !(error = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	    /* we really want this to stay around */
	    if (tvp)
		afs_globalVp = tvp;
	    else
		error = ENOENT;
	}
    }
    if (tvp) {
	struct vnode *vp = AFSTOV(tvp);

#ifdef AFS_FBSD50_ENV
	ASSERT_VI_UNLOCKED(vp, "afs_root");
#endif
	AFS_GUNLOCK();
	/*
	 * I'm uncomfortable about this.  Shouldn't this happen at a
	 * higher level, and shouldn't we busy the top-level directory
	 * to prevent recycling?
	 */
#ifdef AFS_FBSD50_ENV
	error = vget(vp, LK_EXCLUSIVE | LK_RETRY, td);
	vp->v_vflag |= VV_ROOT;
#else
	error = vget(vp, LK_EXCLUSIVE | LK_RETRY, p);
	vp->v_flag |= VROOT;
#endif
	AFS_GLOCK();
	if (error != 0)
		goto tryagain;

	afs_globalVFS = mp;
	*vpp = vp;
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *vpp,
	       ICL_TYPE_INT32, error);
    AFS_GUNLOCK();
    crfree(cr);
    return error;
}

int
afs_statfs(struct mount *mp, struct statfs *abp, THREAD_OR_PROC)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

    abp->f_bsize = mp->vfs_bsize;
    abp->f_iosize = mp->vfs_bsize;

    /* Fake a high number below to satisfy programs that use the statfs call
     * to make sure that there's enough space in the device partition before
     * storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = 2000000;

    abp->f_fsid.val[0] = mp->mnt_stat.f_fsid.val[0];
    abp->f_fsid.val[1] = mp->mnt_stat.f_fsid.val[1];
    if (abp != &mp->mnt_stat) {
	abp->f_type = mp->mnt_vfc->vfc_typenum;
	memcpy((caddr_t) & abp->f_mntonname[0],
	       (caddr_t) mp->mnt_stat.f_mntonname, MNAMELEN);
	memcpy((caddr_t) & abp->f_mntfromname[0],
	       (caddr_t) mp->mnt_stat.f_mntfromname, MNAMELEN);
    }

    AFS_GUNLOCK();
    return 0;
}

int
afs_sync(struct mount *mp, int waitfor, struct ucred *cred, THREAD_OR_PROC)
{
    return 0;
}

int
afs_init(struct vfsconf *vfc)
{
    return 0;
}

struct vfsops afs_vfsops = {
    afs_mount,
    afs_start,
    afs_unmount,
    afs_root,
    vfs_stdquotactl,
    afs_statfs,
    afs_sync,
    vfs_stdvget,
    vfs_stdfhtovp,
    vfs_stdcheckexp,
    vfs_stdvptofh,
    afs_init,
    vfs_stduninit,
    vfs_stdextattrctl,
#ifdef AFS_FBSD50_ENV
    NULL,
#endif
};
