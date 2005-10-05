#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#ifdef AFS_DARWIN80_ENV
static vfstable_t afs_vfstable;
static struct vfs_fsentry afs_vfsentry;
extern struct vnodeopv_desc afs_vnodeop_opv_desc;
extern struct vnodeopv_desc afs_dead_vnodeop_opv_desc;
static struct vnodeopv_desc *afs_vnodeop_opv_desc_list[2] =
   { &afs_vnodeop_opv_desc, &afs_dead_vnodeop_opv_desc };


#include <sys/conf.h>
#include <miscfs/devfs/devfs.h>
#define seltrue eno_select
struct cdevsw afs_cdev = NO_CDEVICE;
#undef seltrue
static int afs_cdev_major;
extern open_close_fcn_t afs_cdev_nop_openclose;
extern ioctl_fcn_t afs_cdev_ioctl;
static void *afs_cdev_devfs_handle;
#else
#ifdef AFS_DARWIN60_ENV		/* not in Kernel.framework anymore !?! */
#include <sys/syscall.h>
#else
#include "sys/syscall.h"
#endif
struct vfsconf afs_vfsconf;
#endif
#include <mach/kmod.h>

extern struct vfsops afs_vfsops;
extern struct mount *afs_globalVFS;
extern int Afs_xsetgroups();
extern int afs_xioctl();
extern int afs3_syscall();

extern int ioctl();
extern int setgroups();
extern int maxvfsconf;
kern_return_t
afs_modload(struct kmod_info *ki, void *data)
{
    osi_Init();
#ifdef AFS_DARWIN80_ENV
    memset(&afs_vfsentry, 0, sizeof(struct vfs_fsentry));
    strcpy(afs_vfsentry.vfe_fsname, "afs");
    afs_vfsentry.vfe_vfsops = &afs_vfsops;
    afs_vfsentry.vfe_vopcnt = 2;
    afs_vfsentry.vfe_opvdescs = afs_vnodeop_opv_desc_list;
    /* We may be 64bit ready too (VFS_TBL64BITREADY) */
    afs_vfsentry.vfe_flags = VFS_TBLTHREADSAFE|VFS_TBLNOTYPENUM;
    if (vfs_fsadd(&afs_vfsentry, &afs_vfstable)) {
	printf("AFS: vfs_fsadd failed. aborting\n");
	return KERN_FAILURE;
    }
    afs_cdev.d_open = &afs_cdev_nop_openclose;
    afs_cdev.d_close = &afs_cdev_nop_openclose;
    afs_cdev.d_ioctl = &afs_cdev_ioctl;
    afs_cdev_major = cdevsw_add(-1, &afs_cdev);
    if (afs_cdev_major == -1) {
	printf("AFS: cdevsw_add failed. aborting\n");
        vfs_fsremove(afs_vfstable);
	return KERN_FAILURE;
    }
    afs_cdev_devfs_handle = devfs_make_node(makedev(afs_cdev_major, 0),
                                            DEVFS_CHAR, UID_ROOT, GID_WHEEL,
                                            0666, "openafs_ioctl", 0);
#else
    memset(&afs_vfsconf, 0, sizeof(struct vfsconf));
    strcpy(afs_vfsconf.vfc_name, "afs");
    afs_vfsconf.vfc_vfsops = &afs_vfsops;
    afs_vfsconf.vfc_typenum = maxvfsconf++;	/* oddly not VT_AFS */
    afs_vfsconf.vfc_flags = MNT_NODEV;
    if (vfsconf_add(&afs_vfsconf)) {
	printf("AFS: vfsconf_add failed. aborting\n");
	return KERN_FAILURE;
    }
    if (sysent[AFS_SYSCALL].sy_call != nosys) {
	printf("AFS_SYSCALL in use. aborting\n");
	return KERN_FAILURE;
    }
    sysent[SYS_setgroups].sy_call = Afs_xsetgroups;
#if 0
    sysent[SYS_ioctl].sy_call = afs_xioctl;
#endif
    sysent[AFS_SYSCALL].sy_call = afs3_syscall;
    sysent[AFS_SYSCALL].sy_narg = 5;
    sysent[AFS_SYSCALL].sy_parallel = 0;
#ifdef KERNEL_FUNNEL
    sysent[AFS_SYSCALL].sy_funnel = KERNEL_FUNNEL;
#endif
#endif
#ifdef AFS_DARWIN80_ENV
    MUTEX_SETUP();
    afs_global_lock = lck_mtx_alloc_init(openafs_lck_grp, 0);
#endif
    return KERN_SUCCESS;
}

kern_return_t
afs_modunload(struct kmod_info * ki, void *data)
{
    if (afs_globalVFS)
	return KERN_FAILURE;
#ifdef AFS_DARWIN80_ENV
    if (vfs_fsremove(afs_vfstable))
	return KERN_FAILURE;
    devfs_remove(afs_cdev_devfs_handle);
    cdevsw_remove(afs_cdev_major, &afs_cdev);
#else
    if (vfsconf_del("afs"))
	return KERN_FAILURE;
    /* give up syscall entries for ioctl & setgroups, which we've stolen */
#if 0
    sysent[SYS_ioctl].sy_call = ioctl;
#endif
#ifndef AFS_DARWIN80_ENV
    sysent[SYS_setgroups].sy_call = setgroups;
#endif
    /* give up the stolen syscall entry */
    sysent[AFS_SYSCALL].sy_narg = 0;
    sysent[AFS_SYSCALL].sy_call = nosys;
#endif
#ifdef AFS_DARWIN80_ENV
    MUTEX_FINISH();
    lck_mtx_free(afs_global_lock, openafs_lck_grp);
#endif
    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(org.openafs.filesystems.afs, "1.3.82", afs_modload,
		   afs_modunload)
