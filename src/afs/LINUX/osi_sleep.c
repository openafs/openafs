/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "../afs/param.h"

RCSID("$Header$");

#include "../afs/sysincludes.h"	/* Standard vendor system headers */
#include "../afs/afsincludes.h"	/* Afs-based standard headers */
#include "../afs/afs_stats.h"   /* afs statistics */



#if defined(AFS_GLOBAL_SUNLOCK)
static int osi_TimedSleep(char *event, afs_int32 ams, int aintok);
#endif

void afs_osi_Wakeup(char *event);
void afs_osi_Sleep(char *event);

static char waitV, dummyV;

#if ! defined(AFS_GLOBAL_SUNLOCK)

/* call procedure aproc with arock as an argument, in ams milliseconds */
static struct timer_list *afs_osi_CallProc(void *aproc, void *arock, int ams)
{
    struct timer_list *timer = NULL;
    
    timer = (struct timer_list*)osi_Alloc(sizeof(struct timer_list));
    if (timer) {
	init_timer(timer);
	timer->expires = (ams*afs_hz)/1000 + 1;
	timer->data = (unsigned long)arock;
	timer->function = aproc;
	add_timer(timer);
    }
    return timer;
}

/* cancel a timeout, whether or not it has already occurred */
static int afs_osi_CancelProc(struct timer_list *timer)
{
    if (timer) {
	del_timer(timer);
	osi_Free(timer, sizeof(struct timer_list));
    }
    return 0;
}

static AfsWaitHack()
{
    AFS_STATCNT(WaitHack);
    wakeup(&waitV);
}

#endif

void afs_osi_InitWaitHandle(struct afs_osi_WaitHandle *achandle)
{
    AFS_STATCNT(osi_InitWaitHandle);
    achandle->proc = (caddr_t) 0;
}

/* cancel osi_Wait */
void afs_osi_CancelWait(struct afs_osi_WaitHandle *achandle)
{
    caddr_t proc;

    AFS_STATCNT(osi_CancelWait);
    proc = achandle->proc;
    if (proc == 0) return;
    achandle->proc = (caddr_t) 0;   /* so dude can figure out he was signalled */
    afs_osi_Wakeup(&waitV);
}

/* afs_osi_Wait
 * Waits for data on ahandle, or ams ms later.  ahandle may be null.
 * Returns 0 if timeout and EINTR if signalled.
 */
int afs_osi_Wait(afs_int32 ams, struct afs_osi_WaitHandle *ahandle, int aintok)
{
    int code;
    afs_int32 endTime, tid;
    struct timer_list *timer = NULL;

    AFS_STATCNT(osi_Wait);
    endTime = osi_Time() + (ams/1000);
    if (ahandle)
	ahandle->proc = (caddr_t) current;

    AFS_ASSERT_GLOCK();
    do {
#if	defined(AFS_GLOBAL_SUNLOCK)
        code = osi_TimedSleep(&waitV, ams, 1);
        if (code == EINTR) {
                if (aintok) 
		    return EINTR;
                flush_signals(current);
        }
#else
	timer = afs_osi_CallProc(AfsWaitHack, (char *) current, ams);
	afs_osi_Sleep(&waitV);
	afs_osi_CancelProc(timer);
#endif /* AFS_GLOBAL_SUNLOCK */
	if (ahandle && (ahandle->proc == (caddr_t) 0)) {
	    /* we've been signalled */
	    return EINTR;
	}
    } while (osi_Time() < endTime);
    return 0;
}




typedef struct afs_event {
    struct afs_event *next;	/* next in hash chain */
    char *event;		/* lwp event: an address */
    int refcount;		/* Is it in use? */
    int seq;			/* Sequence number: this is incremented
				   by wakeup calls; wait will not return until
				   it changes */
#if defined(AFS_LINUX24_ENV)
    wait_queue_head_t cond;
#else
    struct wait_queue *cond;
#endif
} afs_event_t;

#define HASHSIZE 128
afs_event_t *afs_evhasht[HASHSIZE];/* Hash table for events */
#define afs_evhash(event)	(afs_uint32) ((((long)event)>>2) & (HASHSIZE-1));
int afs_evhashcnt = 0;

/* Get and initialize event structure corresponding to lwp event (i.e. address)
 * */
static afs_event_t *afs_getevent(char *event)
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

static void afs_addevent(char *event)
{
    int hashcode;
    afs_event_t *newp;
    
    AFS_ASSERT_GLOCK();
    hashcode = afs_evhash(event);
    newp = osi_AllocSmallSpace(sizeof(afs_event_t));
    afs_evhashcnt++;
    newp->next = afs_evhasht[hashcode];
    afs_evhasht[hashcode] = newp;
#if defined(AFS_LINUX24_ENV)
    init_waitqueue_head(&newp->cond);
#else
    init_waitqueue(&newp->cond);
#endif
    newp->seq = 0;
    newp->event = &dummyV;  /* Dummy address for new events */
    newp->refcount = 0;
}


/* Release the specified event */
#define relevent(evp) ((evp)->refcount--)

/* afs_osi_Sleep -- waits for an event to be notified. */

void afs_osi_Sleep(char *event)
{
    struct afs_event *evp;
    int seq;

    evp = afs_getevent(event);
    if (!evp) {
	/* Can't block because allocating a new event would require dropping
         * the GLOCK, which may cause us to miss the wakeup.  So call the
         * allocator then return immediately.  We'll find the new event next
         * time around without dropping the GLOCK. */
        afs_addevent(event);
        return;
    }

    seq = evp->seq;

    while (seq == evp->seq) {
        afs_Trace4(afs_iclSetp, CM_TRACE_SLEEP,
		ICL_TYPE_POINTER, evp,
		   ICL_TYPE_INT32, 0/*count*/,
		ICL_TYPE_INT32, seq,
		ICL_TYPE_INT32, evp->seq);
	AFS_ASSERT_GLOCK();
	AFS_GUNLOCK();
	interruptible_sleep_on(&evp->cond);
	AFS_GLOCK();
    }
    relevent(evp);
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
static int osi_TimedSleep(char *event, afs_int32 ams, int aintok)
{
    long t = ams * HZ / 1000;
    struct afs_event *evp;

    evp = afs_getevent(event);
    if (!evp) {
        /* Can't block because allocating a new event would require dropping
         * the GLOCK, which may cause us to miss the wakeup.  So call the
         * allocator then return immediately.  We'll find the new event next
         * time around without dropping the GLOCK. */
        afs_addevent(event);
        return EAGAIN;
    }

    AFS_GUNLOCK();
    if (aintok)
	t = interruptible_sleep_on_timeout(&evp->cond, t);
    else
	t = sleep_on_timeout(&evp->cond, t);
    AFS_GLOCK();

    relevent(evp);

    return t ? EINTR : 0;
}


void afs_osi_Wakeup(char *event)
{
    struct afs_event *evp;

    evp = afs_getevent(event);
    if (!evp)                          /* No sleepers */
	return;

    if (evp->refcount > 1) {
	evp->seq++;    
        afs_Trace2(afs_iclSetp, CM_TRACE_WAKE,
		ICL_TYPE_POINTER, evp,
		ICL_TYPE_INT32, evp->seq);
	wake_up(&evp->cond);
    }
    relevent(evp);
}
