/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *      System:		VICE-TWO
 *      Module:		salvager.c
 *      Institution:	The Information Technology Center, Carnegie-Mellon University
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


static int get_salvage_lock = 0;


/* Forward declarations */
/*@printflike@*/ void Log(const char *format, ...);
/*@printflike@*/ void Abort(const char *format, ...);


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

#ifdef FAST_RESTART
    {
	afs_int32 i;
	for (i = 0; i < CMD_MAXPARMS; i++) {
	    if (as->parms[i].items) {
		seenany = 1;
		break;
	    }
	}
    }
    if (!seenany) {
	char *msg =
	    "Exiting immediately without salvage. Look into the FileLog to find volumes which really need to be salvaged!";

	if (useSyslog)
	    Log(msg);
	else
	    printf("%s\n", msg);

	Exit(0);
    }
#endif /* FAST_RESTART */
    if ((ti = as->parms[0].items)) {	/* -partition */
	seenpart = 1;
	strncpy(pname, ti->data, 100);
    }
    if ((ti = as->parms[1].items)) {	/* -volumeid */
	if (!seenpart) {
	    printf
		("You must also specify '-partition' option with the '-volumeid' option\n");
	    exit(-1);
	}
	seenvol = 1;
	vid = atoi(ti->data);
    }
    if (as->parms[2].items)	/* -debug */
	debug = 1;
    if (as->parms[3].items)	/* -nowrite */
	Testing = 1;
    if (as->parms[4].items)	/* -inodes */
	ListInodeOption = 1;
    if (as->parms[5].items)	/* -force */
	ForceSalvage = 1;
    if (as->parms[6].items)	/* -oktozap */
	OKToZap = 1;
    if (as->parms[7].items)	/* -rootinodes */
	ShowRootFiles = 1;
    if (as->parms[8].items)	/* -RebuildDirs */
	RebuildDirs = 1;
    if (as->parms[9].items)	/* -ForceReads */
	forceR = 1;
    if ((ti = as->parms[10].items)) {	/* -Parallel # */
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
    }
    if ((ti = as->parms[11].items)) {	/* -tmpdir */
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
    if ((ti = as->parms[12].items))	/* -showlog */
	ShowLog = 1;
    if ((ti = as->parms[13].items)) {	/* -log */
	Testing = 1;
	ShowSuid = 1;
	Showmode = 1;
    }
    if ((ti = as->parms[14].items)) {	/* -showmounts */
	Testing = 1;
	Showmode = 1;
	ShowMounts = 1;
    }
    if ((ti = as->parms[15].items)) {	/* -orphans */
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
    if ((ti = as->parms[16].items)) {	/* -syslog */
	useSyslog = 1;
	ShowLog = 0;
    }
    if ((ti = as->parms[17].items)) {	/* -syslogfacility */
	useSyslogFacility = atoi(ti->data);
    }

    if ((ti = as->parms[18].items)) {	/* -datelogs */
	TimeStampLogFile(AFSDIR_SERVER_SLVGLOG_FILEPATH);
    }
#endif

#ifdef FAST_RESTART
    if (ti = as->parms[19].items) {	/* -DontSalvage */
	char *msg =
	    "Exiting immediately without salvage. Look into the FileLog to find volumes which really need to be salvaged!";

	if (useSyslog)
	    Log(msg);
	else
	    printf("%s\n", msg);
	Exit(0);
    }
#elif defined(DEMAND_ATTACH_ENABLE)
    if (seenvol && !as->parms[19].items) {
	char * msg =
	    "The standalone salvager cannot be run concurrently with a Demand Attach Fileserver.  Please use 'salvageserver -client <partition> <volume id>' to manually schedule volume salvages with the salvageserver (new versions of 'bos salvage' automatically do this for you).  Or, if you insist on using the standalone salvager, add the -forceDAFS flag to your salvager command line.";

	if (useSyslog)
	    Log(msg);
	else
	    printf("%s\n", msg);
	Exit(1);
    }
#endif

    if (get_salvage_lock) {
	ObtainSalvageLock();
    }

    /* Note:  if seenvol we initialize this as a standard volume utility:  this has the
     * implication that the file server may be running; negotations have to be made with
     * the file server in this case to take the read write volume and associated read-only
     * volumes off line before salvaging */
#ifdef AFS_NT40_ENV
    if (seenvol) {
	if (afs_winsockInit() < 0) {
	    ReportErrorEventAlt(AFSEVT_SVR_WINSOCK_INIT_FAILED, 0,
				AFSDIR_SALVAGER_FILE, 0);
	    Log("Failed to initailize winsock, exiting.\n");
	    Exit(1);
	}
    }
#endif
    VInitVolumePackage(seenvol ? volumeUtility : salvager, 5, 5,
		       DONT_CONNECT_FS, 0);
    DInit(10);
#ifdef AFS_NT40_ENV
    if (myjob.cj_number != NOT_CHILD) {
	if (!seenpart) {
	    seenpart = 1;
	    (void)strcpy(pname, myjob.cj_part);
	}
    }
#endif
    if (seenpart == 0) {
	for (partP = DiskPartitionList; partP; partP = partP->next) {
	    SalvageFileSysParallel(partP);
	}
	SalvageFileSysParallel(0);
    } else {
	partP = VGetPartition(pname, 0);
	if (!partP) {
	    Log("salvage: Unknown or unmounted partition %s; salvage aborted\n", pname);
	    Exit(1);
	}
	if (!seenvol)
	    SalvageFileSys(partP, 0);
	else {
	    /* Salvage individual volume */
	    if (vid <= 0) {
		Log("salvage: invalid volume id specified; salvage aborted\n");
		Exit(1);
	    }
	    SalvageFileSys(partP, vid);
	}
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
    char commandLine[150];

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
		strcat(commandLine, " ");
	    strcat(commandLine, argv[i]);
	}

	/* All entries to the log will be appended.  Useful if there are
	 * multiple salvagers appending to the log.
	 */

	CheckLogFile(AFSDIR_SERVER_SLVGLOG_FILEPATH);
#ifndef AFS_NT40_ENV
#ifdef AFS_LINUX20_ENV
	fcntl(fileno(logFile), F_SETFL, O_APPEND);	/* Isn't this redundant? */
#else
	fcntl(fileno(logFile), F_SETFL, FAPPEND);	/* Isn't this redundant? */
#endif
#endif
	setlinebuf(logFile);

#ifndef AFS_NT40_ENV
	if (geteuid() != 0) {
	    printf("Salvager must be run as root.\n");
	    fflush(stdout);
	    Exit(0);
	}
#endif

	/* bad for normal help flag processing, but can do nada */

	fprintf(logFile, "%s\n", cml_version_number);
	Log("STARTING AFS SALVAGER %s (%s)\n", SalvageVersion, commandLine);

	/* Get and hold a lock for the duration of the salvage to make sure
	 * that no other salvage runs at the same time.  The routine
	 * VInitVolumePackage (called below) makes sure that a file server or
	 * other volume utilities don't interfere with the salvage.
	 */
	get_salvage_lock = 1;
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
    cmd_AddParm(ts, "-force", CMD_FLAG, CMD_OPTIONAL, "Force full salvaging");
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
    cmd_AddParm(ts, "-showsuid", CMD_FLAG, CMD_OPTIONAL,
		"Report on suid/sgid files");
    cmd_AddParm(ts, "-showmounts", CMD_FLAG, CMD_OPTIONAL,
		"Report on mountpoints");
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
#ifdef FAST_RESTART
    cmd_AddParm(ts, "-DontSalvage", CMD_FLAG, CMD_OPTIONAL,
		"Don't salvage. This my be set in BosConfig to let the fileserver restart immediately after a crash. Bad volumes will be taken offline");
#elif defined(DEMAND_ATTACH_ENABLE)
    cmd_AddParm(ts, "-forceDAFS", CMD_FLAG, CMD_OPTIONAL,
		"For Demand Attach Fileserver, permit a manual volume salvage outside of the salvageserver");
#endif /* FAST_RESTART */
    err = cmd_Dispatch(argc, argv);
    Exit(err);
}

