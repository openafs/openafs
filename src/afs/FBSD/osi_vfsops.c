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

static struct syscall_helper_data afs_syscalls[] = {
    {
	.syscall_no = AFS_SYSCALL,
	.new_sysent = {
	    .sy_narg = 5,
	    .sy_call = (sy_call_t *)afs3_syscall,
	    .sy_auevent = AUE_NULL,
	},
    },
    SYSCALL_INIT_LAST
};

static int
afs_init(struct vfsconf *vfc)
{
    int code;
#if defined(FBSD_SYSCALL_REGISTER_TAKES_FLAGS)
    code = syscall_helper_register(afs_syscalls, 0);
#else
    code = syscall_helper_register(afs_syscalls);
#endif
    if (code) {
	printf("AFS_SYSCALL in use, error %i. aborting\n", code);
	return code;
    }
    osi_Init();
    afs_pbuf_freecnt = nswbuf / 2 + 1;
    return 0;
}

static int
afs_uninit(struct vfsconf *vfc)
{
    if (afs_globalVFS)
	return EBUSY;

    return syscall_helper_unregister(afs_syscalls);
}

static int
afs_statfs(struct mount *mp, struct statfs *abp)
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

static int
afs_omount(struct mount *mp, char *path, caddr_t data)
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
    MNT_ILOCK(mp);
    mp->mnt_flag &= ~MNT_LOCAL;
#if __FreeBSD_version < 1000021
    mp->mnt_kern_flag |= MNTK_MPSAFE; /* solid steel */
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
    afs_statfs(mp, &mp->mnt_stat);

    return 0;
}

static int
afs_mount(struct mount *mp)
{
    return afs_omount(mp, NULL, NULL);
}

static int
#if __FreeBSD_version >= 1000004
afs_cmount(struct mntarg *ma, void *data, uint64_t flags)
#else
afs_cmount(struct mntarg *ma, void *data, int flags)
#endif
{
    return kernel_mount(ma, flags);
}

static int
afs_unmount(struct mount *mp, int flags)
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

    if (!error) {
	/*
	 * Release any remaining vnodes on this mount point. The second
	 * argument is how many refs we hold on the root vnode. Since we
	 * released our reference to the root vnode up above, give 0.
	 */
	error = vflush(mp, 0, (flags & MNT_FORCE) ? FORCECLOSE : 0, curthread);
    }
    if (error)
	goto out;
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown(AFS_WARM);
    AFS_GUNLOCK();

out:
    return error;
}

static int
afs_root(struct mount *mp, int flags, struct vnode **vpp)
{
    int error;
    struct vrequest treq;
    struct vcache *tvp = 0;
    struct vcache *gvp;
    struct thread *td = curthread;
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
		error = EIO;
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

static int
afs_sync(struct mount *mp, int waitfor)
{
    return 0;
}

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
