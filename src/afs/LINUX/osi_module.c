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

RCSID("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/unistd.h" /* For syscall numbers. */
#include "h/mm.h"

#include <linux/module.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#endif



#ifdef AFS_SPARC64_LINUX24_ENV
#define __NR_setgroups32      82 /* This number is not exported for some bizarre reason. */
#endif

asmlinkage int (*sys_settimeofdayp)(struct timeval *tv, struct timezone *tz);
asmlinkage int (*sys_killp)(int pid, int signal);
asmlinkage long (*sys_setgroupsp)(int gidsetsize, gid_t *grouplist);

#ifdef AFS_SPARC64_LINUX20_ENV
extern unsigned int sys_call_table[];  /* changed to uint because SPARC64 has syscaltable of 32bit items */
#else
extern void * sys_call_table[]; /* safer for other linuces */
#endif
extern struct file_system_type afs_file_system;

static long get_page_offset(void);

#if defined(AFS_LINUX24_ENV)
DECLARE_MUTEX(afs_global_lock);
#else
struct semaphore afs_global_lock = MUTEX;
#endif
int afs_global_owner = 0;
unsigned long afs_linux_page_offset = 0; /* contains the PAGE_OFFSET value */

/* Since sys_ni_syscall is not exported, I need to cache it in order to restore
 * it.
 */
#ifdef AFS_SPARC64_LINUX20_ENV
static unsigned int afs_ni_syscall = 0;
#else
static void* afs_ni_syscall = 0;
#endif
 
#ifdef AFS_SPARC64_LINUX20_ENV
static unsigned int afs_ni_syscall32 = 0;
asmlinkage int (*sys32_setgroupsp)(int gidsetsize, __kernel_gid_t32 *grouplist);
#if defined(__NR_setgroups32)
asmlinkage int (*sys32_setgroups32p)(int gidsetsize, __kernel_gid_t32 *grouplist);
#endif
extern unsigned int sys_call_table32[];

asmlinkage int afs_syscall32(long syscall, long parm1, long parm2, long parm3,
			     long parm4, long parm5)
{
__asm__ __volatile__ ("
	srl %o4, 0, %o4
	mov %o7, %i7
	call afs_syscall
	srl %o5, 0, %o5
	ret
	nop
");
}
#endif

#ifdef AFS_IA64_LINUX20_ENV

asmlinkage long
afs_syscall_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
__asm__ __volatile__ ("
        alloc r42 = ar.pfs, 8, 3, 6, 0
        mov r41 = b0    		/* save rp */
        mov out0 = in0
        mov out1 = in1
        mov out2 = in2
        mov out3 = in3
        mov out4 = in4
        mov out5 = gp			/* save gp */
        ;;
.L1:    mov r3 = ip
        ;;
        addl r15=.fptr_afs_syscall-.L1,r3
        ;;
        ld8 r15=[r15]
        ;;
        ld8 r16=[r15],8
        ;;
        ld8 gp=[r15]
        mov b6=r16
        br.call.sptk.many b0 = b6
        ;;
        mov ar.pfs = r42
        mov b0 = r41
        mov gp = r48			/* restore gp */
        br.ret.sptk.many b0
.fptr_afs_syscall:
        data8 @fptr(afs_syscall)
");
}

asmlinkage long
afs_xsetgroups_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
__asm__ __volatile__ ("
        alloc r42 = ar.pfs, 8, 3, 6, 0
        mov r41 = b0    		/* save rp */
        mov out0 = in0
        mov out1 = in1
        mov out2 = in2
        mov out3 = in3
        mov out4 = in4
        mov out5 = gp			/* save gp */
        ;;
.L2:    mov r3 = ip
        ;;
        addl r15=.fptr_afs_xsetgroups - .L2,r3
        ;;
        ld8 r15=[r15]
        ;;
        ld8 r16=[r15],8
        ;;
        ld8 gp=[r15]
        mov b6=r16
        br.call.sptk.many b0 = b6
        ;;
        mov ar.pfs = r42
        mov b0 = r41
        mov gp = r48			/* restore gp */
        br.ret.sptk.many b0
.fptr_afs_xsetgroups:
        data8 @fptr(afs_xsetgroups)
");
}

struct fptr
{
	void *ip;
	unsigned long gp;
};

#endif /* AFS_IA64_LINUX20_ENV */

#ifdef AFS_LINUX24_ENV
asmlinkage int (*sys_setgroups32p)(int gidsetsize, __kernel_gid32_t *grouplist);
#endif 

#ifdef AFS_SPARC64_LINUX20_ENV
#define POINTER2SYSCALL (unsigned int)(unsigned long)
#define SYSCALL2POINTER (void *)(long)
#else
#define POINTER2SYSCALL (void *)
#define SYSCALL2POINTER (void *)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
int __init afs_init(void)
#else
int init_module(void)
#endif
{
#if defined(AFS_IA64_LINUX20_ENV)
    unsigned long kernel_gp;
    static struct fptr sys_kill, sys_settimeofday, sys_setgroups;
#endif
    extern int afs_syscall();
    extern long afs_xsetgroups();
#if defined(__NR_setgroups32)
    extern int afs_xsetgroups32();
#endif
#ifdef AFS_SPARC64_LINUX20_ENV
    extern int afs32_xsetgroups();
#if defined(__NR_setgroups32)
    extern int afs32_xsetgroups32();
#endif
#endif

    RWLOCK_INIT(&afs_xosi, "afs_xosi");

    /* obtain PAGE_OFFSET value */
    afs_linux_page_offset = get_page_offset();

#ifndef AFS_S390_LINUX22_ENV
    if (afs_linux_page_offset == 0) {
        /* couldn't obtain page offset so can't continue */
        printf("afs: Unable to obtain PAGE_OFFSET. Exiting..");
        return -EIO;
    }
#endif

    /* Initialize pointers to kernel syscalls. */
#if defined(AFS_IA64_LINUX20_ENV)
    kernel_gp = ((struct fptr *)printk)->gp;

    sys_settimeofdayp = (void *) &sys_settimeofday;
    sys_killp = (void *) &sys_kill;

    ((struct fptr *)sys_settimeofdayp)->ip =
		SYSCALL2POINTER sys_call_table[__NR_settimeofday - 1024];
    ((struct fptr *)sys_settimeofdayp)->gp = kernel_gp;
    
    ((struct fptr *)sys_killp)->ip =
		SYSCALL2POINTER sys_call_table[__NR_kill - 1024];
    ((struct fptr *)sys_killp)->gp = kernel_gp;
#else /* !AFS_IA64_LINUX20_ENV */
    sys_settimeofdayp = SYSCALL2POINTER sys_call_table[__NR_settimeofday];
    sys_killp = SYSCALL2POINTER sys_call_table[__NR_kill];
#endif /* AFS_IA64_LINUX20_ENV */

    /* setup AFS entry point. */
    if (
#if defined(AFS_IA64_LINUX20_ENV)
	SYSCALL2POINTER sys_call_table[__NR_afs_syscall - 1024]
#else
	SYSCALL2POINTER sys_call_table[__NR_afs_syscall] 
#endif
	== afs_syscall) {
	printf("AFS syscall entry point already in use!\n");
	return -EBUSY;
    }


#if defined(AFS_IA64_LINUX20_ENV)
    afs_ni_syscall = sys_call_table[__NR_afs_syscall - 1024];
    sys_call_table[__NR_afs_syscall - 1024] = POINTER2SYSCALL ((struct fptr *)afs_syscall_stub)->ip;
#else /* AFS_IA64_LINUX20_ENV */
    afs_ni_syscall = sys_call_table[__NR_afs_syscall];
    sys_call_table[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall;
# ifdef AFS_SPARC64_LINUX20_ENV
    afs_ni_syscall32 = sys_call_table32[__NR_afs_syscall];
    sys_call_table32[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall32;
# endif
#endif /* AFS_IA64_LINUX20_ENV */

    osi_Init();
    register_filesystem(&afs_file_system);

    /* Intercept setgroups calls */
#if defined(AFS_IA64_LINUX20_ENV)
    sys_setgroupsp = (void *) &sys_setgroups;

    ((struct fptr *)sys_setgroupsp)->ip =
		SYSCALL2POINTER sys_call_table[__NR_setgroups - 1024];
    ((struct fptr *)sys_setgroupsp)->gp = kernel_gp;

    sys_call_table[__NR_setgroups - 1024] = POINTER2SYSCALL ((struct fptr *)afs_xsetgroups_stub)->ip;
#else /* AFS_IA64_LINUX20_ENV */
    sys_setgroupsp = SYSCALL2POINTER sys_call_table[__NR_setgroups];
    sys_call_table[__NR_setgroups] = POINTER2SYSCALL afs_xsetgroups;
# ifdef AFS_SPARC64_LINUX20_ENV
    sys32_setgroupsp = SYSCALL2POINTER sys_call_table32[__NR_setgroups];
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL afs32_xsetgroups;
# endif
# if defined(__NR_setgroups32)
    sys_setgroups32p = SYSCALL2POINTER sys_call_table[__NR_setgroups32];
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL afs_xsetgroups32;
# ifdef AFS_SPARC64_LINUX20_ENV
    sys32_setgroups32p = SYSCALL2POINTER sys_call_table32[__NR_setgroups32];
    sys_call_table32[__NR_setgroups32] = POINTER2SYSCALL afs32_xsetgroups32;
# endif
# endif
#endif /* AFS_IA64_LINUX20_ENV */

    osi_sysctl_init();

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
void __exit afs_cleanup(void)
#else
void cleanup_module(void)
#endif
{
    struct task_struct *t;

    osi_sysctl_clean();

#if defined(AFS_IA64_LINUX20_ENV)
    sys_call_table[__NR_setgroups - 1024] = POINTER2SYSCALL ((struct fptr *) sys_setgroupsp)->ip;
    sys_call_table[__NR_afs_syscall - 1024] = afs_ni_syscall;
#else /* AFS_IA64_LINUX20_ENV */
    sys_call_table[__NR_setgroups] = POINTER2SYSCALL sys_setgroupsp;
    sys_call_table[__NR_afs_syscall] = afs_ni_syscall;
# ifdef AFS_SPARC64_LINUX20_ENV
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL sys32_setgroupsp;
    sys_call_table32[__NR_afs_syscall] = afs_ni_syscall32;
# endif
# if defined(__NR_setgroups32)
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL sys_setgroups32p;
# ifdef AFS_SPARC64_LINUX20_ENV
    sys_call_table32[__NR_setgroups32] = POINTER2SYSCALL sys32_setgroups32p;
# endif
# endif
#endif /* AFS_IA64_LINUX20_ENV */
    unregister_filesystem(&afs_file_system);

    osi_linux_free_inode_pages(); /* Invalidate all pages using AFS inodes. */
    osi_linux_free_afs_memory();

    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
module_init(afs_init);
module_exit(afs_cleanup);
#endif


static long get_page_offset(void)
{
#if defined(AFS_PPC_LINUX22_ENV) || defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV) || defined(AFS_ALPHA_LINUX20_ENV) || defined(AFS_S390_LINUX22_ENV) || defined(AFS_IA64_LINUX20_ENV) || defined(AFS_PARISC_LINUX24_ENV)
    return PAGE_OFFSET;
#else
    struct task_struct *p;

    /* search backward thru the circular list */
    for(p = current; p; p = p->prev_task)
	if (p->pid == 1)
	    return p->addr_limit.seg;

    return 0;
#endif
}
