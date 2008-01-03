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

RCSID
    ("$Header$");

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/unistd.h"		/* For syscall numbers. */
#include "h/mm.h"

#ifdef AFS_AMD64_LINUX20_ENV
#include <asm/ia32_unistd.h>
#endif
#ifdef AFS_SPARC64_LINUX20_ENV
#include <linux/ioctl32.h>
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#endif

extern struct file_system_type afs_fs_type;

#if !defined(AFS_LINUX24_ENV)
static long get_page_offset(void);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
DEFINE_MUTEX(afs_global_lock);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
DECLARE_MUTEX(afs_global_lock);
#else
struct semaphore afs_global_lock = MUTEX;
#endif
int afs_global_owner = 0;
#if !defined(AFS_LINUX24_ENV)
unsigned long afs_linux_page_offset = 0;	/* contains the PAGE_OFFSET value */
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
int __init
afs_init(void)
#else
int
init_module(void)
#endif
{
    int err;
    RWLOCK_INIT(&afs_xosi, "afs_xosi");

#if !defined(AFS_LINUX24_ENV)
    /* obtain PAGE_OFFSET value */
    afs_linux_page_offset = get_page_offset();

#ifndef AFS_S390_LINUX22_ENV
    if (afs_linux_page_offset == 0) {
	/* couldn't obtain page offset so can't continue */
	printf("afs: Unable to obtain PAGE_OFFSET. Exiting..");
	return -EIO;
    }
#endif /* AFS_S390_LINUX22_ENV */
#endif /* !defined(AFS_LINUX24_ENV) */

    osi_Init();
#if defined(AFS_LINUX26_ENV) && defined(CONFIG_GSSRPC)
#if !defined(AFS_NONFSTRANS)
    osi_linux_nfssrv_init();
#endif
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
#ifdef AFS_LINUX24_ENV
    osi_proc_init();
    osi_ioctl_init();
#endif

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
void __exit
afs_cleanup(void)
#else
void
cleanup_module(void)
#endif
{
#ifdef LINUX_KEYRING_SUPPORT
    osi_keyring_shutdown();
#endif
    osi_sysctl_clean();
    osi_syscall_clean();
    unregister_filesystem(&afs_fs_type);

    afs_destroy_inodecache();
#if defined(AFS_LINUX26_ENV) && defined(CONFIG_GSSRPC)
#if !defined(AFS_NONFSTRANS)
    osi_linux_nfssrv_shutdown();
#endif
#endif
    osi_linux_free_afs_memory();

#ifdef AFS_LINUX24_ENV
    osi_ioctl_clean();
    osi_proc_clean();
#endif
    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
MODULE_LICENSE("http://www.openafs.org/dl/license10.html");
module_init(afs_init);
module_exit(afs_cleanup);
#endif


#if !defined(AFS_LINUX24_ENV)
static long
get_page_offset(void)
{
#if defined(AFS_PPC_LINUX22_ENV) || defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV) || defined(AFS_ALPHA_LINUX20_ENV) || defined(AFS_S390_LINUX22_ENV) || defined(AFS_IA64_LINUX20_ENV) || defined(AFS_PARISC_LINUX24_ENV) || defined(AFS_AMD64_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
    return PAGE_OFFSET;
#else
    struct task_struct *p, *q;

    /* search backward thru the circular list */
#if defined(EXPORTED_TASKLIST_LOCK) 
    read_lock(&tasklist_lock);
#endif
    /* search backward thru the circular list */
#ifdef DEFINED_PREV_TASK
    for (q = current; p = q; q = prev_task(p)) {
#else
    for (p = current; p; p = p->prev_task) {
#endif
	if (p->pid == 1) {
#if defined(EXPORTED_TASKLIST_LOCK) 
	    read_unlock(&tasklist_lock);
#endif
	    return p->addr_limit.seg;
	}
    }

#if defined(EXPORTED_TASKLIST_LOCK) 
    read_unlock(&tasklist_lock);
#endif
    return 0;
#endif
}
#endif /* !AFS_LINUX24_ENV */
