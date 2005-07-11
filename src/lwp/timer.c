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
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
* 								    *
\*******************************************************************/


#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/time.h>
#endif
#include <stdlib.h>

#define _TIMER_IMPL_
#include "timer.h"
#include "lwp.h"




typedef unsigned char bool;
#define FALSE	0
#define TRUE	1


#define expiration TotalTime

#define new_elem()	((struct TM_Elem *) malloc(sizeof(struct TM_Elem)))

#define MILLION	1000000

static int globalInitDone = 0;

/* t1 = t2 - t3 */
static void
subtract(register struct timeval *t1, register struct timeval *t2, 
	 register struct timeval *t3)
{
    register int sec2, usec2, sec3, usec3;

    sec2 = t2->tv_sec;
    usec2 = t2->tv_usec;
    sec3 = t3->tv_sec;
    usec3 = t3->tv_usec;

    /* Take care of the probably non-existent case where the
     * usec field has more than 1 second in it. */

    while (usec3 > usec2) {
	usec2 += MILLION;
	sec2--;
    }

    /* Check for a negative time and use zero for the answer,
     * since the tv_sec field is unsigned */

    if (sec3 > sec2) {
	t1->tv_usec = 0;
	t1->tv_sec = (afs_uint32) 0;
    } else {
	t1->tv_usec = usec2 - usec3;
	t1->tv_sec = sec2 - sec3;
    }
}

/* t1 += t2; */

static void
add(register struct timeval *t1, register struct timeval *t2)
{
    t1->tv_usec += t2->tv_usec;
    t1->tv_sec += t2->tv_sec;
    if (t1->tv_usec >= MILLION) {
	t1->tv_sec++;
	t1->tv_usec -= MILLION;
    }
}

/* t1 == t2 */

int
TM_eql(struct timeval *t1, struct timeval *t2)
{
    return (t1->tv_usec == t2->tv_usec) && (t1->tv_sec == t2->tv_sec);
}

/* t1 >= t2 */

/*
obsolete, commentless procedure, all done by hand expansion now.
static bool geq(register struct timeval *t1, register struct timeval *t2)
{
    return (t1->tv_sec > t2->tv_sec) ||
	   (t1->tv_sec == t2->tv_sec && t1->tv_usec >= t2->tv_usec);
}
*/

static bool
blocking(register struct TM_Elem *t)
{
    return (t->TotalTime.tv_sec < 0 || t->TotalTime.tv_usec < 0);
}



/*
    Initializes a list -- returns -1 if failure, else 0.
*/

int
TM_Init(register struct TM_Elem **list)
{
    if (!globalInitDone) {
	FT_Init(0, 0);
	globalInitDone = 1;
    }
    *list = new_elem();
    if (*list == NULL)
	return -1;
    else {
	(*list)->Next = *list;
	(*list)->Prev = *list;
	(*list)->TotalTime.tv_sec = 0;
	(*list)->TotalTime.tv_usec = 0;
	(*list)->TimeLeft.tv_sec = 0;
	(*list)->TimeLeft.tv_usec = 0;
	(*list)->BackPointer = NULL;

	return 0;
    }
}

int
TM_Final(register struct TM_Elem **list)
{
    if (list == NULL || *list == NULL)
	return -1;
    else {
	free(*list);
	*list = NULL;
	return 0;
    }
}

/*
    Inserts elem into the timer list pointed to by *tlistPtr.
*/

void
TM_Insert(struct TM_Elem *tlistPtr, struct TM_Elem *elem)
{
    register struct TM_Elem *next;

    /* TimeLeft must be set for function IOMGR with infinite timeouts */
    elem->TimeLeft = elem->TotalTime;

    /* Special case -- infinite timeout */
    if (blocking(elem)) {
	openafs_insque(elem, tlistPtr->Prev);
	return;
    }

    /* Finite timeout, set expiration time */
    FT_AGetTimeOfDay(&elem->expiration, 0);
    add(&elem->expiration, &elem->TimeLeft);
    next = NULL;
    FOR_ALL_ELTS(p, tlistPtr, {
		 if (blocking(p)
		     || !(elem->TimeLeft.tv_sec > p->TimeLeft.tv_sec
			  || (elem->TimeLeft.tv_sec == p->TimeLeft.tv_sec
			      && elem->TimeLeft.tv_usec >=
			      p->TimeLeft.tv_usec))
		 ) {
		 next = p;	/* Save ptr to element that will be after this one */
		 break;}
		 }
    )

	if (next == NULL)
	    next = tlistPtr;
    openafs_insque(elem, next->Prev);
}

/*
    Walks through the specified list and updates the TimeLeft fields in it.
    Returns number of expired elements in the list.
*/

int
TM_Rescan(struct TM_Elem *tlist)	/* head pointer of timer list */
{
    struct timeval time;
    register int expired;

#ifndef AFS_DJGPP_ENV
    FT_AGetTimeOfDay(&time, 0);
#else
    FT_GetTimeOfDay(&time, 0);	/* we need a real time value */
#endif
    expired = 0;
    FOR_ALL_ELTS(e, tlist, {
		 if (!blocking(e)) {
		 subtract(&e->TimeLeft, &e->expiration, &time);
		 if (0 > e->TimeLeft.tv_sec
		     || (0 == e->TimeLeft.tv_sec && 0 >= e->TimeLeft.tv_usec))
		 expired++;}
		 }
    )
	return expired;
}

/*
    RETURNS POINTER TO earliest expired entry from tlist.
    Returns 0 if no expired entries are present.
*/

struct TM_Elem *
TM_GetExpired(struct TM_Elem *tlist)	/* head pointer of timer list */
{
    FOR_ALL_ELTS(e, tlist, {
		 if (!blocking(e)
		     && (0 > e->TimeLeft.tv_sec
			 || (0 == e->TimeLeft.tv_sec
			     && 0 >= e->TimeLeft.tv_usec)))
		 return e;}
    )
	return NULL;
}

/*
    Returns a pointer to the earliest unexpired element in tlist.
    Its TimeLeft field will specify how much time is left.
    Returns 0 if tlist is empty or if there are no unexpired elements.
*/

struct TM_Elem *
TM_GetEarliest(struct TM_Elem *tlist)
{
    register struct TM_Elem *e;

    e = tlist->Next;
    return (e == tlist ? NULL : e);
}

/* This used to be in hputils.c, but it's only use is in the LWP package. */
/*
 * Emulate the vax instructions for queue insertion and deletion, somewhat.
 * A std_queue structure is defined here and used by these routines.  These
 * routines use caddr_ts so they can operate on any structure.  The std_queue
 * structure is used rather than proc structures so that when the proc struct
 * changes only process management code breaks.  The ideal solution would be
 * to define a std_queue as a global type which is part of all the structures
 * which are manipulated by these routines.  This would involve considerable
 * effort...
 */

void
openafs_insque(struct TM_Elem *elementp, struct TM_Elem *quep)
{
    elementp->Next = quep->Next;
    elementp->Prev = quep;

    quep->Next->Prev = elementp;
    quep->Next = elementp;
}

void
openafs_remque(struct TM_Elem *elementp)
{
    elementp->Next->Prev = elementp->Prev;
    elementp->Prev->Next = elementp->Next;
    elementp->Prev = elementp->Next = NULL;
}
