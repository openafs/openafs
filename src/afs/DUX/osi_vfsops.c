/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_vfsops.c for DUX
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/DUX/Attic/osi_vfsops.c,v 1.15 2003/07/15 23:14:19 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */
#include <sys/types.h>
#include <kern/mach_param.h>
#include <sys/sysconfig.h>
#include <sys/systm.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <machine/machlimits.h>


struct vcache *afs_globalVp = 0;
struct mount *afs_globalVFS = 0;

static u_char afs_mntid;
int afs_vfsdev = 0;
udecl_simple_lock_data(, afsmntid_lock)
#define AFSMNTID_LOCK()         usimple_lock(&afsmntid_lock)
#define AFSMNTID_UNLOCK()       usimple_unlock(&afsmntid_lock)
#define AFSMNTID_LOCK_INIT()    usimple_lock_init(&afsmntid_lock)
     int mp_afs_mount(struct mount *afsp, char *path, caddr_t data,
		      struct nameidata *ndp)
{
    u_int size;

    fsid_t tfsid;
    struct mount *xmp, *getvfs();
    int code;

    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
/*
 * Generate a unique afs mount i.d. ( see nfs_mount() ).
 */
    afsp->m_stat.f_fsid.val[0] = makedev(130, 0);
    afsp->m_stat.f_fsid.val[1] = MOUNT_AFS;
    AFSMNTID_LOCK();
    if (++afs_mntid == 0)
	++afs_mntid;
    AFSMNTID_UNLOCK();
    BM(AFSMNTID_LOCK());
    tfsid.val[0] = makedev(130, afs_mntid);
    tfsid.val[1] = MOUNT_AFS;
    BM(AFSMNTID_UNLOCK());

    while (xmp = getvfs(&tfsid)) {
	UNMOUNT_READ_UNLOCK(xmp);
	tfsid.val[0]++;
	AFSMNTID_LOCK();
	afs_mntid++;
	AFSMNTID_UNLOCK();
    }
    if (major(tfsid.val[0]) != 130) {
	AFS_GUNLOCK();
	return (ENOENT);
    }
    afsp->m_stat.f_fsid.val[0] = tfsid.val[0];

    afsp->m_stat.f_mntonname = AFS_KALLOC(MNAMELEN);
    afsp->m_stat.f_mntfromname = AFS_KALLOC(MNAMELEN);
    if (!afsp->m_stat.f_mntonname || !afsp->m_stat.f_mntfromname)
	panic("malloc failure in afs_mount\n");

    memset(afsp->m_stat.f_mntonname, 0, MNAMELEN);
    memset(afsp->m_stat.f_mntfromname, 0, MNAMELEN);
    AFS_COPYINSTR(path, (caddr_t) afsp->m_stat.f_mntonname, MNAMELEN, &size,
		  code);
    memcpy(afsp->m_stat.f_mntfromname, "AFS", 4);
    AFS_GUNLOCK();
    (void)mp_afs_statfs(afsp);
    AFS_GLOCK();
    afs_vfsdev = afsp->m_stat.f_fsid.val[0];

#ifndef	AFS_NONFSTRANS
    /* Set up the xlator in case it wasn't done elsewhere */
    afs_xlatorinit_v2();
    afs_xlatorinit_v3();
#endif
    AFS_GUNLOCK();
    return 0;
}


int
mp_afs_unmount(struct mount *afsp, int flag)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);
    afs_globalVFS = 0;
    afs_shutdown();
    AFS_GUNLOCK();
    return 0;
}


int
mp_afs_start(struct mount *mp, int flags)
{
    return (0);
}

int
mp_afs_root(struct mount *afsp, struct vnode **avpp)
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp = 0;

    AFS_GLOCK();
    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(code = afs_InitReq(&treq, u.u_cred))
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
	struct vnode *vp = AFSTOV(tvp);
	AFS_GUNLOCK();
	VN_HOLD(vp);
	VN_LOCK(vp);
	vp->v_flag |= VROOT;	/* No-op on Ultrix 2.2 */
	VN_UNLOCK(vp);
	AFS_GLOCK();

	afs_globalVFS = afsp;
	*avpp = vp;
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, code);
    AFS_GUNLOCK();
    return code;
}


mp_afs_quotactl(struct mount * mp, int cmd, uid_t uid, caddr_t arg)
{
    return EOPNOTSUPP;
}

int
mp_afs_statfs(struct mount *afsp)
{
    struct nstatfs *abp = &afsp->m_stat;

    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

    abp->f_type = MOUNT_AFS;
    abp->f_bsize = afsp->vfs_bsize;

    /* Fake a high number below to satisfy programs that use the statfs call
     * to make sure that there's enough space in the device partition before
     * storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = 2000000;
    abp->f_fsize = 1024;

    abp->f_fsid.val[0] = afsp->m_stat.f_fsid.val[0];
    abp->f_fsid.val[1] = afsp->m_stat.f_fsid.val[1];

    AFS_GUNLOCK();
    return 0;
}


int
mp_afs_sync(struct mount *mp, int flags)
{
    AFS_STATCNT(afs_sync);
    return 0;
}


#ifdef AFS_DUX50_ENV
int
mp_afs_smoothsync(struct mount *mp, u_int age, u_int smsync_flag)
{
    AFS_STATCNT(afs_sync);
    return 0;
}
#endif

int
mp_afs_fhtovp(struct mount *afsp, struct fid *fidp, struct vnode **avcp)
{
    struct vrequest treq;
    register code = 0;

    AFS_GLOCK();
    AFS_STATCNT(afs_vget);

    *avcp = NULL;

    if ((code = afs_InitReq(&treq, u.u_cred)) == 0) {
	code = afs_osi_vget((struct vcache **)avcp, fidp, &treq);
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_VGET, ICL_TYPE_POINTER, *avcp,
	       ICL_TYPE_INT32, treq.uid, ICL_TYPE_FID, fidp);

    code = afs_CheckCode(code, &treq, 42);
    AFS_GUNLOCK();
    return code;
}


/*
 *  afs_vptofh
 * 
 * afs_vptofh can return two flavors of NFS fid, depending on if submounts are
 * allowed. The reason for this is that we can't guarantee that we found all 
 * the entry points any OS might use to get the fid for the NFS mountd.
 * Hence we return a "magic" fid for all but /afs. If it goes through the
 * translator code, it will get transformed into a SmallFid that we recognize.
 * So, if submounts are disallowed, and an NFS client tries a submount, it will
 * get a fid which we don't recognize and the mount will either fail or we
 * will ignore subsequent requests for that mount.
 *
 * The Alpha fid is organized differently than for other platforms. Their
 * intention was to have the data portion of the fid aligned on a 4 byte
 * boundary. To do so, the fid is organized as:
 * u_short reserved
 * u_short len
 * char data[8]
 * The len field is the length of the entire fid, from reserved through data.
 * This length is used by fid_copy to include copying the reserved field. 
 * Alpha's zero the reserved field before handing us the fid, but they use
 * it in fid_cmp. We use the reserved field to store the 16 bits of the Vnode.
 *
 * Note that the SmallFid only allows for 8 bits of the cell index and
 * 16 bits of the vnode. 
 */

#define AFS_FIDDATASIZE 8
#define AFS_SIZEOFSMALLFID 12	/* full size of fid, including len field */
extern int afs_NFSRootOnly;	/* 1 => only allow NFS mounts of /afs. */
int afs_fid_vnodeoverflow = 0, afs_fid_uniqueoverflow = 0;

int
mp_afs_vptofh(struct vnode *avn, struct fid *fidp)
{
    struct SmallFid Sfid;
    long addr[2];
    register struct cell *tcell;
    int rootvp = 0;
    struct vcache *avc = VTOAFS(avn);

    AFS_GLOCK();
    AFS_STATCNT(afs_fid);

    if (afs_shuttingdown) {
	AFS_GUNLOCK();
	return EIO;
    }

    if (afs_NFSRootOnly && (avc == afs_globalVp))
	rootvp = 1;
    if (!afs_NFSRootOnly || rootvp) {
	tcell = afs_GetCell(avc->fid.Cell, READ_LOCK);
	Sfid.Volume = avc->fid.Fid.Volume;
	fidp->fid_reserved = avc->fid.Fid.Vnode;
	Sfid.CellAndUnique =
	    ((tcell->cellIndex << 24) + (avc->fid.Fid.Unique & 0xffffff));
	afs_PutCell(tcell, READ_LOCK);
	if (avc->fid.Fid.Vnode > 0xffff)
	    afs_fid_vnodeoverflow++;
	if (avc->fid.Fid.Unique > 0xffffff)
	    afs_fid_uniqueoverflow++;
    } else {
	fidp->fid_reserved = AFS_XLATOR_MAGIC;
	addr[0] = (long)avc;
	AFS_GUNLOCK();
	VN_HOLD(AFSTOV(avc));
	AFS_GLOCK();
    }

    /* Use the fid pointer passed to us. */
    fidp->fid_len = AFS_SIZEOFSMALLFID;
    if (afs_NFSRootOnly) {
	if (rootvp) {
	    memcpy(fidp->fid_data, (caddr_t) & Sfid, AFS_FIDDATASIZE);
	} else {
	    memcpy(fidp->fid_data, (caddr_t) addr, AFS_FIDDATASIZE);
	}
    } else {
	memcpy(fidp->fid_data, (caddr_t) & Sfid, AFS_FIDDATASIZE);
    }
    AFS_GUNLOCK();
    return 0;
}


int mp_Afs_init(void);		/* vfs_init - defined below */


/* This is only called by vfs_mount when afs is going to be mounted as root.
 * Since we don't support diskless clients we shouldn't come here.
 */
int afsmountroot = 0;
int
mp_afs_mountroot(struct mount *afsp, struct vnode **vp)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_mountroot);
    afsmountroot++;
    AFS_GUNLOCK();
    return EINVAL;
}


/* It's called to setup swapping over the net for diskless clients; again
 * not for us.
 */
int afsswapvp = 0;
int
mp_afs_swapvp(void)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_swapvp);
    afsswapvp++;
    AFS_GUNLOCK();
    return EINVAL;
}


struct vfsops afs_vfsops = {
    mp_afs_mount,
    mp_afs_start,
    mp_afs_unmount,
    mp_afs_root,
    mp_afs_quotactl,
    mp_afs_statfs,
    mp_afs_sync,
    mp_afs_fhtovp,		/* afs_vget */
    mp_afs_vptofh,
    mp_Afs_init,
    mp_afs_mountroot,
    mp_afs_swapvp,
#ifdef AFS_DUX50_ENV
    mp_afs_smoothsync
#endif
};


/*
 * System Call Entry Points
 */
#define NULL_FUNC          (int (*)(int))0

int (*afs_syscall_func) () = NULL_FUNC;
int (*afs_xsetgroups_func) () = NULL_FUNC;
int (*afs_xioctl_func) () = NULL_FUNC;

afssyscall(p, args, retval)
     struct proc *p;
     void *args;
     long *retval;
{
    int (*func) ();
    int code;

    AFS_GLOCK();
    func = afs_syscall_func;
    if (func == NULL_FUNC) {
	code = nosys(p, args, retval);
    } else {
	code = (*func) (p, args, retval);
    }
    AFS_GUNLOCK();
    return code;
}

afsxsetgroups(p, args, retval)
     struct proc *p;
     void *args;
     long *retval;
{
    int (*func) ();
    int code;

    AFS_GLOCK();
    func = afs_xsetgroups_func;
    if (func == NULL_FUNC) {
	code = nosys(p, args, retval);
    } else {
	code = (*func) (p, args, retval);
    }
    AFS_GUNLOCK();
    return code;
}

afsxioctl(p, args, retval)
     struct proc *p;
     void *args;
     long *retval;
{
    int (*func) ();
    int code;

    AFS_GLOCK();
    func = afs_xioctl_func;
    if (func == NULL_FUNC) {
	code = nosys(p, args, retval);
    } else {
	code = (*func) (p, args, retval);
    }
    AFS_GUNLOCK();
    return code;
}


/*
 * VFS initialization and unload
 */

afs_unconfig()
{
    return EBUSY;
}


cfg_subsys_attr_t afs_attributes[] = {
    {"", 0, 0, 0, 0, 0, 0}	/* must be the last element */
};

afs_configure(cfg_op_t op, caddr_t indata, size_t indata_size,
	      caddr_t outdata, size_t outdata_size)
{
    cfg_attr_t *attributes;
    int ret = ESUCCESS;
    int i, j, size;
    caddr_t p;

    switch (op) {
    case CFG_OP_CONFIGURE:
	/*
	 * The indata parameter is a list of attributes to be configured, and 
	 * indata_size is the count of attributes.
	 */
	if ((ret = vfssw_add_fsname(MOUNT_AFS, &afs_vfsops, "afs")) != 0)
	    return (ret);
	break;
    case CFG_OP_UNCONFIGURE:
	if ((ret = afs_unconfig()) != 0)
	    return (ret);
	break;
    default:
	ret = EINVAL;
	break;
    }
    return ret;
}


int
mp_Afs_init(void)
{
    extern int Afs_xsetgroups(), afs_xioctl(), afs3_syscall();

    AFS_GLOCK();
    ((struct sysent *)(&sysent[AFS_SYSCALL]))->sy_call = afs3_syscall;
#ifdef SY_NARG
    ((struct sysent *)(&sysent[AFS_SYSCALL]))->sy_info = 6;
#else
    ((struct sysent *)(&sysent[AFS_SYSCALL]))->sy_parallel = 0;
    ((struct sysent *)(&sysent[AFS_SYSCALL]))->sy_narg = 6;
#endif

    ((struct sysent *)(&sysent[SYS_setgroups]))->sy_call = Afs_xsetgroups;
    afs_xioctl_func = afsxioctl;
    afs_xsetgroups_func = afsxsetgroups;
    afs_syscall_func = afssyscall;
    AFS_GUNLOCK();

    return 0;
}
