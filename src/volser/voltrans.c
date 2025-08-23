/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *  Module:	    Voltrans.c
 *  System:	    Volser
 *  Instituition:	    ITC, CMU
 *  Date:		    December, 88
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/opr.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#endif

#ifdef AFS_NT40_ENV
#include <afs/afsutil.h>
#endif

#include <rx/rx_queue.h>
#include <afs/afsint.h>
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <afs/afs_lock.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <rx/rx.h>
#include <ubik.h>
#include <afs/ihandle.h>
#ifdef AFS_NT40_ENV
#include <afs/ntops.h>
#endif
#include <afs/vnode.h>
#include <afs/volume.h>

#include "volint.h"
#include "volser.h"
#include "volser_internal.h"

static struct volser_trans *allTrans = 0;
static afs_int32 transCounter = 1;

/* create a new transaction, returning ptr to same with high ref count */
struct volser_trans *
NewTrans(VolumeId avol, afs_int32 apart)
{
    /* set volid, next, partition */
    struct volser_trans *tt, *newtt;
    struct timeval tp;

    newtt = calloc(1, sizeof(struct volser_trans));
    VTRANS_LOCK;
    /* don't allow the same volume to be attached twice */
    for (tt = allTrans; tt; tt = tt->next) {
	if ((tt->volid == avol) && (tt->partition == apart)) {
	    VTRANS_UNLOCK;
	    free(newtt);
	    return (struct volser_trans *)0;	/* volume busy */
	}
    }
    tt = newtt;
    tt->volid = avol;
    tt->partition = apart;
    tt->refCount = 1;
    tt->rxCallPtr = (struct rx_call *)0;
    strcpy(tt->lastProcName, "");
    gettimeofday(&tp, NULL);
    tt->creationTime = tp.tv_sec;
    tt->time = FT_ApproxTime();
    tt->tid = transCounter++;
    tt->next = allTrans;
    VTRANS_OBJ_LOCK_INIT(tt);
    allTrans = tt;
    VTRANS_UNLOCK;
    return tt;
}

/* find a trans, again returning with high ref count */
struct volser_trans *
FindTrans(afs_int32 atrans)
{
    struct volser_trans *tt;
    VTRANS_LOCK;
    for (tt = allTrans; tt; tt = tt->next) {
	if (tt->tid == atrans) {
	    tt->time = FT_ApproxTime();
	    tt->refCount++;
	    VTRANS_UNLOCK;
	    return tt;
	}
    }
    VTRANS_UNLOCK;
    return (struct volser_trans *)0;
}

/* delete transaction if refcount == 1, otherwise queue delete for later.  Does implicit TRELE */
afs_int32
DeleteTrans(struct volser_trans *atrans, afs_int32 lock)
{
    struct volser_trans *tt, **lt;
    Error error;

    if (lock) VTRANS_LOCK;
    if (atrans->refCount > 1) {
	/* someone else is using it now */
	atrans->refCount--;
	atrans->tflags |= TTDeleted;
	if (lock) VTRANS_UNLOCK;
	return 0;
    }

    /* otherwise we zap it ourselves */
    lt = &allTrans;
    for (tt = *lt; tt; lt = &tt->next, tt = *lt) {
	if (tt == atrans) {
	    if (tt->volume)
		VDetachVolume(&error, tt->volume);
	    tt->volume = NULL;
	    if (tt->rxCallPtr)
		rxi_CallError(tt->rxCallPtr, RX_CALL_DEAD);
	    *lt = tt->next;
            VTRANS_OBJ_LOCK_DESTROY(tt);
	    free(tt);
	    if (lock) VTRANS_UNLOCK;
	    return 0;
	}
    }
    if (lock) VTRANS_UNLOCK;
    return -1;			/* failed to find the transaction in the generic list */
}

/* THOLD is a macro defined in volser.h */

/* put a transaction back */
afs_int32
TRELE(struct volser_trans *at)
{
    VTRANS_LOCK;
    if (at->refCount == 0) {
	Log("TRELE: bad refcount\n");
	VTRANS_UNLOCK;
	return VOLSERTRELE_ERROR;
    }

    at->time = FT_ApproxTime();	/* we're still using it */
    if (at->refCount == 1 && (at->tflags & TTDeleted)) {
	DeleteTrans(at, 0);
	VTRANS_UNLOCK;
	return 0;
    }
    /* otherwise simply drop refcount */
    at->refCount--;
    VTRANS_UNLOCK;
    return 0;
}

/* look for old transactions and delete them */
#define	OLDTRANSTIME	    600	/* seconds */
#define	OLDTRANSWARN	    300	/* seconds */
static int GCDeletes = 0;
afs_int32
GCTrans(void)
{
    struct volser_trans *tt, *nt;
    afs_int32 now;

    now = FT_ApproxTime();

    VTRANS_LOCK;
    for (tt = allTrans; tt; tt = nt) {
	if (tt->time + OLDTRANSWARN < now) {
	    Log("trans %u on volume %" AFS_VOLID_FMT " %s than %d seconds\n", tt->tid,
		afs_printable_VolumeId_lu(tt->volid),
		((tt->refCount > 0) ? "is older" : "has been idle for more"),
		(((now - tt->time) / GCWAKEUP) * GCWAKEUP));
	}
	if ((tt->refCount <= 0) && (tt->time + OLDTRANSTIME < now)) {
	    Log("trans %u on volume %" AFS_VOLID_FMT " has timed out\n", tt->tid, afs_printable_VolumeId_lu(tt->volid));

	    tt->refCount++;	/* we're using it now */

	    if (tt->volume && V_destroyMe(tt->volume) == DESTROY_ME
	        && !(tt->vflags & VTDeleted)) {

		Error error;

		Log("Deleting timed-out temporary volume %lu\n",
		    (long unsigned) tt->volid);

		VTRANS_OBJ_LOCK(tt);
		tt->vflags |= VTDeleted;
		VTRANS_OBJ_UNLOCK(tt);

		VTRANS_UNLOCK;
		VPurgeVolume(&error, tt->volume);
		/*
		 * While the lock was dropped, tt->next may have changed.
		 * Therefore, defer reading tt->next until _after_ we regain the lock.
		 */
		VTRANS_LOCK;
	    }
	    nt = tt->next;
	    /*
	     * DeleteTrans() will decrement tt->refCount; if it falls to 0, it will
	     * also delete tt itself.  Therefore, we must read tt->next _before_
	     * calling DeleteTrans().
	     */
	    DeleteTrans(tt, 0);
	    GCDeletes++;
	    continue;
	}
	/*
	 * This path never dropped VTRANS_LOCK or modified the allTrans list.
	 * Therefore, no special care is required to determine the next trans in the chain.
	 */
	nt = tt->next;
    }
    VTRANS_UNLOCK;
    return 0;
}

/*return the head of the transaction list */
struct volser_trans *
TransList(void)
{
    return (allTrans);
}
