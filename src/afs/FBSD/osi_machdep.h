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
/* #include <kern/sched_prim.h> */
/* #include <sys/unix_defs.h> */

#define getpid()		curproc
extern struct simplelock afs_rxglobal_lock;

/* 
 * Time related macros
 */
#define osi_Time() time_second
#define	afs_hz	    hz

#define PAGESIZE 8192

#define	AFS_UCRED	ucred
#define	AFS_PROC	struct proc

#define afs_bufferpages bufpages

#define osi_vnhold(avc,r) do { VN_HOLD((struct vnode *)(avc)); } while (0)

#define	gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid), curproc)
#undef gop_lookupname
#define gop_lookupname osi_lookupname

#undef afs_suser

#ifdef KERNEL
extern struct lock afs_global_lock;
#if defined(AFS_FBSD50_ENV)
#define VT_AFS		"afs"
#define VROOT		VV_ROOT
#define v_flag		v_vflag
extern struct thread * afs_global_owner;
#define AFS_GLOCK() \
    do { \
        osi_Assert(curthread); \
 	lockmgr(&afs_global_lock, LK_EXCLUSIVE, 0, curthread); \
        osi_Assert(afs_global_owner == 0); \
   	afs_global_owner = curthread; \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
        osi_Assert(curthread); \
 	osi_Assert(afs_global_owner == curthread); \
        afs_global_owner = 0; \
        lockmgr(&afs_global_lock, LK_RELEASE, 0, curthread); \
    } while(0)
#define ISAFS_GLOCK() (afs_global_owner == curthread && curthread)
#else /* FBSD50 */
extern struct proc * afs_global_owner;
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
#define AFS_RXGLOCK()
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK() 1

#undef SPLVAR
#define SPLVAR int splvar
#undef NETPRI
#define NETPRI splvar=splnet()
#undef USERPRI
#define USERPRI splx(splvar)
#endif /* KERNEL */

#endif /* _OSI_MACHDEP_H_ */
