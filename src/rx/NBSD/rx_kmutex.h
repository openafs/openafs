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
 * MACOS implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#include <sys/lock.h>
#include <kern/thread.h>
#include <sys/vm.h>

#define RX_ENABLE_LOCKS         1
#define AFS_GLOBAL_RXLOCK_KERNEL

/*
 * Condition variables
 *
 * In Digital Unix (OSF/1), we use something akin to the ancient sleep/wakeup
 * mechanism.  The condition variable itself plays no role; we just use its
 * address as a convenient unique number.
 * 
 * XXX in darwin, both mach and bsd facilities are available. Should really
 * stick to one or the other (but mach locks don't have a _try.....)
 */
#define CV_INIT(cv,a,b,c)
#define CV_DESTROY(cv)
#define CV_WAIT(cv, lck)    { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        assert_wait((event_t)(cv), 0);  \
	                        MUTEX_EXIT(lck);        \
	                        thread_block(0);                \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck); \
	                    }

#define CV_TIMEDWAIT(cv,lck,t)  { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        assert_wait((event_t)(cv), 0);  \
	                        thread_set_timer(t, NSEC_PER_SEC/hz);   \
	                        MUTEX_EXIT(lck);        \
	                        thread_block(0);                \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck);       \
				}

#define CV_SIGNAL(cv)           thread_wakeup_one((event_t)(cv))
#define CV_BROADCAST(cv)        thread_wakeup((event_t)(cv))

typedef struct {
    struct lock__bsd__ lock;
    thread_t owner;
} afs_kmutex_t;
typedef afs_krwlock_t afs_kmutex_t;
typedef int afs_kcondvar_t;

#define RWLOCK_INIT(a, b, c, d) MUTEX_INIT(a,b,c,d)
#define RWLOCK_DESTROY(l)       MUTEX_DESTROY(l)
#define RWLOCK_UPLOCK(l) 
#define RWLOCK_WRLOCK(l)        MUTEX_ENTER(l)
#define RWLOCK_RDLOCK(l)        MUTEX_ENTER(l)
#define RWLOCK_TRYWRLOCK(l)     MUTEX_TRYENTER(l)
#define RWLOCK_TRYRDLOCK(l)     MUTEX_TRYENTER(l)
#define RWLOCK_UNLOCK(l)        MUTEX_EXIT(l)

#define osi_rxWakeup(cv)        thread_wakeup((event_t)(cv))

#define LOCK_INIT(a,b) \
    do { \
	lockinit(&(a)->lock,PSOCK, "afs rx lock", 0, 0); \
	(a)->owner = (thread_t)0; \
    } while(0);
#define MUTEX_INIT(a,b,c,d) \
    do { \
	lockinit(&(a)->lock,PSOCK, "afs rx mutex", 0, 0); \
	(a)->owner = (thread_t)0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (thread_t)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	lockmgr(&(a)->lock, LK_EXCLUSIVE, 0, current_proc()); \
	osi_Assert((a)->owner == (thread_t)0); \
	(a)->owner = current_thread(); \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( lockmgr(&(a)->lock, LK_EXCLUSIVE|LK_NOWAIT, 0, current_proc()) ? 0 : ((a)->owner = current_thread(), 1) )
#define xMUTEX_TRYENTER(a) \
    ( osi_Assert((a)->owner == (thread_t)0), (a)->owner = current_thread(), 1)
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == current_thread()); \
	(a)->owner = (thread_t)0; \
	lockmgr(&(a)->lock, LK_RELEASE, 0, current_proc()); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == current_thread())

#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);

#endif /* _RX_KMUTEX_H_ */
