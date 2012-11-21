/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* A better queue implementation.
 *
 * This differs from the original queue implementation in that that would
 * only allow a structure to be threaded onto a single queue. This permits
 * a given object to be on multiple queues, providing that each queue has
 * a place in the structure definition.
 */

#ifndef OPENAFS_OPR_QUEUE_H
#define OPENAFS_OPR_QUEUE_H 1

#ifndef KERNEL
#include <stdlib.h>
#else
#ifndef NULL
#define NULL (void *)0
#endif
#endif

struct opr_queue {
   struct opr_queue *next;
   struct opr_queue *prev;
};

#define opr_queue_Scan(head, cursor) \
    cursor = (head)->next; cursor != (head); cursor = cursor->next

#define opr_queue_ScanSafe(head, cursor, store) \
    cursor = (head)->next, store = cursor->next; \
    cursor != (head); \
    cursor = store, store = store->next

#define opr_queue_ScanBackwards(head, cursor) \
    cursor = (head)->prev; cursor != (head); cursor = cursor->prev

#define opr_queue_ScanBackwardsSafe(head, cursor, store) \
   cursor = (head)->prev, store = cursor->prev; \
   cursor != (head); \
   cursor = store, store = store->prev

static_inline void
opr_queue_Zero(struct opr_queue *q) {
    q->prev = q->next = NULL;
}

static_inline void
opr_queue_Init(struct opr_queue *q) {
    q->prev = q->next = q;
}

static_inline void
opr_queue_add(struct opr_queue *element,
	      struct opr_queue *before, struct opr_queue *after) {
   after->prev = element;
   element->next = after;
   element->prev = before;
   before->next = element;
}

static_inline void
opr_queue_Append(struct opr_queue *queue, struct opr_queue *element) {
    opr_queue_add(element, queue->prev, queue);
}

static_inline void
opr_queue_Prepend(struct opr_queue *queue, struct opr_queue *element) {
    opr_queue_add(element, queue, queue->next);
}

static_inline void
opr_queue_InsertBefore(struct opr_queue *exist, struct opr_queue *adding) {
    /* This may seem back to front, but take a list A, B, C where we want
     * to add 1 before B. So, we're adding 1 with A the element before,
     * and B the element after, hence the following: */
    opr_queue_add(adding, exist->prev, exist);
}

static_inline void
opr_queue_InsertAfter(struct opr_queue *exist, struct opr_queue *adding) {
    opr_queue_add(adding, exist, exist->next);
}

static_inline void
opr_queue_Remove(struct opr_queue *element) {
    element->next->prev = element->prev;
    element->prev->next = element->next;
    element->prev = NULL;
    element->next = NULL;
}

static_inline int
opr_queue_IsEmpty(struct opr_queue *q) {
    return (q->prev == q);
}

static_inline int
opr_queue_IsOnQueue(struct opr_queue *q) {
    return (q->prev != NULL);
}

static_inline int
opr_queue_IsEnd(struct opr_queue *q, struct opr_queue *cursor) {
    return (cursor == q);
}

static_inline int
opr_queue_IsLast(struct opr_queue *q, struct opr_queue *cursor) {
    return (cursor->next == q);
}

static_inline int
opr_queue_Count(struct opr_queue *q) {
    struct opr_queue *cursor;
    int n = 0;

    for (opr_queue_Scan(q, cursor)) {
	n++;
    }
    return n;
}

static_inline void
opr_queue_Swap(struct opr_queue *a, struct opr_queue *b)
{
    struct opr_queue tq = *b;

    if (a->prev == a) {
	b->prev = b->next = b;
    } else {
	*b = *a;
	b->prev->next = b;
	b->next->prev = b;
    }

    if (tq.prev == b) {
	a->prev = a->next = a;
    } else {
	*a = tq;
	a->prev->next = a;
	a->next->prev = a;
    }
}

/* Remove the members before pivot from Q1, and append them to Q2 */

static_inline void
opr_queue_SplitBeforeAppend(struct opr_queue *q1, struct opr_queue *q2,
			    struct opr_queue *pivot) {

    if (q1 == pivot->prev)
	return;
    /* Add ourselves to the end of list 2 */
    q2->prev->next = q1->next; /* end of list 2, is now the start of list 1 */
    q1->next->prev = q2->prev;
    pivot->prev->next = q2; /* entry before the pivot is it at end of q2 */
    q2->prev = pivot->prev;

    /* Pull ourselves out of list q1. */
    q1->next = pivot;
    pivot->prev = q1;
}

/* Remove the members after the pivot from Q1, and prepend them onto Q2 */
static_inline void
opr_queue_SplitAfterPrepend(struct opr_queue *q1, struct opr_queue *q2,
			    struct opr_queue *pivot) {

    if (q1 == pivot->next)
	return;

    /* start of q2 has last element of q1 before it */
    q2->next->prev = q1->prev;
    q1->prev->next = q2->next;
    /* item that we're breaking at (pivot->next) is at start of q2 */
    q2->next = pivot->next;
    pivot->next->prev = q2;

    /* Q1 now ends after pivot */
    pivot->next = q1;
    q1->prev = pivot;
}

static_inline void
opr_queue_SpliceAppend(struct opr_queue *target, struct opr_queue *source)
{

    if (source->next == source)
	return;

    /* Stick the contents of source onto the end of target */
    target->prev->next = source->next;
    source->next->prev = target->prev;
    source->prev->next = target;
    target->prev = source->prev;

    /* Reinitialise source */
    source->next = source->prev = source;
}

static_inline void
opr_queue_SplicePrepend(struct opr_queue *target, struct opr_queue *source)
{

    if (source->next == source)
	return;

    /* Contents of source go onto the beginning of target */
    target->next->prev = source->prev;
    source->prev->next = target->next;
    source->next->prev = target;
    target->next = source->next;

    /* Reinitialise source */
    source->next = source->prev = source;
}

#define opr_queue_Entry(queue, structure, member) \
    ((structure *)((char *)(queue)-(char *)(&((structure *)NULL)->member)))

#define opr_queue_First(queue, structure, member) \
    opr_queue_Entry((queue)->next, structure, member)

#define opr_queue_Last(queue, structure, member) \
    opr_queue_Entry((queue)->prev, structure, member)

#define opr_queue_Next(entry, structure, member) \
    opr_queue_Entry((entry)->next, structure, member)

#define opr_queue_Prev(entry, structure, member) \
    opr_queue_Entry((entry)->prev, structure, member)

#endif
