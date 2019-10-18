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
#ifdef AFS_PTHREAD_ENV
# include <opr/softsig.h>
# include <afs/procmgmt_softsig.h> /* must come after softsig.h */
#endif

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <rx/rxstat.h>
#include <afs/cmd.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/auth.h>
#include <afs/audit.h>
#include <afs/com_err.h>
#include <lock.h>
#include <ubik.h>
#include <afs/afsutil.h>

#include "vlserver.h"
#include "vlserver_internal.h"

#define MAXLWP 64
struct afsconf_dir *vldb_confdir = 0;	/* vldb configuration dir */
int lwps = 9;

struct vldstats dynamic_statistics;
struct ubik_dbase *VL_dbase;
afs_uint32 rd_HostAddress[MAXSERVERID + 1];
afs_uint32 wr_HostAddress[MAXSERVERID + 1];

static void *CheckSignal(void*);
int smallMem = 0;
int restrictedQueryLevel = RESTRICTED_QUERY_ANYUSER;
int rxJumbograms = 0;		/* default is to not send and receive jumbo grams */
int rxMaxMTU = -1;
afs_int32 rxBind = 0;
int rxkadDisableDotCheck = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

static void
CheckSignal_Signal(int unused)
{
#if defined(AFS_PTHREAD_ENV)
    CheckSignal(0);
#else
    IOMGR_SoftSig(CheckSignal, 0);
#endif
}

static void *
CheckSignal(void *unused)
{
    int i, errorcode;
    struct vl_ctx ctx;

    if ((errorcode =
	Init_VLdbase(&ctx, LOCKREAD, VLGETSTATS - VL_LOWEST_OPCODE)))
	return (void *)(intptr_t)errorcode;
    VLog(0, ("Dump name hash table out\n"));
    for (i = 0; i < HASHSIZE; i++) {
	HashNDump(&ctx, i);
    }
    VLog(0, ("Dump id hash table out\n"));
    for (i = 0; i < HASHSIZE; i++) {
	HashIdDump(&ctx, i);
    }
    return ((void *)(intptr_t)ubik_EndTrans(ctx.trans));
}				/*CheckSignal */


/* Initialize the stats for the opcodes */
void
initialize_dstats(void)
{
    int i;

    dynamic_statistics.start_time = (afs_uint32) time(0);
    for (i = 0; i < MAX_NUMBER_OPCODES; i++) {
	dynamic_statistics.requests[i] = 0;
	dynamic_statistics.aborts[i] = 0;
    }
}

/* check whether caller is authorized to manage RX statistics */
int
vldb_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(vldb_confdir, call, NULL);
}

/**
 * Return true if this name is a member of the local realm.
 */
int
vldb_IsLocalRealmMatch(void *rock, char *name, char *inst, char *cell)
{
    struct afsconf_dir *dir = (struct afsconf_dir *)rock;
    afs_int32 islocal = 0;	/* default to no */
    int code;

    code = afsconf_IsLocalRealmMatch(dir, &islocal, name, inst, cell);
    if (code) {
	VLog(0,
		("Failed local realm check; code=%d, name=%s, inst=%s, cell=%s\n",
		 code, name, inst, cell));
    }
    return islocal;
}

/* Main server module */

#include "AFS_component_version_number.c"

enum optionsList {
    OPT_noauth,
    OPT_smallmem,
    OPT_auditlog,
    OPT_auditiface,
    OPT_config,
    OPT_debug,
    OPT_database,
    OPT_logfile,
    OPT_threads,
#ifdef HAVE_SYSLOG
    OPT_syslog,
#endif
    OPT_peer,
    OPT_process,
    OPT_nojumbo,
    OPT_jumbo,
    OPT_rxbind,
    OPT_rxmaxmtu,
    OPT_trace,
    OPT_dotted,
    OPT_restricted_query,
    OPT_transarc_logs
};

int
main(int argc, char **argv)
{
    afs_int32 code;
    afs_uint32 myHost;
    struct rx_service *tservice;
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;
    struct afsconf_dir *tdir;
    struct ktc_encryptionKey tkey;
    struct afsconf_cell info;
    struct hostent *th;
    char hostname[VL_MAXNAMELEN];
    int noAuth = 0;
    char clones[MAXHOSTSPERCELL];
    afs_uint32 host = ntohl(INADDR_ANY);
    struct cmd_syndesc *opts;
    struct logOptions logopts;

    char *vl_dbaseName;
    char *configDir;

    char *auditFileName = NULL;
    char *interface = NULL;
    char *optstring = NULL;

    char *restricted_query_parameter = NULL;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    rx_extraPackets = 100;	/* should be a switch, I guess... */
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    osi_audit_init();

    memset(&logopts, 0, sizeof(logopts));

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    vl_dbaseName = strdup(AFSDIR_SERVER_VLDB_FILEPATH);
    configDir = strdup(AFSDIR_SERVER_ETC_DIRPATH);

    cmd_DisableAbbreviations();
    cmd_DisablePositionalCommands();
    opts = cmd_CreateSyntax(NULL, NULL, NULL, 0, NULL);

    /* vlserver specific options */
    cmd_AddParmAtOffset(opts, OPT_noauth, "-noauth", CMD_FLAG,
		        CMD_OPTIONAL, "disable authentication");
    cmd_AddParmAtOffset(opts, OPT_smallmem, "-smallmem", CMD_FLAG,
		        CMD_OPTIONAL, "optimise for small memory systems");

    /* general server options */
    cmd_AddParmAtOffset(opts, OPT_auditlog, "-auditlog", CMD_SINGLE,
		        CMD_OPTIONAL, "location of audit log");
    cmd_AddParmAtOffset(opts, OPT_auditiface, "-audit-interface", CMD_SINGLE,
		        CMD_OPTIONAL, "interface to use for audit logging");
    cmd_AddParmAtOffset(opts, OPT_config, "-config", CMD_SINGLE,
		        CMD_OPTIONAL, "configuration location");
    cmd_AddParmAtOffset(opts, OPT_debug, "-d", CMD_SINGLE,
		        CMD_OPTIONAL, "debug level");
    cmd_AddParmAtOffset(opts, OPT_database, "-database", CMD_SINGLE,
		        CMD_OPTIONAL, "database file");
    cmd_AddParmAlias(opts, OPT_database, "-db");
    cmd_AddParmAtOffset(opts, OPT_logfile, "-logfile", CMD_SINGLE,
		        CMD_OPTIONAL, "location of logfile");
    cmd_AddParmAtOffset(opts, OPT_threads, "-p", CMD_SINGLE, CMD_OPTIONAL,
		        "number of threads");
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
    cmd_AddParmAtOffset(opts, OPT_nojumbo, "-nojumbo", CMD_FLAG,
		        CMD_OPTIONAL, "disable jumbograms");
    cmd_AddParmAtOffset(opts, OPT_jumbo, "-jumbo", CMD_FLAG,
		        CMD_OPTIONAL, "enable jumbograms");
    cmd_AddParmAtOffset(opts, OPT_rxbind, "-rxbind", CMD_FLAG,
		        CMD_OPTIONAL, "bind only to the primary interface");
    cmd_AddParmAtOffset(opts, OPT_rxmaxmtu, "-rxmaxmtu", CMD_SINGLE,
		        CMD_OPTIONAL, "maximum MTU for RX");
    cmd_AddParmAtOffset(opts, OPT_trace, "-trace", CMD_SINGLE,
		        CMD_OPTIONAL, "rx trace file");
    cmd_AddParmAtOffset(opts, OPT_restricted_query, "-restricted_query",
			CMD_SINGLE, CMD_OPTIONAL, "anyuser | admin");


    /* rxkad options */
    cmd_AddParmAtOffset(opts, OPT_dotted, "-allow-dotted-principals",
		        CMD_FLAG, CMD_OPTIONAL,
		        "permit Kerberos 5 principals with dots");

    code = cmd_Parse(argc, argv, &opts);
    if (code == CMD_HELP) {
	exit(0);
    }
    if (code)
	return -1;

    cmd_OptionAsString(opts, OPT_config, &configDir);

    cmd_OpenConfigFile(AFSDIR_SERVER_CONFIG_FILE_FILEPATH);
    cmd_SetCommandName("vlserver");

    /* vlserver options */
    cmd_OptionAsFlag(opts, OPT_noauth, &noAuth);
    cmd_OptionAsFlag(opts, OPT_smallmem, &smallMem);
    if (cmd_OptionAsString(opts, OPT_trace, &optstring) == 0) {
	extern char rxi_tracename[80];
	strcpy(rxi_tracename, optstring);
	free(optstring);
	optstring = NULL;
    }

    /* general server options */

    cmd_OptionAsString(opts, OPT_auditlog, &auditFileName);

    if (cmd_OptionAsString(opts, OPT_auditiface, &interface) == 0) {
	if (osi_audit_interface(interface)) {
	    printf("Invalid audit interface '%s'\n", interface);
	    return -1;
	}
	free(interface);
    }

    cmd_OptionAsString(opts, OPT_database, &vl_dbaseName);

    if (cmd_OptionAsInt(opts, OPT_threads, &lwps) == 0) {
	if (lwps > MAXLWP) {
	     printf("Warning: '-p %d' is too big; using %d instead\n",
		    lwps, MAXLWP);
	     lwps = MAXLWP;
	}
    }

    cmd_OptionAsInt(opts, OPT_debug, &logopts.lopt_logLevel);
#ifdef HAVE_SYSLOG
    if (cmd_OptionPresent(opts, OPT_syslog)) {
	if (cmd_OptionPresent(opts, OPT_logfile)) {
	    fprintf(stderr, "Invalid options: -syslog and -logfile are exclusive.\n");
	    return -1;
	}
	if (cmd_OptionPresent(opts, OPT_transarc_logs)) {
	    fprintf(stderr, "Invalid options: -syslog and -transarc-logs are exclusive.\n");
	    return -1;
	}

	logopts.lopt_dest = logDest_syslog;
	logopts.lopt_facility = LOG_DAEMON; /* default value */
	logopts.lopt_tag = "vlserver";
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
	    logopts.lopt_filename = AFSDIR_SERVER_VLOG_FILEPATH;
    }


    /* rx options */
    if (cmd_OptionPresent(opts, OPT_peer))
	rx_enablePeerRPCStats();
    if (cmd_OptionPresent(opts, OPT_process))
	rx_enableProcessRPCStats();
    if (cmd_OptionPresent(opts, OPT_nojumbo))
	rxJumbograms = 0;
    if (cmd_OptionPresent(opts, OPT_jumbo))
	rxJumbograms = 1;

    cmd_OptionAsFlag(opts, OPT_rxbind, &rxBind);

    cmd_OptionAsInt(opts, OPT_rxmaxmtu, &rxMaxMTU);

    /* rxkad options */
    cmd_OptionAsFlag(opts, OPT_dotted, &rxkadDisableDotCheck);

    /* restricted query */
    if (cmd_OptionAsString(opts, OPT_restricted_query,
			   &restricted_query_parameter) == 0) {
	if (strcmp(restricted_query_parameter, "anyuser") == 0)
	    restrictedQueryLevel = RESTRICTED_QUERY_ANYUSER;
	else if (strcmp(restricted_query_parameter, "admin") == 0)
	    restrictedQueryLevel = RESTRICTED_QUERY_ADMIN;
	else {
	    printf("invalid argument for -restricted_query: %s\n",
		   restricted_query_parameter);
	    return -1;
	}
	free(restricted_query_parameter);
    }

    if (auditFileName) {
	osi_audit_file(auditFileName);
    }

    OpenLog(&logopts);
#ifdef AFS_PTHREAD_ENV
    opr_softsig_Init();
    SetupLogSoftSignals();
#else
    SetupLogSignals();
#endif

    tdir = afsconf_Open(configDir);
    if (!tdir) {
	VLog(0,
	    ("vlserver: can't open configuration files in dir %s, giving up.\n",
	     configDir));
	exit(1);
    }

    /* initialize audit user check */
    osi_audit_set_user_check(tdir, vldb_IsLocalRealmMatch);

#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	VLog(0, ("vlserver: couldn't initialize winsock. \n"));
	exit(1);
    }
#endif
    /* get this host */
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	VLog(0, ("vlserver: couldn't get address of this host (%s).\n",
	       hostname));
	exit(1);
    }
    memcpy(&myHost, th->h_addr, sizeof(afs_uint32));

#if !defined(AFS_HPUX_ENV) && !defined(AFS_NT40_ENV)
    signal(SIGXCPU, CheckSignal_Signal);
#endif
    /* get list of servers */
    code =
	afsconf_GetExtendedCellInfo(tdir, NULL, AFSCONF_VLDBSERVICE, &info,
				    clones);
    if (code) {
	printf("vlserver: Couldn't get cell server list for 'afsvldb'.\n");
	exit(2);
    }

    vldb_confdir = tdir;	/* Preserve our configuration dir */
    /* rxvab no longer supported */
    memset(&tkey, 0, sizeof(tkey));

    if (noAuth)
	afsconf_SetNoAuthFlag(tdir, 1);

    if (rxBind) {
	afs_int32 ccode;
#ifndef AFS_NT40_ENV
        if (AFSDIR_SERVER_NETRESTRICT_FILEPATH ||
            AFSDIR_SERVER_NETINFO_FILEPATH) {
            char reason[1024];
            ccode = afsconf_ParseNetFiles(SHostAddrs, NULL, NULL,
					  ADDRSPERSITE, reason,
					  AFSDIR_SERVER_NETINFO_FILEPATH,
					  AFSDIR_SERVER_NETRESTRICT_FILEPATH);
        } else
#endif
	{
            ccode = rx_getAllAddr(SHostAddrs, ADDRSPERSITE);
        }
        if (ccode == 1) {
            host = SHostAddrs[0];
	    rx_InitHost(host, htons(AFSCONF_VLDBPORT));
	}
    }

    if (!rxJumbograms) {
	rx_SetNoJumbo();
    }
    if (rxMaxMTU != -1) {
	if (rx_SetMaxMTU(rxMaxMTU) != 0) {
	    VLog(0, ("rxMaxMTU %d invalid\n", rxMaxMTU));
	    return -1;
	}
    }

    code = rx_Init(htons(AFSCONF_VLDBPORT));
    if (code < 0) {
        VLog(0, ("vlserver: Rx init failed: %d\n", code));
        exit(1);
    }
    rx_SetRxDeadTime(50);

    ubik_nBuffers = 512;
    ubik_SetClientSecurityProcs(afsconf_ClientAuth, afsconf_UpToDate, tdir);
    ubik_SetServerSecurityProcs(afsconf_BuildServerSecurityObjects,
				afsconf_CheckAuth, tdir);

    ubik_SyncWriterCacheProc = vlsynccache;
    code =
	ubik_ServerInitByInfo(myHost, htons(AFSCONF_VLDBPORT), &info, clones,
			      vl_dbaseName, &VL_dbase);
    if (code) {
	VLog(0, ("vlserver: Ubik init failed: %s\n", afs_error_message(code)));
	exit(2);
    }

    memset(rd_HostAddress, 0, sizeof(rd_HostAddress));
    memset(wr_HostAddress, 0, sizeof(wr_HostAddress));
    initialize_dstats();

    afsconf_BuildServerSecurityObjects(tdir, &securityClasses, &numClasses);

    tservice =
	rx_NewServiceHost(host, 0, USER_SERVICE_ID, "Vldb server",
			  securityClasses, numClasses,
			  VL_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	VLog(0, ("vlserver: Could not create VLDB_SERVICE rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    if (lwps < 4)
	lwps = 4;
    rx_SetMaxProcs(tservice, lwps);

    if (rxkadDisableDotCheck) {
        rx_SetSecurityConfiguration(tservice, RXS_CONFIG_FLAGS,
                                    (void *)RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
    }

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats",
			  securityClasses, numClasses,
			  RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	VLog(0, ("vlserver: Could not create rpc stats rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    LogCommandLine(argc, argv, "vlserver", VldbVersion, "Starting AFS", FSLog);
    if (afsconf_CountKeys(tdir) == 0) {
	VLog(0, ("WARNING: No encryption keys found! "
		 "All authenticated accesses will fail."
		 "Run akeyconvert or asetkey to import encryption keys.\n"));
    } else if (afsconf_GetLatestKey(tdir, NULL, NULL) == 0) {
	LogDesWarning();
    }
    VLog(0, ("%s\n", cml_version_number));

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(vldb_rxstat_userok);

    rx_StartServer(1);		/* Why waste this idle process?? */

    return 0; /* not reachable */
}
