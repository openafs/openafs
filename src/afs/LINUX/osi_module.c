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

#ifdef AFS_AMD64_LINUX20_ENV
#include <asm/ia32_unistd.h>
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>

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

int __init
afs_init(void)
{
    int err;
    AFS_RWLOCK_INIT(&afs_xosi, "afs_xosi");

#ifdef HAVE_LINUX_KUID_T
    afs_ns = afs_current_user_ns();
#endif

    osi_Init();
#if !defined(AFS_NONFSTRANS)
    osi_linux_nfssrv_init();
#endif

#ifndef LINUX_KEYRING_SUPPORT
    err = osi_syscall_init();
    if (err)
	return err;
#endif
    err = afs_init_inodecache();
    if (err) {
#ifndef LINUX_KEYRING_SUPPORT
	osi_syscall_clean();
#endif
	return err;
    }
    err = register_filesystem(&afs_fs_type);
    if (err) {
	afs_destroy_inodecache();
#ifndef LINUX_KEYRING_SUPPORT
	osi_syscall_clean();
#endif
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

void __exit
afs_cleanup(void)
{
    afs_shutdown_pagecopy();

#ifdef LINUX_KEYRING_SUPPORT
    osi_keyring_shutdown();
#endif
    osi_sysctl_clean();
#ifndef LINUX_KEYRING_SUPPORT
    osi_syscall_clean();
#endif
    unregister_filesystem(&afs_fs_type);

    afs_destroy_inodecache();
#if !defined(AFS_NONFSTRANS)
    osi_linux_nfssrv_shutdown();
#endif
    osi_linux_free_afs_memory();

    osi_ioctl_clean();
    osi_proc_clean();

    return;
}

MODULE_LICENSE("http://www.openafs.org/dl/license10.html");
module_init(afs_init);
module_exit(afs_cleanup);

