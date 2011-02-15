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
 * FBSD OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include <sys/lock.h>
#include <sys/time.h>
#include <sys/mutex.h>
#include <sys/vnode.h>
#if defined(AFS_FBSD80_ENV)
#include <sys/priv.h>
#endif

/*
 * Time related macros
 */
#define osi_Time()	time_second
#define	afs_hz		hz

typedef struct ucred afs_ucred_t;
typedef struct proc afs_proc_t;

#define afs_bufferpages bufpages
#ifndef iodone
#define iodone biodone
#endif

#ifdef AFS_FBSD80_ENV
#define VSUID           S_ISUID
#define VSGID           S_ISGID
#endif

#define osi_vnhold(avc,r)	vref(AFSTOV(avc))

#define vType(vc)               AFSTOV(vc)->v_type
#define vSetVfsp(vc, vfsp) 	AFSTOV(vc)->v_mount = (vfsp)
#define vSetType(vc, type)      AFSTOV(vc)->v_type = (type)
#if defined(AFS_FBSD60_ENV) && defined(KERNEL)
extern struct vop_vector afs_vnodeops;
# define IsAfsVnode(v) ((v)->v_op == &afs_vnodeops)
#else
extern int (**afs_vnodeop_p) ();
# define IsAfsVnode(v)           ((v)->v_op == afs_vnodeop_p)
#endif
#define SetAfsVnode(v)          /* nothing; done in getnewvnode() */

#if defined(AFS_FBSD80_ENV)
#define osi_vinvalbuf(vp, flags, slpflag, slptimeo) \
  vinvalbuf((vp), (flags), (slpflag), (slptimeo))
#else
#define osi_vinvalbuf(vp, flags, slpflag, slptimeo) \
  vinvalbuf((vp), (flags), (curthread), (slpflag), (slptimeo))
#define osi_curproc() (curthread)
#endif

#undef gop_lookupname
#define gop_lookupname osi_lookupname

#undef gop_lookupname_user
#define gop_lookupname_user osi_lookupname

#define afs_strcat(s1, s2)	strcat((s1), (s2))

/* malloc */
extern void *osi_fbsd_alloc(size_t size, int dropglobal);
extern void osi_fbsd_free(void *p);

#undef afs_osi_Alloc_NoSleep
#define afs_osi_Alloc_NoSleep(size) osi_fbsd_alloc((size), 0)

#ifdef AFS_FBSD80_ENV
#define VN_RELE(vp)				\
  do {						\
    vrele(vp);					\
  } while(0);
#else
#define VN_RELE(vp)             vrele(vp)
#endif
#define VN_HOLD(vp)		VREF(vp)

#undef afs_suser
#if defined(AFS_FBSD80_ENV)
/* OpenAFS-specific privileges negotiated for FreeBSD, thanks due to
 * Ben Kaduk */
#define osi_suser_client_settings(x)   (!priv_check(curthread, PRIV_AFS_ADMIN))
#define osi_suser_afs_daemon(x)   (!priv_check(curthread, PRIV_AFS_DAEMON))
#define afs_suser(x) (osi_suser_client_settings((x)) && osi_suser_afs_daemon((x)))
#else
#define afs_suser(x)	(!suser(curthread))
#endif

#undef osi_getpid
#define VT_AFS		"afs"
#define VROOT		VV_ROOT
#define v_flag		v_vflag
#define osi_curcred()	(curthread->td_ucred)
#define osi_curproc()   (curthread)
#define osi_getpid()	(curthread->td_proc->p_pid)
#define simple_lock(x)	mtx_lock(x)
#define simple_unlock(x) mtx_unlock(x)
#define        gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(cred),(aresid), curthread)
extern struct mtx afs_global_mtx;
extern struct thread *afs_global_owner;
#define AFS_GLOCK() \
    do { \
	mtx_assert(&afs_global_mtx, (MA_NOTOWNED)); \
	mtx_lock(&afs_global_mtx); \
	mtx_assert(&afs_global_mtx, (MA_OWNED|MA_NOTRECURSED)); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	mtx_assert(&afs_global_mtx, (MA_OWNED|MA_NOTRECURSED)); \
	mtx_unlock(&afs_global_mtx); \
    } while (0)
#define ISAFS_GLOCK() (mtx_owned(&afs_global_mtx))
# if defined(AFS_FBSD80_ENV) && defined(WITNESS)
#  define osi_InitGlock() \
	do { \
	    memset(&afs_global_mtx, 0, sizeof(struct mtx)); \
	    mtx_init(&afs_global_mtx, "AFS global lock", NULL, MTX_DEF); \
	    afs_global_owner = 0; \
	} while(0)
# else
#  define osi_InitGlock() \
    do { \
	mtx_init(&afs_global_mtx, "AFS global lock", NULL, MTX_DEF); \
	afs_global_owner = 0; \
    } while (0)
# endif

#undef SPLVAR
#define SPLVAR
#undef NETPRI
#define NETPRI
#undef USERPRI
#define USERPRI

#define osi_procname(procname, size) strncpy(procname, curproc->p_comm, size)

#endif /* _OSI_MACHDEP_H_ */
