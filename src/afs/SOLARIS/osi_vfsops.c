/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_vfsops.c for SOLARIS
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */
#include "h/modctl.h"
#include "h/syscall.h"
#if defined(AFS_SUN511_ENV)
#include <sys/vfs_opreg.h>
#endif
#include <sys/kobj.h>

#include <sys/mount.h>

struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;

#if defined(AFS_SUN5_64BIT_ENV)
extern struct sysent sysent32[];
#endif

int afsfstype = 0;

int
afs_mount(struct vfs *afsp, struct vnode *amvp, struct mounta *uap,
	  afs_ucred_t *credp)
{

    AFS_GLOCK();

    AFS_STATCNT(afs_mount);

#if defined(AFS_SUN510_ENV)
    if (secpolicy_fs_mount(credp, amvp, afsp) != 0) {
#else
    if (!afs_osi_suser(credp)) {
#endif
	AFS_GUNLOCK();
	return (EPERM);
    }
    afsp->vfs_fstype = afsfstype;

    if (afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (EBUSY);
    }

    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC;	/* magic */
    afsp->vfs_fsid.val[1] = AFS_VFSFSID;
    afsp->vfs_dev = AFS_VFSMAGIC;

    AFS_GUNLOCK();
    return 0;
}

static void
afs_freevfs(void)
{

    afs_globalVFS = 0;

    afs_shutdown(AFS_WARM);
}

int
afs_unmount(struct vfs *afsp, int flag, afs_ucred_t *credp)
{
    struct vcache *rootvp = NULL;

    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

#if defined(AFS_SUN510_ENV)
    if (secpolicy_fs_unmount(credp, afsp) != 0) {
#else
    if (!afs_osi_suser(credp)) {
#endif
        AFS_GUNLOCK();
        return (EPERM);
    }

    if (flag & MS_FORCE) {
	AFS_GUNLOCK();
	return ENOTSUP;
    }

    /* We should have one reference from the caller, and one reference for the
     * root vnode; any more and someone is still referencing something */
    if (afsp->vfs_count > 2) {
	AFS_GUNLOCK();
	return EBUSY;
    }

    /* The root vnode should have one ref for the mount; any more, and someone
     * else is using the root vnode */
    if (afs_globalVp && VREFCOUNT_GT(afs_globalVp, 1)) {
	AFS_GUNLOCK();
	return EBUSY;
    }

    afsp->vfs_flag |= VFS_UNMOUNTED;

    if (afs_globalVp) {
	/* release the root vnode, which should be the last reference to us
	 * besides the caller of afs_unmount */
	rootvp = afs_globalVp;
	afs_globalVp = NULL;
	AFS_RELE(AFSTOV(rootvp));
    }

    AFS_GUNLOCK();
    return 0;
}

void
gafs_freevfs(struct vfs *afsp)
{
    AFS_GLOCK();

    afs_freevfs();

    AFS_GUNLOCK();
}

int
afs_root(struct vfs *afsp, struct vnode **avpp)
{
    afs_int32 code = 0;
    struct vrequest treq;
    struct vcache *tvp = 0;
    struct vcache *gvp;
    struct proc *proc = ttoproc(curthread);
    struct vnode *vp = afsp->vfs_vnodecovered;
    int locked = 0;

    /* Potential deadlock:
     * afs_root is called with the Vnode's v_lock locked. Set VVFSLOCK
     * and drop the v_lock if we need to make an RPC to complete this
     * request. There used to be a deadlock on the global lock until
     * we stopped calling iget while holding the global lock.
     */

    AFS_GLOCK();

    AFS_STATCNT(afs_root);

again:
    if (afs_globalVp && (afs_globalVp->f.states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (MUTEX_HELD(&vp->v_lock)) {
	    vp->v_flag |= VVFSLOCK;
	    locked = 1;
	    mutex_exit(&vp->v_lock);
	}

	if (afs_globalVp) {
	    gvp = afs_globalVp;
	    afs_globalVp = NULL;
	    afs_PutVCache(gvp);
	}

	if (!(code = afs_InitReq(&treq, proc->p_cred))
	    && !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq);
	    /* we really want this to stay around */
	    if (tvp) {
		if (afs_globalVp) {
		    /* someone else got there before us! */
		    afs_PutVCache(tvp);
		    tvp = 0;
		    goto again;
		}
		afs_globalVp = tvp;
	    } else
		code = EIO;
	}
    }
    if (tvp) {
	osi_Assert(osi_vnhold(tvp) == 0);
	mutex_enter(&AFSTOV(tvp)->v_lock);
	AFSTOV(tvp)->v_flag |= VROOT;
	mutex_exit(&AFSTOV(tvp)->v_lock);

	afs_globalVFS = afsp;
	*avpp = AFSTOV(tvp);
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, code);

    AFS_GUNLOCK();
    if (locked) {
	mutex_enter(&vp->v_lock);
	vp->v_flag &= ~VVFSLOCK;
	if (vp->v_flag & VVFSWAIT) {
	    vp->v_flag &= ~VVFSWAIT;
	    cv_broadcast(&vp->v_cv);
	}
    }

    return code;
}

int
afs_statvfs(struct vfs *afsp, struct statvfs64 *abp)
{
    AFS_GLOCK();

    AFS_STATCNT(afs_statfs);

    abp->f_frsize = 1024;
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_favail = abp->f_ffree = AFS_VFS_FAKEFREE;
    abp->f_fsid = (AFS_VFSMAGIC << 16) | AFS_VFSFSID;

    AFS_GUNLOCK();
    return 0;
}

int
afs_sync(struct vfs *afsp, short flags, afs_ucred_t *credp)
{
    return 0;
}

int
afs_vget(struct vfs *afsp, struct vnode **avcp, struct fid *fidp)
{
    cred_t *credp = CRED();
    struct vrequest treq;
    int code;

    AFS_GLOCK();

    AFS_STATCNT(afs_vget);

    *avcp = NULL;
    if (!(code = afs_InitReq(&treq, credp))) {
        struct vcache *tvc = NULL;
	code = afs_osi_vget(&tvc, fidp, &treq);
        if (tvc) {
            *avcp = AFSTOV(tvc);
        }
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_VGET, ICL_TYPE_POINTER, *avcp,
	       ICL_TYPE_INT32, treq.uid, ICL_TYPE_FID, fidp);
    code = afs_CheckCode(code, &treq, 42);

    AFS_GUNLOCK();
    return code;
}

/* This is only called by vfs_mount when afs is going to be mounted as root.
 * Since we don't support diskless clients we shouldn't come here.
 */
int afsmountroot = 0;
afs_mountroot(struct vfs *afsp, whymountroot_t why)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_mountroot);
    afsmountroot++;
    AFS_GUNLOCK();
    return EINVAL;
}

/* afs_swapvp is called to setup swapping over the net for diskless clients.
 * Again not for us.
 */
int afsswapvp = 0;
afs_swapvp(struct vfs *afsp, struct vnode **avpp, char *nm)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_swapvp);
    afsswapvp++;
    AFS_GUNLOCK();
    return EINVAL;
}


#if defined(AFS_SUN511_ENV)
/* The following list must always be NULL-terminated */
static const fs_operation_def_t afs_vfsops_template[] = {
    VFSNAME_MOUNT,		{ .vfs_mount = afs_mount },
    VFSNAME_UNMOUNT,		{ .vfs_unmount = afs_unmount },
    VFSNAME_ROOT,		{ .vfs_root = afs_root },
    VFSNAME_STATVFS,		{ .vfs_statvfs = afs_statvfs },
    VFSNAME_SYNC,		{ .vfs_sync = afs_sync },
    VFSNAME_VGET,		{ .vfs_vget = afs_vget },
    VFSNAME_MOUNTROOT,  	{ .vfs_mountroot = afs_mountroot },
    VFSNAME_FREEVFS,		{ .vfs_freevfs = gafs_freevfs },
    NULL,			NULL
};
struct vfsops *afs_vfsopsp;
#elif defined(AFS_SUN510_ENV)
/* The following list must always be NULL-terminated */
const fs_operation_def_t afs_vfsops_template[] = {
    VFSNAME_MOUNT,		afs_mount,
    VFSNAME_UNMOUNT,		afs_unmount,
    VFSNAME_ROOT,		afs_root,
    VFSNAME_STATVFS,		afs_statvfs,
    VFSNAME_SYNC,		afs_sync,
    VFSNAME_VGET,		afs_vget,
    VFSNAME_MOUNTROOT,  	afs_mountroot,
    VFSNAME_FREEVFS,		gafs_freevfs,
    NULL,			NULL
};
struct vfsops *afs_vfsopsp;
#else
struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statvfs,
    afs_sync,
    afs_vget,
    afs_mountroot,
    afs_swapvp,
    gafs_freevfs,
};
#endif


/*
 * afsinit - intialize VFS
 */
int (*ufs_iallocp) ();
void (*ufs_iupdatp) ();
int (*ufs_igetp) ();
void (*ufs_itimes_nolockp) ();

int (*afs_orig_ioctl) (), (*afs_orig_ioctl32) ();
int (*afs_orig_setgroups) (), (*afs_orig_setgroups32) ();

#ifndef AFS_SUN510_ENV
struct ill_s *ill_g_headp = 0;
#endif

int afs_sinited = 0;

extern struct fs_operation_def afs_vnodeops_template[];

#if	!defined(AFS_NONFSTRANS)
int (*nfs_rfsdisptab_v2) ();
int (*nfs_rfsdisptab_v3) ();
int (*nfs_acldisptab_v2) ();
int (*nfs_acldisptab_v3) ();

int (*nfs_checkauth) ();
#endif

extern Afs_syscall();

static void *
do_mod_lookup(const char * mod, const char * sym)
{
    void * ptr;

    ptr = modlookup(mod, sym);
    if (ptr == NULL) {
        afs_warn("modlookup failed for symbol '%s' in module '%s'\n",
		 sym, mod);
    }

    return ptr;
}

#ifdef AFS_SUN510_ENV
afsinit(int fstype, char *dummy)
#else
afsinit(struct vfssw *vfsswp, int fstype)
#endif
{
    extern int afs_xsetgroups();

    AFS_STATCNT(afsinit);

    afs_orig_setgroups = sysent[SYS_setgroups].sy_callc;
    afs_orig_ioctl = sysent[SYS_ioctl].sy_call;
    sysent[SYS_setgroups].sy_callc = afs_xsetgroups;
    sysent[SYS_ioctl].sy_call = afs_xioctl;

#if defined(AFS_SUN5_64BIT_ENV)
    afs_orig_setgroups32 = sysent32[SYS_setgroups].sy_callc;
    afs_orig_ioctl32 = sysent32[SYS_ioctl].sy_call;
    sysent32[SYS_setgroups].sy_callc = afs_xsetgroups;
    sysent32[SYS_ioctl].sy_call = afs_xioctl;
#endif /* AFS_SUN5_64BIT_ENV */

#ifdef AFS_SUN510_ENV
    vfs_setfsops(fstype, afs_vfsops_template, &afs_vfsopsp);
    afsfstype = fstype;
    vn_make_ops("afs", afs_vnodeops_template, &afs_ops);
#else /* !AFS_SUN510_ENV */
    vfsswp->vsw_vfsops = &Afs_vfsops;
    afsfstype = fstype;
#endif /* !AFS_SUN510_ENV */


#if !defined(AFS_NONFSTRANS)
    nfs_rfsdisptab_v2 = (int (*)()) do_mod_lookup("nfssrv", "rfsdisptab_v2");
    if (nfs_rfsdisptab_v2 != NULL) {
	nfs_acldisptab_v2 = (int (*)()) do_mod_lookup("nfssrv", "acldisptab_v2");
	if (nfs_acldisptab_v2 != NULL) {
	    afs_xlatorinit_v2(nfs_rfsdisptab_v2, nfs_acldisptab_v2);
	}
    }
    nfs_rfsdisptab_v3 = (int (*)()) do_mod_lookup("nfssrv", "rfsdisptab_v3");
    if (nfs_rfsdisptab_v3 != NULL) {
	nfs_acldisptab_v3 = (int (*)()) do_mod_lookup("nfssrv", "acldisptab_v3");
	if (nfs_acldisptab_v3 != NULL) {
	    afs_xlatorinit_v3(nfs_rfsdisptab_v3, nfs_acldisptab_v3);
	}
    }

    nfs_checkauth = (int (*)()) do_mod_lookup("nfssrv", "checkauth");
#endif /* !AFS_NONFSTRANS */

    ufs_iallocp = (int (*)()) do_mod_lookup("ufs", "ufs_ialloc");
    ufs_iupdatp = (void (*)()) do_mod_lookup("ufs", "ufs_iupdat");
    ufs_igetp = (int (*)()) do_mod_lookup("ufs", "ufs_iget");
    ufs_itimes_nolockp = (void (*)()) do_mod_lookup("ufs", "ufs_itimes_nolock");

    if (!ufs_iallocp || !ufs_iupdatp || !ufs_itimes_nolockp || !ufs_igetp) {
	afs_warn("AFS to UFS mapping cannot be fully initialised\n");
    }

#if !defined(AFS_SUN510_ENV)
    ill_g_headp = (struct ill_s *) do_mod_lookup("ip", "ill_g_head");
#endif /* !AFS_SUN510_ENV */

    afs_sinited = 1;
    return 0;

}

#ifdef AFS_SUN510_ENV
#ifdef AFS_SUN511_ENV
static vfsdef_t afs_vfsdef = {
    VFSDEF_VERSION,
    "afs",
    afsinit,
    VSW_STATS,
    NULL
};
#else
static struct vfsdef_v3 afs_vfsdef = {
    VFSDEF_VERSION,
    "afs",
    afsinit,
    VSW_STATS
};
#endif
#else
static struct vfssw afs_vfw = {
    "afs",
    afsinit,
    &Afs_vfsops,
    0
};
#endif

#ifndef AFS_SUN511_ENV
static struct sysent afssysent = {
    6,
    0,
    Afs_syscall
};
#endif /* AFS_SUN511_ENV */

/*
 * Info/Structs to link the afs module into the kernel
 */
extern struct mod_ops mod_fsops;

static struct modlfs afsmodlfs = {
    &mod_fsops,
    "afs filesystem",
#ifdef AFS_SUN510_ENV
    &afs_vfsdef
#else
    &afs_vfw
#endif
};

#ifdef AFS_SUN511_ENV

extern struct modldrv afs_modldrv;

#else /* AFS_SUN511_ENV */

extern struct mod_ops mod_syscallops;
static struct modlsys afsmodlsys = {
    &mod_syscallops,
    "afs syscall interface",
    &afssysent
};

/** The two structures afssysent32 and afsmodlsys32 are being added
  * for supporting 32 bit syscalls. In Solaris 7 there are two system
  * tables viz. sysent ans sysent32. 32 bit applications use sysent32.
  * Since most of our user space binaries are going to be 32 bit
  * we need to attach to sysent32 also. Note that the entry into AFS
  * land still happens through Afs_syscall irrespective of whether we
  * land here from sysent or sysent32
  */

# if defined(AFS_SUN5_64BIT_ENV)
extern struct mod_ops mod_syscallops32;

static struct modlsys afsmodlsys32 = {
    &mod_syscallops32,
    "afs syscall interface(32 bit)",
    &afssysent
};
# endif
#endif /* !AFS_SUN511_ENV */


static struct modlinkage afs_modlinkage = {
    MODREV_1,
    (void *)&afsmodlfs,
#ifdef AFS_SUN511_ENV
    (void *)&afs_modldrv,
#else
    (void *)&afsmodlsys,
# ifdef AFS_SUN5_64BIT_ENV
    (void *)&afsmodlsys32,
# endif
#endif /* !AFS_SUN511_ENV */
    NULL
};

static void
reset_sysent(void)
{
    if (afs_sinited) {
	sysent[SYS_setgroups].sy_callc = afs_orig_setgroups;
	sysent[SYS_ioctl].sy_call = afs_orig_ioctl;
#if defined(AFS_SUN5_64BIT_ENV)
	sysent32[SYS_setgroups].sy_callc = afs_orig_setgroups32;
	sysent32[SYS_ioctl].sy_call = afs_orig_ioctl32;
#endif
    }
}

/** This is the function that modload calls when loading the afs kernel
  * extensions. The solaris modload program searches for the _init
  * function in a module and calls it when modloading
  */

_init()
{
    char *sysn, *mod_getsysname();
    int code;
    extern char *sysbind;
    extern struct bind *sb_hashtab[];
    struct modctl *mp = 0;

    if (afs_sinited)
	return EBUSY;

    if ((!(mp = mod_find_by_filename("fs", "ufs"))
	 && !(mp = mod_find_by_filename(NULL, "/kernel/fs/ufs"))
	 && !(mp = mod_find_by_filename(NULL, "sys/ufs"))) || (mp
							       && !mp->
							       mod_installed))
    {
	printf
	    ("ufs module must be loaded before loading afs; use modload /kernel/fs/ufs\n");
	return (ENOSYS);
    }
#ifndef	AFS_NONFSTRANS
    if ((!(mp = mod_find_by_filename("misc", "nfssrv"))
	 && !(mp = mod_find_by_filename(NULL, NFSSRV))
	 && !(mp = mod_find_by_filename(NULL, NFSSRV_V9))
	 && !(mp = mod_find_by_filename(NULL, NFSSRV_AMD64))) || (mp
							       && !mp->
							       mod_installed))
    {
	printf
	    ("misc/nfssrv module must be loaded before loading afs with nfs-xlator\n");
	return (ENOSYS);
    }
#endif /* !AFS_NONFSTRANS */


    osi_Init();			/* initialize global lock, etc */

    code = mod_install(&afs_modlinkage);
    if (code) {
	/* we failed to load, so make sure we don't leave behind any
	 * references to our syscall handlers */
	reset_sysent();
    }
    return code;
}

_info(modp)
     struct modinfo *modp;
{
    int code;

    code = mod_info(&afs_modlinkage, modp);
    return code;
}

_fini()
{
    int code;

    if (afs_globalVFS)
	return EBUSY;

    reset_sysent();
    code = mod_remove(&afs_modlinkage);
    return code;
}
