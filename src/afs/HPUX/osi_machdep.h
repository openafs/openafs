/* Copyright (C) 1995 Transarc Corporation - All rights reserved. */
/*
 *
 * HPUX OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */

#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include "../h/kern_sem.h"

#define	afs_hz	    hz
extern struct timeval time;
#define osi_Time() (time.tv_sec)

#ifdef PAGESIZE
#undef PAGESIZE
#endif
#define PAGESIZE 8192

#define	AFS_UCRED	ucred
#define	AFS_PROC	proc_t

#define osi_vnhold(avc, r)  do { VN_HOLD((struct vnode *)(avc)); } while(0)
#define gop_rdwr(rw,gp,base,len,offset,segflg,unit,aresid) \
  vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(unit),(aresid),0)

#undef	afs_suser

#define getpid()                (afs_uint32)p_pid(u.u_procp)

#define getppid()               (afs_uint32)p_ppid(u.u_procp)

#ifdef KERNEL
/*
 * Global lock support. 
 *
 * HP uses global mutex to protect afs land
 */

extern sema_t afs_global_sema;

extern void osi_InitGlock(void);

extern void       afsHash(int nbuckets);
extern sv_sema_t *afsHashInsertFind(tid_t key);
extern sv_sema_t *afsHashFind(tid_t key);
extern void       afsHashRelease(tid_t key);

#define AFS_GLOCK_PID   kt_tid(u.u_kthreadp)
#define AFS_SAVE_SEMA   afsHashInsertFind(AFS_GLOCK_PID)
#define AFS_FIND_SEMA   afsHashFind(AFS_GLOCK_PID)
#define AFS_GLOCK()     MP_PXSEMA(&afs_global_sema, AFS_SAVE_SEMA)
#define AFS_GUNLOCK()   (AFS_ASSERT_GLOCK(), MP_VXSEMA(&afs_global_sema,AFS_FIND_SEMA), (!uniprocessor ? (afsHashRelease(AFS_GLOCK_PID),0) : 0))
#define ISAFS_GLOCK()   (!uniprocessor ? owns_sema(&afs_global_sema):1)

#define AFS_RXGLOCK() 
#define AFS_RXGUNLOCK()
#define ISAFS_RXGLOCK() 1

/* Uses splnet only in the SP case */
#define SPLVAR      register ulong_t splvar
#define NETPRI      NET_SPLNET(splvar)
#define USERPRI     NET_SPLX(splvar)
#endif /* KERNEL */

/* 
 * On HP, the global lock is an alpha semaphore, hence it is automatically
 * released and reacquired aroubd a sleep() and wakeup().
 */

#define	afs_osi_Sleep(x)	sleep((caddr_t) x,PZERO-2)
#define	afs_osi_Wakeup(x)	wakeup((caddr_t) x)
#define	osi_NullHandle(x)	((x)->proc == (caddr_t) 0)

extern caddr_t kmem_alloc();
#include <sys/kthread_iface.h>	/* for kt_cred() */

#endif /* _OSI_MACHDEP_H_ */







