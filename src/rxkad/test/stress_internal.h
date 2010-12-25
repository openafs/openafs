/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* RX Authentication Stress test: private shared structures. */


#include "stress_errs.h"

extern struct ktc_encryptionKey serviceKey;
extern long serviceKeyVersion;

#define RXKST_SERVER_NAME "rxkad_stress_test_server"
#define RXKST_SERVER_INST ""
#define RXKST_CLIENT_NAME "rxkad_stress_test_client"
#define RXKST_CLIENT_INST ""
#define RXKST_CLIENT_CELL "rxtest.openafs.org"

extern int errno;

struct serverParms {
    char *whoami;
    u_int threads;
    int authentication;		/* minimum level  of auth to permit */
    char *keyfile;
};

struct clientParms {
    char *whoami;
    u_int threads;
    char server[32];
    u_long sendLen;		/* parameters for call to Copious */
    u_long recvLen;
    u_long fastCalls;		/* number of calls to perform */
    u_long slowCalls;
    u_long copiousCalls;
    int noExit;			/* don't exit after successful end */
    int printStats;		/* print rx statistics before exit */
    int printTiming;		/* print timings for calls */
    int callTest;		/* check call number preservation */
    int hijackTest;		/* check hijack prevention measures */
    int stopServer;		/* send stop server RPC */
    int authentication;		/* type of authentication to use */
    int useTokens;		/* use user's existing tokens */
    char *cell;			/* test cell name */
    u_long repeatInterval;	/* secs between load test activity */
    u_long repeatCount;		/* times load test activity repeated */
};

long rxkst_StartClient(INOUT struct clientParms *parms);
void *rxkst_StartServer(void *rock);

/* For backward compatibility with AFS3.0 release. */

#ifndef assert
#define assert(x) \
    (!(x) ? (fprintf (stderr, "assertion failed: line %d, file %s\n",\
		      __LINE__,__FILE__), fflush(stderr), abort(), 0) : 0)
#endif

#ifdef opaque
#undef opaque
#undef const
#ifdef __STDC__
typedef void *opaque;
#else /* __STDC__ */
#define const
typedef char *opaque;
#endif /* __STDC__ */
#endif

#ifndef rx_GetPacketCksum
#define rxs_Release(a) RXS_Close(a)
#endif


