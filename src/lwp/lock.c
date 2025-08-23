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
#include <afs/param.h>

#include <roken.h>
#include <afs/opr.h>

#include "lwp.h"
#include "afs/afs_lock.h"

#define FALSE	0
#define TRUE	1

void
Lock_Init(struct Lock *lock)
{
    lock->readers_reading = 0;
    lock->excl_locked = 0;
    lock->wait_states = 0;
    lock->num_waiting = 0;
#ifdef AFS_PTHREAD_ENV
    opr_mutex_init(&lock->mutex);
    opr_cv_init(&lock->read_cv);
    opr_cv_init(&lock->write_cv);
#endif /* AFS_PTHREAD_ENV */
}

void
Lock_Destroy(struct Lock *lock)
{
#ifdef AFS_PTHREAD_ENV
    opr_mutex_destroy(&lock->mutex);
    opr_cv_destroy(&lock->read_cv);
    opr_cv_destroy(&lock->write_cv);
#endif /* AFS_PTHREAD_ENV */
}

void
Afs_Lock_Obtain(struct Lock *lock, int how)
{
    switch (how) {

    case READ_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	    opr_cv_wait(&lock->read_cv, &lock->mutex);
#else /* AFS_PTHREAD_ENV */
	    LWP_WaitProcess(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
	} while (lock->excl_locked & WRITE_LOCK);
	lock->num_waiting--;
	lock->readers_reading++;
	break;

    case WRITE_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= WRITE_LOCK;
#ifdef AFS_PTHREAD_ENV
	    opr_cv_wait(&lock->write_cv, &lock->mutex);
#else /* AFS_PTHREAD_ENV */
	    LWP_WaitProcess(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
	} while (lock->excl_locked || lock->readers_reading);
	lock->num_waiting--;
	lock->excl_locked = WRITE_LOCK;
	break;

    case SHARED_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= SHARED_LOCK;
#ifdef AFS_PTHREAD_ENV
	    opr_cv_wait(&lock->write_cv, &lock->mutex);
#else /* AFS_PTHREAD_ENV */
	    LWP_WaitProcess(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
	} while (lock->excl_locked);
	lock->num_waiting--;
	lock->excl_locked = SHARED_LOCK;
	break;

    case BOOSTED_LOCK:
	lock->num_waiting++;
	do {
	    lock->wait_states |= WRITE_LOCK;
#ifdef AFS_PTHREAD_ENV
	    opr_cv_wait(&lock->write_cv, &lock->mutex);
#else /* AFS_PTHREAD_ENV */
	    LWP_WaitProcess(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
	} while (lock->readers_reading);
	lock->num_waiting--;
	lock->excl_locked = WRITE_LOCK;
	break;

    default:
	printf("Can't happen, bad LOCK type: %d\n", how);
	opr_Assert(0);
    }
}

/* wake up readers waiting for this lock */
void
Afs_Lock_WakeupR(struct Lock *lock)
{
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	opr_cv_broadcast(&lock->read_cv);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
    }
}

/* release a lock, giving preference to new readers */
void
Afs_Lock_ReleaseR(struct Lock *lock)
{
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	opr_cv_broadcast(&lock->read_cv);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
    } else {
	lock->wait_states &= ~EXCL_LOCKS;
#ifdef AFS_PTHREAD_ENV
	opr_cv_broadcast(&lock->write_cv);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
    }
}

/* release a lock, giving preference to new writers */
void
Afs_Lock_ReleaseW(struct Lock *lock)
{
    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
#ifdef AFS_PTHREAD_ENV
	opr_cv_broadcast(&lock->write_cv);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
    } else {
	lock->wait_states &= ~READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	opr_cv_broadcast(&lock->read_cv);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
    }
}

#ifndef AFS_PTHREAD_ENV
/* These next guys exist to provide an interface to drop a lock atomically with
 * blocking.  They're trivial to do in a non-preemptive LWP environment.
 */

/* release a write lock and sleep on an address, atomically */
void
LWP_WaitProcessR(void *addr, struct Lock *alock)
{
    ReleaseReadLock(alock);
    LWP_WaitProcess(addr);
}

/* release a write lock and sleep on an address, atomically */
void
LWP_WaitProcessW(void *addr, struct Lock *alock)
{
    ReleaseWriteLock(alock);
    LWP_WaitProcess(addr);
}

/* release a write lock and sleep on an address, atomically */
void
LWP_WaitProcessS(void *addr, struct Lock *alock)
{
    ReleaseSharedLock(alock);
    LWP_WaitProcess(addr);
}
#endif /* AFS_PTHREAD_ENV */
