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
#include "trylock.h"
#include "osi.h"
#include <assert.h>

#define main_NITERS	10000	/* bops between the two */

osi_rwlock_t trylock_first;
osi_mutex_t trylock_second;
osi_rwlock_t trylock_third;

/* counters are not shared */
static long neonCount;
static long salmonCount;
static long interestingEvents;

/* set under trylock_first rwlock */
static int done;

/* neon tetras swim normally, i.e. downstream down the locking hierarchy */
long main_Neon(long parm)
{
	while (1) {
		Sleep(0);
		lock_ObtainRead(&trylock_first);
		Sleep(0);
		lock_ObtainMutex(&trylock_second);
		Sleep(0);
		lock_ObtainWrite(&trylock_third);
		Sleep(0);
		lock_ReleaseWrite(&trylock_third);
		lock_ReleaseMutex(&trylock_second);
		lock_ReleaseRead(&trylock_first);

		if (neonCount++ >= main_NITERS) break;
	}

	/* bump done counter under lock */
	lock_ObtainMutex(&trylock_second);
	done++;
	lock_ReleaseMutex(&trylock_second);

	return 0;
}

/* go upstream against the locking hierarchy */
long main_Salmon(long parm)
{
	long code;
	while (1) {
		code = lock_TryRead(&trylock_third);
		if (code == 0) {
			/* failed, release others, wait for this, and continue */
			lock_ObtainRead(&trylock_third);
			lock_ReleaseRead(&trylock_third);
			interestingEvents++;
			continue;
		}
		code = lock_TryMutex(&trylock_second);
		if (!code) {
			/* failed */
			lock_ReleaseRead(&trylock_third);
			lock_ObtainMutex(&trylock_second);
			lock_ReleaseMutex(&trylock_second);
			interestingEvents++;
			continue;
		}
		code = lock_TryWrite(&trylock_first);
		if (!code) {
			lock_ReleaseRead(&trylock_third);
			lock_ReleaseMutex(&trylock_second);
			lock_ObtainWrite(&trylock_first);
			lock_ReleaseWrite(&trylock_first);
			interestingEvents++;
			continue;
		}
		/* done */
		lock_ReleaseRead(&trylock_third);
		lock_ReleaseMutex(&trylock_second);
		lock_ReleaseWrite(&trylock_first);

		/* check for done */
		if (salmonCount++ >= main_NITERS) break;

		Sleep(0);
	}

	lock_ObtainMutex(&trylock_second);
	done++;
	lock_ReleaseMutex(&trylock_second);
	return 0;
}

main_TryLockTest(HANDLE hWnd)
{
	long mod1ID;
	long mod2ID;
	HANDLE mod1Handle;
	HANDLE mod2Handle;
	long localDone;

	osi_Init();

	salmonCount = 0;
	neonCount = 0;
	interestingEvents = 0;
	
	/* create three processes, two modifiers and one scanner.  The scanner
	 * checks that the basic invariants are being maintained, while the
	 * modifiers modify the global variables, maintaining certain invariants
	 * by using locks.
	 */
	done = 0;
	
	main_ForceDisplay(hWnd);

	lock_InitializeRWLock(&trylock_first, "first lock");
	lock_InitializeRWLock(&trylock_third, "third lock");
	lock_InitializeMutex(&trylock_second, "second mutex");

	mod1Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Neon, 0, 0, &mod1ID);
	if (mod1Handle == NULL) return -1;

	mod2Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Salmon, 0, 0, &mod2ID);
	if (mod2Handle == NULL) return -2;

	/* start running check daemon */
	while (1) {
		Sleep(1000);
		wsprintf(main_screenText[1], "Neon tetra iteration %d", neonCount);
		wsprintf(main_screenText[2], "Salmon iteration %d", salmonCount);
		wsprintf(main_screenText[3], "Interesting events: %d", interestingEvents);
		main_ForceDisplay(hWnd);

		/* copy out count of # of dudes finished */
		lock_ObtainMutex(&trylock_second);
		localDone = done;
		lock_ReleaseMutex(&trylock_second);

		/* right now, we're waiting for 2 threads */
		if (localDone == 2) break;
	}
	
	/* done, release and finalize all locks */
	lock_FinalizeRWLock(&trylock_first);
	lock_FinalizeRWLock(&trylock_third);
	lock_FinalizeMutex(&trylock_second);

	/* finally clean up thread handles */
	CloseHandle(mod1Handle);
	CloseHandle(mod2Handle);
	return 0;
}
