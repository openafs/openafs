/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Sample program using multi_Rx, to execute calls in parallel to multiple hosts */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include "sample.h"
#include "rx/rx_clock.h"

/* Bogus procedure to get internet address of host */
static u_long
GetIpAddress(hostname)
     char *hostname;
{
    struct hostent *hostent;
    u_long host;
    hostent = gethostbyname(hostname);
    if (!hostent) {
	printf("host %s not found", hostname);
	exit(1);
    }
    if (hostent->h_length != sizeof(u_long)) {
	printf("host address is disagreeable length (%d)", hostent->h_length);
	exit(1);
    }
    memcpy((char *)&host, hostent->h_addr, sizeof(host));
    return host;
}

main(argc, argv)
     char **argv;
{
    struct rx_securityClass *null_securityObject;
    int i, nHosts = 0;
    struct {
	u_long ipaddr;
	char *name;
    } host[50];
    int arg[50];
    struct rx_connection *conns[50];
    int nSuccesses = 0;
    int trials = 1;
    int verbose = 0;
    int abort = 0;
    int msec;
    struct clock startTime, endTime;
    int result;
    argc--;
    argv++;
    while (**argv == '-') {
	if (strcmp(*argv, "-verbose") == 0)
	    verbose = 1;
	else if (strcmp(*argv, "-count") == 0)
	    trials = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-abort") == 0)
	    abort = 1;
	else {
	    fprintf("Unknown option %s\n", *argv);
	    exit(1);
	}
	argc--;
	argv++;
    }
    while (argc--) {
	host[nHosts].name = *argv;
	host[nHosts].ipaddr = GetIpAddress(*argv);
	arg[nHosts] = 10000 * (nHosts + 1);	/* a bogus argument to show how an input argument to the multi call can be indexed by multi_i */
	nHosts++;
	argv++;
    }

    rx_Init(0);
    null_securityObject = rxnull_NewClientSecurityObject();
    for (i = 0; i < nHosts; i++) {
	conns[i] =
	    rx_NewConnection(host[i].ipaddr, SAMPLE_SERVER_PORT,
			     SAMPLE_SERVICE_ID, null_securityObject,
			     SAMPLE_NULL);
    }

    clock_NewTime();
    clock_GetTime(&startTime);
    for (i = 0; i < trials; i++) {
	multi_Rx(conns, nHosts) {
	    /* Note that multi_i is available both for arguments (result could also be indexed by multi_i, if you want to keep the results apart, and after the call completes) and in the code which follows the completion of each multi_TEST_Add.  At completion, multi_i will be set to the connection index of the call which completed, and multi_error will be set to the error code returned by that call. */
	    if (verbose)
		printf("%s:  About to add %d to %d\n", host[multi_i].name,
		       arg[multi_i], i * 10, &result);
	    multi_TEST_Add(verbose, arg[multi_i], i * 10, &result);
	    if (verbose)
		printf("%s: error %d, %d + %d is %d\n", host[multi_i].name,
		       multi_error, arg[multi_i], i * 10, result);
	    if (abort && multi_error)
		multi_Abort;
	    if (multi_error == 0)
		nSuccesses++;
	    else if (!verbose)
		printf("%s: error %d\n", host[multi_i].name, multi_error);
	}
	multi_End;
    }
    clock_NewTime();
    clock_GetTime(&endTime);
    msec = clock_ElapsedTime(&startTime, &endTime);
    printf
	("nSuccesses=%d in %d msec; %d msec per iteration for %d iterations over %d hosts\n",
	 nSuccesses, msec, msec / trials, trials, nHosts);

    /* Allow Rx to idle down any calls; it's a good idea, but not essential, to call this routine */
    rx_Finalize();
}
