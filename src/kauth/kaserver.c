/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <WINNT/afsevent.h>
#else
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#endif
#include "kalog.h"		/* for OpenLog() */
#include <time.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
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
#include <sys/stat.h>
#include "kauth.h"
#include "kautils.h"
#include "kaserver.h"


struct kadstats dynamic_statistics;
struct ubik_dbase *KA_dbase;
afs_int32 myHost = 0;
afs_int32 verbose_track = 1;
afs_int32 krb4_cross = 0;
afs_int32 rxBind = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

struct afsconf_dir *KA_conf;	/* for getting cell info */

int MinHours = 0;
int npwSums = KA_NPWSUMS;	/* needs to be variable sometime */

#include <stdarg.h>
#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_DARWIN_ENV) && !defined(AFS_XBSD_ENV)
#undef vfprintf
#define vfprintf(stream,fmt,args) _doprnt(fmt,args,stream)
#endif

static int debugOutput;

/* check whether caller is authorized to manage RX statistics */
int
KA_rxstat_userok(call)
     struct rx_call *call;
{
    return afsconf_SuperUser(KA_conf, call, NULL);
}

afs_int32
es_Report(char *fmt, ...)
{
    va_list pvar;

    if (debugOutput == 0)
	return 0;
    va_start(pvar, fmt);
    vfprintf(stderr, fmt, pvar);
    va_end(pvar);
    return 0;
}

static void
initialize_dstats()
{
    memset(&dynamic_statistics, 0, sizeof(dynamic_statistics));
    dynamic_statistics.start_time = time(0);
    dynamic_statistics.host = myHost;
}

static int
convert_cell_to_ubik(cellinfo, myHost, serverList)
     struct afsconf_cell *cellinfo;
     afs_int32 *myHost;
     afs_int32 *serverList;
{
    int i;
    char hostname[64];
    struct hostent *th;

    /* get this host */
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	ViceLog(0, ("kaserver: couldn't get address of this host.\n"));
	exit(1);
    }
    memcpy(myHost, th->h_addr, sizeof(afs_int32));

    for (i = 0; i < cellinfo->numServers; i++)
	if (cellinfo->hostAddr[i].sin_addr.s_addr != *myHost) {
	    /* omit my host from serverList */
	    *serverList++ = cellinfo->hostAddr[i].sin_addr.s_addr;
	}
    *serverList = 0;		/* terminate list */
    return 0;
}

static afs_int32
kvno_admin_key(rock, kvno, key)
     char *rock;
     afs_int32 kvno;
     struct ktc_encryptionKey *key;
{
    return ka_LookupKvno(0, KA_ADMIN_NAME, KA_ADMIN_INST, kvno, key);

    /* we would like to start a Ubik transaction to fill the cache if that
     * fails, but may deadlock as Rx is now organized. */
}

/* initFlags: 0x01  Do not require authenticated connections.
	      0x02  Do not check the bos NoAuth flag
	      0x04  Use fast key expiration to test oldkey code.
	      0x08  Temporary flag allowing database inconsistency fixup
 */

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char *argv[];
{
    afs_int32 code;
    char *whoami = argv[0];
    afs_int32 serverList[MAXSERVERS];
    struct afsconf_cell cellinfo;
    char *cell;
    const char *cellservdb, *dbpath, *lclpath;
    int a;
    char arg[32];
    char default_lclpath[AFSDIR_PATH_MAX];
    int servers;
    int initFlags;
    int level;			/* security level for Ubik */
    afs_int32 i;
    char clones[MAXHOSTSPERCELL];
    afs_uint32 host = ntohl(INADDR_ANY);

    struct rx_service *tservice;
    struct rx_securityClass *sca[1];
    struct rx_securityClass *scm[3];

    extern int afsconf_ClientAuthSecure();
    extern int afsconf_ServerAuth();
    extern int afsconf_CheckAuth();

    extern int rx_stackSize;
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
    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_Kaserver, osi_NULL)));
    osi_audit_init();

    if (argc == 0) {
      usage:
	printf("Usage: kaserver [-noAuth] [-fastKeys] [-database <dbpath>] "
	       "[-auditlog <log path>] [-rxbind] "
	       "[-localfiles <lclpath>] [-minhours <n>] [-servers <serverlist>] "
	       "[-crossrealm]"
	       /*" [-enable_peer_stats] [-enable_process_stats] " */
	       "[-help]\n");
	exit(1);
    }
#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	fprintf(stderr, "%s: Couldn't initialize winsock.\n", whoami);
	exit(1);
    }
#endif
    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    cellservdb = AFSDIR_SERVER_ETC_DIRPATH;
    dbpath = AFSDIR_SERVER_KADB_FILEPATH;
    strcompose(default_lclpath, AFSDIR_PATH_MAX, AFSDIR_SERVER_LOCAL_DIRPATH,
	       "/", AFSDIR_KADB_FILE, NULL);
    lclpath = default_lclpath;

    debugOutput = 0;
    servers = 0;
    initFlags = 0;
    level = rxkad_crypt;
    for (a = 1; a < argc; a++) {
	int arglen = strlen(argv[a]);
	lcstring(arg, argv[a], sizeof(arg));
#define IsArg(a) (strncmp (arg,a, arglen) == 0)

	if (strcmp(arg, "-database") == 0) {
	    dbpath = argv[++a];
	    if (strcmp(lclpath, default_lclpath) == 0)
		lclpath = dbpath;
	}
	else if (strncmp(arg, "-auditlog", arglen) == 0) {
	    int tempfd, flags;
	    FILE *auditout;
	    char oldName[MAXPATHLEN];
	    char *fileName = argv[++a];
	    
#ifndef AFS_NT40_ENV
	    struct stat statbuf;
	    
	    if ((lstat(fileName, &statbuf) == 0) 
		&& (S_ISFIFO(statbuf.st_mode))) {
		flags = O_WRONLY | O_NONBLOCK;
	    } else 
#endif
	    {
		strcpy(oldName, fileName);
		strcat(oldName, ".old");
		renamefile(fileName, oldName);
		flags = O_WRONLY | O_TRUNC | O_CREAT;
	    }
	    tempfd = open(fileName, flags, 0666);
	    if (tempfd > -1) {
		auditout = fdopen(tempfd, "a");
		if (auditout) {
		    osi_audit_file(auditout);
		} else
		    printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	    } else
		printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	} else if (strcmp(arg, "-localfiles") == 0)
	    lclpath = argv[++a];
	else if (strcmp(arg, "-servers") == 0)
	    debugOutput++, servers = 1;
	else if (strcmp(arg, "-noauth") == 0)
	    debugOutput++, initFlags |= 1;
	else if (strcmp(arg, "-fastkeys") == 0)
	    debugOutput++, initFlags |= 4;
	else if (strcmp(arg, "-dbfixup") == 0)
	    debugOutput++, initFlags |= 8;
	else if (strcmp(arg, "-cellservdb") == 0) {
	    cellservdb = argv[++a];
	    initFlags |= 2;
	    debugOutput++;
	}

	else if (IsArg("-crypt"))
	    level = rxkad_crypt;
	else if (IsArg("-safe"))
	    level = rxkad_crypt;
	else if (IsArg("-clear"))
	    level = rxkad_clear;
	else if (IsArg("-sorry"))
	    level = rxkad_clear;
	else if (IsArg("-debug"))
	    verbose_track = 0;
	else if (IsArg("-crossrealm"))
	    krb4_cross = 1;
	else if (IsArg("-rxbind"))
	    rxBind = 1;
	else if (IsArg("-minhours")) {
	    MinHours = atoi(argv[++a]);
	} else if (IsArg("-enable_peer_stats")) {
	    rx_enablePeerRPCStats();
	} else if (IsArg("-enable_process_stats")) {
	    rx_enableProcessRPCStats();
	} else if (*arg == '-') {
	    /* hack to support help flag */
	    goto usage;
	}
    }
    if (code = ka_CellConfig(cellservdb))
	goto abort;
    cell = ka_LocalCell();
    KA_conf = afsconf_Open(cellservdb);
    if (!KA_conf) {
	code = KANOCELLS;
      abort:
	com_err(whoami, code, "Failed getting cell info");
	exit(1);
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

    fprintf(stderr, "%s: WARNING: kaserver is deprecated due to its weak security "
	    "properties.  Migrating to a Kerberos 5 KDC is advised.  "
	    "http://www.openafs.org/no-more-des.html\n", whoami);
    ViceLog(0, ("WARNING: kaserver is deprecated due to its weak security properties.  "
	    "Migrating to a Kerberos 5 KDC is advised.  "
	    "http://www.openafs.org/no-more-des.html\n"));

    code =
	afsconf_GetExtendedCellInfo(KA_conf, cell, AFSCONF_KAUTHSERVICE,
				    &cellinfo, &clones);
    if (servers) {
	if (code = ubik_ParseServerList(argc, argv, &myHost, serverList)) {
	    com_err(whoami, code, "Couldn't parse server list");
	    exit(1);
	}
	cellinfo.hostAddr[0].sin_addr.s_addr = myHost;
	for (i = 1; i < MAXSERVERS; i++) {
	    if (!serverList[i])
		break;
	    cellinfo.hostAddr[i].sin_addr.s_addr = serverList[i];
	}
	cellinfo.numServers = i;
    } else {
	code = convert_cell_to_ubik(&cellinfo, &myHost, serverList);
	if (code)
	    goto abort;
	ViceLog(0, ("Using server list from %s cell database.\n", cell));
    }

    /* initialize ubik */
    if (level == rxkad_clear)
	ubik_CRXSecurityProc = afsconf_ClientAuth;
    else if (level == rxkad_crypt)
	ubik_CRXSecurityProc = afsconf_ClientAuthSecure;
    else {
	ViceLog(0, ("Unsupported security level %d\n", level));
	exit(5);
    }
    ViceLog(0,
	    ("Using level %s for Ubik connections.\n",
	     (level == rxkad_crypt ? "crypt" : "clear")));
    ubik_CRXSecurityRock = (char *)KA_conf;
    ubik_SRXSecurityProc = afsconf_ServerAuth;
    ubik_SRXSecurityRock = (char *)KA_conf;
    ubik_CheckRXSecurityProc = afsconf_CheckAuth;
    ubik_CheckRXSecurityRock = (char *)KA_conf;

    ubik_nBuffers = 80;

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
	    rx_InitHost(host, htons(AFSCONF_KAUTHPORT));
	}
    }

    if (servers)
	code =
	    ubik_ServerInit(myHost, htons(AFSCONF_KAUTHPORT), serverList,
			    dbpath, &KA_dbase);
    else
	code =
	    ubik_ServerInitByInfo(myHost, htons(AFSCONF_KAUTHPORT), &cellinfo,
				  &clones, dbpath, &KA_dbase);

    if (code) {
	com_err(whoami, code, "Ubik init failed");
	exit(2);
    }

    sca[RX_SCINDEX_NULL] = rxnull_NewServerSecurityObject();

    /* Disable jumbograms */
    rx_SetNoJumbo();

    tservice =
	rx_NewServiceHost(host, 0, KA_AUTHENTICATION_SERVICE, 
			  "AuthenticationService", sca, 1, KAA_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create Authentication rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 1);
    rx_SetMaxProcs(tservice, 1);

    
    tservice =
	rx_NewServiceHost(host, 0, KA_TICKET_GRANTING_SERVICE, "TicketGrantingService",
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
	rxkad_NewServerSecurityObject(rxkad_crypt, 0, kvno_admin_key, 0);
    tservice =
	rx_NewServiceHost(host, 0, KA_MAINTENANCE_SERVICE, "Maintenance", scm, 3,
		      KAM_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create Maintenance rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 1);
    rx_SetMaxProcs(tservice, 1);
    rx_SetStackSize(tservice, 10000);

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats", scm, 3,
		      RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	ViceLog(0, ("Could not create rpc stats rx service\n"));
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    initialize_dstats();

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(KA_rxstat_userok);

    rx_StartServer(0);		/* start handling req. of all types */

    if (init_kaprocs(lclpath, initFlags))
	return -1;

    if (code = init_krb_udp()) {
	ViceLog(0,
		("Failed to initialize UDP interface; code = %d.\n", code));
	ViceLog(0, ("Running without UDP access.\n"));
    }

    ViceLog(0, ("Starting to process AuthServer requests\n"));
    rx_ServerProc();		/* donate this LWP */
    return 0;
}
