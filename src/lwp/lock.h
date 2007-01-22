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
	Include file for using Vice locking routines.
*/

#ifndef _AFS_LWP_LOCK_H
#define _AFS_LWP_LOCK_H

#ifdef KERNEL
#error Do not include lwp/lock.h for kernel code. Use afs/lock.h instead.
#endif


#include <osi/osi_includes.h>

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#define LOCK_LOCK(A) osi_Assert(pthread_mutex_lock(&(A)->mutex) == 0)
#define LOCK_UNLOCK(A) osi_Assert(pthread_mutex_unlock(&(A)->mutex) == 0)
#else /* AFS_PTHREAD_ENV */
#define LOCK_LOCK(A)
#define LOCK_UNLOCK(A)
#endif /* AFS_PTHREAD_ENV */

/* all locks wait on excl_locked except for READ_LOCK, which waits on readers_reading */
struct Lock {
    unsigned char wait_states;	/* type of lockers waiting */
    unsigned char excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned char readers_reading;	/* # readers actually with read locks */
    unsigned char num_waiting;	/* probably need this soon */
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_t mutex;	/* protects this structure */
    pthread_cond_t read_cv;	/* wait for read locks */
    pthread_cond_t write_cv;	/* wait for write/shared locks */
#endif				/* AFS_PTHREAD_ENV */
};

extern void Afs_Lock_Obtain(struct Lock *lock, int how);
extern void Afs_Lock_ReleaseR(struct Lock *lock);
extern void Afs_Lock_ReleaseW(struct Lock *lock);
extern void Lock_Init(struct Lock *lock);
extern void Lock_Destroy(struct Lock *lock);

#define READ_LOCK	1
#define WRITE_LOCK	2
#define SHARED_LOCK	4
/* this next is not a flag, but rather a parameter to Afs_Lock_Obtain */
#define BOOSTED_LOCK 6

/* next defines wait_states for which we wait on excl_locked */
#define EXCL_LOCKS (WRITE_LOCK|SHARED_LOCK)

#define ObtainReadLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    if (!((lock)->excl_locked & WRITE_LOCK) && !(lock)->wait_states)\
		(lock) -> readers_reading++;\
	    else\
		Afs_Lock_Obtain(lock, READ_LOCK); \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

#define ObtainReadLockNoBlock(lock, code)\
        osi_Macro_Begin \
            LOCK_LOCK(lock); \
            if (!((lock)->excl_locked & WRITE_LOCK) && !(lock)->wait_states) {\
                (lock) -> readers_reading++;\
                code = 0;\
            }\
            else\
                code = -1; \
            LOCK_UNLOCK(lock); \
        osi_Macro_End

#define ObtainWriteLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    if (!(lock)->excl_locked && !(lock)->readers_reading)\
		(lock) -> excl_locked = WRITE_LOCK;\
	    else\
		Afs_Lock_Obtain(lock, WRITE_LOCK); \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

#define ObtainWriteLockNoBlock(lock, code)\
        osi_Macro_Begin \
            LOCK_LOCK(lock); \
            if (!(lock)->excl_locked && !(lock)->readers_reading) {\
                (lock) -> excl_locked = WRITE_LOCK;\
                code = 0;\
            }\
            else\
                code = -1; \
            LOCK_UNLOCK(lock); \
        osi_Macro_End

#define ObtainSharedLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    if (!(lock)->excl_locked && !(lock)->wait_states)\
		(lock) -> excl_locked = SHARED_LOCK;\
	    else\
	        Afs_Lock_Obtain(lock, SHARED_LOCK); \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

#define ObtainSharedLockNoBlock(lock, code)\
        osi_Macro_Begin \
            LOCK_LOCK(lock); \
            if (!(lock)->excl_locked && !(lock)->wait_states) {\
                (lock) -> excl_locked = SHARED_LOCK;\
                code = 0;\
            }\
            else\
                code = -1; \
            LOCK_UNLOCK(lock); \
        osi_Macro_End

#define BoostSharedLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    if (!(lock)->readers_reading)\
		(lock)->excl_locked = WRITE_LOCK;\
	    else\
		Afs_Lock_Obtain(lock, BOOSTED_LOCK); \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

/* this must only be called with a WRITE or boosted SHARED lock! */
#define UnboostSharedLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    (lock)->excl_locked = SHARED_LOCK; \
	    if((lock)->wait_states) \
		Afs_Lock_ReleaseR(lock); \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

#ifdef notdef
/* this is what UnboostSharedLock looked like before the hi-C compiler */
/* this must only be called with a WRITE or boosted SHARED lock! */
#define UnboostSharedLock(lock)\
	((lock)->excl_locked = SHARED_LOCK,\
	((lock)->wait_states ?\
		Afs_Lock_ReleaseR(lock) : 0))
#endif /* notdef */

#define ReleaseReadLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    if (!--(lock)->readers_reading && (lock)->wait_states)\
		Afs_Lock_ReleaseW(lock) ; \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End


#ifdef notdef
/* This is what the previous definition should be, but the hi-C compiler generates
  a warning for each invocation */
#define ReleaseReadLock(lock)\
	(!--(lock)->readers_reading && (lock)->wait_states ?\
		Afs_Lock_ReleaseW(lock)    :\
		0)
#endif /* notdef */

#define ReleaseWriteLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    (lock)->excl_locked &= ~WRITE_LOCK;\
	    if ((lock)->wait_states) Afs_Lock_ReleaseR(lock);\
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

#ifdef notdef
/* This is what the previous definition should be, but the hi-C compiler generates
   a warning for each invocation */
#define ReleaseWriteLock(lock)\
	((lock)->excl_locked &= ~WRITE_LOCK,\
	((lock)->wait_states ?\
		Afs_Lock_ReleaseR(lock) : 0))
#endif /* notdef */

/* can be used on shared or boosted (write) locks */
#define ReleaseSharedLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    if ((lock)->wait_states) Afs_Lock_ReleaseR(lock);\
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

#ifdef notdef
/* This is what the previous definition should be, but the hi-C compiler generates
   a warning for each invocation */
/* can be used on shared or boosted (write) locks */
#define ReleaseSharedLock(lock)\
	((lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK),\
	((lock)->wait_states ?\
		Afs_Lock_ReleaseR(lock) : 0))
#endif /* notdef */

/* convert a write lock to a read lock */
#define ConvertWriteToReadLock(lock)\
	osi_Macro_Begin \
	    LOCK_LOCK(lock); \
	    (lock)->excl_locked &= ~WRITE_LOCK;\
	    (lock)->readers_reading++;\
	    if ((lock)->wait_states & READ_LOCK) \
		Afs_Lock_WakeupR(lock) ; \
	    LOCK_UNLOCK(lock); \
	osi_Macro_End

/* I added this next macro to make sure it is safe to nuke a lock -- Mike K. */
#define LockWaiters(lock)\
	((int) ((lock)->num_waiting))

#define CheckLock(lock)\
	((lock)->excl_locked? (int) -1 : (int) (lock)->readers_reading)

#define WriteLocked(lock)\
	((lock)->excl_locked & WRITE_LOCK)

#endif /* _AFS_LWP_LOCK_H */
