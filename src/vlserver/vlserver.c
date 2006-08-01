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

RCSID
    ("$Header$");

#include <afs/stds.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
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

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <afs/auth.h>
#include <lock.h>
#include <ubik.h>
#include <afs/afsutil.h>
#include "vlserver.h"


#define MAXLWP 16
const char *vl_dbaseName;
struct afsconf_dir *vldb_confdir = 0;	/* vldb configuration dir */
int lwps = 9;

struct vldstats dynamic_statistics;
struct ubik_dbase *VL_dbase;
afs_uint32 HostAddress[MAXSERVERID + 1];
extern int afsconf_CheckAuth();
extern int afsconf_ServerAuth();

static CheckSignal();
int LogLevel = 0;
int smallMem = 0;
int rxJumbograms = 1;		/* default is to send and receive jumbo grams */
int rxMaxMTU = -1;
afs_int32 rxBind = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

static void
CheckSignal_Signal()
{
    IOMGR_SoftSig(CheckSignal, 0);
}

static
CheckSignal()
{
    register int i, errorcode;
    struct ubik_trans *trans;

    if (errorcode =
	Init_VLdbase(&trans, LOCKREAD, VLGETSTATS - VL_LOWEST_OPCODE))
	return errorcode;
    VLog(0, ("Dump name hash table out\n"));
    for (i = 0; i < HASHSIZE; i++) {
	HashNDump(trans, i);
    }
    VLog(0, ("Dump id hash table out\n"));
    for (i = 0; i < HASHSIZE; i++) {
	HashIdDump(trans, i);
    }
    return (ubik_EndTrans(trans));
}				/*CheckSignal */


/* Initialize the stats for the opcodes */
void
initialize_dstats()
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
vldb_rxstat_userok(call)
     struct rx_call *call;
{
    return afsconf_SuperUser(vldb_confdir, call, NULL);
}

/* Main server module */

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    register afs_int32 code;
    afs_int32 myHost;
    struct rx_service *tservice;
    struct rx_securityClass *sc[3];
    extern int VL_ExecuteRequest();
    extern int RXSTATS_ExecuteRequest();
    struct afsconf_dir *tdir;
    struct ktc_encryptionKey tkey;
    struct afsconf_cell info;
    struct hostent *th;
    char hostname[VL_MAXNAMELEN];
    int noAuth = 0, index, i;
    extern int rx_extraPackets;
    char commandLine[150];
    char clones[MAXHOSTSPERCELL];
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

	} else if (strcmp(argv[index], "-nojumbo") == 0) {
	    rxJumbograms = 0;

	} else if (strcmp(argv[index], "-rxbind") == 0) {
	    rxBind = 1;

	} else if (!strcmp(argv[index], "-rxmaxmtu")) {
	    if ((index + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n"); 
		return -1; 
	    }
	    rxMaxMTU = atoi(argv[++i]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) || 
		(rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d% invalid; must be between %d-%d\n",
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
	   int tempfd, flags;
           FILE *auditout;
           char oldName[MAXPATHLEN];
           char *fileName = argv[++index];

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
		   "[-rxmaxmtu <bytes>] [-rxbind] "
		   "[-auditlog <log path>] "
		   "[-syslog[=FACILITY]] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#else
	    printf("Usage: vlserver [-p <number of processes>] [-nojumbo] "
		   "[-rxmaxmtu <bytes>] [-rxbind] "
		   "[-auditlog <log path>] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#endif
	    fflush(stdout);
	    exit(0);
	}
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
	printf
	    ("vlserver: can't open configuration files in dir %s, giving up.\n",
	     AFSDIR_SERVER_ETC_DIRPATH);
	exit(1);
    }
#ifdef AFS_NT40_ENV
    /* initialize winsock */
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	fprintf(stderr, "vlserver: couldn't initialize winsock. \n");
	exit(1);
    }
#endif
    /* get this host */
    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th) {
	printf("vlserver: couldn't get address of this host (%s).\n",
	       hostname);
	exit(1);
    }
    memcpy(&myHost, th->h_addr, sizeof(afs_int32));

#if !defined(AFS_HPUX_ENV) && !defined(AFS_NT40_ENV)
    signal(SIGXCPU, CheckSignal_Signal);
#endif
    /* get list of servers */
    code =
	afsconf_GetExtendedCellInfo(tdir, NULL, AFSCONF_VLDBSERVICE, &info,
				    &clones);
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

    ubik_nBuffers = 512;
    ubik_CRXSecurityProc = afsconf_ClientAuth;
    ubik_CRXSecurityRock = (char *)tdir;
    ubik_SRXSecurityProc = afsconf_ServerAuth;
    ubik_SRXSecurityRock = (char *)tdir;
    ubik_CheckRXSecurityProc = afsconf_CheckAuth;
    ubik_CheckRXSecurityRock = (char *)tdir;
    code =
	ubik_ServerInitByInfo(myHost, htons(AFSCONF_VLDBPORT), &info, &clones,
			      vl_dbaseName, &VL_dbase);
    if (code) {
	printf("vlserver: Ubik init failed with code %d\n", code);
	exit(2);
    }
    if (!rxJumbograms) {
	rx_SetNoJumbo();
    }
    if (rxMaxMTU != -1) {
	rx_SetMaxMTU(rxMaxMTU);
    }
    rx_SetRxDeadTime(50);

    memset(HostAddress, 0, sizeof(HostAddress));
    initialize_dstats();

    sc[0] = rxnull_NewServerSecurityObject();
    sc[1] = (struct rx_securityClass *)0;
    sc[2] = rxkad_NewServerSecurityObject(0, tdir, afsconf_GetKey, NULL);

    tservice =
	rx_NewServiceHost(host, 0, USER_SERVICE_ID, "Vldb server", sc, 3,
		      VL_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	printf("vlserver: Could not create VLDB_SERVICE rx service\n");
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    if (lwps < 4)
	lwps = 4;
    rx_SetMaxProcs(tservice, lwps);

    tservice =
	rx_NewServiceHost(host, 0, RX_STATS_SERVICE_ID, "rpcstats", sc, 3,
		      RXSTATS_ExecuteRequest);
    if (tservice == (struct rx_service *)0) {
	printf("vlserver: Could not create rpc stats rx service\n");
	exit(3);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    for (commandLine[0] = '\0', i = 0; i < argc; i++) {
	if (i > 0)
	    strcat(commandLine, " ");
	strcat(commandLine, argv[i]);
    }
    ViceLog(0,
	    ("Starting AFS vlserver %d (%s)\n", VLDBVERSION_4, commandLine));
    printf("%s\n", cml_version_number);	/* Goes to the log */

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(vldb_rxstat_userok);

    rx_StartServer(1);		/* Why waste this idle process?? */
}
