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
 * FBSD implementation.
 */

#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_

#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lock.h>

#define RX_ENABLE_LOCKS         1
#define AFS_GLOBAL_RXLOCK_KERNEL

/*
 * Condition variables
 *
 * In Digital Unix (OSF/1), we use something akin to the ancient sleep/wakeup
 * mechanism.  The condition variable itself plays no role; we just use its
 * address as a convenient unique number.
 */
#define CV_INIT(cv,a,b,c)
#define CV_DESTROY(cv)
#define CV_WAIT(cv, lck)    { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        MUTEX_EXIT(lck);        \
	                        tsleep(cv, PSOCK, "afs_rx_cv_wait", 0);  \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck); \
	                    }

#define CV_TIMEDWAIT(cv,lck,t)  { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        MUTEX_EXIT(lck);        \
	                        tsleep(cv, PSOCK, "afs_rx_cv_timedwait", t); \
	                        if (isGlockOwner) AFS_GLOCK();  \
	                        MUTEX_ENTER(lck);       \

#define CV_SIGNAL(cv)           wakeup_one(cv)
#define CV_BROADCAST(cv)        wakeup(cv)

#define osi_rxWakeup(cv)        wakeup(cv)
typedef int afs_kcondvar_t;

#define HEAVY_LOCKS
#ifdef NULL_LOCKS
typedef struct {
    struct proc *owner;
} afs_kmutex_t;

#define MUTEX_INIT(a,b,c,d) \
    do { \
	(a)->owner = 0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (struct proc *)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curproc; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( osi_Assert((a)->owner == 0), (a)->owner = curproc, 1)
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curproc); \
	(a)->owner = 0; \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == curproc)

#else
#ifdef HEAVY_LOCKS
typedef struct {
    struct lock lock;
    struct proc *owner;
} afs_kmutex_t;


#define MUTEX_INIT(a,b,c,d) \
    do { \
	lockinit(&(a)->lock,PSOCK, "afs rx mutex", 0, 0); \
	(a)->owner = 0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (struct proc *)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	lockmgr(&(a)->lock, LK_EXCLUSIVE, 0, curproc); \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curproc; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( lockmgr(&(a)->lock, LK_EXCLUSIVE|LK_NOWAIT, 0, curproc) ? 0 : ((a)->owner = curproc, 1) )
#define xMUTEX_TRYENTER(a) \
    ( osi_Assert((a)->owner == 0), (a)->owner = curproc, 1)
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curproc); \
	(a)->owner = 0; \
	lockmgr(&(a)->lock, LK_RELEASE, 0, curproc); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == curproc)
#else
typedef struct {
    struct simplelock lock;
    struct proc *owner;
} afs_kmutex_t;


#define MUTEX_INIT(a,b,c,d) \
    do { \
	simple_lock_init(&(a)->lock); \
	(a)->owner = 0; \
    } while(0);
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (struct proc *)-1; \
    } while(0);
#define MUTEX_ENTER(a) \
    do { \
	simple_lock(&(a)->lock); \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curproc; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( simple_lock_try(&(a)->lock) ? 0 : ((a)->owner = curproc, 1) )
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curproc); \
	(a)->owner = 0; \
	simple_unlock(&(a)->lock); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == curproc)
#endif
#endif


#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t *lockaddr, char *msg);

#endif /* _RX_KMUTEX_H_ */

