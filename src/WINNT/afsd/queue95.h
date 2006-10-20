/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* queue.h
 *
 * Class declaration for generic queue for use with Windows 95/DJGPP client
 * disk cache
 *
 ***************************************************************************/

#ifndef _QUEUE_H
#define _QUEUE_H

/* offset of member m in struct T */
#define OFFSETOF(T, m) ((USHORT) &(((T *) NULL)->m))

/* get pointer to parent struct T containing member m at address p */
#define MEM_TO_OBJ(T, m, p) ((char *)(p) - OFFSETOF(T, m))

typedef struct _QLink {
  struct _QLink *next;
  struct _QLink *prev;
  int ord;
} QLink;

typedef struct _Queue {
  QLink *head;
  QLink *tail;
  int size;
  QLink *currpos;
} Queue;

/* add item to tail of queue */
void QAddT(Queue *queue, QLink* node, int ord);

/* add item to head of queue */
void QAddH(Queue *queue, QLink *node, int ord);

/* add item based on order value */
void QAddOrd(Queue *queue, QLink *node, int ord);

/* remove and return head of queue */
QLink* QServe(Queue *queue);

/* move item to tail of queue */
void QMoveToTail(Queue *queue, QLink *x, int ord);

/* remove item from queue */
void QRemove(Queue *queue, QLink* x);

/* return current position */
QLink *QCurrent(Queue *queue);

/* print out list of queued items */
void QIterate(Queue *queue);

#endif
