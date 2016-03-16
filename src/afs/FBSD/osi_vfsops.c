#include <afsconfig.h>
#include <afs/param.h>


#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/syscall.h>
#include <sys/sysent.h>

struct vcache *afs_globalVp = NULL;
struct mount *afs_globalVFS = NULL;
int afs_pbuf_freecnt = -1;

extern int Afs_xsetgroups();
extern int afs_xioctl();

#if !defined(AFS_FBSD90_ENV) && !defined(AFS_FBSD82_ENV)
static sy_call_t *old_handler;
#else
static struct sysent old_sysent;

static struct sysent afs_sysent = {
    5,			/* int sy_narg */
    (sy_call_t *) afs3_syscall,	/* sy_call_t *sy_call */
#ifdef AFS_FBSD60_ENV
    AUE_NULL,		/* au_event_t sy_auevent */
#ifdef AFS_FBSD70_ENV
    NULL,		/* systrace_args_funt_t sy_systrace_args_func */
    0,			/* u_int32_t sy_entry */
    0,			/* u_int32_t sy_return */
#ifdef AFS_FBSD90_ENV
    0,			/* u_int32_t sy_flags */
    0			/* u_int32_t sy_thrcnt */
#endif
#endif
#endif /* FBSD60 */
};
#endif /* FBSD90 */

int
afs_init(struct vfsconf *vfc)
{
    int code;
    int offset = AFS_SYSCALL;
#if defined(AFS_FBSD90_ENV) || defined(AFS_FBSD82_ENV)
# if defined(AFS_FBSD110_ENV)
    code = syscall_register(&offset, &afs_sysent, &old_sysent, 0);
# else
    code = syscall_register(&offset, &afs_sysent, &old_sysent);
# endif
    if (code) {
	printf("AFS_SYSCALL in use, error %i. aborting\n", code);
	return code;
    }
#else
    if (sysent[AFS_SYSCALL].sy_call != nosys
        && sysent[AFS_SYSCALL].sy_call != lkmnosys) {
        printf("AFS_SYSCALL in use. aborting\n");
        return EBUSY;
    }
#endif
    osi_Init();
    afs_pbuf_freecnt = nswbuf / 2 + 1;
#if !defined(AFS_FBSD90_ENV) && !defined(AFS_FBSD82_ENV)
    old_handler = sysent[AFS_SYSCALL].sy_call;
    sysent[AFS_SYSCALL].sy_call = afs3_syscall;
    sysent[AFS_SYSCALL].sy_narg = 5;
#endif
    return 0;
}

int
afs_uninit(struct vfsconf *vfc)
{
#if defined(AFS_FBSD90_ENV) || defined(AFS_FBSD82_ENV)
    int offset = AFS_SYSCALL;
#endif

    if (afs_globalVFS)
	return EBUSY;
#if defined(AFS_FBSD90_ENV) || defined(AFS_FBSD82_ENV)
    syscall_deregister(&offset, &old_sysent);
#else
    sysent[AFS_SYSCALL].sy_narg = 0;
    sysent[AFS_SYSCALL].sy_call = old_handler;
#endif
    return 0;
}

int
afs_start(struct mount *mp, int flags, struct thread *p)
{
    return (0);			/* nothing to do. ? */
}

int
#if defined(AFS_FBSD80_ENV)
afs_omount(struct mount *mp, char *path, caddr_t data)
#elif defined(AFS_FBSD53_ENV)
afs_omount(struct mount *mp, char *path, caddr_t data, struct thread *p)
#else
afs_omount(struct mount *mp, char *path, caddr_t data, struct nameidata *ndp,
	struct thread *p)
#endif
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our thread struct. */
    size_t size;

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return EBUSY;
    }

    afs_globalVFS = mp;
    mp->vfs_bsize = 8192;
    vfs_getnewfsid(mp);
    /*
     * This is kind of ugly, as the interlock has grown to encompass
     * more fields over time and there's not a good way to group the
     * code without duplication.
     */
#ifdef AFS_FBSD62_ENV
    MNT_ILOCK(mp);
#endif
    mp->mnt_flag &= ~MNT_LOCAL;
#if defined(AFS_FBSD61_ENV) && !defined(AFS_FBSD62_ENV)
    MNT_ILOCK(mp);
#endif
#if __FreeBSD_version < 1000021
    mp->mnt_kern_flag |= MNTK_MPSAFE; /* solid steel */
#endif
#ifndef AFS_FBSD61_ENV
    MNT_ILOCK(mp);
#endif
    /*
     * XXX mnt_stat "is considered stable as long as a ref is held".
     * We should check that we hold the only ref.
     */
    mp->mnt_stat.f_iosize = 8192;

    if (path != NULL)
	copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
    else
	bcopy("/afs", mp->mnt_stat.f_mntonname, size = 4);
    memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
    memset(mp->mnt_stat.f_mntfromname, 0, MNAMELEN);
    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
    /* null terminated string "AFS" will fit, just leave it be. */
    strcpy(mp->mnt_stat.f_fstypename, "afs");
    MNT_IUNLOCK(mp);
    AFS_GUNLOCK();
#ifdef AFS_FBSD80_ENV
    afs_statfs(mp, &mp->mnt_stat);
#else
    afs_statfs(mp, &mp->mnt_stat, p);
#endif

    return 0;
}

#ifdef AFS_FBSD53_ENV
int
#ifdef AFS_FBSD80_ENV
afs_mount(struct mount *mp)
#else
afs_mount(struct mount *mp, struct thread *td)
#endif
{
#ifdef AFS_FBSD80_ENV
    return afs_omount(mp, NULL, NULL);
#else
    return afs_omount(mp, NULL, NULL, td);
#endif
}
#endif

#ifdef AFS_FBSD60_ENV
static int
#if (__FreeBSD_version >= 900503 && __FreeBSD_version < 1000000) || __FreeBSD_version >= 1000004
afs_cmount(struct mntarg *ma, void *data, uint64_t flags)
#elif defined(AFS_FBSD80_ENV)
afs_cmount(struct mntarg *ma, void *data, int flags)
#else
afs_cmount(struct mntarg *ma, void *data, int flags, struct thread *td)
#endif
{
    return kernel_mount(ma, flags);
}
#endif

int
#ifdef AFS_FBSD80_ENV
afs_unmount(struct mount *mp, int flags)
#else
afs_unmount(struct mount *mp, int flags, struct thread *p)
#endif
{
    int error = 0;

    AFS_GLOCK();
    if (afs_globalVp &&
	((flags & MNT_FORCE) || !VREFCOUNT_GT(afs_globalVp, 1))) {
	/* Put back afs_root's ref */
	struct vcache *gvp = afs_globalVp;
	afs_globalVp = NULL;
	afs_PutVCache(gvp);
    }
    if (afs_globalVp)
	error = EBUSY;
    AFS_GUNLOCK();

    /*
     * Release any remaining vnodes on this mount point.
     * The `1' means that we hold one extra reference on
     * the root vnode (this is just a guess right now).
     * This has to be done outside the global lock.
     */
    if (!error) {
#if defined(AFS_FBSD80_ENV)
	error = vflush(mp, 1, (flags & MNT_FORCE) ? FORCECLOSE : 0, curthread);
#elif defined(AFS_FBSD53_ENV)
	error = vflush(mp, 1, (flags & MNT_FORCE) ? FORCECLOSE : 0, p);
#else
	error = vflush(mp, 1, (flags & MNT_FORCE) ? FORCECLOSE : 0);
#endif
    }
    if (error)
	goto out;
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown();
    AFS_GUNLOCK();

out:
    return error;
}

int
#if defined(AFS_FBSD80_ENV)
afs_root(struct mount *mp, int flags, struct vnode **vpp)
#elif defined(AFS_FBSD60_ENV)
afs_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
#elif defined(AFS_FBSD53_ENV)
afs_root(struct mount *mp, struct vnode **vpp, struct thread *td)
#else
afs_root(struct mount *mp, struct vnode **vpp)
#endif
{
    int error;
    struct vrequest treq;
    struct vcache *tvp = 0;
    struct vcache *gvp;
#if !defined(AFS_FBSD53_ENV) || defined(AFS_FBSD80_ENV)
    struct thread *td = curthread;
#endif
    struct ucred *cr = osi_curcred();

    AFS_GLOCK();
    AFS_STATCNT(afs_root);
    crhold(cr);
tryagain:
    if (afs_globalVp && (afs_globalVp->f.states & CStatd)) {
	tvp = afs_globalVp;
	error = 0;
    } else {
	if (!(error = afs_InitReq(&treq, cr)) && !(error = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	    /* we really want this to stay around */
	    if (tvp) {
		gvp = afs_globalVp;
		afs_globalVp = tvp;
		if (gvp) {
		    afs_PutVCache(gvp);
		    if (tvp != afs_globalVp) {
			/* someone raced us and won */
			afs_PutVCache(tvp);
			goto tryagain;
		    }
		}
	    } else
		error = ENOENT;
	}
    }
    if (tvp) {
	struct vnode *vp = AFSTOV(tvp);

	ASSERT_VI_UNLOCKED(vp, "afs_root");
	AFS_GUNLOCK();
	error = vget(vp, LK_EXCLUSIVE | LK_RETRY, td);
	AFS_GLOCK();
	/* we dropped the glock, so re-check everything it had serialized */
	if (!afs_globalVp || !(afs_globalVp->f.states & CStatd) ||
		tvp != afs_globalVp) {
	    vput(vp);
	    afs_PutVCache(tvp);
	    goto tryagain;
	}
	if (error != 0)
	    goto tryagain;
	/*
	 * I'm uncomfortable about this.  Shouldn't this happen at a
	 * higher level, and shouldn't we busy the top-level directory
	 * to prevent recycling?
	 */
	vp->v_vflag |= VV_ROOT;

	afs_globalVFS = mp;
	*vpp = vp;
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, tvp ? AFSTOV(tvp) : NULL,
	       ICL_TYPE_INT32, error);
    AFS_GUNLOCK();
    crfree(cr);
    return error;
}

int
#ifdef AFS_FBSD80_ENV
afs_statfs(struct mount *mp, struct statfs *abp)
#else
afs_statfs(struct mount *mp, struct statfs *abp, struct thread *p)
#endif
{
    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

    abp->f_bsize = mp->vfs_bsize;
    abp->f_iosize = mp->vfs_bsize;

    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = AFS_VFS_FAKEFREE;

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
#if defined(AFS_FBSD80_ENV)
afs_sync(struct mount *mp, int waitfor)
#elif defined(AFS_FBSD60_ENV)
afs_sync(struct mount *mp, int waitfor, struct thread *td)
#else
afs_sync(struct mount *mp, int waitfor, struct ucred *cred, struct thread *p)
#endif
{
    return 0;
}

#ifdef AFS_FBSD60_ENV
struct vfsops afs_vfsops = {
	.vfs_init =		afs_init,
	.vfs_mount =		afs_mount,
	.vfs_cmount =		afs_cmount,
	.vfs_root =		afs_root,
	.vfs_statfs =		afs_statfs,
	.vfs_sync =		afs_sync,
	.vfs_uninit =		afs_uninit,
	.vfs_unmount =		afs_unmount,
	.vfs_sysctl =		vfs_stdsysctl,
};
#else
struct vfsops afs_vfsops = {
#ifdef AFS_FBSD53_ENV
    afs_mount,
#endif
    afs_omount,
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
    afs_uninit,
    vfs_stdextattrctl,
    vfs_stdsysctl,
};
#endif
