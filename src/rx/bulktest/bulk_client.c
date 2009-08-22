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
#include <signal.h>
#include <stdio.h>
#include <rx/rx.h>
#include <rx/rx_null.h>
#include "bulk.h"

char myHostName[256];

InterruptSignal()
{
    rx_PrintStats(stdout);
    exit(0);
}


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

long FetchFile(), StoreFile();

struct async_work {
    char *host;
    struct rx_connection *conn;
    int store;
    int count;
    int verbose;
    char *local;
    char *remote;
};

int async_nProcs = 0;

char *
allocString(s)
     char *s;
{
    char *new = (char *)malloc(strlen(s) + 1);
    strcpy(new, s);
    return new;
}

void
async_BulkProc(data)
     char *data;
{
    struct async_work *work = (struct async_work *)data;
    struct rx_call *call;
    struct clock startTime, endTime;
    long length;
    long error;
    long msec;
    int i;

    for (i = 0; i < work->count; i++) {
	call = rx_NewCall(work->conn);
	clock_NewTime();
	clock_GetTime(&startTime);
	error =
	    ((work->store ? StoreFile : FetchFile) (call, work->verbose,
						    work->local, work->remote,
						    &length));
	error = rx_EndCall(call, error);

	clock_NewTime();
	clock_GetTime(&endTime);
	msec = clock_ElapsedTime(&startTime, &endTime);
	if (work->verbose && !error)
	    printf("%s: %s %s, %d bytes in %d msec, %d bps\n", work->host,
		   work->store ? "stored" : "fetched",
		   work->store ? work->local : work->remote, length, msec,
		   length * 1000 / msec);
	else if (error)
	    printf("%s: %s of %s failed: error %d\n", work->host,
		   work->store ? "store" : "fetch",
		   work->store ? work->local : work->remote, error);
    }
    async_nProcs--;
    osi_Wakeup(&async_nProcs);
}

async_BulkTest(host, conn, store, count, verbose, file)
     char *host;
     struct rx_connection *conn;
     int store;
     int count;
     int verbose;
     char *file;
{
    char tempfile[256];
    char *name;
    PROCESS pid;
    struct async_work *work =
	(struct async_work *)malloc(sizeof(struct async_work));
    work->host = host;
    work->conn = conn;
    work->store = store;
    work->count = count;
    work->verbose = verbose;
    name = strrchr(file, '/');
    if (!name)
	name = file;
    else
	name++;
/*   sprintf(tempfile, "/usr/tmp/%s.%s", myHostName, name);*/
    sprintf(tempfile, "/usr/tmp/%s", name);
    work->local = allocString(store ? file : tempfile);
    work->remote = allocString(store ? tempfile : file);
    async_nProcs += 1;
    LWP_CreateProcess(async_BulkProc, 3000, RX_PROCESS_PRIORITY, (void *)work,
		      "bulk", &pid);
}

long
FetchFile(call, verbose, localFile, remoteFile, length_ptr)
     struct rx_call *call;
     int verbose;
     char *localFile, *remoteFile;
     long *length_ptr;
{
    int fd = -1, error = 0;
    struct stat status;

    if (StartBULK_FetchFile(call, verbose, remoteFile))
	return BULK_ERROR;
    fd = open(localFile, O_CREAT | O_TRUNC | O_WRONLY, 0666);
    if (fd < 0 || fstat(fd, &status) < 0) {
	fprintf("Could not create %s\n", localFile);
	error = BULK_ERROR;
    } else if (bulk_ReceiveFile(fd, call, &status))
	error = BULK_ERROR;
    *length_ptr = status.st_size;
    if (fd >= 0)
	close(fd);
    /*  If there were any output parameters, then it would be necessary to call EndBULKFetchFile(call, &out1,...) here to pick them up */
    return error;
}

long
StoreFile(call, verbose, localFile, remoteFile, length_ptr)
     struct rx_call *call;
     int verbose;
     char *localFile, *remoteFile;
     long *length_ptr;
{
    int fd = -1, error = 0;
    struct stat status;

    fd = open(localFile, O_RDONLY, 0);
    if (fd < 0 || fstat(fd, &status) < 0) {
	fprintf("Could not open %s\n", localFile);
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

main(argc, argv)
     char **argv;
{
    int store = 0;
    int count = 1;
    int verbose = 0;
    char *hostname = NULL;
    u_long host;
    struct rx_securityClass *null_securityObject;
    struct rx_connection *conn = (struct rx_connection *)0;
    gethostname(myHostName, sizeof(myHostName));
    signal(SIGINT, InterruptSignal);
    rx_Init(0);

    null_securityObject = rxnull_NewClientSecurityObject();
    argc--;
    argv++;
    while (argc) {
	while (**argv == '-') {
	    if (strcmp(*argv, "-h") == 0) {
		hostname = *++argv;
		argc--;
		host = GetIpAddress(hostname);
		conn =
		    rx_NewConnection(host, BULK_SERVER_PORT, BULK_SERVICE_ID,
				     null_securityObject, BULK_NULL);
	    } else if (strcmp(*argv, "-f") == 0)
		store = 0;
	    else if (strcmp(*argv, "-s") == 0)
		store = 1;
	    else if (strcmp(*argv, "-v") == 0)
		verbose = 1;
	    else if (strcmp(*argv, "-c") == 0)
		(count = atoi(*++argv)), argc--;
	    else {
		fprintf(stderr, "Unknown option %s\n", *argv);
		exit(1);
	    }
	    argc--;
	    argv++;
	}
	if (argc < 1) {
	    fprintf(stderr, "local + remote file names expected\n");
	    exit(1);
	}
	if (conn == (struct rx_connection *)0) {
	    fprintf(stderr, "No host specified\n");
	    exit(1);
	}
	async_BulkTest(hostname, conn, store, count, verbose, argv[0]);
	argv++;
	argc--;
    }
    while (async_nProcs)
	osi_Sleep(&async_nProcs);
    if (verbose)
	fprintf(stderr, "All transfers done\n");
    rx_PrintStats(stdout);
    /* Allow Rx to idle down any calls; it's a good idea, but not essential, to call this routine */
    rx_Finalize();
}
