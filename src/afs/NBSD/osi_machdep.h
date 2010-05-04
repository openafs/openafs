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

typedef struct ucred afs_ucred_t;
typedef struct proc afs_proc_t;

#define afs_bufferpages bufpages

#undef gop_lookupname
#define gop_lookupname(fnamep,segflg,followlink,compvpp) lookupname((fnamep),(segflg),(followlink),NULL,(compvpp))
#undef gop_lookupname_user
#define gop_lookupname_user(fnamep,segflg,followlink,compvpp) lookupname((fnamep),(segflg),(followlink),NULL,(compvpp))

#define osi_vnhold(avc,r)  do { \
       if ((avc)->vrefCount) { VN_HOLD((struct vnode *)(avc)); } \
       else osi_Panic("refcnt==0");  } while(0)

#define	gop_rdwr(rw,gp,base,len,offset,segflg,unit,cred,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(cred),(aresid), curproc)

#undef afs_suser

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
#define osi_InitGlock() \
    do { \
	lockinit(&afs_global_lock, PLOCK, "afs global lock", 0, 0); \
	afs_global_owner = 0; \
    } while (0)
#endif /* 0 */

#undef SPLVAR
#define SPLVAR
#undef NETPRI
#define NETPRI
#undef USERPRI
#define USERPRI

/* Vnode related macros */

extern int (**afs_vnodeop_p) ();
#define vType(vc)               AFSTOV(vc)->v_type
#define vSetVfsp(vc, vfsp)      AFSTOV(vc)->v_mount = (vfsp)
#define vSetType(vc, type)      AFSTOV(vc)->v_type = (type)
#define IsAfsVnode(v)           ((v)->v_op == afs_vnodeop_p)
#define SetAfsVnode(v)          /* nothing; done in getnewvnode() */

#define osi_procname(procname, size) strncpy(procname, curproc->p_comm, size)

#endif /* _OSI_MACHDEP_H_ */
