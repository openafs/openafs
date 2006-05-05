/*
 * Copyright 2006, Sine Nomine Associates and others.
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

RCSID
    ("$Header$");

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
#if	defined(AFS_AIX_ENV) || defined(AFS_SUN4_ENV)
#define WCOREDUMP(x)	(x & 0200)
#endif
#include <rx/xdr.h>
#include <afs/afsint.h>
#include <afs/assert.h>
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
#if !defined(AFS_LINUX20_ENV) && !defined(AFS_XBSD_ENV)
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
#include "volinodes.h"		/* header magic number, etc. stuff */
#include "vol-salvage.h"
#ifdef AFS_NT40_ENV
#include <pthread.h>
#endif


#if !defined(AFS_DEMAND_ATTACH_FS)
#error "online salvager only supported for demand attach fileserver"
#endif /* AFS_DEMAND_ATTACH_FS */

#if defined(AFS_NT40_ENV)
#error "online salvager not supported on NT"
#endif /* AFS_NT40_ENV */


/* Forward declarations */
/*@printflike@*/ void Log(const char *format, ...);
/*@printflike@*/ void Abort(const char *format, ...);


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

static void SalvageServer(void);
static void SalvageClient(VolumeId vid, char * pname);

static int Reap_Child(char * prog, int * pid, int * status);

static void * SalvageLogCleanupThread(void *);
static int SalvageLogCleanup(int pid);

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
handleit(struct cmd_syndesc *as)
{
    register struct cmd_item *ti;
    char pname[100], *temp;
    afs_int32 seenpart = 0, seenvol = 0, vid = 0, seenany = 0;
    struct DiskPartition *partP;


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
	TimeStampLogFile(AFSDIR_SERVER_SALSRVLOG_FILEPATH);
    }
#endif

    if ((ti = as->parms[16].items)) {   /* -client */
	if ((ti = as->parms[0].items)) {	/* -partition */
	    seenpart = 1;
	    strlcpy(pname, ti->data, sizeof(pname));
	}
	if ((ti = as->parms[1].items)) {	/* -volumeid */
	    seenvol = 1;
	    vid = atoi(ti->data);
	}

	if (!seenpart || !seenvol) {
	    printf("You must specify '-partition' and '-volumeid' with the '-client' option\n");
	    exit(-1);
	}

	SalvageClient(vid, pname);

    } else {  /* salvageserver mode */
	SalvageServer();
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

static char commandLine[150];

int
main(int argc, char **argv)
{
    struct cmd_syndesc *ts;
    int err = 0;

    int i;
    extern char cml_version_number[];

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
    main_thread = pthread_self();
    if (spawnDatap && spawnDataLen) {
	/* This is a child per partition salvager. Don't setup log or
	 * try to lock the salvager lock.
	 */
	if (nt_SetupPartitionSalvage(spawnDatap, spawnDataLen) < 0)
	    exit(3);
    } else {
#endif
	for (commandLine[0] = '\0', i = 0; i < argc; i++) {
	    if (i > 0)
		strlcat(commandLine, " ", sizeof(commandLine));
	    strlcat(commandLine, argv[i], sizeof(commandLine));
	}

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

    ts = cmd_CreateSyntax("initcmd", handleit, 0, "initialize the program");
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
}

static void
SalvageClient(VolumeId vid, char * pname)
{
    int done = 0;
    afs_int32 code;
    SYNC_response res;
    SALVSYNC_response_hdr sres;

    VInitVolumePackage(volumeUtility, 5, 5, DONT_CONNECT_FS, 0);
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
SalvageServer(void)
{
    int pid, ret;
    struct SalvageQueueNode * node;
    pthread_t tid;
    pthread_attr_t attrs;
    int slot;

    /* All entries to the log will be appended.  Useful if there are
     * multiple salvagers appending to the log.
     */

    CheckLogFile(AFSDIR_SERVER_SALSRVLOG_FILEPATH);
#ifndef AFS_NT40_ENV
#ifdef AFS_LINUX20_ENV
    fcntl(fileno(logFile), F_SETFL, O_APPEND);	/* Isn't this redundant? */
#else
    fcntl(fileno(logFile), F_SETFL, FAPPEND);	/* Isn't this redundant? */
#endif
#endif
    setlinebuf(logFile);

    fprintf(logFile, "%s\n", cml_version_number);
    Log("Starting OpenAFS Online Salvage Server %s (%s)\n", SalvageVersion, commandLine);
    
    /* Get and hold a lock for the duration of the salvage to make sure
     * that no other salvage runs at the same time.  The routine
     * VInitVolumePackage (called below) makes sure that a file server or
     * other volume utilities don't interfere with the salvage.
     */
    
    /* even demand attach online salvager
     * still needs this because we don't want
     * a stand-alone salvager to conflict with
     * the salvager daemon */
    ObtainSalvageLock();

    child_slot = (int *) malloc(Parallel * sizeof(int));
    assert(child_slot != NULL);
    memset(child_slot, 0, Parallel * sizeof(int));
	    
    /* initialize things */
    VInitVolumePackage(salvageServer, 5, 5,
		       1, 0);
    DInit(10);
    queue_Init(&pending_q);
    queue_Init(&log_cleanup_queue);
    assert(pthread_mutex_init(&worker_lock, NULL) == 0);
    assert(pthread_cond_init(&worker_cv, NULL) == 0);
    assert(pthread_cond_init(&log_cleanup_queue.queue_change_cv, NULL) == 0);
    assert(pthread_attr_init(&attrs) == 0);

    /* start up the reaper and log cleaner threads */
    assert(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);
    assert(pthread_create(&tid, 
			  &attrs, 
			  &SalvageChildReaperThread,
			  NULL) == 0);
    assert(pthread_create(&tid, 
			  &attrs, 
			  &SalvageLogCleanupThread,
			  NULL) == 0);

    /* loop forever serving requests */
    while (1) {
	node = SALVSYNC_getWork();
	assert(node != NULL);

	VOL_LOCK;
	/* find a slot */
	for (slot = 0; slot < Parallel; slot++) {
	  if (!child_slot[slot])
	    break;
	}
	assert (slot < Parallel);

	pid = Fork();
	if (pid == 0) {
	    VOL_UNLOCK;
	    ret = DoSalvageVolume(node, slot);
	    Exit(ret);
	} else if (pid < 0) {
	    VOL_UNLOCK;
	    SALVSYNC_doneWork(node, 1);
	} else {
	    child_slot[slot] = pid;
	    node->pid = pid;
	    VOL_UNLOCK;
	    
	    assert(pthread_mutex_lock(&worker_lock) == 0);
	    current_workers++;
	    
	    /* let the reaper thread know another worker was spawned */
	    assert(pthread_cond_broadcast(&worker_cv) == 0);
	    
	    /* if we're overquota, wait for the reaper */
	    while (current_workers >= Parallel) {
		assert(pthread_cond_wait(&worker_cv, &worker_lock) == 0);
	    }
	    assert(pthread_mutex_unlock(&worker_lock) == 0);
	}
    }
}

static int
DoSalvageVolume(struct SalvageQueueNode * node, int slot)
{
    char childLog[AFSDIR_PATH_MAX];
    int ret;
    struct DiskPartition * partP;

    VChildProcReconnectFS();

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

    if (node->command.sop.volume <= 0) {
	Log("salvageServer: invalid volume id specified; salvage aborted\n");
	return 1;
    }
    
    partP = VGetPartition(node->command.sop.partName, 0);
    if (!partP) {
	Log("salvageServer: Unknown or unmounted partition %s; salvage aborted\n", 
	    node->command.sop.partName);
	return 1;
    }

    /* Salvage individual volume; don't notify fs */
    SalvageFileSys1(partP, node->command.sop.volume);

    VDisconnectFS();

    fclose(logFile);
    return 0;
}


static void *
SalvageChildReaperThread(void * args)
{
    int slot, pid, status, code, found;
    struct SalvageQueueNode *qp, *nqp;
    struct log_cleanup_node * cleanup;

    assert(pthread_mutex_lock(&worker_lock) == 0);

    /* loop reaping our children */
    while (1) {
	/* wait() won't block unless we have children, so
	 * block on the cond var if we're childless */
	while (current_workers == 0) {
	    assert(pthread_cond_wait(&worker_cv, &worker_lock) == 0);
	}

	assert(pthread_mutex_unlock(&worker_lock) == 0);

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
	assert(slot < Parallel);
	child_slot[slot] = 0;
	VOL_UNLOCK;

	assert(pthread_mutex_lock(&worker_lock) == 0);

	if (cleanup) {
	    cleanup->pid = pid;
	    queue_Append(&log_cleanup_queue, cleanup);
	    assert(pthread_cond_signal(&log_cleanup_queue.queue_change_cv) == 0);
	}

	/* ok, we've reaped a child */
	current_workers--;
	SALVSYNC_doneWorkByPid(pid, 0);
	assert(pthread_cond_broadcast(&worker_cv) == 0);
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
	if (WIFSIGNALED(*status) != 0 || WEXITSTATUS(*status) != 0)
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

    assert(pthread_mutex_lock(&worker_lock) == 0);

    while (1) {
	while (queue_IsEmpty(&log_cleanup_queue)) {
	    assert(pthread_cond_wait(&log_cleanup_queue.queue_change_cv, &worker_lock) == 0);
	}

	while (queue_IsNotEmpty(&log_cleanup_queue)) {
	    cleanup = queue_First(&log_cleanup_queue, log_cleanup_node);
	    queue_Remove(cleanup);
	    assert(pthread_mutex_unlock(&worker_lock) == 0);
	    SalvageLogCleanup(cleanup->pid);
	    free(cleanup);
	    assert(pthread_mutex_lock(&worker_lock) == 0);
	}	    
    }

    assert(pthread_mutex_unlock(&worker_lock) == 0);
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
