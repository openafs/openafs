/* Copyright (C) 1999 IBM Corporation - All rights reserved.
 */

/*
 * This file contains sample code for the rxstats interface 
 */

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif
#include <afs/afs_Admin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

void Usage()
{
    fprintf(stderr,
	    "Usage: rxdebug_basic_stats <host> <port>\n");
    exit(1);
}

void ParseArgs(
    int argc,
    char *argv[],
    char **srvrName,
    long *srvrPort)
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

int main(int argc, char *argv[])
{
    int rc;
    afs_status_t st = 0;
    rxdebugHandle_p handle;
    char *srvrName;
    long srvrPort;
    struct rx_debugStats stats;

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

    rc = util_RXDebugBasicStats(handle, &stats, &st);
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
    printf("nFreePackets:      %d\n", stats.nFreePackets);
    printf("packetReclaims:    %d\n", stats.packetReclaims);
    printf("callsExecuted:     %d\n", stats.callsExecuted);
    printf("waitingForPackets: %d\n", stats.waitingForPackets);
    printf("usedFDs:           %d\n", stats.usedFDs);
    printf("version:           '%c'\n", stats.version);
    printf("nWaiting:          %d\n", stats.nWaiting);
    printf("idleThreads:       %d\n", stats.idleThreads);
    printf("\n");

    exit(0);
}
