/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
#include <afs/param.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <netdb.h>
#endif
#include <time.h>
#include <lock.h>
#include <rx/xdr.h>
#include <rx/rx.h>

#define UBIK_INTERNALS
#include "ubik.h"

/* This file contain useful subroutines for parsing command line args for ubik
 *   applications.
 */

ubik_ParseServerList(argc, argv, ahost, aothers)
    int argc;
    char **argv;
    afs_int32 *ahost;
    afs_int32 *aothers; {
    register afs_int32 i;
    register char *tp;
    register struct hostent *th;
    char hostname[64];
    afs_int32 myHost, temp, counter;
    int inServer, sawServer;
    
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) return UBADHOST;
    bcopy(th->h_addr, &myHost, sizeof(afs_int32));
    *ahost = myHost;

    inServer = 0;	/* haven't seen -servers yet */
    sawServer = 0;
    counter = 0;
    for(i=1; i<argc; i++) {
	/* look for -servers argument */
	tp = argv[i];
	
	if (inServer) {
	    if (*tp == '-') {
		inServer = 0;
	    }
	    else {
		/* otherwise this is a new host name */
		th = gethostbyname(tp);
		if (!th) return UBADHOST;
		bcopy(th->h_addr, &temp, sizeof(afs_int32));
		if (temp != myHost) {
		    if (counter++ >= MAXSERVERS) return UNHOSTS;
		    *aothers++ = temp;
		}
	    }
	}
	/* haven't seen a -server yet */
	if (!strcmp(tp, "-servers")) {
	    inServer = 1;
	    sawServer = 1;
	}
	else if (!strcmp(tp, "-dubik")) {
	    ubik_debugFlag = 1;
	}
    }
    if (!sawServer) {
	/* never saw a -server */
	return UNOENT;
    }
    if (counter	< MAXSERVERS) *aothers++ = 0;	/* null terminate if there's room */
    return 0;
}
