/*
 * OpenBSD specific assistance routines & VFS ops
 * Original NetBSD version for Transarc afs by John Kohl <jtk@MIT.EDU>
 * OpenBSD version by Jim Rees <rees@umich.edu>
 *
 * $Id$
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
of the OpenAFS License Agreement, and (ii) you do not use the name
of MIT in any advertising or publicity without the prior written consent
of MIT.  MIT disclaims all liability for your use of MIT's
Modifications.  MIT's Modifications are provided "AS IS" WITHOUT
WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO,
ANY WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
NONINFRINGEMENT.
*/

/*
 * Some code cribbed from ffs_vfsops and other NetBSD sources, which
 * are marked:
 */
/*
 * Copyright (c) 1989, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/namei.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

/* from /usr/src/sys/kern/vfs_subr.c */
extern void insmntque(struct vnode *, struct mount *);

extern int sys_lkmnosys(), afs_xioctl(), Afs_xsetgroups();

static int lkmid = -1;
static int afs_badcall(struct proc *p, void *xx, register_t * yy);
static struct sysent old_sysent;

struct osi_vfs *afs_globalVFS;
struct vcache *afs_globalVp;

int afs_quotactl();
int afs_fhtovp();
int afs_vptofh();
int afsinit();
int afs_start();
int afs_mount();
int afs_unmount();
int afs_root();
int afs_statfs();
int afs_sync();
int afs_vget();
int afs_sysctl();
int afs_checkexp();

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
    afsinit,
    afs_sysctl,
    afs_checkexp,
};

int
afs_obsd_lookupname(char *fnamep, enum uio_seg segflg, int followlink,
		    struct vnode **compvpp)
{
    struct nameidata nd;
    int niflag;
    int error;

    /*
     * Lookup pathname "fnamep", returning leaf in *compvpp.  segflg says
     * whether the pathname is user or system space.
     */
    /* XXX LOCKLEAF ? */
    niflag = followlink ? FOLLOW : NOFOLLOW;
    NDINIT(&nd, LOOKUP, niflag, segflg, fnamep, osi_curproc());
    if ((error = namei(&nd)))
	return error;
    *compvpp = nd.ni_vp;
    return error;
}

int
afs_quotactl()
{
    return EOPNOTSUPP;
}

int
afs_sysctl()
{
    return EOPNOTSUPP;
}

int
afs_checkexp()
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
    return (0);			/* nothing to do. ? */
}

int
afs_mount(mp, path, data, ndp, p)
     struct mount *mp;
     char *path;
     caddr_t data;
     struct nameidata *ndp;
     struct proc *p;
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our proc struct. */
    int size;

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    if (afs_globalVFS) {
	/* Don't allow remounts */
	return EBUSY;
    }

    AFS_STATCNT(afs_mount);
    AFS_GLOCK();

    /* initialize the vcache entries before we start using them */

    /* XXX find a better place for this if possible  */
    afs_globalVFS = mp;
    mp->osi_vfs_bsize = 8192;
    mp->osi_vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    mp->osi_vfs_fsid.val[1] = (int)AFS_VFSFSID;

#if defined(AFS_OBSD53_ENV)
    bzero(mp->mnt_stat.f_mntonname, MNAMELEN);
    strlcpy(mp->mnt_stat.f_mntonname, path, MNAMELEN);
#else
    (void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
    bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
#endif
    bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
    /* null terminated string "AFS" will fit, just leave it be. */
    strcpy(mp->mnt_stat.f_fstypename, MOUNT_AFS);
    AFS_GUNLOCK();
    (void)afs_statfs(mp, &mp->mnt_stat);

    return 0;
}

int
afs_unmount(afsp, flags, p)
     struct mount *afsp;
     int flags;
     struct proc *p;
{
    extern int sys_ioctl(), sys_setgroups();

    struct vnode *vp;

    for (vp = LIST_FIRST(&afsp->mnt_vnodelist); vp != NULL;
	vp = LIST_NEXT(vp, v_mntvnodes)) {
	if (vp->v_usecount) return EBUSY;
    }

    AFS_STATCNT(afs_unmount);
    if (afs_globalVFS == NULL) {
	printf("afs already unmounted\n");
	return 0;
    }
    if (afs_globalVp)
	vrele(AFSTOV(afs_globalVp));
    afs_globalVp = NULL;

    vflush(afsp, NULLVP, 0);	/* don't support forced */
    afsp->mnt_data = NULL;
    AFS_GLOCK();
    afs_globalVFS = 0;
    afs_cold_shutdown = 1;
    afs_shutdown();		/* XXX */
    AFS_GUNLOCK();

    /* give up syscall entries for ioctl & setgroups, which we've stolen */
    sysent[SYS_ioctl].sy_call = sys_ioctl;
    sysent[SYS_setgroups].sy_call = sys_setgroups;

    /* give up the stolen syscall entry */
    sysent[AFS_SYSCALL].sy_narg = 0;
    sysent[AFS_SYSCALL].sy_argsize = 0;
    sysent[AFS_SYSCALL].sy_call = afs_badcall;
    printf
	("AFS unmounted--use `/sbin/modunload -i %d' to unload before restarting AFS\n",
	 lkmid);
    return 0;
}

static int
afs_badcall(struct proc *p, void *xx, register_t * yy)
{
    return ENOSYS;
}

#if defined(AFS_OBSD49_ENV)
extern struct vops afs_vops;
#endif

void
afs_obsd_getnewvnode(struct vcache *tvc)
{
#if defined(AFS_OBSD49_ENV)
    while (getnewvnode(VT_AFS, afs_globalVFS, &afs_vops, &tvc->v)) {
#else
    while (getnewvnode(VT_AFS, afs_globalVFS, afs_vnodeop_p, &tvc->v)) {
#endif
	/* no vnodes available, force an alloc (limits be damned)! */
	desiredvnodes++;
    }
    tvc->v->v_data = (void *)tvc;
}

int
afs_root(struct mount *mp, struct vnode **vpp)
{
    struct vrequest treq;
    struct vcache *tvp;
    int code;

    AFS_STATCNT(afs_root);

    AFS_GLOCK();
    if (!(code = afs_InitReq(&treq, osi_curcred()))
	&& !(code = afs_CheckInit())) {
	tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	if (tvp) {
	    /* There is really no reason to over-hold this bugger--it's held
	     * by the root filesystem reference. */
	    if (afs_globalVp != tvp) {
#ifdef AFS_DONT_OVERHOLD_GLOBALVP
		if (afs_globalVp)
		    AFS_RELE(AFSTOV(afs_globalVp));
#endif
		afs_globalVp = tvp;
		vref(AFSTOV(afs_globalVp));
	    }
	    AFSTOV(tvp)->v_flag |= VROOT;
	    afs_globalVFS = mp;
	    *vpp = AFSTOV(tvp);
	} else
	    code = ENOENT;
    }
    AFS_GUNLOCK();

    if (!code)
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, curproc);	/* return it locked */
    return code;
}

int
afs_statfs(struct osi_vfs *afsp, struct statfs *abp)
{
    AFS_STATCNT(afs_statfs);
    abp->f_bsize = afsp->osi_vfs_bsize;

    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = AFS_VFS_FAKEFREE;
    abp->f_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    abp->f_fsid.val[1] = (int)AFS_VFSFSID;
    return 0;
}

int
afs_sync(struct osi_vfs *afsp)
{
    AFS_STATCNT(afs_sync);
    return 0;
}

int
afs_vget(vp, lfl)
     struct vnode *vp;
     int lfl;
{
    int error;

    if (vp->v_usecount < 0) {
	vprint("bad usecount", vp);
	panic("afs_vget");
    }
    error = vget(vp, lfl, curproc);
    if (!error && vp->v_usecount == 1) {
	/* vget() took it off the freelist; put it on our mount queue */
	insmntque(vp, afs_globalVFS);
    }
    return error;
}

extern struct vfsops afs_vfsops;
extern struct vnodeopv_desc afs_vnodeop_opv_desc;

static struct vfsconf afs_vfsconf = {
    &afs_vfsops,
    "afs",
    0,
    0,
    0,
    NULL,
    NULL,
};

MOD_VFS("afs", 0, &afs_vfsconf);

static char afsgenmem[] = "afsgenmem";
static char afsfidmem[] = "afsfidmem";
static char afsbhdrmem[] = "afsbhdrmem";
static char afsbfrmem[] = "afsbfrmem";

int
afsinit()
{
    old_sysent = sysent[AFS_SYSCALL];

    sysent[AFS_SYSCALL].sy_call = afs3_syscall;
    sysent[AFS_SYSCALL].sy_narg = 6;
    sysent[AFS_SYSCALL].sy_argsize = 6 * sizeof(long);
    sysent[SYS_ioctl].sy_call = afs_xioctl;
    sysent[SYS_setgroups].sy_call = Afs_xsetgroups;
    osi_Init();

    return 0;
}

int
afs_vfs_load(struct lkm_table *lkmtp, int cmd)
{
    extern char *memname[];

#if ! defined(AFS_OBSD49_ENV)
    vfs_opv_init_explicit(&afs_vnodeop_opv_desc);
    vfs_opv_init_default(&afs_vnodeop_opv_desc);
#endif
    if (memname[M_AFSGENERIC] == NULL)
	memname[M_AFSGENERIC] = afsgenmem;
    if (memname[M_AFSFID] == NULL)
	memname[M_AFSFID] = afsfidmem;
    if (memname[M_AFSBUFHDR] == NULL)
	memname[M_AFSBUFHDR] = afsbhdrmem;
    if (memname[M_AFSBUFFER] == NULL)
	memname[M_AFSBUFFER] = afsbfrmem;
    lkmid = lkmtp->id;
    printf("OpenAFS ($Revision$) lkm loaded\n");
    return 0;
}

int
afs_vfs_unload(struct lkm_table *lktmp, int cmd)
{
    extern char *memname[];

    if (afs_globalVp)
	return EBUSY;
    if (sysent[SYS_ioctl].sy_call != sys_ioctl)
	return EBUSY;

    if (memname[M_AFSGENERIC] == afsgenmem)
	memname[M_AFSGENERIC] = NULL;
    if (memname[M_AFSFID] == afsfidmem)
	memname[M_AFSFID] = NULL;
    if (memname[M_AFSBUFHDR] == afsbhdrmem)
	memname[M_AFSBUFHDR] = NULL;
    if (memname[M_AFSBUFFER] == afsbfrmem)
	memname[M_AFSBUFFER] = NULL;

    sysent[AFS_SYSCALL] = old_sysent;
    printf("OpenAFS unloaded\n");
    return 0;
}

int
libafs_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
    if (cmd == LKM_E_LOAD) {
	if (sysent[AFS_SYSCALL].sy_call == afs3_syscall
	    || sysent[AFS_SYSCALL].sy_call == afs_badcall) {
	    printf("AFS already loaded\n");
	    return EINVAL;
	}
    }
    DISPATCH(lkmtp, cmd, ver, afs_vfs_load, afs_vfs_unload, lkm_nofunc);
}
