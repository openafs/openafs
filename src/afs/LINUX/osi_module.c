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
#include "../afs/param.h"

RCSID("$Header: /tmp/cvstemp/openafs/src/afs/LINUX/osi_module.c,v 1.8 2001/10/14 18:43:26 hartmans Exp $");

#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../h/unistd.h" /* For syscall numbers. */
#include "../h/mm.h"

#include <linux/module.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#endif



#ifdef AFS_SPARC64_LINUX24_ENV
#define __NR_setgroups32      82 /* This number is not exported for some bizarre reason. */
#endif

asmlinkage int (*sys_settimeofdayp)(struct timeval *tv, struct timezone *tz);
#if !defined(AFS_ALPHA_LINUX20_ENV)
asmlinkage int (*sys_socketcallp)(int call, long *args);
#endif /* no socketcall on alpha */
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
unsigned char ia64_syscall_stub[] =
{
  0x00, 0x50, 0x45, 0x16, 0x80, 0x05,   //  [MII]  alloc r42=ar.pfs,8,3,6,0
  0x90, 0x02, 0x00, 0x62, 0x00, 0x60,   //         mov r41=b0
  0x05, 0x00, 0x01, 0x84,               //         mov r43=r32
  0x00, 0x60, 0x01, 0x42, 0x00, 0x21,   //  [MII]  mov r44=r33
  0xd0, 0x02, 0x88, 0x00, 0x42, 0xc0,   //         mov r45=r34
  0x05, 0x18, 0x01, 0x84,               //         mov r46=r35
  0x0d, 0x78, 0x01, 0x48, 0x00, 0x21,   //  [MFI]  mov r47=r36
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00,   //         nop.f 0x0
  0x06, 0x08, 0x00, 0x84,               //         mov r48=gp;;
  0x05, 0x00, 0x00, 0x00, 0x01, 0x00,   //  [MLX]  nop.m 0x0
  0x00, 0x00, 0x00, 0x00, 0x00, 0xe0,   //         movl r15=0x0;;
  0x01, 0x00, 0x00, 0x60,               //
  0x0a, 0x80, 0x20, 0x1e, 0x18, 0x14,   //  [MMI]  ld8 r16=[r15],8;;
  0x10, 0x00, 0x3c, 0x30, 0x20, 0xc0,   //         ld8 gp=[r15]
  0x00, 0x09, 0x00, 0x07,               //         mov b6=r16
  0x1d, 0x00, 0x00, 0x00, 0x01, 0x00,   //  [MFB]  nop.m 0x0
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00,   //         nop.f 0x0
  0x68, 0x00, 0x00, 0x10,               //         br.call.sptk.many b0=b6;;
  0x00, 0x00, 0x00, 0x00, 0x01, 0x00,   //  [MII]  nop.m 0x0
  0x00, 0x50, 0x01, 0x55, 0x00, 0x00,   //         mov.i ar.pfs=r42
  0x90, 0x0a, 0x00, 0x07,               //         mov b0=r41
  0x1d, 0x08, 0x00, 0x60, 0x00, 0x21,   //  [MFB]  mov gp=r48
  0x00, 0x00, 0x00, 0x02, 0x00, 0x80,   //         nop.f 0x0
  0x08, 0x00, 0x84, 0x00                //         br.ret.sptk.many b0;;
};

void ia64_imm64_fixup(unsigned long v, void *code)
{
        unsigned long *bundle = (unsigned long *) code;

        unsigned long insn;
        unsigned long slot1;

        insn = ((v & 0x8000000000000000) >> 27) | ((v & 0x0000000000200000)) |
           ((v & 0x00000000001f0000) <<  6) | ((v & 0x000000000000ff80) << 20) |
           ((v & 0x000000000000007f) << 13);

        slot1 = (v & 0x7fffffffffc00000) >> 22;

        *bundle |= slot1 << 46;
        *(bundle+1) |= insn << 23;
        *(bundle+1) |= slot1 >> 18;
}

unsigned char *afs_syscall_stub, *afs_xsetgroups_stub;

struct fptr
{
	unsigned long ip;
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
#ifdef __NR_socketcall
    sys_socketcallp = SYSCALL2POINTER sys_call_table[__NR_socketcall];
#endif /* no socketcall on alpha */
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

    afs_syscall_stub = (void *) kmalloc(sizeof(ia64_syscall_stub), GFP_KERNEL);
    memcpy(afs_syscall_stub, ia64_syscall_stub, sizeof(ia64_syscall_stub));
    ia64_imm64_fixup((unsigned long)afs_syscall, afs_syscall_stub+0x30);
    sys_call_table[__NR_afs_syscall - 1024] = POINTER2SYSCALL afs_syscall_stub;
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

    afs_xsetgroups_stub = (void *) kmalloc(sizeof(ia64_syscall_stub), GFP_KERNEL);
    memcpy(afs_xsetgroups_stub, ia64_syscall_stub, sizeof(ia64_syscall_stub));
    ia64_imm64_fixup((unsigned long)afs_xsetgroups, afs_xsetgroups_stub+0x30);

    ((struct fptr *)sys_setgroupsp)->ip =
		SYSCALL2POINTER sys_call_table[__NR_setgroups - 1024];
    ((struct fptr *)sys_setgroupsp)->gp = kernel_gp;

    sys_call_table[__NR_setgroups - 1024] = POINTER2SYSCALL afs_xsetgroups_stub;
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

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
void __exit afs_cleanup(void)
#else
void cleanup_module(void)
#endif
{
    struct task_struct *t;

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
