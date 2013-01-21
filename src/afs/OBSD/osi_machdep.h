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

/* $Id$ */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include <sys/lock.h>

#if defined(M_IP6OPT)
#define M_AFSFID	(M_IP6OPT-1)
#else
#define M_AFSFID	(M_TEMP-1)
#endif

#define M_AFSBUFHDR	(M_AFSFID-1)
#define M_AFSBUFFER	(M_AFSFID-2)
#define M_AFSGENERIC	(M_AFSFID-3)

/* vfs */
#define osi_vfs		mount
#define osi_vfs_bsize	mnt_stat.f_bsize
#define osi_vfs_fsid	mnt_stat.f_fsid
#define vfs_bsize	mnt_stat.f_bsize
#define vfs_fsid	mnt_stat.f_fsid
#define vfs_vnodecovered mnt_vnodecovered
#define v_vfsp		v_mount

/* vnode */
#define VN_HOLD(vp)	afs_vget((vp), 0)
#define VN_RELE(vp)	vrele(vp)
#define osi_vnhold(avc, r) afs_vget(AFSTOV(avc), 0)
#define va_nodeid	va_fileid
#define vnode_t		struct vnode

/* uio */
#define afsio_iov	uio_iov
#define afsio_iovcnt	uio_iovcnt
#define afsio_offset	uio_offset
#define afsio_resid	uio_resid
#define afsio_seg	uio_segflg
#define AFS_UIOSYS	UIO_SYSSPACE
#define AFS_UIOUSER	UIO_USERSPACE

/* malloc */
inline void afs_osi_Free(void *buf, size_t asize);
inline void afs_osi_FreeStr(char *x);
extern void *osi_obsd_Alloc(size_t asize, int cansleep);
extern void osi_obsd_Free(void *p, size_t asize);

#ifdef AFS_KALLOC
#undef AFS_KALLOC
#endif
#define AFS_KALLOC(s) osi_obsd_Alloc((s), 1 /* cansleep */)

#ifdef AFS_KFREE
#undef AFS_KFREE
#endif
#define AFS_KFREE(p, s) (osi_obsd_Free((p), (s)))

#ifdef AFS_OBSD42_ENV
/* removed, live with it */
#define BSD_KMALLOC(p, ptype, msize, mtype, mflags)     \
  (p) = malloc((msize), (mtype), (mflags))

#define BSD_KFREE(p, mflags) \
  free((p), (mflags))
#else
#define BSD_KMALLOC MALLOC
#define BSD_KFREE FREE
#endif /* AFS_OBSD42_ENV */

/* proc, cred */
typedef struct proc afs_proc_t;
typedef struct ucred afs_ucred_t;

#define afs_suser(x)	afs_osi_suser(osi_curcred())
#define getpid()	curproc
#define osi_curcred()	(curproc->p_cred->pc_ucred)
#define osi_curproc()	curproc
#define p_rcred         p_ucred

/* time */
#define	afs_hz		hz
#define osi_GetTime(x)	microtime(x)
extern time_t osi_Time();

/* str */
#define afs_strcasecmp(s1, s2)	strncasecmp((s1), (s2), 65535)
#ifdef AFS_OBSD34_ENV
#define strcpy(s1, s2)		afs_strcpy((s1), (s2))
#define strcat(s1, s2)		afs_strcat((s1), (s2))
#else
#define afs_strcat(s1, s2)	strcat((s1), (s2))
#endif

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

extern int afs_obsd_lookupname(char *fnamep, enum uio_seg segflg,
			       int followlink, struct vnode **compvpp);
extern void afs_obsd_getnewvnode(struct vcache *tvc);
extern void *afs_obsd_Alloc(size_t asize);
extern void afs_obsd_Free(void *p, size_t asize);
extern int afs_vget();

#undef gop_lookupname
#define	gop_lookupname(fnamep, segflg, followlink, compvpp) \
	afs_obsd_lookupname((fnamep), (segflg), (followlink), (compvpp))

#undef gop_lookupname_user
#define	gop_lookupname_user(fnamep, segflg, followlink, compvpp) \
	afs_obsd_lookupname((fnamep), (segflg), (followlink), (compvpp))

#ifdef AFS_OBSD39_ENV
#define afs_osi_lockmgr(l, f, i, p) lockmgr((l), (f), (i))
#else
#define afs_osi_lockmgr(l, f, i, p) lockmgr((l), (f), (i), (p))
#endif

#ifdef AFS_OBSD44_ENV
/* Revert to classical, BSD locks */

extern struct lock afs_global_lock;
extern struct proc *afs_global_owner;

# ifdef AFS_GLOBAL_SUNLOCK

#  if defined(LOCKDEBUG)

#   define AFS_GLOCK() \
  do { \
  _lockmgr(&afs_global_lock, LK_EXCLUSIVE, NULL, __FILE__, __LINE__); \
  } while(0);
#   define AFS_GUNLOCK() \
  do { \
  _lockmgr(&afs_global_lock, LK_RELEASE, NULL, __FILE__, __LINE__); \
  } while(0);

#  else /* LOCKDEBUG */

#   define AFS_GLOCK() \
  do { \
  lockmgr(&afs_global_lock, LK_EXCLUSIVE, NULL); \
  } while(0);
#   define AFS_GUNLOCK() \
  do { \
  lockmgr(&afs_global_lock, LK_RELEASE, NULL); \
  } while(0);
#  endif /* LOCKDEBUG */
#  define ISAFS_GLOCK() (lockstatus(&afs_global_lock) == LK_EXCLUSIVE)
# else /* AFS_GLOBAL_SUNLOCK */
extern struct lock afs_global_lock;
#  define AFS_GLOCK()
#  define AFS_GUNLOCK()
#  define AFS_ASSERT_GLOCK()
#  define ISAFS_GLOCK() 1
# endif

#else /* AFS_OBSD44_ENV */
/* I don't see doing locks this way for older kernels, either,
 * but, smart folks wrote this
 */
#define AFS_GLOCK() AFS_GLOCKP(curproc)
#define AFS_GUNLOCK() AFS_GUNLOCKP(curproc)
# ifdef AFS_GLOBAL_SUNLOCK
extern struct proc *afs_global_owner;
extern struct lock afs_global_lock;
#  define AFS_GLOCKP(p) \
    do { \
        osi_Assert(p); \
 	afs_osi_lockmgr(&afs_global_lock, LK_EXCLUSIVE, 0, (p)); \
        osi_Assert(afs_global_owner == NULL); \
   	afs_global_owner = (p); \
    } while (0)
#  define AFS_GUNLOCKP(p) \
    do { \
        osi_Assert(p); \
 	osi_Assert(afs_global_owner == (p)); \
        afs_global_owner = NULL; \
        afs_osi_lockmgr(&afs_global_lock, LK_RELEASE, 0, (p)); \
    } while(0)
#  define ISAFS_GLOCK() (afs_global_owner == curproc && curproc)
# else /* AFS_GLOBAL_SUNLOCK */
extern struct lock afs_global_lock;
#  define AFS_GLOCKP(p)
#  define AFS_GUNLOCKP(p)
#  define AFS_ASSERT_GLOCK()
#  define ISAFS_GLOCK() 1
# endif

#endif /* AFS_OBSD44_ENV */

#undef SPLVAR
#define SPLVAR int splvar
#undef NETPRI
#define NETPRI splvar=splnet()
#undef USERPRI
#define USERPRI splx(splvar)

#define osi_InitGlock() \
    do { \
	lockinit(&afs_global_lock, PLOCK, "afs global lock", 0, 0); \
	afs_global_owner = 0; \
    } while (0)

/* vnodes */
#if defined(AFS_OBSD49_ENV)
extern struct vops afs_vops;
#define IsAfsVnode(v)      ((v)->v_op == &afs_vops)
#else
extern int (**afs_vnodeop_p) ();
#define IsAfsVnode(v)      ((v)->v_op == afs_vnodeop_p)
#endif
#define vType(vc)               AFSTOV(vc)->v_type
#define vSetVfsp(vc, vfsp)      AFSTOV(vc)->v_mount = (vfsp)
#define vSetType(vc, type)      AFSTOV(vc)->v_type = (type)
#define SetAfsVnode(v)     /* nothing; done in getnewvnode() */

#define osi_procname(procname, size) strncpy(procname, curproc->p_comm, size)

#endif /* _OSI_MACHDEP_H_ */
