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
 * OpenBSD OSI header file. Extends afs_osi.h.
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
#include <sys/malloc.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include "opt_ddb.h" /* Debugger() */

#define M_AFSFID	(M_TEMP-1)
#define M_AFSBUFHDR	(M_TEMP-2)
#define M_AFSBUFFER	(M_TEMP-3)
#define M_AFSGENERIC	(M_TEMP-4)

/* vfs */
#define osi_vfs		mount
#define osi_vfs_bsize	mnt_stat.f_bsize
#define osi_vfs_iosize	mnt_stat.f_iosize
#define osi_vfs_fsid	mnt_stat.f_fsid
#if 0
#define vfs_bsize	mnt_stat.f_bsize
#define vfs_fsid	mnt_stat.f_fsid
#endif
#define vfs_vnodecovered mnt_vnodecovered
#define v_vfsp		v_mount
#define VFS_STATFS      afs_statvfs

int
sys_ioctl(struct lwp *l, void *v, register_t *retval);

/* vnode */
#define VN_HOLD(vp)	(vref(vp))
#define VN_RELE(vp)	(vrele(vp))
#define osi_vnhold(avc, r) (VN_HOLD(AFSTOV(avc)))


#define va_nodeid	va_fileid
#define vnode_t		struct vnode

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

/* malloc */
inline void * afs_osi_Alloc(size_t asize);
inline void * afs_osi_Alloc_NoSleep(size_t asize);
inline void afs_osi_Free(void *buf, size_t asize);
inline void afs_osi_FreeStr(char *x);
extern void *osi_nbsd_Alloc(size_t asize, int cansleep);
extern void osi_nbsd_Free(void *p, size_t asize);

#ifdef AFS_KALLOC
#undef AFS_KALLOC
#define AFS_KALLOC(s) (osi_nbsd_Alloc((s), 1 /* cansleep */))
#endif

#ifdef AFS_KFREE
#undef AFS_KFREE
#define AFS_KFREE(p, s) (osi_nbsd_Free((p), (s)))
#endif

/* proc */
typedef struct lwp afs_proc_t;
#define osi_curproc()	curlwp
#define getpid()	(osi_curproc())->l_proc->p_pid

/*
 * XXX  I'm exporting the internal definition of kauth_cred_t
 * until I work out protocol for switching group buffers.
 * Matt.
 */

#define KAUTH_EXPORT 1
#if defined(KAUTH_EXPORT)
/* internal type from kern_auth.c */
#ifdef AFS_NBSD40_ENV
struct kauth_cred {
	struct simplelock cr_lock;	/* lock on cr_refcnt */
	u_int cr_refcnt;		/* reference count */
	uid_t cr_uid;			/* user id */
	uid_t cr_euid;			/* effective user id */
	uid_t cr_svuid;			/* saved effective user id */
	gid_t cr_gid;			/* group id */
	gid_t cr_egid;			/* effective group id */
	gid_t cr_svgid;			/* saved effective group id */
	u_int cr_ngroups;		/* number of groups */
	gid_t cr_groups[NGROUPS];	/* group memberships */
};
#else
#error TODO:  verify kauth_cred structure, if this is still here
#endif /* AFS_NBSD40_ENV */
#endif /* KAUTH_EXPORT */

typedef kauth_cred_t afs_ucred_t;
#define osi_curcred()	(kauth_cred_get())
#define afs_suser(x)	afs_osi_suser(osi_curcred())
#define osi_crgetruid(acred) (kauth_cred_getuid(acred))
#define osi_crgetrgid(acred) (kauth_cred_getgid(acred))
#define osi_crngroups(acred) (kauth_cred_ngroups(acred))
#define osi_proccred(aproc) (aproc->p_cred)
#define osi_crdup(acred) (kauth_cred_dup(acred))
#define crref osi_crdup
#define crdup osi_crdup
#define crhold crref
#define crfree kauth_cred_free

#define afs_cr_gid osi_crgetrgid
#define afs_cr_uid osi_crgetruid

inline gid_t osi_crgroupbyid(afs_ucred_t *acred, int gindex);

/* time */
#define	afs_hz		hz
#define osi_GetTime(x)	microtime(x)
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

extern int afs_nbsd_lookupname(char *fnamep, enum uio_seg segflg,
			       int followlink, struct vnode **compvpp);
extern void afs_nbsd_getnewvnode(struct vcache *tvc);
extern int afs_vget();

#undef gop_lookupname
#undef gop_lookupname_user
#define	osi_lookupname_user(fnamep, segflg, followlink, compvpp) \
	afs_nbsd_lookupname((fnamep), (segflg), (followlink), (compvpp))
#define gop_lookupname_user osi_lookupname_user
#define gop_lookupname osi_lookupname_user

#ifdef KERNEL
#ifdef AFS_GLOBAL_SUNLOCK
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
#define ISAFS_GLOCK() (lockstatus(&afs_global_lock) == LK_EXCLUSIVE)
#else
extern struct lock afs_global_lock;
#define AFS_GLOCKP(p)
#define AFS_GUNLOCKP(p)
#define AFS_ASSERT_GLOCK()
#define ISAFS_GLOCK() 1
#endif

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
extern int (**afs_vnodeop_p) ();
#define vType(vc)               AFSTOV(vc)->v_type
#define vSetVfsp(vc, vfsp)      AFSTOV(vc)->v_mount = (vfsp)
#define vSetType(vc, type)      AFSTOV(vc)->v_type = (type)
#define IsAfsVnode(v)      ((v)->v_op == afs_vnodeop_p)
#define SetAfsVnode(v)     /* nothing; done in getnewvnode() */

#endif /* _OSI_MACHDEP_H_ */
