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

    AFS_STATCNT(osi_Wait);
    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) curthreadp;
    do {
	AFS_ASSERT_GLOCK();
	code = 0;
	code = afs_osi_TimedSleep(&waitV, ams, aintok);

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
    kcondvar_t cond;		/* Currently associated condition variable */
} afs_event_t;

#define HASHSIZE 128
afs_event_t *afs_evhasht[HASHSIZE];	/* Hash table for events */
#if (_MIPS_SZPTR == 64)
#define afs_evhash(event)	(afs_uint32) ((((long)event)>>3) & (HASHSIZE-1));
#else
#define afs_evhash(event)	(afs_uint32) ((((long)event)>>2) & (HASHSIZE-1));
#endif
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
	cv_init(&newp->cond, "event cond var", CV_DEFAULT, NULL);
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
	cv_wait(&evp->cond, &afs_global_lock);
    }
    relevent(evp);
}

int
afs_osi_SleepSig(void *event)
{
    afs_osi_Sleep(event);
    return 0;
}

/* afs_osi_TimedSleep
 * 
 * Arguments:
 * event - event to sleep on
 * ams --- max sleep time in milliseconds
 * aintok - 1 if should sleep interruptibly
 *
 * Returns 0 if timeout and EINTR if signalled.
 */
int
afs_osi_TimedSleep(void *event, afs_int32 ams, int aintok)
{
    int code = 0;
    struct afs_event *evp;
    struct timespec ticks;

    ticks.tv_sec = ams / 1000;
    ticks.tv_nsec = (ams - (ticks.tv_sec * 1000)) * 1000000;

    evp = afs_getevent(event);

    AFS_ASSERT_GLOCK();
    if (aintok) {
	if (sv_timedwait_sig
	    (&evp->cond, AFSD_PRI(), &afs_global_lock, 0, 0, &ticks,
	     (struct timespec *)0))
	    code = EINTR;
	AFS_MUTEX_ENTER(&afs_global_lock);
    } else {
	cv_timedwait(&evp->cond, &afs_global_lock, ticks);
    }

    relevent(evp);
    return code;
}


int
afs_osi_Wakeup(void *event)
{
    int ret = 1;
    struct afs_event *evp;

    evp = afs_getevent(event);
    if (evp->refcount > 1) {
	evp->seq++;
	cv_broadcast(&evp->cond);
	ret = 0;
    }
    relevent(evp);
    return ret;
}
