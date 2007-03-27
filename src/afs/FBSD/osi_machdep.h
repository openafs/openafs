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
#if defined(AFS_FBSD50_ENV)
#include <sys/mutex.h>
#endif

/* 
 * Time related macros
 */
#define osi_Time()	time_second
#define	afs_hz		hz

#define	AFS_UCRED	ucred
#define	AFS_PROC	struct proc

#define afs_bufferpages bufpages
#ifndef iodone
#define iodone biodone
#endif

#define osi_vnhold(avc,r)	vref(AFSTOV(avc))
#undef vSetVfsp
#define vSetVfsp(vc, vfsp)	AFSTOV(vc)->v_mount = (vfsp)
#undef vSetType
#define vSetType(vc, type)	AFSTOV(vc)->v_type = (type)
#undef vType
#define	vType(vc)		AFSTOV(vc)->v_type

#undef gop_lookupname
#define gop_lookupname osi_lookupname

#undef afs_suser

#define afs_strcat(s1, s2)	strcat((s1), (s2))

#ifdef KERNEL

#undef afs_osi_Alloc_NoSleep
#define afs_osi_Alloc_NoSleep(size) osi_fbsd_alloc((size), 0)

#define VN_RELE(vp)		vrele(vp)
#define VN_HOLD(vp)		VREF(vp)

#ifdef AFS_FBSD60_ENV
#undef IsAfsVnode
#define IsAfsVnode(v) ((v)->v_op == &afs_vnodeops)
extern struct vop_vector afs_vnodeops;
#endif

#undef osi_getpid
#if defined(AFS_FBSD50_ENV)
#define VT_AFS		"afs"
#define VROOT		VV_ROOT
#define v_flag		v_vflag
#define osi_curcred()	(curthread->td_ucred)
#define afs_suser(x)	(!suser(curthread))
#define osi_getpid()	(curthread->td_proc->p_pid)
#define simple_lock(x)	mtx_lock(x)
#define simple_unlock(x) mtx_unlock(x)
#define        gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(cred),(aresid), curthread)
extern struct mtx afs_global_mtx;
#define AFS_GLOCK() mtx_lock(&afs_global_mtx)
#define AFS_GUNLOCK() mtx_unlock(&afs_global_mtx)
#define ISAFS_GLOCK() (mtx_owned(&afs_global_mtx))

#else /* FBSD50 */
extern struct lock afs_global_lock;

#define osi_curcred()	(curproc->p_cred->pc_ucred)
#define afs_suser(x)	(!suser(curproc))
#define osi_getpid()	(curproc->p_pid)
#define        gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid), curproc)
extern struct proc *afs_global_owner;
#define AFS_GLOCK() \
    do { \
        osi_Assert(curproc); \
 	lockmgr(&afs_global_lock, LK_EXCLUSIVE, 0, curproc); \
        osi_Assert(afs_global_owner == 0); \
   	afs_global_owner = curproc; \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
        osi_Assert(curproc); \
 	osi_Assert(afs_global_owner == curproc); \
        afs_global_owner = 0; \
        lockmgr(&afs_global_lock, LK_RELEASE, 0, curproc); \
    } while(0)
#define ISAFS_GLOCK() (afs_global_owner == curproc && curproc)
#endif /* FBSD50 */

#undef SPLVAR
#define SPLVAR int splvar
#undef NETPRI
#define NETPRI splvar=splnet()
#undef USERPRI
#define USERPRI splx(splvar)
#endif /* KERNEL */

#define ifnet_flags(x) (x?(x)->if_flags:0)

#endif /* _OSI_MACHDEP_H_ */
