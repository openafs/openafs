/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * This file implements the bos related funtions for afscp
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "bos.h"

/*
 * Utility functions
 */

/*
 * Generic fuction for converting input string to an integer.  Pass
 * the error_msg you want displayed if there is an error converting
 * the string.
 */

static int
GetIntFromString(const char *int_str, const char *error_msg)
{
    int i;
    char *bad_char = NULL;

    i = strtoul(int_str, &bad_char, 10);
    if ((bad_char == NULL) || (*bad_char == 0)) {
	return i;
    }

    ERR_EXT(error_msg);
}

/*
 * Functions for reading and displaying bos restart times.  These are copied
 * from util/ktime.c and changed to handle the bos types.
 */

struct token {
    struct token *next;
    char *key;
};

static char *day[] = {
    "sun",
    "mon",
    "tue",
    "wed",
    "thu",
    "fri",
    "sat"
};

static
LocalFreeTokens(struct token *alist)
{
    register struct token *nlist;
    for (; alist; alist = nlist) {
	nlist = alist->next;
	free(alist->key);
	free(alist);
    }
    return 0;
}

static
space(int x)
{
    if (x == 0 || x == ' ' || x == '\t' || x == '\n')
	return 1;
    else
	return 0;
}

static
LocalParseLine(char *aline, struct token **alist)
{
    char tbuffer[256];
    register char *tptr;
    int inToken;
    struct token *first, *last;
    register struct token *ttok;
    register int tc;

    inToken = 0;		/* not copying token chars at start */
    first = NULL;
    last = NULL;
    while (1) {
	tc = *aline++;
	if (tc == 0 || space(tc)) {
	    if (inToken) {
		inToken = 0;	/* end of this token */
		*tptr++ = 0;
		ttok = (struct token *)malloc(sizeof(struct token));
		ttok->next = NULL;
		ttok->key = (char *)malloc(strlen(tbuffer) + 1);
		strcpy(ttok->key, tbuffer);
		if (last) {
		    last->next = ttok;
		    last = ttok;
		} else
		    last = ttok;
		if (!first)
		    first = ttok;
	    }
	} else {
	    /* an alpha character */
	    if (!inToken) {
		tptr = tbuffer;
		inToken = 1;
	    }
	    if (tptr - tbuffer >= sizeof(tbuffer))
		return -1;
	    *tptr++ = tc;
	}
	if (tc == 0) {
	    /* last token flushed 'cause space(0) --> true */
	    if (last)
		last->next = NULL;
	    *alist = first;
	    return 0;
	}
    }
}

/* keyword database for periodic date parsing */
static struct ptemp {
    char *key;
    afs_int32 value;
} ptkeys[] = {
"sun", 0x10000, "mon", 0x10001, "tue", 0x10002, "wed", 0x10003, "thu",
	0x10004, "fri", 0x10005, "sat", 0x10006, "sunday", 0x10000,
	"monday", 0x10001, "tuesday", 0x10002, "wednesday", 0x10003,
	"thursday", 0x10004, "thur", 0x10004, "friday", 0x10005,
	"saturday", 0x10006, "am", 0x20000, "pm", 0x20001, "a.m.",
	0x20000, "p.m.", 0x20001, 0, 0,};

static
ParseTime(bos_RestartTime_p ak, char *astr)
{
    int field;
    short temp;
    register char *tp;
    register int tc;

    field = 0;			/* 0=hour, 1=min, 2=sec */
    temp = 0;

    ak->mask |=
	(BOS_RESTART_TIME_HOUR | BOS_RESTART_TIME_MINUTE |
	 BOS_RESTART_TIME_SECOND);
    for (tp = astr;;) {
	tc = *tp++;
	if (tc == 0 || tc == ':') {
	    if (field == 0)
		ak->hour = temp;
	    else if (field == 1)
		ak->min = temp;
	    else if (field == 2)
		ak->sec = temp;
	    temp = 0;
	    field++;
	    if (tc == 0)
		break;
	    continue;
	} else if (!isdigit(tc))
	    return -1;		/* syntax error */
	else {
	    /* digit */
	    temp *= 10;
	    temp += tc - '0';
	}
    }
    if (ak->hour >= 24 || ak->min >= 60 || ak->sec >= 60)
	return -1;
    return 0;
}

int
ktime_ParsePeriodic(char *adate, bos_RestartTime_p ak)
{
    struct token *tt;
    register afs_int32 code;
    struct ptemp *tp;

    memset(ak, 0, sizeof(*ak));
    code = LocalParseLine(adate, &tt);
    if (code)
	return -1;
    for (; tt; tt = tt->next) {
	/* look at each token */
	if (strcmp(tt->key, "now") == 0) {
	    ak->mask |= BOS_RESTART_TIME_NOW;
	    LocalFreeTokens(tt);
	    return 0;
	}
	if (strcmp(tt->key, "never") == 0) {
	    ak->mask |= BOS_RESTART_TIME_NEVER;
	    LocalFreeTokens(tt);
	    return 0;
	}
	if (strcmp(tt->key, "at") == 0)
	    continue;
	if (strcmp(tt->key, "every") == 0)
	    continue;
	if (isdigit(tt->key[0])) {
	    /* parse a time */
	    code = ParseTime(ak, tt->key);
	    if (code) {
		LocalFreeTokens(tt);
		return -1;
	    }
	    continue;
	}
	/* otherwise use keyword table */
	for (tp = ptkeys;; tp++) {
	    if (tp->key == NULL) {
		LocalFreeTokens(tt);
		return -1;
	    }
	    if (strcmp(tp->key, tt->key) == 0)
		break;
	}
	/* now look at tp->value to see what we've got */
	if ((tp->value >> 16) == 1) {
	    /* a day */
	    ak->mask |= BOS_RESTART_TIME_DAY;
	    ak->day = tp->value & 0xff;
	}
	if ((tp->value >> 16) == 2) {
	    /* am or pm token */
	    if ((tp->value & 0xff) == 1) {
		/* pm */
		if (!(ak->mask & BOS_RESTART_TIME_HOUR))
		    return -1;
		if (ak->hour < 12)
		    ak->hour += 12;
		/* 12 is 12 PM */
		else if (ak->hour != 12) {
		    LocalFreeTokens(tt);
		    return -1;
		}
	    } else {
		/* am is almost a noop, except that we map 12:01 am to 0:01 */
		if (ak->hour > 12) {
		    LocalFreeTokens(tt);
		    return -1;
		}
		if (ak->hour == 12)
		    ak->hour = 0;
	    }
	}
    }
    LocalFreeTokens(tt);
    return 0;
}

int
DoBosProcessCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, BINARY, CRON, CRONTIME,
	NOTIFIER
    } DoBosProcessCreate_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    bos_ProcessType_t process_type = BOS_PROCESS_SIMPLE;
    const char *binary = NULL;
    int is_cron = 0;
    const char *cron_time = NULL;
    int has_cron_time = 0;
    const char *notifier = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (as->parms[BINARY].items) {
	binary = as->parms[BINARY].items->data;
    }

    if (as->parms[CRON].items) {
	is_cron = 1;
	process_type = BOS_PROCESS_CRON;
    }

    if (as->parms[CRONTIME].items) {
	cron_time = as->parms[CRONTIME].items->data;
	has_cron_time = 1;
    }

    if (is_cron) {
	if (!has_cron_time) {
	    ERR_EXT("must specify cron time when creating a cron process");
	}
    } else {
	if (has_cron_time) {
	    ERR_EXT("cron time is meaningless for non cron process");
	}
    }

    if (as->parms[NOTIFIER].items) {
	notifier = as->parms[NOTIFIER].items->data;
    }

    if (!bos_ProcessCreate
	(bos_server, process, process_type, binary, cron_time, notifier,
	 &st)) {
	ERR_ST_EXT("bos_ProcessCreate", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosFSProcessCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, FILESERVER, VOLSERVER, SALVAGER,
	NOTIFIER
    } DoBosFSProcessCreate_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    const char *fileserver = NULL;
    const char *volserver = NULL;
    const char *salvager = NULL;
    const char *notifier = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (as->parms[FILESERVER].items) {
	fileserver = as->parms[FILESERVER].items->data;
    }

    if (as->parms[VOLSERVER].items) {
	volserver = as->parms[VOLSERVER].items->data;
    }

    if (as->parms[SALVAGER].items) {
	salvager = as->parms[SALVAGER].items->data;
    }

    if (as->parms[NOTIFIER].items) {
	notifier = as->parms[NOTIFIER].items->data;
    }

    if (!bos_FSProcessCreate
	(bos_server, process, fileserver, volserver, salvager, notifier,
	 &st)) {
	ERR_ST_EXT("bos_FSProcessCreate", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoBosProcessDelete_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (!bos_ProcessDelete(bos_server, process, &st)) {
	ERR_ST_EXT("bos_ProcessDelete", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

static void
Print_bos_ProcessExecutionState_p(bos_ProcessExecutionState_p state,
				  const char *prefix)
{
    printf("%sThe process executation state is: ", prefix);
    switch (*state) {
    case BOS_PROCESS_STOPPED:
	printf("stopped\n");
	break;
    case BOS_PROCESS_RUNNING:
	printf("running\n");
	break;
    case BOS_PROCESS_STOPPING:
	printf("stopping\n");
	break;
    case BOS_PROCESS_STARTING:
	printf("starting\n");
	break;
    }
}

int
DoBosProcessExecutionStateGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoBosProcessExecutionStateGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    bos_ProcessExecutionState_t state;
    char aux_status[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (!bos_ProcessExecutionStateGet
	(bos_server, process, &state, aux_status, &st)) {
	ERR_ST_EXT("bos_ProcessExecutionStateGet", st);
    }

    Print_bos_ProcessExecutionState_p(&state, "");
    if (aux_status[0] != 0) {
	printf("Aux process status: %s\n", aux_status);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessExecutionStateSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, STOPPED,
	RUNNING
    } DoBosProcessExecutionStateSet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    int stop = 0;
    int run = 0;
    bos_ProcessExecutionState_t state;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (as->parms[STOPPED].items) {
	stop = 1;
	state = BOS_PROCESS_STOPPED;
    }

    if (as->parms[RUNNING].items) {
	run = 1;
	state = BOS_PROCESS_RUNNING;
    }

    if ((stop == 1) && (run == 1)) {
	ERR_EXT("you must specify either running or stopped, but not both");
    }

    if ((stop == 0) && (run == 0)) {
	ERR_EXT("you must specify either running or stopped");
    }

    if (!bos_ProcessExecutionStateSet(bos_server, process, state, &st)) {
	ERR_ST_EXT("bos_ProcessExecutionStateSet", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessExecutionStateSetTemporary(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS, STOPPED,
	RUNNING
    } DoBosProcessExecutionStateSetTemporary_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    int stop = 0;
    int run = 0;
    bos_ProcessExecutionState_t state;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (as->parms[STOPPED].items) {
	stop = 1;
	state = BOS_PROCESS_STOPPED;
    }

    if (as->parms[RUNNING].items) {
	run = 1;
	state = BOS_PROCESS_RUNNING;
    }

    if ((stop == 1) && (run == 1)) {
	ERR_EXT("you must specify either running or stopped, but not both");
    }

    if ((stop == 0) && (run == 0)) {
	ERR_EXT("you must specify either running or stopped");
    }

    if (!bos_ProcessExecutionStateSetTemporary
	(bos_server, process, state, &st)) {
	ERR_ST_EXT("bos_ProcessExecutionStateSetTemporary", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessNameList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosProcessNameList_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    void *iter = NULL;
    char process[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_ProcessNameGetBegin(bos_server, &iter, &st)) {
	ERR_ST_EXT("bos_ProcessNameGetBegin", st);
    }

    printf("Listing processes at server %s:\n",
	   as->parms[SERVER].items->data);

    while (bos_ProcessNameGetNext(iter, process, &st)) {
	printf("\t%s\n", process);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("bos_ProcessNameGetNext", st);
    }

    if (!bos_ProcessNameGetDone(iter, &st)) {
	ERR_ST_EXT("bos_ProcessNameGetDone", st);
    }

    return 0;
}

static void
Print_bos_ProcessType_p(bos_ProcessType_p type, const char *prefix)
{
    printf("%sProcess type: \n", prefix);
    switch (*type) {
    case BOS_PROCESS_SIMPLE:
	printf("simple\n");
	break;
    case BOS_PROCESS_FS:
	printf("fs\n");
	break;
    case BOS_PROCESS_CRON:
	printf("cron\n");
	break;
    }
}

static void
Print_bos_ProcessState_p(bos_ProcessState_p state, const char *prefix)
{
    printf("%sProcess state:\n", prefix);
    /* FIXME: BOS_PROCESS_OK is 0, so this test is not right */
    if (*state & BOS_PROCESS_OK) {
	printf("%s\tBOS_PROCESS_OK:\n", prefix);
    }
    if (*state & BOS_PROCESS_CORE_DUMPED) {
	printf("%s\tBOS_PROCESS_CORE_DUMPED:\n", prefix);
    }
    if (*state & BOS_PROCESS_TOO_MANY_ERRORS) {
	printf("%s\tBOS_PROCESS_TOO_MANY_ERRORS:\n", prefix);
    }
    if (*state & BOS_PROCESS_BAD_FILE_ACCESS) {
	printf("%s\tBOS_PROCESS_BAD_FILE_ACCESS:\n", prefix);
    }
}

static void
Print_bos_ProcessInfo_p(bos_ProcessInfo_p info, const char *prefix)
{
    Print_bos_ProcessExecutionState_p(&info->processGoal, prefix);
    printf("%sStart time %lu\n", prefix, info->processStartTime);
    printf("%sNumber of process starts %lu \n", prefix,
	   info->numberProcessStarts);
    printf("%sProcess exit time %lu\n", prefix, info->processExitTime);
    printf("%sProcess exit error time %lu\n", prefix,
	   info->processExitErrorTime);
    printf("%sProcess error code %lu\n", prefix, info->processErrorCode);
    printf("%sProcess error signal %lu\n", prefix, info->processErrorSignal);
    Print_bos_ProcessState_p(&info->state, prefix);
}

int
DoBosProcessInfoGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoBosProcessInfoGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    bos_ProcessType_t type;
    bos_ProcessInfo_t info;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (!bos_ProcessInfoGet(bos_server, process, &type, &info, &st)) {
	ERR_ST_EXT("bos_ProcessInfoGet", st);
    }

    Print_bos_ProcessType_p(&type, "");
    Print_bos_ProcessInfo_p(&info, "");

    return 0;
}

int
DoBosProcessParameterList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoBosProcessParameterList_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    char *process = NULL;
    void *iter = NULL;
    char parameter[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (!bos_ProcessParameterGetBegin(bos_server, process, &iter, &st)) {
	ERR_ST_EXT("bos_ProcessParameterGetBegin", st);
    }

    printf("Getting parameters for %s\n", process);

    while (bos_ProcessParameterGetNext(iter, parameter, &st)) {
	printf("\t%s\n", parameter);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("bos_ProcessParameterGetNext", st);
    }

    if (!bos_ProcessParameterGetDone(iter, &st)) {
	ERR_ST_EXT("bos_ProcessParameterGetDone", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessNotifierGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoBosProcessNotifierGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;
    char notifier[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (!bos_ProcessNotifierGet(bos_server, process, notifier, &st)) {
	ERR_ST_EXT("bos_ProcessNotifierGet", st);
    }

    if (notifier[0] == 0) {
	printf("%s does not have a notifier.\n", process);
    } else {
	printf("The notifier for %s is %s\n", process, notifier);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessRestart(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PROCESS } DoBosProcessRestart_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *process = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PROCESS].items) {
	process = as->parms[PROCESS].items->data;
    }

    if (!bos_ProcessRestart(bos_server, process, &st)) {
	ERR_ST_EXT("bos_ProcessRestart", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessAllStop(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosProcessAllStop_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_ProcessAllStop(bos_server, &st)) {
	ERR_ST_EXT("bos_ProcessAllStop", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessAllStart(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosProcessAllStart_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_ProcessAllStart(bos_server, &st)) {
	ERR_ST_EXT("bos_ProcessAllStart", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessAllWaitStop(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosProcessAllWaitStop_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_ProcessAllWaitStop(bos_server, &st)) {
	ERR_ST_EXT("bos_ProcessAllWaitStop", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessAllWaitTransition(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosProcessAllWaitTransition_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_ProcessAllWaitTransition(bos_server, &st)) {
	ERR_ST_EXT("bos_ProcessAllWaitTransition", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosProcessAllStopAndRestart(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, INCLUDEBOS } DoBosProcessAllStopAndRestart_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    bos_RestartBosServer_t restart = BOS_DONT_RESTART_BOS_SERVER;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[INCLUDEBOS].items) {
	restart = BOS_RESTART_BOS_SERVER;
    }

    if (!bos_ProcessAllStopAndRestart(bos_server, restart, &st)) {
	ERR_ST_EXT("bos_ProcessAllStopAndRestart", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosAdminCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, ADMIN } DoBosAdminCreate_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *admin;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[ADMIN].items) {
	admin = as->parms[ADMIN].items->data;
    }

    if (!bos_AdminCreate(bos_server, admin, &st)) {
	ERR_ST_EXT("bos_AdminCreate", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosAdminDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, ADMIN } DoBosAdminDelete_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *admin;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[ADMIN].items) {
	admin = as->parms[ADMIN].items->data;
    }

    if (!bos_AdminDelete(bos_server, admin, &st)) {
	ERR_ST_EXT("bos_AdminDelete", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosAdminList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosAdminList_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    void *iter = NULL;
    char admin[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_AdminGetBegin(bos_server, &iter, &st)) {
	ERR_ST_EXT("bos_AdminGetBegin", st);
    }

    printf("Administrators at %s\n", as->parms[SERVER].items->data);

    while (bos_AdminGetNext(iter, admin, &st)) {
	printf("%s\n", admin);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("bos_AdminGetNext", st);
    }

    if (!bos_AdminGetDone(iter, &st)) {
	ERR_ST_EXT("bos_AdminGetDone", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosKeyCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, VERSIONNUMBER, KEY } DoBosKeyCreate_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    int version_number;
    kas_encryptionKey_t key = { {0, 0, 0, 0, 0, 0, 0, 0} };
    const char *cell;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[VERSIONNUMBER].items) {
	version_number =
	    GetIntFromString(as->parms[VERSIONNUMBER].items->data,
			     "invalid version number");
    }

    if (as->parms[KEY].items) {
	const char *str = as->parms[KEY].items->data;
	if (!afsclient_CellNameGet(cellHandle, &cell, &st)) {
	    ERR_ST_EXT("afsclient_CellNameGet", st);
	}
	if (!kas_StringToKey(cell, str, &key, &st)) {
	    ERR_ST_EXT("kas_StringToKey", st);
	}
    }

    if (!bos_KeyCreate(bos_server, version_number, &key, &st)) {
	ERR_ST_EXT("bos_KeyCreate", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosKeyDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, VERSIONNUMBER } DoBosKeyDelete_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    int version_number;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[VERSIONNUMBER].items) {
	version_number =
	    GetIntFromString(as->parms[VERSIONNUMBER].items->data,
			     "invalid version number");
    }

    if (!bos_KeyDelete(bos_server, version_number, &st)) {
	ERR_ST_EXT("bos_KeyDelete", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

static void
Print_bos_KeyInfo_p(bos_KeyInfo_p key, const char *prefix)
{
    int i;
    printf("%sVersion number: %d\n", prefix, key->keyVersionNumber);
    printf("%sLast modification date %d\n", prefix,
	   key->keyStatus.lastModificationDate);
    printf("%sLast modification micro seconds %d\n", prefix,
	   key->keyStatus.lastModificationMicroSeconds);
    printf("%sChecksum %u\n", prefix, key->keyStatus.checkSum);

    printf("%sKey: \n", prefix);
    for (i = 0; i < KAS_ENCRYPTION_KEY_LEN; i++) {
	printf("%s\t%d ", prefix, key->key.key[i]);
    }
    printf("\n");
}

int
DoBosKeyList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosKeyList_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    void *iter = NULL;
    bos_KeyInfo_t key;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_KeyGetBegin(bos_server, &iter, &st)) {
	ERR_ST_EXT("bos_KeyGetBegin", st);
    }

    printf("Listing keys at server %s:\n", as->parms[SERVER].items->data);

    while (bos_KeyGetNext(iter, &key, &st)) {
	Print_bos_KeyInfo_p(&key, "");
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("bos_KeyGetNext", st);
    }

    if (!bos_KeyGetDone(iter, &st)) {
	ERR_ST_EXT("bos_KeyGetDone", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosCellSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, CELL } DoBosCellSet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *cell;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[SERVER].items) {
	cell = as->parms[SERVER].items->data;
    }

    if (!bos_CellSet(bos_server, cell, &st)) {
	ERR_ST_EXT("bos_CellSet", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosCellGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosCellGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    char cell[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_CellGet(bos_server, cell, &st)) {
	ERR_ST_EXT("bos_CellGet", st);
    }

    printf("The cell name is %s\n", cell);

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosHostCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, HOST } DoBosHostCreate_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *host;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[HOST].items) {
	host = as->parms[HOST].items->data;
    }

    if (!bos_HostCreate(bos_server, host, &st)) {
	ERR_ST_EXT("bos_HostCreate", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosHostDelete(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, HOST } DoBosHostDelete_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *host;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[HOST].items) {
	host = as->parms[HOST].items->data;
    }

    if (!bos_HostDelete(bos_server, host, &st)) {
	ERR_ST_EXT("bos_HostDelete", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosHostList(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER } DoBosHostList_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    void *iter = NULL;
    char host[BOS_MAX_NAME_LEN];

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (!bos_HostGetBegin(bos_server, &iter, &st)) {
	ERR_ST_EXT("bos_HostGetBegin", st);
    }

    printf("Listing hosts at server %s\n", as->parms[SERVER].items->data);

    while (bos_HostGetNext(iter, host, &st)) {
	printf("\t%s\n", host);
    }

    if (st != ADMITERATORDONE) {
	ERR_ST_EXT("bos_HostGetNext", st);
    }

    if (!bos_HostGetDone(iter, &st)) {
	ERR_ST_EXT("bos_HostGetDone", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosExecutableCreate(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, BINARY, DEST } DoBosExecutableCreate_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *binary = NULL;
    const char *dest = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[BINARY].items) {
	binary = as->parms[BINARY].items->data;
    }

    if (as->parms[DEST].items) {
	dest = as->parms[DEST].items->data;
    }

    if (!bos_ExecutableCreate(bos_server, binary, dest, &st)) {
	ERR_ST_EXT("bos_ExecutableCreate", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosExecutableRevert(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, EXECUTABLE } DoBosExecutableRevert_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *executable = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[EXECUTABLE].items) {
	executable = as->parms[EXECUTABLE].items->data;
    }

    if (!bos_ExecutableRevert(bos_server, executable, &st)) {
	ERR_ST_EXT("bos_ExecutableRevert", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosExecutableTimestampGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, EXECUTABLE } DoBosExecutableTimestampGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *executable = NULL;
    unsigned long new_time = 0;
    unsigned long old_time = 0;
    unsigned long bak_time = 0;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[EXECUTABLE].items) {
	executable = as->parms[EXECUTABLE].items->data;
    }

    if (!bos_ExecutableTimestampGet
	(bos_server, executable, &new_time, &old_time, &bak_time, &st)) {
	ERR_ST_EXT("bos_ExecutableTimestampGet", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosExecutablePrune(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, OLDFILES, BAKFILES,
	COREFILES
    } DoBosExecutablePrune_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    bos_Prune_t old_files = BOS_DONT_PRUNE;
    bos_Prune_t bak_files = BOS_DONT_PRUNE;
    bos_Prune_t core_files = BOS_DONT_PRUNE;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[OLDFILES].items) {
	old_files = BOS_PRUNE;
    }

    if (as->parms[BAKFILES].items) {
	bak_files = BOS_PRUNE;
    }

    if (as->parms[COREFILES].items) {
	core_files = BOS_PRUNE;
    }

    if (!bos_ExecutablePrune
	(bos_server, old_files, bak_files, core_files, &st)) {
	ERR_ST_EXT("bos_ExecutablePrune", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosExecutableRestartTimeSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, DAILY, WEEKLY,
	TIME
    } DoBosExecutableRestartTimeSet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    bos_Restart_t type;
    int have_daily = 0;
    int have_weekly = 0;
    bos_RestartTime_t time;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[DAILY].items) {
	type = BOS_RESTART_DAILY;
	have_daily = 1;
    }

    if (as->parms[WEEKLY].items) {
	type = BOS_RESTART_WEEKLY;
	have_weekly = 1;
    }

    if ((have_daily == 0) && (have_weekly == 0)) {
	ERR_EXT("must specify either daily or weekly");
    }

    if ((have_daily == 1) && (have_weekly == 1)) {
	ERR_EXT("must specify either daily or weekly, not both");
    }

    if (as->parms[TIME].items) {
	if (ktime_ParsePeriodic(as->parms[TIME].items->data, &time) == -1) {
	    ERR_EXT("error parsing time");
	}
    }

    if (!bos_ExecutableRestartTimeSet(bos_server, type, time, &st)) {
	ERR_ST_EXT("bos_ExecutableRestartTimeSet", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

static void
Print_bos_RestartTime_p(bos_RestartTime_p restart, const char *prefix)
{
    char tempString[50];
    char astring[50];

    astring[0] = 0;
    tempString[0] = 0;

    if (restart->mask & BOS_RESTART_TIME_NEVER) {
	printf("%snever\n", prefix);
    } else if (restart->mask & BOS_RESTART_TIME_NOW) {
	printf("%snow\n", prefix);
    } else {
	strcpy(astring, "at");
	if (restart->mask & BOS_RESTART_TIME_DAY) {
	    strcat(astring, " ");
	    strcat(astring, day[restart->day]);
	}
	if (restart->mask & BOS_RESTART_TIME_HOUR) {
	    if (restart->hour > 12)
		sprintf(tempString, " %d", restart->hour - 12);
	    else if (restart->hour == 0)
		strcpy(tempString, " 12");
	    else
		sprintf(tempString, " %d", restart->hour);
	    strcat(astring, tempString);
	}
	if (restart->mask & BOS_RESTART_TIME_MINUTE) {
	    sprintf(tempString, ":%02d", restart->min);
	    strcat(astring, tempString);
	}
	if ((restart->mask & BOS_RESTART_TIME_SECOND) && restart->sec != 0) {
	    sprintf(tempString, ":%02d", restart->sec);
	    strcat(astring, tempString);
	}
	if (restart->mask & BOS_RESTART_TIME_HOUR) {
	    if (restart->hour >= 12)
		strcat(astring, " pm");
	    else
		strcat(astring, " am");
	}
	printf("%s%s\n", prefix, astring);
    }
}

int
DoBosExecutableRestartTimeGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, DAILY,
	WEEKLY
    } DoBosExecutableRestartTimeGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    bos_Restart_t type;
    int have_daily = 0;
    int have_weekly = 0;
    bos_RestartTime_t restart_time;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[DAILY].items) {
	type = BOS_RESTART_DAILY;
	have_daily = 1;
    }

    if (as->parms[WEEKLY].items) {
	type = BOS_RESTART_WEEKLY;
	have_weekly = 1;
    }

    if ((have_daily == 0) && (have_weekly == 0)) {
	ERR_EXT("must specify either daily or weekly");
    }

    if ((have_daily == 1) && (have_weekly == 1)) {
	ERR_EXT("must specify either daily or weekly, not both");
    }

    if (!bos_ExecutableRestartTimeGet(bos_server, type, &restart_time, &st)) {
	ERR_ST_EXT("bos_ExecutableRestartTimeGet", st);
    }

    Print_bos_RestartTime_p(&restart_time, "");

    bos_ServerClose(bos_server, 0);

    return 0;
}

#define INITIAL_BUF_SIZE 4096

int
DoBosLogGet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, LOGFILE } DoBosLogGet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *log_file;
    unsigned long buf_size = INITIAL_BUF_SIZE;
    char *buf = NULL;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[LOGFILE].items) {
	log_file = as->parms[LOGFILE].items->data;
    }

    st = ADMMOREDATA;
    while (st == ADMMOREDATA) {
	buf = realloc(buf, buf_size);
	if (buf == NULL) {
	    ERR_EXT("cannot dynamically allocate memory");
	}
	bos_LogGet(bos_server, log_file, &buf_size, buf, &st);
	if (st == ADMMOREDATA) {
	    buf_size = buf_size + (unsigned long)(0.2 * buf_size);
	}
    }

    if (st != 0) {
	ERR_ST_EXT("bos_LogGet", st);
    } else {
	printf("Log file:\n%s", buf);
    }

    if (buf != NULL) {
	free(buf);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosAuthSet(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, REQUIREAUTH, DISABLEAUTH } DoBosAuthSet_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    bos_Auth_t auth;
    int have_req = 0;
    int have_dis = 0;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[REQUIREAUTH].items) {
	auth = BOS_AUTH_REQUIRED;
	have_req = 1;
    }

    if (as->parms[DISABLEAUTH].items) {
	auth = BOS_NO_AUTH;
	have_dis = 1;
    }

    if ((have_req == 0) && (have_dis == 0)) {
	ERR_EXT("must specify either requireauth or disableauth");
    }

    if ((have_req == 1) && (have_dis == 1)) {
	ERR_EXT("must specify either requireauth or disableauth, not both");
    }

    if (!bos_AuthSet(bos_server, auth, &st)) {
	ERR_ST_EXT("bos_AuthSet", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosCommandExecute(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, COMMAND } DoBosCommandExecute_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *command;

    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[COMMAND].items) {
	command = as->parms[COMMAND].items->data;
    }

    if (!bos_CommandExecute(bos_server, command, &st)) {
	ERR_ST_EXT("bos_CommandExecute", st);
    }

    bos_ServerClose(bos_server, 0);

    return 0;
}

int
DoBosSalvage(struct cmd_syndesc *as, void *arock)
{
    typedef enum { SERVER, PARTITION, VOLUME, NUMSALVAGERS, TMPDIR, LOGFILE,
	FORCE, NOWRITE, INODES, ROOTINODES, SALVAGEDIRS, BLOCKREADS
    } DoBosSalvage_parm_t;
    afs_status_t st = 0;
    void *bos_server = NULL;
    const char *partition = NULL;
    const char *volume = NULL;
    int num_salvagers = 1;
    const char *tmp_dir = NULL;
    const char *log_file = NULL;
    vos_force_t force = VOS_NORMAL;
    bos_SalvageDamagedVolumes_t no_write = BOS_SALVAGE_DAMAGED_VOLUMES;
    bos_WriteInodes_t inodes = BOS_SALVAGE_WRITE_INODES;
    bos_WriteRootInodes_t root_inodes = BOS_SALVAGE_WRITE_ROOT_INODES;
    bos_ForceDirectory_t salvage_dirs = BOS_SALVAGE_DONT_FORCE_DIRECTORIES;
    bos_ForceBlockRead_t block_reads = BOS_SALVAGE_DONT_FORCE_BLOCK_READS;


    if (as->parms[SERVER].items) {
	if (!bos_ServerOpen
	    (cellHandle, as->parms[SERVER].items->data, &bos_server, &st)) {
	    ERR_ST_EXT("bos_ServerOpen", st);
	}
    }

    if (as->parms[PARTITION].items) {
	partition = as->parms[PARTITION].items->data;
    }

    if (as->parms[VOLUME].items) {
	volume = as->parms[VOLUME].items->data;
    }

    if (as->parms[NUMSALVAGERS].items) {
	num_salvagers =
	    GetIntFromString(as->parms[NUMSALVAGERS].items->data,
			     "invalid number of salvagers");
    }

    if (as->parms[TMPDIR].items) {
	tmp_dir = as->parms[TMPDIR].items->data;
    }

    if (as->parms[LOGFILE].items) {
	log_file = as->parms[LOGFILE].items->data;
    }

    if (as->parms[FORCE].items) {
	force = VOS_FORCE;
    }

    if (as->parms[NOWRITE].items) {
	no_write = BOS_DONT_SALVAGE_DAMAGED_VOLUMES;
    }

    if (as->parms[INODES].items) {
	inodes = BOS_SALVAGE_DONT_WRITE_INODES;
    }

    if (as->parms[ROOTINODES].items) {
	root_inodes = BOS_SALVAGE_DONT_WRITE_ROOT_INODES;
    }

    if (as->parms[SALVAGEDIRS].items) {
	salvage_dirs = BOS_SALVAGE_FORCE_DIRECTORIES;
    }

    if (as->parms[BLOCKREADS].items) {
	block_reads = BOS_SALVAGE_FORCE_BLOCK_READS;
    }

    if (!bos_Salvage
	(cellHandle, bos_server, partition, volume, num_salvagers, tmp_dir,
	 log_file, force, no_write, inodes, root_inodes, salvage_dirs,
	 block_reads, &st)) {
	ERR_ST_EXT("bos_Salvage", st);
    }

    bos_ServerClose(bos_server, 0);

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

void
SetupBosAdminCmd(void)
{
    struct cmd_syndesc *ts;

    ts = cmd_CreateSyntax("BosProcessCreate", DoBosProcessCreate, NULL,
			  "create a new bos process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process will be created");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    cmd_AddParm(ts, "-binary", CMD_SINGLE, CMD_REQUIRED,
		"path to the process binary");
    cmd_AddParm(ts, "-cron", CMD_FLAG, CMD_OPTIONAL,
		"this is a cron process");
    cmd_AddParm(ts, "-crontime", CMD_SINGLE, CMD_OPTIONAL,
		"the time when the process will be run");
    cmd_AddParm(ts, "-notifier", CMD_SINGLE, CMD_OPTIONAL,
		"path to notifier binary that is run when process terminates");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosFSProcessCreate", DoBosFSProcessCreate, NULL,
			  "create a fs bos process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process will be created");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    cmd_AddParm(ts, "-fileserver", CMD_SINGLE, CMD_REQUIRED,
		"path to the fileserver binary");
    cmd_AddParm(ts, "-volserver", CMD_SINGLE, CMD_REQUIRED,
		"path to the volserver binary");
    cmd_AddParm(ts, "-salvager", CMD_SINGLE, CMD_REQUIRED,
		"path to the salvager binary");
    cmd_AddParm(ts, "-notifier", CMD_SINGLE, CMD_OPTIONAL,
		"path to notifier binary that is run when process terminates");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessDelete", DoBosProcessDelete, NULL,
			  "delete a bos process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process will be deleted");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessExecutionStateGet",
			  DoBosProcessExecutionStateGet, NULL,
			  "get the process execution state of a process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessExecutionStateSet",
			  DoBosProcessExecutionStateSet, NULL,
			  "set the process execution state of a process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    cmd_AddParm(ts, "-stopped", CMD_FLAG, CMD_OPTIONAL,
		"set the process state to stopped");
    cmd_AddParm(ts, "-running", CMD_FLAG, CMD_OPTIONAL,
		"set the process state to running");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessExecutionStateSetTemporary",
			  DoBosProcessExecutionStateSetTemporary, NULL,
			  "set the process execution state "
			  "of a process temporarily");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    cmd_AddParm(ts, "-stopped", CMD_FLAG, CMD_OPTIONAL,
		"set the process state to stopped");
    cmd_AddParm(ts, "-running", CMD_FLAG, CMD_OPTIONAL,
		"set the process state to running");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessNameList", DoBosProcessNameList, NULL,
			  "list the names of all processes at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessInfoGet", DoBosProcessInfoGet, NULL,
			  "get information about a process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessParameterList",
			  DoBosProcessParameterList, NULL,
			  "list the parameters of a process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessNotifierGet", DoBosProcessNotifierGet, NULL,
			  "get the notifier for a process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessRestart", DoBosProcessRestart, NULL,
			  "restart a process");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where process exists");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED,
		"the name of the process");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessAllStop", DoBosProcessAllStop, NULL,
			  "stop all processes at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where processes exists");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessAllWaitStop", DoBosProcessAllWaitStop, NULL,
			  "stop all processes at a bos server and block "
			  "until they all exit");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where processes exists");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessAllWaitTransition",
			  DoBosProcessAllWaitTransition, NULL,
			  "wait until all processes have transitioned to "
			  "their desired state");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where processes exists");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosProcessAllStopAndRestart",
			  DoBosProcessAllStopAndRestart, NULL,
			  "stop all processes at a bos server and "
			  "then restart them");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where processes exists");
    cmd_AddParm(ts, "-includebos", CMD_FLAG, CMD_OPTIONAL,
		"include the bos server in the processes to be restarted");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosAdminCreate", DoBosAdminCreate, NULL,
			  "create an admin user at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where admin will be created");
    cmd_AddParm(ts, "-admin", CMD_SINGLE, CMD_REQUIRED,
		"the name of the administrator to add");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosAdminDelete", DoBosAdminDelete, NULL,
			  "delete an admin user at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where admin will be deleted");
    cmd_AddParm(ts, "-admin", CMD_SINGLE, CMD_REQUIRED,
		"the name of the administrator to delete");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosAdminList", DoBosAdminList, NULL,
			  "list all admin users at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where admins will be listed");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosKeyCreate", DoBosKeyCreate, NULL,
			  "create a key at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where key will be created");
    cmd_AddParm(ts, "-versionnumber", CMD_SINGLE, CMD_REQUIRED,
		"version number of new key");
    cmd_AddParm(ts, "-key", CMD_SINGLE, CMD_REQUIRED, "new encryption key");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosKeyDelete", DoBosKeyDelete, NULL,
			  "delete a key at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where key will be deleted");
    cmd_AddParm(ts, "-versionnumber", CMD_SINGLE, CMD_REQUIRED,
		"version number of the key");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosKeyList", DoBosKeyList, NULL,
			  "list keys at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where keys exist");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosCellSet", DoBosCellSet, NULL,
			  "set the cell at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_REQUIRED, "new cell");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosCellGet", DoBosCellGet, NULL,
			  "get the cell at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosHostCreate", DoBosHostCreate, NULL,
			  "add a host entry to the server CellServDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-host", CMD_SINGLE, CMD_REQUIRED, "host to add");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosHostDelete", DoBosHostDelete, NULL,
			  "delete a host entry from the server CellServDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-host", CMD_SINGLE, CMD_REQUIRED, "host to delete");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosHostList", DoBosHostList, NULL,
			  "list all host entries from the server CellServDB");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosExecutableCreate", DoBosExecutableCreate, NULL,
			  "create a new binary at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-binary", CMD_SINGLE, CMD_REQUIRED,
		"path to the binary to create");
    cmd_AddParm(ts, "-dest", CMD_SINGLE, CMD_REQUIRED,
		"path where the binary will be stored");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosExecutableRevert", DoBosExecutableRevert, NULL,
			  "revert a binary at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-executable", CMD_SINGLE, CMD_REQUIRED,
		"path to the binary to revert");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosExecutableTimestampGet",
			  DoBosExecutableTimestampGet, NULL,
			  "get the timestamps for a binary at bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    cmd_AddParm(ts, "-executable", CMD_SINGLE, CMD_REQUIRED,
		"path to the binary to revert");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosExecutablePrune", DoBosExecutablePrune, NULL,
			  "prune various files at bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-oldfiles", CMD_FLAG, CMD_OPTIONAL, "prune .old files");
    cmd_AddParm(ts, "-bakfiles", CMD_FLAG, CMD_OPTIONAL, "prune .bak files");
    cmd_AddParm(ts, "-corefiles", CMD_FLAG, CMD_OPTIONAL, "prune core files");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosExecutableRestartTimeSet",
			  DoBosExecutableRestartTimeSet, NULL,
			  "set the restart times at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-daily", CMD_FLAG, CMD_OPTIONAL,
		"set daily restart time");
    cmd_AddParm(ts, "-weekly", CMD_FLAG, CMD_OPTIONAL,
		"set weekly restart time");
    cmd_AddParm(ts, "-time", CMD_SINGLE, CMD_REQUIRED,
		"the new restart time");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosExecutableRestartTimeGet",
			  DoBosExecutableRestartTimeGet, NULL,
			  "get the restart times at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    cmd_AddParm(ts, "-daily", CMD_FLAG, CMD_OPTIONAL,
		"get daily restart time");
    cmd_AddParm(ts, "-weekly", CMD_FLAG, CMD_OPTIONAL,
		"get weekly restart time");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosLogGet", DoBosLogGet, NULL,
			  "get a log file from the bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to query");
    cmd_AddParm(ts, "-logfile", CMD_SINGLE, CMD_REQUIRED,
		"path to the log file to retrieve");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosAuthSet", DoBosAuthSet, NULL,
			  "set the authorization level at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "server to modify");
    cmd_AddParm(ts, "-requireauth", CMD_FLAG, CMD_OPTIONAL,
		"require authorization");
    cmd_AddParm(ts, "-disableauth", CMD_FLAG, CMD_OPTIONAL,
		"don't require authorization");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosCommandExecute", DoBosCommandExecute, 0,
			  "execute a command at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where command will execute");
    cmd_AddParm(ts, "-command", CMD_SINGLE, CMD_REQUIRED,
		"command to execute");
    SetupCommonCmdArgs(ts);

    ts = cmd_CreateSyntax("BosSalvage", DoBosSalvage, NULL,
			  "execute a salvage command at a bos server");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"server where salvager will execute");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"partition to salvage");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_OPTIONAL, "volume to salvage");
    cmd_AddParm(ts, "-numsalvagers", CMD_SINGLE, CMD_REQUIRED,
		"number of salvagers to run in parallel");
    cmd_AddParm(ts, "-tmpdir", CMD_SINGLE, CMD_OPTIONAL,
		"directory to place temporary files");
    cmd_AddParm(ts, "-logfile", CMD_SINGLE, CMD_OPTIONAL,
		"file where salvager log will be written");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL, "run salvager -force");
    cmd_AddParm(ts, "-nowrite", CMD_FLAG, CMD_OPTIONAL,
		"run salvager -nowrite");
    cmd_AddParm(ts, "-inodes", CMD_FLAG, CMD_OPTIONAL,
		"run salvager -inodes");
    cmd_AddParm(ts, "-rootinodes", CMD_FLAG, CMD_OPTIONAL,
		"run salvager -rootinodes");
    cmd_AddParm(ts, "-salvagedirs", CMD_FLAG, CMD_OPTIONAL,
		"run salvager -salvagedirs");
    cmd_AddParm(ts, "-blockreads", CMD_FLAG, CMD_OPTIONAL,
		"run salvager -blockreads");
    SetupCommonCmdArgs(ts);
}
