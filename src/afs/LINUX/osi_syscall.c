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
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_syscall.c,v 1.1.2.2 2005/02/21 01:15:35 shadow Exp $");

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


/* On SPARC64 and S390X, sys_call_table contains 32-bit entries
 * even though pointers are 64 bit quantities.
 * XXX unify this with osi_probe.c
 */
#if defined(AFS_SPARC64_LINUX20_ENV) || defined(AFS_S390X_LINUX24_ENV)
#define SYSCALLTYPE unsigned int
#define POINTER2SYSCALL (unsigned int)(unsigned long)
#define SYSCALL2POINTER (void *)(long)
#else
#define SYSCALLTYPE void *
#define POINTER2SYSCALL (void *)
#define SYSCALL2POINTER (void *)
#endif

#if defined(AFS_S390X_LINUX24_ENV) && !defined(AFS_LINUX26_ENV)
#define _S(x) ((x)<<1)
#elif defined(AFS_IA64_LINUX20_ENV)
#define _S(x) ((x)-1024)
#else
#define _S(x) x
#endif


/***** ALL PLATFORMS *****/
extern asmlinkage long
afs_syscall(long syscall, long parm1, long parm2, long parm3, long parm4);

static SYSCALLTYPE *afs_sys_call_table;
static SYSCALLTYPE afs_ni_syscall = 0;

extern long afs_xsetgroups();
asmlinkage long (*sys_setgroupsp) (int gidsetsize, gid_t * grouplist);

#ifdef AFS_LINUX24_ENV
extern int afs_xsetgroups32();
asmlinkage int (*sys_setgroups32p) (int gidsetsize,
				    __kernel_gid32_t * grouplist);
#endif

#if !defined(AFS_LINUX24_ENV)
asmlinkage int (*sys_settimeofdayp) (struct timeval * tv, struct timezone * tz);
#endif


/***** AMD64 *****/
#ifdef AFS_AMD64_LINUX20_ENV
static SYSCALLTYPE *afs_ia32_sys_call_table;
static SYSCALLTYPE ia32_ni_syscall = 0;

extern int afs32_xsetgroups();
asmlinkage long (*sys32_setgroupsp) (int gidsetsize, u16 * grouplist);
#ifdef AFS_LINUX24_ENV
extern int afs32_xsetgroups32();
asmlinkage long (*sys32_setgroups32p) (int gidsetsize, gid_t * grouplist);
#endif /* __NR_ia32_setgroups32 */
#endif /* AFS_AMD64_LINUX20_ENV */


/***** PPC64 *****/
#ifdef AFS_PPC64_LINUX20_ENV
extern SYSCALLTYPE *afs_sys_call_table32;
static SYSCALLTYPE afs_ni_syscall32 = 0;

extern int afs32_xsetgroups();
asmlinkage long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);
#endif /* AFS_AMD64_LINUX20_ENV */


/***** SPARC64 *****/
#ifdef AFS_SPARC64_LINUX20_ENV
extern SYSCALLTYPE *afs_sys_call_table32;
static SYSCALLTYPE afs_ni_syscall32 = 0;

extern int afs32_xsetgroups();
asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
				    __kernel_gid_t32 * grouplist);
#ifdef AFS_LINUX24_ENV
/* This number is not exported for some bizarre reason. */
#define __NR_setgroups32      82
extern int afs32_xsetgroups32();
asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
				      __kernel_gid_t32 * grouplist);
#endif

asmlinkage int
afs_syscall32(long syscall, long parm1, long parm2, long parm3, long parm4,
	      long parm5)
{
    __asm__ __volatile__("srl %o4, 0, %o4\n\t"
                         "mov %o7, %i7\n\t"
			 "call afs_syscall\n\t"
                         "srl %o5, 0, %o5\n\t"
			 "ret\n\t"
                         "nop");
}
#endif /* AFS_SPARC64_LINUX20_ENV */


/***** IA64 *****/
#ifdef AFS_IA64_LINUX20_ENV

asmlinkage long
afs_syscall_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
    __asm__ __volatile__("alloc r42 = ar.pfs, 8, 3, 6, 0\n\t"
                         "mov r41 = b0\n\t"	/* save rp */
                         "mov out0 = in0\n\t"
                         "mov out1 = in1\n\t"
                         "mov out2 = in2\n\t"
                         "mov out3 = in3\n\t"
                         "mov out4 = in4\n\t"
                         "mov out5 = gp\n\t"	/* save gp */
                         ";;\n"
                         ".L1:\n\t"
                         "mov r3 = ip\n\t"
                         ";;\n\t"
                         "addl r15=.fptr_afs_syscall-.L1,r3\n\t"
                         ";;\n\t"
                         "ld8 r15=[r15]\n\t"
                         ";;\n\t"
                         "ld8 r16=[r15],8\n\t"
                         ";;\n\t"
                         "ld8 gp=[r15]\n\t"
                         "mov b6=r16\n\t"
                         "br.call.sptk.many b0 = b6\n\t"
                         ";;\n\t"
                         "mov ar.pfs = r42\n\t"
                         "mov b0 = r41\n\t"
                         "mov gp = r48\n\t"	/* restore gp */
                         "br.ret.sptk.many b0\n"
                         ".fptr_afs_syscall:\n\t"
                         "data8 @fptr(afs_syscall)\n\t"
                         ".skip 8");
}

asmlinkage long
afs_xsetgroups_stub(int r0, int r1, long r2, long r3, long r4, long gp)
{
    __asm__ __volatile__("alloc r42 = ar.pfs, 8, 3, 6, 0\n\t"
                         "mov r41 = b0\n\t"	/* save rp */
			 "mov out0 = in0\n\t"
                         "mov out1 = in1\n\t"
                         "mov out2 = in2\n\t"
                         "mov out3 = in3\n\t"
                         "mov out4 = in4\n\t"
                         "mov out5 = gp\n\t"	/* save gp */
			 ";;\n"
                         ".L2:\n\t"
                         "mov r3 = ip\n\t"
                         ";;\n\t"
                         "addl r15=.fptr_afs_xsetgroups - .L2,r3\n\t"
                         ";;\n\t"
                         "ld8 r15=[r15]\n\t"
                         ";;\n\t"
                         "ld8 r16=[r15],8\n\t"
                         ";;\n\t"
                         "ld8 gp=[r15]\n\t"
                         "mov b6=r16\n\t"
                         "br.call.sptk.many b0 = b6\n\t"
                         ";;\n\t"
                         "mov ar.pfs = r42\n\t"
                         "mov b0 = r41\n\t"
                         "mov gp = r48\n\t"	/* restore gp */
			 "br.ret.sptk.many b0\n"
                         ".fptr_afs_xsetgroups:\n\t"
			 "data8 @fptr(afs_xsetgroups)\n\t"
                         ".skip 8");
}

struct fptr {
    void *ip;
    unsigned long gp;
};

#endif /* AFS_IA64_LINUX20_ENV */



/**********************************************************************/
/********************* System Call Initialization *********************/
/**********************************************************************/

int osi_syscall_init(void)
{
/***** IA64 *****/
#ifdef AFS_IA64_LINUX20_ENV
    /* This needs to be first because we are declaring variables, and
     * also because the handling of syscall pointers is bizarre enough
     * that we want to special-case even the "common" part.
     */
    unsigned long kernel_gp = 0;
    static struct fptr sys_setgroups;

    afs_sys_call_table = osi_find_syscall_table(0);
    if (afs_sys_call_table) {

#if !defined(AFS_LINUX24_ENV)
	/* XXX no sys_settimeofday on IA64? */
#endif

	/* check we aren't already loaded */
	/* XXX this can't be right */
	if (SYSCALL2POINTER afs_sys_call_table[_S(__NR_afs_syscall)]
	    == afs_syscall) {
	    printf("AFS syscall entry point already in use!\n");
	    return -EBUSY;
	}

	/* setup AFS entry point */
	afs_ni_syscall = afs_sys_call_table[_S(__NR_afs_syscall)];
	afs_sys_call_table[_S(__NR_afs_syscall)] =
		POINTER2SYSCALL((struct fptr *)afs_syscall_stub)->ip;

	/* setup setgroups */
	sys_setgroupsp = (void *)&sys_setgroups;

	((struct fptr *)sys_setgroupsp)->ip =
	    SYSCALL2POINTER afs_sys_call_table[_S(__NR_setgroups)];
	((struct fptr *)sys_setgroupsp)->gp = kernel_gp;

	afs_sys_call_table[_S(_NR_setgroups)] =
	    POINTER2SYSCALL((struct fptr *)afs_xsetgroups_stub)->ip;
    }

    /* XXX no 32-bit syscalls on IA64? */


/***** COMMON (except IA64) *****/
#else /* !AFS_IA64_LINUX20_ENV */

    afs_sys_call_table = osi_find_syscall_table(0);
    if (afs_sys_call_table) {
#if !defined(AFS_LINUX24_ENV)
	sys_settimeofdayp =
	    SYSCALL2POINTER afs_sys_call_table[_S(__NR_settimeofday)];
#endif /* AFS_LINUX24_ENV */

	/* check we aren't already loaded */
	if (SYSCALL2POINTER afs_sys_call_table[_S(__NR_afs_syscall)]
	    == afs_syscall) {
	    printf("AFS syscall entry point already in use!\n");
	    return -EBUSY;
	}

	/* setup AFS entry point */
	afs_ni_syscall = afs_sys_call_table[_S(__NR_afs_syscall)];
	afs_sys_call_table[_S(__NR_afs_syscall)] = POINTER2SYSCALL afs_syscall;

	/* setup setgroups */
	sys_setgroupsp = SYSCALL2POINTER afs_sys_call_table[_S(__NR_setgroups)];
	afs_sys_call_table[_S(__NR_setgroups)] = POINTER2SYSCALL afs_xsetgroups;

#if defined(__NR_setgroups32)
	/* setup setgroups32 */
	sys_setgroups32p = SYSCALL2POINTER afs_sys_call_table[__NR_setgroups32];
	afs_sys_call_table[__NR_setgroups32] = POINTER2SYSCALL afs_xsetgroups32;
#endif
    }
#endif /* !AFS_IA64_LINUX20_ENV */


/***** AMD64 *****/
#ifdef AFS_AMD64_LINUX20_ENV
    afs_ia32_sys_call_table = osi_find_syscall_table(1);
    if (afs_ia32_sys_call_table) {
	/* setup AFS entry point for IA32 */
	ia32_ni_syscall = afs_ia32_sys_call_table[__NR_ia32_afs_syscall];
	afs_ia32_sys_call_table[__NR_ia32_afs_syscall] =
	    POINTER2SYSCALL afs_syscall;

	/* setup setgroups for IA32 */
	sys32_setgroupsp =
	    SYSCALL2POINTER afs_ia32_sys_call_table[__NR_ia32_setgroups];
	afs_ia32_sys_call_table[__NR_ia32_setgroups] =
	    POINTER2SYSCALL afs32_xsetgroups;

#if AFS_LINUX24_ENV
	/* setup setgroups32 for IA32 */
	sys32_setgroups32p =
	    SYSCALL2POINTER afs_ia32_sys_call_table[__NR_ia32_setgroups32];
	afs_ia32_sys_call_table[__NR_ia32_setgroups32] =
	    POINTER2SYSCALL afs32_xsetgroups32;
#endif /* __NR_ia32_setgroups32 */
    }
#endif /* AFS_AMD64_LINUX20_ENV */


/***** PPC64 *****/
#ifdef AFS_PPC64_LINUX20_ENV
    /* XXX no 32-bit syscalls on PPC64? */
#endif


/***** SPARC64 *****/
#ifdef AFS_SPARC64_LINUX20_ENV
    afs_sys_call_table32 = osi_find_syscall_table(1);
    if (afs_sys_call_table32) {
	/* setup AFS entry point for 32-bit SPARC */
	afs_ni_syscall32 = afs_sys_call_table32[__NR_afs_syscall];
	afs_sys_call_table32[__NR_afs_syscall] = POINTER2SYSCALL afs_syscall32;

	/* setup setgroups for 32-bit SPARC */
	sys32_setgroupsp = SYSCALL2POINTER afs_sys_call_table32[__NR_setgroups];
	afs_sys_call_table32[__NR_setgroups] = POINTER2SYSCALL afs32_xsetgroups;

#ifdef AFS_LINUX24_ENV
	/* setup setgroups32 for 32-bit SPARC */
	sys32_setgroups32p =
	    SYSCALL2POINTER afs_sys_call_table32[__NR_setgroups32];
	afs_sys_call_table32[__NR_setgroups32] =
	    POINTER2SYSCALL afs32_xsetgroups32;
#endif
    }
#endif /* AFS_SPARC64_LINUX20_ENV */
    return 0;
}



/**********************************************************************/
/************************ System Call Cleanup *************************/
/**********************************************************************/

void osi_syscall_clean(void)
{
/***** COMMON *****/
    if (afs_sys_call_table) {
	/* put back the AFS entry point */
	afs_sys_call_table[_S(__NR_afs_syscall)] = afs_ni_syscall;

	/* put back setgroups */
#if defined(AFS_IA64_LINUX20_ENV)
	afs_sys_call_table[_S(__NR_setgroups)] =
	    POINTER2SYSCALL((struct fptr *)sys_setgroupsp)->ip;
#else /* AFS_IA64_LINUX20_ENV */
	afs_sys_call_table[_S(__NR_setgroups)] =
	    POINTER2SYSCALL sys_setgroupsp;
#endif

#if defined(__NR_setgroups32) && !defined(AFS_IA64_LINUX20_ENV)
	/* put back setgroups32 */
	afs_sys_call_table[__NR_setgroups32] = POINTER2SYSCALL sys_setgroups32p;
#endif
    }


/***** IA64 *****/
#ifdef AFS_IA64_LINUX20_ENV
    /* XXX no 32-bit syscalls on IA64? */
#endif


/***** AMD64 *****/
#ifdef AFS_AMD64_LINUX20_ENV
    if (afs_ia32_sys_call_table) {
	/* put back AFS entry point for IA32 */
	afs_ia32_sys_call_table[__NR_ia32_afs_syscall] =
	    POINTER2SYSCALL ia32_ni_syscall;

	/* put back setgroups for IA32 */
	afs_ia32_sys_call_table[__NR_ia32_setgroups] =
	    POINTER2SYSCALL sys32_setgroupsp;

#ifdef AFS_LINUX24_ENV
	/* put back setgroups32 for IA32 */
	afs_ia32_sys_call_table[__NR_ia32_setgroups32] =
	    POINTER2SYSCALL sys32_setgroups32p;
#endif
    }
#endif


/***** PPC64 *****/
#ifdef AFS_PPC64_LINUX20_ENV
    /* XXX no 32-bit syscalls on PPC64? */
#endif


/***** SPARC64 *****/
#ifdef AFS_SPARC64_LINUX20_ENV
    if (afs_sys_call_table32) {
	/* put back AFS entry point for 32-bit SPARC */
	afs_sys_call_table32[__NR_afs_syscall] = afs_ni_syscall32;

	/* put back setgroups for IA32 */
	afs_sys_call_table32[__NR_setgroups] =
	    POINTER2SYSCALL sys32_setgroupsp;

#ifdef AFS_LINUX24_ENV
	/* put back setgroups32 for IA32 */
	afs_sys_call_table32[__NR_setgroups32] =
	    POINTER2SYSCALL sys32_setgroups32p;
#endif
    }
#endif
}
