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
    ("$Header$");

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */


int osi_TimedSleep(char *event, afs_int32 ams, int aintok);

static char waitV;


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
    int code;
    afs_int32 endTime, tid;
    struct proc *p = current_proc();

    AFS_STATCNT(osi_Wait);
    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) p;
    do {
	AFS_ASSERT_GLOCK();
	code = 0;
	code = osi_TimedSleep(&waitV, ams, aintok);

	if (code)
	    break;		/* if something happened, quit now */
	/* if we we're cancelled, quit now */
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
} afs_event_t;

#define HASHSIZE 128
afs_event_t *afs_evhasht[HASHSIZE];	/* Hash table for events */
#define afs_evhash(event)       (afs_uint32) ((((long)event)>>2) & (HASHSIZE-1));
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
    if (!newp) {
	newp = (afs_event_t *) osi_AllocSmallSpace(sizeof(afs_event_t));
	afs_evhashcnt++;
	newp->next = afs_evhasht[hashcode];
	afs_evhasht[hashcode] = newp;
	newp->seq = 0;
    }
    newp->event = event;
    newp->refcount = 1;
    return newp;
}

/* Release the specified event */
#define relevent(evp) ((evp)->refcount--)


void
afs_osi_Sleep(void *event)
{
    struct afs_event *evp;
    int seq;

    evp = afs_getevent(event);
    seq = evp->seq;
    while (seq == evp->seq) {
	AFS_ASSERT_GLOCK();
	AFS_GUNLOCK();
#ifdef AFS_DARWIN80_ENV
        msleep(event, NULL, PVFS, "afs_osi_Sleep", NULL);
#else
#ifdef AFS_DARWIN14_ENV
	/* this is probably safe for all versions, but testing is hard */
	sleep(event, PVFS);
#else
	assert_wait((event_t) event, 0);
	thread_block(0);
#endif
#endif
	AFS_GLOCK();
    }
    relevent(evp);
}

void 
afs_osi_fullSigMask()
{
#ifndef AFS_DARWIN80_ENV
    struct uthread *user_thread = (struct uthread *)get_bsdthread_info(current_act());
       
    /* Protect original sigmask */
    if (!user_thread->uu_oldmask) {
	/* Back up current sigmask */
	user_thread->uu_oldmask = user_thread->uu_sigmask;
	/* Mask all signals */
	user_thread->uu_sigmask = ~(sigset_t)0;
    }
#endif
}

void 
afs_osi_fullSigRestore()
{
#ifndef AFS_DARWIN80_ENV
    struct uthread *user_thread = (struct uthread *)get_bsdthread_info(current_act());
       
    /* Protect original sigmask */
    if (user_thread->uu_oldmask) {
	/* Restore original sigmask */
	user_thread->uu_sigmask = user_thread->uu_oldmask;
	/* Clear the oldmask */
	user_thread->uu_oldmask = (sigset_t)0;
    }
#endif
}

int
afs_osi_SleepSig(void *event)
{
    afs_osi_Sleep(event);
    return 0;
}

/* osi_TimedSleep
 * 
 * Arguments:
 * event - event to sleep on
 * ams --- max sleep time in milliseconds
 * aintok - 1 if should sleep interruptibly
 *
 * Returns 0 if timeout and EINTR if signalled.
 */
int
osi_TimedSleep(char *event, afs_int32 ams, int aintok)
{
    int code = 0;
    struct afs_event *evp;
    int seq;
    int prio;
#ifdef AFS_DARWIN80_ENV
    struct timespec ts;
#else
    int ticks;
#endif



    evp = afs_getevent(event);
    seq = evp->seq;
    AFS_GUNLOCK();
#ifdef AFS_DARWIN80_ENV
    if (aintok)
        prio = PCATCH | PPAUSE;
    else
        prio = PVFS;
    ts.tv_sec = ams / 1000;
    ts.tv_nsec = (ams % 1000) * 1000000;
    code = msleep(event, NULL, prio, "afs_osi_TimedSleep", &ts);
#else
    ticks = (ams * afs_hz) / 1000;
#ifdef AFS_DARWIN14_ENV
    /* this is probably safe for all versions, but testing is hard. */
    /* using tsleep instead of assert_wait/thread_set_timer/thread_block
     * allows shutdown to work in 1.4 */
    /* lack of PCATCH does *not* prevent signal delivery, neither does 
     * a low priority. We would need to deal with ERESTART here if we 
     * wanted to mess with p->p_sigmask, and messing with p_sigignore is
     * not the way to go.... (someone correct me if I'm wrong)
     */
    if (aintok)
	prio = PCATCH | PPAUSE;
    else
	prio = PVFS;
    code = tsleep(event, prio, "afs_osi_TimedSleep", ticks);
#else
    assert_wait((event_t) event, aintok ? THREAD_ABORTSAFE : THREAD_UNINT);
    thread_set_timer(ticks, NSEC_PER_SEC / hz);
    thread_block(0);
    code = 0;
#endif
#endif
    AFS_GLOCK();
    if (seq == evp->seq)
	code = EINTR;

    relevent(evp);
    return code;
}


int
afs_osi_Wakeup(void *event)
{
    struct afs_event *evp;
    int ret = 1;

    evp = afs_getevent(event);
    if (evp->refcount > 1) {
	evp->seq++;
#ifdef AFS_DARWIN14_ENV
	/* this is probably safe for all versions, but testing is hard. */
	wakeup(event);
#else
	thread_wakeup((event_t) event);
#endif
	ret = 0;
    }
    relevent(evp);
    return ret;
}
