/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include "osi.h"
#include <stdlib.h>
#include <assert.h>

/* locking hierarchy:
 * 1. osi_statFDCS
 * 2. osi_activeInfoAllocCS.
 * 3. individual log critical sections (since we call log pkg).
 *
 * There are no cases today where both 1 and 2 are locked simultaneously.
 */

/* el cheapo watch system */
osi_watchProc_t *osi_statWatchProcp;	/* proc to call on too-long held locks */
void *osi_statWatchRockp;		/* with this rock */
unsigned long osi_statWatchMS;		/* after a lock is held this many MS */

/* type index of the statistics gathering lock package */
int osi_statType;

/* log to which to log lock events */
osi_log_t *osi_statLogp;

/* queue of all rwlock auxiliary structures */
osi_queue_t *osi_allRWLocks;

/* queue of all mutex auxiliary structures */
osi_queue_t *osi_allMutexes;

/* free list and mutex for active info structures */
osi_activeInfo_t *osi_activeInfoFreeListp;
CRITICAL_SECTION osi_activeInfoAllocCS;

/* atomicity-providing critical sections */
CRITICAL_SECTION osi_statAtomicCS[OSI_MUTEXHASHSIZE];

/* lock protecting ref count on locks, osi_allMutexes and osi_RWLocks, and
 * file descriptor contents
 */
CRITICAL_SECTION osi_statFDCS;

void osi_SetStatLog(osi_log_t *logp)
{
	/* nicer if ref counted */
	osi_statLogp = logp;
}

static void lock_ObtainWriteStat(osi_rwlock_t *lockp)
{
	osi_rwlockStat_t *realp;
	osi_activeInfo_t *ap;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *)lockp->d.privateDatap;
	ap = NULL;

	csp = &osi_statAtomicCS[lockp->atomicIndex];
	EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if ((lockp->waiters > 0) || (lockp->flags & OSI_LOCKFLAG_EXCL)
		|| (lockp->readers > 0)) {
		lockp->waiters++;
		if (!ap) ap = osi_QueueActiveInfo(&realp->qi,
			OSI_ACTIVEFLAGS_WRITER | OSI_ACTIVEFLAGS_WAITER);
                osi_TWait(&realp->turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, csp);
		lockp->waiters--;
		osi_assert((lockp->flags & OSI_LOCKFLAG_EXCL) && lockp->readers == 0);
	}
        else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
        }

	/* if we have ap set, we have some timer info about the last sleep operation
	 * that we should merge in under the spin lock.
	 */
	if (ap) {
		/* remove from queue and turn time to incremental time */
		osi_RemoveActiveInfo(&realp->qi, ap);
		
		/* add in increment to statistics */
		realp->writeBlockedCount++;
		realp->writeBlockedTime = LargeIntegerAdd(realp->writeBlockedTime,
			ap->startTime);
		osi_FreeActiveInfo(ap);
	}

	osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_WRITER);
	LeaveCriticalSection(csp);
}

static void lock_ObtainReadStat(osi_rwlock_t *lockp)
{
	osi_activeInfo_t *ap;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	ap = NULL;
	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
		lockp->waiters++;
		if (!ap) ap = osi_QueueActiveInfo(&realp->qi,
			OSI_ACTIVEFLAGS_WAITER | OSI_ACTIVEFLAGS_READER);
		osi_TWait(&realp->turn, OSI_SLEEPINFO_W4READ, &lockp->readers, csp);
                lockp->waiters--;
                osi_assert(!(lockp->flags & OSI_LOCKFLAG_EXCL) && lockp->readers > 0);
	}
        else {
		/* if we're here, all clear to set the lock */
		lockp->readers++;
        }

	if (ap) {
		/* have statistics to merge in */
		osi_RemoveActiveInfo(&realp->qi, ap);
		realp->readBlockedCount++;
		realp->readBlockedTime = LargeIntegerAdd(realp->readBlockedTime, ap->startTime);
		osi_FreeActiveInfo(ap);
	}

	osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_READER);
	LeaveCriticalSection(csp);
}

static void lock_ReleaseReadStat(osi_rwlock_t *lockp)
{
	osi_activeInfo_t *ap;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *)lockp->d.privateDatap;

	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->readers > 0);
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap != NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->readLockedCount++;
	realp->readLockedTime = LargeIntegerAdd(realp->readLockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);
	
	if (--lockp->readers == 0 && !osi_TEmpty(&realp->turn)) {
        	osi_TSignalForMLs(&realp->turn, 0, csp);
	}
	else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

static void lock_ConvertWToRStat(osi_rwlock_t *lockp)
{
	osi_activeInfo_t *ap;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *)lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap !=NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->writeLockedCount++;
	realp->writeLockedTime = LargeIntegerAdd(realp->writeLockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);
        
        /* and obtain the read lock */
	lockp->readers++;
	osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_READER);
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&realp->turn)) {
		osi_TSignalForMLs(&realp->turn, 1, csp);
	}
        else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

static void lock_ConvertRToWStat(osi_rwlock_t *lockp)
{
	osi_activeInfo_t *ap;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *)lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap !=NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
        realp->readLockedCount++;
        realp->readLockedTime = LargeIntegerAdd(realp->readLockedTime, ap->startTime);
        osi_FreeActiveInfo(ap);
        
        if (--lockp->readers == 0) {
            /* and obtain the write lock */
            lockp->readers--;
            lockp->flags |= OSI_LOCKFLAG_EXCL;
        } else {
            lockp->waiters++;
            ap = osi_QueueActiveInfo(&realp->qi,
                                     OSI_ACTIVEFLAGS_WRITER | OSI_ACTIVEFLAGS_WAITER);
            osi_TWait(&realp->turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, csp);
            lockp->waiters--;
            osi_assert((lockp->flags & OSI_LOCKFLAG_EXCL) && lockp->readers == 0);

            /*  we have some timer info about the last sleep operation
             * that we should merge in under the spin lock.
             */

            /* remove from queue and turn time to incremental time */
            osi_RemoveActiveInfo(&realp->qi, ap);
		
            /* add in increment to statistics */
            realp->writeBlockedCount++;
            realp->writeBlockedTime = LargeIntegerAdd(realp->writeBlockedTime,
                                                   ap->startTime);
            osi_FreeActiveInfo(ap);
        }

        osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_WRITER);
        LeaveCriticalSection(csp);
}

static void lock_ReleaseWriteStat(osi_rwlock_t *lockp)
{
	osi_activeInfo_t *ap;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *)lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap !=NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->writeLockedCount++;
	realp->writeLockedTime = LargeIntegerAdd(realp->writeLockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&realp->turn)) {
		osi_TSignalForMLs(&realp->turn, 0, csp);
	}
        else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

static void lock_ObtainMutexStat(struct osi_mutex *lockp)
{
	osi_activeInfo_t *ap;
	osi_mutexStat_t *realp;
        CRITICAL_SECTION *csp;

	ap = NULL;
	realp = (osi_mutexStat_t *) lockp->d.privateDatap;

	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
		lockp->waiters++;
		ap = osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_WAITER);
		osi_TWait(&realp->turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, csp);
		lockp->waiters--;
                osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	}
        else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
        }

	/* release the blocking ap */
	if (ap) {
		osi_RemoveActiveInfo(&realp->qi, ap);
		realp->blockedCount++;
		realp->blockedTime = LargeIntegerAdd(realp->blockedTime, ap->startTime);
		osi_FreeActiveInfo(ap);
	}

	/* start tracking this call */
	osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_WRITER);

	LeaveCriticalSection(csp);
}

static void lock_ReleaseMutexStat(struct osi_mutex *lockp)
{
	osi_activeInfo_t *ap;
	osi_mutexStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_mutexStat_t *)lockp->d.privateDatap;

	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap != NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->lockedCount++;
	realp->lockedTime = LargeIntegerAdd(realp->lockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);

	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&realp->turn)) {
        	osi_TSignalForMLs(&realp->turn, 0, csp);
	}
        else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

static int lock_TryReadStat(struct osi_rwlock *lockp)
{
	long i;
	osi_rwlockStat_t *realp; 
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
		i = 0;
	}
	else {
		/* if we're here, all clear to set the lock */
		lockp->readers++;
		i = 1;
	}

	/* start tracking lock holding stats */
	if (i) osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_READER);

	LeaveCriticalSection(csp);

	return i;
}


static int lock_TryWriteStat(struct osi_rwlock *lockp)
{
	long i;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if ((lockp->waiters > 0) || (lockp->flags & OSI_LOCKFLAG_EXCL)
		|| (lockp->readers > 0)) {
		i = 0;
	}
	else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
		i = 1;
	}

	/* start tracking lock holding stats */
	if (i) osi_QueueActiveInfo(&realp->qi, OSI_ACTIVEFLAGS_WRITER);

	LeaveCriticalSection(csp);

	return i;
}


static int lock_GetRWLockStateStat(struct osi_rwlock *lockp)
{
	long i;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	i = 0;
	if (lockp->flags & OSI_LOCKFLAG_EXCL) {
		i |= OSI_RWLOCK_WRITEHELD;
	}
	if (lockp->readers) {
		i |= OSI_RWLOCK_READHELD;
	}

	LeaveCriticalSection(csp);

	return i;
}


static int lock_GetMutexStateStat(struct osi_mutex *lockp) {
	long i;
	osi_mutexStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_mutexStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	i = 0;
	if (lockp->flags & OSI_LOCKFLAG_EXCL) {
		i |= OSI_MUTEX_HELD;
	}

	LeaveCriticalSection(csp);

	return i;
}

static int lock_TryMutexStat(struct osi_mutex *lockp) {
	long i;
	osi_mutexStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_mutexStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if ((lockp->waiters > 0) || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
		i = 0;
	}
	else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
		i = 1;
	}

	if (i) osi_QueueActiveInfo(&realp->qi, 0);

	LeaveCriticalSection(csp);

	return i;
}

static void osi_SleepRStat(LONG_PTR sleepVal, struct osi_rwlock *lockp)
{
	osi_rwlockStat_t *realp;
	osi_activeInfo_t *ap;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->readers > 0);
	
	if (--lockp->readers == 0 && !osi_TEmpty(&realp->turn)) {
        	osi_TSignalForMLs(&realp->turn, 0, NULL);
	}

	/* now merge in lock hold stats */
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap != NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->writeLockedCount++;
	realp->writeLockedTime = LargeIntegerAdd(realp->writeLockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);

	/* now call into scheduler to sleep atomically with releasing spin lock */
	osi_SleepSpin(sleepVal, csp);
}

static void osi_SleepWStat(LONG_PTR sleepVal, struct osi_rwlock *lockp)
{
	osi_activeInfo_t *ap;
	osi_rwlockStat_t *realp;
        CRITICAL_SECTION *csp;

	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&realp->turn)) {
        	osi_TSignalForMLs(&realp->turn, 0, NULL);
	}

	/* now merge in lock hold stats */
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap != NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->readLockedCount++;
	realp->readLockedTime = LargeIntegerAdd(realp->readLockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);

	/* and finally release the big lock */
	osi_SleepSpin(sleepVal, csp);
}

static void osi_SleepMStat(LONG_PTR sleepVal, struct osi_mutex *lockp)
{
	osi_mutexStat_t *realp;
	osi_activeInfo_t *ap;
        CRITICAL_SECTION *csp;

	realp = (osi_mutexStat_t *) lockp->d.privateDatap;

	/* otherwise we're the fast base type */
	csp = &osi_statAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&realp->turn)) {
		osi_TSignalForMLs(&realp->turn, 0, NULL);
	}

	/* now merge in lock hold stats */
	ap = osi_FindActiveInfo(&realp->qi);
	osi_assert(ap != NULL);
	osi_RemoveActiveInfo(&realp->qi, ap);
	realp->lockedCount++;
	realp->lockedTime = LargeIntegerAdd(realp->lockedTime, ap->startTime);
	osi_FreeActiveInfo(ap);

	/* and finally release the big lock */
	osi_SleepSpin(sleepVal, csp);
}

/* this is a function that release a ref count, but we give it a different
 * name than release to avoid confusion with all of the releases here.
 * Must be called holding the osi_statFDCS lock.
 */
static void lock_DecrMutexStat(osi_mutexStat_t *mp)
{
	if (--mp->refCount <= 0 && (mp->states & OSI_STATL_DELETED)) {
		osi_QRemove(&osi_allMutexes, &mp->q);
		free((void *)mp);
	}
}

/* Must be called holding the osi_statFDCS lock. */
static void lock_DecrRWLockStat(osi_rwlockStat_t *rwp)
{
	if (--rwp->refCount <= 0 && (rwp->states & OSI_STATL_DELETED)) {
		osi_QRemove(&osi_allRWLocks, &rwp->q);
		free((void *)rwp);
	}
}

static void lock_FinalizeMutexStat(osi_mutex_t *lockp)
{
	osi_mutexStat_t *realp;
	
	/* pull out the real pointer */
	realp = (osi_mutexStat_t *) lockp->d.privateDatap;
	
	/* remove from the queues, and free */
	EnterCriticalSection(&osi_statFDCS);
	if (realp->refCount <= 0) {
		osi_QRemove(&osi_allMutexes, &realp->q);
		free((void *) realp);
	}
	else realp->states |= OSI_STATL_DELETED;
	LeaveCriticalSection(&osi_statFDCS);
}

static void lock_FinalizeRWLockStat(osi_rwlock_t *lockp)
{
	osi_rwlockStat_t *realp;
	
	/* pull out the real pointer */
	realp = (osi_rwlockStat_t *) lockp->d.privateDatap;
	
	/* remove from the queues, and free */
	EnterCriticalSection(&osi_statFDCS);
	if (realp->refCount <= 0) {
		osi_QRemove(&osi_allRWLocks, &realp->q);
		free((void *) realp);
	}
	else realp->states |= OSI_STATL_DELETED;
	LeaveCriticalSection(&osi_statFDCS);
}

void lock_InitializeRWLockStat(osi_rwlock_t *lockp, char *namep)
{
	osi_rwlockStat_t *realp;
	
	realp = (osi_rwlockStat_t *) malloc(sizeof(*realp));
	lockp->d.privateDatap = (void *) realp;
	lockp->type = osi_statType;
	lockp->flags = 0;
	lockp->readers = 0;
        lockp->atomicIndex = osi_MUTEXHASH(lockp);
	memset(realp, 0, sizeof(*realp));
        osi_TInit(&realp->turn);
	realp->qi.namep = namep;
	realp->qi.backp = lockp;
	EnterCriticalSection(&osi_statFDCS);
	osi_QAdd(&osi_allRWLocks, &realp->q);
	LeaveCriticalSection(&osi_statFDCS);
}

void lock_InitializeMutexStat(osi_mutex_t *lockp, char *namep)
{
	osi_mutexStat_t *realp;
	
	realp = (osi_mutexStat_t *) malloc(sizeof(*realp));
	lockp->d.privateDatap = (void *) realp;
	lockp->type = osi_statType;
	lockp->flags = 0;
        lockp->atomicIndex = osi_MUTEXHASH(lockp);
	memset(realp, 0, sizeof(*realp));
        osi_TInit(&realp->turn);
	realp->qi.namep = namep;
	realp->qi.backp = lockp;
	EnterCriticalSection(&osi_statFDCS);
	osi_QAdd(&osi_allMutexes, &realp->q);
	LeaveCriticalSection(&osi_statFDCS);
}

static void osi_FreeActiveInfo(osi_activeInfo_t *ap)
{
	EnterCriticalSection(&osi_activeInfoAllocCS);
	ap->q.nextp = (osi_queue_t *) osi_activeInfoFreeListp;
	osi_activeInfoFreeListp = ap;
	LeaveCriticalSection(&osi_activeInfoAllocCS);
}

static osi_activeInfo_t *osi_AllocActiveInfo()
{
	osi_activeInfo_t *ap;

	EnterCriticalSection(&osi_activeInfoAllocCS);
	if (!(ap = osi_activeInfoFreeListp)) {
		ap = (osi_activeInfo_t *) malloc(sizeof(osi_activeInfo_t));
	}
	else {
		osi_activeInfoFreeListp = (osi_activeInfo_t *) ap->q.nextp;
	}
	LeaveCriticalSection(&osi_activeInfoAllocCS);

	return ap;
}

static osi_activeInfo_t *osi_QueueActiveInfo(osi_qiStat_t *qp, int flags)
{
	osi_activeInfo_t *ap;
	osi_queue_t **qpp = (osi_queue_t **) &qp->activeListp;
        char *whatp;

	ap = osi_AllocActiveInfo();
	ap->flags = flags;
	ap->startTime.LowPart = GetCurrentTime();
	ap->startTime.HighPart = 0;
	ap->tid = GetCurrentThreadId();
	osi_QAdd(qpp, &ap->q);
        if (osi_statLogp && (flags & OSI_ACTIVEFLAGS_WAITER)) {
		if (flags & OSI_ACTIVEFLAGS_READER)
			whatp = "read lock";
		else if (flags & OSI_ACTIVEFLAGS_WRITER)
			whatp = "write lock";
		else whatp = "mutex";
        	osi_Log2(osi_statLogp, "Blocking on %s on %s", whatp, qp->namep);
	}
	return ap;
}

static void osi_RemoveActiveInfo(osi_qiStat_t *qp, osi_activeInfo_t *ap)
{
	unsigned long now;
	osi_queue_t **qpp = (osi_queue_t **) &qp->activeListp;
        long flags;
	char *whatp;

	now = GetCurrentTime();
	osi_QRemove(qpp, &ap->q);
        flags = ap->flags;
	ap->startTime.LowPart = now - ap->startTime.LowPart;

        if (osi_statLogp && (flags & OSI_ACTIVEFLAGS_WAITER)) {
		if (flags & OSI_ACTIVEFLAGS_READER)
			whatp = "read lock";
		else if (flags & OSI_ACTIVEFLAGS_WRITER)
			whatp = "write lock";
		else whatp = "mutex";
        	osi_Log2(osi_statLogp, "Finally obtained %s on %s", whatp, qp->namep);
	}
        else {
		/* releasing a lock or mutex */
                if (osi_statWatchProcp && ap->startTime.LowPart > osi_statWatchMS) {
			(*osi_statWatchProcp)(osi_statWatchRockp, ap->startTime.LowPart,
                        	qp->backp);
                }
        }
}

static osi_activeInfo_t *osi_FindActiveInfo(osi_qiStat_t *qp)
{
	unsigned long tid;
	osi_activeInfo_t *ap;
	osi_queue_t *tqp;
	osi_queue_t **qpp = (osi_queue_t **) &qp->activeListp;

	ap = NULL;
	tid = GetCurrentThreadId();
	if (*qpp != NULL) {
		for(tqp = *qpp; tqp; tqp = tqp->nextp) {
			ap = (osi_activeInfo_t *) tqp;
			if (ap->tid == tid) break;
		}
	}
	return ap;
}

static osi_lockOps_t osi_statOps = {
	lock_ObtainReadStat,
	lock_ObtainWriteStat,
	lock_ReleaseReadStat,
	lock_ReleaseWriteStat,
	lock_ObtainMutexStat,
	lock_ReleaseMutexStat,
	lock_TryReadStat,
	lock_TryWriteStat,
	lock_TryMutexStat,
	osi_SleepRStat,
	osi_SleepWStat,
	osi_SleepMStat,
	lock_InitializeMutexStat,
	lock_InitializeRWLockStat,
	lock_FinalizeMutexStat,
	lock_FinalizeRWLockStat,
        lock_ConvertWToRStat,
        lock_ConvertRToWStat,
	lock_GetRWLockStateStat,
        lock_GetMutexStateStat
};

long osi_StatFDCreate(osi_fdType_t *typep, osi_fd_t **fdpp)
{
	osi_statFD_t *fdp;
	osi_mutexStat_t *mp;
	osi_rwlockStat_t *rwp;

	fdp = (osi_statFD_t *) malloc(sizeof(*fdp));
	EnterCriticalSection(&osi_statFDCS);
	if (osi_allMutexes) {
		fdp->curp = osi_allMutexes;
		mp = (osi_mutexStat_t *) fdp->curp;
		mp->refCount++;
		fdp->which = 0;
	}
	else if (osi_allRWLocks) {
		fdp->curp = osi_allRWLocks;
		rwp = (osi_rwlockStat_t *) fdp->curp;
		rwp->refCount++;
		fdp->which = 1;
	}
	else fdp->curp = NULL;
	LeaveCriticalSection(&osi_statFDCS);

	*fdpp = &fdp->fd;

	return 0;
}

long osi_StatFDGetInfo(osi_fd_t *ifdp, osi_remGetInfoParms_t *parmsp)
{
	osi_mutexStat_t *mp;
	osi_statFD_t *fdp;
	osi_rwlockStat_t *rwp;
	osi_queue_t *qp;
        osi_mutex_t *backMutexp;
        osi_rwlock_t *backRWLockp;

	/* initialize out structure */
	parmsp->icount = 0;
	parmsp->scount = 0;

	fdp = (osi_statFD_t *) ifdp;
	qp = fdp->curp;

	/* we're done if curp is null */
	if (qp == NULL) return OSI_DBRPC_EOF;

	/* copy out statistics */
	if (fdp->which == 0) {
		/* this is a mutex */
		mp = (osi_mutexStat_t *) qp;

		memset((void *) parmsp, 0, sizeof(*parmsp));
		backMutexp = mp->qi.backp;
		parmsp->idata[0] = (LONG_PTR)backMutexp;
		parmsp->idata[1] = (backMutexp->flags & OSI_LOCKFLAG_EXCL)? 1 : 0;
		/* reader count [2] is 0 */
		parmsp->idata[3] = (backMutexp->waiters > 0)? 1 : 0;
		parmsp->idata[4] = mp->lockedTime.LowPart;
		parmsp->idata[5] = mp->lockedCount;
		parmsp->idata[6] = mp->blockedTime.LowPart;
		parmsp->idata[7] = mp->blockedCount;
		strcpy(parmsp->sdata[0], mp->qi.namep);
		parmsp->icount = 8;
		parmsp->scount = 1;
	}
	else if (fdp->which == 1) {
		/* rwlock */
		rwp = (osi_rwlockStat_t *) qp;

		memset((void *) parmsp, 0, sizeof(*parmsp));
                backRWLockp = rwp->qi.backp;
		parmsp->idata[0] = (LONG_PTR)backRWLockp;
		parmsp->idata[1] = (backRWLockp->flags & OSI_LOCKFLAG_EXCL)? 1 : 0;
		parmsp->idata[2] = backRWLockp->readers;
		parmsp->idata[3] = (backRWLockp->waiters > 0)? 1 : 0;
		parmsp->idata[4] = rwp->writeLockedTime.LowPart;
		parmsp->idata[5] = rwp->writeLockedCount;
		parmsp->idata[6] = rwp->writeBlockedTime.LowPart;
		parmsp->idata[7] = rwp->writeBlockedCount;
		parmsp->idata[8] = rwp->readLockedTime.LowPart;
		parmsp->idata[9] = rwp->readLockedCount;
		parmsp->idata[10] = rwp->readBlockedTime.LowPart;
		parmsp->idata[11] = rwp->readBlockedCount;
		strcpy(parmsp->sdata[0], rwp->qi.namep);
		parmsp->scount = 1;
		parmsp->icount = 12;
	}

	/* advance to next position */
	EnterCriticalSection(&osi_statFDCS);
	if (qp != NULL) {
		if (fdp->which == 0) {
			mp = (osi_mutexStat_t *) qp;
			lock_DecrMutexStat(mp);
		}
		else if (fdp->which == 1) {
			rwp = (osi_rwlockStat_t *) qp;
			lock_DecrRWLockStat(rwp);
		}
		qp = osi_QNext(qp);
	}
	if (qp == NULL && fdp->which == 0) {
		fdp->which = 1;
		if (osi_allRWLocks) qp = osi_allRWLocks;
		else qp = NULL;
	}
	fdp->curp = qp;
	if (qp != NULL) {
		if (fdp->which == 0) {
			mp = (osi_mutexStat_t *) qp;
			mp->refCount++;
		}
		else if (fdp->which == 1) {
			rwp = (osi_rwlockStat_t *) qp;
			rwp->refCount++;
		}
	}
	LeaveCriticalSection(&osi_statFDCS);

	return 0;
}

long osi_StatFDClose(osi_fd_t *ifdp)
{
	free((void *)ifdp);
	return 0;
}

osi_fdOps_t osi_statFDOps = {
	osi_StatFDCreate,
	osi_StatFDGetInfo,
	osi_StatFDClose
};

void osi_StatInit()
{
	osi_fdType_t *typep;
	int i;

	/* initialize the stat package */
	InitializeCriticalSection(&osi_activeInfoAllocCS);
	InitializeCriticalSection(&osi_statFDCS);

	for(i=0; i<OSI_MUTEXHASHSIZE; i++)
        	InitializeCriticalSection(&osi_statAtomicCS[i]);

	/* add stat ops to dynamic registry */
	osi_LockTypeAdd(&osi_statOps, "stat", &osi_statType);

	/* add debugging info and file descriptor support */
	typep = osi_RegisterFDType("lock", &osi_statFDOps, NULL);
	if (typep) {
		/* add formatting info */
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 0,
			"Lock address", OSI_DBRPC_HEX);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 1,
			"Writer count", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 2,
			"Reader count", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 3,
			"Are waiters", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 4,
			"Write-locked time", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 5,
			"Write-locked count", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 6,
			"Write-blocked time", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 7,
			"Write-blocked count", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 8,
			"Read-locked time", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 9,
			"Read-locked count", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 10,
			"Read-blocked time", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 11,
			"Read-blocked count", 0);

		/* and the string */
		osi_AddFDFormatInfo(typep,  OSI_DBRPC_REGIONSTRING, 0,
			"Lock name", 0);
	}
}

/* set lock watching stuff */
void osi_SetWatchProc(long ms, osi_watchProc_t *procp, void *rockp)
{
	osi_statWatchProcp = procp;
        osi_statWatchRockp = rockp;
        osi_statWatchMS = ms;
}
