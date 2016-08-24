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
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

enum optionsList {
    OPT_server,
    OPT_port,
};

static int
rxstat_query_peer(struct cmd_syndesc *as, void *arock)
{
    int rc;
    afs_status_t st = 0;
    struct rx_connection *conn;
    char *srvrName = NULL;
    int srvrPort = 0;
    void *cellHandle;
    afs_RPCStatsState_t state;

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

    rc = util_RPCStatsStateGet(conn, RXSTATS_QueryPeerRPCStats, &state, &st);
    if (!rc) {
	fprintf(stderr, "util_RPCStatsStateGet, status %d\n", st);
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

    printf("\n");
    printf("Peer RPC stats are ");
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

    return 0;
}

int
main(int argc, char *argv[])
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax(NULL, rxstat_query_peer, NULL, 0, "rxstat query peer");
    cmd_AddParmAtOffset(ts, OPT_server, "-server", CMD_SINGLE, CMD_REQUIRED, "server");
    cmd_AddParmAtOffset(ts, OPT_port, "-port", CMD_SINGLE, CMD_REQUIRED, "port");

    return cmd_Dispatch(argc, argv);
}
