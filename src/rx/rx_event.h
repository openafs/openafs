/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Event package */

#ifndef OPENAFS_RX_EVENT_H
#define OPENAFS_RX_EVENT_H

/* This routine must be called to initialize the event package.
 * nEvents is the number of events to allocate in a batch whenever
 * more are needed.  If this is 0, a default number (10) will be
 * allocated. */
extern void rxevent_Init( int nEvents, void (*scheduler)(void) );

/* Arrange for the indicated event at the appointed time.  when is a
 * "struct clock", in the clock.c time base */
struct clock;
struct rxevent;
extern struct rxevent *rxevent_Post(struct clock *when, struct clock *now,
				    void (*func) (struct rxevent *, void *,
						  void *, int),
				    void *arg, void *arg1, int arg2);

/* Remove the indicated event from the event queue.  The event must be
 * pending.  Note that a currently executing event may not cancel itself.
 */
extern int rxevent_Cancel(struct rxevent **);

/* The actions specified for each event that has reached the current clock
 * time will be taken.  The current time returned by GetTime is used
 * (warning:  this may be an old time if the user has not called
 * clock_NewTime)
 */
extern int rxevent_RaiseEvents(struct clock *wait);

/* Acquire a reference to an event */
extern struct rxevent *rxevent_Get(struct rxevent *event);

/* Release a reference to an event */
extern void rxevent_Put(struct rxevent *event);

/* Shutdown the event package */
extern void shutdown_rxevent(void);

#endif /* _EVENT_ */
