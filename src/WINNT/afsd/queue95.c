/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* queue.c
 *
 * Generic queue for use with Windows 95/DJGPP disk cache
 *
 ************************************************************************/

#ifdef DISKCACHE95

#define NULL 0
#include "queue95.h"
#include <stdio.h>

void QInit(Queue *queue)
{
  queue->head = NULL;
  queue->tail = NULL;
  queue->currpos = NULL;
}

void QAddT(Queue *queue, QLink* node, int ord)
{
  /*QLink* node = new QLink;*/

  /*node->item = x;*/
  node->ord = ord;
  node->next = NULL;
  node->prev = NULL;
  if (!queue->tail)
    queue->head = queue->tail = node;
  else {
    queue->tail->next = node;
    queue->tail = node;
    node->prev = queue->tail;
  }
  queue->size++;
}

void QAddH(Queue *queue, QLink *node, int ord)
{
  node->ord = ord;
  node->next = NULL;
  node->prev = NULL;
  if (!queue->head)
    queue->head = queue->tail = node;
  else {
    node->next = queue->head;
    queue->head->prev = node;
    queue->head = node;
  }
  queue->size++;
}
           
void QAddOrd(Queue *queue, QLink *node, int ord)
{
  /*QLink<T>* node = new QLink<T>;*/
  QLink* p, *prev;

  node->ord = ord;
  node->next = NULL;
  node->prev = NULL;
  if (!queue->tail)
    queue->head = queue->tail = node;
  else {
    p = queue->head;
    while (p && ord >= p->ord) {    /* add towards tail end if equals found */
      prev = p;
      p = p->next;
    }
    if (p == queue->head) {
      QAddH(queue, node, ord);
    }
    else if (p == NULL) {
      QAddT(queue, node, ord);
    }
    else {
      node->next = p;
      node->prev = prev;
      prev->next = node;
    }
  }
  queue->size++;
}

QLink* QServe(Queue *queue)
{
  QLink *n = queue->head;

  if (!queue->head) return NULL;
  if (queue->head == queue->tail)
    queue->head = queue->tail = NULL;
  else
    queue->head = n->next;
  queue->size--;
  return n;
}

void QMoveToTail(Queue *queue, QLink *n, int ord)
{
  QRemove(queue, n);
  QAddT(queue, n, ord);
}

void QRemove(Queue *queue, QLink *n)
{
  /*QLink* n2 = NULL;*/

  if (!queue->head) return;
  /*while(n && n != x) {
    n2 = n;
    n = n->next;
    }*/
  if (n == queue->currpos) {
    if (n == queue->head) queue->currpos = n->next;
    if (n == queue->tail) queue->currpos = n->prev;
    if (n->prev) queue->currpos = n->prev;
  }

  if (n->prev)
  {
    /*assert(n->prev->next == n);*/
    n->prev->next = n->next;
  }
  else
    queue->head = n->next;

  if (n->next)
  {
    /*assert(n->next->prev == n);*/
    n->next->prev = n->prev;
  }
  else
    queue->tail = n->prev;
  
  queue->size--;
}

QLink *QCurrent(Queue *queue)
{
  /*if (currpos) return currpos->item;
    else return NULL;*/
  return queue->currpos;
}

void QIterate(Queue *queue)
{
  QLink* node;

  node = queue->head;
  while (node) {
    printf("node=%x, ord=%f\n", node, node->ord);
    node = node->next;
  }
  fflush(stdout);
}

#endif /* DISKCACHE95 */
