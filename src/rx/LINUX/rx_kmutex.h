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
 * Linux implementation.
 * This are noops until such time as the kernel no longer has a global lock.
 */
#ifndef RX_KMUTEX_H_
#define RX_KMUTEX_H_

#include "rx/rx_kernel.h"	/* for osi_Panic() */

/* AFS_GLOBAL_RXLOCK_KERNEL is defined so that the busy tq code paths are
 * used. The thread can sleep when sending packets.
 */
#define	AFS_GLOBAL_RXLOCK_KERNEL 1


#define RX_ENABLE_LOCKS 1

#ifndef _LINUX_CODA_FS_I
#define _LINUX_CODA_FS_I
struct coda_inode_info {
};
#endif
#include "linux/wait.h"
#include "linux/sched.h"

typedef struct afs_kmutex {
    struct semaphore sem;
    int owner;
} afs_kmutex_t;

#ifndef set_current_state
#define set_current_state(X) current->state=X
#endif

typedef struct afs_kcondvar {
    int seq;
#if defined(AFS_LINUX24_ENV)
    wait_queue_head_t waitq;
#else
    struct wait_queue *waitq;
#endif
} afs_kcondvar_t;

static inline int
MUTEX_ISMINE(afs_kmutex_t * l)
{
    return l->owner == current->pid;
}

#define MUTEX_INIT(a,b,c,d)	afs_mutex_init(a)
#define MUTEX_DESTROY(a)
#define MUTEX_ENTER		afs_mutex_enter
#define MUTEX_TRYENTER		afs_mutex_tryenter
#define MUTEX_EXIT		afs_mutex_exit

#if defined(AFS_LINUX24_ENV)
#define CV_INIT(cv,b,c,d)       do { (cv)->seq = 0; init_waitqueue_head(&(cv)->waitq); } while (0)
#else
#define CV_INIT(cv,b,c,d)	do { (cv)->seq = 0; init_waitqueue(&(cv)->waitq); } while (0)
#endif
#define CV_DESTROY(cv)
#define CV_WAIT_SIG(cv, m)	afs_cv_wait(cv, m, 1)
#define CV_WAIT(cv, m)		afs_cv_wait(cv, m, 0)
#define CV_TIMEDWAIT		afs_cv_timedwait

#define CV_SIGNAL(cv)          do { ++(cv)->seq; wake_up(&(cv)->waitq); } while (0)
#if defined(AFS_LINUX24_ENV)
#define CV_BROADCAST(cv)       do { ++(cv)->seq; wake_up_all(&(cv)->waitq); } while (0)
#else
#define CV_BROADCAST(cv)       do { ++(cv)->seq; wake_up(&(cv)->waitq); } while (0)
#endif

#endif /* RX_KMUTEX_H_ */
