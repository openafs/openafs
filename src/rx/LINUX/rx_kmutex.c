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
#include "../afs/param.h"

RCSID("$Header$");

#include "../rx/rx_kcommon.h"
#include "../rx/rx_kmutex.h"
#include "../rx/rx_kernel.h"

#ifdef CONFIG_SMP

void afs_mutex_init(afs_kmutex_t *l)
{
#if defined(AFS_LINUX24_ENV)
    init_MUTEX(&l->sem);
#else
    l->sem = MUTEX;
#endif
    l->owner = 0;
}

void afs_mutex_enter(afs_kmutex_t *l)
{
    down(&l->sem);
    if (l->owner)
	osi_Panic("mutex_enter: 0x%x held by %d", l, l->owner);
    l->owner = current->pid;
}
							      
int afs_mutex_tryenter(afs_kmutex_t *l)
{
    if (down_trylock(&l->sem))
	return 0;
    l->owner = current->pid;
    return 1;
}

void afs_mutex_exit(afs_kmutex_t *l)
{
    if (l->owner != current->pid)
	osi_Panic("mutex_exit: 0x%x held by %d",
		  l, l->owner);
    l->owner = 0;
    up(&l->sem);
}

/*
 * CV_WAIT and CV_TIMEDWAIT rely on the fact that the Linux kernel has
 * a global lock. Thus we can safely drop our locks before calling the
 * kernel sleep services.
 */
int afs_cv_wait(afs_kcondvar_t *cv, afs_kmutex_t *l, int sigok)
{
    int isAFSGlocked = ISAFS_GLOCK();
    sigset_t saved_set;

    if (isAFSGlocked) AFS_GUNLOCK();
    MUTEX_EXIT(l);

    if (!sigok) {
	spin_lock_irq(&current->sigmask_lock);
	saved_set = current->blocked;
	sigfillset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
    }

#if defined(AFS_LINUX24_ENV)
    interruptible_sleep_on((wait_queue_head_t *)cv);
#else
    interruptible_sleep_on((struct wait_queue**)cv);
#endif

    if (!sigok) {
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = saved_set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
    }

    MUTEX_ENTER(l);
    if (isAFSGlocked) AFS_GLOCK();

    return (sigok && signal_pending(current)) ? EINTR : 0;
}

void afs_cv_timedwait(afs_kcondvar_t *cv, afs_kmutex_t *l, int waittime)
{
    int isAFSGlocked = ISAFS_GLOCK();
    long t = waittime * HZ / 1000;

    if (isAFSGlocked) AFS_GUNLOCK();
    MUTEX_EXIT(l);
    
#if defined(AFS_LINUX24_ENV)
    t = interruptible_sleep_on_timeout((wait_queue_head_t *)cv, t);
#else
    t = interruptible_sleep_on_timeout((struct wait_queue**)cv, t);
#endif
    
    MUTEX_ENTER(l);
    if (isAFSGlocked) AFS_GLOCK();
}

#endif
