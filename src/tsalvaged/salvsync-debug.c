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
 * salvsync debug tool
 */


#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif

#include <rx/xdr.h>
#include <rx/rx_queue.h>
#include <afs/afsint.h>
#include <afs/opr_assert.h>
#include <afs/dir.h>

#ifndef AFS_NT40_ENV
#include <afs/osi_inode.h>
#endif

#include <afs/cmd.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>

#include <afs/nfs.h>
#include <lwp.h>
#include <afs/afs_lock.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/partition.h>
#include <afs/daemon_com.h>
#include <afs/salvsync.h>
#ifdef AFS_NT40_ENV
#include <pthread.h>
#endif

int VolumeChanged; /* hack to make dir package happy */


#ifndef AFS_DEMAND_ATTACH_FS
int
main(int argc, char ** argv)
{
    fprintf(stderr, "*** salvsync-debug is only supported for OpenAFS builds with the demand-attach fileserver extension\n");
    return -1;
}
#else /* AFS_DEMAND_ATTACH_FS */

struct salv_state {
    afs_uint32 prio;
    afs_uint32 volume;
    char partName[16];
};

struct fssync_state {
    afs_int32 reason;
    struct salv_state * sop;
};

static int common_prolog(struct cmd_syndesc *, struct fssync_state *);
static int common_salv_prolog(struct cmd_syndesc *, struct fssync_state *);

static int do_salvop(struct fssync_state *, afs_int32 command, SYNC_response * res);

static char * response_code_to_string(afs_int32);
static char * command_code_to_string(afs_int32);
static char * reason_code_to_string(afs_int32);
static char * state_code_to_string(afs_int32);


static int OpStats(struct cmd_syndesc * as, void * rock);
static int OpSalvage(struct cmd_syndesc * as, void * rock);
static int OpCancel(struct cmd_syndesc * as, void * rock);
static int OpCancelAll(struct cmd_syndesc * as, void * rock);
static int OpRaisePrio(struct cmd_syndesc * as, void * rock);
static int OpQuery(struct cmd_syndesc * as, void * rock);


#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

enum {
    OPT_volumeid,
    OPT_partition,
    OPT_priority,
    OPT_reason,
    OPT_programtype,
};

static void
common_parms(struct cmd_syndesc *as)
{
    cmd_AddParmAtOffset(as, OPT_reason, "-reason", CMD_SINGLE, CMD_OPTIONAL,
			"sync protocol reason code");
    cmd_AddParmAtOffset(as, OPT_programtype, "-programtype", CMD_SINGLE,
			CMD_OPTIONAL, "program type code");
}

static void
common_parms_salv(struct cmd_syndesc *as, int partition_required)
{
    afs_int32 partition_flags = CMD_REQUIRED;

    if (!partition_required) {
	partition_flags = CMD_OPTIONAL;
    }

    common_parms(as);

    cmd_AddParmAtOffset(as, OPT_volumeid, "-volumeid", CMD_SINGLE,
			CMD_REQUIRED, "volume id");
    cmd_AddParmAtOffset(as, OPT_partition, "-partition", CMD_SINGLE,
			partition_flags, "partition name");
    cmd_AddParmAtOffset(as, OPT_priority, "-priority", CMD_SINGLE,
			CMD_OPTIONAL, "priority");
}

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    int err = 0;

    /* Initialize directory paths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }


    ts = cmd_CreateSyntax("stats", OpStats, NULL, 0, "get salvageserver statistics (SALVSYNC_NOP opcode)");
    common_parms(ts);
    cmd_CreateAlias(ts, "nop");

    ts = cmd_CreateSyntax("salvage", OpSalvage, NULL, 0, "schedule a salvage (SALVSYNC_SALVAGE opcode)");
    common_parms_salv(ts, 1);

    ts = cmd_CreateSyntax("cancel", OpCancel, NULL, 0, "cancel a salvage (SALVSYNC_CANCEL opcode)");
    common_parms_salv(ts, 0);

    ts = cmd_CreateSyntax("raiseprio", OpRaisePrio, NULL, 0, "raise a salvage priority (SALVSYNC_RAISEPRIO opcode)");
    common_parms_salv(ts, 1);
    cmd_CreateAlias(ts, "rp");

    ts = cmd_CreateSyntax("query", OpQuery, NULL, 0, "query salvage status (SALVSYNC_QUERY opcode)");
    common_parms_salv(ts, 0);
    cmd_CreateAlias(ts, "qry");

    ts = cmd_CreateSyntax("kill", OpCancelAll, NULL, 0, "cancel all scheduled salvages (SALVSYNC_CANCELALL opcode)");
    common_parms(ts);

    err = cmd_Dispatch(argc, argv);
    exit(err);
}

static int
common_prolog(struct cmd_syndesc * as, struct fssync_state * state)
{
    VolumePackageOptions opts;
    char *pt_string = NULL;

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	Exit(1);
    }
#endif

    VOptDefaults(debugUtility, &opts);
    if (VInitVolumePackage2(debugUtility, &opts)) {
	/* VInitVolumePackage2 can fail on e.g. partition attachment errors,
	 * but we don't really care, since all we're doing is trying to use
	 * SALVSYNC */
	fprintf(stderr, "errors encountered initializing volume package, but "
	                "trying to continue anyway\n");
    }
    DInit(1);

    state->sop = calloc(1, sizeof(state->sop[0]));
    assert(state->sop != NULL);

    state->reason = SALVSYNC_REASON_WHATEVER;
    cmd_OptionAsInt(as, OPT_reason, &state->reason);

    if (cmd_OptionAsString(as, OPT_programtype, &pt_string) == 0) {
	if (strcmp(pt_string, "fileServer") == 0) {
	    programType = fileServer;
	} else if (strcmp(pt_string, "volumeUtility") == 0) {
	    programType = volumeUtility;
	} else if (strcmp(pt_string, "salvager") == 0) {
	    programType = salvager;
	} else if (strcmp(pt_string, "salvageServer") == 0) {
	    programType = salvageServer;
	} else if (strcmp(pt_string, "volumeServer") == 0) {
	    programType = volumeServer;
	} else if (strcmp(pt_string, "volumeSalvager") == 0) {
	    programType = volumeSalvager;
	} else {
	    int pt = 0;
	    opr_Verify(cmd_OptionAsInt(as, OPT_programtype, &pt) == 0);
	    programType = pt;
	}
    }

    VConnectSALV();

    free(pt_string);

    return 0;
}

static int
common_salv_prolog(struct cmd_syndesc * as, struct fssync_state * state)
{
    char *part = NULL;

    assert(state->sop != NULL);

    if (cmd_OptionAsUint(as, OPT_volumeid, &state->sop->volume) != 0) {
	fprintf(stderr, "required argument -volumeid not given\n");
    }

    memset(state->sop->partName, 0, sizeof(state->sop->partName));
    if (cmd_OptionAsString(as, OPT_partition, &part) == 0) {
	strlcpy(state->sop->partName, part, sizeof(state->sop->partName));
    }

    state->sop->prio = 0;
    cmd_OptionAsUint(as, OPT_priority, &state->sop->prio);

    free(part);

    return 0;
}

static int
do_salvop(struct fssync_state * state, afs_int32 command, SYNC_response * res)
{
    afs_int32 code;
    SALVSYNC_response_hdr hdr_l, *hdr;
    SYNC_response res_l;

    if (!res) {
	res = &res_l;
	res->payload.len = sizeof(hdr_l);
	res->payload.buf = hdr = &hdr_l;
    } else {
	hdr = (SALVSYNC_response_hdr *) res->payload.buf;
    }

    fprintf(stderr, "calling SALVSYNC_SalvageVolume with command code %d (%s)\n",
	    command, command_code_to_string(command));

    code = SALVSYNC_SalvageVolume(state->sop->volume,
				  state->sop->partName,
				  command,
				  state->reason,
				  state->sop->prio,
				  res);

    switch (code) {
    case SYNC_OK:
    case SYNC_DENIED:
	break;
    default:
	fprintf(stderr, "possible sync protocol error. return code was %d\n", code);
    }

    fprintf(stderr, "SALVSYNC_SalvageVolume returned %d (%s)\n", code, response_code_to_string(code));
    fprintf(stderr, "protocol response code was %d (%s)\n",
	    res->hdr.response, response_code_to_string(res->hdr.response));
    fprintf(stderr, "protocol reason code was %d (%s)\n",
	    res->hdr.reason, reason_code_to_string(res->hdr.reason));

    printf("state = {\n");
    if (res->hdr.flags & SALVSYNC_FLAG_VOL_STATS_VALID) {
	printf("\tstate = %d (%s)\n",
	       hdr->state, state_code_to_string(hdr->state));
	printf("\tprio = %d\n", hdr->prio);
    }
    printf("\tsq_len = %d\n", hdr->sq_len);
    printf("\tpq_len = %d\n", hdr->pq_len);
    printf("}\n");

    VDisconnectSALV();

    return 0;
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
    case SALVSYNC_NOP:
	return "SALVSYNC_NOP";
    case SALVSYNC_SALVAGE:
	return "SALVSYNC_SALVAGE";
    case SALVSYNC_CANCEL:
	return "SALVSYNC_CANCEL";
    case SALVSYNC_RAISEPRIO:
	return "SALVSYNC_RAISEPRIO";
    case SALVSYNC_QUERY:
	return "SALVSYNC_QUERY";
    case SALVSYNC_CANCELALL:
	return "SALVSYNC_CANCELLALL";
    default:
	return "**UNKNOWN**";
    }
}

static char *
reason_code_to_string(afs_int32 reason)
{
    switch (reason) {
    case SALVSYNC_WHATEVER:
	return "SALVSYNC_WHATEVER";
    case SALVSYNC_ERROR:
	return "SALVSYNC_ERROR";
    case SALVSYNC_OPERATOR:
	return "SALVSYNC_OPERATOR";
    case SALVSYNC_SHUTDOWN:
	return "SALVSYNC_SHUTDOWN";
    case SALVSYNC_NEEDED:
	return "SALVSYNC_NEEDED";
    default:
	return "**UNKNOWN**";
    }
}

static char *
state_code_to_string(afs_int32 state)
{
    switch (state) {
    case SALVSYNC_STATE_UNKNOWN:
	return "SALVSYNC_STATE_UNKNOWN";
    case SALVSYNC_STATE_QUEUED:
	return "SALVSYNC_STATE_QUEUED";
    case SALVSYNC_STATE_SALVAGING:
	return "SALVSYNC_STATE_SALVAGING";
    case SALVSYNC_STATE_ERROR:
	return "SALVSYNC_STATE_ERROR";
    case SALVSYNC_STATE_DONE:
	return "SALVSYNC_STATE_DONE";
    default:
	return "**UNKNOWN**";
    }
}

static int
OpStats(struct cmd_syndesc * as, void * rock)
{
    struct fssync_state state;

    common_prolog(as, &state);

    do_salvop(&state, SALVSYNC_NOP, NULL);

    return 0;
}

static int
OpSalvage(struct cmd_syndesc * as, void * rock)
{
    struct fssync_state state;

    common_prolog(as, &state);
    common_salv_prolog(as, &state);

    do_salvop(&state, SALVSYNC_SALVAGE, NULL);

    return 0;
}

static int
OpCancel(struct cmd_syndesc * as, void * rock)
{
    struct fssync_state state;

    common_prolog(as, &state);
    common_salv_prolog(as, &state);

    do_salvop(&state, SALVSYNC_CANCEL, NULL);

    return 0;
}

static int
OpCancelAll(struct cmd_syndesc * as, void * rock)
{
    struct fssync_state state;

    common_prolog(as, &state);

    do_salvop(&state, SALVSYNC_CANCELALL, NULL);

    return 0;
}

static int
OpRaisePrio(struct cmd_syndesc * as, void * rock)
{
    struct fssync_state state;

    common_prolog(as, &state);
    common_salv_prolog(as, &state);

    do_salvop(&state, SALVSYNC_RAISEPRIO, NULL);

    return 0;
}

static int
OpQuery(struct cmd_syndesc * as, void * rock)
{
    struct fssync_state state;

    common_prolog(as, &state);
    common_salv_prolog(as, &state);

    do_salvop(&state, SALVSYNC_QUERY, NULL);

    return 0;
}

#endif /* AFS_DEMAND_ATTACH_FS */
