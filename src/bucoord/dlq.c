/* Copyright (C)  1998  Transarc Corporation.  All rights reserved. */

#include <afs/param.h>
#include <afs/bubasics.h>

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

dlqEmpty(headptr)
     dlqlinkP	headptr;
{
    DLQ_ASSERT_HEAD(headptr);
    if ( headptr->dlq_next == headptr )
	return(1);
    return(0);
}

dlqInit(headptr)
     dlqlinkP	headptr;
{
    headptr->dlq_next = headptr;
    headptr->dlq_prev = headptr;
    headptr->dlq_type = DLQ_HEAD;
    headptr->dlq_structPtr = (char *)0;
    return(0);
}

/* dlqLinkf
 *	link item to front of chain
 */
dlqLinkf(headptr, entryptr)
     dlqlinkP	headptr;
     dlqlinkP	entryptr;
{
    DLQ_ASSERT_HEAD(headptr);
    /* link in as first item in chain */
    entryptr->dlq_next = headptr->dlq_next;
    headptr->dlq_next->dlq_prev = entryptr;
    entryptr->dlq_prev = headptr;
    headptr->dlq_next = entryptr;
    return(0);
}	

/* dlqLinkb
 *	link item to end of chain
 */

dlqLinkb(headptr, entryptr)
     dlqlinkP	headptr;
     dlqlinkP	entryptr;
{
    DLQ_ASSERT_HEAD(headptr);
    entryptr->dlq_next = headptr;
    entryptr->dlq_prev = headptr->dlq_prev;

    headptr->dlq_prev = entryptr;
    entryptr->dlq_prev->dlq_next = entryptr;
    return(0);
}

/* dlqMoveb
 *	move all the items on the fromptr and append to the toptr's list
 */

dlqMoveb(fromptr, toptr)
     dlqlinkP	fromptr;
     dlqlinkP	toptr;
{
    dlqlinkP	tailptr;

    DLQ_ASSERT_HEAD(fromptr);
    DLQ_ASSERT_HEAD(toptr);

    if ( dlqEmpty(fromptr) )
	return(0);

    tailptr = toptr->dlq_prev;

    tailptr->dlq_next = fromptr->dlq_next;
    tailptr->dlq_next->dlq_prev = tailptr;

    /* now fix up the last item in the new chain */
    tailptr = fromptr->dlq_prev;

    tailptr->dlq_next = toptr;
    toptr->dlq_prev = tailptr;

    fromptr->dlq_next = fromptr;
    fromptr->dlq_prev = fromptr;
}

/* dlqUnlinkb
 *	unlink the last item on the queue
 */

dlqlinkP
dlqUnlinkb(headptr) 
     dlqlinkP	headptr;
{
    dlqlinkP	ptr;
    DLQ_ASSERT_HEAD(headptr);

    if ( dlqEmpty(headptr) )
	return(0);

    ptr = headptr->dlq_prev;
    ptr->dlq_prev->dlq_next = headptr;
    headptr->dlq_prev = ptr->dlq_prev;
    
    ptr->dlq_next = ptr;
    ptr->dlq_prev = ptr;
    return(ptr);
}

/* dlqUnlinkf
 *	unlink the item on the front of the queue
 */

dlqlinkP
dlqUnlinkf(headptr)
     dlqlinkP	headptr;
{
    dlqlinkP	ptr;
    DLQ_ASSERT_HEAD(headptr);

    if ( dlqEmpty(headptr) )
	return(0);

    ptr = headptr->dlq_next;

    headptr->dlq_next = ptr->dlq_next;
    ptr->dlq_next->dlq_prev = headptr;

    ptr->dlq_next = ptr;
    ptr->dlq_prev = ptr;
    return(ptr);
}

/* dlqUnlink
 *	unlink the specified item from the queue.
 */

dlqUnlink(ptr)
     dlqlinkP    ptr;
{
    /* must not be the queue head */
    if ( ptr->dlq_type == DLQ_HEAD )
    {
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

dlqlinkP
dlqFront(headptr)
     dlqlinkP	headptr;
{
    DLQ_ASSERT_HEAD(headptr);

    if ( dlqEmpty(headptr) )
	return(0);

    return(headptr->dlq_next);
}

int
dlqCount(headptr)
     dlqlinkP	headptr;
{
    dlqlinkP    ptr;
    int count = 0;

    DLQ_ASSERT_HEAD(headptr);

    ptr = headptr->dlq_next;
    while ( ptr != headptr )
    {
	ptr = ptr->dlq_next;
	count++;
    }
    return(count);
}

dlqTraverseQueue(headptr, fn1, fn2)
     dlqlinkP	headptr;
     int (*fn1)();
     int (*fn2)();
{
    dlqlinkP    ptr, oldPtr;

    DLQ_ASSERT_HEAD(headptr);

    ptr = headptr->dlq_next;
    while ( ptr != headptr )
    {
	if (fn2 && ptr->dlq_structPtr)
	    (*fn2)(ptr->dlq_structPtr);
	oldPtr = ptr;
	ptr = ptr->dlq_next;
	if (fn1) (*fn1)(oldPtr);
    }
    return(0);
}




