/*
 * A large chunk of this file appears to be copied directly from
 * sys/nfsclient/nfs_bio.c, which has the following license:
 */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfs_bio.c	8.9 (Berkeley) 3/30/95
 */
/*
 * Pursuant to a statement of U.C. Berkeley dated 1999-07-22, this license
 * is amended to drop clause (3) above.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/afs/FBSD/osi_vnodeops.c,v 1.18.2.4 2005/05/23 21:26:40 shadow Exp $");

#include <afs/sysincludes.h>	/* Standard vendor system headers */
#include <afsincludes.h>	/* Afs-based standard headers */
#include <afs/afs_stats.h>	/* statistics */
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#ifndef AFS_FBSD50_ENV
#include <vm/vm_zone.h>
#endif
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
extern int afs_pbuf_freecnt;

#ifdef AFS_FBSD60_ENV
static vop_access_t	afs_vop_access;
static vop_advlock_t	afs_vop_advlock;
static vop_close_t	afs_vop_close;
static vop_create_t	afs_vop_create;
static vop_fsync_t	afs_vop_fsync;
static vop_getattr_t	afs_vop_getattr;
static vop_getpages_t	afs_vop_getpages;
static vop_inactive_t	afs_vop_inactive;
static vop_ioctl_t	afs_vop_ioctl;
static vop_link_t	afs_vop_link;
static vop_lookup_t	afs_vop_lookup;
static vop_mkdir_t	afs_vop_mkdir;
static vop_mknod_t	afs_vop_mknod;
static vop_open_t	afs_vop_open;
static vop_pathconf_t	afs_vop_pathconf;
static vop_poll_t	afs_vop_poll;
static vop_print_t	afs_vop_print;
static vop_putpages_t	afs_vop_putpages;
static vop_read_t	afs_vop_read;
static vop_readdir_t	afs_vop_readdir;
static vop_readlink_t	afs_vop_readlink;
static vop_reclaim_t	afs_vop_reclaim;
static vop_remove_t	afs_vop_remove;
static vop_rename_t	afs_vop_rename;
static vop_rmdir_t	afs_vop_rmdir;
static vop_setattr_t	afs_vop_setattr;
static vop_strategy_t	afs_vop_strategy;
static vop_symlink_t	afs_vop_symlink;
static vop_write_t	afs_vop_write;

struct vop_vector afs_vnodeops = {
	.vop_default =		&default_vnodeops,
	.vop_access =		afs_vop_access,
	.vop_advlock =		afs_vop_advlock,
	.vop_close =		afs_vop_close,
	.vop_create =		afs_vop_create,
	.vop_fsync =		afs_vop_fsync,
	.vop_getattr =		afs_vop_getattr,
	.vop_getpages =		afs_vop_getpages,
	.vop_inactive =		afs_vop_inactive,
	.vop_ioctl =		afs_vop_ioctl,
	.vop_lease =		VOP_NULL,
	.vop_link =		afs_vop_link,
	.vop_lookup =		afs_vop_lookup,
	.vop_mkdir =		afs_vop_mkdir,
	.vop_mknod =		afs_vop_mknod,
	.vop_open =		afs_vop_open,
	.vop_pathconf =		afs_vop_pathconf,
	.vop_poll =		afs_vop_poll,
	.vop_print =		afs_vop_print,
	.vop_putpages =		afs_vop_putpages,
	.vop_read =		afs_vop_read,
	.vop_readdir =		afs_vop_readdir,
	.vop_readlink =		afs_vop_readlink,
	.vop_reclaim =		afs_vop_reclaim,
	.vop_remove =		afs_vop_remove,
	.vop_rename =		afs_vop_rename,
	.vop_rmdir =		afs_vop_rmdir,
	.vop_setattr =		afs_vop_setattr,
	.vop_strategy =		afs_vop_strategy,
	.vop_symlink =		afs_vop_symlink,
	.vop_write =		afs_vop_write,
};

#else /* AFS_FBSD60_ENV */

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
int afs_vop_getpages(struct vop_getpages_args *);
int afs_vop_putpages(struct vop_putpages_args *);
int afs_vop_ioctl(struct vop_ioctl_args *);
static int afs_vop_pathconf(struct vop_pathconf_args *);
int afs_vop_poll(struct vop_poll_args *);
#ifndef AFS_FBSD50_ENV
int afs_vop_mmap(struct vop_mmap_args *);
#endif
int afs_vop_fsync(struct vop_fsync_args *);
int afs_vop_remove(struct vop_remove_args *);
int afs_vop_link(struct vop_link_args *);
int afs_vop_rename(struct vop_rename_args *);
int afs_vop_mkdir(struct vop_mkdir_args *);
int afs_vop_rmdir(struct vop_rmdir_args *);
int afs_vop_symlink(struct vop_symlink_args *);
int afs_vop_readdir(struct vop_readdir_args *);
int afs_vop_readlink(struct vop_readlink_args *);
int afs_vop_inactive(struct vop_inactive_args *);
int afs_vop_reclaim(struct vop_reclaim_args *);
int afs_vop_bmap(struct vop_bmap_args *);
int afs_vop_strategy(struct vop_strategy_args *);
int afs_vop_print(struct vop_print_args *);
int afs_vop_advlock(struct vop_advlock_args *);



/* Global vfs data structures for AFS. */
vop_t **afs_vnodeop_p;
struct vnodeopv_entry_desc afs_vnodeop_entries[] = {
    {&vop_default_desc, (vop_t *) vop_defaultop},
    {&vop_access_desc, (vop_t *) afs_vop_access},	/* access */
    {&vop_advlock_desc, (vop_t *) afs_vop_advlock},	/* advlock */
    {&vop_bmap_desc, (vop_t *) afs_vop_bmap},	/* bmap */
#ifndef AFS_FBSD50_ENV
    {&vop_bwrite_desc, (vop_t *) vop_stdbwrite},
#endif
    {&vop_close_desc, (vop_t *) afs_vop_close},	/* close */
    {&vop_createvobject_desc, (vop_t *) vop_stdcreatevobject},
    {&vop_destroyvobject_desc, (vop_t *) vop_stddestroyvobject},
    {&vop_create_desc, (vop_t *) afs_vop_create},	/* create */
    {&vop_fsync_desc, (vop_t *) afs_vop_fsync},	/* fsync */
    {&vop_getattr_desc, (vop_t *) afs_vop_getattr},	/* getattr */
    {&vop_getpages_desc, (vop_t *) afs_vop_getpages},	/* read */
    {&vop_getvobject_desc, (vop_t *) vop_stdgetvobject},
    {&vop_putpages_desc, (vop_t *) afs_vop_putpages},	/* write */
    {&vop_inactive_desc, (vop_t *) afs_vop_inactive},	/* inactive */
    {&vop_lease_desc, (vop_t *) vop_null},
    {&vop_link_desc, (vop_t *) afs_vop_link},	/* link */
    {&vop_lookup_desc, (vop_t *) afs_vop_lookup},	/* lookup */
    {&vop_mkdir_desc, (vop_t *) afs_vop_mkdir},	/* mkdir */
    {&vop_mknod_desc, (vop_t *) afs_vop_mknod},	/* mknod */
#ifndef AFS_FBSD50_ENV
    {&vop_mmap_desc, (vop_t *) afs_vop_mmap},	/* mmap */
#endif
    {&vop_open_desc, (vop_t *) afs_vop_open},	/* open */
    {&vop_pathconf_desc, (vop_t *) afs_vop_pathconf},	/* pathconf */
    {&vop_poll_desc, (vop_t *) afs_vop_poll},	/* select */
    {&vop_print_desc, (vop_t *) afs_vop_print},	/* print */
    {&vop_read_desc, (vop_t *) afs_vop_read},	/* read */
    {&vop_readdir_desc, (vop_t *) afs_vop_readdir},	/* readdir */
    {&vop_readlink_desc, (vop_t *) afs_vop_readlink},	/* readlink */
    {&vop_reclaim_desc, (vop_t *) afs_vop_reclaim},	/* reclaim */
    {&vop_remove_desc, (vop_t *) afs_vop_remove},	/* remove */
    {&vop_rename_desc, (vop_t *) afs_vop_rename},	/* rename */
    {&vop_rmdir_desc, (vop_t *) afs_vop_rmdir},	/* rmdir */
    {&vop_setattr_desc, (vop_t *) afs_vop_setattr},	/* setattr */
    {&vop_strategy_desc, (vop_t *) afs_vop_strategy},	/* strategy */
    {&vop_symlink_desc, (vop_t *) afs_vop_symlink},	/* symlink */
    {&vop_write_desc, (vop_t *) afs_vop_write},	/* write */
    {&vop_ioctl_desc, (vop_t *) afs_vop_ioctl},	/* XXX ioctl */
    /*{ &vop_seek_desc, afs_vop_seek }, *//* seek */
    {NULL, NULL}
};
struct vnodeopv_desc afs_vnodeop_opv_desc =
    { &afs_vnodeop_p, afs_vnodeop_entries };
#endif /* AFS_FBSD60_ENV */

#define GETNAME()       \
    struct componentname *cnp = ap->a_cnp; \
    char *name; \
    MALLOC(name, char *, cnp->cn_namelen+1, M_TEMP, M_WAITOK); \
    memcpy(name, cnp->cn_nameptr, cnp->cn_namelen); \
    name[cnp->cn_namelen] = '\0'

#define DROPNAME() FREE(name, M_TEMP)

/* This is a bit of a cheat... */
#ifdef AFS_FBSD50_ENV
#define a_p a_td
#endif

/*
 * Mosty copied from sys/ufs/ufs/ufs_vnops.c:ufs_pathconf().
 * We should know the correct answers to these questions with
 * respect to the AFS protocol (which may differ from the UFS
 * values) but for the moment this will do.
 */
static int
afs_vop_pathconf(struct vop_pathconf_args *ap)
{
	int error;

	error = 0;
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
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		break;
#ifdef _PC_ACL_EXTENDED
	case _PC_ACL_EXTENDED:
		*ap->a_retval = 0;
		break;
	case _PC_ACL_PATH_MAX:
		*ap->a_retval = 3;
		break;
#endif
#ifdef _PC_MAC_PRESENT
	case _PC_MAC_PRESENT:
		*ap->a_retval = 0;
		break;
#endif
#ifdef _PC_ASYNC_IO
	case _PC_ASYNC_IO:
		/* _PC_ASYNC_IO should have been handled by upper layers. */
		KASSERT(0, ("_PC_ASYNC_IO should not get here"));
		error = EINVAL;
		break;
	case _PC_PRIO_IO:
		*ap->a_retval = 0;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 0;
		break;
#endif
#ifdef _PC_ALLOC_SIZE_MIN
	case _PC_ALLOC_SIZE_MIN:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_bsize;
		break;
#endif
#ifdef _PC_FILESIZEBITS
	case _PC_FILESIZEBITS:
		*ap->a_retval = 32; /* XXX */
		break;
#endif
#ifdef _PC_REC_INCR_XFER_SIZE
	case _PC_REC_INCR_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_MAX_XFER_SIZE:
		*ap->a_retval = -1; /* means ``unlimited'' */
		break;
	case _PC_REC_MIN_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_XFER_ALIGN:
		*ap->a_retval = PAGE_SIZE;
		break;
#endif
#ifdef _PC_SYMLINK_MAX
	case _PC_SYMLINK_MAX:
		*ap->a_retval = MAXPATHLEN;
		break;
#endif
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
afs_vop_lookup(ap)
     struct vop_lookup_args	/* {
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
#ifdef AFS_FBSD50_ENV
    struct thread *p = ap->a_cnp->cn_thread;
#else
    struct proc *p = ap->a_cnp->cn_proc;
#endif
    GETNAME();

    lockparent = flags & LOCKPARENT;
    wantparent = flags & (LOCKPARENT | WANTPARENT);

    if (ap->a_dvp->v_type != VDIR) {
	*ap->a_vpp = 0;
	DROPNAME();
	return ENOTDIR;
    }
    dvp = ap->a_dvp;
    if (flags & ISDOTDOT)
	VOP_UNLOCK(dvp, 0, p);
    AFS_GLOCK();
    error = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	if (flags & ISDOTDOT)
	    VOP_LOCK(dvp, LK_EXCLUSIVE | LK_RETRY, p);
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
	    && (flags & ISLASTCN) && error == ENOENT)
	    error = EJUSTRETURN;
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	    cnp->cn_flags |= SAVENAME;
	DROPNAME();
	*ap->a_vpp = 0;
	return (error);
    }
    vp = AFSTOV(vcp);		/* always get a node if no error */

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
    *ap->a_vpp = vp;

    if ((cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN))
	|| (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN)))
	cnp->cn_flags |= SAVENAME;

    DROPNAME();
    return error;
}

int
afs_vop_create(ap)
     struct vop_create_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    int error = 0;
    struct vcache *vcp;
    register struct vnode *dvp = ap->a_dvp;
#ifdef AFS_FBSD50_ENV
    struct thread *p = ap->a_cnp->cn_thread;
#else
    struct proc *p = ap->a_cnp->cn_proc;
#endif
    GETNAME();

    AFS_GLOCK();
    error =
	afs_create(VTOAFS(dvp), name, ap->a_vap,
		   ap->a_vap->va_vaflags & VA_EXCLUSIVE ? EXCL : NONEXCL,
		   ap->a_vap->va_mode, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	DROPNAME();
	return (error);
    }

    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY, p);
    } else
	*ap->a_vpp = 0;

    DROPNAME();
    return error;
}

int
afs_vop_mknod(ap)
     struct vop_mknod_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap;
{
    return (ENODEV);
}

#if 0
static int
validate_vops(struct vnode *vp, int after)
{
    int ret = 0;
    struct vnodeopv_entry_desc *this;
    for (this = afs_vnodeop_entries; this->opve_op; this++) {
	if (vp->v_op[this->opve_op->vdesc_offset] != this->opve_impl) {
	    if (!ret) {
		printf("v_op %d ", after);
		vprint("check", vp);
	    }
	    ret = 1;
	    printf("For oper %d (%s), func is %p, not %p",
		   this->opve_op->vdesc_offset, this->opve_op->vdesc_name,
		   vp->v_op[this->opve_op->vdesc_offset], this->opve_impl);
	}
    }
    return ret;
}
#endif
int
afs_vop_open(ap)
     struct vop_open_args	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int error;
    struct vcache *vc = VTOAFS(ap->a_vp);

    AFS_GLOCK();
    error = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != ap->a_vp)
	panic("AFS open changed vnode!");
#endif
    AFS_GUNLOCK();
#ifdef AFS_FBSD60_ENV
    vnode_create_vobject(ap->a_vp, vc->m.Length, ap->a_td);
#endif
    osi_FlushPages(vc, ap->a_cred);
    return error;
}

int
afs_vop_close(ap)
     struct vop_close_args	/* {
				 * struct vnode *a_vp;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int code;
    struct vcache *avc = VTOAFS(ap->a_vp);
    AFS_GLOCK();
    if (ap->a_cred)
	code = afs_close(avc, ap->a_fflag, ap->a_cred);
    else
	code = afs_close(avc, ap->a_fflag, afs_osi_credp);
    osi_FlushPages(avc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    AFS_GUNLOCK();
    return code;
}

int
afs_vop_access(ap)
     struct vop_access_args	/* {
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
     struct vop_getattr_args	/* {
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
     struct vop_setattr_args	/* {
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
     struct vop_read_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * 
				 * } */ *ap;
{
    int code;
    struct vcache *avc = VTOAFS(ap->a_vp);
    AFS_GLOCK();
    osi_FlushPages(avc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    code = afs_read(avc, ap->a_uio, ap->a_cred, 0, 0, 0);
    AFS_GUNLOCK();
    return code;
}

/* struct vop_getpages_args {
 *	struct vnode *a_vp;
 *	vm_page_t *a_m;
 *	int a_count;
 *	int a_reqpage;
 *	vm_oofset_t a_offset;
 * };
 */
int
afs_vop_getpages(struct vop_getpages_args *ap)
{
    int code;
    int i, nextoff, size, toff, npages;
    struct uio uio;
    struct iovec iov;
    struct buf *bp;
    vm_offset_t kva;
    vm_object_t object;
    struct vnode *vp;
    struct vcache *avc;

#ifdef AFS_FBSD50_ENV
    GIANT_REQUIRED;
#endif
    vp = ap->a_vp;
    avc = VTOAFS(vp);
    if ((object = vp->v_object) == NULL) {
	printf("afs_getpages: called with non-merged cache vnode??\n");
	return VM_PAGER_ERROR;
    }
    npages = btoc(ap->a_count);
    /*
     * If the requested page is partially valid, just return it and
     * allow the pager to zero-out the blanks.  Partially valid pages
     * can only occur at the file EOF.
     */

    {
	vm_page_t m = ap->a_m[ap->a_reqpage];

#ifdef AFS_FBSD50_ENV
	VM_OBJECT_LOCK(object);
	vm_page_lock_queues();
#endif
	if (m->valid != 0) {
	    /* handled by vm_fault now        */
	    /* vm_page_zero_invalid(m, TRUE); */
	    for (i = 0; i < npages; ++i) {
		if (i != ap->a_reqpage)
		    vm_page_free(ap->a_m[i]);
	    }
#ifdef AFS_FBSD50_ENV
	    vm_page_unlock_queues();
	    VM_OBJECT_UNLOCK(object);
#endif
	    return (0);
	}
#ifdef AFS_FBSD50_ENV
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(object);
#endif
    }
    bp = getpbuf(&afs_pbuf_freecnt);

    kva = (vm_offset_t) bp->b_data;
    pmap_qenter(kva, ap->a_m, npages);
#ifdef AFS_FBSD50_ENV
    cnt.v_vnodein++;
    cnt.v_vnodepgsin += npages;
#endif

    iov.iov_base = (caddr_t) kva;
    iov.iov_len = ap->a_count;
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = IDX_TO_OFF(ap->a_m[0]->pindex);
    uio.uio_resid = ap->a_count;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_rw = UIO_READ;
#ifdef AFS_FBSD50_ENV
    uio.uio_td = curthread;
#else
    uio.uio_procp = curproc;
#endif

    AFS_GLOCK();
    osi_FlushPages(avc, osi_curcred());	/* hold bozon lock, but not basic vnode lock */
    code = afs_read(avc, &uio, osi_curcred(), 0, 0, 0);
    AFS_GUNLOCK();
    pmap_qremove(kva, npages);

    relpbuf(bp, &afs_pbuf_freecnt);

    if (code && (uio.uio_resid == ap->a_count)) {
#ifdef AFS_FBSD50_ENV
	VM_OBJECT_LOCK(object);
	vm_page_lock_queues();
#endif
	for (i = 0; i < npages; ++i) {
	    if (i != ap->a_reqpage)
		vm_page_free(ap->a_m[i]);
	}
#ifdef AFS_FBSD50_ENV
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(object);
#endif
	return VM_PAGER_ERROR;
    }

    size = ap->a_count - uio.uio_resid;
#ifdef AFS_FBSD50_ENV
    VM_OBJECT_LOCK(object);
    vm_page_lock_queues();
#endif
    for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
	vm_page_t m;
	nextoff = toff + PAGE_SIZE;
	m = ap->a_m[i];

	m->flags &= ~PG_ZERO;

	if (nextoff <= size) {
	    /*
	     * Read operation filled an entire page
	     */
	    m->valid = VM_PAGE_BITS_ALL;
	    vm_page_undirty(m);
	} else if (size > toff) {
	    /*
	     * Read operation filled a partial page.
	     */
	    m->valid = 0;
	    vm_page_set_validclean(m, 0, size - toff);
	    /* handled by vm_fault now        */
	    /* vm_page_zero_invalid(m, TRUE); */
	}

	if (i != ap->a_reqpage) {
	    /*
	     * Whether or not to leave the page activated is up in
	     * the air, but we should put the page on a page queue
	     * somewhere (it already is in the object).  Result:
	     * It appears that emperical results show that
	     * deactivating pages is best.
	     */

	    /*
	     * Just in case someone was asking for this page we
	     * now tell them that it is ok to use.
	     */
	    if (!code) {
		if (m->flags & PG_WANTED)
		    vm_page_activate(m);
		else
		    vm_page_deactivate(m);
		vm_page_wakeup(m);
	    } else {
		vm_page_free(m);
	    }
	}
    }
#ifdef AFS_FBSD50_ENV
    vm_page_unlock_queues();
    VM_OBJECT_UNLOCK(object);
#endif
    return 0;
}

int
afs_vop_write(ap)
     struct vop_write_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * } */ *ap;
{
    int code;
    struct vcache *avc = VTOAFS(ap->a_vp);
    AFS_GLOCK();
    osi_FlushPages(avc, ap->a_cred);	/* hold bozon lock, but not basic vnode lock */
    code =
	afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
    AFS_GUNLOCK();
    return code;
}

/*-
 * struct vop_putpages_args {
 *	struct vnode *a_vp;
 *	vm_page_t *a_m;
 *	int a_count;
 *	int a_sync;
 *	int *a_rtvals;
 *	vm_oofset_t a_offset;
 * };
 */
/*
 * All of the pages passed to us in ap->a_m[] are already marked as busy,
 * so there is no additional locking required to set their flags.  -GAW
 */
int
afs_vop_putpages(struct vop_putpages_args *ap)
{
    int code;
    int i, size, npages, sync;
    struct uio uio;
    struct iovec iov;
    struct buf *bp;
    vm_offset_t kva;
    struct vnode *vp;
    struct vcache *avc;

#ifdef AFS_FBSD50_ENV
    GIANT_REQUIRED;
#endif

    vp = ap->a_vp;
    avc = VTOAFS(vp);
    /* Perhaps these two checks should just be KASSERTs instead... */
    if (vp->v_object == NULL) {
	printf("afs_putpages: called with non-merged cache vnode??\n");
	return VM_PAGER_ERROR;	/* XXX I think this is insufficient */
    }
    if (vType(avc) != VREG) {
	printf("afs_putpages: not VREG");
	return VM_PAGER_ERROR;	/* XXX I think this is insufficient */
    }
    npages = btoc(ap->a_count);
    for (i = 0; i < npages; i++)
	ap->a_rtvals[i] = VM_PAGER_AGAIN;
    bp = getpbuf(&afs_pbuf_freecnt);

    kva = (vm_offset_t) bp->b_data;
    pmap_qenter(kva, ap->a_m, npages);
#ifdef AFS_FBSD50_ENV
    cnt.v_vnodeout++;
    cnt.v_vnodepgsout += ap->a_count;
#endif

    iov.iov_base = (caddr_t) kva;
    iov.iov_len = ap->a_count;
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_offset = IDX_TO_OFF(ap->a_m[0]->pindex);
    uio.uio_resid = ap->a_count;
    uio.uio_segflg = UIO_SYSSPACE;
    uio.uio_rw = UIO_WRITE;
#ifdef AFS_FBSD50_ENV
    uio.uio_td = curthread;
#else
    uio.uio_procp = curproc;
#endif
    sync = IO_VMIO;
    if (ap->a_sync & VM_PAGER_PUT_SYNC)
	sync |= IO_SYNC;
    /*if (ap->a_sync & VM_PAGER_PUT_INVAL)
     * sync |= IO_INVAL; */

    AFS_GLOCK();
    code = afs_write(avc, &uio, sync, osi_curcred(), 0);
    AFS_GUNLOCK();

    pmap_qremove(kva, npages);
    relpbuf(bp, &afs_pbuf_freecnt);

    if (!code) {
	size = ap->a_count - uio.uio_resid;
	for (i = 0; i < round_page(size) / PAGE_SIZE; i++) {
	    ap->a_rtvals[i] = VM_PAGER_OK;
	    vm_page_undirty(ap->a_m[i]);
	}
    }
    return ap->a_rtvals[0];
}

int
afs_vop_ioctl(ap)
     struct vop_ioctl_args	/* {
				 * struct vnode *a_vp;
				 * int  a_command;
				 * caddr_t  a_data;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap;
{
    struct vcache *tvc = VTOAFS(ap->a_vp);
    int error = 0;

    /* in case we ever get in here... */

    AFS_STATCNT(afs_ioctl);
    if (((ap->a_command >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
	AFS_GLOCK();
	error = HandleIoctl(tvc, NULL /*Not used */ ,
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
afs_vop_poll(ap)
     struct vop_poll_args	/* {
				 * struct vnode *a_vp;
				 * int  a_events;
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
     struct vop_mmap_args	/* {
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
     struct vop_fsync_args	/* {
				 * struct vnode *a_vp;
				 * struct ucred *a_cred;
				 * int a_waitfor;
				 * struct proc *a_p;
				 * } */ *ap;
{
    int error;
    register struct vnode *vp = ap->a_vp;

    AFS_GLOCK();
    /*vflushbuf(vp, wait); */
#ifdef AFS_FBSD60_ENV
    error = afs_fsync(VTOAFS(vp), ap->a_td->td_ucred);
#else
    if (ap->a_cred)
	error = afs_fsync(VTOAFS(vp), ap->a_cred);
    else
	error = afs_fsync(VTOAFS(vp), afs_osi_credp);
#endif
    AFS_GUNLOCK();
    return error;
}

int
afs_vop_remove(ap)
     struct vop_remove_args	/* {
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
    DROPNAME();
    return error;
}

int
afs_vop_link(ap)
     struct vop_link_args	/* {
				 * struct vnode *a_vp;
				 * struct vnode *a_tdvp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    register struct vnode *dvp = ap->a_tdvp;
    register struct vnode *vp = ap->a_vp;
#ifdef AFS_FBSD50_ENV
    struct thread *p = ap->a_cnp->cn_thread;
#else
    struct proc *p = ap->a_cnp->cn_proc;
#endif

    GETNAME();
    if (dvp->v_mount != vp->v_mount) {
	error = EXDEV;
	goto out;
    }
    if (vp->v_type == VDIR) {
	error = EISDIR;
	goto out;
    }
    if ((error = vn_lock(vp, LK_EXCLUSIVE, p)) != 0) {
	goto out;
    }
    AFS_GLOCK();
    error = afs_link(VTOAFS(vp), VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    if (dvp != vp)
	VOP_UNLOCK(vp, 0, p);
  out:
    DROPNAME();
    return error;
}

int
afs_vop_rename(ap)
     struct vop_rename_args	/* {
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
#ifdef AFS_FBSD50_ENV
    struct thread *p = fcnp->cn_thread;
#else
    struct proc *p = fcnp->cn_proc;
#endif

    /*
     * Check for cross-device rename.
     */
    if ((fvp->v_mount != tdvp->v_mount)
	|| (tvp && (fvp->v_mount != tvp->v_mount))) {
	error = EXDEV;
      abortit:
	if (tdvp == tvp)
	    vrele(tdvp);
	else
	    vput(tdvp);
	if (tvp)
	    vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	return (error);
    }
    /*
     * if fvp == tvp, we're just removing one name of a pair of
     * directory entries for the same element.  convert call into rename.
     ( (pinched from FreeBSD 4.4's ufs_rename())
     
     */
    if (fvp == tvp) {
	if (fvp->v_type == VDIR) {
	    error = EINVAL;
	    goto abortit;
	}

	/* Release destination completely. */
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
	error = relookup(fdvp, &fvp, fcnp);
	if (error == 0)
	    vrele(fdvp);
	if (fvp == NULL) {
	    return (ENOENT);
	}

	error = VOP_REMOVE(fdvp, fvp, fcnp);
	if (fdvp == fvp)
	    vrele(fdvp);
	else
	    vput(fdvp);
	vput(fvp);
	return (error);
    }
    if ((error = vn_lock(fvp, LK_EXCLUSIVE, p)) != 0)
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

    FREE(fname, M_TEMP);
    FREE(tname, M_TEMP);
    if (tdvp == tvp)
	vrele(tdvp);
    else
	vput(tdvp);
    if (tvp)
	vput(tvp);
    vrele(fdvp);
    vput(fvp);
    return error;
}

int
afs_vop_mkdir(ap)
     struct vop_mkdir_args	/* {
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
#ifdef AFS_FBSD50_ENV
    struct thread *p = ap->a_cnp->cn_thread;
#else
    struct proc *p = ap->a_cnp->cn_proc;
#endif

    GETNAME();
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_vop_mkdir: no name");
#endif
    AFS_GLOCK();
    error = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (error) {
	DROPNAME();
	return (error);
    }
    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY, p);
    } else
	*ap->a_vpp = 0;
    DROPNAME();
    return error;
}

int
afs_vop_rmdir(ap)
     struct vop_rmdir_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap;
{
    int error = 0;
    register struct vnode *dvp = ap->a_dvp;

    GETNAME();
    AFS_GLOCK();
    error = afs_rmdir(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    return error;
}

/* struct vop_symlink_args {
 *	struct vnode *a_dvp;
 *	struct vnode **a_vpp;
 *	struct componentname *a_cnp;
 *	struct vattr *a_vap;
 *	char *a_target;
 * };
 */
int
afs_vop_symlink(struct vop_symlink_args *ap)
{
    struct vnode *dvp;
    struct vnode *newvp;
    struct vcache *vcp;
    int error;

    GETNAME();
    AFS_GLOCK();

    dvp = ap->a_dvp;
    newvp = NULL;

    error =
	afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target, cnp->cn_cred);
    if (error == 0) {
	error = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
	if (error == 0) {
	    newvp = AFSTOV(vcp);
#ifdef AFS_FBSD50_ENV
	    vn_lock(newvp, LK_EXCLUSIVE | LK_RETRY, cnp->cn_thread);
#else
	    vn_lock(newvp, LK_EXCLUSIVE | LK_RETRY, cnp->cn_proc);
#endif
	}
    }
    AFS_GUNLOCK();
    DROPNAME();
    *(ap->a_vpp) = newvp;
    return error;
}

int
afs_vop_readdir(ap)
     struct vop_readdir_args	/* {
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
     struct vop_readlink_args	/* {
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
     struct vop_inactive_args	/* {
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
    VOP_UNLOCK(vp, 0, ap->a_p);
    return 0;
}

/*
 * struct vop_reclaim_args {
 *	struct vnode *a_vp;
 * };
 */
int
afs_vop_reclaim(struct vop_reclaim_args *ap)
{
    /* copied from ../OBSD/osi_vnodeops.c:afs_nbsd_reclaim() */
    int code, slept;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);
    int haveGlock = ISAFS_GLOCK();
    int haveVlock = CheckLock(&afs_xvcache);

    if (!haveGlock)
	AFS_GLOCK();
    if (!haveVlock)
	ObtainWriteLock(&afs_xvcache, 901);
#ifndef AFS_DISCON_ENV
    code = afs_FlushVCache(avc, &slept);	/* tosses our stuff from vnode */
#else
    /* reclaim the vnode and the in-memory vcache, but keep the on-disk vcache */
    code = afs_FlushVS(avc);
#endif
    if (!haveVlock)
	ReleaseWriteLock(&afs_xvcache);
    if (!haveGlock)
	AFS_GUNLOCK();

    /*
     * XXX Pretend it worked, to prevent panic on shutdown
     * Garrett, please fix - Jim Rees
     */
    if (code)
	printf("afs_vop_reclaim: afs_FlushVCache failed code %d\n", code);
#ifdef AFS_FBSD60_ENV
    else
	vnode_destroy_vobject(vp);
#endif
    return 0;
}

#ifndef AFS_FBSD60_ENV
int
afs_vop_bmap(ap)
     struct vop_bmap_args	/* {
				 * struct vnode *a_vp;
				 * daddr_t  a_bn;
				 * struct vnode **a_vpp;
				 * daddr_t *a_bnp;
				 * int *a_runp;
				 * int *a_runb;
				 * } */ *ap;
{
    if (ap->a_bnp) {
	*ap->a_bnp = ap->a_bn * (PAGE_SIZE / DEV_BSIZE);
    }
    if (ap->a_vpp) {
	*ap->a_vpp = ap->a_vp;
    }
    if (ap->a_runp != NULL)
	*ap->a_runp = 0;
    if (ap->a_runb != NULL)
	*ap->a_runb = 0;

    return 0;
}
#endif

int
afs_vop_strategy(ap)
     struct vop_strategy_args	/* {
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
     struct vop_print_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap;
{
    register struct vnode *vp = ap->a_vp;
    register struct vcache *vc = VTOAFS(ap->a_vp);
    int s = vc->states;

#ifdef AFS_FBSD50_ENV
    printf("tag %s, fid: %d.%x.%x.%x, opens %d, writers %d", vp->v_tag,
	   (int)vc->fid.Cell, (u_int) vc->fid.Fid.Volume,
	   (u_int) vc->fid.Fid.Vnode, (u_int) vc->fid.Fid.Unique, vc->opens,
	   vc->execsOrWriters);
#else
    printf("tag %d, fid: %ld.%x.%x.%x, opens %d, writers %d", vp->v_tag,
	   vc->fid.Cell, (u_int) vc->fid.Fid.Volume,
	   (u_int) vc->fid.Fid.Vnode, (u_int) vc->fid.Fid.Unique, vc->opens,
	   vc->execsOrWriters);
#endif
    printf("\n  states%s%s%s%s%s", (s & CStatd) ? " statd" : "",
	   (s & CRO) ? " readonly" : "", (s & CDirty) ? " dirty" : "",
	   (s & CMAPPED) ? " mapped" : "",
	   (s & CVFlushed) ? " flush in progress" : "");
    printf("\n");
    return 0;
}

/*
 * Advisory record locking support (fcntl() POSIX style)
 */
int
afs_vop_advlock(ap)
     struct vop_advlock_args	/* {
				 * struct vnode *a_vp;
				 * caddr_t  a_id;
				 * int  a_op;
				 * struct flock *a_fl;
				 * int  a_flags;
				 * } */ *ap;
{
    int error;
    struct ucred cr = *osi_curcred();

    AFS_GLOCK();
    error =
	afs_lockctl(VTOAFS(ap->a_vp), ap->a_fl, ap->a_op, &cr, (int)ap->a_id);
    AFS_GUNLOCK();
    return error;
}
