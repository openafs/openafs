/*
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/ubc.h>
#if defined(AFS_DARWIN70_ENV)
#include <vfs/vfs_support.h>
#endif /* defined(AFS_DARWIN70_ENV) */
#ifdef AFS_DARWIN80_ENV
#include <sys/vnode_if.h>
#endif

#ifdef AFS_DARWIN80_ENV
#define VOPPREF(x) &vnop_ ## x
#define VOPPROT(x) vnop_ ## x
#else
#define VOPPREF(x) &vop_ ## x
#define VOPPROT(x) vop_ ## x
#endif

int afs_vop_lookup(struct VOPPROT(lookup_args) *);
int afs_vop_create(struct VOPPROT(create_args) *);
int afs_vop_mknod(struct VOPPROT(mknod_args) *);
int afs_vop_open(struct VOPPROT(open_args) *);
int afs_vop_close(struct VOPPROT(close_args) *);
int afs_vop_access(struct VOPPROT(access_args) *);
int afs_vop_getattr(struct VOPPROT(getattr_args) *);
int afs_vop_setattr(struct VOPPROT(setattr_args) *);
int afs_vop_read(struct VOPPROT(read_args) *);
int afs_vop_write(struct VOPPROT(write_args) *);
int afs_vop_pagein(struct VOPPROT(pagein_args) *);
int afs_vop_pageout(struct VOPPROT(pageout_args) *);
int afs_vop_ioctl(struct VOPPROT(ioctl_args) *);
int afs_vop_select(struct VOPPROT(select_args) *);
int afs_vop_mmap(struct VOPPROT(mmap_args) *);
int afs_vop_fsync(struct VOPPROT(fsync_args) *);
int afs_vop_remove(struct VOPPROT(remove_args) *);
int afs_vop_link(struct VOPPROT(link_args) *);
int afs_vop_rename(struct VOPPROT(rename_args) *);
int afs_vop_mkdir(struct VOPPROT(mkdir_args) *);
int afs_vop_rmdir(struct VOPPROT(rmdir_args) *);
int afs_vop_symlink(struct VOPPROT(symlink_args) *);
int afs_vop_readdir(struct VOPPROT(readdir_args) *);
int afs_vop_readlink(struct VOPPROT(readlink_args) *);
#if !defined(AFS_DARWIN70_ENV)
extern int ufs_abortop(struct vop_abortop_args *);
#endif /* !defined(AFS_DARWIN70_ENV) */
int afs_vop_inactive(struct VOPPROT(inactive_args) *);
int afs_vop_reclaim(struct VOPPROT(reclaim_args) *);
int afs_vop_strategy(struct VOPPROT(strategy_args) *);
int afs_vop_pathconf(struct VOPPROT(pathconf_args) *);
int afs_vop_advlock(struct VOPPROT(advlock_args) *);
int afs_vop_blktooff __P((struct VOPPROT(blktooff_args) *));
int afs_vop_offtoblk __P((struct VOPPROT(offtoblk_args) *));
#ifndef AFS_DARWIN80_ENV
int afs_vop_truncate(struct VOPPROT(truncate_args) *);
int afs_vop_update(struct VOPPROT(update_args) *);
int afs_vop_lock(struct VOPPROT(lock_args) *);
int afs_vop_unlock(struct VOPPROT(unlock_args) *);
int afs_vop_bmap(struct VOPPROT(bmap_args) *);
int afs_vop_seek(struct VOPPROT(seek_args) *);
int afs_vop_cmap __P((struct VOPPROT(cmap_args) *));
int afs_vop_print(struct VOPPROT(print_args) *);
int afs_vop_islocked(struct VOPPROT(islocked_args) *);
#endif

#define afs_vop_opnotsupp \
	((int (*) __P((struct  vop_reallocblks_args *)))eopnotsupp)
#define afs_vop_valloc afs_vop_opnotsupp
#define afs_vop_vfree afs_vop_opnotsupp
#define afs_vop_blkatoff afs_vop_opnotsupp
#define afs_vop_reallocblks afs_vop_opnotsupp

/* Global vfs data structures for AFS. */
int (**afs_vnodeop_p) ();

#define VOPFUNC int (*)(void *)

struct vnodeopv_entry_desc afs_vnodeop_entries[] = {
    {VOPPREF(default_desc), (VOPFUNC)vn_default_error},
    {VOPPREF(lookup_desc), (VOPFUNC)afs_vop_lookup},	/* lookup */
    {VOPPREF(create_desc), (VOPFUNC)afs_vop_create},	/* create */
    {VOPPREF(mknod_desc), (VOPFUNC)afs_vop_mknod},	/* mknod */
    {VOPPREF(open_desc), (VOPFUNC)afs_vop_open},	/* open */
    {VOPPREF(close_desc), (VOPFUNC)afs_vop_close},	/* close */
    {VOPPREF(access_desc), (VOPFUNC)afs_vop_access},	/* access */
    {VOPPREF(getattr_desc), (VOPFUNC)afs_vop_getattr},	/* getattr */
    {VOPPREF(setattr_desc), (VOPFUNC)afs_vop_setattr},	/* setattr */
    {VOPPREF(read_desc), (VOPFUNC)afs_vop_read},	/* read */
    {VOPPREF(write_desc), (VOPFUNC)afs_vop_write},	/* write */
    {VOPPREF(pagein_desc), (VOPFUNC)afs_vop_pagein},	/* read */
    {VOPPREF(pageout_desc), (VOPFUNC)afs_vop_pageout},	/* write */
    {VOPPREF(ioctl_desc), (VOPFUNC)afs_vop_ioctl},	/* XXX ioctl */
    {VOPPREF(select_desc), (VOPFUNC)afs_vop_select},	/* select */
    {VOPPREF(mmap_desc), (VOPFUNC)afs_vop_mmap},	/* mmap */
    {VOPPREF(fsync_desc), (VOPFUNC)afs_vop_fsync},	/* fsync */
#ifndef AFS_DARWIN80_ENV
    {VOPPREF(seek_desc), (VOPFUNC)afs_vop_seek},	/* seek */
#endif
    {VOPPREF(remove_desc), (VOPFUNC)afs_vop_remove},	/* remove */
    {VOPPREF(link_desc), (VOPFUNC)afs_vop_link},	/* link */
    {VOPPREF(rename_desc), (VOPFUNC)afs_vop_rename},	/* rename */
    {VOPPREF(mkdir_desc), (VOPFUNC)afs_vop_mkdir},	/* mkdir */
    {VOPPREF(rmdir_desc), (VOPFUNC)afs_vop_rmdir},	/* rmdir */
    {VOPPREF(symlink_desc), (VOPFUNC)afs_vop_symlink},	/* symlink */
    {VOPPREF(readdir_desc), (VOPFUNC)afs_vop_readdir},	/* readdir */
    {VOPPREF(readlink_desc), (VOPFUNC)afs_vop_readlink},	/* readlink */
#ifndef AFS_DARWIN80_ENV
#if defined(AFS_DARWIN70_ENV)
    {VOPPREF(abortop_desc), (VOPFUNC)nop_abortop },             /* abortop */
#else /* ! defined(AFS_DARWIN70_ENV) */
    /* Yes, we use the ufs_abortop call.  It just releases the namei
     * buffer stuff */
    {VOPPREF(abortop_desc), (VOPFUNC)ufs_abortop},	/* abortop */
#endif /* defined(AFS_DARWIN70_ENV) */
#endif
    {VOPPREF(inactive_desc), (VOPFUNC)afs_vop_inactive},	/* inactive */
    {VOPPREF(reclaim_desc), (VOPFUNC)afs_vop_reclaim},	/* reclaim */
#ifndef AFS_DARWIN80_ENV
    {VOPPREF(lock_desc), (VOPFUNC)afs_vop_lock},	/* lock */
    {VOPPREF(unlock_desc), (VOPFUNC)afs_vop_unlock},	/* unlock */
    {VOPPREF(bmap_desc), (VOPFUNC)afs_vop_bmap},	/* bmap */
#endif
#ifdef AFS_DARWIN80_ENV
    {VOPPREF(strategy_desc), (VOPFUNC)err_strategy},	/* strategy */
#else
    {VOPPREF(strategy_desc), (VOPFUNC)afs_vop_strategy},	/* strategy */
#endif
#ifndef AFS_DARWIN80_ENV
    {VOPPREF(print_desc), (VOPFUNC)afs_vop_print},	/* print */
    {VOPPREF(islocked_desc), (VOPFUNC)afs_vop_islocked},	/* islocked */
#endif
    {VOPPREF(pathconf_desc), (VOPFUNC)afs_vop_pathconf},	/* pathconf */
    {VOPPREF(advlock_desc), (VOPFUNC)afs_vop_advlock},	/* advlock */
#ifndef AFS_DARWIN80_ENV
    {VOPPREF(blkatoff_desc), (VOPFUNC)afs_vop_blkatoff},	/* blkatoff */
    {VOPPREF(valloc_desc), (VOPFUNC)afs_vop_valloc},	/* valloc */
    {VOPPREF(reallocblks_desc), (VOPFUNC)afs_vop_reallocblks},	/* reallocblks */
    {VOPPREF(vfree_desc), (VOPFUNC)afs_vop_vfree},	/* vfree */
    {VOPPREF(update_desc), (VOPFUNC)afs_vop_update},	/* update */
    {VOPPREF(cmap_desc), (VOPFUNC)afs_vop_cmap},	/* cmap */
#endif
    {VOPPREF(truncate_desc), (VOPFUNC)afs_vop_truncate},	/* truncate */
    {VOPPREF(blktooff_desc), (VOPFUNC)afs_vop_blktooff},	/* blktooff */
    {VOPPREF(offtoblk_desc), (VOPFUNC)afs_vop_offtoblk},	/* offtoblk */
    {VOPPREF(bwrite_desc), (VOPFUNC)vn_bwrite},
    {(struct vnodeop_desc *)NULL, (void (*)())NULL}
};
struct vnodeopv_desc afs_vnodeop_opv_desc =
    { &afs_vnodeop_p, afs_vnodeop_entries };

#define GETNAME()       \
    struct componentname *cnp = ap->a_cnp; \
    char *name; \
    MALLOC(name, char *, cnp->cn_namelen+1, M_TEMP, M_WAITOK); \
    memcpy(name, cnp->cn_nameptr, cnp->cn_namelen); \
    name[cnp->cn_namelen] = '\0'

#define DROPNAME() FREE(name, M_TEMP)

void 
darwin_vn_hold(struct vnode *vp)
{
    int haveGlock=ISAFS_GLOCK(); 
    struct vcache *tvc = VTOAFS(vp);

    tvc->states |= CUBCinit;
    if (haveGlock) AFS_GUNLOCK(); 

    /* vget needed for 0 ref'd vnode in GetVCache to not panic in vref.
       vref needed for multiref'd vnode in vnop_remove not to deadlock
       ourselves during vop_inactive, except we also need to not reinst
       the ubc... so we just call VREF there now anyway. */

    if (VREFCOUNT_GT(tvc, 0))
#ifdef AFS_DARWIN80_ENV
	vnode_ref(vp);
#else
	VREF(((struct vnode *)(vp))); 
#endif
    else
#ifdef AFS_DARWIN80_ENV
	vnode_get(vp);
#else
	afs_vget(afs_globalVFS, 0, (vp));
#endif

    if (UBCINFOMISSING(vp) || UBCINFORECLAIMED(vp)) {
	ubc_info_init(vp); 
    }

    if (haveGlock) AFS_GLOCK(); 
    tvc->states &= ~CUBCinit;
}

int
afs_vop_lookup(ap)
     struct VOPPROT(lookup_args)/* {
				 * struct vnodeop_desc * a_desc;
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error;
    struct vcache *vcp;
    struct vnode *vp, *dvp;
    register int flags = ap->a_cnp->cn_flags;
    int lockparent;		/* 1 => lockparent flag is set */
    int wantparent;		/* 1 => wantparent or lockparent flag */
    struct proc *p;
    GETNAME();
    p = ctx_proc;

    lockparent = flags & LOCKPARENT;
    wantparent = flags & (LOCKPARENT | WANTPARENT);

    if (vnode_type(ap->a_dvp) != VDIR) {
	*ap->a_vpp = 0;
	DROPNAME();
	return ENOTDIR;
    }
    dvp = ap->a_dvp;
    if (flags & ISDOTDOT)
	VOP_UNLOCK(dvp, 0, p);
    AFS_GLOCK();
    error = afs_lookup(VTOAFS(dvp), name, &vcp, ctx_cred);
    AFS_GUNLOCK();
    if (error) {
#ifndef AFS_DARWIN80_ENV
	if (flags & ISDOTDOT)
	    VOP_LOCK(dvp, LK_EXCLUSIVE | LK_RETRY, p);
#endif
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
	    && (flags & ISLASTCN) && error == ENOENT)
	    error = EJUSTRETURN;
#ifndef AFS_DARWIN80_ENV
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	    cnp->cn_flags |= SAVENAME;
#endif
	DROPNAME();
	*ap->a_vpp = 0;
	return (error);
    }
    vp = AFSTOV(vcp);		/* always get a node if no error */
    vp->v_vfsp = dvp->v_vfsp;

    if (UBCINFOMISSING(vp) ||
	UBCINFORECLAIMED(vp)) {
	    ubc_info_init(vp);
    }

    /* The parent directory comes in locked.  We unlock it on return
     * unless the caller wants it left locked.
     * we also always return the vnode locked. */

#ifndef AFS_DARWIN80_ENV
    if (flags & ISDOTDOT) {
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	/* always return the child locked */
	if (lockparent && (flags & ISLASTCN)
	    && (error = vn_lock(dvp, LK_EXCLUSIVE, p))) {
	    vput(vp);
	    DROPNAME();
	    return (error);
	}
    } else if (vp == dvp) {
	/* they're the same; afs_lookup() already ref'ed the leaf.
	 * It came in locked, so we don't need to ref OR lock it */
    } else {
	if (!lockparent || !(flags & ISLASTCN))
	    VOP_UNLOCK(dvp, 0, p);	/* done with parent. */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	/* always return the child locked */
    }
#endif
    *ap->a_vpp = vp;

#ifndef AFS_DARWIN80_ENV
    if ((cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN)
	 || (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))))
	cnp->cn_flags |= SAVENAME;
#endif

    DROPNAME();
    return error;
}

int
afs_vop_create(ap)
     struct VOPPROT(create_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    int error = 0;
    struct vcache *vcp;
    register struct vnode *dvp = ap->a_dvp;
    struct proc *p;
    GETNAME();
    p = cnp->cn_proc;

    /* vnode layer handles excl/nonexcl */
    AFS_GLOCK();
    error =
	afs_create(VTOAFS(dvp), name, ap->a_vap, NONEXCL, ap->a_vap->va_mode,
		   &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return (error);
    }

    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	(*ap->a_vpp)->v_vfsp = dvp->v_vfsp;
	vn_lock(*ap->a_vpp, LK_EXCLUSIVE | LK_RETRY, p);
	if (UBCINFOMISSING(*ap->a_vpp) || UBCINFORECLAIMED(*ap->a_vpp)) {
	    vcp->states |= CUBCinit;
	    ubc_info_init(*ap->a_vpp);
	    vcp->states &= ~CUBCinit;
	}
    } else
	*ap->a_vpp = 0;

#ifndef AFS_DARWIN80_ENV
    if ((cnp->cn_flags & SAVESTART) == 0)
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
    vput(dvp);
    DROPNAME();
    return error;
}

int
afs_vop_mknod(ap)
     struct VOPPROT(mknod_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(ap->a_cnp->cn_pnbuf, ap->a_cnp->cn_pnlen, M_NAMEI);
#endif
    vput(ap->a_dvp);
    return (ENODEV);
}

int
afs_vop_open(ap)
     struct VOPPROT(open_args)	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int error;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);
#ifdef AFS_DARWIN14_ENV
    int didhold = 0;
    /*----------------------------------------------------------------
     * osi_VM_TryReclaim() removes the ubcinfo of a vnode, but that vnode
     * can later be passed to vn_open(), which will skip the call to
     * ubc_hold(), and when the ubcinfo is later added, the ui_refcount
     * will be off.  So we compensate by calling ubc_hold() ourselves
     * when ui_refcount is less than 2.  If an error occurs in afs_open()
     * we must call ubc_rele(), which is what vn_open() would do if it
     * was able to call ubc_hold() in the first place.
     *----------------------------------------------------------------*/
    if (vp->v_type == VREG && !(vp->v_flag & VSYSTEM)
      && vp->v_ubcinfo->ui_refcount < 2)
	didhold = ubc_hold(vp);
#endif /* AFS_DARWIN14_ENV */
    AFS_GLOCK();
    error = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != vp)
	panic("AFS open changed vnode!");
#endif
    osi_FlushPages(vc, ap->a_cred);
    AFS_GUNLOCK();
#ifdef AFS_DARWIN14_ENV
    if (error && didhold)
	ubc_rele(vp);
#endif /* AFS_DARWIN14_ENV */
    return error;
}

int
afs_vop_close(ap)
     struct VOPPROT(close_args)	/* {
				 * struct vnode *a_vp;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);
    AFS_GLOCK();
    if (ap->a_cred)
	code = afs_close(avc, ap->a_fflag, ap->a_cred, ap->a_p);
    else
	code = afs_close(avc, ap->a_fflag, &afs_osi_cred, ap->a_p);
    osi_FlushPages(avc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    AFS_GUNLOCK();

    return code;
}

int
afs_vop_access(ap)
     struct VOPPROT(access_args)	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int code;
    AFS_GLOCK();
    code = afs_access(VTOAFS(ap->a_vp), ap->a_mode, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_getattr(ap)
     struct VOPPROT(getattr_args)	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int code;

    AFS_GLOCK();
    code = afs_getattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_setattr(ap)
     struct VOPPROT(setattr_args)	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int code;
    AFS_GLOCK();
    code = afs_setattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_read(ap)
     struct VOPPROT(read_args)	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);
    AFS_GLOCK();
    osi_FlushPages(avc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    code = afs_read(avc, ap->a_uio, ap->a_cred, 0, 0, 0);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_pagein(ap)
     struct VOPPROT(pagein_args)	/* {
				 * struct vnode *a_vp;
				 * upl_t a_pl;
				 * vm_offset_t a_pl_offset;
				 * off_t a_f_offset;
				 * size_t a_size;
				 * struct ucred *a_cred;
				 * int a_flags;
				 * } */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    size_t size = ap->a_size;
    off_t f_offset = ap->a_f_offset;
    vm_offset_t pl_offset = ap->a_pl_offset;
    int flags = ap->a_flags;
    struct ucred *cred;
    vm_offset_t ioaddr;
    struct uio auio;
    struct iovec aiov;
    struct uio *uio = &auio;
    int nocommit = flags & UPL_NOCOMMIT;

    int code;
    struct vcache *tvc = VTOAFS(vp);

    if (UBCINVALID(vp)) {
#if DIAGNOSTIC
	panic("afs_vop_pagein: invalid vp");
#endif /* DIAGNOSTIC */
	return (EPERM);
    }

    UBCINFOCHECK("afs_vop_pagein", vp);
    if (pl == (upl_t) NULL) {
	panic("afs_vop_pagein: no upl");
    }

    cred = ubc_getcred(vp);
    if (cred == NOCRED)
	cred = ap->a_cred;

    if (size == 0) {
	if (!nocommit)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
	return (0);
    }
    if (f_offset < 0) {
	if (!nocommit)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
	return (EINVAL);
    }
    if (f_offset & PAGE_MASK)
	panic("afs_vop_pagein: offset not page aligned");

    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = f_offset;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_rw = UIO_READ;
    auio.uio_procp = NULL;
    kernel_upl_map(kernel_map, pl, &ioaddr);
    ioaddr += pl_offset;
    auio.uio_resid = aiov.iov_len = size;
    aiov.iov_base = (caddr_t) ioaddr;
    AFS_GLOCK();
    osi_FlushPages(tvc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    code = afs_read(tvc, uio, cred, 0, 0, 0);
    if (code == 0) {
	ObtainWriteLock(&tvc->lock, 2);
	tvc->states |= CMAPPED;
	ReleaseWriteLock(&tvc->lock);
    }
    AFS_GUNLOCK();

    /* Zero out rest of last page if there wasn't enough data in the file */
    if (code == 0 && auio.uio_resid > 0)
	memset(aiov.iov_base, 0, auio.uio_resid);

    kernel_upl_unmap(kernel_map, pl);
    if (!nocommit) {
	if (code)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
	else
	    kernel_upl_commit_range(pl, pl_offset, size,
				    UPL_COMMIT_CLEAR_DIRTY |
				    UPL_COMMIT_FREE_ON_EMPTY,
				    UPL_GET_INTERNAL_PAGE_LIST(pl),
				    MAX_UPL_TRANSFER);
    }
    return code;
}

int
afs_vop_write(ap)
     struct VOPPROT(write_args)	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int code;
    struct vcache *avc = VTOAFS(ap->a_vp);
    void *object;
    AFS_GLOCK();
    osi_FlushPages(avc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    if (UBCINFOEXISTS(ap->a_vp)) {
	ubc_clean(ap->a_vp, 1);
    }
    if (UBCINFOEXISTS(ap->a_vp))
	osi_VM_NukePages(ap->a_vp, ap->a_uio->uio_offset,
			 ap->a_uio->uio_resid);
    code =
	afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_pageout(ap)
     struct VOPPROT(pageout_args)	/* {
				 * struct vnode *a_vp;
				 * upl_t   a_pl,
				 * vm_offset_t   a_pl_offset,
				 * off_t         a_f_offset,
				 * size_t        a_size,
				 * struct ucred *a_cred,
				 * int           a_flags
				 * } */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    size_t size = ap->a_size;
    off_t f_offset = ap->a_f_offset;
    vm_offset_t pl_offset = ap->a_pl_offset;
    int flags = ap->a_flags;
    struct ucred *cred;
    vm_offset_t ioaddr;
    struct uio auio;
    struct iovec aiov;
    struct uio *uio = &auio;
    int nocommit = flags & UPL_NOCOMMIT;
    int iosize;

    int code;
    struct vcache *tvc = VTOAFS(vp);

    if (UBCINVALID(vp)) {
#if DIAGNOSTIC
	panic("afs_vop_pageout: invalid vp");
#endif /* DIAGNOSTIC */
	return (EPERM);
    }

    UBCINFOCHECK("afs_vop_pageout", vp);
    if (pl == (upl_t) NULL) {
	panic("afs_vop_pageout: no upl");
    }
#if 1
    {
	int lbn, s;
	struct buf *bp;
	int biosize = DEV_BSIZE;

	lbn = f_offset / DEV_BSIZE;

	for (iosize = size; iosize > 0; iosize -= biosize, lbn++) {

	    s = splbio();
	    if (bp = incore(vp, lbn)) {
		if (ISSET(bp->b_flags, B_BUSY))
		    panic("nfs_pageout: found BUSY buffer incore\n");

		bremfree(bp);
		SET(bp->b_flags, (B_BUSY | B_INVAL));
		brelse(bp);
	    }
	    splx(s);
	}
    }
#endif
    cred = ubc_getcred(vp);
    if (cred == NOCRED)
	cred = ap->a_cred;

    if (size == 0) {
	if (!nocommit)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	return (0);
    }
    if (flags & (IO_APPEND | IO_SYNC))
	panic("nfs_pageout: (IO_APPEND | IO_SYNC)");
    if (f_offset < 0) {
	if (!nocommit)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	return (EINVAL);
    }
    if (f_offset >= tvc->m.Length) {
	if (!nocommit)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	return (EINVAL);
    }

    if (f_offset & PAGE_MASK)
	panic("afs_vop_pageout: offset not page aligned");

    /* size will always be a multiple of PAGE_SIZE */
    /* pageout isn't supposed to extend files */
    if (f_offset + size > tvc->m.Length) 
        iosize = tvc->m.Length - f_offset;
    else
        iosize = size;

    if (size > (iosize + (PAGE_SIZE - 1)) & ~PAGE_MASK && !nocommit)  {
            int iosize_rnd=(iosize + (PAGE_SIZE - 1)) & ~PAGE_MASK;
	    kernel_upl_abort_range(pl, pl_offset + iosize_rnd,
                                   size - iosize_rnd,
				   UPL_ABORT_FREE_ON_EMPTY);
    }
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = f_offset;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_rw = UIO_WRITE;
    auio.uio_procp = NULL;
    kernel_upl_map(kernel_map, pl, &ioaddr);
    ioaddr += pl_offset;
    auio.uio_resid = aiov.iov_len = iosize;
    aiov.iov_base = (caddr_t) ioaddr;
#if 1				/* USV [ */
    {
	/* 
	 * check for partial page and clear the
	 * contents past end of the file before
	 * releasing it in the VM page cache
	 */
	if ((f_offset < tvc->m.Length) && (f_offset + size) > tvc->m.Length) {
	    size_t io = tvc->m.Length - f_offset;

	    memset((caddr_t) (ioaddr + pl_offset + io), 0, size - io);
	}
    }
#endif /* ] USV */

    AFS_GLOCK();
    osi_FlushPages(tvc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    ObtainWriteLock(&tvc->lock, 1);
    afs_FakeOpen(tvc);
    ReleaseWriteLock(&tvc->lock);

    code = afs_write(tvc, uio, flags, cred, 0);

    ObtainWriteLock(&tvc->lock, 1);
    afs_FakeClose(tvc, cred);
    ReleaseWriteLock(&tvc->lock);
    AFS_GUNLOCK();
    kernel_upl_unmap(kernel_map, pl);
    if (!nocommit) {
	if (code)
	    kernel_upl_abort_range(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	else
	    kernel_upl_commit_range(pl, pl_offset, size,
				    UPL_COMMIT_CLEAR_DIRTY |
				    UPL_COMMIT_FREE_ON_EMPTY,
				    UPL_GET_INTERNAL_PAGE_LIST(pl),
				    MAX_UPL_TRANSFER);
    }

    return code;
}

int
afs_vop_ioctl(ap)
     struct VOPPROT(ioctl_args)	/* {
				 * struct vnode *a_vp;
				 * int  a_command;
				 * caddr_t  a_data;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    struct vcache *tvc = VTOAFS(ap->a_vp);
    struct afs_ioctl data;
    int error = 0;

    /* in case we ever get in here... */

    AFS_STATCNT(afs_ioctl);
    if (((ap->a_command >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
	AFS_GLOCK();
	error = HandleIoctl(tvc, (struct file *)0 /*Not used */ ,
			    ap->a_command, ap->a_data);
	AFS_GUNLOCK();
	return (error);
    } else {
	/* No-op call; just return. */
	return (ENOTTY);
    }
}

/* ARGSUSED */
int
afs_vop_select(ap)
     struct VOPPROT(select_args)	/* {
				 * struct vnode *a_vp;
				 * int  a_which;
				 * int  a_fflags;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    /*
     * We should really check to see if I/O is possible.
     */
    return (1);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
afs_vop_mmap(ap)
     struct VOPPROT(mmap_args)	/* {
				 * struct vnode *a_vp;
				 * int  a_fflags;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    return (EINVAL);
}

int
afs_vop_fsync(ap)
     struct VOPPROT(fsync_args)	/* {
				 * struct vnode *a_vp;
				 * struct ucred *a_cred;
				 * int a_waitfor;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int wait = ap->a_waitfor == MNT_WAIT;
    int error;
    register struct vnode *vp = ap->a_vp;
    int haveGlock = ISAFS_GLOCK();

    /* afs_vop_lookup glocks, can call us through vinvalbuf from GetVCache */
    if (!haveGlock) AFS_GLOCK();
    if (ap->a_cred)
	error = afs_fsync(VTOAFS(vp), ap->a_cred);
    else
	error = afs_fsync(VTOAFS(vp), &afs_osi_cred);
    if (!haveGlock) AFS_GUNLOCK();
    return error;
}

int
afs_vop_seek(ap)
     struct VOPPROT(seek_args)	/* {
				 * struct vnode *a_vp;
				 * off_t  a_oldoff;
				 * off_t  a_newoff;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    if (ap->a_newoff > ULONG_MAX)	/* AFS doesn't support 64-bit offsets */
	return EINVAL;
    return (0);
}

int
afs_vop_remove(ap)
     struct VOPPROT(remove_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    register struct vnode *vp = ap->a_vp;
    register struct vnode *dvp = ap->a_dvp;

    GETNAME();
    AFS_GLOCK();
    error = afs_remove(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    cache_purge(vp);
    vput(dvp);
    if (!error) {
        /* necessary so we don't deadlock ourselves in vclean */
        VOP_UNLOCK(vp, 0, cnp->cn_proc);

	/* If crashes continue in ubc_hold, comment this out */
        (void)ubc_uncache(vp);
    }

    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);

#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
    DROPNAME();
    return error;
}

int
afs_vop_link(ap)
     struct VOPPROT(link_args)	/* {
				 * struct vnode *a_vp;
				 * struct vnode *a_tdvp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    register struct vnode *dvp = ap->a_tdvp;
    register struct vnode *vp = ap->a_vp;
    struct proc *p;

    GETNAME();
    p = cnp->cn_proc;
    if (vp->v_type == VDIR) {
	VOP_ABORTOP(vp, cnp);
	error = EISDIR;
	goto out;
    }
    if (error = vn_lock(vp, LK_EXCLUSIVE, p)) {
	VOP_ABORTOP(dvp, cnp);
	goto out;
    }
    AFS_GLOCK();
    error = afs_link(VTOAFS(vp), VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
    if (dvp != vp)
	VOP_UNLOCK(vp, 0, p);
  out:
    vput(dvp);
    DROPNAME();
    return error;
}

int
afs_vop_rename(ap)
     struct VOPPROT(rename_args)	/* {
				 * struct vnode *a_fdvp;
				 * struct vnode *a_fvp;
				 * struct componentname *a_fcnp;
				 * struct vnode *a_tdvp;
				 * struct vnode *a_tvp;
				 * struct componentname *a_tcnp;
				 * } */ *ap;
{
    int error = 0;
    struct componentname *fcnp = ap->a_fcnp;
    char *fname;
    struct componentname *tcnp = ap->a_tcnp;
    char *tname;
    struct vnode *tvp = ap->a_tvp;
    register struct vnode *tdvp = ap->a_tdvp;
    struct vnode *fvp = ap->a_fvp;
    register struct vnode *fdvp = ap->a_fdvp;
    struct proc *p = fcnp->cn_proc;

    /* Check for cross-device rename.
     * For AFS, this means anything not in AFS-space
     */
    if ((0 != strcmp(tdvp->v_mount->mnt_stat.f_fstypename, "afs")) ||
	(tvp && (0 != strcmp(tvp->v_mount->mnt_stat.f_fstypename, "afs")))) {
	error = EXDEV;
	goto abortit;
    }

    /*
     * if fvp == tvp, we're just removing one name of a pair of
     * directory entries for the same element.  convert call into rename.
     ( (pinched from NetBSD 1.0's ufs_rename())
     */
    if (fvp == tvp) {
	if (fvp->v_type == VDIR) {
	    error = EINVAL;
	  abortit:
	    VOP_ABORTOP(tdvp, tcnp);	/* XXX, why not in NFS? */
	    if (tdvp == tvp)
		vrele(tdvp);
	    else
		vput(tdvp);
	    if (tvp)
		vput(tvp);
	    VOP_ABORTOP(fdvp, fcnp);	/* XXX, why not in NFS? */
	    vrele(fdvp);
	    vrele(fvp);
	    return (error);
	}

	/* Release destination completely. */
	VOP_ABORTOP(tdvp, tcnp);
	vput(tdvp);
	vput(tvp);

	/* Delete source. */
	vrele(fdvp);
	vrele(fvp);
	fcnp->cn_flags &= ~MODMASK;
	fcnp->cn_flags |= LOCKPARENT | LOCKLEAF;
	if ((fcnp->cn_flags & SAVESTART) == 0)
	    panic("afs_rename: lost from startdir");
	fcnp->cn_nameiop = DELETE;

        VREF(fdvp); 
        error=relookup(fdvp, &fvp, fcnp);
        if (error == 0)
	    vrele(fdvp);
        if (fvp == NULL) {
	    return (ENOENT);
        }
        
        error=VOP_REMOVE(fdvp, fvp, fcnp);
        if (fdvp == fvp)
            vrele(fdvp);
        else
            vput(fdvp);
        vput(fvp);
        return (error);
    }
    if (error = vn_lock(fvp, LK_EXCLUSIVE, p))
	goto abortit;

    MALLOC(fname, char *, fcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    memcpy(fname, fcnp->cn_nameptr, fcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    MALLOC(tname, char *, tcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    memcpy(tname, tcnp->cn_nameptr, tcnp->cn_namelen);
    tname[tcnp->cn_namelen] = '\0';


    AFS_GLOCK();
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    error =
	afs_rename(VTOAFS(fdvp), fname, VTOAFS(tdvp), tname, tcnp->cn_cred);
    AFS_GUNLOCK();

    VOP_UNLOCK(fvp, 0, p);
    FREE(fname, M_TEMP);
    FREE(tname, M_TEMP);
    if (error)
	goto abortit;		/* XXX */
    if (tdvp == tvp)
	vrele(tdvp);
    else
	vput(tdvp);
    if (tvp)
	vput(tvp);
    vrele(fdvp);
    vrele(fvp);
    return error;
}

int
afs_vop_mkdir(ap)
     struct VOPPROT(mkdir_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    register struct vnode *dvp = ap->a_dvp;
    register struct vattr *vap = ap->a_vap;
    int error = 0;
    struct vcache *vcp;
    struct proc *p;

    GETNAME();
    p = cnp->cn_proc;
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_vop_mkdir: no name");
#endif
    AFS_GLOCK();
    error = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return (error);
    }
    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	(*ap->a_vpp)->v_vfsp = dvp->v_vfsp;
	vn_lock(*ap->a_vpp, LK_EXCLUSIVE | LK_RETRY, p);
    } else
	*ap->a_vpp = 0;
    DROPNAME();
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
    vput(dvp);
    return error;
}

int
afs_vop_rmdir(ap)
     struct VOPPROT(rmdir_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    register struct vnode *vp = ap->a_vp;
    register struct vnode *dvp = ap->a_dvp;

    GETNAME();
    if (dvp == vp) {
	vrele(dvp);
	vput(vp);
#ifndef AFS_DARWIN80_ENV
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
	DROPNAME();
	return (EINVAL);
    }

    AFS_GLOCK();
    error = afs_rmdir(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    vput(dvp);
    vput(vp);
    return error;
}

int
afs_vop_symlink(ap)
     struct VOPPROT(symlink_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * char *a_target;
				 * } */ *ap;
{
    register struct vnode *dvp = ap->a_dvp;
    int error = 0;
    /* NFS ignores a_vpp; so do we. */

    GETNAME();
    AFS_GLOCK();
    error =
	afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
    vput(dvp);
    return error;
}

int
afs_vop_readdir(ap)
     struct VOPPROT(readdir_args)	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * struct ucred *a_cred;
				 * int *a_eofflag;
				 * u_long *a_cookies;
				 * int ncookies;
				 * } */ *ap;
{
    int error;
    off_t off;
/*    printf("readdir %x cookies %x ncookies %d\n", ap->a_vp, ap->a_cookies,
	   ap->a_ncookies); */
    off = ap->a_uio->uio_offset;
    AFS_GLOCK();
    error =
	afs_readdir(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, ap->a_eofflag);
    AFS_GUNLOCK();
    if (!error && ap->a_ncookies != NULL) {
	struct uio *uio = ap->a_uio;
	const struct dirent *dp, *dp_start, *dp_end;
	int ncookies;
	u_long *cookies, *cookiep;

	if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
	    panic("afs_readdir: burned cookies");
	dp = (const struct dirent *)
	    ((const char *)uio->uio_iov->iov_base - (uio->uio_offset - off));

	dp_end = (const struct dirent *)uio->uio_iov->iov_base;
	for (dp_start = dp, ncookies = 0; dp < dp_end;
	     dp = (const struct dirent *)((const char *)dp + dp->d_reclen))
	    ncookies++;

	MALLOC(cookies, u_long *, ncookies * sizeof(u_long), M_TEMP,
	       M_WAITOK);
	for (dp = dp_start, cookiep = cookies; dp < dp_end;
	     dp = (const struct dirent *)((const char *)dp + dp->d_reclen)) {
	    off += dp->d_reclen;
	    *cookiep++ = off;
	}
	*ap->a_cookies = cookies;
	*ap->a_ncookies = ncookies;
    }

    return error;
}

int
afs_vop_readlink(ap)
     struct VOPPROT(readlink_args)	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int error;
/*    printf("readlink %x\n", ap->a_vp);*/
    AFS_GLOCK();
    error = afs_readlink(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred);
    AFS_GUNLOCK();
    return error;
}

extern int prtactive;

int
afs_vop_inactive(ap)
     struct VOPPROT(inactive_args)	/* {
				 * struct vnode *a_vp;
				 * struct proc *a_p;
				 * } */ *ap;
{
    register struct vnode *vp = ap->a_vp;

    if (prtactive && vp->v_usecount != 0)
	vprint("afs_vop_inactive(): pushing active", vp);

    AFS_GLOCK();
    afs_InactiveVCache(VTOAFS(vp), 0);	/* decrs ref counts */
    AFS_GUNLOCK();
#ifndef AFS_DARWIN80_ENV
    VOP_UNLOCK(vp, 0, ap->a_p);
#endif
    return 0;
}

int
afs_vop_reclaim(ap)
     struct VOPPROT(reclaim_args)	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    int error = 0;
    int sl;
    register struct vnode *vp = ap->a_vp;
    int haveGlock = ISAFS_GLOCK();
    struct vcache *tvc = VTOAFS(vp);

    cache_purge(vp);		/* just in case... */
    if (!haveGlock)
	AFS_GLOCK();
    error = afs_FlushVCache(VTOAFS(vp), &sl);	/* toss our stuff from vnode */
    if (!haveGlock)
	AFS_GUNLOCK();

    if (!error && vp->v_data)
	panic("afs_reclaim: vnode not cleaned");
    if (!error && (tvc->v != NULL)) 
        panic("afs_reclaim: vcache not cleaned");

#ifdef AFS_DARWIN80_ENV
    /* XXX do we need to call vnode_recycle here? */
#endif

    return error;
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
afs_vop_pathconf(ap)
     struct VOPPROT(pathconf_args)	/* {
				 * struct vnode *a_vp;
				 * int a_name;
				 * int *a_retval;
				 * } */ *ap;
{
    AFS_STATCNT(afs_cntl);
    switch (ap->a_name) {
    case _PC_LINK_MAX:
	*ap->a_retval = LINK_MAX;
	break;
    case _PC_NAME_MAX:
	*ap->a_retval = NAME_MAX;
	break;
    case _PC_PATH_MAX:
	*ap->a_retval = PATH_MAX;
	break;
    case _PC_CHOWN_RESTRICTED:
	*ap->a_retval = 1;
	break;
    case _PC_NO_TRUNC:
	*ap->a_retval = 1;
	break;
    case _PC_PIPE_BUF:
	return EINVAL;
	break;
#if defined(AFS_DARWIN70_ENV)
    case _PC_NAME_CHARS_MAX:
        *ap->a_retval = NAME_MAX;
	break;
    case _PC_CASE_SENSITIVE:
        *ap->a_retval = 1;
	break;
    case _PC_CASE_PRESERVING:
        *ap->a_retval = 1;
	break;
#endif /* defined(AFS_DARWIN70_ENV) */
    default:
	return EINVAL;
    }
    return 0;
}

/*
 * Advisory record locking support (fcntl() POSIX style)
 */
int
afs_vop_advlock(ap)
     struct VOPPROT(advlock_args)	/* {
				 * struct vnode *a_vp;
				 * caddr_t  a_id;
				 * int  a_op;
				 * struct flock *a_fl;
				 * int  a_flags;
				 * } */ *ap;
{
    int error;
    struct proc *p = current_proc();
    struct ucred cr;
    pcred_readlock(p);
    cr = *p->p_cred->pc_ucred;
    pcred_unlock(p);
    AFS_GLOCK();
    error =
	afs_lockctl(VTOAFS(ap->a_vp), ap->a_fl, ap->a_op, &cr, (int)ap->a_id);
    AFS_GUNLOCK();
    return error;
}

int
afs_vop_blktooff(ap)
     struct VOPPROT(blktooff_args)	/* {
				 * struct vnode *a_vp;
				 * daddr_t a_lblkno;
				 * off_t *a_offset;    
				 * } */ *ap;
{
    *ap->a_offset = (off_t) (ap->a_lblkno * DEV_BSIZE);
    return 0;
}

int
afs_vop_offtoblk(ap)
     struct VOPPROT(offtoblk_args)	/* {
				 * struct vnode *a_vp;
				 * off_t a_offset;    
				 * daddr_t *a_lblkno;
				 * } */ *ap;
{
    *ap->a_lblkno = (daddr_t) (ap->a_offset / DEV_BSIZE);

    return (0);
}

#ifndef AFS_DARWIN80_ENV
int
afs_vop_lock(ap)
     struct VOPPROT(lock_args)	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    register struct vcache *avc = VTOAFS(vp);

    if (vp->v_tag == VT_NON)
	return (ENOENT);

    return (lockmgr(&avc->rwlock, ap->a_flags, &vp->v_interlock, ap->a_p));
}

int
afs_vop_unlock(ap)
     struct VOPPROT(unlock_args)	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);

    return (lockmgr
	    (&avc->rwlock, ap->a_flags | LK_RELEASE, &vp->v_interlock,
	     ap->a_p));

}

int
afs_vop_truncate(ap)
     struct VOPPROT(truncate_args)	/* {
				 * struct vnode *a_vp;
				 * off_t a_length;
				 * int a_flags;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    printf("stray afs_vop_truncate\n");
    return EOPNOTSUPP;
}

int
afs_vop_update(ap)
     struct VOPPROT(update_args)	/* {
				 * struct vnode *a_vp;
				 * struct timeval *a_access;
				 * struct timeval *a_modify;
				 * int a_waitfor;
				 * } */ *ap;
{
    printf("stray afs_vop_update\n");
    return EOPNOTSUPP;
}

int
afs_vop_bmap(ap)
     struct VOPPROT(bmap_args)	/* {
				 * struct vnode *a_vp;
				 * daddr_t  a_bn;
				 * struct vnode **a_vpp;
				 * daddr_t *a_bnp;
				 * int *a_runp;
				 * int *a_runb;
				 * } */ *ap;
{
    int error;
    if (ap->a_bnp) {
	*ap->a_bnp = ap->a_bn * (PAGE_SIZE / DEV_BSIZE);
    }
    if (ap->a_vpp) {
	*ap->a_vpp = ap->a_vp;
    }
    if (ap->a_runp != NULL)
	*ap->a_runp = 0;
#ifdef notyet
    if (ap->a_runb != NULL)
	*ap->a_runb = 0;
#endif

    return 0;
}

int
afs_vop_strategy(ap)
     struct VOPPROT(strategy_args)	/* {
				 * struct buf *a_bp;
				 * } */ *ap;
{
    int error;
    AFS_GLOCK();
    error = afs_ustrategy(ap->a_bp);
    AFS_GUNLOCK();
    return error;
}

int
afs_vop_print(ap)
     struct VOPPROT(print_args)	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    register struct vcache *vc = VTOAFS(ap->a_vp);
    int s = vc->states;
    printf("tag %d, fid: %ld.%x.%x.%x, opens %d, writers %d", vp->v_tag,
	   vc->fid.Cell, vc->fid.Fid.Volume, vc->fid.Fid.Vnode,
	   vc->fid.Fid.Unique, vc->opens, vc->execsOrWriters);
    printf("\n  states%s%s%s%s%s", (s & CStatd) ? " statd" : "",
	   (s & CRO) ? " readonly" : "", (s & CDirty) ? " dirty" : "",
	   (s & CMAPPED) ? " mapped" : "",
	   (s & CVFlushed) ? " flush in progress" : "");
    if (UBCISVALID(vp)) {
	printf("\n  UBC: ");
	if (UBCINFOEXISTS(vp)) {
	    printf("exists, ");
#ifdef AFS_DARWIN14_ENV
	    printf("refs %d%s%s", vp->v_ubcinfo->ui_refcount,
		   ubc_issetflags(vp, UI_HASOBJREF) ? " HASOBJREF" : "",
		   ubc_issetflags(vp, UI_WASMAPPED) ? " WASMAPPED" : "");
#else
	    printf("holdcnt %d", vp->v_ubcinfo->ui_holdcnt);
#endif
	} else
	    printf("does not exist");
    }
    printf("\n");
    return 0;
}

int
afs_vop_islocked(ap)
     struct VOPPROT(islocked_args)	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    struct vcache *vc = VTOAFS(ap->a_vp);
    return lockstatus(&vc->rwlock);
}

int
afs_vop_cmap(ap)
     struct VOPPROT(cmap_args)	/* {
				 * struct vnode *a_vp;
				 * off_t a_foffset;    
				 * size_t a_size;
				 * daddr_t *a_bpn;
				 * size_t *a_run;
				 * void *a_poff;
				 * } */ *ap;
{
    *ap->a_bpn = (daddr_t) (ap->a_foffset / DEV_BSIZE);
    *ap->a_run = MAX(ap->a_size, AFS_CHUNKSIZE(ap->a_foffset));
    return 0;
}
#endif

void
afs_darwin_getnewvnode(struct vcache *tvc)
{
#ifdef AFS_DARWIN80_ENV
    vnode_t vp;
    int error;

    error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, afs_globalVFS, &vp);
    vnode_settag(vp, VT_AFS);
#else
    while (getnewvnode(VT_AFS, afs_globalVFS, afs_vnodeop_p, &tvc->v)) {
        /* no vnodes available, force an alloc (limits be damned)! */
	printf("failed to get vnode\n");
    }
    tvc->v->v_data = (void *)tvc;
#endif
}
