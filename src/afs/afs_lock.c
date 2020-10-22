/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/


/*
	Locking routines for Vice.

*/

#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "afs/afs_stats.h"	/* afs statistics */

/* probably needed if lock_trace is enabled - should ifdef */
int afs_trclock = 0;

void Lock_ReleaseR(struct afs_lock *lock);
void Lock_ReleaseW(struct afs_lock *lock);

void
Lock_Init(struct afs_lock *lock)
{

    AFS_STATCNT(Lock_Init);
    lock->readers_reading = 0;
    lock->excl_locked = 0;
    lock->wait_states = 0;
    lock->num_waiting = 0;
#if defined(INSTRUMENT_LOCKS)
    lock->pid_last_reader = 0;
    lock->pid_writer = 0;
    lock->src_indicator = 0;
#endif /* INSTRUMENT_LOCKS */
    lock->time_waiting.tv_sec = 0;
    lock->time_waiting.tv_usec = 0;
}

void
Afs_Lock_Obtain(struct afs_lock *lock, int how)
{
    osi_timeval32_t tt1, tt2, et;
    afs_uint32 us;

    AFS_STATCNT(Lock_Obtain);

    AFS_ASSERT_GLOCK();
    osi_GetTime(&tt1);

    switch (how) {

    case READ_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= READ_LOCK;
	    afs_osi_Sleep(&lock->readers_reading);
	} while (lock->excl_locked & WRITE_LOCK);
	lock->num_waiting--;
	lock->readers_reading++;
	break;

    case WRITE_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= WRITE_LOCK;
	    afs_osi_Sleep(&lock->excl_locked);
	} while (lock->excl_locked || lock->readers_reading);
	lock->num_waiting--;
	lock->excl_locked = WRITE_LOCK;
	break;

    case SHARED_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= SHARED_LOCK;
	    afs_osi_Sleep(&lock->excl_locked);
	} while (lock->excl_locked);
	lock->num_waiting--;
	lock->excl_locked = SHARED_LOCK;
	break;

    case BOOSTED_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= WRITE_LOCK;
	    afs_osi_Sleep(&lock->excl_locked);
	} while (lock->readers_reading);
	lock->num_waiting--;
	lock->excl_locked = WRITE_LOCK;
	break;

    default:
	osi_Panic("afs locktype");
    }

    osi_GetTime(&tt2);
    afs_stats_GetDiff(et, tt1, tt2);
    afs_stats_AddTo((lock->time_waiting), et);
    us = (et.tv_sec << 20) + et.tv_usec;

    if (afs_trclock) {
	afs_Trace3(afs_iclSetp, CM_TRACE_LOCKSLEPT, ICL_TYPE_INT32, us,
		   ICL_TYPE_POINTER, lock, ICL_TYPE_INT32, how);
    }
}

/* release a lock, giving preference to new readers */
void
Afs_Lock_ReleaseR(struct afs_lock *lock)
{
    AFS_STATCNT(Lock_ReleaseR);
    AFS_ASSERT_GLOCK();
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
	afs_osi_Wakeup(&lock->readers_reading);
    } else {
	lock->wait_states &= ~EXCL_LOCKS;
	afs_osi_Wakeup(&lock->excl_locked);
    }
}

/* release a lock, giving preference to new writers */
void
Afs_Lock_ReleaseW(struct afs_lock *lock)
{
    AFS_STATCNT(Lock_ReleaseW);
    AFS_ASSERT_GLOCK();
    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
	afs_osi_Wakeup(&lock->excl_locked);
    } else {
	lock->wait_states &= ~READ_LOCK;
	afs_osi_Wakeup(&lock->readers_reading);
    }
}


/* Not static - used conditionally if lock tracing is enabled */
int
Afs_Lock_Trace(int op, struct afs_lock *alock, int type, char *file, int line)
{
    int traceok;
    struct afs_icl_log *tlp;
    struct afs_icl_set *tsp;

    if (!afs_trclock)
	return 1;
    if ((alock) == &afs_icl_lock)
	return 1;

    ObtainReadLock(&afs_icl_lock);
    traceok = 1;
    for (tlp = afs_icl_allLogs; tlp; tlp = tlp->nextp)
	if ((alock) == &tlp->lock)
	    traceok = 0;
    for (tsp = afs_icl_allSets; tsp; tsp = tsp->nextp)
	if ((alock) == &tsp->lock)
	    traceok = 0;
    ReleaseReadLock(&afs_icl_lock);
    if (!traceok)
	return 1;

    afs_Trace4(afs_iclSetp, op, ICL_TYPE_STRING, (long)file, ICL_TYPE_INT32,
	       (long)line, ICL_TYPE_POINTER, (long)alock, ICL_TYPE_LONG,
	       (long)type);
    return 0;
}
