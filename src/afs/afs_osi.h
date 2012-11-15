/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AFS_OSI_
#define _AFS_OSI_

#include "h/types.h"
#if !defined(AFS_LINUX26_ENV)
#include "h/param.h"
#endif

#ifdef AFS_FBSD_ENV
#include <sys/condvar.h>
#endif

#ifdef AFS_NBSD_ENV
#include <sys/lock.h>
#endif

#ifdef AFS_LINUX20_ENV
#ifndef _LINUX_CODA_FS_I
#define _LINUX_CODA_FS_I
#define _CODA_HEADER_
struct coda_inode_info {
};
#endif
#ifndef _LINUX_XFS_FS_I
#define _LINUX_XFS_FS_I
struct xfs_inode_info {
};
#endif
#include "h/fs.h"
#include "h/mm.h"
#endif


/* this is just a dummy type decl, we're really using struct sockets here */
struct osi_socket {
    int junk;
};

struct osi_stat {
    afs_int32 size;		/* file size in bytes */
    afs_int32 mtime;		/* modification date */
    afs_int32 atime;		/* access time */
};

struct osi_file {
    afs_int32 size;		/* file size in bytes XXX Must be first field XXX */
#ifdef AFS_LINUX26_ENV
    struct file *filp;		/* May need this if we really open the file. */
#else
#ifdef AFS_LINUX22_ENV
    struct dentry dentry;	/* merely to hold the pointer to the inode. */
    struct file file;		/* May need this if we really open the file. */
#else
    struct vnode *vnode;
#endif
#endif
#if	defined(AFS_HPUX102_ENV)
    k_off_t offset;
#else
#if defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL)
    afs_offs_t offset;
#else
    afs_int32 offset;
#endif
#endif
    int (*proc) (struct osi_file * afile, afs_int32 code);	/* proc, which, if not null, is called on writes */
    char *rock;			/* rock passed to proc */
#if defined(UKERNEL)
    int fd;			/* file descriptor for user space files */
#endif				/* defined(UKERNEL) */
};

struct osi_dev {
#if defined(AFS_XBSD_ENV)
    struct mount *mp;
    struct vnode *held_vnode;
#elif defined(AFS_AIX42_ENV)
    dev_t dev;
#else
    afs_int32 dev;
#endif
};

struct afs_osi_WaitHandle {
#ifdef AFS_FBSD_ENV
    struct cv wh_condvar;
    int wh_inited;
#else
    caddr_t proc;		/* process waiting */
#endif
};

#define	osi_SetFileProc(x,p)	((x)->proc=(p))
#define	osi_SetFileRock(x,r)	((x)->rock=(r))
#define	osi_GetFileProc(x)	((x)->proc)
#define	osi_GetFileRock(x)	((x)->rock)

#ifdef	AFS_TEXT_ENV
#define osi_FlushText(vp) if (hcmp((vp)->f.m.DataVersion, (vp)->flushDV) > 0) \
			    osi_FlushText_really(vp)
#else
#define osi_FlushText(vp)
#endif


#define AFSOP_STOP_RXEVENT   214	/* stop rx event deamon */
#define AFSOP_STOP_COMPLETE  215	/* afs has been shutdown */
#define AFSOP_STOP_RXK_LISTENER   217	/* stop rx listener daemon */


#define	osi_NPACKETS	20	/* number of cluster pkts to alloc */

/*
 * Alloc declarations.
 */
#if !defined(AFS_OBSD44_ENV) && !defined(AFS_NBSD_ENV)
#define afs_osi_Alloc_NoSleep afs_osi_Alloc
#endif

/*
 * Default vnode related macros
 *
 * Darwin, all of the BSDs, and Linux have their own
 */
#if !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV) && !defined(AFS_LINUX20_ENV)
# define	vType(vc)	    (vc)->v.v_type
# define	vSetType(vc,type)   (vc)->v.v_type = (type)
# define	vSetVfsp(vc,vfsp)   (vc)->v.v_vfsp = (vfsp)
extern struct vnodeops *afs_ops;
# define	IsAfsVnode(v)	    ((v)->v_op == afs_ops)
# define	SetAfsVnode(v)	    (v)->v_op = afs_ops
#endif

struct vcache;
extern int osi_TryEvictVCache(struct vcache *, int *, int);
extern struct vcache *osi_NewVnode(void);
extern void osi_PrePopulateVCache(struct vcache *);
extern void osi_PostPopulateVCache(struct vcache *);
extern void osi_AttachVnode(struct vcache *, int seq);

/*
 * In IRIX 6.5 and NetBSD we cannot have DEBUG turned on since certain
 * system-defined structures are a different size with DEBUG on, the
 * kernel is compiled without DEBUG on, and the resulting differences
 * would break our ability to interact with the rest of the kernel.
 *
 * Is DEBUG only for turning the ASSERT() macro?  If so, we should
 * be able to eliminate DEBUG entirely.
 */
#if !defined(AFS_SGI65_ENV) && !defined(AFS_NBSD_ENV)
#ifndef	DEBUG
#define	DEBUG	1		/* Default is to enable debugging/logging */
#endif
#endif

/*
 * Time related macros
 */
#define osi_GetuTime(x) osi_GetTime(x)

/* osi_timeval_t exists because SGI 6.x has two sizes of timeval. */
/** In 64 bit Solaris the timeval structure has members that are 64 bit
  * In the GetTime() interface we expect pointers to afs_int32. So the need to
  * define osi_timeval_t to have 32 bit members. To make this less ambiguous
  * we now use 32 bit quantities consistently all over the code.
  * In 64 bit HP-UX the timeval structure has a 64 bit member.
  */

#if defined(AFS_HPUX_ENV) || defined(AFS_LINUX_64BIT_KERNEL) || (defined(AFS_SGI61_ENV) && defined(KERNEL) && defined(_K64U64))
typedef struct {
    afs_int32 tv_sec;
    afs_int32 tv_usec;
} osi_timeval_t;
typedef struct {
    afs_int32 tv_sec;
    afs_int32 tv_usec;
} osi_timeval32_t;
#elif defined(AFS_SUN57_ENV)
typedef struct timeval32 osi_timeval_t;
typedef struct timeval32 osi_timeval32_t;
#else
typedef struct timeval osi_timeval_t;
typedef struct timeval osi_timeval32_t;
#endif /* AFS_SGI61_ENV */

#ifndef UKERNEL
#define osi_getpid() 		getpid()
#endif

/*
 * osi_ThreadUnique() should yield a value that can be found in ps
 * output in order to draw correspondences between ICL traces and what
 * is going on in the system.  So if ps cannot show thread IDs it is
 * likely to be the process ID instead.
 */
#ifdef AFS_FBSD_ENV
/* should use curthread, but 'ps' can't display it */
#define osi_ThreadUnique()	(curproc->p_pid)
#elif defined(AFS_LINUX_ENV)
#define osi_ThreadUnique()	(current->pid)
#elif defined(UKERNEL)
#define osi_ThreadUnique()	osi_getpid()
#else
#define osi_ThreadUnique()	getpid()
#endif



#ifdef AFS_GLOBAL_SUNLOCK
#define AFS_ASSERT_GLOCK() \
    do { if (!ISAFS_GLOCK()) osi_Panic("afs global lock not held at %s:%d\n", __FILE__, __LINE__); } while (0)
#endif /* AFS_GLOBAL_SUNLOCK */

#ifdef RX_ENABLE_LOCKS
#define RX_AFS_GLOCK()		AFS_GLOCK()
#define RX_AFS_GUNLOCK()	AFS_GUNLOCK()
#else
#define RX_AFS_GLOCK()
#define RX_AFS_GUNLOCK()
#endif



#ifndef KERNEL
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define ISAFS_GLOCK() 1
#define AFS_ASSERT_GLOCK()
#endif

/* On an MP that uses multithreading, splnet is not sufficient to provide
 * mutual exclusion because the other processors will not see it.  On some
 * early multiprocessors (SunOS413 & SGI5.2) splnet actually obtains a global
 * mutex, which this works in the UP expected way, it means that the whole MP
 * can only take one interrupt at a time; a serious performance penalty. */

#if ((defined(AFS_GLOBAL_SUNLOCK) || defined(RX_ENABLE_LOCKS)) && !defined(AFS_HPUX_ENV)) || !defined(KERNEL)
#define SPLVAR
#define NETPRI
#define USERPRI
#endif

/*
 * vnode/vcache ref count manipulation
 */
#if defined(UKERNEL)
#define AFS_RELE(vp) do { VN_RELE(vp); } while (0)
#else /* defined(UKERNEL) */
#define AFS_RELE(vp) do { AFS_GUNLOCK(); VN_RELE(vp); AFS_GLOCK(); } while (0)
#endif /* defined(UKERNEL) */

/*
 * For some reason we do bare refcount manipulation in some places, for some
 * platforms.  The assumption is apparently that either we wouldn't call
 * afs_inactive anyway (because we know the ref count is high), or that it's
 * OK not to call it (because we don't expect CUnlinked or CDirty).
 * (Also, of course, the vnode is assumed to be one of ours.  Can't use this
 * macro for V-file vnodes.)
 */
/* osi_vnhold is defined in PLATFORM/osi_machdep.h */
#define AFS_FAST_HOLD(vp) osi_vnhold((vp), 0)

#ifdef AFS_AIX_ENV
#define AFS_FAST_RELE(vp) VREFCOUNT_DEC(vp)
#else
#define AFS_FAST_RELE(vp) AFS_RELE(AFSTOV(vp))
#endif

/*
 * MP safe versions of routines to copy memory between user space
 * and kernel space. Call these to avoid taking page faults while
 * holding the global lock.
 */
#if defined(CAST_USER_ADDR_T) && !defined(UKERNEL) && !defined(AFS_DARWIN100_ENV)
#define __U(X) CAST_USER_ADDR_T((X))
#else
#define __U(X) (X)
#endif
#ifdef AFS_GLOBAL_SUNLOCK

#define AFS_COPYIN(SRC,DST,LEN,CODE)				\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = copyin(__U((SRC)),(DST),(LEN));	\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)

#define AFS_COPYINSTR(SRC,DST,LEN,CNT,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = copyinstr(__U((SRC)),(DST),(LEN),(CNT));		\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)

#define AFS_COPYOUT(SRC,DST,LEN,CODE)				\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = copyout((SRC),__U((DST)),(LEN));			\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)

#if defined(AFS_DARWIN80_ENV)
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    uio_setrw((UIO),(RW));				\
	    CODE = uiomove((SRC),(LEN),(UIO));			\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    (UIO)->uio_rw = (RW);				\
	    CODE = uiomove((SRC),(LEN),(UIO));			\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)
#else
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    int haveGlock = ISAFS_GLOCK();			\
	    if (haveGlock)					\
		AFS_GUNLOCK();					\
	    CODE = uiomove((SRC),(LEN),(RW),(UIO));		\
	    if (haveGlock)					\
		AFS_GLOCK();					\
	} while(0)
#endif
#endif /* AFS_DARWIN80_ENV */

#else /* AFS_GLOBAL_SUNLOCK */

#define AFS_COPYIN(SRC,DST,LEN,CODE)				\
	do {							\
	    CODE = copyin(__U((SRC)),(DST),(LEN));			\
	} while(0)

#define AFS_COPYINSTR(SRC,DST,LEN,CNT,CODE)			\
	do {							\
	    CODE = copyinstr(__U((SRC)),(DST),(LEN),(CNT));		\
	} while(0)

#define AFS_COPYOUT(SRC,DST,LEN,CODE)				\
	do {							\
	    CODE = copyout((SRC),__U((DST)),(LEN));			\
	} while(0)

#if defined(AFS_DARWIN80_ENV)
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    uio_setrw((UIO),(RW));				\
	    CODE = uiomove((SRC),(LEN),(UIO));			\
	} while(0)
#elif defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    (UIO)->uio_rw = (RW);				\
	    CODE = uiomove((SRC),(LEN),(UIO));			\
	} while(0)
#else
#define AFS_UIOMOVE(SRC,LEN,RW,UIO,CODE)			\
	do {							\
	    CODE = uiomove((SRC),(LEN),(RW),(UIO));		\
	} while(0)
#endif

#endif /* AFS_GLOBAL_SUNLOCK */

#ifdef AFS_DARWIN80_ENV
#define AFS_UIO_OFFSET(uio) uio_offset(uio)
#define AFS_UIO_RESID(uio) (int)uio_resid(uio)
#define AFS_UIO_SETOFFSET(uio, off) uio_setoffset(uio, off)
#define AFS_UIO_SETRESID(uio, val) uio_setresid(uio, val)
#else
#define AFS_UIO_OFFSET(uio) (uio)->uio_offset
#define AFS_UIO_RESID(uio) (uio)->uio_resid
#define AFS_UIO_SETOFFSET(uio, off) (uio)->uio_offset = off
#define AFS_UIO_SETRESID(uio, val) (uio)->uio_resid = val
#endif


/*
 * encapsulation of kernel data structure accesses
 */
#ifndef UKERNEL
#define setuerror(erval)	u.u_error = (erval)
#define getuerror()		u.u_error
#endif

/* Macros for vcache/vnode and vfs arguments to vnode and vfs ops.
 * These are required for IRIX 6.4 and later, which pass behavior pointers.
 * Note that the _CONVERT routines get the ";" here so that argument lists
 * can have arguments after the OSI_x_CONVERT macro is called.
 */
#define OSI_VN_ARG(V) V
#define OSI_VN_DECL(V) struct vnode *V
#define OSI_VN_CONVERT(V)
#define OSI_VC_ARG(V) V
#define OSI_VC_DECL(V) struct vcache *V
#define OSI_VC_CONVERT(V)
#define OSI_VFS_ARG(V) V
#define OSI_VFS_DECL(V) struct vfs *V
#define OSI_VFS_CONVERT(V)


/*
** Macro for Solaris 2.6 returns 1 if file is larger than 2GB; else returns 0
*/
#define AfsLargeFileUio(uio)       0
#define AfsLargeFileSize(pos, off) 0

/* Now include system specific OSI header file. It will redefine macros
 * defined here as required by the OS.
 */
#include "osi_machdep.h"

/* Declare any structures which use these macros after the OSI implementation
 * has had the opportunity to redefine them.
 */
extern afs_ucred_t afs_osi_cred, *afs_osi_credp;

#ifndef osi_curcred
#define osi_curcred() (u.u_cred)
#endif

#endif /* _AFS_OSI_ */
