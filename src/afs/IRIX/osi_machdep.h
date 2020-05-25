/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * IRIX OSI header file. Extends afs_osi.h.
 *
 * afs_osi.h includes this file, which is the only way this file should
 * be included in a source file. This file can redefine macros declared in
 * afs_osi.h.
 */
#ifndef _OSI_MACHDEP_H_
#define _OSI_MACHDEP_H_

#include <sys/sema.h>
#include <sys/pda.h>
extern kmutex_t afs_global_lock;


#undef osi_Time
extern time_t time;
#define osi_Time() (time)

/* This gets redefined from ucred to cred in osi_vfs.h, just do it right */
typedef struct cred afs_ucred_t;
typedef struct proc afs_proc_t;

#undef gop_lookupname
#define gop_lookupname(fnamep,segflg,followlink,compvpp) lookupname((fnamep),(segflg),(followlink),NULL,(compvpp), NULL)

#undef gop_lookupname_user
#define gop_lookupname_user(fnamep,segflg,followlink,compvpp) lookupname((fnamep),(segflg),(followlink),NULL,(compvpp), NULL)

#include <sys/flock.h>
extern flid_t osi_flid;
#define v_op v_bh.bh_first->bd_ops
#define v_data v_bh.bh_first->bd_pdata
#define vfs_data vfs_bh.bh_first->bd_pdata

/*
 * Global lock, semaphore, mutex and state vector support.
 */
#define SV_INIT(cv, nm, t, c)	cv_init(cv, nm, t, c)
/* Spinlock macros */
#define SV_TYPE sv_t
#define SV_SIGNAL(cv)           sv_signal(cv)
#define SPINLOCK_INIT(l, nm)    spinlock_init((l),(nm))
#define SPLOCK(l)	        mp_mutex_spinlock(&(l))
#define SPUNLOCK(l,s)           mp_mutex_spinunlock(&(l),s)
#define SP_WAIT(l, s, cv, p)    mp_sv_wait_sig(cv, p, (void*)(&(l)), s)
/* Add PLTWAIT for afsd's to wait so we don't rack up the load average. */
#define AFSD_PRI() ((kt_basepri(curthreadp) == PTIME_SHARE) ? PZERO : (PZERO|PLTWAIT))
#undef AFS_MUTEX_ENTER
#define AFS_MUTEX_ENTER(mp) \
  MACRO_BEGIN \
    struct kthread *_kthreadP; \
    while(mutex_tryenter(mp) == 0) { \
      _kthreadP = (struct kthread*)mutex_owner(mp); \
      if (_kthreadP != NULL && _kthreadP->k_sonproc == CPU_NONE) { \
          mutex_lock(mp, AFSD_PRI()); \
          break; \
      } \
    } \
  MACRO_END


#define cv_timedwait(cv, l, t)  {				\
				sv_timedwait(cv, AFSD_PRI(), l, 0, 0, &(t), \
					     (struct timespec*)0);	\
				AFS_MUTEX_ENTER(l); \
				}
#ifdef cv_wait
#undef cv_wait
#endif
#define cv_wait(cv, l) {					\
			sv_wait(cv, AFSD_PRI(),  l, 0);	\
			AFS_MUTEX_ENTER(l); \
		       }

#if defined(KERNEL)
#if defined(MP)
#define _MP_NETLOCKS		/* to get sblock to work right */

/* On SGI mutex_owned doesn't work, so simulate this by remembering the owning
 * thread explicitly.  This is only used for debugging so could be disabled for
 * production builds.
 *
 * CAUTION -- The ISAFS_(RX)?GLOCK macros are not safe to use when the lock is
 *     not held if the test may be made at interrupt level as the code may
 *     appear to be running as the process that is (or last was) running at
 *     non-interrupt level. Worse yet, the interrupt may occur just as the
 *     process is exiting, in which case, the pid may change from the start
 *     of the interrupt to the end, since the u area has been changed. So,
 *     at interrupt level, I'm using the base of the current interrupt stack.
 *     Note that afs_osinet.c also modifies afs_global_owner for osi_Sleep and
 *     afs_osi_Wakeup. Changes made here should be reflected there as well.
 * NOTE - As of 6.2, we can no longer use mutexes in interrupts, so the above
 *     concern no longer exists.
 */

/* Irix does not check for deadlocks unless it's a debug kernel. */
#define AFS_ASSERT_GNOTME() \
    (!ISAFS_GLOCK() || (panic("afs global lock held be me"), 0))
#define AFS_GLOCK() \
	{ AFS_ASSERT_GNOTME(); AFS_MUTEX_ENTER(&afs_global_lock); }
#define AFS_GUNLOCK()  { AFS_ASSERT_GLOCK(); mutex_exit(&afs_global_lock); }
#define ISAFS_GLOCK() mutex_mine(&afs_global_lock)
#else /* MP */
#define AFS_GLOCK()
#define AFS_GUNLOCK()
#define ISAFS_GLOCK() 1

#define SPLVAR      int splvar
#define NETPRI      splvar=splnet()
#define USERPRI     splx(splvar)


#endif /* MP */

#endif /* KERNEL  */

#define osi_InitGlock() \
	mutex_init(&afs_global_lock, MUTEX_DEFAULT, "afs_global_lock");

#define gop_rdwr(rw,gp,base,len,offset,segflg,ioflag,ulimit,cr,aresid) \
   vn_rdwr((rw),(gp),(base),(len),(offset),(segflg),(ioflag),(ulimit),(cr),\
	   (int *)(aresid), &osi_flid)

#undef suser
#define suser()		cap_able(CAP_DEVICE_MGT)
#define afs_suser(x)	suser()

#define afs_hz HZ

#undef setuerror
#undef getuerror


/* OS independent user structure stuff */
/*
 * OSI_GET_CURRENT_PID
 */
#define OSI_GET_CURRENT_PID() proc_pid(curproc())

#define getpid()  OSI_GET_CURRENT_PID()

/*
 * OSI_GET_CURRENT_PROCP
 */
#define OSI_GET_CURRENT_PROCP() UT_TO_PROC(curuthread)


/*
 * OSI_GET_LOCKID
 *
 * Prior to IRIX 6.4, pid sufficed, now we need kthread.
 */
/* IRIX returns k_id, but this way, we've got the thread address for debugging. */
#define OSI_GET_LOCKID() \
	(private.p_curkthread ? (uint64_t)private.p_curkthread : (uint64_t)0)
#define OSI_NO_LOCKID ((uint64_t)-1)

/*
 * OSI_GET_CURRENT_CRED
 */
#define OSI_GET_CURRENT_CRED() get_current_cred()

#define osi_curcred()		OSI_GET_CURRENT_CRED()

/*
 * OSI_SET_CURRENT_CRED
 */
#define OSI_SET_CURRENT_CRED(C) set_current_cred((C))

/*
 * OSI_GET_CURRENT_ABI
 */
#define OSI_GET_CURRENT_ABI() get_current_abi()

/*
 * OSI_GET_CURRENT_SYSID
 */
#define OSI_GET_CURRENT_SYSID() (curprocp->p_flid.fl_sysid)

/*
 * OSI_GET_CURRENT_COMM
 */
#define OSI_GET_CURRENT_COMM() (curprocp->p_comm)

/*
 * OSI_GET_CURRENT_CDIR
 */
#define OSI_GET_CURRENT_CDIR() (curuthread->ut_cdir)


/*
 * OSI_GET_CURRENT_RDIR
 */
#define OSI_GET_CURRENT_RDIR() (curuthread->ut_rdir)



/* Macros for vcache/vnode and vfs arguments to vnode and vfs ops.
 *
 * Note that the _CONVERT routines get the ";" here so that argument lists
 * can have arguments after the OSI_x_CONVERT macro is called.
 */
#undef OSI_VN_ARG
#define OSI_VN_ARG(V) bhv_##V
#undef OSI_VN_DECL
#define OSI_VN_DECL(V)  bhv_desc_t *bhv_##V
#undef OSI_VN_CONVERT
#define OSI_VN_CONVERT(V) struct vnode * V = (struct vnode*)BHV_TO_VNODE(bhv_##V)
#undef OSI_VC_ARG
#define OSI_VC_ARG(V) bhv_##V
#undef OSI_VC_DECL
#define OSI_VC_DECL(V)  bhv_desc_t *bhv_##V
#undef OSI_VC_CONVERT
#define OSI_VC_CONVERT(V) struct vcache * V = VTOAFS(BHV_TO_VNODE(bhv_##V))
#undef OSI_VFS_ARG
#define OSI_VFS_ARG(V) bhv_##V
#undef OSI_VFS_DECL
#define OSI_VFS_DECL(V)  bhv_desc_t *bhv_##V
#undef OSI_VFS_CONVERT
#define OSI_VFS_CONVERT(V) struct vfs * V = (struct vfs*)bhvtovfs(bhv_##V)

#define osi_procname(procname, size) strncpy(procname, proc_name(curproc()), size)

static_inline void
osi_GetTime(osi_timeval32_t *atv)
{
#ifdef _K64U64
    struct __irix5_timeval now;
    irix5_microtime(&now);
#else
    struct timeval now;
    microtime(&now);
#endif
    atv->tv_sec = now.tv_sec;
    atv->tv_usec = now.tv_usec;
}

#endif /* _OSI_MACHDEP_H_ */
