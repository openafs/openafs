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
#include <sys/lkm.h>

#if 0
/* from /usr/src/sys/kern/vfs_subr.c */
extern void insmntque(struct vnode *, struct mount *);
#endif
extern int sys_lkmnosys(), afs3_syscall(), afs_xioctl(), Afs_xsetgroups();

static int lkmid = -1;
static int afs_badcall(struct lwp *, void *, register_t *);

#if 0
int
newcall(l, v, retval)
	struct lwp *l;
	void *v;
	int *retval;
{
	struct afs_sysargs *uap = v;

	printf("kmod: newcall: %ld %ld %ld %ld\n",
	       SCARG(uap, syscall), SCARG(uap, parm1),
	       SCARG(uap, parm2), SCARG(uap, parm3));
	return(0);
}
#endif

struct sysent afs_sysent = { 6,
			     sizeof(struct afs_sysargs),
			     0,
			     afs3_syscall};

static struct sysent old_sysent;

struct osi_vfs *afs_globalVFS;
struct vcache *afs_globalVp;
fsid_t afs_dynamic_fsid;

int afs_mount(struct mount *, const char *, void *,
	      struct nameidata *, struct lwp *);
int afs_start(struct mount *, int, struct lwp *);
int afs_unmount(struct mount *, int, struct lwp *);
int afs_root(struct mount *, struct vnode **);
int afs_quotactl(struct mount *, int, uid_t, void *, struct lwp *);
int afs_statvfs(struct mount *, struct statvfs *, struct lwp *);
int afs_sync(struct mount *, int, kauth_cred_t, struct lwp *);
int afs_vget(struct mount *, ino_t, struct vnode **);
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
    afs_mount,
    afs_start,
    afs_unmount,
    afs_root,
    afs_quotactl,
    afs_statvfs,
    afs_sync,
    afs_vget,
    (void *) eopnotsupp,	/* vfs_fhtovp */
    (void *) eopnotsupp,	/* vfs_vptofh */
    afs_init,
    afs_reinit,
    afs_done,
    (int (*) (void)) eopnotsupp, /* mountroot */
    (int (*)(struct mount *, struct vnode *, struct timespec *)) eopnotsupp,
    vfs_stdextattrctl,
    afs_vnodeopv_descs,
    0,			/* vfs_refcount */
    { NULL, NULL },
};

VFS_ATTACH(afs_vfsops);

int
afs_nbsd_lookupname(char *fnamep, enum uio_seg segflg, int followlink,
		    struct vnode **compvpp)
{
    struct nameidata nd;
    int niflag;
    int error;

    afs_warn("afs_nbsd_lookupname enter (%s)\n", fnamep);

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
afs_quotactl(struct mount *mp, int cmd, uid_t uid,
    void *arg, struct lwp *l)
{
    return EOPNOTSUPP;
}

int
afs_sysctl()
{
    return EOPNOTSUPP;
}

int
afs_start(struct mount *mp, int flags, struct lwp *l)
{
    return (0); /* nothing to do? */
}

void afs_done(void)
{
    return; /* nothing to do? */
}

int
afs_mount(struct mount *mp, const char *path, void *data,
	  struct nameidata *ndp, struct lwp *l)
{
    /* ndp contains the mounted-from device.  Just ignore it.
     * we also don't care about our proc struct. */
    u_int size;

    AFS_STATCNT(afs_mount);

    afs_warn("afs_mount enter\n");

    if (mp->mnt_flag & MNT_UPDATE)
	return EINVAL;

    if (afs_globalVFS) {
	/* Don't allow remounts */
	return EBUSY;
    }

    AFS_GLOCK();
    /* initialize the vcache entries before we start using them */

    /* XXX find a better place for this if possible  */
    init_vcache_entries();
    afs_globalVFS = mp;
    mp->mnt_stat.f_bsize = 8192;
    mp->mnt_stat.f_frsize = 8192;
    mp->mnt_stat.f_iosize = 8192;
#if 0
    mp->osi_vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    mp->osi_vfs_fsid.val[1] = (int)AFS_VFSFSID;
#else
    vfs_getnewfsid(mp);
    afs_dynamic_fsid = mp->mnt_stat.f_fsidx;
#endif
    (void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1,
		    &size);
    bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
    bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
    strcpy(mp->mnt_stat.f_mntfromname, "AFS");
    /* null terminated string "AFS" will fit, just leave it be. */
    strcpy(mp->mnt_stat.f_fstypename, AFS_MOUNT_AFS);
    AFS_GUNLOCK();
    (void)afs_statvfs(mp, &mp->mnt_stat, l);

    afs_warn("afs_mount exit\n");

    return 0;
}

int
afs_unmount(struct mount *mp, int mntflags, struct lwp *l)
{
    extern int sys_ioctl(), sys_setgroups();

    AFS_STATCNT(afs_unmount);
    give_up_cbs();
    if (afs_globalVFS == NULL) {
	printf("afs already unmounted\n");
	return 0;
    }
    if (afs_globalVp)
	vrele(AFSTOV(afs_globalVp));
    afs_globalVp = NULL;

    vflush(mp, NULLVP, 0);	/* don't support forced */
    mp->mnt_data = NULL;
    AFS_GLOCK();
    afs_globalVFS = 0;
    afs_cold_shutdown = 1;
    afs_shutdown();		/* XXX */
    AFS_GUNLOCK();

    printf
	("AFS unmounted--use `/sbin/modunload -i %d' to unload before restarting AFS\n",
	 lkmid);
    return 0;
}

static int
afs_badcall(struct lwp *l, void *xx, register_t * yy)
{
    return ENOSYS;
}

int
afs_root(struct mount *mp, struct vnode **vpp)
{
    struct vrequest treq;
    struct vcache *tvp;
    int code, glocked;

    AFS_STATCNT(afs_root);

    glocked = ISAFS_GLOCK();
    afs_warn("afs_root enter, glocked==%d\n", glocked);

    AFS_GLOCK();

    afs_warn("glocked\n");

    if (!(code = afs_InitReq(&treq, osi_curcred()))
	&& !(code = afs_CheckInit())) {

	afs_warn("afs_root: initReq && CheckInit: code==%d\n", code);

	tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	afs_warn("afs_root: GetVCache: tvp==%lx\n", tvp);
	if (tvp) {
	    /* There is really no reason to over-hold this bugger--it's held
	     * by the root filesystem reference. */
	    if (afs_globalVp != tvp) {
#ifdef AFS_DONT_OVERHOLD_GLOBALVP
		if (afs_globalVp)
		    AFS_RELE(AFSTOV(afs_globalVp));
#endif
		afs_globalVp = tvp;
		VREF(AFSTOV(afs_globalVp));
	    }
	    AFSTOV(tvp)->v_flag |= VROOT;
	    afs_globalVFS = mp;
	    *vpp = AFSTOV(tvp);
	} else
	    code = ENOENT;
    }
    AFS_GUNLOCK();

    afs_warn("afs_root: gunlocked\n");

    if (!code) {
	if (!VOP_ISLOCKED(*vpp))
	    vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY); /* return it locked */
    }

    afs_warn("afs_root exit\n");

    return code;
}

int
afs_statvfs(struct mount *mp, struct statvfs *abp, struct lwp *l)
{
    AFS_STATCNT(afs_statfs);

    afs_warn("afs_statvfs enter\n");

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
afs_sync(struct mount *mp, int waitfor, kauth_cred_t cred, struct lwp *l)
{
    AFS_STATCNT(afs_sync);
    /* Can't do this in OpenBSD 2.7, it faults when called from apm_suspend() */
    store_dirty_vcaches();
    return 0;
}

int
afs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
    return (EOPNOTSUPP);
}

void
afs_init()
{
    osi_Init();
    return;
}

void
afs_reinit(void)
{
    return;
}

/* LKM */

/*
 * declare the filesystem
 */
MOD_VFS("afs", -1, &afs_vfsops);

#if 0
static char afsgenmem[] = "afsgenmem";
static char afsfidmem[] = "afsfidmem";
static char afsbhdrmem[] = "afsbhdrmem";
static char afsbfrmem[] = "afsbfrmem";
#endif

int
afs_vfs_load(struct lkm_table *lkmtp, int cmd)
{
#if 0
    extern char *memname[];

    if (memname[M_AFSGENERIC] == NULL)
	memname[M_AFSGENERIC] = afsgenmem;
    if (memname[M_AFSFID] == NULL)
	memname[M_AFSFID] = afsfidmem;
    if (memname[M_AFSBUFHDR] == NULL)
	memname[M_AFSBUFHDR] = afsbhdrmem;
    if (memname[M_AFSBUFFER] == NULL)
	memname[M_AFSBUFFER] = afsbfrmem;
#endif
    lkmid = lkmtp->id;

    if (sysent[AFS_SYSCALL].sy_call != sys_nosys) {
	printf("LKM afs_vfs_load(): AFS3 syscall %d already used\n",
	       AFS_SYSCALL);
	/* return EEXIST; */
    }

    old_sysent = sysent[AFS_SYSCALL];
    sysent[AFS_SYSCALL] = afs_sysent;

    printf("OpenAFS lkm loaded\n");

    return (0);
}

int
afs_vfs_unload(struct lkm_table *lktmp, int cmd)
{
#if 0
    extern char *memname[];
#endif

    if (afs_globalVp)
	return EBUSY;
#if 0
    if (memname[M_AFSGENERIC] == afsgenmem)
	memname[M_AFSGENERIC] = NULL;
    if (memname[M_AFSFID] == afsfidmem)
	memname[M_AFSFID] = NULL;
    if (memname[M_AFSBUFHDR] == afsbhdrmem)
	memname[M_AFSBUFHDR] = NULL;
    if (memname[M_AFSBUFFER] == afsbfrmem)
	memname[M_AFSBUFFER] = NULL;
#endif

    if (sysent[AFS_SYSCALL].sy_call != afs_sysent.sy_call) {
	printf("LKM afs_vfs_load(): AFS3 syscall %d already used\n",
	       AFS_SYSCALL);
	return EEXIST;
    }

    sysent[AFS_SYSCALL] = old_sysent;
    printf("OpenAFS unloaded\n");

    return (0);
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
