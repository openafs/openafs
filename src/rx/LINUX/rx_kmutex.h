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


#if defined(CONFIG_SMP) || defined(CONFIG_PREEMPT)
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

#if defined(AFS_LINUX24_ENV)
typedef wait_queue_head_t afs_kcondvar_t;
#else
typedef struct wait_queue *afs_kcondvar_t;
#endif

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
#define CV_INIT(cv,b,c,d)	init_waitqueue_head((wait_queue_head_t *)(cv))
#else
#define CV_INIT(cv,b,c,d)	init_waitqueue((struct wait_queue**)(cv))
#endif
#define CV_DESTROY(cv)
#define CV_WAIT_SIG(cv, m)	afs_cv_wait(cv, m, 1)
#define CV_WAIT(cv, m)		afs_cv_wait(cv, m, 0)
#define CV_TIMEDWAIT		afs_cv_timedwait

#if defined(AFS_LINUX24_ENV)
#define CV_SIGNAL(cv)		wake_up((wait_queue_head_t *)cv)
#define CV_BROADCAST(cv)	wake_up((wait_queue_head_t *)cv)
#else
#define CV_SIGNAL(cv)		wake_up((struct wait_queue**)cv)
#define CV_BROADCAST(cv)	wake_up((struct wait_queue**)cv)
#endif

#else

#define MUTEX_ISMINE(a)
#define osirx_AssertMine(addr, msg)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)
#define MUTEX_INIT(a,b,c,d)
#define CV_INIT(a,b,c,d)
#define CV_DESTROY(a)
#endif

/* Since we're using the RX listener daemon, we don't need to hold off
 * interrupts.
 */
#define SPLVAR
#define NETPRI
#define USERPRI

#endif /* RX_KMUTEX_H_ */
