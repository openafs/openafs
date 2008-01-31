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

RCSID
    ("$Header$");

#ifdef	KERNEL
#include <afs/sysincludes.h>
#include <afsincludes.h>
#else
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <malloc.h>
#else
#include <sys/param.h>
#endif
#include <errno.h>
#include <afs/errors.h>
#include "xdr.h"
#ifdef AFS_PTHREAD_ENV
#include "rx.h"
#endif /* AFS_PTHREAD_ENV */
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef AFS_NT40_ENV
#ifndef EDQUOT 
#define EDQUOT WSAEDQUOT
#endif /* EDQUOT */
#endif /* AFS_NT40_ENV */
#endif

/*
 * We currently only include below the errors that
 * affect us the most. We should add to this list
 * more code mappings, as necessary.
 */

/*
 * Convert from the local (host) to the standard 
 * (network) system error code.
 */
int
hton_syserr_conv(register afs_int32 code)
{
    register afs_int32 err;

    if (code == ENOSPC)
	err = VDISKFULL;
#ifdef EDQUOT
    else if (code == EDQUOT)
	err = VOVERQUOTA;
#endif
    else
	err = code;
    return err;
}


/*
 * Convert from the standard (Network) format to the
 * local (host) system error code.
 */
int
ntoh_syserr_conv(int code)
{
    register afs_int32 err;

    if (code == VDISKFULL)
	err = ENOSPC;
    else if (code == VOVERQUOTA)
#ifndef EDQUOT
	err = ENOSPC;
#else
	err = EDQUOT;
#endif
    else
	err = code;
    return err;
}


#ifndef	KERNEL
/*
 * We provide the following because some systems (like aix) would fail if we pass
 * 0 as length.
 */

#ifndef osi_alloc
#ifdef AFS_PTHREAD_ENV
/*
 * This mutex protects the following global variables:
 * osi_alloccnt
 * osi_allocsize
 */

#include <assert.h>
pthread_mutex_t osi_malloc_mutex;
#define LOCK_MALLOC_STATS assert(pthread_mutex_lock(&osi_malloc_mutex)==0)
#define UNLOCK_MALLOC_STATS assert(pthread_mutex_unlock(&osi_malloc_mutex)==0)
#else
#define LOCK_MALLOC_STATS
#define UNLOCK_MALLOC_STATS
#endif /* AFS_PTHREAD_ENV */
long osi_alloccnt = 0, osi_allocsize = 0;
static const char memZero;
char *
osi_alloc(afs_int32 x)
{
    /* 
     * 0-length allocs may return NULL ptr from osi_kalloc, so we special-case
     * things so that NULL returned iff an error occurred 
     */
    if (x == 0)
	return (char *)&memZero;
    LOCK_MALLOC_STATS;
    osi_alloccnt++;
    osi_allocsize += x;
    UNLOCK_MALLOC_STATS;
    return (char *)(mem_alloc(x));
}

int
osi_free(char *x, afs_int32 size)
{
    if ((x == &memZero) || !x)
	return 0;
    LOCK_MALLOC_STATS;
    osi_alloccnt--;
    osi_allocsize -= size;
    UNLOCK_MALLOC_STATS;
    mem_free(x, size);
    return 0;
}
#endif
#endif /* KERNEL */

#if defined(RX_ENABLE_LOCKS) && defined(RX_REFCOUNT_CHECK)
int rx_callHoldType = 0;
#endif


#ifdef RX_LOCKS_DB
/* What follows is a lock database for RX. There is currently room for 400
 * locks to be held. Routines panic if there is an error. To port, add
 * RX_LOCKS_DB versions of MUTEX_{ENTER, EXIT, TRYENTER} and CV_*WAIT to
 * rx_kmutex.h and define lock macros below.
 */
#if (defined(AFS_AIX41_ENV) || (defined(AFS_SGI53_ENV) && defined(MP))) && defined(KERNEL)

#ifdef AFS_AIX41_ENV
Simple_lock rxdb_lock;
#define RXDB_LOCK_INIT()	lock_alloc(&rxdb_lock, LOCK_ALLOC_PIN, 1, 0), \
     				simple_lock_init(&rxdb_lock)
#define RXDB_LOCK_ENTER()	simple_lock(&rxdb_lock)
#define RXDB_LOCK_EXIT()	simple_unlock(&rxdb_lock)
#else /* AFS_AIX41_ENV */
#ifdef AFS_SGI53_ENV
afs_kmutex_t rxdb_lock;
#define RXDB_LOCK_INIT()	mutex_init(&rxdb_lock, "rxdb lock", 0, 0)
#define RXDB_LOCK_ENTER()	AFS_MUTEX_ENTER(&rxdb_lock)
#define RXDB_LOCK_EXIT()	mutex_exit(&rxdb_lock)
#endif /* AFS_SGI53_ENV */
#endif /* AFS_AIX41_ENV */


int RXDB_LockPos = 0;

#define RXDB_NLOCKS 32
struct rxdb_lock_t {
    afs_int32 id;		/* id of lock holder. */
    void *a;			/* address of lock. */
    u_short fileId;		/* fileID# of RX file. */
    u_short line;
    u_short next;
    u_short prev;
};

#define RXDB_HASHSIZE 8


struct rxdb_lock_t rxdb_lockList[RXDB_NLOCKS];
short rxdb_idHash[RXDB_HASHSIZE];
#define RXDB_IDHASH(id) ((((u_long)id)>>1) & (RXDB_HASHSIZE-1))

/* Record locations of all locks we enter/exit. */
struct rxdb_lockloc_t {
    u_short fileId;		/* fileID# of RX file. */
    u_short line;
    u_short next;
    u_short prev;
};
#ifdef RX_LOCKS_COVERAGE
#define RXDB_NlockLocs 512
#define RXDB_LOCHASHSIZE 256
struct rxdb_lockloc_t rxdb_lockLocs[RXDB_NlockLocs];
short rxdb_lockLocHash[RXDB_LOCHASHSIZE];
#define RXDB_LOCHASH(a) ((((u_long)a)) & (RXDB_LOCHASHSIZE-1))
#endif /* RX_LOCKS_COVERAGE */

/* Element 0 of each of the above arrays serves as the pointer to the list of
 * free elements.
 */
void
rxdb_init(void)
{
    static int initted = 0;
    int i;

    if (initted)
	return;

    initted = 1;

    RXDB_LOCK_INIT();
    RXDB_LOCK_ENTER();

    for (i = 1; i < RXDB_NLOCKS - 1; i++) {
	rxdb_lockList[i].next = i + 1;
	rxdb_lockList[i].prev = i - 1;
    }
    rxdb_lockList[0].next = 1;
    rxdb_lockList[0].prev = 0;
    rxdb_lockList[RXDB_NLOCKS - 1].next = 0;
    rxdb_lockList[RXDB_NLOCKS - 1].prev = RXDB_NLOCKS - 2;

#ifdef RX_LOCKS_COVERAGE
    for (i = 1; i < RXDB_NlockLocs - 1; i++) {
	rxdb_lockLocs[i].next = i + 1;
	rxdb_lockLocs[i].prev = i - 1;
    }
    rxdb_lockLocs[0].next = 1;
    rxdb_lockLocs[0].prev = 0;
    rxdb_lockLocs[RXDB_NlockLocs - 1].next = 0;
    rxdb_lockLocs[RXDB_NlockLocs - 1].prev = RXDB_NlockLocs - 2;
#endif /* RX_LOCKS_COVERAGE */

    RXDB_LOCK_EXIT();
}

#ifdef RX_LOCKS_COVERAGE
void
rxdb_RecordLockLocation(fileId, line)
     afs_int32 fileId, line;
{
    u_short i, j;

    i = RXDB_LOCHASH(line);

    /* Only enter lock location into list once. */
    for (j = rxdb_lockLocHash[i]; j; j = rxdb_lockLocs[j].next) {
	if ((rxdb_lockLocs[j].line == line)
	    && (rxdb_lockLocs[j].fileId == fileId))
	    return;
    }

    /* Add lock to list. */
    j = rxdb_lockLocs[0].next;
    if (j == 0) {
	osi_Panic("rxdb_initLock: used up all the lock locations.\n");
    }

    /* Fix up free list. */
    rxdb_lockLocs[0].next = rxdb_lockLocs[j].next;

    /* Put new element at head of list. */
    rxdb_lockLocs[j].next = rxdb_lockLocHash[i];
    rxdb_lockLocs[j].prev = 0;
    if (rxdb_lockLocHash[i]) {
	rxdb_lockLocs[rxdb_lockLocHash[i]].prev = j;
    }
    rxdb_lockLocHash[i] = j;

    /* Set data in element. */
    rxdb_lockLocs[j].fileId = fileId;
    rxdb_lockLocs[j].line = line;

}
#endif /* RX_LOCKS_COVERAGE */


/* Set lock as possessed by me. */
void
rxdb_grablock(a, id, fileId, line)
     void *a;
     afs_int32 id;
     afs_int32 fileId;
     afs_int32 line;
{
    int i, j, k;

    RXDB_LOCK_ENTER();
#ifdef RX_LOCKS_COVERAGE
    rxdb_RecordLockLocation(fileId, line);
#endif /* RX_LOCKS_COVERAGE */
    /* Is lock already held by anyone? */
    for (i = 0; i < RXDB_HASHSIZE; i++) {
	for (j = rxdb_idHash[i]; j; j = rxdb_lockList[j].next) {
	    if (rxdb_lockList[j].a == a) {
		RXDB_LockPos = j;
		osi_Panic("rxdb_grablock: lock already held.");
	    }
	}
    }

    i = RXDB_IDHASH(id);
    j = rxdb_lockList[0].next;
    if (j == 0) {
	osi_Panic("rxdb_grablock: rxdb_lockList is full.");
    }
    rxdb_lockList[0].next = rxdb_lockList[j].next;

    /* Put element at head of list. */
    rxdb_lockList[j].next = rxdb_idHash[i];
    rxdb_lockList[j].prev = 0;
    if (rxdb_idHash[i]) {
	rxdb_lockList[rxdb_idHash[i]].prev = j;
    }
    rxdb_idHash[i] = j;

    /* Set data into element. */
    rxdb_lockList[j].a = a;
    rxdb_lockList[j].id = id;
    rxdb_lockList[j].fileId = fileId;
    rxdb_lockList[j].line = line;

    RXDB_LOCK_EXIT();
}

/* unlock */
rxdb_droplock(a, id, fileId, line)
     void *a;
     afs_int32 id;
     int fileId;
     int line;
{
    int i, j;
    int found;

    RXDB_LOCK_ENTER();
#ifdef RX_LOCKS_COVERAGE
    rxdb_RecordLockLocation(fileId, line);
#endif /* RX_LOCKS_COVERAGE */
    found = 0;

    /* Do I have the lock? */
    i = rxdb_idHash[RXDB_IDHASH(id)];
    for (j = i; j; j = rxdb_lockList[j].next) {
	if (rxdb_lockList[j].a == a) {
	    found = 1;
	    break;
	}
    }

    if (!found) {
	osi_Panic("rxdb_unlock: lock not held by me.\n");
    }

    /* delete lock from queue. */
    if (i == j) {
	/* head of list. */
	i = RXDB_IDHASH(id);
	rxdb_idHash[i] = rxdb_lockList[j].next;
	rxdb_lockList[rxdb_lockList[j].next].prev = 0;
    } else {
	if (rxdb_lockList[j].next)
	    rxdb_lockList[rxdb_lockList[j].next].prev = rxdb_lockList[j].prev;
	rxdb_lockList[rxdb_lockList[j].prev].next = rxdb_lockList[j].next;
    }
    /* Put back on free list. */
    rxdb_lockList[j].next = rxdb_lockList[0].next;
    rxdb_lockList[j].prev = 0;
    rxdb_lockList[0].next = j;

    RXDB_LOCK_EXIT();
}

#endif /* (AIX41 || SGI53) && KERNEL */

#endif /* RX_LOCKS_DB */
