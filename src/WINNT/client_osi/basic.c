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
#include "basic.h"
#include "osi.h"

#define OSI_MOD1LOOPS	15000
#define OSI_MOD2LOOPS	10000

/* global variables for the test */
osi_mutex_t main_aMutex;	/* mutex controlling access to a */
long a;				/* variable a */

osi_rwlock_t main_bRWLock;	/* rwlock controlling access to b */
long b;				/* variable b itself */

osi_rwlock_t main_doneRWLock;	/* lock for done */
int done;			/* count of done dudes */

osi_log_t *main_logp;		/* global log */

/* unlocked stat counters */
long m1Loops;
long m2Loops;
long s1Loops;
long s2Loops;
long s2Events;

unsigned long main_Mod1(void *parm)
{
	long i;
	for(i=0; i<OSI_MOD1LOOPS; i++) {
		lock_ObtainMutex(&main_aMutex);
		osi_Log0(main_logp, "mod1");
		lock_ObtainWrite(&main_bRWLock);
		a -= 52;
		Sleep(0);
		b += 52;
		osi_assert(a+b == 100);
		Sleep(0);
		lock_ReleaseWrite(&main_bRWLock);
		Sleep(0);
		lock_ReleaseMutex(&main_aMutex);
		Sleep(0);
		m1Loops = i;
                osi_Log1(main_logp, "mod1 done, %d", m1Loops);
	}
	lock_ObtainWrite(&main_doneRWLock);
	done++;
	Sleep(0);
	lock_ReleaseWrite(&main_doneRWLock);
	return 0;
}

unsigned long main_Mod2(void *parm)
{
	long i;
	for(i=0; i<OSI_MOD2LOOPS; i++) {
		osi_Log0(main_logp, "mod2");
		lock_ObtainMutex(&main_aMutex);
		lock_ObtainWrite(&main_bRWLock);
		a += 3;
		Sleep(0);
		b -= 3;
		osi_assert(a+b == 100);
		Sleep(0);
		lock_ReleaseWrite(&main_bRWLock);
		Sleep(0);
		lock_ReleaseMutex(&main_aMutex);
		Sleep(0);
		m2Loops = i;
                osi_Log4(main_logp, "mod2 done, %d %d %d %d", m2Loops, 2, 3, 4);
	}
	lock_ObtainWrite(&main_doneRWLock);
	done++;
	Sleep(0);
	lock_ReleaseWrite(&main_doneRWLock);
	return 0;
}

unsigned long main_Scan1(unsigned long parm)
{
	while (1) {
		osi_Log0(main_logp, "scan1");
		/* check to see if we're done */
		lock_ObtainRead(&main_doneRWLock);
		lock_AssertRead(&main_doneRWLock);
		if (done >= 2) break;
		lock_ReleaseRead(&main_doneRWLock);

		/* check state for consistency */
		lock_ObtainMutex(&main_aMutex);
                lock_AssertMutex(&main_aMutex);
		Sleep(0);
		lock_ObtainRead(&main_bRWLock);
		Sleep(0);
		osi_assert(a+b == 100);
		lock_ReleaseRead(&main_bRWLock);
		Sleep(0);
		lock_ReleaseMutex(&main_aMutex);

		/* get a read lock here to test people getting stuck on RW lock alone */
		lock_ObtainRead(&main_bRWLock);
		Sleep(0);
		lock_ReleaseRead(&main_bRWLock);
		
		s1Loops++;
                
                osi_Log2(main_logp, "scan1 done %d %d", s1Loops, 2);
	}
	lock_ReleaseRead(&main_doneRWLock);
	lock_ObtainWrite(&main_doneRWLock);
        lock_AssertWrite(&main_doneRWLock);
	done++;
	lock_ReleaseWrite(&main_doneRWLock);
	return 0;
}

unsigned long main_Scan2(unsigned long parm)
{
	while (1) {
		osi_Log0(main_logp, "scan2");
		/* check to see if we're done */
		lock_ObtainRead(&main_doneRWLock);
                lock_AssertAny(&main_doneRWLock);
		if (done >= 2) break;
		lock_ReleaseRead(&main_doneRWLock);

		/* check state for consistency without locks */
		if (a+b != 100) s2Events++;

		/* and record that we went around again */
		s2Loops++;

		/* give others a chance */
		Sleep(0);
                osi_Log3(main_logp, "scan2 done %d %d %d", s2Loops, 2, 3);
	}
	lock_ReleaseRead(&main_doneRWLock);
	lock_ObtainWrite(&main_doneRWLock);
        lock_AssertAny(&main_doneRWLock);
	done++;
	lock_ReleaseWrite(&main_doneRWLock);
	return 0;
}

main_BasicTest(HANDLE hWnd)
{
	long mod1ID;
	long mod2ID;
	long scan1ID;
	long scan2ID;
	HANDLE mod1Handle;
	HANDLE mod2Handle;
	HANDLE scan1Handle;
	HANDLE scan2Handle;
	long localDone;

	osi_Init();
        
        if (main_logp == NULL) {
        	main_logp = osi_LogCreate("basic", 0);
                osi_LogEnable(main_logp);
                osi_SetStatLog(main_logp);
        }
	
	/* create three processes, two modifiers and one scanner.  The scanner
	 * checks that the basic invariants are being maintained, while the
	 * modifiers modify the global variables, maintaining certain invariants
	 * by using locks.
	 *
	 * The invariant is that global variables a and b total 100.
	 */
	a = 100;
	b = 0;
	done = 0;
	
	lock_InitializeRWLock(&main_doneRWLock, "done lock");
	lock_InitializeRWLock(&main_bRWLock, "b lock");
	lock_InitializeMutex(&main_aMutex, "a mutex");

	mod1Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Mod1, 0, 0, &mod1ID);
	if (mod1Handle == NULL) return -1;

	mod2Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Mod2, 0, 0, &mod2ID);
	if (mod2Handle == NULL) return -2;

	scan1Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Scan1, 0, 0, &scan1ID);
	if (scan1Handle== NULL) return -2;

	scan2Handle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		(LPTHREAD_START_ROUTINE) main_Scan2, 0, 0, &scan2ID);
	if (scan2Handle== NULL) return -2;

	/* start running check daemon */
	while (1) {
		Sleep(1000);
		wsprintf(main_screenText[1], "Mod1 iteration %d", m1Loops);
		wsprintf(main_screenText[2], "Mod2 iteration %d", m2Loops);
		wsprintf(main_screenText[3], "Scan1 iteration %d", s1Loops);
		wsprintf(main_screenText[4], "Scan2 iteration %d, %d opportunites seen",
			s2Loops, s2Events);
		main_ForceDisplay(hWnd);

		/* copy out count of # of dudes finished */
		lock_ObtainRead(&main_doneRWLock);
		localDone = done;
		lock_ReleaseRead(&main_doneRWLock);

		/* right now, we're waiting for 4 threads */
		if (localDone == 4) break;
	}
	
	wsprintf(main_screenText[0], "Test done.");
	main_ForceDisplay(hWnd);

	/* done, release and finalize all locks */
	lock_FinalizeRWLock(&main_doneRWLock);
	lock_FinalizeRWLock(&main_bRWLock);
	lock_FinalizeMutex(&main_aMutex);

	/* finally clean up thread handles */
	CloseHandle(mod1Handle);
	CloseHandle(mod2Handle);
	CloseHandle(scan1Handle);
	CloseHandle(scan2Handle);

	return 0;
}
