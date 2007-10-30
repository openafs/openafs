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
#include <afs/afs_AdminErrors.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

#ifdef AFS_DARWIN_ENV
pthread_mutex_t des_init_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t des_random_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rxkad_random_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* AFS_DARWIN_ENV */

#include <rx/rxstat.h>
#include <afs/afsint.h>
#define FSINT_COMMON_XG
#include <afs/afscbint.h>
#include <afs/kauth.h>
#include <afs/kautils.h>
#include <afs/ptint.h>
#include <afs/ptserver.h>
#include <afs/vldbint.h>
#include <afs/bosint.h>
#include <afs/volint.h>
#include <afs/volser.h>
#include <afs/bosint.h>
#include <ubik.h>
#include <ubik_int.h>
#ifndef AFS_NT40_ENV
#include <arpa/inet.h>		/* for inet_ntoa() */
#endif

extern int RXSTATS_RetrievePeerRPCStats();

void
Usage()
{
    fprintf(stderr, "Usage: rxstat_get_peer <cell> <host> <port>\n");
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

void
GetPrintStrings(afs_RPCStats_p statp, char *ifName, char *ifRole,
		char *hostName, const char ***funcList, int *funcListLen)
{
    struct in_addr ina;

    ina.s_addr = htonl(statp->s.stats_v1.remote_peer);
    strcpy(hostName, inet_ntoa(ina));

    if (statp->s.stats_v1.remote_is_server) {
	strcpy(ifRole, "client");
    } else {
	strcpy(ifRole, "server");
    }

    switch (statp->s.stats_v1.interfaceId) {
    case RXSTATS_STATINDEX:
	strcpy(ifName, "rxstats interface");
	*funcList = RXSTATS_function_names;
	*funcListLen = RXSTATS_NO_OF_STAT_FUNCS;
	break;
    case RXAFS_STATINDEX:
	strcpy(ifName, "fileserver interface");
	*funcList = RXAFS_function_names;
	*funcListLen = RXAFS_NO_OF_STAT_FUNCS;
	break;
    case RXAFSCB_STATINDEX:
	strcpy(ifName, "callback interface");
	*funcList = RXAFSCB_function_names;
	*funcListLen = RXAFSCB_NO_OF_STAT_FUNCS;
	break;
    case PR_STATINDEX:
	strcpy(ifName, "ptserver interface");
	*funcList = PR_function_names;
	*funcListLen = PR_NO_OF_STAT_FUNCS;
	break;
    case DISK_STATINDEX:
	strcpy(ifName, "ubik disk interface");
	*funcList = DISK_function_names;
	*funcListLen = DISK_NO_OF_STAT_FUNCS;
	break;
    case VOTE_STATINDEX:
	strcpy(ifName, "ubik vote interface");
	*funcList = VOTE_function_names;
	*funcListLen = VOTE_NO_OF_STAT_FUNCS;
	break;
    case VL_STATINDEX:
	strcpy(ifName, "volserver interface");
	*funcList = VL_function_names;
	*funcListLen = VL_NO_OF_STAT_FUNCS;
	break;
    case AFSVolSTATINDEX:
	strcpy(ifName, "volserver interface");
	*funcList = AFSVolfunction_names;
	*funcListLen = AFSVolNO_OF_STAT_FUNCS;
	break;
    case BOZO_STATINDEX:
	strcpy(ifName, "bosserver interface");
	*funcList = BOZO_function_names;
	*funcListLen = BOZO_NO_OF_STAT_FUNCS;
	break;
    case KAA_STATINDEX:
	strcpy(ifName, "KAA interface");
	*funcList = KAA_function_names;
	*funcListLen = KAA_NO_OF_STAT_FUNCS;
	break;
    case KAM_STATINDEX:
	strcpy(ifName, "KAM interface");
	*funcList = KAM_function_names;
	*funcListLen = KAM_NO_OF_STAT_FUNCS;
	break;
    case KAT_STATINDEX:
	strcpy(ifName, "KAT interface");
	*funcList = KAT_function_names;
	*funcListLen = KAT_NO_OF_STAT_FUNCS;
	break;
    default:
	sprintf(ifName, "interface 0x%x", statp->s.stats_v1.interfaceId);
	*funcList = NULL;
	*funcListLen = 0;
	break;
    }
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
    void *iterator;
    afs_RPCStats_t stats;
    char ifName[128];
    char role[8];
    char hostName[128];
    const char **funcList;
    int funcListLen;
    int index;

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

    rc = util_RPCStatsGetBegin(conn, RXSTATS_RetrievePeerRPCStats, &iterator,
			       &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsGetBegin, status %d\n", st);
	exit(1);
    }

    while (util_RPCStatsGetNext(iterator, &stats, &st)) {
	index = stats.s.stats_v1.func_index;

	if (index == 0) {
	    GetPrintStrings(&stats, ifName, role, hostName, &funcList,
			    &funcListLen);
	    printf("\nPeer stats for %s accessed as a %s\n"
		   "    host %s, port %d\n\n", ifName, role, hostName,
		   stats.s.stats_v1.remote_port);
	}

	if (index >= funcListLen) {
	    printf("    Function index %d\n", index);
	} else {
	    printf("    %s\n", funcList[index]);
	}

	if (!hiszero(stats.s.stats_v1.invocations)) {
	    printf("\tinvoc (%u.%u) bytes_sent (%u.%u) bytes_rcvd (%u.%u)\n",
		   hgethi(stats.s.stats_v1.invocations),
		   hgetlo(stats.s.stats_v1.invocations),
		   hgethi(stats.s.stats_v1.bytes_sent),
		   hgetlo(stats.s.stats_v1.bytes_sent),
		   hgethi(stats.s.stats_v1.bytes_rcvd),
		   hgetlo(stats.s.stats_v1.bytes_rcvd));
	    printf("\tqsum %d.%06d qsqr %d.%06d"
		   " qmin %d.%06d qmax %d.%06d\n",
		   stats.s.stats_v1.queue_time_sum.sec,
		   stats.s.stats_v1.queue_time_sum.usec,
		   stats.s.stats_v1.queue_time_sum_sqr.sec,
		   stats.s.stats_v1.queue_time_sum_sqr.usec,
		   stats.s.stats_v1.queue_time_min.sec,
		   stats.s.stats_v1.queue_time_min.usec,
		   stats.s.stats_v1.queue_time_max.sec,
		   stats.s.stats_v1.queue_time_max.usec);
	    printf("\txsum %d.%06d xsqr %d.%06d"
		   " xmin %d.%06d xmax %d.%06d\n",
		   stats.s.stats_v1.execution_time_sum.sec,
		   stats.s.stats_v1.execution_time_sum.usec,
		   stats.s.stats_v1.execution_time_sum_sqr.sec,
		   stats.s.stats_v1.execution_time_sum_sqr.usec,
		   stats.s.stats_v1.execution_time_min.sec,
		   stats.s.stats_v1.execution_time_min.usec,
		   stats.s.stats_v1.execution_time_max.sec,
		   stats.s.stats_v1.execution_time_max.usec);
	} else {
	    printf("\tNever invoked\n");
	}
    }
    if (st != ADMITERATORDONE) {
	fprintf(stderr, "util_RPCStatsGetNext, status %d\n", st);
	exit(1);
    }
    printf("\n");

    rc = util_RPCStatsGetDone(iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsGetDone, status %d\n", st);
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

    exit(0);
}
