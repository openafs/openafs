/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*  viced.c	- File Server main loop					 */
/* 									 */
/*  Date: 5/1/85							 */
/* 									 */
/*  Function  	- This routine has the initialization code for		 */
/* 		  FileServer II						 */
/* 									 */
/* ********************************************************************** */

#include <afs/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <afs/procmgmt.h>  /* signal(), kill(), wait(), etc. */
#include <sys/stat.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <WINNT/afsevent.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/resource.h>
#include <unistd.h>	/* sysconf() */
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#endif /* AFS_NT40_ENV */
#include <afs/stds.h>
#undef SHARED
#include <rx/xdr.h>
#include <afs/nfs.h>
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include <afs/assert.h>
#endif /* AFS_PTHREAD_ENV */
#include <lwp.h>
#include <lock.h>
#include <afs/ptclient.h>
#include <afs/afsint.h>
#include <afs/vldbint.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/acl.h>
#include <afs/prs_fs.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/keys.h>
#include <afs/afs_args.h>
#include <afs/vlserver.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#ifndef AFS_NT40_ENV
#include <afs/netutils.h>
#endif
#include "viced.h"
#include "host.h"
#if defined(AFS_SGI_ENV)
#include "sys/schedctl.h"
#include "sys/lock.h"
#endif
#include <rx/rx_globals.h>


extern int      BreakVolumeCallBacks(), InitCallBack();
extern	int     LogLevel, etext;
extern afs_int32	BlocksSpare, PctSpare;

void            ShutDown();
static void	ClearXStatValues(), NewParms(), PrintCounters();
static void     ResetCheckDescriptors(void), ResetCheckSignal(void);
static int	CheckSignal();
static int	FiveMinuteCheckLWP(), HostCheckLWP();
extern	int     GetKeysFromToken();
extern struct rx_securityClass *rxnull_NewServerSecurityObject();
extern int RXAFS_ExecuteRequest();
extern int RXSTATS_ExecuteRequest();

int eventlog = 0, rxlog = 0;
FILE *debugFile;
FILE * console = NULL;

#ifdef AFS_NT40_ENV
#define AFS_QUIETFS_ENV 1
#define NT_OPEN_MAX    1024 /* This is an arbitrary no. we came up with for 
			     * now. We hope this will be replaced by a more
			     * intelligent estimate later. */
#endif

int 	SystemId;		/* ViceID of "Administrators"*/
int     SystemAnyUser;		/* Viceid of "System:AnyUser" */
prlist  SystemAnyUserCPS;	/* CPS for "system:AnyUser */
int 	AnonymousID = 0;	/* ViceId of "Anonymous"*/
prlist  AnonCPS;		/* CPS for "Anonymous"*/

struct afsconf_dir *confDir;	/* Configuration dir object */

int     restartMode = RESTART_ORDINARY;

int Testing = 0; /* for ListViceInodes */

/*
 * Home for the performance statistics.
 */
struct afs_PerfStats afs_perfstats;

extern	int     LogLevel;
extern	int	Statistics;

int     timeout = 30;
int 	SawSpare;
int 	SawPctSpare;
int     debuglevel = 0;
int	printBanner = 0;
int	rxJumbograms = 1; /* default is to send and receive jumbograms. */
afs_int32	implicitAdminRights = PRSFS_LOOKUP;	/* The ADMINISTER right is 
						   already implied */

int     stack = 24;
int	stackSize = 24000;
int     fiveminutes = 300; 		/* 5 minutes.  Change this for debugging only */
int     CurrentConnections = 0;
int	hostaclRefresh = 7200;		/* refresh host clients' acls every 2 hrs */
#if defined(AFS_SGI_ENV)
int	SawLock;
#endif
time_t	StartTime;

int	rxpackets = 150;	/* 100 */
int     nSmallVns = 400;	/* 200 */
int     large = 400;		/* 200 */
int     volcache = 400;		/* 400 */
int     numberofcbs = 60000; 	/* 60000 */
int     lwps = 9;		/* 6 */
int     buffs = 90;		/* 70 */
int	novbc = 0;		/* Enable Volume Break calls */
int     busy_threshold = 600;
int	udpBufSize = 0;		/* UDP buffer size for receive*/

struct timeval  tp;

/*
 * FileServer's name and IP address, both network byte order and
 * host byte order.
 */
#define ADDRSPERSITE 16     /* Same global is in rx/rx_user.c */

char FS_HostName[128] = "localhost";
afs_uint32 FS_HostAddr_NBO;
afs_uint32 FS_HostAddr_HBO;
afs_uint32 FS_HostAddrs[ADDRSPERSITE], FS_HostAddr_cnt = 0, FS_registered=0;
/* All addresses in FS_HostAddrs are in NBO */
afsUUID FS_HostUUID;

static ParseArgs();
static FlagMsg();

/*
 * Home for the performance statistics.
 */

/* DEBUG HACK */
static CheckDescriptors()
{
#ifndef AFS_NT40_ENV
    struct stat status;
    register int tsize = getdtablesize();
    register int i;
    for (i = 0; i<tsize; i++) {
	if (fstat(i, &status) != -1) {
	    printf("%d: dev %x, inode %u, length %u, type/mode %x\n",
		   i, status.st_dev, status.st_ino,
		   status.st_size, status.st_mode);
	}
    }
    fflush(stdout);
    ResetCheckDescriptors();
#endif
} /*CheckDescriptors*/


#ifdef AFS_PTHREAD_ENV
void CheckSignal_Signal(x)       {CheckSignal(0);}
void ShutDown_Signal(x)          {ShutDown(0);}
void CheckDescriptors_Signal(x)  {CheckDescriptors(0);}
#else /* AFS_PTHREAD_ENV */
void CheckSignal_Signal(x)       {IOMGR_SoftSig(CheckSignal, 0);}
void ShutDown_Signal(x)          {IOMGR_SoftSig(ShutDown, 0);}
void CheckDescriptors_Signal(x)  {IOMGR_SoftSig(CheckDescriptors, 0);}
#endif /* AFS_PTHREAD_ENV */

/* check whether caller is authorized to manage RX statistics */
int fs_rxstat_userok(call)
    struct rx_call *call;
{
    return afsconf_SuperUser(confDir, call, (char *)0);
}

static void ResetCheckSignal(void)
{
#ifdef	AFS_HPUX_ENV
    signal(SIGPOLL, CheckSignal_Signal);
#else
#ifdef AFS_NT40_ENV
    signal(SIGUSR2, CheckSignal_Signal);
#else
    signal(SIGXCPU, CheckSignal_Signal);
#endif
#endif
}

static void ResetCheckDescriptors(void)
{
#ifndef AFS_NT40_ENV
    signal(SIGTERM, CheckDescriptors_Signal);
#endif
}


/* proc called by rxkad module to get a key */
static int get_key(arock, akvno, akey)
    char *akey;
    char *arock;
    register afs_int32 akvno;

{
    /* find the key */
    static struct afsconf_key tkey;
    register afs_int32 code;

    if (!confDir) {
	ViceLog(0, ("conf dir not open\n"));
	return 1;
    }
    code = afsconf_GetKey(confDir, akvno, tkey.key);
    if (code)
	return code;
    bcopy(tkey.key, akey, sizeof(tkey.key));
    return 0;

} /*get_key*/

#ifndef AFS_NT40_ENV
int viced_syscall(a3, a4, a5)
afs_uint32 a3, a4;
void * a5;
{
  afs_uint32 rcode;
  void (*old)();
	
#ifndef AFS_LINUX20_ENV
  old = (void (*)())signal(SIGSYS, SIG_IGN);	
#endif
  rcode = syscall (AFS_SYSCALL, 28 /* AFSCALL_CALL */, a3, a4, a5);
#ifndef AFS_LINUX20_ENV
  signal(SIGSYS, old);	
#endif

  return rcode;
}
#endif

#if !defined(AFS_NT40_ENV)
#include "AFS_component_version_number.c"
#endif /* !AFS_NT40_ENV */

main(argc, argv)
    int argc;
    char * argv[];

{
    int     i;
    afs_int32    code;
    FILE   *file;
    char tbuffer[32];
    struct rx_securityClass *sc[4];
    struct rx_service *tservice;
#ifdef AFS_PTHREAD_ENV
    pthread_t parentPid, serverPid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else /* AFS_PTHREAD_ENV */
    PROCESS parentPid, serverPid;
#endif /* AFS_PTHREAD_ENV */
    struct hostent *he;
    int minVnodesRequired;	/* min size of vnode cache */
#ifndef AFS_NT40_ENV
    struct rlimit rlim;		/* max number of open file descriptors */
#endif
    int curLimit;

#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;
    
    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0],0);
#endif
	fprintf(stderr,"%s: Unable to obtain AFS server directory.\n", argv[0]);
	exit(2);
    }

#ifndef AFS_QUIETFS_ENV
    console = fopen("/dev/console","w");
#endif

    if(ParseArgs(argc,argv)) {
	FlagMsg();
	exit(-1);
    }

#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&fileproc_glock_mutex, NULL) == 0);
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_SGI_VNODE_GLUE
    if (afs_init_kernel_config(-1) <0) {
	printf("Can't determine NUMA configuration, not starting fileserver.\n");
	exit(1);
    }
#endif
    confDir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);

    NewParms(1);

    /* Open FileLog on stdout, stderr, fd 1 and fd2 (for perror), sigh. */
    OpenLog(AFSDIR_SERVER_FILELOG_FILEPATH);
    SetupLogSignals(); 

    if (SawSpare && SawPctSpare) {
	ViceLog(0, ("Both -spare and -pctspare specified, exiting.\n"));
	exit(-1);
    }

#ifdef AFS_SGI_XFS_IOPS_ENV
    ViceLog(0, ("XFS/EFS File server starting\n"));
#else
    ViceLog(0, ("File server starting\n"));
#endif

    /* install signal handlers for controlling the fileserver process */
    ResetCheckSignal();  /* set CheckSignal_Signal() sig handler */
    ResetCheckDescriptors();  /* set CheckDescriptors_Signal() sig handler */

#if defined(AFS_SGI_ENV)
    /* give this guy a non-degrading priority so help busy servers */
    schedctl(NDPRI, 0, NDPNORMMAX);
    if (SawLock)
	plock(PROCLOCK);
#else
#ifndef AFS_NT40_ENV
    nice(-5); /* TODO: */
#endif
#endif
    assert(DInit(buffs) == 0);
 
#ifdef AFS_NT40_ENV
    if (afs_winsockInit()<0) {
	ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0, argv[0], 0);
	ViceLog(0, ("File server failed to intialize winsock.\n"));
	exit(1);
    }
#endif
    CheckAdminName();

    /* if we support more than 16 threads, then we better have the ability 
    ** to keep open a large number of files simultaneously 
    */
#if	defined(AFS_AIX_ENV) && !defined(AFS_AIX42_ENV)
    curLimit = OPEN_MAX; 	/* for pre AIX 4.2 systems */
#elif defined(AFS_NT40_ENV)
    curLimit = NT_OPEN_MAX;     /* open file descriptor limit on NT */
#else

    curLimit = 0;		/* the number of open file descriptors */
    code     = getrlimit(RLIMIT_NOFILE, &rlim);
    if (code == 0) {
	curLimit      = rlim.rlim_cur;
	rlim.rlim_cur = rlim.rlim_max;
	code = setrlimit(RLIMIT_NOFILE, &rlim);
	if ( code == 0 ) 
	    curLimit  = rlim.rlim_max;
    }
    if ( code != 0 )
	ViceLog(0, ("Failed to increase open file limit, using default\n"));

#endif /* defined(AFS_AIX_ENV) && !defined(AFS_AIX42_ENV) */

    curLimit -= 32;		/* leave a slack of 32 file descriptors */
    if ( lwps > curLimit ) {
	if ( curLimit > 0)
	    lwps = curLimit; 
	else if ( lwps > 16 )
	    lwps = 16;		  /* default to a maximum of 16 threads */
	ViceLog(0, ("The system supports a max of %d open files and we are starting %d threads\n", curLimit, lwps));
    }


#ifndef AFS_PTHREAD_ENV
    assert(LWP_InitializeProcessSupport(LWP_MAX_PRIORITY - 2, &parentPid) == LWP_SUCCESS);
#endif /* !AFS_PTHREAD_ENV */

    /* Initialize volume support */
    if (!novbc) {
	V_BreakVolumeCallbacks = BreakVolumeCallBacks;
    }

    /* initialize libacl routines */
    acl_Initialize(ACL_VERSION);

    /* initialize RX support */
#ifndef AFS_NT40_ENV
    rxi_syscallp = viced_syscall;
#endif
    rx_extraPackets = rxpackets;
    rx_extraQuota = 4;	/* for outgoing prserver calls from R threads */
    rx_SetBusyThreshold(busy_threshold, VBUSY);
    rx_SetCallAbortThreshold(10);
    rx_SetConnAbortThreshold(10);
    stackSize = lwps * 4000;
    if (stackSize < 32000)
	stackSize = 32000;
    else if (stackSize > 44000)
	stackSize = 44000;
#if    defined(AFS_HPUX_ENV) || defined(AFS_SUN_ENV) || defined(AFS_SGI51_ENV)
    rx_SetStackSize(1, stackSize);	
#endif
    if ( udpBufSize )
	rx_SetUdpBufSize(udpBufSize);/* set the UDP buffer size for receive */
    if (rx_Init((int)htons(7000))<0) {
	ViceLog(0, ("Cannot initialize RX\n"));
	exit(1);
    }
    if (!rxJumbograms) {
	/* Don't send and don't allow 3.4 clients to send jumbograms. */
	rx_SetNoJumbo();
    }
    rx_GetIFInfo();
    rx_SetRxDeadTime(30);
    sc[0] = rxnull_NewServerSecurityObject();
    sc[1] = 0; /* rxvab_NewServerSecurityObject(key1, 0) */
    sc[2] = rxkad_NewServerSecurityObject (rxkad_clear, (char *) 0,
					   get_key, (char *) 0);
    sc[3] = rxkad_NewServerSecurityObject (rxkad_crypt, (char *) 0,
					   get_key, (char *) 0);
    tservice = rx_NewService
	(/* port */ 0, /* service id */ 1, /*service name */ "AFS",
	 /* security classes */ sc, /* numb sec classes */ 4,
	 RXAFS_ExecuteRequest);
    if (!tservice) {
	ViceLog(0, ("Failed to initialize RX, probably two servers running.\n"));
	exit(-1);
    }
    rx_SetDestroyConnProc(tservice, (char (*)()) h_FreeConnection);
    rx_SetMinProcs(tservice, 3);
    rx_SetMaxProcs(tservice, lwps);

    tservice = rx_NewService(0,  RX_STATS_SERVICE_ID, "rpcstats", sc, 4, RXSTATS_ExecuteRequest);
    if (!tservice) {
	ViceLog(0, ("Failed to initialize rpc stat service.\n"));
	exit(-1);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    /*
     * Enable RX hot threads, which allows the listener thread to trade
     * places with an idle thread and moves the context switch from listener
     * to worker out of the critical path.
     */
    rx_EnableHotThread();

    /* Some rx debugging */
    if (rxlog || eventlog) {
	debugFile = fopen("rx_dbg", "w");
	if (rxlog) rx_debugFile = debugFile;
	if (eventlog) rxevent_debugFile = debugFile;
    }

    h_InitHostPackage();  /* set up local cellname and realmname */
    InitCallBack(numberofcbs);
    ClearXStatValues();

    code = InitVL();
    if (code) {
	ViceLog(0,("Fatal error in library initialization, exiting!!\n"));
	exit(1);
    }

    code = InitPR();
    if (code) {
	ViceLog(0,("Fatal error in protection initialization, exiting!!\n"));
	exit(1);
    }

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(fs_rxstat_userok);

    rx_StartServer(0);	/* now start handling requests */

    /* we ensure that there is enough space in the vnode buffer to satisfy
    ** requests from all concurrent threads. 
    ** the maximum number of vnodes used by a single thread at any one time
    ** is three ( "link" uses three vnodes simultaneously, one vLarge and
    ** two vSmall for linking files and two vLarge and one vSmall for linking
    ** files  ) : dhruba 
    */
    minVnodesRequired = 2 * lwps + 1;     
    if ( minVnodesRequired > nSmallVns ) {
	nSmallVns = minVnodesRequired;
	ViceLog(0, ("Overriding -s command line parameter with %d\n",
		    nSmallVns));
    }
    if ( minVnodesRequired > large ) {
	large = minVnodesRequired;
	ViceLog(0, ("Overriding -l command line parameter with %d\n", large));
    }

    /* We now do this after getting the listener up and running, so that client
       connections don't timeout (maybe) if a file server is restarted, since it
       will be available "real soon now".  Worry about whether we can satisfy the 
       calls in the volume package itself.
     */
    if (VInitVolumePackage(fileServer,large,nSmallVns,0, volcache)) {
	ViceLog(0, ("Shutting down: errors encountered initializing volume package\n"));
	VShutdown();
	exit(1);
    }

    /*
     * We are done calling fopen/fdopen. It is safe to use a large
     * of the file descriptor cache.
     */
    ih_UseLargeCache();

#ifdef AFS_PTHREAD_ENV
    assert(pthread_attr_init(&tattr) == 0);
    assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);
    /* Block signals in the threads */
    AFS_SIGSET_CLEAR();
    assert(pthread_create(&serverPid, &tattr, (void *)FiveMinuteCheckLWP, &fiveminutes) == 0);
    assert(pthread_create(&serverPid, &tattr, (void *)HostCheckLWP, &fiveminutes) == 0);
    AFS_SIGSET_RESTORE();
#else /* AFS_PTHREAD_ENV */
    assert(LWP_CreateProcess(FiveMinuteCheckLWP, stack*1024, LWP_MAX_PRIORITY - 2,
	    &fiveminutes, "FiveMinuteChecks", &serverPid) == LWP_SUCCESS);
	    
    assert(LWP_CreateProcess(HostCheckLWP, stack*1024, LWP_MAX_PRIORITY - 2,
	    &fiveminutes, "HostCheck", &serverPid) == LWP_SUCCESS);
#endif /* AFS_PTHREAD_ENV */

    TM_GetTimeOfDay(&tp, 0);

#ifndef AFS_QUIETFS_ENV
    if (console != NULL) {
	fprintf(console, "File server has started at %s\r",
		afs_ctime(&tp.tv_sec, tbuffer, sizeof(tbuffer)));
    }
#endif

    /*
     * Figure out the FileServer's name and primary address.
     */
    ViceLog(0, ("Getting FileServer name...\n"));
    code = gethostname(FS_HostName, 64);
    if (code) {
	ViceLog(0, ("gethostname() failed\n"));
    }
    ViceLog(0, ("FileServer host name is '%s'\n", FS_HostName));

    ViceLog(0, ("Getting FileServer address...\n"));
    he = gethostbyname(FS_HostName);
    if (!he) {
	ViceLog(0, ("Can't find address for FileServer '%s'\n",	FS_HostName));
    }
    else {
	char hoststr[16];
	bcopy(he->h_addr, &FS_HostAddr_NBO, 4);
	afs_inet_ntoa_r(FS_HostAddr_NBO, hoststr);
	FS_HostAddr_HBO = ntohl(FS_HostAddr_NBO);
	ViceLog(0,("FileServer %s has address %s (0x%x or 0x%x in host byte order)\n",
		   FS_HostName, hoststr, FS_HostAddr_NBO, FS_HostAddr_HBO));
    }

    /* Install handler to catch the shutdown signal */
    signal(SIGQUIT, ShutDown_Signal); /* bosserver assumes SIGQUIT shutdown */

    ViceLog(0,("File Server started %s",
	       afs_ctime(&tp.tv_sec, tbuffer, sizeof(tbuffer))));
#if FS_STATS_DETAILED
    afs_FullPerfStats.det.epoch.tv_sec = StartTime = tp.tv_sec;
#endif
#ifdef AFS_PTHREAD_ENV
    while(1) {
	sleep(1000); /* long time */
    }
#else /* AFS_PTHREAD_ENV */
    assert(LWP_WaitProcess(&parentPid) == LWP_SUCCESS);
#endif /* AFS_PTHREAD_ENV */
}


/* This LWP does things roughly every 5 minutes */
static FiveMinuteCheckLWP()

{
    static int msg  = 0;
    char tbuffer[32];

    ViceLog(1, ("Starting five minute check process\n"));
    while (1) {
#ifdef AFS_PTHREAD_ENV
	sleep(fiveminutes);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(fiveminutes);
#endif /* AFS_PTHREAD_ENV */

	/* close the log so it can be removed */
	ReOpenLog(AFSDIR_SERVER_FILELOG_FILEPATH); /* don't trunc, just append */
	ViceLog(2, ("Cleaning up timed out callbacks\n"));
	if(CleanupTimedOutCallBacks())
	    ViceLog(5,("Timed out callbacks deleted\n"));
	ViceLog(2, ("Set disk usage statistics\n"));
	VSetDiskUsage();
	if (FS_registered == 1) Do_VLRegisterRPC();
	if(printBanner && (++msg&1)) { /* Every 10 minutes */
	    time_t now = FT_ApproxTime();
	    if (console != NULL) {
#ifndef AFS_QUIETFS_ENV
		fprintf(console,"File server is running at %s\r",
			afs_ctime(&now, tbuffer, sizeof(tbuffer)));
#endif /* AFS_QUIETFS_ENV */
		ViceLog(2, ("File server is running at %s\n",
			afs_ctime(&now, tbuffer, sizeof(tbuffer))));
	    }
	}
    }
} /*FiveMinuteCheckLWP*/


/* This LWP does host checks every 5 minutes:  it should not be used for
 * other 5 minute activities because it may be delayed by timeouts when
 * it probes the workstations
 */
static HostCheckLWP()

{
    ViceLog(1, ("Starting Host check process\n"));
    while(1) {
#ifdef AFS_PTHREAD_ENV
	sleep(fiveminutes);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(fiveminutes);
#endif /* AFS_PTHREAD_ENV */
	ViceLog(2, ("Checking for dead venii & clients\n"));
	h_CheckHosts();
    }
} /*HostCheckLWP*/


#define MAXADMINNAME 64
char adminName[MAXADMINNAME];

CheckAdminName()

{
    int             fd = 0;
    struct stat     status;
    
    if ((stat("/AdminName", &status)) ||	/* if file does not exist */
	(status.st_size <= 0) ||		/* or it is too short */
	(status.st_size >= (MAXADMINNAME)) ||	/* or it is too long */
	!(fd = open("/AdminName", O_RDONLY, 0))) {	/* or the open fails */
	strcpy(adminName, "System:Administrators");	/* use the default name */
    }
    else {
	read(fd, adminName, status.st_size);	/* use name from the file */
    }
    if (fd)
	close(fd);	/* close fd if it was opened */

} /*CheckAdminName*/


/*------------------------------------------------------------------------
 * PRIVATE ClearXStatValues
 *
 * Description:
 *	Initialize all of the values collected via the xstat
 *	interface.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Must be called during File Server initialization.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static void ClearXStatValues()

{ /*ClearXStatValues*/

    struct fs_stats_opTimingData *opTimeP;	/*Ptr to timing struct*/
    struct fs_stats_xferData *opXferP;		/*Ptr to xfer struct*/
    int i;					/*Loop counter*/

    /*
     * Zero all xstat-related structures.
     */
    bzero((char *)(&afs_perfstats), sizeof(struct afs_PerfStats));
#if FS_STATS_DETAILED
    bzero((char *)(&afs_FullPerfStats), sizeof(struct fs_stats_FullPerfStats));

    /*
     * That's not enough.  We have to set reasonable minima for
     * time and xfer values in the detailed stats.
     */
    opTimeP = &(afs_FullPerfStats.det.rpcOpTimes[0]);
    for (i = 0; i < FS_STATS_NUM_RPC_OPS; i++, opTimeP++)
	opTimeP->minTime.tv_sec = 999999;

    opXferP = &(afs_FullPerfStats.det.xferOpTimes[0]);
    for (i = 0; i < FS_STATS_NUM_XFER_OPS; i++, opXferP++) {
	opXferP->minTime.tv_sec = 999999;
	opXferP->minBytes = 999999999;
    }

    /*
     * There's more.  We have to set our unique system identifier, as
     * declared in param.h.  If such a thing is not defined, we bitch
     * and declare ourselves to be an unknown system type.
     */
#ifdef SYS_NAME_ID
    afs_perfstats.sysname_ID = SYS_NAME_ID;
#else
#ifndef AFS_NT40_ENV
    ViceLog(0, ("Sys name ID constant not defined in param.h!!\n"));
    ViceLog(0, ("[Choosing ``undefined'' sys name ID.\n"));
#endif
    afs_perfstats.sysname_ID = SYS_NAME_ID_UNDEFINED;
#endif /* SYS_NAME_ID */
#endif

} /*ClearXStatValues*/


static void PrintCounters()

{
    int	dirbuff, dircall, dirio;
    struct timeval  tpl;
    int workstations, activeworkstations, delworkstations;
    int processSize = 0;
    char tbuffer[32];

    TM_GetTimeOfDay(&tpl, 0);
    Statistics = 1;
    ViceLog(0, ("Vice was last started at %s\n",
		afs_ctime(&StartTime, tbuffer, sizeof(tbuffer))));

    VPrintCacheStats();
    VPrintDiskStats();
    DStat(&dirbuff, &dircall, &dirio);
    ViceLog(0,("With %d directory buffers; %d reads resulted in %d read I/Os\n",
	    dirbuff, dircall, dirio));
    rx_PrintStats(stderr);
    h_PrintStats();
    PrintCallBackStats();
#ifdef AFS_NT40_ENV
    processSize = -1; /* TODO: */
#else
    processSize = (int)((long) sbrk(0) >> 10);
#endif
    ViceLog(0,("There are %d connections, process size %d\n",  CurrentConnections, processSize));
    h_GetWorkStats(&workstations, &activeworkstations, &delworkstations,
	 tpl.tv_sec-15*60);
    ViceLog(0,
	    ("There are %d workstations, %d are active (req in < 15 mins), %d marked \"down\"\n",
	    workstations, activeworkstations, delworkstations));
    Statistics = 0;

} /*PrintCounters*/



static CheckSignal()

{
    if (FS_registered > 0)  {
	/*
	 * We have proper ip addresses; tell the vlserver what we got; the following
	 * routine will do the proper reporting for us
	 */
	Do_VLRegisterRPC();
    }
    h_DumpHosts();
    h_PrintClients();
    DumpCallBackState();
    PrintCounters();
    ResetCheckSignal();

} /*CheckSignal*/

void ShutDownAndCore(dopanic)
int dopanic;
{
    time_t now = time(0);
    char *tstr;
    char tbuffer[32];

    ViceLog(0, ("Shutting down file server at %s",
		afs_ctime(&now, tbuffer, sizeof(tbuffer))));
    if (dopanic) 
	ViceLog(0, ("ABNORMAL SHUTDOWN, see core file.\n"));
#ifndef AFS_QUIETFS_ENV
    if (console != NULL) {
	fprintf(console,"File server restart/shutdown received at %s\r",
		afs_ctime(&now, tbuffer, sizeof(tbuffer)));
    }
#endif
    DFlush();
    PrintCounters();

    /* do not allows new reqests to be served from now on, all new requests
       are returned with an error code of RX_RESTARTING ( transient failure ) */
    rx_SetRxTranquil();			/* dhruba */
    VShutdown();

    if (debugFile) {
	rx_PrintStats(debugFile);
	fflush(debugFile);
    }
    if (console != NULL) {
	now = time(0);
	if (dopanic) {
#ifndef AFS_QUIETFS_ENV
	    fprintf(console, "File server has terminated abnormally at %s\r",
		    afs_ctime(&now, tbuffer, sizeof(tbuffer)));
#endif
	    ViceLog(0, ("File server has terminated abnormally at %s\n",
		afs_ctime(&now, tbuffer, sizeof(tbuffer))));
	} else {
#ifndef AFS_QUIETFS_ENV
	    fprintf(console, "File server has terminated normally at %s\r",
		    afs_ctime(&now, tbuffer, sizeof(tbuffer)));
#endif
	    ViceLog(0, ("File server has terminated normally at %s\n",
		afs_ctime(&now, tbuffer, sizeof(tbuffer))));
	}
    }

    exit(0);

} /*ShutDown*/

void ShutDown() /* backward compatibility */
{
  ShutDownAndCore(DONTPANIC);
}


static FlagMsg()

{
    char buffer[1024];

	/* default supports help flag */

    strcpy(buffer, "Usage: fileserver ");
    strcat(buffer, "[-d <debug level>] ");
    strcat(buffer, "[-p <number of processes>] ");
    strcat(buffer, "[-spare <number of spare blocks>] ");
    strcat(buffer, "[-pctspare <percentage spare>] ");
    strcat(buffer, "[-b <buffers>] ");
    strcat(buffer, "[-l <large vnodes>] ");
    strcat(buffer, "[-s <small vnodes>] ");
    strcat(buffer, "[-vc <volume cachesize>] ");
    strcat(buffer, "[-w <call back wait interval>] ");
    strcat(buffer, "[-cb <number of call backs>] ");
    strcat(buffer, "[-banner (print banner every 10 minutes)] ");
    strcat(buffer, "[-novbc (whole volume cbs disabled)] ");
    strcat(buffer, "[-implicit <admin mode bits: rlidwka>] ");
    strcat(buffer, "[-hr <number of hours between refreshing the host cps>] ");
    strcat(buffer, "[-busyat <redirect clients when queue > n>] ");
    strcat(buffer, "[-rxpck <number of rx extra packets>] ");
    strcat(buffer, "[-rxdbg (enable rx debugging)] ");
    strcat(buffer, "[-rxdbge (enable rxevent debugging)] ");
#ifdef	AFS_AIX32_ENV
    strcat(buffer, "[-m <min percentage spare in partition>] ");
#endif
#if defined(AFS_SGI_ENV)
    strcat(buffer, "[-lock (keep fileserver from swapping)] ");
#endif
    strcat(buffer, "[-L (large server conf)] ");
    strcat(buffer, "[-S (small server conf)] ");
    strcat(buffer, "[-k <stack size>] ");
    strcat(buffer, "[-realm <Kerberos realm name>] ");
    strcat(buffer, "[-udpsize <size of socket buffer in bytes>] ");
/*   strcat(buffer, "[-enable_peer_stats] "); */
/*   strcat(buffer, "[-enable_process_stats] "); */
    strcat(buffer, "[-help]\n");
/*
    ViceLog(0, ("%s", buffer));
*/

	printf("%s",buffer);
	fflush(stdout);

} /*FlagMsg*/


static afs_int32 ParseRights(arights)
    char *arights;
{
    afs_int32 mode = 0;
    int i, len;
    char tc;

    if (!arights || !strcmp(arights, "")) {
	printf("Missing list of mode bits on -implicit option\n");
	return -1;
    }
    if (!strcmp(arights, "none"))
      mode = 0;
    else if (!strcmp(arights, "read")) 
      mode = PRSFS_READ | PRSFS_LOOKUP;
    else if (!strcmp(arights, "write"))
      mode = PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | 
	     PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK;
    else if (!strcmp(arights, "all"))
      mode = PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | 
	     PRSFS_DELETE | PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    else {
	len = strlen(arights);
	for(i=0;i<len;i++) {
	    tc = *arights++;
	    if (tc == 'r') mode |= PRSFS_READ;
	    else if (tc == 'l') mode |= PRSFS_LOOKUP;
	    else if (tc == 'i') mode |= PRSFS_INSERT;
	    else if (tc == 'd') mode |= PRSFS_DELETE;
	    else if (tc == 'w') mode |= PRSFS_WRITE;
	    else if (tc == 'k') mode |= PRSFS_LOCK;
	    else if (tc == 'a') mode |= PRSFS_ADMINISTER;
	    else if (tc == 'A') mode |= PRSFS_USR0;
	    else if (tc == 'B') mode |= PRSFS_USR1;
	    else if (tc == 'C') mode |= PRSFS_USR2;
	    else if (tc == 'D') mode |= PRSFS_USR3;
	    else if (tc == 'E') mode |= PRSFS_USR4;
	    else if (tc == 'F') mode |= PRSFS_USR5;
	    else if (tc == 'G') mode |= PRSFS_USR6;
	    else if (tc == 'H') mode |= PRSFS_USR7;
	    else {
		printf("Illegal -implicit rights character '%c'.\n", tc);
		return -1;
	    }
	}
    }
    return mode;
}

/*
 * Limit MAX_FILESERVER_THREAD by the system limit on the number of
 * pthreads (sysconf(_SC_THREAD_THREADS_MAX)), if applicable and
 * available.
 *
 * AIX:         sysconf() limit is real
 * HP-UX:       sysconf() limit is real
 * IRIX:        sysconf() limit is apparently NOT real -- too small
 * DUX:         sysconf() limit is apparently NOT real -- too big
 * Linux:       sysconf() limit is apparently NOT real -- too big
 * Solaris:     no sysconf() limit
 */
static int
max_fileserver_thread(void)
{
#if defined(AFS_PTHREAD_ENV)
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV)
	long ans;

	ans = sysconf(_SC_THREAD_THREADS_MAX);
	if (0 < ans && ans < MAX_FILESERVER_THREAD)
		return (int) ans;
#endif
#endif /* defined(AFS_PTHREAD_ENV) */
	return MAX_FILESERVER_THREAD;
}

static ParseArgs(argc, argv)
    int argc;
    char *argv[];

{
    int SawL=0, SawS=0, SawVC=0;
    int Sawrxpck = 0, Sawsmall=0, Sawlarge=0, Sawcbs=0, Sawlwps=0, Sawbufs=0;
    int Sawbusy=0;
    int i;
    int bufSize = 0;      /* temp variable to read in udp socket buf size*/

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-d")) {
	    debuglevel = atoi(argv[++i]);
	    LogLevel = debuglevel;
	}
	else
	    if (!strcmp(argv[i], "-banner")) {
		printBanner = 1;
	} else
	    if (!strcmp(argv[i], "-implicit")) {
		implicitAdminRights = ParseRights(argv[++i]);
		if (implicitAdminRights < 0) return implicitAdminRights;
	} else
	    if (!strcmp(argv[i], "-L")) {
		SawL = 1;
	} else
	    if (!strcmp(argv[i], "-S")) {
		SawS = 1;
	}
	else
	    if (!strcmp(argv[i], "-p")) {
		int lwps_max = max_fileserver_thread() - FILESERVER_HELPER_THREADS;
		Sawlwps = 1;
		lwps = atoi(argv[++i]);
		if (lwps > lwps_max)
		    lwps = lwps_max;
		else if (lwps < 6)
		    lwps = 6;
	    }
	else
	    if (!strcmp(argv[i], "-b")) {
		Sawbufs = 1;
		buffs = atoi(argv[++i]);
	    }
	else
	    if (!strcmp(argv[i], "-l")) {
		Sawlarge = 1;
		large = atoi(argv[++i]);
	    }
	else
	    if (!strcmp(argv[i], "-vc")) {
		SawVC = 1;
		volcache = atoi(argv[++i]);
	    }
	else
	    if (!strcmp(argv[i], "-novbc")) {
		novbc = 1;
	    }
	else
	    if (!strcmp(argv[i], "-rxpck")) {
		Sawrxpck = 1;
		rxpackets = atoi(argv[++i]);
	    }
	else
	    if (!strcmp(argv[i], "-s")) {
		Sawsmall = 1;
		nSmallVns = atoi(argv[++i]);
	    }
	else	
	    if (!strcmp(argv[i], "-k"))
		stack = atoi(argv[++i]);
#if defined(AFS_SGI_ENV)
	else
	    if (!strcmp(argv[i], "-lock")) {
		SawLock = 1;
	    }
#endif
	else
	    if (!strcmp(argv[i], "-spare")) {
		BlocksSpare = atoi(argv[++i]);
		SawSpare = 1;
	    }
	else
	    if (!strcmp(argv[i], "-pctspare")) {
		PctSpare = atoi(argv[++i]);
		BlocksSpare = 0;		/* has non-zero default */
		SawPctSpare = 1;
	    }
	else
	    if (!strcmp(argv[i], "-w"))
		fiveminutes = atoi(argv[++i]);
	else
	    if (!strcmp(argv[i], "-hr")) {
		int hr = atoi(argv[++i]);
		if ((hr < 1) || (hr > 36)) {
		    printf("host acl refresh interval of %d hours is invalid; hours must be between 1 and 36\n\n",
			   hr);
		    return -1;
		}		    
		hostaclRefresh = hr*60*60;
	} else
	    if (!strcmp(argv[i], "-rxdbg"))
		rxlog = 1;
	else 
	    if (!strcmp(argv[i], "-rxdbge"))
		eventlog = 1;
	else
	    if (!strcmp(argv[i], "-cb")) {
		Sawcbs = 1;
		numberofcbs = atoi(argv[++i]);
		if ((numberofcbs < 10000) || (numberofcbs > 65535)) {
		    printf("number of cbs %d invalid; must be between 10000 and 65535\n",
			   numberofcbs);
		    return -1;
		}
	    }
	    else
	      if (!strcmp(argv[i], "-busyat")) {
		Sawbusy = 1;
		busy_threshold = atoi(argv[++i]);
		if (busy_threshold < 10) {
		    printf("Busy threshold %d is too low, will compute default.\n",
			   busy_threshold);
		    Sawbusy = 0;
		}
	    }
#ifdef	AFS_AIX32_ENV
	else
	    if (!strcmp(argv[i], "-m")) {
		extern int aixlow_water;
		 aixlow_water = atoi(argv[++i]);
		if ((aixlow_water < 0) || (aixlow_water > 30)) {
		    printf("space reserved %d% invalid; must be between 0-30%\n", aixlow_water);
		    return -1;
		}
	    }
#endif
	else
	    if (!strcmp(argv[i], "-nojumbo")) {
		rxJumbograms = 0;
	    }
	else
	    if (!strcmp(argv[i], "-realm")) {
		extern char local_realm[AFS_REALM_SZ];
		if (strlen(argv[++i]) >= AFS_REALM_SZ) {
		    printf("-realm argument must contain fewer than %d characters.\n",AFS_REALM_SZ);
		    return -1;
		}
		strncpy (local_realm, argv[i], AFS_REALM_SZ);
	    }
	else
	    if ( !strcmp(argv[i], "-udpsize")) {
		if ( (i+1) >= argc ) {
		    printf("You have to specify -udpsize <integer value>\n");
		    return -1;
		}
		bufSize = atoi(argv[++i]);
		if ( bufSize < rx_GetMinUdpBufSize() )
		    printf("Warning:udpsize %d is less than minimum %d; ignoring\n",
				bufSize, rx_GetMinUdpBufSize() );
		else
		    udpBufSize = bufSize;
	    }
	else
	    if (!strcmp(argv[i], "-enable_peer_stats")) {
		rx_enablePeerRPCStats();
	    }
	else
	    if (!strcmp(argv[i], "-enable_process_stats")) {
		rx_enableProcessRPCStats();
	    }
#ifndef AFS_NT40_ENV
        else 
	    if (strcmp(argv[i], "-syslog")==0) {
		/* set syslog logging flag */
		serverLogSyslog = 1;
	    } 
	else 
	    if (strncmp(argv[i], "-syslog=", 8)==0) {
		serverLogSyslog = 1;
		serverLogSyslogFacility = atoi(argv[i]+8);
	    }
#endif
	else {
	    return(-1);
	}
    }
    if (SawS && SawL) {
	printf("Only one of -L, or -S must be specified\n");
	return -1;
    }
    if (SawS) {
	if (!Sawrxpck) rxpackets = 100;
	if (!Sawsmall) nSmallVns = 200;
	if (!Sawlarge) large = 200;
	if (!Sawcbs) numberofcbs = 20000;
	if (!Sawlwps) lwps = 6;
	if (!Sawbufs) buffs = 70;
	if (!SawVC) volcache = 200;
    }
    if (SawL) {
	if (!Sawrxpck) rxpackets = 200;
	if (!Sawsmall) nSmallVns = 600;
	if (!Sawlarge) large = 600;
	if (!Sawcbs) numberofcbs = 64000;
	if (!Sawlwps) lwps = 12;
	if (!Sawbufs) buffs = 120;
	if (!SawVC) volcache = 600;
    }
    if (!Sawbusy) 
        busy_threshold = 3*rxpackets/2;

    return(0);

} /*ParseArgs*/


#define MAXPARMS 15

static void NewParms(initializing)
    int initializing;

{
    static struct stat sbuf;
    register int i, fd;
    char *parms;
    char *argv[MAXPARMS];
    register int argc;

    if (!(stat("/vice/file/parms",&sbuf))) {
	parms = (char *)malloc(sbuf.st_size);
	if(parms <= (char *)0) return;
	fd = open("parms", O_RDONLY, 0666);
	if(fd <= 0) {
	    ViceLog(0, ("Open for parms failed with errno = %d\n", errno));
	    return;
	}

	i = read(fd, parms, sbuf.st_size);
	close(fd);
	if(i != sbuf.st_size) {
	    if (i < 0 )
		ViceLog(0, ("Read on parms failed with errno = %d\n", errno));
	    else
		ViceLog(0,
			("Read on parms failed; expected %d bytes but read %d\n",
			sbuf.st_size, i));
	    free(parms);
	    return;
	}

	for (i = 0;i < MAXPARMS; argv[i++] = 0 );
	
	for (argc = i = 0; i < sbuf.st_size; i++) {
	    if ((*(parms + i) != ' ') && (*(parms + i) != '\n')){
		if(argv[argc] == 0) argv[argc] = (parms+i);
	    }
	    else {
		*(parms + i) = '\0';
		if(argv[argc] != 0) {
		    if(++argc == MAXPARMS) break;
		}
		while ((*(parms + i + 1) == ' ') || (*(parms + i + 1) == '\n'))
		    i++;
	    }
	}
	if(ParseArgs(argc, argv) == 0)
	    ViceLog(0, ("Change parameters to:"));
	else
	    ViceLog(0, ("Invalid parameter in:"));
	for(i = 0; i < argc; i++) {
	    ViceLog(0, (" %s", argv[i]));
	}
	ViceLog(0,("\n"));
	free(parms);
    }
    else
	if(!initializing)
	    ViceLog(0, ("Received request to change parms but no parms file exists\n"));

} /*NewParms*/


/* Miscellaneous routines */
Die (msg)
    char *msg;

{
    ViceLog (0,("%s\n", msg));
    assert(0);

} /*Die*/


InitPR()

{
    register code;

    /*
     * If this fails, it's because something major is wrong, and is not
     * likely to be time dependent.
     */
    code = pr_Initialize(2, AFSDIR_SERVER_ETC_DIRPATH, 0);
    if (code != 0) {
	ViceLog(0, ("Couldn't initialize protection library; code=%d.\n", code));
	return code; 
    }
    SystemId = SYSADMINID;
    SystemAnyUser = ANYUSERID;
    SystemAnyUserCPS.prlist_len = 0;
    SystemAnyUserCPS.prlist_val = (afs_int32 *)0;
    AnonCPS.prlist_len = 0;
    AnonCPS.prlist_val = (afs_int32 *)0;
    while (1) {
	code = pr_GetCPS(SystemAnyUser, &SystemAnyUserCPS);
	if (code != 0) {
	    ViceLog(0,
		    ("Couldn't get CPS for AnyUser, will try again in 30 seconds; code=%d.\n",
		    code));
	    goto sleep;
	}
	code = pr_GetCPS(ANONYMOUSID,&AnonCPS);
	if (code != 0) {
	    ViceLog(0,("Couldn't get Anonymous CPS, exiting; code=%d.\n", code));
	    return -1;
	}
	AnonymousID = ANONYMOUSID;
	return 0;
sleep:
#ifdef AFS_PTHREAD_ENV
	sleep(30);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(30);
#endif /* AFS_PTHREAD_ENV */
    }
} /*InitPR*/

struct rx_connection *serverconns[MAXSERVERS];
struct ubik_client *cstruct;
afs_int32 vl_Initialize(confDir)
char *confDir;
{   afs_int32 code, scIndex = 0, i;
    struct afsconf_dir *tdir;
    struct rx_securityClass *sc;
    struct afsconf_cell info;

    tdir = afsconf_Open(confDir);
    if (!tdir) {
	ViceLog(0, ("Could not open configuration directory (%s).\n", confDir));
	exit(1);
    }
    code = afsconf_ClientAuth(tdir, &sc, &scIndex);
    if (code) {
	ViceLog(0, ("Could not get security object for localAuth\n"));
	exit(1);
    }
    code = afsconf_GetCellInfo(tdir,(char *)0, AFSCONF_VLDBSERVICE, &info);
    if (info.numServers > MAXSERVERS) {
	ViceLog(0, ("vl_Initialize: info.numServers=%d (> MAXSERVERS=%d)\n",info.numServers, MAXSERVERS));
	exit(1);
    }
    for (i = 0;i<info.numServers;i++) 
	serverconns[i] = rx_NewConnection(info.hostAddr[i].sin_addr.s_addr, info.hostAddr[i].sin_port,
					  USER_SERVICE_ID, sc, scIndex);
    code = ubik_ClientInit(serverconns, &cstruct);
    if (code) {
	ViceLog(0, ("vl_Initialize: ubik client init failed.\n"));
	return code;
    }
    return 0;
}

#define SYSIDMAGIC	0x88aabbcc
#define SYSIDVERSION	1

ReadSysIdFile() {
    afs_int32 fd, nentries, i;
    struct versionStamp vsn;
    struct stat     status;
    afsUUID uuid;

    if ((stat(AFSDIR_SERVER_SYSID_FILEPATH, &status)) || (status.st_size <= 0)) {
	ViceLog(0, ("%s: doesn't exist\n", AFSDIR_SERVER_SYSID_FILEPATH));
	return ENOENT;
    }
    if (!(fd = open(AFSDIR_SERVER_SYSID_FILEPATH, O_RDONLY, 0))) {
	ViceLog(0, ("%s: can't open (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    if ((i = read(fd, (char *)&vsn, sizeof(vsn))) != sizeof(vsn)) {
	ViceLog(0, ("%s: Read failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    if (vsn.magic != SYSIDMAGIC) {
	ViceLog(0, ("%s: wrong magic %x (we support %x)\n", AFSDIR_SERVER_SYSID_FILEPATH, vsn.magic, SYSIDMAGIC));
	return EIO;
    }
    if (vsn.version != SYSIDVERSION) {
	ViceLog(0, ("%s: wrong version %d (we support %d)\n", AFSDIR_SERVER_SYSID_FILEPATH, vsn.version, SYSIDVERSION));
	return EIO;
    }
    if ((i = read(fd, (char *)&uuid, sizeof(struct afsUUID))) != sizeof(struct afsUUID)) {
	ViceLog(0, ("%s: read of uuid failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    afs_ntohuuid(&uuid);
    FS_HostUUID = uuid;
    if ((i = read(fd, (char *)&nentries, sizeof(afs_int32))) != sizeof(afs_int32)) {
	ViceLog(0, ("%s: Read of entries failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    if (nentries <= 0 || nentries > ADDRSPERSITE) {
	ViceLog(0, ("%s: invalid num of interfaces: %d\n", AFSDIR_SERVER_SYSID_FILEPATH, nentries));
	return EIO;
    }
    FS_HostAddr_cnt = nentries;
    for (i = 0; i < nentries; i++) {
	if (read(fd, (char *)&FS_HostAddrs[i], sizeof(afs_int32)) != sizeof(afs_int32)) {	
	    ViceLog(0, ("%s: Read of addresses failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	    FS_HostAddr_cnt = 0;	/* reset it */
	    return EIO;
	}
    }
    close(fd);
    return 0;
}

WriteSysIdFile() {
    afs_int32 fd, nentries, i;
    struct versionStamp vsn;
    struct stat     status;
    afsUUID uuid;
    
    if (!stat(AFSDIR_SERVER_SYSID_FILEPATH, &status)) {
	/*
	 * File exists; keep the old one around
	 */
	renamefile(AFSDIR_SERVER_SYSID_FILEPATH, AFSDIR_SERVER_OLDSYSID_FILEPATH);	
    }
    fd = open(AFSDIR_SERVER_SYSID_FILEPATH, O_WRONLY|O_TRUNC|O_CREAT, 0666);
    if (fd < 1) {
	ViceLog(0, ("%s: can't create (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    vsn.magic = SYSIDMAGIC;
    vsn.version = 1;
    if ((i = write(fd, (char *)&vsn, sizeof(vsn))) != sizeof(vsn)) {
	ViceLog(0, ("%s: write failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    uuid = FS_HostUUID;
    afs_htonuuid(&uuid);
    if ((i = write(fd, (char *)&uuid, sizeof(struct afsUUID))) != sizeof(struct afsUUID)) {
	ViceLog(0, ("%s: write of uuid failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    if ((i = write(fd, (char *)&FS_HostAddr_cnt, sizeof(afs_int32))) != sizeof(afs_int32)) {
	ViceLog(0, ("%s: write of # of entries failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    for (i = 0; i < FS_HostAddr_cnt; i++) {
	if (write(fd, (char *)&FS_HostAddrs[i], sizeof(afs_int32)) != sizeof(afs_int32)) {	
	    ViceLog(0, ("%s: write of addresses failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH, errno));
	    return EIO;
	}
    }
    close(fd);
    return 0;
}

/*
 * defect 10966 
 * This routine sets up the buffers for the VL_RegisterAddrs RPC. All addresses
 * in FS_HostAddrs[] are in NBO, while the RPC treats them as a "blob" of data
 * and so we need to convert each of them into HBO which is what the extra 
 * array called FS_HostAddrs_HBO is used here.
 */ 
Do_VLRegisterRPC() {
    register int code;
    bulkaddrs addrs;
    extern int VL_RegisterAddrs();
    afs_uint32 FS_HostAddrs_HBO[ADDRSPERSITE];
    int i=0;
 
    for (i=0; i < FS_HostAddr_cnt ; i++) 
     FS_HostAddrs_HBO[i]=ntohl(FS_HostAddrs[i]);
    addrs.bulkaddrs_len = FS_HostAddr_cnt;
    addrs.bulkaddrs_val = (afs_uint32 *)FS_HostAddrs_HBO;
    code = ubik_Call(VL_RegisterAddrs, cstruct, 0, &FS_HostUUID, 0, &addrs);
    if (code) {
       if (code == VL_MULTIPADDR) {
	  ViceLog(0, ("VL_RegisterAddrs rpc failed; The ethernet address exist on a different server; repair it\n"));
	  ViceLog(0, ("VL_RegisterAddrs rpc failed; See VLLog for details\n"));
	  return code;
       } else if (code == RXGEN_OPCODE) {
	  ViceLog(0, ("vlserver doesn't support VL_RegisterAddrs rpc; ignored\n"));
	  FS_registered = 2; 	/* So we don't have to retry in the gc daemon */
       } else {
	  ViceLog(0, ("VL_RegisterAddrs rpc failed; will retry periodically (code=%d, err=%d)\n",
		      code, errno));
       }
    } else {
       FS_registered = 2; 	/* So we don't have to retry in the gc daemon */
       WriteSysIdFile();
    }

    return 0;
}

AddrsEqual(cnt, addr1, addr2) 
    int cnt;
    afs_int32 *addr1, *addr2; 
{
    register int i, j;

    for (i = 0; i < cnt; i++) {
	for (j = 0; j < cnt; j++) {
	    if (addr1[i] == addr2[j]) break;
	}
	if (j == cnt) return 0;
    }
    return 1;
}

InitVL() {
    int (*old)();
    afs_int32 code;
    afs_int32 cnt, i;
    extern int rxi_numNetAddrs;
    extern afs_uint32 rxi_NetAddrs[];

    /*
     * If this fails, it's because something major is wrong, and is not
     * likely to be time dependent.
     */
    code = vl_Initialize(AFSDIR_SERVER_ETC_DIRPATH);
    if (code != 0) {
	ViceLog(0, ("Couldn't initialize protection library; code=%d.\n", code));
	return code;
    }

    /* Read or create the sysid file and register the fileserver's
     * IP addresses with the vlserver.
     */
    code = ReadSysIdFile();
    if (code) {
       /* Need to create the file */
       ViceLog(0, ("Creating new SysID file\n")); 
       if (code = afs_uuid_create(&FS_HostUUID)) {
	  ViceLog(0, ("Failed to create new uuid: %d\n", code)); 
	  exit(1);
       }
    }
    /* A good sysid file exists; inform the vlserver. If any conflicts,
     * we always use the latest interface available as the real truth.
     */
#ifndef AFS_NT40_ENV
    if(AFSDIR_SERVER_NETRESTRICT_FILEPATH || AFSDIR_SERVER_NETINFO_FILEPATH) {
      /*
       * Find addresses we are supposed to register as per the netrestrict 
       * and netinfo files (/usr/afs/local/NetInfo and 
       * /usr/afs/local/NetRestict)
       */
      char reason[1024];
      afs_int32 code = parseNetFiles(FS_HostAddrs,NULL, NULL,
				 ADDRSPERSITE, reason,
				 AFSDIR_SERVER_NETINFO_FILEPATH,
				 AFSDIR_SERVER_NETRESTRICT_FILEPATH);
      if (code < 0) {
	ViceLog(0,("Can' register any valid addresses:%s\n",reason));
	exit(1);
      }
      FS_HostAddr_cnt = (afs_uint32) code;
    }
    else 
#endif
    {
      FS_HostAddr_cnt = rx_getAllAddr(FS_HostAddrs, ADDRSPERSITE);
    }

    FS_registered = 1;
    code = Do_VLRegisterRPC();
    return code;
} 
