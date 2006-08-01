/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSISLEEP_H_ENV_
#define _OSISLEEP_H_ENV_ 1

/*#include "osi.h"*/
#include "osifd.h"
#include "osiqueue.h"

/* states bits */
#define OSI_SLEEPINFO_SIGNALLED	1	/* this sleep structure has been signalled */
#define OSI_SLEEPINFO_INHASH	2	/* this guy is in the hash table */
#define OSI_SLEEPINFO_DELETED	4	/* remove this guy when refcount hits 0 */

/* waitinfo bits */
#define OSI_SLEEPINFO_W4READ	1	/* waiting for a read lock */
#define OSI_SLEEPINFO_W4WRITE	2	/* waiting for a write lock */
typedef struct osi_sleepInfo {
	osi_queue_t q;
	LONG_PTR value;		/* sleep value when in a sleep queue, patch addr for turnstiles */
	size_t tid;		/* thread ID of sleeper */
	EVENT_HANDLE sema;	/* semaphore for this entry */
	unsigned short states;	/* states bits */
	unsigned short idx;	/* sleep hash table we're in, if in hash */
        unsigned short waitFor;	/* what are we waiting for; used for bulk wakeups */
	unsigned long refCount; /* reference count from FDs */
} osi_sleepInfo_t;

/* first guy is the most recently added process */
typedef struct osi_turnstile {
	osi_sleepInfo_t *firstp;
	osi_sleepInfo_t *lastp;
} osi_turnstile_t;

typedef struct osi_sleepFD{
	osi_fd_t fd;		/* FD header */
	osi_sleepInfo_t *sip;	/* ptr to the dude */
	int idx;		/* hash index */
} osi_sleepFD_t;

/* struct for single-shot initialization support */
typedef struct osi_once {
	long atomic;	/* used for atomicity */
	int done;	/* tells if initialization is done */
} osi_once_t;

/* size of mutex hash table; should be a prime number; used for mutex and lock hashing */
#define OSI_MUTEXHASHSIZE	251	/* prime number */

#define osi_MUTEXHASH(x) ((unsigned short) (((LONG_PTR) x) % (intptr_t) OSI_MUTEXHASHSIZE))

/* size of sleep value hash table.  Must be power of 2 */
#define OSI_SLEEPHASHSIZE	128

/* hash function */
#define osi_SLEEPHASH(x)	(((x)>>2)&(OSI_SLEEPHASHSIZE-1))

/* export this so that RPC function can call osi_NextSleepCookie while
 * holding this lock, so that locks don't get released while we're copying
 * out this info.
 */
extern Crit_Sec osi_sleepCookieCS;

/* spin lock version of atomic sleep, used internally only */
extern void osi_SleepSpin(LONG_PTR value, Crit_Sec *counterp);

/* spin lock version of wakeup, used internally only */
extern void osi_WakeupSpin(LONG_PTR value);

/* exported function to sleep on a value */
extern void osi_Sleep (LONG_PTR);

extern void osi_FreeSleepInfo(osi_sleepInfo_t *);

/* function to atomically initialize and return a "once only"
 * structure.  Returns true if you're the first caller, otherwise
 * returns 0.
 */
extern int osi_Once(osi_once_t *);

/* function like the above, but doesn't set the once-only flag.
 * Can be used as optimization to tell if osi_Once has been
 * called.  If it returns true, by the time you really call
 * osi_Once, someone else may have called it, but if it
 * return false, you're guaranteed it will stay false, and that
 * osi_Once would return false, too.
 */
extern int osi_TestOnce(osi_once_t *);

/* called once for each call to osi_Once that returns true; permits other
 * calls to osi_Once to proceed (and return false).
 */
extern void osi_EndOnce(osi_once_t *);


/* exported function to wakeup those sleeping on a value */
extern void osi_Wakeup (LONG_PTR);

extern void osi_Init (void);

/* create a ptr to a cookie */
osi_sleepFD_t *osi_CreateSleepCookie(void);

/* release a ptr to a sleep cookie */
void osi_FreeSleepCookie(osi_sleepFD_t *);

/* advance a sleep cookie to the next ptr */
int osi_NextSleepCookie(osi_sleepFD_t *);

/* functions for the sleep FD implementation */
extern long osi_SleepFDCreate(osi_fdType_t *, osi_fd_t **);
extern long osi_SleepFDGetInfo(osi_fd_t *, osi_remGetInfoParms_t *);
extern long osi_SleepFDClose(osi_fd_t *);

/* functions for getting hash sizes */
extern int osi_IsPrime(unsigned long);
extern unsigned long osi_PrimeLessThan(unsigned long);

/* time functions */
unsigned long osi_GetBootTime(void);

#define osi_assert(x)	\
    do { \
	if (!(x)) osi_panic(NULL, __FILE__, __LINE__); \
    } while(0)

#define osi_assertx(x,s)	\
    do { \
	if (!(x)) osi_panic((s), __FILE__, __LINE__); \
    } while(0)

/* panic */
void osi_InitPanic(void *anotifFunc);
void osi_panic(char *, char *, long);

time_t osi_Time(void);

extern void osi_TWait(osi_turnstile_t *turnp, int waitFor, void *patchp,
	Crit_Sec *releasep);

extern void osi_TSignal(osi_turnstile_t *turnp);

extern void osi_TBroadcast(osi_turnstile_t *turnp);

extern void osi_TSignalForMLs(osi_turnstile_t *turnp, int stillHaveReaders, Crit_Sec *csp);

#define osi_TInit(t)	((t)->firstp = (t)->lastp = 0)

#define osi_TEmpty(t)	((t)->firstp == NULL)

#endif /*_OSISLEEP_H_ENV_ */
