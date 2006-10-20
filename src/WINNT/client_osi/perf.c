/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include "windows.h"
#include <string.h>
#include "main.h"
#include "perf.h"
#include "osi.h"
#include <assert.h>

static osi_mutex_t main_perfMutex;
static done;

#define STARTA	1
#define STARTB	2
static long flags;
static long count;

#define main_NITERS	20000	/* bops between the two */

long main_Perf1(long parm)
{
	while (1) {
		lock_ObtainMutex(&main_perfMutex);
		if (!(flags & STARTA)) {
			/* we're not supposed to be running */
			osi_SleepM((long) &flags, &main_perfMutex);
			continue;
		}
		
		/* hand off to the other guy */
		flags &= ~STARTA;
		flags |= STARTB;
		osi_Wakeup((long) &flags);

		/* we're running, bump the counter.
		 * do this after hand-off, so the other guy gets to run.
		 */
		count++;
		if (count > main_NITERS) {
			break;
		}
		
		osi_SleepM((long) &flags, &main_perfMutex);
	}
	done++;
	lock_ReleaseMutex(&main_perfMutex);
	osi_Wakeup((long) &done);	/* wakeup anyone waiting for completion */
	return 0;
}

long main_Perf2(long parm)
{
	while (1) {
		lock_ObtainMutex(&main_perfMutex);
		if (!(flags & STARTB)) {
			/* we're not supposed to be running */
			osi_SleepM((long) &flags, &main_perfMutex);
			continue;
		}
		
		/* hand off to the other guy */
		flags &= ~STARTB;
		flags |= STARTA;
		osi_Wakeup((long) &flags);

		/* we're running, bump the counter.  Do after hand-off so other
		 * guy also gets to notice that we're done.
		 */
		count++;
		if (count > main_NITERS) {
			break;
		}
		
		osi_SleepM((long)&flags, &main_perfMutex);
	}
	done++;
	lock_ReleaseMutex(&main_perfMutex);
	osi_Wakeup((long) &done);	/* wakeup anyone waiting for completion */
	return 0;
}

main_PerfTest(HANDLE hWnd)
{
	long mod1ID;
	long mod2ID;
	HANDLE mod1Handle;
	HANDLE mod2Handle;

	osi_Init();
	
	main_ForceDisplay(hWnd);

	/* create three processes, two modifiers and one scanner.  The scanner
	 * checks that the basic invariants are being maintained, while the
	 * modifiers modify the global variables, maintaining certain invariants
	 * by using locks.
	 *
	 * The invariant is that global variables a and b total 100.
	 */
	done = 0;
	count = 0;
	flags = STARTA;
	
	lock_InitializeMutex(&main_perfMutex, "perf test mutex");

	mod1Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Perf1, 0, 0, &mod1ID);
	if (mod1Handle == NULL) return -1;

	mod2Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Perf2, 0, 0, &mod2ID);
	if (mod2Handle == NULL) return -2;

	/* start running check daemon */
	while (1) {
		/* copy out count of # of dudes finished */
		lock_ObtainMutex(&main_perfMutex);
		if (done == 2) {
			lock_ReleaseMutex(&main_perfMutex);
			break;
		}
		osi_SleepM((long) &done, &main_perfMutex);
	}
	
	/* done, release and finalize all locks */
	lock_FinalizeMutex(&main_perfMutex);

	/* finally clean up thread handles */
	CloseHandle(mod1Handle);
	CloseHandle(mod2Handle);

	return 0;
}
