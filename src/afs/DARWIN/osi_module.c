#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"
#include "afsincludes.h"

#define MYBUNDLEID "org.openafs.filesystems.afs"
extern struct vfsops afs_vfsops;

#ifdef AFS_DARWIN80_ENV
static vfstable_t afs_vfstable;
extern struct vnodeopv_desc afs_vnodeop_opv_desc;
extern struct vnodeopv_desc afs_dead_vnodeop_opv_desc;
static struct vnodeopv_desc *afs_vnodeop_opv_desc_list[2] =
   { &afs_vnodeop_opv_desc, &afs_dead_vnodeop_opv_desc };

struct vfs_fsentry afs_vfsentry = {
  &afs_vfsops,
  2,
  afs_vnodeop_opv_desc_list,
  0,
  "afs",
  VFS_TBLNOTYPENUM|VFS_TBLTHREADSAFE|VFS_TBL64BITREADY,
  NULL,
  NULL,
};

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
#include <sys/syscall.h>
struct vfsconf afs_vfsconf;
#endif
#include <mach/kmod.h>

extern struct mount *afs_globalVFS;
extern int Afs_xsetgroups();

extern int ioctl();
extern int setgroups();
extern int maxvfsconf;
kern_return_t
afs_modload(struct kmod_info *kmod_info, void *data)
{
    int ret;
    osi_Init();
#ifdef AFS_DARWIN80_ENV
    MUTEX_SETUP();
    afs_global_lock = lck_mtx_alloc_init(openafs_lck_grp, 0);

    if (ret = vfs_fsadd(&afs_vfsentry, &afs_vfstable)) {
	afs_warn("AFS: vfs_fsadd failed. aborting: %d\n", ret);
	afs_vfstable = NULL;
	goto fsadd_out;
    }
    afs_cdev.d_open = &afs_cdev_nop_openclose;
    afs_cdev.d_close = &afs_cdev_nop_openclose;
    afs_cdev.d_ioctl = &afs_cdev_ioctl;
    afs_cdev_major = cdevsw_add(-1, &afs_cdev);
    if (afs_cdev_major == -1) {
	afs_warn("AFS: cdevsw_add failed. aborting\n");
	goto cdevsw_out;
    }
    afs_cdev_devfs_handle = devfs_make_node(makedev(afs_cdev_major, 0),
                                            DEVFS_CHAR, UID_ROOT, GID_WHEEL,
                                            0666, "openafs_ioctl", 0);
    if (!afs_cdev_devfs_handle) {
	afs_warn("AFS: devfs_make_node failed. aborting\n");
	cdevsw_remove(afs_cdev_major, &afs_cdev);
    cdevsw_out:
	vfs_fsremove(afs_vfstable);
    fsadd_out:
	MUTEX_FINISH();
	lck_mtx_free(afs_global_lock, openafs_lck_grp);
	return KERN_FAILURE;
    }
#else
    memset(&afs_vfsconf, 0, sizeof(struct vfsconf));
    strcpy(afs_vfsconf.vfc_name, "afs");
    afs_vfsconf.vfc_vfsops = &afs_vfsops;
    afs_vfsconf.vfc_typenum = maxvfsconf++;	/* oddly not VT_AFS */
    afs_vfsconf.vfc_flags = MNT_NODEV;
    if (vfsconf_add(&afs_vfsconf)) {
	afs_warn("AFS: vfsconf_add failed. aborting\n");
	return KERN_FAILURE;
    }
    if (sysent[AFS_SYSCALL].sy_call != nosys) {
	afs_warn("AFS_SYSCALL in use. aborting\n");
	return KERN_FAILURE;
    }
    sysent[SYS_setgroups].sy_call = Afs_xsetgroups;
    sysent[AFS_SYSCALL].sy_call = afs3_syscall;
    sysent[AFS_SYSCALL].sy_narg = 5;
    sysent[AFS_SYSCALL].sy_parallel = 0;
#ifdef KERNEL_FUNNEL
    sysent[AFS_SYSCALL].sy_funnel = KERNEL_FUNNEL;
#endif
#endif
    afs_warn("%s kext loaded; %u pages at 0x%lx (load tag %u).\n",
	   kmod_info->name, (unsigned)kmod_info->size / PAGE_SIZE,
	   (unsigned long)kmod_info->address, (unsigned)kmod_info->id);

    return KERN_SUCCESS;
}

kern_return_t
afs_modunload(struct kmod_info * kmod_info, void *data)
{
    if (afs_globalVFS)
	return KERN_FAILURE;
    if ((afs_initState != 0) || (afs_shuttingdown != AFS_RUNNING))
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
    sysent[SYS_setgroups].sy_call = setgroups;
    /* give up the stolen syscall entry */
    sysent[AFS_SYSCALL].sy_narg = 0;
    sysent[AFS_SYSCALL].sy_call = nosys;
#endif
#ifdef AFS_DARWIN80_ENV
    MUTEX_FINISH();
    lck_mtx_free(afs_global_lock, openafs_lck_grp);
#endif
    afs_warn("%s kext unloaded; (load tag %u).\n",
	   kmod_info->name, (unsigned)kmod_info->id);
    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(MYBUNDLEID, VERSION, afs_modload,
		   afs_modunload)
