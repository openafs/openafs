/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * rx_kmutex.c - mutex and condition variable macros for kernel environment.
 *
 * Linux implementation.
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "rx/rx_kcommon.h"
#include "rx_kmutex.h"
#include "rx/rx_kernel.h"

void
afs_mutex_init(afs_kmutex_t * l)
{
#if defined(AFS_LINUX24_ENV)
    init_MUTEX(&l->sem);
#else
    l->sem = MUTEX;
#endif
    l->owner = 0;
}

void
afs_mutex_enter(afs_kmutex_t * l)
{
    down(&l->sem);
    if (l->owner)
	osi_Panic("mutex_enter: 0x%x held by %d", l, l->owner);
    l->owner = current->pid;
}

int
afs_mutex_tryenter(afs_kmutex_t * l)
{
    if (down_trylock(&l->sem))
	return 0;
    l->owner = current->pid;
    return 1;
}

void
afs_mutex_exit(afs_kmutex_t * l)
{
    if (l->owner != current->pid)
	osi_Panic("mutex_exit: 0x%x held by %d", l, l->owner);
    l->owner = 0;
    up(&l->sem);
}

/* CV_WAIT and CV_TIMEDWAIT sleep until the specified event occurs, or, in the
 * case of CV_TIMEDWAIT, until the specified timeout occurs.
 * - NOTE: that on Linux, there are circumstances in which TASK_INTERRUPTIBLE
 *   can wake up, even if all signals are blocked
 * - TODO: handle signals correctly by passing an indication back to the
 *   caller that the wait has been interrupted and the stack should be cleaned
 *   up preparatory to signal delivery
 */
int
afs_cv_wait(afs_kcondvar_t * cv, afs_kmutex_t * l, int sigok)
{
    int seq, isAFSGlocked = ISAFS_GLOCK();
    sigset_t saved_set;
#ifdef DECLARE_WAITQUEUE
    DECLARE_WAITQUEUE(wait, current);
#else
    struct wait_queue wait = { current, NULL };
#endif

    seq = cv->seq;
    
    set_current_state(TASK_INTERRUPTIBLE);
    add_wait_queue(&cv->waitq, &wait);

    if (isAFSGlocked)
	AFS_GUNLOCK();
    MUTEX_EXIT(l);

    if (!sigok) {
	SIG_LOCK(current);
	saved_set = current->blocked;
	sigfillset(&current->blocked);
	RECALC_SIGPENDING(current);
	SIG_UNLOCK(current);
    }

    while(seq == cv->seq) {
	schedule();
	/* should we refrigerate? */
    }

    remove_wait_queue(&cv->waitq, &wait);
    set_current_state(TASK_RUNNING);

    if (!sigok) {
	SIG_LOCK(current);
	current->blocked = saved_set;
	RECALC_SIGPENDING(current);
	SIG_UNLOCK(current);
    }

    if (isAFSGlocked)
	AFS_GLOCK();
    MUTEX_ENTER(l);

    return (sigok && signal_pending(current)) ? EINTR : 0;
}

void
afs_cv_timedwait(afs_kcondvar_t * cv, afs_kmutex_t * l, int waittime)
{
    int seq, isAFSGlocked = ISAFS_GLOCK();
    long t = waittime * HZ / 1000;
#ifdef DECLARE_WAITQUEUE
    DECLARE_WAITQUEUE(wait, current);
#else
    struct wait_queue wait = { current, NULL };
#endif
    seq = cv->seq;

    set_current_state(TASK_INTERRUPTIBLE);
    add_wait_queue(&cv->waitq, &wait);

    if (isAFSGlocked)
	AFS_GUNLOCK();
    MUTEX_EXIT(l);

    while(seq == cv->seq) {
	t = schedule_timeout(t);
	if (!t)         /* timeout */
	    break;
    }
    
    remove_wait_queue(&cv->waitq, &wait);
    set_current_state(TASK_RUNNING);

    if (isAFSGlocked)
	AFS_GLOCK();
    MUTEX_ENTER(l);
}
