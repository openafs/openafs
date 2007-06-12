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
    ("$Header: /cvs/openafs/src/rx/LINUX/rx_kmutex.c,v 1.7.2.7 2006/12/28 21:32:09 shadow Exp $");

#include "rx/rx_kcommon.h"
#include "rx_kmutex.h"
#include "rx/rx_kernel.h"

void
afs_mutex_init(afs_kmutex_t * l)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    mutex_init(&l->mutex);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
    init_MUTEX(&l->sem);
#else
    l->sem = MUTEX;
#endif
    l->owner = 0;
}

void
afs_mutex_enter(afs_kmutex_t * l)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    mutex_lock(&l->mutex);
#else
    down(&l->sem);
#endif
    if (l->owner)
	osi_Panic("mutex_enter: 0x%x held by %d", l, l->owner);
    l->owner = current->pid;
}

int
afs_mutex_tryenter(afs_kmutex_t * l)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    if (mutex_trylock(&l->mutex) == 0)
#else
    if (down_trylock(&l->sem))
#endif
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    mutex_unlock(&l->mutex);
#else
    up(&l->sem);
#endif
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
	unsigned long f;
	SIG_LOCK(current,f);
	saved_set = current->blocked;
	sigfillset(&current->blocked);
	RECALC_SIGPENDING(current);
	SIG_UNLOCK(current,f);
    }

    while(seq == cv->seq) {
	schedule();
#ifdef AFS_LINUX26_ENV
#ifdef CONFIG_PM
	if (
#ifdef PF_FREEZE
	    current->flags & PF_FREEZE
#else
#if defined(STRUCT_TASK_STRUCT_HAS_TODO)
	    !current->todo
#else
#if defined(STRUCT_TASK_STRUCT_HAS_THREAD_INFO)
	    test_ti_thread_flag(current->thread_info, TIF_FREEZE)
#else
	    test_ti_thread_flag(task_thread_info(current), TIF_FREEZE)
#endif
#endif
#endif
	    )
#ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
	    refrigerator(PF_FREEZE);
#else
	    refrigerator();
#endif
	    set_current_state(TASK_INTERRUPTIBLE);
#endif
#endif
    }

    remove_wait_queue(&cv->waitq, &wait);
    set_current_state(TASK_RUNNING);

    if (!sigok) {
	unsigned long f;
	SIG_LOCK(current, f);
	current->blocked = saved_set;
	RECALC_SIGPENDING(current);
	SIG_UNLOCK(current, f);
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
