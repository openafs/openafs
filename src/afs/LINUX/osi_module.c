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
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_module.c,v 1.52.2.2 2004/10/18 17:43:51 shadow Exp $");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/unistd.h"		/* For syscall numbers. */
#include "h/mm.h"

#ifdef AFS_AMD64_LINUX20_ENV
#include "../asm/ia32_unistd.h"
#endif

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#include <linux/sched.h>
#endif
#if !defined(EXPORTED_SYS_CALL_TABLE) && defined(HAVE_KERNEL_LINUX_SYSCALL_H)
#include <linux/syscall.h>
#endif

#ifdef AFS_SPARC64_LINUX24_ENV
#define __NR_setgroups32      82	/* This number is not exported for some bizarre reason. */
#endif

#if !defined(AFS_LINUX24_ENV)
asmlinkage int (*sys_settimeofdayp) (struct timeval * tv,
				     struct timezone * tz);
#endif
asmlinkage long (*sys_setgroupsp) (int gidsetsize, gid_t * grouplist);
#ifdef EXPORTED_SYS_CALL_TABLE
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
extern unsigned int sys_call_table[];	/* changed to uint because SPARC64 and S390X have syscalltable of 32bit items */
#else
extern void *sys_call_table[];	/* safer for other linuces */
#endif
#else /* EXPORTED_SYS_CALL_TABLE */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
static unsigned int *sys_call_table;	/* changed to uint because SPARC64 and S390X have syscalltable of 32bit items */
#else
static void **sys_call_table;	/* safer for other linuces */
#endif
#endif

#if defined(AFS_S390X_LINUX24_ENV)
#define _S(x) ((x)<<1)
#else
#define _S(x) x
#endif

extern struct file_system_type afs_fs_type;

static long get_page_offset(void);

#if defined(AFS_LINUX24_ENV)
DECLARE_MUTEX(afs_global_lock);
#else
struct semaphore afs_global_lock = MUTEX;
#endif
int afs_global_owner = 0;
#if !defined(AFS_LINUX24_ENV)
unsigned long afs_linux_page_offset = 0;	/* contains the PAGE_OFFSET value */
#endif

/* Since sys_ni_syscall is not exported, I need to cache it in order to restore
 * it.
 */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
static unsigned int afs_ni_syscall = 0;
#else
static void *afs_ni_syscall = 0;
#endif

#ifdef AFS_AMD64_LINUX20_ENV
#ifdef EXPORTED_IA32_SYS_CALL_TABLE
extern void *ia32_sys_call_table[];
#else
static void **ia32_sys_call_table;
#endif

static void *ia32_ni_syscall = 0;
asmlinkage long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
#if defined(__NR_ia32_setgroups32)
asmlinkage long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
#endif /* __NR_ia32_setgroups32 */
#endif /* AFS_AMD64_LINUX20_ENV */

#ifdef AFS_PPC64_LINUX20_ENV
asmlinkage long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */

#ifdef AFS_SPARC64_LINUX20_ENV
static unsigned int afs_ni_syscall32 = 0;
asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
				    __kernel_gid_t32 * grouplist);
#if defined(__NR_setgroups32)
asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
				      __kernel_gid_t32 * grouplist);
#endif /* __NR_setgroups32 */
#ifdef EXPORTED_SYS_CALL_TABLE
extern unsigned int sys_call_table32[];
#else /* EXPORTED_SYS_CALL_TABLE */
static unsigned int *sys_call_table32;
#endif /* EXPORTED_SYS_CALL_TABLE */

asmlinkage int
afs_syscall32(long syscall, long parm1, long parm2, long parm3, long parm4,
	      long parm5)
{
    __asm__ __volatile__("srl %o4, 0, %o4\n\t" "mov %o7, %i7\n\t"
			 "call afs_syscall\n\t" "srl %o5, 0, %o5\n\t"
			 "ret\n\t" "nop");
}
#endif /* AFS_SPARC64_LINUX20_ENV */

static int afs_ioctl(struct inode *, struct file *, unsigned int,
		     unsigned long);

static struct file_operations afs_syscall_fops = {
    .ioctl = afs_ioctl,
};

static struct proc_dir_entry *openafs_procfs;

static int
afsproc_init()
{
    struct proc_dir_entry *entry1;

    openafs_procfs = proc_mkdir(PROC_FSDIRNAME, proc_root_fs);
    entry1 = create_proc_entry(PROC_SYSCALL_NAME, 0666, openafs_procfs);

    entry1->proc_fops = &afs_syscall_fops;

    entry1->owner = THIS_MODULE;

    return 0;
}

static void
afsproc_exit()
{
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

    if (cmd != VIOC_SYSCALL) return -EINVAL;

    if (copy_from_user(&sysargs, (void *)arg, sizeof(struct afsprocdata)))
	return -EFAULT;

    return afs_syscall(sysargs.syscall, sysargs.param1,
		       sysargs.param2, sysargs.param3, sysargs.param4);
}

#ifdef AFS_IA64_LINUX20_ENV

asmlinkage long
afs_syscall_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
    __asm__ __volatile__("alloc r42 = ar.pfs, 8, 3, 6, 0\n\t" "mov r41 = b0\n\t"	/* save rp */
			 "mov out0 = in0\n\t" "mov out1 = in1\n\t" "mov out2 = in2\n\t" "mov out3 = in3\n\t" "mov out4 = in4\n\t" "mov out5 = gp\n\t"	/* save gp */
			 ";;\n" ".L1:    mov r3 = ip\n\t" ";;\n\t" "addl r15=.fptr_afs_syscall-.L1,r3\n\t" ";;\n\t" "ld8 r15=[r15]\n\t" ";;\n\t" "ld8 r16=[r15],8\n\t" ";;\n\t" "ld8 gp=[r15]\n\t" "mov b6=r16\n\t" "br.call.sptk.many b0 = b6\n\t" ";;\n\t" "mov ar.pfs = r42\n\t" "mov b0 = r41\n\t" "mov gp = r48\n\t"	/* restore gp */
			 "br.ret.sptk.many b0\n" ".fptr_afs_syscall:\n\t"
			 "data8 @fptr(afs_syscall)");
}

asmlinkage long
afs_xsetgroups_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
    __asm__ __volatile__("alloc r42 = ar.pfs, 8, 3, 6, 0\n\t" "mov r41 = b0\n\t"	/* save rp */
			 "mov out0 = in0\n\t" "mov out1 = in1\n\t" "mov out2 = in2\n\t" "mov out3 = in3\n\t" "mov out4 = in4\n\t" "mov out5 = gp\n\t"	/* save gp */
			 ";;\n" ".L2:    mov r3 = ip\n\t" ";;\n\t" "addl r15=.fptr_afs_xsetgroups - .L2,r3\n\t" ";;\n\t" "ld8 r15=[r15]\n\t" ";;\n\t" "ld8 r16=[r15],8\n\t" ";;\n\t" "ld8 gp=[r15]\n\t" "mov b6=r16\n\t" "br.call.sptk.many b0 = b6\n\t" ";;\n\t" "mov ar.pfs = r42\n\t" "mov b0 = r41\n\t" "mov gp = r48\n\t"	/* restore gp */
			 "br.ret.sptk.many b0\n" ".fptr_afs_xsetgroups:\n\t"
			 "data8 @fptr(afs_xsetgroups)");
}

struct fptr {
    void *ip;
    unsigned long gp;
};

#endif /* AFS_IA64_LINUX20_ENV */

#ifdef AFS_LINUX24_ENV
asmlinkage int (*sys_setgroups32p) (int gidsetsize,
				    __kernel_gid32_t * grouplist);
#endif /* AFS_LINUX24_ENV */

#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
#define POINTER2SYSCALL (unsigned int)(unsigned long)
#define SYSCALL2POINTER (void *)(long)
#else
#define POINTER2SYSCALL (void *)
#define SYSCALL2POINTER (void *)
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
int __init
afs_init(void)
#else
int
init_module(void)
#endif
{
#if defined(AFS_IA64_LINUX20_ENV)
    unsigned long kernel_gp = 0;
    static struct fptr sys_setgroups;
#endif /* defined(AFS_IA64_LINUX20_ENV) */
    extern long afs_xsetgroups();
#if defined(__NR_setgroups32)
    extern int afs_xsetgroups32();
#endif /* __NR_setgroups32 */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined (AFS_AMD64_LINUX20_ENV) || defined(AFS_PPC64_LINUX20_ENV)
    extern int afs32_xsetgroups();
#if (defined(__NR_setgroups32) && defined(AFS_SPARC64_LINUX20_ENV))
    extern int afs32_xsetgroups32();
#endif /* defined(__NR_setgroups32) && defined(AFS_SPARC64_LINUX20_ENV) */
#if (defined(__NR_ia32_setgroups32) && defined(AFS_AMD64_LINUX20_ENV))
    extern int afs32_xsetgroups32();
#endif /* (defined(__NR_ia32_setgroups32) && defined(AFS_AMD64_LINUX20_ENV) */
#endif /* AFS_SPARC64_LINUX20_ENV || AFS_AMD64_LINUX20_ENV || AFS_PPC64_LINUX20_ENV */

#if !defined(EXPORTED_SYS_CALL_TABLE) || (defined(AFS_AMD64_LINUX20_ENV) && !defined(EXPORTED_IA32_SYS_CALL_TABLE))
    unsigned long *ptr;
    unsigned long offset=0;
    unsigned long datalen=0;
    int ret;
    unsigned long token=0;
    char *mod_name;
    unsigned long mod_start=0;
    unsigned long mod_end=0;
    char *sec_name;
    unsigned long sec_start=0;
    unsigned long sec_end=0;
    char *sym_name;
    unsigned long sym_start=0;
    unsigned long sym_end=0;
#endif /* EXPORTED_SYS_CALL_TABLE */

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

#ifndef EXPORTED_SYS_CALL_TABLE
    sys_call_table = 0;

#ifdef EXPORTED_KALLSYMS_SYMBOL
    ret = 1;
    token = 0;
    while (ret) {
	sym_start = 0;
	ret =
	    kallsyms_symbol_to_address("sys_call_table", &token, &mod_name,
				       &mod_start, &mod_end, &sec_name,
				       &sec_start, &sec_end, &sym_name,
				       &sym_start, &sym_end);
	if (ret && !strcmp(mod_name, "kernel"))
	    break;
    }
    if (ret && sym_start) {
	sys_call_table = sym_start;
    }
#elif defined(EXPORTED_KALLSYMS_ADDRESS)
    ret =
	kallsyms_address_to_symbol((unsigned long)&init_mm, &mod_name,
				   &mod_start, &mod_end, &sec_name,
				   &sec_start, &sec_end, &sym_name,
				   &sym_start, &sym_end);
    ptr = (unsigned long *)sec_start;
    datalen = (sec_end - sec_start) / sizeof(unsigned long);
#elif defined(AFS_IA64_LINUX20_ENV)
    ptr = (unsigned long *)(&sys_close - 0x180000);
    datalen = 0x180000 / sizeof(ptr);
#elif defined(AFS_AMD64_LINUX20_ENV)
    ptr = (unsigned long *)&init_mm;
    datalen = 0x360000 / sizeof(ptr);
#else
    ptr = (unsigned long *)&init_mm;
    datalen = 16384;
#endif
    for (offset = 0; offset < datalen; ptr++, offset++) {
#if defined(AFS_IA64_LINUX20_ENV)
	unsigned long close_ip =
	    (unsigned long)((struct fptr *)&sys_close)->ip;
	unsigned long chdir_ip =
	    (unsigned long)((struct fptr *)&sys_chdir)->ip;
	unsigned long write_ip =
	    (unsigned long)((struct fptr *)&sys_write)->ip;
	if (ptr[0] == close_ip && ptr[__NR_chdir - __NR_close] == chdir_ip
	    && ptr[__NR_write - __NR_close] == write_ip) {
	    sys_call_table = (void *)&(ptr[-1 * (__NR_close - 1024)]);
	    break;
	}
#elif defined(EXPORTED_SYS_WAIT4) && defined(EXPORTED_SYS_CLOSE)
	if (ptr[0] == (unsigned long)&sys_close
	    && ptr[__NR_wait4 - __NR_close] == (unsigned long)&sys_wait4) {
	    sys_call_table = ptr - __NR_close;
	    break;
	}
#elif defined(EXPORTED_SYS_CHDIR) && defined(EXPORTED_SYS_CLOSE)
	if (ptr[0] == (unsigned long)&sys_close
	    && ptr[__NR_chdir - __NR_close] == (unsigned long)&sys_chdir) {
	    sys_call_table = ptr - __NR_close;
	    break;
	}
#elif defined(EXPORTED_SYS_OPEN)
	if (ptr[0] == (unsigned long)&sys_exit
	    && ptr[__NR_open - __NR_exit] == (unsigned long)&sys_open) {
	    sys_call_table = ptr - __NR_exit;
	    break;
	}
#else /* EXPORTED_SYS_OPEN */
	break;
#endif /* EXPORTED_KALLSYMS_ADDRESS */
    }
#ifdef EXPORTED_KALLSYMS_ADDRESS
    ret =
	kallsyms_address_to_symbol((unsigned long)sys_call_table, &mod_name,
				   &mod_start, &mod_end, &sec_name,
				   &sec_start, &sec_end, &sym_name,
				   &sym_start, &sym_end);
    if (ret && strcmp(sym_name, "sys_call_table"))
	sys_call_table = 0;
#endif /* EXPORTED_KALLSYMS_ADDRESS */
    if (!sys_call_table) {
	printf("Failed to find address of sys_call_table\n");
    } else {
	printf("Found sys_call_table at %x\n", sys_call_table);
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
	error cant support this yet.;
#endif /* AFS_SPARC64_LINUX20_ENV */
#endif /* EXPORTED_SYS_CALL_TABLE */

#ifdef AFS_AMD64_LINUX20_ENV
#ifndef EXPORTED_IA32_SYS_CALL_TABLE
	ia32_sys_call_table = 0;
#ifdef EXPORTED_KALLSYMS_SYMBOL
	ret = 1;
	token = 0;
	while (ret) {
	    sym_start = 0;
	    ret = kallsyms_symbol_to_address("ia32_sys_call_table", &token,
					     &mod_name, &mod_start, &mod_end,
					     &sec_name, &sec_start, &sec_end,
					     &sym_name, &sym_start, &sym_end);
	    if (ret && !strcmp(mod_name, "kernel"))
		break;
	}
	if (ret && sym_start) {
	    ia32_sys_call_table = sym_start;
	}
#else /* EXPORTED_KALLSYMS_SYMBOL */
#ifdef EXPORTED_KALLSYMS_ADDRESS
	ret = kallsyms_address_to_symbol((unsigned long)
					 &interruptible_sleep_on,
					 &mod_name, &mod_start, &mod_end,
					 &sec_name, &sec_start, &sec_end,
					 &sym_name, &sym_start, &sym_end);
	ptr = (unsigned long *)sec_start;
	datalen = (sec_end - sec_start) / sizeof(unsigned long);
#else /* EXPORTED_KALLSYMS_ADDRESS */
#if defined(AFS_AMD64_LINUX20_ENV)
	ptr = (unsigned long *)&interruptible_sleep_on;
	datalen = 0x180000 / sizeof(ptr);
#else /* AFS_AMD64_LINUX20_ENV */
	ptr = (unsigned long *)&interruptible_sleep_on;
	datalen = 16384;
#endif /* AFS_AMD64_LINUX20_ENV */
#endif /* EXPORTED_KALLSYMS_ADDRESS */
	for (offset = 0; offset < datalen; ptr++, offset++) {
	    if (ptr[0] == (unsigned long)&sys_exit
		&& ptr[__NR_ia32_open - __NR_ia32_exit] ==
		(unsigned long)&sys_open) {
		    ia32_sys_call_table = ptr - __NR_ia32_exit;
		    break;
	    }
	}
#ifdef EXPORTED_KALLSYMS_ADDRESS
	ret = kallsyms_address_to_symbol((unsigned long)ia32_sys_call_table,
					 &mod_name, &mod_start, &mod_end,
					 &sec_name, &sec_start, &sec_end,
					 &sym_name, &sym_start, &sym_end);
	if (ret && strcmp(sym_name, "ia32_sys_call_table"))
	    ia32_sys_call_table = 0;
#endif /* EXPORTED_KALLSYMS_ADDRESS */
#endif /* EXPORTED_KALLSYMS_SYMBOL */
	if (!ia32_sys_call_table) {
	    printf("Warning: Failed to find address of ia32_sys_call_table\n");
	} else {
	    printf("Found ia32_sys_call_table at %x\n", ia32_sys_call_table);
	}
#else
	printf("Found ia32_sys_call_table at %x\n", ia32_sys_call_table);
#endif /* IA32_SYS_CALL_TABLE */
#endif

	/* Initialize pointers to kernel syscalls. */
#if !defined(AFS_LINUX24_ENV)
	sys_settimeofdayp = SYSCALL2POINTER sys_call_table[_S(__NR_settimeofday)];
#endif /* AFS_IA64_LINUX20_ENV */

	/* setup AFS entry point. */
	if (
#if defined(AFS_IA64_LINUX20_ENV)
		SYSCALL2POINTER sys_call_table[__NR_afs_syscall - 1024]
#else
		SYSCALL2POINTER sys_call_table[_S(__NR_afs_syscall)]
#endif
		== afs_syscall) {
	    printf("AFS syscall entry point already in use!\n");
	    return -EBUSY;
	}
#if defined(AFS_IA64_LINUX20_ENV)
	afs_ni_syscall = sys_call_table[__NR_afs_syscall - 1024];
	sys_call_table[__NR_afs_syscall - 1024] =
		POINTER2SYSCALL((struct fptr *)afs_syscall_stub)->ip;
#else /* AFS_IA64_LINUX20_ENV */
	afs_ni_syscall = sys_call_table[_S(__NR_afs_syscall)];
	sys_call_table[_S(__NR_afs_syscall)] = POINTER2SYSCALL afs_syscall;
#ifdef AFS_SPARC64_LINUX20_ENV
	afs_ni_syscall32 = sys_call_table32[__NR_afs_syscall];
	sys_call_table32[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall32;
#endif
#endif /* AFS_IA64_LINUX20_ENV */
#ifdef AFS_AMD64_LINUX20_ENV
	if (ia32_sys_call_table) {
	    ia32_ni_syscall = ia32_sys_call_table[__NR_ia32_afs_syscall];
	    ia32_sys_call_table[__NR_ia32_afs_syscall] =
		    POINTER2SYSCALL afs_syscall;
	}
#endif /* AFS_S390_LINUX22_ENV */
#ifndef EXPORTED_SYS_CALL_TABLE
    }
#endif /* EXPORTED_SYS_CALL_TABLE */
    osi_Init();
    register_filesystem(&afs_fs_type);

    /* Intercept setgroups calls */
    if (sys_call_table) {
#if defined(AFS_IA64_LINUX20_ENV)
    sys_setgroupsp = (void *)&sys_setgroups;

    ((struct fptr *)sys_setgroupsp)->ip =
	SYSCALL2POINTER sys_call_table[__NR_setgroups - 1024];
    ((struct fptr *)sys_setgroupsp)->gp = kernel_gp;

    sys_call_table[__NR_setgroups - 1024] =
	POINTER2SYSCALL((struct fptr *)afs_xsetgroups_stub)->ip;
#else /* AFS_IA64_LINUX20_ENV */
    sys_setgroupsp = SYSCALL2POINTER sys_call_table[_S(__NR_setgroups)];
    sys_call_table[_S(__NR_setgroups)] = POINTER2SYSCALL afs_xsetgroups;
#ifdef AFS_SPARC64_LINUX20_ENV
    sys32_setgroupsp = SYSCALL2POINTER sys_call_table32[__NR_setgroups];
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL afs32_xsetgroups;
#endif /* AFS_SPARC64_LINUX20_ENV */
#if defined(__NR_setgroups32)
    sys_setgroups32p = SYSCALL2POINTER sys_call_table[__NR_setgroups32];
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL afs_xsetgroups32;
#ifdef AFS_SPARC64_LINUX20_ENV
	sys32_setgroups32p =
	    SYSCALL2POINTER sys_call_table32[__NR_setgroups32];
	sys_call_table32[__NR_setgroups32] =
	    POINTER2SYSCALL afs32_xsetgroups32;
#endif /* AFS_SPARC64_LINUX20_ENV */
#endif /* __NR_setgroups32 */
#ifdef AFS_AMD64_LINUX20_ENV
    if (ia32_sys_call_table) {
	sys32_setgroupsp =
	    SYSCALL2POINTER ia32_sys_call_table[__NR_ia32_setgroups];
	ia32_sys_call_table[__NR_ia32_setgroups] =
	    POINTER2SYSCALL afs32_xsetgroups;
#if defined(__NR_ia32_setgroups32)
	sys32_setgroups32p =
	    SYSCALL2POINTER ia32_sys_call_table[__NR_ia32_setgroups32];
	ia32_sys_call_table[__NR_ia32_setgroups32] =
	    POINTER2SYSCALL afs32_xsetgroups32;
#endif /* __NR_ia32_setgroups32 */
    }
#endif /* AFS_AMD64_LINUX20_ENV */
#endif /* AFS_IA64_LINUX20_ENV */

    }

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
    struct task_struct *t;

    osi_sysctl_clean();
    if (sys_call_table) {
#if defined(AFS_IA64_LINUX20_ENV)
    sys_call_table[__NR_setgroups - 1024] =
	POINTER2SYSCALL((struct fptr *)sys_setgroupsp)->ip;
    sys_call_table[__NR_afs_syscall - 1024] = afs_ni_syscall;
#else /* AFS_IA64_LINUX20_ENV */
    sys_call_table[_S(__NR_setgroups)] = POINTER2SYSCALL sys_setgroupsp;
    sys_call_table[_S(__NR_afs_syscall)] = afs_ni_syscall;
# ifdef AFS_SPARC64_LINUX20_ENV
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL sys32_setgroupsp;
    sys_call_table32[__NR_afs_syscall] = afs_ni_syscall32;
# endif
# if defined(__NR_setgroups32)
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL sys_setgroups32p;
# ifdef AFS_SPARC64_LINUX20_ENV
	sys_call_table32[__NR_setgroups32] =
	    POINTER2SYSCALL sys32_setgroups32p;
# endif
# endif
#endif /* AFS_IA64_LINUX20_ENV */
#ifdef AFS_AMD64_LINUX20_ENV
    if (ia32_sys_call_table) {
	ia32_sys_call_table[__NR_ia32_setgroups] =
	    POINTER2SYSCALL sys32_setgroupsp;
	ia32_sys_call_table[__NR_ia32_afs_syscall] =
	    POINTER2SYSCALL ia32_ni_syscall;
# if defined(__NR_setgroups32)
	ia32_sys_call_table[__NR_ia32_setgroups32] =
	    POINTER2SYSCALL sys32_setgroups32p;
#endif
    }
#endif
    }
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
