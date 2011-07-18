/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Client test program */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <winsock2.h>
#include <fcntl.h>
#include <stdlib.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#include <sys/file.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#include "rx_clock.h"
#include "rx.h"
#include "rx_globals.h"
#include "rx_null.h"
#include <errno.h>
#include <afs/afsutil.h>

#ifndef osi_Alloc
#define osi_Alloc(n) (char *) malloc(n)
#endif

int timeReadvs = 0;
int print = 1, eventlog = 0, rxlog = 0;
int fillPackets;
int burst;
struct clock burstTime;
struct clock retryTime;
FILE *debugFile;
int timeout;
struct clock waitTime, computeTime;

void OpenFD();

void
intSignal(int ignore)
{
    Quit("Interrupted");
}

void
quitSignal(int ignore)
{
    static int quitCount = 0;
    if (++quitCount > 1)
	Quit("rx_ctest: second quit signal, aborting");
    rx_debugFile = debugFile = fopen("rx_ctest.db", "w");
    if (debugFile)
	rx_PrintStats(debugFile);
}

#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
int
test_syscall(a3, a4, a5)
     afs_uint32 a3, a4;
     void *a5;
{
    afs_uint32 rcode;
    void (*old) (int);

    old = signal(SIGSYS, SIG_IGN);
    rcode =
	syscall(31 /* AFS_SYSCALL */ , 28 /* AFSCALL_CALL */ , a3, a4, a5);
    signal(SIGSYS, old);

    return rcode;
}
#endif

main(argc, argv)
     char **argv;
{
    char *hostname;
    struct hostent *hostent;
    afs_uint32 host;
    int logstdout = 0;
    struct rx_connection *conn;
    struct rx_call *call;
    int err = 0;
    int nCalls = 1, nBytes = 1;
    int bufferSize = 4000000;
    char *buffer;
    char *sendFile = 0;
    int setFD = 0;
    int jumbo = 0;

#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
    setlinebuf(stdout);
    rxi_syscallp = test_syscall;
#endif


    argv++;
    argc--;
    while (argc && **argv == '-') {
	if (strcmp(*argv, "-silent") == 0)
	    print = 0;
	if (strcmp(*argv, "-jumbo") == 0)
	    jumbo = 1;
	else if (strcmp(*argv, "-nc") == 0)
	    nCalls = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-nb") == 0)
	    nBytes = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-np") == 0)
	    rx_nPackets = atoi(*++argv), argc--;
	else if (!strcmp(*argv, "-nsf"))
	    rxi_nSendFrags = atoi(*++argv), argc--;
	else if (!strcmp(*argv, "-nrf"))
	    rxi_nRecvFrags = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-twind") == 0)
	    rx_initSendWindow = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-rwind") == 0)
	    rx_initReceiveWindow = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-rxlog") == 0)
	    rxlog = 1;
	else if (strcmp(*argv, "-logstdout") == 0)
	    logstdout = 1;
	else if (strcmp(*argv, "-eventlog") == 0)
	    eventlog = 1;
	else if (strcmp(*argv, "-drop") == 0) {
#ifdef RXDEBUG
	    rx_intentionallyDroppedPacketsPer100 = atoi(*++argv), argc--;
#else
            fprintf(stderr, "ERROR: Compiled without RXDEBUG\n");
#endif
        }
	else if (strcmp(*argv, "-burst") == 0) {
	    burst = atoi(*++argv), argc--;
	    burstTime.sec = atoi(*++argv), argc--;
	    burstTime.usec = atoi(*++argv), argc--;
	} else if (strcmp(*argv, "-retry") == 0) {
	    retryTime.sec = atoi(*++argv), argc--;
	    retryTime.usec = atoi(*++argv), argc--;
	} else if (strcmp(*argv, "-timeout") == 0)
	    timeout = atoi(*++argv), argc--;
	else if (strcmp(*argv, "-fill") == 0)
	    fillPackets++;
	else if (strcmp(*argv, "-file") == 0)
	    sendFile = *++argv, argc--;
	else if (strcmp(*argv, "-timereadvs") == 0)
	    timeReadvs = 1;
	else if (strcmp(*argv, "-wait") == 0) {
	    /* Wait time between calls--to test lastack code */
	    waitTime.sec = atoi(*++argv), argc--;
	    waitTime.usec = atoi(*++argv), argc--;
	} else if (strcmp(*argv, "-compute") == 0) {
	    /* Simulated "compute" time for each call--to test acknowledgement protocol.  This is simulated by doing an iomgr_select:  imperfect, admittedly. */
	    computeTime.sec = atoi(*++argv), argc--;
	    computeTime.usec = atoi(*++argv), argc--;
	} else if (strcmp(*argv, "-fd") == 0) {
	    /* Open at least this many fd's. */
	    setFD = atoi(*++argv), argc--;
	} else {
	    err = 1;
	    break;
	}
	argv++, argc--;
    }
    if (err || argc != 1)
	Quit("usage: rx_ctest [-silent] [-rxlog] [-eventlog] [-nc NCALLS] [-np NPACKETS] hostname");
    hostname = *argv++, argc--;

    if (rxlog || eventlog) {
	if (logstdout)
	    debugFile = stdout;
	else
	    debugFile = fopen("rx_ctest.db", "w");
	if (debugFile == NULL)
	    Quit("Couldn't open rx_ctest.db");
	if (rxlog)
	    rx_debugFile = debugFile;
	if (eventlog)
	    rxevent_debugFile = debugFile;
    }

    signal(SIGINT, intSignal);	/*Changed to sigquit since dbx is broken right now */
#ifndef AFS_NT40_ENV
    signal(SIGQUIT, quitSignal);
#endif

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	printf("Can't initialize winsock.\n");
	exit(1);
    }
    rx_EnableHotThread();
#endif

    rx_SetUdpBufSize(256 * 1024);

    if (!jumbo)
        rx_SetNoJumbo();

    hostent = gethostbyname(hostname);
    if (!hostent)
	Abort("host %s not found", hostname);
    if (hostent->h_length != 4)
	Abort("host address is disagreeable length (%d)", hostent->h_length);
    memcpy((char *)&host, hostent->h_addr, sizeof(host));
    if (setFD > 0)
	OpenFD(setFD);
    if (rx_Init(0) != 0) {
	printf("RX failed to initialize, exiting.\n");
	exit(2);
    }
    if (setFD > 0) {
	printf("rx_socket=%d\n", rx_socket);
    }

    printf("Using %d packet buffers\n", rx_nPackets);

    conn =
	rx_NewConnection(host, htons(2500), 3,
			 rxnull_NewClientSecurityObject(), 0);

    if (!conn)
	Abort("unable to make a new connection");

    /* Set initial parameters.  This is (currently) not the approved interface */
    if (burst)
	conn->peer->burstSize = conn->peer->burst = burst;
    if (!clock_IsZero(&burstTime))
	conn->peer->burstWait = burstTime;
    if (!clock_IsZero(&retryTime))
	conn->peer->rtt = _8THMSEC(&retryTime);
    if (sendFile)
	SendFile(sendFile, conn);
    else {
	buffer = (char *)osi_Alloc(bufferSize);
	while (nCalls--) {
	    struct clock startTime;
	    struct timeval t;
	    int nbytes;
	    int nSent;
	    int bytesSent = 0;
	    int bytesRead = 0;
	    call = rx_NewCall(conn);
	    if (!call)
		Abort("unable to make a new call");

	    clock_GetTime(&startTime);
	    for (bytesSent = 0; bytesSent < nBytes; bytesSent += nSent) {
		int tryCount;
		tryCount =
		    (bufferSize >
		     nBytes - bytesSent) ? nBytes - bytesSent : bufferSize;
		nSent = rx_Write(call, buffer, tryCount);
		if (nSent == 0)
		    break;

	    }
	    for (bytesRead = 0; nbytes = rx_Read(call, buffer, bufferSize);
		 bytesRead += nbytes) {
	    };
	    if (print)
		printf("Received %d characters in response\n", bytesRead);
	    err = rx_EndCall(call, 0);
	    if (err)
		printf("Error %d returned from rpc call\n", err);
	    else {
		struct clock totalTime;
		float elapsedTime;
		clock_GetTime(&totalTime);
		clock_Sub(&totalTime, &startTime);
		elapsedTime = clock_Float(&totalTime);
		fprintf(stderr,
			"Sent %d bytes in %0.3f seconds:  %0.0f bytes per second\n",
			bytesSent, elapsedTime, bytesSent / elapsedTime);
	    }
	    if (!clock_IsZero(&computeTime)) {
		t.tv_sec = computeTime.sec;
		t.tv_usec = computeTime.usec;
		if (select(0, 0, 0, 0, &t) != 0)
		    Quit("Select didn't return 0");
	    }
	    if (!clock_IsZero(&waitTime)) {
		struct timeval t;
		t.tv_sec = waitTime.sec;
		t.tv_usec = waitTime.usec;
#ifdef AFS_PTHREAD_ENV
		select(0, 0, 0, 0, &t);
#else
		IOMGR_Sleep(t.tv_sec);
#endif
	    }
            if (debugFile)
                rx_PrintPeerStats(debugFile, rx_PeerOf(conn));
            rx_PrintPeerStats(stdout, rx_PeerOf(conn));
	}
    }
    Quit("testclient: done!\n");
}

int SendFile(file, conn)
     char *file;
     struct rx_connection *conn;
{
    struct rx_call *call;
    int fd;
    struct stat status;
    int blockSize, bytesLeft;
    char *buf;
    int nbytes;
    int err;
    struct clock startTime;
    int receivedStore = 0;
    struct clock totalReadvDelay;
    int nReadvs;
    int code;
#ifdef	AFS_AIX_ENV
#include <sys/statfs.h>
    struct statfs tstatfs;
#endif

    if (timeReadvs) {
	nReadvs = 0;
	clock_Zero(&totalReadvDelay);
    }
    fd = open(file, O_RDONLY, 0);
    if (fd < 0)
	Abort("Couldn't open %s\n", file);
    fstat(fd, &status);
#ifdef AFS_NT40_ENV
    blockSize = 1024;
#else
#ifdef	AFS_AIX_ENV
/* Unfortunately in AIX valuable fields such as st_blksize are gone from the stat structure!! */
    fstatfs(fd, &tstatfs);
    blockSize = tstatfs.f_bsize;
#else
    blockSize = status.st_blksize;
#endif
#endif
    buf = (char *)osi_Alloc(blockSize);
    bytesLeft = status.st_size;
    clock_GetTime(&startTime);
    call = rx_NewCall(conn);
    while (bytesLeft) {
	if (!receivedStore && rx_GetRemoteStatus(call) == 79) {
	    receivedStore = 1;
	    fprintf(stderr,
		    "Remote status indicates file accepted (\"received store\")\n");
	}
	nbytes = (bytesLeft > blockSize ? blockSize : bytesLeft);
	errno = 0;
	code = read(fd, buf, nbytes);
	if (code != nbytes) {
	    Abort("Only read %d bytes of %d, errno=%d\n", code, nbytes,
		  errno);
	}
	code = rx_Write(call, buf, nbytes);
	if (code != nbytes) {
	    Abort("Only wrote %d bytes of %d\n", code, nbytes);
	}
	bytesLeft -= nbytes;
    }
    while ((nbytes = rx_Read(call, buf, sizeof(buf))) > 0) {
	char *p = buf;
	while (nbytes--) {
	    putchar(*p);
	    *p++;
	}
    }
    if ((err = rx_EndCall(call, 0)) != 0) {
	fprintf(stderr, "rx_Endcall returned error %d\n", err);
    } else {
	struct clock totalTime;
	float elapsedTime;
	clock_GetTime(&totalTime);
	clock_Sub(&totalTime, &startTime);
	elapsedTime = totalTime.sec + totalTime.usec / 1e6;
	fprintf(stderr,
		"Sent %d bytes in %0.3f seconds:  %0.0f bytes per second\n",
		status.st_size, elapsedTime, status.st_size / elapsedTime);
	if (timeReadvs) {
	    float delay = clock_Float(&totalReadvDelay) / nReadvs;
	    fprintf(stderr, "%d readvs, average delay of %0.4f seconds\n",
		    nReadvs, delay);
	}
    }
    close(fd);

    return(0);
}

Abort(msg, a, b, c, d, e)
{
    printf((char *)msg, a, b, c, d, e);
    printf("\n");
    if (debugFile) {
	rx_PrintStats(debugFile);
	fflush(debugFile);
    }
    afs_abort();
    exit(1);
}

Quit(char *msg, int a, int b, int c, int d, int e)
{
    printf((char *)msg, a, b, c, d, e);
    printf("\n");
    if (debugFile) {
	rx_PrintStats(debugFile);
	fflush(debugFile);
    }
    rx_PrintStats(stdout);
    exit(0);
}

/* OpenFD
 *
 * Open file descriptors until file descriptor n or higher is returned.
 */
#include <sys/stat.h>
void
OpenFD(n)
     int n;
{
    int i;
    struct stat sbuf;
    int fd, lfd;

    lfd = -1;
    for (i = 0; i < n; i++) {
	if (fstat(i, &sbuf) == 0)
	    continue;
	if ((fd = open("/dev/null", 0, 0)) < 0) {
	    if (lfd >= 0) {
		close(lfd);
		return;
	    }
	} else {
	    if (fd >= n) {
		close(fd);
		return;
	    } else {
		lfd = fd;
	    }
	}
    }
}
