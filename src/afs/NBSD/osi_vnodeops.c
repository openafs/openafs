/*
 * OpenBSD specific vnodeops + other misc interface glue
 * Original NetBSD version for Transarc afs by John Kohl <jtk@MIT.EDU>
 * OpenBSD version by Jim Rees <rees@umich.edu>
 *
 * $Id: osi_vnodeops.c,v 1.20 2006/03/09 15:27:17 rees Exp $
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

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics */

#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/pool.h>
#include <miscfs/genfs/genfs.h>

#include "afs/afs_cbqueue.h"
#include "afs/nfsclient.h"
#include "afs/afs_osidnlc.h"

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

int afs_debug;

/*
 * Skip:
 *   vop_*xtattr
 *
 */

/* Global vfs data structures for AFS. */
int (**afs_vnodeop_p) __P((void *));
const struct vnodeopv_entry_desc afs_vnodeop_entries[] = {
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
#if NOTYET
    {&vop_ioctl_desc, afs_nbsd_ioctl},		/* XXX ioctl */
#else
    {&vop_ioctl_desc, genfs_enoioctl},		/* ioctl */
#endif
    {&vop_fcntl_desc, genfs_fcntl},		/* fcntl */
    {&vop_poll_desc, genfs_poll},		/* poll */
    {&vop_kqfilter_desc, genfs_kqfilter },	/* kqfilter */
    {&vop_mmap_desc, genfs_mmap},		/* mmap */
    {&vop_fsync_desc, afs_nbsd_fsync},		/* fsync */
    {&vop_seek_desc, genfs_seek},		/* seek */
    {&vop_remove_desc, afs_nbsd_remove},	/* remove */
    {&vop_link_desc, afs_nbsd_link},		/* link */
    {&vop_rename_desc, afs_nbsd_rename},	/* rename */
    {&vop_mkdir_desc, afs_nbsd_mkdir},		/* mkdir */
    {&vop_rmdir_desc, afs_nbsd_rmdir},		/* rmdir */
    {&vop_symlink_desc, afs_nbsd_symlink},	/* symlink */
    {&vop_readdir_desc, afs_nbsd_readdir},	/* readdir */
    {&vop_readlink_desc, afs_nbsd_readlink},	/* readlink */
    {&vop_abortop_desc, genfs_abortop},	        /* abortop */
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
    {&vop_bwrite_desc, vn_bwrite},		/* bwrite */
    {&vop_getpages_desc, genfs_getpages},	/* getpages */
    {&vop_putpages_desc, genfs_putpages},	/* putpages */
    { NULL, NULL}
};
const struct vnodeopv_desc afs_vnodeop_opv_desc =
    { &afs_vnodeop_p, afs_vnodeop_entries };

static void
afs_nbsd_gop_size(struct vnode *vp, off_t size, off_t *eobp, int flags)
{

	*eobp = MAX(size, vp->v_size);
}

static int
afs_nbsd_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    kauth_cred_t cred)
{

    return (0);
}

static const struct genfs_ops afs_genfsops = {
	.gop_size  = afs_nbsd_gop_size,
	.gop_alloc = afs_nbsd_gop_alloc,
	.gop_write = genfs_gop_write,
};

extern void cpu_Debugger(void);

static char *
cnstrdup(const struct componentname *cnp)
{
	char *string;

	string = PNBUF_GET();
	memcpy(string, cnp->cn_nameptr, cnp->cn_namelen);
	string[cnp->cn_namelen] = '\0';

	return string;
}

static void
cnstrfree(char *string)
{
	PNBUF_PUT(string);
}

/* toss "stale" pages by shrinking the vnode uobj to a 0-length
 * region (see uvm_vnp_setsize in uvm_vnode.c) */
#ifdef AFS_NBSD50_ENV
#define VNP_UNCACHE(vp) \
    do {		\
	struct uvm_object *uobj = &vp->v_uobj; \
	mutex_enter(&uobj->vmobjlock); \
	VOP_PUTPAGES( (struct vnode *) uobj, 0 /* offlo */, 0 /* offhi */, PGO_FREE | PGO_SYNCIO); \
	mutex_exit(&uobj->vmobjlock); \
    } while(0);
#else
#define VNP_UNCACHE(vp) \
    do {		\
	struct uvm_object *uobj = &vp->v_uobj; \
	simple_lock(&uobj->vmobjlock); \
	VOP_PUTPAGES( (struct vnode *) uobj, 0 /* offlo */, 0 /* offhi */, PGO_FREE | PGO_SYNCIO); \
	simple_unlock(&uobj->vmobjlock); \
    } while(0);
#endif

/* psuedo-vnop, wherein we learn that obsd and nbsd disagree
 * about vnode refcounting */
void
afs_nbsd_getnewvnode(struct vcache *tvc)
{
    struct nbvdata *vd;

    KASSERT(AFSTOV(tvc) == NULL);
    while (getnewvnode(VT_AFS, afs_globalVFS, afs_vnodeop_p, &AFSTOV(tvc))) {
	/* no vnodes available, force an alloc (limits be damned)! */
	printf("afs: upping desiredvnodes\n");
	desiredvnodes++;
    }

    vd = kmem_zalloc(sizeof(*vd), KM_SLEEP);
#ifdef AFS_NBSD50_ENV
    mutex_enter(&AFSTOV(tvc)->v_interlock);
#else
    simple_lock(&AFSTOV(tvc)->v_interlock);
#endif
    vd->afsvc = tvc;
    AFSTOV(tvc)->v_data = vd;
    genfs_node_init(AFSTOV(tvc), &afs_genfsops);
#ifdef AFS_NBSD50_ENV
    mutex_exit(&AFSTOV(tvc)->v_interlock);
#else
    simple_unlock(&AFSTOV(tvc)->v_interlock);
#endif
    uvm_vnp_setsize(AFSTOV(tvc), 0);
}

int
afs_nbsd_lookup(void *v)
{
    struct vop_lookup_args	/* {
				 * struct vnodeop_desc * a_desc;
				 * struct vnode *a_dvp;
				 * struct vnode **a_vpp;
				 * struct componentname *a_cnp;
				 * } */ *ap = v;
    struct vnode *dvp, *vp;
    struct vcache *vcp;
    struct componentname *cnp;
    char *name;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_lookup a_cnp->cn_nameptr %s cred %p a_dvp %p\n",
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_cred, ap->a_dvp);
    } else {
        KASSERT(VOP_ISLOCKED(ap->a_dvp));
    }

    dvp = ap->a_dvp;
    vp = *ap->a_vpp = NULL;
    cnp = ap->a_cnp;

#if AFS_USE_NBSD_NAMECACHE
    code = cache_lookup(dvp, ap->a_vpp, cnp);
    if (code >= 0)
	goto out;
#endif

    code = 0;

    if (dvp->v_type != VDIR) {
	code = ENOTDIR;
	goto out;
    }

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    cnstrfree(name); name = NULL;

    if (code == ENOENT
	&& (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)
        && (cnp->cn_flags & ISLASTCN)) {
	    *ap->a_vpp = NULL;
	    code = EJUSTRETURN;
#if !defined(AFS_NBSD60_ENV)
            cnp->cn_flags |= SAVENAME;
#endif
	    goto out;
    }

    if (code == 0) {
        vp = *ap->a_vpp = AFSTOV(vcp);
        if (cnp->cn_flags & ISDOTDOT) {
#if defined(AFS_NBSD60_ENV)
            VOP_UNLOCK(dvp);
#else
            VOP_UNLOCK(dvp, 0);
#endif
	    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	    vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
        } else if (vp == dvp) {
	    vref(dvp);
	} else {
	    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
        }
    }

#if AFS_USE_NBSD_NAMECACHE
    if ((cnp->cn_flags & MAKEENTRY) && cnp->cn_nameiop != CREATE) {
	cache_enter(dvp, *ap->a_vpp, cnp);
    }
#endif

 out:
    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_lookup done dvp %p cnt %d\n", dvp, dvp->v_usecount);
    }

    if (code == 0 && afs_debug == 0) {
        KASSERT(VOP_ISLOCKED(*ap->a_vpp));
    }

    return (code);
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
    struct componentname *cnp = ap->a_cnp;
    char *name;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	/* printf("nbsd_create dvp %p cnt %d\n", dvp, dvp->v_usecount); */
	printf("nbsd_create a_cnp->cn_nameptr %s cred %p a_dvp %p\n",
	    ap->a_cnp->cn_nameptr, ap->a_cnp->cn_cred, ap->a_dvp);
	/* printf("name: %d %s\n", ap->a_cnp->cn_namelen, name); */
    }

    /* vnode layer handles excl/nonexcl */

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code =
	afs_create(VTOAFS(dvp), name, ap->a_vap, NONEXCL, ap->a_vap->va_mode,
		   &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    cnstrfree(name);
    if (code) {
	VOP_ABORTOP(dvp, cnp);
	vput(dvp);
	return (code);
    }

    if (vcp) {
	*ap->a_vpp = AFSTOV(vcp);
	vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY);
    } else
	*ap->a_vpp = NULL;

#if !defined(AFS_NBSD60_ENV)
    if (code || (cnp->cn_flags & SAVESTART) == 0) {
        PNBUF_PUT(cnp->cn_pnbuf);
    }
#endif
    vput(dvp);
    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_create done dvp %p cnt %d\n", dvp, dvp->v_usecount);
    }
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

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_mknod: enter %p dvp %p\n", ap, ap->a_dvp);
    }


#if !defined(AFS_NBSD60_ENV)
    PNBUF_PUT(ap->a_cnp->cn_pnbuf);
#endif
    vput(ap->a_dvp);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_mknod: exit ap %p\n", ap);
    }

    return (ENODEV);
}

int
afs_nbsd_open(void *v)
{
    struct vop_open_args	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * struct ucred *a_cred;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_open: enter %p vp %p vc %p\n", ap, ap->a_vp, vc);
    }

    AFS_GLOCK();
    code = afs_open(&vc, ap->a_mode, ap->a_cred);
#ifdef DIAGNOSTIC
    if (AFSTOV(vc) != ap->a_vp)
	panic("nbsd_open: AFS open changed vnode!");
#endif
    AFS_GUNLOCK();

    uvm_vnp_setsize(ap->a_vp, VTOAFS(ap->a_vp)->f.m.Length);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_open: exit vp %p vc %p\n", vp, vc);
    }

    return (code);
}

int
afs_nbsd_close(void *v)
{
    struct vop_close_args	/* {
				 * struct vnode *a_vp;
				 * int  a_fflag;
				 * kauth_cred_t a_cred;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_close: enter %p vp %p vc %p\n", ap, ap->a_vp, vc);
    }

    AFS_GLOCK();
    code = afs_close(VTOAFS(ap->a_vp), ap->a_fflag, ap->a_cred);
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_close: exit vp %p vc %p\n", vp, vc);
    }

    return (code);
}

int
afs_nbsd_access(void *v)
{
    struct vop_access_args	/* {
				 * struct vnode *a_vp;
				 * int  a_mode;
				 * kauth_cred_t a_cred;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_access: enter %p vp %p vc %p mode %d\n", ap, ap->a_vp, vc, ap->a_mode);
    }

    AFS_GLOCK();
    code = afs_access(VTOAFS(ap->a_vp), ap->a_mode, ap->a_cred);
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_access: exit vp %p vc %p mode %d\n", vp, vc, ap->a_mode);
    }

    return (code);
}

int
afs_nbsd_getattr(void *v)
{
    struct vop_getattr_args	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * kauth_cred_t a_cred;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_getattr: enter %p vp %p vc %p acred %p\n",
	    ap, ap->a_vp, vc, ap->a_cred);
    }

    AFS_GLOCK();
    code = afs_getattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_getattr: exit vp %p vc %p acred %p\n", vp, vc,
               ap->a_cred);
    }

    return (code);
}

int
afs_nbsd_setattr(void *v)
{
    struct vop_setattr_args	/* {
				 * struct vnode *a_vp;
				 * struct vattr *a_vap;
				 * kauth_cred_t a_cred;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_setattr: enter %p vp %p\n", ap, ap->a_vp);
    }

    AFS_GLOCK();
    code = afs_setattr(VTOAFS(ap->a_vp), ap->a_vap, ap->a_cred);
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_setattr: exit %p\n", ap);
    }

    return (code);
}

int
afs_nbsd_read(void *v)
{
    struct vop_read_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * kauth_cred_t a_cred;
				 * } */ *ap = v;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_read enter %p vp %p\n", ap, ap->a_vp);
    }

    AFS_GLOCK();
    code = afs_read(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred, 0);
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_read exit %p\n", ap);
    }

    return (code);
}

int
afs_nbsd_write(void *v)
{
    struct vop_write_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * int a_ioflag;
				 * kauth_cred_t a_cred;
				 * } */ *ap = v;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_write enter %p vp %p\n", ap, ap->a_vp);
    }

    AFS_GLOCK();
    /* osi_FlushPages(VTOAFS(ap->a_vp), ap->a_cred); */
    code =
        afs_write(VTOAFS(ap->a_vp), ap->a_uio, ap->a_ioflag, ap->a_cred, 0);
    AFS_GUNLOCK();

    if (ap->a_vp->v_size < ap->a_uio->uio_offset) {
	uvm_vnp_setsize(ap->a_vp, ap->a_uio->uio_offset);
    }

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_write exit %p\n", ap);
    }

    return (code);
}

int
afs_nbsd_ioctl(void *v)
{
    struct vop_ioctl_args	/* {
				 * struct vnode *a_vp;
				 * int  a_command;
				 * caddr_t  a_data;
				 * int  a_fflag;
				 * kauth_cred_t a_cred;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_ioctl: enter %p vp %p\n", ap, ap->a_vp);
    }

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

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_ioctl: exit %p\n", ap);
    }

    return (code);
}

int
afs_nbsd_fsync(void *v)
{
    struct vop_fsync_args	/* {
				 * struct vnode *a_vp;
				 * kauth_cred_t a_cred;
				 * int a_waitfor;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    struct vnode *vp = ap->a_vp;
    int code, wait;

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_fsync: enter %p vp %p\n", ap, ap->a_vp);

    wait = (ap->a_flags & FSYNC_WAIT) != 0;

    AFS_GLOCK();
    vflushbuf(vp, wait);
    code = afs_fsync(VTOAFS(vp), ap->a_cred);
    AFS_GUNLOCK();

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_fsync: exit %p\n", ap);

    return (code);
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
    struct componentname *cnp = ap->a_cnp;
    char *name;

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_remove: enter %p vp %p\n", ap, ap->a_vp);

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code = afs_remove(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    cnstrfree(name); name = NULL;
    if (dvp == vp)
	vrele(vp);
    else
	vput(vp);
    vput(dvp);

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_remove: exit %p\n", ap);

    return (code);
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
    struct componentname *cnp = ap->a_cnp;
    char *name;

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_link: enter %p vp %p\n", ap, ap->a_vp);

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
    if (dvp != vp) {
      if ((code = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY))) {
        VOP_ABORTOP(dvp, cnp);
        goto out;
      }
    }

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code = afs_link(VTOAFS(vp), VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    cnstrfree(name); name = NULL;
    if (dvp != vp) {
#if defined(AFS_NBSD60_ENV)
        VOP_UNLOCK(vp);
#else
        VOP_UNLOCK(vp, 0);
#endif
    }

  out:
    vput(dvp);

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_link: exit %p\n", ap);

    return (code);
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

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_rename: enter %p\n", ap);

    /*
     * Check for cross-device rename.
     */
    if ((fvp->v_mount != tdvp->v_mount)
        || (tvp && (fvp->v_mount != tvp->v_mount))) {
        code = EXDEV;
    abortit:
        VOP_ABORTOP(tdvp, tcnp);    /* XXX, why not in NFS? */
        if (tdvp == tvp)
            vrele(tdvp);
        else
            vput(tdvp);
        if (tvp)
            vput(tvp);
        VOP_ABORTOP(fdvp, fcnp);    /* XXX, why not in NFS? */
        vrele(fdvp);
        vrele(fvp);
        goto out;
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
        fcnp->cn_nameiop = DELETE;
#if !defined(AFS_NBSD60_ENV)
        if ((fcnp->cn_flags & SAVESTART) == 0)
            panic("afs_rename: lost from startdir");
        (void)relookup(fdvp, &fvp, fcnp);
#else
        (void)relookup(fdvp, &fvp, fcnp, 0);
#endif
        code = VOP_REMOVE(fdvp, fvp, fcnp);
	goto out;
    }

    if ((code = vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY)))
        goto abortit;

    fname = cnstrdup(fcnp);
    tname = cnstrdup(tcnp);

    AFS_GLOCK();
    /* XXX use "from" or "to" creds? NFS uses "to" creds */
    code = afs_rename(VTOAFS(fdvp), fname, VTOAFS(tdvp), tname, tcnp->cn_cred);
    AFS_GUNLOCK();

    cnstrfree(fname); fname = NULL;
    cnstrfree(tname); tname = NULL;
    if (code)
        goto abortit;        /* XXX */
    if (tdvp == tvp)
        vrele(tdvp);
    else
        vput(tdvp);
    if (tvp)
        vput(tvp);
    vrele(fdvp);
    vput(fvp);

 out:
    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_rename: exit %p\n", ap);

    return (code);
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
    struct componentname *cnp = ap->a_cnp;
    struct vcache *vcp;
    int code;
    char *name;


    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_mkdir: enter %p dvp %p\n", ap, ap->a_dvp);

#if !defined(AFS_NBSD60_ENV)
#ifdef DIAGNOSTIC
    if ((cnp->cn_flags & HASBUF) == 0)
        panic("afs_nbsd_mkdir: no name");
#endif
#endif

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code = afs_mkdir(VTOAFS(dvp), name, vap, &vcp, cnp->cn_cred);
    AFS_GUNLOCK();
    cnstrfree(name); name = NULL;
    if (code) {
        VOP_ABORTOP(dvp, cnp);
        vput(dvp);
        goto out;
    }
    if (vcp) {
        *ap->a_vpp = AFSTOV(vcp);
        vn_lock(AFSTOV(vcp), LK_EXCLUSIVE | LK_RETRY);
    } else
        *ap->a_vpp = NULL;
#if !defined(AFS_NBSD60_ENV)
    if (code || (cnp->cn_flags & SAVESTART) == 0) {
        PNBUF_PUT(cnp->cn_pnbuf);
    }
#endif
    vput(dvp);

 out:
    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_mkdir: exit %p\n", ap);

    return (code);
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
    struct componentname *cnp = ap->a_cnp;
    char *name;


    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_rmdir: enter %p vp %p\n", ap, ap->a_vp);

    if (dvp == vp) {
        vrele(dvp);
        vput(vp);
        code = EINVAL;
	goto out;
    }

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code = afs_rmdir(VTOAFS(dvp), name, cnp->cn_cred);
    AFS_GUNLOCK();
    cnstrfree(name); name = NULL;

#if AFS_USE_NBSD_NAMECACHE
    if (code == 0) {
	cache_purge(vp);
    }
#endif

    vput(dvp);
    vput(vp);

 out:
    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_rmdir: exit %p\n", ap);

    return (code);
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
    struct vnode *nvp = NULL;
    struct vcache *vcp;
    struct componentname *cnp = ap->a_cnp;
    int code;
    char *name;
    /* NFS ignores a_vpp; so do we. */

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_symlink: enter %p dvp %p\n", ap, ap->a_dvp);

    name = cnstrdup(cnp);
    AFS_GLOCK();
    code =
	afs_symlink(VTOAFS(dvp), name, ap->a_vap, ap->a_target, NULL,
		    cnp->cn_cred);
    if (code == 0) {
	code = afs_lookup(VTOAFS(dvp), name, &vcp, cnp->cn_cred);
        if (code == 0) {
	  nvp = AFSTOV(vcp);
	  vn_lock(nvp, LK_EXCLUSIVE | LK_RETRY);
	}
    }
    AFS_GUNLOCK();
    cnstrfree(name); name = NULL;
#if !defined(AFS_NBSD60_ENV)
    if (code || (cnp->cn_flags & SAVESTART) == 0) {
        PNBUF_PUT(cnp->cn_pnbuf);
    }
#endif

    *(ap->a_vpp) = nvp;

    vput(dvp);

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_symlink: exit %p\n", ap);

    return (code);
}

int
afs_nbsd_readdir(void *v)
{
    struct vop_readdir_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * kauth_cred_t a_cred;
				 * int *a_eofflag;
				 * int *a_ncookies;
				 * u_long **a_cookies;
				 * } */ *ap = v;
    int code;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(vp);

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_readdir: enter %p vp %p vc %p acred %p auio %p\n", ap,
	    vp, vc, ap->a_cred, ap->a_uio);

    AFS_GLOCK();
#ifdef AFS_HAVE_COOKIES
    printf("readdir %p cookies %p ncookies %d\n", ap->a_vp, ap->a_cookies,
           ap->a_ncookies);
    code =
        afs_readdir(vc, ap->a_uio, ap->a_cred, ap->a_eofflag,
		    ap->a_ncookies, ap->a_cookies);
#else
    code =
        afs_readdir(vc, ap->a_uio, ap->a_cred, ap->a_eofflag);
#endif
    AFS_GUNLOCK();

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_readdir: exit %p eofflag %d\n", ap, *ap->a_eofflag);

    return (code);
}

int
afs_nbsd_readlink(void *v)
{
    struct vop_readlink_args	/* {
				 * struct vnode *a_vp;
				 * struct uio *a_uio;
				 * kauth_cred_t a_cred;
				 * } */ *ap = v;
    int code;

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_readlink: enter %p vp %p\n", ap, ap->a_vp);

    AFS_GLOCK();
    code = afs_readlink(VTOAFS(ap->a_vp), ap->a_uio, ap->a_cred);
    AFS_GUNLOCK();

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_readlink: exit %p\n", ap);

    return (code);
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

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_inactive: enter %p vp %p\n", ap, ap->a_vp);

    if (prtactive && vp->v_usecount != 0)
        vprint("afs_nbsd_inactive: pushing active", vp);

    if (!haveGlock)
        AFS_GLOCK();
    afs_InactiveVCache(vc, NULL);	/* decrs ref counts */
    if (!haveGlock)
        AFS_GUNLOCK();

    *ap->a_recycle = (vc->f.states & CUnlinked) != 0;

#if defined(AFS_NBSD60_ENV)
    VOP_UNLOCK(vp);
#else
    VOP_UNLOCK(vp, 0);
#endif

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_inactive: exit %p\n", ap);

    return (0);
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

    if (afs_debug & AFSDEB_VNLAYER)
	printf("nbsd_reclaim: enter %p vp %p\n", ap, vp);

    if (!haveGlock)
		AFS_GLOCK();
    if (!haveVlock)
		ObtainWriteLock(&afs_xvcache, 901);
    /* reclaim the vnode and the in-memory vcache, but keep the on-disk vcache */
    code = afs_FlushVCache(avc, &slept);

#if 1
    if (avc->f.states & CVInit) {
	avc->f.states &= ~CVInit;
	afs_osi_Wakeup(&avc->f.states);
    }
#endif

    if (!haveVlock)
	ReleaseWriteLock(&afs_xvcache);
    if (!haveGlock)
	AFS_GUNLOCK();

    if (vp->v_tag != VT_AFS) {
        vprint("afs reclaim", vp);
    }
    KASSERT(vp->v_tag == VT_AFS);

    if (vp->v_data != NULL) {
        genfs_node_destroy(vp);
        kmem_free(vp->v_data, sizeof(struct nbvdata));
        vp->v_data = NULL;		/* remove from vnode */
        avc->v = NULL;			/* also drop the ptr to vnode */
    } else {
	if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	    vprint("reclaim", vp);
	}
    }

#if AFS_USE_NBSD_NAMECACHE
    cache_purge(vp);
#endif

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_reclaim: exit %p\n", ap);
    }

    return code;
}

int
afs_nbsd_lock(void *v)
{
    struct vop_lock_args	/* {
				 * struct vnode *a_vp;
				 * int a_flags;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_lock: enter %p vp %p\n", ap, ap->a_vp);
    }

    KASSERT(VTOAFS(ap->a_vp) != NULL);

    code = genfs_lock(v);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_lock: exit %p\n", ap);
    }

    return (code);
}

int
afs_nbsd_unlock(void *v)
{
    struct vop_unlock_args	/* {
				 * struct vnode *a_vp;
				 * int a_flags;
				 * struct lwp *a_l;
				 * } */ *ap = v;
    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_unlock: enter %p vp %p\n", ap, ap->a_vp);
    }

    KASSERT(VTOAFS(ap->a_vp) != NULL);

    code = genfs_unlock(v);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_unlock: exit %p\n", ap);
    }

    return (code);
}

int
afs_nbsd_islocked(void *v)
{
    struct vop_islocked_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap = v;

    int code;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_islocked: enter %p vp %p\n", ap, ap->a_vp);
    }

    code = genfs_islocked(v);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_islocked: exit %p\n", ap);
    }

    return (code);
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

    AFS_STATCNT(afs_bmap);

    if (afs_debug & AFSDEB_VNLAYER)
        printf("nbsd_bmap: enter %p vp %p\n", ap, ap->a_vp);

    if (ap->a_bnp)
        *ap->a_bnp = ap->a_bn;
    if (ap->a_vpp)
        *ap->a_vpp = ap->a_vp;
    if (ap->a_runp != NULL)
        *ap->a_runp = 1024 * 1024; /* XXX */

    return (0);
}

int
afs_nbsd_strategy(void *v)
{
    struct vop_strategy_args	/* {
				 * struct buf *a_bp;
				 * } */ *ap = v;
    int code;

    AFS_STATCNT(afs_strategy);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_strategy: enter %p vp %p\n", ap, ap->a_vp);
    }

    AFS_GLOCK();
    code = afs_ustrategy(ap->a_bp, osi_curcred());
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
        printf("nbsd_strategy: exit %p vp %p\n", ap, ap->a_vp);
    }

    return (code);
}

int
afs_nbsd_print(void *v)
{
    struct vop_print_args	/* {
				 * struct vnode *a_vp;
				 * } */ *ap = v;
    struct vnode *vp = ap->a_vp;
    struct vcache *vc = VTOAFS(ap->a_vp);

    printf("tag %d, fid: %d.%x.%x.%x, ", vp->v_tag, vc->f.fid.Cell,
	   vc->f.fid.Fid.Volume, vc->f.fid.Fid.Vnode,
	   vc->f.fid.Fid.Unique);
#ifdef AFS_NBSD50_ENV
#if defined(DDB) && defined(LOCKDEBUG)
    lockdebug_lock_print(&vc->rwlock, printf);
#endif
#else
    lockmgr_printinfo(&vc->rwlock);
#endif
    printf("\n");
    return (0);
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
    int code = 0;

    AFS_STATCNT(afs_cntl);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_pathconf: enter %p vp %p\n", ap, ap->a_vp);
    }

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
        code = EINVAL;
        goto out;
        break;
    default:
        code = EINVAL;
        goto out;
    }

out:
    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_pathconf: exit %p\n", ap);
    }

    return (0);
}

extern int
  afs_lockctl(struct vcache *avc, struct AFS_FLOCK *af, int acmd,
	      afs_ucred_t *acred, pid_t clid);

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

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_pathconf: enter %p vp %p\n", ap, ap->a_vp);
    }

    AFS_GLOCK();
    code = afs_lockctl(VTOAFS(ap->a_vp), ap->a_fl, ap->a_op, osi_curcred(),
	(uintptr_t)ap->a_id);
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	printf("nbsd_pathconf: exit %p\n", ap);
    }

    return (code);
}
