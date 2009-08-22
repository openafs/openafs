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


#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif
#include <afs/afs_Admin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>
#include <rx/rx.h>
#include <rx/rxstat.h>

#ifdef AFS_DARWIN_ENV
pthread_mutex_t des_init_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t des_random_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rxkad_random_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* AFS_DARWIN_ENV */

void
Usage(void)
{
    fprintf(stderr, "Usage: rxstat_query_process <host> <port>\n");
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
    struct rx_connection *conn;
    char *srvrName;
    long srvrPort;
    void *cellHandle;
    afs_RPCStatsState_t state;

    ParseArgs(argc, argv, &srvrName, &srvrPort);

    rc = afsclient_Init(&st);
    if (!rc) {
	fprintf(stderr, "afsclient_Init, status %d\n", st);
	exit(1);
    }

    rc = afsclient_NullCellOpen(&cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_NullCellOpen, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RPCStatOpenPort(cellHandle, srvrName, srvrPort, &conn,
				   &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatOpenPort, status %d\n", st);
	exit(1);
    }

    rc = util_RPCStatsStateGet(conn, RXSTATS_QueryProcessRPCStats, &state,
			       &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsStateGet, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RPCStatClose(conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatClose, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CellClose(cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellClose, status %d\n", st);
	exit(1);
    }

    printf("\n");
    printf("Process RPC stats are ");
    switch (state) {
    case AFS_RPC_STATS_DISABLED:
	printf("disabled\n");
	break;
    case AFS_RPC_STATS_ENABLED:
	printf("enabled\n");
	break;
    default:
	printf("INVALID\n");
	break;
    }
    printf("\n");

    exit(0);
}
