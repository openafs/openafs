/* Copyright (C) 1991 Transarc Corporation - All rights reserved */

/* RX Authentication Stress test: private shared structures. */


#include "stress_errs.h"

extern struct ktc_encryptionKey serviceKey;
extern long serviceKeyVersion;

#define RXKST_SERVER_NAME "rxkad_stress_test_server"
#define RXKST_SERVER_INST ""
#define RXKST_CLIENT_NAME "rxkad_stress_test_client"
#define RXKST_CLIENT_INST ""

extern int errno;

struct serverParms {
    char *whoami;
    u_int threads;
    int authentication;			/* minimum level  of auth to permit */
};

struct clientParms {
    char *whoami;
    u_int threads;
    char server[32];
    u_long sendLen;			/* parameters for call to Copious */
    u_long recvLen;
    u_long fastCalls;			/* number of calls to perform */
    u_long slowCalls;
    u_long copiousCalls;
    int noExit;				/* don't exit after successful end */
    int printStats;			/* print rx statistics before exit */
    int printTiming;			/* print timings for calls */
    int callTest;			/* check call number preservation */
    int hijackTest;			/* check hijack prevention measures */
    int stopServer;			/* send stop server RPC */
    int authentication;			/* type of authentication to use */
    u_long repeatInterval;		/* secs between load test activity */
    u_long repeatCount;			/* times load test activity repeated */
};

long rxkst_StartClient(INOUT struct clientParms *parms);
long rxkst_StartServer(INOUT struct serverParms *parms);

long RXKST_Fast();
long RXKST_Slow();
long RXKST_Copious();
long RXKST_Kill();

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

/* to keep GCC happy */

extern int LWP_CreateProcess();
extern char *lcstring();
extern void exit();
extern int cmd_AddParm();
extern int cmd_Dispatch();
