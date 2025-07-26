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


#include "rx/rx_kcommon.h"
#include "rx_kmutex.h"
#include "rx/rx_kernel.h"

#include "osi_compat.h"

void
afs_mutex_init(afs_kmutex_t * l)
{
    mutex_init(&l->mutex);
    l->owner = 0;
}

void
afs_mutex_enter(afs_kmutex_t * l)
{
    mutex_lock(&l->mutex);
    if (l->owner)
	osi_Panic("mutex_enter: 0x%lx held by %d", (unsigned long)l, l->owner);
    l->owner = current->pid;
}

int
afs_mutex_tryenter(afs_kmutex_t * l)
{
    if (mutex_trylock(&l->mutex) == 0)
	return 0;
    l->owner = current->pid;
    return 1;
}

void
afs_mutex_exit(afs_kmutex_t * l)
{
    if (l->owner != current->pid)
	osi_Panic("mutex_exit: 0x%lx held by %d", (unsigned long)l, l->owner);
    l->owner = 0;
    mutex_unlock(&l->mutex);
}

/* CV_WAIT sleeps until the specified event occurs.
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
    sigemptyset(&saved_set);
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
	afs_try_to_freeze();
	set_current_state(TASK_INTERRUPTIBLE);
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
