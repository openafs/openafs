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

#include <roken.h>

#ifdef AFS_NT40_ENV
#include <pthread.h>
#endif

#include <rx/rx.h>
#include <rx/rxstat.h>
#include <afs/cmd.h>
#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

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

enum optionsList {
    OPT_server,
    OPT_port,
};

void
GetPrintStrings(afs_RPCStats_p statp, char *ifName, char *ifRole,
		const char ***funcList, int *funcListLen)
{
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
	strcpy(ifName, "vlserver interface");
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

static int
rxstat_get_process(struct cmd_syndesc *as, void *arock)
{
    int rc;
    afs_status_t st = 0;
    struct rx_connection *conn;
    char *srvrName = NULL;
    int srvrPort = 0;
    void *cellHandle;
    void *iterator;
    afs_RPCStats_t stats;
    char ifName[128];
    char role[8];
    const char **funcList;
    int funcListLen;
    int index;

    cmd_OptionAsString(as, OPT_server, &srvrName);
    cmd_OptionAsInt(as, OPT_port, &srvrPort);

    if (srvrPort <= 0 || srvrPort > USHRT_MAX) {
	fprintf(stderr, "Out of range -port value.\n");
	return 1;
    }

    rc = afsclient_Init(&st);
    if (!rc) {
	fprintf(stderr, "afsclient_Init, status %d\n", st);
	return 1;
    }

    rc = afsclient_NullCellOpen(&cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_NullCellOpen, status %d\n", st);
	return 1;
    }

    rc = afsclient_RPCStatOpenPort(cellHandle, srvrName, srvrPort, &conn,
				   &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatOpenPort, status %d\n", st);
	return 1;
    }

    rc = util_RPCStatsGetBegin(conn, RXSTATS_RetrieveProcessRPCStats,
			       &iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsGetBegin, status %d\n", st);
	return 1;
    }

    while (util_RPCStatsGetNext(iterator, &stats, &st)) {
	index = stats.s.stats_v1.func_index;

	if (index == 0) {
	    GetPrintStrings(&stats, ifName, role, &funcList, &funcListLen);
	    printf("\nProcess RPC stats for %s accessed as a %s\n\n", ifName,
		   role);
	}

	if (index >= funcListLen) {
	    printf("    Function index %d\n", index);
	} else {
	    printf("    %s\n", funcList[index]);
	}

	if (stats.s.stats_v1.invocations != 0) {
	    printf("\tinvoc %"AFS_UINT64_FMT
		   " bytes_sent %"AFS_UINT64_FMT
		   " bytes_rcvd %"AFS_UINT64_FMT"\n",
		   stats.s.stats_v1.invocations,
		   stats.s.stats_v1.bytes_sent,
		   stats.s.stats_v1.bytes_rcvd);
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
	return 1;
    }
    printf("\n");

    rc = util_RPCStatsGetDone(iterator, &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsGetDone, status %d\n", st);
	return 1;
    }

    rc = afsclient_RPCStatClose(conn, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_RPCStatClose, status %d\n", st);
	return 1;
    }

    rc = afsclient_CellClose(cellHandle, &st);
    if (!rc) {
	fprintf(stderr, "afsclient_CellClose, status %d\n", st);
	return 1;
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, rxstat_get_process, NULL, 0, "rxstat get process");
    cmd_AddParmAtOffset(ts, OPT_server, "-server", CMD_SINGLE, CMD_REQUIRED, "server");
    cmd_AddParmAtOffset(ts, OPT_port, "-port", CMD_SINGLE, CMD_REQUIRED, "port");

    return cmd_Dispatch(argc, argv);
}
