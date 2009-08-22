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
#include <sys/stat.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <rx/xdr.h>
#include "bulk.h"

/* Bogus procedure to get internet address of host */
static u_long
GetIpAddress(char *hostname)
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

long FetchFile(struct rx_call *call, int verbose, char *localFile,
	       char *remoteFile, long *length_ptr);
long StoreFile(struct rx_call *call, int verbose, char *localFile,
	       char *remoteFile, long *length_ptr);


int
main(int argc, char **argv)
{
    char *localFile, *remoteFile;
    u_long host;
    long length;
    struct rx_connection *conn;
    struct rx_call *call;
    struct clock startTime, endTime;
    int fetch = 0, store = 0, verbose = 0;
    struct rx_securityClass *null_securityObject;
    int error = 0;
    long msec;

    argc--;
    argv++;
    while (**argv == '-') {
	if (strcmp(*argv, "-fetch") == 0)
	    fetch = 1;
	else if (strcmp(*argv, "-store") == 0)
	    store = 1;
	else if (strcmp(*argv, "-verbose") == 0)
	    verbose = 1;
	else {
	    fprintf(stderr, "Unknown option %s\n", *argv);
	    exit(1);
	}
	argc--;
	argv++;
    }
    if (argc != 3 || !(fetch ^ store)) {
	fprintf(stderr,
		"bulk_client -fetch/-store localFile host remoteFile\n");
	exit(1);
    }
    localFile = argv[0];
    host = GetIpAddress(argv[1]);
    remoteFile = argv[2];

    rx_Init(0);
    null_securityObject = rxnull_NewClientSecurityObject();
    conn =
	rx_NewConnection(host, BULK_SERVER_PORT, BULK_SERVICE_ID,
			 null_securityObject, BULK_NULL);

    clock_NewTime();
    clock_GetTime(&startTime);

    call = rx_NewCall(conn);
    (fetch ? FetchFile : StoreFile) (call, verbose, localFile, remoteFile,
				     &length);
    error = rx_EndCall(call, error);

    clock_NewTime();
    clock_GetTime(&endTime);
    msec = clock_ElapsedTime(&startTime, &endTime);
    if (!error)
	printf("Transferred %d bytes in %d msec, %d bps\n", length, msec,
	       length * 1000 / msec);
    else
	printf("transfer failed: error %d\n", error);

    /* Allow Rx to idle down any calls; it's a good idea, but not essential, to call this routine */
    rx_Finalize();
}

long
FetchFile(struct rx_call *call, int verbose, char *localFile,
	  char *remoteFile, long *length_ptr)
{
    int fd = -1, error = 0;
    struct stat status;

    if (StartBULK_FetchFile(call, verbose, remoteFile))
	return BULK_ERROR;
    fd = open(localFile, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0 || fstat(fd, &status) < 0) {
	fprintf(stderr, "Could not create %s\n", localFile);
	error = BULK_ERROR;
    }
    if (bulk_ReceiveFile(fd, call, &status))
	error = BULK_ERROR;
    *length_ptr = status.st_size;
    if (fd >= 0)
	close(fd);
    /*  If there were any output parameters, then it would be necessary to call EndBULKFetchFile(call, &out1,...) here to pick them up */
    return error;
}

long
StoreFile(struct rx_call *call, int verbose, char *localFile,
	  char *remoteFile, long *length_ptr)
{
    int fd = -1, error = 0;
    struct stat status;

    fd = open(localFile, O_RDONLY, 0);
    if (fd < 0 || fstat(fd, &status) < 0) {
	fprintf(stderr, "Could not open %s\n", localFile);
	return BULK_ERROR;
    }
    error = StartBULK_StoreFile(call, verbose, remoteFile);
    if (!error)
	error = bulk_SendFile(fd, call, &status);
    /*  If there were any output parameters, then it would be necessary to call EndBULKStoreFile(call, &out1,...) here to pick them up */
    close(fd);
    *length_ptr = status.st_size;
    return error;
}
