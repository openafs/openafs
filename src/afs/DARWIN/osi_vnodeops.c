#include <afs/param.h>  /* Should be always first */
#include <afs/sysincludes.h>            /* Standard vendor system headers */
#include <afs/afsincludes.h>            /* Afs-based standard headers */
#include <afs/afs_stats.h>              /* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/ubc.h>

int afs_vop_lookup(struct vop_lookup_args *);
int afs_vop_create(struct vop_create_args *);
int afs_vop_mknod(struct vop_mknod_args *);
int afs_vop_open(struct vop_open_args *);
int afs_vop_close(struct vop_close_args *);
int afs_vop_access(struct vop_access_args *);
int afs_vop_getattr(struct vop_getattr_args *);
int afs_vop_setattr(struct vop_setattr_args *);
int afs_vop_read(struct vop_read_args *);
int afs_vop_write(struct vop_write_args *);
int afs_vop_pagein(struct vop_pagein_args *);
int afs_vop_pageout(struct vop_pageout_args *);
int afs_vop_ioctl(struct vop_ioctl_args *);
int afs_vop_select(struct vop_select_args *);
int afs_vop_mmap(struct vop_mmap_args *);
int afs_vop_fsync(struct vop_fsync_args *);
int afs_vop_seek(struct vop_seek_args *);
int afs_vop_remove(struct vop_remove_args *);
int afs_vop_link(struct vop_link_args *);
int afs_vop_rename(struct vop_rename_args *);
int afs_vop_mkdir(struct vop_mkdir_args *);
int afs_vop_rmdir(struct vop_rmdir_args *);
int afs_vop_symlink(struct vop_symlink_args *);
int afs_vop_readdir(struct vop_readdir_args *);
int afs_vop_readlink(struct vop_readlink_args *);
extern int ufs_abortop(struct vop_abortop_args *);
int afs_vop_inactive(struct vop_inactive_args *);
int afs_vop_reclaim(struct vop_reclaim_args *);
int afs_vop_lock(struct vop_lock_args *);
int afs_vop_unlock(struct vop_unlock_args *);
int afs_vop_bmap(struct vop_bmap_args *);
int afs_vop_strategy(struct vop_strategy_args *);
int afs_vop_print(struct vop_print_args *);
int afs_vop_islocked(struct vop_islocked_args *);
int afs_vop_pathconf(struct vop_pathconf_args *);
int afs_vop_advlock(struct vop_advlock_args *);
int afs_vop_truncate(struct vop_truncate_args *);
int afs_vop_update(struct vop_update_args *);
int afs_vop_blktooff __P((struct vop_blktooff_args *));
int afs_vop_offtoblk __P((struct vop_offtoblk_args *));
int afs_vop_cmap __P((struct vop_cmap_args *));


#define afs_vop_opnotsupp \
	((int (*) __P((struct  vop_reallocblks_args *)))eopnotsupp)
#define afs_vop_valloc afs_vop_opnotsupp
#define afs_vop_vfree afs_vop_opnotsupp
#define afs_vop_blkatoff afs_vop_opnotsupp
#define afs_vop_reallocblks afs_vop_opnotsupp

/* Global vfs data structures for AFS. */
int (**afs_vnodeop_p)();
struct vnodeopv_entry_desc afs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, afs_vop_lookup },          /* lookup */
	{ &vop_create_desc, afs_vop_create },          /* create */
	{ &vop_mknod_desc, afs_vop_mknod },            /* mknod */
	{ &vop_open_desc, afs_vop_open },              /* open */
	{ &vop_close_desc, afs_vop_close },            /* close */
	{ &vop_access_desc, afs_vop_access },          /* access */
	{ &vop_getattr_desc, afs_vop_getattr },        /* getattr */
	{ &vop_setattr_desc, afs_vop_setattr },        /* setattr */
	{ &vop_read_desc, afs_vop_read },              /* read */
	{ &vop_write_desc, afs_vop_write },            /* write */
	{ &vop_pagein_desc, afs_vop_pagein },              /* read */
	{ &vop_pageout_desc, afs_vop_pageout },            /* write */
	{ &vop_ioctl_desc, afs_vop_ioctl }, /* XXX ioctl */
	{ &vop_select_desc, afs_vop_select },          /* select */
	{ &vop_mmap_desc, afs_vop_mmap },              /* mmap */
	{ &vop_fsync_desc, afs_vop_fsync },            /* fsync */
	{ &vop_seek_desc, afs_vop_seek },              /* seek */
	{ &vop_remove_desc, afs_vop_remove },          /* remove */
	{ &vop_link_desc, afs_vop_link },              /* link */
	{ &vop_rename_desc, afs_vop_rename },          /* rename */
	{ &vop_mkdir_desc, afs_vop_mkdir },            /* mkdir */
	{ &vop_rmdir_desc, afs_vop_rmdir },            /* rmdir */
	{ &vop_symlink_desc, afs_vop_symlink },        /* symlink */
	{ &vop_readdir_desc, afs_vop_readdir },        /* readdir */
	{ &vop_readlink_desc, afs_vop_readlink },      /* readlink */
	/* Yes, we use the ufs_abortop call.  It just releases the namei
	   buffer stuff */
	{ &vop_abortop_desc, ufs_abortop },             /* abortop */
	{ &vop_inactive_desc, afs_vop_inactive },      /* inactive */
	{ &vop_reclaim_desc, afs_vop_reclaim },        /* reclaim */
	{ &vop_lock_desc, afs_vop_lock },              /* lock */
	{ &vop_unlock_desc, afs_vop_unlock },          /* unlock */
	{ &vop_bmap_desc, afs_vop_bmap },              /* bmap */
	{ &vop_strategy_desc, afs_vop_strategy },      /* strategy */
	{ &vop_print_desc, afs_vop_print },            /* print */
	{ &vop_islocked_desc, afs_vop_islocked },      /* islocked */
	{ &vop_pathconf_desc, afs_vop_pathconf },      /* pathconf */
	{ &vop_advlock_desc, afs_vop_advlock },        /* advlock */
	{ &vop_blkatoff_desc, afs_vop_blkatoff },      /* blkatoff */
	{ &vop_valloc_desc, afs_vop_valloc },          /* valloc */
	{ &vop_reallocblks_desc, afs_vop_reallocblks }, /* reallocblks */
	{ &vop_vfree_desc, afs_vop_vfree },            /* vfree */
	{ &vop_truncate_desc, afs_vop_truncate },      /* truncate */
	{ &vop_update_desc, afs_vop_update },          /* update */
	{ &vop_blktooff_desc, afs_vop_blktooff },           /* blktooff */
	{ &vop_offtoblk_desc, afs_vop_offtoblk },           /* offtoblk */
	{ &vop_cmap_desc, afs_vop_cmap },           /* cmap */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc afs_vnodeop_opv_desc =
	{ &afs_vnodeop_p, afs_vnodeop_entries };

#define GETNAME()       \
    struct componentname *cnp = ap->a_cnp; \
    char *name; \
    MALLOC(name, char *, cnp->cn_namelen+1, M_TEMP, M_WAITOK); \
    bcopy(cnp->cn_nameptr, name, cnp->cn_namelen); \
    name[cnp->cn_namelen] = '\0'

#define DROPNAME() FREE(name, M_TEMP)
	


int
afs_vop_lookup(ap)
struct vop_lookup_args /* {
	                  struct vnodeop_desc * a_desc;
	                  struct vnode *a_dvp;
	                  struct vnode **a_vpp;
	                  struct componentname *a_cnp;
	                  } */ *ap;
{
    int error;
    struct vcache *vcp;
    struct vnode *vp, *dvp;
    register int flags = ap->a_cnp->cn_flags;
    int lockparent;                     /* 1 => lockparent flag is set */
    int wantparent;                     /* 1 => wantparent or lockparent flag */
    struct proc *p;
    GETNAME();
    p=cnp->cn_proc;
    lockparent = flags & LOCKPARENT;
    wantparent = flags & (LOCKPARENT|WANTPARENT);

    if (ap->a_dvp->v_type != VDIR) {
	*ap->a_vpp = 0;
	DROPNAME();
	return ENOTDIR;
    }
    dvp = ap->a_dvp;
    if (flags & ISDOTDOT) 
       VOP_UNLOCK(dvp, 0, p);
    AFS_GLOCK();
    error = afs_lookup((struct vcache *)dvp, name, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
        if (flags & ISDOTDOT) 
           VOP_LOCK(dvp, LK_EXCLUSIVE | LK_RETRY, p);
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
	    (flags & ISLASTCN) && error == ENOENT)
	    error = EJUSTRETURN;
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	    cnp->cn_flags |= SAVENAME;
	DROPNAME();
	*ap->a_vpp = 0;
	return (error);
    }
    vp = (struct vnode *)vcp;  /* always get a node if no error */

    /* The parent directory comes in locked.  We unlock it on return
       unless the caller wants it left locked.
       we also always return the vnode locked. */

    if (flags & ISDOTDOT) {
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
        /* always return the child locked */
        if (lockparent && (flags & ISLASTCN) &&
           (error = vn_lock(dvp, LK_EXCLUSIVE, p))) {
            vput(vp);
            DROPNAME();
            return (error);
        }
    } else if (vp == dvp) {
	/* they're the same; afs_lookup() already ref'ed the leaf.
	   It came in locked, so we don't need to ref OR lock it */
    } else {
	if (!lockparent || !(flags & ISLASTCN))
	    VOP_UNLOCK(dvp, 0, p);         /* done with parent. */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
        /* always return the child locked */
    }
    *ap->a_vpp = vp;

    if ((cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN) ||
	 (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))))
	cnp->cn_flags |= SAVENAME;

    DROPNAME();
    return error;
}

int
afs_vop_create(ap)
	struct vop_create_args /* {
	        struct vnode *a_dvp;
	        struct vnode **a_vpp;
	        struct componentname *a_cnp;
	        struct vattr *a_vap;
	} */ *ap;
{
    int error = 0;
    struct vcache *vcp;
    register struct vnode *dvp = ap->a_dvp;
    struct proc *p;
    GETNAME();
    p=cnp->cn_proc;

    /* vnode layer handles excl/nonexcl */
    AFS_GLOCK();
    error = afs_create((struct vcache *)dvp, name, ap->a_vap, NONEXCL,
	               ap->a_vap->va_mode, &vcp,
	               cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return(error);
    }

    if (vcp) {
	*ap->a_vpp = (struct vnode *)vcp;
	vn_lock((struct vnode *)vcp, LK_EXCLUSIVE| LK_RETRY, p);
        if (UBCINFOMISSING((struct vnode *)vcp) ||
            UBCINFORECLAIMED((struct vnode *)vcp))
                ubc_info_init((struct vnode *)vcp);
    }
    else *ap->a_vpp = 0;

    if ((cnp->cn_flags & SAVESTART) == 0)
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    vput(dvp);
    DROPNAME();
    return error;
}

int
afs_vop_mknod(ap)
	struct vop_mknod_args /* {
	        struct vnode *a_dvp;
	        struct vnode **a_vpp;
	        struct componentname *a_cnp;
	        struct vattr *a_vap;
	} */ *ap;
{
    FREE_ZONE(ap->a_cnp->cn_pnbuf, ap->a_cnp->cn_pnlen, M_NAMEI);
    vput(ap->a_dvp);
    return(ENODEV);
}

int
afs_vop_open(ap)
	struct vop_open_args /* {
	        struct vnode *a_vp;
	        int  a_mode;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    int error;
    struct vcache *vc = (struct vcache *)ap->a_vp;
    AFS_GLOCK();
    error = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if ((struct vnode *)vc != ap->a_vp)
	panic("AFS open changed vnode!");
#endif
    afs_BozonLock(&vc->pvnLock, vc);
    osi_FlushPages(vc);
    afs_BozonUnlock(&vc->pvnLock, vc);
    AFS_GUNLOCK();
    return error;
}

int
afs_vop_close(ap)
	struct vop_close_args /* {
	        struct vnode *a_vp;
	        int  a_fflag;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    int code;
    struct vcache *avc=ap->a_vp;
    AFS_GLOCK();
    if (ap->a_cred) 
        code=afs_close(avc, ap->a_fflag, ap->a_cred, ap->a_p);
    else
        code=afs_close(avc, ap->a_fflag, &afs_osi_cred, ap->a_p);
    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc);        /* hold bozon lock, but not basic vnode lock */
    afs_BozonUnlock(&avc->pvnLock, avc);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_access(ap)
	struct vop_access_args /* {
	        struct vnode *a_vp;
	        int  a_mode;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    int code;
    AFS_GLOCK();
    code=afs_access((struct vcache *)ap->a_vp, ap->a_mode, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}
int
afs_vop_getattr(ap)
	struct vop_getattr_args /* {
	        struct vnode *a_vp;
	        struct vattr *a_vap;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    int code;
    AFS_GLOCK();
    code=afs_getattr((struct vcache *)ap->a_vp, ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}
int
afs_vop_setattr(ap)
	struct vop_setattr_args /* {
	        struct vnode *a_vp;
	        struct vattr *a_vap;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    int code;
    AFS_GLOCK();
    code=afs_setattr((struct vcache *)ap->a_vp, ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}
int
afs_vop_read(ap)
	struct vop_read_args /* {
	        struct vnode *a_vp;
	        struct uio *a_uio;
	        int a_ioflag;
	        struct ucred *a_cred;
	} */ *ap;
{
    int code;
    struct vcache *avc=(struct vcache *)ap->a_vp;
    AFS_GLOCK();
    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc);        /* hold bozon lock, but not basic vnode lock */
    code=afs_read(avc, ap->a_uio, ap->a_cred, 0, 0, 0);
    afs_BozonUnlock(&avc->pvnLock, avc);
    AFS_GUNLOCK();
    return code;
}
int
afs_vop_pagein(ap)
	struct vop_pagein_args /* {
	        struct vnode *a_vp;
        	upl_t a_pl;
        	vm_offset_t a_pl_offset;
        	off_t a_f_offset;
        	size_t a_size;
        	struct ucred *a_cred;
        	int a_flags;
	} */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    size_t size= ap->a_size;
    off_t f_offset = ap->a_f_offset;
    vm_offset_t pl_offset = ap->a_pl_offset;
    int flags  = ap->a_flags;
    struct ucred *cred;
    vm_offset_t ioaddr;
    struct uio      auio;
    struct iovec    aiov;
    struct uio * uio = &auio;
    int nocommit = flags & UPL_NOCOMMIT;

    int code;
    struct vcache *tvc=(struct vcache *)vp;

    if (UBCINVALID(vp)) {
#if DIAGNOSTIC
        panic("afs_vop_pagein: invalid vp");
#endif /* DIAGNOSTIC */
        return (EPERM);
    }

    UBCINFOCHECK("afs_vop_pagein", vp);
    if(pl == (upl_t)NULL) {
            panic("afs_vop_pagein: no upl");
    }

    cred = ubc_getcred(vp);
    if (cred == NOCRED)
        cred = ap->a_cred;

    if (size == 0) {
            if (!nocommit)
                    kernel_upl_abort_range(pl, pl_offset, size, 
                            UPL_ABORT_ERROR |  UPL_ABORT_FREE_ON_EMPTY);
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
    aiov.iov_base = (caddr_t)ioaddr;
    AFS_GLOCK();
    afs_BozonLock(&tvc->pvnLock, tvc);
    osi_FlushPages(tvc);        /* hold bozon lock, but not basic vnode lock */
    code=afs_read(tvc, uio, cred, 0, 0, 0);
    if (code == 0) {
      ObtainWriteLock(&tvc->lock, 2);
      tvc->states |= CMAPPED;
      ReleaseWriteLock(&tvc->lock);
    }
    afs_BozonUnlock(&tvc->pvnLock, tvc);
    AFS_GUNLOCK();
    kernel_upl_unmap(kernel_map, pl);
    if (!nocommit) {
      if (code)
	 kernel_upl_abort_range(pl, pl_offset, size, 
			 UPL_ABORT_ERROR |  UPL_ABORT_FREE_ON_EMPTY);
      else
         kernel_upl_commit_range(pl, pl_offset, size,
                          UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY,
                          UPL_GET_INTERNAL_PAGE_LIST(pl), MAX_UPL_TRANSFER);
    }
    return code;
}

int
afs_vop_write(ap)
	struct vop_write_args /* {
	        struct vnode *a_vp;
	        struct uio *a_uio;
	        int a_ioflag;
	        struct ucred *a_cred;
	} */ *ap;
{
    int code;
    struct vcache *avc=(struct vcache *)ap->a_vp;
    void *object;
    AFS_GLOCK();
    afs_BozonLock(&avc->pvnLock, avc);
    osi_FlushPages(avc);        /* hold bozon lock, but not basic vnode lock */
    if (UBCINFOEXISTS(ap->a_vp))
       ubc_clean(ap->a_vp, 1);
    if (UBCINFOEXISTS(ap->a_vp))
       osi_VM_NukePages(ap->a_vp, ap->a_uio->uio_offset, ap->a_uio->uio_resid);
    code=afs_write((struct vcache *)ap->a_vp, ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
    afs_BozonUnlock(&avc->pvnLock, avc);
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_pageout(ap)
	struct vop_pageout_args /* {
	        struct vnode *a_vp;
                upl_t   a_pl,
                vm_offset_t   a_pl_offset,
                off_t         a_f_offset,
                size_t        a_size,
                struct ucred *a_cred,
                int           a_flags
	} */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    upl_t pl = ap->a_pl;
    size_t size= ap->a_size;
    off_t f_offset = ap->a_f_offset;
    vm_offset_t pl_offset = ap->a_pl_offset;
    int flags  = ap->a_flags;
    struct ucred *cred;
    vm_offset_t ioaddr;
    struct uio      auio;
    struct iovec    aiov;
    struct uio * uio = &auio;
    int nocommit = flags & UPL_NOCOMMIT;

    int code;
    struct vcache *tvc=(struct vcache *)vp;

    if (UBCINVALID(vp)) {
#if DIAGNOSTIC
        panic("afs_vop_pageout: invalid vp");
#endif /* DIAGNOSTIC */
        return (EPERM);
    }

    UBCINFOCHECK("afs_vop_pageout", vp);
    if(pl == (upl_t)NULL) {
            panic("afs_vop_pageout: no upl");
    }
#if 1
    { int lbn, iosize, s;
      struct buf *bp;
      int biosize = DEV_BSIZE;

      lbn = f_offset / DEV_BSIZE;

        for (iosize = size; iosize > 0; iosize -= biosize, lbn++) {

                s = splbio();
                if (bp = incore(vp, lbn)) {
                        if (ISSET(bp->b_flags, B_BUSY))
                                panic("nfs_pageout: found BUSY buffer incore\n")
;
                        
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

    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = f_offset;
    auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_rw = UIO_WRITE;
    auio.uio_procp = NULL;
    kernel_upl_map(kernel_map, pl, &ioaddr);
    ioaddr += pl_offset;
    auio.uio_resid = aiov.iov_len = size;
    aiov.iov_base = (caddr_t)ioaddr;
#if 1 /* USV [ */
        {
                /* 
                 * check for partial page and clear the
                 * contents past end of the file before
                 * releasing it in the VM page cache
                 */
                if ((f_offset < tvc->m.Length) && (f_offset + size) > tvc->m.Length) {
                        size_t io = tvc->m.Length - f_offset;

                        bzero((caddr_t)(ioaddr + pl_offset + io), size - io);
                }
        }
#endif /* ] USV */

    AFS_GLOCK();
    afs_BozonLock(&tvc->pvnLock, tvc);
    osi_FlushPages(tvc);        /* hold bozon lock, but not basic vnode lock */
    ObtainWriteLock(&tvc->lock, 1);
    afs_FakeOpen(tvc);
    ReleaseWriteLock(&tvc->lock);

    code=afs_write(tvc, uio, flags, cred, 0);

    ObtainWriteLock(&tvc->lock, 1);
    afs_FakeClose(tvc, cred);
    ReleaseWriteLock(&tvc->lock);
    afs_BozonUnlock(&tvc->pvnLock, tvc);
    AFS_GUNLOCK();
    kernel_upl_unmap(kernel_map, pl);
    if (!nocommit) {
            if(code)
                    kernel_upl_abort_range(pl, pl_offset, size, 
                        UPL_ABORT_FREE_ON_EMPTY);
            else
                    kernel_upl_commit_range(pl, pl_offset, size,
                         UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY,
                         UPL_GET_INTERNAL_PAGE_LIST(pl), MAX_UPL_TRANSFER);
    }

    return code;
}
int
afs_vop_ioctl(ap)
	struct vop_ioctl_args /* {
	        struct vnode *a_vp;
	        int  a_command;
	        caddr_t  a_data;
	        int  a_fflag;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    struct vcache *tvc = (struct vcache *)ap->a_vp;
    struct afs_ioctl data;
    int error = 0;
  
    /* in case we ever get in here... */

    AFS_STATCNT(afs_ioctl);
    if (((ap->a_command >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
    AFS_GLOCK();
	error = HandleIoctl(tvc, (struct file *)0/*Not used*/,
	                    ap->a_command, ap->a_data);
    AFS_GUNLOCK();
	return(error);
    } else {
	/* No-op call; just return. */
	return(ENOTTY);
    }
}

/* ARGSUSED */
int
afs_vop_select(ap)
	struct vop_select_args /* {
	        struct vnode *a_vp;
	        int  a_which;
	        int  a_fflags;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
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
	struct vop_mmap_args /* {
	        struct vnode *a_vp;
	        int  a_fflags;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
	return (EINVAL);
}

int
afs_vop_fsync(ap)
	struct vop_fsync_args /* {
	        struct vnode *a_vp;
	        struct ucred *a_cred;
	        int a_waitfor;
	        struct proc *a_p;
	} */ *ap;
{
    int wait = ap->a_waitfor == MNT_WAIT;
    int error;
    register struct vnode *vp = ap->a_vp;

    AFS_GLOCK();
    /*vflushbuf(vp, wait);*/
    if (ap->a_cred)
      error=afs_fsync((struct vcache *)vp, ap->a_cred);
    else
      error=afs_fsync((struct vcache *)vp, &afs_osi_cred);
    AFS_GUNLOCK();
    return error;
}

int
afs_vop_seek(ap)
	struct vop_seek_args /* {
	        struct vnode *a_vp;
	        off_t  a_oldoff;
	        off_t  a_newoff;
	        struct ucred *a_cred;
	} */ *ap;
{
    if (ap->a_newoff > ULONG_MAX)       /* AFS doesn't support 64-bit offsets */
	return EINVAL;
    return (0);
}

int
afs_vop_remove(ap)
	struct vop_remove_args /* {
	        struct vnode *a_dvp;
	        struct vnode *a_vp;
	        struct componentname *a_cnp;
	} */ *ap;
{
    int error = 0;
    register struct vnode *vp = ap->a_vp;
    register struct vnode *dvp = ap->a_dvp;

    GETNAME();
    AFS_GLOCK();
    error =  afs_remove((struct vcache *)dvp, name, cnp->cn_cred);
    AFS_GUNLOCK();
    cache_purge(vp);
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
    if (UBCINFOEXISTS(vp)) {
             int wasmapped=ubc_issetflags(vp, UI_WASMAPPED);
             int hasobjref=ubc_issetflags(vp, UI_HASOBJREF);
             if (wasmapped)
                (void) ubc_uncache(vp); 
             if (hasobjref)
                ubc_release(vp);
             /* WARNING vp may not be valid after this */
    }

    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    DROPNAME();
    return error;
}

int
afs_vop_link(ap)
	struct vop_link_args /* {
	        struct vnode *a_vp;
	        struct vnode *a_tdvp;
	        struct componentname *a_cnp;
	} */ *ap;
{
    int error = 0;
    register struct vnode *dvp = ap->a_tdvp;
    register struct vnode *vp = ap->a_vp;
    struct proc *p;

    GETNAME();
    p=cnp->cn_proc;
    if (dvp->v_mount != vp->v_mount) {
	VOP_ABORTOP(vp, cnp);
	error = EXDEV;
	goto out;
    }
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
    error = afs_link((struct vcache *)vp, (struct vcache *)dvp, name, cnp->cn_cred);
    AFS_GUNLOCK();
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    if (dvp != vp)
	VOP_UNLOCK(vp,0, p);
out:
    vput(dvp);
    DROPNAME();
    return error;
}

int
afs_vop_rename(ap)
	struct vop_rename_args  /* {
	        struct vnode *a_fdvp;
	        struct vnode *a_fvp;
	        struct componentname *a_fcnp;
	        struct vnode *a_tdvp;
	        struct vnode *a_tvp;
	        struct componentname *a_tcnp;
	} */ *ap;
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
    struct proc *p=fcnp->cn_proc;

    /*
     * Check for cross-device rename.
     */
    if ((fvp->v_mount != tdvp->v_mount) ||
	(tvp && (fvp->v_mount != tvp->v_mount))) {
	error = EXDEV;
abortit:
	VOP_ABORTOP(tdvp, tcnp); /* XXX, why not in NFS? */
	if (tdvp == tvp)
	    vrele(tdvp);
	else
	    vput(tdvp);
	if (tvp)
	    vput(tvp);
	VOP_ABORTOP(fdvp, fcnp); /* XXX, why not in NFS? */
	vrele(fdvp);
	vrele(fvp);
	return (error);
    }
    /*
     * if fvp == tvp, we're just removing one name of a pair of
     * directory entries for the same element.  convert call into rename.
     ( (pinched from NetBSD 1.0's ufs_rename())
     */
    if (fvp == tvp) {
	if (fvp->v_type == VDIR) {
	    error = EINVAL;
	    goto abortit;
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
	(void) relookup(fdvp, &fvp, fcnp);
	return (VOP_REMOVE(fdvp, fvp, fcnp));
    }
    if (error = vn_lock(fvp, LK_EXCLUSIVE, p))
	goto abortit;

    MALLOC(fname, char *, fcnp->cn_namelen+1, M_TEMP, M_WAITOK);
    bcopy(fcnp->cn_nameptr, fname, fcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    MALLOC(tname, char *, tcnp->cn_namelen+1, M_TEMP, M_WAITOK);
    bcopy(tcnp->cn_nameptr, tname, tcnp->cn_namelen);
    tname[tcnp->cn_namelen] = '\0';


    AFS_GLOCK();
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    error = afs_rename((struct vcache *)fdvp, fname, (struct vcache *)tdvp, tname, tcnp->cn_cred);
    AFS_GUNLOCK();

    VOP_UNLOCK(fvp, 0, p);
    FREE(fname, M_TEMP);
    FREE(tname, M_TEMP);
    if (error)
	goto abortit;                   /* XXX */
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
	struct vop_mkdir_args /* {
	        struct vnode *a_dvp;
	        struct vnode **a_vpp;
	        struct componentname *a_cnp;
	        struct vattr *a_vap;
	} */ *ap;
{
    register struct vnode *dvp = ap->a_dvp;
    register struct vattr *vap = ap->a_vap;
    int error = 0;
    struct vcache *vcp;
    struct proc *p;

    GETNAME();
    p=cnp->cn_proc;
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_vop_mkdir: no name");
#endif
    AFS_GLOCK();
    error = afs_mkdir((struct vcache *)dvp, name, vap, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return(error);
    }
    if (vcp) {
	*ap->a_vpp = (struct vnode *)vcp;
	vn_lock((struct vnode *)vcp, LK_EXCLUSIVE|LK_RETRY, p);
    } else
	*ap->a_vpp = 0;
    DROPNAME();
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    vput(dvp);
    return error;
}

int
afs_vop_rmdir(ap)
	struct vop_rmdir_args /* {
	        struct vnode *a_dvp;
	        struct vnode *a_vp;
	        struct componentname *a_cnp;
	} */ *ap;
{
    int error = 0;
    register struct vnode *vp = ap->a_vp;
    register struct vnode *dvp = ap->a_dvp;

    GETNAME();
    if (dvp == vp) {
	vrele(dvp);
	vput(vp);
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
	DROPNAME();
	return (EINVAL);
    }

    AFS_GLOCK();
    error = afs_rmdir((struct vcache *)dvp, name, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    vput(dvp);
    vput(vp);
    return error;
}

int
afs_vop_symlink(ap)
	struct vop_symlink_args /* {
	        struct vnode *a_dvp;
	        struct vnode **a_vpp;
	        struct componentname *a_cnp;
	        struct vattr *a_vap;
	        char *a_target;
	} */ *ap;
{
    register struct vnode *dvp = ap->a_dvp;
    int error = 0;
    /* NFS ignores a_vpp; so do we. */

    GETNAME();
    AFS_GLOCK();
    error = afs_symlink((struct vcache *)dvp, name, ap->a_vap, ap->a_target,
	                cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
    vput(dvp);
    return error;
}

int
afs_vop_readdir(ap)
	struct vop_readdir_args /* {
	        struct vnode *a_vp;
	        struct uio *a_uio;
	        struct ucred *a_cred;
	        int *a_eofflag;
	        u_long *a_cookies;
	        int ncookies;
	} */ *ap;
{
    int error;
    off_t off;
/*    printf("readdir %x cookies %x ncookies %d\n", ap->a_vp, ap->a_cookies,
	   ap->a_ncookies); */
    off=ap->a_uio->uio_offset;
    AFS_GLOCK();
    error= afs_readdir((struct vcache *)ap->a_vp, ap->a_uio, ap->a_cred,
	               ap->a_eofflag);
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

	dp_end = (const struct dirent *) uio->uio_iov->iov_base;
	for (dp_start = dp, ncookies = 0;
	     dp < dp_end;
	     dp = (const struct dirent *)((const char *) dp + dp->d_reclen))
	    ncookies++;

	MALLOC(cookies, u_long *, ncookies * sizeof(u_long),
	       M_TEMP, M_WAITOK);
	for (dp = dp_start, cookiep = cookies;
	     dp < dp_end;
	     dp = (const struct dirent *)((const char *) dp + dp->d_reclen)) {
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
	struct vop_readlink_args /* {
	        struct vnode *a_vp;
	        struct uio *a_uio;
	        struct ucred *a_cred;
	} */ *ap;
{
    int error;
/*    printf("readlink %x\n", ap->a_vp);*/
    AFS_GLOCK();
    error= afs_readlink((struct vcache *)ap->a_vp, ap->a_uio, ap->a_cred);
    AFS_GUNLOCK();
    return error;
}

extern int prtactive;

int
afs_vop_inactive(ap)
	struct vop_inactive_args /* {
	        struct vnode *a_vp;
                struct proc *a_p;
	} */ *ap;
{
    register struct vnode *vp = ap->a_vp;

    if (prtactive && vp->v_usecount != 0)
	vprint("afs_vop_inactive(): pushing active", vp);

    AFS_GLOCK();
    afs_InactiveVCache((struct vcache *)vp, 0);   /* decrs ref counts */
    AFS_GUNLOCK();
    VOP_UNLOCK(vp, 0, ap->a_p);
    return 0;
}

int
afs_vop_reclaim(ap)
	struct vop_reclaim_args /* {
	        struct vnode *a_vp;
	} */ *ap;
{
    int error;
    int sl;
    register struct vnode *vp = ap->a_vp;

    cache_purge(vp);                    /* just in case... */

#if 0 
    AFS_GLOCK();
    error = afs_FlushVCache((struct vcache *)vp, &sl); /* tosses our stuff from vnode */
    AFS_GUNLOCK();
    ubc_unlink(vp);
    if (!error && vp->v_data)
	panic("afs_reclaim: vnode not cleaned");
    return error;
#else
   if (vp->v_usecount == 2) {
        vprint("reclaim count==2", vp);
   } else if (vp->v_usecount == 1) {
        vprint("reclaim count==1", vp);
   } else 
        vprint("reclaim bad count", vp);

   return 0;
#endif
}

int
afs_vop_lock(ap)
	struct vop_lock_args /* {
	        struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct vcache *avc = (struct vcache *)vp;

	if (vp->v_tag == VT_NON)
	        return (ENOENT);
	return (lockmgr(&avc->rwlock, ap->a_flags, &vp->v_interlock,
                ap->a_p));
}

int
afs_vop_unlock(ap)
	struct vop_unlock_args /* {
	        struct vnode *a_vp;
	} */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = (struct vcache *)vp;
    return (lockmgr(&avc->rwlock, ap->a_flags | LK_RELEASE,
            &vp->v_interlock, ap->a_p));

}

int
afs_vop_bmap(ap)
	struct vop_bmap_args /* {
	        struct vnode *a_vp;
	        daddr_t  a_bn;
	        struct vnode **a_vpp;
	        daddr_t *a_bnp;
	        int *a_runp;
	        int *a_runb;
	} */ *ap;
{
    struct vcache *vcp;
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
	struct vop_strategy_args /* {
	        struct buf *a_bp;
	} */ *ap;
{
    int error;
    AFS_GLOCK();
    error= afs_ustrategy(ap->a_bp);
    AFS_GUNLOCK();
    return error;
}
int
afs_vop_print(ap)
	struct vop_print_args /* {
	        struct vnode *a_vp;
	} */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    register struct vcache *vc = (struct vcache *)ap->a_vp;
    int s = vc->states;
    char buf[20];
    printf("tag %d, fid: %ld.%x.%x.%x, opens %d, writers %d", vp->v_tag, vc->fid.Cell,
	   vc->fid.Fid.Volume, vc->fid.Fid.Vnode, vc->fid.Fid.Unique, vc->opens,
	   vc->execsOrWriters);
    printf("\n  states%s%s%s%s%s", (s&CStatd) ? " statd" : "", (s&CRO) ? " readonly" : "",(s&CDirty) ? " dirty" : "",(s&CMAPPED) ? " mapped" : "", (s&CVFlushed) ? " flush in progress" : "");
    if (UBCISVALID(vp))
        printf("\n  UBC: %s%s",
               UBCINFOEXISTS(vp) ? "exists, " : "does not exist",
               UBCINFOEXISTS(vp) ?
                 sprintf(buf, "holdcnt %d", vp->v_ubcinfo->ui_holdcnt),buf : "");
    printf("\n");
    return 0;
}

int
afs_vop_islocked(ap)
	struct vop_islocked_args /* {
	        struct vnode *a_vp;
	} */ *ap;
{
    struct vcache *vc = (struct vcache *)ap->a_vp;
    return lockstatus(&vc->rwlock);
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
afs_vop_pathconf(ap)
	struct vop_pathconf_args /* {
	        struct vnode *a_vp;
	        int a_name;
	        int *a_retval;
	} */ *ap;
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
	struct vop_advlock_args /* {
	        struct vnode *a_vp;
	        caddr_t  a_id;
	        int  a_op;
	        struct flock *a_fl;
	        int  a_flags;
	} */ *ap;
{
    int error;
    struct proc *p=current_proc();
    struct ucred cr;
    pcred_readlock(p);
    cr=*p->p_cred->pc_ucred;
    pcred_unlock(p);
    AFS_GLOCK();
    error= afs_lockctl((struct vcache *)ap->a_vp, ap->a_fl, ap->a_op, &cr,
	               (int) ap->a_id);
    AFS_GUNLOCK();
    return error;
}

int
afs_vop_truncate(ap)
	struct vop_truncate_args /* {
	        struct vnode *a_vp;
	        off_t a_length;
	        int a_flags;
	        struct ucred *a_cred;
	        struct proc *a_p;
	} */ *ap;
{
    printf("stray afs_vop_truncate\n");
    return EOPNOTSUPP;
}

int
afs_vop_update(ap)
	struct vop_update_args /* {
	        struct vnode *a_vp;
	        struct timeval *a_access;
	        struct timeval *a_modify;
	        int a_waitfor;
	} */ *ap;
{
    printf("stray afs_vop_update\n");
    return EOPNOTSUPP;
}

int afs_vop_blktooff(ap)
        struct vop_blktooff_args /* {
                struct vnode *a_vp;
                daddr_t a_lblkno;
                off_t *a_offset;    
        } */ *ap;
{
	*ap->a_offset = (off_t)(ap->a_lblkno *  DEV_BSIZE);
        return 0;
}

int afs_vop_offtoblk(ap)
        struct vop_offtoblk_args /* {
                struct vnode *a_vp;
                off_t a_offset;    
                daddr_t *a_lblkno;
        } */ *ap;
{
	*ap->a_lblkno = (daddr_t)(ap->a_offset /  DEV_BSIZE);

        return (0);
}

int afs_vop_cmap(ap)
        struct vop_cmap_args /* {
                struct vnode *a_vp;
                off_t a_foffset;    
                size_t a_size;
                daddr_t *a_bpn;
                size_t *a_run;
                void *a_poff;
        } */ *ap;
{
        *ap->a_bpn = (daddr_t)(ap->a_foffset /  DEV_BSIZE);	
        *ap->a_run= MAX(ap->a_size, AFS_CHUNKSIZE(ap->a_foffset));
	return 0;
}

