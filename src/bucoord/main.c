/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>


#include <afs/cmd.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <lwp.h>
#include <afs/bubasics.h>
#include <afs/afsutil.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/cmd.h>
#include <rx/rxkad.h>
#include <afs/afsint.h>
#include <afs/volser.h>		/*VLDB_MAXSERVERS */
#include <afs/com_err.h>
#include <afs/afs_lock.h>
#include <afs/budb.h>
#include <afs/kautils.h>
#include <afs/vlserver.h>
#include <afs/butm.h>
#include <afs/butx.h>
#include <afs/tcdata.h>

#include "bc.h"			/*Backup Coordinator structs and defs */
#include "bucoord_internal.h"
#include "bucoord_prototypes.h"

int localauth, interact, nobutcauth;
char tcell[64];

/*
 * Global configuration information for the Backup Coordinator.
 */
extern struct bc_config *bc_globalConfig;	/*Ptr to global BC configuration info */

extern struct ubik_client *cstruct;	/* Ptr to Ubik client structure */
time_t tokenExpires;		/* The token's expiration time */

static const char *DefaultConfDir;	/*Default backup config directory */
static int bcInit = 0;		/* backupInit called yet ? */
char *whoami = "backup";

/* dummy routine for the audit work.  It should do nothing since audits */
/* occur at the server level and bos is not a server. */
int
osi_audit(void)
{
    return 0;
}

/*
 * Initialize all the error tables that may be used by com_err
 * in this module.
 */
void
InitErrTabs(void)
{
    initialize_ACFG_error_table();
    initialize_KA_error_table();
    initialize_RXK_error_table();
    initialize_CMD_error_table();
    initialize_VL_error_table();
    initialize_BUTM_error_table();
    initialize_VOLS_error_table();
    initialize_BUTC_error_table();
    initialize_BUTX_error_table();
    initialize_BUDB_error_table();
    initialize_BUCD_error_table();
    initialize_KTC_error_table();
}

/*
 * got to account for the errors which are volume related but
 * not dealt with by standard errno and com_err stuff.
 */
void
bc_HandleMisc(afs_int32 code)
{
    if (((code <= VMOVED) && (code >= VSALVAGE)) || (code < 0)) {
	switch (code) {
	case -1:
	    fprintf(STDERR, "Possible communication failure\n");
	    break;
	case VSALVAGE:
	    fprintf(STDERR, "Volume needs salvage\n");
	    break;
	case VNOVNODE:
	    fprintf(STDERR, "Bad vnode number quoted\n");
	    break;
	case VNOVOL:
	    fprintf(STDERR,
		    "Volume not attached, does not exist, or not on line\n");
	    break;
	case VVOLEXISTS:
	    fprintf(STDERR, "Volume already exists\n");
	    break;
	case VNOSERVICE:
	    fprintf(STDERR, "Volume is not in service\n");
	    break;
	case VOFFLINE:
	    fprintf(STDERR, "Volume is off line\n");
	    break;
	case VONLINE:
	    fprintf(STDERR, "Volume is already on line\n");
	    break;
	case VDISKFULL:
	    fprintf(STDERR, "Partition is full\n");
	    break;
	case VOVERQUOTA:
	    fprintf(STDERR, "Volume max quota exceeded\n");
	    break;
	case VBUSY:
	    fprintf(STDERR, "Volume temporarily unavailable\n");
	    break;
	case VMOVED:
	    fprintf(STDERR, "Volume has moved to another server\n");
	    break;
	default:
	    break;
	}
    }
    return;
}

/* Return true if line is all whitespace */
static int
LineIsBlank(char *aline)
{
    int tc;

    while ((tc = *aline++))
	if ((tc != ' ') && (tc != '\t') && (tc != '\n'))
	    return (0);

    return (1);
}


/* bc_InitTextConfig
 *	initialize configuration information that is stored as text blocks
 */

afs_int32
bc_InitTextConfig(void)
{
    udbClientTextP ctPtr;
    int i;

    mkdir(DefaultConfDir, 777);	/* temporary */

    /* initialize the client text structures */
    ctPtr = &bc_globalConfig->configText[0];

    for (i = 0; i < TB_NUM; i++) {
	memset(ctPtr, 0, sizeof(*ctPtr));
	ctPtr->textType = i;
	ctPtr->textVersion = -1;
	ctPtr++;
    }

    return (0);
}

/*----------------------------------------------------------------------------
 * backupInit
 *
 * Description:
 *	Routine that is called when the backup coordinator is invoked, responsible for
 *	basic initialization and some command line parsing.
 *
 * Returns:
 *	Zero (but may exit the entire program on error!)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	Initializes this program.
 *----------------------------------------------------------------------------
 */

static int
backupInit(void)
{
    afs_int32 code;
    static int initd = 0;	/* ever called? */
    PROCESS watcherPid;
    PROCESS pid;		/* LWP process ID */

    /* Initialization */
    initialize_CMD_error_table();

    /* don't run more than once */
    if (initd) {
	afs_com_err(whoami, 0, "Backup already initialized.");
	return 0;
    }
    initd = 1;

    code = bc_InitConfig((char *)DefaultConfDir);
    if (code) {
	afs_com_err(whoami, code,
		"Can't initialize from config files in directory '%s'",
		DefaultConfDir);
	return (code);
    }

    /*
     * Set up Rx.
     */
    code = LWP_InitializeProcessSupport(LWP_NORMAL_PRIORITY, &pid);
    if (code) {
	afs_com_err(whoami, code, "; Can't initialize LWP");
	return (code);
    }

    code = rx_Init(htons(0));
    if (code) {
	afs_com_err(whoami, code, "; Can't initialize Rx");
	return (code);
    }

    rx_SetRxDeadTime(60);

    /* VLDB initialization */
    code = vldbClientInit(0, localauth, tcell, &cstruct, &tokenExpires);
    if (code)
	return (code);

    /* Backup database initialization */
    code = udbClientInit(0, localauth, tcell);
    if (code)
	return (code);

    /* setup status monitoring thread */
    initStatus();
    code =
	LWP_CreateProcess(statusWatcher, 20480, LWP_NORMAL_PRIORITY,
			  (void *)2, "statusWatcher", &watcherPid);
    if (code) {
	afs_com_err(whoami, code, "; Can't create status monitor task");
	return (code);
    }

    return (0);
}

/*----------------------------------------------------------------------------
 * MyBeforeProc
 *
 * Description:
 *	Make sure we mark down when we need to exit the Backup Coordinator.
 *	Specifically, we don't want to continue when our help and apropos
 *	routines are called from the command line.
 *
 * Arguments:
 *	as : Ptr to the command syntax descriptor.
 *
 * Returns:
 *	0.
 *
 * Environment:
 *	This routine is called right before each of the Backup Coordinator
 *	opcode routines are invoked.
 *
 *----------------------------------------------------------------------------
 */

static int
MyBeforeProc(struct cmd_syndesc *as, void *arock)
{
    afs_int32 code;

    /* Handling the command line opcode */
    if (!bcInit) {
	localauth = ((as && as->parms[14].items) ? 1 : 0);
	nobutcauth = ((as && as->parms[16].items) ? 1 : 0);
	if (as && as->parms[15].items)
	    strcpy(tcell, as->parms[15].items->data);
	else
	    tcell[0] = '\0';

	code = backupInit();
	if (code) {
	    afs_com_err(whoami, code, "; Can't initialize backup");
	    exit(1);
	}

	/* Get initial information from the database */
	code = bc_InitTextConfig();
	if (code) {
	    afs_com_err(whoami, code,
		    "; Can't obtain configuration text from backup database");
	    exit(1);
	}
    }
    bcInit = 1;

    return 0;
}


#define	MAXV	100

#include "AFS_component_version_number.c"

#define MAXRECURSION 20
extern int dontExecute;		/* declared in commands.c */
extern char *loadFile;		/* declared in commands.c */
char lineBuffer[1024];		/* Line typed in by user or read from load file */

/*
 * This will dispatch a command.  It holds a recursive loop for the
 * "dump -file" option. This option reads backup commands from a file.
 *
 * Cannot put this code on other side of cmd_Dispatch call (in
 * commands.c) because when make a dispatch call when in a dispatch
 * call, environment is mucked up.
 *
 * To avoid multiple processes stepping on each other in the dispatch code,
 * put a lock around it so only 1 process gets in at a time.
 */

struct Lock dispatchLock;	/* lock on the Dispatch call */
#define lock_Dispatch()     ObtainWriteLock(&dispatchLock)
#define unlock_Dispatch()   ReleaseWriteLock(&dispatchLock)

afs_int32
doDispatch(afs_int32 targc,
	   char *targv[],
	   afs_int32 dispatchCount) /* to prevent infinite recursion */
{
    char *sargv[MAXV];
    afs_int32 sargc;
    afs_int32 code, c;
    FILE *fd;
    int i;
    int noExecute;		/* local capy of global variable */
    char *internalLoadFile;

    lock_Dispatch();

    loadFile = NULL;
    code = cmd_Dispatch(targc, targv);
    internalLoadFile = loadFile;

    unlock_Dispatch();

    if (internalLoadFile) {	/* Load a file in */
	if (dispatchCount > MAXRECURSION) {	/* Beware recursive loops. */
	    afs_com_err(whoami, 0, "Potential recursion: will not load file %s",
		    internalLoadFile);
	    code = -1;
	    goto done;
	}

	fd = fopen(internalLoadFile, "r");	/* Open the load file */
	if (!fd) {
	    afs_com_err(whoami, errno, "; Cannot open file %s", internalLoadFile);
	    code = -1;
	    goto done;
	}

	noExecute = dontExecute;
	if (noExecute)
	    printf("Would have executed the following commands:\n");

	while (fgets(lineBuffer, sizeof(lineBuffer) - 1, fd)) {	/* Get commands from file */

	    i = strlen(lineBuffer) - 1;
	    if (lineBuffer[i] == '\n')	/* Drop return at end of line */
		lineBuffer[i] = '\0';

	    if (noExecute)
		printf("        %s\n", lineBuffer);	/* echo */
	    else
		printf("------> %s\n", lineBuffer);	/* echo */

	    if (!LineIsBlank(lineBuffer) &&	/* Skip if blank line */
		(lineBuffer[0] != '#') &&	/*      or comment    */
		(!noExecute)) {	/*      or no execute */
		c = cmd_ParseLine(lineBuffer, sargv, &sargc, MAXV);
		if (c) {
		    afs_com_err(whoami, c, "; Can't parse line");
		} else {
		    doDispatch(sargc, sargv, dispatchCount + 1);	/* Recursive - ignore error */
		    cmd_FreeArgv(sargv);	/* Free up arguments */
		}
	    }
	}

	fclose(fd);
    }

  done:
    if (internalLoadFile)
	free(internalLoadFile);
    return (code);
}

int
bc_interactCmd(struct cmd_syndesc *as, void *arock)
{
    interact = 1;
    return 0;
}

static void
add_std_args(struct cmd_syndesc *ts)
{
    cmd_Seek(ts, 14);
    cmd_AddParm(ts, "-localauth", CMD_FLAG, CMD_OPTIONAL,
		"local authentication");
    cmd_AddParm(ts, "-cell", CMD_SINGLE, CMD_OPTIONAL, "cell name");
    cmd_AddParm(ts, "-nobutcauth", CMD_FLAG, CMD_OPTIONAL,
		"no authentication to butc");
}

int
main(int argc, char **argv)
{				/*main */
    char *targv[MAXV];		/*Ptr to parsed argv stuff */
    afs_int32 targc;		/*Num parsed arguments */
    afs_int32 code;		/*Return code */
    struct cmd_syndesc *ts;	/*Ptr to parsed command line */
    int i;


#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    Lock_Init(&dispatchLock);
    InitErrTabs();		/* init all the error tables which may be used */

    /* setup the default backup dir */
    DefaultConfDir = AFSDIR_SERVER_BACKUP_DIRPATH;
    /* Get early warning if the command is interacive mode or not */
    if (argc < 2) {
	interact = 1;
    } else {
	interact = 0;
	if (argv[1][0] == '-') {
	    interact = 1;
	    if (strcmp(argv[1], "-help") == 0) {
		interact = 0;
	    }
	}
    }

    cmd_SetBeforeProc(MyBeforeProc, NULL);

    ts = cmd_CreateSyntax("dump", bc_DumpCmd, NULL, 0, "start dump");
    cmd_AddParm(ts, "-volumeset", CMD_SINGLE, CMD_OPTIONAL,
		"volume set name");
    cmd_AddParm(ts, "-dump", CMD_SINGLE, CMD_OPTIONAL, "dump level name");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    cmd_AddParm(ts, "-at", CMD_LIST, CMD_OPTIONAL, "Date/time to start dump");
    cmd_AddParm(ts, "-append", CMD_FLAG, CMD_OPTIONAL,
		"append to existing dump set");
    cmd_AddParm(ts, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
                "list what would be done, don't do it");
    cmd_AddParmAlias(ts, 5, "-n");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "load file");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("volrestore", bc_VolRestoreCmd, NULL, 0,
			  "restore volume");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"destination machine");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"destination partition");
    cmd_AddParm(ts, "-volume", CMD_LIST, CMD_REQUIRED,
		"volume(s) to restore");
    cmd_AddParm(ts, "-extension", CMD_SINGLE, CMD_OPTIONAL,
		"new volume name extension");
    cmd_AddParm(ts, "-date", CMD_LIST, CMD_OPTIONAL,
		"date from which to restore");
    cmd_AddParm(ts, "-portoffset", CMD_LIST, CMD_OPTIONAL, "TC port offsets");
    cmd_AddParm(ts, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
                "list what would be done, don't do it");
    cmd_AddParmAlias(ts, 6, "-n");
    cmd_AddParm(ts, "-usedump", CMD_SINGLE, CMD_OPTIONAL,
		"specify the dumpID to restore from");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("diskrestore", bc_DiskRestoreCmd, NULL, 0,
			  "restore partition");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED,
		"machine to restore");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED,
		"partition to restore");
    cmd_AddParm(ts, "-portoffset", CMD_LIST, CMD_OPTIONAL, "TC port offset");
    cmd_Seek(ts, 8);
    cmd_AddParm(ts, "-newserver", CMD_SINGLE, CMD_OPTIONAL,
		"destination machine");
    cmd_AddParm(ts, "-newpartition", CMD_SINGLE, CMD_OPTIONAL,
		"destination partition");
    cmd_AddParm(ts, "-extension", CMD_SINGLE, CMD_OPTIONAL,
		"new volume name extension");
    cmd_AddParm(ts, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
                "list what would be done, don't do it");
    cmd_AddParmAlias(ts, 11, "-n");
    if (!interact)
	add_std_args(ts);

    cmd_CreateSyntax("quit", bc_QuitCmd, NULL, 0, "leave the program");

    ts = cmd_CreateSyntax("volsetrestore", bc_VolsetRestoreCmd, NULL, 0,
			  "restore a set of volumes");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_OPTIONAL, "volume set name");
    cmd_AddParm(ts, "-file", CMD_SINGLE, CMD_OPTIONAL, "file name");
    cmd_AddParm(ts, "-portoffset", CMD_LIST, CMD_OPTIONAL, "TC port offset");
    cmd_AddParm(ts, "-extension", CMD_SINGLE, CMD_OPTIONAL,
		"new volume name extension");
    cmd_AddParm(ts, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
                "list what would be done, don't do it");
    cmd_AddParmAlias(ts, 4, "-n");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("addhost", bc_AddHostCmd, NULL, 0, "add host to config");
    cmd_AddParm(ts, "-tapehost", CMD_SINGLE, CMD_REQUIRED,
		"tape machine name");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("delhost", bc_DeleteHostCmd, NULL, 0,
			  "delete host to config");
    cmd_AddParm(ts, "-tapehost", CMD_SINGLE, CMD_REQUIRED,
		"tape machine name");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("listhosts", bc_ListHostsCmd, NULL, 0,
			  "list config hosts");
    if (!interact)
	add_std_args(ts);

    cmd_CreateSyntax("jobs", bc_JobsCmd, NULL, 0, "list running jobs");

    ts = cmd_CreateSyntax("kill", bc_KillCmd, NULL, 0, "kill running job");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_REQUIRED,
		"job ID or dump set name");

    ts = cmd_CreateSyntax("listvolsets", bc_ListVolSetCmd, NULL, 0,
			  "list volume sets");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_OPTIONAL, "volume set name");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("listdumps", bc_ListDumpScheduleCmd, NULL, 0,
			  "list dump schedules");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("addvolset", bc_AddVolSetCmd, NULL, 0,
			  "create a new volume set");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED, "volume set name");
    cmd_AddParm(ts, "-temporary", CMD_FLAG, CMD_OPTIONAL,
		"temporary volume set");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("status", bc_GetTapeStatusCmd, NULL, 0,
			  "get tape coordinator status");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("delvolset", bc_DeleteVolSetCmd, NULL, 0,
			  "delete a volume set");
    cmd_AddParm(ts, "-name", CMD_LIST, CMD_REQUIRED, "volume set name");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("addvolentry", bc_AddVolEntryCmd, NULL, 0,
			  "add a new volume entry");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED, "volume set name");
    cmd_AddParm(ts, "-server", CMD_SINGLE, CMD_REQUIRED, "machine name");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_REQUIRED, "partition name");
    cmd_AddParm(ts, "-volumes", CMD_SINGLE, CMD_REQUIRED,
		"volume name (regular expression)");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("delvolentry", bc_DeleteVolEntryCmd, NULL, 0,
			  "delete a volume set sub-entry");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_REQUIRED, "volume set name");
    cmd_AddParm(ts, "-entry", CMD_SINGLE, CMD_REQUIRED, "volume set index");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("adddump", bc_AddDumpCmd, NULL, 0, "add dump schedule");
    cmd_AddParm(ts, "-dump", CMD_LIST, CMD_REQUIRED, "dump level name");
    cmd_AddParm(ts, "-expires", CMD_LIST, CMD_OPTIONAL, "expiration date");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("deldump", bc_DeleteDumpCmd, NULL, 0,
			  "delete dump schedule");
    cmd_AddParm(ts, "-dump", CMD_SINGLE, CMD_REQUIRED, "dump level name");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("labeltape", bc_LabelTapeCmd, NULL, 0, "label a tape");
    cmd_AddParm(ts, "-name", CMD_SINGLE, CMD_OPTIONAL,
		"AFS tape name, defaults to NULL");
    cmd_AddParm(ts, "-size", CMD_SINGLE, CMD_OPTIONAL,
		"tape size in Kbytes, defaults to size in tapeconfig");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    cmd_AddParm(ts, "-pname", CMD_SINGLE, CMD_OPTIONAL,
		"permanent tape name");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("readlabel", bc_ReadLabelCmd, NULL, 0,
			  "read the label on tape");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("scantape", bc_ScanDumpsCmd, NULL, 0,
			  "dump information recovery from tape");
    cmd_AddParm(ts, "-dbadd", CMD_FLAG, CMD_OPTIONAL,
		"add information to the database");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("volinfo", bc_dblookupCmd, NULL, 0,
			  "query the backup database");
    cmd_AddParm(ts, "-volume", CMD_SINGLE, CMD_REQUIRED, "volume name");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("setexp", bc_SetExpCmd, NULL, 0,
			  "set/clear dump expiration dates");
    cmd_AddParm(ts, "-dump", CMD_LIST, CMD_REQUIRED, "dump level name");
    cmd_AddParm(ts, "-expires", CMD_LIST, CMD_OPTIONAL, "expiration date");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("savedb", bc_saveDbCmd, NULL, 0, "save backup database");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    cmd_AddParm(ts, "-archive", CMD_LIST, CMD_OPTIONAL, "date time");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("restoredb", bc_restoreDbCmd, NULL, 0,
			  "restore backup database");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL,
		"TC port offset");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("dumpinfo", bc_dumpInfoCmd, NULL, 0,
			  "provide information about a dump in the database");
    cmd_AddParm(ts, "-ndumps", CMD_SINGLE, CMD_OPTIONAL, "no. of dumps");
    cmd_AddParm(ts, "-id", CMD_SINGLE, CMD_OPTIONAL, "dump id");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL,
		"detailed description");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("dbverify", bc_dbVerifyCmd, NULL, 0,
			  "check ubik database integrity");
    cmd_AddParm(ts, "-detail", CMD_FLAG, CMD_OPTIONAL, "additional details");
    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("deletedump", bc_deleteDumpCmd, NULL, 0,
			  "delete dumps from the database");
    cmd_AddParm(ts, "-dumpid", CMD_LIST, CMD_OPTIONAL, "dump id");
    cmd_AddParm(ts, "-from", CMD_LIST, CMD_OPTIONAL, "date time");
    cmd_AddParm(ts, "-to", CMD_LIST, CMD_OPTIONAL, "date time");
    cmd_AddParm(ts, "-portoffset", CMD_SINGLE, CMD_OPTIONAL, "TC port offset");
    cmd_AddParmAlias(ts, 3, "-port");
    cmd_AddParm(ts, "-groupid", CMD_SINGLE, CMD_OPTIONAL, "group ID");
    cmd_AddParm(ts, "-dbonly", CMD_FLAG, CMD_OPTIONAL,
		"delete the dump from the backup database only");
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL,
		"always delete from backup database");
    cmd_AddParm(ts, "-noexecute", CMD_FLAG, CMD_OPTIONAL|CMD_HIDDEN, "");
    cmd_AddParm(ts, "-dryrun", CMD_FLAG, CMD_OPTIONAL,
		"list the dumps, don't delete anything");
    cmd_AddParmAlias(ts, 8, "-n");

    if (!interact)
	add_std_args(ts);

    ts = cmd_CreateSyntax("interactive", bc_interactCmd, NULL, 0,
			  "enter interactive mode");
    add_std_args(ts);

    /*
     * Now execute the command.
     */

    targc = 0;
    targv[targc++] = argv[0];
    if (interact)
	targv[targc++] = "interactive";
    for (i = 1; i < argc; i++)
	targv[targc++] = argv[i];

    code = doDispatch(targc, targv, 1);

    if (!interact || !bcInit) {	/* Non-interactive mode */
	if (code)
	    exit(-1);
	if (bcInit)
	    code = bc_WaitForNoJobs();	/* wait for any jobs to finish */
	exit(code);		/* and exit */
    }

    /* Iterate on command lines, interpreting user commands (interactive mode) */
    while (1) {
	int ret;

	printf("backup> ");
	fflush(stdout);


	while ((ret = LWP_GetLine(lineBuffer, sizeof(lineBuffer))) == 0)
	    printf("%s: Command line too long\n", whoami);	/* line too long */

	if (ret == -1)
	    return 0;		/* Got EOF */

	if (!LineIsBlank(lineBuffer)) {
	    code = cmd_ParseLine(lineBuffer, targv, &targc, MAXV);
	    if (code)
		afs_com_err(whoami, code, "; Can't parse line: '%s'",
			afs_error_message(code));
	    else {
		doDispatch(targc, targv, 1);
		cmd_FreeArgv(targv);
	    }
	}
    }
}				/*main */
