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


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */
#include "osi_compat.h"

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
	code = afs_osi_TimedSleep(&waitV, ams, 1);
	if (code)
	    break;
	if (ahandle && (ahandle->proc == (caddr_t) 0)) {
	    /* we've been signalled */
	    break;
	}
    } while (osi_Time() < endTime);
    return code;
}

afs_event_t *afs_evhasht[AFS_EVHASHSIZE];	/* Hash table for events */
#define afs_evhash(event)	(afs_uint32) ((((long)event)>>2) & (AFS_EVHASHSIZE-1))
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
 *     Called with GLOCK held.
 */

static void
afs_addevent(char *event)
{
    int hashcode;
    afs_event_t *newp;

    AFS_ASSERT_GLOCK();
    hashcode = afs_evhash(event);
    newp = kzalloc(sizeof(afs_event_t), GFP_NOFS);
    afs_evhashcnt++;
    newp->next = afs_evhasht[hashcode];
    afs_evhasht[hashcode] = newp;
    init_waitqueue_head(&newp->cond);
    newp->event = &dummyV;	/* Dummy address for new events */
}

#ifndef set_current_state
#define set_current_state(x)            current->state = (x)
#endif

/* Release the specified event */
#define relevent(evp) ((evp)->refcount--)

static int
afs_linux_sleep(void *event, int killable)
{
    struct afs_event *evp;
    int seq;
    int code;
    sigset_t saved_set;

    sigemptyset(&saved_set);

    evp = afs_getevent(event);
    if (!evp) {
	afs_addevent(event);
	evp = afs_getevent(event);
    }

    seq = evp->seq;

    AFS_GUNLOCK();

    SIG_LOCK(current);
    saved_set = current->blocked;
    if (killable) {
	siginitsetinv(&current->blocked, sigmask(SIGKILL));
    } else {
	sigfillset(&current->blocked);
    }
    RECALC_SIGPENDING(current);
    SIG_UNLOCK(current);

    code = wait_event_freezable(evp->cond, seq != evp->seq);

    SIG_LOCK(current);
    current->blocked = saved_set;
    RECALC_SIGPENDING(current);
    SIG_UNLOCK(current);

    AFS_GLOCK();

    if (code == -ERESTARTSYS)
	code = EINTR;
    else
	code = -code;

    relevent(evp);
    return code;
}

/* afs_osi_SleepSig
 *
 * Waits for an event to be notified, returning early if a signal
 * is received.  Returns EINTR if signaled, and 0 otherwise.
 */
int
afs_osi_SleepSig(void *event)
{
    return afs_linux_sleep(event, 1);
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
    afs_linux_sleep(event, 0);
}

/* afs_osi_TimedSleep
 * 
 * Arguments:
 * event - event to sleep on
 * ams --- max sleep time in milliseconds
 * aintok - 1 if should sleep interruptibly
 *
 * Returns 0 if timeout, EINTR if signalled, and EGAIN if it might
 * have raced.
 */
int
afs_osi_TimedSleep(void *event, afs_int32 ams, int aintok)
{
    int code = 0;
    long ticks = (ams * HZ / 1000) + 1;
    struct afs_event *evp;
    int seq;

    evp = afs_getevent(event);
    if (!evp) {
	afs_addevent(event);
	evp = afs_getevent(event);
    }

    seq = evp->seq;

    AFS_GUNLOCK();
    code = wait_event_freezable_timeout(evp->cond, evp->seq != seq, ticks);
    AFS_GLOCK();
    if (code == -ERESTARTSYS)
	code = EINTR;
    else
	code = -code;

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
