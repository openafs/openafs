/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.
 */

/*
 * This file contains sample code for the rxstats interface 
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif
#include <afs/afs_Admin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

#ifdef AFS_DARWIN_ENV
pthread_mutex_t des_init_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t des_random_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rxkad_random_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* AFS_DARWIN_ENV */

void
Usage()
{
    fprintf(stderr, "Usage: rxdebug_conns <host> <port>\n");
    exit(1);
}

void
ParseArgs(int argc, char *argv[], char **srvrName, long *srvrPort)
{
    char **argp = argv;

    if (!*(++argp))
	Usage();
    *srvrName = *(argp++);
    if (!*(argp))
	Usage();
    *srvrPort = strtol(*(argp++), NULL, 0);
    if (*srvrPort <= 0 || *srvrPort >= 65536)
	Usage();
    if (*(argp))
	Usage();
}

int
main(int argc, char *argv[])
{
    int rc;
    afs_status_t st = 0;
    rxdebugHandle_p handle;
    char *srvrName;
    long srvrPort;
    void *iterator;
    struct rx_debugConn conn;
    afs_uint32 supportedStats;
    afs_uint32 supportedValues;
    int allconns = 1;
    int i;

    ParseArgs(argc, argv, &srvrName, &srvrPort);

    rc = afsclient_Init(&st);
    if (!rc) {
	fprintf(stderr, "afsclient_Init, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RXDebugOpenPort(srvrName, srvrPort, &handle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RXDebugOpenPort, status %d\n", st);
	exit(1);
    }

    rc = util_RXDebugSupportedStats(handle, &supportedStats, &st);
    if (!rc) {
	fprintf(stderr, "util_RXDebugSupportedStats, status %d\n", st);
	exit(1);
    }

    rc = util_RXDebugConnectionsBegin(handle, allconns, &iterator, &st);
    if (!rc && st == ADMCLIENTRXDEBUGNOTSUPPORTED) {
	allconns = 0;
	rc = util_RXDebugConnectionsBegin(handle, allconns, &iterator, &st);
    }
    if (!rc) {
	fprintf(stderr, "util_RXDebugConnectionsBegin, status %d\n", st);
	exit(1);
    }

    printf("\n");
    if (allconns) {
	printf("Listing all connections for server %s (port %d)\n", srvrName,
	       srvrPort);
    } else {
	printf
	    ("Listing only interesting connections for server %s (port %d)\n",
	     srvrName, srvrPort);
    }

    while (util_RXDebugConnectionsNext
	   (iterator, &conn, &supportedValues, &st)) {
	printf("\n");
	printf("host:                     %u.%u.%u.%u\n",
	       (conn.host >> 24) & 0xff, (conn.host >> 16) & 0xff,
	       (conn.host >> 8) & 0xff, conn.host & 0xff);
	printf("cid:                      %08x\n", conn.cid);
	printf("serial:                   %08x\n", conn.serial);
	printf("error:                    %u\n", conn.error);
	printf("port:                     %u\n", conn.port);
	printf("flags:                    %x\n", conn.flags);
	printf("type:                     %u\n", conn.type);
	printf("securityIndex:            %u\n", conn.securityIndex);
	for (i = 0; i < RX_MAXCALLS; i++) {
	    printf("callNumber[%u]:            %u\n", i, conn.callNumber[i]);
	    printf("callState[%u]:             %u\n", i, conn.callState[i]);
	    printf("callMode[%u]:              %u\n", i, conn.callMode[i]);
	    printf("callFlags[%u]:             %x\n", i, conn.callFlags[i]);
	    printf("callOther[%u]:             %x\n", i, conn.callOther[i]);
	}
	if (supportedStats & RX_SERVER_DEBUG_SEC_STATS) {
	    printf("secStats.type:            %u\n", conn.secStats.type);
	    printf("secStats.level:           %u\n", conn.secStats.level);
	    printf("secStats.flags:           %x\n", conn.secStats.flags);
	    printf("secStats.expires:         %x\n", conn.secStats.expires);
	    printf("secStats.packetsReceived: %x\n",
		   conn.secStats.packetsReceived);
	    printf("secStats.packetsSent:     %x\n",
		   conn.secStats.packetsSent);
	    printf("secStats.bytesReceived:   %x\n",
		   conn.secStats.bytesReceived);
	    printf("secStats.bytesSent:       %x\n", conn.secStats.bytesSent);
	    printf("natMTU:                   %u\n", conn.natMTU);
	    printf("epoch:                    %08x\n", conn.epoch);
	}
    }
    if (st != ADMITERATORDONE) {
	fprintf(stderr, "util_RXDebugConnectionsNext, status %d\n", st);
	exit(1);
    }
    printf("\n");

    rc = util_RXDebugConnectionsDone(iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_RXDebugConnectionsDone, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RXDebugClose(handle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RXDebugClose, status %d\n", st);
	exit(1);
    }

    exit(0);
}
