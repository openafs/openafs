/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* rx_kmutex.h - mutex and condition variable macros for kernel environment.
 *
 * HPUX implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#if defined(AFS_HPUX110_ENV) && defined(KERNEL)
/* rx fine grain locking primitives */

#include <sys/ksleep.h>
#include <sys/spinlock.h>
#include <sys/sem_beta.h>
#include <sys/errno.h>
#include <net/netmp.h>

#include "../rx/rx_kernel.h"	/* For osi_Panic() */

#define RX_ENABLE_LOCKS         1

extern lock_t *rx_sleepLock;

/* We use beta semaphores instead of sync semaphores for Rx locks as
 * recommended by HP labs. Sync semaphores are not supported by HP
 * any more.
 */

#define CV_INIT(cv,a,b,c)

/* This is supposed to atomically drop the mutex and go to sleep
 * and reacquire the mutex when it wakes up.
 */

/* With 11.23, ksleep_prepare is not defined anywhere  and
 * ksleep_one is only referenced in a comment. sleep, get_sleep_lock
 * and wakeup are defined in driver manuals.
 * This works with 11.0, 11i, and 11.23
 * Note: wakeup wakes up all threads waiting on cv.
 */

#define CV_WAIT(cv, lck) \
	do { \
		get_sleep_lock((caddr_t)(cv)); \
		if (!b_owns_sema(lck)) \
			osi_Panic("CV_WAIT mutex not held \n"); \
		b_vsema(lck);	\
		sleep((caddr_t)(cv), PRIBIO); \
		b_psema(lck); \
	} while(0)

#define CV_SIGNAL(cv)  \
	do { \
		lock_t * sleep_lock = get_sleep_lock((caddr_t)(cv)); \
		wakeup((caddr_t)(cv)); \
		spinunlock(sleep_lock); \
	} while(0)

#define CV_BROADCAST(cv) \
	do { \
		lock_t * sleep_lock = get_sleep_lock((caddr_t)(cv)); \
		wakeup((caddr_t)(cv)); \
		spinunlock(sleep_lock); \
	} while(0)


#define CV_DESTROY(a)

/* We now use beta semaphores for mutexes */
typedef b_sema_t afs_kmutex_t;
typedef caddr_t afs_kcondvar_t;

#else /* AFS_HPUX110_ENV */

#if defined(AFS_HPUX102_ENV)
#define CV_INIT(a,b,c,d)
#define CV_DESTROY(a)
#endif /* AFS_HPUX102_ENV */
#endif /* else AFS_HPUX110_ENV */

#ifdef AFS_HPUX102_ENV

#if defined(AFS_HPUX110_ENV)

#define AFS_RX_ORDER 30

#define MUTEX_INIT(a,b,c,d) b_initsema((a), 1, AFS_RX_ORDER, (b))
#define MUTEX_DESTROY(a)

#define MUTEX_TRYENTER(a) b_cpsema(a)

#ifdef AFS_HPUX1111_ENV
#define MUTEX_ENTER(a) \
	((b_owns_sema(a)) ? osi_Panic("Already Held") : b_psema(a))
#define MUTEX_EXIT(a) \
	((b_owns_sema(a)) ? b_vsema(a) : osi_Panic("mutex not held"))
#else
#define MUTEX_ENTER(a) \
    ((b_owns_sema(a)) ? (osi_Panic("Already Held"), 0) : b_psema(a))

#define MUTEX_EXIT(a) \
    ((b_owns_sema(a)) ? b_vsema(a) : (osi_Panic("mutex not held"), 0))
#endif

#define MUTEX_ASSERT(a) osi_Assert(b_owns_sema(a))

#else /* AFS_HPUX110_ENV */

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)
#define MUTEX_INIT(a,b,c,d)
#define MUTEX_ASSERT(a)

#endif /* else AFS_HPUX110_ENV */
#endif /* AFS_HPUX102_ENV */
#endif /* _RX_KMUTEX_H_ */
