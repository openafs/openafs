
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/* Event package */ 

#ifndef _EVENT_
#define _EVENT_

#ifdef	KERNEL
#include "../rx/rx_queue.h"
#include "../rx/rx_clock.h"
#else /* KERNEL */
#include "rx_queue.h"
#include "rx_clock.h"
#endif /* KERNEL */

/* An event is something that will happen at (or after) a specified clock time, unless cancelled prematurely.  The user routine (*func)() is called with arguments (event, arg, arg1) when the event occurs.  Warnings:  (1) The user supplied routine should NOT cause process preemption.   (2) The event passed to the user is still on the event queue at that time.  The user must not remove (event_Cancel) it explicitly, but the user may remove or schedule any OTHER event at this time. */

struct rxevent {
    struct rx_queue junk;    /* Events are queued */
    struct clock eventTime; /* When this event times out (in clock.c units) */
    void (*func)();	    /* Function to call when this expires */
    char *arg;		    /* Argument to the function */
    char *arg1;		    /* Another argument */
};

/* We used to maintain a sorted list of events, but the amount of CPU
 * required to maintain the list grew with the square of the number of
 * connections. Now we keep a list of epochs, each epoch contains the
 * events scheduled for a particular second. Each epoch contains a sorted
 * list of the events scheduled for that epoch. */
struct rxepoch {
    struct rx_queue junk;	/* Epochs are queued */
    int epochSec;		/* each epoch spans one second */
    struct rx_queue events;     /* list of events for this epoch */
};

/* Some macros to make macros more reasonable (this allows a block to be used within a macro which does not cause if statements to screw up).   That is, you can use "if (...) macro_name(); else ...;" without having things blow up on the semi-colon. */

#ifndef BEGIN
#define BEGIN do {
#define END } while(0)
#endif

/* This routine must be called to initialize the event package.  nEvents is the number of events to allocate in a batch whenever more are needed.  If this is 0, a default number (10) will be allocated. */
extern void rxevent_Init(/* nEvents, scheduler */);

/* Get the expiration time for the next event */
extern void exevent_NextEvent(/* when */);

/* Arrange for the indicated event at the appointed time.  When is a "struct clock", in the clock.c time base */
extern struct rxevent *rxevent_Post(/* when, func, arg, arg1 */);

/* Remove the indicated event from the event queue.  The event must be pending.  Also see the warning, above.  The event pointer supplied is zeroed. */
#ifdef RX_ENABLE_LOCKS
#ifdef RX_REFCOUNT_CHECK
extern void rxevent_Cancel_1(/* event_ptr, call , type*/);
#define	rxevent_Cancel(event_ptr, call, type)			    \
	BEGIN					    \
	    if (event_ptr) {			    \
		rxevent_Cancel_1(event_ptr, call, type);	    \
		event_ptr = (struct rxevent *) 0;	    \
	    }					    \
	END
#else /* RX_REFCOUNT_CHECK */
extern void rxevent_Cancel_1(/* event_ptr, call*/);
#define	rxevent_Cancel(event_ptr, call, type)			    \
	BEGIN					    \
	    if (event_ptr) {			    \
		rxevent_Cancel_1(event_ptr, call);	    \
		event_ptr = (struct rxevent *) 0;	    \
	    }					    \
	END
#endif /* RX_REFCOUNT_CHECK */
#else /* RX_ENABLE_LOCKS */
extern void rxevent_Cancel_1(/* event_ptr */);
#define	rxevent_Cancel(event_ptr, call, type)			    \
	BEGIN					    \
	    if (event_ptr) {			    \
		rxevent_Cancel_1(event_ptr);	    \
		event_ptr = (struct rxevent *) 0;	    \
	    }					    \
	END
#endif /* RX_ENABLE_LOCKS */

/* The actions specified for each event that has reached the current clock time will be taken.  The current time returned by GetTime is used (warning:  this may be an old time if the user has not called clock_NewTime) */
extern int rxevent_RaiseEvents();

#endif /* _EVENT_ */
