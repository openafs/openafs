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
#include <sys/lockmgr.h>

#define RX_ENABLE_LOCKS         1

typedef int afs_kcondvar_t;

#define HEAVY_LOCKS
#if defined(NULL_LOCKS)
typedef struct {
    struct proc *owner;
} afs_kmutex_t;

#define MUTEX_INIT(a,b,c,d) \
    do { \
	(a)->owner = 0; \
    } while(0)
#define MUTEX_DESTROY(a) \
    do { \
	(a)->owner = (struct proc *)-1; \
    } while(0)
#define MUTEX_ENTER(a) \
    do { \
	osi_Assert((a)->owner == 0); \
	(a)->owner = curproc; \
    } while(0)
#define MUTEX_TRYENTER(a) \
    ( osi_Assert((a)->owner == 0), (a)->owner = curproc, 1)
#define MUTEX_EXIT(a) \
    do { \
	osi_Assert((a)->owner == curproc); \
	(a)->owner = 0; \
    } while(0)

#define MUTEX_ASSERT(a) osi_Assert(((afs_kmutex_t *)(a))->owner == curproc)

#else /* NULL_LOCKS */

typedef struct sx afs_kmutex_t;

#ifdef WITNESS
#define WITCLEAR_MTX(a)					\
    do { memset((a), 0, sizeof(struct sx)); } while(0)
#else
#define WITCLEAR_MTX(a) {}
#endif

#define MUTEX_INIT(a,b,c,d)					     \
  do {								     \
      WITCLEAR_MTX(a);						     \
      sx_init((a), (b)); \
  } while(0)

#define MUTEX_DESTROY(a)			\
    do {					\
	sx_destroy((a));			\
    } while(0)

#define MUTEX_ENTER(a) \
    do {	       \
	sx_xlock((a)); \
    } while(0)

#define MUTEX_TRYENTER(a)			\
    ( sx_try_xlock((a)) )

#define MUTEX_EXIT(a)	 \
    do {		 \
	sx_xunlock((a)); \
    } while(0)

#define MUTEX_ASSERT(a)				\
    osi_Assert(sx_xlocked((a)))

#endif /* !NULL_LOCKS */

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
    int isGlockOwner = ISAFS_GLOCK();					\
    if (isGlockOwner) AFS_GUNLOCK();					\
    msleep(cv, lck, PSOCK, "afs_rx_cv_wait", 0);			\
    if (isGlockOwner) AFS_GLOCK();					\
  }

#define CV_SIGNAL(cv)           wakeup_one(cv)
#define CV_BROADCAST(cv)        wakeup(cv)

/* #define osi_rxWakeup(cv)        wakeup(cv) */


#endif /* _RX_KMUTEX_H_ */
