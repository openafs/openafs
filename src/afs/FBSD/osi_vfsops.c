#include <afsconfig.h>
#include <afs/param.h>

RCSID("$Header$");

#include <afs/sysincludes.h>            /* Standard vendor system headers */
#include <afs/afsincludes.h>            /* Afs-based standard headers */
#include <afs/afs_stats.h>              /* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/syscall.h>

struct vcache *afs_globalVp = 0;
struct mount *afs_globalVFS = 0;
int afs_pbuf_freecnt=-1;
int
afs_quotactl()
{
	return EOPNOTSUPP;
}

int
afs_fhtovp(mp, fhp, vpp)
struct mount *mp;
struct fid *fhp;
struct vnode **vpp;
{

	return (EINVAL);
}

int
afs_vptofh(vp, fhp)
struct vnode *vp;
struct fid *fhp;
{

	return (EINVAL);
}

int
afs_start(mp, flags, p)
struct mount *mp;
int flags;
struct proc *p;
{
    afs_pbuf_freecnt = nswbuf / 2 + 1;
    return (0);                         /* nothing to do. ? */
}

int
afs_mount(mp, path, data, ndp, p)
register struct mount *mp;
char *path;
caddr_t data;
struct nameidata *ndp;
struct proc *p;
{
    /* ndp contains the mounted-from device.  Just ignore it.
       we also don't care about our proc struct. */
    size_t size;
    int error;

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) { /* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = mp;
    mp->vfs_bsize = 8192;
    vfs_getnewfsid(mp);
    mp->mnt_stat.f_iosize=8192;
    
    (void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN-1, &size);
    memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
    memset(mp->mnt_stat.f_mntfromname, 0, MNAMELEN);
    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
    /* null terminated string "AFS" will fit, just leave it be. */
    strcpy(mp->mnt_stat.f_fstypename, "afs");
    AFS_GUNLOCK();
    (void) afs_statfs(mp, &mp->mnt_stat, p);
    return 0;
}

int
afs_unmount(mp, flags, p)
struct mount *mp;
int flags;
struct proc *p;
{
    
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown();
    AFS_GUNLOCK();

    return 0;
}

int
afs_root(struct mount *mp,
	      struct vnode **vpp)
{
    int error;
    struct vrequest treq;
    register struct vcache *tvp=0;
    struct proc *p=curproc;
    struct ucred cr;

    cr=*p->p_cred->pc_ucred;
    AFS_GLOCK();
    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
        error=0;
    } else {
	
	if (!(error = afs_InitReq(&treq, &cr)) &&
	    !(error = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, (afs_int32 *)0,
	                        (struct vcache*)0, WRITE_LOCK);
	    /* we really want this to stay around */
	    if (tvp) {
	        afs_globalVp = tvp;
	    } else
	        error = ENOENT;
	}
    }
    if (tvp) {
        osi_vnhold(tvp,0);
    AFS_GUNLOCK();
        vn_lock(AFSTOV(tvp), LK_EXCLUSIVE | LK_RETRY, p);
    AFS_GLOCK();
	afs_globalVFS = mp;
	*vpp = AFSTOV(tvp);
        tvp->v.v_flag |= VROOT;
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *vpp,
	       ICL_TYPE_INT32, error);
    AFS_GUNLOCK();
    return error;
}

int
afs_vget(mp, lfl, vp)
struct mount *mp;
struct vnode *vp;
int lfl;
{
    int error;
    printf("vget called. help!\n");
    if (vp->v_usecount < 0) {
	vprint("bad usecount", vp);
	panic("afs_vget");
    }
    error = vget(vp, lfl, curproc);
    if (!error)
	insmntque(vp, afs_globalVFS);   /* take off free list */
    return error;
}

int afs_statfs(struct mount *mp, struct statfs *abp, struct proc *p)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

#if 0
    abp->f_type = MOUNT_AFS;
#endif
    abp->f_bsize = mp->vfs_bsize;
    abp->f_iosize = mp->vfs_bsize;

    /* Fake a high number below to satisfy programs that use the statfs call
     * to make sure that there's enough space in the device partition before
     * storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree  = 2000000;

    abp->f_fsid.val[0] = mp->mnt_stat.f_fsid.val[0];
    abp->f_fsid.val[1] = mp->mnt_stat.f_fsid.val[1];
    if (abp != &mp->mnt_stat) {
	abp->f_type = mp->mnt_vfc->vfc_typenum;
	memcpy((caddr_t)&abp->f_mntonname[0], (caddr_t)mp->mnt_stat.f_mntonname, MNAMELEN);
	memcpy((caddr_t)&abp->f_mntfromname[0], (caddr_t)mp->mnt_stat.f_mntfromname, MNAMELEN);
    }

    AFS_GUNLOCK();
    return 0;
}

int afs_sync(mp, waitfor, cred, p) 
struct mount *mp;
int waitfor;
struct ucred *cred;
struct prioc *p;
{
return 0;
}

int afs_sysctl() {
   return EOPNOTSUPP;
}


int afs_init(struct vfsconf *vfc) {
   return 0;
}

struct vfsops afs_vfsops = {
  afs_mount,
  afs_start,
  afs_unmount,
  afs_root,
  afs_quotactl,
  afs_statfs,
  afs_sync,
  afs_vget,
  afs_fhtovp,
  afs_vptofh,
  afs_init,
  afs_sysctl
};
