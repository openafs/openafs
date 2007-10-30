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

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <afs/afsutil.h>
#else
#include <sys/time.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <afs/afsint.h>
#include <signal.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <lock.h>
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

#include "volser.h"

/*@printflike@*/ extern void Log(const char *format, ...);

static struct volser_trans *allTrans = 0;
static afs_int32 transCounter = 1;

/* create a new transaction, returning ptr to same with high ref count */
struct volser_trans *
NewTrans(afs_int32 avol, afs_int32 apart)
{
    /* set volid, next, partition */
    register struct volser_trans *tt;
    struct timeval tp;
    struct timezone tzp;

    VTRANS_LOCK;
    /* don't allow the same volume to be attached twice */
    for (tt = allTrans; tt; tt = tt->next) {
	if ((tt->volid == avol) && (tt->partition == apart)) {
	    VTRANS_UNLOCK;
	    return (struct volser_trans *)0;	/* volume busy */
	}
    }
    VTRANS_UNLOCK;
    tt = (struct volser_trans *)malloc(sizeof(struct volser_trans));
    memset(tt, 0, sizeof(struct volser_trans));
    tt->volid = avol;
    tt->partition = apart;
    tt->refCount = 1;
    tt->rxCallPtr = (struct rx_call *)0;
    strcpy(tt->lastProcName, "");
    gettimeofday(&tp, &tzp);
    tt->creationTime = tp.tv_sec;
    tt->time = FT_ApproxTime();
    VTRANS_LOCK;
    tt->tid = transCounter++;
    tt->next = allTrans;
    allTrans = tt;
    VTRANS_UNLOCK;
    return tt;
}

/* find a trans, again returning with high ref count */
struct volser_trans *
FindTrans(register afs_int32 atrans)
{
    register struct volser_trans *tt;
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
DeleteTrans(register struct volser_trans *atrans, afs_int32 lock)
{
    register struct volser_trans *tt, **lt;
    afs_int32 error;

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
TRELE(register struct volser_trans *at)
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
GCTrans()
{
    register struct volser_trans *tt, *nt;
    afs_int32 now;

    now = FT_ApproxTime();

    VTRANS_LOCK;
    for (tt = allTrans; tt; tt = nt) {
	nt = tt->next;		/* remember in case we zap it */
	if (tt->time + OLDTRANSWARN < now) {
	    Log("trans %u on volume %u %s than %d seconds\n", tt->tid,
		tt->volid,
		((tt->refCount > 0) ? "is older" : "has been idle for more"),
		(((now - tt->time) / GCWAKEUP) * GCWAKEUP));
	}
	if (tt->refCount > 0)
	    continue;
	if (tt->time + OLDTRANSTIME < now) {
	    Log("trans %u on volume %u has timed out\n", tt->tid, tt->volid);
	    tt->refCount++;	/* we're using it now */
	    DeleteTrans(tt, 0);	/* drops refCount or deletes it */
	    GCDeletes++;
	}
    }
    VTRANS_UNLOCK;
    return 0;
}

/*return the head of the transaction list */
struct volser_trans *
TransList()
{
    return (allTrans);
}
