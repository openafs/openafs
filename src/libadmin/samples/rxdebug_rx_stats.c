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
#include <string.h>
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
    fprintf(stderr, "Usage: rxdebug_rx_stats <host> <port>\n");
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

static char *packetTypes[] = RX_PACKET_TYPES;

int
main(int argc, char *argv[])
{
    int rc;
    afs_status_t st = 0;
    rxdebugHandle_p handle;
    char *srvrName;
    long srvrPort;
    struct rx_stats stats;
    afs_uint32 supportedStats;
    char tstr[32];
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

    rc = util_RXDebugRxStats(handle, &stats, &supportedStats, &st);
    if (!rc) {
	fprintf(stderr, "util_RXDebugBasicStats, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RXDebugClose(handle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RXDebugClose, status %d\n", st);
	exit(1);
    }

    printf("\n");
    printf("RX stats: host %s (port %d)\n", srvrName, srvrPort);
    printf("\n");
    printf("    packetRequests:              %d\n", stats.packetRequests);
    printf("    receivePktAllocFailures:     %d\n",
	   stats.receivePktAllocFailures);
    if (supportedStats & RX_SERVER_DEBUG_NEW_PACKETS) {
	printf("    receiveCbufPktAllocFailures: %d\n",
	       stats.receiveCbufPktAllocFailures);
    }
    printf("    sendPktAllocFailures:        %d\n",
	   stats.sendPktAllocFailures);
    if (supportedStats & RX_SERVER_DEBUG_NEW_PACKETS) {
	printf("    sendCbufPktAllocFailures:    %d\n",
	       stats.sendCbufPktAllocFailures);
    }
    printf("    specialPktAllocFailures:     %d\n",
	   stats.specialPktAllocFailures);
    printf("    socketGreedy:                %d\n", stats.socketGreedy);
    printf("    bogusPacketOnRead:           %d\n", stats.bogusPacketOnRead);
    printf("    bogusHost:                   %d\n", stats.bogusHost);
    printf("    noPacketOnRead:              %d\n", stats.noPacketOnRead);
    printf("    noPacketBuffersOnRead:       %d\n",
	   stats.noPacketBuffersOnRead);
    printf("    selects:                     %d\n", stats.selects);
    printf("    sendSelects:                 %d\n", stats.sendSelects);
    printf("    packetsRead:\n");
    for (i = 0; i < RX_N_PACKET_TYPES; i++) {
	strcpy(tstr, packetTypes[i]);
	printf("\t%-24s %d\n", strcat(tstr, ":"), stats.packetsRead[i]);
    }
    printf("    dataPacketsRead:             %d\n", stats.dataPacketsRead);
    printf("    ackPacketsRead:              %d\n", stats.ackPacketsRead);
    printf("    dupPacketsRead:              %d\n", stats.dupPacketsRead);
    printf("    spuriousPacketsRead:         %d\n",
	   stats.spuriousPacketsRead);
    printf("    ignorePacketDally:           %d\n", stats.ignorePacketDally);
    printf("    packetsSent:\n");
    for (i = 0; i < RX_N_PACKET_TYPES; i++) {
	strcpy(tstr, packetTypes[i]);
	printf("\t%-24s %d\n", strcat(tstr, ":"), stats.packetsSent[i]);
    }
    printf("    ackPacketsSent:              %d\n", stats.ackPacketsSent);
    printf("    dataPacketsSent:             %d\n", stats.dataPacketsSent);
    printf("    dataPacketsReSent:           %d\n", stats.dataPacketsReSent);
    printf("    dataPacketsPushed:           %d\n", stats.dataPacketsPushed);
    printf("    ignoreAckedPacket:           %d\n", stats.ignoreAckedPacket);
    printf("    netSendFailures:             %d\n", stats.netSendFailures);
    printf("    fatalErrors:                 %d\n", stats.fatalErrors);
    printf("    nRttSamples:                 %d\n", stats.nRttSamples);
    printf("    totalRtt:                    %.6f\n",
	   clock_Float(&stats.totalRtt));
    printf("    minRtt:                      %.6f\n",
	   clock_Float(&stats.minRtt));
    printf("    maxRtt:                      %.6f\n",
	   clock_Float(&stats.maxRtt));
    printf("    nServerConns:                %d\n", stats.nServerConns);
    printf("    nClientConns:                %d\n", stats.nClientConns);
    printf("    nPeerStructs:                %d\n", stats.nPeerStructs);
    printf("    nCallStructs:                %d\n", stats.nCallStructs);
    printf("    nFreeCallStructs:            %d\n", stats.nFreeCallStructs);
    printf("\n");

    exit(0);
}
