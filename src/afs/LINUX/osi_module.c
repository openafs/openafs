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
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_module.c,v 1.52.2.7 2005/02/21 01:15:35 shadow Exp $");

#include <linux/module.h> /* early to avoid printf->printk mapping */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/unistd.h"		/* For syscall numbers. */
#include "h/mm.h"

#ifdef AFS_AMD64_LINUX20_ENV
#include "../asm/ia32_unistd.h"
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#include <linux/sched.h>
#endif

extern struct file_system_type afs_fs_type;

#if !defined(AFS_LINUX24_ENV)
static long get_page_offset(void);
#endif

#if defined(AFS_LINUX24_ENV)
DECLARE_MUTEX(afs_global_lock);
#else
struct semaphore afs_global_lock = MUTEX;
#endif
int afs_global_owner = 0;
#if !defined(AFS_LINUX24_ENV)
unsigned long afs_linux_page_offset = 0;	/* contains the PAGE_OFFSET value */
#endif


static int afs_ioctl(struct inode *, struct file *, unsigned int,
		     unsigned long);

static struct file_operations afs_syscall_fops = {
    .ioctl = afs_ioctl,
};

int
csdbproc_read(char *buffer, char **start, off_t offset, int count,
	      int *eof, void *data)
{
    int len, j;
    struct afs_q *cq, *tq;
    struct cell *tc;
    char tbuffer[16];
    afs_uint32 addr;
    
    len = 0;
    ObtainReadLock(&afs_xcell);
    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
	tc = QTOC(cq); tq = QNext(cq);
	len += sprintf(buffer + len, ">%s #(%d/%d)\n", tc->cellName, 
		       tc->cellNum, tc->cellIndex);
	for (j = 0; j < MAXCELLHOSTS; j++) {
	    if (!tc->cellHosts[j]) break;
	    addr = ntohl(tc->cellHosts[j]->addr->sa_ip);
	    sprintf(tbuffer, "%d.%d.%d.%d", 
		    (int)((addr>>24) & 0xff), (int)((addr>>16) & 0xff),
		    (int)((addr>>8)  & 0xff), (int)( addr      & 0xff));
            len += sprintf(buffer + len, "%s #%s\n", tbuffer, tbuffer);
	}
    }
    ReleaseReadLock(&afs_xcell);
    
    if (offset >= len) {
	*start = buffer;
	*eof = 1;
	return 0;
    }
    *start = buffer + offset;
    if ((len -= offset) > count)
	return count;
    *eof = 1;
    return len;
}

static struct proc_dir_entry *openafs_procfs;

static int
afsproc_init(void)
{
    struct proc_dir_entry *entry1;
    struct proc_dir_entry *entry2;

    openafs_procfs = proc_mkdir(PROC_FSDIRNAME, proc_root_fs);
    entry1 = create_proc_entry(PROC_SYSCALL_NAME, 0666, openafs_procfs);

    entry1->proc_fops = &afs_syscall_fops;

    entry1->owner = THIS_MODULE;

    entry2 = create_proc_read_entry(PROC_CELLSERVDB_NAME, (S_IFREG|S_IRUGO), openafs_procfs, csdbproc_read, NULL);

    return 0;
}

static void
afsproc_exit(void)
{
    remove_proc_entry(PROC_CELLSERVDB_NAME, openafs_procfs);
    remove_proc_entry(PROC_SYSCALL_NAME, openafs_procfs);
    remove_proc_entry(PROC_FSDIRNAME, proc_root_fs);
}

extern asmlinkage long
afs_syscall(long syscall, long parm1, long parm2, long parm3, long parm4);

static int
afs_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{

    struct afsprocdata sysargs;
    struct afsprocdata32 sysargs32;

    if (cmd != VIOC_SYSCALL) return -EINVAL;

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV)
#ifdef AFS_SPARC64_LINUX24_ENV
    if (current->thread.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_SPARC64_LINUX20_ENV)
    if (current->tss.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_AMD64_LINUX20_ENV)
#ifdef AFS_LINUX26_ENV
    if (test_thread_flag(TIF_IA32))
#else
    if (current->thread.flags & THREAD_IA32)
#endif
#elif defined(AFS_PPC64_LINUX20_ENV)
#ifdef AFS_PPC64_LINUX26_ENV
    if (current->thread_info->flags & _TIF_32BIT)
#else /*Linux 2.6 */
    if (current->thread.flags & PPC_FLAG_32BIT)
#endif
#elif defined(AFS_S390X_LINUX20_ENV)
    if (current->thread.flags & S390_FLAG_31BIT)
#else
#error Not done for this linux type
#endif
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


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
int __init
afs_init(void)
#else
int
init_module(void)
#endif
{
    int e;
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

    e = osi_syscall_init();
    if (e) return e;
    register_filesystem(&afs_fs_type);
    osi_sysctl_init();
    afsproc_init();

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
    osi_sysctl_clean();
    osi_syscall_clean();
    unregister_filesystem(&afs_fs_type);

    osi_linux_free_inode_pages();	/* Invalidate all pages using AFS inodes. */
    osi_linux_free_afs_memory();

    afsproc_exit();
    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    read_lock(&tasklist_lock);
#endif
    /* search backward thru the circular list */
#ifdef DEFINED_PREV_TASK
    for (q = current; p = q; q = prev_task(p)) {
#else
    for (p = current; p; p = p->prev_task) {
#endif
	if (p->pid == 1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	    read_unlock(&tasklist_lock);
#endif
	    return p->addr_limit.seg;
	}
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    read_unlock(&tasklist_lock);
#endif
    return 0;
#endif
}
#endif /* !AFS_LINUX24_ENV */
