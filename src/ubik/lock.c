/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/opr.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#endif
#include <afs/afs_lock.h>
#include <afs/afsutil.h>

#define UBIK_INTERNALS 1
#include "ubik.h"
#include "ubik_int.h"

/*! \file
 * Locks hang off of each transaction, with all the transaction hanging off of
 * the appropriate dbase.  This package expects to be used in a two-phase locking
 * protocol, so it doesn't provide a way to release anything but all of the locks in the
 * transaction.
 *
 * At present, it doesn't support the setting of more than one byte-position lock at a time, that is
 * the length field must be 1.  This doesn't mean that a single transaction can't set more than
 * one lock, however.
 *
 * It is the responsibility of the user to avoid deadlock by setting locks in a partial order.
 *
 * #EWOULDBLOCK has been replaced in this file by #EAGAIN. Many Unix's but not
 * all (eg. HP) do not replace #EWOULDBLOCK with #EAGAIN. The bad news is this
 * goes over the wire. The good news is that the code path is never triggered
 * as it requires ulock_getLock to be called with await = 0. And ulock_SetLock
 * isn't even used in this code base. Since NT doesn't have a native
 * #EAGAIN, we are replacing all instances of #EWOULDBLOCK with #EAGAIN.
 *
 */

#define WouldReadBlock(lock)\
  ((((lock)->excl_locked & WRITE_LOCK) || (lock)->wait_states) ? 0 : 1)
#define WouldWriteBlock(lock)\
  ((((lock)->excl_locked & WRITE_LOCK) || (lock)->readers_reading) ? 0 : 1)

struct Lock rwlock;

/*!
 * \Initialize locks
 */
void
ulock_Init(void)
{
    Lock_Init(&rwlock);
}

/*!
 * \brief Set a transaction lock.
 * \param atype is #LOCKREAD or #LOCKWRITE.
 * \param await is TRUE if you want to wait for the lock instead of returning
 * #EWOULDBLOCK.
 *
 * \note The #DBHOLD lock must be held.
 */
extern int
ulock_getLock(struct ubik_trans *atrans, int atype, int await)
{
    struct ubik_dbase *dbase = atrans->dbase;

    if ((atype != LOCKREAD) && (atype != LOCKWRITE))
	return EINVAL;

    if (atrans->flags & TRDONE)
	return UDONE;

    if (atype != LOCKREAD && (atrans->flags & TRREADWRITE)) {
	return EINVAL;
    }

    if (atrans->locktype != 0) {
	ViceLog(0, ("Ubik: Internal Error: attempted to take lock twice\n"));
	abort();
    }

/*
 *ViceLog(0, ("Ubik: DEBUG: Thread 0x%x request %s lock\n", lwp_cpptr,
 *	     ((atype == LOCKREAD) ? "READ" : "WRITE")));
 */

    /* Check if the lock would would block */
    if (!await && !(atrans->flags & TRREADWRITE)) {
	if (atype == LOCKREAD) {
	    if (WouldReadBlock(&rwlock))
		return EAGAIN;
	} else {
	    if (WouldWriteBlock(&rwlock))
		return EAGAIN;
	}
    }

    /* Create new lock record and add to spec'd transaction:
     * locktype. This field also tells us if the thread is
     * waiting for a lock: It will be equal to LOCKWAIT.
     */
    atrans->locktype = LOCKWAIT;
    DBRELE(dbase);
    if (atrans->flags & TRREADWRITE) {
	/* noop; don't actually lock anything for TRREADWRITE */
    } else if (atype == LOCKREAD) {
	ObtainReadLock(&rwlock);
    } else {
	ObtainWriteLock(&rwlock);
    }
    DBHOLD(dbase);
    atrans->locktype = atype;

/*
 *ViceLog(0, ("Ubik: DEBUG: Thread 0x%x took %s lock\n", lwp_cpptr,
 *	     ((atype == LOCKREAD) ? "READ" : "WRITE")));
 */
    return 0;
}

/*!
 * \brief Release the transaction lock.
 */
void
ulock_relLock(struct ubik_trans *atrans)
{
    if (atrans->locktype == LOCKWRITE && (atrans->flags & TRREADWRITE)) {
	ViceLog(0, ("Ubik: Internal Error: unlocking write lock with "
	           "TRREADWRITE?\n"));
	abort();
    }

    if (atrans->flags & TRREADWRITE) {
	/* noop, TRREADWRITE means we don't actually lock anything */
    } else if (atrans->locktype == LOCKREAD) {
	ReleaseReadLock(&rwlock);
    } else if (atrans->locktype == LOCKWRITE) {
	ReleaseWriteLock(&rwlock);
    }

/*
 *ViceLog(0, ("Ubik: DEBUG: Thread 0x%x %s unlock\n", lwp_cpptr,
 *	     ((atrans->locktype == LOCKREAD) ? "READ" : "WRITE")));
 */

    atrans->locktype = 0;
    return;
}

/*!
 * \brief debugging hooks
 */
void
ulock_Debug(struct ubik_debug *aparm)
{
    aparm->anyReadLocks = rwlock.readers_reading;
    aparm->anyWriteLocks = ((rwlock.excl_locked == WRITE_LOCK) ? 1 : 0);
}
