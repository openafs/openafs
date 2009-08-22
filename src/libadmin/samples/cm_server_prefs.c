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
 * This file contains sample code for the client admin interface 
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
    fprintf(stderr, "Usage: cm_server_prefs <host> <port>\n");
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
    afs_CMServerPref_t prefs;
    void *iterator;
    afs_uint32 taddr;

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

    rc = afsclient_CMStatOpenPort(cellHandle, srvrName, srvrPort, &conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CMStatOpenPort, status %d\n", st);
	exit(1);
    }

    rc = util_CMGetServerPrefsBegin(conn, &iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_CMGetServerPrefsBegin, status %d\n", st);
	exit(1);
    }

    printf("\n");
    while (util_CMGetServerPrefsNext(iterator, &prefs, &st)) {
	taddr = prefs.ipAddr;
	printf("%d.%d.%d.%d\t\t\t%d\n", (taddr >> 24) & 0xff,
	       (taddr >> 16) & 0xff, (taddr >> 8) & 0xff, taddr & 0xff,
	       prefs.ipRank);
    }
    if (st != ADMITERATORDONE) {
	fprintf(stderr, "util_CMGetServerPrefsNext, status %d\n", st);
	exit(1);
    }
    printf("\n");

    rc = util_CMGetServerPrefsDone(iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_CMGetServerPrefsDone, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CMStatClose(conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CMStatClose, status %d\n", st);
	exit(1);
    }

    rc = afsclient_CellClose(cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellClose, status %d\n", st);
	exit(1);
    }

    exit(0);
}
