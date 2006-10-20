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

#ifndef DJGPP
#include <windows.h>
#endif /* !DJGPP */
#include "osi.h"
#include <stdlib.h>

/* critical section protecting allocation of osi_queueData_t elements */
Crit_Sec osi_qdcrit;

/* free list of queue elements */
osi_queueData_t *osi_QDFreeListp = NULL;

void osi_QAdd(osi_queue_t **headpp, osi_queue_t *eltp)
{
	osi_queue_t *tp;

	/* and both paths do the following; do this early to keep
	 * machine busy while processing delay on conditional check
	 */
	eltp->prevp = NULL;

	if (tp = *headpp) {
		/* there is one element here */
		eltp->nextp = tp;
		tp->prevp = eltp;
	}
	else {
		/* we're the first */
		eltp->nextp = NULL;
	}

	/* and both paths do the following */
	*headpp = eltp;
}

void osi_QAddH(osi_queue_t **headpp, osi_queue_t **tailpp, osi_queue_t *eltp)
{
	osi_queue_t *tp;

	/* and both paths do the following; do this early to keep
	 * machine busy while processing delay on conditional check
	 */
	eltp->prevp = NULL;

	if (tp = *headpp) {
		/* there is one element here */
		eltp->nextp = tp;
		tp->prevp = eltp;
	}
	else {
		/* we're the first */
		eltp->nextp = NULL;
                *tailpp = eltp;
	}

	/* and both paths do the following */
	*headpp = eltp;
}

void osi_QAddT(osi_queue_t **headpp, osi_queue_t **tailpp, osi_queue_t *eltp)
{
	osi_queue_t *tp;

	eltp->nextp = NULL;

	if (tp = *tailpp) {
		/* there's at least one element in the list; append ourselves */
                eltp->prevp = tp;
                tp->nextp = eltp;
                *tailpp = eltp;
        }
        else {
		/* we're the only element in the list */
                *headpp = eltp;
                *tailpp = eltp;
                eltp->prevp = NULL;
        }
}

void osi_QRemove(osi_queue_t **headpp, osi_queue_t *eltp)
{
    osi_queue_t *np = eltp->nextp;	/* next dude */
    osi_queue_t *pp = eltp->prevp;	/* prev dude */

    if (eltp == *headpp) {
	/* we're the first element in the list */
	*headpp = np;
	if (np) 
	    np->prevp = NULL;
    }
    else {
	pp->nextp = np;
	if (np) 
	    np->prevp = pp;
    }
    eltp->prevp = NULL;
    eltp->nextp = NULL;
}

void osi_QRemoveHT(osi_queue_t **headpp, osi_queue_t **tailpp, osi_queue_t *eltp)
{
    osi_queue_t *np = eltp->nextp;	/* next dude */
    osi_queue_t *pp = eltp->prevp;	/* prev dude */

    if (eltp == *headpp && eltp == *tailpp) 
    {
    	*headpp = *tailpp = NULL;
    }
    else if (eltp == *headpp) {
	/* we're the first element in the list */
	*headpp = np;
	if (np) 
	    np->prevp = NULL;
    }	
    else if (eltp == *tailpp) {
	/* we're the last element in the list */
	*tailpp = pp;
	if (pp) 
	    pp->nextp = NULL;
    }	
    else {
	if (pp)
		pp->nextp = np;
	if (np)
		np->prevp = pp;
    }
    eltp->prevp = NULL; 
    eltp->nextp = NULL;
}

void osi_InitQueue(void)
{
	static int initd = 0;

	if (initd) return;

	initd = 1;
	thrd_InitCrit(&osi_qdcrit);
}

osi_queueData_t *osi_QDAlloc(void)
{
	osi_queueData_t *tp;
	int i;

	thrd_EnterCrit(&osi_qdcrit);
	if (tp = osi_QDFreeListp) {
		osi_QDFreeListp = (osi_queueData_t *) tp->q.nextp;
	}
	else {
		/* need to allocate a block more */
		tp = (osi_queueData_t *) malloc(OSI_NQDALLOC * sizeof(osi_queueData_t));

		/* leave last guy off of the free list; this is the one we'll
		 * return.
		 */
		for(i=0; i<OSI_NQDALLOC-1; i++, tp++) {
			tp->q.nextp = (osi_queue_t *) osi_QDFreeListp;
                        tp->datap = NULL;
			osi_QDFreeListp = tp;
		}
		
		/* when we get here, tp is pointing to the last dude allocated.
		 * This guy wasn't put on the free list, so we can return him now.
		 */
                tp->datap = NULL;
	}
	thrd_LeaveCrit(&osi_qdcrit);

	osi_assertx(tp->datap == NULL, "queue freelist screwup");

	return tp;
}

void osi_QDFree(osi_queueData_t *qp)
{
	thrd_EnterCrit(&osi_qdcrit);
	qp->q.nextp = (osi_queue_t *) osi_QDFreeListp;
        qp->datap = NULL;
	osi_QDFreeListp = qp;
        thrd_LeaveCrit(&osi_qdcrit);
}
