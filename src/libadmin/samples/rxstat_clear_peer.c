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
    fprintf(stderr, "Usage: rxstat_clear_peer <cell> <host> <port>\n");
    exit(1);
}

void
ParseArgs(int argc, char *argv[], char **cellName, char **srvrName,
	  long *srvrPort)
{
    char **argp = argv;

    if (!*(++argp))
	Usage();
    *cellName = *(argp++);
    if (!*(argp))
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
    char *cellName;
    void *tokenHandle;
    void *cellHandle;

    ParseArgs(argc, argv, &cellName, &srvrName, &srvrPort);

    rc = afsclient_Init(&st);
    if (!rc) {
	fprintf(stderr, "afsclient_Init, status %d\n", st);
	exit(1);
    }

    rc = afsclient_TokenGetExisting(cellName, &tokenHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_TokenGetExisting, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CellOpen(cellName, tokenHandle, &cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellOpen, status %d\n", st);
	exit(1);
    }

    rc = afsclient_RPCStatOpenPort(cellHandle, srvrName, srvrPort, &conn,
				   &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatOpenPort, status %d\n", st);
	exit(1);
    }

    rc = util_RPCStatsClear(conn, RXSTATS_ClearPeerRPCStats,
			    AFS_RX_STATS_CLEAR_ALL, &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsClear, status %d\n", st);
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

    rc = afsclient_TokenClose(tokenHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_TokenClose, status %d\n", st);
	exit(1);
    }

    exit(0);
}
