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
#include "../afs/param.h"
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../h/unistd.h" /* For syscall numbers. */
#include "../h/mm.h"

#include <linux/module.h>



asmlinkage int (*sys_settimeofdayp)(struct timeval *tv, struct timezone *tz);
#if !defined(AFS_ALPHA_LINUX20_ENV)
asmlinkage int (*sys_socketcallp)(int call, long *args);
#endif /* no socketcall on alpha */
asmlinkage int (*sys_killp)(int pid, int signal);
asmlinkage int (*sys_setgroupsp)(int gidsetsize, gid_t *grouplist);

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
asmlinkage int (*sys_setgroupsp32)(int gidsetsize, __kernel_gid_t32 *grouplist);
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

#if defined(AFS_LINUX24_ENV)
asmlinkage int (*sys_setgroups32p)(int gidsetsize, __kernel_gid32_t *grouplist);
#endif 

#ifdef AFS_SPARC64_LINUX20_ENV
#define POINTER2SYSCALL (unsigned int)(unsigned long)
#define SYSCALL2POINTER (void *)(long)
#else
#define POINTER2SYSCALL (void *)
#define SYSCALL2POINTER (void *)
#endif

int init_module(void)
{
    extern int afs_syscall();
    extern int afs_xsetgroups();
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_LINUX24_ENV)
    extern int afs_xsetgroups32();
#endif

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
    sys_settimeofdayp = SYSCALL2POINTER sys_call_table[__NR_settimeofday];
#if !defined(AFS_ALPHA_LINUX20_ENV)
    sys_socketcallp = SYSCALL2POINTER sys_call_table[__NR_socketcall];
#endif /* no socketcall on alpha */
    sys_killp = SYSCALL2POINTER sys_call_table[__NR_kill];

    /* setup AFS entry point. */
    if (SYSCALL2POINTER sys_call_table[__NR_afs_syscall] == afs_syscall) {
	printf("AFS syscall entry point already in use!\n");
	return -EBUSY;
    }


    afs_ni_syscall = sys_call_table[__NR_afs_syscall];
    sys_call_table[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall;
#ifdef AFS_SPARC64_LINUX20_ENV
    afs_ni_syscall32 = sys_call_table32[__NR_afs_syscall];
    sys_call_table32[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall32;
#endif

    osi_Init();
    register_filesystem(&afs_file_system);

    /* Intercept setgroups calls */
    sys_setgroupsp = SYSCALL2POINTER sys_call_table[__NR_setgroups];
    sys_call_table[__NR_setgroups] = POINTER2SYSCALL afs_xsetgroups;
#ifdef AFS_SPARC64_LINUX20_ENV
    sys_setgroupsp32 = SYSCALL2POINTER sys_call_table32[__NR_setgroups];
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL afs_xsetgroups32;
#endif
#if defined(AFS_LINUX24_ENV)
    sys_setgroups32p = SYSCALL2POINTER sys_call_table[__NR_setgroups32];
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL afs_xsetgroups32;
#endif

    return 0;
}

void cleanup_module(void)
{
    struct task_struct *t;

    sys_call_table[__NR_setgroups] = POINTER2SYSCALL sys_setgroupsp;
    sys_call_table[__NR_afs_syscall] = afs_ni_syscall;
#ifdef AFS_SPARC64_LINUX20_ENV
    sys_call_table32[__NR_setgroups] = POINTER2SYSCALL sys_setgroupsp32;
    sys_call_table32[__NR_afs_syscall] = afs_ni_syscall32;
#endif
#if defined(AFS_LINUX24_ENV)
    sys_call_table[__NR_setgroups32] = POINTER2SYSCALL sys_setgroups32p;
#endif
    unregister_filesystem(&afs_file_system);

    osi_linux_free_inode_pages(); /* Invalidate all pages using AFS inodes. */
    osi_linux_free_afs_memory();

    return;
}

static long get_page_offset(void)
{
#if defined(AFS_PPC_LINUX22_ENV) || defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_SPARC_LINUX20_ENV) || defined(AFS_ALPHA_LINUX20_ENV) || defined(AFS_S390_LINUX22_ENV)
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
