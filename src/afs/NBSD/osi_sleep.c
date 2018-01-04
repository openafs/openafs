/*
 * $Id: osi_sleep.c,v 1.9 2005/07/26 15:25:43 rees Exp $
 */

/*
copyright 2002
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works
and redistribute this software and such derivative works
for any purpose, so long as the name of the university of
michigan is not used in any advertising or publicity
pertaining to the use or distribution of this software
without specific, written prior authorization.  if the
above copyright notice or any other identification of the
university of michigan is included in any copy of any
portion of this software, then the disclaimer below must
also be included.

this software is provided as is, without representation
from the university of michigan as to its fitness for any
purpose, and without warranty by the university of
michigan of any kind, either express or implied, including
without limitation the implied warranties of
merchantability and fitness for a particular purpose. the
regents of the university of michigan shall not be liable
for any damages, including special, indirect, incidental, or
consequential damages, with respect to any claim arising
out of or in connection with the use of the software, even
if it has been or is hereafter advised of the possibility of
such damages.
*/

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
#include "afs/afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

#if !defined(AFS_NBSD50_ENV)
static char waitV;

/* cancel osi_Wait */
void
afs_osi_CancelWait(struct afs_osi_WaitHandle *achandle)
{
    caddr_t proc;

    AFS_STATCNT(osi_CancelWait);
    proc = achandle->proc;
    if (proc == NULL)
	return;
    achandle->proc = NULL;
    wakeup(&waitV);
}

/* afs_osi_Wait
 * Waits for data on ahandle, or ams ms later.  ahandle may be null.
 * Returns 0 if timeout and EINTR if signalled.
 */
int
afs_osi_Wait(afs_int32 ams, struct afs_osi_WaitHandle *ahandle, int aintok)
{
    int timo, code = 0;
    struct timeval atv, time_now, endTime;
    const struct timeval timezero = { 0, 0 };

    AFS_STATCNT(osi_Wait);

    atv.tv_sec = ams / 1000;
    atv.tv_usec = (ams % 1000) * 1000;
    getmicrouptime(&time_now);
    timeradd(&atv, &time_now, &endTime);

    if (ahandle)
	ahandle->proc = (caddr_t) osi_curproc();
    AFS_ASSERT_GLOCK();
    AFS_GUNLOCK();

    do {
	timersub(&endTime, &time_now, &atv);
	if (timercmp(&atv, &timezero, <))
	    break;
	timo = tvtohz(&atv);
	if (aintok) {
	    code = tsleep(&waitV, PCATCH | PVFS, "afs_W1", timo);
	} else {
	    code = tsleep(&waitV, PVFS, "afs_W2", timo);
	}
	if (code)
	    code = (code == EWOULDBLOCK) ? 0 : EINTR;

        getmicrouptime(&time_now);

	/* if we were cancelled, quit now */
	if (ahandle && (ahandle->proc == NULL)) {
	    /* we've been signalled */
	    break;
	}
    } while (timercmp(&time_now, &endTime, <));

    AFS_GLOCK();

    return code;
}

void
afs_osi_Sleep(void *event)
{
    AFS_ASSERT_GLOCK();
    AFS_GUNLOCK();
    tsleep(event, PVFS, "afsslp", 0);
    AFS_GLOCK();
}

int
afs_osi_SleepSig(void *event)
{
    afs_osi_Sleep(event);
    return 0;
}

int
afs_osi_Wakeup(void *event)
{
    wakeup(event);
    return 1;
}
#else
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
    afs_int32 endTime;

    AFS_STATCNT(osi_Wait);
    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) osi_curproc();
    do {
	AFS_ASSERT_GLOCK();
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
    if (!newp) {
	newp = osi_AllocSmallSpace(sizeof(afs_event_t));
	afs_evhashcnt++;
	newp->next = afs_evhasht[hashcode];
	afs_evhasht[hashcode] = newp;
	cv_init(&newp->cond, "afsevent");
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
	cv_wait(&evp->cond, &afs_global_mtx);
    }
    relevent(evp);
}

int
afs_osi_SleepSig(void *event)
{
    struct afs_event *evp;
    int seq, code = 0;

    evp = afs_getevent(event);
    seq = evp->seq;
    while (seq == evp->seq) {
	AFS_ASSERT_GLOCK();
	code = cv_wait_sig(&evp->cond, &afs_global_mtx);
	if (code) {
	    code = (code == EWOULDBLOCK) ? 0 : EINTR;
	    break;
	}
    }
    relevent(evp);
    return code;
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
    int code;
    struct afs_event *evp;
    int ticks;

    ticks = mstohz(ams);
    ticks = ticks ? ticks : 1;
    evp = afs_getevent(event);

    AFS_ASSERT_GLOCK();
    if (aintok) {
	code = cv_timedwait_sig(&evp->cond, &afs_global_mtx, ticks);
    } else {
	code = cv_timedwait(&evp->cond, &afs_global_mtx, ticks);
    }

    switch (code) {
    default:
	code = EINTR;
	break;
    case EWOULDBLOCK:
	code = 0;
	break;
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
    return 0;
}
#endif
