/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006 Sine Nomine Associates
 */

/*  viced.c	- File Server main loop					 */
/* 									 */
/*  Date: 5/1/85							 */
/* 									 */
/*  Function  	- This routine has the initialization code for		 */
/* 		  FileServer II						 */
/* 									 */
/* ********************************************************************** */

#include <afsconfig.h>
#include <afs/param.h>
#include <afs/stds.h>

#include <afs/procmgmt.h>
#include <roken.h>

#ifdef AFS_NT40_ENV
# include <windows.h>
# include <WINNT/afsevent.h>
#endif

#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#undef SHARED

#include <afs/opr.h>
#include <afs/nfs.h>
#include <rx/rx_queue.h>
#include <lwp.h>
#include <opr/lock.h>
#include <opr/proc.h>
#include <opr/softsig.h>
#include <afs/cmd.h>
#include <afs/ptclient.h>
#include <afs/afsint.h>
#include <afs/vldbint.h>
#include <afs/errors.h>
#include <afs/ihandle.h>
#include <afs/vnode.h>
#include <afs/volume.h>
#include <afs/auth.h>
#include <afs/authcon.h>
#include <afs/cellconfig.h>
#include <afs/acl.h>
#include <afs/prs_fs.h>
#include <rx/rx.h>
#include <rx/rxstat.h>
#include <afs/keys.h>
#include <afs/afs_args.h>
#include <afs/vlserver.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/ptuser.h>
#include <afs/audit.h>
#include <afs/partition.h>
#include <afs/dir.h>
#include <afs/afsutil.h>
#include "viced_prototypes.h"
#include "viced.h"
#include "host.h"
#if defined(AFS_SGI_ENV)
# include "sys/schedctl.h"
# include "sys/lock.h"
#endif
#include <rx/rx_globals.h>

extern int etext;

static void ClearXStatValues(void);
static void PrintCounters(void);

static afs_int32 Do_VLRegisterRPC(void);

int eventlog = 0, rxlog = 0;
FILE *debugFile;
static struct logOptions logopts;

pthread_mutex_t fsync_glock_mutex;
pthread_cond_t fsync_cond;

#ifdef AFS_NT40_ENV
# define NT_OPEN_MAX    1024	/* This is an arbitrary no. we came up with for
				 * now. We hope this will be replaced by a more
				 * intelligent estimate later. */
#endif

int SystemId;			/* ViceID of "Administrators" */
int SystemAnyUser;		/* Viceid of "System:AnyUser" */
prlist SystemAnyUserCPS;	/* CPS for "system:AnyUser */
int AnonymousID = 0;		/* ViceId of "Anonymous" */
prlist AnonCPS;			/* CPS for "Anonymous" */

struct afsconf_dir *confDir;	/* Configuration dir object */

int restartMode = RESTART_ORDINARY;

/*
 * Home for the performance statistics.
 */
struct afs_PerfStats afs_perfstats;

extern int Statistics;

int busyonrst = 1;
int timeout = 30;
int printBanner = 0;
int rxJumbograms = 0;		/* default is to not send and receive jumbograms. */
int rxBind = 0;		/* don't bind */
int rxkadDisableDotCheck = 0;      /* disable check for dot in principal name */
int rxMaxMTU = -1;
afs_int32 implicitAdminRights = PRSFS_LOOKUP;	/* The ADMINISTER right is
						 * already implied */
afs_int32 readonlyServer = 0;
afs_int32 adminwriteServer = 0;

int stackSize = 24000;
int fiveminutes = 300;		/* 5 minutes.  Change this for debugging only */
int CurrentConnections = 0;
int hostaclRefresh = 7200;	/* refresh host clients' acls every 2 hrs */
#if defined(AFS_SGI_ENV)
int SawLock;
#endif
time_t StartTime;

/**
 * seconds to wait until forcing a panic during ShutDownAndCore(PANIC)
 * in case we get stuck.
 */
#ifdef AFS_DEMAND_ATTACH_FS
static int panic_timeout = 2 * 60;
#else
static int panic_timeout = 30 * 60;
#endif

static int host_thread_quota;
int rxpackets = 150;		/* 100 */
int nSmallVns = 400;		/* 200 */
int large = 400;		/* 200 */
int volcache = 400;		/* 400 */
int numberofcbs = 60000;	/* 60000 */
int lwps = 9;			/* 6 */
int buffs = 90;			/* 70 */
int novbc = 0;			/* Enable Volume Break calls */
int busy_threshold = 600;
int abort_threshold = 10;
int udpBufSize = 0;		/* UDP buffer size for receive */
int sendBufSize = 16384;	/* send buffer size */
int saneacls = 0;		/* Sane ACLs Flag */
static int unsafe_attach = 0;   /* avoid inUse check on vol attach? */
static int offline_timeout = -1; /* -offline-timeout option */
static int offline_shutdown_timeout = -1; /* -offline-shutdown-timeout option */

struct timeval tp;

pthread_key_t viced_uclient_key;

/*
 * FileServer's name and IP address, both network byte order and
 * host byte order.
 */
#define ADDRSPERSITE 16		/* Same global is in rx/rx_user.c */

char FS_HostName[128] = "localhost";
char *FS_configPath = NULL;
afs_uint32 FS_HostAddr_NBO;
afs_uint32 FS_HostAddr_HBO;
afs_uint32 FS_HostAddrs[ADDRSPERSITE], FS_HostAddr_cnt = 0, FS_registered = 0;
/* All addresses in FS_HostAddrs are in NBO */
afsUUID FS_HostUUID;

#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * fileserver mode support
 *
 * during fileserver shutdown, we have to track the graceful shutdown of
 * certain background threads before we are allowed to dump state to
 * disk
 */

# if !defined(PTHREAD_RWLOCK_INITIALIZER) && defined(AFS_DARWIN80_ENV)
#  define PTHREAD_RWLOCK_INITIALIZER {0x2DA8B3B4, {0}}
# endif

# ifndef AFS_NT40_ENV
struct fs_state fs_state =
    { FS_MODE_NORMAL,
      0,
      0,
      0,
      0,
      { 1,1,1,1 },
      PTHREAD_COND_INITIALIZER,
      PTHREAD_RWLOCK_INITIALIZER
    };
# else /* AFS_NT40_ENV */
struct fs_state fs_state;

static int fs_stateInit(void)
{
    fs_state.mode = FS_MODE_NORMAL;
    fs_state.FiveMinuteLWP_tranquil = 0;
    fs_state.HostCheckLWP_tranquil = 0;
    fs_state.FsyncCheckLWP_tranquil = 0;
    fs_state.salvsync_fatal_error = 0;

    fs_state.options.fs_state_save = 1;
    fs_state.options.fs_state_restore = 1;
    fs_state.options.fs_state_verify_before_save = 1;
    fs_state.options.fs_state_verify_after_restore = 1;

    opr_cv_init(&fs_state.worker_done_cv, "worker done");
    opr_Verify(pthread_rwlock_init(&fs_state.state_lock, NULL) == 0);
}
# endif /* AFS_NT40_ENV */
#endif /* AFS_DEMAND_ATTACH_FS */

/*
 * Home for the performance statistics.
 */

/* DEBUG HACK */
#ifndef AFS_NT40_ENV
void
CheckDescriptors_Signal(int signo)
{
    struct afs_stat status;
    int tsize = getdtablesize();
    int i;
    for (i = 0; i < tsize; i++) {
	if (afs_fstat(i, &status) != -1) {
	    printf("%d: dev %x, inode %u, length %u, type/mode %x\n", i,
		   (unsigned int) status.st_dev,
		   (unsigned int) status.st_ino,
		   (unsigned int) status.st_size,
		   status.st_mode);
	}
    }
    fflush(stdout);
}
#endif

/* Signal number for dumping debug info is platform dependent. */
#if defined(AFS_HPUX_ENV)
# define AFS_SIG_CHECK    SIGPOLL
#elif defined(AFS_NT40_ENV)
# define AFS_SIG_CHECK    SIGUSR2
#else
# define AFS_SIG_CHECK    SIGXCPU
#endif
void
CheckSignal_Signal(int x)
{
    if (FS_registered > 0) {
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
}

void
ShutDown_Signal(int x)
{
    ShutDownAndCore(DONTPANIC);
}

/* check whether caller is authorized to perform admin operations */
int
viced_SuperUser(struct rx_call *call)
{
    return afsconf_SuperUser(confDir, call, NULL);
}

/**
 * Return true if this name is a member of the local realm.
 */
int
fs_IsLocalRealmMatch(void *rock, char *name, char *inst, char *cell)
{
    struct afsconf_dir *dir = (struct afsconf_dir *)rock;
    afs_int32 islocal = 0;	/* default to no */
    int code;

    code = afsconf_IsLocalRealmMatch(dir, &islocal, name, inst, cell);
    if (code) {
	ViceLog(0,
		("Failed local realm check; code=%d, name=%s, inst=%s, cell=%s\n",
		 code, name, inst, cell));
    }
    return islocal;
}

#if defined(AFS_NT40_ENV)
/* no viced_syscall */
#elif defined(AFS_DARWIN160_ENV)
/* no viced_syscall */
#elif !defined(AFS_SYSCALL)
int
viced_syscall(afs_uint32 a3, afs_uint32 a4, void *a5)
{
    errno = ENOSYS;
    return -1;
}
#else
int
viced_syscall(afs_uint32 a3, afs_uint32 a4, void *a5)
{
    afs_uint32 rcode;
# ifndef AFS_LINUX20_ENV
    void (*old) (int);

    old = (void (*)(int))signal(SIGSYS, SIG_IGN);
# endif
    rcode = syscall(AFS_SYSCALL, 28 /* AFSCALL_CALL */ , a3, a4, a5);
# ifndef AFS_LINUX20_ENV
    signal(SIGSYS, old);
# endif

    return rcode;
}
#endif

#if !defined(AFS_NT40_ENV)
# include "AFS_component_version_number.c"
#endif /* !AFS_NT40_ENV */

#define MAXADMINNAME 64
char adminName[MAXADMINNAME];

static void
CheckAdminName(void)
{
    int fd = -1;
    struct afs_stat status;

    if ((afs_stat("/AdminName", &status)) ||	/* if file does not exist */
	(status.st_size <= 0) ||	/* or it is too short */
	(status.st_size >= (MAXADMINNAME)) ||	/* or it is too long */
	(fd = afs_open("/AdminName", O_RDONLY, 0)) < 0 || /* or open fails */
	read(fd, adminName, status.st_size) != status.st_size) { /* or read */

	strcpy(adminName, "System:Administrators");	/* use the default name */
    }
    if (fd >= 0)
	close(fd);		/* close fd if it was opened */

}				/*CheckAdminName */


static void
setThreadId(char *s)
{
#if !defined(AFS_NT40_ENV)
    int threadId;

    /* set our 'thread-id' so that the host hold table works */
    threadId = rx_SetThreadNum();
    opr_threadname_set(s);
    ViceLog(0, ("Set thread id 0x%x for '%s'\n", threadId, s));
#endif
}

/* This LWP does things roughly every 5 minutes */
static void *
FiveMinuteCheckLWP(void *unused)
{
    static int msg = 0;
    char tbuffer[32];

    ViceLog(1, ("Starting five minute check process\n"));
    setThreadId("FiveMinuteCheckLWP");

#ifdef AFS_DEMAND_ATTACH_FS
    FS_STATE_WRLOCK;
    while (fs_state.mode == FS_MODE_NORMAL) {
	fs_state.FiveMinuteLWP_tranquil = 1;
	FS_STATE_UNLOCK;
#else
    while (1) {
#endif

	sleep(fiveminutes);

#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
	if (fs_state.mode != FS_MODE_NORMAL) {
	    break;
	}
	fs_state.FiveMinuteLWP_tranquil = 0;
	FS_STATE_UNLOCK;
#endif

	/* close the log so it can be removed */
	ReOpenLog();	/* don't trunc, just append */
	ViceLog(2, ("Cleaning up timed out callbacks\n"));
	if (CleanupTimedOutCallBacks())
	    ViceLog(5, ("Timed out callbacks deleted\n"));
	ViceLog(2, ("Set disk usage statistics\n"));
	VSetDiskUsage();
	if (FS_registered == 1)
	    Do_VLRegisterRPC();
	/* Force wakeup in case we missed something; pthreads does timedwait */
	if (printBanner && (++msg & 1)) {	/* Every 10 minutes */
	    time_t now = time(NULL);
	    struct tm tm;
	    strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y",
		     localtime_r(&now, &tm));
	    ViceLog(2,
		    ("File server is running at %s\n", tbuffer));
	}
#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
#endif
    }
#ifdef AFS_DEMAND_ATTACH_FS
    fs_state.FiveMinuteLWP_tranquil = 1;
    FS_LOCK;
    opr_cv_broadcast(&fs_state.worker_done_cv);
    FS_UNLOCK;
    FS_STATE_UNLOCK;
    return NULL;
#else
    AFS_UNREACHED(return(NULL));
#endif
}				/*FiveMinuteCheckLWP */


/* This LWP does host checks every 5 minutes:  it should not be used for
 * other 5 minute activities because it may be delayed by timeouts when
 * it probes the workstations
 */

static void *
HostCheckLWP(void *unused)
{
    ViceLog(1, ("Starting Host check process\n"));
    setThreadId("HostCheckLWP");
#ifdef AFS_DEMAND_ATTACH_FS
    FS_STATE_WRLOCK;
    while (fs_state.mode == FS_MODE_NORMAL) {
	fs_state.HostCheckLWP_tranquil = 1;
	FS_STATE_UNLOCK;
#else
    while(1) {
#endif

	sleep(fiveminutes);

#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
	if (fs_state.mode != FS_MODE_NORMAL) {
	    break;
	}
	fs_state.HostCheckLWP_tranquil = 0;
	FS_STATE_UNLOCK;
#endif

	ViceLog(2, ("Checking for dead venii & clients\n"));
	h_CheckHosts();

#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
#endif
    }
#ifdef AFS_DEMAND_ATTACH_FS
    fs_state.HostCheckLWP_tranquil = 1;
    FS_LOCK;
    opr_cv_broadcast(&fs_state.worker_done_cv);
    FS_UNLOCK;
    FS_STATE_UNLOCK;
    return NULL;
#else
    AFS_UNREACHED(return(NULL));
#endif
}				/*HostCheckLWP */

/* This LWP does fsync checks every 5 minutes:  it should not be used for
 * other 5 minute activities because it may be delayed by timeouts when
 * it probes the workstations
 */
static void *
FsyncCheckLWP(void *unused)
{
    afs_int32 code;
    struct timespec fsync_next;
    ViceLog(1, ("Starting fsync check process\n"));

    setThreadId("FsyncCheckLWP");

#ifdef AFS_DEMAND_ATTACH_FS
    FS_STATE_WRLOCK;
    while (fs_state.mode == FS_MODE_NORMAL) {
	fs_state.FsyncCheckLWP_tranquil = 1;
	FS_STATE_UNLOCK;
#else
    while(1) {
#endif
	FSYNC_LOCK;
	/* rounding is fine */
	fsync_next.tv_nsec = 0;
	fsync_next.tv_sec = time(0) + fiveminutes;

	code = opr_cv_timedwait(&fsync_cond, &fsync_glock_mutex,
			    &fsync_next);
	FSYNC_UNLOCK;

#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
	if (fs_state.mode != FS_MODE_NORMAL) {
	    break;
	}
	fs_state.FsyncCheckLWP_tranquil = 0;
	FS_STATE_UNLOCK;
#endif /* AFS_DEMAND_ATTACH_FS */

	ViceLog(2, ("Checking for fsync events\n"));
	do {
	    code = BreakLaterCallBacks();
	} while (code != 0);
#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
#endif
    }
#ifdef AFS_DEMAND_ATTACH_FS
    fs_state.FsyncCheckLWP_tranquil = 1;
    FS_LOCK;
    opr_cv_broadcast(&fs_state.worker_done_cv);
    FS_UNLOCK;
    FS_STATE_UNLOCK;
    return NULL;
#else
    AFS_UNREACHED(return(NULL));
#endif /* !AFS_DEMAND_ATTACH_FS */
}

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

static void
ClearXStatValues(void)
{				/*ClearXStatValues */

    struct fs_stats_opTimingData *opTimeP;	/*Ptr to timing struct */
    struct fs_stats_xferData *opXferP;	/*Ptr to xfer struct */
    int i;			/*Loop counter */

    /*
     * Zero all xstat-related structures.
     */
    memset((&afs_perfstats), 0, sizeof(struct afs_PerfStats));
    memset((&afs_FullPerfStats), 0,
	   sizeof(struct fs_stats_FullPerfStats));

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
# ifdef SYS_NAME_ID
    afs_perfstats.sysname_ID = SYS_NAME_ID;
# else
#  ifndef AFS_NT40_ENV
    ViceLog(0, ("Sys name ID constant not defined in param.h!!\n"));
    ViceLog(0, ("[Choosing ``undefined'' sys name ID.\n"));
#  endif
    afs_perfstats.sysname_ID = SYS_NAME_ID_UNDEFINED;
# endif /* SYS_NAME_ID */

}				/*ClearXStatValues */

int CopyOnWrite_calls = 0, CopyOnWrite_off0 = 0, CopyOnWrite_size0 = 0;
afs_fsize_t CopyOnWrite_maxsize = 0;

static void
PrintCounters(void)
{
    int dirbuff, dircall, dirio;
    struct timeval tpl;
    int workstations, activeworkstations, delworkstations;
    int processSize = 0;
    char tbuffer[32];
    struct tm tm;
#ifdef AFS_DEMAND_ATTACH_FS
    int stats_flags = 0;
#endif

    gettimeofday(&tpl, 0);
    Statistics = 1;
    strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y",
	     localtime_r(&StartTime, &tm));
    ViceLog(0, ("Vice was last started at %s\n", tbuffer));

#ifdef AFS_DEMAND_ATTACH_FS
    if (GetLogLevel() >= 125) {
	stats_flags = VOL_STATS_PER_CHAIN2;
    } else if (GetLogLevel() >= 25) {
	stats_flags = VOL_STATS_PER_CHAIN;
    }
    VPrintExtendedCacheStats(stats_flags);
#endif
    VPrintCacheStats();
    VPrintDiskStats();
    DStat(&dirbuff, &dircall, &dirio);
    ViceLog(0,
	    ("With %d directory buffers; %d reads resulted in %d read I/Os\n",
	     dirbuff, dircall, dirio));
    rx_PrintStats(stderr);
    audit_PrintStats(stderr);
    h_PrintStats();
    PrintCallBackStats();
    processSize = opr_procsize();
    ViceLog(0,
	    ("There are %d connections, process size %d\n",
	     CurrentConnections, processSize));
    h_GetWorkStats(&workstations, &activeworkstations, &delworkstations,
		   tpl.tv_sec - 15 * 60);
    ViceLog(0,
	    ("There are %d workstations, %d are active (req in < 15 mins), %d marked \"down\"\n",
	     workstations, activeworkstations, delworkstations));
    ViceLog(0, ("CopyOnWrite: calls %d off0 %d size0 %d maxsize 0x%llx\n",
		CopyOnWrite_calls, CopyOnWrite_off0, CopyOnWrite_size0, CopyOnWrite_maxsize));

    Statistics = 0;

}				/*PrintCounters */

static void *
ShutdownWatchdogLWP(void *unused)
{
    opr_threadname_set("ShutdownWatchdog");
    sleep(panic_timeout);
    ViceLogThenPanic(0, ("ShutdownWatchdogLWP: Failed to shutdown and panic "
                         "within %d seconds; forcing panic\n",
			 panic_timeout));
    return NULL;
}

void
ShutDownAndCore(int dopanic)
{
    time_t now = time(NULL);
    struct tm tm;
    char tbuffer[32];

    if (dopanic) {
	pthread_t watchdogPid;
	pthread_attr_t tattr;
	opr_Verify(pthread_attr_init(&tattr) == 0);
	opr_Verify(pthread_create(&watchdogPid, &tattr,
				  ShutdownWatchdogLWP, NULL) == 0);
    }

    /* do not allows new reqests to be served from now on, all new requests
     * are returned with an error code of RX_RESTARTING ( transient failure ) */
    rx_SetRxTranquil();		/* dhruba */

    VSetTranquil();

#ifdef AFS_DEMAND_ATTACH_FS
    FS_STATE_WRLOCK;
    if (fs_state.mode == FS_MODE_SHUTDOWN) {
	/* it is possible for at least fs_stateSave() (called below) to call
	 * ShutDownAndCore if there's host list corruption; prevent
	 * deinitializing some stuff twice */
	ViceLog(0, ("ShutDownAndCore called during shutdown? Skipping volume "
	            "and host package shutdown\n"));
	FS_STATE_UNLOCK;
	goto done_vol_host;
    }
    fs_state.mode = FS_MODE_SHUTDOWN;
    FS_STATE_UNLOCK;
#endif

    strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y",
	     localtime_r(&now, &tm));
    ViceLog(0, ("Shutting down file server at %s\n", tbuffer));
    if (dopanic)
	ViceLog(0, ("ABNORMAL SHUTDOWN, see core file.\n"));
    DFlush();
    if (!dopanic)
	PrintCounters();

    /* allow audit interfaces to shutdown */
    osi_audit_close();
    /* shut down volume package */
    VShutdown();

#ifdef AFS_DEMAND_ATTACH_FS
    if (fs_state.options.fs_state_save) {
	/*
	 * demand attach fs
	 * save fileserver state to disk */

	if (dopanic) {
	    ViceLog(0, ("Not saving fileserver state; abnormal shutdown\n"));

	} else {
	    /* make sure background threads have finished all of their asynchronous
	     * work on host and callback structures */
	    FS_STATE_RDLOCK;
	    while (!fs_state.FiveMinuteLWP_tranquil ||
	           !fs_state.HostCheckLWP_tranquil ||
	           !fs_state.FsyncCheckLWP_tranquil) {
		FS_LOCK;
		FS_STATE_UNLOCK;
		ViceLog(0, ("waiting for background host/callback threads to quiesce before saving fileserver state...\n"));
		opr_cv_wait(&fs_state.worker_done_cv, &fileproc_glock_mutex);
		FS_UNLOCK;
		FS_STATE_RDLOCK;
	    }
	    FS_STATE_UNLOCK;

	    /* ok. it should now be fairly safe. let's do the state dump */
	    fs_stateSave();
	}
    }
 done_vol_host:

#endif /* AFS_DEMAND_ATTACH_FS */

    if (debugFile) {
	rx_PrintStats(debugFile);
	fflush(debugFile);
    }
    now = time(0);
    strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y",
	     localtime_r(&now, &tm));
    if (dopanic) {
      ViceLog(0, ("File server has terminated abnormally at %s\n", tbuffer));
    } else {
      ViceLog(0, ("File server has terminated normally at %s\n", tbuffer));
    }

    if (dopanic) /* XXX pass in file and line? */
	osi_Panic("Panic requested\n");

    exit(0);
}

static afs_int32
ParseRights(char *arights)
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
	mode =
	    PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
	    PRSFS_WRITE | PRSFS_LOCK;
    else if (!strcmp(arights, "all"))
	mode =
	    PRSFS_READ | PRSFS_LOOKUP | PRSFS_INSERT | PRSFS_DELETE |
	    PRSFS_WRITE | PRSFS_LOCK | PRSFS_ADMINISTER;
    else {
	len = strlen(arights);
	for (i = 0; i < len; i++) {
	    tc = *arights++;
	    if (tc == 'r')
		mode |= PRSFS_READ;
	    else if (tc == 'l')
		mode |= PRSFS_LOOKUP;
	    else if (tc == 'i')
		mode |= PRSFS_INSERT;
	    else if (tc == 'd')
		mode |= PRSFS_DELETE;
	    else if (tc == 'w')
		mode |= PRSFS_WRITE;
	    else if (tc == 'k')
		mode |= PRSFS_LOCK;
	    else if (tc == 'a')
		mode |= PRSFS_ADMINISTER;
	    else if (tc == 'A')
		mode |= PRSFS_USR0;
	    else if (tc == 'B')
		mode |= PRSFS_USR1;
	    else if (tc == 'C')
		mode |= PRSFS_USR2;
	    else if (tc == 'D')
		mode |= PRSFS_USR3;
	    else if (tc == 'E')
		mode |= PRSFS_USR4;
	    else if (tc == 'F')
		mode |= PRSFS_USR5;
	    else if (tc == 'G')
		mode |= PRSFS_USR6;
	    else if (tc == 'H')
		mode |= PRSFS_USR7;
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
 * Linux:       sysconf() limit is apparently NOT real -- too big
 * Solaris:     no sysconf() limit
 */
static int
max_fileserver_thread(void)
{
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV)
    long ans;

    ans = sysconf(_SC_THREAD_THREADS_MAX);
    if (0 < ans && ans < MAX_FILESERVER_THREAD)
	return (int)ans;
#endif
    return MAX_FILESERVER_THREAD;
}

/* from ihandle.c */
extern ih_init_params vol_io_params;

enum optionsList {
    OPT_large,
    OPT_small,
    OPT_banner,
    OPT_implicit,
    OPT_lock,
    OPT_readonly,
    OPT_adminwrite,
    OPT_saneacls,
    OPT_buffers,
    OPT_callbacks,
    OPT_vcsize,
    OPT_lvnodes,
    OPT_svnodes,
    OPT_sendsize,
    OPT_minspare,
    OPT_spare,
    OPT_pctspare,
    OPT_hostcpsrefresh,
    OPT_vattachthreads,
    OPT_abortthreshold,
    OPT_busyat,
    OPT_nobusy,
    OPT_offline_timeout,
    OPT_offline_shutdown_timeout,
    OPT_vhandle_setaside,
    OPT_vhandle_max_cachesize,
    OPT_vhandle_initial_cachesize,
    OPT_fs_state_dont_save,
    OPT_fs_state_dont_restore,
    OPT_fs_state_verify,
    OPT_vhashsize,
    OPT_vlrudisable,
    OPT_vlruthresh,
    OPT_vlruinterval,
    OPT_vlrumax,
    OPT_unsafe_nosalvage,
    OPT_cbwait,
    OPT_novbc,
    OPT_auditlog,
    OPT_auditiface,
    OPT_config,
    OPT_debug,
    OPT_logfile,
    OPT_mrafslogs,
    OPT_threads,
#ifdef HAVE_SYSLOG
    OPT_syslog,
#endif
    OPT_peer,
    OPT_process,
    OPT_nojumbo,
    OPT_jumbo,
    OPT_rxbind,
    OPT_rxdbg,
    OPT_rxdbge,
    OPT_rxpck,
    OPT_rxmaxmtu,
    OPT_udpsize,
    OPT_dotted,
    OPT_realm,
    OPT_sync,
    OPT_transarc_logs
};

static int
ParseArgs(int argc, char *argv[])
{
    int code;
    int optval;
    char *optstring = NULL;
    struct cmd_item *optlist;
    struct cmd_syndesc *opts;

    int lwps_max;
    char *auditIface = NULL;
    struct cmd_item *auditLogList = NULL;
    char *sync_behavior = NULL;

#if defined(AFS_AIX32_ENV)
    extern int aixlow_water;
#endif

    opts = cmd_CreateSyntax(NULL, NULL, NULL, 0, NULL);

    /* fileserver options */
    cmd_AddParmAtOffset(opts, OPT_large, "-L", CMD_FLAG,
			CMD_OPTIONAL, "defaults for a 'large' server");
    cmd_AddParmAtOffset(opts, OPT_small, "-S", CMD_FLAG,
			CMD_OPTIONAL, "defaults for a 'small' server");

    cmd_AddParmAtOffset(opts, OPT_banner, "-banner", CMD_FLAG,
			CMD_OPTIONAL, "print regular banners to log");
    cmd_AddParmAtOffset(opts, OPT_implicit, "-implicit", CMD_SINGLE,
			CMD_OPTIONAL, "implicit admin access bits");

#if defined(AFS_SGI_ENV)
    cmd_AddParmAtOffset(opts, OPT_lock, "-lock", CMD_FLAG,
			CMD_OPTIONAL, "lock filserver binary in memory");
#endif
    cmd_AddParmAtOffset(opts, OPT_readonly, "-readonly", CMD_FLAG,
			CMD_OPTIONAL, "be a readonly fileserver");
    cmd_AddParmAtOffset(opts, OPT_adminwrite, "-admin-write", CMD_FLAG,
			CMD_OPTIONAL, "if read-only, allow writes for users "
			"from system:administrators");
    cmd_AddParmAtOffset(opts, OPT_saneacls, "-saneacls", CMD_FLAG,
		        CMD_OPTIONAL, "set the saneacls capability bit");

    cmd_AddParmAtOffset(opts, OPT_buffers, "-b", CMD_SINGLE,
			CMD_OPTIONAL, "buffers");
    cmd_AddParmAtOffset(opts, OPT_callbacks, "-cb", CMD_SINGLE,
			CMD_OPTIONAL, "number of callbacks");
    cmd_AddParmAtOffset(opts, OPT_vcsize, "-vc", CMD_SINGLE,
			CMD_OPTIONAL, "volume cachesize");
    cmd_AddParmAtOffset(opts, OPT_lvnodes, "-l", CMD_SINGLE,
			CMD_OPTIONAL, "large vnodes");
    cmd_AddParmAtOffset(opts, OPT_svnodes, "-s", CMD_SINGLE,
			CMD_OPTIONAL, "small vnodes");
    cmd_AddParmAtOffset(opts, OPT_sendsize, "-sendsize", CMD_SINGLE,
			CMD_OPTIONAL, "size of send buffer in bytes");

#if defined(AFS_AIX32_ENV)
    cmd_AddParmAtOffset(opts, OPT_minspare, "-m", CMD_SINGLE,
			CMD_OPTIONAL, "minimum percentage spare in partition");
#endif

    cmd_AddParmAtOffset(opts, OPT_spare, "-spare", CMD_SINGLE,
			CMD_OPTIONAL, "kB overage on volume quota");
    cmd_AddParmAtOffset(opts, OPT_pctspare, "-pctspare", CMD_SINGLE,
			CMD_OPTIONAL, "percentage overage on volume quota");

    cmd_AddParmAtOffset(opts, OPT_hostcpsrefresh, "-hr", CMD_SINGLE,
			CMD_OPTIONAL, "hours between host CPS refreshes");

    cmd_AddParmAtOffset(opts, OPT_vattachthreads, "-vattachpar", CMD_SINGLE,
			CMD_OPTIONAL, "# of volume attachment threads");

    cmd_AddParmAtOffset(opts, OPT_abortthreshold, "-abortthreshold",
			CMD_SINGLE, CMD_OPTIONAL,
			"abort threshold");
    cmd_AddParmAtOffset(opts, OPT_busyat, "-busyat", CMD_SINGLE, CMD_OPTIONAL,
			"# of queued entries after which server is busy");
    cmd_AddParmAtOffset(opts, OPT_nobusy, "-nobusy", CMD_FLAG, CMD_OPTIONAL,
			"send VRESTARTING while restarting the server");

    cmd_AddParmAtOffset(opts, OPT_offline_timeout, "-offline-timeout",
			CMD_SINGLE, CMD_OPTIONAL,
			"timeout for offlining volumes");
    cmd_AddParmAtOffset(opts, OPT_offline_shutdown_timeout,
			"-offline-shutdown-timeout", CMD_SINGLE, CMD_OPTIONAL,
			"timeout offlining volumes during shutdown");

    cmd_AddParmAtOffset(opts, OPT_vhandle_setaside, "-vhandle-setaside",
			CMD_SINGLE, CMD_OPTIONAL,
			"# fds reserved for non-cache IO");
    cmd_AddParmAtOffset(opts, OPT_vhandle_max_cachesize, 
			"-vhandle-max-cachesize", CMD_SINGLE, CMD_OPTIONAL,
			"max open files");
    cmd_AddParmAtOffset(opts, OPT_vhandle_initial_cachesize,
			"-vhandle-initial-cachesize", CMD_SINGLE,
			CMD_OPTIONAL, "# fds reserved for cache IO");
    cmd_AddParmAtOffset(opts, OPT_vhashsize, "-vhashsize",
			CMD_SINGLE, CMD_OPTIONAL,
			"log(2) of # of volume hash buckets");

#ifdef AFS_DEMAND_ATTACH_FS
    /* dafs options */
    cmd_AddParmAtOffset(opts, OPT_fs_state_dont_save,
			"-fs-state-dont-save", CMD_FLAG, CMD_OPTIONAL,
			"disable state save during shutdown");
    cmd_AddParmAtOffset(opts, OPT_fs_state_dont_restore,
			"-fs-state-dont-restore", CMD_FLAG, CMD_OPTIONAL,
			"disable state restore during startup");
    cmd_AddParmAtOffset(opts, OPT_fs_state_verify, "-fs-state-verify",
			CMD_SINGLE, CMD_OPTIONAL, "none|save|restore|both");
    cmd_AddParmAtOffset(opts, OPT_vlrudisable, "-vlrudisable",
			CMD_FLAG, CMD_OPTIONAL, "disable VLRU functionality");
    cmd_AddParmAtOffset(opts, OPT_vlruthresh, "-vlruthresh",
			CMD_SINGLE, CMD_OPTIONAL,
			"mins before unused vols become eligible for detach");
    cmd_AddParmAtOffset(opts, OPT_vlruinterval, "-vlruinterval",
			CMD_SINGLE, CMD_OPTIONAL, "secs between VLRU scans");
    cmd_AddParmAtOffset(opts, OPT_vlrumax, "-vlrumax", CMD_SINGLE, CMD_OPTIONAL,
		        "max volumes to detach in one scan");
    cmd_AddParmAtOffset(opts, OPT_unsafe_nosalvage, "-unsafe-nosalvage",
			CMD_FLAG, CMD_OPTIONAL,
			"bypass safety checks on volume attach");
#endif

    /* unrecommend options - should perhaps be CMD_HIDE */
    cmd_AddParmAtOffset(opts, OPT_cbwait, "-w", CMD_SINGLE, CMD_OPTIONAL,
			"callback wait interval");
    cmd_AddParmAtOffset(opts, OPT_novbc, "-novbc", CMD_FLAG, CMD_OPTIONAL,
			"disable callback breaks on reattach");

    /* general options */
    cmd_AddParmAtOffset(opts, OPT_auditlog, "-auditlog", CMD_LIST,
			CMD_OPTIONAL, "[interface:]path[:options]");
    cmd_AddParmAtOffset(opts, OPT_auditiface, "-audit-interface", CMD_SINGLE,
			CMD_OPTIONAL, "default interface");
    cmd_AddParmAtOffset(opts, OPT_debug, "-d", CMD_SINGLE, CMD_OPTIONAL,
			"debug level");
    cmd_AddParmAtOffset(opts, OPT_mrafslogs, "-mrafslogs", CMD_FLAG,
			CMD_OPTIONAL, "enable MRAFS style logging");
    cmd_AddParmAtOffset(opts, OPT_transarc_logs, "-transarc-logs", CMD_FLAG,
			CMD_OPTIONAL, "enable Transarc style logging");
    cmd_AddParmAtOffset(opts, OPT_threads, "-p", CMD_SINGLE, CMD_OPTIONAL,
		        "number of threads");
#ifdef HAVE_SYSLOG
    cmd_AddParmAtOffset(opts, OPT_syslog, "-syslog", CMD_SINGLE_OR_FLAG,
			CMD_OPTIONAL, "log to syslog");
#endif

    /* rx options */
    cmd_AddParmAtOffset(opts, OPT_peer, "-enable_peer_stats", CMD_FLAG,
			CMD_OPTIONAL, "enable RX RPC statistics by peer");
    cmd_AddParmAtOffset(opts, OPT_process, "-enable_process_stats", CMD_FLAG,
			CMD_OPTIONAL, "enable RX RPC statistics");
    cmd_AddParmAtOffset(opts, OPT_nojumbo, "-nojumbo", CMD_FLAG,
			CMD_OPTIONAL, "disable jumbograms");
    cmd_AddParmAtOffset(opts, OPT_jumbo, "-jumbo", CMD_FLAG, CMD_OPTIONAL,
			"enable jumbograms");
    cmd_AddParmAtOffset(opts, OPT_rxbind, "-rxbind", CMD_FLAG, CMD_OPTIONAL,
			"bind only to the primary interface");
    cmd_AddParmAtOffset(opts, OPT_rxdbg, "-rxdbg", CMD_FLAG, CMD_OPTIONAL,
			"enable rx debugging");
    cmd_AddParmAtOffset(opts, OPT_rxdbge, "-rxdbge", CMD_FLAG, CMD_OPTIONAL,
			"enable rx event debugging");
    cmd_AddParmAtOffset(opts, OPT_rxpck, "-rxpck", CMD_SINGLE, CMD_OPTIONAL,
			"# of extra rx packets");
    cmd_AddParmAtOffset(opts, OPT_rxmaxmtu, "-rxmaxmtu", CMD_SINGLE,
			CMD_OPTIONAL, "maximum MTU for RX");
    cmd_AddParmAtOffset(opts, OPT_udpsize, "-udpsize", CMD_SINGLE,
			CMD_OPTIONAL, "size of socket buffer in bytes");

    /* rxkad options */
    cmd_AddParmAtOffset(opts, OPT_dotted, "-allow-dotted-principals",
			CMD_FLAG, CMD_OPTIONAL,
			"permit Kerberos 5 principals with dots");
    cmd_AddParmAtOffset(opts, OPT_realm, "-realm",
			CMD_LIST, CMD_OPTIONAL, "local realm");
    cmd_AddParmAtOffset(opts, OPT_sync, "-sync",
			CMD_SINGLE, CMD_OPTIONAL, "always | onclose | never");

    /* testing options */
    cmd_AddParmAtOffset(opts, OPT_logfile, "-logfile", CMD_SINGLE,
	    CMD_OPTIONAL, "location of log file");
    cmd_AddParmAtOffset(opts, OPT_config, "-config", CMD_SINGLE,
	    CMD_OPTIONAL, "configuration location");

    code = cmd_Parse(argc, argv, &opts);
    if (code == CMD_HELP) {
	exit(0);
    }
    if (code)
	return -1;

    cmd_OpenConfigFile(AFSDIR_SERVER_CONFIG_FILE_FILEPATH);
    cmd_SetCommandName("fileserver");

    if (cmd_OptionPresent(opts, OPT_large)
	&& cmd_OptionPresent(opts, OPT_small)) {
	printf("Only one of -L or -S must be specified\n");
	return -1;
    }

    if (cmd_OptionPresent(opts, OPT_spare)
	&& cmd_OptionPresent(opts, OPT_pctspare)) {
	printf("Both -spare and -pctspare specified, exiting.\n");
	return -1;
    }

    if (cmd_OptionPresent(opts, OPT_large)) {
	rxpackets = 200;
	nSmallVns = 600;
	large = 600;
	numberofcbs = 64000;
	lwps = 128;
	buffs = 120;
	volcache = 600;
    }

    if (cmd_OptionPresent(opts, OPT_small)) {
	rxpackets = 100;
	nSmallVns = 200;
	large = 200;
	numberofcbs = 20000;
	lwps = 6;
	buffs = 70;
	volcache = 200;
    }

    cmd_OptionAsFlag(opts, OPT_banner, &printBanner);

    if (cmd_OptionAsString(opts, OPT_implicit, &optstring) == 0) {
	implicitAdminRights = ParseRights(optstring);
	free(optstring);
	optstring = NULL;
	if (implicitAdminRights < 0)
	    return implicitAdminRights;
    }

#if defined(AFS_SGI_ENV)
    cmd_OptionAsFlag(opts, OPT_lock, &SawLock);
#endif
    cmd_OptionAsFlag(opts, OPT_readonly, &readonlyServer);
    cmd_OptionAsFlag(opts, OPT_adminwrite, &adminwriteServer);
    cmd_OptionAsFlag(opts, OPT_saneacls, &saneacls);
    cmd_OptionAsInt(opts, OPT_buffers, &buffs);

    if (cmd_OptionAsInt(opts, OPT_callbacks, &numberofcbs) == 0) {
	if ((numberofcbs < 10000) || (numberofcbs > 2147483647)) {
	    printf("number of cbs %d invalid; "
		   "must be between 10000 and 2147483647\n", numberofcbs);
	    return -1;
	}
    }

    cmd_OptionAsInt(opts, OPT_vcsize, &volcache);
    cmd_OptionAsInt(opts, OPT_lvnodes, &large);
    cmd_OptionAsInt(opts, OPT_svnodes, &nSmallVns);
    if (cmd_OptionAsInt(opts, OPT_sendsize, &optval) == 0) {
	if (optval < 16384) {
	    printf("Warning:sendsize %d is less than minimum %d; ignoring\n",
		   optval, 16384);
	} else
	    sendBufSize = optval;
    }

#if defined(AFS_AIX32_ENV)
    if (cmd_OptionAsInt(opts, OPT_minspare, &aixlow_water) == 0) {
	if ((aixlow_water < 0) || (aixlow_water > 30)) {
	    printf("space reserved %d%% invalid; must be between 0-30%%\n",
		   aixlow_water);
	    return -1;
	}
    }
#endif

    cmd_OptionAsInt(opts, OPT_spare, &BlocksSpare);
    if (cmd_OptionAsInt(opts, OPT_pctspare, &PctSpare) == 0) {
	BlocksSpare = 0;
    }

    if (cmd_OptionAsInt(opts, OPT_hostcpsrefresh, &optval) == 0) {
	if ((optval < 1) || (optval > 36)) {
	    printf("host acl refresh interval of %d hours is invalid; "
		   "hours must be between 1 and 36\n\n", optval);
	    return -1;
	}
	hostaclRefresh = optval * 60 * 60;
    }

    cmd_OptionAsInt(opts, OPT_vattachthreads, &vol_attach_threads);

    cmd_OptionAsInt(opts, OPT_abortthreshold, &abort_threshold);

    /* busyat is at the end, as rxpackets has to be set before we can use it */
    if (cmd_OptionPresent(opts, OPT_nobusy))
	busyonrst = 0;

    if (cmd_OptionAsInt(opts, OPT_offline_timeout, &offline_timeout) == 0) {
	if (offline_timeout < -1) {
	    printf("Invalid -offline-timeout value %d; the only valid "
		   "negative value is -1\n", offline_timeout);
	    return -1;
	}
    }

    if (cmd_OptionAsInt(opts, OPT_offline_shutdown_timeout,
			&offline_shutdown_timeout) == 0) {
	if (offline_shutdown_timeout < -1) {
	    printf("Invalid -offline-timeout value %d; the only valid "
		   "negative value is -1\n", offline_shutdown_timeout);
	    return -1;
	}
    }

    cmd_OptionAsUint(opts, OPT_vhandle_setaside,
		    &vol_io_params.fd_handle_setaside);
    cmd_OptionAsUint(opts, OPT_vhandle_max_cachesize,
		    &vol_io_params.fd_max_cachesize);
    cmd_OptionAsUint(opts, OPT_vhandle_initial_cachesize,
		    &vol_io_params.fd_initial_cachesize);
    if (cmd_OptionAsString(opts, OPT_sync, &sync_behavior) == 0) {
	if (ih_SetSyncBehavior(sync_behavior)) {
	    printf("Invalid -sync value %s\n", sync_behavior);
	    return -1;
	}
    }
    if (cmd_OptionAsInt(opts, OPT_vhashsize, &optval) == 0) {
	if (VSetVolHashSize(optval)) {
	    fprintf(stderr, "specified -vhashsize (%d) is invalid or out "
		            "of range\n", optval);
	    return -1;
	}
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (cmd_OptionPresent(opts, OPT_fs_state_dont_save))
	fs_state.options.fs_state_save = 0;
    if (cmd_OptionPresent(opts, OPT_fs_state_dont_restore))
	fs_state.options.fs_state_restore = 0;
    if (cmd_OptionAsString(opts, OPT_fs_state_verify, &optstring) == 0) {
	if (strcmp(optstring, "none") == 0) {
	    fs_state.options.fs_state_verify_before_save = 0;
	    fs_state.options.fs_state_verify_after_restore = 0;
	} else if (strcmp(optstring, "save") == 0) {
	    fs_state.options.fs_state_verify_after_restore = 0;
	} else if (strcmp(optstring, "restore") == 0) {
	    fs_state.options.fs_state_verify_before_save = 0;
	} else if (strcmp(optstring, "both") == 0) {
	    /* default */
	} else {
	    fprintf(stderr, "invalid argument for -fs-state-verify\n");
	    return -1;
	}
    }
    if (cmd_OptionPresent(opts, OPT_vlrudisable))
	VLRU_SetOptions(VLRU_SET_ENABLED, 0);
    if (cmd_OptionAsInt(opts, OPT_vlruthresh, &optval) == 0)
	VLRU_SetOptions(VLRU_SET_THRESH, 60*optval);
    if (cmd_OptionAsInt(opts, OPT_vlruinterval, &optval) == 0)
	VLRU_SetOptions(VLRU_SET_INTERVAL, optval);
    if (cmd_OptionAsInt(opts, OPT_vlrumax, &optval) == 0)
	VLRU_SetOptions(VLRU_SET_MAX, optval);
    cmd_OptionAsFlag(opts, OPT_unsafe_nosalvage, &unsafe_attach);
#endif /* AFS_DEMAND_ATTACH_FS */

    cmd_OptionAsInt(opts, OPT_cbwait, &fiveminutes);
    cmd_OptionAsFlag(opts, OPT_novbc, &novbc);

    /* general server options */
    cmd_OptionAsString(opts, OPT_auditiface, &auditIface);
    cmd_OptionAsList(opts, OPT_auditlog, &auditLogList);

    if (cmd_OptionAsInt(opts, OPT_threads, &lwps) == 0) {
	lwps_max = max_fileserver_thread() - FILESERVER_HELPER_THREADS;
	if (lwps > lwps_max)
	    lwps = lwps_max;
	else if (lwps <6)
	    lwps = 6;
    }

    /* Logging options. */
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
	if (cmd_OptionPresent(opts, OPT_mrafslogs)) {
	    fprintf(stderr, "Invalid options: -syslog and -mrafslogs are exclusive.\n");
	    return -1;
	}

	logopts.lopt_dest = logDest_syslog;
	logopts.lopt_facility = LOG_DAEMON;
	logopts.lopt_tag = "fileserver";
	cmd_OptionAsInt(opts, OPT_syslog, &logopts.lopt_facility);
    } else
#endif
    {
	logopts.lopt_dest = logDest_file;

	if (cmd_OptionPresent(opts, OPT_transarc_logs)) {
	    if (cmd_OptionPresent(opts, OPT_mrafslogs)) {
		fprintf(stderr,
			"Invalid options: -transarc-logs and -mrafslogs are exclusive.\n");
		return -1;
	    }
	    logopts.lopt_rotateOnOpen = 1;
	    logopts.lopt_rotateStyle = logRotate_old;
	} else if (cmd_OptionPresent(opts, OPT_mrafslogs)) {
	    logopts.lopt_rotateOnOpen = 1;
	    logopts.lopt_rotateOnReset = 1;
	    logopts.lopt_rotateStyle = logRotate_timestamp;
	}

	if (cmd_OptionPresent(opts, OPT_logfile))
	    cmd_OptionAsString(opts, OPT_logfile, (char**)&logopts.lopt_filename);
	else
	    logopts.lopt_filename = AFSDIR_SERVER_FILELOG_FILEPATH;

    }
    cmd_OptionAsInt(opts, OPT_debug, &logopts.lopt_logLevel);

    if (cmd_OptionPresent(opts, OPT_peer))
	rx_enablePeerRPCStats();
    if (cmd_OptionPresent(opts, OPT_process))
	rx_enableProcessRPCStats();
    if (cmd_OptionPresent(opts, OPT_nojumbo))
	rxJumbograms = 0;
    if (cmd_OptionPresent(opts, OPT_jumbo))
	rxJumbograms = 1;
    cmd_OptionAsFlag(opts, OPT_rxbind, &rxBind);
    cmd_OptionAsFlag(opts, OPT_rxdbg, &rxlog);
    cmd_OptionAsFlag(opts, OPT_rxdbge, &eventlog);
    cmd_OptionAsInt(opts, OPT_rxpck, &rxpackets);

    cmd_OptionAsInt(opts, OPT_rxmaxmtu, &rxMaxMTU);

    if (cmd_OptionAsInt(opts, OPT_udpsize, &optval) == 0) {
	if (optval < rx_GetMinUdpBufSize()) {
	    printf("Warning:udpsize %d is less than minimum %d; ignoring\n",
		   optval, rx_GetMinUdpBufSize());
	} else
	    udpBufSize = optval;
    }

    /* rxkad options */
    cmd_OptionAsFlag(opts, OPT_dotted, &rxkadDisableDotCheck);
    if (cmd_OptionAsList(opts, OPT_realm, &optlist) == 0) {

	for (; optlist != NULL; optlist=optlist->next) {
	    if (strlen(optlist->data) >= AFS_REALM_SZ) {
		printf("-realm argument must contain fewer than %d "
		       "characters.\n", AFS_REALM_SZ);
		return -1;
	    }
	    afsconf_SetLocalRealm(optlist->data); /* overrides krb.conf file, if one */
	}
    }

    /* anything setting rxpackets must come before this */
    if (cmd_OptionAsInt(opts, OPT_busyat, &optval) == 0) {
	if (optval < 10) {
	    printf("Busy threshold %d is too low, will use default.\n",
		   busy_threshold);
	    busy_threshold = 3 * rxpackets / 2;
	} else {
	    busy_threshold = optval;
	}
    } else {
	busy_threshold = 3 * rxpackets / 2;
    }

    cmd_OptionAsString(opts, OPT_config, &FS_configPath);

    code = osi_audit_cmd_Options(auditIface, auditLogList);
    free(auditIface);
    if (code)
	return -1;

    if (lwps > 64) {
	host_thread_quota = 5;
    } else if (lwps > 32) {
	host_thread_quota = 4;
    } else if (lwps > 16) {
	host_thread_quota = 3;
    } else {
	host_thread_quota = 2;
    }

    return (0);
}				/*ParseArgs */

/* Once upon a time, in a galaxy far far away, IBM AFS supported the use of
 * a file /vice/file/parms, the contents of which would override any command
 * line parameters. We no longer support the use of such a file, but we warn
 * if we encounter its presence from an older release
 */
static void
CheckParms(void)
{
    struct afs_stat sbuf;

    if (afs_stat("/vice/file/parms", &sbuf) == 0) {
	ViceLog(0, ("Using /vice/file/parms to override command line "
		    "options is no longer supported"));
    }
}

/* Miscellaneous routines */
void
Die(const char *msg)
{

    ViceLogThenPanic(0, ("%s\n", msg));
}				/*Die */


afs_int32
InitPR(void)
{
    int code;

    /*
     * If this fails, it's because something major is wrong, and is not
     * likely to be time dependent.
     */
    code = pr_Initialize(2, AFSDIR_SERVER_ETC_DIRPATH, 0);
    if (code != 0) {
	ViceLog(0,
		("Couldn't initialize protection library; code=%d.\n", code));
	return code;
    }

    opr_Verify(pthread_key_create(&viced_uclient_key, NULL) == 0);

    SystemId = SYSADMINID;
    SystemAnyUser = ANYUSERID;
    SystemAnyUserCPS.prlist_len = 0;
    SystemAnyUserCPS.prlist_val = NULL;
    AnonCPS.prlist_len = 0;
    AnonCPS.prlist_val = NULL;
    while (1) {
	code = pr_GetCPS(SystemAnyUser, &SystemAnyUserCPS);
	if (code != 0) {
	    ViceLog(0,
		    ("Couldn't get CPS for AnyUser, will try again in 30 seconds; code=%d.\n",
		     code));
	    goto sleep;
	}
	code = pr_GetCPS(ANONYMOUSID, &AnonCPS);
	if (code != 0) {
	    ViceLog(0,
		    ("Couldn't get Anonymous CPS, exiting; code=%d.\n",
		     code));
	    return -1;
	}
	AnonymousID = ANONYMOUSID;
	return 0;
      sleep:
	sleep(30);
    }
}				/*InitPR */

static struct ubik_client *cstruct;

static afs_int32
vl_Initialize(struct afsconf_dir *dir)
{
    afs_int32 code, i;
    afs_int32 scIndex = RX_SECIDX_NULL;
    struct afsconf_cell info;
    struct rx_securityClass *sc;
    struct rx_connection *serverconns[MAXSERVERS];

    memset(serverconns, 0, sizeof(serverconns));
    code = afsconf_ClientAuth(dir, &sc, &scIndex);
    if (code) {
	ViceLog(0, ("Could not get security object for localAuth\n"));
	exit(1);
    }
    code = afsconf_GetCellInfo(dir, NULL, AFSCONF_VLDBSERVICE, &info);
    if (code) {
	ViceLog(0,
		("vl_Initialize: Failed to get cell information\n"));
	exit(1);
    }
    if (info.numServers > MAXSERVERS) {
	ViceLog(0,
		("vl_Initialize: info.numServers=%d (> MAXSERVERS=%d)\n",
		 info.numServers, MAXSERVERS));
	exit(1);
    }
    for (i = 0; i < info.numServers; i++)
	serverconns[i] =
	    rx_NewConnection(info.hostAddr[i].sin_addr.s_addr,
			     info.hostAddr[i].sin_port, USER_SERVICE_ID, sc,
			     scIndex);
    code = ubik_ClientInit(serverconns, &cstruct);
    if (code) {
	ViceLog(0, ("vl_Initialize: ubik client init failed.\n"));
	return code;
    }
    return 0;
}

#define SYSIDMAGIC	0x88aabbcc
#define SYSIDVERSION	1

afs_int32
ReadSysIdFile(void)
{
    afs_int32 fd, nentries, i;
    struct versionStamp vsn;
    struct afs_stat status;
    afsUUID uuid;

    if ((afs_stat(AFSDIR_SERVER_SYSID_FILEPATH, &status))
	|| (status.st_size <= 0)) {
	ViceLog(0, ("%s: doesn't exist\n", AFSDIR_SERVER_SYSID_FILEPATH));
	return ENOENT;
    }
    if (!(fd = afs_open(AFSDIR_SERVER_SYSID_FILEPATH, O_RDONLY, 0))) {
	ViceLog(0,
		("%s: can't open (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH,
		 errno));
	return EIO;
    }
    if (read(fd, (char *)&vsn, sizeof(vsn)) != sizeof(vsn)) {
	ViceLog(0,
		("%s: Read failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH,
		 errno));
	return EIO;
    }
    if (vsn.magic != SYSIDMAGIC) {
	ViceLog(0,
		("%s: wrong magic %x (we support %x)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, vsn.magic, SYSIDMAGIC));
	return EIO;
    }
    if (vsn.version != SYSIDVERSION) {
	ViceLog(0,
		("%s: wrong version %d (we support %d)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, vsn.version, SYSIDVERSION));
	return EIO;
    }
    if (read(fd, (char *)&uuid, sizeof(struct afsUUID))
	    != sizeof(struct afsUUID)) {
	ViceLog(0,
		("%s: read of uuid failed (%d)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    afs_ntohuuid(&uuid);
    FS_HostUUID = uuid;
    if (read(fd, (char *)&nentries, sizeof(afs_int32)) != sizeof(afs_int32)) {
	ViceLog(0,
		("%s: Read of entries failed (%d)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    if (nentries <= 0 || nentries > ADDRSPERSITE) {
	ViceLog(0,
		("%s: invalid num of interfaces: %d\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, nentries));
	return EIO;
    }
    if (FS_HostAddr_cnt == 0) {
	FS_HostAddr_cnt = nentries;
	for (i = 0; i < nentries; i++) {
	    if (read(fd, (char *)&FS_HostAddrs[i], sizeof(afs_int32)) !=
		sizeof(afs_int32)) {
		ViceLog(0,
			("%s: Read of addresses failed (%d)\n",
			 AFSDIR_SERVER_SYSID_FILEPATH, errno));
		FS_HostAddr_cnt = 0;	/* reset it */
		return EIO;
	    }
	}
    } else {
	ViceLog(1,
		("%s: address list ignored (NetInfo/NetRestrict override)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH));
    }
    close(fd);
    return 0;
}

afs_int32
WriteSysIdFile(void)
{
    afs_int32 fd, i;
    struct versionStamp vsn;
    struct afs_stat status;
    afsUUID uuid;

    if (!afs_stat(AFSDIR_SERVER_SYSID_FILEPATH, &status)) {
	/*
	 * File exists; keep the old one around
	 */
	rk_rename(AFSDIR_SERVER_SYSID_FILEPATH,
		  AFSDIR_SERVER_OLDSYSID_FILEPATH);
    }
    fd = afs_open(AFSDIR_SERVER_SYSID_FILEPATH, O_WRONLY | O_TRUNC | O_CREAT,
		  0666);
    if (fd < 1) {
	ViceLog(0,
		("%s: can't create (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH,
		 errno));
	return EIO;
    }
    vsn.magic = SYSIDMAGIC;
    vsn.version = 1;
    if ((i = write(fd, (char *)&vsn, sizeof(vsn))) != sizeof(vsn)) {
	ViceLog(0,
		("%s: write failed (%d)\n", AFSDIR_SERVER_SYSID_FILEPATH,
		 errno));
	return EIO;
    }
    uuid = FS_HostUUID;
    afs_htonuuid(&uuid);
    if ((i =
	 write(fd, (char *)&uuid,
	       sizeof(struct afsUUID))) != sizeof(struct afsUUID)) {
	ViceLog(0,
		("%s: write of uuid failed (%d)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    if ((i =
	 write(fd, (char *)&FS_HostAddr_cnt,
	       sizeof(afs_int32))) != sizeof(afs_int32)) {
	ViceLog(0,
		("%s: write of # of entries failed (%d)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    for (i = 0; i < FS_HostAddr_cnt; i++) {
	if (write(fd, (char *)&FS_HostAddrs[i], sizeof(afs_int32)) !=
	    sizeof(afs_int32)) {
	    ViceLog(0,
		    ("%s: write of addresses failed (%d)\n",
		     AFSDIR_SERVER_SYSID_FILEPATH, errno));
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
static afs_int32
Do_VLRegisterRPC(void)
{
    int code;
    bulkaddrs addrs;
    afs_uint32 FS_HostAddrs_HBO[ADDRSPERSITE];
    int i = 0;

    for (i = 0; i < FS_HostAddr_cnt; i++)
	FS_HostAddrs_HBO[i] = ntohl(FS_HostAddrs[i]);
    addrs.bulkaddrs_len = FS_HostAddr_cnt;
    addrs.bulkaddrs_val = (afs_uint32 *) FS_HostAddrs_HBO;
    code = ubik_VL_RegisterAddrs(cstruct, 0, &FS_HostUUID, 0, &addrs);
    if (code) {
	if (code == VL_MULTIPADDR) {
	    ViceLog(0,
		    ("VL_RegisterAddrs rpc failed; The IP address exists on a different server; repair it\n"));
	    ViceLog(0,
		    ("VL_RegisterAddrs rpc failed; See VLLog for details\n"));
	    return code;
	} else if (code == RXGEN_OPCODE) {
	    ViceLog(0,
		    ("vlserver doesn't support VL_RegisterAddrs rpc; ignored\n"));
	    FS_registered = 2;	/* So we don't have to retry in the gc daemon */
	} else {
	    ViceLog(0,
		    ("VL_RegisterAddrs rpc failed; will retry periodically (code=%d, err=%d)\n",
		     code, errno));
	    FS_registered = 1;	/* Retry in the gc daemon */
	}
    } else {
	FS_registered = 2;	/* So we don't have to retry in the gc daemon */
	WriteSysIdFile();
    }

    return 0;
}

afs_int32
SetupVL(void)
{
    afs_int32 code;

    if (AFSDIR_SERVER_NETRESTRICT_FILEPATH || AFSDIR_SERVER_NETINFO_FILEPATH) {
	/*
	 * Find addresses we are supposed to register as per the netrestrict
	 * and netinfo files (/usr/afs/local/NetInfo and
	 * /usr/afs/local/NetRestict)
	 */
	char reason[1024];
	afs_int32 code;

	code = afsconf_ParseNetFiles(FS_HostAddrs, NULL, NULL,
				     ADDRSPERSITE, reason,
				     AFSDIR_SERVER_NETINFO_FILEPATH,
				     AFSDIR_SERVER_NETRESTRICT_FILEPATH);
	if (code < 0) {
	    ViceLog(0, ("Can't register any valid addresses: %s\n", reason));
	    exit(1);
	}
	FS_HostAddr_cnt = (afs_uint32) code;
    } else
    {
	FS_HostAddr_cnt = rx_getAllAddr(FS_HostAddrs, ADDRSPERSITE);
    }

    if (FS_HostAddr_cnt == 1 && rxBind == 1)
	code = FS_HostAddrs[0];
    else
	code = htonl(INADDR_ANY);
    return code;
}

afs_int32
InitVL(struct afsconf_dir *dir)
{
    afs_int32 code;

    /*
     * If this fails, it's because something major is wrong, and is not
     * likely to be time dependent.
     */
    code = vl_Initialize(dir);
    if (code != 0) {
	ViceLog(0,
		("Couldn't initialize volume location library; code=%d.\n", code));
	return code;
    }

    /* Read or create the sysid file and register the fileserver's
     * IP addresses with the vlserver.
     */
    code = ReadSysIdFile();
    if (code) {
	/* Need to create the file */
	ViceLog(0, ("Creating new SysID file\n"));
	if ((code = afs_uuid_create(&FS_HostUUID))) {
	    ViceLog(0, ("Failed to create new uuid: %d\n", code));
	    exit(1);
	}
    }
    /* A good sysid file exists; inform the vlserver. If any conflicts,
     * we always use the latest interface available as the real truth.
     */

    code = Do_VLRegisterRPC();
    return code;
}

int
main(int argc, char *argv[])
{
    afs_int32 code;
    char tbuffer[32];
    struct rx_securityClass **securityClasses;
    afs_int32 numClasses;
    struct rx_service *tservice;
    pthread_t serverPid;
    pthread_attr_t tattr;
    struct hostent *he;
    int minVnodesRequired;	/* min size of vnode cache */
#ifndef AFS_NT40_ENV
    struct rlimit rlim;		/* max number of open file descriptors */
#endif
    int curLimit;
    time_t t;
    struct tm tm;
    char hoststr[16];
    afs_uint32 rx_bindhost;
    VolumePackageOptions opts;
    struct afsconf_bsso_info bsso;

#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    osi_audit_init();

    memset(&bsso, 0, sizeof(bsso));

    /* Initialize dirpaths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }
    /* set ihandle package defaults prior to parsing args */
    ih_PkgDefaults();

    /* check for the parameter file */
    CheckParms();

    FS_configPath = strdup(AFSDIR_SERVER_ETC_DIRPATH);
    memset(&logopts, 0, sizeof(logopts));

    if (ParseArgs(argc, argv)) {
	exit(-1);
    }
    opr_mutex_init(&fileproc_glock_mutex);

#ifdef AFS_SGI_VNODE_GLUE
    if (afs_init_kernel_config(-1) < 0) {
	printf
	    ("Can't determine NUMA configuration, not starting fileserver.\n");
	exit(1);
    }
#endif

    confDir = afsconf_Open(FS_configPath);
    if (!confDir) {
	fprintf(stderr, "Unable to open config directory %s\n",
		FS_configPath);
	exit(-1);
    }

    /* initialize audit user check */
    osi_audit_set_user_check(confDir, fs_IsLocalRealmMatch);

    OpenLog(&logopts);

    LogCommandLine(argc, argv, "starting", "", "File server", FSLog);

    /* initialize the pthread soft signal handler thread */
    opr_softsig_Init();
    SetupLogSoftSignals();
    opr_softsig_Register(AFS_SIG_CHECK, CheckSignal_Signal);
#ifndef AFS_NT40_ENV
    opr_softsig_Register(SIGTERM, CheckDescriptors_Signal);
#endif

    /* finish audit interface initalization */
    osi_audit_open();

#if defined(AFS_SGI_ENV)
    /* give this guy a non-degrading priority so help busy servers */
    schedctl(NDPRI, 0, NDPNORMMAX);
    if (SawLock)
	plock(PROCLOCK);
#elif !defined(AFS_NT40_ENV)
    if (nice(-5) < 0) {
	/* don't care */
    }
#endif
    DInit(buffs);
#ifdef AFS_DEMAND_ATTACH_FS
    FS_STATE_INIT;
#endif

#ifdef AFS_NT40_ENV
    if (afs_winsockInit() < 0) {
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
    curLimit = OPEN_MAX;	/* for pre AIX 4.2 systems */
#elif defined(AFS_NT40_ENV)
    curLimit = NT_OPEN_MAX;	/* open file descriptor limit on NT */
#else

    curLimit = 0;		/* the number of open file descriptors */
    code = getrlimit(RLIMIT_NOFILE, &rlim);
    if (code == 0) {
	curLimit = rlim.rlim_cur;
	rlim.rlim_cur = rlim.rlim_max;
	code = setrlimit(RLIMIT_NOFILE, &rlim);
	if (code == 0)
	    curLimit = rlim.rlim_max;
    }
    if (code != 0)
	ViceLog(0, ("Failed to increase open file limit, using default\n"));

#endif /* defined(AFS_AIX_ENV) && !defined(AFS_AIX42_ENV) */

    curLimit -= 32;		/* leave a slack of 32 file descriptors */
    if (lwps > curLimit) {
	if (curLimit > 0)
	    lwps = curLimit;
	else if (lwps > 16)
	    lwps = 16;		/* default to a maximum of 16 threads */

        /* tune the ihandle fd cache accordingly */
        if (vol_io_params.fd_max_cachesize < curLimit)
            vol_io_params.fd_max_cachesize = curLimit + 1;

	ViceLog(0,
		("The system supports a max of %d open files and we are starting %d threads (ihandle fd cache is %d)\n",
		 curLimit, lwps, vol_io_params.fd_max_cachesize));
    }

    /* Initialize volume support */
    if (!novbc) {
	V_BreakVolumeCallbacks = BreakVolumeCallBacksLater;
    }

    SetLogThreadNumProgram( rx_GetThreadNum );

    /* initialize libacl routines */
    acl_Initialize(ACL_VERSION);

    /* initialize RX support */
#if !defined(AFS_NT40_ENV) && !defined(AFS_DARWIN160_ENV)
    rxi_syscallp = viced_syscall;
#endif
    rx_extraPackets = rxpackets;
    rx_extraQuota = 4;		/* for outgoing prserver calls from R threads */
    rx_SetBusyThreshold(busy_threshold, VBUSY);
    rx_SetCallAbortThreshold(abort_threshold);
    rx_SetConnAbortThreshold(abort_threshold);
#ifdef AFS_XBSD_ENV
    stackSize = 128 * 1024;
#else
    stackSize = lwps * 4000;
    if (stackSize < 32000)
	stackSize = 32000;
    else if (stackSize > 44000)
	stackSize = 44000;
#endif
#if defined(AFS_HPUX_ENV) || defined(AFS_SUN_ENV) || defined(AFS_SGI51_ENV) || defined(AFS_XBSD_ENV)
    rx_SetStackSize(1, stackSize);
#endif
    if (udpBufSize)
	rx_SetUdpBufSize(udpBufSize);	/* set the UDP buffer size for receive */
    rx_bindhost = SetupVL();

    ViceLog(0, ("File server binding rx to %s:%d\n",
            afs_inet_ntoa_r(rx_bindhost, hoststr), 7000));
    if (rx_InitHost(rx_bindhost, (int)htons(7000)) < 0) {
	ViceLog(0, ("Cannot initialize RX\n"));
	exit(1);
    }
    if (!rxJumbograms) {
	/* Don't send and don't allow 3.4 clients to send jumbograms. */
	rx_SetNoJumbo();
    }
    if (rxMaxMTU != -1) {
	if (rx_SetMaxMTU(rxMaxMTU) != 0) {
	    ViceLog(0, ("rxMaxMTU %d is invalid\n", rxMaxMTU));
	    exit(1);
	}
    }
    rx_GetIFInfo();
    rx_SetRxDeadTime(30);
    afsconf_SetSecurityFlags(confDir, AFSCONF_SECOPTS_ALWAYSENCRYPT);

    bsso.dir = confDir;
    bsso.logger = FSLog;
    afsconf_BuildServerSecurityObjects_int(&bsso, &securityClasses,
					   &numClasses);

    tservice = rx_NewServiceHost(rx_bindhost,  /* port */ 0, /* service id */
				 1,	/*service name */
				 "AFS",
				 securityClasses, numClasses,
				 RXAFS_ExecuteRequest);
    if (!tservice) {
	ViceLog(0,
		("Failed to initialize RX, probably two servers running.\n"));
	exit(-1);
    }
    if (rxkadDisableDotCheck) {
	code = rx_SetSecurityConfiguration(tservice, RXS_CONFIG_FLAGS,
					   (void *)RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
	if (code) {
	    ViceLog(0, ("Failed to allow dotted principals: code %d\n", code));
	    exit(-1);
	}
    }
    rx_SetMinProcs(tservice, 3);
    rx_SetMaxProcs(tservice, lwps);
    rx_SetCheckReach(tservice, 1);

    tservice =
	rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats", securityClasses,
		      numClasses, RXSTATS_ExecuteRequest);
    if (!tservice) {
	ViceLog(0, ("Failed to initialize rpc stat service.\n"));
	exit(-1);
    }
    rx_SetMinProcs(tservice, 2);
    rx_SetMaxProcs(tservice, 4);

    /* Some rx debugging */
    if (rxlog || eventlog) {
	debugFile = afs_fopen("rx_dbg", "w");
	if (rxlog)
	    rx_debugFile = debugFile;
	if (eventlog)
	    rxevent_debugFile = debugFile;
    }

    init_sys_error_to_et();	/* Set up error table translation */
    h_InitHostPackage(host_thread_quota); /* set up local cellname and realmname */
    InitCallBack(numberofcbs);
    ClearXStatValues();

    code = InitVL(confDir);
    if (code && code != VL_MULTIPADDR) {
	ViceLog(0, ("Fatal error in library initialization, exiting!!\n"));
	exit(1);
    }

    code = InitPR();
    if (code && code != -1) {
	ViceLog(0, ("Fatal error in protection initialization, exiting!!\n"));
	exit(1);
    }

    /* allow super users to manage RX statistics */
    rx_SetRxStatUserOk(viced_SuperUser);

    opr_cv_init(&fsync_cond);
    opr_mutex_init(&fsync_glock_mutex);

#if !defined(AFS_DEMAND_ATTACH_FS)
    /*
     * For DAFS, we do not start the Rx server threads until after
     * the volume package is initialized, and fileserver state is
     * restored.  This is necessary in order to keep host and callback
     * package state pristine until we have a chance to restore state.
     *
     * Furthermore, startup latency is much lower with dafs, so this
     * shouldn't pose a serious problem.
     */
    rx_StartServer(0);		/* now start handling requests */
#endif

    /* we ensure that there is enough space in the vnode buffer to satisfy
     ** requests from all concurrent threads.
     ** the maximum number of vnodes used by a single thread at any one time
     ** is three ( "link" uses three vnodes simultaneously, one vLarge and
     ** two vSmall for linking files and two vLarge and one vSmall for linking
     ** files  ) : dhruba
     */
    minVnodesRequired = 2 * lwps + 1;
    if (minVnodesRequired > nSmallVns) {
	nSmallVns = minVnodesRequired;
	ViceLog(0,
		("Overriding -s command line parameter with %d\n",
		 nSmallVns));
    }
    if (minVnodesRequired > large) {
	large = minVnodesRequired;
	ViceLog(0, ("Overriding -l command line parameter with %d\n", large));
    }

    /* We now do this after getting the listener up and running, so that client
     * connections don't timeout (maybe) if a file server is restarted, since it
     * will be available "real soon now".  Worry about whether we can satisfy the
     * calls in the volume package itself.
     */
    VOptDefaults(fileServer, &opts);
    opts.nLargeVnodes = large;
    opts.nSmallVnodes = nSmallVns;
    opts.volcache = volcache;
    opts.unsafe_attach = unsafe_attach;
    if (offline_timeout != -1) {
	opts.interrupt_rxcall = rx_InterruptCall;
	opts.offline_timeout = offline_timeout;
    }
    if (offline_shutdown_timeout == -1) {
	/* default to -offline-timeout, if shutdown-specific timeout is not
	 * specified */
	opts.offline_shutdown_timeout = offline_timeout;
    } else {
	opts.interrupt_rxcall = rx_InterruptCall;
	opts.offline_shutdown_timeout = offline_shutdown_timeout;
    }

    if (VInitVolumePackage2(fileServer, &opts)) {
	ViceLog(0,
		("Shutting down: errors encountered initializing volume package\n"));
	VShutdown();
	exit(1);
    }

    /* Install handler to catch the shutdown signal;
     * bosserver assumes SIGQUIT shutdown
     */
    opr_softsig_Register(SIGQUIT, ShutDown_Signal);

    if (VInitAttachVolumes(fileServer)) {
	ViceLog(0,
		("Shutting down: errors encountered initializing volume package\n"));
	VShutdown();
	exit(1);
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (fs_state.options.fs_state_restore) {
	/*
	 * demand attach fs
	 * restore fileserver state */
	fs_stateRestore();
    }
    rx_StartServer(0);  /* now start handling requests */
#endif /* AFS_DEMAND_ATTACH_FS */

    /*
     * We are done calling fopen/fdopen. It is safe to use a large
     * of the file descriptor cache.
     */
    ih_UseLargeCache();

    ViceLog(5, ("Starting pthreads\n"));
    opr_Verify(pthread_attr_init(&tattr) == 0);
    opr_Verify(pthread_attr_setdetachstate(&tattr,
					   PTHREAD_CREATE_DETACHED) == 0);

    opr_Verify(pthread_create(&serverPid, &tattr, FiveMinuteCheckLWP,
			      &fiveminutes) == 0);
    opr_Verify(pthread_create(&serverPid, &tattr, HostCheckLWP,
			      &fiveminutes) == 0);
    opr_Verify(pthread_create(&serverPid, &tattr, FsyncCheckLWP,
			      &fiveminutes) == 0);

    gettimeofday(&tp, 0);

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
	ViceLog(0, ("Can't find address for FileServer '%s'\n", FS_HostName));
    } else {
	memcpy(&FS_HostAddr_NBO, he->h_addr, 4);
	(void)afs_inet_ntoa_r(FS_HostAddr_NBO, hoststr);
	FS_HostAddr_HBO = ntohl(FS_HostAddr_NBO);
	ViceLog(0,
		("FileServer %s has address %s (0x%x or 0x%x in host byte order)\n",
		 FS_HostName, hoststr, FS_HostAddr_NBO, FS_HostAddr_HBO));
    }

    t = tp.tv_sec;
    strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y",
	     localtime_r(&t, &tm));
    ViceLog(0, ("File Server started %s\n", tbuffer));
    afs_FullPerfStats.det.epoch.tv_sec = StartTime = tp.tv_sec;
    while (1) {
	sleep(1000);		/* long time */
    }
    AFS_UNREACHED(return(0));
}
