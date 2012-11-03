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

    pthread_mutex_lock(&eventListMutex);
    rxevent_Put(evrecord->event);
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

    memset(events, sizeof(events), 0);
    pthread_mutex_init(&eventListMutex, NULL);

    /* Start up the event system */
    rxevent_Init(20, reschedule);
    ok(1, "Started event subsystem");

    clock_GetTime(&now);
    /* Test for a problem when there is only a single event in the tree */
    event = rxevent_Post(&now, &now, reportSub, NULL, NULL, 0);
    ok(event != NULL, "Created a single event");
    rxevent_Cancel(&event);
    ok(1, "Cancelled a single event");
    rxevent_RaiseEvents(&now);
    ok(1, "RaiseEvents happened without error");

    ok(pthread_create(&handler, NULL, eventHandler, NULL) == 0,
       "Created handler thread");

    /* Add 1000 random events to fire over the next 3 seconds */

    for (counter = 0; counter < NUMEVENTS; counter++) {
        when = random() % 3000;
	clock_GetTime(&now);
	eventTime = now;
	clock_Addmsec(&eventTime, when);
	pthread_mutex_lock(&eventListMutex);
	events[counter].event
	    = rxevent_Post(&eventTime, &now, eventSub, &events[counter], NULL, 0);

	/* A 10% chance that we will schedule another event at the same time */
	if (counter!=999 && random() % 10 == 0) {
	     counter++;
	     events[counter].event
		 = rxevent_Post(&eventTime, &now, eventSub, &events[counter],
				NULL, 0);
	}

	/* A 25% chance that we will cancel a random event */
	if (random() % 4 == 0) {
	    int victim = random() % counter;

	    if (events[victim].event != NULL) {
		rxevent_Cancel(&events[victim].event);
		events[victim].cancelled = 1;
	    }
	}
	pthread_mutex_unlock(&eventListMutex);
    }

    ok(1, "Added %d events", NUMEVENTS);

    sleep(3);

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
    ok(fired+cancelled == NUMEVENTS,
	"Number of fired and cancelled events sum to correct total");

    return 0;
}
