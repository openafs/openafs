/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_timout.c
 *
 * Implements:
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */
#include "sys/limits.h"
#include "sys/types.h"
#include "sys/user.h"
#include "sys/pri.h"
#include "sys/priv.h"		/* XXX */
#include "sys/lockl.h"
#include "sys/malloc.h"
#include <sys/timer.h>		/* For the timer related defines                */
#include <sys/intr.h>		/* for the serialization defines                */

/* NOTE: This lock makes the callout table MP-safe. timeout itself could
 * be subject to deadlocks if used for anything more complex than we are
 * doing here.
 */
Simple_lock afs_callout_lock;

#define AFS_DISABLE_LOCK(_pri, _lock) disable_lock((_pri), (_lock))
#define AFS_UNLOCK_ENABLE(_pri, _lock) unlock_enable((_pri), (_lock))


struct tos {
    struct tos *toprev;		/* previous tos in callout table */
    struct tos *tonext;		/* next tos in callout table    */
    struct trb *trb;		/* this timer request block     */
    afs_int32 type;
    long p1;
};

struct callo {
    int ncallo;			/* number of callout table elements     */
    struct tos *head;		/* callout table head element           */
};

struct callo afs_callo = { 0, NULL };	/* callout table anchor                     */

static void timeout_end(struct trb *);	/* timeout()'s timeout function */
extern void tstart(struct trb *);
extern void i_enable(int);

void
timeout(void (*func) (),	/* function to call at timeout */
	caddr_t arg,	/*   It's argument. */
	int ticks,	/* when to set timeout for */
	int type, char *p1)
{
    int ipri;		/* caller's interrupt priority  */
    struct tos *tos;	/* tos to use for timeout       */
    struct trb *trb;	/* trb in the tos being used    */
    struct itimerstruc_t tv;	/* timeout interval             */

    tv.it_value.tv_sec = ticks / HZ;
    tv.it_value.tv_nsec = (ticks % HZ) * (NS_PER_SEC / HZ);

    osi_Assert(afs_callo.ncallo != 0);

  timeout_retry:

    ipri = AFS_DISABLE_LOCK(INTMAX, &afs_callout_lock);
    /*
     *  Run the callout table chain to see if there is already a pending
     *  timeout for the specified function.  If so, that timeout will
     *  be cancelled and the tos re-used.
     */
    for (tos = afs_callo.head; tos != NULL; tos = tos->tonext) {
	if ((tos->trb->tof == func) && (tos->trb->func_data == (ulong) arg)) {
	    break;
	}
    }

    /*
     *  If a pending timeout for the specified function was NOT found,
     *  then the callout table chain will have to be run to find an
     *  unused tos.
     */
    if (tos == NULL) {
	for (tos = afs_callo.head; tos != NULL; tos = tos->tonext) {
	    if (tos->trb->tof == NULL) {
		break;
	    }
	}

	/*
	 *  If there isn't an available tos, then there is no error
	 *  recovery.  This means that either the caller has not
	 *  correctly registered the number of callout table entries
	 *  that would be needed or is incorrectly using the ones that
	 *  were registered.  Either way, panic is the only recourse.
	 */
	osi_Assert(tos != NULL);
    }
    /* 
     *  A pending timeout for the specified function WAS found.
     *  If the request is still active, stop it.
     */
    while (tstop(tos->trb)) {
	AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);
	goto timeout_retry;
    }

    tos->type = type;		/* Temp */
    tos->p1 = (long)p1;		/* Temp */
    tos->trb->knext = NULL;
    tos->trb->kprev = NULL;
    tos->trb->flags = 0;
    tos->trb->timeout = tv;
    tos->trb->tof = func;
    tos->trb->func = timeout_end;
    tos->trb->func_data = (ulong) arg;
    tos->trb->ipri = INTTIMER;
    tos->trb->id = -1;
    tstart(tos->trb);

    AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);
}


void
untimeout(void (*func) (), ulong arg)
{
    int ipri;		/* caller's interrupt priority  */
    struct tos *tos;	/* tos to walk callout table    */
    struct trb *trb;	/* trb for this tos             */

  untimeout_retry:

    ipri = AFS_DISABLE_LOCK(INTMAX, &afs_callout_lock);
    /*  Run the callout table chain looking for the timeout.  */
    for (tos = afs_callo.head; tos != NULL; tos = tos->tonext) {
	if (tos->trb->tof == func && tos->trb->func_data == arg) {
	    break;
	}
    }

    if (tos) {
	/*
	 *  Found it on the timeout list - stop the pending timeout 
	 *  if it is active.
	 */
	while (tstop(tos->trb)) {
	    AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);
	    goto untimeout_retry;
	}

	/*  Mark this callout table entry as free.  */
	tos->trb->knext = NULL;
	tos->trb->kprev = NULL;
	tos->trb->tof = NULL;
    }
    AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);
}


static void
timeout_end(struct trb *trb)
{				/* trb of the current timeout     */
    void (*func) ();	/* function to call at timeout  */
    int ipri;
    ipri = AFS_DISABLE_LOCK(INTMAX, &afs_callout_lock);

    func = trb->tof;
    trb->func = NULL;
    trb->tof = NULL;		/* Zero out pointer to user function  */

    AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);

    (*func) (trb->func_data);
    /* for compatibility with untimeout() */
}


int
timeoutcf(int cocnt)
{				/* # entries to change callout table by   */
    int ipri;		/* caller's interrupt priority  */
    int rv;		/* return value to the caller           */
    struct tos *tos;	/* tos to add to/remove from table    */
    struct trb *trb;	/* trb in the tos to be added/removed */

    rv = 0;

    if (cocnt > 0) {
	/*
	 *  Callout table is being enlarged - keep working until the
	 *  right number of elements have been added.
	 */
	while (cocnt > 0) {
	    /*  Allocate a timer request block.  */
	    trb = (struct trb *)talloc();

	    /*
	     *  If the low-level timer service could not provide
	     *  a trb, the callout table can't be expanded any
	     *  more so get out.
	     */
	    if (trb == NULL) {
		rv = -1;
		break;
	    }

	    /*  Allocate memory for the callout table structure.  */
	    tos = (struct tos *)
		xmalloc((uint) sizeof(struct tos), (uint) 0, pinned_heap);

	    /*
	     *  If memory couldn't be allocated for the tos, the
	     *  callout table can't be expanded any more so get out.
	     */
	    if (tos == NULL) {
		rv = -1;
		break;
	    } else {
		memset(tos, 0, sizeof(struct tos));
	    }

	    /*  The trb and the tos were both allocated.  */
	    tos->trb = trb;
#ifdef DEBUG
	    /* 
	     *  Debug code to ensure that the low-level timer 
	     *  service talloc() clears out the pointers.
	     */
	    osi_Assert(trb->knext == NULL);
	    osi_Assert(trb->kprev == NULL);
#endif /* DEBUG */

	    ipri = AFS_DISABLE_LOCK(INTMAX, &afs_callout_lock);
	    if (afs_callo.head == NULL) {
		/*
		 *  The callout table is currently empty.  This
		 *  is the easy case, just set the head of the
		 *  callout chain to this tos.
		 */
		afs_callo.head = tos;
	    } else {
		/*
		 *  The callout table is not empty.  Chain this
		 *  trb to the head of the callout chain.
		 */
		tos->tonext = afs_callo.head;
		afs_callo.head->toprev = tos;
		afs_callo.head = tos;
	    }

	    /*  Just finished adding a trb to the callout table.  */
	    afs_callo.ncallo++;
	    cocnt--;
	    AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);
	}
    } else {
	/*
	 *  Callout table is being shrunk - keep working until the
	 *  right number of elements have been removed being careful
	 *  only to remove elements which do not belong to timeout
	 *  requests that are currently active.
	 */
	if (cocnt < 0) {

	    /*
	     *  There had better be at least as many tos's in
	     *  the callout table as the size by which the caller 
	     *  wants to decrease the size of the table.
	     */
	    osi_Assert(afs_callo.ncallo >= -cocnt);

	    while (cocnt < 0) {

		/*
		 *  Start from the head of the callout chain,
		 *  making sure that there is a tos at the 
		 *  head (i.e. that there is a callout chain).
		 */
		ipri = AFS_DISABLE_LOCK(INTMAX, &afs_callout_lock);
		tos = afs_callo.head;
		osi_Assert(tos != NULL);

		/*
		 *  Keep walking down the callout chain until
		 *  a tos is found which is not currently 
		 *  active.
		 */
		while ((tos != NULL) && (tos->trb->tof != NULL)) {
		    tos = tos->tonext;
		}

		/*
		 *  If trb is not NULL, then there was not a
		 *  callout table entry that wasn't set to
		 *  timeout.  Panic.
		 */
		osi_Assert(tos != NULL);

		/*
		 *  Found a free callout table element, free
		 *  it and remove it from the callout table.
		 */
		tfree(tos->trb);
		if (afs_callo.head == tos) {
		    afs_callo.head = tos->tonext;
		    if (afs_callo.head != NULL) {
			afs_callo.head->toprev = NULL;
		    }
		} else {
		    osi_Assert(tos->toprev != NULL);
		    tos->toprev->tonext = tos->tonext;
		    if (tos->tonext != NULL) {
			tos->tonext->toprev = tos->toprev;
		    }
		}
		/*
		 *  Just finished removing a trb from the
		 *  callout table.
		 */
		afs_callo.ncallo--;
		cocnt++;
		AFS_UNLOCK_ENABLE(ipri, &afs_callout_lock);
		xmfree((void *)tos, pinned_heap);

	    }
	}
    }

    return (rv);
}
