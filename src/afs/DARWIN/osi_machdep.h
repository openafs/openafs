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
 * MACOS OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#ifdef XAFS_DARWIN_ENV
#ifndef _MACH_ETAP_H_
#define _MACH_ETAP_H_
typedef unsigned short etap_event_t;
#endif
#endif

#include <sys/lock.h>
#include <kern/thread.h>
#include <sys/user.h>

#define getpid()                current_proc()->p_pid
#define getppid()               current_proc()->p_pptr->p_pid
#undef gop_lookupname
#define gop_lookupname osi_lookupname

#define FTRUNC 0

/* vcexcl - used only by afs_create */
enum vcexcl { EXCL, NONEXCL };

/* 
 * Time related macros
 */
#ifndef AFS_DARWIN60_ENV
extern struct timeval time;
#endif
#define osi_Time() (time.tv_sec)
#define afs_hz      hz

#define PAGESIZE 8192

#define AFS_UCRED       ucred

#define AFS_PROC        struct proc

#define osi_vnhold(avc,r)       VN_HOLD(AFSTOV(avc))
#define VN_HOLD(vp) darwin_vn_hold(vp)
#define VN_RELE(vp) vrele(vp);


#define gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid),current_proc())

#undef afs_suser

#ifdef KERNEL
extern thread_t afs_global_owner;
/* simple locks cannot be used since sleep can happen at any time */
/* Should probably use mach locks rather than bsd locks, since we use the
   mach thread control api's elsewhere (mach locks not used for consistency
   with rx, since rx needs lock_write_try() in order to use mach locks
 */
extern struct lock__bsd__ afs_global_lock;
#define AFS_GLOCK() \
    do { \
        lockmgr(&afs_global_lock, LK_EXCLUSIVE, 0, current_proc()); \
	osi_Assert(afs_global_owner == 0); \
	afs_global_owner = current_thread(); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	osi_Assert(afs_global_owner == current_thread()); \
	afs_global_owner = 0; \
        lockmgr(&afs_global_lock, LK_RELEASE, 0, current_proc()); \
    } while(0)
#define ISAFS_GLOCK() (afs_global_owner == current_thread())
#define AFS_RXGLOCK()
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK() 1

#define SPLVAR
#define NETPRI
#define USERPRI
#if 0
#undef SPLVAR
#define SPLVAR int x;
#undef NETPRI
#define NETPRI x=splnet();
#undef USERPRI
#define USERPRI splx(x);
#endif

#define AFS_APPL_UFS_CACHE 1
#define AFS_APPL_HFS_CACHE 2

extern ino_t VnodeToIno(vnode_t * vp);
extern dev_t VnodeToDev(vnode_t * vp);

#define osi_curproc() current_proc()

/* FIXME */
#define osi_curcred() &afs_osi_cred 

#endif /* KERNEL */

#endif /* _OSI_MACHDEP_H_ */
