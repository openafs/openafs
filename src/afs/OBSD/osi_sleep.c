/*
 * $Id$
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

static char waitV;


time_t
osi_Time()
{
    struct timeval now;

    getmicrotime(&now);
    return now.tv_sec;
}

void
afs_osi_SetTime(osi_timeval_t * atv)
{
    printf("afs attempted to set clock; use \"afsd -nosettime\"\n");
}

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
    struct timeval atv, now, endTime;

    AFS_STATCNT(osi_Wait);

    atv.tv_sec = ams / 1000;
    atv.tv_usec = (ams % 1000) * 1000;
    getmicrotime(&now);
    timeradd(&atv, &now, &endTime);

    if (ahandle)
	ahandle->proc = (caddr_t) curproc;
    AFS_ASSERT_GLOCK();
    AFS_GUNLOCK();

    do {
	timersub(&endTime, &now, &atv);
	timo = atv.tv_sec * hz + atv.tv_usec * hz / 1000000 + 1;
	if (aintok) {
	    code = tsleep(&waitV, PCATCH | PVFS, "afs_W1", timo);
	    if (code)
		code = (code == EWOULDBLOCK) ? 0 : EINTR;
	} else
	    tsleep(&waitV, PVFS, "afs_W2", timo);

	/* if we were cancelled, quit now */
	if (ahandle && (ahandle->proc == NULL)) {
	    /* we've been signalled */
	    break;
	}
	getmicrotime(&now);
    } while (timercmp(&now, &endTime, <));

    AFS_GLOCK();
    return code;
}

/*
 * All this gluck should probably also be replaced with CVs.
 */
typedef struct afs_event {
    struct afs_event *next;     /* next in hash chain */
    char *event;                /* lwp event: an address */
    int refcount;               /* Is it in use? */
    int seq;                    /* Sequence number: this is incremented
                                 * by wakeup calls; wait will not return until
                                 * it changes */
    int cond;
} afs_event_t;

#define HASHSIZE 128
afs_event_t *afs_evhasht[HASHSIZE];     /* Hash table for events */
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

int
afs_osi_TimedSleep(void *event, afs_int32 ams, int aintok)
{
    int code = 0;
    struct afs_event *evp;
    int seq, prio;
    int ticks;

    evp = afs_getevent(event);
    seq = evp->seq;
    AFS_GUNLOCK();
    if (aintok)
	prio = PCATCH | PPAUSE;
    else
	prio = PVFS;
    ticks = (ams * afs_hz) / 1000;
    code = tsleep(event, prio, "afs_osi_TimedSleep", ticks);
    if (seq == evp->seq)
	code = EINTR;
    relevent(evp);
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
