/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

#ifdef IGNORE_SOME_GCC_WARNINGS
# pragma GCC diagnostic warning "-Wold-style-definition"
# pragma GCC diagnostic warning "-Wstrict-prototypes"
#endif

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"
#include "rx/rx_globals.h"
#if !defined(UKERNEL) && !defined(AFS_LINUX_ENV)
#include "net/if.h"
#ifdef AFS_SGI_ENV
#include "h/hashing.h"
#endif
#if !defined(AFS_HPUX110_ENV) && !defined(AFS_DARWIN_ENV)
#include "netinet/in_var.h"
#endif
#endif /* !defined(UKERNEL) */

#if (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)) || defined(AFS_HPUX_64BIT_ENV) || defined(AFS_SUN5_64BIT_ENV) || (defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64)) || defined(NEED_IOCTL32)
static void
afs_ioctl32_to_afs_ioctl(const struct afs_ioctl32 *src, struct afs_ioctl *dst)
{
#ifdef AFS_DARWIN100_ENV
    dst->in = CAST_USER_ADDR_T(src->in);
    dst->out = CAST_USER_ADDR_T(src->out);
#else
    dst->in = (char *)(unsigned long)src->in;
    dst->out = (char *)(unsigned long)src->out;
#endif
    dst->in_size = src->in_size;
    dst->out_size = src->out_size;
}
#endif

/*
 * If you need to change copyin_afs_ioctl(), you may also need to change
 * copyin_iparam().
 */

int
#ifdef AFS_DARWIN100_ENV
copyin_afs_ioctl(user_addr_t cmarg, struct afs_ioctl *dst)
#else
copyin_afs_ioctl(caddr_t cmarg, struct afs_ioctl *dst)
#endif
{
    int code;
#if defined(AFS_DARWIN100_ENV)
    struct afs_ioctl32 dst32;

    if (!proc_is64bit(current_proc())) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif
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

#if defined(AFS_SUN5_64BIT_ENV)
    struct afs_ioctl32 dst32;

    if (get_udatamodel() == DATAMODEL_ILP32) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_SUN5_64BIT_ENV) */

#if defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64)
    struct afs_ioctl32 dst32;

    if (!ABI_IS_64BIT(get_current_abi())) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    afs_ioctl32_to_afs_ioctl(&dst32, dst);
	return code;
    }
#endif /* defined(AFS_SGI_ENV) && (_MIPS_SZLONG==64) */

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX_ENV) && !defined(AFS_IA64_LINUX_ENV)
    if (afs_in_compat_syscall()) {
	struct afs_ioctl32 dst32;

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
    int rval1 = 0, code;
    int monster;
    int retval = 0;
#ifndef AFS_AIX41_ENV
    extern lock_t kernel_lock;
    monster = lockl(&kernel_lock, LOCK_SHORT);
#endif /* !AFS_AIX41_ENV */

    AFS_STATCNT(syscall);
    setuerror(0);
    switch (syscall) {
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

    case AFSCALL_ICL:
	AFS_GLOCK();
	code = Afscall_icl(p1, p2, p3, p4, p5, &retval);
	AFS_GUNLOCK();
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
int
lsetpag(void)
{

    AFS_STATCNT(lsetpag);
    return syscall(AFSCALL_SETPAG, 0, 0, 0, 0, 0);
}

/*
 * lpioctl -	interface to pioctl()
 */
int
lpioctl(char *path, int cmd, void *cmarg, int follow)
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
    case AFSCALL_ICL:
	retval = 0;
	AFS_GLOCK();
	error =
	    Afscall_icl(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			uap->parm5, &retval);
	AFS_GUNLOCK();
	rvp->r_val1 = retval;
	break;
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
#endif
#ifdef AFS_SGI_VNODE_GLUE
    case AFSCALL_INIT_KERNEL_CONFIG:
	error = afs_init_kernel_config(uap->parm1);
	break;
#endif
    default:
	error =
	    afs_syscall_call(uap->syscall, uap->parm1, uap->parm2, uap->parm3,
			     uap->parm4, uap->parm5);
    }
    return error;
}

#else /* AFS_SGI_ENV */

struct iparam {
    iparmtype param1;
    iparmtype param2;
    iparmtype param3;
    iparmtype param4;
};

struct iparam32 {
    int param1;
    int param2;
    int param3;
    int param4;
};


#if defined(AFS_HPUX_64BIT_ENV) || defined(AFS_SUN5_64BIT_ENV) || (defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX_ENV) && !defined(AFS_IA64_LINUX_ENV)) || defined(NEED_IOCTL32)
static void
iparam32_to_iparam(const struct iparam32 *src, struct iparam *dst)
{
    dst->param1 = (iparmtype)(uintptrsz)src->param1;
    dst->param2 = (iparmtype)(uintptrsz)src->param2;
    dst->param3 = (iparmtype)(uintptrsz)src->param3;
    dst->param4 = (iparmtype)(uintptrsz)src->param4;
}
#endif

/*
 * If you need to change copyin_iparam(), you may also need to change
 * copyin_afs_ioctl().
 *
 * This function is needed only for icreate, meaning, only on platforms
 * providing the inode fileserver.
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

#if defined(AFS_SUN5_64BIT_ENV)
    struct iparam32 dst32;

    if (get_udatamodel() == DATAMODEL_ILP32) {
	AFS_COPYIN(cmarg, (caddr_t) & dst32, sizeof dst32, code);
	if (!code)
	    iparam32_to_iparam(&dst32, dst);
	return code;
    }
#endif /* AFS_SUN5_64BIT_ENV */

#if defined(AFS_LINUX_64BIT_KERNEL) && !defined(AFS_ALPHA_LINUX_ENV) && !defined(AFS_IA64_LINUX_ENV)
    if (afs_in_compat_syscall()) {
	struct iparam32 dst32;

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

Afs_syscall(struct afssysa *uap, rval_t * rvp)
{
    int *retval = &rvp->r_val1;
#elif defined(AFS_DARWIN100_ENV)
struct afssysa {
    afs_int32 syscall;
    afs_int32 parm1;
    afs_int32 parm2;
    afs_int32 parm3;
    afs_int32 parm4;
    afs_int32 parm5;
    afs_int32 parm6;
};
struct afssysa64 {
    afs_int64 parm1;
    afs_int64 parm2;
    afs_int64 parm3;
    afs_int64 parm4;
    afs_int64 parm5;
    afs_int64 parm6;
    afs_int32 syscall;
};
int
afs3_syscall(afs_proc_t *p, void *args, unsigned int *retval)
{
    struct afssysa64 *uap64 = NULL;
    struct afssysa *uap = NULL;
#elif defined(AFS_FBSD_ENV)
int
afs3_syscall(struct thread *p, void *args)
{
    struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)args;
    long fbsd_ret = 0;
    long *retval = &fbsd_ret;
#elif defined(AFS_NBSD40_ENV)
int
afs3_syscall(struct lwp *p, const void *args, register_t *retval)
{
    /* see osi_machdep.h */
    struct afs_sysargs *uap = (struct afs_sysargs *) args;
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
int
afs3_syscall(afs_proc_t *p, void *args, long *retval)
{
    struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)args;
#elif defined(AFS_LINUX_ENV)
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
    long eparm[4];		/* matches AFSCALL_ICL in fstrace.c */
# ifdef AFS_SPARC64_LINUX_ENV
    afs_int32 eparm32[4];
# endif
    /* eparm is also used by AFSCALL_CALL in afsd.c */
#else
# if defined(UKERNEL)
int
Afs_syscall(void)
{
    struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)get_user_struct()->u_ap;
# else /* UKERNEL */
int
Afs_syscall()
{
    struct a {
	long syscall;
	long parm1;
	long parm2;
	long parm3;
	long parm4;
	long parm5;
	long parm6;
    } *uap = (struct a *)u.u_ap;
# endif /* UKERNEL */
# if defined(AFS_HPUX_ENV)
    long *retval = &u.u_rval1;
# elif defined(UKERNEL)
    int *retval = &(get_user_struct()->u_rval1);
# else
    int *retval = &u.u_rval1;
# endif
#endif
    int code = 0;

    AFS_STATCNT(afs_syscall);
#ifdef        AFS_SUN5_ENV
    rvp->r_vals = 0;
    if (!afs_sinited) {
	return (ENODEV);
    }
#endif
#ifdef AFS_LINUX_ENV
    /* setup uap for use below - pull out the magic decoder ring to know
     * which syscalls have folded argument lists.
     */
    uap->syscall = syscall;
    uap->parm1 = parm1;
    uap->parm2 = parm2;
    uap->parm3 = parm3;
    if (syscall == AFSCALL_ICL || syscall == AFSCALL_CALL) {
#ifdef AFS_SPARC64_LINUX_ENV
/* from arch/sparc64/kernel/sys_sparc32.c */
#define AA(__x)                                \
({     unsigned long __ret;            \
       __asm__ ("srl   %0, 0, %0"      \
                : "=r" (__ret)         \
                : "0" (__x));          \
       __ret;                          \
})


#ifdef AFS_SPARC64_LINUX_ENV
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
#endif
	    AFS_COPYIN((char *)parm4, (char *)eparm, sizeof(eparm), code);
	uap->parm4 = eparm[0];
	uap->parm5 = eparm[1];
	uap->parm6 = eparm[2];
    } else {
	uap->parm4 = parm4;
	uap->parm5 = 0;
	uap->parm6 = 0;
    }
#endif
#if defined(AFS_DARWIN80_ENV)
    get_vfs_context();
    osi_Assert(*retval == 0);
#ifdef AFS_DARWIN100_ENV
    if (proc_is64bit(p)) {
	uap64 = (struct afssysa64 *)args;
	if (uap64->syscall == AFSCALL_CALL) {
	    code =
		afs_syscall64_call(uap64->parm1, uap64->parm2, uap64->parm3,
				   uap64->parm4, uap64->parm5, uap64->parm6);
	    /* pass back the code as syscall retval */
	    if (code < 0) {
		*retval = code;
		code = 0;
	    }
	} else if (uap64->syscall == AFSCALL_SETPAG) {
	    AFS_GLOCK();
	    code = afs_setpag(p, args, retval);
	    AFS_GUNLOCK();
	} else if (uap64->syscall == AFSCALL_PIOCTL) {
	    AFS_GLOCK();
	    code =
		afs_syscall64_pioctl(uap64->parm1, (unsigned int)uap64->parm2,
				     uap64->parm3, (int)uap64->parm4,
				     kauth_cred_get());
	    AFS_GUNLOCK();
	} else if (uap64->syscall == AFSCALL_ICL) {
	    AFS_GLOCK();
	    code =
		Afscall64_icl(uap64->parm1, uap64->parm2, uap64->parm3,
			    uap64->parm4, uap64->parm5, retval);
	    AFS_GUNLOCK();
	} else
	    code = EINVAL;
	if (uap64->syscall != AFSCALL_CALL)
	    put_vfs_context();
    } else { /* and the default case for 32 bit procs */
#endif
	uap = (struct afssysa *)args;
#endif
#if defined(AFS_HPUX_ENV)
    /*
     * There used to be code here (duplicated from osi_Init()) for
     * initializing the semaphore used by AFS_GLOCK().  Was the
     * duplication to handle the case of a dynamically loaded kernel
     * module?
     */
	osi_InitGlock();
#endif

#if defined(AFS_NBSD40_ENV)
	if (SCARG(uap, syscall) == AFSCALL_CALL) {
	    code =
		afs_syscall_call(SCARG(uap, parm1), SCARG(uap, parm2),
                                 SCARG(uap, parm3), SCARG(uap, parm4),
                                 SCARG(uap, parm5), SCARG(uap, parm6));
	} else if (SCARG(uap, syscall) == AFSCALL_SETPAG) {
#else
	if (uap->syscall == AFSCALL_CALL) {
	    code =
		afs_syscall_call(uap->parm1, uap->parm2, uap->parm3,
				 uap->parm4, uap->parm5, uap->parm6);
#ifdef AFS_DARWIN_ENV
	    /* pass back the code as syscall retval */
	    if (code < 0) {
		*retval = code;
		code = 0;
	    }
#endif
	} else if (uap->syscall == AFSCALL_SETPAG) {
#endif
#ifdef	AFS_SUN5_ENV
	    proc_t *procp;

	    procp = ttoproc(curthread);
	    AFS_GLOCK();
	    code = afs_setpag(&procp->p_cred);
	    AFS_GUNLOCK();
#else
	    AFS_GLOCK();
#if	defined(AFS_FBSD_ENV)
	    code = afs_setpag(p, args);
#elif	defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	    code = afs_setpag(p, args, retval);
#else /* AFS_DARWIN_ENV || AFS_XBSD_ENV */
	    code = afs_setpag();
#endif
	    AFS_GUNLOCK();
#endif
	} else if
#if defined(AFS_NBSD40_ENV)
		(SCARG(uap, syscall) == AFSCALL_PIOCTL) {
#else
	    (uap->syscall == AFSCALL_PIOCTL) {
#endif
	    AFS_GLOCK();
#if defined(AFS_SUN5_ENV)
	    code =
		afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3,
				   uap->parm4, rvp, CRED());
#elif defined(AFS_FBSD_ENV)
	    code =
		afs_syscall_pioctl((void *)uap->parm1, uap->parm2, (void *)uap->parm3,
				   uap->parm4, p->td_ucred);
#elif defined(AFS_DARWIN80_ENV)
	    code =
		afs_syscall_pioctl((char *)uap->parm1, uap->parm2, (caddr_t)uap->parm3,
				   uap->parm4, kauth_cred_get());
#elif defined(AFS_NBSD40_ENV)
	    code =
		afs_syscall_pioctl((char *)SCARG(uap, parm1), SCARG(uap, parm2),
				   (void *)SCARG(uap, parm3), SCARG(uap, parm4),
				   kauth_cred_get());
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
	    code =
		afs_syscall_pioctl(uap->parm1, uap->parm2, uap->parm3,
				   uap->parm4, p->p_cred->pc_ucred);
#else
	    code =
		afs_syscall_pioctl((char *)uap->parm1,
				   (unsigned int)uap->parm2,
				   (caddr_t)uap->parm3,
				   (int) uap->parm4);
#endif
	    AFS_GUNLOCK();

#ifdef AFS_NBSD40_ENV
	    } else if (SCARG(uap, syscall) == AFSCALL_ICREATE) {
		struct iparam iparams;
		code = copyin_iparam((char *) SCARG(uap, parm3), &iparams);
#else
	} else if (uap->syscall == AFSCALL_ICREATE) {
	    struct iparam iparams;

	    code = copyin_iparam((char *)uap->parm3, &iparams);
#endif
	    if (code) {
#if defined(KERNEL_HAVE_UERROR)
		setuerror(code);
#endif
	    } else {
#if defined(AFS_SUN5_ENV)
		code =
		    afs_syscall_icreate(uap->parm1, uap->parm2, iparams.param1,
					iparams.param2, iparams.param3,
					iparams.param4, rvp, CRED());
#elif defined(AFS_NBSD40_ENV)
		code =
			afs_syscall_icreate(SCARG(uap, parm1), SCARG(uap, parm2),
				iparams.param1, iparams.param2, iparams.param3,
				iparams.param4, retval
			);
#else
		code =
		    afs_syscall_icreate(uap->parm1, uap->parm2, iparams.param1,
					iparams.param2, iparams.param3,
					iparams.param4
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
					, retval
#endif
			);
#endif /* AFS_SUN5_ENV */
	    }
#if defined(AFS_NBSD40_ENV)
	    } else if (SCARG(uap, syscall) == AFSCALL_IOPEN) {
#else
	    } else if (uap->syscall == AFSCALL_IOPEN) {
#endif /* !AFS_NBSD40_ENV */
#if defined(AFS_SUN5_ENV)
	    code =
		afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3, rvp,
				  CRED());
#elif defined(AFS_NBSD40_ENV)
	    code = afs_syscall_iopen(SCARG(uap, parm1), SCARG(uap, parm2),
				     SCARG(uap, parm3), retval);
#else
	    code = afs_syscall_iopen(uap->parm1, uap->parm2, uap->parm3
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
				     , retval
#endif
		);
#endif /* AFS_SUN5_ENV */
#if defined(AFS_NBSD40_ENV)
        } else if (SCARG(uap, syscall) == AFSCALL_IDEC) {
#else
	} else if (uap->syscall == AFSCALL_IDEC) {
#endif
#if defined(AFS_NBSD40_ENV)
	    code = afs_syscall_iincdec(SCARG(uap, parm1), SCARG(uap, parm2),
					 SCARG(uap, parm3), -1);
#else


	    code =
		afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, -1
#if defined(AFS_SUN5_ENV)
				    , rvp, CRED()
#endif
		    );

#endif /* !AFS_NBSD40_ENV */
#if defined(AFS_NBSD40_ENV)
	    } else if (SCARG(uap, syscall) == AFSCALL_IINC) {
#else
	    } else if (uap->syscall == AFSCALL_IINC) {
#endif
#if defined(AFS_NBSD40_ENV)
	      code = afs_syscall_iincdec(SCARG(uap, parm1), SCARG(uap, parm2),
					 SCARG(uap, parm3), 1);
#else
	    code =
		afs_syscall_iincdec(uap->parm1, uap->parm2, uap->parm3, 1
#ifdef	AFS_SUN5_ENV
				    , rvp, CRED()
#endif
		    );
#endif /* !AFS_NBSD40_ENV */
#if defined(AFS_NBSD40_ENV)
	    } else if (SCARG(uap, syscall) == AFSCALL_ICL) {
#else
	    } else if (uap->syscall == AFSCALL_ICL) {
#endif
	    AFS_GLOCK();
	    code =
#if defined(AFS_NBSD40_ENV)
	      Afscall_icl(SCARG(uap, parm1), SCARG(uap, parm2),
			  SCARG(uap, parm3), SCARG(uap, parm4),
			  SCARG(uap, parm5), retval);
#else
		Afscall_icl(uap->parm1, uap->parm2, uap->parm3, uap->parm4,
			    uap->parm5, (long *)retval);
#endif /* !AFS_NBSD40_ENV */
	    AFS_GUNLOCK();
#ifdef AFS_LINUX_ENV
	    if (!code) {
		/* ICL commands can return values. */
		code = -linux_ret;	/* Gets negated again at exit below */
	    }
#else
	    if (code) {
#if defined(KERNEL_HAVE_UERROR)
		setuerror(code);
#endif
	    }
#endif /* !AFS_LINUX_ENV */
	} else {
#if defined(KERNEL_HAVE_UERROR)
	    setuerror(EINVAL);
#else
	    code = EINVAL;
#endif
	}
#if defined(AFS_DARWIN80_ENV)
	if (uap->syscall != AFSCALL_CALL)
	    put_vfs_context();
#ifdef AFS_DARWIN100_ENV
    } /* 32 bit procs */
#endif
#endif
#ifdef AFS_LINUX_ENV
    code = -code;
#endif
    return code;
}
#endif /* AFS_SGI_ENV */
#endif /* !AFS_AIX32_ENV */
