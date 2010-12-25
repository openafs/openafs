/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Elapsed time package */
/* See rx_clock.h for calling conventions */

#include <afsconfig.h>
#ifdef	KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

#ifdef AFS_SUN59_ENV
#include <sys/time_impl.h>
#endif


#ifdef KERNEL
#ifndef UKERNEL
#include "rx/rx_clock.h"
#include "h/types.h"
#include "h/time.h"
#else /* !UKERNEL */
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "rx/rx.h"
#include "rx/rx_clock.h"
#endif /* !UKERNEL */
#else /* KERNEL */
#include <sys/time.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "rx.h"
#include "rx_clock.h"
#endif

#if !defined(AFS_USE_GETTIMEOFDAY)
/*use this package only if gettimeofday is much much costlier than getitime */

#ifndef KERNEL

#define STARTVALUE 3600
static struct clock startvalue;
static struct clock relclock_epoch;   /* The elapsed time of the last itimer reset */

struct clock clock_now;		/* The last elapsed time ready by clock_GetTimer */

/* This is set to 1 whenever the time is read, and reset to 0 whenever clock_NewTime is called.  This is to allow the caller to control the frequency with which the actual time is re-evaluated (an expensive operation) */
int clock_haveCurrentTime;

int clock_nUpdates;		/* The actual number of clock updates */
static int clockInitialized = 0;

static void
clock_Sync(void)
{
    struct itimerval itimer, otimer;
    itimer.it_value.tv_sec = STARTVALUE;
    itimer.it_value.tv_usec = 0;
    itimer.it_interval.tv_sec = 0;
    itimer.it_interval.tv_usec = 0;

    signal(SIGALRM, SIG_IGN);
    if (setitimer(ITIMER_REAL, &itimer, &otimer) != 0) {
	osi_Panic("clock:  could not set interval timer; aborted(errno=%d)\n",
                  errno);
    }
    if (relclock_epoch.usec + startvalue.usec >= otimer.it_value.tv_usec) {
	relclock_epoch.sec = relclock_epoch.sec +
	    startvalue.sec - otimer.it_value.tv_sec;
	relclock_epoch.usec = relclock_epoch.usec +
	    startvalue.usec - otimer.it_value.tv_usec;
    } else {
	relclock_epoch.sec = relclock_epoch.sec +
	    startvalue.sec - 1 - otimer.it_value.tv_sec;
	relclock_epoch.usec = relclock_epoch.usec +
	    startvalue.usec + 1000000 - otimer.it_value.tv_usec;
    }
    if (relclock_epoch.usec >= 1000000)
	relclock_epoch.usec -= 1000000, relclock_epoch.sec++;
    /* the initial value of the interval timer may not be exactly the same
     * as the arg passed to setitimer. POSIX allows the implementation to
     * round it up slightly, and some nonconformant implementations truncate
     * it */
    getitimer(ITIMER_REAL, &itimer);
    startvalue.sec = itimer.it_value.tv_sec;
    startvalue.usec = itimer.it_value.tv_usec;
}

/* Initialize the clock */
void
clock_Init(void)
{
    if (!clockInitialized) {
	relclock_epoch.sec = relclock_epoch.usec = 0;
	startvalue.sec = startvalue.usec = 0;
	clock_Sync();
	clockInitialized = 1;
    }

    clock_UpdateTime();
}

/* Make clock uninitialized. */
int
clock_UnInit(void)
{
    clockInitialized = 0;
    return 0;
}

/* Compute the current time.  The timer gets the current total elapsed time since startup, expressed in seconds and microseconds.  This call is almost 200 usec on an APC RT */
void
clock_UpdateTime(void)
{
    struct itimerval itimer;
    struct clock offset;
    struct clock new;

    getitimer(ITIMER_REAL, &itimer);

    if (startvalue.usec >= itimer.it_value.tv_usec) {
	offset.sec = startvalue.sec - itimer.it_value.tv_sec;
	offset.usec = startvalue.usec - itimer.it_value.tv_usec;
    } else {
	/* The "-1" makes up for adding 1000000 usec, on the next line */
	offset.sec = startvalue.sec - 1 - itimer.it_value.tv_sec;
	offset.usec = startvalue.usec + 1000000 - itimer.it_value.tv_usec;
    }
    new.sec = relclock_epoch.sec + offset.sec;
    new.usec = relclock_epoch.usec + offset.usec;
    if (new.usec >= 1000000)
	new.usec -= 1000000, new.sec++;
    clock_now.sec = new.sec;
    clock_now.usec = new.usec;
    if (itimer.it_value.tv_sec < startvalue.sec / 2)
	clock_Sync();
    clock_haveCurrentTime = 1;
    clock_nUpdates++;
}
#else /* KERNEL */
#endif /* KERNEL */

#endif /* AFS_USE_GETTIMEOFDAY */
