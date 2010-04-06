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

#ifdef IGNORE_SOME_GCC_WARNINGS
 # pragma GCC diagnostic warning "-Wformat"
#endif


#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif

#include <rx/rx.h>
#include <rx/rxstat.h>

#include <afs/afs_Admin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

#ifdef AFS_DARWIN_ENV
pthread_mutex_t des_init_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t des_random_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rxkad_random_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* AFS_DARWIN_ENV */

void
Usage(void)
{
    fprintf(stderr, "Usage: rxdebug_peers <host> <port>\n");
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
    struct rx_debugPeer peer;
    afs_uint32 supportedValues;

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

    rc = util_RXDebugPeersBegin(handle, &iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_RXDebugPeersBegin, status %d\n", st);
	exit(1);
    }

    while (util_RXDebugPeersNext(iterator, &peer, &supportedValues, &st)) {
	printf("\n");
	printf("host:            %u.%u.%u.%u\n", (peer.host >> 24) & 0xff,
	       (peer.host >> 16) & 0xff, (peer.host >> 8) & 0xff,
	       peer.host & 0xff);
	printf("port:            %u\n", peer.port);
	printf("ifMTU:           %u\n", peer.ifMTU);
	printf("idleWhen:        %u\n", peer.idleWhen);
	printf("refCount:        %u\n", peer.refCount);
	printf("burstSize:       %u\n", peer.burstSize);
	printf("burst:           %u\n", peer.burst);
	printf("burstWait:       %u.%06u\n", peer.burstWait.sec,
	       peer.burstWait.usec);
	printf("rtt:             %u\n", peer.rtt);
	printf("rtt_dev:         %u\n", peer.rtt_dev);
	printf("timeout:         %u.%06u\n", peer.timeout.sec,
	       peer.timeout.usec);
	printf("nSent:           %u\n", peer.nSent);
	printf("reSends:         %u\n", peer.reSends);
	printf("inPacketSkew:    %u\n", peer.inPacketSkew);
	printf("outPacketSkew:   %u\n", peer.outPacketSkew);
	printf("rateFlag:        %u\n", peer.rateFlag);
	printf("natMTU:          %u\n", peer.natMTU);
	printf("maxMTU:          %u\n", peer.maxMTU);
	printf("maxDgramPackets: %u\n", peer.maxDgramPackets);
	printf("ifDgramPackets:  %u\n", peer.ifDgramPackets);
	printf("MTU:             %u\n", peer.MTU);
	printf("cwind:           %u\n", peer.cwind);
	printf("nDgramPackets:   %u\n", peer.nDgramPackets);
	printf("congestSeq:      %u\n", peer.congestSeq);
	printf("bytesSent:       (%u.%u)\n", hgethi(peer.bytesSent),
	       hgetlo(peer.bytesSent));
	printf("bytesReceived:   (%u.%u)\n", hgethi(peer.bytesReceived),
	       hgetlo(peer.bytesReceived));
    }
    if (st != ADMITERATORDONE) {
	fprintf(stderr, "util_RXDebugPeersNext, status %d\n", st);
	exit(1);
    }
    printf("\n");

    rc = util_RXDebugPeersDone(iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_RXDebugPeersDone, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RXDebugClose(handle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RXDebugClose, status %d\n", st);
	exit(1);
    }

    exit(0);
}
