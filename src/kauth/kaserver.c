/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Revision 2.3  1991/12/30  20:36:46
 * #837: Added AuthLog support for the kaserver
 *
 * Revision 2.2  90/08/29  15:10:50
 * Cleanups.
 * Don't create rxvab security object.
 * 
 * Revision 2.1  90/08/07  19:11:30
 * Start with clean version to sync test and dev trees.
 * */

#include <afs/param.h>
#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <winsock2.h>
#include <WINNT/afsevent.h>
#else
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include "kalog.h"           /* for OpenLog() */
#include <time.h>
#include <stdio.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <lwp.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <rx/rx_globals.h>
#include <afs/cellconfig.h>
#include <lock.h>
#include <afs/afsutil.h>
#include <ubik.h>
#include "kauth.h"
#include "kautils.h"
#include "kaserver.h"


struct kadstats dynamic_statistics;
struct ubik_dbase *KA_dbase;
afs_int32 myHost = 0;
afs_int32 verbose_track = 1;
struct afsconf_dir *KA_conf;		/* for getting cell info */

extern afs_int32 ubik_lastYesTime;
extern afs_int32 ubik_nBuffers;
int MinHours = 0;
int npwSums = KA_NPWSUMS;               /* needs to be variable sometime */

#include <stdarg.h>
#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
#undef vfprintf
#define vfprintf(stream,fmt,args) _doprnt(fmt,args,stream)
#endif

static int debugOutput;

/* check whether caller is authorized to manage RX statistics */
int KA_rxstat_userok(call)
    struct rx_call *call;
{
    return afsconf_SuperUser(KA_conf, call, (char *)0);
}

afs_int32 es_Report(char *fmt, ...)
{
    va_list pvar;

    if (debugOutput == 0) return 0;
    va_start (pvar, fmt);
    vfprintf (stderr, fmt, pvar);
    va_end(pvar);
    return 0;
}

static void initialize_dstats ()
{
    bzero (&dynamic_statistics, sizeof(dynamic_statistics));
    dynamic_statistics.start_time = time(0);
    dynamic_statistics.host = myHost;
}

static int convert_cell_to_ubik (cellinfo, myHost, serverList)
  struct afsconf_cell *cellinfo;
  afs_int32		      *myHost;
  afs_int32		      *serverList;
{   int  i;
    char hostname[64];
    struct hostent *th;

    /* get this host */
    gethostname(hostname,sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	ViceLog(0, ("kaserver: couldn't get address of this host.\n"));
	exit(1);
    }
    bcopy(th->h_addr,myHost,sizeof(afs_int32));

    for (i=0; i<cellinfo->numServers; i++)
	if (cellinfo->hostAddr[i].sin_addr.s_addr != *myHost) {
	    /* omit my host from serverList */
	    *serverList++ = cellinfo->hostAddr[i].sin_addr.s_addr;
	}
    *serverList = 0;			/* terminate list */
    return 0;
}

static afs_int32 kvno_admin_key (rock, kvno, key)
  char *rock;
  afs_int32  kvno;
  struct ktc_encryptionKey *key;
{
    return ka_LookupKvno (0, KA_ADMIN_NAME, KA_ADMIN_INST, kvno, key);

    /* we would like to start a Ubik transaction to fill the cache if that
       fails, but may deadlock as Rx is now organized. */
}

/* initFlags: 0x01  Do not require authenticated connections.
	      0x02  Do not check the bos NoAuth flag
	      0x04  Use fast key expiration to test oldkey code.
	      0x08  Temporary flag allowing database inconsistency fixup
 */

#include "AFS_component_version_number.c"

main (argc, argv)
  int   argc;
  char *argv[];
{
    afs_int32  code;
    char *whoami = argv[0];
    afs_int32  serverList[MAXSERVERS];
    struct afsconf_cell  cellinfo;
    char *cell;
    const char *cellservdb, *dbpath, *lclpath;
    int	  a;
    char  arg[32];
    char  default_lclpath[AFSDIR_PATH_MAX];
    int	  servers;
    int	  initFlags;
    int   level;			/* security level for Ubik */

    struct rx_service *tservice;
    struct rx_securityClass *sca[1];
    struct rx_securityClass *scm[3];
    
    extern int afsconf_ClientAuth();
    extern int afsconf_ClientAuthSecure();
    extern int afsconf_ServerAuth();
    extern int afsconf_CheckAuth();

    extern int rx_stackSize;
    extern struct rx_securityClass *rxnull_NewServerSecurityObject();
    extern int KAA_ExecuteRequest();
    extern int KAT_ExecuteRequest();
    extern int KAM_ExecuteRequest();
    extern int RXSTATS_ExecuteRequest();

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
    if (argc == 0) {
      usage:
        printf("Usage: kaserver [-noAuth] [-fastKeys] [-database <dbpath>] "
	       "[-localfiles <lclpath>] [-minhours <n>] [-servers <serverlist>] "
	       /*" [-enable_peer_stats] [-enable_process_stats] " */
	       "[-help]\n");
	exit(1);
    }
#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit()<0) {
      ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0,
			  argv[0],0);
      fprintf(stderr, "%s: Couldn't initialize winsock.\n", whoami);
      exit(1);
    }
#endif
    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr,"%s: Unable to obtain AFS server directory.\n", argv[0]);
	exit(2);
    }

    cellservdb = AFSDIR_SERVER_ETC_DIRPATH;
    dbpath = AFSDIR_SERVER_KADB_FILEPATH;
    strcompose(default_lclpath, AFSDIR_PATH_MAX,
	       AFSDIR_SERVER_LOCAL_DIRPATH, "/", AFSDIR_KADB_FILE, NULL);
    lclpath = default_lclpath;

    debugOutput = 0;
    servers = 0;
    initFlags = 0;
    level = rxkad_crypt;
    for (a=1; a<argc; a++) {
	int arglen = strlen(argv[a]);
	lcstring (arg, argv[a], sizeof(arg));
#define IsArg(a) (strncmp (arg,a, arglen) == 0)

	if (strcmp (arg, "-database") == 0) {
	    dbpath = argv[++a];
	    if (strcmp(lclpath, default_lclpath) == 0) lclpath = dbpath;
	}
	else if (strcmp (arg, "-localfiles") == 0) lclpath = argv[++a];
	else if (strcmp (arg, "-servers") == 0) debugOutput++, servers = 1;
	else if (strcmp (arg, "-noauth") == 0) debugOutput++, initFlags |= 1;
	else if (strcmp (arg, "-fastkeys") == 0) debugOutput++, initFlags |= 4;
	else if (strcmp (arg, "-dbfixup") == 0) debugOutput++, initFlags |= 8;
	else if (strcmp (arg, "-cellservdb") == 0) {
	    cellservdb = argv[++a];
	    initFlags |= 2;
	    debugOutput++;
	}

	else if (IsArg("-crypt")) level = rxkad_crypt;
	else if (IsArg("-safe")) level = rxkad_crypt;
	else if (IsArg("-clear")) level = rxkad_clear;
	else if (IsArg("-sorry")) level = rxkad_clear;
	else if (IsArg("-debug")) verbose_track = 0;
	else if (IsArg("-minhours")) {
             MinHours = atoi(argv[++a]);
        }
	else if (IsArg("-enable_peer_stats")) {
	    rx_enablePeerRPCStats();
	}
	else if (IsArg("-enable_process_stats")) {
	    rx_enableProcessRPCStats();
	}
	else if (*arg == '-') {
		/* hack to support help flag */
	    goto usage;
	}
    }
    if (code = ka_CellConfig (cellservdb)) goto abort;
    cell = ka_LocalCell();
    KA_conf = afsconf_Open (cellservdb);
    if (!KA_conf) {
	code = KANOCELLS;
      abort:
	com_err (whoami, code, "Failed getting cell info");
	exit (1);
    }
#ifdef        AUTH_DBM_LOG
    kalog_Init();
#else
    /* NT & HPUX do not have dbm package support. So we can only do some
     * text logging. So open the AuthLog file for logging and redirect
     * stdin and stdout to it 
     */
    OpenLog(AFSDIR_SERVER_KALOG_FILEPATH); 
    SetupLogSignals();
#endif
    if (servers) {
	if (code = ubik_ParseServerList(argc, argv, &myHost, serverList)) {
	    com_err(whoami, code, "Couldn't parse server list");
	    exit(1);
	}
    }
    else {
	code = afsconf_GetCellInfo (KA_conf, cell, AFSCONF_KAUTHSERVICE,
				    &cellinfo);
	code = convert_cell_to_ubik (&cellinfo, &myHost, serverList);
	if (code) goto abort;
	ViceLog (0, ("Using server list from %s cell database.\n", cell));
    }

    /* initialize ubik */
    if (level == rxkad_clear)
	ubik_CRXSecurityProc = afsconf_ClientAuth;
    else if (level == rxkad_crypt)
	ubik_CRXSecurityProc = afsconf_ClientAuthSecure;
    else {
	ViceLog(0, ("Unsupported security level %d\n", level));
	exit (5);
    }
    ViceLog (0, ("Using level %s for Ubik connections.\n", (level == rxkad_crypt ? "crypt": "clear")));
    ubik_CRXSecurityRock = (char *)KA_conf;
    ubik_SRXSecurityProc = afsconf_ServerAuth;
    ubik_SRXSecurityRock = (char *)KA_conf;
    ubik_CheckRXSecurityProc = afsconf_CheckAuth;
    ubik_CheckRXSecurityRock = (char *)KA_conf;

    ubik_nBuffers = 80;
    code = ubik_ServerInit (myHost, htons(AFSCONF_KAUTHPORT), serverList,
			    dbpath, &KA_dbase);
    if (code) {
	com_err(whoami, code, "Ubik init failed");
	exit(2);
    }

    sca[RX_SCINDEX_NULL] = rxnull_NewServerSecurityObject();

    /* These two lines disallow jumbograms */
    rx_maxReceiveSize = OLD_MAX_PACKET_SIZE;
    rxi_nSendFrags = rxi_nRecvFrags = 1;

    tservice =
	rx_NewService (0, KA_AUTHENTICATION_SERVICE, "AuthenticationService",
		       sca, 1, KAA_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create Authentication rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 1);
    rx_SetMaxProcs(tservice, 1);

    tservice =
	rx_NewService (0, KA_TICKET_GRANTING_SERVICE, "TicketGrantingService",
		       sca, 1, KAT_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create Ticket Granting rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 1);
    rx_SetMaxProcs(tservice, 1);

    scm[RX_SCINDEX_NULL] = sca[RX_SCINDEX_NULL];
    scm[RX_SCINDEX_VAB] = 0;
    scm[RX_SCINDEX_KAD] =
	rxkad_NewServerSecurityObject (rxkad_crypt, 0, kvno_admin_key, 0);
    tservice = rx_NewService(0, KA_MAINTENANCE_SERVICE, "Maintenance",
			     scm, 3, KAM_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create Maintenance rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 1);
    rx_SetMaxProcs(tservice, 1);
    rx_SetStackSize(tservice, 10000);

    tservice =
	rx_NewService (0, RX_STATS_SERVICE_ID, "rpcstats",
		       scm, 3, RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create rpc stats rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    initialize_dstats();

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(KA_rxstat_userok);

    rx_StartServer(0);			/* start handling req. of all types */

    if (init_kaprocs (lclpath, initFlags)) return -1;

    if (code = init_krb_udp()) {
	ViceLog (0, ("Failed to initialize UDP interface; code = %d.\n", code));
	ViceLog (0, ("Running without UDP access.\n"));
    }

    ViceLog (0, ("Starting to process AuthServer requests\n"));
    rx_ServerProc();			/* donate this LWP */
}
