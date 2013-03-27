/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * osi_vfsops.c for HPUX
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* statistics stuff */
#include <sys/scall_kernprivate.h>

#if defined(AFS_HPUX1123_ENV)
#include <sys/moddefs.h>
#endif /* AFS_HPUX1123_ENV */

#if defined(AFS_HPUX1123_ENV)
/* defind DLKM tables  so we can load dynamicly */
/* we still need an afs_unload to unload */
/* Note: There is to be a dependency on the
 * the name of the struct <name>_wrapper, and the 
 * name of the dynamicly loaded file <name>  
 * We will define -DAFS_WRAPPER=<name>_wrapper
 * and -DAFS_CONF_DATA=<name>_conf_data  and pass into
 * this routine 
 */ 

extern struct mod_operations mod_misc_ops;
extern struct mod_conf_data AFS_CONF_DATA;

static int afs_load(void *arg);
/* static int afs_unload(void *arg); */

struct mod_type_data afs_mod_link = {
	"AFS kernel module", 
	NULL
};

struct modlink afs_modlink[] = {
	{&mod_misc_ops, &afs_mod_link},
	{ NULL, NULL }
};

struct modwrapper AFS_WRAPPER = {
	MODREV, 
	afs_load, 
	NULL,      /* should be afs_unload if we had one */ 
	NULL,
	&AFS_CONF_DATA, 
	afs_modlink 
}; 

#endif /* AFS_HPUX1123_ENV */

static char afs_mountpath[512];
struct vfs *afs_globalVFS = 0;
struct vcache *afs_globalVp = 0;

int
afs_mount(struct vfs *afsp, char *path, smountargs_t * data)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_mount);

    if (afs_globalVFS) {	/* Don't allow remounts. */
	AFS_GUNLOCK();
	return (setuerror(EBUSY));
    }

    afs_globalVFS = afsp;
    afsp->vfs_bsize = 8192;
    afsp->vfs_fsid[0] = AFS_VFSMAGIC;	/* magic */
    afsp->vfs_fsid[1] = AFS_VFSFSID;
    strcpy(afsp->vfs_name, "AFS");
    afsp->vfs_name[3] = '\0';

    strncpy(afs_mountpath, path, sizeof(afs_mountpath));
    afs_mountpath[sizeof afs_mountpath - 1] = '\0';

#ifndef	AFS_NONFSTRANS
    /* Set up the xlator in case it wasn't done elsewhere */
    afs_xlatorinit_v2();
#endif

    AFS_GUNLOCK();
    return 0;
}


int
afs_unmount(struct vfs *afsp)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_unmount);

    afs_globalVFS = 0;
    afs_shutdown();

    AFS_GUNLOCK();
    return 0;
}

int
afs_root(struct vfs *afsp, struct vnode **avpp, char *unused1)
{
    int code = 0;
    struct vrequest treq;
    struct vcache *tvp = 0;
    AFS_GLOCK();
    AFS_STATCNT(afs_root);

    if (afs_globalVp && (afs_globalVp->f.states & CStatd)) {
	tvp = afs_globalVp;
    } else {
	if (afs_globalVp) {
	    afs_PutVCache(afs_globalVp);
	    afs_globalVp = NULL;
	}

	if (!(code = afs_InitReq(&treq, p_cred(u.u_procp)))
	    && !(code = afs_CheckInit())) {
	    tvp = afs_GetVCache(&afs_rootFid, &treq, NULL, NULL);
	    /* we really want this to stay around */
	    if (tvp) {
		afs_globalVp = tvp;
	    } else
		code = ENOENT;
	}
    }
    if (tvp) {
	VN_HOLD(AFSTOV(tvp));
	SET_V_FLAG(AFSTOV(tvp), VROOT);

	afs_globalVFS = afsp;
	*avpp = AFSTOV(tvp);
    }

    afs_Trace2(afs_iclSetp, CM_TRACE_VFSROOT, ICL_TYPE_POINTER, *avpp,
	       ICL_TYPE_INT32, code);

    AFS_GUNLOCK();
    return code;
}

int
afs_statfs(struct vfs *afsp, struct k_statvfs *abp)
{
    AFS_GLOCK();
    AFS_STATCNT(afs_statfs);

    abp->f_type = 0;
    abp->f_frsize = 1024;
    abp->f_bsize = afsp->vfs_bsize;
    /* Fake a high number below to satisfy programs that use the statfs
     * call to make sure that there's enough space in the device partition
     * before storing something there.
     */
    abp->f_blocks = abp->f_bfree = abp->f_bavail = abp->f_files =
	abp->f_ffree = abp->f_favail = AFS_VFS_FAKEFREE;
    abp->f_fsid = (AFS_VFSMAGIC << 16) || AFS_VFSFSID;

    AFS_GUNLOCK();
    return 0;
}

int
afs_sync(struct vfs *unused1, int unused2)
{
    AFS_STATCNT(afs_sync);
    return 0;
}

int
afs_vget(struct vfs *afsp, struct vnode **avcp, struct fid *fidp)
{
    int code;
    struct vrequest treq;
    AFS_GLOCK();
    AFS_STATCNT(afs_vget);

    *avcp = NULL;

    if ((code = afs_InitReq(&treq, p_cred(u.u_procp))) == 0) {
	code = afs_osi_vget((struct vcache **)avcp, fidp, &treq);
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_VGET, ICL_TYPE_POINTER, *avcp,
	       ICL_TYPE_INT32, treq.uid, ICL_TYPE_FID, fidp);
    code = afs_CheckCode(code, &treq, 42);

    AFS_GUNLOCK();
    return code;
}

int
afs_getmount(struct vfs *vfsp, char *fsmntdir, struct mount_data *mdp,
	     char *unused1)
{
    int l;

    mdp->md_msite = 0;
    mdp->md_dev = 0;
    mdp->md_rdev = 0;
    return (copyoutstr
	    (afs_mountpath, fsmntdir, strlen(afs_mountpath) + 1, &l));
}


struct vfsops Afs_vfsops = {
    afs_mount,
    afs_unmount,
    afs_root,
    afs_statfs,
    afs_sync,
    afs_vget,
    afs_getmount,
    (vfs_freeze_t *) 0,		/* vfs_freeze */
    (vfs_thaw_t *) 0,		/* vfs_thaw */
    (vfs_quota_t *) 0,		/* vfs_quota */
    (vfs_mountroot_t *) 0,	/* vfs_mountroot. Note: afs_mountroot_nullop in this
				 *                position panicked HP 11.00+
				 */
    (vfs_size_t *) 0		/* vfs_size */
};

static int afs_Starting = 0;

#pragma align 64
#if !defined(AFS_HPUX110_ENV)
sema_t afs_global_sema = {
    NULL, 0, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0,	/* sa_type */
    0, 0, 0, 0, 0, 0, 0, NULL,	/* sa_link */
    NULL, NULL
#ifdef SEMA_COUNTING
	, 0, 0, 0, NULL
#endif
};
#else
b_sema_t afs_global_sema = { 0 };
#endif

void
osi_InitGlock()
{
    ulong_t context;

    SPINLOCK_USAV(sched_lock, context);
    if (!afs_Starting) {
	afs_Starting = 1;
	SPINUNLOCK_USAV(sched_lock, context);
#if defined(AFS_HPUX110_ENV)
	b_initsema(&afs_global_sema, 1, NFS_LOCK_ORDER2, "AFS GLOCK");
	/* afsHash(64); *//* 64 buckets */
#else
	initsema(&afs_global_sema, 1, FILESYS_SEMA_PRI, FILESYS_SEMA_ORDER);
	afsHash(64);		/* 64 buckets */
#endif
    } else {
	SPINUNLOCK_USAV(sched_lock, context);
    }
    if (!afs_Starting) {
	osi_Panic("osi_Init lost initialization race");
    }
}

#if defined(AFS_HPUX1123_ENV)
/* DLKM routine called when loaded */
static int
afs_load(void *arg)
{
	afsc_link();
	return 0;
}
#endif /* AFS_HPUX1123_ENV */

/*
 * afsc_link - Initialize VFS
 */
int afs_vfs_slot = -1;


afsc_link()
{
    extern int Afs_syscall(), afs_xioctl(), Afs_xsetgroups();

    /* For now nothing special is required during AFS initialization. */
    AFS_STATCNT(afsc_link);
    osi_Init();
    if ((afs_vfs_slot = add_vfs_type("afs", &Afs_vfsops)) < 0)
	return;
    sysent_assign_function(AFS_SYSCALL, 7, (void (*)())Afs_syscall,
			   "Afs_syscall");
    sysent_define_arg(AFS_SYSCALL, 0, longArg);
    sysent_define_arg(AFS_SYSCALL, 1, longArg);
    sysent_define_arg(AFS_SYSCALL, 2, longArg);
    sysent_define_arg(AFS_SYSCALL, 3, longArg);
    sysent_define_arg(AFS_SYSCALL, 4, longArg);
    sysent_define_arg(AFS_SYSCALL, 5, longArg);
    sysent_define_arg(AFS_SYSCALL, 6, longArg);
    sysent_returns_long(AFS_SYSCALL);

    sysent_delete(80);
    sysent_assign_function(80, 2, (void (*)())Afs_xsetgroups, "setgroups");
    sysent_define_arg(80, 0, longArg);
    sysent_define_arg(80, 1, longArg);
    sysent_returns_long(80);
    return 0;
}
