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
#ifdef AFS_FBSD70_ENV
#include <sys/lock.h>
#include <sys/lockmgr.h>
#else
#include <sys/lock.h>
#endif

#define RX_ENABLE_LOCKS         1
#define AFS_GLOBAL_RXLOCK_KERNEL

typedef int afs_kcondvar_t;

#define HEAVY_LOCKS
#if defined(NULL_LOCKS)
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

#elif defined(AFS_FBSD70_ENV) /* dunno about 6.x */

typedef struct mtx afs_kmutex_t;

#if defined(AFS_FBSD80_ENV) && defined(WITNESS)
#define WITCLEAR_MTX(a)					\
    do { memset((a), 0, sizeof(struct mtx)); } while(0);
#else
#define WITCLEAR_MTX(a) {}
#endif

#define MUTEX_INIT(a,b,c,d)					     \
  do {								     \
      WITCLEAR_MTX(a);						     \
      mtx_init((a), (b), 0 /* type defaults to name */, MTX_DEF | MTX_DUPOK);    \
  } while(0);

#define MUTEX_DESTROY(a)			\
    do {					\
	mtx_destroy((a));			\
    } while(0);

#define MUTEX_ENTER(a) \
    do {	       \
	mtx_lock((a)); \
    } while(0);

#define MUTEX_TRYENTER(a)			\
    ( mtx_trylock((a)) )

#define MUTEX_EXIT(a)	 \
    do {		 \
	mtx_unlock((a)); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a)				\
    ( mtx_owned((a)) )

#else

typedef struct {
    struct lock lock;
    struct thread *owner;
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
	lockmgr(&(a)->lock, LK_EXCLUSIVE, 0, curthread); \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curthread; \
    } while(0);
#define MUTEX_TRYENTER(a) \
    ( lockmgr(&(a)->lock, LK_EXCLUSIVE|LK_NOWAIT, 0, curthread) ? 0 : ((a)->owner = curthread, 1) )
#define xMUTEX_TRYENTER(a) \
    ( osi_Assert((a)->owner == 0), (a)->owner = curthread, 1)
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curthread); \
	(a)->owner = 0; \
	lockmgr(&(a)->lock, LK_RELEASE, 0, curthread); \
    } while(0);

#undef MUTEX_ISMINE
#define MUTEX_ISMINE(a) (((afs_kmutex_t *)(a))->owner == curthread)
#endif


#undef osirx_AssertMine
extern void osirx_AssertMine(afs_kmutex_t * lockaddr, char *msg);


/*
 * Condition variables
 *
 * In Digital Unix (OSF/1), we use something akin to the ancient sleep/wakeup
 * mechanism.  The condition variable itself plays no role; we just use its
 * address as a convenient unique number.
 */
#define CV_INIT(cv,a,b,c)
#define CV_DESTROY(cv)

#if defined(AFS_FBSD70_ENV)

#define CV_WAIT(cv, lck)    { \
    int isGlockOwner = ISAFS_GLOCK();					\
    if (isGlockOwner) AFS_GUNLOCK();					\
    msleep(cv, lck, PSOCK, "afs_rx_cv_wait", 0);			\
    if (isGlockOwner) AFS_GLOCK();					\
  }

#define CV_TIMEDWAIT(cv,lck,t)  { \
	                        int isGlockOwner = ISAFS_GLOCK(); \
	                        if (isGlockOwner) AFS_GUNLOCK();  \
	                        msleep(cv, lck, PSOCK, "afs_rx_cv_timedwait", t); \
	                        if (isGlockOwner) AFS_GLOCK();  \
				}
#else /* !AFS_FBSD70_ENV */
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
				}
#endif /* AFS_FBSD80_ENV */

#define CV_SIGNAL(cv)           wakeup_one(cv)
#define CV_BROADCAST(cv)        wakeup(cv)

/* #define osi_rxWakeup(cv)        wakeup(cv) */


#endif /* _RX_KMUTEX_H_ */
