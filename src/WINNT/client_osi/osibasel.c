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
#include <assert.h>

/* atomicity-providing critical sections */
CRITICAL_SECTION osi_baseAtomicCS[OSI_MUTEXHASHSIZE];

void osi_BaseInit(void)
{
	int i;

        for(i=0; i<OSI_MUTEXHASHSIZE; i++)
		InitializeCriticalSection(&osi_baseAtomicCS[i]);
}

void lock_ObtainWrite(osi_rwlock_t *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ObtainWriteProc)(lockp);
	    return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)
		|| (lockp->readers > 0)) {
                lockp->waiters++;
		osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, csp);
                lockp->waiters--;
		osi_assert(lockp->readers == 0 && (lockp->flags & OSI_LOCKFLAG_EXCL));
	}
        else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
	}

	LeaveCriticalSection(csp);
}

void lock_ObtainRead(osi_rwlock_t *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ObtainReadProc)(lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
		lockp->waiters++;
		osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4READ, &lockp->readers, csp);
		lockp->waiters--;
		osi_assert(!(lockp->flags & OSI_LOCKFLAG_EXCL) && lockp->readers > 0);
	}
        else {
		/* if we're here, all clear to set the lock */
		lockp->readers++;
	}
	LeaveCriticalSection(csp);
}

void lock_ReleaseRead(osi_rwlock_t *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ReleaseReadProc)(lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->readers > 0, "read lock not held");
	
	/* releasing a read lock can allow readers or writers */
	if (--lockp->readers == 0 && !osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, 0, csp);
	}
        else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

void lock_ReleaseWrite(osi_rwlock_t *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ReleaseWriteProc)(lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "write lock not held");
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, 0, csp);
	}
	else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

void lock_ConvertWToR(osi_rwlock_t *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ConvertWToRProc)(lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "write lock not held");
	
	/* convert write lock to read lock */
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
        lockp->readers++;

	if (!osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, /* still have readers */ 1, csp);
	}
        else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

void lock_ObtainMutex(struct osi_mutex *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ObtainMutexProc)(lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
        	lockp->waiters++;
		osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, csp);
                lockp->waiters--;
		osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
	}
        else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
	}
        lockp->tid = thrd_Current();
	LeaveCriticalSection(csp);
}

void lock_ReleaseMutex(struct osi_mutex *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->ReleaseMutexProc)(lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "mutex not held");
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
        lockp->tid = 0;
	if (!osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, 0, csp);
	}
	else {
		/* and finally release the big lock */
		LeaveCriticalSection(csp);
	}
}

int lock_TryRead(struct osi_rwlock *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lockp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		return (osi_lockOps[i]->TryReadProc)(lockp);

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
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

	LeaveCriticalSection(csp);

	return i;
}


int lock_TryWrite(struct osi_rwlock *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lockp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		return (osi_lockOps[i]->TryWriteProc)(lockp);

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)
		|| (lockp->readers > 0)) {
		i = 0;
	}
	else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
		i = 1;
	}

	LeaveCriticalSection(csp);

	return i;
}


int lock_TryMutex(struct osi_mutex *lockp) {
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lockp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		return (osi_lockOps[i]->TryMutexProc)(lockp);

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
		i = 0;
	}
	else {
		/* if we're here, all clear to set the lock */
		lockp->flags |= OSI_LOCKFLAG_EXCL;
		i = 1;
	}

	LeaveCriticalSection(csp);

	return i;
}

void osi_SleepR(LONG_PTR sleepVal, struct osi_rwlock *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->SleepRProc)(sleepVal, lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->readers > 0, "osi_SleepR: not held");
	
	/* XXX better to get the list of things to wakeup from TSignalForMLs, and
         * then do the wakeup after SleepSpin releases the low-level mutex.
         */
	if (--lockp->readers == 0 && !osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, 0, NULL);
	}

	/* now call into scheduler to sleep atomically with releasing spin lock */
	osi_SleepSpin(sleepVal, csp);
}

void osi_SleepW(LONG_PTR sleepVal, struct osi_rwlock *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->SleepWProc)(sleepVal, lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "osi_SleepW: not held");
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, 0, NULL);
	}

	/* and finally release the big lock */
	osi_SleepSpin(sleepVal, csp);
}

void osi_SleepM(LONG_PTR sleepVal, struct osi_mutex *lockp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i = lockp->type) != 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->SleepMProc)(sleepVal, lockp);
		return;
	}

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lockp->atomicIndex];
        EnterCriticalSection(csp);

	osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "osi_SleepM not held");
	
	lockp->flags &= ~OSI_LOCKFLAG_EXCL;
	if (!osi_TEmpty(&lockp->d.turn)) {
		osi_TSignalForMLs(&lockp->d.turn, 0, NULL);
	}

	/* and finally release the big lock */
	osi_SleepSpin(sleepVal, csp);
}

void lock_FinalizeRWLock(osi_rwlock_t *lockp)
{
	long i;

	if ((i=lockp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->FinalizeRWLockProc)(lockp);
}

void lock_FinalizeMutex(osi_mutex_t *lockp)
{
	long i;

	if ((i=lockp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->FinalizeMutexProc)(lockp);
}

void lock_InitializeMutex(osi_mutex_t *mp, char *namep)
{
	int i;

	if ((i = osi_lockTypeDefault) > 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->InitializeMutexProc)(mp, namep);
		return;
	}

	/* otherwise we have the base case, which requires no special
	 * initialization.
	 */
	mp->type = 0;
	mp->flags = 0;
        mp->tid = 0;
	mp->atomicIndex = osi_MUTEXHASH(mp);
        osi_TInit(&mp->d.turn);
	return;
}

void lock_InitializeRWLock(osi_rwlock_t *mp, char *namep)
{
	int i;

	if ((i = osi_lockTypeDefault) > 0) {
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		(osi_lockOps[i]->InitializeRWLockProc)(mp, namep);
		return;
	}
	
	/* otherwise we have the base case, which requires no special
	 * initialization.
	 */
	mp->type = 0;
	mp->flags = 0;
	mp->atomicIndex = osi_MUTEXHASH(mp);
	mp->readers = 0;
        osi_TInit(&mp->d.turn);
	return;
}

int lock_GetRWLockState(osi_rwlock_t *lp)
{
	long i;
        CRITICAL_SECTION *csp;

	if ((i=lp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		return (osi_lockOps[i]->GetRWLockState)(lp);

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[lp->atomicIndex];
        EnterCriticalSection(csp);

	/* here we have the fast lock, so see if we can obtain the real lock */
	if (lp->flags & OSI_LOCKFLAG_EXCL) i = OSI_RWLOCK_WRITEHELD;
        else i = 0;
	if (lp->readers > 0) i |= OSI_RWLOCK_READHELD;

	LeaveCriticalSection(csp);

	return i;
}

int lock_GetMutexState(struct osi_mutex *mp) {
	long i;
        CRITICAL_SECTION *csp;

	if ((i=mp->type) != 0)
	    if (i >= 0 && i < OSI_NLOCKTYPES)
		return (osi_lockOps[i]->GetMutexState)(mp);

	/* otherwise we're the fast base type */
	csp = &osi_baseAtomicCS[mp->atomicIndex];
        EnterCriticalSection(csp);

	if (mp->flags & OSI_LOCKFLAG_EXCL)
        	i = OSI_MUTEX_HELD;
	else
        	i = 0;

	LeaveCriticalSection(csp);

	return i;
}
