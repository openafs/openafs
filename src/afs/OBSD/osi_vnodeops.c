/*
Copyright 1995 Massachusetts Institute of Technology.  All Rights
Reserved.

You are hereby granted a worldwide, irrevocable, paid-up, right and
license to use, execute, display, modify, copy and distribute MIT's
Modifications, provided that (i) you abide by the terms and conditions
of your Transarc AFS License Agreement, and (ii) you do not use the name
of MIT in any advertising or publicity without the prior written consent
of MIT.  MIT disclaims all liability for your use of MIT's
Modifications.  MIT's Modifications are provided "AS IS" WITHOUT
WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO,
ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
NONINFRINGEMENT.
*/

/*
 * OpenBSD specific vnodeops + other misc interface glue
 * Original NetBSD version for Transarc afs by John Kohl <jtk@MIT.EDU>
 * OpenBSD version by Jim Rees <rees@umich.edu>
 *
 * $Id$
 */

/*
 * A bunch of code cribbed from NetBSD ufs_vnops.c, ffs_vnops.c, and
 * nfs_vnops.c which carry this copyright:
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h" /* statistics */

#include <sys/malloc.h>
#include <sys/namei.h>

#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

#ifdef AFS_DISCON_ENV
extern int afs_FlushVS(struct vcache *tvc);
#endif

#define M_AFSNODE (M_TEMP-1)		/* XXX */

int afs_nbsd_lookup(struct vop_lookup_args *);
int afs_nbsd_create(struct vop_create_args *);
int afs_nbsd_mknod(struct vop_mknod_args *);
int afs_nbsd_open(struct vop_open_args *);
int afs_nbsd_close(struct vop_close_args *);
int afs_nbsd_access(struct vop_access_args *);
int afs_nbsd_getattr(struct vop_getattr_args *);
int afs_nbsd_setattr(struct vop_setattr_args *);
int afs_nbsd_read(struct vop_read_args *);
int afs_nbsd_write(struct vop_write_args *);
int afs_nbsd_ioctl(struct vop_ioctl_args *);
int afs_nbsd_select(struct vop_select_args *);
int afs_nbsd_fsync(struct vop_fsync_args *);
int afs_nbsd_remove(struct vop_remove_args *);
int afs_nbsd_link(struct vop_link_args *);
int afs_nbsd_rename(struct vop_rename_args *);
int afs_nbsd_mkdir(struct vop_mkdir_args *);
int afs_nbsd_rmdir(struct vop_rmdir_args *);
int afs_nbsd_symlink(struct vop_symlink_args *);
int afs_nbsd_readdir(struct vop_readdir_args *);
int afs_nbsd_readlink(struct vop_readlink_args *);
extern int ufs_abortop(struct vop_abortop_args *);
int afs_nbsd_inactive(struct vop_inactive_args *);
int afs_nbsd_reclaim(struct vop_reclaim_args *);
int afs_nbsd_lock(struct vop_lock_args *);
int afs_nbsd_unlock(struct vop_unlock_args *);
int afs_nbsd_bmap(struct vop_bmap_args *);
int afs_nbsd_strategy(struct vop_strategy_args *);
int afs_nbsd_print(struct vop_print_args *);
int afs_nbsd_islocked(struct vop_islocked_args *);
int afs_nbsd_pathconf(struct vop_pathconf_args *);
int afs_nbsd_advlock(struct vop_advlock_args *);

#define afs_nbsd_opnotsupp \
	((int (*) __P((struct  vop_reallocblks_args *)))eopnotsupp)
#define afs_nbsd_reallocblks afs_nbsd_opnotsupp

/* Global vfs data structures for AFS. */
int (**afs_vnodeop_p) __P((void *));
struct vnodeopv_entry_desc afs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, afs_nbsd_lookup },		/* lookup */
	{ &vop_create_desc, afs_nbsd_create },		/* create */
	{ &vop_mknod_desc, afs_nbsd_mknod },		/* mknod */
	{ &vop_open_desc, afs_nbsd_open },		/* open */
	{ &vop_close_desc, afs_nbsd_close },		/* close */
	{ &vop_access_desc, afs_nbsd_access },		/* access */
	{ &vop_getattr_desc, afs_nbsd_getattr },	/* getattr */
	{ &vop_setattr_desc, afs_nbsd_setattr },	/* setattr */
	{ &vop_read_desc, afs_nbsd_read },		/* read */
	{ &vop_write_desc, afs_nbsd_write },		/* write */
	{ &vop_ioctl_desc, afs_nbsd_ioctl }, /* XXX ioctl */
	{ &vop_select_desc, afs_nbsd_select },		/* select */
	{ &vop_fsync_desc, afs_nbsd_fsync },		/* fsync */
	{ &vop_remove_desc, afs_nbsd_remove },		/* remove */
	{ &vop_link_desc, afs_nbsd_link },		/* link */
	{ &vop_rename_desc, afs_nbsd_rename },		/* rename */
	{ &vop_mkdir_desc, afs_nbsd_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, afs_nbsd_rmdir },		/* rmdir */
	{ &vop_symlink_desc, afs_nbsd_symlink },	/* symlink */
	{ &vop_readdir_desc, afs_nbsd_readdir },	/* readdir */
	{ &vop_readlink_desc, afs_nbsd_readlink },	/* readlink */
	{ &vop_abortop_desc, vop_generic_abortop },	/* abortop */
	{ &vop_inactive_desc, afs_nbsd_inactive },	/* inactive */
	{ &vop_reclaim_desc, afs_nbsd_reclaim },	/* reclaim */
	{ &vop_lock_desc, afs_nbsd_lock },		/* lock */
	{ &vop_unlock_desc, afs_nbsd_unlock },		/* unlock */
	{ &vop_bmap_desc, afs_nbsd_bmap },		/* bmap */
	{ &vop_strategy_desc, afs_nbsd_strategy },	/* strategy */
	{ &vop_print_desc, afs_nbsd_print },		/* print */
	{ &vop_islocked_desc, afs_nbsd_islocked },	/* islocked */
	{ &vop_pathconf_desc, afs_nbsd_pathconf },	/* pathconf */
	{ &vop_advlock_desc, afs_nbsd_advlock },	/* advlock */
	{ &vop_reallocblks_desc, afs_nbsd_reallocblks }, /* reallocblks */
	{ &vop_bwrite_desc, vop_generic_bwrite },
	{ (struct vnodeop_desc *) NULL, (int (*) __P((void *))) NULL}
};
struct vnodeopv_desc afs_vnodeop_opv_desc =
	{ &afs_vnodeop_p, afs_vnodeop_entries };

#define GETNAME()	\
    struct componentname *cnp = ap->a_cnp; \
    char *name; \
    MALLOC(name, char *, cnp->cn_namelen+1, M_TEMP, M_WAITOK); \
    bcopy(cnp->cn_nameptr, name, cnp->cn_namelen); \
    name[cnp->cn_namelen] = '\0'

#define DROPNAME() FREE(name, M_TEMP)

int afs_debug;

#define NBSD_WRITES_ALLOWED
#ifndef NBSD_WRITES_ALLOWED
int nbsd_writes_allowed = 0;
#endif

#undef vrele
#define vrele afs_nbsd_rele
#undef VREF
#define VREF afs_nbsd_ref

int
afs_nbsd_lookup(ap)
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
    int flags = ap->a_cnp->cn_flags;
    int lockparent;			/* 1 => lockparent flag is set */
    int wantparent;			/* 1 => wantparent or lockparent flag */

    GETNAME();
    lockparent = flags & LOCKPARENT;
    wantparent = flags & (LOCKPARENT|WANTPARENT);

    if (ap->a_dvp->v_type != VDIR) {
	*ap->a_vpp = 0;
	DROPNAME();
	return ENOTDIR;
    }
    dvp = ap->a_dvp;
    if (afs_debug & AFSDEB_VNLAYER && !(dvp->v_flag & VROOT))
	printf("nbsd_lookup dvp %p flags %x name %s cnt %d\n", dvp, flags, name, dvp->v_usecount);
    error = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
    if (error) {
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
	    (flags & ISLASTCN) && error == ENOENT)
	    error = EJUSTRETURN;
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	    cnp->cn_flags |= SAVENAME;
	DROPNAME();
	*ap->a_vpp = 0;
	return (error);
    }
    vp = AFSTOV(vcp);			/* always get a node if no error */

    /* The parent directory comes in locked.  We unlock it on return
       unless the caller wants it left locked.
       we also always return the vnode locked. */

    if (vp == dvp) {
	/* they're the same; afs_lookup() already ref'ed the leaf.
	   It came in locked, so we don't need to ref OR lock it */
	if (afs_debug & AFSDEB_VNLAYER)
	    printf("ref'ed %p as .\n", dvp);
    } else {
	if (!lockparent || !(flags & ISLASTCN))
	    VOP_UNLOCK(dvp, 0, curproc);		/* done with parent. */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curproc);			/* always return the child locked */
	if (afs_debug & AFSDEB_VNLAYER)
	    printf("locked ret %p from lookup\n", vp);
    }
    *ap->a_vpp = vp;

    if (((cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN))
	 || (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))))
	cnp->cn_flags |= SAVENAME;

    DROPNAME();
    if (afs_debug & AFSDEB_VNLAYER && !(dvp->v_flag & VROOT))
	printf("nbsd_lookup done dvp %p cnt %d\n", dvp, dvp->v_usecount);
    return error;
}

int
afs_nbsd_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
    int error = 0;
    struct vcache *vcp;
    struct vnode *dvp = ap->a_dvp;
    GETNAME();

    if (afs_debug & AFSDEB_VNLAYER)
	printf("nbsd_create dvp %p cnt %d\n", dvp, dvp->v_usecount);

    /* vnode layer handles excl/nonexcl */

#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    error = afs_create(VTOAFS(dvp), name, ap->a_vap, NONEXCL,
		       ap->a_vap->va_mode, &vcp,
		       cnp->cn_cred);
    if (error) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return(error);
    }

    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY, curproc);
    }
    else *ap->a_vpp = 0;

    if ((cnp->cn_flags & SAVESTART) == 0)
	FREE(cnp->cn_pnbuf, M_NAMEI);
    vput(dvp);
    DROPNAME();
    if (afs_debug & AFSDEB_VNLAYER)
	printf("nbsd_create done dvp %p cnt %d\n", dvp, dvp->v_usecount);
    return error;
}

int
afs_nbsd_mknod(ap)
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
    free(ap->a_cnp->cn_pnbuf, M_NAMEI);
    vput(ap->a_dvp);
    return(ENODEV);
}

int
afs_nbsd_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
    int error;
    struct vcache *vc = VTOAFS(ap->a_vp);
    error = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != ap->a_vp)
	panic("AFS open changed vnode!");
#endif
    return error;
}

int
afs_nbsd_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
    return afs_close(VTOAFS(ap->a_vp), ap->a_fflag, ap->a_cred, ap->a_p);
}

int
afs_nbsd_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
    return afs_access(VTOAFS(ap->a_vp), ap->a_mode, ap->a_cred);
}
int
afs_nbsd_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
    return afs_getattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
}
int
afs_nbsd_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
    return afs_setattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
}
int
afs_nbsd_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
    return afs_read(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, 0, 0, 0);
}
int
afs_nbsd_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
#ifdef UVM
    (void) uvm_vnp_uncache(ap->a_vp);	/* toss stale pages */
#else
    vnode_pager_uncache(ap->a_vp);
#endif
    return afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
}
int
afs_nbsd_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
    struct vcache *tvc = VTOAFS(ap->a_vp);
    int error = 0;

    /* in case we ever get in here... */

    AFS_STATCNT(afs_ioctl);
    if (((ap->a_command >> 8) & 0xff) == 'V') {
	/* This is a VICEIOCTL call */
	error = HandleIoctl(tvc, (struct file *)0/*Not used*/,
			    ap->a_command, ap->a_data);
	return(error);
    } else {
	/* No-op call; just return. */
	return(ENOTTY);
    }
}

/* ARGSUSED */
int
afs_nbsd_select(ap)
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

int
afs_nbsd_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
    int wait = ap->a_waitfor == MNT_WAIT;
    struct vnode *vp = ap->a_vp;
    vflushbuf(vp, wait);
    return afs_fsync(VTOAFS(vp), ap->a_cred);
}

int
afs_nbsd_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
    int error = 0;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    error =  afs_remove(VTOAFS(dvp), name, cnp->cn_cred);
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
    FREE(cnp->cn_pnbuf, M_NAMEI);
    DROPNAME();
    return error;
}

int
afs_nbsd_link(ap)
	struct vop_link_args /* {
		struct vnode *a_vp;
		struct vnode *a_tdvp;
		struct componentname *a_cnp;
	} */ *ap;
{
    int error = 0;
    struct vnode *dvp = ap->a_dvp;
    struct vnode *vp = ap->a_vp;

    GETNAME();
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
    if ((error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curproc))) {
	VOP_ABORTOP(dvp, cnp);
	goto out;
    }
#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    error = afs_link(VTOAFS(vp), VTOAFS(dvp), name, cnp->cn_cred);
    FREE(cnp->cn_pnbuf, M_NAMEI);
    if (dvp != vp)
	VOP_UNLOCK(vp, 0, curproc);
out:
    vput(dvp);
    DROPNAME();
    return error;
}

int
afs_nbsd_rename(ap)
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
    struct vnode *tdvp = ap->a_tdvp;
    struct vnode *fvp = ap->a_fvp;
    struct vnode *fdvp = ap->a_fdvp;

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

    if ((error = vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY, curproc)))
	goto abortit;

    MALLOC(fname, char *, fcnp->cn_namelen+1, M_TEMP, M_WAITOK);
    bcopy(fcnp->cn_nameptr, fname, fcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    MALLOC(tname, char *, tcnp->cn_namelen+1, M_TEMP, M_WAITOK);
    bcopy(tcnp->cn_nameptr, tname, tcnp->cn_namelen);
    tname[tcnp->cn_namelen] = '\0';


#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    error = afs_rename(VTOAFS(fdvp), fname, VTOAFS(tdvp), tname, tcnp->cn_cred);

    VOP_UNLOCK(fvp, 0, curproc);
    FREE(fname, M_TEMP);
    FREE(tname, M_TEMP);
    if (error)
	goto abortit;			/* XXX */
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
afs_nbsd_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
    struct vnode *dvp = ap->a_dvp;
    struct vattr *vap = ap->a_vap;
    int error = 0;
    struct vcache *vcp;

    GETNAME();
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_nbsd_mkdir: no name");
#endif
#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    error = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, cnp->cn_cred);
    if (error) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return(error);
    }
    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY, curproc);
    } else
	*ap->a_vpp = 0;
    DROPNAME();
    FREE(cnp->cn_pnbuf, M_NAMEI);
    vput(dvp);
    return error;
}

int
afs_nbsd_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
    int error = 0;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
    if (dvp == vp) {
	vrele(dvp);
	vput(vp);
	FREE(cnp->cn_pnbuf, M_NAMEI);
	DROPNAME();
	return (EINVAL);
    }

#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    error = afs_rmdir(VTOAFS(dvp), name, cnp->cn_cred);
    DROPNAME();
    vput(dvp);
    vput(vp);
    return error;
}

int
afs_nbsd_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
    struct vnode *dvp = ap->a_dvp;
    int error = 0;
    /* NFS ignores a_vpp; so do we. */

    GETNAME();
#ifndef NBSD_WRITES_ALLOWED
    if (!nbsd_writes_allowed)
	error = EROFS;
    if (!error)
#endif
    error = afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target,
			cnp->cn_cred);
    DROPNAME();
    FREE(cnp->cn_pnbuf, M_NAMEI);
    vput(dvp);
    return error;
}

int
afs_nbsd_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
/*    printf("readdir %p cookies %p ncookies %d\n", ap->a_vp, ap->a_cookies,
	   ap->a_ncookies); */
    return afs_readdir(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred,
		       ap->a_eofflag, ap->a_ncookies, ap->a_cookies);
}

int
afs_nbsd_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
/*    printf("readlink %p\n", ap->a_vp);*/
    return afs_readlink(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred);
}

extern int prtactive;

int
afs_nbsd_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    AFS_STATCNT(afs_inactive);

    if (prtactive && vp->v_usecount != 0)
	vprint("afs_nbsd_inactive(): pushing active", vp);

    vc->states &= ~CMAPPED;
    vc->states &= ~CDirty;

    lockinit(&vc->rwlock, PINOD, "vcache", 0, 0);
    return 0;
}

int
afs_nbsd_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
    int error, slept;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);

    cache_purge(vp);			/* just in case... */
#ifdef UVM
    uvm_vnp_uncache(vp);
#else
    vnode_pager_uncache(vp);
#endif

#ifndef AFS_DISCON_ENV
    error = afs_FlushVCache(avc, &slept); /* tosses our stuff from vnode */
#else
    /* reclaim the vnode and the in-memory vcache, but keep the on-disk vcache */
    error = afs_FlushVS(avc);
#endif
    if (!error && vp->v_data)
	panic("afs_reclaim: vnode not cleaned");
    return error;
}

int
afs_nbsd_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		sturct proc *a_p;
	} */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

#ifdef DIAGNOSTIC
    if (!vc)
	panic("afs_nbsd_lock: null vcache");
#endif
    return lockmgr(&vc->rwlock, ap->a_flags | LK_CANRECURSE, &vp->v_interlock, ap->a_p);
}

int
afs_nbsd_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

#ifdef DIAGNOSTIC
    if (!vc)
	panic("afs_nbsd_unlock: null vcache");
#endif
    return lockmgr(&vc->rwlock, ap->a_flags | LK_RELEASE, &vp->v_interlock, ap->a_p);
}

int
afs_nbsd_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{
    struct vcache *vcp = VTOAFS(ap->a_vp);

    AFS_STATCNT(afs_bmap);
    if (ap->a_bnp)
	ap->a_bnp = (daddr_t *) (ap->a_bn * (8192 / DEV_BSIZE));
    if (ap->a_vpp)
	*ap->a_vpp = (vcp) ? AFSTOV(vcp) : NULL;
    return 0;
}

int
afs_nbsd_strategy(ap)
    struct vop_strategy_args /* {
	struct buf *a_bp;
    } */ *ap;
{
    struct buf *abp = ap->a_bp;
    struct uio tuio;
    struct iovec tiovec[1];
    struct vcache *tvc = VTOAFS(abp->b_vp);
    struct ucred *credp = osi_curcred();
    long len = abp->b_bcount;
    int code;

    AFS_STATCNT(afs_strategy);

    tuio.afsio_iov = tiovec;
    tuio.afsio_iovcnt = 1;
    tuio.afsio_seg = AFS_UIOSYS;
    tuio.afsio_resid = len;
    tiovec[0].iov_base = abp->b_un.b_addr;
    tiovec[0].iov_len = len;

    if ((abp->b_flags & B_READ) == B_READ) {
	code = afs_rdwr(tvc, &tuio, UIO_READ, 0, credp);
	if (code == 0 && tuio.afsio_resid > 0)
	    bzero(abp->b_un.b_addr + len - tuio.afsio_resid, tuio.afsio_resid);
    } else
	code = afs_rdwr(tvc, &tuio, UIO_WRITE, 0, credp);

    ReleaseWriteLock(&tvc->lock);
    AFS_RELE(AFSTOV(tvc));
    return code;
}

int
afs_nbsd_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(ap->a_vp);

    printf("tag %d, fid: %ld.%x.%x.%x, ", vp->v_tag, vc->fid.Cell,
	   (int) vc->fid.Fid.Volume, (int) vc->fid.Fid.Vnode, (int) vc->fid.Fid.Unique);
    lockmgr_printinfo(&vc->rwlock);
    printf("\n");
    return 0;
}

int
afs_nbsd_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
    return lockstatus(&VTOAFS(ap->a_vp)->rwlock);
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
afs_nbsd_pathconf(ap)
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

extern int
afs_lockctl(struct vcache *avc, struct AFS_FLOCK *af, int acmd, struct AFS_UCRED *acred, pid_t clid);

/*
 * Advisory record locking support (fcntl() POSIX style)
 */
int
afs_nbsd_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{
    return afs_lockctl(VTOAFS(ap->a_vp), ap->a_fl, ap->a_op, osi_curcred(),
		       (int) ap->a_id);
}
