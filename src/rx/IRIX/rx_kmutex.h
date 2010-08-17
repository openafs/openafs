/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * IRIX implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#ifdef AFS_SGI62_ENV

#ifdef MP
#define	RX_ENABLE_LOCKS	1
#define	AFS_GLOBAL_RXLOCK_KERNEL 1


#include "sys/sema.h"
#ifndef mutex_tryenter
#define mutex_tryenter(m) cpsema(m)
#endif /* mutex_tryenter */
typedef kmutex_t afs_kmutex_t;
typedef kcondvar_t afs_kcondvar_t;

#ifndef	CV_DEFAULT
#define	CV_DEFAULT	0
#endif
#ifndef	MUTEX_DEFAULT
#define	MUTEX_DEFAULT	0
#endif

#ifdef AFS_SGI62_ENV
#define MUTEX_INIT(m, nm, type , a)  mutex_init(m, type, nm)
#else
#define MUTEX_INIT(a,b,c,d)  mutex_init(a,b,c,d)
#endif
#define MUTEX_DESTROY(a) mutex_destroy(a)
#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a)	1
#define CV_INIT(cv, a,b,c)	cv_init(cv, a, b, c)
#define CV_SIGNAL(_cv)		cv_signal(_cv)
#define CV_BROADCAST(_cv)	cv_broadcast(_cv)
#define CV_DESTROY(_cv)		cv_destroy(_cv)
#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);
#ifdef AFS_SGI64_ENV
/* Add PLTWAIT for afsd's to wait so we don't rack up the load average. */
#ifdef AFS_SGI65_ENV
#define AFSD_PRI() ((kt_basepri(curthreadp) == PTIME_SHARE) ? PZERO : (PZERO|PLTWAIT))
#else
#define AFSD_PRI() ((curprocp && curprocp->p_rss==0) ? (PZERO|PLTWAIT) : PZERO)
#endif /* SGI65 */
#undef cv_wait
#define cv_wait(cv, mp)	{ \
	sv_wait(cv, AFSD_PRI(), mp, 0); \
	AFS_MUTEX_ENTER(mp); \
}
#endif /* AFS_SGI64_ENV */
#ifdef RX_LOCKS_DB
#define MUTEX_ENTER(a)		do { \
				     AFS_MUTEX_ENTER(a); \
				     rxdb_grablock((a), osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__); \
				 } while(0)
#define MUTEX_TRYENTER(a)	(mutex_tryenter(a) ? \
				     (rxdb_grablock((a), osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__), 1) \
						   : 0)
#define MUTEX_EXIT(a)  		do { \
				     rxdb_droplock((a), osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__); \
				     mutex_exit(a); \
				 } while(0)
#define CV_WAIT(_cv, _lck) 	do { \
				     int haveGlock = ISAFS_GLOCK(); \
				     if (haveGlock) \
					AFS_GUNLOCK(); \
				     rxdb_droplock((_lck), \
						   osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__); \
				     cv_wait(_cv, _lck); \
				     rxdb_grablock((_lck), \
						   osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__); \
				     if (haveGlock) { \
					MUTEX_EXIT(_lck); \
					AFS_GLOCK(); \
					MUTEX_ENTER(_lck); \
				     } \
				 } while (0)
#define CV_TIMEDWAIT(_cv,_lck,_t)	do { \
				     int haveGlock = ISAFS_GLOCK(); \
				     if (haveGlock) \
					AFS_GUNLOCK(); \
				     rxdb_droplock((_lck), \
						   osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__); \
				     cv_timedwait(_cv, _lck, t); \
				     rxdb_grablock((_lck), \
						   osi_ThreadUnique(), \
						   rxdb_fileID, __LINE__); \
				     if (haveGlock) { \
					MUTEX_EXIT(_lck); \
					AFS_GLOCK(); \
					MUTEX_ENTER(_lck); \
				     } \
				 } while (0)
#else /* RX_LOCKS_DB */
#define MUTEX_ENTER(a) AFS_MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) mutex_tryenter(a)
#define MUTEX_EXIT(a)  mutex_exit(a)
#define CV_WAIT(_cv, _lck) 	do { \
					int haveGlock = ISAFS_GLOCK(); \
					if (haveGlock) \
					    AFS_GUNLOCK(); \
					cv_wait(_cv, _lck); \
					if (haveGlock) { \
					    MUTEX_EXIT(_lck); \
					    AFS_GLOCK(); \
					    MUTEX_ENTER(_lck); \
					} \
				    } while (0)
#define CV_TIMEDWAIT(cv,lck,t)	do { \
					int haveGlock = ISAFS_GLOCK(); \
					if (haveGlock) \
					    AFS_GUNLOCK(); \
					cv_timedwait(_cv, _lck, t); \
					if (haveGlock) { \
					    MUTEX_EXIT(_lck); \
					    AFS_GLOCK(); \
					    MUTEX_ENTER(_lck); \
					} \
				    } while (0)
#endif /* RX_LOCKS_DB */


#else /* MP */
#define MUTEX_INIT(m, nm, type , a)
#define MUTEX_DESTROY(a)
#define MUTEX_ISMINE(a)	1
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)

#define osirx_AssertMine(addr, msg)

#define CV_INIT(cv, a,b,c)
#define CV_SIGNAL(_cv)
#define CV_BROADCAST(_cv)
#define CV_DESTROY(_cv)
#define CV_WAIT(_cv, _lck)
#define CV_TIMEDWAIT(cv,lck,t)

#endif /* MP */

#endif /* SGI62 */

#endif /* _RX_KMUTEX_H_ */
