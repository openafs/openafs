/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "lwp.h"

static char semaphore;

static void *
OtherProcess(void *arg)
{
    for (;;) {
	LWP_SignalProcess(&semaphore);
    }
    return NULL;
}

int
main(int argc, char *argv[])
{
    struct timeval t1, t2;
    int pid, otherpid;
    int i, count, x;
    static char c[] = "OtherProcess";

    count = atoi(argv[1]);

    assert(LWP_InitializeProcessSupport(0, (PROCESS *) & pid) == LWP_SUCCESS);
    assert(LWP_CreateProcess
	   (OtherProcess, 4096, 0, 0, c,
	    (PROCESS *) & otherpid) == LWP_SUCCESS);

    gettimeofday(&t1, NULL);
    for (i = 0; i < count; i++) {
	LWP_WaitProcess(&semaphore);
    }
    gettimeofday(&t2, NULL);

    x = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);
    printf("%d milliseconds for %d MWaits (%f usec per Mwait and Signal)\n",
	   x / 1000, count, (float)(x / count));

    return 0;
}
