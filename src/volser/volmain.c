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

#include <sys/types.h>
#include <string.h>
#ifdef AFS_NT40_ENV
#include <time.h>
#include <fcntl.h>
#include <windows.h>
#include <WINNT/afsevent.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <stdio.h>
#include <signal.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#include <afs/prs_fs.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <lock.h>
#include <afs/afssyscalls.h>
#include <afs/ihandle.h>
#ifdef AFS_NT40_ENV
#include <afs/ntops.h>
#endif
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/partition.h>
#include <rx/rx.h>
#include <rx/rx_globals.h>
#include <afs/auth.h>
#include <rx/rxkad.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "volser.h"
#include <errno.h>
#include <afs/audit.h>
#include <afs/afsutil.h>

/*@printflike@*/ extern void Log(const char *format, ...);
/*@printflike@*/ extern void Abort(const char *format, ...);

#define VolserVersion "2.0"
#define N_SECURITY_OBJECTS 3

extern struct Lock localLock;
extern struct volser_trans *TransList();
#ifndef AFS_PTHREAD_ENV
extern int (*vol_PollProc) ();
extern int IOMGR_Poll();
#endif
char *GlobalNameHack = NULL;
int hackIsIn = 0;
afs_int32 GlobalVolCloneId, GlobalVolParentId;
int GlobalVolType;
int VolumeChanged;		/* XXXX */
static char busyFlags[MAXHELPERS];
struct volser_trans *QI_GlobalWriteTrans = 0;
extern void AFSVolExecuteRequest();
extern void RXSTATS_ExecuteRequest();
struct afsconf_dir *tdir;
static afs_int32 runningCalls = 0;
int DoLogging = 0;
#define MAXLWP 16
int lwps = 9;
int udpBufSize = 0;		/* UDP buffer size for receive */

int rxBind = 0;

#define ADDRSPERSITE 16         /* Same global is in rx/rx_user.c */
afs_uint32 SHostAddrs[ADDRSPERSITE];

#define VS_EXIT(code)  {                                          \
                          osi_audit(VS_ExitEvent, code, AUD_END); \
			  exit(code);                             \
		       }

#if defined(AFS_PTHREAD_ENV)
char *
threadNum(void)
{
    return pthread_getspecific(rx_thread_id_key);
}
#endif

static afs_int32
MyBeforeProc(struct rx_call *acall)
{
    VTRANS_LOCK;
    runningCalls++;
    VTRANS_UNLOCK;
    return 0;
}

static afs_int32
MyAfterProc(struct rx_call *acall, afs_int32 code)
{
    VTRANS_LOCK;
    runningCalls--;
    VTRANS_UNLOCK;
    return 0;
}

/* Called every GCWAKEUP seconds to try to unlock all our partitions,
 * if we're idle and there are no active transactions 
 */
static void
TryUnlock()
{
    /* if there are no running calls, and there are no active transactions, then
     * it should be safe to release any partition locks we've accumulated */
    VTRANS_LOCK;
    if (runningCalls == 0 && TransList() == (struct volser_trans *)0) {
        VTRANS_UNLOCK;
	VPFullUnlock();		/* in volprocs.c */
    } else
        VTRANS_UNLOCK;
}

/* background daemon for timing out transactions */
static void
BKGLoop()
{
    struct timeval tv;
    int loop = 0;

    while (1) {
	tv.tv_sec = GCWAKEUP;
	tv.tv_usec = 0;
#ifdef AFS_PTHREAD_ENV
        select(0, 0, 0, 0, &tv);
#else
	(void)IOMGR_Select(0, 0, 0, 0, &tv);
#endif
	GCTrans();
	TryUnlock();
	loop++;
	if (loop == 10) {	/* reopen log every 5 minutes */
	    loop = 0;
	    ReOpenLog(AFSDIR_SERVER_VOLSERLOG_FILEPATH);
	}
    }
}

/* Background daemon for sleeping so the volserver does not become I/O bound */
afs_int32 TTsleep, TTrun;
static void
BKGSleep()
{
    struct volser_trans *tt;

    if (TTsleep) {
	while (1) {
#ifdef AFS_PTHREAD_ENV
	    sleep(TTrun);
#else /* AFS_PTHREAD_ENV */
	    IOMGR_Sleep(TTrun);
#endif
	    VTRANS_LOCK;
	    for (tt = TransList(); tt; tt = tt->next) {
		if ((strcmp(tt->lastProcName, "DeleteVolume") == 0)
		    || (strcmp(tt->lastProcName, "Clone") == 0)
		    || (strcmp(tt->lastProcName, "ReClone") == 0)
		    || (strcmp(tt->lastProcName, "Forward") == 0)
		    || (strcmp(tt->lastProcName, "Restore") == 0)
		    || (strcmp(tt->lastProcName, "ForwardMulti") == 0))
		    break;
	    }
	    if (tt) {
	        VTRANS_UNLOCK;
		sleep(TTsleep);
	    } else
	        VTRANS_UNLOCK;
	}
    }
}

#ifndef AFS_NT40_ENV
int
volser_syscall(afs_uint32 a3, afs_uint32 a4, void *a5)
{
    afs_uint32 rcode;
    void (*old) ();

#ifndef AFS_LINUX20_ENV
    old = signal(SIGSYS, SIG_IGN);
#endif
    rcode =
	syscall(AFS_SYSCALL /* AFS_SYSCALL */ , 28 /* AFSCALL_CALL */ , a3,
		a4, a5);
#ifndef AFS_LINUX20_ENV
    signal(SIGSYS, old);
#endif

    return rcode;
}
#endif


/* check whether caller is authorized to manage RX statistics */
int
vol_rxstat_userok(struct rx_call *call)
{
    return afsconf_SuperUser(tdir, call, NULL);
}

#include "AFS_component_version_number.c"
int 
main(int argc, char **argv)
{
    register afs_int32 code;
    struct rx_securityClass *(securityObjects[3]);
    struct rx_service *service;
    struct ktc_encryptionKey tkey;
    int rxpackets = 100;
    char commandLine[150];
    int i;
    int rxJumbograms = 1;	/* default is to send and receive jumbograms. */
    int rxMaxMTU = -1;
    int bufSize = 0;		/* temp variable to read in udp socket buf size */
    afs_uint32 host = ntohl(INADDR_ANY);

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
    osi_audit(VS_StartEvent, 0, AUD_END);

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }

    for (commandLine[0] = '\0', i = 0; i < argc; i++) {
	if (i > 0)
	    strcat(commandLine, " ");
	strcat(commandLine, argv[i]);
    }

    TTsleep = TTrun = 0;

    /* parse cmd line */
    for (code = 1; code < argc; code++) {
	if (strcmp(argv[code], "-log") == 0) {
	    /* set extra logging flag */
	    DoLogging = 1;
	} else if (strcmp(argv[code], "-help") == 0) {
	    goto usage;
	} else if (strcmp(argv[code], "-rxbind") == 0) {
	    rxBind = 1;
	} else if (strcmp(argv[code], "-p") == 0) {
	    lwps = atoi(argv[++code]);
	    if (lwps > MAXLWP) {
		printf("Warning: '-p %d' is too big; using %d instead\n",
		       lwps, MAXLWP);
		lwps = MAXLWP;
	    }
	} else if (strcmp(argv[code], "-auditlog") == 0) {
	    int tempfd, flags;
	    FILE *auditout;
	    char oldName[MAXPATHLEN];
	    char *fileName = argv[++code];

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
		    osi_audit(VS_StartEvent, 0, AUD_END);
		} else
		    printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	    } else
		printf("Warning: auditlog %s not writable, ignored.\n", fileName);
	} else if (strcmp(argv[code], "-nojumbo") == 0) {
	    rxJumbograms = 0;
	} else if (!strcmp(argv[code], "-rxmaxmtu")) {
	    if ((code + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n"); 
		exit(1); 
	    }
	    rxMaxMTU = atoi(argv[++code]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) || 
		(rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d% invalid; must be between %d-%d\n",
		       rxMaxMTU, RX_MIN_PACKET_SIZE, 
		       RX_MAX_PACKET_DATA_SIZE);
		exit(1);
	    }
	} else if (strcmp(argv[code], "-sleep") == 0) {
	    sscanf(argv[++code], "%d/%d", &TTsleep, &TTrun);
	    if ((TTsleep < 0) || (TTrun <= 0)) {
		printf("Warning: '-sleep %d/%d' is incorrect; ignoring\n",
		       TTsleep, TTrun);
		TTsleep = TTrun = 0;
	    }
	} else if (strcmp(argv[code], "-udpsize") == 0) {
	    if ((code + 1) >= argc) {
		printf("You have to specify -udpsize <integer value>\n");
		exit(1);
	    }
	    sscanf(argv[++code], "%d", &bufSize);
	    if (bufSize < rx_GetMinUdpBufSize())
		printf
		    ("Warning:udpsize %d is less than minimum %d; ignoring\n",
		     bufSize, rx_GetMinUdpBufSize());
	    else
		udpBufSize = bufSize;
	} else if (strcmp(argv[code], "-enable_peer_stats") == 0) {
	    rx_enablePeerRPCStats();
	} else if (strcmp(argv[code], "-enable_process_stats") == 0) {
	    rx_enableProcessRPCStats();
	}
#ifndef AFS_NT40_ENV
	else if (strcmp(argv[code], "-syslog") == 0) {
	    /* set syslog logging flag */
	    serverLogSyslog = 1;
	} else if (strncmp(argv[code], "-syslog=", 8) == 0) {
	    serverLogSyslog = 1;
	    serverLogSyslogFacility = atoi(argv[code] + 8);
	}
#endif
	else {
	    printf("volserver: unrecognized flag '%s'\n", argv[code]);
	  usage:
#ifndef AFS_NT40_ENV
	    printf("Usage: volserver [-log] [-p <number of processes>] "
		   "[-auditlog <log path>] "
		   "[-nojumbo] [-rxmaxmtu <bytes>] [-rxbind] "
		   "[-udpsize <size of socket buffer in bytes>] "
		   "[-syslog[=FACILITY]] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#else
	    printf("Usage: volserver [-log] [-p <number of processes>] "
		   "[-auditlog <log path>] "
		   "[-nojumbo] [-rxmaxmtu <bytes>] [-rxbind] "
		   "[-udpsize <size of socket buffer in bytes>] "
		   "[-enable_peer_stats] [-enable_process_stats] "
		   "[-help]\n");
#endif
	    VS_EXIT(1);
	}
    }
#ifdef AFS_SGI_VNODE_GLUE
    if (afs_init_kernel_config(-1) < 0) {
	printf
	    ("Can't determine NUMA configuration, not starting volserver.\n");
	exit(1);
    }
#endif
    InitErrTabs();

#ifdef AFS_PTHREAD_ENV
    SetLogThreadNumProgram( threadNum );
#endif

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	printf("Volume server unable to start winsock, exiting.\n");
	exit(1);
    }
#endif
    /* Open VolserLog and map stdout, stderr into it; VInitVolumePackage can
       log, so we need to do this here */
    OpenLog(AFSDIR_SERVER_VOLSERLOG_FILEPATH);

    VInitVolumePackage(volumeUtility, 0, 0, CONNECT_FS, 0);
    /* For nuke() */
    Lock_Init(&localLock);
    DInit(40);
#ifndef AFS_PTHREAD_ENV
    vol_PollProc = IOMGR_Poll;	/* tell vol pkg to poll io system periodically */
#endif
#ifndef AFS_NT40_ENV
    rxi_syscallp = volser_syscall;
#endif
    rx_nPackets = rxpackets;	/* set the max number of packets */
    if (udpBufSize)
	rx_SetUdpBufSize(udpBufSize);	/* set the UDP buffer size for receive */
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
        if (ccode == 1) 
            host = SHostAddrs[0];
    }

    code = rx_InitHost(host, (int)htons(AFSCONF_VOLUMEPORT));
    if (code) {
	fprintf(stderr, "rx init failed on socket AFSCONF_VOLUMEPORT %u\n",
		AFSCONF_VOLUMEPORT);
	VS_EXIT(1);
    }
    if (!rxJumbograms) {
	/* Don't allow 3.4 vos clients to send jumbograms and we don't send. */
	rx_SetNoJumbo();
    }
    if (rxMaxMTU != -1) {
	rx_SetMaxMTU(rxMaxMTU);
    }
    rx_GetIFInfo();
    rx_SetRxDeadTime(420);
    memset(busyFlags, 0, sizeof(busyFlags));

    SetupLogSignals();

    {
#ifdef AFS_PTHREAD_ENV
	pthread_t tid;
	pthread_attr_t tattr;
	assert(pthread_attr_init(&tattr) == 0);
	assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);

	assert(pthread_create(&tid, &tattr, (void *)BKGLoop, NULL) == 0);
#else
	PROCESS pid;
	LWP_CreateProcess(BKGLoop, 16*1024, 3, 0, "vol bkg daemon", &pid);
	LWP_CreateProcess(BKGSleep,16*1024, 3, 0, "vol slp daemon", &pid);
#endif
    }

    /* Create a single security object, in this case the null security object, for unauthenticated connections, which will be used to control security on connections made to this server */

    tdir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!tdir) {
	Abort("volser: could not open conf files in %s\n",
	      AFSDIR_SERVER_ETC_DIRPATH);
	VS_EXIT(1);
    }
    afsconf_GetKey(tdir, 999, &tkey);
    securityObjects[0] = rxnull_NewServerSecurityObject();
    securityObjects[1] = (struct rx_securityClass *)0;	/* don't bother with rxvab */
    securityObjects[2] =
	rxkad_NewServerSecurityObject(0, tdir, afsconf_GetKey, NULL);
    if (securityObjects[0] == (struct rx_securityClass *)0)
	Abort("rxnull_NewServerSecurityObject");
    service =
	rx_NewServiceHost(host, 0, VOLSERVICE_ID, "VOLSER", securityObjects, 3,
		      AFSVolExecuteRequest);
    if (service == (struct rx_service *)0)
	Abort("rx_NewService");
    rx_SetBeforeProc(service, MyBeforeProc);
    rx_SetAfterProc(service, MyAfterProc);
    rx_SetIdleDeadTime(service, 0);	/* never timeout */
    if (lwps < 4)
	lwps = 4;
    rx_SetMaxProcs(service, lwps);
#if defined(AFS_XBSD_ENV)
    rx_SetStackSize(service, (128 * 1024));
#elif defined(AFS_SGI_ENV)
    rx_SetStackSize(service, (48 * 1024));
#else
    rx_SetStackSize(service, (32 * 1024));
#endif

    service =
	rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats", securityObjects, 3,
		      RXSTATS_ExecuteRequest);
    if (service == (struct rx_service *)0)
	Abort("rx_NewService");
    rx_SetMinProcs(service, 2);
    rx_SetMaxProcs(service, 4);

    Log("Starting AFS Volserver %s (%s)\n", VolserVersion, commandLine);
    if (TTsleep) {
	Log("Will sleep %d second%s every %d second%s\n", TTsleep,
	    (TTsleep > 1) ? "s" : "", TTrun + TTsleep,
	    (TTrun + TTsleep > 1) ? "s" : "");
    }

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(vol_rxstat_userok);

    rx_StartServer(1);		/* Donate this process to the server process pool */

    osi_audit(VS_FinishEvent, (-1), AUD_END);
    Abort("StartServer returned?");
    return 0; /* not reached */
}
