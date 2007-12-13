/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_vfsops.c for IRIX
 */
#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */
#include "sys/syssgi.h"


struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;

#ifdef AFS_SGI_VNODE_GLUE
#include <sys/invent.h>
int afs_is_numa_arch;
mutex_t afs_init_kern_lock;
#endif


#define SYS_setgroups SGI_SETGROUPS

int (*nfs_rfsdisptab_v2) () = NULL;

int afs_fstype;
lock_t afs_rxlock;

#include "sys/mload.h"
char *Afs_mversion = M_VERSION;

extern int (*setgroupsp) (int, gid_t *);
extern struct afs_lock afs_xvcache;
extern int idbg_afsuser();
extern void afs_mpservice(void *);

/*
 * AFS fs initialization - we also plug system calls here
 */
#define NewSystemCall(n,f,a) \
	syscallsw[ABI_IRIX5].sc_sysent[(n)-1000].sy_narg = a; \
	syscallsw[ABI_IRIX5].sc_sysent[(n)-1000].sy_call = f; \
	syscallsw[ABI_IRIX5].sc_sysent[(n)-1000].sy_flags = 0;
extern struct vfsops Afs_vfsops, *afs_vfsopsp;
extern struct vnodeops Afs_vnodeops, *afs_vnodeopsp;
extern void (*afsidestroyp) (struct inode *);
extern void (*afsdptoipp) (struct efs_dinode *, struct inode *);
extern void (*afsiptodpp) (struct inode *, struct efs_dinode *);
extern void afsidestroy(struct inode *);
extern void afsdptoip(struct efs_dinode *, struct inode *);
extern void afsiptodp(struct inode *, struct efs_dinode *);
extern int (*idbg_prafsnodep) (vnode_t *);
extern int (*idbg_afsvfslistp) (void);
extern int idbg_prafsnode(vnode_t *);
extern int idbg_afsvfslist(void);


int
Afs_init(struct vfssw *vswp, int fstype)
{
    extern int Afs_syscall(), Afs_xsetgroups(), afs_pioctl(), afs_setpag();
    extern int icreate(), iopen(), iinc(), idec();
#ifdef AFS_SGI_XFS_IOPS_ENV
    extern int iopen64();
#else
    extern int iread(), iwrite();
#endif

    AFS_STATCNT(afsinit);
    osi_Init();
    afs_fstype = fstype;

#ifdef AFS_SGI_VNODE_GLUE
    /* Synchronize doing NUMA test. */
    mutex_init(&afs_init_kern_lock, MUTEX_DEFAULT, "init_kern_lock");
#endif
    /*
     * set up pointers from main kernel into us
     */
    afs_vnodeopsp = &Afs_vnodeops;
    afs_vfsopsp = &Afs_vfsops;
    afsidestroyp = afsidestroy;
    afsiptodpp = afsiptodp;
    afsdptoipp = afsdptoip;
    idbg_prafsnodep = idbg_prafsnode;
    idbg_afsvfslistp = idbg_afsvfslist;
    NewSystemCall(AFS_SYSCALL, Afs_syscall, 6);
    NewSystemCall(AFS_PIOCTL, afs_pioctl, 4);
    NewSystemCall(AFS_SETPAG, afs_setpag, 0);
    NewSystemCall(AFS_IOPEN, iopen, 3);
    NewSystemCall(AFS_ICREATE, icreate, 6);
    NewSystemCall(AFS_IINC, iinc, 3);
    NewSystemCall(AFS_IDEC, idec, 3);
#ifdef AFS_SGI_XFS_IOPS_ENV
    NewSystemCall(AFS_IOPEN64, iopen64, 4);
#else
    NewSystemCall(AFS_IREAD, iread, 6);
    NewSystemCall(AFS_IWRITE, iwrite, 6);
#endif

    /* last replace these */
    setgroupsp = Afs_xsetgroups;

    idbg_addfunc("afsuser", idbg_afsuser);
    return (0);
}


extern int afs_mount(), afs_unmount(), afs_root(), afs_statfs();
#ifdef AFS_SGI65_ENV
extern int afs_sync(OSI_VFS_DECL(afsp), int flags, struct cred *cr);
#else
extern int afs_sync(OSI_VFS_DECL(afsp), short flags, struct cred *cr);
#endif
extern int afs_vget(OSI_VFS_DECL(afsp), vnode_t ** vpp, struct fid *afidp);
#ifdef MP
struct vfsops afs_lockedvfsops =
#else
struct vfsops Afs_vfsops =
#endif
{
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
#else
    VFS_POSITION_BASE,
#endif
#endif
    afs_mount,
#ifdef AFS_SGI64_ENV
    fs_nosys,			/* rootinit */
    fs_nosys,			/* mntupdate */
    fs_dounmount,
#endif
    afs_unmount,
    afs_root,
    afs_statfs,
    afs_sync,
    afs_vget,
    fs_nosys,			/* mountroot */
#ifdef AFS_SGI65_ENV
    fs_nosys,			/* realvfsops */
    fs_import,			/* import */
    fs_nosys,			/* quotactl */
#else
    fs_nosys,			/* swapvp */
#endif
};
extern struct afs_q VLRU;	/*vcache LRU */

#ifdef AFS_SGI64_ENV
static bhv_desc_t afs_vfs_bhv;
#endif
afs_mount(struct vfs *afsp, vnode_t * mvp, struct mounta *uap,
#ifdef AFS_SGI65_ENV
	  char *attrs,
#endif
	  cred_t * cr)
{
    AFS_STATCNT(afs_mount);

    if (!suser())
	return EPERM;

    if (mvp->v_type != VDIR)
	return ENOTDIR;

    if (afs_globalVFS) {	/* Don't allow remounts. */
	return EBUSY;
    }

    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    afsp->vfs_fsid.val[1] = afs_fstype;
#ifdef AFS_SGI64_ENV
    vfs_insertbhv(afsp, &afs_vfs_bhv, &Afs_vfsops, &afs_vfs_bhv);
#else
    afsp->vfs_data = NULL;
#endif
    afsp->vfs_fstype = afs_fstype;
    afsp->vfs_dev = 0xbabebabe;	/* XXX this should be unique */

#ifndef	AFS_NONFSTRANS
    if (nfs_rfsdisptab_v2)
	afs_xlatorinit_v2(nfs_rfsdisptab_v2);
    afs_xlatorinit_v3();
#endif
    return 0;
}

afs_unmount(OSI_VFS_ARG(afsp), flags, cr)
    OSI_VFS_DECL(afsp);
     int flags;
     cred_t *cr;
{
    struct vcache *tvc;
    vnode_t *vp, *rootvp = NULL;
    register struct afs_q *tq;
    struct afs_q *uq;
    int error, fv_slept;
    OSI_VFS_CONVERT(afsp);

    AFS_STATCNT(afs_unmount);

    if (!suser())
	return EPERM;

    /*
     * flush all pages from inactive vnodes - return
     * EBUSY if any still in use
     */
    ObtainWriteLock(&afs_xvcache, 172);
    for (tq = VLRU.prev; tq != &VLRU; tq = uq) {
	tvc = QTOV(tq);
	uq = QPrev(tq);
	vp = (vnode_t *) tvc;
	if (error = afs_FlushVCache(tvc, &fv_slept))
	    if (vp->v_flag & VROOT) {
		rootvp = vp;
		continue;
	    } else {
		ReleaseWriteLock(&afs_xvcache);
		return error;
	    }
    }

    /*
     * rootvp gets lots of ref counts
     */
    if (rootvp) {
	tvc = VTOAFS(rootvp);
	if (tvc->opens || CheckLock(&tvc->lock) || LockWaiters(&tvc->lock)) {
	    ReleaseWriteLock(&afs_xvcache);
	    return EBUSY;
	}
	ReleaseWriteLock(&afs_xvcache);
	rootvp->v_count = 1;
	AFS_RELE(rootvp);
	ObtainWriteLock(&afs_xvcache, 173);
	afs_FlushVCache(tvc, &fv_slept);
    }
    ReleaseWriteLock(&afs_xvcache);
    afs_globalVFS = 0;
    afs_shutdown();
#ifdef AFS_SGI65_ENV
    VFS_REMOVEBHV(afsp, &afs_vfs_bhv);
#endif
    return 0;
}



afs_root(OSI_VFS_ARG(afsp), avpp)
    OSI_VFS_DECL(afsp);
     struct vnode **avpp;
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp = 0;
    OSI_VFS_CONVERT(afsp);

    AFS_STATCNT(afs_root);
    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(code = afs_InitReq(&treq, OSI_GET_CURRENT_CRED()))
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
	int s;
	VN_HOLD(AFSTOV(tvp));
	s = VN_LOCK(AFSTOV(tvp));
	AFSTOV(tvp)->v_flag |= VROOT;
	VN_UNLOCK(AFSTOV(tvp), s);

	afs_globalVFS = afsp;
	*avpp = AFSTOV(tvp);
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, code);
    return code;
}

afs_statfs(OSI_VFS_ARG(afsp), abp, avp)
    OSI_VFS_DECL(afsp);
     struct statvfs *abp;
     struct vnode *avp;		/* unused */
{
    OSI_VFS_CONVERT(afsp);

    AFS_STATCNT(afs_statfs);
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_frsize = afsp->vfs_bsize;
    /* Fake a high number below to satisfy programs that use the statfs
     * call to make sure that there's enough space in the device partition
     * before storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = abp->f_favail = 900000;

    abp->f_fsid = AFS_VFSMAGIC;	/* magic */
    strcpy(abp->f_basetype, AFS_MOUNT_AFS);
    abp->f_flag = 0;
    abp->f_namemax = 256;
    return 0;
}



/*
 * sync's responsibilities include pushing back DELWRI pages
 * Things to watch out for:
 *	1) don't want to hold off new vnodes in the file system
 *		while pushing back pages
 *	2) since we can deal with un-referenced vndoes need to watch
 *		races with folks who recycle vnodes
 * Flags:
 *      SYNC_BDFLUSH - do NOT sleep waiting for an inode - also, when
 *                      when pushing DELWRI - only push old ones.
 *	SYNC_PDFLUSH - push v_dpages.
 *      SYNC_ATTR    - sync attributes - note that ordering considerations
 *                      dictate that we also flush dirty pages
 *      SYNC_WAIT    - do synchronouse writes - inode & delwri
 *	SYNC_NOWAIT  - start delayed writes.
 *      SYNC_DELWRI  - look at inodes w/ delwri pages. Other flags
 *                      decide how to deal with them.
 *      SYNC_CLOSE   - flush delwri and invalidate others.
 *      SYNC_FSDATA  - push fs data (e.g. superblocks)
 */

extern afs_int32 vcachegen;
#define PREEMPT_MASK    0x7f
#ifdef AFS_SGI64_ENV
#define PREEMPT()
#endif

int
afs_sync(OSI_VFS_DECL(afsp),
#ifdef AFS_SGI65_ENV
	 int flags,
#else
	 short flags,
#endif
	 struct cred *cr)
{
    /* Why enable the vfs sync operation?? */
    int error, lasterr, preempt;
    struct vcache *tvc;
    struct vnode *vp;
    afs_uint32 lvcachegen;
    register struct afs_q *tq;
    struct afs_q *uq;
    int s;
    OSI_VFS_CONVERT(afsp);

    error = lasterr = 0;
    /*
     * if not interested in vnodes, skip all this
     */
#ifdef AFS_SGI61_ENV
    if ((flags & (SYNC_CLOSE | SYNC_DELWRI | SYNC_PDFLUSH)) == 0)
	goto end;
#else /* AFS_SGI61_ENV */
    if ((flags & (SYNC_CLOSE | SYNC_DELWRI | SYNC_ATTR)) == 0)
	goto end;
#endif /* AFS_SGI61_ENV */
  loop:
    ObtainReadLock(&afs_xvcache);
    for (tq = VLRU.prev; tq != &VLRU; tq = uq) {
	tvc = QTOV(tq);
	uq = QPrev(tq);
	vp = (vnode_t *) tvc;
	/*
	 * Since we push all dirty pages on last close/VOP_INACTIVE
	 * we are only concerned with vnodes with
	 * active reference counts.
	 */
	s = VN_LOCK(vp);
	if (vp->v_count == 0) {
	    VN_UNLOCK(vp, s);
	    continue;
	}
	if ((flags & SYNC_CLOSE) == 0 && !AFS_VN_DIRTY(vp)) {
	    VN_UNLOCK(vp, s);
	    continue;
	}

	/*
	 * ignore vnodes which need no flushing
	 */
	if (flags & SYNC_DELWRI) {
	    if (!AFS_VN_DIRTY(vp)) {
		VN_UNLOCK(vp, s);
		continue;
	    }
	}
#ifdef AFS_SGI61_ENV
	else if (flags & SYNC_PDFLUSH) {
	    if (!VN_GET_DPAGES(vp)) {
		VN_UNLOCK(vp, s);
		continue;
	    }
	}
#endif /* AFS_SGI61_ENV */

	vp->v_count++;
	VN_UNLOCK(vp, s);
	lvcachegen = vcachegen;
	ReleaseReadLock(&afs_xvcache);

	/*
	 * Try to lock rwlock without sleeping.  If we can't, we must
	 * sleep for rwlock.
	 */
	if (afs_rwlock_nowait(vp, 1) == 0) {
#ifdef AFS_SGI61_ENV
	    if (flags & (SYNC_BDFLUSH | SYNC_PDFLUSH))
#else /* AFS_SGI61_ENV */
	    if (flags & SYNC_BDFLUSH)
#endif /* AFS_SGI61_ENV */
	    {
		AFS_RELE(vp);
		ObtainReadLock(&afs_xvcache);
		if (vcachegen != lvcachegen) {
		    ReleaseReadLock(&afs_xvcache);
		    goto loop;
		}
		continue;
	    }
	    AFS_RWLOCK(vp, VRWLOCK_WRITE);
	}

	AFS_GUNLOCK();
	if (flags & SYNC_CLOSE) {
	    PFLUSHINVALVP(vp, (off_t) 0, (off_t) tvc->m.Length);
	}
#ifdef AFS_SGI61_ENV
	else if (flags & SYNC_PDFLUSH) {
	    if (VN_GET_DPAGES(vp)) {
		pdflush(vp, B_ASYNC);
	    }
	}
#endif /* AFS_SGI61_ENV */


	if ((flags & SYNC_DELWRI) && AFS_VN_DIRTY(vp)) {
#ifdef AFS_SGI61_ENV
	    PFLUSHVP(vp, (off_t) tvc->m.Length,
		     (flags & SYNC_WAIT) ? 0 : B_ASYNC, error);
#else /* AFS_SGI61_ENV */
	    if (flags & SYNC_WAIT)
		/* push all and wait */
		PFLUSHVP(vp, (off_t) tvc->m.Length, (off_t) 0, error);
	    else if (flags & SYNC_BDFLUSH) {
		/* push oldest */
		error = pdflush(vp, B_ASYNC);
	    } else {
		/* push all but don't wait */
		PFLUSHVP(vp, (off_t) tvc->m.Length, (off_t) B_ASYNC, error);
	    }
#endif /* AFS_SGI61_ENV */
	}

	/*
	 * Release vp, check error and whether to preempt, and if
	 * we let go of xvcache lock and someone has changed the
	 * VLRU, restart the loop
	 */
	AFS_GLOCK();
	AFS_RWUNLOCK(vp, VRWLOCK_WRITE);
	AFS_RELE(vp);
	if (error)
	    lasterr = error;
	if ((++preempt & PREEMPT_MASK) == 0) {
	    AFS_GUNLOCK();
	    PREEMPT();
	    AFS_GLOCK();
	}
	ObtainReadLock(&afs_xvcache);
	if (vcachegen != lvcachegen) {
	    ReleaseReadLock(&afs_xvcache);
	    goto loop;
	}
    }
    ReleaseReadLock(&afs_xvcache);
  end:
    return lasterr;
}


afs_vget(OSI_VFS_DECL(afsp), vnode_t ** avcp, struct fid * fidp)
{
    struct VenusFid vfid;
    struct vrequest treq;
    register struct cell *tcell;
    register afs_int32 code = 0;
    afs_int32 ret;

#if defined(AFS_SGI64_ENV) && defined(CKPT) && !defined(_R5000_CVT_WAR)
    afs_fid2_t *afid2;
#endif

    OSI_VFS_CONVERT(afsp);

    AFS_STATCNT(afs_vget);

    *avcp = NULL;

#if defined(AFS_SGI64_ENV) && defined(CKPT) && !defined(_R5000_CVT_WAR)
    afid2 = (afs_fid2_t *) fidp;
    if (afid2->af_len == sizeof(afs_fid2_t) - sizeof(afid2->af_len)) {
	/* It's a checkpoint restart fid. */
	tcell = afs_GetCellByIndex(afid2->af_cell, READ_LOCK);
	if (!tcell) {
	    code = ENOENT;
	    goto out;
	}
	vfid.Cell = tcell->cellNum;
	afs_PutCell(tcell, READ_LOCK);
	vfid.Fid.Volume = afid2->af_volid;
	vfid.Fid.Vnode = afid2->af_vno;
	vfid.Fid.Unique = afid2->af_uniq;

	if (code = afs_InitReq(&treq, OSI_GET_CURRENT_CRED()))
	    goto out;
	*avcp =
	    (vnode_t *) afs_GetVCache(&vfid, &treq, NULL, (struct vcache *)0);
	if (!*avcp) {
	    code = ENOENT;
	}
	goto out;
    }
#endif

    if (code = afs_InitReq(&treq, OSI_GET_CURRENT_CRED()))
	goto out;
    code = afs_osi_vget((struct vcache **)avcp, fidp, &treq);

  out:
    afs_Trace3(afs_iclSetp, CM_TRACE_VGET, ICL_TYPE_POINTER, *avcp,
	       ICL_TYPE_INT32, treq.uid, ICL_TYPE_FID, &vfid);
    code = afs_CheckCode(code, &treq, 42);
    return code;
}


#ifdef MP			/* locked versions of vfs operations. */

/* wrappers for vfs calls */
#ifdef AFS_SGI64_ENV
#define AFS_MP_VFS_ARG(A) bhv_desc_t A
#else
#define AFS_MP_VFS_ARG(A) struct vfs A
#endif

int
mp_afs_mount(struct vfs *a, struct vnode *b, struct mounta *c,
#ifdef AFS_SGI65_ENV
	     char *d,
#endif
	     struct cred *e)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvfsops.vfs_mount(a, b, c, d
#ifdef AFS_SGI65_ENV
				    , e
#endif
	);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_unmount(AFS_MP_VFS_ARG(*a), int b, struct cred *c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvfsops.vfs_unmount(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_root(AFS_MP_VFS_ARG(*a), struct vnode **b)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvfsops.vfs_root(a, b);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_statvfs(AFS_MP_VFS_ARG(*a), struct statvfs *b, struct vnode *c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvfsops.vfs_statvfs(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_sync(AFS_MP_VFS_ARG(*a),
#ifdef AFS_SGI65_ENV
	    int b,
#else
	    short b,
#endif
	    struct cred *c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvfsops.vfs_sync(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

int
mp_afs_vget(AFS_MP_VFS_ARG(*a), struct vnode **b, struct fid *c)
{
    int rv;
    AFS_GLOCK();
    rv = afs_lockedvfsops.vfs_vget(a, b, c);
    AFS_GUNLOCK();
    return rv;
}

struct vfsops Afs_vfsops = {
#ifdef AFS_SGI64_ENV
#ifdef AFS_SGI65_ENV
    BHV_IDENTITY_INIT_POSITION(VFS_POSITION_BASE),
#else
    VFS_POSITION_BASE,
#endif
#endif
    mp_afs_mount,
#ifdef AFS_SGI64_ENV
    fs_nosys,			/* rootinit */
    fs_nosys,			/* mntupdate */
    fs_dounmount,
#endif
    mp_afs_unmount,
    mp_afs_root,
    mp_afs_statvfs,
    mp_afs_sync,
    mp_afs_vget,
    fs_nosys,			/* mountroot */
#ifdef AFS_SGI65_ENV
    fs_nosys,			/* realvfsops */
    fs_import,			/* import */
    fs_nosys,			/* quotactl */
#else
    fs_nosys,			/* swapvp */
#endif
};

#endif /* MP */
