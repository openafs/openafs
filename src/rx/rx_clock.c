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

#ifdef	KERNEL
#include "../afs/param.h"
#ifndef UKERNEL
#include "../rx/rx_clock.h"
#include "../h/types.h"
#include "../h/time.h"
#else /* !UKERNEL */
#include "../afs/sysincludes.h"
#include "../afs/afsincludes.h"
#include "../rx/rx_clock.h"
#endif /* !UKERNEL */
#else /* KERNEL */
#include <afs/param.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include "rx_clock.h"
#endif

#if	!defined(AFS_USE_GETTIMEOFDAY)
/*use this package only if gettimeofday is much much costlier than getitime */

#ifndef KERNEL

#if defined(AFS_GFS_ENV)
#define STARTVALUE 8000000	/* Ultrix bounds smaller, too small for general use */
#else
#define	STARTVALUE 100000000	/* Max number of seconds setitimer allows, for some reason */
#endif

struct clock clock_now;		/* The last elapsed time ready by clock_GetTimer */

/* This is set to 1 whenever the time is read, and reset to 0 whenever clock_NewTime is called.  This is to allow the caller to control the frequency with which the actual time is re-evaluated (an expensive operation) */
int clock_haveCurrentTime;

int clock_nUpdates;		/* The actual number of clock updates */
static int clockInitialized = 0;

/* Initialize the clock */
void clock_Init(void) {
    struct itimerval itimer, otimer;

    if (!clockInitialized) {
	itimer.it_value.tv_sec = STARTVALUE;
	itimer.it_value.tv_usec = 0;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &itimer, &otimer) != 0) {
	    fprintf(stderr, "clock:  could not set interval timer; \
				aborted(errno=%d)\n", errno);
	    fflush (stderr);
	    exit(1);
	}
	clockInitialized = 1;
    }

    clock_UpdateTime();
}

#ifndef KERNEL
/* Make clock uninitialized. */
int
clock_UnInit()
{
    clockInitialized = 0;
    return 0;
} 
#endif 

/* Compute the current time.  The timer gets the current total elapsed time since startup, expressed in seconds and microseconds.  This call is almost 200 usec on an APC RT */
void clock_UpdateTime()
{
    struct itimerval itimer;
    getitimer(ITIMER_REAL, &itimer);
    clock_now.sec = STARTVALUE - 1 - itimer.it_value.tv_sec; /* The "-1" makes up for adding 1000000 usec, on the next line */
    clock_now.usec = 1000000 - itimer.it_value.tv_usec;
    if (clock_now.usec == 1000000) clock_now.usec = 0, clock_now.sec++;
    clock_haveCurrentTime = 1;
    clock_nUpdates++;
}
#else /* KERNEL */
#endif /* KERNEL */

#endif /* AFS_USE_GETTIMEOFDAY */
