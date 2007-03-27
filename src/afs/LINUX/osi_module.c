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
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_module.c,v 1.52.2.26 2007/02/09 01:30:33 shadow Exp $");

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

#ifdef HAVE_KERNEL_LINUX_SEQ_FILE_H
#include <linux/seq_file.h>
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

static inline int afs_ioctl(struct inode *, struct file *, unsigned int,
		     unsigned long);
#if defined(HAVE_UNLOCKED_IOCTL) || defined(HAVE_COMPAT_IOCTL)
static long afs_unlocked_ioctl(struct file *, unsigned int, unsigned long);
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

#ifdef HAVE_KERNEL_LINUX_SEQ_FILE_H
static void *c_start(struct seq_file *m, loff_t *pos)
{
	struct afs_q *cq, *tq;
	loff_t n = 0;

	ObtainReadLock(&afs_xcell);
	for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
		tq = QNext(cq);

		if (n++ == *pos)
			break;
	}
	if (cq == &CellLRU)
		return NULL;

	return cq;
}

static void *c_next(struct seq_file *m, void *p, loff_t *pos)
{
	struct afs_q *cq = p, *tq;

	(*pos)++;
	tq = QNext(cq);

	if (tq == &CellLRU)
		return NULL;

	return tq;
}

static void c_stop(struct seq_file *m, void *p)
{
	ReleaseReadLock(&afs_xcell);
}

static int c_show(struct seq_file *m, void *p)
{
	struct afs_q *cq = p;
	struct cell *tc = QTOC(cq);
	int j;

	seq_printf(m, ">%s #(%d/%d)\n", tc->cellName,
		   tc->cellNum, tc->cellIndex);

	for (j = 0; j < MAXCELLHOSTS; j++) {
		afs_uint32 addr;

		if (!tc->cellHosts[j]) break;

		addr = tc->cellHosts[j]->addr->sa_ip;
		seq_printf(m, "%u.%u.%u.%u #%u.%u.%u.%u\n",
			   NIPQUAD(addr), NIPQUAD(addr));
	}

	return 0;
}

static struct seq_operations afs_csdb_op = {
	.start		= c_start,
	.next		= c_next,
	.stop		= c_stop,
	.show		= c_show,
};

static int afs_csdb_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &afs_csdb_op);
}

static struct file_operations afs_csdb_operations = {
	.open		= afs_csdb_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

#else /* HAVE_KERNEL_LINUX_SEQ_FILE_H */

int
csdbproc_info(char *buffer, char **start, off_t offset, int
length)
{
    int len = 0;
    off_t pos = 0;
    int cnt;
    struct afs_q *cq, *tq;
    struct cell *tc;
    char tbuffer[16];
    /* 90 - 64 cellname, 10 for 32 bit num and index, plus
       decor */
    char temp[91];
    afs_uint32 addr;
    
    ObtainReadLock(&afs_xcell);

    for (cq = CellLRU.next; cq != &CellLRU; cq = tq) {
        tc = QTOC(cq); tq = QNext(cq);

        pos += 90;

        if (pos <= offset) {
            len = 0;
        } else {
            sprintf(temp, ">%s #(%d/%d)\n", tc->cellName, 
                    tc->cellNum, tc->cellIndex);
            sprintf(buffer + len, "%-89s\n", temp);
            len += 90;
            if (pos >= offset+length) {
                ReleaseReadLock(&afs_xcell);
                goto done;
            }
        }

        for (cnt = 0; cnt < MAXCELLHOSTS; cnt++) {
            if (!tc->cellHosts[cnt]) break;
            pos += 90;
            if (pos <= offset) {
                len = 0;
            } else {
                addr = ntohl(tc->cellHosts[cnt]->addr->sa_ip);
                sprintf(tbuffer, "%d.%d.%d.%d", 
                        (int)((addr>>24) & 0xff),
(int)((addr>>16) & 0xff),
                        (int)((addr>>8)  & 0xff), (int)( addr & 0xff));
                sprintf(temp, "%s #%s\n", tbuffer, tbuffer);
                sprintf(buffer + len, "%-89s\n", temp);
                len += 90;
                if (pos >= offset+length) {
                    ReleaseReadLock(&afs_xcell);
                    goto done;
                }
            }
        }
    }

    ReleaseReadLock(&afs_xcell);
    
done:
    *start = buffer + len - (pos - offset);
    len = pos - offset;
    if (len > length)
        len = length;
    return len;
}

#endif /* HAVE_KERNEL_LINUX_SEQ_FILE_H */

static struct proc_dir_entry *openafs_procfs;
#if defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
static int ioctl32_done;
#endif

#ifdef AFS_LINUX24_ENV
static int
afsproc_init(void)
{
    struct proc_dir_entry *entry1;
    struct proc_dir_entry *entry2;

    openafs_procfs = proc_mkdir(PROC_FSDIRNAME, proc_root_fs);
    entry1 = create_proc_entry(PROC_SYSCALL_NAME, 0666, openafs_procfs);

    entry1->proc_fops = &afs_syscall_fops;

    entry1->owner = THIS_MODULE;

#ifdef HAVE_KERNEL_LINUX_SEQ_FILE_H
    entry2 = create_proc_entry(PROC_CELLSERVDB_NAME, 0, openafs_procfs);
    if (entry2)
	entry2->proc_fops = &afs_csdb_operations;
#else
    entry2 = create_proc_info_entry(PROC_CELLSERVDB_NAME, (S_IFREG|S_IRUGO), openafs_procfs, csdbproc_info);
#endif

#if defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
    if (register_ioctl32_conversion(VIOC_SYSCALL32, NULL) == 0) 
	ioctl32_done = 1;
#endif
    return 0;
}

static void
afsproc_exit(void)
{
    remove_proc_entry(PROC_CELLSERVDB_NAME, openafs_procfs);
    remove_proc_entry(PROC_SYSCALL_NAME, openafs_procfs);
    remove_proc_entry(PROC_FSDIRNAME, proc_root_fs);
#if defined(NEED_IOCTL32) && !defined(HAVE_COMPAT_IOCTL)
    if (ioctl32_done)
	    unregister_ioctl32_conversion(VIOC_SYSCALL32);
#endif
}
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

    err = osi_syscall_init();
    if (err)
	return err;
    err = afs_init_inodecache();
    if (err)
	return err;
    register_filesystem(&afs_fs_type);
    osi_sysctl_init();
#ifdef LINUX_KEYRING_SUPPORT
    osi_keyring_init();
#endif
#ifdef AFS_LINUX24_ENV
    afsproc_init();
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
    osi_keyring_shutdown();
    osi_sysctl_clean();
    osi_syscall_clean();
    unregister_filesystem(&afs_fs_type);

    afs_destroy_inodecache();
    osi_linux_free_afs_memory();

#ifdef AFS_LINUX24_ENV
    afsproc_exit();
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
