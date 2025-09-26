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

#include <afs/procmgmt.h>
#include <roken.h>

#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif

#ifndef WCOREDUMP
#define WCOREDUMP(x)	((x) & 0200)
#endif

#include <rx/xdr.h>
#include <afs/afsint.h>
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
#if !defined(AFS_LINUX_ENV) && !defined(AFS_XBSD_ENV) && !defined(AFS_DARWIN_ENV)
#include <sys/inode.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_SGI_ENV */
#ifdef	AFS_AIX_ENV
#include <sys/vfs.h>
#include <sys/lockf.h>
#else
#ifdef	AFS_HPUX_ENV
#include <checklist.h>
#else
#if defined(AFS_SGI_ENV)
#include <mntent.h>
#else
#if	defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#ifdef	  AFS_SUN5_ENV
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
#ifndef AFS_NT40_ENV
#include <afs/osi_inode.h>
#endif
#include <afs/cmd.h>
#include <afs/dir.h>
#include <afs/afsutil.h>
#include <afs/fileutil.h>
#include <rx/rx_queue.h>

#include "nfs.h"
#include "lwp.h"
#include <afs/afs_lock.h>
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
pthread_t main_thread;
#endif

extern char cml_version_number[];
static int get_salvage_lock = 0;

struct CmdLine {
   int argc;
   char **argv;
};

static int
TimeStampLogFile(char **logfile)
{
    char *stampSlvgLog;
    struct tm *lt;
    time_t now;

    now = time(0);
    lt = localtime(&now);
    if (asprintf(&stampSlvgLog,
		 "%s.%04d-%02d-%02d.%02d:%02d:%02d",
		 AFSDIR_SERVER_SLVGLOG_FILEPATH,
		 lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour,
		 lt->tm_min, lt->tm_sec) < 0) {
	return ENOMEM;
    }
    *logfile = stampSlvgLog;
    return 0;
}

static int
handleit(struct cmd_syndesc *as, void *arock)
{
    struct CmdLine *cmdline = (struct CmdLine*)arock;
    struct cmd_item *ti;
    char pname[100], *temp;
    afs_int32 seenpart = 0, seenvol = 0;
    VolumeId vid = 0;
    ProgramType pt;

#ifdef FAST_RESTART
    afs_int32  seenany = 0;
#endif

    char *filename = NULL;
    struct logOptions logopts;
    VolumePackageOptions opts;
    struct DiskPartition64 *partP;

    memset(&logopts, 0, sizeof(logopts));

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
	printf
	    ("Exiting immediately without salvage. "
	     "Look into the FileLog to find volumes which really need to be salvaged!\n");
	Exit(0);
    }
#endif /* FAST_RESTART */
    if ((ti = as->parms[0].items)) {	/* -partition */
	seenpart = 1;
	strncpy(pname, ti->data, 100);
    }
    if ((ti = as->parms[1].items)) {	/* -volumeid */
	char *end;
	unsigned long vid_l;
	if (!seenpart) {
	    printf
		("You must also specify '-partition' option with the '-volumeid' option\n");
	    exit(-1);
	}
	seenvol = 1;
	vid_l = strtoul(ti->data, &end, 10);
	if (vid_l >= MAX_AFS_UINT32 || vid_l == ULONG_MAX || *end != '\0') {
	    fprintf(stderr, "salvage: invalid volume id specified; salvage aborted\n");
	    Exit(1);
	}
	vid = (VolumeId)vid_l;
    }
    if (as->parms[2].items)	/* -debug */
	debug = 1;
    if (as->parms[3].items)	/* -nowrite */
	Testing = 1;
    if (as->parms[4].items)	/* -inodes */
	ListInodeOption = 1;
    if (as->parms[5].items || as->parms[21].items)	/* -force, -f */
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
    if ((ti = as->parms[13].items)) {	/* -showsuid */
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

    if ((ti = as->parms[16].items)) {	/* -syslog */
	if (ShowLog) {
	    fprintf(stderr, "Invalid options: -syslog and -showlog are exclusive.\n");
	    Exit(1);
	}
	if ((ti = as->parms[18].items)) {	/* -datelogs */
	    fprintf(stderr, "Invalid option: -syslog and -datelogs are exclusive.\n");
	    Exit(1);
	}
#ifndef HAVE_SYSLOG
	/* Do not silently ignore. */
	fprintf(stderr, "Invalid option: -syslog is not available on this platform.\n");
	Exit(1);
#else
	logopts.lopt_dest = logDest_syslog;
	logopts.lopt_tag = "salvager";

	if ((ti = as->parms[17].items))  /* -syslogfacility */
	    logopts.lopt_facility = atoi(ti->data);
	else
	    logopts.lopt_facility = LOG_DAEMON; /* default value */
#endif
    } else {
	logopts.lopt_dest = logDest_file;

	if ((ti = as->parms[18].items)) {	/* -datelogs */
	    int code = TimeStampLogFile(&filename);
	    if (code != 0) {
	        fprintf(stderr, "Failed to format log file name for -datelogs; code=%d\n", code);
	        Exit(code);
	    }
	    logopts.lopt_filename = filename;
	} else {
	    logopts.lopt_filename = AFSDIR_SERVER_SLVGLOG_FILEPATH;
	}
    }

    OpenLog(&logopts);
    SetupLogSignals();
    free(filename); /* Free string created by -datelogs, if one. */

    Log("%s\n", cml_version_number);
    LogCommandLine(cmdline->argc, cmdline->argv, "SALVAGER", SalvageVersion, "STARTING AFS", Log);

#ifdef FAST_RESTART
    if (ti = as->parms[19].items) {	/* -DontSalvage */
	char *msg =
	    "Exiting immediately without salvage. Look into the FileLog to find volumes which really need to be salvaged!";
	Log("%s\n", msg);
	printf("%s\n", msg);
	Exit(0);
    }
#endif

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

    if (seenvol) {
	pt = volumeSalvager;
    } else {
	pt = salvager;
    }

    VOptDefaults(pt, &opts);
    if (VInitVolumePackage2(pt, &opts)) {
	Log("errors encountered initializing volume package; salvage aborted\n");
	Exit(1);
    }

    /* defer lock until we init volume package */
    if (get_salvage_lock) {
	if (seenvol && AskDAFS()) /* support forceDAFS */
	    ObtainSharedSalvageLock();
	else
	    ObtainSalvageLock();
    }

    /*
     * Ok to defer this as Exit will clean up and no real work is done
     * init'ing volume package
     */
    if (seenvol) {
	char *msg = NULL;
#ifdef AFS_DEMAND_ATTACH_FS
	if (!AskDAFS()) {
	    msg =
		"The DAFS dasalvager cannot be run with a non-DAFS fileserver.  Please use 'salvager'.";
	}
	if (!msg && !as->parms[20].items) {
	    msg =
		"The standalone salvager cannot be run concurrently with a Demand Attach Fileserver.  Please use 'salvageserver -client <partition> <volume id>' to manually schedule volume salvages with the salvageserver (new versions of 'bos salvage' automatically do this for you).  Or, if you insist on using the standalone salvager, add the -forceDAFS flag to your salvager command line.";
	}
#else
	if (AskDAFS()) {
	    msg =
		"The non-DAFS salvager cannot be run with a Demand Attach Fileserver.  Please use 'salvageserver -client <partition> <volume id>' to manually schedule volume salvages with the salvageserver (new versions of 'bos salvage' automatically do this for you).  Or, if you insist on using the standalone salvager, run dasalvager with the -forceDAFS flag.";
	}
#endif

	if (msg) {
	    Log("%s\n", msg);
	    printf("%s\n", msg);
	    Exit(1);
	}
    }

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
	    SalvageFileSys(partP, vid);
	}
    }
    return (0);
}


#ifndef AFS_NT40_ENV
#include "AFS_component_version_number.c"
#endif

int
main(int argc, char **argv)
{
    struct CmdLine cmdline;
    struct cmd_syndesc *ts;
    int err = 0;

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

	/* Get and hold a lock for the duration of the salvage to make sure
	 * that no other salvage runs at the same time.  The routine
	 * VInitVolumePackage2 (called below) makes sure that a file server or
	 * other volume utilities don't interfere with the salvage.
	 */
	get_salvage_lock = 1;
#ifdef AFS_NT40_ENV
    }
#endif

    cmdline.argc = argc;
    cmdline.argv = argv;
    ts = cmd_CreateSyntax("initcmd", handleit, &cmdline, 0, "initialize the program");
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
#elif defined(AFS_DEMAND_ATTACH_FS)
    cmd_Seek(ts, 20); /* skip DontSalvage */
    cmd_AddParm(ts, "-forceDAFS", CMD_FLAG, CMD_OPTIONAL,
		"For Demand Attach Fileserver, permit a manual volume salvage outside of the salvageserver");
#endif /* FAST_RESTART */
    cmd_Seek(ts, 21); /* skip DontSalvage and forceDAFS if needed */
    cmd_AddParm(ts, "-f", CMD_FLAG, CMD_OPTIONAL, "Alias for -force");
    err = cmd_Dispatch(argc, argv);
    Exit(err);
    AFS_UNREACHED(return 0);
}

