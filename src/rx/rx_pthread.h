/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* rx_pthread.h defines the lock and cv primitives required for a thread
 * safe user mode rx.
 */

#ifndef RX_PTHREAD_H
#define RX_PTHREAD_H

#ifndef RX_ENABLE_LOCKS
#define RX_ENABLE_LOCKS 1
#endif

/* Block signals to child threads. */
#include <afs/pthread_nosigs.h>
#include <afs/opr.h>
#include <opr/lock.h>

#ifdef AFS_NT40_ENV
#include <wtypes.h>
#include <winbase.h>
#include <winsock2.h>
#include <pthread.h>

typedef pthread_mutex_t afs_kmutex_t;
typedef pthread_cond_t afs_kcondvar_t;

#define pthread_yield() Sleep(0)

#else /* AFS_NT40_ENV */

#include <pthread.h>
typedef pthread_mutex_t afs_kmutex_t;
typedef pthread_cond_t afs_kcondvar_t;

#if !defined(pthread_yield) && defined(AFS_SUN5_ENV)
#define pthread_yield() thr_yield()
#endif
#if !defined(pthread_yield) && !defined(AFS_AIX_ENV)
#define pthread_yield() sleep(0)
#endif
#if !defined(pthread_yield) && (_XOPEN_SOURCE + 0) >= 500
#define pthread_yield() sched_yield()
#endif
#endif /* AFS_NT40_ENV */

#define MUTEX_INIT(a, b, c, d) opr_mutex_init(a)
#define MUTEX_DESTROY(l) opr_mutex_destroy(l)
#define MUTEX_ENTER(l) opr_mutex_enter(l)
#define MUTEX_TRYENTER(l) opr_mutex_tryenter(l)
#define MUTEX_EXIT(l) opr_mutex_exit(l)
#define MUTEX_ASSERT(l) opr_mutex_assert(l)
#define CV_INIT(cv, a, b, c) opr_cv_init(cv)
#define CV_DESTROY(cv) opr_cv_destroy(cv)
#define CV_WAIT(cv, l) opr_cv_wait(cv, l)
#define CV_TIMEDWAIT(cv, l, t) opr_cv_timedwait(cv, l, t)
#define CV_SIGNAL(cv) opr_cv_signal(cv)
#define CV_BROADCAST(cv) opr_cv_broadcast(cv)

#endif /* RX_PTHREAD_H */
