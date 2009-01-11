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
 * AIX 4.x implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#ifdef AFS_AIX41_ENV


#undef	LOCK_INIT
#include <sys/lockl.h>
#include <sys/lock_def.h>
#include <sys/lock_alloc.h>
#include <sys/sleep.h>
#define	RX_ENABLE_LOCKS		1
#define AFS_GLOBAL_RXLOCK_KERNEL
/*
 * `condition variables' -- well, not really.  these just map to the
 * AIX event-list routines.  Thus, if one signals a condition prior to
 * a process waiting for it, the signal gets lost.
 * Note:
 *	`t' in `cv_timedwait' has different interpretation
 */
#ifndef	CV_DEFAULT
#define	CV_DEFAULT	0
#endif
#ifndef	MUTEX_DEFAULT
#define	MUTEX_DEFAULT	0
#endif
#define CV_INIT(cv, a,b,c)	(*(cv) = EVENT_NULL)
#define CV_DESTROY(cv)
#define CV_SIGNAL(_cv)		e_wakeup_one(_cv)
#define CV_BROADCAST(_cv)	e_wakeup(_cv)
typedef simple_lock_data afs_kmutex_t;
typedef tid_t afs_kcondvar_t;


#define	LOCK_INIT(a, b)		lock_alloc((void*)(a), LOCK_ALLOC_PIN, 1, 1), \
				simple_lock_init((void *)(a))
#define MUTEX_INIT(a,b,c,d)	lock_alloc((void*)(a), LOCK_ALLOC_PIN, 1, 1), \
				simple_lock_init((void *)(a))
#define MUTEX_DESTROY(a)	lock_free((void*)(a))

#ifdef RX_LOCKS_DB

#define MUTEX_ENTER(a)		simple_lock((void *)(a)), \
				rxdb_grablock((void *)(a), thread_self(),rxdb_fileID,\
					      __LINE__)

#define MUTEX_TRYENTER(a)	(simple_lock_try((void *)(a)) ?\
				rxdb_grablock((void *)(a), thread_self(), rxdb_fileID,\
					      __LINE__), 1 : 0)

#define MUTEX_EXIT(a)  		rxdb_droplock((void *)(a), thread_self(), rxdb_fileID,\
					      __LINE__), \
				simple_unlock((void *)(a))

#define CV_WAIT(_cv, _lck) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	rxdb_droplock((void *)(_lck), thread_self(), rxdb_fileID, __LINE__); \
	e_sleep_thread((_cv), (_lck), LOCK_SIMPLE); \
	rxdb_grablock((void *)(_lck), thread_self(), rxdb_fileID, __LINE__); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
    } while(0)

#define CV_TIMEDWAIT(_cv, _lck, _t) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	rxdb_droplock((void *)(_lck), thread_self(), rxdb_fileID, __LINE__); \
	e_mpsleep((_cv), (_t), (_lck), LOCK_SIMPLE); \
	rxdb_grablock((void *)(_lck), thread_self(), rxdb_fileID, __LINE__); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
    } while(0)

#else /* RX_LOCK_DB */

#define MUTEX_ENTER(a) 		simple_lock((void *)(a))
#define MUTEX_TRYENTER(a)	simple_lock_try((void *)(a))
#define MUTEX_EXIT(a) 		simple_unlock((void *)(a))

#define CV_WAIT(_cv, _lck) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	e_sleep_thread((_cv), (_lck), LOCK_SIMPLE); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
    } while(0)

#define CV_TIMEDWAIT(_cv, _lck, _t) \
    do { \
	int haveGlock = ISAFS_GLOCK(); \
	if (haveGlock) \
	    AFS_GUNLOCK(); \
	e_mpsleep((_cv), (_t), (_lck), LOCK_SIMPLE); \
	if (haveGlock) { \
	    MUTEX_EXIT(_lck); \
	    AFS_GLOCK(); \
	    MUTEX_ENTER(_lck); \
	} \
    } while(0)

#endif /* RX_LOCK_DB */

#define	MUTEX_DEFAULT	0

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a)	(lock_mine((void *)(a)))

#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);

#endif /* AFS_AIX41_ENV */

#endif /* _RX_KMUTEX_H_ */
