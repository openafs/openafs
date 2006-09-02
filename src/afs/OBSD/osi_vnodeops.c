/*
 * OpenBSD specific vnodeops + other misc interface glue
 * Original NetBSD version for Transarc afs by John Kohl <jtk@MIT.EDU>
 * OpenBSD version by Jim Rees <rees@umich.edu>
 *
 * $Id: osi_vnodeops.c,v 1.18.2.2 2006/06/23 14:21:12 rees Exp $
 */

/*
copyright 2002
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/

/*
Copyright 1995 Massachusetts Institute of Technology.  All Rights
Reserved.

You are hereby granted a worldwide, irrevocable, paid-up, right and
license to use, execute, display, modify, copy and distribute MIT's
Modifications, provided that (i) you abide by the terms and conditions
of your OpenAFS License Agreement, and (ii) you do not use the name
of MIT in any advertising or publicity without the prior written consent
of MIT.  MIT disclaims all liability for your use of MIT's
Modifications.  MIT's Modifications are provided "AS IS" WITHOUT
WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO,
ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
NONINFRINGEMENT.
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

RCSID
    ("$Header: /cvs/openafs/src/afs/OBSD/osi_vnodeops.c,v 1.18.2.2 2006/06/23 14:21:12 rees Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */

#include <sys/malloc.h>
#include <sys/namei.h>
#ifdef AFS_OBSD36_ENV
#include <sys/pool.h>
#endif

#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

#ifdef AFS_DISCON_ENV
extern int afs_FlushVS(struct vcache *tvc);
#endif

#define M_AFSNODE (M_TEMP-1)	/* XXX */

int afs_nbsd_lookup(void *);
int afs_nbsd_create(void *);
int afs_nbsd_mknod(void *);
int afs_nbsd_open(void *);
int afs_nbsd_close(void *);
int afs_nbsd_access(void *);
int afs_nbsd_getattr(void *);
int afs_nbsd_setattr(void *);
int afs_nbsd_read(void *);
int afs_nbsd_write(void *);
int afs_nbsd_ioctl(void *);
int afs_nbsd_select(void *);
int afs_nbsd_fsync(void *);
int afs_nbsd_remove(void *);
int afs_nbsd_link(void *);
int afs_nbsd_rename(void *);
int afs_nbsd_mkdir(void *);
int afs_nbsd_rmdir(void *);
int afs_nbsd_symlink(void *);
int afs_nbsd_readdir(void *);
int afs_nbsd_readlink(void *);
int afs_nbsd_inactive(void *);
int afs_nbsd_reclaim(void *);
int afs_nbsd_lock(void *);
int afs_nbsd_unlock(void *);
int afs_nbsd_bmap(void *);
int afs_nbsd_strategy(void *);
int afs_nbsd_print(void *);
int afs_nbsd_islocked(void *);
int afs_nbsd_pathconf(void *);
int afs_nbsd_advlock(void *);

#define afs_nbsd_opnotsupp \
	((int (*) __P((void *)))eopnotsupp)
#define afs_nbsd_reallocblks afs_nbsd_opnotsupp

/* Global vfs data structures for AFS. */
int (**afs_vnodeop_p) __P((void *));
struct vnodeopv_entry_desc afs_vnodeop_entries[] = {
    {&vop_default_desc, vn_default_error},
    {&vop_lookup_desc, afs_nbsd_lookup},	/* lookup */
    {&vop_create_desc, afs_nbsd_create},	/* create */
    {&vop_mknod_desc, afs_nbsd_mknod},		/* mknod */
    {&vop_open_desc, afs_nbsd_open},		/* open */
    {&vop_close_desc, afs_nbsd_close},		/* close */
    {&vop_access_desc, afs_nbsd_access},	/* access */
    {&vop_getattr_desc, afs_nbsd_getattr},	/* getattr */
    {&vop_setattr_desc, afs_nbsd_setattr},	/* setattr */
    {&vop_read_desc, afs_nbsd_read},		/* read */
    {&vop_write_desc, afs_nbsd_write},		/* write */
    {&vop_ioctl_desc, afs_nbsd_ioctl},		/* XXX ioctl */
#ifdef AFS_OBSD35_ENV
    {&vop_poll_desc, afs_nbsd_select},		/* select */
#else
    {&vop_select_desc, afs_nbsd_select},	/* select */
#endif
    {&vop_fsync_desc, afs_nbsd_fsync},		/* fsync */
    {&vop_remove_desc, afs_nbsd_remove},	/* remove */
    {&vop_link_desc, afs_nbsd_link},		/* link */
    {&vop_rename_desc, afs_nbsd_rename},	/* rename */
    {&vop_mkdir_desc, afs_nbsd_mkdir},		/* mkdir */
    {&vop_rmdir_desc, afs_nbsd_rmdir},		/* rmdir */
    {&vop_symlink_desc, afs_nbsd_symlink},	/* symlink */
    {&vop_readdir_desc, afs_nbsd_readdir},	/* readdir */
    {&vop_readlink_desc, afs_nbsd_readlink},	/* readlink */
    {&vop_abortop_desc, vop_generic_abortop},	/* abortop */
    {&vop_inactive_desc, afs_nbsd_inactive},	/* inactive */
    {&vop_reclaim_desc, afs_nbsd_reclaim},	/* reclaim */
    {&vop_lock_desc, afs_nbsd_lock},		/* lock */
    {&vop_unlock_desc, afs_nbsd_unlock},	/* unlock */
    {&vop_bmap_desc, afs_nbsd_bmap},		/* bmap */
    {&vop_strategy_desc, afs_nbsd_strategy},	/* strategy */
    {&vop_print_desc, afs_nbsd_print},		/* print */
    {&vop_islocked_desc, afs_nbsd_islocked},	/* islocked */
    {&vop_pathconf_desc, afs_nbsd_pathconf},	/* pathconf */
    {&vop_advlock_desc, afs_nbsd_advlock},	/* advlock */
    {&vop_reallocblks_desc, afs_nbsd_reallocblks},	/* reallocblks */
    {&vop_bwrite_desc, vop_generic_bwrite},
    {(struct vnodeop_desc *)NULL, (int (*)__P((void *)))NULL}
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

#ifdef AFS_OBSD36_ENV
#define DROPCNP(cnp) pool_put(&namei_pool, (cnp)->cn_pnbuf)
#else
#define DROPCNP(cnp) FREE((cnp)->cn_pnbuf, M_NAMEI)
#endif

int afs_debug;

int
afs_nbsd_lookup(void *v)
{
    struct vop_lookup_args	/* {
				 * struct vnodeop_desc * a_desc;
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * } */ *ap = v;
    int code;
    struct vcache *vcp;
    struct vnode *vp, *dvp;
    int flags = ap->a_cnp->cn_flags;
    int lockparent;		/* 1 => lockparent flag is set */
    int wantparent;		/* 1 => wantparent or lockparent flag */

    GETNAME();
    lockparent = flags & LOCKPARENT;
    wantparent = flags & (LOCKPARENT | WANTPARENT);
#ifdef PDIRUNLOCK
    cnp->cn_flags &= ~PDIRUNLOCK;
#endif

    if (ap->a_dvp->v_type != VDIR) {
	*ap->a_vpp = NULL;
	DROPNAME();
	return ENOTDIR;
    }
    dvp = ap->a_dvp;
    if (afs_debug & AFSDEB_VNLAYER && !(dvp->v_flag & VROOT))
	printf("nbsd_lookup dvp %p flags %x name %s cnt %d\n", dvp, flags,
	       name, dvp->v_usecount);
    AFS_GLOCK();
    code = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (code) {
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
	    && (flags & ISLASTCN) && code == ENOENT)
	    code = EJUSTRETURN;
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
	    cnp->cn_flags |= SAVENAME;
	DROPNAME();
	*ap->a_vpp = NULL;
	return (code);
    }
    vp = AFSTOV(vcp);		/* always get a node if no error */

    /*
     * The parent directory comes in locked.  We unlock it on return
     * unless the caller wants it left locked.
     * we also always return the vnode locked.
     */

    if (vp == dvp) {
	/* they're the same; afs_lookup() already ref'ed the leaf.
	 * It came in locked, so we don't need to ref OR lock it */
	if (afs_debug & AFSDEB_VNLAYER)
	    printf("ref'ed %p as .\n", dvp);
    } else {
	if (!lockparent || !(flags & ISLASTCN)) {
	    VOP_UNLOCK(dvp, 0, curproc);	/* done with parent. */
#ifdef PDIRUNLOCK
	    cnp->cn_flags |= PDIRUNLOCK;
#endif
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curproc);	/* always return the child locked */
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
    return code;
}

int
afs_nbsd_create(void *v)
{
    struct vop_create_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap = v;
    int code = 0;
    struct vcache *vcp;
    struct vnode *dvp = ap->a_dvp;
    GETNAME();

    if (afs_debug & AFSDEB_VNLAYER)
	printf("nbsd_create dvp %p cnt %d\n", dvp, dvp->v_usecount);

    /* vnode layer handles excl/nonexcl */

    AFS_GLOCK();
    code =
	afs_create(VTOAFS(dvp), name, ap->a_vap, NONEXCL, ap->a_vap->va_mode,
		   &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (code) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return (code);
    }

    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY, curproc);
    } else
	*ap->a_vpp = 0;

    if ((cnp->cn_flags & SAVESTART) == 0)
	DROPCNP(cnp);
    vput(dvp);
    DROPNAME();
    if (afs_debug & AFSDEB_VNLAYER)
	printf("nbsd_create done dvp %p cnt %d\n", dvp, dvp->v_usecount);
    return code;
}

int
afs_nbsd_mknod(void *v)
{
    struct vop_mknod_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap = v;
    DROPCNP(ap->a_cnp);
    vput(ap->a_dvp);
    return (ENODEV);
}

int
afs_nbsd_open(void *v)
{
    struct vop_open_args	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int code;
    struct vcache *vc = VTOAFS(ap->a_vp);

    AFS_GLOCK();
    code = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != ap->a_vp)
	panic("AFS open changed vnode!");
#endif
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_close(void *v)
{
    struct vop_close_args	/* {
				 * struct vnode *a_vp;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code = afs_close(VTOAFS(ap->a_vp), ap->a_fflag, ap->a_cred, ap->a_p);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_access(void *v)
{
    struct vop_access_args	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code = afs_access(VTOAFS(ap->a_vp), ap->a_mode, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_getattr(void *v)
{
    struct vop_getattr_args	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code = afs_getattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_setattr(void *v)
{
    struct vop_setattr_args	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code = afs_setattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_read(void *v)
{
    struct vop_read_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code =
	afs_read(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, (daddr_t) 0, NULL,
		 0);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_write(void *v)
{
    struct vop_write_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * struct ucred *a_cred;
				 * } */ *ap = v;
    int code;

#ifdef UVM
    (void)uvm_vnp_uncache(ap->a_vp);	/* toss stale pages */
#else
    vnode_pager_uncache(ap->a_vp);
#endif
    AFS_GLOCK();
    code =
	afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_ioctl(void *v)
{
    struct vop_ioctl_args	/* {
				 * struct vnode *a_vp;
				 * int  a_command;
				 * caddr_t  a_data;
				 * int  a_fflag;
				 * struct ucred *a_cred;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int code;

    /* in case we ever get in here... */

    AFS_STATCNT(afs_ioctl);
    AFS_GLOCK();
    if (((ap->a_command >> 8) & 0xff) == 'V')
	/* This is a VICEIOCTL call */
	code =
	    HandleIoctl(VTOAFS(ap->a_vp), ap->a_command,
			(struct afs_ioctl *)ap->a_data);
    else
	/* No-op call; just return. */
	code = ENOTTY;
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_select(void *v)
{
    return 1;
}

int
afs_nbsd_fsync(void *v)
{
    struct vop_fsync_args	/* {
				 * struct vnode *a_vp;
				 * struct ucred *a_cred;
				 * int a_waitfor;
				 * struct proc *a_p;
				 * } */ *ap = v;
    int wait = ap->a_waitfor == MNT_WAIT;
    struct vnode *vp = ap->a_vp;
    int code;

    AFS_GLOCK();
    vflushbuf(vp, wait);
    code = afs_fsync(VTOAFS(vp), ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_remove(void *v)
{
    struct vop_remove_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
    AFS_GLOCK();
    code = afs_remove(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);
    DROPCNP(cnp);
    DROPNAME();
    return code;
}

int
afs_nbsd_link(void *v)
{
    struct vop_link_args	/* {
				 * struct vnode *a_vp;
				 * struct vnode *a_tdvp;
				 * struct componentname *a_cnp;
				 * } */ *ap = v;
    int code;
    struct vnode *dvp = ap->a_dvp;
    struct vnode *vp = ap->a_vp;

    GETNAME();
    if (dvp->v_mount != vp->v_mount) {
	VOP_ABORTOP(vp, cnp);
	code = EXDEV;
	goto out;
    }
    if (vp->v_type == VDIR) {
	VOP_ABORTOP(vp, cnp);
	code = EISDIR;
	goto out;
    }
    if ((code = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curproc))) {
	VOP_ABORTOP(dvp, cnp);
	goto out;
    }

    AFS_GLOCK();
    code = afs_link(VTOAFS(vp), VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPCNP(cnp);
    if (dvp != vp)
	VOP_UNLOCK(vp, 0, curproc);

  out:
    vput(dvp);
    DROPNAME();
    return code;
}

int
afs_nbsd_rename(void *v)
{
    struct vop_rename_args	/* {
				 * struct vnode *a_fdvp;
				 * struct vnode *a_fvp;
				 * struct componentname *a_fcnp;
				 * struct vnode *a_tdvp;
				 * struct vnode *a_tvp;
				 * struct componentname *a_tcnp;
				 * } */ *ap = v;
    int code = 0;
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
    if ((fvp->v_mount != tdvp->v_mount)
	|| (tvp && (fvp->v_mount != tvp->v_mount))) {
	code = EXDEV;
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
	return (code);
    }
    /*
     * if fvp == tvp, we're just removing one name of a pair of
     * directory entries for the same element.  convert call into rename.
     ( (pinched from NetBSD 1.0's ufs_rename())
     */
    if (fvp == tvp) {
	if (fvp->v_type == VDIR) {
	    code = EINVAL;
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
	(void)relookup(fdvp, &fvp, fcnp);
	return (VOP_REMOVE(fdvp, fvp, fcnp));
    }

    if ((code = vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY, curproc)))
	goto abortit;

    MALLOC(fname, char *, fcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    bcopy(fcnp->cn_nameptr, fname, fcnp->cn_namelen);
    fname[fcnp->cn_namelen] = '\0';
    MALLOC(tname, char *, tcnp->cn_namelen + 1, M_TEMP, M_WAITOK);
    bcopy(tcnp->cn_nameptr, tname, tcnp->cn_namelen);
    tname[tcnp->cn_namelen] = '\0';


    AFS_GLOCK();
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    code =
	afs_rename(VTOAFS(fdvp), fname, VTOAFS(tdvp), tname, tcnp->cn_cred);
    AFS_GUNLOCK();

    VOP_UNLOCK(fvp, 0, curproc);
    FREE(fname, M_TEMP);
    FREE(tname, M_TEMP);
    if (code)
	goto abortit;		/* XXX */
    if (tdvp == tvp)
	vrele(tdvp);
    else
	vput(tdvp);
    if (tvp)
	vput(tvp);
    vrele(fdvp);
    vrele(fvp);
    return code;
}

int
afs_nbsd_mkdir(void *v)
{
    struct vop_mkdir_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * } */ *ap = v;
    struct vnode *dvp = ap->a_dvp;
    struct vattr *vap = ap->a_vap;
    int code;
    struct vcache *vcp;

    GETNAME();
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
	panic("afs_nbsd_mkdir: no name");
#endif
    AFS_GLOCK();
    code = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    if (code) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	DROPNAME();
	return (code);
    }
    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY, curproc);
    } else
	*ap->a_vpp = 0;
    DROPCNP(cnp);
    DROPNAME();
    vput(dvp);
    return code;
}

int
afs_nbsd_rmdir(void *v)
{
    struct vop_rmdir_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode *a_vp;
				 * struct componentname *a_cnp;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vnode *dvp = ap->a_dvp;

    GETNAME();
    if (dvp == vp) {
	vrele(dvp);
	vput(vp);
	DROPCNP(cnp);
	DROPNAME();
	return (EINVAL);
    }

    AFS_GLOCK();
    code = afs_rmdir(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPNAME();
    vput(dvp);
    vput(vp);
    return code;
}

int
afs_nbsd_symlink(void *v)
{
    struct vop_symlink_args	/* {
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * struct vattr *a_vap;
				 * char *a_target;
				 * } */ *ap = v;
    struct vnode *dvp = ap->a_dvp;
    int code;
    /* NFS ignores a_vpp; so do we. */

    GETNAME();
    AFS_GLOCK();
    code =
	afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target, cnp->cn_cred);
    AFS_GUNLOCK();
    DROPCNP(cnp);
    DROPNAME();
    vput(dvp);
    return code;
}

int
afs_nbsd_readdir(void *v)
{
    struct vop_readdir_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * struct ucred *a_cred;
				 * int *a_eofflag;
				 * int *a_ncookies;
				 * u_long **a_cookies;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
#ifdef AFS_HAVE_COOKIES
    printf("readdir %p cookies %p ncookies %d\n", ap->a_vp, ap->a_cookies,
	   ap->a_ncookies);
    code =
	afs_readdir(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, ap->a_eofflag,
		    ap->a_ncookies, ap->a_cookies);
#else
    code =
	afs_readdir(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, ap->a_eofflag);
#endif
    AFS_GUNLOCK();
    return code;
}

int
afs_nbsd_readlink(void *v)
{
    struct vop_readlink_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * struct ucred *a_cred;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code = afs_readlink(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred);
    AFS_GUNLOCK();
    return code;
}

extern int prtactive;

int
afs_nbsd_inactive(void *v)
{
    struct vop_inactive_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap = v;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);
    int haveGlock = ISAFS_GLOCK();

    AFS_STATCNT(afs_inactive);

    if (prtactive && vp->v_usecount != 0)
	vprint("afs_nbsd_inactive(): pushing active", vp);

    if (!haveGlock)
	AFS_GLOCK();
    afs_InactiveVCache(vc, 0);	/* decrs ref counts */
    if (!haveGlock)
	AFS_GUNLOCK();

    lockinit(&vc->rwlock, PINOD, "vcache", 0, 0);
    return 0;
}

int
afs_nbsd_reclaim(void *v)
{
    struct vop_reclaim_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap = v;
    int code, slept;
    struct vnode *vp = ap->a_vp;
    struct vcache *avc = VTOAFS(vp);
    int haveGlock = ISAFS_GLOCK();
    int haveVlock = CheckLock(&afs_xvcache);

#if 0
    printf("reclaim usecount %d\n", vp->v_usecount);
    /* OK, there are no internal vrefCounts, so there shouldn't
     * be any more refs here. */
    vp->v_data = NULL;		/* remove from vnode */
    avc->v = NULL;		/* also drop the ptr to vnode */
    return 0;
#else
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
    return code;
#endif
}

int
afs_nbsd_lock(void *v)
{
    struct vop_lock_args	/* {
				 * struct vnode *a_vp;
				 * int a_flags;
				 * sturct proc *a_p;
				 * } */ *ap = v;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if (!vc)
	panic("afs_nbsd_lock: null vcache");
    return afs_osi_lockmgr(&vc->rwlock, ap->a_flags | LK_CANRECURSE, &vp->v_interlock,
		   ap->a_p);
}

int
afs_nbsd_unlock(void *v)
{
    struct vop_unlock_args	/* {
				 * struct vnode *a_vp;
				 * int a_flags;
				 * struct proc *a_p;
				 * } */ *ap = v;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if (!vc)
	panic("afs_nbsd_unlock: null vcache");
    return afs_osi_lockmgr(&vc->rwlock, ap->a_flags | LK_RELEASE, &vp->v_interlock,
		   ap->a_p);
}

int
afs_nbsd_bmap(void *v)
{
    struct vop_bmap_args	/* {
				 * struct vnode *a_vp;
				 * daddr_t  a_bn;
				 * struct vnode **a_vpp;
				 * daddr_t *a_bnp;
				 * int *a_runp;
				 * } */ *ap = v;
    struct vcache *vcp = VTOAFS(ap->a_vp);

    AFS_STATCNT(afs_bmap);
    if (ap->a_bnp)
	ap->a_bnp = (daddr_t *) (ap->a_bn * (8192 / DEV_BSIZE));
    if (ap->a_vpp)
	*ap->a_vpp = (vcp) ? AFSTOV(vcp) : NULL;
    return 0;
}

int
afs_nbsd_strategy(void *v)
{
    struct vop_strategy_args	/* {
				 * struct buf *a_bp;
				 * } */ *ap = v;
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

    AFS_GLOCK();
    if ((abp->b_flags & B_READ) == B_READ) {
	code = afs_rdwr(tvc, &tuio, UIO_READ, 0, credp);
	if (code == 0 && tuio.afsio_resid > 0)
	    bzero(abp->b_un.b_addr + len - tuio.afsio_resid,
		  tuio.afsio_resid);
    } else
	code = afs_rdwr(tvc, &tuio, UIO_WRITE, 0, credp);
    AFS_GUNLOCK();

    ReleaseWriteLock(&tvc->lock);
    AFS_RELE(AFSTOV(tvc));
    return code;
}

int
afs_nbsd_print(void *v)
{
    struct vop_print_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap = v;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(ap->a_vp);

    printf("tag %d, fid: %d.%x.%x.%x, ", vp->v_tag, vc->fid.Cell,
	   (int)vc->fid.Fid.Volume, (int)vc->fid.Fid.Vnode,
	   (int)vc->fid.Fid.Unique);
    lockmgr_printinfo(&vc->rwlock);
    printf("\n");
    return 0;
}

int
afs_nbsd_islocked(void *v)
{
    struct vop_islocked_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap = v;
    return lockstatus(&VTOAFS(ap->a_vp)->rwlock);
}

/*
 * Return POSIX pathconf information applicable to ufs filesystems.
 */
int
afs_nbsd_pathconf(void *v)
{
    struct vop_pathconf_args	/* {
				 * struct vnode *a_vp;
				 * int a_name;
				 * int *a_retval;
				 * } */ *ap = v;
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
  afs_lockctl(struct vcache *avc, struct AFS_FLOCK *af, int acmd,
	      struct AFS_UCRED *acred, pid_t clid);

/*
 * Advisory record locking support (fcntl() POSIX style)
 */
int
afs_nbsd_advlock(void *v)
{
    struct vop_advlock_args	/* {
				 * struct vnode *a_vp;
				 * caddr_t  a_id;
				 * int  a_op;
				 * struct flock *a_fl;
				 * int  a_flags;
				 * } */ *ap = v;
    int code;

    AFS_GLOCK();
    code =
	afs_lockctl(VTOAFS(ap->a_vp), ap->a_fl, ap->a_op, osi_curcred(),
		    (int)ap->a_id);
    AFS_GUNLOCK();
    return code;
}
