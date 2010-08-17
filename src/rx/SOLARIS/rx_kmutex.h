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
 * Solaris implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#if	defined(AFS_SUN5_ENV) && defined(KERNEL)

#define RX_ENABLE_LOCKS 1
#define AFS_GLOBAL_RXLOCK_KERNEL 1

#include <sys/tiuser.h>
#include <sys/t_lock.h>
#include <sys/mutex.h>

typedef kmutex_t afs_kmutex_t;
typedef kcondvar_t afs_kcondvar_t;

#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);

#define MUTEX_DESTROY(a)	mutex_destroy(a)
#define MUTEX_INIT(a,b,c,d)	mutex_init(a, b, c, d)
#define MUTEX_ISMINE(a)		mutex_owned((afs_kmutex_t *)(a))
#define CV_INIT(a,b,c,d)	cv_init(a, b, c, d)
#define CV_DESTROY(a)		cv_destroy(a)
#define CV_SIGNAL(a)		cv_signal(a)
#define CV_BROADCAST(a)		cv_broadcast(a)

#ifdef RX_LOCKS_DB

#define MUTEX_ENTER(a) \
    do { \
	mutex_enter(a); \
	rxdb_grablock((a), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
    } while(0)

#define MUTEX_TRYENTER(a) \
    (mutex_tryenter(a) ? (rxdb_grablock((a), osi_ThreadUnique(), \
     rxdb_fileID, __LINE__), 1) : 0)

#define MUTEX_EXIT(a) \
    do { \
	rxdb_droplock((a), osi_ThreadUnique(), rxdb_fileID, __LINE__); \
	mutex_exit(a); \
    } while(0)

#define CV_WAIT_SIG(cv, m)	afs_cv_wait(cv, m, 1, rxdb_fileID, __LINE__)
#define CV_WAIT(cv, m)		afs_cv_wait(cv, m, 0, rxdb_fileID, __LINE__)

#define CV_TIMEDWAIT(cv, m, t)	\
			afs_cv_timedwait(cv, lck, t, 0, rxdb_fileID, __LINE__)

#else /* RX_LOCKS_DB */

#define MUTEX_ENTER(a)		mutex_enter(a)
#define MUTEX_TRYENTER(a)	mutex_tryenter(a)
#define MUTEX_EXIT(a) 		mutex_exit(a)

#define CV_WAIT_SIG(cv, m)	afs_cv_wait(cv, m, 1)
#define CV_WAIT(cv, m)		afs_cv_wait(cv, m, 0)

#define CV_TIMEDWAIT(cv, m, t)	afs_cv_timedwait(cv, m, t, 0)

#endif /* RX_LOCKS_DB */

#endif /* SUN5 && KERNEL */

#endif /* _RX_KMUTEX_H_ */
