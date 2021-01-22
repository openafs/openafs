/* A simple test of the rx event layer */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <pthread.h>

#include <tests/tap/basic.h>

#include "rx/rx_event.h"
#include "rx/rx_clock.h"

#define NUMEVENTS 10000

/* Mutexes and condvars for the scheduler */
static int rescheduled = 0;
static pthread_mutex_t eventMutex;
static pthread_cond_t eventCond;

/* Mutexes and condvars for the event list */

static pthread_mutex_t eventListMutex;
struct testEvent {
    struct rxevent *event;
    int fired;
    int cancelled;
};

static struct testEvent events[NUMEVENTS];

static void
reschedule(void)
{
    pthread_mutex_lock(&eventMutex);
    pthread_cond_signal(&eventCond);
    rescheduled = 1;
    pthread_mutex_unlock(&eventMutex);
    return;
}

static void
eventSub(struct rxevent *event, void *arg, void *arg1, int arg2)
{
    struct testEvent *evrecord = arg;

    /*
     * The eventListMutex protects the contents of fields in the global
     * 'events' array, including reading/writing evrecord->event.
     * However, in this test code, we have an additional guarantee that
     * the events array will remain allocated for the duration of the test,
     * and as such that it is safe to dereference |evrecord| at all.  In real
     * application code where the passed args are pointers to allocated data
     * structures with finite lifetime, the programmer must ensure that the
     * firing event can safely access these fields (i.e., that the object
     * lifetime does not permit the object to be destroyed while an event
     * pointing to it is outstanding or in progress).  The simplest way to
     * do this (for reference counted objects) is to have the pending event
     * hold a reference on the pointed-to object. This reference should be
     * dropped at the end of the event handler or if the event is
     * (successfully!) cancelled before it fires.  Other strategies are also
     * possible, such as deferring object destruction until after all pending
     * events have run or gotten cancelled, noting that the calling code must
     * take care to allow the event handler to obtain any needed locks and
     * avoid deadlock.
     */
    pthread_mutex_lock(&eventListMutex);
    if (evrecord->event != NULL)
	rxevent_Put(&evrecord->event);
    evrecord->event = NULL;
    evrecord->fired = 1;
    pthread_mutex_unlock(&eventListMutex);
    return;
}

static void
reportSub(struct rxevent *event, void *arg, void *arg1, int arg2)
{
    printf("Event fired\n");
}

static void *
eventHandler(void *dummy) {
    struct timespec nextEvent;
    struct clock cv;
    struct clock next;

    pthread_mutex_lock(&eventMutex);
    while (1) {
	pthread_mutex_unlock(&eventMutex);

	next.sec = 30;
	next.usec = 0;
	clock_GetTime(&cv);
	rxevent_RaiseEvents(&next);

	pthread_mutex_lock(&eventMutex);

	/* If we were rescheduled whilst running the event queue,
	 * process the queue again */
	if (rescheduled) {
	    rescheduled = 0;
	    continue;
	}

	clock_Add(&cv, &next);
	nextEvent.tv_sec = cv.sec;
	nextEvent.tv_nsec = cv.usec * 1000;
	pthread_cond_timedwait(&eventCond, &eventMutex, &nextEvent);
    }
    pthread_mutex_unlock(&eventMutex);

    return NULL;
}

int
main(void)
{
    int when, counter, fail, fired, cancelled;
    struct clock now, eventTime;
    struct rxevent *event;
    pthread_t handler;

    plan(8);

    pthread_mutex_init(&eventMutex, NULL);
    pthread_cond_init(&eventCond, NULL);

    memset(events, 0, sizeof(events));
    pthread_mutex_init(&eventListMutex, NULL);

    /* Start up the event system */
    rxevent_Init(20, reschedule);
    ok(1, "Started event subsystem");

    clock_GetTime(&now);
    /* Test for a problem when there is only a single event in the tree */
    event = rxevent_Post(&now, &now, reportSub, NULL, NULL, 0);
    ok(event != NULL, "Created a single event");
    ok(rxevent_Cancel(&event), "Cancelled a single event");
    rxevent_RaiseEvents(&now);
    ok(1, "RaiseEvents happened without error");

    ok(pthread_create(&handler, NULL, eventHandler, NULL) == 0,
       "Created handler thread");

    /* Add a number of random events to fire over the next 3 seconds, but front-loaded
     * a bit so that we can exercise the cancel/fire race path. */

    for (counter = 0; counter < NUMEVENTS; counter++) {
        when = random() % 4000;
	/* Put 1/4 of events "right away" so we cancel them as they fire */
	if (when >= 3000)
	    when = random() % 5;
	clock_GetTime(&now);
	eventTime = now;
	clock_Addmsec(&eventTime, when);
	pthread_mutex_lock(&eventListMutex);
	events[counter].event
	    = rxevent_Post(&eventTime, &now, eventSub, &events[counter], NULL, 0);

	/* A 10% chance that we will schedule another event at the same time */
	if (counter < (NUMEVENTS - 1) && random() % 10 == 0) {
	     counter++;
	     events[counter].event
		 = rxevent_Post(&eventTime, &now, eventSub, &events[counter],
				NULL, 0);
	}
	/*
	 * A 25% chance that we will cancel some event.
	 * Randomly pick any event that was scheduled before the current event.
	 */
	if (counter > 0 && (random() % 4 == 0)) {
	    int victim = random() % counter;

	    if (rxevent_Cancel(&events[victim].event))
		events[victim].cancelled = 1;
	}
	pthread_mutex_unlock(&eventListMutex);
    }

    ok(1, "Added %d events", NUMEVENTS);

    sleep(4);

    fired = 0;
    cancelled = 0;
    fail = 0;
    for (counter = 0; counter < NUMEVENTS; counter++) {
	if (events[counter].fired)
	    fired++;
	if (events[counter].cancelled)
	    cancelled++;
	if (events[counter].cancelled && events[counter].fired)
	    fail = 1;
    }
    ok(!fail, "Didn't fire any cancelled events");
    diag("fired %d/%d events", fired, NUMEVENTS);
    diag("cancelled %d/%d events", cancelled, NUMEVENTS);
    is_int(NUMEVENTS, fired+cancelled,
	"Number of fired and cancelled events sum to correct total");

    return 0;
}
