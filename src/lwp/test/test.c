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

#include <stdio.h>
#include <sys/time.h>
#include <potpourri.h>
#include "lwp.h"

char semaphore;

int OtherProcess()
    {
    for(;;)
	{
	LWP_SignalProcess(&semaphore);
	}
    }

main(argc, argv)
int argc; char *argv[];
    {
    struct timeval t1, t2;
    int pid, otherpid;
    register int i,  count, x;
    char *waitarray[2];
    static char c[] = "OtherProcess";
    
    count = atoi(argv[1]);

    assert(LWP_InitializeProcessSupport(0, &pid) == LWP_SUCCESS);
    assert(LWP_CreateProcess(OtherProcess,4096,0, 0, c, &otherpid) == LWP_SUCCESS);

    waitarray[0] = &semaphore;
    waitarray[1] = 0;
    gettimeofday(&t1, NULL);
    for (i = 0; i < count; i++)
	{
	LWP_MwaitProcess(1, waitarray, 1);
	}
    gettimeofday(&t2, NULL);

    x = (t2.tv_sec -t1.tv_sec)*1000000 + (t2.tv_usec - t1.tv_usec);
    printf("%d milliseconds for %d MWaits (%f usec per Mwait and Signal)\n", x/1000, count, (float)(x/count));
    }
