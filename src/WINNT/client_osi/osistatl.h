/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef _OSISTATL_H_ENV_
#define _OSISTATL_H_ENV_ 1

#include "osibasel.h"
#if !defined(_MSC_VER) || (_MSC_VER < 1300)
#include "largeint.h"
#endif
#include "osiqueue.h"

/* structure representing all information about someone holding a lock
 * or mutex, or about someone waiting for a lock or a mutex.
 */
#define OSI_ACTIVEFLAGS_WAITER	1	/* waiting (not owner) */
#define OSI_ACTIVEFLAGS_READER	2	/* for rwlocks, a reader */
#define OSI_ACTIVEFLAGS_WRITER	4	/* for rwlocks, a writer */
typedef struct osi_activeInfo {
	osi_queue_t q;		/* queue of all dudes interested in this lock/mutex */
	LARGE_INTEGER startTime;	/* time we started doing whatever */
	unsigned long tid;	/* thread id */
	char flags;		/* flags of interest */
} osi_activeInfo_t;

/* file descriptor for lock seaches */
typedef struct osi_statFD {
	osi_fd_t fd;
	osi_queue_t *curp;	/* where we're at scan-wise */
	int which;		/* scanning rwlock or mutex queue */
} osi_statFD_t;

/* real states */
#define OSI_STATL_DELETED	1

/* common info needed by lock state queueing package */
typedef struct osi_qiStat {
	osi_activeInfo_t *activeListp;	/* list of active elements */
        char *namep;			/* for user friendliness */
        void *backp;			/* back ptr to real lock/mutex */
} osi_qiStat_t;

/* structure referenced by base lock private data pointer
 * if this is a statistics-gathering mutex.
 * WARNING: we count on the fields q through states having the
 * same layout on the mutex and rwlock structures to make following
 * pointers in the FD code easier.
 */
typedef struct osi_mutexStat {
	osi_queue_t q;		/* queue of all mutexes */
	osi_turnstile_t turn;  	/* the real turnstile */
	unsigned long refCount;	/* so we can iterate cleanly */
	short states;
        DWORD tid;

	/* track # of lock calls and blocks */
	LARGE_INTEGER lockedTime;	/* total time held */
	LARGE_INTEGER blockedTime;/* total time someone was blocked here */
	long lockedCount;	/* count of # of obtains */
	long blockedCount;	/* count of # of blocks */

	osi_qiStat_t qi;
} osi_mutexStat_t;

/* structure referenced by base lock private data pointer
 * if this is a statistics-gathering mutex.
 *
 * WARNING: we count on the fields q through states having the
 * same layout on the mutex and rwlock structures to make following
 * pointers in the FD code easier.
 */
typedef struct osi_rwlockStat {
	osi_queue_t q;			/* queue of all mutexes */
	osi_turnstile_t turn;		/* the real turnstile */
	unsigned long refCount;		/* so we can iterate cleanly */
	short states;
        DWORD tid;

	/* statistics */
	LARGE_INTEGER writeLockedTime;	/* total time held */
	LARGE_INTEGER writeBlockedTime;	/* total time someone was blocked here */
	LARGE_INTEGER readLockedTime;	/* total time held */
	LARGE_INTEGER readBlockedTime;	/* total time someone was blocked here */
	long writeLockedCount;		/* count of # of obtains */
	long writeBlockedCount;		/* count of # of blocks */
	long readLockedCount;		/* count of # of obtains */
	long readBlockedCount;		/* count of # of blocks */

	osi_qiStat_t qi;		/* queue info */
} osi_rwlockStat_t;

typedef void (osi_watchProc_t)(void *rockp, long actualMs, void *lockp);

extern void lock_InitializeRWLockStat(osi_rwlock_t *, char *);

extern void lock_InitializeMutexStat(osi_mutex_t *, char *);

extern osi_activeInfo_t *osi_QueueActiveInfo(osi_qiStat_t *, int);

extern osi_activeInfo_t *osi_FindActiveInfo(osi_qiStat_t *);

extern void osi_RemoveActiveInfo(osi_qiStat_t *, osi_activeInfo_t *);

extern void osi_FreeActiveInfo(osi_activeInfo_t *);

extern void osi_StatInit(void);

extern void osi_SetStatLog(struct osi_log *logp);

extern void osi_SetWatchProc(long ms, osi_watchProc_t *procp, void *rockp);

#endif /*_OSISTATL_H_ENV_ */
