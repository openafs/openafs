/*
 * Copyright (c) 2011 Your File System Inc. All rights reserved.
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

/* A reimplementation of the rx_event handler using red/black trees
 *
 * The first rx_event implementation used a simple sorted queue of all
 * events, which lead to O(n^2) performance, where n is the number of
 * outstanding events. This was found to scale poorly, so was replaced.
 *
 * The second implementation used a set of per-second buckets to store
 * events. Each bucket (referred to as an epoch in the code) stored all
 * of the events which expired in that second. However, on modern networks
 * where RTT times are in the millisecond, most connections will have events
 * expiring within the next second, so the problem reoccurs.
 *
 * This new implementation uses Red-Black trees to store a sorted list of
 * events. Red Black trees are guaranteed to have no worse than O(log N)
 * insertion, and are commonly used in timer applications
 */

#include <afsconfig.h>
#include <afs/param.h>

#ifdef KERNEL
# include "afs/sysincludes.h"
# include "afsincludes.h"
#else
# include <roken.h>
#endif

#include <afs/opr.h>
#include <opr/queue.h>
#include <opr/rbtree.h>

#include "rx.h"
#include "rx_atomic.h"
#include "rx_globals.h"

struct rxevent {
    struct opr_queue q;
    struct opr_rbtree_node node;
    struct clock eventTime;
    struct rxevent *next;
    rx_atomic_t refcnt;
    int handled;
    void (*func)(struct rxevent *, void *, void *, int);
    void *arg;
    void *arg1;
    int arg2;
};

struct malloclist {
    void *mem;
    int size;
    struct malloclist *next;
};

static struct {
    afs_kmutex_t lock;
    struct opr_queue list;
    struct malloclist *mallocs;
} freeEvents;

static struct {
    afs_kmutex_t lock;
    struct opr_rbtree head;
    struct rxevent *first;
} eventTree;

static struct {
    afs_kmutex_t lock;
    struct clock last;
    struct clock next;
    void (*func)(void);
    int raised;
} eventSchedule;

static int allocUnit = 10;

static struct rxevent *
rxevent_alloc(void) {
     struct rxevent *evlist;
     struct rxevent *ev;
     struct malloclist *mrec;
     int i;

     MUTEX_ENTER(&freeEvents.lock);
     if (opr_queue_IsEmpty(&freeEvents.list)) {
	MUTEX_EXIT(&freeEvents.lock);

#if	defined(AFS_AIX32_ENV) && defined(KERNEL)
	ev = rxi_Alloc(sizeof(struct rxevent));
#else
	evlist = osi_Alloc(sizeof(struct rxevent) * allocUnit);
	mrec = osi_Alloc(sizeof(struct malloclist));

	mrec->mem = evlist;
	mrec->size = sizeof(struct rxevent) * allocUnit;

	MUTEX_ENTER(&freeEvents.lock);
	for (i = 1; i < allocUnit; i++) {
	    opr_queue_Append(&freeEvents.list, &evlist[i].q);
	}
	mrec->next = freeEvents.mallocs;
	freeEvents.mallocs = mrec;
	MUTEX_EXIT(&freeEvents.lock);
	ev = &evlist[0];
#endif
    } else {
	ev = opr_queue_First(&freeEvents.list, struct rxevent, q);
	opr_queue_Remove(&ev->q);
	MUTEX_EXIT(&freeEvents.lock);
    }

    memset(ev, 0, sizeof(struct rxevent));
    rx_atomic_set(&ev->refcnt, 1);

    return ev;
}

static void
rxevent_free(struct rxevent *ev) {
    MUTEX_ENTER(&freeEvents.lock);
    opr_queue_Prepend(&freeEvents.list, &ev->q);
    MUTEX_EXIT(&freeEvents.lock);
}

static_inline void
rxevent_put(struct rxevent *ev) {
    if (rx_atomic_dec_and_read(&ev->refcnt) == 0) {
        rxevent_free(ev);
    }
}

void
rxevent_Put(struct rxevent **ev)
{
    rxevent_put(*ev);
    *ev = NULL;
}

static_inline struct rxevent *
rxevent_get(struct rxevent *ev) {
    rx_atomic_inc(&ev->refcnt);
    return ev;
}

struct rxevent *
rxevent_Get(struct rxevent *ev) {
    return rxevent_get(ev);
}

/* Called if the time now is older than the last time we recorded running an
 * event. This test catches machines where the system time has been set
 * backwards, and avoids RX completely stalling when timers fail to fire.
 *
 * Take the different between now and the last event time, and subtract that
 * from the timing of every event on the system. This does a relatively slow
 * walk of the completely eventTree, but time-travel will hopefully be a pretty
 * rare occurrence.
 *
 * This can only safely be called from the event thread, as it plays with the
 * schedule directly.
 *
 */
static void
adjustTimes(void)
{
    struct opr_rbtree_node *node;
    struct clock adjTime, now;

    MUTEX_ENTER(&eventTree.lock);
    /* Time adjustment is expensive, make absolutely certain that we have
     * to do it, by getting an up to date time to base our decision on
     * once we've acquired the relevant locks.
     */
    clock_GetTime(&now);
    if (!clock_Lt(&now, &eventSchedule.last))
	goto out;

    adjTime = eventSchedule.last;
    clock_Zero(&eventSchedule.last);

    clock_Sub(&adjTime, &now);

    /* If there are no events in the tree, then there's nothing to adjust */
    if (eventTree.first == NULL)
	goto out;

    node = opr_rbtree_first(&eventTree.head);
    while(node) {
	struct rxevent *event = opr_containerof(node, struct rxevent, node);

	clock_Sub(&event->eventTime, &adjTime);
	node = opr_rbtree_next(node);
    }
    eventSchedule.next = eventTree.first->eventTime;

out:
    MUTEX_EXIT(&eventTree.lock);
}

static int initialised = 0;
void
rxevent_Init(int nEvents, void (*scheduler)(void))
{
    if (initialised)
	return;

    initialised = 1;

    clock_Init();
    MUTEX_INIT(&eventTree.lock, "event tree lock", MUTEX_DEFAULT, 0);
    opr_rbtree_init(&eventTree.head);

    MUTEX_INIT(&freeEvents.lock, "free events lock", MUTEX_DEFAULT, 0);
    opr_queue_Init(&freeEvents.list);
    freeEvents.mallocs = NULL;

    if (nEvents)
	allocUnit = nEvents;

    clock_Zero(&eventSchedule.next);
    clock_Zero(&eventSchedule.last);
    eventSchedule.raised = 0;
    eventSchedule.func = scheduler;
}

struct rxevent *
rxevent_Post(struct clock *when, struct clock *now,
	     void (*func) (struct rxevent *, void *, void *, int),
	     void *arg, void *arg1, int arg2)
{
    struct rxevent *ev, *event;
    struct opr_rbtree_node **childptr, *parent = NULL;

    ev = rxevent_alloc();
    ev->eventTime = *when;
    ev->func = func;
    ev->arg = arg;
    ev->arg1 = arg1;
    ev->arg2 = arg2;

    if (clock_Lt(now, &eventSchedule.last))
	adjustTimes();

    MUTEX_ENTER(&eventTree.lock);

    /* Work out where in the tree we'll be storing this */
    childptr = &eventTree.head.root;

    while(*childptr) {
	event = opr_containerof((*childptr), struct rxevent, node);

	parent = *childptr;
	if (clock_Lt(when, &event->eventTime))
	    childptr = &(*childptr)->left;
	else if (clock_Gt(when, &event->eventTime))
	    childptr = &(*childptr)->right;
	else {
	    opr_queue_Append(&event->q, &ev->q);
	    goto out;
	}
    }
    opr_queue_Init(&ev->q);
    opr_rbtree_insert(&eventTree.head, parent, childptr, &ev->node);

    if (eventTree.first == NULL ||
	clock_Lt(when, &(eventTree.first->eventTime))) {
	eventTree.first = ev;
	eventSchedule.raised = 1;
	clock_Zero(&eventSchedule.next);
	MUTEX_EXIT(&eventTree.lock);
	if (eventSchedule.func != NULL)
	    (*eventSchedule.func)();
	return rxevent_get(ev);
    }

out:
    MUTEX_EXIT(&eventTree.lock);
    return rxevent_get(ev);
}

/* We're going to remove ev from the tree, so set the first pointer to the
 * next event after it */
static_inline void
resetFirst(struct rxevent *ev)
{
    struct opr_rbtree_node *next = opr_rbtree_next(&ev->node);
    if (next)
	eventTree.first = opr_containerof(next, struct rxevent, node);
    else
	eventTree.first = NULL;
}

/*!
 * Cancel an event
 *
 * Cancels the event pointed to by evp. Returns true if the event has
 * been succesfully cancelled, or false if the event has already fired.
 */

int
rxevent_Cancel(struct rxevent **evp)
{
    struct rxevent *event;
    int cancelled = 0;

    if (!evp || !*evp)
	return 0;

    event = *evp;

    MUTEX_ENTER(&eventTree.lock);

    if (!event->handled) {
	/* We're a node on the red/black tree. If our list is non-empty,
	 * then swap the first element in the list in in our place,
	 * promoting it to the list head */
	if (event->node.parent == NULL
	    && eventTree.head.root != &event->node) {
	    /* Not in the rbtree, therefore must be a list element */
	    opr_queue_Remove(&event->q);
	} else {
	    if (!opr_queue_IsEmpty(&event->q)) {
	        struct rxevent *next;

		next = opr_queue_First(&event->q, struct rxevent, q);
		opr_queue_Remove(&next->q); /* Remove ourselves from list */
		if (event->q.prev == &event->q) {
		    next->q.prev = next->q.next = &next->q;
		} else {
		    next->q = event->q;
		    next->q.prev->next = &next->q;
		    next->q.next->prev = &next->q;
		}

		opr_rbtree_replace(&eventTree.head, &event->node,
				   &next->node);

		if (eventTree.first == event)
		    eventTree.first = next;

	    } else {
		if (eventTree.first == event)
		    resetFirst(event);

		opr_rbtree_remove(&eventTree.head, &event->node);
	    }
	}
	event->handled = 1;
	rxevent_put(event); /* Dispose of eventTree reference */
	cancelled = 1;
    }

    MUTEX_EXIT(&eventTree.lock);

    *evp = NULL;
    rxevent_put(event); /* Dispose of caller's reference */

    return cancelled;
}

/* Process all events which have expired. If events remain, then the relative
 * time until the next event is returned in the parameter 'wait', and the
 * function returns 1. If no events currently remain, the function returns 0
 *
 * If the current time is older than that of the last event processed, then we
 * assume that time has gone backwards (for example, due to a system time reset)
 * When this happens, all events in the current queue are rescheduled, using
 * the difference between the current time and the last event time as a delta
 */

int
rxevent_RaiseEvents(struct clock *wait)
{
    struct clock now;
    struct rxevent *event;
    int ret;

    clock_GetTime(&now);

    /* Check for time going backwards */
    if (clock_Lt(&now, &eventSchedule.last))
	  adjustTimes();
    eventSchedule.last = now;

    MUTEX_ENTER(&eventTree.lock);
    /* Lock our event tree */
    while (eventTree.first != NULL
	   && clock_Lt(&eventTree.first->eventTime, &now)) {

	/* Grab the next node, either in the event's list, or in the tree node
	 * itself, and remove it from the event tree */
	event = eventTree.first;
	if (!opr_queue_IsEmpty(&event->q)) {
	    event = opr_queue_Last(&event->q, struct rxevent, q);
	    opr_queue_Remove(&event->q);
	} else {
	    resetFirst(event);
	    opr_rbtree_remove(&eventTree.head, &event->node);
	}
	event->handled = 1;
        MUTEX_EXIT(&eventTree.lock);

        /* Fire the event, then free the structure */
	event->func(event, event->arg, event->arg1, event->arg2);
	rxevent_put(event);

	MUTEX_ENTER(&eventTree.lock);
    }

    /* Figure out when we next need to be scheduled */
    if (eventTree.first != NULL) {
	*wait = eventSchedule.next = eventTree.first->eventTime;
	ret = eventSchedule.raised = 1;
	clock_Sub(wait, &now);
    } else {
	ret = eventSchedule.raised = 0;
    }

    MUTEX_EXIT(&eventTree.lock);

    return ret;
}

void
shutdown_rxevent(void)
{
    struct malloclist *mrec, *nmrec;

    if (!initialised) {
	return;
    }
    MUTEX_DESTROY(&eventTree.lock);

#if !defined(AFS_AIX32_ENV) || !defined(KERNEL)
    MUTEX_DESTROY(&freeEvents.lock);
    mrec = freeEvents.mallocs;
    while (mrec) {
	nmrec = mrec->next;
	osi_Free(mrec->mem, mrec->size);
	osi_Free(mrec, sizeof(struct malloclist));
	mrec = nmrec;
    }
    mrec = NULL;
#endif
}
