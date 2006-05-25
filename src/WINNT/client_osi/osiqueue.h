/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSI_QUEUE_H_ENV_
#define _OSI_QUEUE_H_ENV_ 1

/* this package implements a doubly linked queue of elements.
 * Each element starts with an osi_queue_t element.
 *
 * Utility functions are passed a pointer to a pointer to the first
 * element in the list; this word is NULL if the list is empty (the
 * pointer to it, of course, is not NULL).
 * The list is *not* circularly linked; rather prevp of the first
 * element and nextp of the last element are both NULL; this makes
 * checking for the end of the list easier, and still provides us a
 * quick deletion.
 *
 * Some of these things are macros for performance reasons.
 */

typedef struct osi_queue {
	struct osi_queue *nextp;
	struct osi_queue *prevp;
} osi_queue_t;

typedef struct osi_queueData {
	osi_queue_t q;
	void *datap;
} osi_queueData_t;

/* # of elements to allocate at once */
#define OSI_NQDALLOC		64

/* add an element to the head of a queue, first parm is
 * address of head pointer, and second parm is addr of
 * element to add.
 */
extern void osi_QAdd(osi_queue_t **headpp, osi_queue_t *eltp);

/* add an element to the tail of a queue, first parm is
 * address of head pointer, second is addr of ptr to tail elt,
 * and third parm is addr of element to add.
 */
extern void osi_QAddT(osi_queue_t **headpp, osi_queue_t **tailpp, osi_queue_t *eltp);

/* add to the head (like osi_QAdd) only be prepared to set tailpp if necessary */
extern void osi_QAddH(osi_queue_t **headpp, osi_queue_t **tailpp, osi_queue_t *eltp);

/* remove an element from a queue; takes address of head list, and
 * element to remove as parameters.
 */
extern void osi_QRemove(osi_queue_t **headpp, osi_queue_t *eltp);

/* remove an element from a queue with both head and tail pointers; 
 * takes address of head and tail lists, and element to remove as parameters.
 */
extern void osi_QRemoveHT(osi_queue_t **headpp, osi_queue_t **tailpp, osi_queue_t *eltp);

/* initialize the queue package */
extern void osi_InitQueue(void);

/* allocate a queue element with one data ptr */
extern osi_queueData_t *osi_QDAlloc(void);

/* free a single element queue pointer */
extern void osi_QDFree(osi_queueData_t *);

/* retrieve the queue data from a one-element block */
#define osi_GetQData(x)		((x)->datap)

/* set the queue data in a one-element block */
#define osi_SetQData(x,y)	((x)->datap = (y))

/* get the next ptr from a queue element */
#define osi_QNext(x)	((x)->nextp)

/* get the prev ptr from a queue element */
#define osi_QPrev(x)	((x)->prevp)

/* find out if a queue is empty */
#define osi_QIsEmpty(x)	((*x) == ((osi_queue_t *) 0))

#endif /* _OSI_QUEUE_H_ENV_ */
