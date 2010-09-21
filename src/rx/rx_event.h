/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Event package */

#ifndef _EVENT_
#define _EVENT_

#ifdef	KERNEL
#include "rx/rx_queue.h"
#include "rx/rx_clock.h"
#else /* KERNEL */
#include "rx_queue.h"
#include "rx_clock.h"
#endif /* KERNEL */

/* An event is something that will happen at (or after) a specified clock
 * time, unless cancelled prematurely.  The user routine (*func)() is called
 * with arguments (event, arg, arg1) when the event occurs.
 * Warnings:
 *   (1) The user supplied routine should NOT cause process preemption.
 *   (2) The event passed to the user is still on the event queue at that
 *       time.  The user must not remove (event_Cancel) it explicitly, but
 *       the user may remove or schedule any OTHER event at this time.
 */

struct rxevent {
    struct rx_queue junk;	/* Events are queued */
    struct clock eventTime;	/* When this event times out (in clock.c units) */
    union {
	void (*oldfunc) (struct rxevent *, void *, void *);
	void (*newfunc) (struct rxevent *, void *, void *, int);
    } func; 			/* Function to call when this expires */
    void *arg;			/* Argument to the function */
    void *arg1;			/* Another argument */
    int arg2;			/* An integer argument */
    int newargs;		/* Nonzero if new-form arguments should be used */
};

/* We used to maintain a sorted list of events, but the amount of CPU
 * required to maintain the list grew with the square of the number of
 * connections. Now we keep a list of epochs, each epoch contains the
 * events scheduled for a particular second. Each epoch contains a sorted
 * list of the events scheduled for that epoch. */
struct rxepoch {
    struct rx_queue junk;	/* Epochs are queued */
    int epochSec;		/* each epoch spans one second */
    struct rx_queue events;	/* list of events for this epoch */
};

/* Some macros to make macros more reasonable (this allows a block to be
 * used within a macro which does not cause if statements to screw up).
 * That is, you can use "if (...) macro_name(); else ...;" without
 * having things blow up on the semi-colon. */

#ifndef BEGIN
#define BEGIN do {
#define END } while(0)
#endif

/* This routine must be called to initialize the event package.
 * nEvents is the number of events to allocate in a batch whenever
 * more are needed.  If this is 0, a default number (10) will be
 * allocated. */
#if 0
extern void rxevent_Init( /* nEvents, scheduler */ );
#endif

/* Get the expiration time for the next event */
#if 0
extern void exevent_NextEvent( /* when */ );
#endif

/* Arrange for the indicated event at the appointed time.  When is a
 * "struct clock", in the clock.c time base */
#if 0
extern struct rxevent *rxevent_Post( /* when, func, arg, arg1 */ );
#endif

/* Remove the indicated event from the event queue.  The event must be
 * pending.  Also see the warning, above.  The event pointer supplied
 * is zeroed.
 */
#ifdef RX_ENABLE_LOCKS
#ifdef RX_REFCOUNT_CHECK
#define	rxevent_Cancel(event_ptr, call, type)			    \
	BEGIN					    \
	    if (event_ptr) {			    \
		rxevent_Cancel_1(event_ptr, call, type);	    \
		event_ptr = NULL;	    \
	    }					    \
	END
#else /* RX_REFCOUNT_CHECK */
#define	rxevent_Cancel(event_ptr, call, type)			    \
	BEGIN					    \
	    if (event_ptr) {			    \
		rxevent_Cancel_1(event_ptr, call, 0);	    \
		event_ptr = NULL;	    \
	    }					    \
	END
#endif /* RX_REFCOUNT_CHECK */
#else /* RX_ENABLE_LOCKS */
#define	rxevent_Cancel(event_ptr, call, type)			    \
	BEGIN					    \
	    if (event_ptr) {			    \
		rxevent_Cancel_1(event_ptr, NULL, 0);	    \
		event_ptr = NULL;	    \
	    }					    \
	END
#endif /* RX_ENABLE_LOCKS */

/* The actions specified for each event that has reached the current clock
 * time will be taken.  The current time returned by GetTime is used
 * (warning:  this may be an old time if the user has not called
 * clock_NewTime)
 */
#if 0
extern int rxevent_RaiseEvents();
#endif

#endif /* _EVENT_ */
