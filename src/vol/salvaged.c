/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * demand attach fs
 * online salvager daemon
 */

/* Main program file. Define globals. */
#define MAIN 1

#include <afsconfig.h>
#include <afs/param.h>


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#include <WINNT/afsevent.h>
#else
#include <sys/param.h>
#include <sys/file.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#endif
#ifndef WCOREDUMP
#define WCOREDUMP(x)	((x) & 0200)
#endif
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <afs/afs_assert.h>
#if !defined(AFS_SGI_ENV) && !defined(AFS_NT40_ENV)
#if defined(AFS_VFSINCL_ENV)
#include <sys/vnode.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_inode.h>
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#else
#include <ufs/inode.h>
#endif
#endif
#else /* AFS_VFSINCL_ENV */
#ifdef	AFS_OSF_ENV
#include <ufs/inode.h>
#else /* AFS_OSF_ENV */
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_XBSD_ENV) && !defined(AFS_DARWIN_ENV)
#include <sys/inode.h>
#endif
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_SGI_ENV */
#ifdef	AFS_AIX_ENV
#include <sys/vfs.h>
#include <sys/lockf.h>
#else
#ifdef	AFS_HPUX_ENV
#include <unistd.h>
#include <checklist.h>
#else
#if defined(AFS_SGI_ENV)
#include <unistd.h>
#include <fcntl.h>
#include <mntent.h>
#else
#if	defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#ifdef	  AFS_SUN5_ENV
#include <unistd.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#else
#include <mntent.h>
#endif
#else
#endif /* AFS_SGI_ENV */
#endif /* AFS_HPUX_ENV */
#endif
#endif
#include <fcntl.h>
#ifndef AFS_NT40_ENV
#include <afs/osi_inode.h>
#endif
#include <afs/cmd.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <afs/procmgmt.h>	/* signal(), kill(), wait(), etc. */
#include <afs/dir.h>
#ifndef AFS_NT40_ENV
#include <syslog.h>
#endif

#include "nfs.h"
#include "lwp.h"
#include "lock.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "daemon_com.h"
#include "fssync.h"
#include "salvsync.h"
#include "viceinode.h"
#include "salvage.h"
#include "vol-salvage.h"
#include "common.h"
#ifdef AFS_NT40_ENV
#include <pthread.h>
#endif


#if !defined(AFS_DEMAND_ATTACH_FS)
#error "online salvager only supported for demand attach fileserver"
#endif /* AFS_DEMAND_ATTACH_FS */

#if defined(AFS_NT40_ENV)
#error "online salvager not supported on NT"
#endif /* AFS_NT40_ENV */

/*@+fcnmacros +macrofcndecl@*/
#ifdef O_LARGEFILE
#define afs_fopen	fopen64
#else /* !O_LARGEFILE */
#define afs_fopen	fopen
#endif /* !O_LARGEFILE */
/*@=fcnmacros =macrofcndecl@*/



static volatile int current_workers = 0;
static volatile struct rx_queue pending_q;
static pthread_mutex_t worker_lock;
static pthread_cond_t worker_cv;

static void * SalvageChildReaperThread(void *);
static int DoSalvageVolume(struct SalvageQueueNode * node, int slot);

static void SalvageServer(int argc, char **argv);
static void SalvageClient(VolumeId vid, char * pname);

static int Reap_Child(char * prog, int * pid, int * status);

static void * SalvageLogCleanupThread(void *);
static int SalvageLogCleanup(int pid);

static void * SalvageLogScanningThread(void *);
static void ScanLogs(struct rx_queue *log_watch_queue);

struct cmdline_rock {
    int argc;
    char **argv;
};

struct log_cleanup_node {
    struct rx_queue q;
    int pid;
};

struct {
    struct rx_queue queue_head;
    pthread_cond_t queue_change_cv;
} log_cleanup_queue;


#define DEFAULT_PARALLELISM 4 /* allow 4 parallel salvage workers by default */

static int
handleit(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *ti;
    char pname[100], *temp;
    afs_int32 seenpart = 0, seenvol = 0;
    VolumeId vid = 0;
    struct cmdline_rock *rock = (struct cmdline_rock *)arock;

#ifdef AFS_SGI_VNODE_GLUE
    if (afs_init_kernel_config(-1) < 0) {
	printf
	    ("Can't determine NUMA configuration, not starting salvager.\n");
	exit(1);
    }
#endif

    if (as->parms[2].items)	/* -debug */
	debug = 1;
    if (as->parms[3].items)	/* -nowrite */
	Testing = 1;
    if (as->parms[4].items)	/* -inodes */
	ListInodeOption = 1;
    if (as->parms[5].items)	/* -oktozap */
	OKToZap = 1;
    if (as->parms[6].items)	/* -rootinodes */
	ShowRootFiles = 1;
    if (as->parms[8].items)	/* -ForceReads */
	forceR = 1;
    if ((ti = as->parms[9].items)) {	/* -Parallel # */
	temp = ti->data;
	if (strncmp(temp, "all", 3) == 0) {
	    PartsPerDisk = 1;
	    temp += 3;
	}
	if (strlen(temp) != 0) {
	    Parallel = atoi(temp);
	    if (Parallel < 1)
		Parallel = 1;
	    if (Parallel > MAXPARALLEL) {
		printf("Setting parallel salvages to maximum of %d \n",
		       MAXPARALLEL);
		Parallel = MAXPARALLEL;
	    }
	}
    } else {
	Parallel = MIN(DEFAULT_PARALLELISM, MAXPARALLEL);
    }
    if ((ti = as->parms[10].items)) {	/* -tmpdir */
	DIR *dirp;

	tmpdir = ti->data;
	dirp = opendir(tmpdir);
	if (!dirp) {
	    printf
		("Can't open temporary placeholder dir %s; using current partition \n",
		 tmpdir);
	    tmpdir = NULL;
	} else
	    closedir(dirp);
    }
    if ((ti = as->parms[11].items))	/* -showlog */
	ShowLog = 1;
    if ((ti = as->parms[12].items)) {	/* -orphans */
	if (Testing)
	    orphans = ORPH_IGNORE;
	else if (strcmp(ti->data, "remove") == 0
		 || strcmp(ti->data, "r") == 0)
	    orphans = ORPH_REMOVE;
	else if (strcmp(ti->data, "attach") == 0
		 || strcmp(ti->data, "a") == 0)
	    orphans = ORPH_ATTACH;
    }
#ifndef AFS_NT40_ENV		/* ignore options on NT */
    if ((ti = as->parms[13].items)) {	/* -syslog */
	useSyslog = 1;
	ShowLog = 0;
    }
    if ((ti = as->parms[14].items)) {	/* -syslogfacility */
	useSyslogFacility = atoi(ti->data);
    }

    if ((ti = as->parms[15].items)) {	/* -datelogs */
	TimeStampLogFile((char *)AFSDIR_SERVER_SALSRVLOG_FILEPATH);
    }
#endif

    if ((ti = as->parms[16].items)) {   /* -client */
	if ((ti = as->parms[0].items)) {	/* -partition */
	    seenpart = 1;
	    strlcpy(pname, ti->data, sizeof(pname));
	}
	if ((ti = as->parms[1].items)) {	/* -volumeid */
	    char *end;
	    unsigned long vid_l;
	    seenvol = 1;
	    vid_l = strtoul(ti->data, &end, 10);
	    if (vid_l >= MAX_AFS_UINT32 || vid_l == ULONG_MAX || *end != '\0') {
		printf("Invalid volume id specified; salvage aborted\n");
		exit(-1);
	    }
	    vid = (VolumeId)vid_l;
	}

	if (ShowLog) {
	    printf("-showlog does not work with -client\n");
	    exit(-1);
	}

	if (!seenpart || !seenvol) {
	    printf("You must specify '-partition' and '-volumeid' with the '-client' option\n");
	    exit(-1);
	}

	SalvageClient(vid, pname);

    } else {  /* salvageserver mode */
	SalvageServer(rock->argc, rock->argv);
    }
    return (0);
}


#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif
#define MAX_ARGS 128
#ifdef AFS_NT40_ENV
char *save_args[MAX_ARGS];
int n_save_args = 0;
pthread_t main_thread;
#endif

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    int err = 0;
    struct cmdline_rock arock;

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

    /* Initialize directory paths */
    if (!(initAFSDirPath() & AFSDIR_SERVER_PATHS_OK)) {
#ifdef AFS_NT40_ENV
	ReportErrorEventAlt(AFSEVT_SVR_NO_INSTALL_DIR, 0, argv[0], 0);
#endif
	fprintf(stderr, "%s: Unable to obtain AFS server directory.\n",
		argv[0]);
	exit(2);
    }
#ifdef AFS_NT40_ENV
    /* Default to binary mode for fopen() */
    _set_fmode(_O_BINARY);

    main_thread = pthread_self();
    if (spawnDatap && spawnDataLen) {
	/* This is a child per partition salvager. Don't setup log or
	 * try to lock the salvager lock.
	 */
	if (nt_SetupPartitionSalvage(spawnDatap, spawnDataLen) < 0)
	    exit(3);
    } else {
#endif

#ifndef AFS_NT40_ENV
	if (geteuid() != 0) {
	    printf("Salvager must be run as root.\n");
	    fflush(stdout);
	    Exit(0);
	}
#endif

	/* bad for normal help flag processing, but can do nada */

#ifdef AFS_NT40_ENV
    }
#endif

    arock.argc = argc;
    arock.argv = argv;

    ts = cmd_CreateSyntax("initcmd", handleit, &arock, "initialize the program");
    cmd_AddParm(ts, "-partition", CMD_SINGLE, CMD_OPTIONAL,
		"Name of partition to salvage");
    cmd_AddParm(ts, "-volumeid", CMD_SINGLE, CMD_OPTIONAL,
		"Volume Id to salvage");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL,
		"Run in Debugging mode");
    cmd_AddParm(ts, "-nowrite", CMD_FLAG, CMD_OPTIONAL,
		"Run readonly/test mode");
    cmd_AddParm(ts, "-inodes", CMD_FLAG, CMD_OPTIONAL,
		"Just list affected afs inodes - debugging flag");
    cmd_AddParm(ts, "-oktozap", CMD_FLAG, CMD_OPTIONAL,
		"Give permission to destroy bogus inodes/volumes - debugging flag");
    cmd_AddParm(ts, "-rootinodes", CMD_FLAG, CMD_OPTIONAL,
		"Show inodes owned by root - debugging flag");
    cmd_AddParm(ts, "-salvagedirs", CMD_FLAG, CMD_OPTIONAL,
		"Force rebuild/salvage of all directories");
    cmd_AddParm(ts, "-blockreads", CMD_FLAG, CMD_OPTIONAL,
		"Read smaller blocks to handle IO/bad blocks");
    cmd_AddParm(ts, "-parallel", CMD_SINGLE, CMD_OPTIONAL,
		"# of max parallel partition salvaging");
    cmd_AddParm(ts, "-tmpdir", CMD_SINGLE, CMD_OPTIONAL,
		"Name of dir to place tmp files ");
    cmd_AddParm(ts, "-showlog", CMD_FLAG, CMD_OPTIONAL,
		"Show log file upon completion");
    cmd_AddParm(ts, "-orphans", CMD_SINGLE, CMD_OPTIONAL,
		"ignore | remove | attach");

    /* note - syslog isn't avail on NT, but if we make it conditional, have
     * to deal with screwy offsets for cmd params */
    cmd_AddParm(ts, "-syslog", CMD_FLAG, CMD_OPTIONAL,
		"Write salvage log to syslogs");
    cmd_AddParm(ts, "-syslogfacility", CMD_SINGLE, CMD_OPTIONAL,
		"Syslog facility number to use");
    cmd_AddParm(ts, "-datelogs", CMD_FLAG, CMD_OPTIONAL,
		"Include timestamp in logfile filename");

    cmd_AddParm(ts, "-client", CMD_FLAG, CMD_OPTIONAL,
		"Use SALVSYNC to ask salvageserver to salvage a volume");

    err = cmd_Dispatch(argc, argv);
    Exit(err);
    return 0; /* not reached */
}

static void
SalvageClient(VolumeId vid, char * pname)
{
    int done = 0;
    afs_int32 code;
    SYNC_response res;
    SALVSYNC_response_hdr sres;
    VolumePackageOptions opts;

    VOptDefaults(volumeUtility, &opts);
    if (VInitVolumePackage2(volumeUtility, &opts)) {
	/* VInitVolumePackage2 can fail on e.g. partition attachment errors,
	 * but we don't really care, since all we're doing is trying to use
	 * SALVSYNC */
	fprintf(stderr, "errors encountered initializing volume package, but "
	                "trying to continue anyway\n");
    }
    SALVSYNC_clientInit();

    code = SALVSYNC_SalvageVolume(vid, pname, SALVSYNC_SALVAGE, SALVSYNC_OPERATOR, 0, NULL);
    if (code != SYNC_OK) {
	goto sync_error;
    }

    res.payload.buf = (void *) &sres;
    res.payload.len = sizeof(sres);

    while(!done) {
	sleep(2);
	code = SALVSYNC_SalvageVolume(vid, pname, SALVSYNC_QUERY, SALVSYNC_WHATEVER, 0, &res);
	if (code != SYNC_OK) {
	    goto sync_error;
	}
	switch (sres.state) {
	case SALVSYNC_STATE_ERROR:
	    printf("salvageserver reports salvage ended in an error; check log files for more details\n");
	case SALVSYNC_STATE_DONE:
	case SALVSYNC_STATE_UNKNOWN:
	    done = 1;
	}
    }
    SALVSYNC_clientFinis();
    return;

 sync_error:
    if (code == SYNC_DENIED) {
	printf("salvageserver refused to salvage volume %u on partition %s\n",
	       vid, pname);
    } else if (code == SYNC_BAD_COMMAND) {
	printf("SALVSYNC protocol mismatch; please make sure fileserver, volserver, salvageserver and salvager are same version\n");
    } else if (code == SYNC_COM_ERROR) {
	printf("SALVSYNC communications error\n");
    }
    SALVSYNC_clientFinis();
    exit(-1);
}

static int * child_slot;

static void
SalvageServer(int argc, char **argv)
{
    int pid, ret;
    struct SalvageQueueNode * node;
    pthread_t tid;
    pthread_attr_t attrs;
    int slot;
    VolumePackageOptions opts;

    /* All entries to the log will be appended.  Useful if there are
     * multiple salvagers appending to the log.
     */

    CheckLogFile((char *)AFSDIR_SERVER_SALSRVLOG_FILEPATH);
#ifndef AFS_NT40_ENV
#ifdef AFS_LINUX20_ENV
    fcntl(fileno(logFile), F_SETFL, O_APPEND);	/* Isn't this redundant? */
#else
    fcntl(fileno(logFile), F_SETFL, FAPPEND);	/* Isn't this redundant? */
#endif
#endif
    setlinebuf(logFile);

    fprintf(logFile, "%s\n", cml_version_number);
    LogCommandLine(argc, argv, "Online Salvage Server",
		   SalvageVersion, "Starting OpenAFS", Log);
    /* Get and hold a lock for the duration of the salvage to make sure
     * that no other salvage runs at the same time.  The routine
     * VInitVolumePackage2 (called below) makes sure that a file server or
     * other volume utilities don't interfere with the salvage.
     */

    /* even demand attach online salvager
     * still needs this because we don't want
     * a stand-alone salvager to conflict with
     * the salvager daemon */
    ObtainSharedSalvageLock();

    child_slot = (int *) malloc(Parallel * sizeof(int));
    osi_Assert(child_slot != NULL);
    memset(child_slot, 0, Parallel * sizeof(int));

    /* initialize things */
    VOptDefaults(salvageServer, &opts);
    if (VInitVolumePackage2(salvageServer, &opts)) {
	Log("Shutting down: errors encountered initializing volume package\n");
	Exit(1);
    }
    DInit(10);
    queue_Init(&pending_q);
    queue_Init(&log_cleanup_queue);
    MUTEX_INIT(&worker_lock, "worker", MUTEX_DEFAULT, 0);
    CV_INIT(&worker_cv, "worker", CV_DEFAULT, 0);
    CV_INIT(&log_cleanup_queue.queue_change_cv, "queuechange", CV_DEFAULT, 0);
    osi_Assert(pthread_attr_init(&attrs) == 0);

    /* start up the reaper and log cleaner threads */
    osi_Assert(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);
    osi_Assert(pthread_create(&tid,
			  &attrs,
			  &SalvageChildReaperThread,
			  NULL) == 0);
    osi_Assert(pthread_create(&tid,
			  &attrs,
			  &SalvageLogCleanupThread,
			  NULL) == 0);
    osi_Assert(pthread_create(&tid,
			  &attrs,
			  &SalvageLogScanningThread,
			  NULL) == 0);

    /* loop forever serving requests */
    while (1) {
	node = SALVSYNC_getWork();
	osi_Assert(node != NULL);

	Log("dispatching child to salvage volume %u...\n",
	    node->command.sop.parent);

	VOL_LOCK;
	/* find a slot */
	for (slot = 0; slot < Parallel; slot++) {
	  if (!child_slot[slot])
	    break;
	}
	osi_Assert (slot < Parallel);

    do_fork:
	pid = Fork();
	if (pid == 0) {
	    VOL_UNLOCK;
	    ret = DoSalvageVolume(node, slot);
	    Exit(ret);
	} else if (pid < 0) {
	    Log("failed to fork child worker process\n");
	    sleep(1);
	    goto do_fork;
	} else {
	    child_slot[slot] = pid;
	    node->pid = pid;
	    VOL_UNLOCK;

	    MUTEX_ENTER(&worker_lock);
	    current_workers++;

	    /* let the reaper thread know another worker was spawned */
	    CV_BROADCAST(&worker_cv);

	    /* if we're overquota, wait for the reaper */
	    while (current_workers >= Parallel) {
		CV_WAIT(&worker_cv, &worker_lock);
	    }
	    MUTEX_EXIT(&worker_lock);
	}
    }
}

static int
DoSalvageVolume(struct SalvageQueueNode * node, int slot)
{
    char childLog[AFSDIR_PATH_MAX];
    struct DiskPartition64 * partP;

    /* do not allow further forking inside salvager */
    canfork = 0;

    /* do not attempt to close parent's logFile handle as
     * another thread may have held the lock on the FILE
     * structure when fork was called! */

    afs_snprintf(childLog, sizeof(childLog), "%s.%d",
		 AFSDIR_SERVER_SLVGLOG_FILEPATH, getpid());

    logFile = afs_fopen(childLog, "a");
    if (!logFile) {		/* still nothing, use stdout */
	logFile = stdout;
	ShowLog = 0;
    }

    if (node->command.sop.parent <= 0) {
	Log("salvageServer: invalid volume id specified; salvage aborted\n");
	return 1;
    }

    partP = VGetPartition(node->command.sop.partName, 0);
    if (!partP) {
	Log("salvageServer: Unknown or unmounted partition %s; salvage aborted\n",
	    node->command.sop.partName);
	return 1;
    }

    /* obtain a shared salvage lock in the child worker, so if the
     * salvageserver restarts (and we continue), we will still hold a lock and
     * prevent standalone salvagers from interfering */
    ObtainSharedSalvageLock();

    /* Salvage individual volume; don't notify fs */
    SalvageFileSys1(partP, node->command.sop.parent);

    fclose(logFile);
    return 0;
}


static void *
SalvageChildReaperThread(void * args)
{
    int slot, pid, status;
    struct log_cleanup_node * cleanup;

    MUTEX_ENTER(&worker_lock);

    /* loop reaping our children */
    while (1) {
	/* wait() won't block unless we have children, so
	 * block on the cond var if we're childless */
	while (current_workers == 0) {
	    CV_WAIT(&worker_cv, &worker_lock);
	}

	MUTEX_EXIT(&worker_lock);

	cleanup = (struct log_cleanup_node *) malloc(sizeof(struct log_cleanup_node));

	while (Reap_Child("salvageserver", &pid, &status) < 0) {
	    /* try to prevent livelock if something goes wrong */
	    sleep(1);
	}

	VOL_LOCK;
	for (slot = 0; slot < Parallel; slot++) {
	    if (child_slot[slot] == pid)
		break;
	}
	osi_Assert(slot < Parallel);
	child_slot[slot] = 0;
	VOL_UNLOCK;

	SALVSYNC_doneWorkByPid(pid, status);

	MUTEX_ENTER(&worker_lock);

	if (cleanup) {
	    cleanup->pid = pid;
	    queue_Append(&log_cleanup_queue, cleanup);
	    CV_SIGNAL(&log_cleanup_queue.queue_change_cv);
	}

	/* ok, we've reaped a child */
	current_workers--;
	CV_BROADCAST(&worker_cv);
    }

    return NULL;
}

static int
Reap_Child(char *prog, int * pid, int * status)
{
    int ret;
    ret = wait(status);

    if (ret >= 0) {
	*pid = ret;
        if (WCOREDUMP(*status))
	    Log("\"%s\" core dumped!\n", prog);
	if ((WIFSIGNALED(*status) != 0) ||
	    ((WEXITSTATUS(*status) != 0) &&
	     (WEXITSTATUS(*status) != SALSRV_EXIT_VOLGROUP_LINK)))
	    Log("\"%s\" (pid=%d) terminated abnormally!\n", prog, ret);
    } else {
	Log("wait returned -1\n");
    }
    return ret;
}

/*
 * thread to combine salvager child logs
 * back into the main salvageserver log
 */
static void *
SalvageLogCleanupThread(void * arg)
{
    struct log_cleanup_node * cleanup;

    MUTEX_ENTER(&worker_lock);

    while (1) {
	while (queue_IsEmpty(&log_cleanup_queue)) {
	    CV_WAIT(&log_cleanup_queue.queue_change_cv, &worker_lock);
	}

	while (queue_IsNotEmpty(&log_cleanup_queue)) {
	    cleanup = queue_First(&log_cleanup_queue, log_cleanup_node);
	    queue_Remove(cleanup);
	    MUTEX_EXIT(&worker_lock);
	    SalvageLogCleanup(cleanup->pid);
	    free(cleanup);
	    MUTEX_ENTER(&worker_lock);
	}
    }

    MUTEX_EXIT(&worker_lock);
    return NULL;
}

#define LOG_XFER_BUF_SIZE 65536
static int
SalvageLogCleanup(int pid)
{
    int pidlog, len;
    char fn[AFSDIR_PATH_MAX];
    static char buf[LOG_XFER_BUF_SIZE];

    afs_snprintf(fn, sizeof(fn), "%s.%d",
		 AFSDIR_SERVER_SLVGLOG_FILEPATH, pid);


    pidlog = open(fn, O_RDONLY);
    unlink(fn);
    if (pidlog < 0)
	return 1;

    len = read(pidlog, buf, LOG_XFER_BUF_SIZE);
    while (len) {
	fwrite(buf, len, 1, logFile);
	len = read(pidlog, buf, LOG_XFER_BUF_SIZE);
    }

    close(pidlog);

    return 0;
}

/* wake up every five minutes to see if a non-child salvage has finished */
#define SALVAGE_SCAN_POLL_INTERVAL 300

/**
 * Thread to look for SalvageLog.$pid files that are not from our child
 * worker salvagers, and notify SalvageLogCleanupThread to clean them
 * up. This can happen if we restart during salvages, or the
 * salvageserver crashes or something.
 *
 * @param arg  unused
 *
 * @return always NULL
 */
static void *
SalvageLogScanningThread(void * arg)
{
    struct rx_queue log_watch_queue;

    queue_Init(&log_watch_queue);

    {
	DIR *dp;
	struct dirent *dirp;
	char prefix[AFSDIR_PATH_MAX];
	size_t prefix_len;

	afs_snprintf(prefix, sizeof(prefix), "%s.", AFSDIR_SLVGLOG_FILE);
	prefix_len = strlen(prefix);

	dp = opendir(AFSDIR_LOGS_DIR);
	osi_Assert(dp);

	while ((dirp = readdir(dp)) != NULL) {
	    pid_t pid;
	    struct log_cleanup_node *cleanup;
	    int i;

	    if (strncmp(dirp->d_name, prefix, prefix_len) != 0) {
		/* not a salvage logfile; skip */
		continue;
	    }

	    errno = 0;
	    pid = strtol(dirp->d_name + prefix_len, NULL, 10);

	    if (errno != 0) {
		/* file is SalvageLog.<something> but <something> isn't
		 * a pid, so skip */
		 continue;
	    }

	    VOL_LOCK;
	    for (i = 0; i < Parallel; ++i) {
		if (pid == child_slot[i]) {
		    break;
		}
	    }
	    VOL_UNLOCK;
	    if (i < Parallel) {
		/* this pid is one of our children, so the reaper thread
		 * will take care of it; skip */
		continue;
	    }

	    cleanup =
		(struct log_cleanup_node *) malloc(sizeof(struct log_cleanup_node));
	    cleanup->pid = pid;

	    queue_Append(&log_watch_queue, cleanup);
	}

	closedir(dp);
    }

    ScanLogs(&log_watch_queue);

    while (queue_IsNotEmpty(&log_watch_queue)) {
	sleep(SALVAGE_SCAN_POLL_INTERVAL);
	ScanLogs(&log_watch_queue);
    }

    return NULL;
}

/**
 * look through log_watch_queue, and if any processes are not still
 * running, hand them off to the SalvageLogCleanupThread
 *
 * @param log_watch_queue  a queue of PIDs that we should clean up if
 * that PID has died
 */
static void
ScanLogs(struct rx_queue *log_watch_queue)
{
    struct log_cleanup_node *cleanup, *next;

    MUTEX_ENTER(&worker_lock);

    for (queue_Scan(log_watch_queue, cleanup, next, log_cleanup_node)) {
	/* if a process is still running, assume it's the salvage process
	 * still going, and keep waiting for it */
	if (kill(cleanup->pid, 0) < 0 && errno == ESRCH) {
	    queue_Remove(cleanup);
	    queue_Append(&log_cleanup_queue, cleanup);
	    CV_SIGNAL(&log_cleanup_queue.queue_change_cv);
	}
    }

    MUTEX_EXIT(&worker_lock);
}
