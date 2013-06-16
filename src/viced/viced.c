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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef AFS_NT40_ENV
#include <io.h>
#include <windows.h>
#include <WINNT/afsevent.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>		/* sysconf() */

#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#include <sys/resource.h>
#endif /* AFS_NT40_ENV */
#include <afs/stds.h>
#undef SHARED
#include <rx/xdr.h>
#include <afs/nfs.h>
#include <afs/afs_assert.h>
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
#ifndef AFS_NT40_ENV
#include <afs/netutils.h>
#endif
#include "viced_prototypes.h"
#include "viced.h"
#include "host.h"
#ifdef AFS_PTHREAD_ENV
#include <afs/softsig.h>
#endif
#if defined(AFS_SGI_ENV)
#include "sys/schedctl.h"
#include "sys/lock.h"
#endif
#include <rx/rx_globals.h>

#ifdef O_LARGEFILE
#define afs_stat	stat64
#define afs_fstat	fstat64
#define afs_open	open64
#define afs_fopen	fopen64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#define afs_fstat	fstat
#define afs_open	open
#define afs_fopen	fopen
#endif /* !O_LARGEFILE */

extern int etext;

void *ShutDown(void *);
static void ClearXStatValues(void);
static void NewParms(int);
static void PrintCounters(void);
static void ResetCheckDescriptors(void);
static void ResetCheckSignal(void);
static void *CheckSignal(void *);

static afs_int32 Do_VLRegisterRPC(void);

int eventlog = 0, rxlog = 0;
FILE *debugFile;

#ifdef AFS_PTHREAD_ENV
pthread_mutex_t fsync_glock_mutex;
pthread_cond_t fsync_cond;
#else
char fsync_wait[1];
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_NT40_ENV
#define NT_OPEN_MAX    1024	/* This is an arbitrary no. we came up with for
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

extern int LogLevel;
extern int Statistics;

int busyonrst = 1;
int timeout = 30;
int SawSpare;
int SawPctSpare;
int debuglevel = 0;
int printBanner = 0;
int rxJumbograms = 0;		/* default is to not send and receive jumbograms. */
int rxBind = 0;		/* don't bind */
int rxkadDisableDotCheck = 0;      /* disable check for dot in principal name */
int rxMaxMTU = -1;
afs_int32 implicitAdminRights = PRSFS_LOOKUP;	/* The ADMINISTER right is
						 * already implied */
afs_int32 readonlyServer = 0;

int stack = 24;
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

#ifdef AFS_PTHREAD_ENV
pthread_key_t viced_uclient_key;
#endif

/*
 * FileServer's name and IP address, both network byte order and
 * host byte order.
 */
#define ADDRSPERSITE 16		/* Same global is in rx/rx_user.c */

char FS_HostName[128] = "localhost";
afs_uint32 FS_HostAddr_NBO;
afs_uint32 FS_HostAddr_HBO;
afs_uint32 FS_HostAddrs[ADDRSPERSITE], FS_HostAddr_cnt = 0, FS_registered = 0;
/* All addresses in FS_HostAddrs are in NBO */
afsUUID FS_HostUUID;

static void FlagMsg(void);

#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * fileserver mode support
 *
 * during fileserver shutdown, we have to track the graceful shutdown of
 * certain background threads before we are allowed to dump state to
 * disk
 */

#if !defined(PTHREAD_RWLOCK_INITIALIZER) && defined(AFS_DARWIN80_ENV)
#define PTHREAD_RWLOCK_INITIALIZER {0x2DA8B3B4, {0}}
#endif

#ifndef AFS_NT40_ENV
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
#else /* AFS_NT40_ENV */
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

    CV_INIT(&fs_state.worker_done_cv, "worker done", CV_DEFAULT, 0);
    osi_Assert(pthread_rwlock_init(&fs_state.state_lock, NULL) == 0);
}
#endif /* AFS_NT40_ENV */
#endif /* AFS_DEMAND_ATTACH_FS */

/*
 * Home for the performance statistics.
 */

/* DEBUG HACK */
static void *
CheckDescriptors(void *unused)
{
#ifndef AFS_NT40_ENV
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
    ResetCheckDescriptors();
#endif
    return 0;
}				/*CheckDescriptors */


#ifdef AFS_PTHREAD_ENV
void
CheckSignal_Signal(int x)
{
    CheckSignal(NULL);
}

void
ShutDown_Signal(int x)
{
    ShutDown(NULL);
}

void
CheckDescriptors_Signal(int x)
{
    CheckDescriptors(NULL);
}
#else /* AFS_PTHREAD_ENV */
void
CheckSignal_Signal(int x)
{
    IOMGR_SoftSig(CheckSignal, 0);
}

void
ShutDown_Signal(int x)
{
    IOMGR_SoftSig(ShutDown, 0);
}

void
CheckDescriptors_Signal(int x)
{
    IOMGR_SoftSig(CheckDescriptors, 0);
}
#endif /* AFS_PTHREAD_ENV */

/* check whether caller is authorized to perform admin operations */
int
viced_SuperUser(struct rx_call *call)
{
    return afsconf_SuperUser(confDir, call, NULL);
}

static void
ResetCheckSignal(void)
{
    int signo;

#if defined(AFS_HPUX_ENV)
    signo = SIGPOLL;
#else
#if defined(AFS_NT40_ENV)
    signo = SIGUSR2;
#else
    signo = SIGXCPU;
#endif
#endif

#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    softsig_signal(signo, CheckSignal_Signal);
#else
    signal(signo, CheckSignal_Signal);
#endif
}

static void
ResetCheckDescriptors(void)
{
#ifndef AFS_NT40_ENV
#if defined(AFS_PTHREAD_ENV)
    softsig_signal(SIGTERM, CheckDescriptors_Signal);
#else
    (void)signal(SIGTERM, CheckDescriptors_Signal);
#endif
#endif
}

#if defined(AFS_PTHREAD_ENV)
int
threadNum(void)
{
    return (intptr_t)pthread_getspecific(rx_thread_id_key);
}
#endif

#ifndef AFS_NT40_ENV
int
viced_syscall(afs_uint32 a3, afs_uint32 a4, void *a5)
{
    afs_uint32 rcode;
#ifndef AFS_LINUX20_ENV
    void (*old) (int);

    old = (void (*)(int))signal(SIGSYS, SIG_IGN);
#endif
    rcode = syscall(AFS_SYSCALL, 28 /* AFSCALL_CALL */ , a3, a4, a5);
#ifndef AFS_LINUX20_ENV
    signal(SIGSYS, old);
#endif

    return rcode;
}
#endif

#if !defined(AFS_NT40_ENV)
#include "AFS_component_version_number.c"
#endif /* !AFS_NT40_ENV */

#define MAXADMINNAME 64
char adminName[MAXADMINNAME];

static void
CheckAdminName(void)
{
    int fd = 0;
    struct afs_stat status;

    if ((afs_stat("/AdminName", &status)) ||	/* if file does not exist */
	(status.st_size <= 0) ||	/* or it is too short */
	(status.st_size >= (MAXADMINNAME)) ||	/* or it is too long */
	!(fd = afs_open("/AdminName", O_RDONLY, 0))) {	/* or the open fails */
	strcpy(adminName, "System:Administrators");	/* use the default name */
    } else {
	(void)read(fd, adminName, status.st_size);	/* use name from the file */
    }
    if (fd)
	close(fd);		/* close fd if it was opened */

}				/*CheckAdminName */


static void
setThreadId(char *s)
{
#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    /* set our 'thread-id' so that the host hold table works */
    MUTEX_ENTER(&rx_stats_mutex);	/* protects rxi_pthread_hinum */
    ++rxi_pthread_hinum;
    pthread_setspecific(rx_thread_id_key, (void *)(intptr_t)rxi_pthread_hinum);
    MUTEX_EXIT(&rx_stats_mutex);
    ViceLog(0,
	    ("Set thread id %p for '%s'\n",
	     pthread_getspecific(rx_thread_id_key), s));
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

#ifdef AFS_PTHREAD_ENV
	sleep(fiveminutes);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(fiveminutes);
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
	if (fs_state.mode != FS_MODE_NORMAL) {
	    break;
	}
	fs_state.FiveMinuteLWP_tranquil = 0;
	FS_STATE_UNLOCK;
#endif

	/* close the log so it can be removed */
	ReOpenLog(AFSDIR_SERVER_FILELOG_FILEPATH);	/* don't trunc, just append */
	ViceLog(2, ("Cleaning up timed out callbacks\n"));
	if (CleanupTimedOutCallBacks())
	    ViceLog(5, ("Timed out callbacks deleted\n"));
	ViceLog(2, ("Set disk usage statistics\n"));
	VSetDiskUsage();
	if (FS_registered == 1)
	    Do_VLRegisterRPC();
	/* Force wakeup in case we missed something; pthreads does timedwait */
#ifndef AFS_PTHREAD_ENV
	LWP_NoYieldSignal(fsync_wait);
#endif
	if (printBanner && (++msg & 1)) {	/* Every 10 minutes */
	    time_t now = FT_ApproxTime();
	    ViceLog(2,
		    ("File server is running at %s\n",
		     afs_ctime(&now, tbuffer, sizeof(tbuffer))));
	}
#ifdef AFS_DEMAND_ATTACH_FS
	FS_STATE_WRLOCK;
#endif
    }
#ifdef AFS_DEMAND_ATTACH_FS
    fs_state.FiveMinuteLWP_tranquil = 1;
    FS_LOCK;
    CV_BROADCAST(&fs_state.worker_done_cv);
    FS_UNLOCK;
    FS_STATE_UNLOCK;
#endif
    return NULL;
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

#ifdef AFS_PTHREAD_ENV
	sleep(fiveminutes);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(fiveminutes);
#endif /* AFS_PTHREAD_ENV */

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
    CV_BROADCAST(&fs_state.worker_done_cv);
    FS_UNLOCK;
    FS_STATE_UNLOCK;
#endif
    return NULL;
}				/*HostCheckLWP */

/* This LWP does fsync checks every 5 minutes:  it should not be used for
 * other 5 minute activities because it may be delayed by timeouts when
 * it probes the workstations
 */
static void *
FsyncCheckLWP(void *unused)
{
    afs_int32 code;
#ifdef AFS_PTHREAD_ENV
    struct timespec fsync_next;
#endif
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
#ifdef AFS_PTHREAD_ENV
	/* rounding is fine */
	fsync_next.tv_nsec = 0;
	fsync_next.tv_sec = time(0) + fiveminutes;

	code = CV_TIMEDWAIT(&fsync_cond, &fsync_glock_mutex,
			    &fsync_next);
	if (code != 0 && code != ETIMEDOUT)
	    ViceLog(0, ("pthread_cond_timedwait returned %d\n", code));
#else /* AFS_PTHREAD_ENV */
	if ((code = LWP_WaitProcess(fsync_wait)) != LWP_SUCCESS)
	    ViceLog(0, ("LWP_WaitProcess returned %d\n", code));
#endif /* AFS_PTHREAD_ENV */
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
    CV_BROADCAST(&fs_state.worker_done_cv);
    FS_UNLOCK;
    FS_STATE_UNLOCK;
#endif /* AFS_DEMAND_ATTACH_FS */
    return NULL;
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
#if FS_STATS_DETAILED
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
#ifdef AFS_DEMAND_ATTACH_FS
    int stats_flags = 0;
#endif

    FT_GetTimeOfDay(&tpl, 0);
    Statistics = 1;
    ViceLog(0,
	    ("Vice was last started at %s\n",
	     afs_ctime(&StartTime, tbuffer, sizeof(tbuffer))));

#ifdef AFS_DEMAND_ATTACH_FS
    if (LogLevel >= 125) {
	stats_flags = VOL_STATS_PER_CHAIN2;
    } else if (LogLevel >= 25) {
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
#if defined(AFS_NT40_ENV) || defined(AFS_DARWIN_ENV)
    processSize = -1;		/* TODO: */
#else
    processSize = (int)((long)sbrk(0) >> 10);
#endif
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
CheckSignal(void *unused)
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
    ResetCheckSignal();
    return 0;
}				/*CheckSignal */

static void *
ShutdownWatchdogLWP(void *unused)
{
    sleep(panic_timeout);
    ViceLog(0, ("ShutdownWatchdogLWP: Failed to shutdown and panic "
                "within %d seconds; forcing panic\n", panic_timeout));
    osi_Panic("ShutdownWatchdogLWP: Failed to shutdown and panic "
	      "within %d seconds; forcing panic\n", panic_timeout);
    return NULL;
}

void
ShutDownAndCore(int dopanic)
{
    time_t now = time(0);
    char tbuffer[32];

    if (dopanic) {
#ifdef AFS_PTHREAD_ENV
	pthread_t watchdogPid;
	pthread_attr_t tattr;
	osi_Assert(pthread_attr_init(&tattr) == 0);
	osi_Assert(pthread_create(&watchdogPid, &tattr, ShutdownWatchdogLWP, NULL) == 0);
#else
	PROCESS watchdogPid;
	osi_Assert(LWP_CreateProcess
	       (ShutdownWatchdogLWP, stack * 1024, LWP_MAX_PRIORITY - 2,
	        NULL, "ShutdownWatchdog", &watchdogPid) == LWP_SUCCESS);
#endif
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

    ViceLog(0,
	    ("Shutting down file server at %s",
	     afs_ctime(&now, tbuffer, sizeof(tbuffer))));
    if (dopanic)
	ViceLog(0, ("ABNORMAL SHUTDOWN, see core file.\n"));
    DFlush();
    if (!dopanic)
	PrintCounters();

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
		CV_WAIT(&fs_state.worker_done_cv, &fileproc_glock_mutex);
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
    if (dopanic) {
      ViceLog(0,
	      ("File server has terminated abnormally at %s\n",
	       afs_ctime(&now, tbuffer, sizeof(tbuffer))));
    } else {
      ViceLog(0,
	      ("File server has terminated normally at %s\n",
	       afs_ctime(&now, tbuffer, sizeof(tbuffer))));
    }

    if (dopanic) /* XXX pass in file and line? */
	osi_Panic("Panic requested\n");

    exit(0);
}

void *
ShutDown(void *unused)
{				/* backward compatibility */
    ShutDownAndCore(DONTPANIC);
    return 0;
}


static void
FlagMsg(void)
{
    /* default supports help flag */

    fputs("Usage: fileserver ", stdout);
    fputs("[-auditlog <log path>] ", stdout);
    fputs("[-audit-interface <file|sysvmq> (default is file)] ", stdout);
    fputs("[-d <debug level>] ", stdout);
    fputs("[-p <number of processes>] ", stdout);
    fputs("[-spare <number of spare blocks>] ", stdout);
    fputs("[-pctspare <percentage spare>] ", stdout);
    fputs("[-b <buffers>] ", stdout);
    fputs("[-l <large vnodes>] ", stdout);
    fputs("[-s <small vnodes>] ", stdout);
    fputs("[-vc <volume cachesize>] ", stdout);
    fputs("[-w <call back wait interval>] ", stdout);
    fputs("[-cb <number of call backs>] ", stdout);
    fputs("[-banner (print banner every 10 minutes)] ", stdout);
    fputs("[-novbc (whole volume cbs disabled)] ", stdout);
    fputs("[-implicit <admin mode bits: rlidwka>] ", stdout);
    fputs("[-readonly (read-only file server)] ", stdout);
    fputs("[-hr <number of hours between refreshing the host cps>] ", stdout);
    fputs("[-busyat <redirect clients when queue > n>] ", stdout);
    fputs("[-nobusy <no VBUSY before a volume is attached>] ", stdout);
    fputs("[-rxpck <number of rx extra packets>] ", stdout);
    fputs("[-rxdbg (enable rx debugging)] ", stdout);
    fputs("[-rxdbge (enable rxevent debugging)] ", stdout);
    fputs("[-rxmaxmtu <bytes>] ", stdout);
    fputs("[-rxbind (bind the Rx socket to one address)] ", stdout);
    fputs("[-allow-dotted-principals (disable the rxkad principal name dot check)] ", stdout);
    fputs("[-vhandle-setaside (fds reserved for non-cache io [default 128])] ", stdout);
    fputs("[-vhandle-max-cachesize (max open files [default 128])] ", stdout);
    fputs("[-vhandle-initial-cachesize (fds reserved for cache io [default 128])] ", stdout);
#ifdef AFS_DEMAND_ATTACH_FS
    fputs("[-fs-state-dont-save (disable state save during shutdown)] ", stdout);
    fputs("[-fs-state-dont-restore (disable state restore during startup)] ", stdout);
    fputs("[-fs-state-verify <none|save|restore|both> (default is both)] ", stdout);
    fputs("[-vattachpar <max number of volume attach/shutdown threads> (default is 1)] ", stdout);
    fputs("[-vhashsize <log(2) of number of volume hash buckets> (default is 8)] ", stdout);
    fputs("[-vlrudisable (disable VLRU functionality)] ", stdout);
    fputs("[-vlruthresh <minutes before unused volumes become eligible for soft detach> (default is 2 hours)] ", stdout);
    fputs("[-vlruinterval <seconds between VLRU scans> (default is 2 minutes)] ", stdout);
    fputs("[-vlrumax <max volumes to soft detach in one VLRU scan> (default is 8)] ", stdout);
    fputs("[-unsafe-nosalvage (bypass volume inUse safety check on attach, bypassing salvage)] ", stdout);
#elif AFS_PTHREAD_ENV
    fputs("[-vattachpar <number of volume attach threads> (default is 1)] ", stdout);
#endif
#ifdef	AFS_AIX32_ENV
    fputs("[-m <min percentage spare in partition>] ", stdout);
#endif
#if defined(AFS_SGI_ENV)
    fputs("[-lock (keep fileserver from swapping)] ", stdout);
#endif
    fputs("[-L (large server conf)] ", stdout);
    fputs("[-S (small server conf)] ", stdout);
    fputs("[-k <stack size>] ", stdout);
    fputs("[-realm <Kerberos realm name>] ", stdout);
    fputs("[-udpsize <size of socket buffer in bytes>] ", stdout);
    fputs("[-sendsize <size of send buffer in bytes>] ", stdout);
    fputs("[-abortthreshold <abort threshold>] ", stdout);
    fputs("[-nojumbo (disable jumbogram network packets - deprecated)] ", stdout);
    fputs("[-jumbo (enable jumbogram network packets)] ", stdout);
    fputs("[-sync <always | delayed | onclose | never>]", stdout);
    fputs("[-offline-timeout <client RX timeout for offlining volumes>]", stdout);
    fputs("[-offline-shutdown-timeout <RX timeout for offlining volumes during shutdown>]", stdout);
/*   fputs("[-enable_peer_stats] ", stdout); */
/*   fputs("[-enable_process_stats] ", stdout); */
    fputs("[-help]\n", stdout);
/*
    ViceLog(0, ("%s", buffer));
*/

    fflush(stdout);

}				/*FlagMsg */


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
	return (int)ans;
#endif
#endif /* defined(AFS_PTHREAD_ENV) */
    return MAX_FILESERVER_THREAD;
}

/* from ihandle.c */
extern ih_init_params vol_io_params;

static int
ParseArgs(int argc, char *argv[])
{
    int SawL = 0, SawS = 0, SawVC = 0;
    int Sawrxpck = 0, Sawsmall = 0, Sawlarge = 0, Sawcbs = 0, Sawlwps =
	0, Sawbufs = 0;
    int Sawbusy = 0;
    int i;
    int bufSize = 0;		/* temp variable to read in udp socket buf size */
    int lwps_max;
    char *auditFileName = NULL;

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-d")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -d\n");
		return -1;
	    }
	    debuglevel = atoi(argv[++i]);
	    LogLevel = debuglevel;
	} else if (!strcmp(argv[i], "-banner")) {
	    printBanner = 1;
	} else if (!strcmp(argv[i], "-implicit")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -implicit\n");
		return -1;
	    }
	    implicitAdminRights = ParseRights(argv[++i]);
	    if (implicitAdminRights < 0)
		return implicitAdminRights;
	} else if (!strcmp(argv[i], "-readonly")) {
	    readonlyServer = 1;
	} else if (!strcmp(argv[i], "-L")) {
	    SawL = 1;
	} else if (!strcmp(argv[i], "-S")) {
	    SawS = 1;
	} else if (!strcmp(argv[i], "-p")) {
	    Sawlwps = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -p\n");
		return -1;
	    }
	    lwps = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-b")) {
	    Sawbufs = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -b\n");
		return -1;
	    }
	    buffs = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-l")) {
	    Sawlarge = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -l\n");
		return -1;
	    }
	    large = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-vc")) {
	    SawVC = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -vc\n");
		return -1;
	    }
	    volcache = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-novbc")) {
	    novbc = 1;
	} else if (!strcmp(argv[i], "-rxpck")) {
	    Sawrxpck = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxpck\n");
		return -1;
	    }
	    rxpackets = atoi(argv[++i]);
#ifdef AFS_PTHREAD_ENV
	} else if (!strcmp(argv[i], "-vattachpar")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for %s\n", argv[i]);
		return -1;
	    }
	    vol_attach_threads = atoi(argv[++i]);
#endif /* AFS_PTHREAD_ENV */
        } else if (!strcmp(argv[i], "-vhandle-setaside")) {
            if ((i + 1) >= argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i]);
                return -1;
	    }
            vol_io_params.fd_handle_setaside = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-vhandle-max-cachesize")) {
            if ((i + 1) >= argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i]);
                return -1;
            }
            vol_io_params.fd_max_cachesize = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-vhandle-initial-cachesize")) {
            if ((i + 1) >= argc) {
                fprintf(stderr, "missing argument for %s\n", argv[i]);
                return -1;
            }
            vol_io_params.fd_initial_cachesize = atoi(argv[++i]);
#ifdef AFS_DEMAND_ATTACH_FS
	} else if (!strcmp(argv[i], "-fs-state-dont-save")) {
	    fs_state.options.fs_state_save = 0;
	} else if (!strcmp(argv[i], "-fs-state-dont-restore")) {
	    fs_state.options.fs_state_restore = 0;
	} else if (!strcmp(argv[i], "-fs-state-verify")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for %s\n", argv[i]);
		return -1;
	    }
	    i++;
	    if (!strcmp(argv[i], "none")) {
		fs_state.options.fs_state_verify_before_save = 0;
		fs_state.options.fs_state_verify_after_restore = 0;
	    } else if (!strcmp(argv[i], "save")) {
		fs_state.options.fs_state_verify_after_restore = 0;
	    } else if (!strcmp(argv[i], "restore")) {
		fs_state.options.fs_state_verify_before_save = 0;
	    } else if (!strcmp(argv[i], "both")) {
		/* default */
	    } else {
		fprintf(stderr, "invalid argument for %s\n", argv[i-1]);
		return -1;
	    }
	} else if (!strcmp(argv[i], "-vhashsize")) {
	    int hashsize;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for %s\n", argv[i]);
		return -1;
	    }
	    hashsize = atoi(argv[++i]);
	    if (VSetVolHashSize(hashsize)) {
		fprintf(stderr, "specified -vhashsize (%s) is invalid or out "
		                "of range\n", argv[i]);
		return -1;
	    }
	} else if (!strcmp(argv[i], "-vlrudisable")) {
	    VLRU_SetOptions(VLRU_SET_ENABLED, 0);
	} else if (!strcmp(argv[i], "-vlruthresh")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for %s\n", argv[i]);
		return -1;
	    }
	    VLRU_SetOptions(VLRU_SET_THRESH, 60*atoi(argv[++i]));
	} else if (!strcmp(argv[i], "-vlruinterval")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for %s\n", argv[i]);
		return -1;
	    }
	    VLRU_SetOptions(VLRU_SET_INTERVAL, atoi(argv[++i]));
	} else if (!strcmp(argv[i], "-vlrumax")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for %s\n", argv[i]);
		return -1;
	    }
	    VLRU_SetOptions(VLRU_SET_MAX, atoi(argv[++i]));
	} else if (!strcmp(argv[i], "-unsafe-nosalvage")) {
	    unsafe_attach = 1;
#endif /* AFS_DEMAND_ATTACH_FS */
	} else if (!strcmp(argv[i], "-s")) {
	    Sawsmall = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -s\n");
		return -1;
	    }
	    nSmallVns = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-abortthreshold")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -abortthreshold\n");
		return -1;
	    }
	    abort_threshold = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-k")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -k\n");
		return -1;
	    }
	    stack = atoi(argv[++i]);
	}
#if defined(AFS_SGI_ENV)
	else if (!strcmp(argv[i], "-lock")) {
	    SawLock = 1;
	}
#endif
	else if (!strcmp(argv[i], "-spare")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -spare\n");
		return -1;
	    }
	    BlocksSpare = atoi(argv[++i]);
	    SawSpare = 1;
	} else if (!strcmp(argv[i], "-pctspare")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -pctspare\n");
		return -1;
	    }
	    PctSpare = atoi(argv[++i]);
	    BlocksSpare = 0;	/* has non-zero default */
	    SawPctSpare = 1;
	} else if (!strcmp(argv[i], "-w")) {
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -w\n");
		return -1;
	    }
	    fiveminutes = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-hr")) {
	    int hr;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -hr\n");
		return -1;
	    }
	    hr = atoi(argv[++i]);
	    if ((hr < 1) || (hr > 36)) {
		printf
		    ("host acl refresh interval of %d hours is invalid; hours must be between 1 and 36\n\n",
		     hr);
		return -1;
	    }
	    hostaclRefresh = hr * 60 * 60;
	} else if (!strcmp(argv[i], "-rxdbg"))
	    rxlog = 1;
	else if (!strcmp(argv[i], "-rxdbge"))
	    eventlog = 1;
	else if (!strcmp(argv[i], "-cb")) {
	    Sawcbs = 1;
            if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -cb\n");
		return -1;
	    }
	    numberofcbs = atoi(argv[++i]);
	    if ((numberofcbs < 10000) || (numberofcbs > 2147483647)) {
		printf
		    ("number of cbs %d invalid; must be between 10000 and 2147483647\n",
		     numberofcbs);
		return -1;
	    }
	} else if (!strcmp(argv[i], "-busyat")) {
	    Sawbusy = 1;
	    if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -busyat\n");
		return -1;
	    }
	    busy_threshold = atoi(argv[++i]);
	    if (busy_threshold < 10) {
		printf
		    ("Busy threshold %d is too low, will compute default.\n",
		     busy_threshold);
		Sawbusy = 0;
	    }
	} else if (!strcmp(argv[i], "-nobusy"))
	    busyonrst = 0;
#ifdef	AFS_AIX32_ENV
	else if (!strcmp(argv[i], "-m")) {
	    extern int aixlow_water;
	    if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -m\n");
		return -1;
	    }
	    aixlow_water = atoi(argv[++i]);
	    if ((aixlow_water < 0) || (aixlow_water > 30)) {
		printf("space reserved %d% invalid; must be between 0-30%\n",
		       aixlow_water);
		return -1;
	    }
	}
#endif
	else if (!strcmp(argv[i], "-nojumbo")) {
	    rxJumbograms = 0;
	} else if (!strcmp(argv[i], "-jumbo")) {
	    rxJumbograms = 1;
	} else if (!strcmp(argv[i], "-rxbind")) {
	    rxBind = 1;
	} else if (!strcmp(argv[i], "-allow-dotted-principals")) {
	    rxkadDisableDotCheck = 1;
	} else if (!strcmp(argv[i], "-rxmaxmtu")) {
	    if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -rxmaxmtu\n");
		return -1;
	    }
	    rxMaxMTU = atoi(argv[++i]);
	    if ((rxMaxMTU < RX_MIN_PACKET_SIZE) ||
		(rxMaxMTU > RX_MAX_PACKET_DATA_SIZE)) {
		printf("rxMaxMTU %d%% invalid; must be between %d-%" AFS_SIZET_FMT "\n",
		       rxMaxMTU, RX_MIN_PACKET_SIZE,
		       RX_MAX_PACKET_DATA_SIZE);
		return -1;
		}
	} else if (!strcmp(argv[i], "-realm")) {
	    extern char local_realms[AFS_NUM_LREALMS][AFS_REALM_SZ];
	    extern int  num_lrealms;
	    if ((i + 1) >= argc) {
		fprintf(stderr, "missing argument for -realm\n");
		return -1;
	    }
	    if (strlen(argv[++i]) >= AFS_REALM_SZ) {
		printf
		    ("-realm argument must contain fewer than %d characters.\n",
		     AFS_REALM_SZ);
		return -1;
	    }
	    if (num_lrealms == -1)
		num_lrealms = 0;
	    if (num_lrealms >= AFS_NUM_LREALMS) {
		printf
		    ("a maximum of %d -realm arguments can be specified.\n",
		     AFS_NUM_LREALMS);
		return -1;
	    }
	    strncpy(local_realms[num_lrealms++], argv[i], AFS_REALM_SZ);
	} else if (!strcmp(argv[i], "-udpsize")) {
	    if ((i + 1) >= argc) {
		printf("You have to specify -udpsize <integer value>\n");
		return -1;
	    }
	    bufSize = atoi(argv[++i]);
	    if (bufSize < rx_GetMinUdpBufSize())
		printf
		    ("Warning:udpsize %d is less than minimum %d; ignoring\n",
		     bufSize, rx_GetMinUdpBufSize());
	    else
		udpBufSize = bufSize;
	} else if (!strcmp(argv[i], "-sendsize")) {
	    if ((i + 1) >= argc) {
		printf("You have to specify -sendsize <integer value>\n");
		return -1;
	    }
	    bufSize = atoi(argv[++i]);
	    if (bufSize < 16384)
		printf
		    ("Warning:sendsize %d is less than minimum %d; ignoring\n",
		     bufSize, 16384);
	    else
		sendBufSize = bufSize;
	} else if (!strcmp(argv[i], "-enable_peer_stats")) {
	    rx_enablePeerRPCStats();
	} else if (!strcmp(argv[i], "-enable_process_stats")) {
	    rx_enableProcessRPCStats();
	}
	else if (strcmp(argv[i], "-auditlog") == 0) {
	    auditFileName = argv[++i];
	}
	else if (strcmp(argv[i], "-audit-interface") == 0) {
	    char *interface = argv[++i];

	    if (osi_audit_interface(interface)) {
		printf("Invalid audit interface '%s'\n", interface);
		return -1;
	    }
	}
#ifndef AFS_NT40_ENV
	else if (strcmp(argv[i], "-syslog") == 0) {
	    /* set syslog logging flag */
	    serverLogSyslog = 1;
	} else if (strncmp(argv[i], "-syslog=", 8) == 0) {
	    serverLogSyslog = 1;
	    serverLogSyslogFacility = atoi(argv[i] + 8);
	}
#endif
	else if (strcmp(argv[i], "-mrafslogs") == 0) {
	    /* set syslog logging flag */
	    mrafsStyleLogs = 1;
	}
	else if (strcmp(argv[i], "-saneacls") == 0) {
	    saneacls = 1;
	}
	else if (strcmp(argv[i], "-sync") == 0) {
	    if ((i + 1) >= argc) {
		printf("You have to specify -sync <sync behavior>\n");
		return -1;
	    }
	    if (ih_SetSyncBehavior(argv[++i])) {
		printf("Invalid -sync value %s\n", argv[i]);
		return -1;
	    }
	}
	else if (strcmp(argv[i], "-offline-timeout") == 0) {
	    if (i + 1 >= argc) {
		printf("You have to specify -offline-timeout <integer>\n");
		return -1;
	    }
	    offline_timeout = atoi(argv[++i]);
#ifndef AFS_PTHREAD_ENV
	    if (offline_timeout != -1) {
		printf("The only valid -offline-timeout value for the LWP "
		       "fileserver is -1\n");
		return -1;
	    }
#endif /* AFS_PTHREAD_ENV */
	    if (offline_timeout < -1) {
		printf("Invalid -offline-timeout value %s; the only valid "
		       "negative value is -1\n", argv[i]);
		return -1;
	    }
	}
	else if (strcmp(argv[i], "-offline-shutdown-timeout") == 0) {
	    if (i + 1 >= argc) {
		printf("You have to specify -offline-shutdown-timeout "
		       "<integer>\n");
		return -1;
	    }
	    offline_shutdown_timeout = atoi(argv[++i]);
#ifndef AFS_PTHREAD_ENV
	    if (offline_shutdown_timeout != -1) {
		printf("The only valid -offline-shutdown-timeout value for the "
		       "LWP fileserver is -1\n");
		return -1;
	    }
#endif /* AFS_PTHREAD_ENV */
	    if (offline_shutdown_timeout < -1) {
		printf("Invalid -offline-timeout value %s; the only valid "
		       "negative value is -1\n", argv[i]);
		return -1;
	    }
	}
	else {
	    return (-1);
	}
    }
    if (SawS && SawL) {
	printf("Only one of -L, or -S must be specified\n");
	return -1;
    }
    if (SawS) {
	if (!Sawrxpck)
	    rxpackets = 100;
	if (!Sawsmall)
	    nSmallVns = 200;
	if (!Sawlarge)
	    large = 200;
	if (!Sawcbs)
	    numberofcbs = 20000;
	if (!Sawlwps)
	    lwps = 6;
	if (!Sawbufs)
	    buffs = 70;
	if (!SawVC)
	    volcache = 200;
    }
    if (SawL) {
	if (!Sawrxpck)
	    rxpackets = 200;
	if (!Sawsmall)
	    nSmallVns = 600;
	if (!Sawlarge)
	    large = 600;
	if (!Sawcbs)
	    numberofcbs = 64000;
	if (!Sawlwps)
	    lwps = 128;
	if (!Sawbufs)
	    buffs = 120;
	if (!SawVC)
	    volcache = 600;
    }
    if (!Sawbusy)
	busy_threshold = 3 * rxpackets / 2;
    if (auditFileName)
	osi_audit_file(auditFileName);

    lwps_max = max_fileserver_thread() - FILESERVER_HELPER_THREADS;
    if (lwps > lwps_max)
	lwps = lwps_max;
    else if (lwps < 6)
	lwps = 6;

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


#define MAXPARMS 15

static void
NewParms(int initializing)
{
    static struct afs_stat sbuf;
    int i, fd;
    char *parms;
    char *argv[MAXPARMS];
    int argc;

    if (!(afs_stat("/vice/file/parms", &sbuf))) {
	ViceLog(0, ("/vice/file/parms is deprecated, and will be removed in a future release!"));
	parms = (char *)malloc(sbuf.st_size);
	if (!parms)
	    return;
	fd = afs_open("parms", O_RDONLY, 0666);
	if (fd <= 0) {
	    ViceLog(0, ("Open for parms failed with errno = %d\n", errno));
	    return;
	}

	i = read(fd, parms, sbuf.st_size);
	close(fd);
	if (i != sbuf.st_size) {
	    if (i < 0) {
		ViceLog(0, ("Read on parms failed with errno = %d\n", errno));
	    } else {
		ViceLog(0,
			("Read on parms failed; expected %ld bytes but read %d\n",
			 (long) sbuf.st_size, i));
	    }
	    free(parms);
	    return;
	}

	for (i = 0; i < MAXPARMS; argv[i++] = 0);

	for (argc = i = 0; i < sbuf.st_size; i++) {
	    if ((*(parms + i) != ' ') && (*(parms + i) != '\n')) {
		if (argv[argc] == 0)
		    argv[argc] = (parms + i);
	    } else {
		*(parms + i) = '\0';
		if (argv[argc] != 0) {
		    if (++argc == MAXPARMS)
			break;
		}
		while ((*(parms + i + 1) == ' ')
		       || (*(parms + i + 1) == '\n'))
		    i++;
	    }
	}
	if (ParseArgs(argc, argv) == 0) {
	    ViceLog(0, ("Change parameters to:"));
	} else {
	    ViceLog(0, ("Invalid parameter in:"));
	}
	for (i = 0; i < argc; i++) {
	    ViceLog(0, (" %s", argv[i]));
	}
	ViceLog(0, ("\n"));
	free(parms);
    } else if (!initializing)
	ViceLog(0,
		("Received request to change parms but no parms file exists\n"));

}				/*NewParms */


/* Miscellaneous routines */
void
Die(char *msg)
{
    ViceLog(0, ("%s\n", msg));
    osi_Panic("%s\n", msg);

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

#ifdef AFS_PTHREAD_ENV
    osi_Assert(pthread_key_create(&viced_uclient_key, NULL) == 0);
#endif

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
#ifdef AFS_PTHREAD_ENV
	sleep(30);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(30);
#endif /* AFS_PTHREAD_ENV */
    }
}				/*InitPR */

struct rx_connection *serverconns[MAXSERVERS];
struct ubik_client *cstruct;

afs_int32
vl_Initialize(const char *confDir)
{
    afs_int32 code, i;
    afs_int32 scIndex = RX_SECIDX_NULL;
    struct afsconf_dir *tdir;
    struct rx_securityClass *sc;
    struct afsconf_cell info;

    tdir = afsconf_Open(confDir);
    if (!tdir) {
	ViceLog(0,
		("Could not open configuration directory (%s).\n", confDir));
	exit(1);
    }
    code = afsconf_ClientAuth(tdir, &sc, &scIndex);
    if (code) {
	ViceLog(0, ("Could not get security object for localAuth\n"));
	exit(1);
    }
    code = afsconf_GetCellInfo(tdir, NULL, AFSCONF_VLDBSERVICE, &info);
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
    afsconf_Close(tdir);
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
    if ((i = read(fd, (char *)&vsn, sizeof(vsn))) != sizeof(vsn)) {
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
    if ((i =
	 read(fd, (char *)&uuid,
	      sizeof(struct afsUUID))) != sizeof(struct afsUUID)) {
	ViceLog(0,
		("%s: read of uuid failed (%d)\n",
		 AFSDIR_SERVER_SYSID_FILEPATH, errno));
	return EIO;
    }
    afs_ntohuuid(&uuid);
    FS_HostUUID = uuid;
    if ((i =
	 read(fd, (char *)&nentries,
	      sizeof(afs_int32))) != sizeof(afs_int32)) {
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
	renamefile(AFSDIR_SERVER_SYSID_FILEPATH,
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
	afs_int32 code = parseNetFiles(FS_HostAddrs, NULL, NULL,
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
InitVL(void)
{
    afs_int32 code;

    /*
     * If this fails, it's because something major is wrong, and is not
     * likely to be time dependent.
     */
    code = vl_Initialize(AFSDIR_SERVER_ETC_DIRPATH);
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
#ifdef AFS_PTHREAD_ENV
    pthread_t serverPid;
    pthread_attr_t tattr;
#else /* AFS_PTHREAD_ENV */
    PROCESS parentPid, serverPid;
#endif /* AFS_PTHREAD_ENV */
    struct hostent *he;
    int minVnodesRequired;	/* min size of vnode cache */
#ifndef AFS_NT40_ENV
    struct rlimit rlim;		/* max number of open file descriptors */
#endif
    int curLimit;
    time_t t;
    afs_uint32 rx_bindhost;
    VolumePackageOptions opts;

#ifdef	AFS_AIX32_ENV
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGABRT, &nsa, NULL);
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    osi_audit_init();

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

    if (ParseArgs(argc, argv)) {
	FlagMsg();
	exit(-1);
    }
#ifdef AFS_PTHREAD_ENV
    MUTEX_INIT(&fileproc_glock_mutex, "fileproc", MUTEX_DEFAULT, 0);
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_SGI_VNODE_GLUE
    if (afs_init_kernel_config(-1) < 0) {
	printf
	    ("Can't determine NUMA configuration, not starting fileserver.\n");
	exit(1);
    }
#endif
    confDir = afsconf_Open(AFSDIR_SERVER_ETC_DIRPATH);
    if (!confDir) {
	fprintf(stderr, "Unable to open config directory %s\n",
		AFSDIR_SERVER_ETC_DIRPATH);
	exit(-1);
    }

    NewParms(1);

    /* Open FileLog on stdout, stderr, fd 1 and fd2 (for perror), sigh. */
#ifndef AFS_NT40_ENV
    serverLogSyslogTag = "fileserver";
#endif
    OpenLog(AFSDIR_SERVER_FILELOG_FILEPATH);
    SetupLogSignals();

    if (SawSpare && SawPctSpare) {
	ViceLog(0, ("Both -spare and -pctspare specified, exiting.\n"));
	exit(-1);
    }
    LogCommandLine(argc, argv, "starting", "", "File server", FSLog);
    if (afsconf_GetLatestKey(confDir, NULL, NULL) == 0) {
	LogDesWarning();
    }

#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    /* initialize the pthread soft signal handler thread */
    softsig_init();
#endif

    /* install signal handlers for controlling the fileserver process */
    ResetCheckSignal();		/* set CheckSignal_Signal() sig handler */
    ResetCheckDescriptors();	/* set CheckDescriptors_Signal() sig handler */

#if defined(AFS_SGI_ENV)
    /* give this guy a non-degrading priority so help busy servers */
    schedctl(NDPRI, 0, NDPNORMMAX);
    if (SawLock)
	plock(PROCLOCK);
#else
#ifndef AFS_NT40_ENV
    if (nice(-5) < 0)
	; /* don't care */
#endif
#endif
    osi_Assert(DInit(buffs) == 0);
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
#ifndef AFS_PTHREAD_ENV
    osi_Assert(LWP_InitializeProcessSupport(LWP_MAX_PRIORITY - 2, &parentPid) ==
	   LWP_SUCCESS);
#endif /* !AFS_PTHREAD_ENV */

    /* Initialize volume support */
    if (!novbc) {
	V_BreakVolumeCallbacks = BreakVolumeCallBacksLater;
    }

#ifdef AFS_PTHREAD_ENV
    SetLogThreadNumProgram( threadNum );
#endif

    /* initialize libacl routines */
    acl_Initialize(ACL_VERSION);

    /* initialize RX support */
#ifndef AFS_NT40_ENV
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

    if (rx_InitHost(rx_bindhost, (int)htons(7000)) < 0) {
	ViceLog(0, ("Cannot initialize RX\n"));
	exit(1);
    }
    if (!rxJumbograms) {
	/* Don't send and don't allow 3.4 clients to send jumbograms. */
	rx_SetNoJumbo();
    }
    if (rxMaxMTU != -1) {
	rx_SetMaxMTU(rxMaxMTU);
    }
    rx_GetIFInfo();
    rx_SetRxDeadTime(30);
    afsconf_BuildServerSecurityObjects(confDir, AFSCONF_SEC_OBJS_RXKAD_CRYPT,
				       &securityClasses, &numClasses);

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
        rx_SetSecurityConfiguration(tservice, RXS_CONFIG_FLAGS,
                                    (void *)RXS_CONFIG_FLAGS_DISABLE_DOTCHECK);
    }
    rx_SetMinProcs(tservice, 3);
    rx_SetMaxProcs(tservice, lwps);
    rx_SetCheckReach(tservice, 1);
    rx_SetServerIdleDeadErr(tservice, VNOSERVICE);

    tservice =
	rx_NewService(0, RX_STATS_SERVICE_ID, "rpcstats", securityClasses,
		      numClasses, RXSTATS_ExecuteRequest);
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

    code = InitVL();
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

    CV_INIT(&fsync_cond, "fsync", CV_DEFAULT, 0);
    MUTEX_INIT(&fsync_glock_mutex, "fsync", MUTEX_DEFAULT, 0);

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
#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)
    softsig_signal(SIGQUIT, ShutDown_Signal);
#else
    (void)signal(SIGQUIT, ShutDown_Signal);
#endif

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

#ifdef AFS_PTHREAD_ENV
    ViceLog(5, ("Starting pthreads\n"));
    osi_Assert(pthread_attr_init(&tattr) == 0);
    osi_Assert(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED) == 0);

    osi_Assert(pthread_create
	   (&serverPid, &tattr, FiveMinuteCheckLWP,
	    &fiveminutes) == 0);
    osi_Assert(pthread_create
	   (&serverPid, &tattr, HostCheckLWP, &fiveminutes) == 0);
    osi_Assert(pthread_create
	   (&serverPid, &tattr, FsyncCheckLWP, &fiveminutes) == 0);
#else /* AFS_PTHREAD_ENV */
    ViceLog(5, ("Starting LWP\n"));
    osi_Assert(LWP_CreateProcess
	   (FiveMinuteCheckLWP, stack * 1024, LWP_MAX_PRIORITY - 2,
	    (void *)&fiveminutes, "FiveMinuteChecks",
	    &serverPid) == LWP_SUCCESS);

    osi_Assert(LWP_CreateProcess
	   (HostCheckLWP, stack * 1024, LWP_MAX_PRIORITY - 2,
	    (void *)&fiveminutes, "HostCheck", &serverPid) == LWP_SUCCESS);
    osi_Assert(LWP_CreateProcess
	   (FsyncCheckLWP, stack * 1024, LWP_MAX_PRIORITY - 2,
	    (void *)&fiveminutes, "FsyncCheck", &serverPid) == LWP_SUCCESS);
#endif /* AFS_PTHREAD_ENV */

    FT_GetTimeOfDay(&tp, 0);

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
	char hoststr[16];
	memcpy(&FS_HostAddr_NBO, he->h_addr, 4);
	(void)afs_inet_ntoa_r(FS_HostAddr_NBO, hoststr);
	FS_HostAddr_HBO = ntohl(FS_HostAddr_NBO);
	ViceLog(0,
		("FileServer %s has address %s (0x%x or 0x%x in host byte order)\n",
		 FS_HostName, hoststr, FS_HostAddr_NBO, FS_HostAddr_HBO));
    }

    t = tp.tv_sec;
    ViceLog(0,
	    ("File Server started %s",
	     afs_ctime(&t, tbuffer, sizeof(tbuffer))));
#if FS_STATS_DETAILED
    afs_FullPerfStats.det.epoch.tv_sec = StartTime = tp.tv_sec;
#endif
#ifdef AFS_PTHREAD_ENV
    while (1) {
	sleep(1000);		/* long time */
    }
#else /* AFS_PTHREAD_ENV */
    osi_Assert(LWP_WaitProcess(&parentPid) == LWP_SUCCESS);
#endif /* AFS_PTHREAD_ENV */
    return 0;
}
