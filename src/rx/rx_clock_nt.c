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
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/rx/rx_clock_nt.c,v 1.9.2.2 2005/09/16 02:28:50 jaltman Exp $");

#ifdef AFS_NT40_ENV
#include <stdio.h>
#include <stdlib.h>
#include <windef.h>
#include <winbase.h>
#include "rx_clock.h"

void clock_UpdateTime(void);	/* forward reference */

struct clock clock_now;		/* The last elapsed time ready by clock_GetTimer */

/* This is set to 1 whenever the time is read, and reset to 0 whenever
 * clock_NewTime is called.  This is to allow the caller to control the
 * frequency with which the actual time is re-evaluated.
 */
int clock_haveCurrentTime;

int clock_nUpdates;		/* The actual number of clock updates */
static int clockInitialized = 0;

/* Timing tests show that we can compute times at about 4uS per call. */
LARGE_INTEGER rxi_clock0;
LARGE_INTEGER rxi_clockFreq;

#undef clock_UpdateTime
void clock_UpdateTime(void);

void
clock_Init(void)
{
    if (!QueryPerformanceFrequency(&rxi_clockFreq)) {
	OutputDebugString("No High Performance clock, exiting.\n");
	exit(1);
    }

    clockInitialized = 1;
    (void)QueryPerformanceCounter(&rxi_clock0);

    clock_UpdateTime();
}

#ifndef KERNEL
/* Make clock uninitialized. */
int
clock_UnInit(void)
{
    clockInitialized = 0;
    return 0;
}
#endif

void
clock_UpdateTime(void)
{
    LARGE_INTEGER now, delta;
    double seconds;

    (void)QueryPerformanceCounter(&now);

    delta.QuadPart = now.QuadPart - rxi_clock0.QuadPart;

    seconds = (double)delta.QuadPart / (double)rxi_clockFreq.QuadPart;

    clock_now.sec = (int)seconds;
    clock_now.usec = (int)((seconds - (double)clock_now.sec)
			   * (double)1000000);
    clock_haveCurrentTime = 1;
    clock_nUpdates++;

}
#endif /* AFS_NT40_ENV */
