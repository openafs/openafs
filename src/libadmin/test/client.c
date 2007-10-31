/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the client related funtions for afscp
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "client.h"
#include <afs/cellconfig.h>
#include <afs/bosint.h>
#include <rx/rxstat.h>
#include <afs/afsint.h>
#define FSINT_COMMON_XG
#include <afs/afscbint.h>
#include <afs/kauth.h>
#include <afs/kautils.h>
#include <afs/ptint.h>
#include <afs/ptserver.h>
#include <afs/vldbint.h>
#include <afs/volint.h>
#include <afs/volser.h>
#include <ubik.h>
#include <ubik_int.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <pthread.h>
#endif

/* These aren't coming from a header, currently, so they must stay here. 
   Fix elsewhere, or leave alone. */
extern int RXSTATS_RetrieveProcessRPCStats();
extern int RXSTATS_RetrievePeerRPCStats();
extern int RXSTATS_QueryProcessRPCStats();
extern int RXSTATS_QueryPeerRPCStats();
extern int RXSTATS_EnableProcessRPCStats();
extern int RXSTATS_EnablePeerRPCStats();
extern int RXSTATS_DisableProcessRPCStats();
extern int RXSTATS_DisablePeerRPCStats();
extern int RXSTATS_ClearProcessRPCStats();
extern int RXSTATS_ClearPeerRPCStats();

/*
 * This structure stores the client and server function lists.
 * This is kept separate from the actual interface definitions
 * since an rpc interface can be offered by several servers
 * (e.g. ubik and rxstat)
 *
 * The purpose of these functions is to allow a mapping from interfaceId
 * to text representations of server process names and function names.
 */

typedef struct {
    const char **functionList;
    size_t functionListLen;
} interface_function_list_t, *interface_function_list_p;

#ifdef AFS_NT40_ENV

/*
 * On NT, you cannot define an array of character pointers in a dll
 * and then access this array outside the dll via a global initialization
 * because the msvc compiler will complain that the initializer is not
 * a constant (i.e. C2099: initializer is not a constant).  This is because
 * the dllimport and dllexport c language extensions cause references
 * to the character array to go through another level of indirection -
 * and this indirection is unknown at compilation time.
 *
 * To get around this problem I hand initialize this array on NT only
 */

static interface_function_list_t afs_server;
static interface_function_list_t afscb_server;
static interface_function_list_t bos_server;
static interface_function_list_t kauth_kaa_server;
static interface_function_list_t kauth_kam_server;
static interface_function_list_t kauth_kat_server;
static interface_function_list_t pts_server;
static interface_function_list_t rxstat_server;
static interface_function_list_t ubik_disk_server;
static interface_function_list_t ubik_vote_server;
static interface_function_list_t vl_server;
static interface_function_list_t vol_server;
static pthread_once_t pthread_func_list_once = PTHREAD_ONCE_INIT;
static int pthread_func_list_done;

static void
cr_list(void)
{
    afs_server.functionList = RXAFS_function_names;
    afs_server.functionListLen = RXAFS_NO_OF_STAT_FUNCS;
    afscb_server.functionList = RXAFSCB_function_names;
    afscb_server.functionListLen = RXAFSCB_NO_OF_STAT_FUNCS;
    bos_server.functionList = BOZO_function_names;
    bos_server.functionListLen = BOZO_NO_OF_STAT_FUNCS;
    kauth_kaa_server.functionList = KAA_function_names;
    kauth_kaa_server.functionListLen = KAA_NO_OF_STAT_FUNCS;
    kauth_kam_server.functionList = KAM_function_names;
    kauth_kam_server.functionListLen = KAM_NO_OF_STAT_FUNCS;
    kauth_kat_server.functionList = KAT_function_names;
    kauth_kat_server.functionListLen = KAT_NO_OF_STAT_FUNCS;
    pts_server.functionList = PR_function_names;
    pts_server.functionListLen = PR_NO_OF_STAT_FUNCS;
    rxstat_server.functionList = RXSTATS_function_names;
    rxstat_server.functionListLen = RXSTATS_NO_OF_STAT_FUNCS;
    ubik_disk_server.functionList = DISK_function_names;
    ubik_disk_server.functionListLen = DISK_NO_OF_STAT_FUNCS;
    ubik_vote_server.functionList = VOTE_function_names;
    ubik_vote_server.functionListLen = VOTE_NO_OF_STAT_FUNCS;
    vl_server.functionList = VL_function_names;
    vl_server.functionListLen = VL_NO_OF_STAT_FUNCS;
    vol_server.functionList = AFSVolfunction_names;
    vol_server.functionListLen = AFSVolNO_OF_STAT_FUNCS;
    pthread_func_list_done = 1;
}

#else

static interface_function_list_t afs_server = {
    RXAFS_function_names,
    RXAFS_NO_OF_STAT_FUNCS
};

static interface_function_list_t afscb_server = {
    RXAFSCB_function_names,
    RXAFSCB_NO_OF_STAT_FUNCS
};

static interface_function_list_t bos_server = {
    BOZO_function_names,
    BOZO_NO_OF_STAT_FUNCS
};

static interface_function_list_t kauth_kaa_server = {
    KAA_function_names,
    KAA_NO_OF_STAT_FUNCS
};

static interface_function_list_t kauth_kam_server = {
    KAM_function_names,
    KAM_NO_OF_STAT_FUNCS
};

static interface_function_list_t kauth_kat_server = {
    KAT_function_names,
    KAT_NO_OF_STAT_FUNCS
};

static interface_function_list_t pts_server = {
    PR_function_names,
    PR_NO_OF_STAT_FUNCS
};

static interface_function_list_t rxstat_server = {
    RXSTATS_function_names,
    RXSTATS_NO_OF_STAT_FUNCS
};

static interface_function_list_t ubik_disk_server = {
    DISK_function_names,
    DISK_NO_OF_STAT_FUNCS,
};

static interface_function_list_t ubik_vote_server = {
    VOTE_function_names,
    VOTE_NO_OF_STAT_FUNCS,
};

static interface_function_list_t vl_server = {
    VL_function_names,
    VL_NO_OF_STAT_FUNCS
};

static interface_function_list_t vol_server = {
    AFSVolfunction_names,
    AFSVolNO_OF_STAT_FUNCS
};

#endif /* AFS_NT40_ENV */

static interface_function_list_t unknown_server = {
    0,
    0
};

typedef struct {
    afs_uint32 interfaceId;
    const char *interfaceName;
    interface_function_list_p functionList;
} interface_t, *interface_p;

interface_t int_list[] = {
    {RXAFS_STATINDEX,
     "file server",
     &afs_server},

    {RXSTATS_STATINDEX,
     "rx stats",
     &rxstat_server},

    {RXAFSCB_STATINDEX,
     "cache manager",
     &afscb_server},

    {PR_STATINDEX,
     "pts server",
     &pts_server},

    {DISK_STATINDEX,
     "ubik disk server",
     &ubik_disk_server},

    {VOTE_STATINDEX,
     "ubik vote server",
     &ubik_vote_server},

    {VL_STATINDEX,
     "vldb server",
     &vl_server},

    {AFSVolSTATINDEX,
     "vol server",
     &vol_server},

    {BOZO_STATINDEX,
     "bos server",
     &bos_server},

    {KAA_STATINDEX,
     "kas kaa server",
     &kauth_kaa_server},

    {KAM_STATINDEX,
     "kas kam server",
     &kauth_kam_server},

    {KAT_STATINDEX,
     "kas kat server",
     &kauth_kat_server},

    /*
     * Note the code below assumes that the following entry is the last entry
     * in this array
     */

    {0, "unknown", &unknown_server}
};

/*
 * Utility functions
 */

int
DoClientLocalCellGet(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    char cellName[MAXCELLCHARS];

    if (!afsclient_LocalCellGet(cellName, &st)) {
	ERR_ST_EXT("afsclient_LocalCellGet", st);
    }

    printf("This machine belongs to cell: %s\n", cellName);

    return 0;
}

int
DoClientMountPointCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { DIRECTORY, VOLUME, READWRITE,
	CHECK
    } DoClientMountPointCreate_parm_t;
    afs_status_t st = 0;
    const char *directory;
    const char *volume;
    vol_type_t vol_type = READ_ONLY;
    vol_check_t vol_check = DONT_CHECK_VOLUME;

    if (as->parms[DIRECTORY].items) {
	directory = as->parms[DIRECTORY].items->data;
    }

    if (as->parms[VOLUME].items) {
	volume = as->parms[VOLUME].items->data;
    }

    if (as->parms[READWRITE].items) {
	vol_type = READ_WRITE;
    }

    if (as->parms[CHECK].items) {
	vol_check = CHECK_VOLUME;
    }

    if (!afsclient_MountPointCreate
	(cellHandle, directory, volume, vol_type, vol_check, &st)) {
	ERR_ST_EXT("afsclient_MountPointCreate", st);
    }

    return 0;
}

static void
Print_afs_serverEntry_p(afs_serverEntry_p serv, const char *prefix)
{
    int i = 0;

    printf("%sInformation for server %s\n", prefix, serv->serverName);
    if (serv->serverType & DATABASE_SERVER) {
	printf("%s\tIt is a database server\n", prefix);
    }
    if (serv->serverType & FILE_SERVER) {
	printf("%s\tIt is a file server\n", prefix);
    }
    printf("%s\tServer addresses:%s\n", prefix, serv->serverName);
    while (serv->serverAddress[i] != 0) {
	printf("\t\t%s%x\n", prefix, serv->serverAddress[i++]);
    }
}

int
DoClientAFSServerGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoClientAFSServerGet_parm_t;
    afs_status_t st = 0;
    const char *server;
    afs_serverEntry_t entry;

    if (as->parms[SERVER].items) {
	server = as->parms[SERVER].items->data;
    }

    if (!afsclient_AFSServerGet(cellHandle, server, &entry, &st)) {
	ERR_ST_EXT("afsclient_AFSServerGet", st);
    }

    Print_afs_serverEntry_p(&entry, "");

    return 0;
}

int
DoClientAFSServerList(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    afs_serverEntry_t entry;
    void *iter = NULL;

    if (!afsclient_AFSServerGetBegin(cellHandle, &iter, &st)) {
	ERR_ST_EXT("afsclient_AFSServerGetBegin", st);
    }

    while (afsclient_AFSServerGetNext(iter, &entry, &st)) {
	Print_afs_serverEntry_p(&entry, "");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("afsclient_AFSServerGetNext", st);
    }

    if (!afsclient_AFSServerGetDone(iter, &st)) {
	ERR_ST_EXT("afsclient_AFSServerGetDone", st);
    }


    return 0;
}

static void
Print_afs_RPCStatsState_p(afs_RPCStatsState_p state, const char *prefix)
{
    printf("%sThe rpc stats state is: ", prefix);
    switch (*state) {
    case AFS_RPC_STATS_DISABLED:
	printf("disabled\n");
	break;
    case AFS_RPC_STATS_ENABLED:
	printf("enabled\n");
	break;
    }
}

typedef struct {
    const char *tag;
    afs_stat_source_t value;
} afs_type_map_t, *afs_type_map_p;

static afs_type_map_t map[] = {
    {"bosserver", AFS_BOSSERVER},
    {"fileserver", AFS_FILESERVER},
    {"kaserver", AFS_KASERVER},
    {"ptserver", AFS_PTSERVER},
    {"volserver", AFS_VOLSERVER},
    {"vlserver", AFS_VLSERVER},
    {"client", AFS_CLIENT},
    {0, 0}
};

static int
GetStatPortFromString(const char *type, int *port)
{
    char *end;
    long tport;

    errno = 0;
    tport = strtol(type, &end, 0);
    if (tport == 0 || end == type || *end != '\0') {
	return 0;
    }

    *port = (int)tport;
    return 1;
}

static int
GetStatSourceFromString(const char *type, afs_stat_source_t * src, int *port)
{
    int i;
    size_t type_len = strlen(type);

    for (i = 0; (map[i].tag) && strncasecmp(type, map[i].tag, type_len); i++);

    if (map[i].tag == 0) {
	/*
	 * Try to convert string to port number
	 */
	if (GetStatPortFromString(type, port)) {
	    return 0;
	}

	fprintf(stderr,
		"couldn't convert server to type, try one of the "
		"following:\n");
	for (i = 0; map[i].tag; i++) {
	    fprintf(stderr, "%s ", map[i].tag);
	}

	ERR_EXT("");
    } else {
	*src = map[i].value;
	return 1;
    }
}

typedef enum {
    AFS_PEER_STATS,
    AFS_PROCESS_STATS
} afs_stat_type_t, *afs_stat_type_p;

static afs_stat_type_t
GetStatTypeFromString(const char *type)
{
    afs_stat_type_t rc;

    if (!strcmp(type, "peer")) {
	rc = AFS_PEER_STATS;
    } else if (!strcmp(type, "process")) {
	rc = AFS_PROCESS_STATS;
    } else {
	ERR_EXT("stat_type must be process or peer");
    }

    return rc;
}

int
DoClientRPCStatsStateGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS,
	STAT_TYPE
    } DoClientRPCStatsStateGet_parm_t;
    afs_status_t st = 0;
    struct rx_connection *conn;
    int servAddr = 0;
    afs_stat_source_t type;
    int srvrPort;
    int typeIsValid;
    afs_stat_type_t which;
    afs_RPCStatsState_t state;

    if (as->parms[PROCESS].items) {
	typeIsValid =
	    GetStatSourceFromString(as->parms[PROCESS].items->data, &type,
				    &srvrPort);
    }

    if (as->parms[STAT_TYPE].items) {
	which = GetStatTypeFromString(as->parms[STAT_TYPE].items->data);
    }

    if (as->parms[SERVER].items) {
	if (typeIsValid) {
	    if (!afsclient_RPCStatOpen
		(cellHandle, as->parms[SERVER].items->data, type, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpen", st);
	    }
	} else {
	    if (!afsclient_RPCStatOpenPort
		(cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpenPort", st);
	    }
	}
    }

    if (which == AFS_PEER_STATS) {
	if (!util_RPCStatsStateGet
	    (conn, RXSTATS_QueryPeerRPCStats, &state, &st)) {
	    ERR_ST_EXT("util_RPCStatsStateGet", st);
	}
    } else {
	if (!util_RPCStatsStateGet
	    (conn, RXSTATS_QueryProcessRPCStats, &state, &st)) {
	    ERR_ST_EXT("util_RPCStatsStateGet", st);
	}
    }

    Print_afs_RPCStatsState_p(&state, "");

    afsclient_RPCStatClose(conn, 0);

    return 0;
}

int
DoClientRPCStatsStateEnable(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, STAT_TYPE } DoClientRPCStatsEnable_parm_t;
    afs_status_t st = 0;
    struct rx_connection *conn;
    int servAddr = 0;
    afs_stat_source_t type;
    int srvrPort;
    int typeIsValid;
    afs_stat_type_t which;

    if (as->parms[PROCESS].items) {
	typeIsValid =
	    GetStatSourceFromString(as->parms[PROCESS].items->data, &type,
				    &srvrPort);
    }

    if (as->parms[STAT_TYPE].items) {
	which = GetStatTypeFromString(as->parms[STAT_TYPE].items->data);
    }

    if (as->parms[SERVER].items) {
	if (typeIsValid) {
	    if (!afsclient_RPCStatOpen
		(cellHandle, as->parms[SERVER].items->data, type, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpen", st);
	    }
	} else {
	    if (!afsclient_RPCStatOpenPort
		(cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpenPort", st);
	    }
	}
    }

    if (which == AFS_PEER_STATS) {
	if (!util_RPCStatsStateEnable(conn, RXSTATS_EnablePeerRPCStats, &st)) {
	    ERR_ST_EXT("util_RPCStatsStateEnable", st);
	}
    } else {
	if (!util_RPCStatsStateEnable
	    (conn, RXSTATS_EnableProcessRPCStats, &st)) {
	    ERR_ST_EXT("util_RPCStatsStateEnable", st);
	}
    }

    afsclient_RPCStatClose(conn, 0);

    return 0;
}

int
DoClientRPCStatsStateDisable(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS,
	STAT_TYPE
    } DoClientRPCStatsDisable_parm_t;
    afs_status_t st = 0;
    struct rx_connection *conn;
    int servAddr = 0;
    afs_stat_source_t type;
    int srvrPort;
    int typeIsValid;
    afs_stat_type_t which;

    if (as->parms[PROCESS].items) {
	typeIsValid =
	    GetStatSourceFromString(as->parms[PROCESS].items->data, &type,
				    &srvrPort);
    }

    if (as->parms[STAT_TYPE].items) {
	which = GetStatTypeFromString(as->parms[STAT_TYPE].items->data);
    }

    if (as->parms[SERVER].items) {
	if (typeIsValid) {
	    if (!afsclient_RPCStatOpen
		(cellHandle, as->parms[SERVER].items->data, type, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpen", st);
	    }
	} else {
	    if (!afsclient_RPCStatOpenPort
		(cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpenPort", st);
	    }
	}
    }

    if (which == AFS_PEER_STATS) {
	if (!util_RPCStatsStateDisable
	    (conn, RXSTATS_DisablePeerRPCStats, &st)) {
	    ERR_ST_EXT("util_RPCStatsStateDisable", st);
	}
    } else {
	if (!util_RPCStatsStateDisable
	    (conn, RXSTATS_DisableProcessRPCStats, &st)) {
	    ERR_ST_EXT("util_RPCStatsStateDisable", st);
	}
    }

    afsclient_RPCStatClose(conn, 0);

    return 0;
}

static void
Print_afs_RPCStats_p(afs_RPCStats_p stat, interface_function_list_p f_list,
		     const char *prefix)
{
    afs_int32 index = stat->s.stats_v1.func_index;

    if (index > ((afs_int32) f_list->functionListLen - 1)) {
	printf("%sUnknown function ", prefix);
    } else {
	printf("%s%s ", prefix,
	       f_list->functionList[stat->s.stats_v1.func_index]);
    }

    if (!hiszero(stat->s.stats_v1.invocations)) {
	printf("%sinvoc (%u.%u) bytes_sent (%u.%u) bytes_rcvd (%u.%u)\n",
	       prefix, hgethi(stat->s.stats_v1.invocations),
	       hgetlo(stat->s.stats_v1.invocations),
	       hgethi(stat->s.stats_v1.bytes_sent),
	       hgetlo(stat->s.stats_v1.bytes_sent),
	       hgethi(stat->s.stats_v1.bytes_rcvd),
	       hgetlo(stat->s.stats_v1.bytes_rcvd)
	    );
	printf("\tqsum %d.%06d\tqsqr %d.%06d"
	       "\tqmin %d.%06d\tqmax %d.%06d\n",
	       stat->s.stats_v1.queue_time_sum.sec,
	       stat->s.stats_v1.queue_time_sum.usec,
	       stat->s.stats_v1.queue_time_sum_sqr.sec,
	       stat->s.stats_v1.queue_time_sum_sqr.usec,
	       stat->s.stats_v1.queue_time_min.sec,
	       stat->s.stats_v1.queue_time_min.usec,
	       stat->s.stats_v1.queue_time_max.sec,
	       stat->s.stats_v1.queue_time_max.usec);
	printf("\txsum %d.%06d\txsqr %d.%06d"
	       "\txmin %d.%06d\txmax %d.%06d\n",
	       stat->s.stats_v1.execution_time_sum.sec,
	       stat->s.stats_v1.execution_time_sum.usec,
	       stat->s.stats_v1.execution_time_sum_sqr.sec,
	       stat->s.stats_v1.execution_time_sum_sqr.usec,
	       stat->s.stats_v1.execution_time_min.sec,
	       stat->s.stats_v1.execution_time_min.usec,
	       stat->s.stats_v1.execution_time_max.sec,
	       stat->s.stats_v1.execution_time_max.usec);
    } else {
	printf("never invoked\n");
    }
}

int
DoClientRPCStatsList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, STAT_TYPE } DoClientRPCStatsList_parm_t;
    afs_status_t st = 0;
    struct rx_connection *conn;
    int servAddr = 0;
    afs_stat_source_t type;
    int srvrPort;
    int typeIsValid;
    afs_stat_type_t which;
    afs_RPCStats_t stats;
    void *iter;
    int i;

#ifdef AFS_NT40_ENV
    (pthread_func_list_done
     || pthread_once(&pthread_func_list_once, cr_list));
#endif

    if (as->parms[PROCESS].items) {
	typeIsValid =
	    GetStatSourceFromString(as->parms[PROCESS].items->data, &type,
				    &srvrPort);
    }

    if (as->parms[STAT_TYPE].items) {
	which = GetStatTypeFromString(as->parms[STAT_TYPE].items->data);
    }

    if (as->parms[SERVER].items) {
	if (typeIsValid) {
	    if (!afsclient_RPCStatOpen
		(cellHandle, as->parms[SERVER].items->data, type, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpen", st);
	    }
	} else {
	    if (!afsclient_RPCStatOpenPort
		(cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpenPort", st);
	    }
	}
    }

    if (which == AFS_PEER_STATS) {
	if (!util_RPCStatsGetBegin
	    (conn, RXSTATS_RetrievePeerRPCStats, &iter, &st)) {
	    ERR_ST_EXT("util_RPCStatsGetBegin", st);
	}
    } else {
	if (!util_RPCStatsGetBegin
	    (conn, RXSTATS_RetrieveProcessRPCStats, &iter, &st)) {
	    ERR_ST_EXT("util_RPCStatsGetBegin", st);
	}
    }

    printf("Listing rpc stats at server %s process %s:\n",
	   as->parms[SERVER].items->data, as->parms[PROCESS].items->data);

    while (util_RPCStatsGetNext(iter, &stats, &st)) {

	/*
	 * Print a new heading for each stat collection
	 */

	if (stats.s.stats_v1.func_index == 0) {

	    printf("\n\n");

	    /*
	     * Look up the interface in our list
	     */

	    for (i = 0; i < ((sizeof(int_list) - 1) / sizeof(interface_t));
		 i++) {
		if (stats.s.stats_v1.interfaceId == int_list[i].interfaceId) {
		    break;
		}
	    }

	    /*
	     * Print out a meaningful header for each stat collection
	     */

	    if (which == AFS_PEER_STATS) {
		struct in_addr ina;
		ina.s_addr = htonl(stats.s.stats_v1.remote_peer);

		printf("%s stats for remote peer located at %s port %u "
		       "%s %s as a %s via the %s interface\n",
		       as->parms[PROCESS].items->data, inet_ntoa(ina),
		       stats.s.stats_v1.remote_port,
		       ((stats.s.stats_v1.
			 remote_is_server) ? "accessed by" : "accessing"),
		       as->parms[PROCESS].items->data,
		       ((stats.s.stats_v1.
			 remote_is_server) ? "client" : "server"),
		       int_list[i].interfaceName);
	    } else {
		printf("%s stats for the %s interface " "accessed as a %s\n",
		       as->parms[PROCESS].items->data,
		       int_list[i].interfaceName,
		       ((stats.s.stats_v1.
			 remote_is_server) ? "client" : "server")
		    );
	    }
	}
	Print_afs_RPCStats_p(&stats, int_list[i].functionList, "    ");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("util_RPCStatsGetNext", st);
    }

    if (!util_RPCStatsGetDone(iter, &st)) {
	ERR_ST_EXT("util_RPCStatsGetDone", st);
    }

    afsclient_RPCStatClose(conn, 0);

    return 0;
}

int
DoClientRPCStatsClear(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, STAT_TYPE, CLEAR_ALL, CLEAR_INVOCATIONS,
	CLEAR_BYTES_SENT, CLEAR_BYTES_RCVD,
	CLEAR_QUEUE_TIME_SUM, CLEAR_QUEUE_TIME_SQUARE,
	CLEAR_QUEUE_TIME_MIN, CLEAR_QUEUE_TIME_MAX,
	CLEAR_EXEC_TIME_SUM, CLEAR_EXEC_TIME_SQUARE,
	CLEAR_EXEC_TIME_MIN, CLEAR_EXEC_TIME_MAX
    } DoClientRPCStatsClear_parm_t;
    afs_status_t st = 0;
    struct rx_connection *conn;
    int servAddr = 0;
    afs_stat_source_t type;
    int srvrPort;
    int typeIsValid;
    afs_stat_type_t which;
    afs_RPCStatsClearFlag_t flag = 0;
    int seen_all = 0;
    int seen_any = 0;

    if (as->parms[PROCESS].items) {
	typeIsValid =
	    GetStatSourceFromString(as->parms[PROCESS].items->data, &type,
				    &srvrPort);
    }

    if (as->parms[STAT_TYPE].items) {
	which = GetStatTypeFromString(as->parms[STAT_TYPE].items->data);
    }

    if (as->parms[SERVER].items) {
	if (typeIsValid) {
	    if (!afsclient_RPCStatOpen
		(cellHandle, as->parms[SERVER].items->data, type, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpen", st);
	    }
	} else {
	    if (!afsclient_RPCStatOpenPort
		(cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpenPort", st);
	    }
	}
    }

    if (as->parms[CLEAR_ALL].items) {
	seen_all = 1;
	seen_any = 1;
	flag = AFS_RX_STATS_CLEAR_ALL;
    }

    if (as->parms[CLEAR_INVOCATIONS].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_INVOCATIONS;
    }

    if (as->parms[CLEAR_BYTES_SENT].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_BYTES_SENT;
    }

    if (as->parms[CLEAR_BYTES_RCVD].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_BYTES_RCVD;
    }

    if (as->parms[CLEAR_QUEUE_TIME_SUM].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_QUEUE_TIME_SUM;
    }

    if (as->parms[CLEAR_QUEUE_TIME_SQUARE].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_QUEUE_TIME_SQUARE;
    }

    if (as->parms[CLEAR_QUEUE_TIME_MIN].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_QUEUE_TIME_MIN;
    }

    if (as->parms[CLEAR_QUEUE_TIME_MAX].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_QUEUE_TIME_MAX;
    }

    if (as->parms[CLEAR_EXEC_TIME_SUM].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_EXEC_TIME_SUM;
    }

    if (as->parms[CLEAR_EXEC_TIME_SQUARE].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_EXEC_TIME_SQUARE;
    }

    if (as->parms[CLEAR_EXEC_TIME_MIN].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_EXEC_TIME_MIN;
    }

    if (as->parms[CLEAR_EXEC_TIME_MAX].items) {
	if (seen_all) {
	    ERR_EXT("cannot specify additional flags when "
		    "specifying clear_all");
	}
	seen_any = 1;
	flag |= AFS_RX_STATS_CLEAR_EXEC_TIME_MAX;
    }

    if (!seen_any) {
	ERR_EXT("you must specify something to clear");
    }

    if (which == AFS_PEER_STATS) {
	if (!util_RPCStatsClear(conn, RXSTATS_ClearPeerRPCStats, flag, &st)) {
	    ERR_ST_EXT("util_RPCStatsClear", st);
	}
    } else {
	if (!util_RPCStatsClear
	    (conn, RXSTATS_ClearProcessRPCStats, flag, &st)) {
	    ERR_ST_EXT("util_RPCStatsClear", st);
	}
    }

    afsclient_RPCStatClose(conn, 0);

    return 0;
}

int
DoClientRPCStatsVersionGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoClientRPCStatsVersionGet_parm_t;
    afs_status_t st = 0;
    struct rx_connection *conn;
    afs_stat_source_t type;
    int servAddr = 0;
    int srvrPort;
    int typeIsValid;
    afs_RPCStatsVersion_t version;

    if (as->parms[PROCESS].items) {
	typeIsValid =
	    GetStatSourceFromString(as->parms[PROCESS].items->data, &type,
				    &srvrPort);
    }

    if (as->parms[SERVER].items) {
	if (typeIsValid) {
	    if (!afsclient_RPCStatOpen
		(cellHandle, as->parms[SERVER].items->data, type, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpen", st);
	    }
	} else {
	    if (!afsclient_RPCStatOpenPort
		(cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
		 &st)) {
		ERR_ST_EXT("afsclient_RPCStatOpenPort", st);
	    }
	}
    }

    if (!util_RPCStatsVersionGet(conn, &version, &st)) {
	ERR_ST_EXT("util_RPCStatsVersionGet", st);
    }

    printf("the rpc stat version number is %u\n", version);

    afsclient_RPCStatClose(conn, 0);

    return 0;
}

static void
Print_afs_CMServerPref_p(afs_CMServerPref_p pref)
{
    afs_uint32 taddr;

    taddr = pref->ipAddr;
    printf("%d.%d.%d.%d\t\t\t%d\n", (taddr >> 24) & 0xff,
	   (taddr >> 16) & 0xff, (taddr >> 8) & 0xff, taddr & 0xff,
	   pref->ipRank);
}

int
DoClientCMGetServerPrefs(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    typedef enum { SERVER, PORT } DoClientCMGetServerPrefs_parm_t;
    struct rx_connection *conn;
    int servAddr = 0;
    int srvrPort = AFSCONF_CALLBACKPORT;
    afs_CMServerPref_t prefs;
    void *iter;

#ifdef AFS_NT40_ENV
    (pthread_func_list_done
     || pthread_once(&pthread_func_list_once, cr_list));
#endif

    if (as->parms[PORT].items) {
	if (!GetStatPortFromString(as->parms[PORT].items->data, &srvrPort)) {
	    ERR_EXT("Couldn't undertand port number");
	}
    }

    if (as->parms[SERVER].items) {
	if (!afsclient_CMStatOpenPort
	    (cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
	     &st)) {
	    ERR_ST_EXT("afsclient_CMStatOpenPort", st);
	}
    }

    if (!util_CMGetServerPrefsBegin(conn, &iter, &st)) {
	ERR_ST_EXT("util_CMGetServerPrefsBegin", st);
    }

    printf("Listing CellServDB for %s at port %s:\n",
	   as->parms[SERVER].items->data, as->parms[PORT].items->data);

    while (util_CMGetServerPrefsNext(iter, &prefs, &st)) {

	Print_afs_CMServerPref_p(&prefs);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("util_CMGetServerPrefsNext", st);
    }

    if (!util_CMGetServerPrefsDone(iter, &st)) {
	ERR_ST_EXT("util_CMGetServerPrefsDone", st);
    }

    afsclient_CMStatClose(conn, 0);

    return 0;
}

static void
Print_afs_CMListCell_p(afs_CMListCell_p cellInfo)
{
    int i;
    afs_uint32 taddr;

    printf("Cell %s on hosts", cellInfo->cellname);
    for (i = 0; i < UTIL_MAX_CELL_HOSTS && cellInfo->serverAddr[i]; i++) {
	taddr = cellInfo->serverAddr[i];
	printf(" %d.%d.%d.%d", (taddr >> 24) & 0xff, (taddr >> 16) & 0xff,
	       (taddr >> 8) & 0xff, taddr & 0xff);
    }
    printf("\n");
}

int
DoClientCMListCells(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    typedef enum { SERVER, PORT } DoClientCMListCells_parm_t;
    struct rx_connection *conn;
    int servAddr = 0;
    int srvrPort = AFSCONF_CALLBACKPORT;
    afs_CMListCell_t cellInfo;
    void *iter;

#ifdef AFS_NT40_ENV
    (pthread_func_list_done
     || pthread_once(&pthread_func_list_once, cr_list));
#endif

    if (as->parms[PORT].items) {
	if (!GetStatPortFromString(as->parms[PORT].items->data, &srvrPort)) {
	    ERR_EXT("Couldn't undertand port number");
	}
    }

    if (as->parms[SERVER].items) {
	if (!afsclient_CMStatOpenPort
	    (cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
	     &st)) {
	    ERR_ST_EXT("afsclient_CMStatOpenPort", st);
	}
    }

    if (!util_CMListCellsBegin(conn, &iter, &st)) {
	ERR_ST_EXT("util_CMListCellsBegin", st);
    }

    printf("Listing CellServDB for %s at port %s:\n",
	   as->parms[SERVER].items->data, as->parms[PORT].items->data);

    while (util_CMListCellsNext(iter, &cellInfo, &st)) {

	Print_afs_CMListCell_p(&cellInfo);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("util_CMListCellsNext", st);
    }

    if (!util_CMListCellsDone(iter, &st)) {
	ERR_ST_EXT("util_CMListCellsDone", st);
    }

    afsclient_CMStatClose(conn, 0);

    return 0;
}

int
DoClientCMLocalCell(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    typedef enum { SERVER, PORT } DoClientCMLocalCell_parm_t;
    struct rx_connection *conn;
    int servAddr = 0;
    int srvrPort = AFSCONF_CALLBACKPORT;
    afs_CMCellName_t cellname;

#ifdef AFS_NT40_ENV
    (pthread_func_list_done
     || pthread_once(&pthread_func_list_once, cr_list));
#endif

    if (as->parms[PORT].items) {
	if (!GetStatPortFromString(as->parms[PORT].items->data, &srvrPort)) {
	    ERR_EXT("Couldn't undertand port number");
	}
    }

    if (as->parms[SERVER].items) {
	if (!afsclient_CMStatOpenPort
	    (cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
	     &st)) {
	    ERR_ST_EXT("afsclient_CMStatOpenPort", st);
	}
    }

    if (!util_CMLocalCell(conn, cellname, &st)) {
	ERR_ST_EXT("util_CMLocalCell", st);
    }

    printf("Client %s (port %s) is in cell %s\n",
	   as->parms[SERVER].items->data, as->parms[PORT].items->data,
	   cellname);

    afsclient_CMStatClose(conn, 0);

    return 0;
}

static void
Print_afs_ClientConfig_p(afs_ClientConfig_p config)
{
    printf("    clientVersion:  %d\n", config->clientVersion);
    printf("    serverVersion:  %d\n", config->serverVersion);
    printf("    nChunkFiles:    %d\n", config->c.config_v1.nChunkFiles);
    printf("    nStatCaches:    %d\n", config->c.config_v1.nStatCaches);
    printf("    nDataCaches:    %d\n", config->c.config_v1.nDataCaches);
    printf("    nVolumeCaches:  %d\n", config->c.config_v1.nVolumeCaches);
    printf("    firstChunkSize: %d\n", config->c.config_v1.firstChunkSize);
    printf("    otherChunkSize: %d\n", config->c.config_v1.otherChunkSize);
    printf("    cacheSize:      %d\n", config->c.config_v1.cacheSize);
    printf("    setTime:        %d\n", config->c.config_v1.setTime);
    printf("    memCache:       %d\n", config->c.config_v1.memCache);

}

int
DoClientCMClientConfig(struct cmd_syndesc *as, void *arock)
{
    afs_status_t st = 0;
    typedef enum { SERVER, PORT } DoClientCMLocalCell_parm_t;
    struct rx_connection *conn;
    int servAddr = 0;
    int srvrPort = AFSCONF_CALLBACKPORT;
    afs_ClientConfig_t config;

#ifdef AFS_NT40_ENV
    (pthread_func_list_done
     || pthread_once(&pthread_func_list_once, cr_list));
#endif

    if (as->parms[PORT].items) {
	if (!GetStatPortFromString(as->parms[PORT].items->data, &srvrPort)) {
	    ERR_EXT("Couldn't undertand port number");
	}
    }

    if (as->parms[SERVER].items) {
	if (!afsclient_CMStatOpenPort
	    (cellHandle, as->parms[SERVER].items->data, srvrPort, &conn,
	     &st)) {
	    ERR_ST_EXT("afsclient_CMStatOpenPort", st);
	}
    }

    if (!util_CMClientConfig(conn, &config, &st)) {
	ERR_ST_EXT("util_CMClientConfig", st);
    }

    printf("Cache configuration for client %s (port %s):\n\n",
	   as->parms[SERVER].items->data, as->parms[PORT].items->data);

    Print_afs_ClientConfig_p(&config);

    printf("\n");

    afsclient_CMStatClose(conn, 0);

    return 0;
}

void
SetupClientAdminCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("ClientLocalCellGet", DoClientLocalCellGet, NULL,
			  "get the name of this machine's cell");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientMountPointCreate", DoClientMountPointCreate,
			  NULL, "create a mount point");
    cmd_AddParm(ts, "-directory", CMD_SINGLE, CMD_REQUIRED,
		"directory where mount point will be created");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED,
		"the name of the volume to mount");
    cmd_AddParm(ts, "-readwrite", CMD_FLAG, CMD_OPTIONAL,
		"mount a read write volume");
    cmd_AddParm(ts, "-check", CMD_FLAG, CMD_OPTIONAL,
		"check that the volume exists before mounting");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientAFSServerGet", DoClientAFSServerGet, NULL,
			  "retrieve information about an afs server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientAFSServerList", DoClientAFSServerList, NULL,
			  "retrieve information about all afs "
			  "servers in a cell");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    cmd_AddParm(ts, "-stat_type", CMD_SINGLE, CMD_REQUIRED,
		"stats to retrieve <peer or process>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientRPCStatsStateGet", DoClientRPCStatsStateGet,
			  NULL, "retrieve the rpc stat collection state");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    cmd_AddParm(ts, "-stat_type", CMD_SINGLE, CMD_REQUIRED,
		"stats to retrieve <peer or process>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientRPCStatsStateEnable",
			  DoClientRPCStatsStateEnable, NULL,
			  "set the rpc stat collection state to on");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    cmd_AddParm(ts, "-stat_type", CMD_SINGLE, CMD_REQUIRED,
		"stats to retrieve <peer or process>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientRPCStatsStateDisable",
			  DoClientRPCStatsStateDisable, NULL,
			  "set the rpc stat collection state to off");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    cmd_AddParm(ts, "-stat_type", CMD_SINGLE, CMD_REQUIRED,
		"stats to retrieve <peer or process>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientRPCStatsList", DoClientRPCStatsList, NULL,
			  "list the rpc stats");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    cmd_AddParm(ts, "-stat_type", CMD_SINGLE, CMD_REQUIRED,
		"stats to retrieve <peer or process>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientRPCStatsClear", DoClientRPCStatsClear, NULL,
			  "reset rpc stat counters");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    cmd_AddParm(ts, "-stat_type", CMD_SINGLE, CMD_REQUIRED,
		"stats to retrieve <peer or process>");
    cmd_AddParm(ts, "-clear_all", CMD_FLAG, CMD_OPTIONAL,
		"clear all existing counters");
    cmd_AddParm(ts, "-clear_invocations", CMD_FLAG, CMD_OPTIONAL,
		"clear invocation count");
    cmd_AddParm(ts, "-clear_bytes_sent", CMD_FLAG, CMD_OPTIONAL,
		"clear bytes_sent count");
    cmd_AddParm(ts, "-clear_bytes_rcvd", CMD_FLAG, CMD_OPTIONAL,
		"clear bytes_rcvd count");
    cmd_AddParm(ts, "-clear_queue_time_sum", CMD_FLAG, CMD_OPTIONAL,
		"clear queue time sum");
    cmd_AddParm(ts, "-clear_queue_time_square", CMD_FLAG, CMD_OPTIONAL,
		"clear queue time square");
    cmd_AddParm(ts, "-clear_queue_time_min", CMD_FLAG, CMD_OPTIONAL,
		"clear queue time min");
    cmd_AddParm(ts, "-clear_queue_time_max", CMD_FLAG, CMD_OPTIONAL,
		"clear queue time max");
    cmd_AddParm(ts, "-clear_exec_time_sum", CMD_FLAG, CMD_OPTIONAL,
		"clear exec time sum");
    cmd_AddParm(ts, "-clear_exec_time_square", CMD_FLAG, CMD_OPTIONAL,
		"clear exec time square");
    cmd_AddParm(ts, "-clear_exec_time_min", CMD_FLAG, CMD_OPTIONAL,
		"clear exec time min");
    cmd_AddParm(ts, "-clear_exec_time_max", CMD_FLAG, CMD_OPTIONAL,
		"clear exec time max");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientRPCStatsVersionGet",
			  DoClientRPCStatsVersionGet, NULL,
			  "list the server's rpc stats version");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-process", CMD_SINGLE, CMD_REQUIRED,
		"process to query <bosserver fileserver ptserver "
		"kaserver client vlserver volserver>");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientCMGetServerPrefs", DoClientCMGetServerPrefs,
			  NULL, "list a client's server preferences ");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "UDP port to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientCMListCells", DoClientCMListCells, NULL,
			  "list a client's CellServDB ");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "UDP port to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientCMLocalCell", DoClientCMLocalCell, NULL,
			  "get the name of the client's local cell");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "UDP port to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("ClientCMClientConfig", DoClientCMClientConfig, NULL,
			  "get the client's cache configuration");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-port", CMD_SINGLE, CMD_OPTIONAL, "UDP port to query");
    SetupCommonCmdArgs(ts);
}
