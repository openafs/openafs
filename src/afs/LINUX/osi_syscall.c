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
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_syscall.c,v 1.1.2.9 2007/03/27 03:22:25 shadow Exp $");

#ifdef AFS_LINUX24_ENV
#include <linux/module.h> /* early to avoid printf->printk mapping */
#endif
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "h/unistd.h"		/* For syscall numbers. */
#include "h/mm.h"

#ifdef AFS_AMD64_LINUX20_ENV
#include <asm/ia32_unistd.h>
#endif

#include <linux/proc_fs.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <linux/init.h>
#include <linux/sched.h>
#endif

#ifndef NR_syscalls
#define NR_syscalls 222
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

#if defined(AFS_S390X_LINUX24_ENV) 
#define INSERT_SYSCALL(SLOT, TMPPAGE, FUNC) \
	if (SYSCALL2POINTER FUNC > 0x7fffffff) { \
	    TMPPAGE = kmalloc ( PAGE_SIZE, GFP_DMA|GFP_KERNEL );	\
	    if (SYSCALL2POINTER TMPPAGE > 0x7fffffff) { \
		printf("Cannot allocate page for FUNC syscall jump vector\n"); \
		return EINVAL; \
	    } \
	    memcpy(TMPPAGE, syscall_jump_code, sizeof(syscall_jump_code)); \
	    *(void **)(TMPPAGE + 0x0c) = &FUNC; \
	    afs_sys_call_table[_S(SLOT)] = POINTER2SYSCALL TMPPAGE; \
	} else \
	    afs_sys_call_table[_S(SLOT)] = POINTER2SYSCALL FUNC;
#else
#define INSERT_SYSCALL(SLOT, TMPPAGE, FUNC) \
    afs_sys_call_table[_S(SLOT)] = POINTER2SYSCALL FUNC;
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

#ifdef AFS_S390X_LINUX24_ENV
static void *afs_sys_setgroups_page = 0;
static void *afs_sys_setgroups32_page = 0;
static void *afs_syscall_page = 0;

/* Because of how the syscall table is handled, we need to ensure our 
   syscalls are within the first 2gb of address space. This means we need
   self-modifying code we can inject to call our handlers if the module 
   is loaded high. If keyrings had advanced as fast as false protection
   this would be unnecessary. */

uint32_t syscall_jump_code[] = {
  0xe3d0f030, 0x00240dd0, 0xa7f40006, 0xffffffff, 0xffffffff, 0xe310d004, 
  0x0004e3d0, 0xf0300004, 0x07f10000, 
};
#endif

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
#ifdef AFS_PPC64_LINUX26_ENV
static SYSCALLTYPE *afs_sys_call_table32;
static SYSCALLTYPE afs_ni_syscall32 = 0;
static SYSCALLTYPE old_sys_setgroupsp = 0;
static SYSCALLTYPE old_sys32_setgroupsp = 0;

extern int afs32_xsetgroups();
asmlinkage long (*sys32_setgroupsp)(int gidsetsize, gid_t *grouplist);

asmlinkage long sys_close(unsigned int fd);
static void sys_setgroups_stub() 
	__attribute__ ((pure,const,no_instrument_function));
static void sys_setgroups_stub() 
{ 
	printf("*** error! sys_setgroups_stub called\n");
}

static void sys32_setgroups_stub() 
	__attribute__ ((pure,const,no_instrument_function));
static void sys32_setgroups_stub() 
{ 
	printf("*** error! sys32_setgroups_stub called\n");
}

#endif /* AFS_AMD64_LINUX20_ENV */


/***** SPARC64 *****/
#ifdef AFS_SPARC64_LINUX20_ENV
#ifdef AFS_SPARC64_LINUX26_ENV
static SYSCALLTYPE *afs_sys_call_table32;
#else
extern SYSCALLTYPE *afs_sys_call_table32;
#endif
static SYSCALLTYPE afs_ni_syscall32 = 0;

extern int afs32_xsetgroups();
#ifdef AFS_SPARC64_LINUX26_ENV
asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
				    __kernel_gid32_t * grouplist);
#else
asmlinkage int (*sys32_setgroupsp) (int gidsetsize,
				    __kernel_gid_t32 * grouplist);
#endif
#ifdef AFS_LINUX24_ENV
/* This number is not exported for some bizarre reason. */
#define __NR_setgroups32      82
extern int afs32_xsetgroups32();
#ifdef AFS_SPARC64_LINUX26_ENV
asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
				      __kernel_gid32_t * grouplist);
#else
asmlinkage int (*sys32_setgroups32p) (int gidsetsize,
				      __kernel_gid_t32 * grouplist);
#endif
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

/***** PPC64 ***** 
 * Spring 2005
 * sys_call_table hook for PPC64 
 * by Soewono Effendi <Soewono.Effendi@sysgo.de>
 * for IBM Deutschland
 * Thanks go to SYSGO's team for their support:
 * Horst Birthelmer <Horst.Birthelmer@sysgo.de>
 * Marius Groeger <Marius.Groeger@sysgo.de>
 */
#if defined(AFS_PPC64_LINUX26_ENV)
extern void flush_cache(void *, unsigned long);
#define PPC_LO(v) ((v) & 0xffff)
#define PPC_HI(v) (((v) >> 16) & 0xffff)
#define PPC_HA(v) PPC_HI ((v) + 0x8000)
#define PPC_HLO(v) ((short)(((v) >> 32) & 0xffff))
#define PPC_HHI(v) ((short)(((v) >> 48) & 0xffff))

struct ppc64_opd
{
	unsigned long funcaddr;
	unsigned long r2;
};

struct ppc64_stub
{
	unsigned char jump[136];
	unsigned long r2;
	unsigned long lr;
	struct ppc64_opd opd;
} __attribute__ ((packed));

/* a stub to fix up r2 (TOC ptr) and to jump to our sys_call hook
   function.  We patch the new r2 value and function pointer into 
   the stub. */
#define PPC64_STUB(stub) \
static struct ppc64_stub stub = \
{ .jump = { \
        0xf8, 0x41, 0x00, 0x28, /*     std     r2,40(r1) */ \
        0xfb, 0xc1, 0xff, 0xf0, /*     std     r30,-16(r1) */ \
        0xfb, 0xa1, 0xff, 0xe8, /*     std     r29,-24(r1) */ \
        0x7c, 0x5d, 0x13, 0x78, /*     mr      r29,r2 */ \
        0x3c, 0x40, 0x12, 0x34, /*16:  lis     r2,4660 */ \
        0x60, 0x42, 0x56, 0x78, /*20:  ori     r2,r2,22136 */ \
        0x78, 0x42, 0x07, 0xc6, /*     rldicr  r2,r2,32,31 */ \
        0x64, 0x42, 0x90, 0xab, /*28:  oris    r2,r2,37035 */ \
        0x60, 0x42, 0xcd, 0xef, /*32:  ori     r2,r2,52719 */ \
        0x3f, 0xc2, 0x00, 0x00, /*36:  addis   r30,r2,0 */ \
        0x3b, 0xde, 0x00, 0x00, /*40:  addi    r30,r30,0 */ \
        0xfb, 0xbe, 0x00, 0x88, /*     std     r29,136(r30) */ \
        0x7f, 0xa8, 0x02, 0xa6, /*     mflr    r29 */ \
        0xfb, 0xbe, 0x00, 0x90, /*     std     r29,144(r30) */ \
        0xeb, 0xde, 0x00, 0x98, /*     ld      r30,152(r30) */ \
        0x7f, 0xc8, 0x03, 0xa6, /*     mtlr    r30 */ \
        0xeb, 0xa1, 0xff, 0xe8, /*     ld      r29,-24(r1) */ \
        0xeb, 0xc1, 0xff, 0xf0, /*     ld      r30,-16(r1) */ \
        0x4e, 0x80, 0x00, 0x21, /*     blrl */ \
        0x3c, 0x40, 0x12, 0x34, /*76:  lis     r2,4660 */ \
        0x60, 0x42, 0x56, 0x78, /*80:  ori     r2,r2,22136 */ \
        0x78, 0x42, 0x07, 0xc6, /*     rldicr  r2,r2,32,31 */ \
        0x64, 0x42, 0x90, 0xab, /*88:  oris    r2,r2,37035 */ \
        0x60, 0x42, 0xcd, 0xef, /*92:  ori     r2,r2,52719 */ \
        0xfb, 0xc1, 0xff, 0xf0, /*     std     r30,-16(r1) */ \
        0xfb, 0xa1, 0xff, 0xe8, /*     std     r29,-24(r1) */ \
        0x3f, 0xc2, 0xab, 0xcd, /*104: addis   r30,r2,-21555 */ \
        0x3b, 0xde, 0x78, 0x90, /*108: addi    r30,r30,30864 */ \
        0xeb, 0xbe, 0x00, 0x90, /*     ld      r29,144(r30) */ \
        0x7f, 0xa8, 0x03, 0xa6, /*     mtlr    r29 */ \
        0xe8, 0x5e, 0x00, 0x88, /*     ld      r2,136(r30) */ \
        0xeb, 0xa1, 0xff, 0xe8, /*     ld      r29,-24(r1) */ \
        0xeb, 0xc1, 0xff, 0xf0, /*     ld      r30,-16(r1) */ \
        0x4e, 0x80, 0x00, 0x20  /*     blr */ \
}} 

static void * create_stub(struct ppc64_stub *stub,
                          struct ppc64_opd *opd)
{
	unsigned short *p1, *p2, *p3, *p4;
	unsigned long addr;

	stub->opd.funcaddr = opd->funcaddr;
	stub->opd.r2 = opd->r2;
	addr = (unsigned long) opd->r2;
	p1 = (unsigned short*) &stub->jump[18];
	p2 = (unsigned short*) &stub->jump[22];
	p3 = (unsigned short*) &stub->jump[30];
	p4 = (unsigned short*) &stub->jump[34];

	*p1 = PPC_HHI(addr);
	*p2 = PPC_HLO(addr);
	*p3 = PPC_HI(addr);
	*p4 = PPC_LO(addr);

	addr = (unsigned long) stub - opd->r2;
	p1 = (unsigned short*) &stub->jump[38];
	p2 = (unsigned short*) &stub->jump[42];
	*p1 = PPC_HA(addr);
	*p2 = PPC_LO(addr);
	p1 = (unsigned short*) &stub->jump[106];
	p2 = (unsigned short*) &stub->jump[110];
	*p1 = PPC_HA(addr);
	*p2 = PPC_LO(addr);

	addr = (unsigned long) opd->r2;
	p1 = (unsigned short*) &stub->jump[78];
	p2 = (unsigned short*) &stub->jump[82];
	p3 = (unsigned short*) &stub->jump[90];
	p4 = (unsigned short*) &stub->jump[94];

	*p1 = PPC_HHI(addr);
	*p2 = PPC_HLO(addr);
	*p3 = PPC_HI(addr);
	*p4 = PPC_LO(addr);

        flush_cache((void *)stub, sizeof(*stub));
        return ((void*)(stub));
}

PPC64_STUB(afs_sys_call_stub);
PPC64_STUB(afs_xsetgroups_stub);
PPC64_STUB(afs_xsetgroups32_stub);
#endif /* AFS_PPC64_LINUX26_ENV */


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

	afs_sys_call_table[_S(__NR_setgroups)] =
	    POINTER2SYSCALL((struct fptr *)afs_xsetgroups_stub)->ip;
    }

    /* XXX no 32-bit syscalls on IA64? */


#elif defined(AFS_PPC64_LINUX26_ENV)

    afs_sys_call_table = osi_find_syscall_table(0);
    if (afs_sys_call_table) {
    	SYSCALLTYPE p;
    	struct ppc64_opd* opd = (struct ppc64_opd*) sys_close;
	unsigned long r2 = opd->r2;
    	opd = (struct ppc64_opd*) afs_syscall;
    	afs_sys_call_table32 = (unsigned long)afs_sys_call_table - 
		NR_syscalls * sizeof(SYSCALLTYPE);
	/* check we aren't already loaded */
	p = SYSCALL2POINTER afs_sys_call_table[_S(__NR_afs_syscall)];
	if ((unsigned long)p == opd->funcaddr) {
	    printf("AFS syscall entry point already in use!\n");
	    return -EBUSY;
	}
	/* setup AFS entry point */
	p = create_stub(&afs_sys_call_stub, opd);
	afs_ni_syscall = afs_sys_call_table[_S(__NR_afs_syscall)];
	afs_sys_call_table[_S(__NR_afs_syscall)] = POINTER2SYSCALL p;

	/* setup setgroups */
    	opd = (struct ppc64_opd*) afs_xsetgroups;
	p = create_stub(&afs_xsetgroups_stub, opd);
	old_sys_setgroupsp = SYSCALL2POINTER afs_sys_call_table[_S(__NR_setgroups)];
	afs_sys_call_table[_S(__NR_setgroups)] = POINTER2SYSCALL p;
    	opd = (struct ppc64_opd*) sys_setgroups_stub;
	opd->funcaddr = old_sys_setgroupsp;
	opd->r2 = r2;

	/* setup setgroups32 */
    	opd = (struct ppc64_opd*) afs32_xsetgroups;
	p = create_stub(&afs_xsetgroups32_stub, opd);
	old_sys32_setgroupsp = SYSCALL2POINTER afs_sys_call_table32[_S(__NR_setgroups)];
	afs_sys_call_table32[_S(__NR_setgroups)] = POINTER2SYSCALL p;
    	opd = (struct ppc64_opd*) sys32_setgroups_stub;
	opd->funcaddr = old_sys32_setgroupsp;
	opd->r2 = r2;

        flush_cache((void *)afs_sys_call_table, 2*NR_syscalls*sizeof(void*));

	sys_setgroupsp = sys_setgroups_stub;
	sys32_setgroupsp = sys32_setgroups_stub;
    }
/***** COMMON (except IA64 or PPC64) *****/
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

	INSERT_SYSCALL(__NR_afs_syscall, afs_syscall_page, afs_syscall)

	/* setup setgroups */
	sys_setgroupsp = SYSCALL2POINTER afs_sys_call_table[_S(__NR_setgroups)];
	INSERT_SYSCALL(__NR_setgroups, afs_sys_setgroups_page, afs_xsetgroups)

#if defined(__NR_setgroups32)
	/* setup setgroups32 */
	sys_setgroups32p = SYSCALL2POINTER afs_sys_call_table[__NR_setgroups32];
	INSERT_SYSCALL(__NR_setgroups32, afs_sys_setgroups32_page, afs_xsetgroups32)
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
#elif defined(AFS_PPC64_LINUX26_ENV)
	afs_sys_call_table[_S(__NR_setgroups)] =
	    POINTER2SYSCALL old_sys_setgroupsp;
	/* put back setgroups32 for PPC64 */
	afs_sys_call_table32[__NR_setgroups] =
	    POINTER2SYSCALL old_sys32_setgroupsp;
#else /* AFS_IA64_LINUX20_ENV */
	afs_sys_call_table[_S(__NR_setgroups)] =
	    POINTER2SYSCALL sys_setgroupsp;
#endif

#if defined(__NR_setgroups32) && !defined(AFS_IA64_LINUX20_ENV)
	/* put back setgroups32 */
	afs_sys_call_table[__NR_setgroups32] = POINTER2SYSCALL sys_setgroups32p;
#endif
#if defined(AFS_S390X_LINUX24_ENV)
#if defined(__NR_setgroups32) && !defined(AFS_IA64_LINUX20_ENV)
	if (afs_sys_setgroups32_page)
	    kfree(afs_sys_setgroups32_page);
#endif
	if (afs_sys_setgroups_page)
	    kfree(afs_sys_setgroups_page);
	if (afs_syscall_page)
	    kfree(afs_syscall_page);
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
