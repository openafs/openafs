/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *
 * NetBSD OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#define RXK_LISTENER_ENV 1
#define AFS_DIRENT 1

#include <sys/lock.h>
#if defined(AFS_NBSD50_ENV)
#include <sys/kmem.h>
#include <sys/specificdata.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#endif
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#if defined(AFS_NBSD50_ENV)
# if !defined(DEF_CADDR_T)
typedef char * caddr_t;
#define DEF_CADDR_T
# endif
#endif

/* vfs */
#define osi_vfs		mount
#define osi_vfs_bsize	mnt_stat.f_bsize
#define osi_vfs_iosize	mnt_stat.f_iosize
#define osi_vfs_fsid	mnt_stat.f_fsid
#define vfs_vnodecovered mnt_vnodecovered
#define v_vfsp		v_mount

/* vnode */
#define VN_HOLD(vp)	(vget(vp, 0))
#define VN_RELE(vp)	(vrele(vp))

#define va_nodeid	va_fileid

/* syscall */
struct afs_sysargs {
    syscallarg(long) syscall;
    syscallarg(long) parm1;
    syscallarg(long) parm2;
    syscallarg(long) parm3;
    syscallarg(long) parm4;
    syscallarg(long) parm5;
    syscallarg(long) parm6;
};

/* uio */
#define afsio_iov	uio_iov
#define afsio_iovcnt	uio_iovcnt
#define afsio_offset	uio_offset
#define afsio_resid	uio_resid
#define afsio_seg	uio_segflg
#define AFS_UIOSYS	UIO_SYSSPACE
#define AFS_UIOUSER	UIO_USERSPACE

/* proc */
typedef struct lwp afs_proc_t;
#define osi_curproc()	curlwp
#define getpid()	(osi_curproc())->l_proc->p_pid
#define osi_procname(procname, size) strncpy(procname, curproc->p_comm, size)

typedef struct kauth_cred afs_ucred_t;
#define osi_curcred()	(kauth_cred_get())
#define afs_suser(x)	afs_osi_suser(osi_curcred())
#define osi_crgetruid(acred) (kauth_cred_getuid(acred))
#define osi_crgetrgid(acred) (kauth_cred_getgid(acred))
#define osi_crngroups(acred) (kauth_cred_ngroups(acred))
#define osi_proccred(aproc) ((aproc)->l_proc->p_cred)
#define osi_crgroupbyid kauth_cred_group
#define crdup kauth_cred_dup
#define crhold kauth_cred_hold
#define crfree kauth_cred_free

#define afs_cr_gid osi_crgetrgid
#define afs_cr_uid osi_crgetruid

/* time */
#define	afs_hz		hz
#define osi_Time()      time_second

/* str */
#define afs_strcasecmp(s1, s2)	strncasecmp((s1), (s2), 65535)
#define afs_strcat(s1, s2)	strcat((s1), (s2))

/* other */
#define afs_bufferpages bufpages
#ifndef iodone
#define iodone biodone
#endif
#define printk		printf	/* for RX version of xdr_* */
#define setgroups	sys_setgroups
#define UVM

/* This is not always in scope yet */
struct vcache;

extern int afs_nbsd_lookupname(const char *fnamep, enum uio_seg segflg,
			       int followlink, struct vnode **compvpp);
extern void afs_nbsd_getnewvnode(struct vcache *tvc);

#undef gop_lookupname
#undef gop_lookupname_user
#define	osi_lookupname_user(fnamep, segflg, followlink, compvpp) \
	afs_nbsd_lookupname((fnamep), (segflg), (followlink), (compvpp))
#define gop_lookupname_user osi_lookupname_user
#define gop_lookupname osi_lookupname_user

#ifdef KERNEL
#ifdef AFS_GLOBAL_SUNLOCK

#if defined(AFS_NBSD50_ENV)
extern kmutex_t afs_global_mtx;
#define AFS_GLOCK() \
    do { \
	mutex_enter(&afs_global_mtx); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	mutex_exit(&afs_global_mtx); \
    } while (0)
#define ISAFS_GLOCK() (mutex_owned(&afs_global_mtx))
#define osi_InitGlock() \
    do { \
	mutex_init(&afs_global_mtx, MUTEX_DEFAULT, IPL_NONE); \
    } while (0)
#else /* !50 */
extern struct lock afs_global_lock;
#if defined(LOCKDEBUG)
#define AFS_GLOCK() \
  do { \
      _lockmgr(&afs_global_lock, LK_EXCLUSIVE, NULL, __FILE__, __LINE__); \
  } while(0);
#define AFS_GUNLOCK() \
  do { \
      _lockmgr(&afs_global_lock, LK_RELEASE, NULL, __FILE__, __LINE__); \
  } while(0);
#else
#define AFS_GLOCK() \
  do { \
    lockmgr(&afs_global_lock, LK_EXCLUSIVE, NULL); \
  } while(0);
#define AFS_GUNLOCK() \
  do { \
    lockmgr(&afs_global_lock, LK_RELEASE, NULL); \
  } while(0);
#endif /* LOCKDEBUG */
#endif /* !50 */
#else
#define ISAFS_GLOCK() (lockstatus(&afs_global_lock) == LK_EXCLUSIVE)
extern struct lock afs_global_lock;
#define AFS_GLOCKP(p)
#define AFS_GUNLOCKP(p)
#define AFS_ASSERT_GLOCK()
#define ISAFS_GLOCK() 1
#endif /* !AFS_GLOBAL_SUNLOCK */

#undef SPLVAR
#define SPLVAR int splvar
#undef NETPRI
#define NETPRI splvar=splnet()
#undef USERPRI
#define USERPRI splx(splvar)
#endif /* KERNEL */

#if	!defined(ASSEMBLER) && !defined(__LANGUAGE_ASSEMBLY__) && !defined(IGNORE_STDS_H)
enum vcexcl { NONEXCL, EXCL };

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif
#ifndef MAX
#define MAX(A,B) ((A) > (B) ? (A) : (B))
#endif

#endif /* ASSEMBLER */

/* vnodes */
extern int (**afs_vnodeop_p) __P((void *));
#define vType(vc)               AFSTOV(vc)->v_type
#define vSetVfsp(vc, vfsp)      AFSTOV(vc)->v_mount = (vfsp)
#define vSetType(vc, type)      AFSTOV(vc)->v_type = (type)
#define IsAfsVnode(v)      ((v)->v_op == afs_vnodeop_p)
#define SetAfsVnode(v)     /* nothing; done in getnewvnode() */

extern int afs_debug;

#define AFS_USE_NBSD_NAMECACHE 0

static_inline void
osi_GetTime(osi_timeval32_t *atv)
{
    struct timeval now;
    getmicrotime(&now);
    atv->tv_sec = now.tv_sec;
    atv->tv_usec = now.tv_usec;
}

#endif /* _OSI_MACHDEP_H_ */
