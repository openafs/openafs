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
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#include <time.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <rx/rxstat.h>
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

#define MAXLWP 16
const char *vl_dbaseName;
struct afsconf_dir *vldb_confdir = 0;	/* vldb configuration dir */
int lwps = 9;

struct vldstats dynamic_statistics;
struct ubik_dbase *VL_dbase;
afs_uint32 rd_HostAddress[MAXSERVERID + 1];

static void *CheckSignal(void*);
int LogLevel = 0;
int smallMem = 0;
int rxJumbograms = 0;		/* default is to not send and receive jumbo grams */
int rxMaxMTU = -1;
afs_int32 rxBind = 0;
int rxkadDisableDotCheck = 0;
int debuglevel = 0;

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

/* Main server module */

#include "AFS_component_version_number.c"

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
    int noAuth = 0, index;
    char clones[MAXHOSTSPERCELL];
    char *auditFileName = NULL;
    afs_uint32 host = ntohl(INADDR_ANY);

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

    /* Parse command line */
    for (index = 1; index < argc; index++) {
	if (strcmp(argv[index], "-noauth") == 0) {
	    noAuth = 1;
	} else if (strcmp(argv[index], "-p") == 0) {
	    lwps = atoi(argv[++index]);
	    if (lwps > MAXLWP) {
		printf("Warning: '-p %d' is too big; using %d instead\n",
		       lwps, MAXLWP);
		lwps = MAXLWP;
	    }
	} else if (strcmp(argv[index], "-d") == 0) {
	    if ((index + 1) >= argc) {
		fprintf(stderr, "missing argument for -d\n");
		return -1;
	    }
	    debuglevel = atoi(argv[++index]);
	    LogLevel = debuglevel;
	} else if (strcmp(argv[index], "-nojumbo") == 0) {
	    rxJumbograms = 0;
	} else if (strcmp(argv[index], "-jumbo") == 0) {
	    rxJumbograms = 1;
	} else if (strcmp(argv[index], "-rxbind") == 0) {
	    rxBind = 1;
	} else if (strcmp(argv[index], "-allow-dotted-principals") == 0) {
	    rxkadDisableDotCheck = 1;
	} else if (!strcmp(argv[index], "-rxmaxmtu")) {
	    if ((index + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n");
		return -1;
	    }
	    rxMaxMTU = atoi(argv[++index]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) ||
		(rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d invalid; must be between %d-%" AFS_SIZET_FMT "\n",
		       rxMaxMTU, RX_MIN_PACKET_SIZE,
		       RX_MAX_PACKET_DATA_SIZE);
		return -1;
	    }

	} else if (strcmp(argv[index], "-smallmem") == 0) {
	    smallMem = 1;

	} else if (strcmp(argv[index], "-trace") == 0) {
	    extern char rxi_tracename[80];
	    strcpy(rxi_tracename, argv[++index]);

	} else if (strcmp(argv[index], "-auditlog") == 0) {
	    auditFileName = argv[++index];

	} else if (strcmp(argv[index], "-audit-interface") == 0) {
	    char *interface = argv[++index];

	    if (osi_audit_interface(interface)) {
		printf("Invalid audit interface '%s'\n", interface);
		return -1;
	    }

	} else if (strcmp(argv[index], "-enable_peer_stats") == 0) {
	    rx_enablePeerRPCStats();
	} else if (strcmp(argv[index], "-enable_process_stats") == 0) {
	    rx_enableProcessRPCStats();
#ifndef AFS_NT40_ENV
	} else if (strcmp(argv[index], "-syslog") == 0) {
	    /* set syslog logging flag */
	    serverLogSyslog = 1;
	} else if (strncmp(argv[index], "-syslog=", 8) == 0) {
	    serverLogSyslog = 1;
	    serverLogSyslogFacility = atoi(argv[index] + 8);
#endif
	} else {
	    /* support help flag */
#ifndef AFS_NT40_ENV
	    printf("Usage: vlserver [-p <number of processes>] [-nojumbo] "
		   "[-rxmaxmtu <bytes>] [-rxbind] [-allow-dotted-principals] "
		   "[-auditlog <log path>] [-jumbo] [-d <debug level>] "
		   "[-syslog[=FACILITY]] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#else
	    printf("Usage: vlserver [-p <number of processes>] [-nojumbo] "
		   "[-rxmaxmtu <bytes>] [-rxbind] [-allow-dotted-principals] "
		   "[-auditlog <log path>] [-jumbo] [-d <debug level>] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#endif
	    fflush(stdout);
	    exit(0);
	}
    }

    if (auditFileName) {
	osi_audit_file(auditFileName);
    }

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }
    vl_dbaseName = AFSDIR_SERVER_VLDB_FILEPATH;

#ifndef AFS_NT40_ENV
    serverLogSyslogTag = "vlserver";
#endif
    OpenLog(AFSDIR_SERVER_VLOG_FILEPATH);	/* set up logging */
    SetupLogSignals();

    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	VLog(0,
	    ("vlserver: can't open configuration files in dir %s, giving up.\n",
	     AFSDIR_SERVER_ETC_DIRPATH));
	exit(1);
    }
#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	VLog(0, ("vlserver: couldn't initialize winsock.\n"));
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
	VLog(0, ("vlserver: Couldn't get cell server list for 'afsvldb'.\n"));
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
            ccode = parseNetFiles(SHostAddrs, NULL, NULL,
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
	rx_SetMaxMTU(rxMaxMTU);
    }

    ubik_nBuffers = 512;
    ubik_CRXSecurityProc = afsconf_ClientAuth;
    ubik_CRXSecurityRock = (char *)tdir;
    ubik_SRXSecurityProc = afsconf_ServerAuth;
    ubik_SRXSecurityRock = (char *)tdir;
    ubik_CheckRXSecurityProc = afsconf_CheckAuth;
    ubik_CheckRXSecurityRock = (char *)tdir;
    code =
	ubik_ServerInitByInfo(myHost, htons(AFSCONF_VLDBPORT), &info, clones,
			      vl_dbaseName, &VL_dbase);
    if (code) {
	printf("vlserver: Ubik init failed: %s\n", afs_error_message(code));
	exit(2);
    }
    rx_SetRxDeadTime(50);

    memset(rd_HostAddress, 0, sizeof(rd_HostAddress));
    initialize_dstats();

    afsconf_BuildServerSecurityObjects(tdir, 0, &securityClasses, &numClasses);

    tservice =
	rx_NewServiceHost(host, 0, USER_SERVICE_ID, "Vldb server",
			  securityClasses, numClasses,
			  VL_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	printf("vlserver: Could not create VLDB_SERVICE rx service\n");
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
    if (afsconf_GetLatestKey(tdir, NULL, NULL) == 0) {
	LogDesWarning();
    }
    printf("%s\n", cml_version_number);	/* Goes to the log */

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(vldb_rxstat_userok);

    rx_StartServer(1);		/* Why waste this idle process?? */

    return 0; /* not reachable */
}
