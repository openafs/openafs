/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/stds.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

RCSID
    ("$Header: /cvs/openafs/src/bucoord/dlq.c,v 1.9.2.1 2007/01/05 03:34:09 shadow Exp $");

#include "bc.h"
#include <afs/bubasics.h>

/* protos */
int dlqEmpty(dlqlinkP );
int dlqInit(dlqlinkP headptr);
int dlqLinkf(dlqlinkP, dlqlinkP );
int dlqLinkb(dlqlinkP, dlqlinkP );
void dlqUnlink( dlqlinkP );
int dlqTraverseQueue(dlqlinkP,  int *(), int *());
int dlqCount(dlqlinkP );
void dlqMoveb( dlqlinkP, dlqlinkP);
dlqlinkP dlqUnlinkb(dlqlinkP );
dlqlinkP dlqUnlinkf(dlqlinkP );
dlqlinkP dlqFront(dlqlinkP headptr);

#define	DLQ_ASSERT_HEAD(headptr)				\
        if ( (headptr)->dlq_type != DLQ_HEAD )			\
	{							\
	    printf("file %s line %d, invalid queue head\n",	\
		   __FILE__, __LINE__);				\
	    exit(1);						\
	}


/* dlqEmpty
 * exit:
 *	1 - queue is empty
 *	0 - items on queue
 */

int dlqEmpty(dlqlinkP headptr )
{
    DLQ_ASSERT_HEAD(headptr);
    if (headptr->dlq_next == headptr)
	return (1);
    return (0);
}

int dlqInit(dlqlinkP headptr)
{
    headptr->dlq_next = headptr;
    headptr->dlq_prev = headptr;
    headptr->dlq_type = DLQ_HEAD;
    headptr->dlq_structPtr = NULL;
    return (0);
}

/* dlqLinkf
 *	link item to front of chain
 */
int dlqLinkf(dlqlinkP headptr, dlqlinkP entryptr)
{
    DLQ_ASSERT_HEAD(headptr);
    /* link in as first item in chain */
    entryptr->dlq_next = headptr->dlq_next;
    headptr->dlq_next->dlq_prev = entryptr;
    entryptr->dlq_prev = headptr;
    headptr->dlq_next = entryptr;
    return (0);
}

/* dlqLinkb
 *	link item to end of chain
 */

int dlqLinkb(dlqlinkP headptr, dlqlinkP entryptr)
{
    DLQ_ASSERT_HEAD(headptr);
    entryptr->dlq_next = headptr;
    entryptr->dlq_prev = headptr->dlq_prev;

    headptr->dlq_prev = entryptr;
    entryptr->dlq_prev->dlq_next = entryptr;
    return (0);
}

/* dlqMoveb
 *	move all the items on the fromptr and append to the toptr's list
 */

void dlqMoveb( dlqlinkP fromptr, dlqlinkP toptr)
{
    dlqlinkP tailptr;

    DLQ_ASSERT_HEAD(fromptr);
    DLQ_ASSERT_HEAD(toptr);

    if (dlqEmpty(fromptr))
	return;

    tailptr = toptr->dlq_prev;

    tailptr->dlq_next = fromptr->dlq_next;
    tailptr->dlq_next->dlq_prev = tailptr;

    /* now fix up the last item in the new chain */
    tailptr = fromptr->dlq_prev;

    tailptr->dlq_next = toptr;
    toptr->dlq_prev = tailptr;

    fromptr->dlq_next = fromptr;
    fromptr->dlq_prev = fromptr;
    return;
}

/* dlqUnlinkb
 *	unlink the last item on the queue
 */

dlqlinkP dlqUnlinkb(dlqlinkP headptr)
{
    dlqlinkP ptr;
    DLQ_ASSERT_HEAD(headptr);

    if (dlqEmpty(headptr))
	return (0);

    ptr = headptr->dlq_prev;
    ptr->dlq_prev->dlq_next = headptr;
    headptr->dlq_prev = ptr->dlq_prev;

    ptr->dlq_next = ptr;
    ptr->dlq_prev = ptr;
    return (ptr);
}

/* dlqUnlinkf
 *	unlink the item on the front of the queue
 */

dlqlinkP dlqUnlinkf(dlqlinkP headptr) 
{
    dlqlinkP ptr;
    DLQ_ASSERT_HEAD(headptr);

    if (dlqEmpty(headptr))
	return (0);

    ptr = headptr->dlq_next;

    headptr->dlq_next = ptr->dlq_next;
    ptr->dlq_next->dlq_prev = headptr;

    ptr->dlq_next = ptr;
    ptr->dlq_prev = ptr;
    return (ptr);
}

/* dlqUnlink
 *	unlink the specified item from the queue.
 */

void dlqUnlink( dlqlinkP ptr)
{
    /* must not be the queue head */
    if (ptr->dlq_type == DLQ_HEAD) {
	printf("dlqUnlink: invalid unlink\n");
	exit(1);
    }

    ptr->dlq_prev->dlq_next = ptr->dlq_next;
    ptr->dlq_next->dlq_prev = ptr->dlq_prev;

    ptr->dlq_next = 0;
    ptr->dlq_prev = 0;
}

/* dlqFront
 *	return point to item at front of queuen
 */

dlqlinkP dlqFront(dlqlinkP headptr)
{
    DLQ_ASSERT_HEAD(headptr);

    if (dlqEmpty(headptr))
	return (0);

    return (headptr->dlq_next);
}

int dlqCount(dlqlinkP headptr)
{
    dlqlinkP ptr;
    int count = 0;

    DLQ_ASSERT_HEAD(headptr);

    ptr = headptr->dlq_next;
    while (ptr != headptr) {
	ptr = ptr->dlq_next;
	count++;
    }
    return (count);
}

int dlqTraverseQueue(dlqlinkP headptr, int (*fn1()), int (*fn2()))
{
    dlqlinkP ptr, oldPtr;

    DLQ_ASSERT_HEAD(headptr);

    ptr = headptr->dlq_next;
    while (ptr != headptr) {
	if (fn2 && ptr->dlq_structPtr)
	    (*fn2) (ptr->dlq_structPtr);
	oldPtr = ptr;
	ptr = ptr->dlq_next;
	if (fn1)
	    (*fn1) (oldPtr);
    }
    return (0);
}
