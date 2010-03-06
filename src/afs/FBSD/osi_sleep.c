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

#ifndef AFS_FBSD50_ENV
static char waitV;
#endif


void
afs_osi_InitWaitHandle(struct afs_osi_WaitHandle *achandle)
{
    AFS_STATCNT(osi_InitWaitHandle);
#ifdef AFS_FBSD50_ENV
    cv_init(&achandle->wh_condvar, "afscondvar");
    achandle->wh_inited = 1;
#else
    achandle->proc = NULL;
#endif
}

/* cancel osi_Wait */
/* XXX
 * I can't tell -- is this supposed to be cv_signal() or cv_waitq_remove()?
 * Or perhaps cv_broadcast()?
 * Assuming cv_signal() is the desired meaning.  -GAW
 */
void
afs_osi_CancelWait(struct afs_osi_WaitHandle *achandle)
{
#ifndef AFS_FBSD50_ENV
    caddr_t proc;
#endif

    AFS_STATCNT(osi_CancelWait);

#ifdef AFS_FBSD50_ENV
    /* XXX should not be necessary */
    if (!achandle->wh_inited)
	return;
    AFS_ASSERT_GLOCK();
    cv_signal(&achandle->wh_condvar);
#else
    proc = achandle->proc;
    if (proc == 0)
	return;
    achandle->proc = NULL;	/* so dude can figure out he was signalled */
    afs_osi_Wakeup(&waitV);
#endif
}

/* afs_osi_Wait
 * Waits for data on ahandle, or ams ms later.  ahandle may be null.
 * Returns 0 if timeout and EINTR if signalled.
 */
int
afs_osi_Wait(afs_int32 ams, struct afs_osi_WaitHandle *ahandle, int aintok)
{
    int code;
#ifdef AFS_FBSD50_ENV
    struct timeval tv;
    int ticks;
#else
    afs_int32 endTime;
#endif

    AFS_STATCNT(osi_Wait);
#ifdef AFS_FBSD50_ENV
    tv.tv_sec = ams / 1000;
    tv.tv_usec = (ams % 1000) * 1000;
    ticks = tvtohz(&tv);

    AFS_ASSERT_GLOCK();
    if (ahandle == NULL) {
	/* This is nasty and evil and rude. */
	afs_global_owner = 0;
	code = msleep(&tv, &afs_global_mtx, (aintok ? PPAUSE|PCATCH : PVFS),
	    "afswait", ticks);
	afs_global_owner = curthread;
    } else {
	if (!ahandle->wh_inited)
	    afs_osi_InitWaitHandle(ahandle);	/* XXX should not be needed */

	if (aintok)
	    code = cv_timedwait_sig(&ahandle->wh_condvar, &afs_global_mtx,
		ticks);
	else
	    code = cv_timedwait(&ahandle->wh_condvar, &afs_global_mtx, ticks);
    }
#else
    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) curproc;
    do {
	AFS_ASSERT_GLOCK();
	code = afs_osi_TimedSleep(&waitV, ams, aintok);
	if (code)
	    break;		/* if something happened, quit now */
	/* if we we're cancelled, quit now */
	if (ahandle && (ahandle->proc == NULL)) {
	    /* we've been signalled */
	    break;
	}
    } while (osi_Time() < endTime);
#endif
    return code;
}

/*
 * All this gluck should probably also be replaced with CVs.
 */
typedef struct afs_event {
    struct afs_event *next;	/* next in hash chain */
    char *event;		/* lwp event: an address */
    int refcount;		/* Is it in use? */
    int seq;			/* Sequence number: this is incremented
				 * by wakeup calls; wait will not return until
				 * it changes */
#ifdef AFS_FBSD50_ENV
    struct mtx *lck;
    struct thread *owner;
#else
    int cond;
#endif
} afs_event_t;

#define HASHSIZE 128
afs_event_t *afs_evhasht[HASHSIZE];	/* Hash table for events */
#define afs_evhash(event)	(afs_uint32) ((((long)event)>>2) & (HASHSIZE-1));
int afs_evhashcnt = 0;

#ifdef AFS_FBSD50_ENV
#define EVTLOCK_INIT(e) \
    do { \
        mtx_init((e)->lck, "event lock", NULL, MTX_DEF); \
        (e)->owner = 0; \
    } while (0)
#define EVTLOCK_LOCK(e) \
    do { \
        osi_Assert((e)->owner != curthread); \
        mtx_lock((e)->lck);		     \
        osi_Assert((e)->owner == 0); \
        (e)->owner = curthread; \
    } while (0)
#define EVTLOCK_UNLOCK(e) \
    do { \
        osi_Assert((e)->owner == curthread); \
        (e)->owner = 0; \
        mtx_unlock((e)->lck); \
    } while (0)
#define EVTLOCK_DESTROY(e) mtx_destroy((e)->lck)
#else
#define EVTLOCK_INIT(e)
#define EVTLOCK_LOCK(e)
#define EVTLOCK_UNLOCK(e)
#define EVTLOCK_DESTROY(e)
#endif

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
	EVTLOCK_LOCK(evp);
	if (evp->event == event) {
	    evp->refcount++;
	    return evp;
	}
	if (evp->refcount == 0)
	    newp = evp;
	EVTLOCK_UNLOCK(evp);
	evp = evp->next;
    }
    if (!newp) {
	newp = (afs_event_t *) osi_AllocSmallSpace(sizeof(afs_event_t));
	afs_evhashcnt++;
	newp->next = afs_evhasht[hashcode];
	afs_evhasht[hashcode] = newp;
	newp->seq = 0;
	EVTLOCK_INIT(newp);
    }
    EVTLOCK_LOCK(newp);
    newp->event = event;
    newp->refcount = 1;
    return newp;
}

/* Release the specified event */
#ifdef AFS_FBSD50_ENV
#define relevent(evp) \
    do { \
        osi_Assert((evp)->owner == curthread); \
        (evp)->refcount--; \
        (evp)->owner = 0; \
        mtx_unlock((evp)->lck); \
    } while (0)
#else
#define relevent(evp) ((evp)->refcount--)
#endif

void
afs_osi_Sleep(void *event)
{
    struct afs_event *evp;
    int seq;

    evp = afs_getevent(event);
    seq = evp->seq;
    while (seq == evp->seq) {
	AFS_ASSERT_GLOCK();
#ifdef AFS_FBSD50_ENV
	evp->owner = 0;
	msleep(event, &afs_global_mtx, PVFS, "afsslp", 0);
	evp->owner = curthread;
#else
	AFS_GUNLOCK();
	tsleep(event, PVFS, "afs_osi_Sleep", 0);
	AFS_GLOCK();
#endif
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
    int seq, prio;

    evp = afs_getevent(event);
    seq = evp->seq;
    AFS_GUNLOCK();
    if (aintok)
	prio = PCATCH | PPAUSE;
    else
	prio = PVFS;
    evp->owner = 0;
    code = msleep(event, evp->lck, prio, "afsslp", (ams * hz) / 1000);
    evp->owner = curthread;
    if (seq == evp->seq)
	code = EINTR;
    relevent(evp);
    AFS_GLOCK();
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
	wakeup(event);
	ret = 0;
    }
    relevent(evp);
    return ret;
}

void
shutdown_osisleep(void) {
    struct afs_event *evp, *nevp, **pevpp;
    int i;
    for (i=0; i < HASHSIZE; i++) {
        evp = afs_evhasht[i];
        pevpp = &afs_evhasht[i];
        while (evp) {
            EVTLOCK_LOCK(evp);
            nevp = evp->next;
            if (evp->refcount == 0) {
                EVTLOCK_DESTROY(evp);
                *pevpp = evp->next;
                osi_FreeSmallSpace(evp);
                afs_evhashcnt--;
            } else {
                EVTLOCK_UNLOCK(evp);
                pevpp = &evp->next;
            }
            evp = nevp;
        }
    }
}
