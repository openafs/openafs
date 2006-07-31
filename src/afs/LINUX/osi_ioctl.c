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
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>

extern struct proc_dir_entry *openafs_procfs;
#if defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
static int ioctl32_done;
#endif

extern asmlinkage long
afs_syscall(long syscall, long parm1, long parm2, long parm3, long parm4);

static int
afs_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{

    struct afsprocdata sysargs;
#ifdef NEED_IOCTL32
    struct afsprocdata32 sysargs32;
#endif

    if (cmd != VIOC_SYSCALL && cmd != VIOC_SYSCALL32) return -EINVAL;

#ifdef NEED_IOCTL32
#ifdef AFS_LINUX26_ENV 
#ifdef AFS_S390X_LINUX26_ENV
    if (test_thread_flag(TIF_31BIT))
#elif AFS_AMD64_LINUX20_ENV
    if (test_thread_flag(TIF_IA32))
#else
    if (test_thread_flag(TIF_32BIT))
#endif /* AFS_S390X_LINUX26_ENV */
#else
#ifdef AFS_SPARC64_LINUX24_ENV
    if (current->thread.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_SPARC64_LINUX20_ENV)
    if (current->tss.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_AMD64_LINUX20_ENV)
    if (current->thread.flags & THREAD_IA32)
#elif defined(AFS_PPC64_LINUX20_ENV)
    if (current->thread.flags & PPC_FLAG_32BIT)
#elif defined(AFS_S390X_LINUX20_ENV)
    if (current->thread.flags & S390_FLAG_31BIT)
#else
#error Not done for this linux type
#endif /* AFS_LINUX26_ENV */
#endif /* NEED_IOCTL32 */
    {
	if (copy_from_user(&sysargs32, (void *)arg,
			   sizeof(struct afsprocdata32)))
	    return -EFAULT;

	return afs_syscall((unsigned long)sysargs32.syscall,
			   (unsigned long)sysargs32.param1,
			   (unsigned long)sysargs32.param2,
			   (unsigned long)sysargs32.param3,
			   (unsigned long)sysargs32.param4);
    } else
#endif
    {
	if (copy_from_user(&sysargs, (void *)arg, sizeof(struct afsprocdata)))
	    return -EFAULT;

	return afs_syscall(sysargs.syscall, sysargs.param1,
			   sysargs.param2, sysargs.param3, sysargs.param4);
    }
}

#if defined(HAVE_UNLOCKED_IOCTL) || defined(HAVE_COMPAT_IOCTL)
static long afs_unlocked_ioctl(struct file *file, unsigned int cmd,
                               unsigned long arg) {
    return afs_ioctl(FILE_INODE(file), file, cmd, arg);
}
#endif

static struct file_operations afs_syscall_fops = {
#ifdef HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl = afs_unlocked_ioctl,
#else
    .ioctl = afs_ioctl,
#endif
#ifdef HAVE_COMPAT_IOCTL
    .compat_ioctl = afs_unlocked_ioctl,
#endif
};

void
osi_ioctl_init(void)
{
    struct proc_dir_entry *entry;

    entry = create_proc_entry(PROC_SYSCALL_NAME, 0666, openafs_procfs);
    entry->proc_fops = &afs_syscall_fops;
    entry->owner = THIS_MODULE;

#if defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
    if (register_ioctl32_conversion(VIOC_SYSCALL32, NULL) == 0) 
	ioctl32_done = 1;
#endif
}

void
osi_ioctl_clean(void)
{
    remove_proc_entry(PROC_SYSCALL_NAME, openafs_procfs);
#if defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
    if (ioctl32_done)
	    unregister_ioctl32_conversion(VIOC_SYSCALL32);
#endif
}
