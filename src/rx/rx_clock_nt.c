
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

/* Elapsed time package */
/* See rx_clock.h for calling conventions */

#include <afs/param.h>
#ifdef AFS_NT40_ENV
#include <stdio.h>
#include <stdlib.h>
#include <windef.h>
#include <winbase.h>
#include "rx_clock.h"

struct clock clock_now;	/* The last elapsed time ready by clock_GetTimer */

/* This is set to 1 whenever the time is read, and reset to 0 whenever
 * clock_NewTime is called.  This is to allow the caller to control the
 * frequency with which the actual time is re-evaluated.
 */
int clock_haveCurrentTime;

int clock_nUpdates;		/* The actual number of clock updates */

/* Timing tests show that we can compute times at about 4uS per call. */
LARGE_INTEGER rxi_clock0;
LARGE_INTEGER rxi_clockFreq;
void clock_Init()
{
    if (!QueryPerformanceFrequency(&rxi_clockFreq)) {
	printf("No High Performance clock, exiting.\n");
	exit(1);
    }

    (void) QueryPerformanceCounter(&rxi_clock0);

    clock_UpdateTime();
}

void clock_UpdateTime(void)
{
    LARGE_INTEGER now, delta;
    double seconds;

    (void) QueryPerformanceCounter(&now);

    delta.QuadPart = now.QuadPart - rxi_clock0.QuadPart;

    seconds = (double)delta.QuadPart / (double)rxi_clockFreq.QuadPart;

    clock_now.sec = (int) seconds;
    clock_now.usec = (int) ((seconds - (double)clock_now.sec)
			    * (double)1000000);
    clock_haveCurrentTime = 1;
    clock_nUpdates++;

}
#endif /* AFS_NT40_ENV */
