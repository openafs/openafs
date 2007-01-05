/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __AFSLOCK_INCLUDE__
#define	__AFSLOCK_INCLUDE__	    1

#if !defined(KERNEL) && !defined(KDUMP_KERNEL)
#error Do not include afs/lock.h except for kernel code.
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#if	(defined(AFS_SUN5_ENV)) || defined(AFS_OSF_ENV)
#define	AFS_NOBOZO_LOCK
#endif

#if !defined(AFS_OSF20_ENV) || defined(AFS_OSF30_ENV) || defined(AFS_OSF32_ENV)
    /* We do not instrument locks on osf20 because the vcache structure
     ** exceeds the maximim possible limit for a vnode.
     */
#define INSTRUMENT_LOCKS
/* This is the max lock number in use. Please update it if you add any new
 * lock numbers.
 */
#define MAX_LOCK_NUMBER 700
#endif

struct afs_bozoLock {
    short count;		/* count of excl locks */
    char flags;			/* bit 1: is anyone waiting? */
    char spare;			/* for later */
    char *proc;			/* process holding the lock, really a struct proc * */
};
#ifndef	AFS_NOBOZO_LOCK
typedef struct afs_bozoLock afs_bozoLock_t;
#else
#ifdef	AFS_SUN5_ENV
typedef kmutex_t afs_bozoLock_t;
#else
typedef struct afs_bozoLock afs_bozoLock_t;
#endif
#define afs_BozonLock(lock, avc)
#define afs_BozonUnlock(lock, avc)
#define afs_BozonInit(lock, nm)
#define	afs_CheckBozonLock(lock)		0
#define afs_CheckBozonLockBlocking(lock)	0
#endif

#define	AFS_BOZONWAITING    1	/* someone is waiting for this lock */

#undef MObtainWriteLock		/* Defined also in ../rx/rx_machdep.h" */
#undef MReleaseWriteLock
#define MObtainReadLock(lock)	ObtainReadLock(lock)
#define MObtainWriteLock(lock,src)	ObtainWriteLock(lock,src)
#define MObtainSharedLock(lock,src)	ObtainSharedLock(lock,src)
#define MUpgradeSToWLock(lock,src)	UpgradeSToWLock(lock,src)
#define MConvertWToSLock(lock)	ConvertWToSLock(lock)
#define MReleaseReadLock(lock)	ReleaseReadLock(lock)
#define MReleaseWriteLock(lock)	ReleaseWriteLock(lock)
#define MReleaseSharedLock(lock) ReleaseSharedLock(lock)

#define	RWLOCK_INIT(lock, nm)	Lock_Init(lock)
#undef	LOCK_INIT
#define	LOCK_INIT(lock, nm)	Lock_Init(lock)

/* The following macros allow multi statement macros to be defined safely, i.e.
   - the multi statement macro can be the object of an if statement;
   - the call to the multi statement macro may be legally followed by a semi-colon.
   BEGINMAC and ENDMAC have been tested with both the portable C compiler and
   Hi-C.  Both compilers were from the Palo Alto 4.2BSD software releases, and
   both optimized out the constant loop code.  For an example of the use
   of BEGINMAC and ENDMAC, see the definition for ReleaseWriteLock, below.
   An alternative to this, using "if(1)" for BEGINMAC is not used because it
   may generate worse code with pcc, and may generate warning messages with hi-C.
*/

#define BEGINMAC do {
#define ENDMAC   } while (0)

#if defined(AFS_SUN57_ENV) 
#define MyPidxx (curthread->t_tid)
#else
#if defined(AFS_OBSD_ENV) || defined(AFS_SUN5_ENV)
#define MyPidxx (curproc->p_pid)
#else
#if defined(AFS_AIX41_ENV)
extern tid_t thread_self();
#define MyPidxx thread_self()
#else /* AFS_AIX41_ENV */
#if defined(AFS_HPUX101_ENV)
#define MyPidxx ((int)p_pid(u.u_procp))
#else
#if defined(AFS_SGI64_ENV)
#if defined(AFS_SGI65_ENV)
#define MyPidxx proc_pid(curproc())
#else
#define MyPidxx current_pid()
#endif
#else /* AFS_SGI64_ENV */
#ifdef AFS_LINUX20_ENV
#define MyPidxx current->pid
#else
#if defined(AFS_DARWIN_ENV)
#if defined(AFS_DARWIN80_ENV)
#define MyPidxx (proc_selfpid())
#else
#define MyPidxx (current_proc()->p_pid )
#endif
#else
#if defined(AFS_FBSD_ENV)
#define MyPidxx (curproc->p_pid )
#else
#define MyPidxx (u.u_procp->p_pid )
#endif /* AFS_FBSD_ENV */
#endif /* AFS_DARWIN_ENV */
#endif /* AFS_LINUX20_ENV */
#endif /* AFS_SGI64_ENV */
#endif /* AFS_HPUX101_ENV */
#endif /* AFS_AIX41_ENV */
#endif
#endif

/* all locks wait on excl_locked except for READ_LOCK, which waits on readers_reading */
struct afs_lock {
    unsigned char wait_states;	/* type of lockers waiting */
    unsigned char excl_locked;	/* anyone have boosted, shared or write lock? */
    unsigned short readers_reading;	/* # readers actually with read locks */
    unsigned short num_waiting;	/* probably need this soon */
    unsigned short spare;	/* not used now */
    osi_timeval_t time_waiting;	/* for statistics gathering */
#if defined(INSTRUMENT_LOCKS)
    /* the following are useful for debugging 
     ** the field 'src_indicator' is updated only by ObtainLock() and
     ** only for writes/shared  locks. Hence, it indictes where in the
     ** source code the shared/write lock was set.
     */
    unsigned int pid_last_reader;	/* proceess id of last reader */
    unsigned int pid_writer;	/* process id of writer, else 0 */
    unsigned int src_indicator;	/* third param to ObtainLock() */
#endif				/* INSTRUMENT_LOCKS */
};
typedef struct afs_lock afs_lock_t;
typedef struct afs_lock afs_rwlock_t;

#define READ_LOCK	1
#define WRITE_LOCK	2
#define SHARED_LOCK	4
/* this next is not a flag, but rather a parameter to Afs_Lock_Obtain */
#define BOOSTED_LOCK 6

/* next defines wait_states for which we wait on excl_locked */
#define EXCL_LOCKS (WRITE_LOCK|SHARED_LOCK)

#ifdef KERNEL
#include "icl.h"

extern int afs_trclock;

#define AFS_LOCK_TRACE_ENABLE 0
#if AFS_LOCK_TRACE_ENABLE
#define AFS_LOCK_TRACE(op, lock, type) \
	if (afs_trclock) Afs_Lock_Trace(op, lock, type, __FILE__, __LINE__);
#else
#define AFS_LOCK_TRACE(op, lock, type)
#endif

#if defined(INSTRUMENT_LOCKS)

#define ObtainReadLock(lock)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, READ_LOCK);\
	if (!((lock)->excl_locked & WRITE_LOCK)) \
            ((lock)->readers_reading)++; \
	else \
	    Afs_Lock_Obtain(lock, READ_LOCK); \
        (lock)->pid_last_reader = MyPidxx; \
   ENDMAC

#define ObtainWriteLock(lock, src)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, WRITE_LOCK);\
	if (!(lock)->excl_locked && !(lock)->readers_reading)\
	    (lock) -> excl_locked = WRITE_LOCK;\
	else\
	    Afs_Lock_Obtain(lock, WRITE_LOCK); \
	(lock)->pid_writer = MyPidxx; \
	(lock)->src_indicator = src;\
   ENDMAC

#define NBObtainWriteLock(lock, src) (((lock)->excl_locked || (lock)->readers_reading) ? EWOULDBLOCK : (((lock) -> excl_locked = WRITE_LOCK), ((lock)->pid_writer = MyPidxx), ((lock)->src_indicator = src), 0))

#define ObtainSharedLock(lock, src)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, SHARED_LOCK);\
	if (!(lock)->excl_locked)\
	    (lock) -> excl_locked = SHARED_LOCK;\
	else\
	    Afs_Lock_Obtain(lock, SHARED_LOCK); \
	(lock)->pid_writer = MyPidxx; \
	(lock)->src_indicator = src;\
   ENDMAC

#define NBObtainSharedLock(lock, src) (((lock)->excl_locked) ? EWOULDBLOCK : (((lock) -> excl_locked = SHARED_LOCK), ((lock)->pid_writer = MyPidxx), ((lock)->src_indicator = src), 0))

#define UpgradeSToWLock(lock, src)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, BOOSTED_LOCK);\
	if (!(lock)->readers_reading)\
	    (lock)->excl_locked = WRITE_LOCK;\
	else\
	    Afs_Lock_Obtain(lock, BOOSTED_LOCK); \
	(lock)->pid_writer = MyPidxx; \
	(lock)->src_indicator = src;\
   ENDMAC

/* this must only be called with a WRITE or boosted SHARED lock! */
#define ConvertWToSLock(lock)\
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDOWN, lock, SHARED_LOCK);\
	    (lock)->excl_locked = SHARED_LOCK; \
	    if((lock)->wait_states) \
		Afs_Lock_ReleaseR(lock); \
	ENDMAC

#define ConvertWToRLock(lock) \
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDOWN, lock, READ_LOCK);\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    ((lock)->readers_reading)++;\
	    (lock)->pid_last_reader = MyPidxx ; \
	    (lock)->pid_writer = 0;\
	    Afs_Lock_ReleaseR(lock);\
	ENDMAC

#define ConvertSToRLock(lock) \
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDOWN, lock, READ_LOCK);\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    ((lock)->readers_reading)++;\
	    (lock)->pid_last_reader = MyPidxx ; \
	    (lock)->pid_writer = 0;\
	    Afs_Lock_ReleaseR(lock);\
	ENDMAC

#define ReleaseReadLock(lock)\
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDONE, lock, READ_LOCK);\
	    if (!(--((lock)->readers_reading)) && (lock)->wait_states)\
		Afs_Lock_ReleaseW(lock) ; \
	if ( (lock)->pid_last_reader == MyPidxx ) \
		(lock)->pid_last_reader =0;\
	ENDMAC

#define ReleaseWriteLock(lock)\
        BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDONE, lock, WRITE_LOCK);\
	    (lock)->excl_locked &= ~WRITE_LOCK;\
	    if ((lock)->wait_states) Afs_Lock_ReleaseR(lock);\
	    (lock)->pid_writer=0; \
        ENDMAC

/* can be used on shared or boosted (write) locks */
#define ReleaseSharedLock(lock)\
        BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDONE, lock, SHARED_LOCK);\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    if ((lock)->wait_states) Afs_Lock_ReleaseR(lock);\
	    (lock)->pid_writer=0; \
        ENDMAC

#else /* INSTRUMENT_LOCKS */

#define ObtainReadLock(lock)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, READ_LOCK);\
	if (!((lock)->excl_locked & WRITE_LOCK)) \
            ((lock)->readers_reading)++; \
	else \
	    Afs_Lock_Obtain(lock, READ_LOCK); \
   ENDMAC

#define ObtainWriteLock(lock, src)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, WRITE_LOCK);\
	if (!(lock)->excl_locked && !(lock)->readers_reading)\
	    (lock) -> excl_locked = WRITE_LOCK;\
	else\
	    Afs_Lock_Obtain(lock, WRITE_LOCK); \
   ENDMAC

#define NBObtainWriteLock(lock, src) (((lock)->excl_locked || (lock)->readers_reading) ? EWOULDBLOCK : (((lock) -> excl_locked = WRITE_LOCK),  0))

#define ObtainSharedLock(lock, src)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, SHARED_LOCK);\
	if (!(lock)->excl_locked)\
	    (lock) -> excl_locked = SHARED_LOCK;\
	else\
	    Afs_Lock_Obtain(lock, SHARED_LOCK); \
   ENDMAC

#define NBObtainSharedLock(lock, src) (((lock)->excl_locked) ? EWOULDBLOCK : (((lock) -> excl_locked = SHARED_LOCK), 0))

#define UpgradeSToWLock(lock, src)\
  BEGINMAC  \
	AFS_LOCK_TRACE(CM_TRACE_LOCKOBTAIN, lock, BOOSTED_LOCK);\
	if (!(lock)->readers_reading)\
	    (lock)->excl_locked = WRITE_LOCK;\
	else\
	    Afs_Lock_Obtain(lock, BOOSTED_LOCK); \
   ENDMAC

/* this must only be called with a WRITE or boosted SHARED lock! */
#define ConvertWToSLock(lock)\
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDOWN, lock, SHARED_LOCK);\
	    (lock)->excl_locked = SHARED_LOCK; \
	    if((lock)->wait_states) \
		Afs_Lock_ReleaseR(lock); \
	ENDMAC

#define ConvertWToRLock(lock) \
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDOWN, lock, READ_LOCK);\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    ((lock)->readers_reading)++;\
	    Afs_Lock_ReleaseR(lock);\
	ENDMAC

#define ConvertSToRLock(lock) \
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDOWN, lock, READ_LOCK);\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    ((lock)->readers_reading)++;\
	    Afs_Lock_ReleaseR(lock);\
	ENDMAC

#define ReleaseReadLock(lock)\
	BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDONE, lock, READ_LOCK);\
	    if (!(--((lock)->readers_reading)) && (lock)->wait_states)\
		Afs_Lock_ReleaseW(lock) ; \
	ENDMAC

#define ReleaseWriteLock(lock)\
        BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDONE, lock, WRITE_LOCK);\
	    (lock)->excl_locked &= ~WRITE_LOCK;\
	    if ((lock)->wait_states) Afs_Lock_ReleaseR(lock);\
        ENDMAC

/* can be used on shared or boosted (write) locks */
#define ReleaseSharedLock(lock)\
        BEGINMAC\
	AFS_LOCK_TRACE(CM_TRACE_LOCKDONE, lock, SHARED_LOCK);\
	    (lock)->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);\
	    if ((lock)->wait_states) Afs_Lock_ReleaseR(lock);\
        ENDMAC

#endif /* INSTRUMENT_LOCKS */

/* I added this next macro to make sure it is safe to nuke a lock -- Mike K. */
#define LockWaiters(lock)\
	((int) ((lock)->num_waiting))

#define CheckLock(lock)\
	((lock)->excl_locked? (int) -1 : (int) (lock)->readers_reading)

#define WriteLocked(lock)\
	((lock)->excl_locked & WRITE_LOCK)
#endif

/*

You can also use the lock package for handling parent locks for independently-lockable sets of
small objects.  The concept here is that the parent lock is at the same level in the
locking hierarchy as the little locks, but certain restrictions apply.

The general usage pattern is as follows.  You have a set of entries to search.  When searching it, you
have a "scan" lock on the table.  If you find what you're looking for, you drop the lock down
to a "hold" lock, lock the entry, and release the parent lock.  If you don't find what
you're looking for, you create the entry, downgrade the "scan" lock to a "hold" lock,
lock the entry and unlock the parent.

To delete an item from the table, you initially obtain a "purge" lock on the parent.  Unlike all
of the other parent lock modes described herein, in order to obtain a "purge" lock mode, you
must have released all locks on any items in the table.  Once you have obtained the parent
lock in "purge" mode, you should check to see if the entry is locked.  If its not locked, you
are free to delete the entry, knowing that no one else can attempt to obtain a lock
on the entry while you have the purge lock held on the parent.  Unfortunately, if it *is* locked,
you can not lock it yourself and wait for the other dude to release it, since the entry's locker
may need to lock another entry before unlocking the entry you want (which would result in
deadlock).  Instead, then, you must release the parent lock, and try again "later" (see Lock_Wait
for assistance in waiting until later). Unfortunately, this is the best locking paradigm I've yet
come up with.

What are the advantages to this scheme?  First, the use of the parent lock ensures that
two people don't try to add the same entry at the same time or delete an entry while someone
else is adding it.  It also ensures that when one process is deleting an entry, no one else is
preparing to lock the entry.  Furthermore, when obtaining a lock on a little entry, you
are only holding a "hold" lock on the parent lock, so that others may come in and search
the table during this time.  Thus it will not hold up the system if a little entry takes
a great deal of time to free up.

Here's how to compute the compatibility matrix:

The invariants are:

add	no deletions, additions allowed, additions will be performed, will obtain little locks
hold	no deletions, additions allowed, no additions will be performed, will obtain little locks
purge	no deletions or additions allowed, deletions will be performed, don't obtain little locks

When we compute the locking matrix, we note that hold is compatible with hold and add.
Add is compatible only with hold.  purge is not compatible with anything.  This is the same
matrix as obtained by mapping add->S, hold->read and purge->write locks.  Thus we
can use the locks above to solve this problem, and we do.

*/

#endif /* __AFSLOCK_INCLUDE__ */
