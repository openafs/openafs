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
#include <stdio.h>

/* atomicity-providing critical sections */
CRITICAL_SECTION osi_baseAtomicCS[OSI_MUTEXHASHSIZE];
static long     atomicIndexCounter = 0;

/* Thread local storage index for lock tracking */
static DWORD tls_LockRefH = 0;
static DWORD tls_LockRefT = 0;
static BOOLEAN lockOrderValidation = 0;
static osi_lock_ref_t * lock_ref_FreeListp = NULL;
static osi_lock_ref_t * lock_ref_FreeListEndp = NULL;
CRITICAL_SECTION lock_ref_CS;

void osi_BaseInit(void)
{
    int i;

    for(i=0; i<OSI_MUTEXHASHSIZE; i++)
        InitializeCriticalSection(&osi_baseAtomicCS[i]);

    if ((tls_LockRefH = TlsAlloc()) == TLS_OUT_OF_INDEXES)
        osi_panic("TlsAlloc(tls_LockRefH) failure", __FILE__, __LINE__);

    if ((tls_LockRefT = TlsAlloc()) == TLS_OUT_OF_INDEXES)
        osi_panic("TlsAlloc(tls_LockRefT) failure", __FILE__, __LINE__);

    InitializeCriticalSection(&lock_ref_CS);
}

void
osi_SetLockOrderValidation(int on)
{
    lockOrderValidation = (BOOLEAN)on;
}

static osi_lock_ref_t *
lock_GetLockRef(void * lockp, char type)
{
    osi_lock_ref_t * lockRefp = NULL;

    EnterCriticalSection(&lock_ref_CS);
    if (lock_ref_FreeListp) {
        lockRefp = lock_ref_FreeListp;
        osi_QRemoveHT( (osi_queue_t **) &lock_ref_FreeListp,
                       (osi_queue_t **) &lock_ref_FreeListEndp,
                       &lockRefp->q);
    }
    LeaveCriticalSection(&lock_ref_CS);

    if (lockRefp == NULL)
        lockRefp = (osi_lock_ref_t *)malloc(sizeof(osi_lock_ref_t));

    memset(lockRefp, 0, sizeof(osi_lock_ref_t));
    lockRefp->type = type;
    switch (type) {
    case OSI_LOCK_MUTEX:
        lockRefp->mx = lockp;
        break;
    case OSI_LOCK_RW:
        lockRefp->rw = lockp;
        break;
    default:
        osi_panic("Invalid Lock Type", __FILE__, __LINE__);
    }

    return lockRefp;
}

static void
lock_FreeLockRef(osi_lock_ref_t * lockRefp)
{
    EnterCriticalSection(&lock_ref_CS);
    osi_QAddH( (osi_queue_t **) &lock_ref_FreeListp,
               (osi_queue_t **) &lock_ref_FreeListEndp,
               &lockRefp->q);
    LeaveCriticalSection(&lock_ref_CS);
}

void lock_VerifyOrderRW(osi_queue_t *lockRefH, osi_queue_t *lockRefT, osi_rwlock_t *lockp)
{
    char msg[512];
    osi_lock_ref_t * lockRefp;

    for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
        if (lockRefp->type == OSI_LOCK_RW) {
            if (lockRefp->rw == lockp) {
                sprintf(msg, "RW Lock 0x%p level %d already held", lockp, lockp->level);
                osi_panic(msg, __FILE__, __LINE__);
            }
            if (lockRefp->rw->level > lockp->level) {
                sprintf(msg, "Lock hierarchy violation Held lock 0x%p level %d > Requested lock 0x%p level %d",
                         lockRefp->rw, lockRefp->rw->level, lockp, lockp->level);
                osi_panic(msg, __FILE__, __LINE__);
            }
        } else {
            if (lockRefp->mx->level > lockp->level) {
                sprintf(msg, "Lock hierarchy violation Held lock 0x%p level %d > Requested lock 0x%p level %d",
                         lockRefp->mx, lockRefp->mx->level, lockp, lockp->level);
                osi_panic(msg, __FILE__, __LINE__);
            }
            osi_assertx(lockRefp->mx->level <= lockp->level, "Lock hierarchy violation");
        }
    }
}

void lock_VerifyOrderMX(osi_queue_t *lockRefH, osi_queue_t *lockRefT, osi_mutex_t *lockp)
{
    char msg[512];
    osi_lock_ref_t * lockRefp;

    for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
        if (lockRefp->type == OSI_LOCK_MUTEX) {
            if (lockRefp->mx == lockp) {
                sprintf(msg, "MX Lock 0x%p level %d already held", lockp, lockp->level);
                osi_panic(msg, __FILE__, __LINE__);
            }
            if (lockRefp->mx->level > lockp->level) {
                sprintf(msg, "Lock hierarchy violation Held lock 0x%p level %d > Requested lock 0x%p level %d",
                         lockRefp->mx, lockRefp->mx->level, lockp, lockp->level);
                osi_panic(msg, __FILE__, __LINE__);
            }
        } else {
            if (lockRefp->rw->level > lockp->level) {
                sprintf(msg, "Lock hierarchy violation Held lock 0x%p level %d > Requested lock 0x%p level %d",
                         lockRefp->rw, lockRefp->rw->level, lockp, lockp->level);
                osi_panic(msg, __FILE__, __LINE__);
            }
        }
    }
}

void lock_ObtainWrite(osi_rwlock_t *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i=lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ObtainWriteProc)(lockp);
        return;
    }

    if (lockOrderValidation) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        if (lockp->level != 0)
            lock_VerifyOrderRW(lockRefH, lockRefT, lockp);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    /* here we have the fast lock, so see if we can obtain the real lock */
    if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL) ||
        (lockp->readers > 0)) {
        lockp->waiters++;
        osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, lockp->tid, csp);
        lockp->waiters--;
        osi_assert(lockp->readers == 0 && (lockp->flags & OSI_LOCKFLAG_EXCL));
    } else {
        /* if we're here, all clear to set the lock */
        lockp->flags |= OSI_LOCKFLAG_EXCL;
        lockp->tid[0] = thrd_Current();
    }
    LeaveCriticalSection(csp);

    if (lockOrderValidation) {
        lockRefp = lock_GetLockRef(lockp, OSI_LOCK_RW);
        osi_QAddH(&lockRefH, &lockRefT, &lockRefp->q);
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }
}

void lock_ObtainRead(osi_rwlock_t *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;
    DWORD tid = thrd_Current();

    if ((i=lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ObtainReadProc)(lockp);
        return;
    }

    if (lockOrderValidation) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        if (lockp->level != 0)
            lock_VerifyOrderRW(lockRefH, lockRefT, lockp);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    for ( i=0; i < lockp->readers; i++ ) {
        osi_assertx(lockp->tid[i] != tid, "OSI_RWLOCK_READHELD");
    }

    /* here we have the fast lock, so see if we can obtain the real lock */
    if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
        lockp->waiters++;
        osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4READ, &lockp->readers, lockp->tid, csp);
        lockp->waiters--;
        osi_assert(!(lockp->flags & OSI_LOCKFLAG_EXCL) && lockp->readers > 0);
    } else {
        /* if we're here, all clear to set the lock */
        if (++lockp->readers <= OSI_RWLOCK_THREADS)
            lockp->tid[lockp->readers-1] = tid;
    }
    LeaveCriticalSection(csp);

    if (lockOrderValidation) {
        lockRefp = lock_GetLockRef(lockp, OSI_LOCK_RW);
        osi_QAddH(&lockRefH, &lockRefT, &lockRefp->q);
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }
}

void lock_ReleaseRead(osi_rwlock_t *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;
    DWORD tid = thrd_Current();

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ReleaseReadProc)(lockp);
        return;
    }

    if (lockOrderValidation && lockp->level != 0) {
        int found = 0;
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
            if (lockRefp->type == OSI_LOCK_RW && lockRefp->rw == lockp) {
                osi_QRemoveHT(&lockRefH, &lockRefT, &lockRefp->q);
                lock_FreeLockRef(lockRefp);
                found = 1;
                break;
            }
        }
        osi_assertx(found, "read lock not found in TLS queue");

        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(lockp->readers > 0, "read lock not held");

    for ( i=0; i < lockp->readers; i++) {
        if ( lockp->tid[i] == tid ) {
            for ( ; i < lockp->readers - 1; i++)
                lockp->tid[i] = lockp->tid[i+1];
            lockp->tid[i] = 0;
            break;
        }
    }

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
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ReleaseWriteProc)(lockp);
        return;
    }

    if (lockOrderValidation && lockp->level != 0) {
        int found = 0;
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
            if (lockRefp->type == OSI_LOCK_RW && lockRefp->rw == lockp) {
                osi_QRemoveHT(&lockRefH, &lockRefT, &lockRefp->q);
                lock_FreeLockRef(lockRefp);
                found = 1;
                break;
            }
        }
        osi_assertx(found, "write lock not found in TLS queue");

        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "write lock not held");
    osi_assertx(lockp->tid[0] == thrd_Current(), "write lock not held by current thread");

    lockp->tid[0] = 0;

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
    osi_assertx(lockp->tid[0] == thrd_Current(), "write lock not held by current thread");

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

void lock_ConvertRToW(osi_rwlock_t *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    DWORD tid = thrd_Current();

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ConvertRToWProc)(lockp);
        return;
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(!(lockp->flags & OSI_LOCKFLAG_EXCL), "write lock held");
    osi_assertx(lockp->readers > 0, "read lock not held");

    for ( i=0; i < lockp->readers; i++) {
        if ( lockp->tid[i] == tid ) {
            for ( ; i < lockp->readers - 1; i++)
                lockp->tid[i] = lockp->tid[i+1];
            lockp->tid[i] = 0;
            break;
        }
    }

    if (--lockp->readers == 0) {
        /* convert read lock to write lock */
        lockp->flags |= OSI_LOCKFLAG_EXCL;
        lockp->tid[0] = tid;
    } else {
        lockp->waiters++;
        osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, lockp->tid, csp);
        lockp->waiters--;
        osi_assert(lockp->readers == 0 && (lockp->flags & OSI_LOCKFLAG_EXCL));
    }

    LeaveCriticalSection(csp);
}

void lock_ObtainMutex(struct osi_mutex *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i=lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ObtainMutexProc)(lockp);
        return;
    }

    if (lockOrderValidation) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        if (lockp->level != 0)
            lock_VerifyOrderMX(lockRefH, lockRefT, lockp);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    /* here we have the fast lock, so see if we can obtain the real lock */
    if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
        lockp->waiters++;
        osi_TWait(&lockp->d.turn, OSI_SLEEPINFO_W4WRITE, &lockp->flags, &lockp->tid, csp);
        lockp->waiters--;
        osi_assert(lockp->flags & OSI_LOCKFLAG_EXCL);
    } else {
        /* if we're here, all clear to set the lock */
        lockp->flags |= OSI_LOCKFLAG_EXCL;
        lockp->tid = thrd_Current();
    }
    LeaveCriticalSection(csp);

    if (lockOrderValidation) {
        lockRefp = lock_GetLockRef(lockp, OSI_LOCK_MUTEX);
        osi_QAddH(&lockRefH, &lockRefT, &lockRefp->q);
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }
}

void lock_ReleaseMutex(struct osi_mutex *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->ReleaseMutexProc)(lockp);
        return;
    }

    if (lockOrderValidation && lockp->level != 0) {
        int found = 0;
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
            if (lockRefp->type == OSI_LOCK_MUTEX && lockRefp->mx == lockp) {
                osi_QRemoveHT(&lockRefH, &lockRefT, &lockRefp->q);
                lock_FreeLockRef(lockRefp);
                found = 1;
                break;
            }
        }

        osi_assertx(found, "mutex lock not found in TLS queue");
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "mutex not held");
    osi_assertx(lockp->tid == thrd_Current(), "mutex not held by current thread");

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
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i=lockp->type) != 0)
        if (i >= 0 && i < OSI_NLOCKTYPES)
            return (osi_lockOps[i]->TryReadProc)(lockp);

    if (lockOrderValidation) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        if (lockp->level != 0) {
            for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
                if (lockRefp->type == OSI_LOCK_RW) {
                    osi_assertx(lockRefp->rw != lockp, "RW Lock already held");
                }
            }
        }
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    /* here we have the fast lock, so see if we can obtain the real lock */
    if (lockp->waiters > 0 || (lockp->flags & OSI_LOCKFLAG_EXCL)) {
        i = 0;
    }
    else {
        /* if we're here, all clear to set the lock */
        if (++lockp->readers < OSI_RWLOCK_THREADS)
            lockp->tid[lockp->readers-1] = thrd_Current();
        i = 1;
    }

    LeaveCriticalSection(csp);

    if (lockOrderValidation && i) {
        lockRefp = lock_GetLockRef(lockp, OSI_LOCK_RW);
        osi_QAddH(&lockRefH, &lockRefT, &lockRefp->q);
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    return i;
}


int lock_TryWrite(struct osi_rwlock *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i=lockp->type) != 0)
        if (i >= 0 && i < OSI_NLOCKTYPES)
            return (osi_lockOps[i]->TryWriteProc)(lockp);

    if (lockOrderValidation) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        if (lockp->level != 0) {
            for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
                if (lockRefp->type == OSI_LOCK_RW) {
                    osi_assertx(lockRefp->rw != lockp, "RW Lock already held");
                }
            }
        }
    }

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
        lockp->tid[0] = thrd_Current();
        i = 1;
    }

    LeaveCriticalSection(csp);

    if (lockOrderValidation && i) {
        lockRefp = lock_GetLockRef(lockp, OSI_LOCK_RW);
        osi_QAddH(&lockRefH, &lockRefT, &lockRefp->q);
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    return i;
}


int lock_TryMutex(struct osi_mutex *lockp) {
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i=lockp->type) != 0)
        if (i >= 0 && i < OSI_NLOCKTYPES)
            return (osi_lockOps[i]->TryMutexProc)(lockp);

    if (lockOrderValidation) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        if (lockp->level != 0) {
            for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
                if (lockRefp->type == OSI_LOCK_MUTEX) {
                    osi_assertx(lockRefp->mx != lockp, "Mutex already held");
                }
            }
        }
    }

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
        lockp->tid = thrd_Current();
        i = 1;
    }

    LeaveCriticalSection(csp);

    if (lockOrderValidation && i) {
        lockRefp = lock_GetLockRef(lockp, OSI_LOCK_MUTEX);
        osi_QAddH(&lockRefH, &lockRefT, &lockRefp->q);
        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }
    return i;
}

void osi_SleepR(LONG_PTR sleepVal, struct osi_rwlock *lockp)
{
    long i;
    CRITICAL_SECTION *csp;
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;
    DWORD tid = thrd_Current();

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->SleepRProc)(sleepVal, lockp);
        return;
    }

    if (lockOrderValidation && lockp->level != 0) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
            if (lockRefp->type == OSI_LOCK_RW && lockRefp->rw == lockp) {
                osi_QRemoveHT(&lockRefH, &lockRefT, &lockRefp->q);
                lock_FreeLockRef(lockRefp);
                break;
            }
        }

        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(lockp->readers > 0, "osi_SleepR: not held");

    for ( i=0; i < lockp->readers; i++) {
        if ( lockp->tid[i] == tid ) {
            for ( ; i < lockp->readers - 1; i++)
                lockp->tid[i] = lockp->tid[i+1];
            lockp->tid[i] = 0;
            break;
        }
    }

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
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;
    DWORD tid = thrd_Current();

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->SleepWProc)(sleepVal, lockp);
        return;
    }

    if (lockOrderValidation && lockp->level != 0) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
            if (lockRefp->type == OSI_LOCK_RW && lockRefp->rw == lockp) {
                osi_QRemoveHT(&lockRefH, &lockRefT, &lockRefp->q);
                lock_FreeLockRef(lockRefp);
                break;
            }
        }

        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "osi_SleepW: not held");

    lockp->flags &= ~OSI_LOCKFLAG_EXCL;
    lockp->tid[0] = 0;
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
    osi_queue_t * lockRefH, *lockRefT;
    osi_lock_ref_t *lockRefp;

    if ((i = lockp->type) != 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->SleepMProc)(sleepVal, lockp);
        return;
    }

    if (lockOrderValidation && lockp->level != 0) {
        lockRefH = (osi_queue_t *)TlsGetValue(tls_LockRefH);
        lockRefT = (osi_queue_t *)TlsGetValue(tls_LockRefT);

        for (lockRefp = (osi_lock_ref_t *)lockRefH ; lockRefp; lockRefp = (osi_lock_ref_t *)osi_QNext(&lockRefp->q)) {
            if (lockRefp->type == OSI_LOCK_MUTEX && lockRefp->mx == lockp) {
                osi_QRemoveHT(&lockRefH, &lockRefT, &lockRefp->q);
                lock_FreeLockRef(lockRefp);
                break;
            }
        }

        TlsSetValue(tls_LockRefH, lockRefH);
        TlsSetValue(tls_LockRefT, lockRefT);
    }

    /* otherwise we're the fast base type */
    csp = &osi_baseAtomicCS[lockp->atomicIndex];
    EnterCriticalSection(csp);

    osi_assertx(lockp->flags & OSI_LOCKFLAG_EXCL, "osi_SleepM not held");

    lockp->flags &= ~OSI_LOCKFLAG_EXCL;
    lockp->tid = 0;
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

void lock_InitializeMutex(osi_mutex_t *mp, char *namep, unsigned short level)
{
    int i;

    if ((i = osi_lockTypeDefault) > 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->InitializeMutexProc)(mp, namep, level);
        return;
    }

    /* otherwise we have the base case, which requires no special
     * initialization.
     */
    mp->type = 0;
    mp->flags = 0;
    mp->tid = 0;
    mp->atomicIndex = (unsigned short)(InterlockedIncrement(&atomicIndexCounter) % OSI_MUTEXHASHSIZE);
    mp->level = level;
    osi_TInit(&mp->d.turn);
    return;
}

void lock_InitializeRWLock(osi_rwlock_t *mp, char *namep, unsigned short level)
{
    int i;

    if ((i = osi_lockTypeDefault) > 0) {
        if (i >= 0 && i < OSI_NLOCKTYPES)
            (osi_lockOps[i]->InitializeRWLockProc)(mp, namep, level);
        return;
    }

    /* otherwise we have the base case, which requires no special
     * initialization.
     */
    memset(mp, 0, sizeof(osi_rwlock_t));
    mp->atomicIndex = (unsigned short)(InterlockedIncrement(&atomicIndexCounter) % OSI_MUTEXHASHSIZE);
    mp->level = level;
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
    if (lp->flags & OSI_LOCKFLAG_EXCL)
        i = OSI_RWLOCK_WRITEHELD;
    else
        i = 0;
    if (lp->readers > 0)
        i |= OSI_RWLOCK_READHELD;

    LeaveCriticalSection(csp);

    return i;
}

int lock_GetMutexState(struct osi_mutex *mp)
{
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
