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
 * Based to the degree possible on FreeBSD implementation (which is by
 * Garrett Wollman (?) and Jim Rees).  I couldn't rework it as I did for
 * FreeBSD, because NetBSD doesn't have anything like FreeBSD's  new
 * locking primitives.  So anyway, these are potentially heavier locks than
 * the *ahem* locking Jim had in the OpenBSD port, although it looks as
 * if struct lock is evolving into an adaptive mutex implementation (see
 * LOCK(9)), which should be reasonable for the code we have today.  The
 * available optimization would be to replace such a lock with a simple_lock
 * any place we only consider the current CPU, and could not sleep
 * (Matt).
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#include <sys/lock.h>

/* You can't have AFS_GLOBAL_SUNLOCK and not RX_ENABLE_LOCKS */
#define RX_ENABLE_LOCKS 1
#define AFS_GLOBAL_RXLOCK_KERNEL

/*
 * Condition variables
 *
 * In Digital Unix (OSF/1), we use something akin to the ancient sleep/wakeup
 * mechanism.  The condition variable itself plays no role; we just use its
 * address as a convenient unique number.  NetBSD has some improvements in
 * its versions of these mechanisms.
 */
#define CV_INIT(cv, a, b, c)
#define CV_DESTROY(cv)
#define CV_WAIT(cv, lck)    { \
	struct simplelock slock = SIMPLELOCK_INITIALIZER;		\
	simple_lock(&slock);						\
	int glocked = ISAFS_GLOCK();					\
	if (glocked)							\
	    AFS_GUNLOCK();						\
	MUTEX_EXIT(lck);						\
	ltsleep(cv, PSOCK, "afs_rx_cv_wait", 0, &slock);		\
	if (glocked)							\
	    AFS_GLOCK();						\
	MUTEX_ENTER(lck);						\
	simple_unlock(&slock);						\
    }

#define CV_TIMEDWAIT(cv, lck, t)  {					\
	struct simplelock slock = SIMPLELOCK_INITIALIZER;		\
	simple_lock(&slock);						\
	int glocked = ISAFS_GLOCK();					\
	if (glocked)							\
	    AFS_GUNLOCK();						\
	MUTEX_EXIT(lck);						\
	tsleep(cv, PSOCK, "afs_rx_cv_timedwait", t, &slock);		\
	if (glocked)							\
	    AFS_GLOCK();						\
	MUTEX_ENTER(lck);						\
	simple_unlock(&slock);						\
    }

#define CV_SIGNAL(cv)           wakeup_one(cv)
#define CV_BROADCAST(cv)        wakeup(cv)

/* #define osi_rxWakeup(cv)        wakeup(cv) */
typedef int afs_kcondvar_t;

typedef struct {
    struct lock lock;
    struct lwp *owner;
} afs_kmutex_t;

#define MUTEX_INIT(a,b,c,d) \
    do { \
	lockinit(&(a)->lock, PSOCK, "afs rx mutex", 0, 0); \
	(a)->owner = 0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (struct lwp *)-1; \
    } while(0);

#if defined(LOCKDEBUG)
#define MUTEX_ENTER(a) \
    do { \
	_lockmgr(&(a)->lock, LK_EXCLUSIVE, 0, __FILE__, __LINE__); \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curlwp; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( _lockmgr(&(a)->lock, LK_EXCLUSIVE | LK_NOWAIT, 0, __FILE__, __LINE__) ? 0	\
      : ((a)->owner = curlwp, 1) )
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curlwp); \
	(a)->owner = 0; \
	_lockmgr(&(a)->lock, LK_RELEASE, 0, __FILE__, __LINE__); \
    } while(0);
#else
#define MUTEX_ENTER(a) \
    do { \
	lockmgr(&(a)->lock, LK_EXCLUSIVE, 0); \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curlwp; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( lockmgr(&(a)->lock, LK_EXCLUSIVE | LK_NOWAIT, 0) ? 0 \
      : ((a)->owner = curlwp, 1) )
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curlwp); \
	(a)->owner = 0; \
	lockmgr(&(a)->lock, LK_RELEASE, 0); \
    } while(0);
#endif /* LOCKDEBUG */
#define MUTEX_ISMINE(a) \
    (lockstatus(a) == LK_EXCLUSIVE)
#define MUTEX_LOCKED(a) \
    (lockstatus(a) == LK_EXCLUSIVE)

#endif /* _RX_KMUTEX_H_ */
