/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006-2007 Sine Nomine Associates
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "rx/rx_globals.h"
#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "net/if.h"
#ifdef AFS_SGI62_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_DARWIN60_ENV)
#include "netinet/in_var.h"
#endif
#endif /* !defined(UKERNEL) */
#ifdef AFS_LINUX22_ENV
#include "h/smp_lock.h"
#endif

#if (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)) || defined(AFS_HPUX_64BIT_ENV) || defined(AFS_SUN57_64BIT_ENV) || (defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64)) || defined(NEED_IOCTL32)
static void
afs_ioctl32_to_afs_ioctl(const struct afs_ioctl32 *src, struct afs_ioctl *dst)
{
    dst->in = (char *)(unsigned long)src->in;
    dst->out = (char *)(unsigned long)src->out;
    dst->in_size = src->in_size;
    dst->out_size = src->out_size;
}
#endif

/*
 * If you need to change copyin_afs_ioctl(), you may also need to change
 * copyin_iparam().
 */

int
copyin_afs_ioctl(caddr_t cmarg, struct afs_ioctl *dst)
{
    int code;
#if defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)
    struct afs_ioctl32 dst32;

    if (!(IS64U)) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL) */


#if defined(AFS_HPUX_64BIT_ENV)
    struct afs_ioctl32 dst32;

    if (is_32bit(u.u_procp)) {	/* is_32bit() in proc_iface.h */
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_HPUX_64BIT_ENV) */

#if defined(AFS_SUN57_64BIT_ENV)
    struct afs_ioctl32 dst32;

    if (get_udatamodel() == DATAMODEL_ILP32) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_SUN57_64BIT_ENV) */

#if defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64)
    struct afs_ioctl32 dst32;

    if (!ABI_IS_64BIT(get_current_abi())) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64) */

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV)
    struct afs_ioctl32 dst32;

#ifdef AFS_SPARC64_LINUX26_ENV
    if (test_thread_flag(TIF_32BIT))
#elif defined(AFS_SPARC64_LINUX24_ENV)
    if (current->thread.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_SPARC64_LINUX20_ENV)
    if (current->tss.flags & SPARC_FLAG_32BIT)

#elif defined(AFS_AMD64_LINUX26_ENV)
    if (test_thread_flag(TIF_IA32))
#elif defined(AFS_AMD64_LINUX20_ENV)
    if (current->thread.flags & THREAD_IA32)

#elif defined(AFS_PPC64_LINUX26_ENV)
    if (current->thread_info->flags & _TIF_32BIT)
#elif defined(AFS_PPC64_LINUX20_ENV)
    if (current->thread.flags & PPC_FLAG_32BIT)

#elif defined(AFS_S390X_LINUX26_ENV)
    if (test_thread_flag(TIF_31BIT))
#elif defined(AFS_S390X_LINUX20_ENV)
    if (current->thread.flags & S390_FLAG_31BIT)

#else
#error pioctl32 not done for this linux
#endif
    {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_LINUX_64BIT_KERNEL) */

    AFS_COPYIN(cmarg, (caddr_t) dst, sizeof *dst, code);
    return code;
}


#ifdef AFS_AIX32_ENV

#include "sys/lockl.h"

/*
 * syscall -	this is the VRMIX system call entry point.
 *
 * NOTE:
 *	THIS SHOULD BE CHANGED TO afs_syscall(), but requires
 *	all the user-level calls to `syscall' to change.
 */
syscall(syscall, p1, p2, p3, p4, p5, p6)
{
    register rval1 = 0, code;
    int retval = 0;
#ifndef AFS_AIX41_ENV
    register int monster;
    extern lock_t kernel_lock;
    monster = lockl(&kernel_lock, LOCK_SHORT);
#endif /* !AFS_AIX41_ENV */

    AFS_STATCNT(syscall);
    setuerror(0);
    switch (syscall) {
#if !defined(LIBKTRACE)
    case AFSCALL_CALL:
	rval1 = afs_syscall_call(p1, p2, p3, p4, p5, p6);
	break;

    case AFSCALL_SETPAG:
	AFS_GLOCK();
	rval1 = afs_setpag();
	AFS_GUNLOCK();
	break;

    case AFSCALL_PIOCTL:
	AFS_GLOCK();
	rval1 = afs_syscall_pioctl(p1, p2, p3, p4);
	AFS_GUNLOCK();
	break;

    case AFSCALL_ICREATE:
	rval1 = afs_syscall_icreate(p1, p2, p3, p4, p5, p6);
	break;

    case AFSCALL_IOPEN:
	rval1 = afs_syscall_iopen(p1, p2, p3);
	break;

    case AFSCALL_IDEC:
	rval1 = afs_syscall_iincdec(p1, p2, p3, -1);
	break;

    case AFSCALL_IINC:
	rval1 = afs_syscall_iincdec(p1, p2, p3, 1);
	break;
#endif /* !LIBKTRACE */

    case AFSCALL_OSI_TRACE:
	code = osi_Trace_syscall_handler(p1, p2, p3, p4, p5, &retval);
	if (!code)
	    rval1 = retval;
	if (!rval1)
	    rval1 = code;
	break;

    default:
	rval1 = EINVAL;
	setuerror(EINVAL);
	break;
    }

  out:
#ifndef AFS_AIX41_ENV
    if (monster != LOCK_NEST)
	unlockl(&kernel_lock);
#endif /* !AFS_AIX41_ENV */
    return getuerror()? -1 : rval1;
}

/*
 * lsetpag -	interface to afs_setpag().
 */
lsetpag()
{

    AFS_STATCNT(lsetpag);
    return syscall(AFSCALL_SETPAG, 0, 0, 0, 0, 0);
}

/*
 * lpioctl -	interface to pioctl()
 */
lpioctl(path, cmd, cmarg, follow)
     char *path, *cmarg;
{

    AFS_STATCNT(lpioctl);
    return syscall(AFSCALL_PIOCTL, path, cmd, cmarg, follow);
}

#else /* !AFS_AIX32_ENV       */

#if defined(AFS_SGI_ENV)
struct afsargs {
    sysarg_t syscall;
    sysarg_t parm1;
    sysarg_t parm2;
    sysarg_t parm3;
    sysarg_t parm4;
    sysarg_t parm5;
};


int
Afs_syscall(struct afsargs *uap, rval_t * rvp)
{
    int error;
    long retval;

    AFS_STATCNT(afs_syscall);
    switch (uap->syscall) {
    case AFSCALL_OSI_TRACE:
	retval = 0;
	error =
	    osi_Trace_syscall_handler(uap->parm1, uap->parm2, uap->parm3, 
				      uap->parm4, uap->parm5, &retval);
	rvp->r_val1 = retval;
	break;

#if !defined(LIBKTRACE)
#ifdef AFS_SGI_XFS_IOPS_ENV
    case AFSCALL_IDEC64:
	error =
	    afs_syscall_idec64(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			       uap->parm5);
	break;
    case AFSCALL_IINC64:
	error =
	    afs_syscall_iinc64(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			       uap->parm5);
	break;
    case AFSCALL_ILISTINODE64:
	error =
	    afs_syscall_ilistinode64(uap->parm1, uap->parm2, uap->parm3,
				     uap->parm4, uap->parm5);
	break;
    case AFSCALL_ICREATENAME64:
	error =
	    afs_syscall_icreatename64(uap->parm1, uap->parm2, uap->parm3,
				      uap->parm4, uap->parm5);
	break;
#endif /* AFS_SGI_XFS_IOPS_ENV */
#ifdef AFS_SGI_VNODE_GLUE
    case AFSCALL_INIT_KERNEL_CONFIG:
	error = afs_init_kernel_config(uap->parm1);
	break;
#endif
    default:
	error =
	    afs_syscall_call(uap->syscall, uap->parm1, uap->parm2, uap->parm3,
			     uap->parm4, uap->parm5);
#else /* LIBKTRACE */
    default:
	error = EINVAL;
#endif /* LIBKTRACE */
    }
    return error;
}

#else /* !AFS_SGI_ENV */

struct iparam {
    long param1;
    long param2;
    long param3;
    long param4;
};

struct iparam32 {
    int param1;
    int param2;
    int param3;
    int param4;
};


#if defined(AFS_HPUX_64BIT_ENV) || defined(AFS_SUN57_64BIT_ENV) || (defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV))
static void
iparam32_to_iparam(const struct iparam32 *src, struct iparam *dst)
{
    dst->param1 = src->param1;
    dst->param2 = src->param2;
    dst->param3 = src->param3;
    dst->param4 = src->param4;
}
#endif

/*
 * If you need to change copyin_iparam(), you may also need to change
 * copyin_afs_ioctl().
 */

static int
copyin_iparam(caddr_t cmarg, struct iparam *dst)
{
    int code;

#if defined(AFS_HPUX_64BIT_ENV)
    struct iparam32 dst32;

    if (is_32bit(u.u_procp)) {	/* is_32bit() in proc_iface.h */
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    iparam32_to_iparam(&dst32, dst);
	return code;
    }
#endif /* AFS_HPUX_64BIT_ENV */

#if defined(AFS_SUN57_64BIT_ENV)
    struct iparam32 dst32;

    if (get_udatamodel() == DATAMODEL_ILP32) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    iparam32_to_iparam(&dst32, dst);
	return code;
    }
#endif /* AFS_SUN57_64BIT_ENV */

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX20_ENV) && !defined(AFS_IA64_LINUX20_ENV)
    struct iparam32 dst32;

#ifdef AFS_SPARC64_LINUX26_ENV
    if (test_thread_flag(TIF_32BIT))
#elif defined(AFS_SPARC64_LINUX24_ENV)
    if (current->thread.flags & SPARC_FLAG_32BIT)
#elif defined(AFS_SPARC64_LINUX20_ENV)
    if (current->tss.flags & SPARC_FLAG_32BIT)

#elif defined(AFS_AMD64_LINUX26_ENV)
    if (test_thread_flag(TIF_IA32))
#elif defined(AFS_AMD64_LINUX20_ENV)
    if (current->thread.flags & THREAD_IA32)

#elif defined(AFS_PPC64_LINUX26_ENV)
    if (current->thread_info->flags & _TIF_32BIT) 
#elif defined(AFS_PPC64_LINUX20_ENV)
    if (current->thread.flags & PPC_FLAG_32BIT) 

#elif defined(AFS_S390X_LINUX26_ENV)
    if (test_thread_flag(TIF_31BIT))
#elif defined(AFS_S390X_LINUX20_ENV)
    if (current->thread.flags & S390_FLAG_31BIT) 

#else
#error iparam32 not done for this linux platform
#endif
    {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    iparam32_to_iparam(&dst32, dst);
	return code;
    }
#endif /* AFS_LINUX_64BIT_KERNEL */

    AFS_COPYIN(cmarg, (caddr_t) dst, sizeof *dst, code);
    return code;
}

/* Main entry of all afs system calls */
#ifdef	AFS_SUN5_ENV
extern int afs_sinited;

/** The 32 bit OS expects the members of this structure to be 32 bit
 * quantities and the 64 bit OS expects them as 64 bit quanties. Hence
 * to accomodate both, *long* is used instead of afs_int32
 */

#ifdef AFS_SUN57_ENV
struct afssysa {
    long syscall;
    long parm1;
    long parm2;
    long parm3;
    long parm4;
    long parm5;
    long parm6;
};
#else /* !AFS_SUN57_ENV */
struct afssysa {
    afs_int32 syscall;
    afs_int32 parm1;
    afs_int32 parm2;
    afs_int32 parm3;
    afs_int32 parm4;
    afs_int32 parm5;
    afs_int32 parm6;
};
#endif /* !AFS_SUN57_ENV */

Afs_syscall(register struct afssysa *uap, rval_t * rvp)
{
    int *retval = &rvp->r_val1;
#else /* !AFS_SUN5_ENV */
#if	defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int
afs3_syscall(p, args, retval)
#ifdef AFS_FBSD50_ENV
     struct thread *p;
#else
     struct proc *p;
#endif
     void *args;
     long *retval;
{
    register struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)args;
#else /* !AFS_OSF_ENV && !AFS_DARWIN_ENV && !AFS_XBSD_ENV */
#ifdef AFS_LINUX20_ENV
struct afssysargs {
    long syscall;
    long parm1;
    long parm2;
    long parm3;
    long parm4;
    long parm5;
    long parm6;			/* not actually used - should be removed */
};
/* Linux system calls only set up for 5 arguments. */
asmlinkage long
afs_syscall(long syscall, long parm1, long parm2, long parm3, long parm4)
{
    struct afssysargs args, *uap = &args;
    long linux_ret = 0;
    long *retval = &linux_ret;
    long eparm[4];
#ifdef AFS_SPARC64_LINUX24_ENV
    afs_int32 eparm32[4];
#endif
    /* eparm is also used by AFSCALL_CALL in afsd.c */
#else /* !AFS_LINUX20_ENV */
#if defined(UKERNEL)
Afs_syscall()
{
    register struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)u.u_ap;
#else /* !UKERNEL */
int
Afs_syscall()
{
    register struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)u.u_ap;
#endif /* !UKERNEL */
#if defined(AFS_HPUX_ENV)
    long *retval = &u.u_rval1;
#else /* !AFS_HPUX_ENV */
    int *retval = &u.u_rval1;
#endif /* !AFS_HPUX_ENV */
#endif /* !AFS_LINUX20_ENV */
#endif /* !AFS_OSF_ENV && !AFS_DARWIN_ENV && !AFS_XBSD_ENV */
#endif /* !AFS_SUN5_ENV */
    register int code = 0;

    AFS_STATCNT(afs_syscall);

#ifdef AFS_SUN5_ENV
    rvp->r_vals = 0;
    if (!afs_sinited) {
	return (ENODEV);
    }
#endif /* AFS_SUN5_ENV */

#ifdef AFS_LINUX20_ENV
    if (syscall != AFSCALL_OSI_TRACE) {
	/* osi trace is mp-scaleable */
	lock_kernel();
    }
    /* setup uap for use below - pull out the magic decoder ring to know
     * which syscalls have folded argument lists.
     */
    uap->syscall = syscall;
    uap->parm1 = parm1;
    uap->parm2 = parm2;
    uap->parm3 = parm3;
    if (syscall == AFSCALL_CALL) {
#ifdef AFS_SPARC64_LINUX24_ENV
/* from arch/sparc64/kernel/sys_sparc32.c */
#define AA(__x)                                \
({     unsigned long __ret;            \
       __asm__ ("srl   %0, 0, %0"      \
                : "=r" (__ret)         \
                : "0" (__x));          \
       __ret;                          \
})


#ifdef AFS_SPARC64_LINUX26_ENV
	if (test_thread_flag(TIF_32BIT))
#else
	if (current->thread.flags & SPARC_FLAG_32BIT)
#endif
	{
	    AFS_COPYIN((char *)parm4, (char *)eparm32, sizeof(eparm32), code);
	    eparm[0] = AA(eparm32[0]);
	    eparm[1] = AA(eparm32[1]);
	    eparm[2] = AA(eparm32[2]);
#undef AA
	} else
#endif /* AFS_SPARC64_LINUX24_ENV */
	    AFS_COPYIN((char *)parm4, (char *)eparm, sizeof(eparm), code);
	uap->parm4 = eparm[0];
	uap->parm5 = eparm[1];
	uap->parm6 = eparm[2];
    } else {
	uap->parm4 = parm4;
	uap->parm5 = 0;
	uap->parm6 = 0;
    }
#endif /* AFS_LINUX20_ENV */

#if defined(AFS_DARWIN80_ENV)
    get_vfs_context();
    osi_Assert(*retval == 0);
#endif

#if defined(AFS_HPUX_ENV)
    /*
     * There used to be code here (duplicated from osi_Init()) for
     * initializing the semaphore used by AFS_GLOCK().  Was the
     * duplication to handle the case of a dynamically loaded kernel
     * module?
     */
    osi_InitGlock();
#endif /* AFS_HPUX_ENV */

    /* initialization complete ; now process syscall args */

#if !defined(LIBKTRACE)
    /* only AFSCALL_OSI_TRACE is implemented for libktrace */
    if (uap->syscall == AFSCALL_CALL) {
	code =
	    afs_syscall_call(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			     uap->parm5, uap->parm6);
    } else if (uap->syscall == AFSCALL_SETPAG) {
#ifdef AFS_SUN5_ENV
	register proc_t *procp;

	procp = ttoproc(curthread);
	AFS_GLOCK();
	code = afs_setpag(&procp->p_cred);
	AFS_GUNLOCK();
#else /* !AFS_SUN5_ENV */
	AFS_GLOCK();
#if	defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	code = afs_setpag(p, args, retval);
#else /* !AFS_OSF_ENV && !AFS_DARWIN_ENV && !AFS_XBSD_ENV */
	code = afs_setpag();
#endif /* !AFS_OSF_ENV && !AFS_DARWIN_ENV && !AFS_XBSD_ENV */
	AFS_GUNLOCK();
#endif /* !AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_PIOCTL) {
	AFS_GLOCK();
#if defined(AFS_SUN5_ENV)
	code =
	    afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			       rvp, CRED());
#elif defined(AFS_FBSD50_ENV)
	code =
	    afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			       p->td_ucred);
#elif defined(AFS_DARWIN80_ENV)
	code =
	    afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			       kauth_cred_get());
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	code =
	    afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			       p->p_cred->pc_ucred);
#else
	code =
	    afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3,
			       uap->parm4);
#endif
	AFS_GUNLOCK();
    } else if (uap->syscall == AFSCALL_ICREATE) {
	struct iparam iparams;

	code = copyin_iparam((char *)uap->parm3, &iparams);
	if (code) {
#if defined(KERNEL_HAVE_UERROR)
	    setuerror(code);
#endif
	} else
#ifdef AFS_SUN5_ENV
	    code =
		afs_syscall_icreate(uap->parm1, uap->parm2, iparams.param1,
				    iparams.param2, iparams.param3,
				    iparams.param4, rvp, CRED());
#else /* !AFS_SUN5_ENV */
	    code =
		afs_syscall_icreate(uap->parm1, uap->parm2, iparams.param1,
				    iparams.param2,
#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
				    iparams.param3, iparams.param4, retval);
#else
				    iparams.param3, iparams.param4);
#endif
#endif /* !AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_IOPEN) {
#ifdef AFS_SUN5_ENV
	code =
	    afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3, rvp,
			      CRED());
#else
#if defined(AFS_OSF_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	code = afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3, retval);
#else
	code = afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3);
#endif
#endif /* !AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_IDEC) {
#ifdef AFS_SUN5_ENV
	code =
	    afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, -1, rvp,
				CRED());
#else
	code = afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, -1);
#endif /* !AFS_SUN5_ENV */
    } else if (uap->syscall == AFSCALL_IINC) {
#ifdef AFS_SUN5_ENV
	code =
	    afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, 1, rvp,
				CRED());
#else
	code = afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, 1);
#endif /* !AFS_SUN5_ENV */
    } else
#endif /* !LIBKTRACE */
    if (uap->syscall == AFSCALL_OSI_TRACE) {
	code =
	    osi_Trace_syscall_handler(uap->parm1, uap->parm2, uap->parm3,
				      uap->parm4, uap->parm5, retval);
#ifdef AFS_LINUX20_ENV
	if (!code) {
	    /* osi_trace commands can return values. */
	    code = -linux_ret;	/* Gets negated again at exit below */
	}
#elif defined(KERNEL_HAVE_UERROR)
	if (code) {
	    setuerror(code);
	}
#endif /* KERNEL_HAVE_UERROR */
    } else {
#if defined(KERNEL_HAVE_UERROR)
	setuerror(EINVAL);
#else
	code = EINVAL;
#endif
    }

#if defined(AFS_DARWIN80_ENV)
    put_vfs_context();
#endif
#ifdef AFS_LINUX20_ENV
    code = -code;
    if (syscall != AFSCALL_OSI_TRACE) {
	unlock_kernel();
    }
#endif
    return code;
}
#endif /* AFS_SGI_ENV */
#endif /* !AFS_AIX32_ENV       */
