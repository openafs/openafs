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

#if !defined(AFS_HPUX110_ENV)
static char waitV;
#endif

/* call procedure aproc with arock as an argument, in ams milliseconds */
static int
afs_osi_CallProc(aproc, arock, ams)
     void (*aproc) ();
     char *arock;
     afs_int32 ams;
{
    int code;

    AFS_STATCNT(osi_CallProc);
#if !defined(AFS_HPUX110_ENV)
    AFS_GUNLOCK();
#endif
    /* hz is in cycles/second, and timeout's 3rd parm is in cycles */
    code = timeout(aproc, arock, (ams * afs_hz) / 1000 + 1);
#if !defined(AFS_HPUX110_ENV)
    AFS_GLOCK();
#endif
    return code;
}

/* cancel a timeout, whether or not it has already occurred */
static int
afs_osi_CancelProc(aproc, arock)
     void (*aproc) ();
     char *arock;
{
    int code = 0;
    AFS_STATCNT(osi_CancelProc);

#if !defined(AFS_HPUX110_ENV)
    AFS_GUNLOCK();
#endif
    code = untimeout(aproc, arock);
#if !defined(AFS_HPUX110_ENV)
    AFS_GLOCK();
#endif
    return code;
}

#if defined(AFS_HPUX110_ENV)
static void
AfsWaitHack(char *event)
{
    lock_t *sleep_lock;

    AFS_STATCNT(WaitHack);
    sleep_lock = get_sleep_lock(event);
    wakeup(event);
    spinunlock(sleep_lock);
}
#else

static void
AfsWaitHack()
{
    AFS_STATCNT(WaitHack);
    wakeup(&waitV);
}
#endif

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
#if defined(AFS_HPUX110_ENV)
    afs_osi_Wakeup((char *)achandle);
#else
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
    afs_int32 endTime, tid;
#if defined(AFS_HPUX110_ENV)
    char localwait;
    char *event;
#endif

    AFS_STATCNT(osi_Wait);
    endTime = osi_Time() + (ams / 1000);
    if (ahandle)
	ahandle->proc = (caddr_t) u.u_procp;
    do {
	AFS_ASSERT_GLOCK();
	code = 0;
	/* do not do anything for solaris, digital, AIX, and SGI MP */
#if defined(AFS_HPUX110_ENV)
	if (ahandle) {
	    event = (char *)ahandle;
	} else {
	    event = &localwait;
	}
	afs_osi_CallProc(AfsWaitHack, event, ams);
	afs_osi_Sleep(event);
	afs_osi_CancelProc(AfsWaitHack, event);
#else
	afs_osi_CallProc(AfsWaitHack, (char *)u.u_procp, ams);
	afs_osi_Sleep(&waitV);	/* for HP 10.0 */

	/* do not do anything for solaris, digital, and SGI MP */
	afs_osi_CancelProc(AfsWaitHack, (char *)u.u_procp);
	if (code)
	    break;		/* if something happened, quit now */
#endif
	/* if we we're cancelled, quit now */
	if (ahandle && (ahandle->proc == (caddr_t) 0)) {
	    /* we've been signalled */
	    break;
	}
    } while (osi_Time() < endTime);
    return code;
}

int
afs_osi_SleepSig(void *event)
{
    afs_osi_Sleep(event);
    return 0;
}

#if defined(AFS_HPUX110_ENV)
int
afs_osi_TimedSleep(void *event, afs_int32 ams, int aintok)
{
    lock_t *sleep_lock;
    int intr = EWOULDBLOCK;

    AFS_ASSERT_GLOCK();
    AFS_GUNLOCK();
    afs_osi_CallProc(AfsWaitHack, event, ams);
    sleep((caddr_t) event, PZERO - 2);
    if (afs_osi_CancelProc(AfsWaitHack, event) < 0)
	intr = 0;
    AFS_GLOCK();
    return intr;
}

void
afs_osi_Sleep(void *event)
{
    lock_t *sleep_lock;

    AFS_ASSERT_GLOCK();
    get_sleep_lock(event);
    AFS_GUNLOCK();
    sleep((caddr_t) event, PZERO - 2);
    AFS_GLOCK();
}

int
afs_osi_Wakeup(void *event)
{
    lock_t *sleep_lock;

    sleep_lock = get_sleep_lock(event);
    wakeup((caddr_t) event);
    spinunlock(sleep_lock);
    return 0;
}
#else
int
afs_osi_Wakeup(void *event)
{
    wakeup((caddr_t) event);
    return 0;
}
#endif
