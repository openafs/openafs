#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/DARWIN/osi_module.c,v 1.10 2003/07/15 23:14:18 shadow Exp $");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#ifdef AFS_DARWIN60_ENV		/* not in Kernel.framework anymore !?! */
#include <sys/syscall.h>
#else
#include "sys/syscall.h"
#endif
#include <mach/kmod.h>

struct vfsconf afs_vfsconf;
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
    if (sysent[AFS_SYSCALL].sy_call != nosys) {
	printf("AFS_SYSCALL in use. aborting\n");
	return KERN_FAILURE;
    }
    memset(&afs_vfsconf, 0, sizeof(struct vfsconf));
    strcpy(afs_vfsconf.vfc_name, "afs");
    afs_vfsconf.vfc_vfsops = &afs_vfsops;
    afs_vfsconf.vfc_typenum = maxvfsconf++;	/* oddly not VT_AFS */
    afs_vfsconf.vfc_flags = MNT_NODEV;
    if (vfsconf_add(&afs_vfsconf)) {
	printf("AFS: vfsconf_add failed. aborting\n");
	return KERN_FAILURE;
    }
    sysent[SYS_setgroups].sy_call = Afs_xsetgroups;
    sysent[SYS_ioctl].sy_call = afs_xioctl;
    sysent[AFS_SYSCALL].sy_call = afs3_syscall;
    sysent[AFS_SYSCALL].sy_narg = 5;
    sysent[AFS_SYSCALL].sy_parallel = 0;
#ifdef KERNEL_FUNNEL
    sysent[AFS_SYSCALL].sy_funnel = KERNEL_FUNNEL;
#endif
    return KERN_SUCCESS;
}

kern_return_t
afs_modunload(struct kmod_info * ki, void *data)
{
    if (afs_globalVFS)
	return KERN_FAILURE;
    if (vfsconf_del("afs"))
	return KERN_FAILURE;
    /* give up syscall entries for ioctl & setgroups, which we've stolen */
    sysent[SYS_ioctl].sy_call = ioctl;
    sysent[SYS_setgroups].sy_call = setgroups;
    /* give up the stolen syscall entry */
    sysent[AFS_SYSCALL].sy_narg = 0;
    sysent[AFS_SYSCALL].sy_call = nosys;
    return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL(org.openafs.filesystems.afs, VERSION, afs_modload,
		   afs_modunload)
