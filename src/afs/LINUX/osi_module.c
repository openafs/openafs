/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Linux module support routines.
 *
 */
#include <afsconfig.h>
#include "afs/param.h"


#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include <linux/unistd.h>		/* For syscall numbers. */
#include <linux/mm.h>

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>

#if defined(IOP_TAKES_MNT_IDMAP)
# include <linux/fs_struct.h>
#endif

#include "osi_pagecopy.h"

extern struct file_system_type afs_fs_type;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
DEFINE_MUTEX(afs_global_lock);
#else
DECLARE_MUTEX(afs_global_lock);
#endif
int afs_global_owner = 0;

#ifdef HAVE_LINUX_KUID_T
struct user_namespace *afs_ns;
#endif

#if defined(IOP_TAKES_MNT_IDMAP)
struct mnt_idmap *afs_mnt_idmap;

static void
afs_init_idmap(void)
{
    struct path fs_root;

    get_fs_root(current->fs, &fs_root);
    afs_mnt_idmap = mnt_idmap(fs_root.mnt);
    path_put(&fs_root);
}
#endif

static int __init
afs_init(void)
{
    int err;

#ifdef HAVE_LINUX_KUID_T
    afs_ns = afs_current_user_ns();
#endif

#if defined(IOP_TAKES_MNT_IDMAP)
    afs_init_idmap();
#endif

    osi_Init();

    /* Initialize CellLRU since it is used while traversing CellServDB proc
     * entry */

    QInit(&CellLRU);

#if !defined(AFS_NONFSTRANS)
    osi_linux_nfssrv_init();
#endif

    err = osi_syscall_init();
    if (err)
	return err;
    err = afs_init_inodecache();
    if (err) {
	osi_syscall_clean();
	return err;
    }
    err = register_filesystem(&afs_fs_type);
    if (err) {
	afs_destroy_inodecache();
	osi_syscall_clean();
	return err;
    }

    osi_sysctl_init();
#ifdef LINUX_KEYRING_SUPPORT
    osi_keyring_init();
#endif
    osi_proc_init();
    osi_ioctl_init();
    afs_init_pagecopy();

    return 0;
}

static void __exit
afs_cleanup(void)
{
    afs_shutdown_pagecopy();

#ifdef LINUX_KEYRING_SUPPORT
    osi_keyring_shutdown();
#endif
    osi_sysctl_clean();
    osi_syscall_clean();
    unregister_filesystem(&afs_fs_type);

    afs_destroy_inodecache();
#if !defined(AFS_NONFSTRANS)
    osi_linux_nfssrv_shutdown();
#endif
    shutdown_osisleep();
    osi_linux_free_afs_memory();

    osi_ioctl_clean();
    osi_proc_clean();

    return;
}

MODULE_LICENSE("http://www.openafs.org/dl/license10.html");
MODULE_DESCRIPTION("OpenAFS AFS filesystem client");
module_init(afs_init);
module_exit(afs_cleanup);

