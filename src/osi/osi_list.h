/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/* It's simple, but, I think, it's pretty nice to use, and it's *very* efficient (especially so with a good optimizing compiler).   WARNING:  Since these functions are implemented as macros, it is best to use only *VERY* simple expressions for all parameters.  Double warning:  this uses a lot of type coercion, so you have to be *REAL* careful.  But C doesn't give me a reasonable alternative (i.e.. in-line expanded functions). */

#ifndef _OSI_OSI_LIST_H
#define _OSI_OSI_LIST_H 1


#if !defined(offsetof)
#include <stddef.h>
#endif /* !offsetof */

/*
 * osi data structures
 * doubly-linked list
 *
 * types:
 *
 *  osi_list_head
 *  osi_list_element
 *
 * example usage:
 *
 *  osi_list_head my_list;
 *
 *  struct mystruct {
 *    osi_list list1;
 *    osi_list list2;
 *    ...
 *  };
 *
 * void foo(void) {
 *     struct mystruct * foo;
 *     osi_list_Init_Head(&my_list);
 *
 *     foo = osi_mem_alloc(sizeof(struct mystruct));
 *     osi_list_Append(&my_list, foo, struct mystruct, list1);
 *     osi_list_Init_Element(foo, struct mystruct, list2);
 *
 * interfaces:
 *
 *  osi_list_Init(osi_list *);
 *
 *  osi_list_Prepend(osi_list * list, type * element, type, list_member);
 *    -- prepend element pointer $element$ of type $type$ to list $list$
 *       storing links in element structure member $list_member$
 *
 *  osi_list_Append(osi_list * list, type * element, type, list_member);
 *    -- append element pointer $element$ of type $type$ to list $list$
 *       storing links in element structure member $list_member$
 *
 *  osi_list_InsertBefore(type * e1, type * e2, type, list_member);
 *
 *  osi_list_InsertAfter(type * e1, type * e2, type, list_member);
 *
 */


/*
 * each link in the list points to the next queue element 
 */

typedef struct osi_list {
    struct osi_list *prev;
    struct osi_list *next;
} osi_list;

typedef struct osi_list_volatile {
    struct osi_list_volatile * osi_volatile_mt prev;
    struct osi_list_volatile * osi_volatile_mt next;
} osi_list_volatile;

typedef osi_list osi_list_head;
typedef osi_list osi_list_element;

typedef osi_list_volatile osi_list_head_volatile;
typedef osi_list_volatile osi_list_element_volatile;

#define osi_list_Head_Static_Initializer(x) { &x, &x }
#define osi_list_Head_Prototype(x) \
    osi_extern osi_list_head x
#define osi_list_Head_Decl(x) \
    osi_list_head x = osi_list_Head_Static_Initializer(x)
#define osi_list_Head_Volatile_Prototype(x) \
    osi_extern osi_list_head_volatile x
#define osi_list_Head_Volatile_Decl(x) \
    osi_list_head_volatile x = osi_list_Head_Static_Initializer(x)
#define osi_list_Element_Static_Initializer { osi_NULL, osi_NULL }


/* INTERNAL macros */

/* This one coerces the user's structure to a queue element (or queue head) */
#define	_osi_list(x) ((struct osi_list *)(x))

/* This one coerces a user structure pointer into a queue element pointer */
#define _osi_list_t2q(i, t, e) (_osi_list(&((i)->e)))

/* This one coerces a queue element to a user's structure */
#define _osi_list_q2t(x, t, e) ((t *)(((osi_uint8 *)x)-offsetof(t,e)))

/* This one casts back to the user structure type */
#define _osi_list_t(x, t) ((t *)x)

/* This one adds a queue element (i) before or after another queue element (or queue head) (q), doubly linking everything together.  It's called by the user usable macros, below.  If (a,b) is (next,prev) then the element i is linked after q; if it is (prev,next) then it is linked before q */
/* N.B.  I don't think it is possible to write this expression, correctly, with less than one comma (you can easily write an alternative expression with no commas that works with most or all compilers, but it's not clear that it really is un-ambiguous, legal C-code). */
#define _osi_listA(q,i,a,b) (((i->a=q->a)->b=i)->b=q, q->a=i)

/* move an element from q1 to and end of q2 */
#define _osi_listM(q1,q2,a,b,i,t,e) (_osi_listR(i,t,e), _osi_listA(q2,_osi_list_t2q(i,t,e),a,b))

/* These ones splice two queues together.  If (a,b) is (next,prev) then (*q2) is prepended to (*q1), otherwise (*q2) is appended to (*q1). */
#define _osi_listS(q1,q2,a,b) if (osi_list_IsEmpty(q2)); else \
    ((((q2->a->b=q1)->a->b=q2->b)->a=q1->a, q1->a=q2->a), osi_list_Init(q2))

/* This one removes part of queue (*q1) and attaches it to queue (*q2).
 * If (a,b) is (next,prev) then the subchain is prepended to (*q2), 
 * otherwise the subchain is appended to (*q2).
 * If (c,d) is (prev,next) then the subchain is the elements in (*q1) before (i),
 * otherwise the subchain is the elements in (*q1) after (i).
 * If (x,y) is (q1,i) then operation is either BeforePrepend of AfterAppend.
 * If (x,y) is (i,q1) then operation is either BeforeAppend or AfterPrepend. */
#define _osi_listSP(q1,q2,i,a,b,c,d,x,y) \
  ((q1 == i->c) ? 0 : \
   (((y->b->a=q2->a)->b=y->b), ((x->a->b=q2)->a=x->a), ((i->c=q1)->d=i)))

/* Basic remove operation.  Doesn't update the queue item to indicate it's been removed */
#define _osi_listR(i,t,e) ((_osi_list_t2q(i,t,e)->prev->next=_osi_list_t2q(i,t,e)->next)->prev=_osi_list_t2q(i,t,e)->prev)

/* EXPORTED macros */

/* Initialize a queue head (*q).  A queue head is just a queue element */
#define osi_list_Init(q) (_osi_list(q))->prev = (_osi_list(q))->next = (_osi_list(q))

/* Initialize a queue head (*q).  A queue head is just a queue element */
#define osi_list_Init_Head(q) (_osi_list(q))->prev = (_osi_list(q))->next = (_osi_list(q))

/* Initialize a queue element (i)->(e), where (i) is of type (t) */
#define osi_list_Init_Element(i,t,e) (_osi_list_t2q(i,t,e)->next = osi_NULL)

/* Prepend a queue element (*i) to the head of the queue, after the queue head (*q).  The new queue element should not currently be on any list. */
#define osi_list_Prepend(q,i,t,e) _osi_listA(_osi_list(q),_osi_list_t2q(i,t,e),next,prev)

/* Append a queue element (*i) to the end of the queue, before the queue head (*q).  The new queue element should not currently be on any list. */
#define osi_list_Append(q,i,t,e) _osi_listA(_osi_list(q),_osi_list_t2q(i,t,e),prev,next)

/* Insert a queue element (*i2) before another element (*i1) in the queue.  The new queue element should not currently be on any list. */
#define osi_list_InsertBefore(i1,i2,t,e) _osi_listA(_osi_list_t2q(i1,t,e),_osi_list_t2q(i2,t,e),prev,next)

/* Insert a queue element (*i2) after another element (*i1) in the queue.  The new queue element should not currently be on any list. */
#define osi_list_InsertAfter(i1,i2,t,e) _osi_listA(_osi_list_t2q(i1,t,e),_osi_list_t2q(i2,t,e),next,prev)

/* Spice the members of (*q2) to the beginning of (*q1), re-initialize (*q2) */
#define osi_list_SplicePrepend(q1,q2) _osi_listS(_osi_list(q1),_osi_list(q2),next,prev)

/* Splice the members of queue (*q2) to the end of (*q1), re-initialize (*q2) */
#define osi_list_SpliceAppend(q1,q2) _osi_listS(_osi_list(q1),_osi_list(q2),prev,next)

/* split the members after i off of queue (*q1), and append them onto queue (*q2) */
#define osi_list_SplitAfterAppend(q1,q2,i,t,e) _osi_listSP(_osi_list(q1),_osi_list(q2),_osi_list_t2q(i,t,e),prev,next,next,prev,_osi_list(q1),_osi_list_t2q(i,t,e))

/* split the members after i off of queue (*q1), and prepend them onto queue (*q2) */
#define osi_list_SplitAfterPrepend(q1,q2,i,t,e) _osi_listSP(_osi_list(q1),_osi_list(q2),_osi_list_t2q(i,t,e),next,prev,next,prev,_osi_list_t2q(i,t,e),_osi_list(q1))

/* split the members before i off of queue (*q1), and append them onto queue (*q2) */
#define osi_list_SplitBeforeAppend(q1,q2,i,t,e) _osi_listSP(_osi_list(q1),_osi_list(q2),_osi_list_t2q(i,t,e),prev,next,prev,next,_osi_list_t2q(i,t,e),_osi_list(q1))

/* split the members before i off of queue (*q1), and prepend them onto queue (*q2) */
#define osi_list_SplitBeforePrepend(q1,q2,i,t,e) _osi_listSP(_osi_list(q1),_osi_list(q2),_osi_list_t2q(i,t,e),next,prev,prev,next,_osi_list(q1),_osi_list_t2q(i,t,e))

/* Replace the queue (*q1) with the contents of the queue (*q2), re-initialize (*q2) */
#define osi_list_Replace(q1,q2) if (osi_list_IsEmpty(q2)) osi_list_Init(q1); else \
    (*_osi_list(q1) = *_osi_list(q2), _osi_list(q1)->next->prev = _osi_list(q1)->prev->next = _osi_list(q1), osi_list_Init(q2))

/* Remove a queue element (*i) from it's queue.  The next field is 0'd, so that any further use of this q entry will hopefully cause a core dump.  Multiple removes of the same queue item are not supported */
#define osi_list_Remove(i,t,e) (_osi_listR(i,t,e), _osi_list_t2q(i,t,e)->next = osi_NULL)

/* Move the queue element (*i) from it's queue to the end of the queue (*q) */
#define osi_list_MoveAppend(q,i,t,e) (_osi_listR(i,t,e), osi_list_Append(q,i,t,e))

/* Move the queue element (*i) from it's queue to the head of the queue (*q) */
#define osi_list_MovePrepend(q,i,t,e) (_osi_listR(i,t,e), osi_list_Prepend(q,i,t,e))

/* Return the first element of a queue, coerced too the specified structure s */
/* Warning:  this returns the queue head, if the queue is empty */
#define osi_list_First(q,t,e) (_osi_list_q2t((_osi_list(q)->next),t,e))

/* Return the last element of a queue, coerced to the specified structure s */
/* Warning:  this returns the queue head, if the queue is empty */
#define osi_list_Last(q,t,e) (_osi_list_q2t((_osi_list(q)->prev),t,e))

/* Return the next element in a queue, beyond the specified item, coerced to the specified structure s */
/* Warning:  this returns the queue head, if the item specified is the last in the queue */
#define osi_list_Next(i,t,e) (_osi_list_q2t((_osi_list_t2q(i,t,e)->next),t,e))

/* Return the previous element to a specified element of a queue, coerced to the specified structure s */
/* Warning:  this returns the queue head, if the item specified is the first in the queue */
#define osi_list_Prev(i,t,e) (_osi_list_q2t((_osi_list_t2q(i,t,e)->prev),t,e))

/* Return true if the queue is empty, i.e. just consists of a queue head.  The queue head must have been initialized some time prior to this call */
#define osi_list_IsEmpty(q) (_osi_list(q)->next == _osi_list(q))

/* Return true if the queue is non-empty, i.e. consists of a queue head plus at least one queue item */
#define osi_list_IsNotEmpty(q) (_osi_list(q)->next != _osi_list(q))

/* Return true if the queue item is currently in a queue */
/* Returns false if the item was removed from a queue OR is uninitialized (zero) */
#define osi_list_IsOnList(i,t,e) (_osi_list_t2q(i,t,e)->next != osi_NULL)

/* Returns true if the queue item (i) is the first element of the queue (q) */
#define osi_list_IsFirst(q,i,t,e) (_osi_list(q)->first == _osi_list_t2q(i,t,e))

/* Returns true if the queue item (i) is the last element of the queue (q) */
#define osi_list_IsLast(q,i,t,e) (_osi_list(q)->prev == _osi_list_t2q(i,t,e))

/* Returns true if the queue item (i) is the end of the queue (q), that is, i is the head of the queue */
#define osi_list_IsEnd(q,i,t,e) (_osi_list(q) == _osi_list_t2q(i,t,e))

/* Prototypical loop to scan an entire queue forwards.  q is the queue
 * head, qe is the loop variable, next is a variable used to store the
 * queue entry for the next iteration of the loop, s is the user's
 * queue structure name.  Called using "for (osi_list_Scan(...)) {...}".
 * Note that extra initializers can be added before the osi_list_Scan,
 * and additional expressions afterwards.  So "for (sum=0,
 * osi_list_Scan(...), sum += value) {value = qe->value}" is possible.
 * If the current queue entry is deleted, the loop will work
 * correctly.  Care must be taken if other elements are deleted or
 * inserted.  Next may be updated within the loop to alter the item
 * used in the next iteration. */
#define	osi_list_Scan(q, qe, next, t, e)			    \
    (qe) = osi_list_First(q,t,e), (next) = osi_list_Next(qe,t,e);   \
    !osi_list_IsEnd(q,qe,t,e);				            \
    (qe) = (next), (next) = osi_list_Next(qe,t,e)

#define	osi_list_Scan_Immutable(q, qe, t, e)		            \
    (qe) = osi_list_First(q,t,e);                                   \
    !osi_list_IsEnd(q,qe,t,e);				            \
    (qe) = osi_list_Next(qe,t,e)

/* similar to osi_list_Scan except start at element 'start' instead of the beginning */
#define osi_list_ScanFrom(q, start, qe, next, t, e)                 \
    (qe) = _osi_list_t(start,t), (next) = osi_list_Next(qe,t,e);    \
    !osi_list_IsEnd(q,qe,t,e);                                      \
    (qe) = (next), (next) = osi_list_Next(qe,t,e)

/* similar to osi_list_Scan except start at element 'start' instead of the beginning */
#define osi_list_ScanFrom_Immutable(q, start, qe, t, e)             \
    (qe) = _osi_list_t(start,t);                                    \
    !osi_list_IsEnd(q,qe,t,e);                                      \
    (qe) = osi_list_Next(qe,t,e)

/* This is similar to osi_list_Scan, but scans from the end of the queue to the beginning.  Next is the previous queue entry.  */
#define	osi_list_ScanBackwards(q, qe, prev, t, e)		    \
    (qe) = osi_list_Last(q,t,e), (prev) = osi_list_Prev(qe,t,e);    \
    !osi_list_IsEnd(q,qe,t,e);				            \
    (qe) = (prev), (prev) = osi_list_Prev(qe,t,e)

/* This is similar to osi_list_Scan, but scans from the end of the queue to the beginning.  Next is the previous queue entry.  */
#define	osi_list_ScanBackwards_Immutable(q, qe, t, e)		    \
    (qe) = osi_list_Last(q,t,e);                                    \
    !osi_list_IsEnd(q,qe,t,e);				            \
    (qe) = osi_list_Prev(qe,t,e)

/* This is similar to osi_list_ScanBackwards, but start at element 'start' instead of the end.  Next is the previous queue entry.  */
#define osi_list_ScanBackwardsFrom(q, start, qe, prev, t, e)        \
    (qe) = _osi_list_t(start,t), (prev) = osi_list_Prev(qe,t,e);    \
    !osi_list_IsEnd(q,qe,t,e);                                      \
    (qe) = (prev), (prev) = osi_list_Prev(qe,t,e)

/* This is similar to osi_list_ScanBackwards, but start at element 'start' instead of the end.  Next is the previous queue entry.  */
#define osi_list_ScanBackwardsFrom_Immutable(q, start, qe, t, e)    \
    (qe) = _osi_list_t(start,t);                                    \
    !osi_list_IsEnd(q,qe,t,e);                                      \
    (qe) = osi_list_Prev(qe,t,e)

#define osi_list_Count(q, qe, t, e, n) 			            \
    for (n=0, osi_list_Scan_Immutable(q,qe,t,e), n++) {}

#endif /* _OSI_OSI_LIST_H */
