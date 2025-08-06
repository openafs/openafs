/*
 * Portions Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 */
#include <afsconfig.h>
#include <afs/param.h>


#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <afs/opr.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/ubc.h>
#include <vfs/vfs_support.h>
#ifdef AFS_DARWIN80_ENV
#include <sys/vnode_if.h>
#include <sys/kauth.h>
#endif

#ifdef AFS_DARWIN80_ENV
#define VOPPREF(x) &vnop_ ## x
#define VOPPROT(x) vnop_ ## x
#define OSI_UPL_ABORT_RANGE(pl, offset, size, flags) \
  ubc_upl_abort_range((pl), (offset), (size), (flags))
#define OSI_UPL_COMMIT_RANGE(pl, offset, size, flags) \
  ubc_upl_commit_range((pl), (offset), (size), (flags))
#define OSI_UPL_MAP(upl, offset) ubc_upl_map((upl), (offset))
#define OSI_UPL_UNMAP(upl) ubc_upl_unmap((upl))
#define VOP_ABORTOP(x, y)
#else
#define VOPPREF(x) &vop_ ## x
#define VOPPROT(x) vop_ ## x
#define OSI_UPL_ABORT_RANGE(pl, offset, size, flags) \
  kernel_upl_abort_range((pl), (offset), (size), (flags))
#define OSI_UPL_COMMIT_RANGE(pl, offset, size, flags) \
  kernel_upl_commit_range((pl), (offset), (size), (flags), \
                          UPL_GET_INTERNAL_PAGE_LIST((pl)),\
                                    MAX_UPL_TRANSFER)
#define OSI_UPL_MAP(upl, offset) kernel_upl_map(kernel_map, (upl), (offset))
#define OSI_UPL_UNMAP(upl) kernel_upl_unmap(kernel_map, (upl))
#endif

extern char afs_zeros[AFS_ZEROS];

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
    {VOPPREF(abortop_desc), (VOPFUNC)nop_abortop },             /* abortop */
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
    {VOPPREF(truncate_desc), (VOPFUNC)afs_vop_truncate},	/* truncate */
#endif
    {VOPPREF(blktooff_desc), (VOPFUNC)afs_vop_blktooff},	/* blktooff */
    {VOPPREF(offtoblk_desc), (VOPFUNC)afs_vop_offtoblk},	/* offtoblk */
    {VOPPREF(bwrite_desc), (VOPFUNC)vn_bwrite},
    {NULL}
};
struct vnodeopv_desc afs_vnodeop_opv_desc =
    { &afs_vnodeop_p, afs_vnodeop_entries };

#ifdef AFS_DARWIN80_ENV
/* vfs structures for incompletely initialized vnodes */
int (**afs_dead_vnodeop_p) ();

struct vnodeopv_entry_desc afs_dead_vnodeop_entries[] = {
    {VOPPREF(default_desc), (VOPFUNC)vn_default_error},
    {VOPPREF(lookup_desc), (VOPFUNC)vn_default_error},	/* lookup */
    {VOPPREF(create_desc), (VOPFUNC)err_create},	/* create */
    {VOPPREF(mknod_desc), (VOPFUNC)err_mknod},	/* mknod */
    {VOPPREF(open_desc), (VOPFUNC)err_open},	/* open */
    {VOPPREF(close_desc), (VOPFUNC)err_close},	/* close */
    {VOPPREF(access_desc), (VOPFUNC)err_access},	/* access */
    {VOPPREF(getattr_desc), (VOPFUNC)err_getattr},	/* getattr */
    {VOPPREF(setattr_desc), (VOPFUNC)err_setattr},	/* setattr */
    {VOPPREF(read_desc), (VOPFUNC)err_read},	/* read */
    {VOPPREF(write_desc), (VOPFUNC)err_write},	/* write */
    {VOPPREF(pagein_desc), (VOPFUNC)err_pagein},	/* read */
    {VOPPREF(pageout_desc), (VOPFUNC)err_pageout},	/* write */
    {VOPPREF(ioctl_desc), (VOPFUNC)err_ioctl},	/* XXX ioctl */
    {VOPPREF(select_desc), (VOPFUNC)nop_select},	/* select */
    {VOPPREF(mmap_desc), (VOPFUNC)err_mmap},	/* mmap */
    {VOPPREF(fsync_desc), (VOPFUNC)err_fsync},	/* fsync */
    {VOPPREF(remove_desc), (VOPFUNC)err_remove},	/* remove */
    {VOPPREF(link_desc), (VOPFUNC)err_link},	/* link */
    {VOPPREF(rename_desc), (VOPFUNC)err_rename},	/* rename */
    {VOPPREF(mkdir_desc), (VOPFUNC)err_mkdir},	/* mkdir */
    {VOPPREF(rmdir_desc), (VOPFUNC)err_rmdir},	/* rmdir */
    {VOPPREF(symlink_desc), (VOPFUNC)err_symlink},	/* symlink */
    {VOPPREF(readdir_desc), (VOPFUNC)err_readdir},	/* readdir */
    {VOPPREF(readlink_desc), (VOPFUNC)err_readlink},	/* readlink */
    {VOPPREF(inactive_desc), (VOPFUNC)afs_vop_inactive},	/* inactive */
    {VOPPREF(reclaim_desc), (VOPFUNC)afs_vop_reclaim},	/* reclaim */
    {VOPPREF(strategy_desc), (VOPFUNC)err_strategy},	/* strategy */
    {VOPPREF(pathconf_desc), (VOPFUNC)err_pathconf},	/* pathconf */
    {VOPPREF(advlock_desc), (VOPFUNC)err_advlock},	/* advlock */
    {VOPPREF(blktooff_desc), (VOPFUNC)err_blktooff},	/* blktooff */
    {VOPPREF(offtoblk_desc), (VOPFUNC)err_offtoblk},	/* offtoblk */
    {VOPPREF(bwrite_desc), (VOPFUNC)err_bwrite},
    {NULL}
};
struct vnodeopv_desc afs_dead_vnodeop_opv_desc =
    { &afs_dead_vnodeop_p, afs_dead_vnodeop_entries };
#endif

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

#ifndef AFS_DARWIN80_ENV
    tvc->f.states |= CUBCinit;
#endif
#ifdef AFS_DARWIN80_ENV
    osi_Assert((tvc->f.states & CVInit) == 0);
    if (tvc->f.states & CDeadVnode)
       osi_Assert(!vnode_isinuse(vp, 1));
#endif
    if (haveGlock) AFS_GUNLOCK(); 

#ifdef AFS_DARWIN80_ENV
	if (vnode_get(vp)) {
           /* being terminated. kernel won't give us a ref. Now what? our
              callers don't expect us to fail */
           if (haveGlock) AFS_GLOCK(); 
           return;
        }
	if (vnode_ref(vp)) {
	    vnode_put(vp);
	    if (haveGlock) AFS_GLOCK(); 
	    return;
	}
	vnode_put(vp);
#else
    /* vget needed for 0 ref'd vnode in GetVCache to not panic in vref.
       vref needed for multiref'd vnode in vnop_remove not to deadlock
       ourselves during vop_inactive, except we also need to not reinst
       the ubc... so we just call VREF there now anyway. */

    if (VREFCOUNT_GT(tvc, 0))
	VREF(((struct vnode *)(vp))); 
     else 
	afs_vget(afs_globalVFS, 0, (vp));
#endif

#ifndef AFS_DARWIN80_ENV
    if (UBCINFOMISSING(vp) || UBCINFORECLAIMED(vp)) {
	ubc_info_init(vp); 
    }
#endif

    if (haveGlock) AFS_GLOCK(); 
#ifndef AFS_DARWIN80_ENV
    tvc->f.states &= ~CUBCinit;
#endif
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
    int flags = ap->a_cnp->cn_flags;
    int lockparent;		/* 1 => lockparent flag is set */
    int wantparent;		/* 1 => wantparent or lockparent flag */
    struct proc *p;
#ifdef AFS_DARWIN80_ENV
    vcp = VTOAFS(ap->a_dvp);
    /*
     * ._ file attribute mirroring touches this.
     * we can't flag the vcache as there is none, so fail here.
     * needed for fsevents support.
     */
    if (ap->a_context == afs_osi_ctxtp)
	return ENOENT;
    if (vcp->mvstat != AFS_MVSTAT_MTPT) {
	error = cache_lookup(ap->a_dvp, ap->a_vpp, ap->a_cnp);
	if (error == -1) 
	    return 0;
	if (error == ENOENT) 
	    return error;
    }
#endif

    GETNAME();
    p = vop_cn_proc;

    lockparent = flags & LOCKPARENT;
    wantparent = flags & (LOCKPARENT | WANTPARENT);

    if (!vnode_isdir(ap->a_dvp)) {
	*ap->a_vpp = 0;
	DROPNAME();
	return ENOTDIR;
    }
    dvp = ap->a_dvp;
#ifndef AFS_DARWIN80_ENV
    if (flags & ISDOTDOT)
	VOP_UNLOCK(dvp, 0, p);
#endif
    AFS_GLOCK();
    error = afs_lookup(VTOAFS(dvp), name, &vcp, vop_cn_cred);
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
#ifdef AFS_DARWIN80_ENV
    if ((error=afs_darwin_finalizevnode(vcp, ap->a_dvp, ap->a_cnp, 0, 0))) {
	DROPNAME();
	*ap->a_vpp = 0;
	return error;
    }
#endif
    vp = AFSTOV(vcp);		/* always get a node if no error */
#ifndef AFS_DARWIN80_ENV /* XXX needed for multi-mount thing, but can't have it yet */
    vp->v_vfsp = dvp->v_vfsp;

    if (UBCINFOMISSING(vp) ||
	UBCINFORECLAIMED(vp)) {
	    ubc_info_init(vp);
    }
#endif

#ifndef AFS_DARWIN80_ENV
    /* The parent directory comes in locked.  We unlock it on return
     * unless the caller wants it left locked.
     * we also always return the vnode locked. */

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
    struct vnode *dvp = ap->a_dvp;
    struct proc *p;
    GETNAME();
    p = vop_cn_proc;

    /* vnode layer handles excl/nonexcl */
    AFS_GLOCK();
    error =
	afs_create(VTOAFS(dvp), name, ap->a_vap, NONEXCL, ap->a_vap->va_mode,
		   &vcp, vop_cn_cred);
    AFS_GUNLOCK();
    if (error) {
#ifndef AFS_DARWIN80_ENV
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
#endif
	DROPNAME();
	return (error);
    }

    if (vcp) {
#ifdef AFS_DARWIN80_ENV
      if ((error=afs_darwin_finalizevnode(vcp, ap->a_dvp, ap->a_cnp, 0, 0))) {
	    DROPNAME();
	    *ap->a_vpp=0;
	    return error;
        }
#endif
	*ap->a_vpp = AFSTOV(vcp);
#ifndef AFS_DARWIN80_ENV /* XXX needed for multi-mount thing, but can't have it yet */
	(*ap->a_vpp)->v_vfsp = dvp->v_vfsp;
	vn_lock(*ap->a_vpp, LK_EXCLUSIVE | LK_RETRY, p);
	if (UBCINFOMISSING(*ap->a_vpp) || UBCINFORECLAIMED(*ap->a_vpp)) {
	    vcp->f.states |= CUBCinit;
	    ubc_info_init(*ap->a_vpp);
	    vcp->f.states &= ~CUBCinit;
	}
#endif
    } else
	*ap->a_vpp = 0;

#ifndef AFS_DARWIN80_ENV
    if ((cnp->cn_flags & SAVESTART) == 0)
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    vput(dvp);
#endif
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
    vput(ap->a_dvp);
#endif
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
#if !defined(AFS_DARWIN80_ENV)
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
#endif /* !AFS_DARWIN80_ENV */
    AFS_GLOCK();
    error = afs_open(&vc, ap->a_mode, vop_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != vp)
	panic("AFS open changed vnode!");
#endif
    osi_FlushPages(vc, vop_cred);
    AFS_GUNLOCK();
#if !defined(AFS_DARWIN80_ENV)
    if (error && didhold)
	ubc_rele(vp);
#endif /* !AFS_DARWIN80_ENV */
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
    /* allows faking FSE_CONTENT_MODIFIED */
    if (afs_osi_ctxtp == ap->a_context)
	return 0;
    AFS_GLOCK();
    if (vop_cred)
	code = afs_close(avc, ap->a_fflag, vop_cred);
    else
	code = afs_close(avc, ap->a_fflag, afs_osi_credp);
    osi_FlushPages(avc, vop_cred);	/* hold GLOCK, but not basic vnode lock */
    /* This is legit; it just forces the fstrace event to happen */
    code = afs_CheckCode(code, NULL, 60);
    AFS_GUNLOCK();

    return code;
}

#ifdef AFS_DARWIN80_ENV
extern int afs_fakestat_enable;

int
afs_vop_access(ap)
     struct VOPPROT(access_args)        /* {
                                 * struct vnode *a_vp;
                                 * int  a_action;
                                 * vfs_context_t a_context;
                                 * } */ *ap;
{
    int code;
    struct vrequest treq;
    struct afs_fakestat_state fakestate;
    struct vcache * tvc = VTOAFS(ap->a_vp);
    int bits=0;
    int cmb = CHECK_MODE_BITS;
#ifdef AFS_DARWIN80_ENV
    /*
     * needed for fsevents. ._ file attribute mirroring touches this.
     * we can't flag the vcache, as there is none, so fail here.
     */
    if (ap->a_context == afs_osi_ctxtp)
	return ENOENT;
#endif
    AFS_GLOCK();
    afs_InitFakeStat(&fakestate);
    if ((code = afs_InitReq(&treq, vop_cred)))
        goto out2;

    code = afs_TryEvalFakeStat(&tvc, &fakestate, &treq);
    if (code) {
        code = afs_CheckCode(code, &treq, 55);
        goto out;
    }

    code = afs_VerifyVCache(tvc, &treq);
    if (code) {
        code = afs_CheckCode(code, &treq, 56);
        goto out;
    }
    if (afs_fakestat_enable && tvc->mvstat != AFS_MVSTAT_FILE && !(tvc->f.states & CStatd)) {
        code = 0;
        goto out;
    }
    if (vnode_isdir(ap->a_vp)) {
       if (ap->a_action & KAUTH_VNODE_LIST_DIRECTORY)
          bits |= PRSFS_LOOKUP;
       if (ap->a_action & KAUTH_VNODE_ADD_FILE)
          bits |= PRSFS_INSERT;
       if (ap->a_action & KAUTH_VNODE_SEARCH)
          bits |= PRSFS_LOOKUP;
       if (ap->a_action & KAUTH_VNODE_DELETE)
          bits |= PRSFS_DELETE;
       if (ap->a_action & KAUTH_VNODE_ADD_SUBDIRECTORY)
          bits |= PRSFS_INSERT;
       if (ap->a_action & KAUTH_VNODE_DELETE_CHILD)
          bits |= PRSFS_DELETE;
    } else {
       if (ap->a_action & KAUTH_VNODE_READ_DATA)
          bits |= PRSFS_READ;
       if (ap->a_action & KAUTH_VNODE_WRITE_DATA)
          bits |= PRSFS_WRITE;
       if (ap->a_action & KAUTH_VNODE_EXECUTE)
          bits |= PRSFS_READ; /* and mode bits.... */
       if (ap->a_action & KAUTH_VNODE_READ_ATTRIBUTES)
          bits |= PRSFS_LOOKUP;
       if (ap->a_action & KAUTH_VNODE_READ_SECURITY) /* mode bits/gid, not afs acl */
          bits |= PRSFS_LOOKUP;
       if ((ap->a_action & ((1 << 25) - 1)) == KAUTH_VNODE_EXECUTE)
          /* if only exec, don't check for read mode bit */
          /* high bits of ap->a_action are not for 'generic rights bits', and
             so should not be checked (KAUTH_VNODE_ACCESS is often present
             and needs to be masked off) */
	  cmb |= CMB_ALLOW_EXEC_AS_READ;
    }
    if (ap->a_action & KAUTH_VNODE_WRITE_ATTRIBUTES)
       bits |= PRSFS_WRITE;
    if (ap->a_action & KAUTH_VNODE_WRITE_SECURITY)
       bits |= PRSFS_WRITE;
    /* we can't check for KAUTH_VNODE_TAKE_OWNERSHIP, so we always permit it */
    
    code = afs_AccessOK(tvc, bits, &treq, cmb);
    /*
     * Special cased dropbox handling:
     * cp on 10.4 behaves badly, looping on EACCES
     * Finder may reopen the file. Let it.
     */
    if (code == 0 && ((bits &~(PRSFS_READ|PRSFS_WRITE)) == 0))
	code = afs_AccessOK(tvc, PRSFS_ADMINISTER|PRSFS_INSERT|bits, &treq, cmb);
    /* Finder also treats dropboxes as insert+delete. fake it out. */
    if (code == 0 && (bits == (PRSFS_INSERT|PRSFS_DELETE)))
	code = afs_AccessOK(tvc, PRSFS_INSERT, &treq, cmb);

    if (code == 1 && vnode_vtype(ap->a_vp) == VREG &&
        ap->a_action & KAUTH_VNODE_EXECUTE &&
        (tvc->f.m.Mode & 0100) != 0100) {
        code = 0;
    }
    if (code) {
        code= 0;               /* if access is ok */
    } else {
	    code = afs_CheckCode(EACCES, &treq, 57);        /* failure code */
    }
out:
     afs_PutFakeStat(&fakestate);
out2:
    AFS_GUNLOCK();
    return code;
}
#else
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
    code = afs_access(VTOAFS(ap->a_vp), ap->a_mode, vop_cred);
    AFS_GUNLOCK();
    return code;
}
#endif

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

#ifdef AFS_DARWIN80_ENV
    /* CEvent excludes the fsevent. our context excludes the ._ */
    if ((VTOAFS(ap->a_vp)->f.states & CEvent) ||
	(ap->a_context == afs_osi_ctxtp)){
	struct vcache *avc = VTOAFS(ap->a_vp);
	int isglock = ISAFS_GLOCK();

	/* this is needed because of how and when we re-enter */
	if (!isglock)
	  AFS_GLOCK();
	/* do minimal work to return fake result for fsevents */
	if (afs_fakestat_enable && VTOAFS(ap->a_vp)->mvstat == AFS_MVSTAT_MTPT) {
	    struct afs_fakestat_state fakestat;
	    struct vrequest treq;

	    code = afs_InitReq(&treq, vop_cred);
	    if (code) {
		if (!isglock)
		    AFS_GUNLOCK();
		return code;
	    }
	    afs_InitFakeStat(&fakestat);
	    /* expects GLOCK */
	    code = afs_TryEvalFakeStat(&avc, &fakestat, &treq);
	    if (code) {
		if (!isglock)
		    AFS_GUNLOCK();
		afs_PutFakeStat(&fakestat);
		return code;
	    }
	}
	code = afs_CopyOutAttrs(avc, ap->a_vap);
	if (!isglock)
	    AFS_GUNLOCK();
	if (0 && !code) {
	    /* tweak things so finder will recheck */
	    (ap->a_vap)->va_gid = ((ap->a_vap)->va_gid == 1) ? 2 : 1;
	    (ap->a_vap)->va_mode &= ~(VSGID);
	}
    } else
#endif
    {
	AFS_GLOCK();
	code = afs_getattr(VTOAFS(ap->a_vp), ap->a_vap, vop_cred);
	/* This is legit; it just forces the fstrace event to happen */
	code = afs_CheckCode(code, NULL, 58);
	AFS_GUNLOCK();
    }
#ifdef AFS_DARWIN80_ENV
    VATTR_SET_SUPPORTED(ap->a_vap, va_type);
    VATTR_SET_SUPPORTED(ap->a_vap, va_mode);
    VATTR_SET_SUPPORTED(ap->a_vap, va_uid);
    VATTR_SET_SUPPORTED(ap->a_vap, va_gid);
    VATTR_SET_SUPPORTED(ap->a_vap, va_fsid);
    VATTR_SET_SUPPORTED(ap->a_vap, va_fileid);
    VATTR_SET_SUPPORTED(ap->a_vap, va_nlink);
    VATTR_SET_SUPPORTED(ap->a_vap, va_data_size);
    VATTR_SET_SUPPORTED(ap->a_vap, va_access_time);
    VATTR_SET_SUPPORTED(ap->a_vap, va_modify_time);
    VATTR_SET_SUPPORTED(ap->a_vap, va_change_time);
    VATTR_SET_SUPPORTED(ap->a_vap, va_gen);
    VATTR_SET_SUPPORTED(ap->a_vap, va_flags);
    VATTR_SET_SUPPORTED(ap->a_vap, va_iosize);
    VATTR_SET_SUPPORTED(ap->a_vap, va_total_alloc);
#endif
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
    int code, pass = 0;
    struct vcache *avc = VTOAFS(ap->a_vp);
#ifdef AFS_DARWIN80_ENV
    /* fsevents tries to set attributes. drop it. */
    if (ap->a_context == afs_osi_ctxtp)
	return 0;
#endif
    AFS_GLOCK();
retry:
    code = afs_setattr(avc, ap->a_vap, vop_cred);
    /* This is legit; it just forces the fstrace event to happen */
    code = afs_CheckCode(code, NULL, 59);
    if (!pass && code == EINVAL && (VATTR_IS_ACTIVE(ap->a_vap, va_mode) &&
				    (vType(avc) == VLNK))) {
	VATTR_CLEAR_ACTIVE(ap->a_vap, va_mode);
	pass++;
	goto retry;
    }
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

    if (vnode_isdir(ap->a_vp)) 
	return EISDIR;
#ifdef AFS_DARWIN80_ENV
    ubc_msync_range(ap->a_vp, AFS_UIO_OFFSET(ap->a_uio), AFS_UIO_OFFSET(ap->a_uio) + AFS_UIO_RESID(ap->a_uio), UBC_PUSHDIRTY);
#else
    if (UBCINFOEXISTS(ap->a_vp)) {
	ubc_clean(ap->a_vp, 0);
    }
#endif
    AFS_GLOCK();
    osi_FlushPages(avc, vop_cred);	/* hold GLOCK, but not basic vnode lock */
    code = afs_read(avc, ap->a_uio, vop_cred, 0);
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
    struct vnode *vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    size_t size = ap->a_size;
    off_t f_offset = ap->a_f_offset;
    vm_offset_t pl_offset = ap->a_pl_offset;
    int flags = ap->a_flags;
    struct ucred *cred;
    vm_offset_t ioaddr;
    int code;
    struct vcache *tvc = VTOAFS(vp);
    int nocommit = flags & UPL_NOCOMMIT;
#ifdef AFS_DARWIN80_ENV
    struct uio *uio;
#else
    struct uio auio;
    struct iovec aiov;
    struct uio *uio = &auio;

    memset(&auio, 0, sizeof(auio));
    memset(&aiov, 0, sizeof(aiov));
#endif

#ifndef AFS_DARWIN80_ENV
    if (UBCINVALID(vp)) {
#if DIAGNOSTIC
	panic("afs_vop_pagein: invalid vp");
#endif /* DIAGNOSTIC */
	return (EPERM);
    }

    UBCINFOCHECK("afs_vop_pagein", vp);
#endif
    if (pl == (upl_t) NULL) {
	panic("afs_vop_pagein: no upl");
    }

    cred = ubc_getcred(vp);
    if (cred == NOCRED)
	cred = vop_cred;

    if (size == 0) {
	if (!nocommit)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
	return (0);
    }
    if (f_offset < 0) {
	if (!nocommit)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
	return (EINVAL);
    }
    if (f_offset & PAGE_MASK)
	panic("afs_vop_pagein: offset not page aligned");

    OSI_UPL_MAP(pl, &ioaddr);
    ioaddr += pl_offset;
#ifdef AFS_DARWIN80_ENV
    uio = uio_create(1, f_offset, UIO_SYSSPACE32, UIO_READ);
    uio_addiov(uio, CAST_USER_ADDR_T(ioaddr), size);
#else
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = f_offset;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_rw = UIO_READ;
    auio.uio_procp = NULL;
    auio.uio_resid = aiov.iov_len = size;
    aiov.iov_base = (caddr_t) ioaddr;
#endif
    AFS_GLOCK();
    osi_FlushPages(tvc, vop_cred);	/* hold GLOCK, but not basic vnode lock */
    code = afs_read(tvc, uio, cred, 0);
    if (code == 0) {
	ObtainWriteLock(&tvc->lock, 2);
	tvc->f.states |= CMAPPED;
	ReleaseWriteLock(&tvc->lock);
    }
    AFS_GUNLOCK();

    /* Zero out rest of last page if there wasn't enough data in the file */
    if (code == 0 && AFS_UIO_RESID(uio) > 0) {
#ifdef AFS_DARWIN80_ENV
	memset(((caddr_t)ioaddr) + (size - AFS_UIO_RESID(uio)), 0,
               AFS_UIO_RESID(uio));
#else
	memset(aiov.iov_base, 0, auio.uio_resid);
#endif
    }

    OSI_UPL_UNMAP(pl);
    if (!nocommit) {
	if (code)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_ERROR | UPL_ABORT_FREE_ON_EMPTY);
	else
	    OSI_UPL_COMMIT_RANGE(pl, pl_offset, size,
				    UPL_COMMIT_CLEAR_DIRTY |
				    UPL_COMMIT_FREE_ON_EMPTY);
    }
#ifdef AFS_DARWIN80_ENV
    uio_free(uio);
#endif
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
#ifdef AFS_DARWIN80_ENV
    ubc_msync_range(ap->a_vp, AFS_UIO_OFFSET(ap->a_uio), AFS_UIO_OFFSET(ap->a_uio) + AFS_UIO_RESID(ap->a_uio), UBC_INVALIDATE);
#else
    if (UBCINFOEXISTS(ap->a_vp)) {
	ubc_clean(ap->a_vp, 1);
    }
    if (UBCINFOEXISTS(ap->a_vp))
	osi_VM_NukePages(ap->a_vp, AFS_UIO_OFFSET(ap->a_uio),
			 AFS_UIO_RESID(ap->a_uio));
#endif
    AFS_GLOCK();
    osi_FlushPages(avc, vop_cred);	/* hold GLOCK, but not basic vnode lock */
    code =
	afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, vop_cred, 0);
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
    struct vnode *vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    size_t size = ap->a_size;
    off_t f_offset = ap->a_f_offset;
    vm_offset_t pl_offset = ap->a_pl_offset;
    int flags = ap->a_flags;
    struct ucred *cred;
    vm_offset_t ioaddr;
    int nocommit = flags & UPL_NOCOMMIT;
    int iosize;
    int code;
    struct vcache *tvc = VTOAFS(vp);
#ifdef AFS_DARWIN80_ENV
    struct uio *uio;
#else
    struct uio auio;
    struct iovec aiov;
    struct uio *uio = &auio;

    memset(&auio, 0, sizeof(auio));
    memset(&aiov, 0, sizeof(aiov));
#endif

#ifndef AFS_DARWIN80_ENV
    if (UBCINVALID(vp)) {
#if DIAGNOSTIC
	panic("afs_vop_pageout: invalid vp");
#endif /* DIAGNOSTIC */
	return (EPERM);
    }

    UBCINFOCHECK("afs_vop_pageout", vp);
#endif
    if (pl == (upl_t) NULL) {
	panic("afs_vop_pageout: no upl");
    }
#if !defined(AFS_DARWIN80_ENV) /* XXX nfs now uses it's own bufs (struct nfsbuf)
                                  maybe the generic
                                  layer doesn't have them anymore? In any case,
                                  we can't just copy code from nfs... */
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
	cred = vop_cred;

    if (size == 0) {
	if (!nocommit)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	return (0);
    }
    if (flags & (IO_APPEND | IO_SYNC))
	panic("nfs_pageout: (IO_APPEND | IO_SYNC)");
    if (f_offset < 0) {
	if (!nocommit)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	return (EINVAL);
    }
    if (f_offset >= tvc->f.m.Length) {
	if (!nocommit)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	return (EINVAL);
    }

    if (f_offset & PAGE_MASK)
	panic("afs_vop_pageout: offset not page aligned");

    /* size will always be a multiple of PAGE_SIZE */
    /* pageout isn't supposed to extend files */
    if (f_offset + size > tvc->f.m.Length) 
        iosize = tvc->f.m.Length - f_offset;
    else
        iosize = size;

    if (size > (iosize + (PAGE_SIZE - 1)) & ~PAGE_MASK && !nocommit)  {
            int iosize_rnd=(iosize + (PAGE_SIZE - 1)) & ~PAGE_MASK;
	    OSI_UPL_ABORT_RANGE(pl, pl_offset + iosize_rnd,
                                   size - iosize_rnd,
				   UPL_ABORT_FREE_ON_EMPTY);
    }
    OSI_UPL_MAP(pl, &ioaddr);
    ioaddr += pl_offset;
#ifdef AFS_DARWIN80_ENV
    uio = uio_create(1, f_offset, UIO_SYSSPACE32, UIO_READ);
    uio_addiov(uio, CAST_USER_ADDR_T(ioaddr), size);
#else
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = f_offset;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_rw = UIO_WRITE;
    auio.uio_procp = NULL;
    auio.uio_resid = aiov.iov_len = iosize;
    aiov.iov_base = (caddr_t) ioaddr;
#endif
    {
	/* USV?
	 * check for partial page and clear the
	 * contents past end of the file before
	 * releasing it in the VM page cache
	 */
	if ((f_offset < tvc->f.m.Length) && (f_offset + size) > tvc->f.m.Length) {
	    size_t io = tvc->f.m.Length - f_offset;

	    memset((caddr_t) (ioaddr + pl_offset + io), 0, size - io);
	}
    }

    AFS_GLOCK();
    osi_FlushPages(tvc, vop_cred);	/* hold GLOCK, but not basic vnode lock */
    ObtainWriteLock(&tvc->lock, 1);
    afs_FakeOpen(tvc);
    ReleaseWriteLock(&tvc->lock);

    code = afs_write(tvc, uio, flags, cred, 0);

    ObtainWriteLock(&tvc->lock, 1);
    afs_FakeClose(tvc, cred);
    ReleaseWriteLock(&tvc->lock);
    AFS_GUNLOCK();
    OSI_UPL_UNMAP(pl);
    if (!nocommit) {
	if (code)
	    OSI_UPL_ABORT_RANGE(pl, pl_offset, size,
				   UPL_ABORT_FREE_ON_EMPTY);
	else
	    OSI_UPL_COMMIT_RANGE(pl, pl_offset, size,
				    UPL_COMMIT_CLEAR_DIRTY |
				    UPL_COMMIT_FREE_ON_EMPTY);
    }

#ifdef AFS_DARWIN80_ENV
    uio_free(uio);
#endif
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
	error = HandleIoctl(tvc, ap->a_command, ap->a_data);
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
    struct vnode *vp = ap->a_vp;
    int haveGlock = ISAFS_GLOCK();

    /* in order to recycle faked vnodes for bulkstat */
    if (VTOAFS(vp) == NULL)
	return ENOTSUP;

    /* afs_vop_lookup glocks, can call us through vinvalbuf from GetVCache */
    if (!haveGlock) AFS_GLOCK();
    if (vop_cred)
	error = afs_fsync(VTOAFS(vp), vop_cred);
    else
	error = afs_fsync(VTOAFS(vp), afs_osi_credp);
    if (!haveGlock) AFS_GUNLOCK();
    return error;
}

#ifndef AFS_DARWIN80_ENV
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
#endif

int
afs_vop_remove(ap)
     struct VOPPROT(remove_args)	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

#ifdef AFS_DARWIN80_ENV
    if (ap->a_flags & VNODE_REMOVE_NODELETEBUSY) {
            /* Caller requested Carbon delete semantics */
            if (vnode_isinuse(vp, 0)) {
                    return EBUSY;
            }
    }
#endif

    GETNAME();
    AFS_GLOCK();
    error = afs_remove(VTOAFS(dvp), name, vop_cn_cred);
    error = afs_CheckCode(error, NULL, 61);
    AFS_GUNLOCK();
    cache_purge(vp);
    if (!error) {
#ifdef AFS_DARWIN80_ENV
	struct vcache *tvc = VTOAFS(vp);
	
	if (!(tvc->f.states & CUnlinked)) {
            ubc_setsize(vp, (off_t)0);
            vnode_recycle(vp);
	}
#else
        /* necessary so we don't deadlock ourselves in vclean */
        VOP_UNLOCK(vp, 0, cnp->cn_proc);

	/* If crashes continue in ubc_hold, comment this out */
        (void)ubc_uncache(vp);
#endif
    } else {
	/* should check for PRSFS_INSERT and not PRSFS_DELETE, but the
	   goal here is to deal with Finder's unhappiness with resource
	   forks that have no resources in a dropbox setting */
	if (name[0] == '.' && name[1] == '_' && error == EACCES) 
	    error = 0;
    }

#ifndef AFS_DARWIN80_ENV
    vput(dvp);
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
#endif

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
    struct vnode *dvp = ap->a_tdvp;
    struct vnode *vp = ap->a_vp;
    struct proc *p;

    GETNAME();
    p = vop_cn_proc;
    if (vnode_isdir(vp)) {
	VOP_ABORTOP(vp, cnp);
	error = EISDIR;
	goto out;
    }
#ifndef AFS_DARWIN80_ENV
    if (error = vn_lock(vp, LK_EXCLUSIVE, p)) {
	VOP_ABORTOP(dvp, cnp);
	goto out;
    }
#endif
    AFS_GLOCK();
    error = afs_link(VTOAFS(vp), VTOAFS(dvp), name, vop_cn_cred);
    AFS_GUNLOCK();
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
#ifndef AFS_DARWIN80_ENV
    if (dvp != vp)
	VOP_UNLOCK(vp, 0, p);
#endif
  out:
#ifndef AFS_DARWIN80_ENV
    vput(dvp);
#endif
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
    struct vnode *tdvp = ap->a_tdvp;
    struct vnode *fvp = ap->a_fvp;
    struct vnode *fdvp = ap->a_fdvp;
    struct proc *p; 

    p = cn_proc(fcnp);

#ifdef AFS_DARWIN80_ENV
    /*
     * generic code tests for v_mount equality, so we don't have to, but we
     * don't get the multiple-mount "benefits" of the old behavior
     * the generic code doesn't do this, so we really should, but all the
     * vrele's are wrong...
     */
#else
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
	if (vnode_isdir(fvp)) {
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
#endif

    MALLOC(fname, char *, fcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    memcpy(fname, fcnp->cn_nameptr, fcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    MALLOC(tname, char *, tcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    memcpy(tname, tcnp->cn_nameptr, tcnp->cn_namelen);
    tname[tcnp->cn_namelen] = '\0';


    AFS_GLOCK();
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    error =
	afs_rename(VTOAFS(fdvp), fname, VTOAFS(tdvp), tname, cn_cred(tcnp));

#if !defined(AFS_DARWIN80_ENV) 
    AFS_GUNLOCK();
    VOP_UNLOCK(fvp, 0, p);
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
#else
    if (error == EXDEV) {
        struct brequest *tb;
        struct afs_uspc_param mvReq;
        struct vcache *tvc;
        struct vcache *fvc = VTOAFS(fdvp);
        int code = 0;
        struct afs_fakestat_state fakestate;
        int fakestatdone = 0;

	tvc = VTOAFS(tdvp);

        /* unrewritten mount point? */
        if (tvc->mvstat == AFS_MVSTAT_MTPT) {
            if (tvc->mvid.target_root && (tvc->f.states & CMValid)) {
                struct vrequest treq;

                afs_InitFakeStat(&fakestate);
                code = afs_InitReq(&treq, vop_cred);
                if (!code) {
                    fakestatdone = 1;
                    code = afs_EvalFakeStat(&tvc, &fakestate, &treq);
                } else
                    afs_PutFakeStat(&fakestate);
            }
        }

        if (!code) {
	    /* at some point in the future we should allow other types */
	    mvReq.reqtype = AFS_USPC_UMV;
            mvReq.req.umv.id = afs_cr_uid(cn_cred(tcnp));
            mvReq.req.umv.idtype = IDTYPE_UID;
            mvReq.req.umv.sCell = fvc->f.fid.Cell;
            mvReq.req.umv.sVolume = fvc->f.fid.Fid.Volume;
            mvReq.req.umv.sVnode = fvc->f.fid.Fid.Vnode;
            mvReq.req.umv.sUnique = fvc->f.fid.Fid.Unique;
            mvReq.req.umv.dCell = tvc->f.fid.Cell;
            mvReq.req.umv.dVolume = tvc->f.fid.Fid.Volume;
            mvReq.req.umv.dVnode = tvc->f.fid.Fid.Vnode;
            mvReq.req.umv.dUnique = tvc->f.fid.Fid.Unique;

	    /*
	     * su %d -c mv /afs/.:mount/%d:%d:%d:%d/%s
	     * /afs/.:mount/%d:%d:%d:%d/%s where:
	     * mvReq.req.umv.id, fvc->f.fid.Cell, fvc->f.fid.Fid.Volume,
	     * fvc->f.fid.Fid.Vnode, fvc->f.fid.Fid.Unique, fname,
	     * tvc->f.fid.Cell, tvc->f.fid.Fid.Volume, tvc->f.fid.Fid.Vnode,
	     * tvc->f.fid.Fid.Unique, tname
	     */

            tb = afs_BQueue(BOP_MOVE, NULL, 0, 1, cn_cred(tcnp),
                            0L, 0L, &mvReq, fname, tname);
	    /* wait to collect result */
            while ((tb->flags & BUVALID) == 0) {
                tb->flags |= BUWAIT;
                afs_osi_Sleep(tb);
            }
            /* if we succeeded, clear the error. otherwise, EXDEV */
            if (mvReq.retval == 0)
                error = 0;

            afs_BRelease(tb);
        }

        if (fakestatdone)
            afs_PutFakeStat(&fakestate);
    }
    AFS_GUNLOCK();

    cache_purge(fdvp);
    cache_purge(fvp);
    cache_purge(tdvp);
    if (tvp) {
	cache_purge(tvp);
	if (!error) {
	    vnode_recycle(tvp);
	}
    }
    if (!error)
	cache_enter(tdvp, fvp, tcnp);
#endif
    FREE(fname, M_TEMP);
    FREE(tname, M_TEMP);
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
    struct vnode *dvp = ap->a_dvp;
    struct vattr *vap = ap->a_vap;
    int error = 0;
    struct vcache *vcp;
    struct proc *p;

    GETNAME();
    p = vop_cn_proc;
#if defined(DIAGNOSTIC) && !defined(AFS_DARWIN80_ENV)
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_vop_mkdir: no name");
#endif
    AFS_GLOCK();
    error = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, vop_cn_cred);
    AFS_GUNLOCK();
    if (error) {
#ifndef AFS_DARWIN80_ENV
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
#endif
	DROPNAME();
	return (error);
    }
    if (vcp) {
#ifdef AFS_DARWIN80_ENV
	afs_darwin_finalizevnode(vcp, ap->a_dvp, ap->a_cnp, 0, 0);
#endif
	*ap->a_vpp = AFSTOV(vcp);
#ifndef AFS_DARWIN80_ENV /* XXX needed for multi-mount thing, but can't have it yet */
	(*ap->a_vpp)->v_vfsp = dvp->v_vfsp;
	vn_lock(*ap->a_vpp, LK_EXCLUSIVE | LK_RETRY, p);
#endif
    } else
	*ap->a_vpp = 0;
    DROPNAME();
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    vput(dvp);
#endif
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
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
    if (dvp == vp) {
#ifndef AFS_DARWIN80_ENV
	vrele(dvp);
	vput(vp);
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
#endif
	DROPNAME();
	return (EINVAL);
    }

    AFS_GLOCK();
    error = afs_rmdir(VTOAFS(dvp), name, vop_cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    cache_purge(dvp);
    cache_purge(vp);
#ifndef AFS_DARWIN80_ENV
    vput(dvp);
    vput(vp);
#endif
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
    struct vnode *dvp = ap->a_dvp;
    struct vcache *pvc = NULL;
    int error = 0;

    GETNAME();
    AFS_GLOCK();
    error = afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target, &pvc,
			vop_cn_cred);
    AFS_GUNLOCK();
#ifndef AFS_DARWIN80_ENV
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    vput(dvp);
#endif
    *ap->a_vpp = NULL;
    if (!error) {
	error = afs_darwin_finalizevnode(pvc, dvp, ap->a_cnp, 0, 0);
	if (!error)
	    *ap->a_vpp = AFSTOV(pvc);
    }
    DROPNAME();
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
#ifdef AFS_DARWIN80_ENV
    /* too much work for now */
    /* should only break nfs exports */
    if (ap->a_flags & (VNODE_READDIR_EXTENDED | VNODE_READDIR_REQSEEKOFF))
         return (EINVAL);
#endif
    off = AFS_UIO_OFFSET(ap->a_uio);
    AFS_GLOCK();
    error =
	afs_readdir(VTOAFS(ap->a_vp), ap->a_uio, vop_cred, ap->a_eofflag);
    AFS_GUNLOCK();
#ifndef AFS_DARWIN80_ENV
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
#endif

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
    error = afs_readlink(VTOAFS(ap->a_vp), ap->a_uio, vop_cred);
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
    struct vnode *vp = ap->a_vp;
    struct vcache *tvc = VTOAFS(vp);
#ifndef AFS_DARWIN80_ENV
    if (prtactive && vp->v_usecount != 0)
	vprint("afs_vop_inactive(): pushing active", vp);
#endif
    if (tvc) {
#ifdef AFS_DARWIN80_ENV
        int unlinked = tvc->f.states & CUnlinked;
#endif
	AFS_GLOCK();
	afs_InactiveVCache(tvc, 0);     /* decrs ref counts */
	AFS_GUNLOCK();
#ifdef AFS_DARWIN80_ENV
	if (unlinked) {
	    vnode_recycle(vp);
	    cache_purge(vp);
	}
#endif
    }
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
    int sl, writelocked;
    struct vnode *vp = ap->a_vp;
    struct vcache *tvc = VTOAFS(vp);

    osi_Assert(!ISAFS_GLOCK());
    cache_purge(vp);		/* just in case... */
    if (tvc) {
       AFS_GLOCK();
       writelocked = (0 == NBObtainWriteLock(&afs_xvcache, 335));
       if (!writelocked) {
	   ObtainWriteLock(&afs_xvreclaim, 176);
#ifdef AFS_DARWIN80_ENV
	   vnode_clearfsnode(AFSTOV(tvc));
	   vnode_removefsref(AFSTOV(tvc));
#else
	   tvc->v->v_data = NULL;  /* remove from vnode */
#endif
	   AFSTOV(tvc) = NULL;             /* also drop the ptr to vnode */
	   tvc->f.states |= CVInit; /* also CDeadVnode? */
	   tvc->nextfree = ReclaimedVCList;
	   ReclaimedVCList = tvc;
	   ReleaseWriteLock(&afs_xvreclaim);
       } else {
	   error = afs_FlushVCache(tvc, &sl);	/* toss our stuff from vnode */
	   if (tvc->f.states & (CVInit
#ifdef AFS_DARWIN80_ENV
			      | CDeadVnode
#endif
		   )) {
	       tvc->f.states &= ~(CVInit
#ifdef AFS_DARWIN80_ENV
				| CDeadVnode
#endif
		   );
	       afs_osi_Wakeup(&tvc->f.states);
	   }
	   if (!error && vnode_fsnode(vp))
	       panic("afs_reclaim: vnode not cleaned");
	   if (!error && (tvc->v != NULL)) 
	       panic("afs_reclaim: vcache not cleaned");
	   ReleaseWriteLock(&afs_xvcache);
       }
       AFS_GUNLOCK();
    }
    return error;
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
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
    case _PC_NAME_CHARS_MAX:
        *ap->a_retval = NAME_MAX;
	break;
    case _PC_CASE_SENSITIVE:
        *ap->a_retval = 1;
	break;
    case _PC_CASE_PRESERVING:
        *ap->a_retval = 1;
	break;
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
    struct ucred *tcr;
    int clid;
    int op;
#ifdef AFS_DARWIN80_ENV
    proc_t p;
    tcr=vop_cred;
#else
    struct proc *p = current_proc();
    struct ucred cr;
    pcred_readlock(p);
    cr = *p->p_cred->pc_ucred;
    pcred_unlock(p);
    tcr=&cr;
#endif
    if (ap->a_flags & F_POSIX) {
#ifdef AFS_DARWIN80_ENV
	p = (proc_t) ap->a_id;
	clid = proc_pid(p);
#else
	p = (struct proc *) ap->a_id;
	clid = p->p_pid;
#endif
    } else {
	clid = (int)ap->a_id;
    }
    if (ap->a_op == F_UNLCK) {
	op = F_SETLK;
    } else if (ap->a_op == F_SETLK && ap->a_flags & F_WAIT) {
	op = F_SETLKW;
    } else {
	op = ap->a_op;
    }
    AFS_GLOCK();
    error = afs_lockctl(VTOAFS(ap->a_vp), ap->a_fl, op, tcr, clid);
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
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);

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
    /* printf("stray afs_vop_truncate\n"); */
    return ENOTSUP;
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
    /* printf("stray afs_vop_update\n"); */
    return ENOTSUP;
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
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(ap->a_vp);
    int s = vc->f.states;
    printf("tag %d, fid: %ld.%x.%x.%x, opens %d, writers %d", vp->v_tag,
	   vc->f.fid.Cell, vc->f.fid.Fid.Volume, vc->f.fid.Fid.Vnode,
	   vc->f.fid.Fid.Unique, vc->opens, vc->execsOrWriters);
    printf("\n  states%s%s%s%s%s", (s & CStatd) ? " statd" : "",
	   (s & CRO) ? " readonly" : "", (s & CDirty) ? " dirty" : "",
	   (s & CMAPPED) ? " mapped" : "",
	   (s & CVFlushed) ? " flush in progress" : "");
    if (UBCISVALID(vp)) {
	printf("\n  UBC: ");
	if (UBCINFOEXISTS(vp)) {
	    printf("exists, ");
	    printf("refs %d%s%s", vp->v_ubcinfo->ui_refcount,
		   ubc_issetflags(vp, UI_HASOBJREF) ? " HASOBJREF" : "",
		   ubc_issetflags(vp, UI_WASMAPPED) ? " WASMAPPED" : "");
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
    *ap->a_run = opr_max(ap->a_size, AFS_CHUNKSIZE(ap->a_foffset));
    return 0;
}
#endif

int
afs_darwin_getnewvnode(struct vcache *avc)
{
#ifdef AFS_DARWIN80_ENV
    vnode_t vp;
    int error, dead;
    struct vnode_fsparam par;

    memset(&par, 0, sizeof(struct vnode_fsparam));
    par.vnfs_vtype = VNON;
    par.vnfs_vops = afs_dead_vnodeop_p;
    par.vnfs_flags = VNFS_NOCACHE|VNFS_CANTCACHE;
    par.vnfs_mp = afs_globalVFS;
    par.vnfs_fsnode = avc;

    error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &par, &vp);
    if (!error) {
      vnode_addfsref(vp);
      vnode_ref(vp);
      avc->v = vp;
      vnode_recycle(vp); /* terminate as soon as iocount drops */
      avc->f.states |= CDeadVnode;
    }
    return error;
#else
    while (getnewvnode(VT_AFS, afs_globalVFS, afs_vnodeop_p, &avc->v)) {
        /* no vnodes available, force an alloc (limits be damned)! */
	printf("failed to get vnode\n");
    }
    avc->v->v_data = (void *)avc;
    return 0;
#endif
}
#ifdef AFS_DARWIN80_ENV
/* if this fails, then tvc has been unrefed and may have been freed. 
   Don't touch! */
int
afs_darwin_finalizevnode(struct vcache *avc, struct vnode *dvp,
			 struct componentname *cnp, int isroot, int locked)
{
    vnode_t ovp;
    vnode_t nvp;
    int error;
    struct vnode_fsparam par;

    if (!locked) {
	AFS_GLOCK();
	ObtainWriteLock(&avc->lock,325);
    }
    ovp = AFSTOV(avc);

    if (!(avc->f.states & CDeadVnode) && vnode_vtype(ovp) != VNON) {
	AFS_GUNLOCK();
	/* Can end up in reclaim... drop GLOCK */
	vnode_rele(ovp);
	AFS_GLOCK();
	if (!locked) {
	    ReleaseWriteLock(&avc->lock);
	    AFS_GUNLOCK();
	}
	return 0;
    }

    if ((avc->f.states & CDeadVnode) && vnode_vtype(ovp) != VNON)
	panic("vcache %p should not be CDeadVnode", avc);
    AFS_GUNLOCK();
    memset(&par, 0, sizeof(struct vnode_fsparam));
    par.vnfs_mp = afs_globalVFS;
    par.vnfs_vtype = avc->f.m.Type;
    par.vnfs_vops = afs_vnodeop_p;
    par.vnfs_filesize = avc->f.m.Length;
    par.vnfs_fsnode = avc;
    par.vnfs_dvp = dvp;
    if (cnp && (cnp->cn_flags & ISDOTDOT) == 0)
	par.vnfs_cnp = cnp;
    if (!dvp || !cnp || (cnp->cn_flags & MAKEENTRY) == 0)
	par.vnfs_flags = VNFS_NOCACHE;
    if (isroot)
	par.vnfs_markroot = 1;
    error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &par, &nvp);
    if (!error) {
	vnode_addfsref(nvp);
	if ((avc->f.states & CDeadVnode) && vnode_vtype(ovp) != VNON)
	    printf("vcache %p should not be CDeadVnode", avc);
	if (avc->v == ovp) {
	    if (!(avc->f.states & CVInit)) {
		vnode_clearfsnode(ovp);
		vnode_removefsref(ovp);
	    }
	    /* we're discarding on a fixup. mark for recycle */
	    if (!(avc->f.states & CDeadVnode))
		vnode_recycle(ovp);
	}
	avc->v = nvp;
	avc->f.states &=~ CDeadVnode;
	/* If we were carrying an extra ref for dirty, hold/push it. */
	if (avc->f.ddirty_flags) {
	    vnode_get(nvp);
	    vnode_ref(nvp);
	}
	/* If we were carrying an extra ref for shadow, hold/push it. */
	if (avc->f.shadow.vnode) {
	    vnode_get(nvp);
	    vnode_ref(nvp);
	}
    }
    /* Drop any extra dirty ref on the old vnode */
    if (avc->f.ddirty_flags) {
	vnode_put(ovp);
	vnode_rele(ovp);
    }
    /* Drop any extra shadow ref on the old vnode */
    if (avc->f.shadow.vnode) {
	vnode_put(ovp);
	vnode_rele(ovp);
    }
    /* If it's ref'd still, unmark stat'd to force new lookup */
    if ((vnode_vtype(ovp) != avc->f.m.Type) && VREFCOUNT_GT(avc, 1))
	avc->f.states &= ~CStatd;

    vnode_put(ovp);
    vnode_rele(ovp);
    AFS_GLOCK();
    if (!locked)
	ReleaseWriteLock(&avc->lock);
    if (!error)
	afs_osi_Wakeup(&avc->f.states);
    if (!locked)
	AFS_GUNLOCK();
    return error;
}
#endif
