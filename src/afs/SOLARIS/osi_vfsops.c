/* Copyright (C) 1995, 1998 Transarc Corporation - All rights reserved. */
/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 * osi_vfsops.c for SOLARIS
 */
#include "../afs/param.h"	/* Should be always first */
#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* statistics stuff */
#include "../h/modctl.h"
#include "../h/syscall.h"
#include <sys/kobj.h>



struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;

#if defined(AFS_SUN57_64BIT_ENV)
extern struct sysent sysent32[];
#endif

int afsfstype = 0;

int afs_mount(struct vfs *afsp, struct vnode *amvp, struct mounta *uap,
	  struct AFS_UCRED *credp)
{

    AFS_GLOCK();

    AFS_STATCNT(afs_mount);

    if (!suser(credp))
	return EPERM;
    afsp->vfs_fstype = afsfstype;

    if (afs_globalVFS) { /* Don't allow remounts. */
	return EBUSY;
    }

    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid.val[0] = AFS_VFSMAGIC; /* magic */
    afsp->vfs_fsid.val[1] = AFS_VFSFSID; 

    AFS_GUNLOCK();
    return 0;
}

int afs_unmount (struct vfs *afsp, struct AFS_UCRED *credp)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    if (!suser(credp))
	return EPERM;
    afs_globalVFS = 0;
    afs_shutdown();

    AFS_GUNLOCK();
    return 0;
}

int afs_root (struct vfs *afsp, struct vnode **avpp)
{
    register afs_int32 code = 0;
    struct vrequest treq;
    register struct vcache *tvp=0;
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

    if (afs_globalVp && (afs_globalVp->states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (MUTEX_HELD(&vp->v_lock)) {
	    vp->v_flag |= VVFSLOCK;
	    locked = 1;
	    mutex_exit(&vp->v_lock);
	}
	if (!(code = afs_InitReq(&treq, proc->p_cred)) &&
	    !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, (afs_int32 *)0,
				(struct vcache*)0, WRITE_LOCK);
	    /* we really want this to stay around */
	    if (tvp) {
		afs_globalVp = tvp;
	    } else
		code = ENOENT;
	}
    }
    if (tvp) {
	VN_HOLD((struct vnode *)tvp);
	mutex_enter(&(((struct vnode*)tvp)->v_lock));
	tvp->v.v_flag |= VROOT;
	mutex_exit(&(((struct vnode*)tvp)->v_lock));

	afs_globalVFS = afsp;
	*avpp = (struct vnode *) tvp;
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

#ifdef AFS_SUN56_ENV
int afs_statvfs(struct vfs *afsp, struct statvfs64 *abp)
#else
int afs_statvfs(struct vfs *afsp, struct statvfs *abp)
#endif
{
    AFS_GLOCK();

    AFS_STATCNT(afs_statfs);

    abp->f_frsize = 1024;
    abp->f_favail =  9000000;
    abp->f_bsize = afsp->vfs_bsize;
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree  = 9000000;
    abp->f_fsid = (AFS_VFSMAGIC << 16) || AFS_VFSFSID;

    AFS_GUNLOCK();
    return 0;
}

int afs_sync(struct vfs *afsp, short flags, struct AFS_UCRED *credp)
{
    return 0;
}

int afs_vget(struct vfs *afsp, struct vnode **avcp, struct fid *fidp)
{
    cred_t *credp = CRED();
    struct vrequest treq;
    int code;

    AFS_GLOCK();

    AFS_STATCNT(afs_vget);

    *avcp = NULL;
    if (!(code = afs_InitReq(&treq, credp))) {
	code = afs_osi_vget((struct vcache**)avcp, fidp, &treq);
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
int afsmountroot=0;
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
int afsswapvp=0;
afs_swapvp(struct vfs *afsp, struct vnode **avpp, char *nm)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_swapvp);
    afsswapvp++;
    AFS_GUNLOCK();
    return EINVAL;
}


struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statvfs,
    afs_sync,
    afs_vget,
    afs_mountroot,
    afs_swapvp
};


/*
 * afsinit - intialize VFS
 */
void (*ufs_iputp)();
int (*ufs_iallocp)();
void (*ufs_iupdatp)();
int (*ufs_igetp)();

struct streamtab *udp_infop = 0;
struct ill_s *ill_g_headp = 0;

int afs_sinited = 0;

#if	!defined(AFS_NONFSTRANS)
int (*nfs_rfsdisptab_v2)();
int (*nfs_rfsdisptab_v3)();
int (*nfs_acldisptab_v2)();
int (*nfs_acldisptab_v3)();

int (*nfs_checkauth)();
#endif

extern Afs_syscall();

afsinit(struct vfssw *vfsswp, int fstype)
{
    extern int afs_xioctl(), afs_xflock();
    extern int afs_xsetgroups();

    AFS_STATCNT(afsinit);

    sysent[SYS_setgroups].sy_callc = afs_xsetgroups;
    sysent[SYS_ioctl].sy_call = afs_xioctl;

#if defined(AFS_SUN57_64BIT_ENV)
    sysent32[SYS_setgroups].sy_callc = afs_xsetgroups;
    sysent32[SYS_ioctl].sy_call = afs_xioctl;
#endif

    vfsswp->vsw_vfsops = &Afs_vfsops;
    afsfstype = fstype;


#if	!defined(AFS_NONFSTRANS)
    nfs_rfsdisptab_v2 = (int (*)()) modlookup("nfssrv", "rfsdisptab_v2");
    if ( !nfs_rfsdisptab_v2 ) {
	afs_warn("warning : rfsdisptab_v2 NOT FOUND\n");
    }
    if (nfs_rfsdisptab_v2) {
	nfs_acldisptab_v2 = (int (*)()) modlookup("nfssrv", "acldisptab_v2");
	if ( !nfs_acldisptab_v2 ) {
	    afs_warn("warning : acldisptab_v2 NOT FOUND\n");
	}
	else {
	    afs_xlatorinit_v2(nfs_rfsdisptab_v2, nfs_acldisptab_v2);
	}
    }
    nfs_rfsdisptab_v3 = (int (*)()) modlookup("nfssrv", "rfsdisptab_v3");
    if ( !nfs_rfsdisptab_v3 ) {
	afs_warn("warning : rfsdisptab_v3 NOT FOUND\n");
    }
    if (nfs_rfsdisptab_v3) {
	nfs_acldisptab_v3 = (int (*)()) modlookup("nfssrv", "acldisptab_v3");
	if ( !nfs_acldisptab_v3 ) {
	    afs_warn("warning : acldisptab_v3 NOT FOUND\n");
	}
	else {
	    afs_xlatorinit_v3(nfs_rfsdisptab_v3, nfs_acldisptab_v3);
	}
    }

    nfs_checkauth = (int (*)()) modlookup("nfssrv", "checkauth");
    if ( !nfs_checkauth ) afs_warn("nfs_checkauth not initialised");
#endif
    ufs_iputp = (int (*)()) modlookup("ufs", "ufs_iput");    
    ufs_iallocp = (int (*)()) modlookup("ufs", "ufs_ialloc");    
    ufs_iupdatp = (int (*)()) modlookup("ufs", "ufs_iupdat");
    ufs_igetp = (int (*)()) modlookup("ufs", "ufs_iget");    
    udp_infop = (struct streamtab *) modlookup("udp", "udpinfo");    
    ill_g_headp = (struct ill_s *) modlookup("ip", "ill_g_head");    

    if ( !ufs_iputp || !ufs_iallocp || !ufs_iupdatp ||
	 !ufs_igetp || !udp_infop || !ill_g_headp )
	afs_warn("AFS to UFS mapping cannot be fully initialised\n");

    afs_sinited = 1;
    return 0;

}

static struct vfssw afs_vfw = {
    "afs",
    afsinit,
    &Afs_vfsops,
    0
    };

static struct sysent afssysent = {
    6,
    0,
    Afs_syscall
    };

/* inter-module dependencies */
char _depends_on[] = "drv/ip drv/udp strmod/rpcmod";

/*
 * Info/Structs to link the afs module into the kernel
 */
extern struct mod_ops mod_fsops;
extern struct mod_ops mod_syscallops;

static struct modlfs afsmodlfs = {
    &mod_fsops,
    "afs filesystem",
    &afs_vfw
    };

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

#if defined(AFS_SUN57_64BIT_ENV)
extern struct mod_ops mod_syscallops32;

static struct modlsys afsmodlsys32 = {
    &mod_syscallops32,
    "afs syscall interface(32 bit)",
    &afssysent
};
#endif


static struct modlinkage afs_modlinkage = {
    MODREV_1,
    (void *)&afsmodlsys,
#ifdef AFS_SUN57_64BIT_ENV
    (void *)&afsmodlsys32,
#endif
    (void *)&afsmodlfs,
    NULL
    };

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

    if ((!(mp = mod_find_by_filename("fs", "ufs")) && 
	!(mp = mod_find_by_filename(NULL, "/kernel/fs/ufs")) &&
	!(mp = mod_find_by_filename(NULL, "sys/ufs"))) ||
	(mp && !mp->mod_installed)) {
	printf("ufs module must be loaded before loading afs; use modload /kernel/fs/ufs\n");
	return (ENOSYS);
    }
#ifndef	AFS_NONFSTRANS
#if     defined(AFS_SUN55_ENV)
    if ((!(mp = mod_find_by_filename("misc", "nfssrv")) &&
	 !(mp = mod_find_by_filename(NULL, NFSSRV)) &&
	 !(mp = mod_find_by_filename(NULL, NFSSRV_V9))) || 
	(mp && !mp->mod_installed)) {
        printf("misc/nfssrv module must be loaded before loading afs with nfs-xlator\n");
        return (ENOSYS);
    }
#else
#if	defined(AFS_SUN52_ENV)
    if ((!(mp = mod_find_by_filename("fs", "nfs")) && 
	!(mp = mod_find_by_filename(NULL, "/kernel/fs/nfs")) &&
	!(mp = mod_find_by_filename(NULL, "sys/nfs"))) ||
	(mp && !mp->mod_installed)) {
	printf("fs/nfs module must be loaded before loading afs with nfs-xlator\n");
	return (ENOSYS);
    }
#endif
#endif
#endif
    /* 
     * Re-read the /etc/name_to_sysnum file to make sure afs isn't added after
     * reboot.  Ideally we would like to call modctl_read_sysbinding_file() but
     * unfortunately in Solaris 2.2 it became a local function so we have to do
     * the read_binding_file() direct call with the appropriate text file and
     * system call hashtable.  make_syscallname actually copies "afs" to the
     * proper slot entry and we also actually have to properly initialize the
     * global sysent[AFS_SYSCALL] entry!
     */
#ifdef	AFS_SUN53_ENV
#ifndef	SYSBINDFILE
#define	SYSBINDFILE	"/etc/name_to_sysnum"
#endif
    read_binding_file(SYSBINDFILE, sb_hashtab);
#else
    read_binding_file(sysbind, sb_hashtab);
#endif
    make_syscallname("afs", AFS_SYSCALL);

    if (sysent[AFS_SYSCALL].sy_call == nosys) {
	if ((sysn = mod_getsysname(AFS_SYSCALL)) != NULL) {
	    sysent[AFS_SYSCALL].sy_lock =
		(krwlock_t *) kobj_zalloc(sizeof (krwlock_t), KM_SLEEP);
	    rw_init(sysent[AFS_SYSCALL].sy_lock, "afs_syscall",
#ifdef AFS_SUN57_ENV
		    RW_DEFAULT, NULL);
#else
			RW_DEFAULT, DEFAULT_WT);
#endif	
	}
    }

    osi_Init();				/* initialize global lock, etc */

    code = mod_install(&afs_modlinkage);
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

    if (afs_sinited)
	return (EBUSY);
    code = mod_remove(&afs_modlinkage);
    return code;
}
