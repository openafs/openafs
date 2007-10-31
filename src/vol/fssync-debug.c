/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Main program file. Define globals. */
#define MAIN 1

/*
 * fssync administration tool
 */


#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <WINNT/afsevent.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#endif
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <afs/assert.h>


#include <fcntl.h>

#ifndef AFS_NT40_ENV
#include <afs/osi_inode.h>
#endif

#include <afs/cmd.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>

#include "nfs.h"
#include "lwp.h"
#include "lock.h"
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "daemon_com.h"
#include "fssync.h"
#ifdef AFS_NT40_ENV
#include <pthread.h>
#endif

int VolumeChanged; /* hack to make dir package happy */


struct volop_state {
    afs_uint32 volume;
    char partName[16];
};

struct state {
    afs_int32 reason;
    struct volop_state * vop;
};

static int common_prolog(struct cmd_syndesc *, struct state *);
static int common_volop_prolog(struct cmd_syndesc *, struct state *);

static int do_volop(struct state *, afs_int32 command, SYNC_response * res);

static char * response_code_to_string(afs_int32);
static char * command_code_to_string(afs_int32);
static char * reason_code_to_string(afs_int32);
static char * program_type_to_string(afs_int32);

static int VolOnline(struct cmd_syndesc * as, void * rock);
static int VolOffline(struct cmd_syndesc * as, void * rock);
static int VolMode(struct cmd_syndesc * as, void * rock);
static int VolDetach(struct cmd_syndesc * as, void * rock);
static int VolBreakCBKs(struct cmd_syndesc * as, void * rock);
static int VolMove(struct cmd_syndesc * as, void * rock);
static int VolList(struct cmd_syndesc * as, void * rock);
static int VolQuery(struct cmd_syndesc * as, void * rock);
static int VolHdrQuery(struct cmd_syndesc * as, void * rock);
static int VolOpQuery(struct cmd_syndesc * as, void * rock);
static int StatsQuery(struct cmd_syndesc * as, void * rock);


static void print_vol_stats_general(VolPkgStats * stats);
static void print_vol_stats_viceP(struct DiskPartitionStats * stats);
static void print_vol_stats_hash(struct VolumeHashChainStats * stats);
#ifdef AFS_DEMAND_ATTACH_FS
static void print_vol_stats_hdr(struct volume_hdr_LRU_stats * stats);
#endif

#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif
#define MAX_ARGS 128

#define COMMON_PARMS_OFFSET    12
#define COMMON_PARMS(ts) \
    cmd_Seek(ts, COMMON_PARMS_OFFSET); \
    cmd_AddParm(ts, "-reason", CMD_SINGLE, CMD_OPTIONAL, "sync protocol reason code"); \
    cmd_AddParm(ts, "-programtype", CMD_SINGLE, CMD_OPTIONAL, "program type code")

#define COMMON_VOLOP_PARMS_OFFSET    10
#define COMMON_VOLOP_PARMS(ts) \
    cmd_Seek(ts, COMMON_VOLOP_PARMS_OFFSET); \
    cmd_AddParm(ts, "-volumeid", CMD_SINGLE, 0, "volume id"); \
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL, "partition name")

#define CUSTOM_PARMS_OFFSET 1


#define VOLOP_PARMS_DECL(ts) \
    COMMON_VOLOP_PARMS(ts); \
    COMMON_PARMS(ts)
#define COMMON_PARMS_DECL(ts) \
    COMMON_PARMS(ts)

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    int err = 0;
    int i;
    extern char cml_version_number[];

    /* Initialize directory paths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    
    ts = cmd_CreateSyntax("online", VolOnline, NULL, "bring a volume online (FSYNC_VOL_ON opcode)");
    VOLOP_PARMS_DECL(ts);

    ts = cmd_CreateSyntax("offline", VolOffline, NULL, "take a volume offline (FSYNC_VOL_OFF opcode)");
    VOLOP_PARMS_DECL(ts);

    ts = cmd_CreateSyntax("mode", VolMode, NULL, "change volume attach mode (FSYNC_VOL_NEEDVOLUME opcode)");
    VOLOP_PARMS_DECL(ts);
    cmd_CreateAlias(ts, "needvolume");

    ts = cmd_CreateSyntax("detach", VolDetach, NULL, "detach a volume (FSYNC_VOL_DONE opcode)");
    VOLOP_PARMS_DECL(ts);

    ts = cmd_CreateSyntax("callback", VolBreakCBKs, NULL, "break callbacks for volume (FSYNC_VOL_BREAKCBKS opcode)");
    VOLOP_PARMS_DECL(ts);
    cmd_CreateAlias(ts, "cbk");

    ts = cmd_CreateSyntax("move", VolMove, NULL, "set volume moved flag (FSYNC_VOL_MOVE opcode)");
    VOLOP_PARMS_DECL(ts);

    ts = cmd_CreateSyntax("list", VolList, NULL, "sync local volume list (FSYNC_VOL_LISTVOLUMES opcode)");
    VOLOP_PARMS_DECL(ts);
    cmd_CreateAlias(ts, "ls");

    ts = cmd_CreateSyntax("query", VolQuery, NULL, "get volume structure (FSYNC_VOL_QUERY opcode)");
    VOLOP_PARMS_DECL(ts);
    cmd_CreateAlias(ts, "qry");

    ts = cmd_CreateSyntax("header", VolHdrQuery, NULL, "get volume disk data structure (FSYNC_VOL_QUERY_HDR opcode)");
    VOLOP_PARMS_DECL(ts);
    cmd_CreateAlias(ts, "hdr");

    ts = cmd_CreateSyntax("volop", VolOpQuery, NULL, "get pending volume operation info (FSYNC_VOL_QUERY_VOP opcode)");
    VOLOP_PARMS_DECL(ts);
    cmd_CreateAlias(ts, "vop");

    ts = cmd_CreateSyntax("stats", StatsQuery, NULL, "see 'stats help' for more information");
    cmd_Seek(ts, CUSTOM_PARMS_OFFSET);
    cmd_AddParm(ts, "-cmd", CMD_SINGLE, 0, "subcommand");
    cmd_AddParm(ts, "-arg1", CMD_SINGLE, CMD_OPTIONAL, "arg1");
    cmd_AddParm(ts, "-arg2", CMD_SINGLE, CMD_OPTIONAL, "arg2");
    COMMON_PARMS_DECL(ts);

    err = cmd_Dispatch(argc, argv);
    exit(err);
}

static int
common_prolog(struct cmd_syndesc * as, struct state * state)
{
    register struct cmd_item *ti;

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	Exit(1);
    }
#endif

    VInitVolumePackage(debugUtility, 1, 1,
		       DONT_CONNECT_FS, 0);
    DInit(1);

    if ((ti = as->parms[COMMON_PARMS_OFFSET].items)) {	/* -reason */
	state->reason = atoi(ti->data);
    }
    if ((ti = as->parms[COMMON_PARMS_OFFSET+1].items)) {	/* -programtype */
	if (!strcmp(ti->data, "fileServer")) {
	    programType = fileServer;
	} else if (!strcmp(ti->data, "volumeUtility")) {
	    programType = volumeUtility;
	} else if (!strcmp(ti->data, "salvager")) {
	    programType = salvager;
	} else if (!strcmp(ti->data, "salvageServer")) {
	    programType = salvageServer;
	} else {
	    programType = (ProgramType) atoi(ti->data);
	}
    }

    VConnectFS();

    return 0;
}

static int
common_volop_prolog(struct cmd_syndesc * as, struct state * state)
{
    register struct cmd_item *ti;
    char pname[100], *temp;

    state->vop = (struct volop_state *) calloc(1, sizeof(struct volop_state));
    assert(state->vop != NULL);

    if ((ti = as->parms[COMMON_VOLOP_PARMS_OFFSET].items)) {	/* -volumeid */
	state->vop->volume = atoi(ti->data);
    } else {
	fprintf(stderr, "required argument -volumeid not given\n");
    }

    if ((ti = as->parms[COMMON_VOLOP_PARMS_OFFSET+1].items)) {	/* -partition */
	strlcpy(state->vop->partName, ti->data, sizeof(state->vop->partName));
    } else {
	memset(state->vop->partName, 0, sizeof(state->vop->partName));
    }

    return 0;
}

static int
do_volop(struct state * state, afs_int32 command, SYNC_response * res)
{
    afs_int32 code;
    SYNC_PROTO_BUF_DECL(res_buf);
    SYNC_response res_l;

    if (!res) {
	res = &res_l;
	res->payload.len = SYNC_PROTO_MAX_LEN;
	res->payload.buf = res_buf;
    }

    fprintf(stderr, "calling FSYNC_VolOp with command code %d (%s)\n", 
	    command, command_code_to_string(command));

    code = FSYNC_VolOp(state->vop->volume,
		       state->vop->partName,
		       command,
		       state->reason,
		       res);

    switch (code) {
    case SYNC_OK:
    case SYNC_DENIED:
	break;
    default:
	fprintf(stderr, "possible sync protocol error. return code was %d\n", code);
    }

    fprintf(stderr, "FSYNC_VolOp returned %d (%s)\n", code, response_code_to_string(code));
    fprintf(stderr, "protocol response code was %d (%s)\n", 
	    res->hdr.response, response_code_to_string(res->hdr.response));
    fprintf(stderr, "protocol reason code was %d (%s)\n", 
	    res->hdr.reason, reason_code_to_string(res->hdr.reason));

    VDisconnectFS();
}

static char *
response_code_to_string(afs_int32 response)
{
    switch (response) {
    case SYNC_OK:
	return "SYNC_OK";
    case SYNC_DENIED:
	return "SYNC_DENIED";
    case SYNC_COM_ERROR:
	return "SYNC_COM_ERROR";
    case SYNC_BAD_COMMAND:
	return "SYNC_BAD_COMMAND";
    case SYNC_FAILED:
	return "SYNC_FAILED";
    default:
	return "**UNKNOWN**";
    }
}

static char *
command_code_to_string(afs_int32 command)
{
    switch (command) {
    case SYNC_COM_CHANNEL_CLOSE:
	return "SYNC_COM_CHANNEL_CLOSE";
    case FSYNC_VOL_ON:
	return "FSYNC_VOL_ON";
    case FSYNC_VOL_OFF:
	return "FSYNC_VOL_OFF";
    case FSYNC_VOL_LISTVOLUMES:
	return "FSYNC_VOL_LISTVOLUMES";
    case FSYNC_VOL_NEEDVOLUME:
	return "FSYNC_VOL_NEEDVOLUME";
    case FSYNC_VOL_MOVE:
	return "FSYNC_VOL_MOVE";
    case FSYNC_VOL_BREAKCBKS:
	return "FSYNC_VOL_BREAKCBKS";
    case FSYNC_VOL_DONE:
	return "FSYNC_VOL_DONE";
    case FSYNC_VOL_QUERY:
	return "FSYNC_VOL_QUERY";
    case FSYNC_VOL_QUERY_HDR:
	return "FSYNC_VOL_QUERY_HDR";
    case FSYNC_VOL_QUERY_VOP:
	return "FSYNC_VOL_QUERY_VOP";
    case FSYNC_VOL_STATS_GENERAL:
	return "FSYNC_VOL_STATS_GENERAL";
    case FSYNC_VOL_STATS_VICEP:
	return "FSYNC_VOL_STATS_VICEP";
    case FSYNC_VOL_STATS_HASH:
	return "FSYNC_VOL_STATS_HASH";
    case FSYNC_VOL_STATS_HDR:
	return "FSYNC_VOL_STATS_HDR";
    case FSYNC_VOL_STATS_VLRU:
	return "FSYNC_VOL_STATS_VLRU";
    default:
	return "**UNKNOWN**";
    }
}

static char *
reason_code_to_string(afs_int32 reason)
{
    switch (reason) {
    case SYNC_REASON_NONE:
	return "SYNC_REASON_NONE";
    case SYNC_REASON_MALFORMED_PACKET:
	return "SYNC_REASON_MALFORMED_PACKET";
    case FSYNC_WHATEVER:
	return "FSYNC_WHATEVER";
    case FSYNC_SALVAGE:
	return "FSYNC_SALVAGE";
    case FSYNC_MOVE:
	return "FSYNC_MOVE";
    case FSYNC_OPERATOR:
	return "FSYNC_OPERATOR";
    case FSYNC_EXCLUSIVE:
	return "FSYNC_EXCLUSIVE";
    case FSYNC_UNKNOWN_VOLID:
	return "FSYNC_UNKNOWN_VOLID";
    case FSYNC_HDR_NOT_ATTACHED:
	return "FSYNC_HDR_NOT_ATTACHED";
    case FSYNC_NO_PENDING_VOL_OP:
	return "FSYNC_NO_PENDING_VOL_OP";
    case FSYNC_VOL_PKG_ERROR:
	return "FSYNC_VOL_PKG_ERROR";
    default:
	return "**UNKNOWN**";
    }
}

static char *
program_type_to_string(afs_int32 type)
{
    switch ((ProgramType)type) {
    case fileServer:
	return "fileServer";
    case volumeUtility:
	return "volumeUtility";
    case salvager:
	return "salvager";
    case salvageServer:
	return "salvageServer";
    case debugUtility:
      return "debugUtility";
    default:
	return "**UNKNOWN**";
    }
}

static int 
VolOnline(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_ON, NULL);

    return 0;
}

static int 
VolOffline(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_OFF, NULL);

    return 0;
}

static int
VolMode(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_NEEDVOLUME, NULL);

    return 0;
}

static int
VolDetach(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_DONE, NULL);

    return 0;
}

static int
VolBreakCBKs(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_BREAKCBKS, NULL);

    return 0;
}

static int
VolMove(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_MOVE, NULL);

    return 0;
}

static int
VolList(struct cmd_syndesc * as, void * rock)
{
    struct state state;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_LISTVOLUMES, NULL);

    return 0;
}

#ifdef AFS_DEMAND_ATTACH_FS
static char *
vol_state_to_string(VolState state)
{
    switch (state) {
    case VOL_STATE_UNATTACHED:
	return "VOL_STATE_UNATTACHED";
    case VOL_STATE_PREATTACHED:
	return "VOL_STATE_PREATTACHED";
    case VOL_STATE_ATTACHING:
	return "VOL_STATE_ATTACHING";
    case VOL_STATE_ATTACHED:
	return "VOL_STATE_ATTACHED";
    case VOL_STATE_UPDATING:
	return "VOL_STATE_UPDATING";
    case VOL_STATE_GET_BITMAP:
	return "VOL_STATE_GET_BITMAP";
    case VOL_STATE_HDR_LOADING:
	return "VOL_STATE_HDR_LOADING";
    case VOL_STATE_HDR_ATTACHING:
	return "VOL_STATE_HDR_ATTACHING";
    case VOL_STATE_SHUTTING_DOWN:
	return "VOL_STATE_SHUTTING_DOWN";
    case VOL_STATE_GOING_OFFLINE:
	return "VOL_STATE_GOING_OFFLINE";
    case VOL_STATE_OFFLINING:
	return "VOL_STATE_OFFLINING";
    case VOL_STATE_DETACHING:
	return "VOL_STATE_DETACHING";
    case VOL_STATE_SALVSYNC_REQ:
      return "VOL_STATE_SALVSYNC_REQ";
    case VOL_STATE_SALVAGING:
	return "VOL_STATE_SALVAGING";
    case VOL_STATE_ERROR:
	return "VOL_STATE_ERROR";
    case VOL_STATE_FREED:
	return "VOL_STATE_FREED";
    default:
	return "**UNKNOWN**";
    }
}

static char *
vol_flags_to_string(afs_uint16 flags)
{
    static char str[128];
    int count = 0;
    str[0]='\0';

    if (flags & VOL_HDR_ATTACHED) {
	strlcat(str, "VOL_HDR_ATTACHED", sizeof(str));
	count++;
    }

    if (flags & VOL_HDR_LOADED) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_HDR_LOADED", sizeof(str));
	count++;
    }

    if (flags & VOL_HDR_IN_LRU) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_HDR_IN_LRU", sizeof(str));
	count++;
    }

    if (flags & VOL_IN_HASH) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_IN_HASH", sizeof(str));
	count++;
    }

    if (flags & VOL_ON_VBYP_LIST) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_ON_VBYP_LIST", sizeof(str));
	count++;
    }

    if (flags & VOL_IS_BUSY) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_IS_BUSY", sizeof(str));
	count++;
    }

    if (flags & VOL_ON_VLRU) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_ON_VLRU", sizeof(str));
    }

    if (flags & VOL_HDR_DONTSALV) {
	if (count) {
	    strlcat(str, " | ", sizeof(str));
	}
	strlcat(str, "VOL_HDR_DONTSALV", sizeof(str));
    }

    return str;
}

static char *
vlru_idx_to_string(int idx)
{
    switch (idx) {
    case VLRU_QUEUE_NEW:
	return "VLRU_QUEUE_NEW";
    case VLRU_QUEUE_MID:
	return "VLRU_QUEUE_MID";
    case VLRU_QUEUE_OLD:
	return "VLRU_QUEUE_OLD";
    case VLRU_QUEUE_CANDIDATE:
	return "VLRU_QUEUE_CANDIDATE";
    case VLRU_QUEUE_HELD:
	return "VLRU_QUEUE_HELD";
    case VLRU_QUEUE_INVALID:
	return "VLRU_QUEUE_INVALID";
    default:
	return "**UNKNOWN**";
    }
}
#endif

static int
VolQuery(struct cmd_syndesc * as, void * rock)
{
    struct state state;
    SYNC_PROTO_BUF_DECL(res_buf);
    SYNC_response res;
    Volume v;
    int hi, lo;

    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = res_buf;
    res.payload.len = SYNC_PROTO_MAX_LEN;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_QUERY, &res);

    if (res.hdr.response == SYNC_OK) {
	memcpy(&v, res.payload.buf, sizeof(Volume));

	printf("volume = {\n");
	printf("\thashid          = %u\n", v.hashid);
	printf("\theader          = 0x%x\n", v.header);
	printf("\tdevice          = %d\n", v.device);
	printf("\tpartition       = 0x%x\n", v.partition);
	printf("\tlinkHandle      = 0x%x\n", v.linkHandle);
	printf("\tnextVnodeUnique = %u\n", v.nextVnodeUnique);
	printf("\tdiskDataHandle  = 0x%x\n", v.diskDataHandle);
	printf("\tvnodeHashOffset = %u\n", v.vnodeHashOffset);
	printf("\tshuttingDown    = %d\n", v.shuttingDown);
	printf("\tgoingOffline    = %d\n", v.goingOffline);
	printf("\tcacheCheck      = %u\n", v.cacheCheck);
	printf("\tnUsers          = %d\n", v.nUsers);
	printf("\tneedsPutBack    = %d\n", v.needsPutBack);
	printf("\tspecialStatus   = %d\n", v.specialStatus);
	printf("\tupdateTime      = %u\n", v.updateTime);
	
	printf("\tvnodeIndex[vSmall] = {\n");
        printf("\t\thandle       = 0x%x\n", v.vnodeIndex[vSmall].handle);
        printf("\t\tbitmap       = 0x%x\n", v.vnodeIndex[vSmall].bitmap);
	printf("\t\tbitmapSize   = %u\n", v.vnodeIndex[vSmall].bitmapSize);
	printf("\t\tbitmapOffset = %u\n", v.vnodeIndex[vSmall].bitmapOffset);
	printf("\t}\n");
	printf("\tvnodeIndex[vLarge] = {\n");
        printf("\t\thandle       = 0x%x\n", v.vnodeIndex[vLarge].handle);
        printf("\t\tbitmap       = 0x%x\n", v.vnodeIndex[vLarge].bitmap);
	printf("\t\tbitmapSize   = %u\n", v.vnodeIndex[vLarge].bitmapSize);
	printf("\t\tbitmapOffset = %u\n", v.vnodeIndex[vLarge].bitmapOffset);
	printf("\t}\n");
#ifdef AFS_DEMAND_ATTACH_FS
	if (res.hdr.flags & SYNC_FLAG_DAFS_EXTENSIONS) {
	    printf("\tupdateTime      = %u\n", v.updateTime);
	    printf("\tattach_state    = %s\n", vol_state_to_string(v.attach_state));
	    printf("\tattach_flags    = %s\n", vol_flags_to_string(v.attach_flags));
	    printf("\tnWaiters        = %d\n", v.nWaiters);
	    printf("\tchainCacheCheck = %d\n", v.chainCacheCheck);
	    
	    /* online salvage structure */
	    printf("\tsalvage = {\n");
	    printf("\t\tprio      = %u\n", v.salvage.prio);
	    printf("\t\treason    = %d\n", v.salvage.reason);
	    printf("\t\trequested = %d\n", v.salvage.requested);
	    printf("\t\tscheduled = %d\n", v.salvage.scheduled);
	    printf("\t}\n");
	    
	    /* statistics structure */
	    printf("\tstats = {\n");

	    printf("\t\thash_lookups = {\n");
	    SplitInt64(v.stats.hash_lookups,hi,lo);
	    printf("\t\t\thi = %u\n", hi);
	    printf("\t\t\tlo = %u\n", lo);
	    printf("\t\t}\n");

	    printf("\t\thash_short_circuits = {\n");
	    SplitInt64(v.stats.hash_short_circuits,hi,lo);
	    printf("\t\t\thi = %u\n", hi);
	    printf("\t\t\tlo = %u\n", lo);
	    printf("\t\t}\n");

	    printf("\t\thdr_loads = {\n");
	    SplitInt64(v.stats.hdr_loads,hi,lo);
	    printf("\t\t\thi = %u\n", hi);
	    printf("\t\t\tlo = %u\n", lo);
	    printf("\t\t}\n");

	    printf("\t\thdr_gets = {\n");
	    SplitInt64(v.stats.hdr_gets,hi,lo);
	    printf("\t\t\thi = %u\n", hi);
	    printf("\t\t\tlo = %u\n", lo);
	    printf("\t\t}\n");
	    
	    printf("\t\tattaches         = %u\n", v.stats.attaches);
	    printf("\t\tsoft_detaches    = %u\n", v.stats.soft_detaches);
	    printf("\t\tsalvages         = %u\n", v.stats.salvages);
	    printf("\t\tvol_ops          = %u\n", v.stats.vol_ops);
	    
	    printf("\t\tlast_attach      = %u\n", v.stats.last_attach);
	    printf("\t\tlast_get         = %u\n", v.stats.last_get);
	    printf("\t\tlast_promote     = %u\n", v.stats.last_promote);
	    printf("\t\tlast_hdr_get     = %u\n", v.stats.last_hdr_get);
	    printf("\t\tlast_salvage     = %u\n", v.stats.last_salvage);
	    printf("\t\tlast_salvage_req = %u\n", v.stats.last_salvage_req);
	    printf("\t\tlast_vol_op      = %u\n", v.stats.last_vol_op);
	    printf("\t}\n");
	    
	    /* VLRU state */
	    printf("\tvlru = {\n");
	    printf("\t\tidx = %d (%s)\n", 
		   v.vlru.idx, vlru_idx_to_string(v.vlru.idx));
	    printf("\t}\n");

	    /* volume op state */
	    printf("\tpending_vol_op  = 0x%x\n", v.pending_vol_op);
	}
#else /* !AFS_DEMAND_ATTACH_FS */
	if (res.hdr.flags & SYNC_FLAG_DAFS_EXTENSIONS) {
	    printf("*** server asserted demand attach extensions. fssync-debug not built to\n");
	    printf("*** recognize those extensions. please recompile fssync-debug if you need\n");
	    printf("*** to dump dafs extended state\n");
	}
#endif /* !AFS_DEMAND_ATTACH_FS */
	printf("}\n");
    }

    return 0;
}

static int
VolHdrQuery(struct cmd_syndesc * as, void * rock)
{
    struct state state;
    SYNC_PROTO_BUF_DECL(res_buf);
    SYNC_response res;
    VolumeDiskData v;
    int i;

    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = res_buf;
    res.payload.len = SYNC_PROTO_MAX_LEN;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_QUERY_HDR, &res);

    if (res.hdr.response == SYNC_OK) {
	memcpy(&v, res.payload.buf, sizeof(VolumeDiskData));

	printf("VolumeDiskData = {\n");
	printf("\tstamp = {\n");
	printf("\t\tmagic   = 0x%x\n", v.stamp.magic);
	printf("\t\tversion = %u\n", v.stamp.version);
	printf("\t}\n");
	
	printf("\tid               = %u\n", v.id);
	printf("\tname             = '%s'\n", v.name);
	printf("\tinUse            = %d\n", v.inUse);
	printf("\tinService        = %d\n", v.inService);
	printf("\tblessed          = %d\n", v.blessed);
	printf("\tneedsSalvaged    = %d\n", v.needsSalvaged);
	printf("\tuniquifier       = %u\n", v.uniquifier);
	printf("\ttype             = %d\n", v.type);
	printf("\tparentId         = %u\n", v.parentId);
	printf("\tcloneId          = %u\n", v.cloneId);
	printf("\tbackupId         = %u\n", v.backupId);
	printf("\trestoredFromId   = %u\n", v.restoredFromId);
	printf("\tneedsCallback    = %d\n", v.needsCallback);
	printf("\tdestroyMe        = %d\n", v.destroyMe);
	printf("\tdontSalvage      = %d\n", v.dontSalvage);
	printf("\tmaxquota         = %d\n", v.maxquota);
	printf("\tminquota         = %d\n", v.minquota);
	printf("\tmaxfiles         = %d\n", v.maxfiles);
	printf("\taccountNumber    = %u\n", v.accountNumber);
	printf("\towner            = %u\n", v.owner);
	printf("\tfilecount        = %d\n", v.filecount);
	printf("\tdiskused         = %d\n", v.diskused);
	printf("\tdayUse           = %d\n", v.dayUse);
	for (i = 0; i < 7; i++) {
	    printf("\tweekUse[%d]       = %d\n", i, v.weekUse[i]);
	}
	printf("\tdayUseDate       = %u\n", v.dayUseDate);
	printf("\tcreationDate     = %u\n", v.creationDate);
	printf("\taccessDate       = %u\n", v.accessDate);
	printf("\tupdateDate       = %u\n", v.updateDate);
	printf("\texpirationDate   = %u\n", v.expirationDate);
	printf("\tbackupDate       = %u\n", v.backupDate);
	printf("\tcopyDate         = %u\n", v.copyDate);
#ifdef OPENAFS_VOL_STATS
	printf("\tstat_initialized = %d\n", v.stat_initialized);
#else
        printf("\tmtd              = '%s'\n", v.motd);
#endif
	printf("}\n");
    }

    return 0;
}

static int
VolOpQuery(struct cmd_syndesc * as, void * rock)
{
    struct state state;
    SYNC_PROTO_BUF_DECL(res_buf);
    SYNC_response res;
    FSSYNC_VolOp_info vop;
    int i;

    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = res_buf;
    res.payload.len = SYNC_PROTO_MAX_LEN;

    common_prolog(as, &state);
    common_volop_prolog(as, &state);

    do_volop(&state, FSYNC_VOL_QUERY_VOP, &res);

    if (!(res.hdr.flags & SYNC_FLAG_DAFS_EXTENSIONS)) {
	printf("*** file server not compiled with demand attach extensions.\n");
	printf("*** pending volume operation metadata not available.\n");
    }

    if (res.hdr.response == SYNC_OK) {
	memcpy(&vop, res.payload.buf, sizeof(FSSYNC_VolOp_info));

	printf("pending_vol_op = {\n");

	printf("\tcom = {\n");
	printf("\t\tproto_version  = %u\n", vop.com.proto_version);
	printf("\t\tprogramType    = %d (%s)\n", 
	       vop.com.programType, program_type_to_string(vop.com.programType));
	printf("\t\tcommand        = %d (%s)\n", 
	       vop.com.command, command_code_to_string(vop.com.command));
	printf("\t\treason         = %d (%s)\n", 
	       vop.com.reason, reason_code_to_string(vop.com.reason));
	printf("\t\tcommand_len    = %u\n", vop.com.command_len);
	printf("\t\tflags          = 0x%x\n", vop.com.flags);
	printf("\t}\n");

	printf("\tvop = {\n");
	printf("\t\tvolume         = %u\n", vop.vop.volume);
	if (afs_strnlen(vop.vop.partName, sizeof(vop.vop.partName)) <
	    sizeof(vop.vop.partName)) {
	    printf("\t\tpartName       = '%s'\n", vop.vop.partName);
	} else {
	    printf("\t\tpartName       = (illegal string)\n");
	}
	printf("\t}\n");

	printf("}\n");
    }

    return 0;
}

static int
StatsQuery(struct cmd_syndesc * as, void * rock)
{
    afs_int32 code;
    int command;
    struct cmd_item *ti;
    struct state state;
    SYNC_PROTO_BUF_DECL(res_buf);
    SYNC_response res;
    FSSYNC_StatsOp_hdr scom;
    union {
	void * ptr;
	struct VolPkgStats * vol_stats;
	struct VolumeHashChainStats * hash_stats;
#ifdef AFS_DEMAND_ATTACH_FS
	struct volume_hdr_LRU_stats * hdr_stats;
#endif
	struct DiskPartitionStats * vicep_stats;
    } sres;

    sres.ptr = res_buf;
    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = res_buf;
    res.payload.len = SYNC_PROTO_MAX_LEN;

    if ((ti = as->parms[CUSTOM_PARMS_OFFSET].items)) {	/* -subcommand */
	if (!strcasecmp(ti->data, "vicep")) {
	    command = FSYNC_VOL_STATS_VICEP;
	} else if (!strcasecmp(ti->data, "hash")) {
	    command = FSYNC_VOL_STATS_HASH;
#ifdef AFS_DEMAND_ATTACH_FS
	} else if (!strcasecmp(ti->data, "hdr")) {
	    command = FSYNC_VOL_STATS_HDR;
	} else if (!strcasecmp(ti->data, "vlru")) {
	    command = FSYNC_VOL_STATS_VLRU;
#endif
	} else if (!strcasecmp(ti->data, "pkg")) {
	    command = FSYNC_VOL_STATS_GENERAL;
	} else if (!strcasecmp(ti->data, "help")) {
	    fprintf(stderr, "fssync-debug stats subcommands:\n");
	    fprintf(stderr, "\tpkg\tgeneral volume package stats\n");
	    fprintf(stderr, "\tvicep\tvice partition stats\n");
	    fprintf(stderr, "\thash\tvolume hash chain stats\n");
#ifdef AFS_DEMAND_ATTACH_FS
	    fprintf(stderr, "\thdr\tvolume header cache stats\n");
	    fprintf(stderr, "\tvlru\tvlru generation stats\n");
#endif
	    exit(0);
	} else {
	    fprintf(stderr, "invalid stats subcommand");
	    exit(1);
	}
    } else {
	command = FSYNC_VOL_STATS_GENERAL;
    }

    if ((ti = as->parms[CUSTOM_PARMS_OFFSET+1].items)) {	/* -arg1 */
	switch (command) {
	case FSYNC_VOL_STATS_VICEP:
	    strlcpy(scom.args.partName, ti->data, sizeof(state.vop->partName));
	    break;
	case FSYNC_VOL_STATS_HASH:
	    scom.args.hash_bucket = atoi(ti->data);
	    break;
	case FSYNC_VOL_STATS_VLRU:
	    scom.args.vlru_generation = atoi(ti->data);
	    break;
	default:
	    fprintf(stderr, "unrecognized arguments\n");
	    exit(1);
	}
    } else {
	switch (command) {
	case FSYNC_VOL_STATS_VICEP:
	case FSYNC_VOL_STATS_HASH:
	case FSYNC_VOL_STATS_VLRU:
	    fprintf(stderr, "this subcommand requires more parameters\n");
	    exit(1);
	}
    }

    common_prolog(as, &state);

    fprintf(stderr, "calling FSYNC_askfs with command code %d (%s)\n", 
	    command, command_code_to_string(command));

    code = FSYNC_StatsOp(&scom, command, FSYNC_WHATEVER, &res);

    switch (code) {
    case SYNC_OK:
    case SYNC_DENIED:
	break;
    default:
	fprintf(stderr, "possible sync protocol error. return code was %d\n", code);
    }

    fprintf(stderr, "FSYNC_VolOp returned %d (%s)\n", code, response_code_to_string(code));
    fprintf(stderr, "protocol response code was %d (%s)\n", 
	    res.hdr.response, response_code_to_string(res.hdr.response));
    fprintf(stderr, "protocol reason code was %d (%s)\n", 
	    res.hdr.reason, reason_code_to_string(res.hdr.reason));

    VDisconnectFS();

    if (res.hdr.response == SYNC_OK) {
	switch (command) {
	case FSYNC_VOL_STATS_GENERAL:
	    print_vol_stats_general(sres.vol_stats);
	    break;
	case FSYNC_VOL_STATS_VICEP:
	    print_vol_stats_viceP(sres.vicep_stats);
	    break;
	case FSYNC_VOL_STATS_HASH:
	    print_vol_stats_hash(sres.hash_stats);
	    break;
#ifdef AFS_DEMAND_ATTACH_FS
	case FSYNC_VOL_STATS_HDR:
	    print_vol_stats_hdr(sres.hdr_stats);
	    break;
#endif /* AFS_DEMAND_ATTACH_FS */
	}
    }

    return 0;
}

static void
print_vol_stats_general(VolPkgStats * stats)
{
    int i;
    afs_uint32 hi, lo;

    printf("VolPkgStats = {\n");
#ifdef AFS_DEMAND_ATTACH_FS
    for (i = 0; i < VOL_STATE_COUNT; i++) {
	printf("\tvol_state_count[%s] = %d\n", 
	       vol_state_to_string(i),
	       stats->state_levels[i]);
    }

    SplitInt64(stats->hash_looks, hi, lo);
    printf("\thash_looks = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->hash_reorders, hi, lo);
    printf("\thash_reorders = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->salvages, hi, lo);
    printf("\tsalvages = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->vol_ops, hi, lo);
    printf("\tvol_ops = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");
#endif
    SplitInt64(stats->hdr_loads, hi, lo);
    printf("\thdr_loads = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->hdr_gets, hi, lo);
    printf("\thdr_gets = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->attaches, hi, lo);
    printf("\tattaches = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->soft_detaches, hi, lo);
    printf("\tsoft_detaches = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    printf("\thdr_cache_size = %d\n", stats->hdr_cache_size);
	    
    printf("}\n");
}

static void
print_vol_stats_viceP(struct DiskPartitionStats * stats)
{
    printf("DiskPartitionStats = {\n");
    printf("\tfree = %d\n", stats->free);
    printf("\tminFree = %d\n", stats->minFree);
    printf("\ttotalUsable = %d\n", stats->totalUsable);
    printf("\tf_files = %d\n", stats->f_files);
#ifdef AFS_DEMAND_ATTACH_FS
    printf("\tvol_list_len = %d\n", stats->vol_list_len);
#endif
    printf("}\n");
}

static void
print_vol_stats_hash(struct VolumeHashChainStats * stats)
{
    afs_uint32 hi, lo;

    printf("DiskPartitionStats = {\n");
    printf("\ttable_size = %d\n", stats->table_size);
    printf("\tchain_len = %d\n", stats->chain_len);

#ifdef AFS_DEMAND_ATTACH_FS
    printf("\tchain_cacheCheck = %d\n", stats->chain_cacheCheck);
    printf("\tchain_busy = %d\n", stats->chain_busy);

    SplitInt64(stats->chain_looks, hi, lo);
    printf("\tchain_looks = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->chain_gets, hi, lo);
    printf("\tchain_gets = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");

    SplitInt64(stats->chain_reorders, hi, lo);
    printf("\tchain_reorders = {\n");
    printf("\t\thi = %u\n", hi);
    printf("\t\tlo = %u\n", lo);
    printf("\t}\n");
#endif /* AFS_DEMAND_ATTACH_FS */

    printf("}\n");
}


#ifdef AFS_DEMAND_ATTACH_FS
static void
print_vol_stats_hdr(struct volume_hdr_LRU_stats * stats)
{
    printf("volume_hdr_LRU_stats = {\n");
    printf("\tfree = %d\n", stats->free);
    printf("\tused = %d\n", stats->used);
    printf("\tattached = %d\n", stats->attached);
    printf("}\n");
}
#endif /* AFS_DEMAND_ATTACH_FS */

