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

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "osi.h"

/* Locking hierarchy for these critical sections:
 *
 * 1. lock osi_sleepFDCS
 * 2. lock osi_critSec[i]
 * 3. lock osi_sleepInfoAllocCS
 */

/* file descriptor for iterating over sleeping threads */
osi_fdOps_t osi_sleepFDOps = {
	osi_SleepFDCreate,
	osi_SleepFDGetInfo,
	osi_SleepFDClose
};

/*
 * Thread-local storage for sleep Info structures
 */
DWORD osi_SleepSlot;

/* critical section serializing contents of all sleep FDs, so that
 * concurrent GetInfo calls don't damage each other if applied
 * to the same FD.
 */
CRITICAL_SECTION osi_sleepFDCS;

/* critical regions used for SleepSched to guarantee atomicity.
 * protects all sleep info structures while they're in the
 * sleep hash table.
 */
static CRITICAL_SECTION osi_critSec[OSI_SLEEPHASHSIZE];

/* the sleep info structure hash table.
 * all active entries are in here.  In addition, deleted entries
 * may be present, referenced by file descriptors from remote
 * debuggers; these will have OSI_SLEEPINFO_DELETED set and
 * should be ignored.
 */
static osi_sleepInfo_t *osi_sleepers[OSI_SLEEPHASHSIZE];
static osi_sleepInfo_t *osi_sleepersEnd[OSI_SLEEPHASHSIZE];

/* allocate space for lock operations */
osi_lockOps_t *osi_lockOps[OSI_NLOCKTYPES];

/* some global statistics */
long osi_totalSleeps = 0;

/* critical section protecting sleepInfoFreeListp and all sleep entries in
 * the free list.
 */
CRITICAL_SECTION osi_sleepInfoAllocCS;

/* sleep entry free list */
osi_sleepInfo_t *osi_sleepInfoFreeListp;

/* boot time */
unsigned long osi_bootTime;

/* count of free entries in free list, protected by osi_sleepInfoAllocCS */
long osi_sleepInfoCount=0;

/* count of # of allocates of sleep info structures */
long osi_sleepInfoAllocs = 0;

/* the sleep bucket lock must be held.
 * Releases the reference count and frees the structure if the item has
 * been deleted.
 */
void osi_ReleaseSleepInfo(osi_sleepInfo_t *ap)
{
	if (--ap->refCount == 0 && (ap->states & OSI_SLEEPINFO_DELETED))
		osi_FreeSleepInfo(ap);
}

/* must be called with sleep bucket locked.
 * Frees the structure if it has a 0 reference count (and removes it
 * from the hash bucket).  Otherwise, we simply mark the item
 * for deleting when the ref count hits zero.
 */
void osi_FreeSleepInfo(osi_sleepInfo_t *ap)
{
    LONG_PTR idx;

    if (ap->refCount > 0) {
	TlsSetValue(osi_SleepSlot, NULL);	/* don't reuse me */
	ap->states |= OSI_SLEEPINFO_DELETED;
	return;
    }

    /* remove from hash if still there */
    if (ap->states & OSI_SLEEPINFO_INHASH) {
	ap->states &= ~OSI_SLEEPINFO_INHASH;
	idx = osi_SLEEPHASH(ap->value);
	osi_QRemoveHT((osi_queue_t **) &osi_sleepers[idx], (osi_queue_t **) &osi_sleepersEnd[idx], &ap->q);
    }

    if (ap->states & OSI_SLEEPINFO_DELETED) {
	EnterCriticalSection(&osi_sleepInfoAllocCS);
	ap->q.nextp = (osi_queue_t *) osi_sleepInfoFreeListp;
	osi_sleepInfoFreeListp = ap;
	osi_sleepInfoCount++;
	LeaveCriticalSection(&osi_sleepInfoAllocCS);
    }
}

/* allocate a new sleep structure from the free list */
osi_sleepInfo_t *osi_AllocSleepInfo()
{
	osi_sleepInfo_t *ap;

	EnterCriticalSection(&osi_sleepInfoAllocCS);
	if (!(ap = osi_sleepInfoFreeListp)) {
		ap = (osi_sleepInfo_t *) malloc(sizeof(osi_sleepInfo_t));
		ap->sema = CreateSemaphore(NULL, 0, 65536, (char *) 0);
                osi_sleepInfoAllocs++;
	}
	else {
		osi_sleepInfoFreeListp = (osi_sleepInfo_t *) ap->q.nextp;
		osi_sleepInfoCount--;
	}
	ap->tid = GetCurrentThreadId();
	ap->states = 0;	/* not signalled yet */
	LeaveCriticalSection(&osi_sleepInfoAllocCS);

	return ap;
}

int osi_Once(osi_once_t *argp)
{
	long i;

	while ((i=InterlockedExchange(&argp->atomic, 1)) != 0) {
		Sleep(0);
	}
	
	if (argp->done == 0) {
		argp->done = 1;
		return 1;
	}

	/* otherwise we've already been initialized, so clear lock and return */
	InterlockedExchange(&argp->atomic, 0);
	return 0;
}

void osi_EndOnce(osi_once_t *argp)
{
	InterlockedExchange(&argp->atomic, 0);
}

int osi_TestOnce(osi_once_t *argp)
{
	long localDone;
	long i;

	while ((i=InterlockedExchange(&argp->atomic, 1)) != 0) {
		Sleep(0);
	}
	
	localDone = argp->done;

	/* drop interlock */
	InterlockedExchange(&argp->atomic, 0);

	return (localDone? 0 : 1);
}

/* Initialize the package, should be called while single-threaded.
 * Can be safely called multiple times.
 * Must be called before any osi package calls.
 */
void osi_Init(void)
{
	int i;
	static osi_once_t once;
        unsigned long remainder;		/* for division output */
	osi_fdType_t *typep;
        SYSTEMTIME sysTime;
        FILETIME fileTime;
        osi_hyper_t bootTime;

	/* check to see if already initialized; if so, claim success */
	if (!osi_Once(&once)) return;

	/* setup boot time values */
        GetSystemTime(&sysTime);
        SystemTimeToFileTime(&sysTime, &fileTime);

	/* change the base of the time so it won't be negative for a long time */
	fileTime.dwHighDateTime -= 28000000;

        bootTime.HighPart = fileTime.dwHighDateTime;
        bootTime.LowPart = fileTime.dwLowDateTime;
        /* now, bootTime is in 100 nanosecond units, and we'd really rather
         * have it in 1 second units, units 10,000,000 times bigger.
         * So, we divide.
         */
        bootTime = ExtendedLargeIntegerDivide(bootTime, 10000000, &remainder);
        osi_bootTime = bootTime.LowPart;

	/* initialize thread-local storage for sleep Info structures */
	osi_SleepSlot = TlsAlloc();

	/* init FD system */
	osi_InitFD();

	/* initialize critical regions and semaphores */
	for(i=0;i<OSI_SLEEPHASHSIZE; i++) {
		InitializeCriticalSection(&osi_critSec[i]);
		osi_sleepers[i] = (osi_sleepInfo_t *) NULL;
	        osi_sleepersEnd[i] = (osi_sleepInfo_t *) NULL;
	}

	/* free list CS */
	InitializeCriticalSection(&osi_sleepInfoAllocCS);

	/* initialize cookie system */
	InitializeCriticalSection(&osi_sleepFDCS);

	/* register the FD type */
	typep = osi_RegisterFDType("sleep", &osi_sleepFDOps, NULL);
	if (typep) {
		/* add formatting info */
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 0,
			"Sleep address", OSI_DBRPC_HEX);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 1,
			"Thread ID", 0);
		osi_AddFDFormatInfo(typep, OSI_DBRPC_REGIONINT, 2,
			"States", OSI_DBRPC_HEX);
	}
	
	osi_BaseInit();

	osi_StatInit();
        
        osi_InitQueue();

	osi_EndOnce(&once);
}

void osi_TWait(osi_turnstile_t *turnp, int waitFor, void *patchp, CRITICAL_SECTION *releasep)
{
    osi_sleepInfo_t *sp;
    unsigned int code;

    sp = TlsGetValue(osi_SleepSlot);
    if (sp == NULL) {
	sp = osi_AllocSleepInfo();
	TlsSetValue(osi_SleepSlot, sp);
    }
    else {
	sp->states = 0;
    }
    sp->refCount = 0;
    sp->waitFor = waitFor;
    sp->value = (LONG_PTR) patchp;
    osi_QAddT((osi_queue_t **) &turnp->firstp, (osi_queue_t **) &turnp->lastp, &sp->q);
    if (!turnp->lastp) 
	turnp->lastp = sp;
    LeaveCriticalSection(releasep);

	/* now wait for the signal */
	while(1) {
		/* wait */
		code = WaitForSingleObject(sp->sema,
                	/* timeout */ INFINITE);

		/* if the reason for the wakeup was that we were signalled,
		 * break out, otherwise try again, since the semaphore count is
		 * decreased only when we get WAIT_OBJECT_0 back.
		 */
		if (code == WAIT_OBJECT_0) break;
	}	/* while we're waiting */
        
	/* we're the only one who should be looking at or changing this
	 * structure after it gets signalled.  Sema sp->sema isn't signalled
	 * any longer after we're back from WaitForSingleObject, so we can
	 * free this element directly.
         */
        osi_assert(sp->states & OSI_SLEEPINFO_SIGNALLED);
        
        osi_FreeSleepInfo(sp);
        
	/* reobtain, since caller commonly needs it */
        EnterCriticalSection(releasep);
}

/* must be called with a critical section held that guards the turnstile
 * structure.  We remove the sleepInfo structure from the queue so we don't
 * wake the guy again, but we don't free it because we're still using the
 * semaphore until the guy waiting wakes up.
 */
void osi_TSignal(osi_turnstile_t *turnp)
{
	osi_sleepInfo_t *sp;
        
	if (!turnp->lastp) 
	    return;
        
        sp = turnp->lastp;
	turnp->lastp = (osi_sleepInfo_t *) osi_QPrev(&sp->q);
        osi_QRemoveHT((osi_queue_t **) &turnp->firstp, (osi_queue_t **) &turnp->lastp, &sp->q);
        sp->states |= OSI_SLEEPINFO_SIGNALLED;
        ReleaseSemaphore(sp->sema, 1, (long *) 0);
}

/* like TSignal, only wake *everyone* */
void osi_TBroadcast(osi_turnstile_t *turnp)
{
	osi_sleepInfo_t *sp;
        
        while(sp = turnp->lastp) {
		turnp->lastp = (osi_sleepInfo_t *) osi_QPrev(&sp->q);
	        osi_QRemoveHT((osi_queue_t **) &turnp->firstp, (osi_queue_t **) &turnp->lastp, &sp->q);
	        sp->states |= OSI_SLEEPINFO_SIGNALLED;
	        ReleaseSemaphore(sp->sema, 1, (long *) 0);
	}	/* while someone's still asleep */
}

/* special turnstile signal for mutexes and locks.  Wakes up only those who
 * will really be able to lock the lock.  The assumption is that everyone who
 * already can use the lock has already been woken (and is thus not in the
 * turnstile any longer).
 *
 * The stillHaveReaders parm is set to 1 if this is a convert from write to read,
 * indicating that there is still at least one reader, and we should only wake
 * up other readers.  We use it in a tricky manner: we just pretent we already woke
 * a reader, and that is sufficient to prevent us from waking a writer.
 *
 * The crit sec. csp is released before the threads are woken, but after they
 * are removed from the turnstile.  It helps ensure that we won't have a spurious
 * context swap back to us if the release performs a context swap for some reason.
 */
void osi_TSignalForMLs(osi_turnstile_t *turnp, int stillHaveReaders, CRITICAL_SECTION *csp)
{
	osi_sleepInfo_t *tsp;		/* a temp */
	osi_sleepInfo_t *nsp;		/* a temp */
        osi_queue_t *wakeupListp;	/* list of dudes to wakeup after dropping lock */
        int wokeReader;
        unsigned short *sp;
        unsigned char *cp;
        
	wokeReader = stillHaveReaders;
	wakeupListp = NULL;
        while(tsp = turnp->lastp) {
		/* look at each sleepInfo until we find someone we're not supposed to
                 * wakeup.
                 */
		if (tsp->waitFor & OSI_SLEEPINFO_W4WRITE) {
			if (wokeReader) break;
                }
                else wokeReader = 1;
                
                /* otherwise, we will wake this guy.  For now, remove from this list
                 * and move to private one, so we can do the wakeup after releasing
                 * the crit sec.
                 */
		turnp->lastp = (osi_sleepInfo_t *) osi_QPrev(&tsp->q);
	        osi_QRemoveHT((osi_queue_t **) &turnp->firstp, (osi_queue_t **) &turnp->lastp, &tsp->q);
                
		/* do the patching required for lock obtaining */
                if (tsp->waitFor & OSI_SLEEPINFO_W4WRITE) {
			cp = (void *) tsp->value;
                        (*cp) |= OSI_LOCKFLAG_EXCL;
                }
                else if (tsp->waitFor & OSI_SLEEPINFO_W4READ) {
                	sp = (void *) tsp->value;
                        (*sp)++;
                }

                /* and add to our own list */
                tsp->q.nextp = wakeupListp;
                wakeupListp = &tsp->q;
                
                /* now if we woke a writer, we're done, since it is pointless
                 * to wake more than one writer.
                 */
                if (!wokeReader) break;
        }
        
        /* hit end, or found someone we're not supposed to wakeup */
	if (csp) LeaveCriticalSection(csp);
        
        /* finally, wakeup everyone we found.  Don't free things since the sleeper
         * will free the sleepInfo structure.
         */
	for(tsp = (osi_sleepInfo_t *) wakeupListp; tsp; tsp = nsp) {
		/* pull this out first, since *tsp *could* get freed immediately
                 * after the ReleaseSemaphore, if a context swap occurs.
                 */
		nsp = (osi_sleepInfo_t *) tsp->q.nextp;
	        tsp->states |= OSI_SLEEPINFO_SIGNALLED;
	        ReleaseSemaphore(tsp->sema, 1, (long *) 0);
        }
}

/* utility function to atomically (with respect to WakeSched)
 * release an atomic counter spin lock and sleep on an
 * address (value).
 * Called with no locks held.
 */
void osi_SleepSpin(LONG_PTR sleepValue, CRITICAL_SECTION *releasep)
{
    register LONG_PTR idx;
    int code;
    osi_sleepInfo_t *sp;
    CRITICAL_SECTION *csp;

    sp = TlsGetValue(osi_SleepSlot);
    if (sp == NULL) {
	sp = osi_AllocSleepInfo();
	TlsSetValue(osi_SleepSlot, sp);
    }
    else {
	sp->states = 0;
    }
    sp->refCount = 0;
    sp->value = sleepValue;
    idx = osi_SLEEPHASH(sleepValue);
    csp = &osi_critSec[idx];
    EnterCriticalSection(csp);
    osi_QAddT((osi_queue_t **) &osi_sleepers[idx], (osi_queue_t **) &osi_sleepersEnd[idx], &sp->q);
    sp->states |= OSI_SLEEPINFO_INHASH;
    LeaveCriticalSection(releasep);
    LeaveCriticalSection(csp);
    osi_totalSleeps++;	/* stats */
    while(1) {
	/* wait */
	code = WaitForSingleObject(sp->sema,
				    /* timeout */ INFINITE);

	/* if the reason for the wakeup was that we were signalled,
	* break out, otherwise try again, since the semaphore count is
	* decreased only when we get WAIT_OBJECT_0 back.
	*/
	if (code == WAIT_OBJECT_0) break;
    }

    /* now clean up */
    EnterCriticalSection(csp);

    /* must be signalled */
    osi_assert(sp->states & OSI_SLEEPINFO_SIGNALLED);

    /* free the sleep structure, must be done under bucket lock
     * so that we can check reference count and serialize with
     * those who change it.
     */
    osi_FreeSleepInfo(sp);

    LeaveCriticalSection(csp);
}

/* utility function to wakeup someone sleeping in SleepSched */
void osi_WakeupSpin(LONG_PTR sleepValue)
{
    register LONG_PTR idx;
    register CRITICAL_SECTION *csp;
    register osi_sleepInfo_t *tsp;

    idx = osi_SLEEPHASH(sleepValue);
    csp = &osi_critSec[idx];
    EnterCriticalSection(csp);
	for(tsp=osi_sleepers[idx]; tsp; tsp=(osi_sleepInfo_t *) osi_QNext(&tsp->q)) {
	    if ((!(tsp->states & (OSI_SLEEPINFO_DELETED|OSI_SLEEPINFO_SIGNALLED)))
		 && tsp->value == sleepValue) {
		ReleaseSemaphore(tsp->sema, 1, (long *) 0);
		tsp->states |= OSI_SLEEPINFO_SIGNALLED;
	    }
	}	
    LeaveCriticalSection(csp);
}	

void osi_Sleep(LONG_PTR sleepVal)
{
	CRITICAL_SECTION *csp;
        
	/* may as well save some code by using SleepSched again */
        csp = &osi_baseAtomicCS[0];
        EnterCriticalSection(csp);
	osi_SleepSpin(sleepVal, csp);
}

void osi_Wakeup(LONG_PTR sleepVal)
{
	/* how do we do osi_Wakeup on a per-lock package type? */

	osi_WakeupSpin(sleepVal);
}

long osi_SleepFDCreate(osi_fdType_t *fdTypep, osi_fd_t **outpp)
{
	osi_sleepFD_t *cp;

	cp = (osi_sleepFD_t *)malloc(sizeof(*cp));
	memset((void *) cp, 0, sizeof(*cp));
	cp->idx = 0;
	cp->sip = NULL;

	/* done */
	*outpp = &cp->fd;
	return 0;
}

long osi_SleepFDClose(osi_fd_t *cp)
{
	free((void *) cp);
	return 0;
}

/* called with osi_sleepFDCS locked; returns with same, so that
 * we know that the sleep info pointed to by the cookie won't change
 * until the caller releases the lock.
 */
void osi_AdvanceSleepFD(osi_sleepFD_t *cp)
{
	int idx;		/* index we're dealing with */
	int oidx;		/* index we locked */
	osi_sleepInfo_t *sip;
	osi_sleepInfo_t *nsip;

	idx = 0;	/* so we go around once safely */
	sip = NULL;
	while(idx < OSI_SLEEPHASHSIZE) {
		/* cp->sip should be held */
		idx = cp->idx;
		EnterCriticalSection(&osi_critSec[idx]);
		oidx = idx;	/* remember original index; that's the one we locked */

		/* if there's a sleep info structure in the FD, it should be held; it
		 * is the one we just processed, so we want to move on to the next.
		 * If not, then we want to process the chain in the bucket idx points
		 * to.
		 */
		if ((sip = cp->sip) == NULL) {
			sip = osi_sleepers[idx];
			if (!sip) idx++;
			else sip->refCount++;
		}
		else {
			/* it is safe to release the current sleep info guy now
			 * since we hold the bucket lock.  Pull next guy out first,
			 * since if sip is deleted, Release will move him into
			 * free list.
			 */
			nsip = (osi_sleepInfo_t *) sip->q.nextp;
			osi_ReleaseSleepInfo(sip);
			sip = nsip;
			
			if (sip) sip->refCount++;
			else idx++;
		}
		cp->idx = idx;
		cp->sip = sip;
		LeaveCriticalSection(&osi_critSec[oidx]);

		/* now, if we advanced to a new sleep info structure, we're
		 * done, otherwise we continue and look at the next hash bucket
		 * until we're out of them.
		 */
		if (sip) break;
	}
}


long osi_SleepFDGetInfo(osi_fd_t *ifdp, osi_remGetInfoParms_t *parmsp)
{
    osi_sleepFD_t *fdp = (osi_sleepFD_t *) ifdp;
    osi_sleepInfo_t *sip;
    long code;

    /* now, grab a mutex serializing all iterations over FDs, so that
     * if the RPC screws up and sends us two calls on the same FD, we don't
     * crash and burn advancing the same FD concurrently.  Probably paranoia,
     * but you generally shouldn't trust stuff coming over the network.
     */
    EnterCriticalSection(&osi_sleepFDCS);

    /* this next call advances the FD to the next guy, and simultaneously validates
     * that the info from the network is valid.  If it isn't, we do our best to
     * resynchronize our position, but we might return some info multiple times.
     */
    osi_AdvanceSleepFD(fdp);

    /* now copy out info */
    if (sip = fdp->sip) {	/* one '=' */
	parmsp->idata[0] = sip->value;
	parmsp->idata[1] = sip->tid;
	parmsp->idata[2] = sip->states;
	parmsp->icount = 3;
	parmsp->scount = 0;
	code = 0;
    }
    else code = OSI_DBRPC_EOF;

    LeaveCriticalSection(&osi_sleepFDCS);

    return code;
}

/* finally, DLL-specific code for NT */
BOOL APIENTRY DLLMain(HANDLE inst, DWORD why, char *reserved)
{
	return 1;
}

/* some misc functions for setting hash table sizes */

/* return true iff x is prime */
int osi_IsPrime(unsigned long x)
{
	unsigned long c;
        
	/* even numbers aren't prime */
	if ((x & 1) == 0 && x != 2) return 0;

        for(c = 3; c<x; c += 2) {
		/* see if x is divisible by c */
		if ((x % c) == 0) return 0;	/* yup, it ain't prime */
                
                /* see if we've gone far enough; only have to compute until
                 * square root of x.
                 */
                if (c*c > x) return 1;
        }

	/* probably never get here */
        return 1;
}

/* return first prime number less than or equal to x */
unsigned long osi_PrimeLessThan(unsigned long x) {
	unsigned long c;
        
        for(c = x; c > 1; c--) {
		if (osi_IsPrime(c)) return c;
        }

	/* ever reached? */
        return 1;
}

/* return the # of seconds since some fixed date */
unsigned long osi_GetBootTime(void)
{
	return osi_bootTime;
}

static int (*notifFunc)(char *, char *, long) = NULL;

void osi_InitPanic(void *anotifFunc)
{
	notifFunc = anotifFunc;
}

void osi_panic(char *msgp, char *filep, long line)
{
	osi_LogPanic(filep, line);

	if (notifFunc)
       		(*notifFunc)(msgp, filep, line);
}

/* get time in seconds since some relatively recent time */
time_t osi_Time(void)
{
    FILETIME fileTime;
    SYSTEMTIME sysTime;
    unsigned long remainder;
    LARGE_INTEGER bootTime;

    /* setup boot time values */
    GetSystemTime(&sysTime);
    SystemTimeToFileTime(&sysTime, &fileTime);

    /* change the base of the time so it won't be negative for a long time */
    fileTime.dwHighDateTime -= 28000000;

    bootTime.HighPart = fileTime.dwHighDateTime;
    bootTime.LowPart = fileTime.dwLowDateTime;
    /* now, bootTime is in 100 nanosecond units, and we'd really rather
     * have it in 1 second units, units 10,000,000 times bigger.
     * So, we divide.
     */
    bootTime = ExtendedLargeIntegerDivide(bootTime, 10000000, &remainder);
#ifdef __WIN64
    return bootTime.QuadPart;
#else
    return bootTime.LowPart;
#endif
}

/* get time in seconds since some relatively recent time */
void osi_GetTime(long *timesp)
{
	FILETIME fileTime;
        SYSTEMTIME sysTime;
        unsigned long remainder;
        LARGE_INTEGER bootTime;

	/* setup boot time values */
        GetSystemTime(&sysTime);
        SystemTimeToFileTime(&sysTime, &fileTime);

	/* change the base of the time so it won't be negative for a long time */
	fileTime.dwHighDateTime -= 28000000;

        bootTime.HighPart = fileTime.dwHighDateTime;
        bootTime.LowPart = fileTime.dwLowDateTime;
        /* now, bootTime is in 100 nanosecond units, and we'd really rather
         * have it in 1 microsecond units, units 10 times bigger.
         * So, we divide.
         */
        bootTime = ExtendedLargeIntegerDivide(bootTime, 10, &remainder);
	bootTime = ExtendedLargeIntegerDivide(bootTime, 1000000, &remainder);
	timesp[0] = bootTime.LowPart;		/* seconds */
        timesp[1] = remainder;			/* microseconds */
}
