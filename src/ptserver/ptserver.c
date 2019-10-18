/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *                      A general description of the supergroup changes
 *                      to ptserver:
 *
 *                      In AFS users can be members of groups. When you add
 *                      a user, u1, to a group, g1, you add the user id for u1
 *                      to an entries list in g1. This is a list of all the
 *                      members of g1.
 *                      You also add the id for g1 to an entries list in u1.
 *                      This is a list of all the groups this user is a
 *                      member of.
 *                      The list has room for 10 entries. If more are required,
 *                      a continuation record is created.
 *
 *                      With UMICH mods, u1 can be a group. When u1 is a group
 *                      a new field is required to list the groups this group
 *                      is a member of (since the entries field is used to
 *                      list it's members). This new field is supergroups and
 *                      has two entries. If more are required, a continuation
 *                      record is formed.
 *                      There are two additional fields required, nextsg is
 *                      an address of the next continuation record for this
 *                      group, and countsg is the count for the number of
 *                      groups this group is a member of.
 *
 *
 *
 *      09/18/95 jjm    Add mdw's changes to afs-3.3a Changes:
 *                      (1) Add the parameter -groupdepth or -depth to
 *                          define the maximum search depth for supergroups.
 *                          Define the variable depthsg to be the value of
 *                          the parameter. The default value is set to 5.
 *
 *                      (3) Make sure the sizes of the struct prentry and
 *                          struct prentryg are equal. If they aren't equal
 *                          the pt database will be corrupted.
 *                          The error is reported with an fprintf statement,
 *                          but this doesn't print when ptserver is started by
 *                          bos, so all you see is an error message in the
 *                          /usr/afs/logs/BosLog file. If you start the
 *                          ptserver without bos the fprintf will print
 *                          on stdout.
 *                          The program will terminate with a PT_EXIT(1).
 *
 *
 *                      Transarc does not currently use opcodes past 520, but
 *                      they *could* decide at any time to use more opcodes.
 *                      If they did, then one part of our local mods,
 *                      ListSupergroups, would break.  I've therefore
 *                      renumbered it to 530, and put logic in to enable the
 *                      old opcode to work (for now).
 *
 *      2/1/98 jjm      Add mdw's changes for bit mapping for supergroups
 *                      Overview:
 *                      Before fetching a supergroup, this version of ptserver
 *                      checks to see if it was marked "found" and "not a
 *                      member".  If and only if so, it doesn't fetch the group.
 *                      Since this should be the case with most groups, this
 *                      should save a significant amount of CPU in redundant
 *                      fetches of the same group.  After fetching the group,
 *                      it sets "found", and either sets or clears "not a
 *                      member", depending on if the group was a member of
 *                      other groups.  When it writes group entries to the
 *                      database, it clears the "found" flag.
 */

#if defined(SUPERGROUPS)
/*
 *  A general description of the supergroup changes
 *  to ptserver:
 *
 *  In AFS users can be members of groups. When you add a user, u1,
 *  to a group, g1, you add the user id for u1 to an entries list
 *  in g1. This is a list of all the members of g1.  You also add
 *  the id for g1 to an entries list in u1.  This is a list of all
 *  the groups this user is a member of.  The list has room for 10
 *  entries. If more are required, a continuation record is created.
 *
 *  With UMICH mods, u1 can be a group. When u1 is a group a new
 *  field is required to list the groups this group is a member of
 *  (since the entries field is used to list it's members). This
 *  new field is supergroups and has two entries. If more are
 *  required, a continuation record is formed.
 *
 *  There are two additional fields required, nextsg is an address
 *  of the next continuation record for this group, and countsg is
 *  the count for the number of groups this group is a member of.
 *
 *  Bit mapping support for supergroups:
 *
 *  Before fetching a supergroup, this version of ptserver checks to
 *  see if it was marked "found" and "not a member".  If and only if
 *  so, it doesn't fetch the group.  Since this should be the case
 *  with most groups, this should save a significant amount of CPU in
 *  redundant fetches of the same group.  After fetching the group, it
 *  sets "found", and either sets or clears "not a member", depending
 *  on if the group was a member of other groups.  When it writes
 *  group entries to the database, it clears the "found" flag.
 */
#endif

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <roken.h>
#include <afs/opr.h>
#ifdef AFS_PTHREAD_ENV
# include <opr/softsig.h>
# include <afs/procmgmt_softsig.h> /* must come after softsig.h */
#endif

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif

#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <rx/rxstat.h>
#include <lock.h>
#include <ubik.h>
#include <afs/cmd.h>
#include <afs/cellconfig.h>
#include <afs/auth.h>
#include <afs/keys.h>
#include <afs/afsutil.h>
#include <afs/audit.h>
#include <afs/com_err.h>

#include "ptserver.h"
#include "ptprototypes.h"
#include "error_macros.h"

/* make	all of these into a structure if you want */
struct prheader cheader;
struct ubik_dbase *dbase;
struct afsconf_dir *prdir;

#if defined(SUPERGROUPS)
extern afs_int32 depthsg;
#endif

int restricted = 0;
int restrict_anonymous = 0;
int rxMaxMTU = -1;
int rxBind = 0;
int rxkadDisableDotCheck = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

static struct afsconf_cell info;

extern int prp_group_default;
extern int prp_user_default;

#include "AFS_component_version_number.c"

int
prp_access_mask(char *s)
{
    int r;
    if (*s >= '0' && *s <= '9') {
	return strtol(s, NULL, 0);
    }
    r = 0;
    while (*s) switch(*s++)
    {
    case 'S':	r |= PRP_STATUS_ANY; break;
    case 's':	r |= PRP_STATUS_MEM; break;
    case 'O':	r |= PRP_OWNED_ANY; break;
    case 'M':	r |= PRP_MEMBER_ANY; break;
    case 'm':	r |= PRP_MEMBER_MEM; break;
    case 'A':	r |= PRP_ADD_ANY; break;
    case 'a':	r |= PRP_ADD_MEM; break;
    case 'r':	r |= PRP_REMOVE_MEM; break;
    }
    return r;
}

/* check whether caller is authorized to manage RX statistics */
int
pr_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(prdir, call, NULL);
}

/**
 * Return true if this name is a member of the local realm.
 */
int
pr_IsLocalRealmMatch(void *rock, char *name, char *inst, char *cell)
{
    struct afsconf_dir *dir = (struct afsconf_dir *)rock;
    afs_int32 islocal = 0;	/* default to no */
    int code;

    code = afsconf_IsLocalRealmMatch(dir, &islocal, name, inst, cell);
    if (code) {
	ViceLog(0, ("Failed local realm check; code=%d, name=%s, inst=%s, cell=%s\n",
		 code, name, inst, cell));
    }
    return islocal;
}


enum optionsList {
    OPT_database,
    OPT_access,
    OPT_groupdepth,
    OPT_restricted,
    OPT_restrict_anonymous,
    OPT_auditlog,
    OPT_auditiface,
    OPT_config,
    OPT_debug,
    OPT_logfile,
    OPT_threads,
#ifdef HAVE_SYSLOG
    OPT_syslog,
#endif
    OPT_peer,
    OPT_process,
    OPT_rxbind,
    OPT_rxmaxmtu,
    OPT_dotted,
    OPT_transarc_logs
};

int
main(int argc, char **argv)
{
    afs_int32 code;
    afs_uint32 myHost;
    struct hostent *th;
    char hostname[64];
    struct rx_service *tservice;
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;
    int lwps = 3;
    char clones[MAXHOSTSPERCELL];
    afs_uint32 host = htonl(INADDR_ANY);
    struct cmd_syndesc *opts;
    struct cmd_item *list;

    char *pr_dbaseName;
    char *configDir;
    struct logOptions logopts;
    char *whoami = "ptserver";

    char *auditFileName = NULL;
    char *interface = NULL;

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
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    osi_audit_init();
    osi_audit(PTS_StartEvent, 0, AUD_END);

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    pr_dbaseName = strdup(AFSDIR_SERVER_PRDB_FILEPATH);
    configDir = strdup(AFSDIR_SERVER_ETC_DIRPATH);
    memset(&logopts, 0, sizeof(logopts));

#if defined(SUPERGROUPS)
    /* make sure the structures for database records are the same size */
    if ((sizeof(struct prentry) != ENTRYSIZE)
	|| (sizeof(struct prentryg) != ENTRYSIZE)) {
	fprintf(stderr,
		"The structures for the database records are different"
		" sizes\n" "struct prentry = %" AFS_SIZET_FMT "\n"
                "struct prentryg = %" AFS_SIZET_FMT "\n"
		"ENTRYSIZE = %d\n", sizeof(struct prentry),
		sizeof(struct prentryg), ENTRYSIZE);
	PT_EXIT(1);
    }
#endif

    cmd_DisableAbbreviations();
    cmd_DisablePositionalCommands();
    opts = cmd_CreateSyntax(NULL, NULL, NULL, 0, NULL);

/* ptserver specific options */
    cmd_AddParmAtOffset(opts, OPT_database, "-database", CMD_SINGLE,
		        CMD_OPTIONAL, "database file");
    cmd_AddParmAlias(opts, OPT_database, "-db");

    cmd_AddParmAtOffset(opts, OPT_access, "-default_access", CMD_LIST,
		        CMD_OPTIONAL, "default access flags for new entries");
#if defined(SUPERGROUPS)
    cmd_AddParmAtOffset(opts, OPT_groupdepth, "-groupdepth", CMD_SINGLE,
		        CMD_OPTIONAL, "max search depth for supergroups");
    cmd_AddParmAlias(opts, OPT_groupdepth, "-depth");
#endif
    cmd_AddParmAtOffset(opts, OPT_restricted, "-restricted", CMD_FLAG,
		        CMD_OPTIONAL, "enable restricted mode");
    cmd_AddParmAtOffset(opts, OPT_restrict_anonymous, "-restrict_anonymous",
			CMD_FLAG, CMD_OPTIONAL, "enable restricted anonymous mode");

    /* general server options */
    cmd_AddParmAtOffset(opts, OPT_auditlog, "-auditlog", CMD_SINGLE,
		 	CMD_OPTIONAL, "location of audit log");
    cmd_AddParmAtOffset(opts, OPT_auditiface, "-audit-interface", CMD_SINGLE,
		        CMD_OPTIONAL, "interface to use for audit logging");
    cmd_AddParmAtOffset(opts, OPT_config, "-config", CMD_SINGLE,
		        CMD_OPTIONAL, "configuration location");
    cmd_AddParmAtOffset(opts, OPT_debug, "-d", CMD_SINGLE,
		        CMD_OPTIONAL, "debug level");
    cmd_AddParmAtOffset(opts, OPT_logfile, "-logfile", CMD_SINGLE,
		        CMD_OPTIONAL, "location of logfile");
    cmd_AddParmAtOffset(opts, OPT_threads, "-p", CMD_SINGLE,
		        CMD_OPTIONAL, "number of threads");
#ifdef HAVE_SYSLOG
    cmd_AddParmAtOffset(opts, OPT_syslog, "-syslog", CMD_SINGLE_OR_FLAG, 
		        CMD_OPTIONAL, "log to syslog");
#endif
    cmd_AddParmAtOffset(opts, OPT_transarc_logs, "-transarc-logs", CMD_FLAG,
			CMD_OPTIONAL, "enable Transarc style logging");

    /* rx options */
    cmd_AddParmAtOffset(opts, OPT_peer, "-enable_peer_stats", CMD_FLAG,
		        CMD_OPTIONAL, "enable RX transport statistics");
    cmd_AddParmAtOffset(opts, OPT_process, "-enable_process_stats", CMD_FLAG,
		        CMD_OPTIONAL, "enable RX RPC statistics");
    cmd_AddParmAtOffset(opts, OPT_rxbind, "-rxbind", CMD_FLAG,
		        CMD_OPTIONAL, "bind only to the primary interface");
    cmd_AddParmAtOffset(opts, OPT_rxmaxmtu, "-rxmaxmtu", CMD_SINGLE,
		        CMD_OPTIONAL, "maximum MTU for RX");

    /* rxkad options */
    cmd_AddParmAtOffset(opts, OPT_dotted, "-allow-dotted-principals",
		        CMD_FLAG, CMD_OPTIONAL,
		        "permit Kerberos 5 principals with dots");

    code = cmd_Parse(argc, argv, &opts);
    if (code == CMD_HELP) {
	PT_EXIT(0);
    }
    if (code)
	PT_EXIT(1);

    cmd_OptionAsString(opts, OPT_config, &configDir);

    cmd_OpenConfigFile(AFSDIR_SERVER_CONFIG_FILE_FILEPATH);
    cmd_SetCommandName("ptserver");

    if (cmd_OptionAsList(opts, OPT_access, &list) == 0) {
	prp_user_default = prp_access_mask(list->data);
	if (list->next == NULL || list->next->data == NULL) {
	    fprintf(stderr, "Missing second argument for -default_access\n");
	    PT_EXIT(1);
	}
	prp_group_default = prp_access_mask(list->next->data);
    }

#if defined(SUPERGROUPS)
    cmd_OptionAsInt(opts, OPT_groupdepth, &depthsg);
#endif

    cmd_OptionAsFlag(opts, OPT_restricted, &restricted);
    cmd_OptionAsFlag(opts, OPT_restrict_anonymous, &restrict_anonymous);

    /* general server options */
    cmd_OptionAsString(opts, OPT_auditlog, &auditFileName);

    if (cmd_OptionAsString(opts, OPT_auditiface, &interface) == 0) {
	if (osi_audit_interface(interface)) {
	    printf("Invalid audit interface '%s'\n", interface);
	    PT_EXIT(1);
	}
	free(interface);
    }

    cmd_OptionAsString(opts, OPT_database, &pr_dbaseName);

    if (cmd_OptionAsInt(opts, OPT_threads, &lwps) == 0) {
	if (lwps > 64) {	/* maximum of 64 */
	    printf("Warning: '-p %d' is too big; using %d instead\n",
		   lwps, 64);
	    lwps = 64;
	} else if (lwps < 3) {	/* minimum of 3 */
	    printf("Warning: '-p %d' is too small; using %d instead\n",
		   lwps, 3);
	    lwps = 3;
	}
    }

#ifdef HAVE_SYSLOG
    if (cmd_OptionPresent(opts, OPT_syslog)) {
	if (cmd_OptionPresent(opts, OPT_logfile)) {
	    fprintf(stderr, "Invalid options: -syslog and -logfile are exclusive.");
	    PT_EXIT(1);
	}
	if (cmd_OptionPresent(opts, OPT_transarc_logs)) {
	    fprintf(stderr, "Invalid options: -syslog and -transarc-logs are exclusive.");
	    PT_EXIT(1);
	}
	logopts.lopt_dest = logDest_syslog;
	logopts.lopt_facility = LOG_DAEMON;
	logopts.lopt_tag = "ptserver";
	cmd_OptionAsInt(opts, OPT_syslog, &logopts.lopt_facility);
    } else
#endif
    {
	logopts.lopt_dest = logDest_file;
	if (cmd_OptionPresent(opts, OPT_transarc_logs)) {
	    logopts.lopt_rotateOnOpen = 1;
	    logopts.lopt_rotateStyle = logRotate_old;
	}
	if (cmd_OptionPresent(opts, OPT_logfile))
	    cmd_OptionAsString(opts, OPT_logfile, (char**)&logopts.lopt_filename);
	else
	    logopts.lopt_filename = AFSDIR_SERVER_PTLOG_FILEPATH;
    }
    cmd_OptionAsInt(opts, OPT_debug, &logopts.lopt_logLevel);

    /* rx options */
    if (cmd_OptionPresent(opts, OPT_peer))
	rx_enablePeerRPCStats();

    if (cmd_OptionPresent(opts, OPT_process))
	rx_enableProcessRPCStats();

    cmd_OptionAsFlag(opts, OPT_rxbind, &rxBind);

    cmd_OptionAsInt(opts, OPT_rxmaxmtu, &rxMaxMTU);

    /* rxkad options */
    cmd_OptionAsFlag(opts, OPT_dotted, &rxkadDisableDotCheck);

    cmd_FreeOptions(&opts);

    if (auditFileName) {
	osi_audit_file(auditFileName);
	osi_audit(PTS_StartEvent, 0, AUD_END);
    }

    OpenLog(&logopts);
#ifdef AFS_PTHREAD_ENV
    opr_softsig_Init();
    SetupLogSoftSignals();
#else
    SetupLogSignals();
#endif

    prdir = afsconf_Open(configDir);
    if (!prdir) {
	fprintf(stderr, "ptserver: can't open configuration directory.\n");
	PT_EXIT(1);
    }
    if (afsconf_GetNoAuthFlag(prdir))
	printf("ptserver: running unauthenticated\n");

#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);

	fprintf(stderr, "ptserver: couldn't initialize winsock. \n");
	PT_EXIT(1);
    }
#endif
    /* get this host */
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	fprintf(stderr, "ptserver: couldn't get address of this host.\n");
	PT_EXIT(1);
    }
    memcpy(&myHost, th->h_addr, sizeof(afs_uint32));

    /* get list of servers */
    code =
	afsconf_GetExtendedCellInfo(prdir, NULL, "afsprot", &info, clones);
    if (code) {
	afs_com_err(whoami, code, "Couldn't get server list");
	PT_EXIT(2);
    }

    /* initialize audit user check */
    osi_audit_set_user_check(prdir, pr_IsLocalRealmMatch);

    /* initialize ubik */
    ubik_SetClientSecurityProcs(afsconf_ClientAuth, afsconf_UpToDate, prdir);
    ubik_SetServerSecurityProcs(afsconf_BuildServerSecurityObjects,
				afsconf_CheckAuth, prdir);

    /* The max needed is when deleting an entry.  A full CoEntry deletion
     * required removal from 39 entries.  Each of which may refers to the entry
     * being deleted in one of its CoEntries.  If a CoEntry is freed its
     * predecessor CoEntry will be modified as well.  Any freed blocks also
     * modifies the database header.  Counting the entry being deleted and its
     * CoEntry this adds up to as much as 1+1+39*3 = 119.  If all these entries
     * and the header are in separate Ubik buffers then 120 buffers may be
     * required. */
    ubik_nBuffers = 120 + /*fudge */ 40;

    if (rxBind) {
	afs_int32 ccode;
	if (AFSDIR_SERVER_NETRESTRICT_FILEPATH ||
	    AFSDIR_SERVER_NETINFO_FILEPATH) {
	    char reason[1024];
	    ccode = afsconf_ParseNetFiles(SHostAddrs, NULL, NULL,
					  ADDRSPERSITE, reason,
					  AFSDIR_SERVER_NETINFO_FILEPATH,
					  AFSDIR_SERVER_NETRESTRICT_FILEPATH);
	} else
	{
	    ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
	}
	if (ccode == 1) {
	    host = SHostAddrs[0];
	    /* the following call is idempotent so if/when it gets called
	     * again by the ubik init stuff, it doesn't really matter
	     * -- klm
	     */
	    rx_InitHost(host, htons(AFSCONF_PROTPORT));
	}
    }

    /* Disable jumbograms */
    rx_SetNoJumbo();

    if (rxMaxMTU != -1) {
	if (rx_SetMaxMTU(rxMaxMTU) != 0) {
	    printf("rxMaxMTU %d is invalid\n", rxMaxMTU);
	    PT_EXIT(1);
	}
    }

    code =
	ubik_ServerInitByInfo(myHost, htons(AFSCONF_PROTPORT), &info, clones,
			      pr_dbaseName, &dbase);
    if (code) {
	afs_com_err(whoami, code, "Ubik init failed");
	PT_EXIT(2);
    }

#if defined(SUPERGROUPS)
    pt_hook_write();
#endif

    afsconf_BuildServerSecurityObjects(prdir, &securityClasses, &numClasses);

    tservice =
	rx_NewServiceHost(host, 0, PRSRV, "Protection Server", securityClasses,
		          numClasses, PR_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf(stderr, "ptserver: Could not create new rx service.\n");
	PT_EXIT(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, lwps);
    if (rxkadDisableDotCheck) {
        rx_SetSecurityConfiguration(tservice, RXS_CONFIG_FLAGS,
                                    (void *)RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
    }

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats",
			  securityClasses, numClasses, RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	fprintf(stderr, "ptserver: Could not create new rx service.\n");
	PT_EXIT(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(pr_rxstat_userok);

    LogCommandLine(argc, argv, "ptserver",
#if defined(SUPERGROUPS)
		   "1.1",
#else
		   "1.0",
#endif
		   "Starting AFS", FSLog);
    if (afsconf_CountKeys(prdir) == 0) {
	ViceLog(0, ("WARNING: No encryption keys found! "
		    "All authenticated accesses will fail. "
		    "Run akeyconvert or asetkey to import encryption keys.\n"));
    } else if (afsconf_GetLatestKey(prdir, NULL, NULL) == 0) {
	LogDesWarning();
    }

    rx_StartServer(1);
    osi_audit(PTS_FinishEvent, -1, AUD_END);
    exit(0);
}
