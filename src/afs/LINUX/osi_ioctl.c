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

#if defined(AFS_SPARC64_LINUX26_ENV) && defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
#include <linux/ioctl32.h>
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>

#include "osi_compat.h"

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
# if defined(AFS_S390X_LINUX26_ENV)
    if (test_thread_flag(TIF_31BIT))
# elif defined(AFS_AMD64_LINUX20_ENV)
    if (test_thread_flag(TIF_IA32))
# else
    if (test_thread_flag(TIF_32BIT))
# endif /* AFS_S390X_LINUX26_ENV */
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
#endif /* NEED_IOCTL32 */
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
#if defined(HAVE_LINUX_STRUCT_PROC_OPS)
static struct proc_ops afs_syscall_ops = {
    .proc_ioctl = afs_unlocked_ioctl,
# ifdef STRUCT_PROC_OPS_HAS_PROC_COMPAT_IOCTL
    .proc_compat_ioctl = afs_unlocked_ioctl,
# endif
};
#else
static struct file_operations afs_syscall_ops = {
# ifdef HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl = afs_unlocked_ioctl,
# else
    .ioctl = afs_ioctl,
# endif
# ifdef HAVE_COMPAT_IOCTL
    .compat_ioctl = afs_unlocked_ioctl,
# endif
};
#endif /* HAVE_LINUX_STRUCT_PROC_OPS */
void
osi_ioctl_init(void)
{
    struct proc_dir_entry *entry;

    entry = afs_proc_create(PROC_SYSCALL_NAME, 0666, openafs_procfs, &afs_syscall_ops);
#if defined(STRUCT_PROC_DIR_ENTRY_HAS_OWNER)
    if (entry)
	entry->owner = THIS_MODULE;
#endif

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
