
#ifndef lint
#endif

/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
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

#include <afs/param.h>
#ifdef AFS_PTHREAD_ENV
#include <afs/assert.h>
#else /* AFS_PTHREAD_ENV */
#include <assert.h>
#endif /* AFS_PTHRED_ENV */
#include "lwp.h"
#include "lock.h"
#include <stdio.h>

#define FALSE	0
#define TRUE	1

void Lock_Init(struct Lock *lock)
{
    lock -> readers_reading = 0;
    lock -> excl_locked = 0;
    lock -> wait_states = 0;
    lock -> num_waiting = 0;
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&lock->mutex, (const pthread_mutexattr_t*)0)==0);
    assert(pthread_cond_init(&lock->read_cv, (const pthread_condattr_t*)0)==0);
    assert(pthread_cond_init(&lock->write_cv, (const pthread_condattr_t*)0)==0);
#endif /* AFS_PTHRED_ENV */
}

void Lock_Destroy(struct Lock *lock)
{
#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_destroy(&lock->mutex) == 0);
    assert(pthread_cond_destroy(&lock->read_cv) == 0);
    assert(pthread_cond_destroy(&lock->write_cv) == 0);
#endif /* AFS_PTHRED_ENV */
}

void Afs_Lock_Obtain(struct Lock * lock, int how)
{
    switch (how) {

	case READ_LOCK:		lock->num_waiting++;
				do {
				    lock -> wait_states |= READ_LOCK;
#ifdef AFS_PTHREAD_ENV
				    assert(pthread_cond_wait(&lock->read_cv,
						&lock->mutex) == 0);
#else /* AFS_PTHREAD_ENV */
				    LWP_WaitProcess(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
				} while (lock->excl_locked & WRITE_LOCK);
				lock->num_waiting--;
				lock->readers_reading++;
				break;

	case WRITE_LOCK:	lock->num_waiting++;
				do {
				    lock -> wait_states |= WRITE_LOCK;
#ifdef AFS_PTHREAD_ENV
				    assert(pthread_cond_wait(&lock->write_cv,
						&lock->mutex) == 0);
#else /* AFS_PTHREAD_ENV */
				    LWP_WaitProcess(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
				} while (lock->excl_locked || lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	case SHARED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= SHARED_LOCK;
#ifdef AFS_PTHREAD_ENV
				    assert(pthread_cond_wait(&lock->write_cv,
						&lock->mutex) == 0);
#else /* AFS_PTHREAD_ENV */
				    LWP_WaitProcess(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
				} while (lock->excl_locked);
				lock->num_waiting--;
				lock->excl_locked = SHARED_LOCK;
				break;

	case BOOSTED_LOCK:	lock->num_waiting++;
				do {
				    lock->wait_states |= WRITE_LOCK;
#ifdef AFS_PTHREAD_ENV
				    assert(pthread_cond_wait(&lock->write_cv,
						&lock->mutex) == 0);
#else /* AFS_PTHREAD_ENV */
				    LWP_WaitProcess(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
				} while (lock->readers_reading);
				lock->num_waiting--;
				lock->excl_locked = WRITE_LOCK;
				break;

	default:		printf("Can't happen, bad LOCK type: %d\n", how);
				assert(0);
    }
}

/* wake up readers waiting for this lock */
void Afs_Lock_WakeupR(struct Lock *lock)
{
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&lock->read_cv) == 0);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
    }
}

/* release a lock, giving preference to new readers */
void Afs_Lock_ReleaseR(struct Lock *lock)
{
    if (lock->wait_states & READ_LOCK) {
	lock->wait_states &= ~READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&lock->read_cv) == 0);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->readers_reading);
#endif /* AFS_PTHREAD_ENV */
    }
    else {
	lock->wait_states &= ~EXCL_LOCKS;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&lock->write_cv) == 0);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
    }
}

/* release a lock, giving preference to new writers */
void Afs_Lock_ReleaseW(struct Lock * lock)
{
    if (lock->wait_states & EXCL_LOCKS) {
	lock->wait_states &= ~EXCL_LOCKS;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&lock->write_cv) == 0);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(&lock->excl_locked);
#endif /* AFS_PTHREAD_ENV */
    }
    else {
	lock->wait_states &= ~READ_LOCK;
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&lock->read_cv) == 0);
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
void LWP_WaitProcessR(addr, alock)
register char *addr;
register struct Lock *alock; {
    ReleaseReadLock(alock);
    LWP_WaitProcess(addr);
}

/* release a write lock and sleep on an address, atomically */
void LWP_WaitProcessW(addr, alock)
register char *addr;
register struct Lock *alock; {
    ReleaseWriteLock(alock);
    LWP_WaitProcess(addr);
}

/* release a write lock and sleep on an address, atomically */
void LWP_WaitProcessS(addr, alock)
register char *addr;
register struct Lock *alock; {
    ReleaseSharedLock(alock);
    LWP_WaitProcess(addr);
}
#endif /* AFS_PTHREAD_ENV */
