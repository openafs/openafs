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
 * DUX OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include <sys/lock.h>
/* #include <kern/sched_prim.h> */
/* #include <sys/unix_defs.h> */

#define getpid()		curproc

/* 
 * Time related macros
 */
extern struct timeval time;
#define osi_Time() (time.tv_sec)
#define	afs_hz	    hz

#define PAGESIZE 8192

#define	AFS_UCRED	ucred
#define	AFS_PROC	struct proc

#define afs_bufferpages bufpages

#define osi_vnhold(avc,r)  do { \
       if ((avc)->vrefCount) { VN_HOLD((struct vnode *)(avc)); } \
       else osi_Panic("refcnt==0");  } while(0)

#define	gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid), curproc)

#undef afs_suser

#ifdef KERNEL
extern struct simplelock afs_global_lock;
#if 0
extern thread_t afs_global_owner;
#define AFS_GLOCK() \
    do { \
	usimple_lock(&afs_global_lock); \
 	osi_Assert(afs_global_owner == (thread_t)0); \
   	afs_global_owner = current_thread(); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
 	osi_Assert(afs_global_owner == current_thread()); \
        afs_global_owner = (thread_t)0; \
	usimple_unlock(&afs_global_lock); \
    } while(0)
#define ISAFS_GLOCK() (afs_global_owner == current_thread())
#else
#define AFS_GLOCK() \
    do { \
	simple_lock(&afs_global_lock); \
    } while (0)
#define AFS_GUNLOCK() \
    do { \
	simple_unlock(&afs_global_lock); \
    } while(0)
#endif /* 0 */

#undef SPLVAR
#define SPLVAR
#undef NETPRI
#define NETPRI
#undef USERPRI
#define USERPRI
#endif /* KERNEL */

#endif /* _OSI_MACHDEP_H_ */
