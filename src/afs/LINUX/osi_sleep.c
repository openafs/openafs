/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header: /cvs/openafs/src/afs/LINUX/osi_sleep.c,v 1.22.2.10 2007/01/04 21:26:34 shadow Exp $");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if defined(FREEZER_H_EXISTS)
#include <linux/freezer.h>
#endif

static int osi_TimedSleep(char *event, afs_int32 ams, int aintok);

static char waitV, dummyV;

void
afs_osi_InitWaitHandle(struct afs_osi_WaitHandle *achandle)
{
    AFS_STATCNT(osi_InitWaitHandle);
    achandle->proc = (caddr_t) 0;
}

/* cancel osi_Wait */
void
afs_osi_CancelWait(struct afs_osi_WaitHandle *achandle)
{
    caddr_t proc;

    AFS_STATCNT(osi_CancelWait);
    proc = achandle->proc;
    if (proc == 0)
	return;
    achandle->proc = (caddr_t) 0;	/* so dude can figure out he was signalled */
    afs_osi_Wakeup(&waitV);
}

/* afs_osi_Wait
 * Waits for data on ahandle, or ams ms later.  ahandle may be null.
 * Returns 0 if timeout and EINTR if signalled.
 */
int
afs_osi_Wait(afs_int32 ams, struct afs_osi_WaitHandle *ahandle, int aintok)
{
    afs_int32 endTime;
    int code;

    AFS_STATCNT(osi_Wait);
    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) current;

    do {
	AFS_ASSERT_GLOCK();
	code = osi_TimedSleep(&waitV, ams, 1);
	if (code)
	    break;
	if (ahandle && (ahandle->proc == (caddr_t) 0)) {
	    /* we've been signalled */
	    break;
	}
    } while (osi_Time() < endTime);
    return code;
}




typedef struct afs_event {
    struct afs_event *next;	/* next in hash chain */
    char *event;		/* lwp event: an address */
    int refcount;		/* Is it in use? */
    int seq;			/* Sequence number: this is incremented
				 * by wakeup calls; wait will not return until
				 * it changes */
#if defined(AFS_LINUX24_ENV)
    wait_queue_head_t cond;
#else
    struct wait_queue *cond;
#endif
} afs_event_t;

#define HASHSIZE 128
afs_event_t *afs_evhasht[HASHSIZE];	/* Hash table for events */
#define afs_evhash(event)	(afs_uint32) ((((long)event)>>2) & (HASHSIZE-1));
int afs_evhashcnt = 0;

/* Get and initialize event structure corresponding to lwp event (i.e. address)
 * */
static afs_event_t *
afs_getevent(char *event)
{
    afs_event_t *evp, *newp = 0;
    int hashcode;

    AFS_ASSERT_GLOCK();
    hashcode = afs_evhash(event);
    evp = afs_evhasht[hashcode];
    while (evp) {
	if (evp->event == event) {
	    evp->refcount++;
	    return evp;
	}
	if (evp->refcount == 0)
	    newp = evp;
	evp = evp->next;
    }
    if (!newp)
	return NULL;

    newp->event = event;
    newp->refcount = 1;
    return newp;
}

/* afs_addevent -- allocates a new event for the address.  It isn't returned;
 *     instead, afs_getevent should be called again.  Thus, the real effect of
 *     this routine is to add another event to the hash bucket for this
 *     address.
 *
 * Locks:
 *     Called with GLOCK held. However the function might drop
 *     GLOCK when it calls osi_AllocSmallSpace for allocating
 *     a new event (In Linux, the allocator drops GLOCK to avoid
 *     a deadlock).
 */

static void
afs_addevent(char *event)
{
    int hashcode;
    afs_event_t *newp;

    AFS_ASSERT_GLOCK();
    hashcode = afs_evhash(event);
    newp = osi_linux_alloc(sizeof(afs_event_t), 0);
    afs_evhashcnt++;
    newp->next = afs_evhasht[hashcode];
    afs_evhasht[hashcode] = newp;
#if defined(AFS_LINUX24_ENV)
    init_waitqueue_head(&newp->cond);
#else
    init_waitqueue(&newp->cond);
#endif
    newp->seq = 0;
    newp->event = &dummyV;	/* Dummy address for new events */
    newp->refcount = 0;
}

#ifndef set_current_state
#define set_current_state(x)            current->state = (x);
#endif

/* Release the specified event */
#define relevent(evp) ((evp)->refcount--)

/* afs_osi_SleepSig
 *
 * Waits for an event to be notified, returning early if a signal
 * is received.  Returns EINTR if signaled, and 0 otherwise.
 */
int
afs_osi_SleepSig(void *event)
{
    struct afs_event *evp;
    int seq, retval;
#ifdef DECLARE_WAITQUEUE
    DECLARE_WAITQUEUE(wait, current);
#else
    struct wait_queue wait = { current, NULL };
#endif

    evp = afs_getevent(event);
    if (!evp) {
	afs_addevent(event);
	evp = afs_getevent(event);
    }

    seq = evp->seq;
    retval = 0;

    add_wait_queue(&evp->cond, &wait);
    while (seq == evp->seq) {
	set_current_state(TASK_INTERRUPTIBLE);
	AFS_ASSERT_GLOCK();
	AFS_GUNLOCK();
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
            test_ti_thread_flag(current->thread_info, TIF_FREEZE)
#endif
#endif
	    )
#ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
	    refrigerator(PF_FREEZE);
#else
	    refrigerator();
#endif
#endif
#endif
	AFS_GLOCK();
	if (signal_pending(current)) {
	    retval = EINTR;
	    break;
	}
    }
    remove_wait_queue(&evp->cond, &wait);
    set_current_state(TASK_RUNNING);

    relevent(evp);
    return retval;
}

/* afs_osi_Sleep -- waits for an event to be notified, ignoring signals.
 * - NOTE: that on Linux, there are circumstances in which TASK_INTERRUPTIBLE
 *   can wake up, even if all signals are blocked
 * - TODO: handle signals correctly by passing an indication back to the
 *   caller that the wait has been interrupted and the stack should be cleaned
 *   up preparatory to signal delivery
 */
void
afs_osi_Sleep(void *event)
{
    sigset_t saved_set;
    unsigned long f;

    SIG_LOCK(current,f);
    saved_set = current->blocked;
    sigfillset(&current->blocked);
    RECALC_SIGPENDING(current);
    SIG_UNLOCK(current,f);

    afs_osi_SleepSig(event);

    SIG_LOCK(current,f);
    current->blocked = saved_set;
    RECALC_SIGPENDING(current);
    SIG_UNLOCK(current,f);
}

/* osi_TimedSleep
 * 
 * Arguments:
 * event - event to sleep on
 * ams --- max sleep time in milliseconds
 * aintok - 1 if should sleep interruptibly
 *
 * Returns 0 if timeout, EINTR if signalled, and EGAIN if it might
 * have raced.
 */
static int
osi_TimedSleep(char *event, afs_int32 ams, int aintok)
{
    int code = 0;
    long ticks = (ams * HZ / 1000) + 1;
    struct afs_event *evp;
#ifdef DECLARE_WAITQUEUE
    DECLARE_WAITQUEUE(wait, current);
#else
    struct wait_queue wait = { current, NULL };
#endif

    evp = afs_getevent(event);
    if (!evp) {
	afs_addevent(event);
	evp = afs_getevent(event);
    }

    add_wait_queue(&evp->cond, &wait);
    set_current_state(TASK_INTERRUPTIBLE);
    /* always sleep TASK_INTERRUPTIBLE to keep load average
     * from artifically increasing. */
    AFS_GUNLOCK();

    if (aintok) {
	if (schedule_timeout(ticks))
	    code = EINTR;
    } else
	schedule_timeout(ticks);
#ifdef AFS_LINUX26_ENV
#ifdef CONFIG_PM
    if (
#ifdef PF_FREEZE
	    current->flags & PF_FREEZE
#else
#if defined(STRUCT_TASK_STRUCT_HAS_TODO)
	    !current->todo
#else
            test_ti_thread_flag(current->thread_info, TIF_FREEZE)
#endif
#endif
	    )
#ifdef LINUX_REFRIGERATOR_TAKES_PF_FREEZE
	refrigerator(PF_FREEZE);
#else
	refrigerator();
#endif
#endif
#endif

    AFS_GLOCK();
    remove_wait_queue(&evp->cond, &wait);
    set_current_state(TASK_RUNNING);

    relevent(evp);

    return code;
}


int
afs_osi_Wakeup(void *event)
{
    int ret = 2;
    struct afs_event *evp;

    evp = afs_getevent(event);
    if (!evp)			/* No sleepers */
	return 1;

    if (evp->refcount > 1) {
	evp->seq++;
	wake_up(&evp->cond);
	ret = 0;
    }
    relevent(evp);
    return ret;
}
