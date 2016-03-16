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
 *      Module:		vol-salvage.c
 *      Institution:	The Information Technology Center, Carnegie-Mellon University
 */

/*  1.2 features:
        Correct handling of bad "." and ".." entries.
	Message if volume has "destroyMe" flag set--but doesn't delete yet.
	Link count bug fixed--bug was that vnodeEssence link count was unsigned
	14 bits.  Needs to be signed.

    1.3 features:
        Change to DirHandle stuff to make sure that cache entries are reused at the
	right time (this parallels the file server change, but is not identical).

	Added calls to directory salvager routines; doesn't salvage dir unless debug=1.

    1.4 features:
    	Fixed bug which was causing inode link counts to go bad (thus leaking
	disk blocks).
Vnodes with 0 inode pointers in RW volumes are now deleted.
	An inode with a matching inode number to the vnode is preferred to an
	inode with a higer data version.
	Bug is probably fixed that was causing data version to remain wrong,
	despite assurances from the salvager to the contrary.

    1.5 features:
	Added limited salvaging:  unless ForceSalvage is on, then the volume will
	not be salvaged if the dontSalvage flag is set in the Volume Header.
	The ForceSalvage flag is turned on if an individual volume is salvaged or
	if the file FORCESALVAGE exists	in the partition header of the file system
	being salvaged.  This isn't used for anything but could be set by vfsck.
	A -f flag was also added to force salvage.

    1.6 features:
	It now deletes obsolete volume inodes without complaining

    1.7 features:
	Repairs rw volume headers (again).

    1.8 features:
	Correlates volume headers & inodes correctly, thus preventing occasional deletion
	of read-only volumes...
	No longer forces a directory salvage for volume 144 (which may be a good volume
	at some other site!)
	Some of the messages are cleaned up or made more explicit.  One or two added.
	Logging cleaned up.
	A bug was fixed which forced salvage of read-only volumes without a corresponding
	read/write volume.

    1.9 features:
	When a volume header is recreated, the new name will be "bogus.volume#"

    2.0 features:
	Directory salvaging turned on!!!

    2.1 features:
        Prints warning messages for setuid programs.

    2.2 features:
	Logs missing inode numbers.

    2.3 features:
	    Increments directory version number by 200 (rather than by 1) when it is salvaged, in order to prevent problems due to the fact that a version number can be promised to a workstation before it is written to disk.  If the server crashes, it may have an older version.  Salvaging it could bring the version number up to the same version the workstation believed it already had a call back on.

    2.4 features:
	    Locks the file /vice/vol/salvage.lock before starting.  Aborts if it can't acquire the lock.
	    Time stamps on log entries.
	    Fcntl on stdout to cause all entries to be appended.
	    Problems writing to temporary files are now all detected.
	    Inode summary files are now dynamically named (so that multiple salvagers wouldn't conflict).
	    Some cleanup of error messages.
*/


#include <afsconfig.h>
#include <afs/param.h>


#ifndef AFS_NT40_ENV
#include <sys/param.h>
#include <sys/file.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#endif
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
#include <afs/dir.h>
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
#include "daemon_com_inline.h"
#include "fssync.h"
#include "fssync_inline.h"
#include "volume_inline.h"
#include "salvsync.h"
#include "viceinode.h"
#include "salvage.h"
#include "volinodes.h"		/* header magic number, etc. stuff */
#include "vol-salvage.h"
#include "common.h"
#include "vol_internal.h"
#include <afs/acl.h>
#include <afs/prs_fs.h>

#ifdef FSSYNC_BUILD_CLIENT
#include "vg_cache.h"
#endif

#ifdef AFS_NT40_ENV
#include <pthread.h>
#endif

/*@+fcnmacros +macrofcndecl@*/
#ifdef O_LARGEFILE
#ifdef S_SPLINT_S
extern off64_t afs_lseek(int FD, off64_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)	lseek64(FD, (off64_t) (O), F)
#define afs_stat	stat64
#define afs_fstat	fstat64
#define afs_open	open64
#define afs_fopen	fopen64
#else /* !O_LARGEFILE */
#ifdef S_SPLINT_S
extern off_t afs_lseek(int FD, off_t O, int F);
#endif /*S_SPLINT_S */
#define afs_lseek(FD, O, F)	lseek(FD, (off_t) (O), F)
#define afs_stat	stat
#define afs_fstat	fstat
#define afs_open	open
#define afs_fopen	fopen
#endif /* !O_LARGEFILE */
/*@=fcnmacros =macrofcndecl@*/

#ifdef	AFS_OSF_ENV
extern void *calloc();
#endif
static char *TimeStamp(time_t clock, int precision);


int debug;			/* -d flag */
extern int Testing;		/* -n flag */
int ListInodeOption;		/* -i flag */
int ShowRootFiles;		/* -r flag */
int RebuildDirs;		/* -sal flag */
int Parallel = 4;		/* -para X flag */
int PartsPerDisk = 8;		/* Salvage up to 8 partitions on same disk sequentially */
int forceR = 0;			/* -b flag */
int ShowLog = 0;		/* -showlog flag */
int ShowSuid = 0;		/* -showsuid flag */
int ShowMounts = 0;		/* -showmounts flag */
int orphans = ORPH_IGNORE;	/* -orphans option */
int Showmode = 0;


#ifndef AFS_NT40_ENV
int useSyslog = 0;		/* -syslog flag */
int useSyslogFacility = LOG_DAEMON;	/* -syslogfacility option */
#endif

#ifdef AFS_NT40_ENV
int canfork = 0;
#else
int canfork = 1;
#endif

#define	MAXPARALLEL	32

int OKToZap;			/* -o flag */
int ForceSalvage;		/* If salvage should occur despite the DONT_SALVAGE flag
				 * in the volume header */

FILE *logFile = 0;	/* one of {/usr/afs/logs,/vice/file}/SalvageLog */

#define ROOTINODE	2	/* Root inode of a 4.2 Unix file system
				 * partition */
/**
 * information that is 'global' to a particular salvage job.
 */
struct SalvInfo {
    Device fileSysDevice;    /**< The device number of the current partition
			      *   being salvaged */
    char fileSysPath[9];     /**< The path of the mounted partition currently
                              *   being salvaged, i.e. the directory containing
                              *   the volume headers */
    char *fileSysPathName;   /**< NT needs this to make name pretty log. */
    IHandle_t *VGLinkH;      /**< Link handle for current volume group. */
    int VGLinkH_cnt;         /**< # of references to lnk handle. */
    struct DiskPartition64 *fileSysPartition; /**< Partition being salvaged */

#ifndef AFS_NT40_ENV
    char *fileSysDeviceName; /**< The block device where the file system being
                              *   salvaged was mounted */
    char *filesysfulldev;
#endif
    int VolumeChanged;       /**< Set by any routine which would change the
                              *   volume in a way which would require callbacks
                              *   to be broken if the volume was put back on
                              *   on line by an active file server */

    VolumeDiskData VolInfo;  /**< A copy of the last good or salvaged volume
                              *   header dealt with */

    int nVolumesInInodeFile; /**< Number of read-write volumes summarized */
    int inodeFd;             /**< File descriptor for inode file */

    struct VolumeSummary *volumeSummaryp; /**< Holds all the volumes in a part */
    int nVolumes;            /**< Number of volumes (read-write and read-only)
                              *   in volume summary */
    struct InodeSummary *inodeSummary; /**< contains info on all the relevant
                                        *   inodes */

    struct VnodeInfo vnodeInfo[nVNODECLASSES]; /**< contains info on all of the
                                                *   vnodes in the volume that
                                                *   we are currently looking
                                                *   at */
    int useFSYNC; /**< 0 if the fileserver is unavailable; 1 if we should try
                   *   to contact the fileserver over FSYNC */
};

char *tmpdir = NULL;



/* Forward declarations */
static int IsVnodeOrphaned(struct SalvInfo *salvinfo, VnodeId vnode);
static int AskVolumeSummary(struct SalvInfo *salvinfo,
                            VolumeId singleVolumeNumber);
static void MaybeAskOnline(struct SalvInfo *salvinfo, VolumeId volumeId);
static void AskError(struct SalvInfo *salvinfo, VolumeId volumeId);

#ifdef AFS_DEMAND_ATTACH_FS
static int LockVolume(struct SalvInfo *salvinfo, VolumeId volumeId);
#endif /* AFS_DEMAND_ATTACH_FS */

/* Uniquifier stored in the Inode */
static Unique
IUnique(Unique u)
{
#ifdef	AFS_3DISPARES
    return (u & 0x3fffff);
#else
#if defined(AFS_SGI_EXMAG)
    return (u & SGI_UNIQMASK);
#else
    return (u);
#endif /* AFS_SGI_EXMAG */
#endif
}

static int
BadError(int aerror)
{
    if (aerror == EPERM || aerror == ENXIO || aerror == ENOENT)
	return 1;
    return 0;			/* otherwise may be transient, e.g. EMFILE */
}

#define MAX_ARGS 128
#ifdef AFS_NT40_ENV
char *save_args[MAX_ARGS];
int n_save_args = 0;
extern pthread_t main_thread;
childJob_t myjob = { SALVAGER_MAGIC, NOT_CHILD, "" };
#endif

/**
 * Get the salvage lock if not already held. Hold until process exits.
 *
 * @param[in] locktype READ_LOCK or WRITE_LOCK
 */
static void
_ObtainSalvageLock(int locktype)
{
    struct VLockFile salvageLock;
    int offset = 0;
    int nonblock = 1;
    int code;

    VLockFileInit(&salvageLock, AFSDIR_SERVER_SLVGLOCK_FILEPATH);

    code = VLockFileLock(&salvageLock, offset, locktype, nonblock);
    if (code == EBUSY) {
	fprintf(stderr,
	        "salvager:  There appears to be another salvager running!  "
	        "Aborted.\n");
	Exit(1);
    } else if (code) {
	fprintf(stderr,
	        "salvager:  Error %d trying to acquire salvage lock!  "
	        "Aborted.\n", code);
	Exit(1);
    }
}
void
ObtainSalvageLock(void)
{
    _ObtainSalvageLock(WRITE_LOCK);
}
void
ObtainSharedSalvageLock(void)
{
    _ObtainSalvageLock(READ_LOCK);
}


#ifdef AFS_SGI_XFS_IOPS_ENV
/* Check if the given partition is mounted. For XFS, the root inode is not a
 * constant. So we check the hard way.
 */
int
IsPartitionMounted(char *part)
{
    FILE *mntfp;
    struct mntent *mntent;

    osi_Assert(mntfp = setmntent(MOUNTED, "r"));
    while (mntent = getmntent(mntfp)) {
	if (!strcmp(part, mntent->mnt_dir))
	    break;
    }
    endmntent(mntfp);

    return mntent ? 1 : 1;
}
#endif
/* Check if the given inode is the root of the filesystem. */
#ifndef AFS_SGI_XFS_IOPS_ENV
int
IsRootInode(struct afs_stat *status)
{
    /*
     * The root inode is not a fixed value in XFS partitions. So we need to
     * see if the partition is in the list of mounted partitions. This only
     * affects the SalvageFileSys path, so we check there.
     */
    return (status->st_ino == ROOTINODE);
}
#endif

#ifdef AFS_AIX42_ENV
#ifndef AFS_NAMEI_ENV
/* We don't want to salvage big files filesystems, since we can't put volumes on
 * them.
 */
int
CheckIfBigFilesFS(char *mountPoint, char *devName)
{
    struct superblock fs;
    char name[128];

    if (strncmp(devName, "/dev/", 5)) {
	(void)sprintf(name, "/dev/%s", devName);
    } else {
	(void)strcpy(name, devName);
    }

    if (ReadSuper(&fs, name) < 0) {
	Log("Unable to read superblock. Not salvaging partition %s.\n",
	    mountPoint);
	return 1;
    }
    if (IsBigFilesFileSystem(&fs)) {
	Log("Partition %s is a big files filesystem, not salvaging.\n",
	    mountPoint);
	return 1;
    }
    return 0;
}
#endif
#endif

#ifdef AFS_NT40_ENV
#define HDSTR "\\Device\\Harddisk"
#define HDLEN  (sizeof(HDSTR)-1)	/* Length of "\Device\Harddisk" */
int
SameDisk(struct DiskPartition64 *p1, struct DiskPartition64 *p2)
{
#define RES_LEN 256
    char res1[RES_LEN];
    char res2[RES_LEN];

    static int dowarn = 1;

    if (!QueryDosDevice(p1->devName, res1, RES_LEN - 1))
	return 1;
    if (strncmp(res1, HDSTR, HDLEN)) {
	if (dowarn) {
	    dowarn = 0;
	    Log("WARNING: QueryDosDevice is returning %s, not %s for %s\n",
		res1, HDSTR, p1->devName);
	}
    }
    if (!QueryDosDevice(p2->devName, res2, RES_LEN - 1))
	return 1;
    if (strncmp(res2, HDSTR, HDLEN)) {
	if (dowarn) {
	    dowarn = 0;
	    Log("WARNING: QueryDosDevice is returning %s, not %s for %s\n",
		res2, HDSTR, p2->devName);
	}
    }

    return (0 == _strnicmp(res1, res2, RES_LEN - 1));
}
#else
#define SameDisk(P1, P2) ((P1)->device/PartsPerDisk == (P2)->device/PartsPerDisk)
#endif

/* This assumes that two partitions with the same device number divided by
 * PartsPerDisk are on the same disk.
 */
void
SalvageFileSysParallel(struct DiskPartition64 *partP)
{
    struct job {
	struct DiskPartition64 *partP;
	int pid;		/* Pid for this job */
	int jobnumb;		/* Log file job number */
	struct job *nextjob;	/* Next partition on disk to salvage */
    };
    static struct job *jobs[MAXPARALLEL] = { 0 };	/* Need to zero this */
    struct job *thisjob = 0;
    static int numjobs = 0;
    static int jobcount = 0;
    char buf[1024];
    int wstatus;
    struct job *oldjob;
    int startjob;
    FILE *passLog;
    char logFileName[256];
    int i, j, pid;

    if (partP) {
	/* We have a partition to salvage. Copy it into thisjob */
	thisjob = (struct job *)malloc(sizeof(struct job));
	if (!thisjob) {
	    Log("Can't salvage '%s'. Not enough memory\n", partP->name);
	    return;
	}
	memset(thisjob, 0, sizeof(struct job));
	thisjob->partP = partP;
	thisjob->jobnumb = jobcount;
	jobcount++;
    } else if (jobcount == 0) {
	/* We are asking to wait for all jobs (partp == 0), yet we never
	 * started any.
	 */
	Log("No file system partitions named %s* found; not salvaged\n",
	    VICE_PARTITION_PREFIX);
	return;
    }

    if (debug || Parallel == 1) {
	if (thisjob) {
	    SalvageFileSys(thisjob->partP, 0);
	    free(thisjob);
	}
	return;
    }

    if (thisjob) {
	/* Check to see if thisjob is for a disk that we are already
	 * salvaging. If it is, link it in as the next job to do. The
	 * jobs array has 1 entry per disk being salvages. numjobs is
	 * the total number of disks currently being salvaged. In
	 * order to keep thejobs array compact, when a disk is
	 * completed, the hightest element in the jobs array is moved
	 * down to now open slot.
	 */
	for (j = 0; j < numjobs; j++) {
	    if (SameDisk(jobs[j]->partP, thisjob->partP)) {
		/* On same disk, add it to this list and return */
		thisjob->nextjob = jobs[j]->nextjob;
		jobs[j]->nextjob = thisjob;
		thisjob = 0;
		break;
	    }
	}
    }

    /* Loop until we start thisjob or until all existing jobs are finished */
    while (thisjob || (!partP && (numjobs > 0))) {
	startjob = -1;		/* No new job to start */

	if ((numjobs >= Parallel) || (!partP && (numjobs > 0))) {
	    /* Either the max jobs are running or we have to wait for all
	     * the jobs to finish. In either case, we wait for at least one
	     * job to finish. When it's done, clean up after it.
	     */
	    pid = wait(&wstatus);
	    osi_Assert(pid != -1);
	    for (j = 0; j < numjobs; j++) {	/* Find which job it is */
		if (pid == jobs[j]->pid)
		    break;
	    }
	    osi_Assert(j < numjobs);
	    if (WCOREDUMP(wstatus)) {	/* Say if the job core dumped */
		Log("Salvage of %s core dumped!\n", jobs[j]->partP->name);
	    }

	    numjobs--;		/* job no longer running */
	    oldjob = jobs[j];	/* remember */
	    jobs[j] = jobs[j]->nextjob;	/* Step to next part on same disk */
	    free(oldjob);	/* free the old job */

	    /* If there is another partition on the disk to salvage, then
	     * say we will start it (startjob). If not, then put thisjob there
	     * and say we will start it.
	     */
	    if (jobs[j]) {	/* Another partitions to salvage */
		startjob = j;	/* Will start it */
	    } else {		/* There is not another partition to salvage */
		if (thisjob) {
		    jobs[j] = thisjob;	/* Add thisjob */
		    thisjob = 0;
		    startjob = j;	/* Will start it */
		} else {
		    jobs[j] = jobs[numjobs];	/* Move last job up to this slot */
		    startjob = -1;	/* Don't start it - already running */
		}
	    }
	} else {
	    /* We don't have to wait for a job to complete */
	    if (thisjob) {
		jobs[numjobs] = thisjob;	/* Add this job */
		thisjob = 0;
		startjob = numjobs;	/* Will start it */
	    }
	}

	/* Start up a new salvage job on a partition in job slot "startjob" */
	if (startjob != -1) {
	    if (!Showmode)
		Log("Starting salvage of file system partition %s\n",
		    jobs[startjob]->partP->name);
#ifdef AFS_NT40_ENV
	    /* For NT, we not only fork, but re-exec the salvager. Pass in the
	     * commands and pass the child job number via the data path.
	     */
	    pid =
		nt_SalvagePartition(jobs[startjob]->partP->name,
				    jobs[startjob]->jobnumb);
	    jobs[startjob]->pid = pid;
	    numjobs++;
#else
	    pid = Fork();
	    if (pid) {
		jobs[startjob]->pid = pid;
		numjobs++;
	    } else {
		int fd;

		ShowLog = 0;
		for (fd = 0; fd < 16; fd++)
		    close(fd);
		open(OS_DIRSEP, 0);
		dup2(0, 1);
		dup2(0, 2);
#ifndef AFS_NT40_ENV
		if (useSyslog) {
		    openlog("salvager", LOG_PID, useSyslogFacility);
		} else
#endif
		{
		    (void)afs_snprintf(logFileName, sizeof logFileName,
				       "%s.%d",
				       AFSDIR_SERVER_SLVGLOG_FILEPATH,
				       jobs[startjob]->jobnumb);
		    logFile = afs_fopen(logFileName, "w");
		}
		if (!logFile)
		    logFile = stdout;

		SalvageFileSys1(jobs[startjob]->partP, 0);
		Exit(0);
	    }
#endif
	}
    }				/* while ( thisjob || (!partP && numjobs > 0) ) */

    /* If waited for all jobs to complete, now collect log files and return */
#ifndef AFS_NT40_ENV
    if (!useSyslog)		/* if syslogging - no need to collect */
#endif
	if (!partP) {
	    for (i = 0; i < jobcount; i++) {
		(void)afs_snprintf(logFileName, sizeof logFileName, "%s.%d",
				   AFSDIR_SERVER_SLVGLOG_FILEPATH, i);
		if ((passLog = afs_fopen(logFileName, "r"))) {
		    while (fgets(buf, sizeof(buf), passLog)) {
			fputs(buf, logFile);
		    }
		    fclose(passLog);
		}
		(void)unlink(logFileName);
	    }
	    fflush(logFile);
	}
    return;
}


void
SalvageFileSys(struct DiskPartition64 *partP, VolumeId singleVolumeNumber)
{
    if (!canfork || debug || Fork() == 0) {
	SalvageFileSys1(partP, singleVolumeNumber);
	if (canfork && !debug) {
	    ShowLog = 0;
	    Exit(0);
	}
    } else
	Wait("SalvageFileSys");
}

char *
get_DevName(char *pbuffer, char *wpath)
{
    char pbuf[128], *ptr;
    strcpy(pbuf, pbuffer);
    ptr = (char *)strrchr(pbuf, OS_DIRSEPC);
    if (ptr) {
	*ptr = '\0';
	strcpy(wpath, pbuf);
    } else
	return NULL;
    ptr = (char *)strrchr(pbuffer, OS_DIRSEPC);
    if (ptr) {
	strcpy(pbuffer, ptr + 1);
	return pbuffer;
    } else
	return NULL;
}

void
SalvageFileSys1(struct DiskPartition64 *partP, VolumeId singleVolumeNumber)
{
    char *name, *tdir;
    char inodeListPath[256];
    FILE *inodeFile = NULL;
    static char tmpDevName[100];
    static char wpath[100];
    struct VolumeSummary *vsp, *esp;
    int i, j;
    int code;
    int tries = 0;
    struct SalvInfo l_salvinfo;
    struct SalvInfo *salvinfo = &l_salvinfo;

 retry:
    memset(salvinfo, 0, sizeof(*salvinfo));

    tries++;
    if (inodeFile) {
	fclose(inodeFile);
	inodeFile = NULL;
    }
    if (tries > VOL_MAX_CHECKOUT_RETRIES) {
	Abort("Raced too many times with fileserver restarts while trying to "
	      "checkout/lock volumes; Aborted\n");
    }
#ifdef AFS_DEMAND_ATTACH_FS
    if (tries > 1) {
	/* unlock all previous volume locks, since we're about to lock them
	 * again */
	VLockFileReinit(&partP->volLockFile);
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    salvinfo->fileSysPartition = partP;
    salvinfo->fileSysDevice = salvinfo->fileSysPartition->device;
    salvinfo->fileSysPathName = VPartitionPath(salvinfo->fileSysPartition);

#ifdef AFS_NT40_ENV
    /* Opendir can fail on "C:" but not on "C:\" if C is empty! */
    (void)sprintf(salvinfo->fileSysPath, "%s" OS_DIRSEP, salvinfo->fileSysPathName);
    name = partP->devName;
#else
    strlcpy(salvinfo->fileSysPath, salvinfo->fileSysPathName, sizeof(salvinfo->fileSysPath));
    strcpy(tmpDevName, partP->devName);
    name = get_DevName(tmpDevName, wpath);
    salvinfo->fileSysDeviceName = name;
    salvinfo->filesysfulldev = wpath;
#endif

    if (singleVolumeNumber) {
#ifndef AFS_DEMAND_ATTACH_FS
	/* only non-DAFS locks the partition when salvaging a single volume;
	 * DAFS will lock the individual volumes in the VG */
	VLockPartition(partP->name);
#endif /* !AFS_DEMAND_ATTACH_FS */

	ForceSalvage = 1;

	/* salvageserver already setup fssync conn for us */
	if ((programType != salvageServer) && !VConnectFS()) {
	    Abort("Couldn't connect to file server\n");
	}

	salvinfo->useFSYNC = 1;
	AskOffline(salvinfo, singleVolumeNumber);
#ifdef AFS_DEMAND_ATTACH_FS
	if (LockVolume(salvinfo, singleVolumeNumber)) {
	    goto retry;
	}
#endif /* AFS_DEMAND_ATTACH_FS */

    } else {
	salvinfo->useFSYNC = 0;
	VLockPartition(partP->name);
	if (ForceSalvage) {
	    ForceSalvage = 1;
	} else {
	    ForceSalvage = UseTheForceLuke(salvinfo->fileSysPath);
	}
	if (!Showmode)
	    Log("SALVAGING FILE SYSTEM PARTITION %s (device=%s%s)\n",
		partP->name, name, (Testing ? "(READONLY mode)" : ""));
	if (ForceSalvage)
	    Log("***Forced salvage of all volumes on this partition***\n");
    }


    /*
     * Remove any leftover /vicepa/salvage.inodes.* or /vicepa/salvage.temp.*
     * files
     */
    {
	DIR *dirp;
	struct dirent *dp;

	osi_Assert((dirp = opendir(salvinfo->fileSysPath)) != NULL);
	while ((dp = readdir(dirp))) {
	    if (!strncmp(dp->d_name, "salvage.inodes.", 15)
		|| !strncmp(dp->d_name, "salvage.temp.", 13)) {
		char npath[1024];
		Log("Removing old salvager temp files %s\n", dp->d_name);
		strcpy(npath, salvinfo->fileSysPath);
		strcat(npath, OS_DIRSEP);
		strcat(npath, dp->d_name);
		OS_UNLINK(npath);
	    }
	}
	closedir(dirp);
    }
    tdir = (tmpdir ? tmpdir : salvinfo->fileSysPath);
#ifdef AFS_NT40_ENV
    (void)_putenv("TMP=");	/* If "TMP" is set, then that overrides tdir. */
    (void)strncpy(inodeListPath, _tempnam(tdir, "salvage.inodes."), 255);
#else
    snprintf(inodeListPath, 255, "%s" OS_DIRSEP "salvage.inodes.%s.%d", tdir, name,
	     getpid());
#endif

    inodeFile = fopen(inodeListPath, "w+b");
    if (!inodeFile) {
	Abort("Error %d when creating inode description file %s; not salvaged\n", errno, inodeListPath);
    }
#ifdef AFS_NT40_ENV
    /* Using nt_unlink here since we're really using the delete on close
     * semantics of unlink. In most places in the salvager, we really do
     * mean to unlink the file at that point. Those places have been
     * modified to actually do that so that the NT crt can be used there.
     *
     * jaltman - On NT delete on close cannot be applied to a file while the
     * process has an open file handle that does not have DELETE file
     * access and FILE_SHARE_DELETE.  fopen() calls CreateFile() without
     * delete privileges.  As a result the nt_unlink() call will always
     * fail.
     */
    code = nt_unlink(inodeListPath);
#else
    code = unlink(inodeListPath);
#endif
    if (code < 0) {
	Log("Error %d when trying to unlink %s\n", errno, inodeListPath);
    }

    if (GetInodeSummary(salvinfo, inodeFile, singleVolumeNumber) < 0) {
	fclose(inodeFile);
	if (singleVolumeNumber) {
	    /* the volume group -- let alone the volume -- does not exist,
	     * but we checked it out, so give it back to the fileserver */
	    AskDelete(salvinfo, singleVolumeNumber);
	}
	return;
    }
    salvinfo->inodeFd = fileno(inodeFile);
    if (salvinfo->inodeFd == -1)
	Abort("Temporary file %s is missing...\n", inodeListPath);
    afs_lseek(salvinfo->inodeFd, 0L, SEEK_SET);
    if (ListInodeOption) {
	PrintInodeList(salvinfo);
	if (singleVolumeNumber) {
	    /* We've checked out the volume from the fileserver, and we need
	     * to give it back. We don't know if the volume exists or not,
	     * so we don't know whether to AskOnline or not. Try to determine
	     * if the volume exists by trying to read the volume header, and
	     * AskOnline if it is readable. */
	    MaybeAskOnline(salvinfo, singleVolumeNumber);
	}
	return;
    }
    /* enumerate volumes in the partition.
     * figure out sets of read-only + rw volumes.
     * salvage each set, read-only volumes first, then read-write.
     * Fix up inodes on last volume in set (whether it is read-write
     * or read-only).
     */
    if (GetVolumeSummary(salvinfo, singleVolumeNumber)) {
	goto retry;
    }

    if (singleVolumeNumber) {
	/* If we delete a volume during the salvage, we indicate as such by
	 * setting the volsummary->deleted field. We need to know if we
	 * deleted a volume or not in order to know which volumes to bring
	 * back online after the salvage. If we fork, we will lose this
	 * information, since volsummary->deleted will not get set in the
	 * parent. So, don't fork. */
	canfork = 0;
    }

    for (i = j = 0, vsp = salvinfo->volumeSummaryp, esp = vsp + salvinfo->nVolumes;
	 i < salvinfo->nVolumesInInodeFile; i = j) {
	VolumeId rwvid = salvinfo->inodeSummary[i].RWvolumeId;
	for (j = i;
	     j < salvinfo->nVolumesInInodeFile && salvinfo->inodeSummary[j].RWvolumeId == rwvid;
	     j++) {
	    VolumeId vid = salvinfo->inodeSummary[j].volumeId;
	    struct VolumeSummary *tsp;
	    /* Scan volume list (from partition root directory) looking for the
	     * current rw volume number in the volume list from the inode scan.
	     * If there is one here that is not in the inode volume list,
	     * delete it now. */
	    for (; vsp < esp && (vsp->header.parent < rwvid); vsp++) {
		if (vsp->unused)
		    DeleteExtraVolumeHeaderFile(salvinfo, vsp);
	    }
	    /* Now match up the volume summary info from the root directory with the
	     * entry in the volume list obtained from scanning inodes */
	    salvinfo->inodeSummary[j].volSummary = NULL;
	    for (tsp = vsp; tsp < esp && (tsp->header.parent == rwvid); tsp++) {
		if (tsp->header.id == vid) {
		    salvinfo->inodeSummary[j].volSummary = tsp;
		    tsp->unused = 0;
		    break;
		}
	    }
	}
	/* Salvage the group of volumes (several read-only + 1 read/write)
	 * starting with the current read-only volume we're looking at.
	 */
	SalvageVolumeGroup(salvinfo, &salvinfo->inodeSummary[i], j - i);
    }

    /* Delete any additional volumes that were listed in the partition but which didn't have any corresponding inodes */
    for (; vsp < esp; vsp++) {
	if (vsp->unused)
	    DeleteExtraVolumeHeaderFile(salvinfo, vsp);
    }

    if (!singleVolumeNumber)	/* Remove the FORCESALVAGE file */
	RemoveTheForce(salvinfo->fileSysPath);

    if (!Testing && singleVolumeNumber) {
	int foundSVN = 0;
#ifdef AFS_DEMAND_ATTACH_FS
	/* unlock vol headers so the fs can attach them when we AskOnline */
	VLockFileReinit(&salvinfo->fileSysPartition->volLockFile);
#endif /* AFS_DEMAND_ATTACH_FS */

	/* Step through the volumeSummary list and set all volumes on-line.
	 * Most volumes were taken off-line in GetVolumeSummary.
	 * If a volume was deleted, don't tell the fileserver anything, since
	 * we already told the fileserver the volume was deleted back when we
	 * we destroyed the volume header.
	 * Also, make sure we bring the singleVolumeNumber back online first.
	 */

	for (j = 0; j < salvinfo->nVolumes; j++) {
	    if (salvinfo->volumeSummaryp[j].header.id == singleVolumeNumber) {
		foundSVN = 1;
		if (!salvinfo->volumeSummaryp[j].deleted) {
		    AskOnline(salvinfo, singleVolumeNumber);
		}
	    }
	}

	if (!foundSVN) {
	    /* If singleVolumeNumber is not in our volumeSummary, it means that
	     * at least one other volume in the VG is on the partition, but the
	     * RW volume is not. We've already AskOffline'd it by now, though,
	     * so make sure we don't still have the volume checked out. */
	    AskDelete(salvinfo, singleVolumeNumber);
	}

	for (j = 0; j < salvinfo->nVolumes; j++) {
	    if (salvinfo->volumeSummaryp[j].header.id != singleVolumeNumber) {
		if (!salvinfo->volumeSummaryp[j].deleted) {
		    AskOnline(salvinfo, salvinfo->volumeSummaryp[j].header.id);
		}
	    }
	}
    } else {
	if (!Showmode)
	    Log("SALVAGING OF PARTITION %s%s COMPLETED\n",
		salvinfo->fileSysPartition->name, (Testing ? " (READONLY mode)" : ""));
    }

    fclose(inodeFile);		/* SalvageVolumeGroup was the last which needed it. */
}

void
DeleteExtraVolumeHeaderFile(struct SalvInfo *salvinfo, struct VolumeSummary *vsp)
{
    char path[64];
    char filename[VMAXPATHLEN];

    if (vsp->deleted) {
	return;
    }

    VolumeExternalName_r(vsp->header.id, filename, sizeof(filename));
    sprintf(path, "%s" OS_DIRSEP "%s", salvinfo->fileSysPath, filename);

    if (!Showmode)
	Log("The volume header file %s is not associated with any actual data (%sdeleted)\n", path, (Testing ? "would have been " : ""));
    if (!Testing) {
	afs_int32 code;
	code = VDestroyVolumeDiskHeader(salvinfo->fileSysPartition, vsp->header.id, vsp->header.parent);
	if (code) {
	    Log("Error %ld destroying volume disk header for volume %lu\n",
	        afs_printable_int32_ld(code),
	        afs_printable_uint32_lu(vsp->header.id));
	}

	/* make sure we actually delete the header file; ENOENT
	 * is fine, since VDestroyVolumeDiskHeader probably already
	 * unlinked it */
	if (unlink(path) && errno != ENOENT) {
	    Log("Unable to unlink %s (errno = %d)\n", path, errno);
	}
	if (salvinfo->useFSYNC) {
	    AskDelete(salvinfo, vsp->header.id);
	}
	vsp->deleted = 1;
    }
}

int
CompareInodes(const void *_p1, const void *_p2)
{
    const struct ViceInodeInfo *p1 = _p1;
    const struct ViceInodeInfo *p2 = _p2;
    if (p1->u.vnode.vnodeNumber == INODESPECIAL
	|| p2->u.vnode.vnodeNumber == INODESPECIAL) {
	VolumeId p1rwid, p2rwid;
	p1rwid =
	    (p1->u.vnode.vnodeNumber ==
	     INODESPECIAL ? p1->u.special.parentId : p1->u.vnode.volumeId);
	p2rwid =
	    (p2->u.vnode.vnodeNumber ==
	     INODESPECIAL ? p2->u.special.parentId : p2->u.vnode.volumeId);
	if (p1rwid < p2rwid)
	    return -1;
	if (p1rwid > p2rwid)
	    return 1;
	if (p1->u.vnode.vnodeNumber == INODESPECIAL
	    && p2->u.vnode.vnodeNumber == INODESPECIAL) {
	    if (p1->u.vnode.volumeId == p2->u.vnode.volumeId)
		return (p1->u.special.type < p2->u.special.type ? -1 : 1);
	    if (p1->u.vnode.volumeId == p1rwid)
		return -1;
	    if (p2->u.vnode.volumeId == p2rwid)
		return 1;
	    return (p1->u.vnode.volumeId < p2->u.vnode.volumeId ? -1 : 1);
	}
	if (p1->u.vnode.vnodeNumber != INODESPECIAL)
	    return (p2->u.vnode.volumeId == p2rwid ? 1 : -1);
	return (p1->u.vnode.volumeId == p1rwid ? -1 : 1);
    }
    if (p1->u.vnode.volumeId < p2->u.vnode.volumeId)
	return -1;
    if (p1->u.vnode.volumeId > p2->u.vnode.volumeId)
	return 1;
    if (p1->u.vnode.vnodeNumber < p2->u.vnode.vnodeNumber)
	return -1;
    if (p1->u.vnode.vnodeNumber > p2->u.vnode.vnodeNumber)
	return 1;
    /* The following tests are reversed, so that the most desirable
     * of several similar inodes comes first */
    if (p1->u.vnode.vnodeUniquifier > p2->u.vnode.vnodeUniquifier) {
#ifdef	AFS_3DISPARES
	if (p1->u.vnode.vnodeUniquifier > 3775414 /* 90% of 4.2M */  &&
	    p2->u.vnode.vnodeUniquifier < 419490 /* 10% of 4.2M */ )
	    return 1;
#endif
#ifdef	AFS_SGI_EXMAG
	if (p1->u.vnode.vnodeUniquifier > 15099494 /* 90% of 16M */  &&
	    p2->u.vnode.vnodeUniquifier < 1677721 /* 10% of 16M */ )
	    return 1;
#endif
	return -1;
    }
    if (p1->u.vnode.vnodeUniquifier < p2->u.vnode.vnodeUniquifier) {
#ifdef	AFS_3DISPARES
	if (p2->u.vnode.vnodeUniquifier > 3775414 /* 90% of 4.2M */  &&
	    p1->u.vnode.vnodeUniquifier < 419490 /* 10% of 4.2M */ )
	    return -1;
#endif
#ifdef	AFS_SGI_EXMAG
	if (p2->u.vnode.vnodeUniquifier > 15099494 /* 90% of 16M */  &&
	    p1->u.vnode.vnodeUniquifier < 1677721 /* 10% of 16M */ )
	    return 1;
#endif
	return 1;
    }
    if (p1->u.vnode.inodeDataVersion > p2->u.vnode.inodeDataVersion) {
#ifdef	AFS_3DISPARES
	if (p1->u.vnode.inodeDataVersion > 1887437 /* 90% of 2.1M */  &&
	    p2->u.vnode.inodeDataVersion < 209716 /* 10% of 2.1M */ )
	    return 1;
#endif
#ifdef	AFS_SGI_EXMAG
	if (p1->u.vnode.inodeDataVersion > 15099494 /* 90% of 16M */  &&
	    p2->u.vnode.inodeDataVersion < 1677721 /* 10% of 16M */ )
	    return 1;
#endif
	return -1;
    }
    if (p1->u.vnode.inodeDataVersion < p2->u.vnode.inodeDataVersion) {
#ifdef	AFS_3DISPARES
	if (p2->u.vnode.inodeDataVersion > 1887437 /* 90% of 2.1M */  &&
	    p1->u.vnode.inodeDataVersion < 209716 /* 10% of 2.1M */ )
	    return -1;
#endif
#ifdef	AFS_SGI_EXMAG
	if (p2->u.vnode.inodeDataVersion > 15099494 /* 90% of 16M */  &&
	    p1->u.vnode.inodeDataVersion < 1677721 /* 10% of 16M */ )
	    return 1;
#endif
	return 1;
    }
    return 0;
}

void
CountVolumeInodes(struct ViceInodeInfo *ip, int maxInodes,
		  struct InodeSummary *summary)
{
    VolumeId volume = ip->u.vnode.volumeId;
    VolumeId rwvolume = volume;
    int n, nSpecial;
    Unique maxunique;
    n = nSpecial = 0;
    maxunique = 0;
    while (maxInodes-- && volume == ip->u.vnode.volumeId) {
	n++;
	if (ip->u.vnode.vnodeNumber == INODESPECIAL) {
	    nSpecial++;
	    rwvolume = ip->u.special.parentId;
	    /* This isn't quite right, as there could (in error) be different
	     * parent inodes in different special vnodes */
	} else {
	    if (maxunique < ip->u.vnode.vnodeUniquifier)
		maxunique = ip->u.vnode.vnodeUniquifier;
	}
	ip++;
    }
    summary->volumeId = volume;
    summary->RWvolumeId = rwvolume;
    summary->nInodes = n;
    summary->nSpecialInodes = nSpecial;
    summary->maxUniquifier = maxunique;
}

int
OnlyOneVolume(struct ViceInodeInfo *inodeinfo, afs_uint32 singleVolumeNumber, void *rock)
{
    if (inodeinfo->u.vnode.vnodeNumber == INODESPECIAL)
	return (inodeinfo->u.special.parentId == singleVolumeNumber);
    return (inodeinfo->u.vnode.volumeId == singleVolumeNumber);
}

/* GetInodeSummary
 *
 * Collect list of inodes in file named by path. If a truly fatal error,
 * unlink the file and abort. For lessor errors, return -1. The file will
 * be unlinked by the caller.
 */
int
GetInodeSummary(struct SalvInfo *salvinfo, FILE *inodeFile, VolumeId singleVolumeNumber)
{
    struct afs_stat status;
    int forceSal, err;
    int code;
    struct ViceInodeInfo *ip, *ip_save;
    struct InodeSummary summary;
    char summaryFileName[50];
    FILE *summaryFile;
#ifdef AFS_NT40_ENV
    char *dev = salvinfo->fileSysPath;
    char *wpath = salvinfo->fileSysPath;
#else
    char *dev = salvinfo->fileSysDeviceName;
    char *wpath = salvinfo->filesysfulldev;
#endif
    char *part = salvinfo->fileSysPath;
    char *tdir;
    int i;
    int retcode = 0;
    int deleted = 0;

    /* This file used to come from vfsck; cobble it up ourselves now... */
    if ((err =
	 ListViceInodes(dev, salvinfo->fileSysPath, inodeFile,
			singleVolumeNumber ? OnlyOneVolume : 0,
			singleVolumeNumber, &forceSal, forceR, wpath, NULL)) < 0) {
	if (err == -2) {
	    Log("*** I/O error %d when writing a tmp inode file; Not salvaged %s ***\nIncrease space on partition or use '-tmpdir'\n", errno, dev);
	    retcode = -1;
	    goto error;
	}
	Abort("Unable to get inodes for \"%s\"; not salvaged\n", dev);
    }
    if (forceSal && !ForceSalvage) {
	Log("***Forced salvage of all volumes on this partition***\n");
	ForceSalvage = 1;
    }
    fseek(inodeFile, 0L, SEEK_SET);
    salvinfo->inodeFd = fileno(inodeFile);
    if (salvinfo->inodeFd == -1 || afs_fstat(salvinfo->inodeFd, &status) == -1) {
	Abort("No inode description file for \"%s\"; not salvaged\n", dev);
    }
    tdir = (tmpdir ? tmpdir : part);
#ifdef AFS_NT40_ENV
    (void)_putenv("TMP=");	/* If "TMP" is set, then that overrides tdir. */
    (void)strcpy(summaryFileName, _tempnam(tdir, "salvage.temp."));
#else
    (void)afs_snprintf(summaryFileName, sizeof summaryFileName,
		       "%s" OS_DIRSEP "salvage.temp.%d", tdir, getpid());
#endif
    summaryFile = afs_fopen(summaryFileName, "a+");
    if (summaryFile == NULL) {
	Abort("Unable to create inode summary file\n");
    }

#ifdef AFS_NT40_ENV
    /* Using nt_unlink here since we're really using the delete on close
     * semantics of unlink. In most places in the salvager, we really do
     * mean to unlink the file at that point. Those places have been
     * modified to actually do that so that the NT crt can be used there.
     *
     * jaltman - As commented elsewhere, this cannot work because fopen()
     * does not open files with DELETE and FILE_SHARE_DELETE.
     */
    code = nt_unlink(summaryFileName);
#else
    code = unlink(summaryFileName);
#endif
    if (code < 0) {
	Log("Error %d when trying to unlink %s\n", errno, summaryFileName);
    }

    if (!canfork || debug || Fork() == 0) {
	int nInodes;
	unsigned long st_size=(unsigned long) status.st_size;
	nInodes = st_size / sizeof(struct ViceInodeInfo);
	if (nInodes == 0) {
	    fclose(summaryFile);
	    if (!singleVolumeNumber)	/* Remove the FORCESALVAGE file */
		RemoveTheForce(salvinfo->fileSysPath);
	    else {
		struct VolumeSummary *vsp;
		int i;
		int foundSVN = 0;

		GetVolumeSummary(salvinfo, singleVolumeNumber);

		for (i = 0, vsp = salvinfo->volumeSummaryp; i < salvinfo->nVolumes; i++) {
		    if (vsp->unused) {
			if (vsp->header.id == singleVolumeNumber) {
			    foundSVN = 1;
			}
			DeleteExtraVolumeHeaderFile(salvinfo, vsp);
		    }
		}

		if (!foundSVN) {
		    if (Testing) {
			MaybeAskOnline(salvinfo, singleVolumeNumber);
		    } else {
			/* make sure we get rid of stray .vol headers, even if
			 * they're not in our volume summary (might happen if
			 * e.g. something else created them and they're not in the
			 * fileserver VGC) */
			VDestroyVolumeDiskHeader(salvinfo->fileSysPartition,
			                         singleVolumeNumber, 0 /*parent*/);
			AskDelete(salvinfo, singleVolumeNumber);
		    }
		}
	    }
	    Log("%s vice inodes on %s; not salvaged\n",
		singleVolumeNumber ? "No applicable" : "No", dev);
	    retcode = -1;
	    deleted = 1;
	    goto error;
	}
	ip = (struct ViceInodeInfo *)malloc(nInodes*sizeof(struct ViceInodeInfo));
	if (ip == NULL) {
	    fclose(summaryFile);
	    Abort
		("Unable to allocate enough space to read inode table; %s not salvaged\n",
		 dev);
	}
	if (read(salvinfo->inodeFd, ip, st_size) != st_size) {
	    fclose(summaryFile);
	    Abort("Unable to read inode table; %s not salvaged\n", dev);
	}
	qsort(ip, nInodes, sizeof(struct ViceInodeInfo), CompareInodes);
	if (afs_lseek(salvinfo->inodeFd, 0, SEEK_SET) == -1
	    || write(salvinfo->inodeFd, ip, st_size) != st_size) {
	    fclose(summaryFile);
	    Abort("Unable to rewrite inode table; %s not salvaged\n", dev);
	}
	summary.index = 0;
	ip_save = ip;
	while (nInodes) {
	    CountVolumeInodes(ip, nInodes, &summary);
	    if (fwrite(&summary, sizeof(summary), 1, summaryFile) != 1) {
		Log("Difficulty writing summary file (errno = %d); %s not salvaged\n", errno, dev);
		fclose(summaryFile);
		retcode = -1;
		goto error;
	    }
	    summary.index += (summary.nInodes);
	    nInodes -= summary.nInodes;
	    ip += summary.nInodes;
	}
	free(ip_save);
	ip = ip_save = NULL;
	/* Following fflush is not fclose, because if it was debug mode would not work */
	if (fflush(summaryFile) == EOF || fsync(fileno(summaryFile)) == -1) {
	    Log("Unable to write summary file (errno = %d); %s not salvaged\n", errno, dev);
	    fclose(summaryFile);
	    retcode = -1;
	    goto error;
	}
	if (canfork && !debug) {
	    ShowLog = 0;
	    Exit(0);
	}
    } else {
	if (Wait("Inode summary") == -1) {
	    fclose(summaryFile);
	    Exit(1);		/* salvage of this partition aborted */
	}
    }
    osi_Assert(afs_fstat(fileno(summaryFile), &status) != -1);
    if (status.st_size != 0) {
	int ret;
	unsigned long st_status=(unsigned long)status.st_size;
	salvinfo->inodeSummary = (struct InodeSummary *)malloc(st_status);
	osi_Assert(salvinfo->inodeSummary != NULL);
	/* For GNU we need to do lseek to get the file pointer moved. */
	osi_Assert(afs_lseek(fileno(summaryFile), 0, SEEK_SET) == 0);
	ret = read(fileno(summaryFile), salvinfo->inodeSummary, st_status);
	osi_Assert(ret == st_status);
    }
    salvinfo->nVolumesInInodeFile =(unsigned long)(status.st_size) / sizeof(struct InodeSummary);
    for (i = 0; i < salvinfo->nVolumesInInodeFile; i++) {
	salvinfo->inodeSummary[i].volSummary = NULL;
    }
    Log("%d nVolumesInInodeFile %lu \n",salvinfo->nVolumesInInodeFile,(unsigned long)(status.st_size));
    fclose(summaryFile);

 error:
    if (retcode && singleVolumeNumber && !deleted) {
	AskError(salvinfo, singleVolumeNumber);
    }

    return retcode;
}

/* Comparison routine for volume sort.
   This is setup so that a read-write volume comes immediately before
   any read-only clones of that volume */
int
CompareVolumes(const void *_p1, const void *_p2)
{
    const struct VolumeSummary *p1 = _p1;
    const struct VolumeSummary *p2 = _p2;
    if (p1->header.parent != p2->header.parent)
	return p1->header.parent < p2->header.parent ? -1 : 1;
    if (p1->header.id == p1->header.parent)	/* p1 is rw volume */
	return -1;
    if (p2->header.id == p2->header.parent)	/* p2 is rw volume */
	return 1;
    return p1->header.id < p2->header.id ? -1 : 1;	/* Both read-only */
}

/**
 * Gleans volumeSummary information by asking the fileserver
 *
 * @param[in] singleVolumeNumber  the volume we're salvaging. 0 if we're
 *                                salvaging a whole partition
 *
 * @return whether we obtained the volume summary information or not
 *  @retval 0  success; we obtained the volume summary information
 *  @retval -1 we raced with a fileserver restart; volume locks and checkout
 *             must be retried
 *  @retval 1  we did not get the volume summary information; either the
 *             fileserver responded with an error, or we are not supposed to
 *             ask the fileserver for the information (e.g. we are salvaging
 *             the entire partition or we are not the salvageserver)
 *
 * @note for non-DAFS, always returns 1
 */
static int
AskVolumeSummary(struct SalvInfo *salvinfo, VolumeId singleVolumeNumber)
{
    afs_int32 code = 1;
#if defined(FSSYNC_BUILD_CLIENT) && defined(AFS_DEMAND_ATTACH_FS)
    if (programType == salvageServer) {
	if (singleVolumeNumber) {
	    FSSYNC_VGQry_response_t q_res;
	    SYNC_response res;
	    struct VolumeSummary *vsp;
	    int i;
	    struct VolumeDiskHeader diskHdr;

	    memset(&res, 0, sizeof(res));

	    code = FSYNC_VGCQuery(salvinfo->fileSysPartition->name, singleVolumeNumber, &q_res, &res);

	    /*
	     * We must wait for the partition to finish scanning before
	     * can continue, since we will not know if we got the entire
	     * VG membership unless the partition is fully scanned.
	     * We could, in theory, just scan the partition ourselves if
	     * the VG cache is not ready, but we would be doing the exact
	     * same scan the fileserver is doing; it will almost always
	     * be faster to wait for the fileserver. The only exceptions
	     * are if the partition does not take very long to scan, and
	     * in that case it's fast either way, so who cares?
	     */
	    if (code == SYNC_FAILED && res.hdr.reason == FSYNC_PART_SCANNING) {
		Log("waiting for fileserver to finish scanning partition %s...\n",
		    salvinfo->fileSysPartition->name);

		for (i = 1; code == SYNC_FAILED && res.hdr.reason == FSYNC_PART_SCANNING; i++) {
		    /* linearly ramp up from 1 to 10 seconds; nothing fancy,
		     * just so small partitions don't need to wait over 10
		     * seconds every time, and large partitions are generally
		     * polled only once every ten seconds. */
		    sleep((i > 10) ? (i = 10) : i);

		    code = FSYNC_VGCQuery(salvinfo->fileSysPartition->name, singleVolumeNumber, &q_res, &res);
		}
	    }

	    if (code == SYNC_FAILED && res.hdr.reason == FSYNC_UNKNOWN_VOLID) {
		/* This can happen if there's no header for the volume
		 * we're salvaging, or no headers exist for the VG (if
		 * we're salvaging an RW). Act as if we got a response
		 * with no VG members. The headers may be created during
		 * salvaging, if there are inodes in this VG. */
		code = 0;
		memset(&q_res, 0, sizeof(q_res));
		q_res.rw = singleVolumeNumber;
	    }

	    if (code) {
		Log("fileserver refused VGCQuery request for volume %lu on "
		    "partition %s, code %ld reason %ld\n",
		    afs_printable_uint32_lu(singleVolumeNumber),
		    salvinfo->fileSysPartition->name,
		    afs_printable_int32_ld(code),
		    afs_printable_int32_ld(res.hdr.reason));
		goto done;
	    }

	    if (q_res.rw != singleVolumeNumber) {
		Log("fileserver requested salvage of clone %lu; scheduling salvage of volume group %lu...\n",
		    afs_printable_uint32_lu(singleVolumeNumber),
		    afs_printable_uint32_lu(q_res.rw));
#ifdef SALVSYNC_BUILD_CLIENT
		if (SALVSYNC_LinkVolume(q_res.rw,
		                       singleVolumeNumber,
		                       salvinfo->fileSysPartition->name,
		                       NULL) != SYNC_OK) {
		    Log("schedule request failed\n");
		}
#endif /* SALVSYNC_BUILD_CLIENT */
		Exit(SALSRV_EXIT_VOLGROUP_LINK);
	    }

	    salvinfo->volumeSummaryp = calloc(VOL_VG_MAX_VOLS, sizeof(struct VolumeSummary));
	    osi_Assert(salvinfo->volumeSummaryp != NULL);

	    salvinfo->nVolumes = 0;
	    vsp = salvinfo->volumeSummaryp;

	    for (i = 0; i < VOL_VG_MAX_VOLS; i++) {
		char name[VMAXPATHLEN];

		if (!q_res.children[i]) {
		    continue;
		}

		/* AskOffline for singleVolumeNumber was called much earlier */
		if (q_res.children[i] != singleVolumeNumber) {
		    AskOffline(salvinfo, q_res.children[i]);
		    if (LockVolume(salvinfo, q_res.children[i])) {
			/* need to retry */
			return -1;
		    }
		}

		code = VReadVolumeDiskHeader(q_res.children[i], salvinfo->fileSysPartition, &diskHdr);
		if (code) {
		    Log("Cannot read header for %lu; trying to salvage group anyway\n",
		        afs_printable_uint32_lu(q_res.children[i]));
		    code = 0;
		    continue;
		}

		DiskToVolumeHeader(&vsp->header, &diskHdr);
		VolumeExternalName_r(q_res.children[i], name, sizeof(name));
		vsp->unused = 1;
		salvinfo->nVolumes++;
		vsp++;
	    }

	    qsort(salvinfo->volumeSummaryp, salvinfo->nVolumes, sizeof(struct VolumeSummary),
	          CompareVolumes);
	}
      done:
	if (code) {
	    Log("Cannot get volume summary from fileserver; falling back to scanning "
	        "entire partition\n");
	}
    }
#endif /* FSSYNC_BUILD_CLIENT && AFS_DEMAND_ATTACH_FS */
    return code;
}

/**
 * count how many volume headers are found by VWalkVolumeHeaders.
 *
 * @param[in] dp   the disk partition (unused)
 * @param[in] name full path to the .vol header (unused)
 * @param[in] hdr  the header data (unused)
 * @param[in] last whether this is the last try or not (unused)
 * @param[in] rock actually an afs_int32*; the running count of how many
 *                 volumes we have found
 *
 * @retval 0 always
 */
static int
CountHeader(struct DiskPartition64 *dp, const char *name,
            struct VolumeDiskHeader *hdr, int last, void *rock)
{
    afs_int32 *nvols = (afs_int32 *)rock;
    (*nvols)++;
    return 0;
}

/**
 * parameters to pass to the VWalkVolumeHeaders callbacks when recording volume
 * data.
 */
struct SalvageScanParams {
    VolumeId singleVolumeNumber; /**< 0 for a partition-salvage, otherwise the
                                  * vol id of the VG we're salvaging */
    struct VolumeSummary *vsp;   /**< ptr to the current volume summary object
                                  * we're filling in */
    afs_int32 nVolumes;          /**< # of vols we've encountered */
    afs_int32 totalVolumes;      /**< max # of vols we should encounter (the
                                  * # of vols we've alloc'd memory for) */
    int retry;  /**< do we need to retry vol lock/checkout? */
    struct SalvInfo *salvinfo; /**< salvage job info */
};

/**
 * records volume summary info found from VWalkVolumeHeaders.
 *
 * Found volumes are also taken offline if they are in the specific volume
 * group we are looking for.
 *
 * @param[in] dp   the disk partition
 * @param[in] name full path to the .vol header
 * @param[in] hdr  the header data
 * @param[in] last 1 if this is the last try to read the header, 0 otherwise
 * @param[in] rock actually a struct SalvageScanParams*, containing the
 *                 information needed to record the volume summary data
 *
 * @return operation status
 *  @retval 0  success
 *  @retval -1 volume locking raced with fileserver restart; checking out
 *             and locking volumes needs to be retried
 *  @retval 1  volume header is mis-named and should be deleted
 */
static int
RecordHeader(struct DiskPartition64 *dp, const char *name,
             struct VolumeDiskHeader *hdr, int last, void *rock)
{
    char nameShouldBe[64];
    struct SalvageScanParams *params;
    struct VolumeSummary summary;
    VolumeId singleVolumeNumber;
    struct SalvInfo *salvinfo;

    params = (struct SalvageScanParams *)rock;

    memset(&summary, 0, sizeof(summary));

    singleVolumeNumber = params->singleVolumeNumber;
    salvinfo = params->salvinfo;

    DiskToVolumeHeader(&summary.header, hdr);

    if (singleVolumeNumber && summary.header.id == singleVolumeNumber
        && summary.header.parent != singleVolumeNumber) {

	if (programType == salvageServer) {
#ifdef SALVSYNC_BUILD_CLIENT
	    Log("fileserver requested salvage of clone %u; scheduling salvage of volume group %u...\n",
	        summary.header.id, summary.header.parent);
	    if (SALVSYNC_LinkVolume(summary.header.parent,
		                    summary.header.id,
		                    dp->name,
		                    NULL) != SYNC_OK) {
		Log("schedule request failed\n");
	    }
#endif
	    Exit(SALSRV_EXIT_VOLGROUP_LINK);

	} else {
	    Log("%u is a read-only volume; not salvaged\n",
	        singleVolumeNumber);
	    Exit(1);
	}
    }

    if (!singleVolumeNumber || summary.header.id == singleVolumeNumber
	|| summary.header.parent == singleVolumeNumber) {

	/* check if the header file is incorrectly named */
	int badname = 0;
	const char *base = strrchr(name, OS_DIRSEPC);
	if (base) {
	    base++;
	} else {
	    base = name;
	}

	(void)afs_snprintf(nameShouldBe, sizeof nameShouldBe,
	                   VFORMAT, afs_printable_uint32_lu(summary.header.id));


	if (strcmp(nameShouldBe, base)) {
	    /* .vol file has wrong name; retry/delete */
	    badname = 1;
	}

	if (!badname || last) {
	    /* only offline the volume if the header is good, or if this is
	     * the last try looking at it; avoid AskOffline'ing the same vol
	     * multiple times */

	    if (singleVolumeNumber
	        && summary.header.id != singleVolumeNumber) {
		/* don't offline singleVolumeNumber; we already did that
		 * earlier */

	        AskOffline(salvinfo, summary.header.id);

#ifdef AFS_DEMAND_ATTACH_FS
		if (!badname) {
		    /* don't lock the volume if the header is bad, since we're
		     * about to delete it anyway. */
		    if (LockVolume(salvinfo, summary.header.id)) {
			params->retry = 1;
			return -1;
		    }
		}
#endif /* AFS_DEMAND_ATTACH_FS */
	    }
	}
	if (badname) {
	    if (last && !Showmode) {
		Log("Volume header file %s is incorrectly named (should be %s "
		    "not %s); %sdeleted (it will be recreated later, if "
		    "necessary)\n", name, nameShouldBe, base,
		    (Testing ? "it would have been " : ""));
	    }
	    return 1;
	}

	summary.unused = 1;
	params->nVolumes++;

	if (params->nVolumes > params->totalVolumes) {
	    /* We found more volumes than we found on the first partition walk;
	     * apparently something created a volume while we were
	     * partition-salvaging, or we found more than 20 vols when salvaging a
	     * particular volume. Abort if we detect this, since other programs
	     * supposed to not touch the partition while it is partition-salvaging,
	     * and we shouldn't find more than 20 vols in a VG.
	     */
	    Abort("Found %ld vol headers, but should have found at most %ld! "
	          "Make sure the volserver/fileserver are not running at the "
		  "same time as a partition salvage\n",
		  afs_printable_int32_ld(params->nVolumes),
		  afs_printable_int32_ld(params->totalVolumes));
	}

	memcpy(params->vsp, &summary, sizeof(summary));
	params->vsp++;
    }

    return 0;
}

/**
 * possibly unlinks bad volume headers found from VWalkVolumeHeaders.
 *
 * If the header could not be read in at all, the header is always unlinked.
 * If instead RecordHeader said the header was bad (that is, the header file
 * is mis-named), we only unlink if we are doing a partition salvage, as
 * opposed to salvaging a specific volume group.
 *
 * @param[in] dp   the disk partition
 * @param[in] name full path to the .vol header
 * @param[in] hdr  header data, or NULL if the header could not be read
 * @param[in] rock actually a struct SalvageScanParams*, with some information
 *                 about the scan
 */
static void
UnlinkHeader(struct DiskPartition64 *dp, const char *name,
             struct VolumeDiskHeader *hdr, void *rock)
{
    struct SalvageScanParams *params;
    int dounlink = 0;

    params = (struct SalvageScanParams *)rock;

    if (!hdr) {
	/* no header; header is too bogus to read in at all */
	if (!Showmode) {
	    Log("%s is not a legitimate volume header file; %sdeleted\n", name, (Testing ? "it would have been " : ""));
	}
	if (!Testing) {
	    dounlink = 1;
	}

    } else if (!params->singleVolumeNumber) {
	/* We were able to read in a header, but RecordHeader said something
	 * was wrong with it. We only unlink those if we are doing a partition
	 * salvage. */
	if (!Testing) {
	    dounlink = 1;
	}
    }

    if (dounlink && unlink(name)) {
	Log("Error %d while trying to unlink %s\n", errno, name);
    }
}

/**
 * Populates salvinfo->volumeSummaryp with volume summary information, either by asking
 * the fileserver for VG information, or by scanning the /vicepX partition.
 *
 * @param[in] singleVolumeNumber  the volume ID of the single volume group we
 *                                are salvaging, or 0 if this is a partition
 *                                salvage
 *
 * @return operation status
 *  @retval 0  success
 *  @retval -1 we raced with a fileserver restart; checking out and locking
 *             volumes must be retried
 */
int
GetVolumeSummary(struct SalvInfo *salvinfo, VolumeId singleVolumeNumber)
{
    afs_int32 nvols = 0;
    struct SalvageScanParams params;
    int code;

    code = AskVolumeSummary(salvinfo, singleVolumeNumber);
    if (code == 0) {
	/* we successfully got the vol information from the fileserver; no
	 * need to scan the partition */
	return 0;
    }
    if (code < 0) {
	/* we need to retry volume checkout */
	return code;
    }

    if (!singleVolumeNumber) {
	/* Count how many volumes we have in /vicepX */
	code = VWalkVolumeHeaders(salvinfo->fileSysPartition, salvinfo->fileSysPath, CountHeader,
	                          NULL, &nvols);
	if (code < 0) {
	    Abort("Can't read directory %s; not salvaged\n", salvinfo->fileSysPath);
	}
	if (!nvols)
	    nvols = 1;
    } else {
	nvols = VOL_VG_MAX_VOLS;
    }

    salvinfo->volumeSummaryp = calloc(nvols, sizeof(struct VolumeSummary));
    osi_Assert(salvinfo->volumeSummaryp != NULL);

    params.singleVolumeNumber = singleVolumeNumber;
    params.vsp = salvinfo->volumeSummaryp;
    params.nVolumes = 0;
    params.totalVolumes = nvols;
    params.retry = 0;
    params.salvinfo = salvinfo;

    /* walk the partition directory of volume headers and record the info
     * about them; unlinking invalid headers */
    code = VWalkVolumeHeaders(salvinfo->fileSysPartition, salvinfo->fileSysPath, RecordHeader,
                              UnlinkHeader, &params);
    if (params.retry) {
	/* we apparently need to retry checking-out/locking volumes */
	return -1;
    }
    if (code < 0) {
	Abort("Failed to get volume header summary\n");
    }
    salvinfo->nVolumes = params.nVolumes;

    qsort(salvinfo->volumeSummaryp, salvinfo->nVolumes, sizeof(struct VolumeSummary),
	  CompareVolumes);

    return 0;
}

/* Find the link table. This should be associated with the RW volume, even
 * if there is only an RO volume at this site.
 */
Inode
FindLinkHandle(struct InodeSummary *isp, int nVols,
	       struct ViceInodeInfo *allInodes)
{
    int i, j;
    struct ViceInodeInfo *ip;

    for (i = 0; i < nVols; i++) {
	ip = allInodes + isp[i].index;
	for (j = 0; j < isp[i].nSpecialInodes; j++) {
	    if (ip[j].u.special.volumeId == isp->RWvolumeId &&
	        ip[j].u.special.parentId == isp->RWvolumeId &&
	        ip[j].u.special.type == VI_LINKTABLE) {
		return ip[j].inodeNumber;
	    }
	}
    }
    return (Inode) - 1;
}

#ifdef AFS_NAMEI_ENV
static int
CheckDupLinktable(struct SalvInfo *salvinfo, struct InodeSummary *isp, struct ViceInodeInfo *ip)
{
    afs_ino_str_t stmp;
    if (ip->u.vnode.vnodeNumber != INODESPECIAL) {
	/* not a linktable; process as a normal file */
	return 0;
    }
    if (ip->u.special.type != VI_LINKTABLE) {
	/* not a linktable; process as a normal file */
	return 0;
    }

    /* make sure nothing inc/decs it */
    ip->linkCount = 0;

    if (ip->u.special.volumeId == ip->u.special.parentId) {
	/* This is a little weird, but shouldn't break anything, and there is
	 * no known way that this can happen; just do nothing, in case deleting
	 * it would screw something up. */
	Log("Inode %s appears to be a valid linktable for id (%u), but it's not\n",
	    PrintInode(stmp, ip->inodeNumber), ip->u.special.parentId);
	Log("the linktable for our volume group (%u). This is unusual, since\n",
	    isp->RWvolumeId);
	Log("there should only be one linktable per volume group. I'm leaving\n");
	Log("it alone, just to be safe.\n");
	return -1;
    }

    Log("Linktable %s appears to be invalid (parentid/volumeid mismatch: %u != %u)\n",
        PrintInode(stmp, ip->inodeNumber), ip->u.special.parentId, ip->u.special.volumeId);
    if (Testing) {
	Log("Would have deleted linktable inode %s\n", PrintInode(stmp, ip->inodeNumber));
    } else {
	IHandle_t *tmpH;
	namei_t ufs_name;

	Log("Deleting linktable inode %s\n", PrintInode(stmp, ip->inodeNumber));
	IH_INIT(tmpH, salvinfo->fileSysDevice, isp->RWvolumeId, ip->inodeNumber);
	namei_HandleToName(&ufs_name, tmpH);
	if (unlink(ufs_name.n_path) < 0) {
	    Log("Error %d unlinking path %s\n", errno, ufs_name.n_path);
	}
    }

    return -1;
}
#endif

int
CreateLinkTable(struct SalvInfo *salvinfo, struct InodeSummary *isp, Inode ino)
{
    struct versionStamp version;
    FdHandle_t *fdP;

    if (!VALID_INO(ino))
	ino =
	    IH_CREATE(NULL, salvinfo->fileSysDevice, salvinfo->fileSysPath, 0, isp->RWvolumeId,
		      INODESPECIAL, VI_LINKTABLE, isp->RWvolumeId);
    if (!VALID_INO(ino))
	Abort
	    ("Unable to allocate link table inode for volume %u (error = %d)\n",
	     isp->RWvolumeId, errno);
    IH_INIT(salvinfo->VGLinkH, salvinfo->fileSysDevice, isp->RWvolumeId, ino);
    fdP = IH_OPEN(salvinfo->VGLinkH);
    if (fdP == NULL)
	Abort("Can't open link table for volume %u (error = %d)\n",
	      isp->RWvolumeId, errno);

    if (FDH_TRUNC(fdP, sizeof(version) + sizeof(short)) < 0)
	Abort("Can't truncate link table for volume %u (error = %d)\n",
	      isp->RWvolumeId, errno);

    version.magic = LINKTABLEMAGIC;
    version.version = LINKTABLEVERSION;

    if (FDH_PWRITE(fdP, (char *)&version, sizeof(version), 0)
	!= sizeof(version))
	Abort("Can't truncate link table for volume %u (error = %d)\n",
	      isp->RWvolumeId, errno);

    FDH_REALLYCLOSE(fdP);

    /* If the volume summary exits (i.e.,  the V*.vol header file exists),
     * then set this inode there as well.
     */
    if (isp->volSummary)
	isp->volSummary->header.linkTable = ino;

    return 0;
}

#ifdef AFS_NT40_ENV
void *
nt_SVG(void *arg)
{
    SVGParms_t *parms = (SVGParms_t *) arg;
    DoSalvageVolumeGroup(parms->svgp_salvinfo, parms->svgp_inodeSummaryp, parms->svgp_count);
    return NULL;
}

void
SalvageVolumeGroup(struct SalvInfo *salvinfo, struct InodeSummary *isp, int nVols)
{
    pthread_t tid;
    pthread_attr_t tattr;
    int code;
    SVGParms_t parms;

    /* Initialize per volume global variables, even if later code does so */
    salvinfo->VolumeChanged = 0;
    salvinfo->VGLinkH = NULL;
    salvinfo->VGLinkH_cnt = 0;
    memset(&salvinfo->VolInfo, 0, sizeof(salvinfo->VolInfo));

    parms.svgp_inodeSummaryp = isp;
    parms.svgp_count = nVols;
    parms.svgp_salvinfo = salvinfo;
    code = pthread_attr_init(&tattr);
    if (code) {
	Log("Failed to salvage volume group %u: pthread_attr_init()\n",
	    isp->RWvolumeId);
	return;
    }
    code = pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_JOINABLE);
    if (code) {
	Log("Failed to salvage volume group %u: pthread_attr_setdetachstate()\n", isp->RWvolumeId);
	return;
    }
    code = pthread_create(&tid, &tattr, nt_SVG, &parms);
    if (code) {
	Log("Failed to create thread to salvage volume group %u\n",
	    isp->RWvolumeId);
	return;
    }
    (void)pthread_join(tid, NULL);
}
#endif /* AFS_NT40_ENV */

void
DoSalvageVolumeGroup(struct SalvInfo *salvinfo, struct InodeSummary *isp, int nVols)
{
    struct ViceInodeInfo *inodes, *allInodes, *ip;
    int i, totalInodes, size, salvageTo;
    int haveRWvolume;
    int check;
    Inode ino;
    int dec_VGLinkH = 0;
    int VGLinkH_p1 =0;
    FdHandle_t *fdP = NULL;

    salvinfo->VGLinkH_cnt = 0;
    haveRWvolume = (isp->volumeId == isp->RWvolumeId
		    && isp->nSpecialInodes > 0);
    if ((!ShowMounts) || (ShowMounts && !haveRWvolume)) {
	if (!ForceSalvage && QuickCheck(salvinfo, isp, nVols))
	    return;
    }
    if (ShowMounts && !haveRWvolume)
	return;
    if (canfork && !debug && Fork() != 0) {
	(void)Wait("Salvage volume group");
	return;
    }
    for (i = 0, totalInodes = 0; i < nVols; i++)
	totalInodes += isp[i].nInodes;
    size = totalInodes * sizeof(struct ViceInodeInfo);
    inodes = (struct ViceInodeInfo *)malloc(size);
    allInodes = inodes - isp->index;	/* this would the base of all the inodes
					 * for the partition, if all the inodes
					 * had been read into memory */
    osi_Assert(afs_lseek
	   (salvinfo->inodeFd, isp->index * sizeof(struct ViceInodeInfo),
	    SEEK_SET) != -1);
    osi_Assert(read(salvinfo->inodeFd, inodes, size) == size);

    /* Don't try to salvage a read write volume if there isn't one on this
     * partition */
    salvageTo = haveRWvolume ? 0 : 1;

#ifdef AFS_NAMEI_ENV
    ino = FindLinkHandle(isp, nVols, allInodes);
    if (VALID_INO(ino)) {
	IH_INIT(salvinfo->VGLinkH, salvinfo->fileSysDevice, isp->RWvolumeId, ino);
	fdP = IH_OPEN(salvinfo->VGLinkH);
    }
    if (VALID_INO(ino) && fdP != NULL) {
	struct versionStamp header;
	afs_sfsize_t nBytes;

	nBytes = FDH_PREAD(fdP, (char *)&header, sizeof(struct versionStamp), 0);
	if (nBytes != sizeof(struct versionStamp)
	    || header.magic != LINKTABLEMAGIC) {
            Log("Bad linktable header for volume %u.\n", isp->RWvolumeId);
	    FDH_REALLYCLOSE(fdP);
	    fdP = NULL;
	}
    }
    if (!VALID_INO(ino) || fdP == NULL) {
	Log("%s link table for volume %u.\n",
	    Testing ? "Would have recreated" : "Recreating", isp->RWvolumeId);
	if (Testing) {
	    IH_INIT(salvinfo->VGLinkH, salvinfo->fileSysDevice, -1, -1);
	} else {
	    int i, j;
	    struct ViceInodeInfo *ip;
	    CreateLinkTable(salvinfo, isp, ino);
	    fdP = IH_OPEN(salvinfo->VGLinkH);
	    /* Sync fake 1 link counts to the link table, now that it exists */
	    if (fdP) {
		for (i = 0; i < nVols; i++) {
		    ip = allInodes + isp[i].index;
		    for (j = isp[i].nSpecialInodes; j < isp[i].nInodes; j++) {
			namei_SetLinkCount(fdP, ip[j].inodeNumber, 1, 1);
			ip[j].linkCount = 1;
		    }
		}
	    }
	}
    }
    if (fdP)
	FDH_REALLYCLOSE(fdP);
#else
    IH_INIT(salvinfo->VGLinkH, salvinfo->fileSysDevice, -1, -1);
#endif

    /* Salvage in reverse order--read/write volume last; this way any
     * Inodes not referenced by the time we salvage the read/write volume
     * can be picked up by the read/write volume */
    /* ACTUALLY, that's not done right now--the inodes just vanish */
    for (i = nVols - 1; i >= salvageTo; i--) {
	int rw = (i == 0);
	struct InodeSummary *lisp = &isp[i];
#ifdef AFS_NAMEI_ENV
	if (rw && (nVols > 1 || isp[i].nSpecialInodes == isp[i].nInodes)) {
	    /* If nVols > 1, we have more than one vol in this volgroup, so
	     * the RW inodes we detected may just be for the linktable, and
	     * there is no actual RW volume.
	     *
	     * Additionally, if we only have linktable inodes (no other
	     * special inodes, no data inodes), there is also no actual RW
	     * volume to salvage; this is just cruft left behind by something
	     * else. In that case nVols will only be 1, though, so also
	     * perform this linktables-only check if we don't have any
	     * non-special inodes. */
	    int inode_i;
	    int all_linktables = 1;
	    for (inode_i = 0; inode_i < isp[i].nSpecialInodes; inode_i++) {
		if (inodes[inode_i].u.special.type != VI_LINKTABLE) {
		    all_linktables = 0;
		    break;
		}
	    }
	    if (all_linktables) {
		/* All we have are linktable special inodes, so skip salvaging
		 * the RW; there was never an RW volume here. If we don't do
		 * this, we risk creating a new "phantom" RW that the VLDB
		 * doesn't know about, which is confusing and can cause
		 * problems. */
		 haveRWvolume = 0;
		 continue;
	    }
	}
#endif
	if (!Showmode)
	    Log("%s VOLUME %u%s.\n", rw ? "SALVAGING" : "CHECKING CLONED",
		lisp->volumeId, (Testing ? "(READONLY mode)" : ""));
	/* Check inodes twice.  The second time do things seriously.  This
	 * way the whole RO volume can be deleted, below, if anything goes wrong */
	for (check = 1; check >= 0; check--) {
	    int deleteMe;
	    if (SalvageVolumeHeaderFile(salvinfo, lisp, allInodes, rw, check, &deleteMe)
		== -1) {
		MaybeZapVolume(salvinfo, lisp, "Volume header", deleteMe, check);
		if (rw && deleteMe) {
		    haveRWvolume = 0;	/* This will cause its inodes to be deleted--since salvage
					 * volume won't be called */
		    break;
		}
		if (!rw)
		    break;
	    }
	    if (rw && check == 1)
		continue;
	    if (SalvageVnodes(salvinfo, isp, lisp, allInodes, check) == -1) {
		MaybeZapVolume(salvinfo, lisp, "Vnode index", 0, check);
		break;
	    }
	}
    }

    /* Fix actual inode counts */
    if (!Showmode) {
	afs_ino_str_t stmp;
	Log("totalInodes %d\n",totalInodes);
	for (ip = inodes; totalInodes; ip++, totalInodes--) {
	    static int TraceBadLinkCounts = 0;
#ifdef AFS_NAMEI_ENV
	    if (salvinfo->VGLinkH->ih_ino == ip->inodeNumber) {
		dec_VGLinkH = ip->linkCount - salvinfo->VGLinkH_cnt;
		VGLinkH_p1 = ip->u.param[0];
		continue;	/* Deal with this last. */
	    } else if (CheckDupLinktable(salvinfo, isp, ip)) {
		/* Don't touch this inode; CheckDupLinktable has handled it */
		continue;
	    }
#endif
	    if (ip->linkCount != 0 && TraceBadLinkCounts) {
		TraceBadLinkCounts--;	/* Limit reports, per volume */
		Log("#### DEBUG #### Link count incorrect by %d; inode %s, size %llu, p=(%u,%u,%u,%u)\n", ip->linkCount, PrintInode(stmp, ip->inodeNumber), (afs_uintmax_t) ip->byteCount, ip->u.param[0], ip->u.param[1], ip->u.param[2], ip->u.param[3]);
	    }
	    while (ip->linkCount > 0) {
		/* below used to assert, not break */
		if (!Testing) {
		    if (IH_DEC(salvinfo->VGLinkH, ip->inodeNumber, ip->u.param[0])) {
			Log("idec failed. inode %s errno %d\n",
			    PrintInode(stmp, ip->inodeNumber), errno);
			break;
		    }
		}
		ip->linkCount--;
	    }
	    while (ip->linkCount < 0) {
		/* these used to be asserts */
		if (!Testing) {
		    if (IH_INC(salvinfo->VGLinkH, ip->inodeNumber, ip->u.param[0])) {
			Log("iinc failed. inode %s errno %d\n",
			    PrintInode(stmp, ip->inodeNumber), errno);
			break;
		    }
		}
		ip->linkCount++;
	    }
	}
#ifdef AFS_NAMEI_ENV
	while (dec_VGLinkH > 0) {
	    if (IH_DEC(salvinfo->VGLinkH, salvinfo->VGLinkH->ih_ino, VGLinkH_p1) < 0) {
		Log("idec failed on link table, errno = %d\n", errno);
	    }
	    dec_VGLinkH--;
	}
	while (dec_VGLinkH < 0) {
	    if (IH_INC(salvinfo->VGLinkH, salvinfo->VGLinkH->ih_ino, VGLinkH_p1) < 0) {
		Log("iinc failed on link table, errno = %d\n", errno);
	    }
	    dec_VGLinkH++;
	}
#endif
    }
    free(inodes);
    /* Directory consistency checks on the rw volume */
    if (haveRWvolume)
	SalvageVolume(salvinfo, isp, salvinfo->VGLinkH);
    IH_RELEASE(salvinfo->VGLinkH);

    if (canfork && !debug) {
	ShowLog = 0;
	Exit(0);
    }
}

int
QuickCheck(struct SalvInfo *salvinfo, struct InodeSummary *isp, int nVols)
{
    /* Check headers BEFORE forking */
    int i;
    IHandle_t *h;

    for (i = 0; i < nVols; i++) {
	struct VolumeSummary *vs = isp[i].volSummary;
	VolumeDiskData volHeader;
	if (!vs) {
	    /* Don't salvage just because phantom rw volume is there... */
	    /* (If a read-only volume exists, read/write inodes must also exist) */
	    if (i == 0 && isp->nSpecialInodes == 0 && nVols > 1)
		continue;
	    return 0;
	}
	IH_INIT(h, salvinfo->fileSysDevice, vs->header.parent, vs->header.volumeInfo);
	if (IH_IREAD(h, 0, (char *)&volHeader, sizeof(volHeader))
	    == sizeof(volHeader)
	    && volHeader.stamp.magic == VOLUMEINFOMAGIC
	    && volHeader.dontSalvage == DONT_SALVAGE
	    && volHeader.needsSalvaged == 0 && volHeader.destroyMe == 0) {
	    if (volHeader.inUse != 0) {
		volHeader.inUse = 0;
		volHeader.inService = 1;
		if (!Testing) {
		    if (IH_IWRITE(h, 0, (char *)&volHeader, sizeof(volHeader))
			!= sizeof(volHeader)) {
			IH_RELEASE(h);
			return 0;
		    }
		}
	    }
	    IH_RELEASE(h);
	} else {
	    IH_RELEASE(h);
	    return 0;
	}
    }
    return 1;
}


/* SalvageVolumeHeaderFile
 *
 * Salvage the top level V*.vol header file. Make sure the special files
 * exist and that there are no duplicates.
 *
 * Calls SalvageHeader for each possible type of volume special file.
 */

int
SalvageVolumeHeaderFile(struct SalvInfo *salvinfo, struct InodeSummary *isp,
			struct ViceInodeInfo *inodes, int RW,
			int check, int *deleteMe)
{
    int i;
    struct ViceInodeInfo *ip;
    int allinodesobsolete = 1;
    struct VolumeDiskHeader diskHeader;
    afs_int32 (*writefunc)(VolumeDiskHeader_t *, struct DiskPartition64 *) = NULL;
    int *skip;
    struct VolumeHeader tempHeader;
    struct afs_inode_info stuff[MAXINODETYPE];

    /* keeps track of special inodes that are probably 'good'; they are
     * referenced in the vol header, and are included in the given inodes
     * array */
    struct {
	int valid;
	Inode inode;
    } goodspecial[MAXINODETYPE];

    if (deleteMe)
	*deleteMe = 0;

    memset(goodspecial, 0, sizeof(goodspecial));

    skip = malloc(isp->nSpecialInodes * sizeof(*skip));
    if (skip) {
	memset(skip, 0, isp->nSpecialInodes * sizeof(*skip));
    } else {
	Log("cannot allocate memory for inode skip array when salvaging "
	    "volume %lu; not performing duplicate special inode recovery\n",
	    afs_printable_uint32_lu(isp->volumeId));
	/* still try to perform the salvage; the skip array only does anything
	 * if we detect duplicate special inodes */
    }

    init_inode_info(&tempHeader, stuff);

    /*
     * First, look at the special inodes and see if any are referenced by
     * the existing volume header. If we find duplicate special inodes, we
     * can use this information to use the referenced inode (it's more
     * likely to be the 'good' one), and throw away the duplicates.
     */
    if (isp->volSummary && skip) {
	/* use tempHeader, so we can use the stuff[] array to easily index
	 * into the isp->volSummary special inodes */
	memcpy(&tempHeader, &isp->volSummary->header, sizeof(struct VolumeHeader));

	for (i = 0; i < isp->nSpecialInodes; i++) {
	    ip = &inodes[isp->index + i];
	    if (ip->u.special.type <= 0 || ip->u.special.type > MAXINODETYPE) {
		/* will get taken care of in a later loop */
		continue;
	    }
	    if (ip->inodeNumber == *(stuff[ip->u.special.type - 1].inode)) {
		goodspecial[ip->u.special.type-1].valid = 1;
		goodspecial[ip->u.special.type-1].inode = ip->inodeNumber;
	    }
	}
    }

    memset(&tempHeader, 0, sizeof(tempHeader));
    tempHeader.stamp.magic = VOLUMEHEADERMAGIC;
    tempHeader.stamp.version = VOLUMEHEADERVERSION;
    tempHeader.id = isp->volumeId;
    tempHeader.parent = isp->RWvolumeId;

    /* Check for duplicates (inodes are sorted by type field) */
    for (i = 0; i < isp->nSpecialInodes - 1; i++) {
	ip = &inodes[isp->index + i];
	if (ip->u.special.type == (ip + 1)->u.special.type) {
	    afs_ino_str_t stmp1, stmp2;

	    if (ip->u.special.type <= 0 || ip->u.special.type > MAXINODETYPE) {
		/* Will be caught in the loop below */
		continue;
	    }
	    if (!Showmode) {
		Log("Duplicate special %d inodes for volume %u found (%s, %s);\n",
		    ip->u.special.type, isp->volumeId,
		    PrintInode(stmp1, ip->inodeNumber),
		    PrintInode(stmp2, (ip+1)->inodeNumber));
	    }
	    if (skip && goodspecial[ip->u.special.type-1].valid) {
		Inode gi = goodspecial[ip->u.special.type-1].inode;

		if (!Showmode) {
		    Log("using special inode referenced by vol header (%s)\n",
		        PrintInode(stmp1, gi));
		}

		/* the volume header references some special inode of
		 * this type in the inodes array; are we it? */
		if (ip->inodeNumber != gi) {
		    skip[i] = 1;
		} else if ((ip+1)->inodeNumber != gi) {
		    /* in case this is the last iteration; we need to
		     * make sure we check ip+1, too */
		    skip[i+1] = 1;
		}
	    } else {
		if (!Showmode)
		    Log("cannot determine which is correct; salvage of volume %u aborted\n", isp->volumeId);
		if (skip) {
		    free(skip);
		}
		return -1;
	    }
	}
    }
    for (i = 0; i < isp->nSpecialInodes; i++) {
	afs_ino_str_t stmp;
	ip = &inodes[isp->index + i];
	if (ip->u.special.type <= 0 || ip->u.special.type > MAXINODETYPE) {
	    if (check) {
		Log("Rubbish header inode %s of type %d\n",
		    PrintInode(stmp, ip->inodeNumber),
		    ip->u.special.type);
		if (skip) {
		    free(skip);
		}
		return -1;
	    }
	    Log("Rubbish header inode %s of type %d; deleted\n",
	        PrintInode(stmp, ip->inodeNumber),
	        ip->u.special.type);
	} else if (!stuff[ip->u.special.type - 1].obsolete) {
	    if (skip && skip[i]) {
		if (orphans == ORPH_REMOVE) {
		    Log("Removing orphan special inode %s of type %d\n",
		        PrintInode(stmp, ip->inodeNumber), ip->u.special.type);
		    continue;
		} else {
		    Log("Ignoring orphan special inode %s of type %d\n",
		        PrintInode(stmp, ip->inodeNumber), ip->u.special.type);
		    /* fall through to the ip->linkCount--; line below */
		}
	    } else {
		*(stuff[ip->u.special.type - 1].inode) = ip->inodeNumber;
		allinodesobsolete = 0;
	    }
	    if (!check && ip->u.special.type != VI_LINKTABLE)
		ip->linkCount--;	/* Keep the inode around */
	}
    }
    if (skip) {
	free(skip);
    }
    skip = NULL;

    if (allinodesobsolete) {
	if (deleteMe)
	    *deleteMe = 1;
	return -1;
    }

    if (!check)
	salvinfo->VGLinkH_cnt++;		/* one for every header. */

    if (!RW && !check && isp->volSummary) {
	ClearROInUseBit(isp->volSummary);
	return 0;
    }

    for (i = 0; i < MAXINODETYPE; i++) {
	if (stuff[i].inodeType == VI_LINKTABLE) {
	    /* Gross hack: SalvageHeader does a bcmp on the volume header.
	     * And we may have recreated the link table earlier, so set the
	     * RW header as well. The header magic was already checked.
	     */
	    if (VALID_INO(salvinfo->VGLinkH->ih_ino)) {
		*stuff[i].inode = salvinfo->VGLinkH->ih_ino;
	    }
	    continue;
	}
	if (SalvageHeader(salvinfo, &stuff[i], isp, check, deleteMe) == -1 && check)
	    return -1;
    }

    if (isp->volSummary == NULL) {
	char path[64];
	char headerName[64];
	(void)afs_snprintf(headerName, sizeof headerName, VFORMAT, afs_printable_uint32_lu(isp->volumeId));
	(void)afs_snprintf(path, sizeof path, "%s" OS_DIRSEP "%s", salvinfo->fileSysPath, headerName);
	if (check) {
	    Log("No header file for volume %u\n", isp->volumeId);
	    return -1;
	}
	if (!Showmode)
	    Log("No header file for volume %u; %screating %s\n",
		isp->volumeId, (Testing ? "it would have been " : ""),
		path);
	isp->volSummary = calloc(1, sizeof(struct VolumeSummary));

	writefunc = VCreateVolumeDiskHeader;
    } else {
	char path[64];
	char headerName[64];
	/* hack: these two fields are obsolete... */
	isp->volSummary->header.volumeAcl = 0;
	isp->volSummary->header.volumeMountTable = 0;

	if (memcmp
	    (&isp->volSummary->header, &tempHeader,
	     sizeof(struct VolumeHeader))) {
	    VolumeExternalName_r(isp->volumeId, headerName, sizeof(headerName));
	    (void)afs_snprintf(path, sizeof path, "%s" OS_DIRSEP "%s", salvinfo->fileSysPath, headerName);

	    Log("Header file %s is damaged or no longer valid%s\n", path,
		(check ? "" : "; repairing"));
	    if (check)
		return -1;

	    writefunc = VWriteVolumeDiskHeader;
	}
    }
    if (writefunc) {
	memcpy(&isp->volSummary->header, &tempHeader,
	       sizeof(struct VolumeHeader));
	if (Testing) {
	    if (!Showmode)
		Log("It would have written a new header file for volume %u\n",
		    isp->volumeId);
	} else {
	    afs_int32 code;
	    VolumeHeaderToDisk(&diskHeader, &tempHeader);
	    code = (*writefunc)(&diskHeader, salvinfo->fileSysPartition);
	    if (code) {
		Log("Error %ld writing volume header file for volume %lu\n",
		    afs_printable_int32_ld(code),
		    afs_printable_uint32_lu(diskHeader.id));
		return -1;
	    }
	}
    }
    IH_INIT(isp->volSummary->volumeInfoHandle, salvinfo->fileSysDevice, isp->RWvolumeId,
	    isp->volSummary->header.volumeInfo);
    return 0;
}

int
SalvageHeader(struct SalvInfo *salvinfo, struct afs_inode_info *sp,
              struct InodeSummary *isp, int check, int *deleteMe)
{
    union {
	VolumeDiskData volumeInfo;
	struct versionStamp fileHeader;
    } header;
    IHandle_t *specH;
    int recreate = 0;
    ssize_t nBytes;
    FdHandle_t *fdP;

    if (sp->obsolete)
	return 0;
#ifndef AFS_NAMEI_ENV
    if (sp->inodeType == VI_LINKTABLE)
	return 0; /* header magic was already checked */
#endif
    if (*(sp->inode) == 0) {
	if (check) {
	    Log("Missing inode in volume header (%s)\n", sp->description);
	    return -1;
	}
	if (!Showmode)
	    Log("Missing inode in volume header (%s); %s\n", sp->description,
		(Testing ? "it would have recreated it" : "recreating"));
	if (!Testing) {
	    *(sp->inode) =
		IH_CREATE(NULL, salvinfo->fileSysDevice, salvinfo->fileSysPath, 0, isp->volumeId,
			  INODESPECIAL, sp->inodeType, isp->RWvolumeId);
	    if (!VALID_INO(*(sp->inode)))
		Abort
		    ("Unable to allocate inode (%s) for volume header (error = %d)\n",
		     sp->description, errno);
	}
	recreate = 1;
    }

    IH_INIT(specH, salvinfo->fileSysDevice, isp->RWvolumeId, *(sp->inode));
    fdP = IH_OPEN(specH);
    if (OKToZap && (fdP == NULL) && BadError(errno)) {
	/* bail out early and destroy the volume */
	if (!Showmode)
	    Log("Still can't open volume header inode (%s), destroying volume\n", sp->description);
	if (deleteMe)
	    *deleteMe = 1;
	IH_RELEASE(specH);
	return -1;
    }
    if (fdP == NULL)
	Abort("Unable to open inode (%s) of volume header (error = %d)\n",
	      sp->description, errno);

    if (!recreate
	&& (FDH_PREAD(fdP, (char *)&header, sp->size, 0) != sp->size
	    || header.fileHeader.magic != sp->stamp.magic)) {
	if (check) {
	    Log("Part of the header (%s) is corrupted\n", sp->description);
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(specH);
	    return -1;
	}
	Log("Part of the header (%s) is corrupted; recreating\n",
	    sp->description);
	recreate = 1;
	/* header can be garbage; make sure we don't read garbage data from
	 * it below */
	memset(&header, 0, sizeof(header));
    }
#ifdef AFS_NAMEI_ENV
    if (namei_FixSpecialOGM(fdP, check)) {
	Log("Error with namei header OGM data (%s)\n", sp->description);
	FDH_REALLYCLOSE(fdP);
	IH_RELEASE(specH);
	return -1;
    }
#endif
    if (sp->inodeType == VI_VOLINFO
	&& header.volumeInfo.destroyMe == DESTROY_ME) {
	if (deleteMe)
	    *deleteMe = 1;
	FDH_REALLYCLOSE(fdP);
	IH_RELEASE(specH);
	return -1;
    }
    if (recreate && !Testing) {
	if (check)
	    Abort
		("Internal error: recreating volume header (%s) in check mode\n",
		 sp->description);
	nBytes = FDH_TRUNC(fdP, 0);
	if (nBytes == -1)
	    Abort("Unable to truncate volume header file (%s) (error = %d)\n",
		  sp->description, errno);

	/* The following code should be moved into vutil.c */
	if (sp->inodeType == VI_VOLINFO) {
	    struct timeval tp;
	    memset(&header.volumeInfo, 0, sizeof(header.volumeInfo));
	    header.volumeInfo.stamp = sp->stamp;
	    header.volumeInfo.id = isp->volumeId;
	    header.volumeInfo.parentId = isp->RWvolumeId;
	    sprintf(header.volumeInfo.name, "bogus.%u", isp->volumeId);
	    Log("Warning: the name of volume %u is now \"bogus.%u\"\n",
		isp->volumeId, isp->volumeId);
	    header.volumeInfo.inService = 0;
	    header.volumeInfo.blessed = 0;
	    /* The + 1000 is a hack in case there are any files out in venus caches */
	    header.volumeInfo.uniquifier = (isp->maxUniquifier + 1) + 1000;
	    header.volumeInfo.type = (isp->volumeId == isp->RWvolumeId ? readwriteVolume : readonlyVolume);	/* XXXX */
	    header.volumeInfo.needsCallback = 0;
	    gettimeofday(&tp, 0);
	    header.volumeInfo.creationDate = tp.tv_sec;
	    nBytes =
		FDH_PWRITE(fdP, (char *)&header.volumeInfo,
			   sizeof(header.volumeInfo), 0);
	    if (nBytes != sizeof(header.volumeInfo)) {
		if (nBytes < 0)
		    Abort
			("Unable to write volume header file (%s) (errno = %d)\n",
			 sp->description, errno);
		Abort("Unable to write entire volume header file (%s)\n",
		      sp->description);
	    }
	} else {
	    nBytes = FDH_PWRITE(fdP, (char *)&sp->stamp, sizeof(sp->stamp), 0);
	    if (nBytes != sizeof(sp->stamp)) {
		if (nBytes < 0)
		    Abort
			("Unable to write version stamp in volume header file (%s) (errno = %d)\n",
			 sp->description, errno);
		Abort
		    ("Unable to write entire version stamp in volume header file (%s)\n",
		     sp->description);
	    }
	}
    }
    FDH_REALLYCLOSE(fdP);
    IH_RELEASE(specH);
    if (sp->inodeType == VI_VOLINFO) {
	salvinfo->VolInfo = header.volumeInfo;
	if (check) {
	    char update[25];

	    if (salvinfo->VolInfo.updateDate) {
		strcpy(update, TimeStamp(salvinfo->VolInfo.updateDate, 0));
		if (!Showmode)
		    Log("%s (%u) %supdated %s\n", salvinfo->VolInfo.name,
			salvinfo->VolInfo.id,
			(Testing ? "it would have been " : ""), update);
	    } else {
		strcpy(update, TimeStamp(salvinfo->VolInfo.creationDate, 0));
		if (!Showmode)
		    Log("%s (%u) not updated (created %s)\n",
			salvinfo->VolInfo.name, salvinfo->VolInfo.id, update);
	    }

	}
    }

    return 0;
}

int
SalvageVnodes(struct SalvInfo *salvinfo,
              struct InodeSummary *rwIsp,
	      struct InodeSummary *thisIsp,
	      struct ViceInodeInfo *inodes, int check)
{
    int ilarge, ismall, ioffset, RW, nInodes;
    ioffset = rwIsp->index + rwIsp->nSpecialInodes;	/* first inode */
    if (Showmode)
	return 0;
    RW = (rwIsp == thisIsp);
    nInodes = (rwIsp->nInodes - rwIsp->nSpecialInodes);
    ismall =
	SalvageIndex(salvinfo, thisIsp->volSummary->header.smallVnodeIndex, vSmall, RW,
		     &inodes[ioffset], nInodes, thisIsp->volSummary, check);
    if (check && ismall == -1)
	return -1;
    ilarge =
	SalvageIndex(salvinfo, thisIsp->volSummary->header.largeVnodeIndex, vLarge, RW,
		     &inodes[ioffset], nInodes, thisIsp->volSummary, check);
    return (ilarge == 0 && ismall == 0 ? 0 : -1);
}

int
SalvageIndex(struct SalvInfo *salvinfo, Inode ino, VnodeClass class, int RW,
	     struct ViceInodeInfo *ip, int nInodes,
             struct VolumeSummary *volSummary, int check)
{
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    int err = 0;
    StreamHandle_t *file;
    struct VnodeClassInfo *vcp;
    afs_sfsize_t size;
    afs_sfsize_t nVnodes;
    afs_fsize_t vnodeLength;
    int vnodeIndex;
    afs_ino_str_t stmp1, stmp2;
    IHandle_t *handle;
    FdHandle_t *fdP;

    IH_INIT(handle, salvinfo->fileSysDevice, volSummary->header.parent, ino);
    fdP = IH_OPEN(handle);
    osi_Assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    osi_Assert(file != NULL);
    vcp = &VnodeClassInfo[class];
    size = OS_SIZE(fdP->fd_fd);
    osi_Assert(size != -1);
    nVnodes = (size / vcp->diskSize) - 1;
    if (nVnodes > 0) {
	osi_Assert((nVnodes + 1) * vcp->diskSize == size);
	osi_Assert(STREAM_ASEEK(file, vcp->diskSize) == 0);
    } else {
	nVnodes = 0;
    }
    for (vnodeIndex = 0;
	 nVnodes && STREAM_READ(vnode, vcp->diskSize, 1, file) == 1;
	 nVnodes--, vnodeIndex++) {
	if (vnode->type != vNull) {
	    int vnodeChanged = 0;
	    int vnodeNumber = bitNumberToVnodeNumber(vnodeIndex, class);
	    /* Log programs that belong to root (potentially suid root);
	     * don't bother for read-only or backup volumes */
#ifdef	notdef			/* This is done elsewhere */
	    if (ShowRootFiles && RW && vnode->owner == 0 && vnodeNumber != 1)
		Log("OWNER IS ROOT %s %u dir %u vnode %u author %u owner %u mode %o\n", salvinfo->VolInfo.name, volumeNumber, vnode->parent, vnodeNumber, vnode->author, vnode->owner, vnode->modeBits);
#endif
	    if (VNDISK_GET_INO(vnode) == 0) {
		if (RW) {
		    /* Log("### DEBUG ### Deleted Vnode with 0 inode (vnode %d)\n", vnodeNumber); */
		    memset(vnode, 0, vcp->diskSize);
		    vnodeChanged = 1;
		}
	    } else {
		if (vcp->magic != vnode->vnodeMagic) {
		    /* bad magic #, probably partially created vnode */
		    if (check) {
		       Log("Partially allocated vnode %d: bad magic (is %lx should be %lx)\n",
		           vnodeNumber, afs_printable_uint32_lu(vnode->vnodeMagic),
		           afs_printable_uint32_lu(vcp->magic));
		       memset(vnode, 0, vcp->diskSize);
		       err = -1;
		       goto zooks;
		    }
		    Log("Partially allocated vnode %d deleted.\n",
			vnodeNumber);
		    memset(vnode, 0, vcp->diskSize);
		    vnodeChanged = 1;
		    goto vnodeDone;
		}
		/* ****** Should do a bit more salvage here:  e.g. make sure
		 * vnode type matches what it should be given the index */
		while (nInodes && ip->u.vnode.vnodeNumber < vnodeNumber) {
/*       	    if (vnodeIdToClass(ip->u.vnode.vnodeNumber) == class && RW) {
 *		       Log("Inode %d: says it belongs to non-existing vnode %d\n",
 *			   ip->inodeNumber, ip->u.vnode.vnodeNumber);
 *		    }
 */
		    ip++;
		    nInodes--;
		}
		if (!RW) {
		    while (nInodes && ip->u.vnode.vnodeNumber == vnodeNumber) {
			/* The following doesn't work, because the version number
			 * is not maintained correctly by the file server */
			/*if (vnode->uniquifier == ip->u.vnode.vnodeUniquifier &&
			 * vnode->dataVersion == ip->u.vnode.inodeDataVersion)
			 * break; */
			if (VNDISK_GET_INO(vnode) == ip->inodeNumber)
			    break;
			ip++;
			nInodes--;
		    }
		} else {
		    /* For RW volume, look for vnode with matching inode number;
		     * if no such match, take the first determined by our sort
		     * order */
		    struct ViceInodeInfo *lip = ip;
		    int lnInodes = nInodes;
		    while (lnInodes
			   && lip->u.vnode.vnodeNumber == vnodeNumber) {
			if (VNDISK_GET_INO(vnode) == lip->inodeNumber) {
			    ip = lip;
			    nInodes = lnInodes;
			    break;
			}
			lip++;
			lnInodes--;
		    }
		}
		if (nInodes && ip->u.vnode.vnodeNumber == vnodeNumber) {
		    /* "Matching" inode */
		    if (RW) {
			Unique vu, iu;
			FileVersion vd, id;
			vu = vnode->uniquifier;
			iu = ip->u.vnode.vnodeUniquifier;
			vd = vnode->dataVersion;
			id = ip->u.vnode.inodeDataVersion;
			/*
			 * Because of the possibility of the uniquifier overflows (> 4M)
			 * we compare them modulo the low 22-bits; we shouldn't worry
			 * about mismatching since they shouldn't to many old
			 * uniquifiers of the same vnode...
			 */
			if (IUnique(vu) != IUnique(iu)) {
			    if (!Showmode) {
				Log("Vnode %u: vnode.unique, %u, does not match inode unique, %u; fixed, but status will be wrong\n", vnodeNumber, IUnique(vu), IUnique(iu));
			    }

			    vnode->uniquifier = iu;
#ifdef	AFS_3DISPARES
			    vnode->dataVersion = (id >= vd ?
						  /* 90% of 2.1M */
						  ((id - vd) >
						   1887437 ? vd : id) :
						  /* 90% of 2.1M */
						  ((vd - id) >
						   1887437 ? id : vd));
#else
#if defined(AFS_SGI_EXMAG)
			    vnode->dataVersion = (id >= vd ?
						  /* 90% of 16M */
						  ((id - vd) >
						   15099494 ? vd : id) :
						  /* 90% of 16M */
						  ((vd - id) >
						   15099494 ? id : vd));
#else
			    vnode->dataVersion = (id > vd ? id : vd);
#endif /* AFS_SGI_EXMAG */
#endif /* AFS_3DISPARES */
			    vnodeChanged = 1;
			} else {
			    /* don't bother checking for vd > id any more, since
			     * partial file transfers always result in this state,
			     * and you can't do much else anyway (you've already
			     * found the best data you can) */
#ifdef	AFS_3DISPARES
			    if (!vnodeIsDirectory(vnodeNumber)
				&& ((vd < id && (id - vd) < 1887437)
				    || ((vd > id && (vd - id) > 1887437)))) {
#else
#if defined(AFS_SGI_EXMAG)
			    if (!vnodeIsDirectory(vnodeNumber)
				&& ((vd < id && (id - vd) < 15099494)
				    || ((vd > id && (vd - id) > 15099494)))) {
#else
			    if (!vnodeIsDirectory(vnodeNumber) && vd < id) {
#endif /* AFS_SGI_EXMAG */
#endif
				if (!Showmode)
				    Log("Vnode %d: version < inode version; fixed (old status)\n", vnodeNumber);
				vnode->dataVersion = id;
				vnodeChanged = 1;
			    }
			}
		    }
		    if (ip->inodeNumber != VNDISK_GET_INO(vnode)) {
			if (check) {
			    if (!Showmode) {
				Log("Vnode %d:  inode number incorrect (is %s should be %s). FileSize=%llu\n", vnodeNumber, PrintInode(stmp1, VNDISK_GET_INO(vnode)), PrintInode(stmp2, ip->inodeNumber), (afs_uintmax_t) ip->byteCount);
			    }
			    VNDISK_SET_INO(vnode, ip->inodeNumber);
			    err = -1;
			    goto zooks;
			}
			if (!Showmode) {
			    Log("Vnode %d: inode number incorrect; changed from %s to %s. FileSize=%llu\n", vnodeNumber, PrintInode(stmp1, VNDISK_GET_INO(vnode)), PrintInode(stmp2, ip->inodeNumber), (afs_uintmax_t) ip->byteCount);
			}
			VNDISK_SET_INO(vnode, ip->inodeNumber);
			vnodeChanged = 1;
		    }
		    VNDISK_GET_LEN(vnodeLength, vnode);
		    if (ip->byteCount != vnodeLength) {
			if (check) {
			    if (!Showmode)
				Log("Vnode %d: length incorrect; (is %llu should be %llu)\n", vnodeNumber, (afs_uintmax_t) vnodeLength, (afs_uintmax_t) ip->byteCount);
			    err = -1;
			    goto zooks;
			}
			if (!Showmode)
			    Log("Vnode %d: length incorrect; changed from %llu to %llu\n", vnodeNumber, (afs_uintmax_t) vnodeLength, (afs_uintmax_t) ip->byteCount);
			VNDISK_SET_LEN(vnode, ip->byteCount);
			vnodeChanged = 1;
		    }
		    if (!check)
			ip->linkCount--;	/* Keep the inode around */
		    ip++;
		    nInodes--;
		} else {	/* no matching inode */
		    afs_ino_str_t stmp;
		    if (VNDISK_GET_INO(vnode) != 0
			|| vnode->type == vDirectory) {
			/* No matching inode--get rid of the vnode */
			if (check) {
			    if (VNDISK_GET_INO(vnode)) {
				if (!Showmode) {
				    Log("Vnode %d (unique %u): corresponding inode %s is missing\n", vnodeNumber, vnode->uniquifier, PrintInode(stmp, VNDISK_GET_INO(vnode)));
				}
			    } else {
				if (!Showmode)
				    Log("Vnode %d (unique %u): bad directory vnode (no inode number listed)\n", vnodeNumber, vnode->uniquifier);
			    }
			    err = -1;
			    goto zooks;
			}
			if (VNDISK_GET_INO(vnode)) {
			    if (!Showmode) {
				time_t serverModifyTime = vnode->serverModifyTime;
				Log("Vnode %d (unique %u): corresponding inode %s is missing; vnode deleted, vnode mod time=%s", vnodeNumber, vnode->uniquifier, PrintInode(stmp, VNDISK_GET_INO(vnode)), ctime(&serverModifyTime));
			    }
			} else {
			    if (!Showmode) {
				time_t serverModifyTime = vnode->serverModifyTime;
				Log("Vnode %d (unique %u): bad directory vnode (no inode number listed); vnode deleted, vnode mod time=%s", vnodeNumber, vnode->uniquifier, ctime(&serverModifyTime));
			    }
			}
			memset(vnode, 0, vcp->diskSize);
			vnodeChanged = 1;
		    } else {
			/* Should not reach here becuase we checked for
			 * (inodeNumber == 0) above. And where we zero the vnode,
			 * we also goto vnodeDone.
			 */
		    }
		}
		while (nInodes && ip->u.vnode.vnodeNumber == vnodeNumber) {
		    ip++;
		    nInodes--;
		}
	    }			/* VNDISK_GET_INO(vnode) != 0 */
	  vnodeDone:
	    osi_Assert(!(vnodeChanged && check));
	    if (vnodeChanged && !Testing) {
		osi_Assert(IH_IWRITE
		       (handle, vnodeIndexOffset(vcp, vnodeNumber),
			(char *)vnode, vcp->diskSize)
		       == vcp->diskSize);
		salvinfo->VolumeChanged = 1;	/* For break call back */
	    }
	}
    }
  zooks:
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
    IH_RELEASE(handle);
    return err;
}

struct VnodeEssence *
CheckVnodeNumber(struct SalvInfo *salvinfo, VnodeId vnodeNumber)
{
    VnodeClass class;
    struct VnodeInfo *vip;
    int offset;

    class = vnodeIdToClass(vnodeNumber);
    vip = &salvinfo->vnodeInfo[class];
    offset = vnodeIdToBitNumber(vnodeNumber);
    return (offset >= vip->nVnodes ? NULL : &vip->vnodes[offset]);
}

void
CopyOnWrite(struct SalvInfo *salvinfo, struct DirSummary *dir)
{
    /* Copy the directory unconditionally if we are going to change it:
     * not just if was cloned.
     */
    struct VnodeDiskObject vnode;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[vLarge];
    Inode oldinode, newinode;
    afs_sfsize_t code;

    if (dir->copied || Testing)
	return;
    DFlush();			/* Well justified paranoia... */

    code =
	IH_IREAD(salvinfo->vnodeInfo[vLarge].handle,
		 vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		 sizeof(vnode));
    osi_Assert(code == sizeof(vnode));
    oldinode = VNDISK_GET_INO(&vnode);
    /* Increment the version number by a whole lot to avoid problems with
     * clients that were promised new version numbers--but the file server
     * crashed before the versions were written to disk.
     */
    newinode =
	IH_CREATE(dir->ds_linkH, salvinfo->fileSysDevice, salvinfo->fileSysPath, 0, dir->rwVid,
		  dir->vnodeNumber, vnode.uniquifier, vnode.dataVersion +=
		  200);
    osi_Assert(VALID_INO(newinode));
    osi_Assert(CopyInode(salvinfo->fileSysDevice, oldinode, newinode, dir->rwVid) == 0);
    vnode.cloned = 0;
    VNDISK_SET_INO(&vnode, newinode);
    code =
	IH_IWRITE(salvinfo->vnodeInfo[vLarge].handle,
		  vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		  sizeof(vnode));
    osi_Assert(code == sizeof(vnode));

    SetSalvageDirHandle(&dir->dirHandle, dir->dirHandle.dirh_handle->ih_vid,
			salvinfo->fileSysDevice, newinode,
                        &salvinfo->VolumeChanged);
    /* Don't delete the original inode right away, because the directory is
     * still being scanned.
     */
    dir->copied = 1;
}

/*
 * This function should either successfully create a new dir, or give up
 * and leave things the way they were.  In particular, if it fails to write
 * the new dir properly, it should return w/o changing the reference to the
 * old dir.
 */
void
CopyAndSalvage(struct SalvInfo *salvinfo, struct DirSummary *dir)
{
    struct VnodeDiskObject vnode;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[vLarge];
    Inode oldinode, newinode;
    DirHandle newdir;
    FdHandle_t *fdP;
    afs_int32 code;
    afs_sfsize_t lcode;
    afs_int32 parentUnique = 1;
    struct VnodeEssence *vnodeEssence;
    afs_fsize_t length;

    if (Testing)
	return;
    Log("Salvaging directory %u...\n", dir->vnodeNumber);
    lcode =
	IH_IREAD(salvinfo->vnodeInfo[vLarge].handle,
		 vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		 sizeof(vnode));
    osi_Assert(lcode == sizeof(vnode));
    oldinode = VNDISK_GET_INO(&vnode);
    /* Increment the version number by a whole lot to avoid problems with
     * clients that were promised new version numbers--but the file server
     * crashed before the versions were written to disk.
     */
    newinode =
	IH_CREATE(dir->ds_linkH, salvinfo->fileSysDevice, salvinfo->fileSysPath, 0, dir->rwVid,
		  dir->vnodeNumber, vnode.uniquifier, vnode.dataVersion +=
		  200);
    osi_Assert(VALID_INO(newinode));
    SetSalvageDirHandle(&newdir, dir->rwVid, salvinfo->fileSysDevice, newinode,
                        &salvinfo->VolumeChanged);

    /* Assign . and .. vnode numbers from dir and vnode.parent.
     * The uniquifier for . is in the vnode.
     * The uniquifier for .. might be set to a bogus value of 1 and
     * the salvager will later clean it up.
     */
    if (vnode.parent && (vnodeEssence = CheckVnodeNumber(salvinfo, vnode.parent))) {
	parentUnique = (vnodeEssence->unique ? vnodeEssence->unique : 1);
    }
    code =
	DirSalvage(&dir->dirHandle, &newdir, dir->vnodeNumber,
		   vnode.uniquifier,
		   (vnode.parent ? vnode.parent : dir->vnodeNumber),
		   parentUnique);
    if (code == 0)
	code = DFlush();
    if (code) {
	/* didn't really build the new directory properly, let's just give up. */
	code = IH_DEC(dir->ds_linkH, newinode, dir->rwVid);
	Log("Directory salvage returned code %d, continuing.\n", code);
	if (code) {
	    Log("also failed to decrement link count on new inode");
	}
	osi_Assert(1 == 2);
    }
    Log("Checking the results of the directory salvage...\n");
    if (!DirOK(&newdir)) {
	Log("Directory salvage failed!!!; restoring old version of the directory.\n");
	code = IH_DEC(dir->ds_linkH, newinode, dir->rwVid);
	osi_Assert(code == 0);
	osi_Assert(1 == 2);
    }
    vnode.cloned = 0;
    VNDISK_SET_INO(&vnode, newinode);
    length = Length(&newdir);
    VNDISK_SET_LEN(&vnode, length);
    lcode =
	IH_IWRITE(salvinfo->vnodeInfo[vLarge].handle,
		  vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		  sizeof(vnode));
    osi_Assert(lcode == sizeof(vnode));
    IH_CONDSYNC(salvinfo->vnodeInfo[vLarge].handle);

    /* make sure old directory file is really closed */
    fdP = IH_OPEN(dir->dirHandle.dirh_handle);
    FDH_REALLYCLOSE(fdP);

    code = IH_DEC(dir->ds_linkH, oldinode, dir->rwVid);
    osi_Assert(code == 0);
    dir->dirHandle = newdir;
}

/**
 * arguments for JudgeEntry.
 */
struct judgeEntry_params {
    struct DirSummary *dir;    /**< directory we're examining entries in */
    struct SalvInfo *salvinfo; /**< SalvInfo for the current salvage job */
};

int
JudgeEntry(void *arock, char *name, afs_int32 vnodeNumber,
	   afs_int32 unique)
{
    struct judgeEntry_params *params = arock;
    struct DirSummary *dir = params->dir;
    struct SalvInfo *salvinfo = params->salvinfo;
    struct VnodeEssence *vnodeEssence;
    afs_int32 dirOrphaned, todelete;

    dirOrphaned = IsVnodeOrphaned(salvinfo, dir->vnodeNumber);

    vnodeEssence = CheckVnodeNumber(salvinfo, vnodeNumber);
    if (vnodeEssence == NULL) {
	if (!Showmode) {
	    Log("dir vnode %u: invalid entry deleted: %s" OS_DIRSEP "%s (vnode %u, unique %u)\n", dir->vnodeNumber, dir->name ? dir->name : "??", name, vnodeNumber, unique);
	}
	if (!Testing) {
	    CopyOnWrite(salvinfo, dir);
	    osi_Assert(Delete(&dir->dirHandle, name) == 0);
	}
	return 0;
    }
#ifdef AFS_AIX_ENV
#ifndef AFS_NAMEI_ENV
    /* On AIX machines, don't allow entries to point to inode 0. That is a special
     * mount inode for the partition. If this inode were deleted, it would crash
     * the machine.
     */
    if (vnodeEssence->InodeNumber == 0) {
	Log("dir vnode %d: invalid entry: %s" OS_DIRSEP "%s has no inode (vnode %d, unique %d)%s\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, (Testing ? "-- would have deleted" : " -- deleted"));
	if (!Testing) {
	    CopyOnWrite(salvinfo, dir);
	    osi_Assert(Delete(&dir->dirHandle, name) == 0);
	}
	return 0;
    }
#endif
#endif

    if (!(vnodeNumber & 1) && !Showmode
	&& !(vnodeEssence->count || vnodeEssence->unique
	     || vnodeEssence->modeBits)) {
	Log("dir vnode %u: invalid entry: %s" OS_DIRSEP "%s (vnode %u, unique %u)%s\n",
	    dir->vnodeNumber, (dir->name ? dir->name : "??"), name,
	    vnodeNumber, unique,
	    ((!unique) ? (Testing ? "-- would have deleted" : " -- deleted") :
	     ""));
	if (!unique) {
	    if (!Testing) {
		CopyOnWrite(salvinfo, dir);
		osi_Assert(Delete(&dir->dirHandle, name) == 0);
	    }
	    return 0;
	}
    }

    /* Check if the Uniquifiers match. If not, change the directory entry
     * so its unique matches the vnode unique. Delete if the unique is zero
     * or if the directory is orphaned.
     */
    if (!vnodeEssence->unique || (vnodeEssence->unique) != unique) {
	todelete = ((!vnodeEssence->unique || dirOrphaned) ? 1 : 0);

	if (todelete
	    && ((strcmp(name, "..") == 0) || (strcmp(name, ".") == 0))) {
		if (dirOrphaned) {
		    /* This is an orphaned directory. Don't delete the . or ..
		     * entry. Otherwise, it will get created in the next
		     * salvage and deleted again here. So Just skip it.
		     * */
		    return 0;
		}
		/* (vnodeEssence->unique == 0 && ('.' || '..'));
		 * Entries arriving here should be deleted, but the directory
		 * is not orphaned. Therefore, the entry must be pointing at
		 * the wrong vnode.  Skip the 'else' clause and fall through;
		 * the code below will repair the entry so it correctly points
		 * at the vnode of the current directory (if '.') or the parent
		 * directory (if '..'). */
	} else {
	    if (!Showmode) {
		Log("dir vnode %u: %s" OS_DIRSEP "%s (vnode %u): unique changed from %u to %u %s\n",
		    dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique,
		    vnodeEssence->unique, (!todelete ? "" : (Testing ? "-- would have deleted" : "-- deleted")));
	    }
	    if (!Testing) {
		AFSFid fid;
		fid.Vnode = vnodeNumber;
		fid.Unique = vnodeEssence->unique;
		CopyOnWrite(salvinfo, dir);
		osi_Assert(Delete(&dir->dirHandle, name) == 0);
		if (!todelete)
		    osi_Assert(Create(&dir->dirHandle, name, &fid) == 0);
	    }
	    if (todelete)
		return 0;		/* no need to continue */
	}
    }

    if (strcmp(name, ".") == 0) {
	if (dir->vnodeNumber != vnodeNumber || (dir->unique != unique)) {
	    AFSFid fid;
	    if (!Showmode)
		Log("directory vnode %u.%u: bad '.' entry (was %u.%u); fixed\n", dir->vnodeNumber, dir->unique, vnodeNumber, unique);
	    if (!Testing) {
		CopyOnWrite(salvinfo, dir);
		osi_Assert(Delete(&dir->dirHandle, ".") == 0);
		fid.Vnode = dir->vnodeNumber;
		fid.Unique = dir->unique;
		osi_Assert(Create(&dir->dirHandle, ".", &fid) == 0);
	    }

	    vnodeNumber = fid.Vnode;	/* Get the new Essence */
	    unique = fid.Unique;
	    vnodeEssence = CheckVnodeNumber(salvinfo, vnodeNumber);
	}
	dir->haveDot = 1;
    } else if (strcmp(name, "..") == 0) {
	AFSFid pa;
	if (dir->parent) {
	    struct VnodeEssence *dotdot;
	    pa.Vnode = dir->parent;
	    dotdot = CheckVnodeNumber(salvinfo, pa.Vnode);
	    osi_Assert(dotdot != NULL);	/* XXX Should not be assert */
	    pa.Unique = dotdot->unique;
	} else {
	    pa.Vnode = dir->vnodeNumber;
	    pa.Unique = dir->unique;
	}
	if ((pa.Vnode != vnodeNumber) || (pa.Unique != unique)) {
	    if (!Showmode)
		Log("directory vnode %u.%u: bad '..' entry (was %u.%u); fixed\n", dir->vnodeNumber, dir->unique, vnodeNumber, unique);
	    if (!Testing) {
		CopyOnWrite(salvinfo, dir);
		osi_Assert(Delete(&dir->dirHandle, "..") == 0);
		osi_Assert(Create(&dir->dirHandle, "..", &pa) == 0);
	    }

	    vnodeNumber = pa.Vnode;	/* Get the new Essence */
	    unique = pa.Unique;
	    vnodeEssence = CheckVnodeNumber(salvinfo, vnodeNumber);
	}
	dir->haveDotDot = 1;
    } else if (strncmp(name, ".__afs", 6) == 0) {
	if (!Showmode) {
	    Log("dir vnode %u: special old unlink-while-referenced file %s %s deleted (vnode %u)\n", dir->vnodeNumber, name, (Testing ? "would have been" : "is"), vnodeNumber);
	}
	if (!Testing) {
	    CopyOnWrite(salvinfo, dir);
	    osi_Assert(Delete(&dir->dirHandle, name) == 0);
	}
	vnodeEssence->claimed = 0;	/* Not claimed: Orphaned */
	vnodeEssence->todelete = 1;	/* Will later delete vnode and decr inode */
	return 0;
    } else {
	if (ShowSuid && (vnodeEssence->modeBits & 06000))
	    Log("FOUND suid/sgid file: %s" OS_DIRSEP "%s (%u.%u %05o) author %u (vnode %u dir %u)\n", dir->name ? dir->name : "??", name, vnodeEssence->owner, vnodeEssence->group, vnodeEssence->modeBits, vnodeEssence->author, vnodeNumber, dir->vnodeNumber);
	if (/* ShowMounts && */ (vnodeEssence->type == vSymlink)
	    && !(vnodeEssence->modeBits & 0111)) {
	    afs_sfsize_t nBytes;
	    afs_sfsize_t size;
	    char buf[1025];
	    IHandle_t *ihP;
	    FdHandle_t *fdP;

	    IH_INIT(ihP, salvinfo->fileSysDevice, dir->dirHandle.dirh_handle->ih_vid,
		    vnodeEssence->InodeNumber);
	    fdP = IH_OPEN(ihP);
	    if (fdP == NULL) {
		Log("ERROR %s could not open mount point vnode %u\n", dir->vname, vnodeNumber);
		IH_RELEASE(ihP);
		return 0;
	    }
	    size = FDH_SIZE(fdP);
	    if (size < 0) {
		Log("ERROR %s mount point has invalid size %d, vnode %u\n", dir->vname, (int)size, vnodeNumber);
		FDH_REALLYCLOSE(fdP);
		IH_RELEASE(ihP);
		return 0;
	    }

	    if (size > 1024)
		size = 1024;
	    nBytes = FDH_PREAD(fdP, buf, size, 0);
	    if (nBytes == size) {
		buf[size] = '\0';
		if ( (*buf != '#' && *buf != '%') || buf[strlen(buf)-1] != '.' ) {
		    Log("Volume %u (%s) mount point %s" OS_DIRSEP "%s to '%s' invalid, %s to symbolic link\n",
			dir->dirHandle.dirh_handle->ih_vid, dir->vname, dir->name ? dir->name : "??", name, buf,
			Testing ? "would convert" : "converted");
		    vnodeEssence->modeBits |= 0111;
		    vnodeEssence->changed = 1;
		} else if (ShowMounts) Log("In volume %u (%s) found mountpoint %s" OS_DIRSEP "%s to '%s'\n",
		    dir->dirHandle.dirh_handle->ih_vid, dir->vname,
		    dir->name ? dir->name : "??", name, buf);
	    } else {
		Log("Volume %s cound not read mount point vnode %u size %d code %d\n",
		    dir->vname, vnodeNumber, (int)size, (int)nBytes);
	    }
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ihP);
	}
	if (ShowRootFiles && vnodeEssence->owner == 0 && vnodeNumber != 1)
	    Log("FOUND root file: %s" OS_DIRSEP "%s (%u.%u %05o) author %u (vnode %u dir %u)\n", dir->name ? dir->name : "??", name, vnodeEssence->owner, vnodeEssence->group, vnodeEssence->modeBits, vnodeEssence->author, vnodeNumber, dir->vnodeNumber);
	if (vnodeIdToClass(vnodeNumber) == vLarge
	    && vnodeEssence->name == NULL) {
	    char *n;
	    if ((n = (char *)malloc(strlen(name) + 1)))
		strcpy(n, name);
	    vnodeEssence->name = n;
	}

	/* The directory entry points to the vnode. Check to see if the
	 * vnode points back to the directory. If not, then let the
	 * directory claim it (else it might end up orphaned). Vnodes
	 * already claimed by another directory are deleted from this
	 * directory: hardlinks to the same vnode are not allowed
	 * from different directories.
	 */
	if (vnodeEssence->parent != dir->vnodeNumber) {
	    if (!vnodeEssence->claimed && !dirOrphaned && vnodeNumber != 1) {
		/* Vnode does not point back to this directory.
		 * Orphaned dirs cannot claim a file (it may belong to
		 * another non-orphaned dir).
		 */
		if (!Showmode) {
		    Log("dir vnode %u: %s" OS_DIRSEP "%s (vnode %u, unique %u) -- parent vnode %schanged from %u to %u\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, (Testing ? "would have been " : ""), vnodeEssence->parent, dir->vnodeNumber);
		}
		vnodeEssence->parent = dir->vnodeNumber;
		vnodeEssence->changed = 1;
	    } else {
		/* Vnode was claimed by another directory */
		if (!Showmode) {
		    if (dirOrphaned) {
			Log("dir vnode %u: %s" OS_DIRSEP "%s parent vnode is %u (vnode %u, unique %u) -- %sdeleted\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeEssence->parent, vnodeNumber, unique, (Testing ? "would have been " : ""));
		    } else if (vnodeNumber == 1) {
			Log("dir vnode %d: %s" OS_DIRSEP "%s is invalid (vnode %d, unique %d) -- %sdeleted\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, (Testing ? "would have been " : ""));
		    } else {
			Log("dir vnode %u: %s" OS_DIRSEP "%s already claimed by directory vnode %u (vnode %u, unique %u) -- %sdeleted\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeEssence->parent, vnodeNumber, unique, (Testing ? "would have been " : ""));
		    }
		}
		if (!Testing) {
		    CopyOnWrite(salvinfo, dir);
		    osi_Assert(Delete(&dir->dirHandle, name) == 0);
		}
		return 0;
	    }
	}
	/* This directory claims the vnode */
	vnodeEssence->claimed = 1;
    }
    vnodeEssence->count--;
    return 0;
}

void
DistilVnodeEssence(struct SalvInfo *salvinfo, VolumeId rwVId,
                   VnodeClass class, Inode ino, Unique * maxu)
{
    struct VnodeInfo *vip = &salvinfo->vnodeInfo[class];
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    afs_sfsize_t size;
    StreamHandle_t *file;
    int vnodeIndex;
    int nVnodes;
    FdHandle_t *fdP;

    IH_INIT(vip->handle, salvinfo->fileSysDevice, rwVId, ino);
    fdP = IH_OPEN(vip->handle);
    osi_Assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    osi_Assert(file != NULL);
    size = OS_SIZE(fdP->fd_fd);
    osi_Assert(size != -1);
    vip->nVnodes = (size / vcp->diskSize) - 1;
    if (vip->nVnodes > 0) {
	osi_Assert((vip->nVnodes + 1) * vcp->diskSize == size);
	osi_Assert(STREAM_ASEEK(file, vcp->diskSize) == 0);
	osi_Assert((vip->vnodes = (struct VnodeEssence *)
		calloc(vip->nVnodes, sizeof(struct VnodeEssence))) != NULL);
	if (class == vLarge) {
	    osi_Assert((vip->inodes = (Inode *)
		    calloc(vip->nVnodes, sizeof(Inode))) != NULL);
	} else {
	    vip->inodes = NULL;
	}
    } else {
	vip->nVnodes = 0;
	vip->vnodes = NULL;
	vip->inodes = NULL;
    }
    vip->volumeBlockCount = vip->nAllocatedVnodes = 0;
    for (vnodeIndex = 0, nVnodes = vip->nVnodes;
	 nVnodes && STREAM_READ(vnode, vcp->diskSize, 1, file) == 1;
	 nVnodes--, vnodeIndex++) {
	if (vnode->type != vNull) {
	    struct VnodeEssence *vep = &vip->vnodes[vnodeIndex];
	    afs_fsize_t vnodeLength;
	    vip->nAllocatedVnodes++;
	    vep->count = vnode->linkCount;
	    VNDISK_GET_LEN(vnodeLength, vnode);
	    vep->blockCount = nBlocks(vnodeLength);
	    vip->volumeBlockCount += vep->blockCount;
	    vep->parent = vnode->parent;
	    vep->unique = vnode->uniquifier;
	    if (*maxu < vnode->uniquifier)
		*maxu = vnode->uniquifier;
	    vep->modeBits = vnode->modeBits;
	    vep->InodeNumber = VNDISK_GET_INO(vnode);
	    vep->type = vnode->type;
	    vep->author = vnode->author;
	    vep->owner = vnode->owner;
	    vep->group = vnode->group;
	    if (vnode->type == vDirectory) {
		if (class != vLarge) {
		    VnodeId vnodeNumber = bitNumberToVnodeNumber(vnodeIndex, class);
		    vip->nAllocatedVnodes--;
		    memset(vnode, 0, sizeof(*vnode));
		    IH_IWRITE(salvinfo->vnodeInfo[vSmall].handle,
			      vnodeIndexOffset(vcp, vnodeNumber),
			      (char *)&vnode, sizeof(vnode));
		    salvinfo->VolumeChanged = 1;
		} else
		    vip->inodes[vnodeIndex] = VNDISK_GET_INO(vnode);
	    }
	}
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
}

static char *
GetDirName(struct SalvInfo *salvinfo, VnodeId vnode, struct VnodeEssence *vp,
           char *path)
{
    struct VnodeEssence *parentvp;

    if (vnode == 1) {
	strcpy(path, ".");
	return path;
    }
    if (vp->parent && vp->name && (parentvp = CheckVnodeNumber(salvinfo, vp->parent))
	&& GetDirName(salvinfo, vp->parent, parentvp, path)) {
	strcat(path, OS_DIRSEP);
	strcat(path, vp->name);
	return path;
    }
    return 0;
}

/* To determine if a vnode is orhpaned or not, the vnode and all its parent
 * vnodes must be "claimed". The vep->claimed flag is set in JudgeEntry().
 */
static int
IsVnodeOrphaned(struct SalvInfo *salvinfo, VnodeId vnode)
{
    struct VnodeEssence *vep;

    if (vnode == 0)
	return (1);		/* Vnode zero does not exist */
    if (vnode == 1)
	return (0);		/* The root dir vnode is always claimed */
    vep = CheckVnodeNumber(salvinfo, vnode);	/* Get the vnode essence */
    if (!vep || !vep->claimed)
	return (1);		/* Vnode is not claimed - it is orphaned */

    return (IsVnodeOrphaned(salvinfo, vep->parent));
}

void
SalvageDir(struct SalvInfo *salvinfo, char *name, VolumeId rwVid,
	   struct VnodeInfo *dirVnodeInfo, IHandle_t * alinkH, int i,
	   struct DirSummary *rootdir, int *rootdirfound)
{
    static struct DirSummary dir;
    static struct DirHandle dirHandle;
    struct VnodeEssence *parent;
    static char path[MAXPATHLEN];
    int dirok, code;

    if (dirVnodeInfo->vnodes[i].salvaged)
	return;			/* already salvaged */

    dir.rwVid = rwVid;
    dirVnodeInfo->vnodes[i].salvaged = 1;

    if (dirVnodeInfo->inodes[i] == 0)
	return;			/* Not allocated to a directory */

    if (bitNumberToVnodeNumber(i, vLarge) == 1) {
	if (dirVnodeInfo->vnodes[i].parent) {
	    Log("Bad parent, vnode 1; %s...\n",
		(Testing ? "skipping" : "salvaging"));
	    dirVnodeInfo->vnodes[i].parent = 0;
	    dirVnodeInfo->vnodes[i].changed = 1;
	}
    } else {
	parent = CheckVnodeNumber(salvinfo, dirVnodeInfo->vnodes[i].parent);
	if (parent && parent->salvaged == 0)
	    SalvageDir(salvinfo, name, rwVid, dirVnodeInfo, alinkH,
		       vnodeIdToBitNumber(dirVnodeInfo->vnodes[i].parent),
		       rootdir, rootdirfound);
    }

    dir.vnodeNumber = bitNumberToVnodeNumber(i, vLarge);
    dir.unique = dirVnodeInfo->vnodes[i].unique;
    dir.copied = 0;
    dir.vname = name;
    dir.parent = dirVnodeInfo->vnodes[i].parent;
    dir.haveDot = dir.haveDotDot = 0;
    dir.ds_linkH = alinkH;
    SetSalvageDirHandle(&dir.dirHandle, dir.rwVid, salvinfo->fileSysDevice,
			dirVnodeInfo->inodes[i], &salvinfo->VolumeChanged);

    dirok = ((RebuildDirs && !Testing) ? 0 : DirOK(&dir.dirHandle));
    if (!dirok) {
	if (!RebuildDirs) {
	    Log("Directory bad, vnode %u; %s...\n", dir.vnodeNumber,
		(Testing ? "skipping" : "salvaging"));
	}
	if (!Testing) {
	    CopyAndSalvage(salvinfo, &dir);
	    dirok = 1;
	    dirVnodeInfo->inodes[i] = dir.dirHandle.dirh_inode;
	}
    }
    dirHandle = dir.dirHandle;

    dir.name =
	GetDirName(salvinfo, bitNumberToVnodeNumber(i, vLarge),
		   &dirVnodeInfo->vnodes[i], path);

    if (dirok) {
	/* If enumeration failed for random reasons, we will probably delete
	 * too much stuff, so we guard against this instead.
	 */
	struct judgeEntry_params judge_params;
	judge_params.salvinfo = salvinfo;
	judge_params.dir = &dir;

	osi_Assert(EnumerateDir(&dirHandle, JudgeEntry, &judge_params) == 0);
    }

    /* Delete the old directory if it was copied in order to salvage.
     * CopyOnWrite has written the new inode # to the disk, but we still
     * have the old one in our local structure here.  Thus, we idec the
     * local dude.
     */
    DFlush();
    if (dir.copied && !Testing) {
	code = IH_DEC(dir.ds_linkH, dirHandle.dirh_handle->ih_ino, rwVid);
	osi_Assert(code == 0);
	dirVnodeInfo->inodes[i] = dir.dirHandle.dirh_inode;
    }

    /* Remember rootdir DirSummary _after_ it has been judged */
    if (dir.vnodeNumber == 1 && dir.unique == 1) {
	memcpy(rootdir, &dir, sizeof(struct DirSummary));
	*rootdirfound = 1;
    }

    return;
}

/**
 * Get a new FID that can be used to create a new file.
 *
 * @param[in] volHeader vol header for the volume
 * @param[in] class     what type of vnode we'll be creating (vLarge or vSmall)
 * @param[out] afid     the FID that we can use (only Vnode and Unique are set)
 * @param[inout] maxunique  max uniquifier for all vnodes in the volume;
 *                          updated to the new max unique if we create a new
 *                          vnode
 */
static void
GetNewFID(struct SalvInfo *salvinfo, VolumeDiskData *volHeader,
          VnodeClass class, AFSFid *afid, Unique *maxunique)
{
    int i;
    for (i = 0; i < salvinfo->vnodeInfo[class].nVnodes; i++) {
	if (salvinfo->vnodeInfo[class].vnodes[i].type == vNull) {
	    break;
	}
    }
    if (i == salvinfo->vnodeInfo[class].nVnodes) {
	/* no free vnodes; make a new one */
	salvinfo->vnodeInfo[class].nVnodes++;
	salvinfo->vnodeInfo[class].vnodes =
	    realloc(salvinfo->vnodeInfo[class].vnodes,
	            sizeof(struct VnodeEssence) * (i+1));

	salvinfo->vnodeInfo[class].vnodes[i].type = vNull;
    }

    afid->Vnode = bitNumberToVnodeNumber(i, class);

    if (volHeader->uniquifier < (*maxunique + 1)) {
	/* header uniq is bad; it will get bumped by 2000 later */
	afid->Unique = *maxunique + 1 + 2000;
	(*maxunique)++;
    } else {
	/* header uniq seems okay; just use that */
	afid->Unique = *maxunique = volHeader->uniquifier++;
    }
}

/**
 * Create a vnode for a README file explaining not to use a recreated-root vol.
 *
 * @param[in] volHeader vol header for the volume
 * @param[in] alinkH    ihandle for i/o for the volume
 * @param[in] vid       volume id
 * @param[inout] maxunique  max uniquifier for all vnodes in the volume;
 *                          updated to the new max unique if we create a new
 *                          vnode
 * @param[out] afid     FID for the new readme vnode
 * @param[out] ainode   the inode for the new readme file
 *
 * @return operation status
 *  @retval 0 success
 *  @retval -1 error
 */
static int
CreateReadme(struct SalvInfo *salvinfo, VolumeDiskData *volHeader,
             IHandle_t *alinkH, VolumeId vid, Unique *maxunique, AFSFid *afid,
             Inode *ainode)
{
    Inode readmeinode;
    struct VnodeDiskObject *rvnode = NULL;
    afs_sfsize_t bytes;
    IHandle_t *readmeH = NULL;
    struct VnodeEssence *vep;
    afs_fsize_t length;
    time_t now = time(NULL);

    /* Try to make the note brief, but informative. Only administrators should
     * be able to read this file at first, so we can hopefully assume they
     * know what AFS is, what a volume is, etc. */
    char readme[] =
"This volume has been salvaged, but has lost its original root directory.\n"
"The root directory that exists now has been recreated from orphan files\n"
"from the rest of the volume. This recreated root directory may interfere\n"
"with old cached data on clients, and there is no way the salvager can\n"
"reasonably prevent that. So, it is recommended that you do not continue to\n"
"use this volume, but only copy the salvaged data to a new volume.\n"
"Continuing to use this volume as it exists now may cause some clients to\n"
"behave oddly when accessing this volume.\n"
"\n\t -- Your friendly neighborhood OpenAFS salvager\n";
    /* ^ the person reading this probably just lost some data, so they could
     * use some cheering up. */

    /* -1 for the trailing NUL */
    length = sizeof(readme) - 1;

    GetNewFID(salvinfo, volHeader, vSmall, afid, maxunique);

    vep = &salvinfo->vnodeInfo[vSmall].vnodes[vnodeIdToBitNumber(afid->Vnode)];

    /* create the inode and write the contents */
    readmeinode = IH_CREATE(alinkH, salvinfo->fileSysDevice,
                            salvinfo->fileSysPath, 0, vid,
                            afid->Vnode, afid->Unique, 1);
    if (!VALID_INO(readmeinode)) {
	Log("CreateReadme: readme IH_CREATE failed\n");
	goto error;
    }

    IH_INIT(readmeH, salvinfo->fileSysDevice, vid, readmeinode);
    bytes = IH_IWRITE(readmeH, 0, readme, length);
    IH_RELEASE(readmeH);

    if (bytes != length) {
	Log("CreateReadme: IWRITE failed (%d/%d)\n", (int)bytes,
	    (int)sizeof(readme));
	goto error;
    }

    /* create the vnode and write it out */
    rvnode = calloc(1, SIZEOF_SMALLDISKVNODE);
    if (!rvnode) {
	Log("CreateRootDir: error alloc'ing memory\n");
	goto error;
    }

    rvnode->type = vFile;
    rvnode->cloned = 0;
    rvnode->modeBits = 0777;
    rvnode->linkCount = 1;
    VNDISK_SET_LEN(rvnode, length);
    rvnode->uniquifier = afid->Unique;
    rvnode->dataVersion = 1;
    VNDISK_SET_INO(rvnode, readmeinode);
    rvnode->unixModifyTime = rvnode->serverModifyTime = now;
    rvnode->author = 0;
    rvnode->owner = 0;
    rvnode->parent = 1;
    rvnode->group = 0;
    rvnode->vnodeMagic = VnodeClassInfo[vSmall].magic;

    bytes = IH_IWRITE(salvinfo->vnodeInfo[vSmall].handle,
                      vnodeIndexOffset(&VnodeClassInfo[vSmall], afid->Vnode),
                      (char*)rvnode, SIZEOF_SMALLDISKVNODE);

    if (bytes != SIZEOF_SMALLDISKVNODE) {
	Log("CreateReadme: IH_IWRITE failed (%d/%d)\n", (int)bytes,
	    (int)SIZEOF_SMALLDISKVNODE);
	goto error;
    }

    /* update VnodeEssence for new readme vnode */
    salvinfo->vnodeInfo[vSmall].nAllocatedVnodes++;
    vep->count = 0;
    vep->blockCount = nBlocks(length);
    salvinfo->vnodeInfo[vSmall].volumeBlockCount += vep->blockCount;
    vep->parent = rvnode->parent;
    vep->unique = rvnode->uniquifier;
    vep->modeBits = rvnode->modeBits;
    vep->InodeNumber = VNDISK_GET_INO(rvnode);
    vep->type = rvnode->type;
    vep->author = rvnode->author;
    vep->owner = rvnode->owner;
    vep->group = rvnode->group;

    free(rvnode);
    rvnode = NULL;

    vep->claimed = 1;
    vep->changed = 0;
    vep->salvaged = 1;
    vep->todelete = 0;

    *ainode = readmeinode;

    return 0;

 error:
    if (IH_DEC(alinkH, readmeinode, vid)) {
	Log("CreateReadme (recovery): IH_DEC failed\n");
    }

    if (rvnode) {
	free(rvnode);
	rvnode = NULL;
    }

    return -1;
}

/**
 * create a root dir for a volume that lacks one.
 *
 * @param[in] volHeader vol header for the volume
 * @param[in] alinkH    ihandle for disk access for this volume group
 * @param[in] vid       volume id we're dealing with
 * @param[out] rootdir  populated with info about the new root dir
 * @param[inout] maxunique  max uniquifier for all vnodes in the volume;
 *                          updated to the new max unique if we create a new
 *                          vnode
 *
 * @return operation status
 *  @retval 0  success
 *  @retval -1 error
 */
static int
CreateRootDir(struct SalvInfo *salvinfo, VolumeDiskData *volHeader,
              IHandle_t *alinkH, VolumeId vid, struct DirSummary *rootdir,
              Unique *maxunique)
{
    FileVersion dv;
    int decroot = 0, decreadme = 0;
    AFSFid did, readmeid;
    afs_fsize_t length;
    Inode rootinode;
    struct VnodeDiskObject *rootvnode = NULL;
    struct acl_accessList *ACL;
    Inode *ip;
    afs_sfsize_t bytes;
    struct VnodeEssence *vep;
    Inode readmeinode = 0;
    time_t now = time(NULL);

    if (!salvinfo->vnodeInfo[vLarge].vnodes && !salvinfo->vnodeInfo[vSmall].vnodes) {
	Log("Not creating new root dir; volume appears to lack any vnodes\n");
	goto error;
    }

    if (!salvinfo->vnodeInfo[vLarge].vnodes) {
	/* We don't have any large vnodes in the volume; allocate room
	 * for one so we can recreate the root dir */
	salvinfo->vnodeInfo[vLarge].nVnodes = 1;
	salvinfo->vnodeInfo[vLarge].vnodes = calloc(1, sizeof(struct VnodeEssence));
	salvinfo->vnodeInfo[vLarge].inodes = calloc(1, sizeof(Inode));

	osi_Assert(salvinfo->vnodeInfo[vLarge].vnodes);
	osi_Assert(salvinfo->vnodeInfo[vLarge].inodes);
    }

    vep = &salvinfo->vnodeInfo[vLarge].vnodes[vnodeIdToBitNumber(1)];
    ip = &salvinfo->vnodeInfo[vLarge].inodes[vnodeIdToBitNumber(1)];
    if (vep->type != vNull) {
	Log("Not creating new root dir; existing vnode 1 is non-null\n");
	goto error;
    }

    if (CreateReadme(salvinfo, volHeader, alinkH, vid, maxunique, &readmeid,
                     &readmeinode) != 0) {
	goto error;
    }
    decreadme = 1;

    /* set the DV to a very high number, so it is unlikely that we collide
     * with a cached DV */
    dv = 1 << 30;

    rootinode = IH_CREATE(alinkH, salvinfo->fileSysDevice, salvinfo->fileSysPath,
                          0, vid, 1, 1, dv);
    if (!VALID_INO(rootinode)) {
	Log("CreateRootDir: IH_CREATE failed\n");
	goto error;
    }
    decroot = 1;

    SetSalvageDirHandle(&rootdir->dirHandle, vid, salvinfo->fileSysDevice,
                        rootinode, &salvinfo->VolumeChanged);
    did.Volume = vid;
    did.Vnode = 1;
    did.Unique = 1;
    if (MakeDir(&rootdir->dirHandle, (afs_int32*)&did, (afs_int32*)&did)) {
	Log("CreateRootDir: MakeDir failed\n");
	goto error;
    }
    if (Create(&rootdir->dirHandle, "README.ROOTDIR", &readmeid)) {
	Log("CreateRootDir: Create failed\n");
	goto error;
    }
    DFlush();
    length = Length(&rootdir->dirHandle);
    DZap((void *)&rootdir->dirHandle);

    /* create the new root dir vnode */
    rootvnode = calloc(1, SIZEOF_LARGEDISKVNODE);
    if (!rootvnode) {
	Log("CreateRootDir: malloc failed\n");
	goto error;
    }

    /* only give 'rl' permissions to 'system:administrators'. We do this to
     * try to catch the attention of an administrator, that they should not
     * be writing to this directory or continue to use it. */
    ACL = VVnodeDiskACL(rootvnode);
    ACL->size = sizeof(struct acl_accessList);
    ACL->version = ACL_ACLVERSION;
    ACL->total = 1;
    ACL->positive = 1;
    ACL->negative = 0;
    ACL->entries[0].id = -204; /* system:administrators */
    ACL->entries[0].rights = PRSFS_READ | PRSFS_LOOKUP;

    rootvnode->type = vDirectory;
    rootvnode->cloned = 0;
    rootvnode->modeBits = 0777;
    rootvnode->linkCount = 2;
    VNDISK_SET_LEN(rootvnode, length);
    rootvnode->uniquifier = 1;
    rootvnode->dataVersion = dv;
    VNDISK_SET_INO(rootvnode, rootinode);
    rootvnode->unixModifyTime = rootvnode->serverModifyTime = now;
    rootvnode->author = 0;
    rootvnode->owner = 0;
    rootvnode->parent = 0;
    rootvnode->group = 0;
    rootvnode->vnodeMagic = VnodeClassInfo[vLarge].magic;

    /* write it out to disk */
    bytes = IH_IWRITE(salvinfo->vnodeInfo[vLarge].handle,
	      vnodeIndexOffset(&VnodeClassInfo[vLarge], 1),
	      (char*)rootvnode, SIZEOF_LARGEDISKVNODE);

    if (bytes != SIZEOF_LARGEDISKVNODE) {
	/* just cast to int and don't worry about printing real 64-bit ints;
	 * a large disk vnode isn't anywhere near the 32-bit limit */
	Log("CreateRootDir: IH_IWRITE failed (%d/%d)\n", (int)bytes,
	    (int)SIZEOF_LARGEDISKVNODE);
	goto error;
    }

    /* update VnodeEssence for the new root vnode */
    salvinfo->vnodeInfo[vLarge].nAllocatedVnodes++;
    vep->count = 0;
    vep->blockCount = nBlocks(length);
    salvinfo->vnodeInfo[vLarge].volumeBlockCount += vep->blockCount;
    vep->parent = rootvnode->parent;
    vep->unique = rootvnode->uniquifier;
    vep->modeBits = rootvnode->modeBits;
    vep->InodeNumber = VNDISK_GET_INO(rootvnode);
    vep->type = rootvnode->type;
    vep->author = rootvnode->author;
    vep->owner = rootvnode->owner;
    vep->group = rootvnode->group;

    free(rootvnode);
    rootvnode = NULL;

    vep->claimed = 0;
    vep->changed = 0;
    vep->salvaged = 1;
    vep->todelete = 0;

    /* update DirSummary for the new root vnode */
    rootdir->vnodeNumber = 1;
    rootdir->unique = 1;
    rootdir->haveDot = 1;
    rootdir->haveDotDot = 1;
    rootdir->rwVid = vid;
    rootdir->copied = 0;
    rootdir->parent = 0;
    rootdir->name = strdup(".");
    rootdir->vname = volHeader->name;
    rootdir->ds_linkH = alinkH;

    *ip = rootinode;

    return 0;

 error:
    if (decroot && IH_DEC(alinkH, rootinode, vid)) {
	Log("CreateRootDir (recovery): IH_DEC (root) failed\n");
    }
    if (decreadme && IH_DEC(alinkH, readmeinode, vid)) {
	Log("CreateRootDir (recovery): IH_DEC (readme) failed\n");
    }
    if (rootvnode) {
	free(rootvnode);
	rootvnode = NULL;
    }
    return -1;
}

/**
 * salvage a volume group.
 *
 * @param[in] salvinfo information for the curent salvage job
 * @param[in] rwIsp    inode summary for rw volume
 * @param[in] alinkH   link table inode handle
 *
 * @return operation status
 *   @retval 0 success
 */
int
SalvageVolume(struct SalvInfo *salvinfo, struct InodeSummary *rwIsp, IHandle_t * alinkH)
{
    /* This routine, for now, will only be called for read-write volumes */
    int i, j, code;
    int BlocksInVolume = 0, FilesInVolume = 0;
    VnodeClass class;
    struct DirSummary rootdir, oldrootdir;
    struct VnodeInfo *dirVnodeInfo;
    struct VnodeDiskObject vnode;
    VolumeDiskData volHeader;
    VolumeId vid;
    int orphaned, rootdirfound = 0;
    Unique maxunique = 0;	/* the maxUniquifier from the vnodes */
    afs_int32 ofiles = 0, oblocks = 0;	/* Number of orphaned files/blocks */
    struct VnodeEssence *vep;
    afs_int32 v, pv;
    IHandle_t *h;
    afs_sfsize_t nBytes;
    AFSFid pa;
    VnodeId LFVnode, ThisVnode;
    Unique LFUnique, ThisUnique;
    char npath[128];
    int newrootdir = 0;

    vid = rwIsp->volSummary->header.id;
    IH_INIT(h, salvinfo->fileSysDevice, vid, rwIsp->volSummary->header.volumeInfo);
    nBytes = IH_IREAD(h, 0, (char *)&volHeader, sizeof(volHeader));
    osi_Assert(nBytes == sizeof(volHeader));
    osi_Assert(volHeader.stamp.magic == VOLUMEINFOMAGIC);
    osi_Assert(volHeader.destroyMe != DESTROY_ME);
    /* (should not have gotten this far with DESTROY_ME flag still set!) */

    DistilVnodeEssence(salvinfo, vid, vLarge,
                       rwIsp->volSummary->header.largeVnodeIndex, &maxunique);
    DistilVnodeEssence(salvinfo, vid, vSmall,
                       rwIsp->volSummary->header.smallVnodeIndex, &maxunique);

    dirVnodeInfo = &salvinfo->vnodeInfo[vLarge];
    for (i = 0; i < dirVnodeInfo->nVnodes; i++) {
	SalvageDir(salvinfo, volHeader.name, vid, dirVnodeInfo, alinkH, i,
		   &rootdir, &rootdirfound);
    }
#ifdef AFS_NT40_ENV
    nt_sync(salvinfo->fileSysDevice);
#else
    sync();				/* This used to be done lower level, for every dir */
#endif
    if (Showmode) {
	IH_RELEASE(h);
	return 0;
    }

    if (!rootdirfound && (orphans == ORPH_ATTACH) && !Testing) {

	Log("Cannot find root directory for volume %lu; attempting to create "
	    "a new one\n", afs_printable_uint32_lu(vid));

	code = CreateRootDir(salvinfo, &volHeader, alinkH, vid, &rootdir,
	                     &maxunique);
	if (code == 0) {
	    rootdirfound = 1;
	    newrootdir = 1;
	    salvinfo->VolumeChanged = 1;
	}
    }

    /* Parse each vnode looking for orphaned vnodes and
     * connect them to the tree as orphaned (if requested).
     */
    oldrootdir = rootdir;
    for (class = 0; class < nVNODECLASSES; class++) {
	for (v = 0; v < salvinfo->vnodeInfo[class].nVnodes; v++) {
	    vep = &(salvinfo->vnodeInfo[class].vnodes[v]);
	    ThisVnode = bitNumberToVnodeNumber(v, class);
	    ThisUnique = vep->unique;

	    if ((vep->type == 0) || vep->claimed || ThisVnode == 1)
		continue;	/* Ignore unused, claimed, and root vnodes */

	    /* This vnode is orphaned. If it is a directory vnode, then the '..'
	     * entry in this vnode had incremented the parent link count (In
	     * JudgeEntry()). We need to go to the parent and decrement that
	     * link count. But if the parent's unique is zero, then the parent
	     * link count was not incremented in JudgeEntry().
	     */
	    if (class == vLarge) {	/* directory vnode */
		pv = vnodeIdToBitNumber(vep->parent);
		if (salvinfo->vnodeInfo[vLarge].vnodes[pv].unique != 0) {
		    if (vep->parent == 1 && newrootdir) {
			/* this vnode's parent was the volume root, and
			 * we just created the volume root. So, the parent
			 * dir didn't exist during JudgeEntry, so the link
			 * count was not inc'd there, so don't dec it here.
			 */

			 /* noop */

		    } else {
			salvinfo->vnodeInfo[vLarge].vnodes[pv].count++;
		    }
		}
	    }

	    if (!rootdirfound)
		continue;	/* If no rootdir, can't attach orphaned files */

	    /* Here we attach orphaned files and directories into the
	     * root directory, LVVnode, making sure link counts stay correct.
	     */
	    if ((orphans == ORPH_ATTACH) && !vep->todelete && !Testing) {
		LFVnode = rootdir.vnodeNumber;	/* Lost+Found vnode number */
		LFUnique = rootdir.unique;	/* Lost+Found uniquifier */

		/* Update this orphaned vnode's info. Its parent info and
		 * link count (do for orphaned directories and files).
		 */
		vep->parent = LFVnode;	/* Parent is the root dir */
		vep->unique = LFUnique;
		vep->changed = 1;
		vep->claimed = 1;
		vep->count--;	/* Inc link count (root dir will pt to it) */

		/* If this orphaned vnode is a directory, change '..'.
		 * The name of the orphaned dir/file is unknown, so we
		 * build a unique name. No need to CopyOnWrite the directory
		 * since it is not connected to tree in BK or RO volume and
		 * won't be visible there.
		 */
		if (class == vLarge) {
		    AFSFid pa;
		    DirHandle dh;

		    /* Remove and recreate the ".." entry in this orphaned directory */
		    SetSalvageDirHandle(&dh, vid, salvinfo->fileSysDevice,
					salvinfo->vnodeInfo[class].inodes[v],
		                        &salvinfo->VolumeChanged);
		    pa.Vnode = LFVnode;
		    pa.Unique = LFUnique;
		    osi_Assert(Delete(&dh, "..") == 0);
		    osi_Assert(Create(&dh, "..", &pa) == 0);

		    /* The original parent's link count was decremented above.
		     * Here we increment the new parent's link count.
		     */
		    pv = vnodeIdToBitNumber(LFVnode);
		    salvinfo->vnodeInfo[vLarge].vnodes[pv].count--;

		}

		/* Go to the root dir and add this entry. The link count of the
		 * root dir was incremented when ".." was created. Try 10 times.
		 */
		for (j = 0; j < 10; j++) {
		    pa.Vnode = ThisVnode;
		    pa.Unique = ThisUnique;

		    (void)afs_snprintf(npath, sizeof npath, "%s.%u.%u",
				       ((class ==
					 vLarge) ? "__ORPHANDIR__" :
					"__ORPHANFILE__"), ThisVnode,
				       ThisUnique);

		    CopyOnWrite(salvinfo, &rootdir);
		    code = Create(&rootdir.dirHandle, npath, &pa);
		    if (!code)
			break;

		    ThisUnique += 50;	/* Try creating a different file */
		}
		osi_Assert(code == 0);
		Log("Attaching orphaned %s to volume's root dir as %s\n",
		    ((class == vLarge) ? "directory" : "file"), npath);
	    }
	}			/* for each vnode in the class */
    }				/* for each class of vnode */

    /* Delete the old rootinode directory if the rootdir was CopyOnWrite */
    DFlush();
    if (rootdirfound && !oldrootdir.copied && rootdir.copied) {
	code =
	    IH_DEC(oldrootdir.ds_linkH, oldrootdir.dirHandle.dirh_inode,
		   oldrootdir.rwVid);
	osi_Assert(code == 0);
	/* dirVnodeInfo->inodes[?] is not updated with new inode number */
    }

    DFlush();			/* Flush the changes */
    if (!rootdirfound && (orphans == ORPH_ATTACH)) {
	Log("Cannot attach orphaned files and directories: Root directory not found\n");
	orphans = ORPH_IGNORE;
    }

    /* Write out all changed vnodes. Orphaned files and directories
     * will get removed here also (if requested).
     */
    for (class = 0; class < nVNODECLASSES; class++) {
	afs_sfsize_t nVnodes = salvinfo->vnodeInfo[class].nVnodes;
	struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
	struct VnodeEssence *vnodes = salvinfo->vnodeInfo[class].vnodes;
	FilesInVolume += salvinfo->vnodeInfo[class].nAllocatedVnodes;
	BlocksInVolume += salvinfo->vnodeInfo[class].volumeBlockCount;
	for (i = 0; i < nVnodes; i++) {
	    struct VnodeEssence *vnp = &vnodes[i];
	    VnodeId vnodeNumber = bitNumberToVnodeNumber(i, class);

	    /* If the vnode is good but is unclaimed (not listed in
	     * any directory entries), then it is orphaned.
	     */
	    orphaned = -1;
	    if ((vnp->type != 0) && (orphaned = IsVnodeOrphaned(salvinfo, vnodeNumber))) {
		vnp->claimed = 0;	/* Makes IsVnodeOrphaned calls faster */
		vnp->changed = 1;
	    }

	    if (vnp->changed || vnp->count) {
		int oldCount;
		nBytes =
		    IH_IREAD(salvinfo->vnodeInfo[class].handle,
			     vnodeIndexOffset(vcp, vnodeNumber),
			     (char *)&vnode, sizeof(vnode));
		osi_Assert(nBytes == sizeof(vnode));

		vnode.parent = vnp->parent;
		oldCount = vnode.linkCount;
		vnode.linkCount = vnode.linkCount - vnp->count;

		if (orphaned == -1)
		    orphaned = IsVnodeOrphaned(salvinfo, vnodeNumber);
		if (orphaned) {
		    if (!vnp->todelete) {
			/* Orphans should have already been attached (if requested) */
			osi_Assert(orphans != ORPH_ATTACH);
			oblocks += vnp->blockCount;
			ofiles++;
		    }
		    if (((orphans == ORPH_REMOVE) || vnp->todelete)
			&& !Testing) {
			BlocksInVolume -= vnp->blockCount;
			FilesInVolume--;
			if (VNDISK_GET_INO(&vnode)) {
			    code =
				IH_DEC(alinkH, VNDISK_GET_INO(&vnode), vid);
			    osi_Assert(code == 0);
			}
			memset(&vnode, 0, sizeof(vnode));
		    }
		} else if (vnp->count) {
		    if (!Showmode) {
			Log("Vnode %u: link count incorrect (was %d, %s %d)\n", vnodeNumber, oldCount, (Testing ? "would have changed to" : "now"), vnode.linkCount);
		    }
		} else {
		    vnode.modeBits = vnp->modeBits;
		}

		vnode.dataVersion++;
		if (!Testing) {
		    nBytes =
			IH_IWRITE(salvinfo->vnodeInfo[class].handle,
				  vnodeIndexOffset(vcp, vnodeNumber),
				  (char *)&vnode, sizeof(vnode));
		    osi_Assert(nBytes == sizeof(vnode));
		}
		salvinfo->VolumeChanged = 1;
	    }
	}
    }
    if (!Showmode && ofiles) {
	Log("%s %d orphaned files and directories (approx. %u KB)\n",
	    (!Testing
	     && (orphans == ORPH_REMOVE)) ? "Removed" : "Found", ofiles,
	    oblocks);
    }

    for (class = 0; class < nVNODECLASSES; class++) {
	struct VnodeInfo *vip = &salvinfo->vnodeInfo[class];
	for (i = 0; i < vip->nVnodes; i++)
	    if (vip->vnodes[i].name)
		free(vip->vnodes[i].name);
	if (vip->vnodes)
	    free(vip->vnodes);
	if (vip->inodes)
	    free(vip->inodes);
    }

    /* Set correct resource utilization statistics */
    volHeader.filecount = FilesInVolume;
    volHeader.diskused = BlocksInVolume;

    /* Make sure the uniquifer is big enough: maxunique is the real maxUniquifier */
    if (volHeader.uniquifier < (maxunique + 1)) {
	if (!Showmode)
	    Log("Volume uniquifier is too low; fixed\n");
	/* Plus 2,000 in case there are workstations out there with
	 * cached vnodes that have since been deleted
	 */
	volHeader.uniquifier = (maxunique + 1 + 2000);
    }

    if (newrootdir) {
	Log("*** WARNING: Root directory recreated, but volume is fragile! "
	    "Only use this salvaged volume to copy data to another volume; "
	    "do not continue to use this volume (%lu) as-is.\n",
	    afs_printable_uint32_lu(vid));
    }

    if (!Testing && salvinfo->VolumeChanged) {
#ifdef FSSYNC_BUILD_CLIENT
	if (salvinfo->useFSYNC) {
	    afs_int32 fsync_code;

	    fsync_code = FSYNC_VolOp(vid, NULL, FSYNC_VOL_BREAKCBKS, FSYNC_SALVAGE, NULL);
	    if (fsync_code) {
		Log("Error trying to tell the fileserver to break callbacks for "
		    "changed volume %lu; error code %ld\n",
		    afs_printable_uint32_lu(vid),
		    afs_printable_int32_ld(fsync_code));
	    } else {
		salvinfo->VolumeChanged = 0;
	    }
	}
#endif /* FSSYNC_BUILD_CLIENT */

#ifdef AFS_DEMAND_ATTACH_FS
	if (!salvinfo->useFSYNC) {
	    /* A volume's contents have changed, but the fileserver will not
	     * break callbacks on the volume until it tries to load the vol
	     * header. So, to reduce the amount of time a client could have
	     * stale data, remove fsstate.dat, so the fileserver will init
	     * callback state with all clients. This is a very coarse hammer,
	     * and in the future we should just record which volumes have
	     * changed. */
	    code = unlink(AFSDIR_SERVER_FSSTATE_FILEPATH);
	    if (code && errno != ENOENT) {
		Log("Error %d when trying to unlink FS state file %s\n", errno,
		    AFSDIR_SERVER_FSSTATE_FILEPATH);
	    }
	}
#endif
    }

    /* Turn off the inUse bit; the volume's been salvaged! */
    volHeader.inUse = 0;	/* clear flag indicating inUse@last crash */
    volHeader.needsSalvaged = 0;	/* clear 'damaged' flag */
    volHeader.inService = 1;	/* allow service again */
    volHeader.needsCallback = (salvinfo->VolumeChanged != 0);
    volHeader.dontSalvage = DONT_SALVAGE;
    salvinfo->VolumeChanged = 0;
    if (!Testing) {
	nBytes = IH_IWRITE(h, 0, (char *)&volHeader, sizeof(volHeader));
	osi_Assert(nBytes == sizeof(volHeader));
    }
    if (!Showmode) {
	Log("%sSalvaged %s (%u): %d files, %d blocks\n",
	    (Testing ? "It would have " : ""), volHeader.name, volHeader.id,
	    FilesInVolume, BlocksInVolume);
    }

    IH_RELEASE(salvinfo->vnodeInfo[vSmall].handle);
    IH_RELEASE(salvinfo->vnodeInfo[vLarge].handle);
    IH_RELEASE(h);
    return 0;
}

void
ClearROInUseBit(struct VolumeSummary *summary)
{
    IHandle_t *h = summary->volumeInfoHandle;
    afs_sfsize_t nBytes;

    VolumeDiskData volHeader;

    nBytes = IH_IREAD(h, 0, (char *)&volHeader, sizeof(volHeader));
    osi_Assert(nBytes == sizeof(volHeader));
    osi_Assert(volHeader.stamp.magic == VOLUMEINFOMAGIC);
    volHeader.inUse = 0;
    volHeader.needsSalvaged = 0;
    volHeader.inService = 1;
    volHeader.dontSalvage = DONT_SALVAGE;
    if (!Testing) {
	nBytes = IH_IWRITE(h, 0, (char *)&volHeader, sizeof(volHeader));
	osi_Assert(nBytes == sizeof(volHeader));
    }
}

/* MaybeZapVolume
 * Possible delete the volume.
 *
 * deleteMe - Always do so, only a partial volume.
 */
void
MaybeZapVolume(struct SalvInfo *salvinfo, struct InodeSummary *isp,
               char *message, int deleteMe, int check)
{
    if (readOnly(isp) || deleteMe) {
	if (isp->volSummary && !isp->volSummary->deleted) {
	    if (deleteMe) {
		if (!Showmode)
		    Log("Volume %u (is only a partial volume--probably an attempt was made to move/restore it when a machine crash occured.\n", isp->volumeId);
		if (!Showmode)
		    Log("It will be deleted on this server (you may find it elsewhere)\n");
	    } else {
		if (!Showmode)
		    Log("Volume %u needs to be salvaged.  Since it is read-only, however,\n", isp->volumeId);
		if (!Showmode)
		    Log("it will be deleted instead.  It should be recloned.\n");
	    }
	    if (!Testing) {
		afs_int32 code;
		char path[64];
		char filename[VMAXPATHLEN];
		VolumeExternalName_r(isp->volumeId, filename, sizeof(filename));
		sprintf(path, "%s" OS_DIRSEP "%s", salvinfo->fileSysPath, filename);

		code = VDestroyVolumeDiskHeader(salvinfo->fileSysPartition, isp->volumeId, isp->RWvolumeId);
		if (code) {
		    Log("Error %ld destroying volume disk header for volume %lu\n",
		        afs_printable_int32_ld(code),
		        afs_printable_uint32_lu(isp->volumeId));
		}

		/* make sure we actually delete the header file; ENOENT
		 * is fine, since VDestroyVolumeDiskHeader probably already
		 * unlinked it */
		if (unlink(path) && errno != ENOENT) {
		    Log("Unable to unlink %s (errno = %d)\n", path, errno);
		}
		if (salvinfo->useFSYNC) {
		    AskDelete(salvinfo, isp->volumeId);
		}
		isp->volSummary->deleted = 1;
	    }
	}
    } else if (!check) {
	Log("%s salvage was unsuccessful: read-write volume %u\n", message,
	    isp->volumeId);
	Abort("Salvage of volume %u aborted\n", isp->volumeId);
    }
}

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * Locks a volume on disk for salvaging.
 *
 * @param[in] volumeId   volume ID to lock
 *
 * @return operation status
 *  @retval 0  success
 *  @retval -1 volume lock raced with a fileserver restart; all volumes must
 *             checked out and locked again
 *
 * @note DAFS only
 */
static int
LockVolume(struct SalvInfo *salvinfo, VolumeId volumeId)
{
    afs_int32 code;
    int locktype;

    /* should always be WRITE_LOCK, but keep the lock-type logic all
     * in one place, in VVolLockType. Params will be ignored, but
     * try to provide what we're logically doing. */
    locktype = VVolLockType(V_VOLUPD, 1);

    code = VLockVolumeByIdNB(volumeId, salvinfo->fileSysPartition, locktype);
    if (code) {
	if (code == EBUSY) {
	    Abort("Someone else appears to be using volume %lu; Aborted\n",
	          afs_printable_uint32_lu(volumeId));
	}
	Abort("Error %ld trying to lock volume %lu; Aborted\n",
	      afs_printable_int32_ld(code),
	      afs_printable_uint32_lu(volumeId));
    }

    code = FSYNC_VerifyCheckout(volumeId, salvinfo->fileSysPartition->name, FSYNC_VOL_OFF, FSYNC_SALVAGE);
    if (code == SYNC_DENIED) {
	/* need to retry checking out volumes */
	return -1;
    }
    if (code != SYNC_OK) {
	Abort("FSYNC_VerifyCheckout failed for volume %lu with code %ld\n",
	      afs_printable_uint32_lu(volumeId), afs_printable_int32_ld(code));
    }

    /* set inUse = programType in the volume header to ensure that nobody
     * tries to use this volume again without salvaging, if we somehow crash
     * or otherwise exit before finishing the salvage.
     */
    if (!Testing) {
       IHandle_t *h;
       struct VolumeHeader header;
       struct VolumeDiskHeader diskHeader;
       struct VolumeDiskData volHeader;

       code = VReadVolumeDiskHeader(volumeId, salvinfo->fileSysPartition, &diskHeader);
       if (code) {
           return 0;
       }

       DiskToVolumeHeader(&header, &diskHeader);

       IH_INIT(h, salvinfo->fileSysDevice, header.parent, header.volumeInfo);
       if (IH_IREAD(h, 0, (char*)&volHeader, sizeof(volHeader)) != sizeof(volHeader) ||
           volHeader.stamp.magic != VOLUMEINFOMAGIC) {

           IH_RELEASE(h);
           return 0;
       }

       volHeader.inUse = programType;

       /* If we can't re-write the header, bail out and error. We don't
        * assert when reading the header, since it's possible the
        * header isn't really there (when there's no data associated
        * with the volume; we just delete the vol header file in that
        * case). But if it's there enough that we can read it, but
        * somehow we cannot write to it to signify we're salvaging it,
        * we've got a big problem and we cannot continue. */
       osi_Assert(IH_IWRITE(h, 0, (char*)&volHeader, sizeof(volHeader)) == sizeof(volHeader));

       IH_RELEASE(h);
    }

    return 0;
}
#endif /* AFS_DEMAND_ATTACH_FS */

static void
AskError(struct SalvInfo *salvinfo, VolumeId volumeId)
{
#if defined(AFS_DEMAND_ATTACH_FS) || defined(AFS_DEMAND_ATTACH_UTIL)
    afs_int32 code;
    code = FSYNC_VolOp(volumeId, salvinfo->fileSysPartition->name,
                       FSYNC_VOL_FORCE_ERROR, FSYNC_WHATEVER, NULL);
    if (code != SYNC_OK) {
	Log("AskError: failed to force volume %lu into error state; "
	    "SYNC error code %ld (%s)\n", (long unsigned)volumeId,
	    (long)code, SYNC_res2string(code));
    }
#endif /* AFS_DEMAND_ATTACH_FS || AFS_DEMAND_ATTACH_UTIL */
}

void
AskOffline(struct SalvInfo *salvinfo, VolumeId volumeId)
{
    afs_int32 code, i;
    SYNC_response res;

    memset(&res, 0, sizeof(res));

    for (i = 0; i < 3; i++) {
	code = FSYNC_VolOp(volumeId, salvinfo->fileSysPartition->name,
	                   FSYNC_VOL_OFF, FSYNC_SALVAGE, &res);

	if (code == SYNC_OK) {
	    break;
	} else if (code == SYNC_DENIED) {
	    if (AskDAFS())
		Log("AskOffline:  file server denied offline request; a general salvage may be required.\n");
	    else
		Log("AskOffline:  file server denied offline request; a general salvage is required.\n");
	    Abort("Salvage aborted\n");
	} else if (code == SYNC_BAD_COMMAND) {
	    Log("AskOffline:  fssync protocol mismatch (bad command word '%d'); salvage aborting.\n",
		FSYNC_VOL_OFF);
	    if (AskDAFS()) {
#ifdef AFS_DEMAND_ATTACH_FS
		Log("AskOffline:  please make sure dafileserver, davolserver, salvageserver and dasalvager binaries are same version.\n");
#else
		Log("AskOffline:  fileserver is DAFS but we are not.\n");
#endif
	    } else {
#ifdef AFS_DEMAND_ATTACH_FS
		Log("AskOffline:  fileserver is not DAFS but we are.\n");
#else
		Log("AskOffline:  please make sure fileserver, volserver and salvager binaries are same version.\n");
#endif
	    }
	    Abort("Salvage aborted\n");
	} else if (i < 2) {
	    /* try it again */
	    Log("AskOffline:  request for fileserver to take volume offline failed; trying again...\n");
	    FSYNC_clientFinis();
	    FSYNC_clientInit();
	}
    }
    if (code != SYNC_OK) {
	Log("AskOffline:  request for fileserver to take volume offline failed; salvage aborting.\n");
	Abort("Salvage aborted\n");
    }
}

/* don't want to pass around state; remember it here */
static int isDAFS = -1;
int
AskDAFS(void)
{
    SYNC_response res;
    afs_int32 code = 1, i;

    /* we don't care if we race. the answer shouldn't change */
    if (isDAFS != -1)
	return isDAFS;

    memset(&res, 0, sizeof(res));

    for (i = 0; code && i < 3; i++) {
	code = FSYNC_VolOp(0, NULL, FSYNC_VOL_LISTVOLUMES, FSYNC_SALVAGE, &res);
	if (code) {
	    Log("AskDAFS: FSYNC_VOL_LISTVOLUMES failed with code %ld reason "
	        "%ld (%s); trying again...\n", (long)code, (long)res.hdr.reason,
	        FSYNC_reason2string(res.hdr.reason));
	    FSYNC_clientFinis();
	    FSYNC_clientInit();
	}
    }

    if (code) {
	Log("AskDAFS: could not determine DAFS-ness, assuming not DAFS\n");
	res.hdr.flags = 0;
    }

    if ((res.hdr.flags & SYNC_FLAG_DAFS_EXTENSIONS)) {
	isDAFS = 1;
    } else {
	isDAFS = 0;
    }

    return isDAFS;
}

static void
MaybeAskOnline(struct SalvInfo *salvinfo, VolumeId volumeId)
{
    struct VolumeDiskHeader diskHdr;
    int code;
    code = VReadVolumeDiskHeader(volumeId, salvinfo->fileSysPartition, &diskHdr);
    if (code) {
	/* volume probably does not exist; no need to bring back online */
	return;
    }
    AskOnline(salvinfo, volumeId);
}

void
AskOnline(struct SalvInfo *salvinfo, VolumeId volumeId)
{
    afs_int32 code, i;

    for (i = 0; i < 3; i++) {
	code = FSYNC_VolOp(volumeId, salvinfo->fileSysPartition->name,
	                   FSYNC_VOL_ON, FSYNC_WHATEVER, NULL);

	if (code == SYNC_OK) {
	    break;
	} else if (code == SYNC_DENIED) {
	    Log("AskOnline:  file server denied online request to volume %u partition %s; trying again...\n", volumeId, salvinfo->fileSysPartition->name);
	} else if (code == SYNC_BAD_COMMAND) {
	    Log("AskOnline:  fssync protocol mismatch (bad command word '%d')\n",
		FSYNC_VOL_ON);
	    Log("AskOnline:  please make sure file server binaries are same version.\n");
	    break;
	} else if (i < 2) {
	    /* try it again */
	    Log("AskOnline:  request for fileserver to put volume online failed; trying again...\n");
	    FSYNC_clientFinis();
	    FSYNC_clientInit();
	}
    }
}

void
AskDelete(struct SalvInfo *salvinfo, VolumeId volumeId)
{
    afs_int32 code, i;
    SYNC_response res;

    for (i = 0; i < 3; i++) {
	memset(&res, 0, sizeof(res));
	code = FSYNC_VolOp(volumeId, salvinfo->fileSysPartition->name,
	                   FSYNC_VOL_DONE, FSYNC_SALVAGE, &res);

	if (code == SYNC_OK) {
	    break;
	} else if (code == SYNC_DENIED) {
	    Log("AskOnline:  file server denied DONE request to volume %u partition %s; trying again...\n", volumeId, salvinfo->fileSysPartition->name);
	} else if (code == SYNC_BAD_COMMAND) {
	    Log("AskOnline:  fssync protocol mismatch (bad command word '%d')\n",
		FSYNC_VOL_DONE);
	    if (AskDAFS()) {
#ifdef AFS_DEMAND_ATTACH_FS
		Log("AskOnline:  please make sure dafileserver, davolserver, salvageserver and dasalvager binaries are same version.\n");
#else
		Log("AskOnline:  fileserver is DAFS but we are not.\n");
#endif
	    } else {
#ifdef AFS_DEMAND_ATTACH_FS
		Log("AskOnline:  fileserver is not DAFS but we are.\n");
#else
		Log("AskOnline:  please make sure fileserver, volserver and salvager binaries are same version.\n");
#endif
	    }
	    break;
	} else if (code == SYNC_FAILED &&
	             (res.hdr.reason == FSYNC_UNKNOWN_VOLID ||
	              res.hdr.reason == FSYNC_WRONG_PART)) {
	    /* volume is already effectively 'deleted' */
	    break;
	} else if (i < 2) {
	    /* try it again */
	    Log("AskOnline:  request for fileserver to delete volume failed; trying again...\n");
	    FSYNC_clientFinis();
	    FSYNC_clientInit();
	}
    }
}

int
CopyInode(Device device, Inode inode1, Inode inode2, int rwvolume)
{
    /* Volume parameter is passed in case iopen is upgraded in future to
     * require a volume Id to be passed
     */
    char buf[4096];
    IHandle_t *srcH, *destH;
    FdHandle_t *srcFdP, *destFdP;
    ssize_t nBytes = 0;
    afs_foff_t size = 0;

    IH_INIT(srcH, device, rwvolume, inode1);
    srcFdP = IH_OPEN(srcH);
    osi_Assert(srcFdP != NULL);
    IH_INIT(destH, device, rwvolume, inode2);
    destFdP = IH_OPEN(destH);
    while ((nBytes = FDH_PREAD(srcFdP, buf, sizeof(buf), size)) > 0) {
	osi_Assert(FDH_PWRITE(destFdP, buf, nBytes, size) == nBytes);
	size += nBytes;
    }
    osi_Assert(nBytes == 0);
    FDH_REALLYCLOSE(srcFdP);
    FDH_REALLYCLOSE(destFdP);
    IH_RELEASE(srcH);
    IH_RELEASE(destH);
    return 0;
}

void
PrintInodeList(struct SalvInfo *salvinfo)
{
    struct ViceInodeInfo *ip;
    struct ViceInodeInfo *buf;
    struct afs_stat status;
    int nInodes;
    afs_ino_str_t stmp;

    osi_Assert(afs_fstat(salvinfo->inodeFd, &status) == 0);
    buf = (struct ViceInodeInfo *)malloc(status.st_size);
    osi_Assert(buf != NULL);
    nInodes = status.st_size / sizeof(struct ViceInodeInfo);
    osi_Assert(read(salvinfo->inodeFd, buf, status.st_size) == status.st_size);
    for (ip = buf; nInodes--; ip++) {
	Log("Inode:%s, linkCount=%d, size=%#llx, p=(%u,%u,%u,%u)\n",
	    PrintInode(stmp, ip->inodeNumber), ip->linkCount,
	    (afs_uintmax_t) ip->byteCount, ip->u.param[0], ip->u.param[1],
	    ip->u.param[2], ip->u.param[3]);
    }
    free(buf);
}

void
PrintInodeSummary(struct SalvInfo *salvinfo)
{
    int i;
    struct InodeSummary *isp;

    for (i = 0; i < salvinfo->nVolumesInInodeFile; i++) {
	isp = &salvinfo->inodeSummary[i];
	Log("VID:%u, RW:%u, index:%d, nInodes:%d, nSpecialInodes:%d, maxUniquifier:%u, volSummary\n", isp->volumeId, isp->RWvolumeId, isp->index, isp->nInodes, isp->nSpecialInodes, isp->maxUniquifier);
    }
}

int
Fork(void)
{
    int f;
#ifdef AFS_NT40_ENV
    f = 0;
    osi_Assert(0);			/* Fork is never executed in the NT code path */
#else
    f = fork();
    osi_Assert(f >= 0);
#ifdef AFS_DEMAND_ATTACH_FS
    if ((f == 0) && (programType == salvageServer)) {
	/* we are a salvageserver child */
#ifdef FSSYNC_BUILD_CLIENT
	VChildProcReconnectFS_r();
#endif
#ifdef SALVSYNC_BUILD_CLIENT
	VReconnectSALV_r();
#endif
    }
#endif /* AFS_DEMAND_ATTACH_FS */
#endif /* !AFS_NT40_ENV */
    return f;
}

void
Exit(int code)
{
    if (ShowLog)
	showlog();

#ifdef AFS_DEMAND_ATTACH_FS
    if (programType == salvageServer) {
	/* release all volume locks before closing down our SYNC channels.
	 * the fileserver may try to online volumes we have checked out when
	 * we close down FSSYNC, so we should make sure we don't have those
	 * volumes locked when it does */
	struct DiskPartition64 *dp;
	int i;
	for (i = 0; i <= VOLMAXPARTS; i++) {
	    dp = VGetPartitionById(i, 0);
	    if (dp) {
		VLockFileReinit(&dp->volLockFile);
	    }
	}
# ifdef SALVSYNC_BUILD_CLIENT
	VDisconnectSALV();
# endif
# ifdef FSSYNC_BUILD_CLIENT
	VDisconnectFS();
# endif
    }
#endif /* AFS_DEMAND_ATTACH_FS */

#ifdef AFS_NT40_ENV
    if (main_thread != pthread_self())
	pthread_exit((void *)code);
    else
	exit(code);
#else
    exit(code);
#endif
}

int
Wait(char *prog)
{
    int status;
    int pid;
    pid = wait(&status);
    osi_Assert(pid != -1);
    if (WCOREDUMP(status))
	Log("\"%s\" core dumped!\n", prog);
    if (WIFSIGNALED(status) != 0 || WEXITSTATUS(status) != 0)
	return -1;
    return pid;
}

static char *
TimeStamp(time_t clock, int precision)
{
    struct tm *lt;
    static char timestamp[20];
    lt = localtime(&clock);
    if (precision)
	(void)strftime(timestamp, 20, "%m/%d/%Y %H:%M:%S", lt);
    else
	(void)strftime(timestamp, 20, "%m/%d/%Y %H:%M", lt);
    return timestamp;
}

void
CheckLogFile(char * log_path)
{
    char oldSlvgLog[AFSDIR_PATH_MAX];

#ifndef AFS_NT40_ENV
    if (useSyslog) {
	ShowLog = 0;
	return;
    }
#endif

    strcpy(oldSlvgLog, log_path);
    strcat(oldSlvgLog, ".old");
    if (!logFile) {
	renamefile(log_path, oldSlvgLog);
	logFile = afs_fopen(log_path, "a");

	if (!logFile) {		/* still nothing, use stdout */
	    logFile = stdout;
	    ShowLog = 0;
	}
#ifndef AFS_NAMEI_ENV
	AFS_DEBUG_IOPS_LOG(logFile);
#endif
    }
}

#ifndef AFS_NT40_ENV
void
TimeStampLogFile(char * log_path)
{
    char stampSlvgLog[AFSDIR_PATH_MAX];
    struct tm *lt;
    time_t now;

    now = time(0);
    lt = localtime(&now);
    (void)afs_snprintf(stampSlvgLog, sizeof stampSlvgLog,
		       "%s.%04d-%02d-%02d.%02d:%02d:%02d",
		       log_path, lt->tm_year + 1900,
		       lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min,
		       lt->tm_sec);

    /* try to link the logfile to a timestamped filename */
    /* if it fails, oh well, nothing we can do */
    if (link(log_path, stampSlvgLog))
	; /* oh well */
}
#endif

void
showlog(void)
{
    char line[256];

#ifndef AFS_NT40_ENV
    if (useSyslog) {
	printf("Can't show log since using syslog.\n");
	fflush(stdout);
	return;
    }
#endif

    if (logFile) {
	rewind(logFile);
	fclose(logFile);
    }

    logFile = afs_fopen(AFSDIR_SERVER_SLVGLOG_FILEPATH, "r");

    if (!logFile)
	printf("Can't read %s, exiting\n", AFSDIR_SERVER_SLVGLOG_FILEPATH);
    else {
	rewind(logFile);
	while (fgets(line, sizeof(line), logFile))
	    printf("%s", line);
	fflush(stdout);
    }
}

void
Log(const char *format, ...)
{
    struct timeval now;
    char tmp[1024];
    va_list args;

    va_start(args, format);
    (void)afs_vsnprintf(tmp, sizeof tmp, format, args);
    va_end(args);
#ifndef AFS_NT40_ENV
    if (useSyslog) {
	syslog(LOG_INFO, "%s", tmp);
    } else
#endif
	if (logFile) {
	    gettimeofday(&now, 0);
	    fprintf(logFile, "%s %s", TimeStamp(now.tv_sec, 1), tmp);
	    fflush(logFile);
	}
}

void
Abort(const char *format, ...)
{
    va_list args;
    char tmp[1024];

    va_start(args, format);
    (void)afs_vsnprintf(tmp, sizeof tmp, format, args);
    va_end(args);
#ifndef AFS_NT40_ENV
    if (useSyslog) {
	syslog(LOG_INFO, "%s", tmp);
    } else
#endif
	if (logFile) {
	    fprintf(logFile, "%s", tmp);
	    fflush(logFile);
	    if (ShowLog)
		showlog();
	}

    if (debug)
	abort();
    Exit(1);
}

char *
ToString(const char *s)
{
    char *p;
    p = (char *)malloc(strlen(s) + 1);
    osi_Assert(p != NULL);
    strcpy(p, s);
    return p;
}

/* Remove the FORCESALVAGE file */
void
RemoveTheForce(char *path)
{
    char target[1024];
    struct afs_stat force; /* so we can use afs_stat to find it */
    strcpy(target,path);
    strcat(target,"/FORCESALVAGE");
    if (!Testing && ForceSalvage) {
        if (afs_stat(target,&force) == 0)  unlink(target);
    }
}

#ifndef AFS_AIX32_ENV
/*
 * UseTheForceLuke -	see if we can use the force
 */
int
UseTheForceLuke(char *path)
{
    struct afs_stat force;
    char target[1024];
    strcpy(target,path);
    strcat(target,"/FORCESALVAGE");

    return (afs_stat(target, &force) == 0);
}
#else
/*
 * UseTheForceLuke -	see if we can use the force
 *
 * NOTE:
 *	The VRMIX fsck will not muck with the filesystem it is supposedly
 *	fixing and create a "FORCESALVAGE" file (by design).  Instead, we
 *	muck directly with the root inode, which is within the normal
 *	domain of fsck.
 *	ListViceInodes() has a side effect of setting ForceSalvage if
 *	it detects a need, based on root inode examination.
 */
int
UseTheForceLuke(char *path)
{

    return 0;			/* sorry OB1    */
}
#endif

#ifdef AFS_NT40_ENV
/* NT support routines */

static char execpathname[MAX_PATH];
int
nt_SalvagePartition(char *partName, int jobn)
{
    int pid;
    int n;
    childJob_t job;
    if (!*execpathname) {
	n = GetModuleFileName(NULL, execpathname, MAX_PATH - 1);
	if (!n || n == 1023)
	    return -1;
    }
    job.cj_magic = SALVAGER_MAGIC;
    job.cj_number = jobn;
    (void)strcpy(job.cj_part, partName);
    pid = (int)spawnprocveb(execpathname, save_args, NULL, &job, sizeof(job));
    return pid;
}

int
nt_SetupPartitionSalvage(void *datap, int len)
{
    childJob_t *jobp = (childJob_t *) datap;
    char logname[AFSDIR_PATH_MAX];

    if (len != sizeof(childJob_t))
	return -1;
    if (jobp->cj_magic != SALVAGER_MAGIC)
	return -1;
    myjob = *jobp;

    /* Open logFile */
    (void)sprintf(logname, "%s.%d", AFSDIR_SERVER_SLVGLOG_FILEPATH,
		  myjob.cj_number);
    logFile = afs_fopen(logname, "w");
    if (!logFile)
	logFile = stdout;

    return 0;
}


#endif /* AFS_NT40_ENV */
