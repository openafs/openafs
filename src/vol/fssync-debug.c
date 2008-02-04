/*
 * Copyright 2006-2008, Sine Nomine Associates and others.
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
    afs_uint32 vnode;
    afs_uint32 unique;
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
static int VnQuery(struct cmd_syndesc * as, void * rock);

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

    ts = cmd_CreateSyntax("vnode", VnQuery, NULL, "get vnode structure (FSYNC_VOL_QUERY_VNODE opcode)");
    cmd_Seek(ts, CUSTOM_PARMS_OFFSET);
    cmd_AddParm(ts, "-volumeid", CMD_SINGLE, 0, "volume id");
    cmd_AddParm(ts, "-vnodeid", CMD_SINGLE, 0, "vnode id");
    cmd_AddParm(ts, "-unique", CMD_SINGLE, 0, "uniquifier");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, 0, "paritition name");
    COMMON_PARMS_DECL(ts);

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


#define ENUMTOSTRING(en)  #en
#define ENUMCASE(en) \
    case en: \
        return ENUMTOSTRING(en); \
        break

#define FLAGTOSTRING(fl)  #fl
#define FLAGCASE(bitstr, fl, str, count) \
    do { \
        if ((bitstr) & (fl)) { \
            if (count) \
                strlcat((str), " | ", sizeof(str)); \
            strlcat((str), FLAGTOSTRING(fl), sizeof(str)); \
            (count)++; \
        } \
    } while (0)

static char *
response_code_to_string(afs_int32 response)
{
    switch (response) {
	ENUMCASE(SYNC_OK);
	ENUMCASE(SYNC_DENIED);
	ENUMCASE(SYNC_COM_ERROR);
	ENUMCASE(SYNC_BAD_COMMAND);
	ENUMCASE(SYNC_FAILED);
    default:
	return "**UNKNOWN**";
    }
}

static char *
command_code_to_string(afs_int32 command)
{
    switch (command) {
	ENUMCASE(SYNC_COM_CHANNEL_CLOSE);
	ENUMCASE(FSYNC_VOL_ON);
	ENUMCASE(FSYNC_VOL_OFF);
	ENUMCASE(FSYNC_VOL_LISTVOLUMES);
	ENUMCASE(FSYNC_VOL_NEEDVOLUME);
	ENUMCASE(FSYNC_VOL_MOVE);
	ENUMCASE(FSYNC_VOL_BREAKCBKS);
	ENUMCASE(FSYNC_VOL_DONE);
	ENUMCASE(FSYNC_VOL_QUERY);
	ENUMCASE(FSYNC_VOL_QUERY_HDR);
	ENUMCASE(FSYNC_VOL_QUERY_VOP);
	ENUMCASE(FSYNC_VOL_STATS_GENERAL);
	ENUMCASE(FSYNC_VOL_STATS_VICEP);
	ENUMCASE(FSYNC_VOL_STATS_HASH);
	ENUMCASE(FSYNC_VOL_STATS_HDR);
	ENUMCASE(FSYNC_VOL_STATS_VLRU);
	ENUMCASE(FSYNC_VOL_ATTACH);
	ENUMCASE(FSYNC_VOL_FORCE_ERROR);
	ENUMCASE(FSYNC_VOL_LEAVE_OFF);
	ENUMCASE(FSYNC_VOL_QUERY_VNODE);
    default:
	return "**UNKNOWN**";
    }
}

static char *
reason_code_to_string(afs_int32 reason)
{
    switch (reason) {
	ENUMCASE(SYNC_REASON_NONE);
	ENUMCASE(SYNC_REASON_MALFORMED_PACKET);
	ENUMCASE(SYNC_REASON_NOMEM);
	ENUMCASE(SYNC_REASON_ENCODING_ERROR);
	ENUMCASE(FSYNC_WHATEVER);
	ENUMCASE(FSYNC_SALVAGE);
	ENUMCASE(FSYNC_MOVE);
	ENUMCASE(FSYNC_OPERATOR);
	ENUMCASE(FSYNC_EXCLUSIVE);
	ENUMCASE(FSYNC_UNKNOWN_VOLID);
	ENUMCASE(FSYNC_HDR_NOT_ATTACHED);
	ENUMCASE(FSYNC_NO_PENDING_VOL_OP);
	ENUMCASE(FSYNC_VOL_PKG_ERROR);
	ENUMCASE(FSYNC_UNKNOWN_VNID);
    default:
	return "**UNKNOWN**";
    }
}

static char *
program_type_to_string(afs_int32 type)
{
    switch ((ProgramType)type) {
	ENUMCASE(fileServer);
	ENUMCASE(volumeUtility);
	ENUMCASE(salvager);
	ENUMCASE(salvageServer);
	ENUMCASE(debugUtility);
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
	ENUMCASE(VOL_STATE_UNATTACHED);
	ENUMCASE(VOL_STATE_PREATTACHED);
	ENUMCASE(VOL_STATE_ATTACHING);
	ENUMCASE(VOL_STATE_ATTACHED);
	ENUMCASE(VOL_STATE_UPDATING);
	ENUMCASE(VOL_STATE_GET_BITMAP);
	ENUMCASE(VOL_STATE_HDR_LOADING);
	ENUMCASE(VOL_STATE_HDR_ATTACHING);
	ENUMCASE(VOL_STATE_SHUTTING_DOWN);
	ENUMCASE(VOL_STATE_GOING_OFFLINE);
	ENUMCASE(VOL_STATE_OFFLINING);
	ENUMCASE(VOL_STATE_DETACHING);
	ENUMCASE(VOL_STATE_SALVSYNC_REQ);
	ENUMCASE(VOL_STATE_SALVAGING);
	ENUMCASE(VOL_STATE_ERROR);
	ENUMCASE(VOL_STATE_VNODE_ALLOC);
	ENUMCASE(VOL_STATE_VNODE_GET);
	ENUMCASE(VOL_STATE_VNODE_CLOSE);
	ENUMCASE(VOL_STATE_VNODE_RELEASE);
	ENUMCASE(VOL_STATE_VLRU_ADD);
	ENUMCASE(VOL_STATE_FREED);
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

    FLAGCASE(flags, VOL_HDR_ATTACHED, str, count);
    FLAGCASE(flags, VOL_HDR_LOADED, str, count);
    FLAGCASE(flags, VOL_HDR_IN_LRU, str, count);
    FLAGCASE(flags, VOL_IN_HASH, str, count);
    FLAGCASE(flags, VOL_ON_VBYP_LIST, str, count);
    FLAGCASE(flags, VOL_IS_BUSY, str, count);
    FLAGCASE(flags, VOL_ON_VLRU, str, count);
    FLAGCASE(flags, VOL_HDR_DONTSALV, str, count);

    return str;
}

static char *
vlru_idx_to_string(int idx)
{
    switch (idx) {
	ENUMCASE(VLRU_QUEUE_NEW);
	ENUMCASE(VLRU_QUEUE_MID);
	ENUMCASE(VLRU_QUEUE_OLD);
	ENUMCASE(VLRU_QUEUE_CANDIDATE);
	ENUMCASE(VLRU_QUEUE_HELD);
	ENUMCASE(VLRU_QUEUE_INVALID);
    default:
	return "**UNKNOWN**";
    }
}


static char *
vn_state_to_string(VnState state)
{
    switch (state) {
	ENUMCASE(VN_STATE_INVALID);
	ENUMCASE(VN_STATE_RELEASING);
	ENUMCASE(VN_STATE_CLOSING);
	ENUMCASE(VN_STATE_ALLOC);
	ENUMCASE(VN_STATE_ONLINE);
	ENUMCASE(VN_STATE_LOAD);
	ENUMCASE(VN_STATE_EXCLUSIVE);
	ENUMCASE(VN_STATE_STORE);
	ENUMCASE(VN_STATE_READ);
	ENUMCASE(VN_STATE_ERROR);
    default:
	return "**UNKNOWN**";
    }
}

static char *
vn_flags_to_string(afs_uint32 flags)
{
    static char str[128];
    int count = 0;
    str[0]='\0';

    FLAGCASE(flags, VN_ON_HASH, str, count);
    FLAGCASE(flags, VN_ON_LRU, str, count);
    FLAGCASE(flags, VN_ON_VVN, str, count);

    return str;
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
	    printf("\t\tlast_hdr_load    = %u\n", v.stats.last_hdr_load);
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
vn_prolog(struct cmd_syndesc * as, struct state * state)
{
    register struct cmd_item *ti;
    char pname[100], *temp;

    state->vop = (struct volop_state *) calloc(1, sizeof(struct volop_state));
    assert(state->vop != NULL);

    if ((ti = as->parms[CUSTOM_PARMS_OFFSET].items)) {	/* -volumeid */
	state->vop->volume = atoi(ti->data);
    } else {
	fprintf(stderr, "required argument -volumeid not given\n");
    }

    if ((ti = as->parms[CUSTOM_PARMS_OFFSET+1].items)) {	/* -vnodeid */
	state->vop->vnode = atoi(ti->data);
    } else {
	fprintf(stderr, "required argument -vnodeid not given\n");
    }

    if ((ti = as->parms[CUSTOM_PARMS_OFFSET+2].items)) {	/* -unique */
	state->vop->unique = atoi(ti->data);
    } else {
	state->vop->unique = 0;
    }

    if ((ti = as->parms[COMMON_VOLOP_PARMS_OFFSET+3].items)) {	/* -partition */
	strlcpy(state->vop->partName, ti->data, sizeof(state->vop->partName));
    } else {
	memset(state->vop->partName, 0, sizeof(state->vop->partName));
    }

    return 0;
}

static int
do_vnqry(struct state * state, SYNC_response * res)
{
    afs_int32 code;
    int command = FSYNC_VOL_QUERY_VNODE;
    FSSYNC_VnQry_hdr qry;

    qry.volume = state->vop->volume;
    qry.vnode = state->vop->vnode;
    qry.unique = state->vop->unique;
    qry.spare = 0;
    strlcpy(qry.partName, state->vop->partName, sizeof(qry.partName));

    fprintf(stderr, "calling FSYNC_GenericOp with command code %d (%s)\n", 
	    command, command_code_to_string(command));

    code = FSYNC_GenericOp(&qry, sizeof(qry), command, FSYNC_OPERATOR, res);

    switch (code) {
    case SYNC_OK:
    case SYNC_DENIED:
	break;
    default:
	fprintf(stderr, "possible sync protocol error. return code was %d\n", code);
    }

    fprintf(stderr, "FSYNC_GenericOp returned %d (%s)\n", code, response_code_to_string(code));
    fprintf(stderr, "protocol response code was %d (%s)\n", 
	    res->hdr.response, response_code_to_string(res->hdr.response));
    fprintf(stderr, "protocol reason code was %d (%s)\n", 
	    res->hdr.reason, reason_code_to_string(res->hdr.reason));

    VDisconnectFS();

    return 0;
}

static int
VnQuery(struct cmd_syndesc * as, void * rock)
{
    struct state state;
    SYNC_PROTO_BUF_DECL(res_buf);
    SYNC_response res;
    Vnode v;
    int hi, lo;

    res.hdr.response_len = sizeof(res.hdr);
    res.payload.buf = res_buf;
    res.payload.len = SYNC_PROTO_MAX_LEN;

    common_prolog(as, &state);
    vn_prolog(as, &state);

    do_vnqry(&state, &res);

    if (res.hdr.response == SYNC_OK) {
	memcpy(&v, res.payload.buf, sizeof(Volume));

	printf("vnode = {\n");

	printf("\tvid_hash = {\n");
	printf("\t\tnext = 0x%lx\n", v.vid_hash.next);
	printf("\t\tprev = 0x%lx\n", v.vid_hash.prev);
	printf("\t}\n");

	printf("\thashNext        = 0x%lx\n", v.hashNext);
	printf("\tlruNext         = 0x%lx\n", v.lruNext);
	printf("\tlruPrev         = 0x%lx\n", v.lruPrev);
	printf("\thashIndex       = %hu\n", v.hashIndex);
	printf("\tchanged_newTime = %u\n", (unsigned int) v.changed_newTime);
	printf("\tchanged_oldTime = %u\n", (unsigned int) v.changed_oldTime);
	printf("\tdelete          = %u\n", (unsigned int) v.delete);
	printf("\tvnodeNumber     = %u\n", v.vnodeNumber);
	printf("\tvolumePtr       = 0x%lx\n", v.volumePtr);
	printf("\tnUsers          = %u\n", v.nUsers);
	printf("\tcacheCheck      = %u\n", v.cacheCheck);

#ifdef AFS_DEMAND_ATTACH_FS
	if (!(res.hdr.flags & SYNC_FLAG_DAFS_EXTENSIONS)) {
	    printf("*** fssync-debug built to expect demand attach extensions.  server asserted\n");
	    printf("*** that it was not compiled with demand attach turned on.  please recompile\n");
	    printf("*** fssync-debug to match your server\n");
	    goto done;
	}

	printf("\tnReaders        = %u\n", v.nReaders);
	printf("\tvn_state_flags  = %s\n", vn_flags_to_string(v.vn_state_flags));
	printf("\tvn_state        = %s\n", vn_state_to_string(v.vn_state));
#else
	if (res.hdr.flags & SYNC_FLAG_DAFS_EXTENSIONS) {
	    printf("*** server asserted demand attach extensions. fssync-debug not built to\n");
	    printf("*** recognize those extensions. please recompile fssync-debug if you need\n");
	    printf("*** to dump dafs extended state\n");
	    goto done;
	}
#endif /* !AFS_DEMAND_ATTACH_FS */

	printf("\twriter          = %u\n", v.writer);
	printf("\tvcp             = 0x%lx\n", v.vcp);
	printf("\thandle          = 0x%lx\n", v.handle);

	printf("\tdisk = {\n");
	printf("\t\ttype              = %u\n", v.disk.type);
	printf("\t\tcloned            = %u\n", v.disk.cloned);
	printf("\t\tmodeBits          = %u\n", v.disk.modeBits);
	printf("\t\tlinkCount         = %d\n", v.disk.linkCount);
	printf("\t\tlength            = %u\n", v.disk.length);
	printf("\t\tuniquifier        = %u\n", v.disk.uniquifier);
	printf("\t\tdataVersion       = %u\n", v.disk.dataVersion);
	printf("\t\tvn_ino_lo         = %u\n", v.disk.vn_ino_lo);
	printf("\t\tunixModifyTime    = %u\n", v.disk.unixModifyTime);
	printf("\t\tauthor            = %u\n", v.disk.author);
	printf("\t\towner             = %u\n", v.disk.owner);
	printf("\t\tparent            = %u\n", v.disk.parent);
	printf("\t\tvnodeMagic        = %u\n", v.disk.vnodeMagic);

	printf("\t\tlock = {\n");
	printf("\t\t\tlockCount   = %d\n", v.disk.lock.lockCount);
	printf("\t\t\tlockTime    = %d\n", v.disk.lock.lockTime);
	printf("\t\t}\n");

	printf("\t\tserverModifyTime  = %u\n", v.disk.serverModifyTime);
	printf("\t\tgroup             = %d\n", v.disk.group);
	printf("\t\tvn_ino_hi         = %d\n", v.disk.vn_ino_hi);
	printf("\t\treserved6         = %u\n", v.disk.reserved6);
	printf("\t}\n");

	printf("}\n");
    }

 done:
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

