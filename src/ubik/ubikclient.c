/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#if defined(UKERNEL)
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header: /cvs/openafs/src/ubik/ubikclient.c,v 1.8.2.3 2006/07/31 17:07:51 shadow Exp $");

#if defined(UKERNEL)
#include "afs/sysincludes.h"
#include "afsincludes.h"
#include "afs/stds.h"
#include "rx/xdr.h"
#include "rx/rx.h"
#include "afs/lock.h"
#include "afs/rxgen_consts.h"
#include "ubik.h"
#include "afs/pthread_glock.h"
#else /* defined(UKERNEL) */
#include <afs/stds.h>
#include <afs/pthread_glock.h>
#include <stdio.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <lock.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#endif
#include <afs/rxgen_consts.h>
#include "ubik.h"
#endif /* defined(UKERNEL) */


short ubik_initializationState;	/* initial state is zero */


/*
 * parse list for clients
 */
int
ubik_ParseClientList(int argc, char **argv, afs_int32 * aothers)
{
    register afs_int32 i;
    register char *tp;
    register struct hostent *th;
    afs_int32 temp, counter;
    int inServer;

    inServer = 0;		/* haven't seen -servers yet */
    counter = 0;
    for (i = 1; i < argc; i++) {
	/* look for -servers argument */
	tp = argv[i];

	if (inServer) {
	    if (*tp == '-')
		break;		/* done */
	    /* otherwise this is a new host name */
	    LOCK_GLOBAL_MUTEX;
	    th = gethostbyname(tp);
	    if (!th) {
		UNLOCK_GLOBAL_MUTEX;
		return UBADHOST;
	    }
	    memmove((void *)&temp, (const void *)th->h_addr,
		    sizeof(afs_int32));
	    UNLOCK_GLOBAL_MUTEX;
	    if (counter++ >= MAXSERVERS)
		return UNHOSTS;
	    *aothers++ = temp;
	} else {
	    /* haven't seen a -server yet */
	    if (!strcmp(tp, "-servers")) {
		inServer = 1;
	    }
	}
    }
    if (!inServer) {
	/* never saw a -server */
	return UNOENT;
    }
    if (counter < MAXSERVERS)
	*aothers++ = 0;		/* null terminate if room */
    return 0;
}

#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>

static pthread_once_t random_once = PTHREAD_ONCE_INIT;
static int called_afs_random_once;
static pthread_key_t random_number_key;

static void
afs_random_once(void)
{
    assert(pthread_key_create(&random_number_key, NULL) == 0);
    called_afs_random_once = 1;
}

#endif
/* 
 * Random number generator and constants from KnuthV2 2d ed, p170
 *
 * Rules:
 * X = (aX + c) % m
 * m is a power of two 
 * a % 8 is 5
 * a is 0.73m  should be 0.01m .. 0.99m
 * c is more or less immaterial.  1 or a is suggested.
 *
 * NB:  LOW ORDER BITS are not very random.  To get small random numbers,
 *      treat result as <1, with implied binary point, and multiply by 
 *      desired modulus.
 * NB:  Has to be unsigned, since shifts on signed quantities may preserve
 *      the sign bit.
 * 
 * In this case, m == 2^32, the mod operation is implicit. a == pi, which
 * is used because it has some interesting characteristics (lacks any
 * interesting bit-patterns).   
 * 
 */

/* 
 * use time and pid to try to get some initial randomness.
 */
#if !defined(UKERNEL)
#define	ranstage(x)	(x)= (afs_uint32) (3141592621U*((afs_uint32)x)+1)

unsigned int
afs_random(void)
{
#ifdef AFS_PTHREAD_ENV
    afs_uint32 state;

    (called_afs_random_once || pthread_once(&random_once, afs_random_once));
    state = (afs_uint32) pthread_getspecific(random_number_key);
#else
    static afs_uint32 state = 0;
#endif

    if (!state) {
	int i;
	state = time(0) + getpid();
	for (i = 0; i < 15; i++) {
	    ranstage(state);
	}
    }

    ranstage(state);
#ifdef AFS_PTHREAD_ENV
    pthread_setspecific(random_number_key, (const void *)state);
#endif
    return (state);

}

/*
 * returns int 0..14 using the high bits of a pseudo-random number instead of
 * the low bits, as the low bits are "less random" than the high ones...
 * slight roundoff error exists, an excercise for the reader.
 * need to multiply by something with lots of ones in it, so multiply by 
 * 8 or 16 is right out.
 */

static unsigned int
afs_randomMod15(void)
{
    afs_uint32 temp;

    temp = afs_random() >> 4;
    temp = (temp * 15) >> 28;

    return temp;
}
#endif /* !defined(UKERNEL) */

#ifdef abs
#undef abs
#endif /* abs */
#define abs(a) ((a) < 0 ? -1*(a) : (a))
int
ubik_ClientInit(register struct rx_connection **serverconns,
		struct ubik_client **aclient)
{
    int i, j;
    int count;
    int offset;
    register struct ubik_client *tc;

    initialize_U_error_table();

    if (*aclient) {		/* the application is doing a re-initialization */
	LOCK_UBIK_CLIENT((*aclient));
	/* this is an important defensive check */
	if (!((*aclient)->initializationState)) {
	    UNLOCK_UBIK_CLIENT((*aclient));
	    return UREINITIALIZE;
	}

	/* release all existing connections */
	for (tc = *aclient, i = 0; i < MAXSERVERS; i++) {
	    struct rx_connection *rxConn = ubik_GetRPCConn(tc, i);
	    if (rxConn == 0)
		break;
#ifdef AFS_PTHREAD_ENV
	    rx_ReleaseCachedConnection(rxConn);
#else
	    rx_DestroyConnection(rxConn);
#endif
	}
	UNLOCK_UBIK_CLIENT((*aclient));
#ifdef AFS_PTHREAD_ENV
	if (pthread_mutex_destroy(&((*aclient)->cm)))
	    return UMUTEXDESTROY;
#endif
    } else {
	tc = (struct ubik_client *)malloc(sizeof(struct ubik_client));
    }
    if (tc == NULL)
	return UNOMEM;
    memset((void *)tc, 0, sizeof(*tc));
#ifdef AFS_PTHREAD_ENV
    if (pthread_mutex_init(&(tc->cm), (const pthread_mutexattr_t *)0)) {
	return UMUTEXINIT;
    }
#endif
    tc->initializationState = ++ubik_initializationState;

    /* first count the # of server conns so we can randomize properly */
    count = 0;
    for (i = 0; i < MAXSERVERS; i++) {
	if (serverconns[i] == (struct rx_connection *)0)
	    break;
	count++;
    }

    /* here count is the # of servers we're actually passed in.  Compute
     * offset, a number between 0..count-1, where we'll start copying from the
     * client-provided array. */
    for (i = 0; i < count; i++) {
	offset = afs_randomMod15() % count;
	for (j = abs(offset); j < 2 * count; j++) {
	    if (!tc->conns[abs(j % count)]) {
		tc->conns[abs(j % count)] = serverconns[i];
		break;
	    }
	}
    }

    *aclient = tc;
    return 0;
}

/* 
 * ubik_ClientDestroy - destroys a ubik connection.  It calls rx to destroy the
 * component rx connections, then frees the ubik connection structure.
 */

afs_int32
ubik_ClientDestroy(struct ubik_client * aclient)
{
    register int c;

    if (aclient == 0)
	return 0;
    LOCK_UBIK_CLIENT(aclient);
    for (c = 0; c < MAXSERVERS; c++) {
	struct rx_connection *rxConn = ubik_GetRPCConn(aclient, c);
	if (rxConn == 0)
	    break;
#ifdef AFS_PTHREAD_ENV
	rx_ReleaseCachedConnection(rxConn);
#else
	rx_DestroyConnection(rxConn);
#endif
    }
    aclient->initializationState = 0;	/* client in not initialized */
    UNLOCK_UBIK_CLIENT(aclient);
#ifdef AFS_PTHREAD_ENV
    pthread_mutex_destroy(&(aclient->cm));	/* ignore failure */
#endif
    free(aclient);
    return 0;
}

/*
 * RefreshConn -- So that intermittent failures that cause connections to die
 *     don't kill whole ubik connection, refresh them when the connection is in
 *     error.
 */

struct rx_connection *
ubik_RefreshConn(struct rx_connection *tc)
{
    afs_uint32 host;
    u_short port;
    u_short service;
    struct rx_securityClass *sc;
    int si;
    struct rx_connection *newTc;

    host = rx_HostOf(rx_PeerOf(tc));
    port = rx_PortOf(rx_PeerOf(tc));
    service = rx_ServiceIdOf(tc);
    sc = rx_SecurityObjectOf(tc);
    si = rx_SecurityClassOf(tc);

    /*
     * destroy old one after creating new one so that refCount on security
     * object cannot reach zero.
     */
    newTc = rx_NewConnection(host, port, service, sc, si);
    rx_DestroyConnection(tc);
    return newTc;
}

#ifdef AFS_PTHREAD_ENV

pthread_once_t ubik_client_once = PTHREAD_ONCE_INIT;
pthread_mutex_t ubik_client_mutex;
#define LOCK_UCLNT_CACHE \
    assert(pthread_once(&ubik_client_once, ubik_client_init_mutex) == 0 && \
	   pthread_mutex_lock(&ubik_client_mutex)==0)
#define UNLOCK_UCLNT_CACHE assert(pthread_mutex_unlock(&ubik_client_mutex)==0)

void
ubik_client_init_mutex()
{
    assert(pthread_mutex_init(&ubik_client_mutex, NULL) == 0);
}

#else

#define LOCK_UCLNT_CACHE
#define UNLOCK_UCLNT_CACHE

#endif

#define SYNCCOUNT 10
static int *calls_needsync[SYNCCOUNT];	/* proc calls that need the sync site */
static int synccount = 0;

/*
 * call this instead of stub and we'll guarantee to find a host that's up.
 * in the future, we should also put in a protocol to find the sync site
 */
afs_int32
ubik_Call(aproc, aclient, aflags, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,
	  p11, p12, p13, p14, p15, p16)
     int (*aproc) ();
     register struct ubik_client *aclient;
     afs_int32 aflags;
     long p1;
     long p2;
     long p3;
     long p4;
     long p5;
     long p6;
     long p7;
     long p8;
     long p9;
     long p10;
     long p11;
     long p12;
     long p13;
     long p14;
     long p15;
     long p16;
{
    afs_int32 rcode, code, newHost, thisHost, i, count;
    int chaseCount, pass, needsync, inlist, j;
    struct rx_connection *tc;
    struct rx_peer *rxp;
    short origLevel;

    if (!aclient)
	return UNOENT;
    LOCK_UBIK_CLIENT(aclient);

  restart:
    origLevel = aclient->initializationState;
    rcode = UNOSERVERS;
    chaseCount = inlist = needsync = 0;

    LOCK_UCLNT_CACHE;
    for (j = 0; ((j < SYNCCOUNT) && calls_needsync[j]); j++) {
	if (calls_needsync[j] == (int *)aproc) {
	    inlist = needsync = 1;
	    break;
	}
    }
    UNLOCK_UCLNT_CACHE;
    /* 
     * First  pass, we try all servers that are up.
     * Second pass, we try all servers.
     */
    for (pass = 0; pass < 2; pass++) {	/*p */
	/* For each entry in our servers list */
	for (count = 0;; count++) {	/*s */

	    if (needsync) {
		/* Need a sync site. Lets try to quickly find it */
		if (aclient->syncSite) {
		    newHost = aclient->syncSite;	/* already in network order */
		    aclient->syncSite = 0;	/* Will reset if it works */
		} else if (aclient->conns[3]) {
		    /* If there are fewer than four db servers in a cell,
		     * there's no point in making the GetSyncSite call.
		     * At best, it's a wash. At worst, it results in more
		     * RPCs than you would otherwise make.
		     */
		    tc = aclient->conns[count];
		    if (tc && rx_ConnError(tc)) {
			aclient->conns[count] = tc = ubik_RefreshConn(tc);
		    }
		    if (!tc)
			break;
		    code = VOTE_GetSyncSite(tc, &newHost);
		    if (aclient->initializationState != origLevel)
			goto restart;	/* somebody did a ubik_ClientInit */
		    if (code)
			newHost = 0;
		    newHost = htonl(newHost);	/* convert to network order */
		} else {
		    newHost = 0;
		}
		if (newHost) {
		    /* position count at the appropriate slot in the client
		     * structure and retry. If we can't find in slot, we'll
		     * just continue through the whole list 
		     */
		    for (i = 0; i < MAXSERVERS && aclient->conns[i]; i++) {
			rxp = rx_PeerOf(aclient->conns[i]);
			thisHost = rx_HostOf(rxp);
			if (!thisHost)
			    break;
			if (thisHost == newHost) {
			    if (chaseCount++ > 2)
				break;	/* avoid loop asking */
			    count = i;	/* this index is the sync site */
			    break;
			}
		    }
		}
	    }
	    /*needsync */
	    tc = aclient->conns[count];
	    if (tc && rx_ConnError(tc)) {
		aclient->conns[count] = tc = ubik_RefreshConn(tc);
	    }
	    if (!tc)
		break;

	    if ((pass == 0) && (aclient->states[count] & CFLastFailed)) {
		continue;	/* this guy's down */
	    }

	    rcode =
		(*aproc) (tc, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11,
			  p12, p13, p14, p15, p16);
	    if (aclient->initializationState != origLevel) {
		/* somebody did a ubik_ClientInit */
		if (rcode)
		    goto restart;	/* call failed */
		else
		    goto done;	/* call suceeded */
	    }
	    if (rcode < 0) {	/* network errors */
		aclient->states[count] |= CFLastFailed;	/* Mark serer down */
	    } else if (rcode == UNOTSYNC) {
		needsync = 1;
	    } else if (rcode != UNOQUORUM) {
		/* either misc ubik code, or misc appl code, or success. */
		aclient->states[count] &= ~CFLastFailed;	/* mark server up */
		goto done;	/* all done */
	    }
	}			/*s */
    }				/*p */

  done:
    if (needsync) {
	if (!inlist) {		/* Remember proc call that needs sync site */
	    LOCK_UCLNT_CACHE;
	    calls_needsync[synccount % SYNCCOUNT] = (int *)aproc;
	    synccount++;
	    UNLOCK_UCLNT_CACHE;
	    inlist = 1;
	}
	if (!rcode) {		/* Remember the sync site - cmd successful */
	    rxp = rx_PeerOf(aclient->conns[count]);
	    aclient->syncSite = rx_HostOf(rxp);
	}
    }
    UNLOCK_UBIK_CLIENT(aclient);
    return rcode;
}



/*
 * call this after getting back a UNOTSYNC 
 * note that getting a UNOTSYNC error code back does *not* guarantee
 * that there is a sync site yet elected.  However, if there is a sync
 * site out there somewhere, and you're trying an operation that
 * requires a sync site, ubik will return UNOTSYNC, indicating the
 * operation won't work until you find a sync site
 */
static int
try_GetSyncSite(register struct ubik_client *aclient, afs_int32 apos)
{
    struct rx_peer *rxp;
    afs_int32 code;
    int i;
    afs_int32 thisHost, newHost;
    struct rx_connection *tc;
    short origLevel;

    origLevel = aclient->initializationState;

    /* get this conn */
    tc = aclient->conns[apos];
    if (tc && rx_ConnError(tc)) {
	aclient->conns[apos] = (tc = ubik_RefreshConn(tc));
    }
    if (!tc) {
	return -1;
    }

    /* now see if we can find the sync site host */
    code = VOTE_GetSyncSite(tc, &newHost);
    if (aclient->initializationState != origLevel) {
	return -1;		/* somebody did a ubik_ClientInit */
    }

    if (!code && newHost) {
	newHost = htonl(newHost);	/* convert back to network order */

	/*
	 * position count at the appropriate slot in the client
	 * structure and retry. If we can't find in slot, we'll just
	 * continue through the whole list
	 */
	for (i = 0; i < MAXSERVERS; i++) {
	    rxp = rx_PeerOf(aclient->conns[i]);
	    thisHost = rx_HostOf(rxp);
	    if (!thisHost) {
		return -1;
	    } else if (thisHost == newHost) {
		return i;	/* we were told to use this one */
	    }
	}
    }
    return -1;
}

/* 
 * Create an internal version of ubik_CallIter that takes an additional
 * parameter - to indicate whether the ubik client handle has already
 * been locked.
 */

#define NEED_LOCK 1
#define NO_LOCK 0

static afs_int32
CallIter(aproc, aclient, aflags, apos, p1, p2, p3, p4, p5, p6, p7, p8, p9,
	 p10, p11, p12, p13, p14, p15, p16, needlock)
     int (*aproc) ();
     register struct ubik_client *aclient;
     afs_int32 aflags;
     int *apos;
     long p1;
     long p2;
     long p3;
     long p4;
     long p5;
     long p6;
     long p7;
     long p8;
     long p9;
     long p10;
     long p11;
     long p12;
     long p13;
     long p14;
     long p15;
     long p16;
     int needlock;
{
    register afs_int32 code;
    struct rx_connection *tc;
    short origLevel;

    if (needlock) {
	LOCK_UBIK_CLIENT(aclient);
    }
    origLevel = aclient->initializationState;

    code = UNOSERVERS;

    while (*apos < MAXSERVERS) {
	/* tc is the next conn to try */
	tc = aclient->conns[*apos];
	if (!tc) {
	    if (needlock) {
		UNLOCK_UBIK_CLIENT(aclient);
	    }
	    return UNOSERVERS;
	}

	if (rx_ConnError(tc)) {
	    tc = ubik_RefreshConn(tc);
	    aclient->conns[*apos] = tc;
	}

	if ((aflags & UPUBIKONLY) && (aclient->states[*apos] & CFLastFailed)) {
	    (*apos)++;		/* try another one if this server is down */
	} else {
	    break;		/* this is the desired path */
	}
    }
    if (*apos >= MAXSERVERS) {
	if (needlock) {
	    UNLOCK_UBIK_CLIENT(aclient);
	}
	return UNOSERVERS;
    }

    code =
	(*aproc) (tc, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13,
		  p14, p15, p16);
    if (aclient->initializationState != origLevel) {
	if (needlock) {
	    UNLOCK_UBIK_CLIENT(aclient);
	}
	return code;		/* somebody did a ubik_ClientInit */
    }

    /* what should I do in case of UNOQUORUM ? */
    if (code < 0) {
	aclient->states[*apos] |= CFLastFailed;	/* network errors */
    } else {
	/* either misc ubik code, or misc application code or success. */
	aclient->states[*apos] &= ~CFLastFailed;	/* operation worked */
    }

    (*apos)++;
    if (needlock) {
	UNLOCK_UBIK_CLIENT(aclient);
    }
    return code;
}

/* 
 * call this instead of stub and we'll guarantee to find a host that's up.
 * in the future, we should also put in a protocol to find the sync site
 */
afs_int32
ubik_Call_New(aproc, aclient, aflags, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,
	      p11, p12, p13, p14, p15, p16)
     int (*aproc) ();
     register struct ubik_client *aclient;
     afs_int32 aflags;
     long p1;
     long p2;
     long p3;
     long p4;
     long p5;
     long p6;
     long p7;
     long p8;
     long p9;
     long p10;
     long p11;
     long p12;
     long p13;
     long p14;
     long p15;
     long p16;
{
    afs_int32 code, rcode;
    afs_int32 count;
    afs_int32 temp;
    int pass;
    int stepBack;
    short origLevel;

    LOCK_UBIK_CLIENT(aclient);
  restart:
    rcode = UNOSERVERS;
    origLevel = aclient->initializationState;

    /* Do two passes. First pass only checks servers known running */
    for (aflags |= UPUBIKONLY, pass = 0; pass < 2;
	 pass++, aflags &= ~UPUBIKONLY) {
	stepBack = 0;
	count = 0;
	while (1) {
	    code =
		CallIter(aproc, aclient, aflags, &count, p1, p2, p3, p4, p5,
			 p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16,
			 NO_LOCK);
	    if (code && (aclient->initializationState != origLevel)) {
		goto restart;
	    }
	    if (code == UNOSERVERS) {
		break;
	    }
	    rcode = code;	/* remember code from last good call */

	    if (code == UNOTSYNC) {	/* means this requires a sync site */
		if (aclient->conns[3]) {	/* don't bother unless 4 or more srv */
		    temp = try_GetSyncSite(aclient, count);
		    if (aclient->initializationState != origLevel) {
			goto restart;	/* somebody did a ubik_ClientInit */
		    }
		    if ((temp >= 0) && ((temp > count) || (stepBack++ <= 2))) {
			count = temp;	/* generally try to make progress */
		    }
		}
	    } else if ((code >= 0) && (code != UNOQUORUM)) {
		UNLOCK_UBIK_CLIENT(aclient);
		return code;	/* success or global error condition */
	    }
	}
    }
    UNLOCK_UBIK_CLIENT(aclient);
    return rcode;
}

/* 
 * This is part of an iterator.  It doesn't handle finding sync sites
 */
afs_int32
ubik_CallIter(int (*aproc) (), struct ubik_client *aclient,
			       afs_int32 aflags, int *apos, long p1, long p2,
			       long p3, long p4, long p5, long p6, long p7,
			       long p8, long p9, long p10, long p11, long p12,
			       long p13, long p14, long p15, long p16)
{
    return CallIter(aproc, aclient, aflags, apos, p1, p2, p3, p4, p5, p6, p7,
		    p8, p9, p10, p11, p12, p13, p14, p15, p16, NEED_LOCK);
}
