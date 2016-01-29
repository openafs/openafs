/*
 * OpenBSD specific assistance routines & VFS ops
 * Original NetBSD version for Transarc afs by John Kohl <jtk@MIT.EDU>
 * OpenBSD version by Jim Rees <rees@umich.edu>
 * Reported to NetBSD 4.0 by Matt Benjamin (matt@linuxbox.com)
 *
 * $Id: osi_vfsops.c,v 1.20 2005/03/08 21:58:04 shadow Exp $
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

#include <sys/ioctl.h>
#ifndef AFS_NBSD60_ENV
#include <sys/lkm.h>
#endif
#include <sys/namei.h>
#include <miscfs/genfs/genfs.h>

VFS_PROTOS(afs);

#ifndef AFS_NBSD60_ENV
extern int sys_lkmnosys(struct lwp *, const void *, register_t *);
extern int afs3_syscall(struct lwp *, const void *, register_t *);
extern int afs_xioctl(struct lwp *, const void *, register_t *);
extern int Afs_xsetgroups(struct lwp *, const void *, register_t *);
static int afs_badcall(struct lwp *, const void *, register_t *);

static int lkmid = -1;

struct sysent afs_sysent = { 6,
			     sizeof(struct afs_sysargs),
			     0,
			     afs3_syscall};

static struct sysent old_sysent;
static struct sysent old_setgroups;
#endif

struct osi_vfs *afs_globalVFS;
struct vcache *afs_globalVp;
fsid_t afs_dynamic_fsid;

int afs_mount(struct mount *, const char *, void *, size_t *);
int afs_start(struct mount *, int);
int afs_unmount(struct mount *, int);
int afs_root(struct mount *, struct vnode **);
int afs_statvfs(struct mount *, struct statvfs *);
int afs_sync(struct mount *, int, kauth_cred_t);
void afs_init(void);
void afs_reinit(void);
void afs_done(void);

extern struct vnodeopv_desc afs_vnodeop_opv_desc;
static const struct vnodeopv_desc *afs_vnodeopv_descs[] = {
	&afs_vnodeop_opv_desc,
	NULL,
};

struct vfsops afs_vfsops = {
    AFS_MOUNT_AFS,
#ifdef AFS_NBSD50_ENV
	0,						/* vfs_min_mount_data */
#endif
    afs_mount,
    afs_start,
    afs_unmount,
    afs_root,
    (void *) eopnotsupp,	/* vfs_quotactl */
    afs_statvfs,
    afs_sync,
    (void *) eopnotsupp,	/* vfs_vget */
    (void *) eopnotsupp,	/* vfs_fhtovp */
    (void *) eopnotsupp,	/* vfs_vptofh */
    afs_init,
    afs_reinit,
    afs_done,
    (void *) eopnotsupp,	/* vfs_mountroot */
    (void *) eopnotsupp,	/* vfs_snapshot */
    vfs_stdextattrctl,
#ifdef AFS_NBSD50_ENV
    (void *) eopnotsupp, 	/* vfs_suspendctl */
    genfs_renamelock_enter,	/* vfs_renamelock_enter */
    genfs_renamelock_exit,	/* vfs_renamelock_exit */
    (void *) eopnotsupp,	/* vfs_fsync */
#endif
    afs_vnodeopv_descs,
    0,				/* vfs_refcount */
    { NULL, NULL },
};

int
afs_nbsd_lookupname(const char *fnamep, enum uio_seg segflg, int followlink,
		    struct vnode **compvpp)
{
    struct nameidata nd;
    int niflag;
    int error;

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	afs_warn("afs_nbsd_lookupname enter (%s)\n", fnamep);
    }

    /*
     * Lookup pathname "fnamep", returning leaf in *compvpp.  segflg says
     * whether the pathname is user or system space.
     */
    /* XXX LOCKLEAF ? */
    niflag = followlink ? FOLLOW : NOFOLLOW;
	/*
	*	NBSD50 seems to have stopped caring about the curproc of things.
	*	mattjsm
	*/
#if defined(AFS_NBSD60_ENV)
    struct pathbuf *ipb = NULL;
    ipb = pathbuf_create(fnamep);
    NDINIT(&nd, LOOKUP, niflag, ipb);
#elif defined(AFS_NBSD50_ENV)
    NDINIT(&nd, LOOKUP, niflag, segflg, fnamep);
#else
    NDINIT(&nd, LOOKUP, niflag, segflg, fnamep, osi_curproc());
#endif
    if ((error = namei(&nd))) {
	goto out;
    }
    *compvpp = nd.ni_vp;
out:
#if defined(AFS_NBSD60_ENV)
    pathbuf_destroy(ipb);
#endif
    return error;
}

int
afs_start(struct mount *mp, int flags)
{
    return (0); /* nothing to do? */
}

void afs_done(void)
{
    return; /* nothing to do? */
}

int
afs_mount(struct mount *mp, const char *path, void *data,
	  size_t *dlen)
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our proc struct. */
    size_t size;

    AFS_STATCNT(afs_mount);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	afs_warn("afs_mount enter\n");
    }

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    if (afs_globalVFS != NULL) {
	/* Don't allow remounts */
	return EBUSY;
    }

    AFS_GLOCK();
#ifdef AFS_DISCON_ENV
    /* initialize the vcache entries before we start using them */

    /* XXX find a better place for this if possible  */
    init_vcache_entries();
#endif
    afs_globalVFS = mp;
    mp->mnt_stat.f_bsize = 8192;
    mp->mnt_stat.f_frsize = 8192;
    mp->mnt_stat.f_iosize = 8192;
    mp->mnt_fs_bshift = DEV_BSHIFT;
    mp->mnt_dev_bshift = DEV_BSHIFT;
    vfs_getnewfsid(mp);
    afs_dynamic_fsid = mp->mnt_stat.f_fsidx;
    (void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1,
		    &size);
    memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
    memset(mp->mnt_stat.f_mntfromname, 0, MNAMELEN);
    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
    /* null terminated string "AFS" will fit, just leave it be. */
    strcpy(mp->mnt_stat.f_fstypename, AFS_MOUNT_AFS);
    AFS_GUNLOCK();
    (void)afs_statvfs(mp, &mp->mnt_stat);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	afs_warn("afs_mount exit\n");
    }

    return 0;
}

int
afs_unmount(struct mount *mp, int mntflags)
{
    AFS_STATCNT(afs_unmount);
#ifdef AFS_DISCON_ENV
    give_up_cbs();
#endif
    if (afs_globalVFS == NULL) {
	printf("afs already unmounted\n");
	return 0;
    }
    if (afs_globalVp)
	vrele(AFSTOV(afs_globalVp));
    afs_globalVp = NULL;

    vflush(mp, NULLVP, 0);	/* don't support forced */
    AFS_GLOCK();
    afs_globalVFS = NULL;
    afs_shutdown(AFS_COLD);
    AFS_GUNLOCK();

    mp->mnt_data = NULL;

    printf("AFS unmounted.\n");
    return 0;
}

#ifndef AFS_NBSD60_ENV
static int
afs_badcall(struct lwp *l, const void *xx, register_t *yy)
{
    return ENOSYS;
}
#endif

int
afs_root(struct mount *mp, struct vnode **vpp)
{
    struct vrequest treq;
    struct vcache *tvp;
    struct vcache *gvp;
    int code;

    AFS_STATCNT(afs_root);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	int glocked = ISAFS_GLOCK();
	afs_warn("afs_root enter, glocked==%d\n", glocked);
    }

    AFS_GLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	afs_warn("afs_root: glocked\n");
    }

    tvp = NULL;
tryagain:
    if (afs_globalVp && (afs_globalVp->f.states & CStatd)) {
	tvp = afs_globalVp;
	code = 0;
    } else {
	if (afs_globalVp) {
	    gvp = afs_globalVp;
	    afs_globalVp = NULL;
	    afs_PutVCache(gvp);
	}

	if (!(code = afs_InitReq(&treq, osi_curcred()))
	    && !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);

	    if (tvp) {
		if (afs_globalVp) {

		    afs_PutVCache(tvp);
		    tvp = NULL;
		    goto tryagain;
		}
		afs_globalVp = tvp;
	    } else
		code = EIO;
	}
    }
    if (tvp) {
	struct vnode *vp = AFSTOV(tvp);
	AFS_GUNLOCK();
	vref(vp);
	if (code != 0) {
		vrele(vp);
		return code;
	}
	if (!VOP_ISLOCKED(*vpp))
	    code = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	AFS_GLOCK();
	if (!afs_globalVp || !(afs_globalVp->f.states & CStatd) ||
	    tvp != afs_globalVp) {
	    vput(vp);
	    afs_PutVCache(tvp);
	    tvp = NULL;
	    goto tryagain;
	}
	if (code != 0)
	    goto tryagain;
	vp->v_vflag |= VV_ROOT;
	if (afs_globalVFS != mp)
	    afs_globalVFS = mp;
	*vpp = vp;
    }
    AFS_GUNLOCK();

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	afs_warn("afs_root exit\n");
    }

    return code;
}

int
afs_statvfs(struct mount *mp, struct statvfs *abp)
{
    AFS_STATCNT(afs_statfs);

    if ((afs_debug & AFSDEB_VNLAYER) != 0) {
	afs_warn("afs_statvfs enter\n");
    }

    /* thank you, NetBSD */
    copy_statvfs_info(abp, mp);

    /* not actually sure which we really must set, but
     * pretty sure of the right values (and in 40, none touched by
     * the above convenience function) */
    abp->f_bsize = mp->osi_vfs_bsize;
    abp->f_frsize = mp->osi_vfs_bsize;
    abp->f_iosize = mp->osi_vfs_bsize;

    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = AFS_VFS_FAKEFREE;

    return (0);
}

int
afs_sync(struct mount *mp, int waitfor, kauth_cred_t cred)
{
    AFS_STATCNT(afs_sync);
#if defined(AFS_DISCON_ENV)
    /* Can't do this in OpenBSD 2.7, it faults when called from apm_suspend() */
    store_dirty_vcaches();
#endif
    return 0;
}

void
afs_init(void)
{
    osi_Init();
    return;
}

void
afs_reinit(void)
{
    return;
}

#ifndef AFS_NBSD60_ENV
/* LKM */

/*
 * declare the filesystem
 */
MOD_VFS("afs", -1, &afs_vfsops);

static int
afs_vfs_load(struct lkm_table *lkmtp, int cmd)
{
    lkmid = lkmtp->id;
    if (sysent[AFS_SYSCALL].sy_call != sys_lkmnosys) {
	printf("LKM afs_vfs_load(): AFS3 syscall %d already used\n",
	    AFS_SYSCALL);
	/* return EEXIST; */
    }

    old_sysent = sysent[AFS_SYSCALL];
    sysent[AFS_SYSCALL] = afs_sysent;
    old_setgroups = sysent[SYS_setgroups];
    sysent[SYS_setgroups].sy_call = Afs_xsetgroups;
#if NOTYET
    old_ioctl = sysent[SYS_ioctl];
    sysent[SYS_ioctl].sy_call = afs_xioctl;
#endif

    aprint_verbose("OpenAFS loaded\n");

    return (0);
}

static int
afs_vfs_unload(struct lkm_table *lktmp, int cmd)
{
    if (afs_globalVp)
	return EBUSY;

    if (sysent[AFS_SYSCALL].sy_call != afs_sysent.sy_call) {
	printf("LKM afs_vfs_load(): AFS3 syscall %d already used\n",
	       AFS_SYSCALL);
	return EEXIST;
    }

    sysent[AFS_SYSCALL] = old_sysent;
    sysent[SYS_setgroups] = old_setgroups;
#if NOTYET
    sysent[SYS_ioctl] = old_ioctl;
#endif
    printf("OpenAFS unloaded\n");

    return (0);
}

int libafs_lkmentry(struct lkm_table *, int, int);

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
#endif
