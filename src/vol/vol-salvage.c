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


#define SalvageVersion "2.4"

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
#include "fssync.h"
#include "viceinode.h"
#include "salvage.h"
#include "volinodes.h"		/* header magic number, etc. stuff */
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

#define ORPH_IGNORE 0
#define ORPH_REMOVE 1
#define ORPH_ATTACH 2


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

#define	MAXPARALLEL	32

int OKToZap;			/* -o flag */
int ForceSalvage;		/* If salvage should occur despite the DONT_SALVAGE flag
				 * in the volume header */

static FILE *logFile = 0;	/* one of {/usr/afs/logs,/vice/file}/SalvageLog */

#define ROOTINODE	2	/* Root inode of a 4.2 Unix file system
				 * partition */
Device fileSysDevice;		/* The device number of the current
				 * partition being salvaged */
#ifdef AFS_NT40_ENV
char fileSysPath[8];
#else
char *fileSysPath;		/* The path of the mounted partition currently
				 * being salvaged, i.e. the directory
				 * containing the volume headers */
#endif
char *fileSysPathName;		/* NT needs this to make name pretty in log. */
IHandle_t *VGLinkH;		/* Link handle for current volume group. */
int VGLinkH_cnt;		/* # of references to lnk handle. */
struct DiskPartition *fileSysPartition;	/* Partition  being salvaged */
#ifndef AFS_NT40_ENV
char *fileSysDeviceName;	/* The block device where the file system
				 * being salvaged was mounted */
char *filesysfulldev;
#endif
int VolumeChanged;		/* Set by any routine which would change the volume in
				 * a way which would require callback is to be broken if the
				 * volume was put back on line by an active file server */

VolumeDiskData VolInfo;		/* A copy of the last good or salvaged volume header dealt with */

struct InodeSummary {		/* Inode summary file--an entry for each
				 * volume in the inode file for a partition */
    VolId volumeId;		/* Volume id */
    VolId RWvolumeId;		/* RW volume associated */
    int index;			/* index into inode file (0, 1, 2 ...) */
    int nInodes;		/* Number of inodes for this volume */
    int nSpecialInodes;		/* Number of special inodes, i.e.  volume
				 * header, index, etc.  These are all
				 * marked (viceinode.h) and will all be sorted
				 * to the beginning of the information for
				 * this volume.  Read-only volumes should
				 * ONLY have special inodes (all the other
				 * inodes look as if they belong to the
				 * original RW volume). */
    Unique maxUniquifier;	/* The maximum uniquifier found in all the inodes.
				 * This is only useful for RW volumes and is used
				 * to compute a new volume uniquifier in the event
				 * that the header needs to be recreated. The inode
				 * uniquifier may be a truncated version of vnode
				 * uniquifier (AFS_3DISPARES). The real maxUniquifer
				 * is from the vnodes and later calcuated from it */
    struct VolumeSummary *volSummary;
    /* Either a pointer to the original volume
     * header summary, or constructed summary
     * information */
} *inodeSummary;
#define readOnly(isp)	((isp)->volumeId != (isp)->RWvolumeId)
int nVolumesInInodeFile;	/* Number of read-write volumes summarized */
int inodeFd;			/* File descriptor for inode file */


struct VolumeSummary {		/* Volume summary an entry for each
				 * volume in a volume directory.
				 * Assumption: one volume directory per
				 * partition */
    char *fileName;		/* File name on the partition for the volume
				 * header */
    struct VolumeHeader header;
    /* volume number, rw volume number, inode
     * numbers of each major component of
     * the volume */
    IHandle_t *volumeInfoHandle;
    byte wouldNeedCallback;	/* set if the file server should issue
				 * call backs for all the files in this volume when
				 * the volume goes back on line */
};

struct VnodeInfo {
    IHandle_t *handle;		/* Inode containing this index */
    int nVnodes;		/* Total number of vnodes in index */
    int nAllocatedVnodes;	/* Total number actually used */
    int volumeBlockCount;	/* Total number of blocks used by volume */
    Inode *inodes;		/* Directory only */
    struct VnodeEssence {
	short count;		/* Number of references to vnode; MUST BE SIGNED */
	unsigned claimed:1;	/* Set when a parent directory containing an entry
				 * referencing this vnode is found.  The claim
				 * is that the parent in "parent" can point to
				 * this vnode, and no other */
	unsigned changed:1;	/* Set if any parameters (other than the count)
				 * in the vnode change.   It is determined if the
				 * link count has changed by noting whether it is
				 * 0 after scanning all directories */
	unsigned salvaged:1;	/* Set if this directory vnode has already been salvaged. */
	unsigned todelete:1;	/* Set if this vnode is to be deleted (should not be claimed) */
	afs_fsize_t blockCount;
	/* Number of blocks (1K) used by this vnode,
	 * approximately */
	VnodeId parent;		/* parent in vnode */
	Unique unique;		/* Must match entry! */
	char *name;		/* Name of directory entry */
	int modeBits;		/* File mode bits */
	Inode InodeNumber;	/* file's inode */
	int type;		/* File type */
	int author;		/* File author */
	int owner;		/* File owner */
	int group;		/* File group */
    } *vnodes;
} vnodeInfo[nVNODECLASSES];

struct DirSummary {
    struct DirHandle dirHandle;
    VnodeId vnodeNumber;
    Unique unique;
    unsigned haveDot, haveDotDot;
    VolumeId rwVid;
    int copied;			/* If the copy-on-write stuff has been applied */
    VnodeId parent;
    char *name;
    char *vname;
    IHandle_t *ds_linkH;
};


struct VolumeSummary *volumeSummaryp;	/* Holds all the volumes in a part */
int nVolumes;			/* Number of volumes (read-write and read-only)
				 * in volume summary */

#ifdef AFS_NT40_ENV
/* For NT, we can fork the per partition salvagers to gain the required
 * safety against Aborts. But there's too many complex data structures at
 * the per volume salvager layer to easilty copy the data across.
 * childJobNumber is resset from -1 to the job number if this is a
 * per partition child of the main salvager. This information is passed
 * out-of-band in the extra data area setup for the now unused parent/child
 * data transfer.
 */
#define SALVAGER_MAGIC 0x00BBaaDD
#define NOT_CHILD -1		/* job numbers start at 0 */
/* If new options need to be passed to child, add them here. */
typedef struct {
    int cj_magic;
    int cj_number;
    char cj_part[32];
} childJob_t;

/* Child job this process is running. */
childJob_t myjob = { SALVAGER_MAGIC, NOT_CHILD, "" };

int nt_SalvagePartition(char *partName, int jobn);
int nt_SetupPartitionSalvage(void *datap, int len);

typedef struct {
    struct InodeSummary *svgp_inodeSummaryp;
    int svgp_count;
} SVGParms_t;
#define canfork 0
#else
#define canfork 1
#endif



/* Forward declarations */
/*@printflike@*/ void Log(const char *format, ...);
/*@printflike@*/ void Abort(const char *format, ...);
void Exit(int code);
int Fork(void);
int Wait(char *prog);
char *ToString(char *s);
void AskOffline(VolumeId volumeId);
void AskOnline(VolumeId volumeId, char *partition);
void CheckLogFile(void);
#ifndef AFS_NT40_ENV
void TimeStampLogFile(void);
#endif
void ClearROInUseBit(struct VolumeSummary *summary);
void CopyAndSalvage(register struct DirSummary *dir);
int CopyInode(Device device, Inode inode1, Inode inode2, int rwvolume);
void CopyOnWrite(register struct DirSummary *dir);
void CountVolumeInodes(register struct ViceInodeInfo *ip, int maxInodes,
		       register struct InodeSummary *summary);
void DeleteExtraVolumeHeaderFile(register struct VolumeSummary *vsp);
void DistilVnodeEssence(VolumeId vid, VnodeClass class, Inode ino,
			Unique * maxu);
int GetInodeSummary(char *path, VolumeId singleVolumeNumber);
void GetVolumeSummary(VolumeId singleVolumeNumber);
void JudgeEntry(struct DirSummary *dir, char *name, VnodeId vnodeNumber,
		Unique unique);
void MaybeZapVolume(register struct InodeSummary *isp, char *message,
		    int deleteMe, int check);
void ObtainSalvageLock(void);
void PrintInodeList(void);
void PrintInodeSummary(void);
void PrintVolumeSummary(void);
int QuickCheck(register struct InodeSummary *isp, int nVols);
void RemoveTheForce(char *path);
void SalvageDir(char *name, VolumeId rwVid, struct VnodeInfo *dirVnodeInfo,
		IHandle_t * alinkH, int i, struct DirSummary *rootdir,
		int *rootdirfound);
void SalvageFileSysParallel(struct DiskPartition *partP);
void SalvageFileSys(struct DiskPartition *partP, VolumeId singleVolumeNumber);
void SalvageFileSys1(struct DiskPartition *partP,
		     VolumeId singleVolumeNumber);
int SalvageHeader(register struct stuff *sp, struct InodeSummary *isp,
		  int check, int *deleteMe);
int SalvageIndex(Inode ino, VnodeClass class, int RW,
		 register struct ViceInodeInfo *ip, int nInodes,
		 struct VolumeSummary *volSummary, int check);
int SalvageVnodes(register struct InodeSummary *rwIsp,
		  register struct InodeSummary *thisIsp,
		  register struct ViceInodeInfo *inodes, int check);
int SalvageVolume(register struct InodeSummary *rwIsp, IHandle_t * alinkH);
void DoSalvageVolumeGroup(register struct InodeSummary *isp, int nVols);
#ifdef AFS_NT40_ENV
void SalvageVolumeGroup(register struct InodeSummary *isp, int nVols);
#else
#define SalvageVolumeGroup DoSalvageVolumeGroup
#endif
int SalvageVolumeHeaderFile(register struct InodeSummary *isp,
			    register struct ViceInodeInfo *inodes, int RW,
			    int check, int *deleteMe);
void showlog(void);
int UseTheForceLuke(char *path);

static int IsVnodeOrphaned(VnodeId vnode);

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
BadError(register int aerror)
{
    if (aerror == EPERM || aerror == ENXIO || aerror == ENOENT)
	return 1;
    return 0;			/* otherwise may be transient, e.g. EMFILE */
}


char *tmpdir = 0;
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
	TimeStampLogFile();
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
#endif /* FAST_RESTART */

    /* Note:  if seemvol we initialize this as a standard volume utility:  this has the
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

	CheckLogFile();
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
	ObtainSalvageLock();
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
#endif /* FAST_RESTART */
    err = cmd_Dispatch(argc, argv);
    Exit(err);
}

/* Get the salvage lock if not already held. Hold until process exits. */
void
ObtainSalvageLock(void)
{
    int salvageLock;

#ifdef AFS_NT40_ENV
    salvageLock =
	(int)CreateFile(AFSDIR_SERVER_SLVGLOCK_FILEPATH, 0, 0, NULL,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (salvageLock == (int)INVALID_HANDLE_VALUE) {
	fprintf(stderr,
		"salvager:  There appears to be another salvager running!  Aborted.\n");
	Exit(1);
    }
#else
    salvageLock =
	afs_open(AFSDIR_SERVER_SLVGLOCK_FILEPATH, O_CREAT | O_RDWR, 0666);
    if (salvageLock < 0) {
	fprintf(stderr,
		"salvager:  can't open salvage lock file %s, aborting\n",
		AFSDIR_SERVER_SLVGLOCK_FILEPATH);
	Exit(1);
    }
#ifdef AFS_DARWIN_ENV
    if (flock(salvageLock, LOCK_EX) == -1) {
#else
    if (lockf(salvageLock, F_LOCK, 0) == -1) {
#endif
	fprintf(stderr,
		"salvager:  There appears to be another salvager running!  Aborted.\n");
	Exit(1);
    }
#endif
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

    assert(mntfp = setmntent(MOUNTED, "r"));
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
SameDisk(struct DiskPartition *p1, struct DiskPartition *p2)
{
#define RES_LEN 256
    char res[RES_LEN];
    int d1, d2;
    static int dowarn = 1;

    if (!QueryDosDevice(p1->devName, res, RES_LEN - 1))
	return 1;
    if (strncmp(res, HDSTR, HDLEN)) {
	if (dowarn) {
	    dowarn = 0;
	    Log("WARNING: QueryDosDevice is returning %s, not %s for %s\n",
		res, HDSTR, p1->devName);
	}
	return 1;
    }
    d1 = atoi(&res[HDLEN]);

    if (!QueryDosDevice(p2->devName, res, RES_LEN - 1))
	return 1;
    if (strncmp(res, HDSTR, HDLEN)) {
	if (dowarn) {
	    dowarn = 0;
	    Log("WARNING: QueryDosDevice is returning %s, not %s for %s\n",
		res, HDSTR, p2->devName);
	}
	return 1;
    }
    d2 = atoi(&res[HDLEN]);

    return d1 == d2;
}
#else
#define SameDisk(P1, P2) ((P1)->device/PartsPerDisk == (P2)->device/PartsPerDisk)
#endif

/* This assumes that two partitions with the same device number divided by
 * PartsPerDisk are on the same disk.
 */
void
SalvageFileSysParallel(struct DiskPartition *partP)
{
    struct job {
	struct DiskPartition *partP;
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
	    assert(pid != -1);
	    for (j = 0; j < numjobs; j++) {	/* Find which job it is */
		if (pid == jobs[j]->pid)
		    break;
	    }
	    assert(j < numjobs);
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
		open("/", 0);
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
SalvageFileSys(struct DiskPartition *partP, VolumeId singleVolumeNumber)
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
    ptr = (char *)strrchr(pbuf, '/');
    if (ptr) {
	*ptr = '\0';
	strcpy(wpath, pbuf);
    } else
	return NULL;
    ptr = (char *)strrchr(pbuffer, '/');
    if (ptr) {
	strcpy(pbuffer, ptr + 1);
	return pbuffer;
    } else
	return NULL;
}

void
SalvageFileSys1(struct DiskPartition *partP, VolumeId singleVolumeNumber)
{
    char *name, *tdir;
    char inodeListPath[256];
    static char tmpDevName[100];
    static char wpath[100];
    struct VolumeSummary *vsp, *esp;
    int i, j;

    fileSysPartition = partP;
    fileSysDevice = fileSysPartition->device;
    fileSysPathName = VPartitionPath(fileSysPartition);

#ifdef AFS_NT40_ENV
    /* Opendir can fail on "C:" but not on "C:\" if C is empty! */
    (void)sprintf(fileSysPath, "%s\\", fileSysPathName);
    name = partP->devName;
#else
    fileSysPath = fileSysPathName;
    strcpy(tmpDevName, partP->devName);
    name = get_DevName(tmpDevName, wpath);
    fileSysDeviceName = name;
    filesysfulldev = wpath;
#endif

    VLockPartition(partP->name);
    if (singleVolumeNumber || ForceSalvage)
	ForceSalvage = 1;
    else
	ForceSalvage = UseTheForceLuke(fileSysPath);

    if (singleVolumeNumber) {
	if (!VConnectFS()) {
	    Abort("Couldn't connect to file server\n");
	}
	AskOffline(singleVolumeNumber);
    } else {
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

	assert((dirp = opendir(fileSysPath)) != NULL);
	while ((dp = readdir(dirp))) {
	    if (!strncmp(dp->d_name, "salvage.inodes.", 15)
		|| !strncmp(dp->d_name, "salvage.temp.", 13)) {
		char npath[1024];
		Log("Removing old salvager temp files %s\n", dp->d_name);
		strcpy(npath, fileSysPath);
		strcat(npath, "/");
		strcat(npath, dp->d_name);
		unlink(npath);
	    }
	}
	closedir(dirp);
    }
    tdir = (tmpdir ? tmpdir : fileSysPath);
#ifdef AFS_NT40_ENV
    (void)_putenv("TMP=");	/* If "TMP" is set, then that overrides tdir. */
    (void)strncpy(inodeListPath, _tempnam(tdir, "salvage.inodes."), 255);
#else
    snprintf(inodeListPath, 255, "%s/salvage.inodes.%s.%d", tdir, name,
	     getpid());
#endif
    if (GetInodeSummary(inodeListPath, singleVolumeNumber) < 0) {
	unlink(inodeListPath);
	return;
    }
#ifdef AFS_NT40_ENV
    /* Using nt_unlink here since we're really using the delete on close
     * semantics of unlink. In most places in the salvager, we really do
     * mean to unlink the file at that point. Those places have been
     * modified to actually do that so that the NT crt can be used there.
     */
    inodeFd =
	_open_osfhandle((long)nt_open(inodeListPath, O_RDWR, 0), O_RDWR);
    nt_unlink(inodeListPath);	/* NT's crt unlink won't if file is open. */
#else
    inodeFd = afs_open(inodeListPath, O_RDONLY);
    unlink(inodeListPath);
#endif
    if (inodeFd == -1)
	Abort("Temporary file %s is missing...\n", inodeListPath);
    if (ListInodeOption) {
	PrintInodeList();
	return;
    }
    /* enumerate volumes in the partition.
     * figure out sets of read-only + rw volumes.
     * salvage each set, read-only volumes first, then read-write.
     * Fix up inodes on last volume in set (whether it is read-write
     * or read-only).
     */
    GetVolumeSummary(singleVolumeNumber);

    for (i = j = 0, vsp = volumeSummaryp, esp = vsp + nVolumes;
	 i < nVolumesInInodeFile; i = j) {
	VolumeId rwvid = inodeSummary[i].RWvolumeId;
	for (j = i;
	     j < nVolumesInInodeFile && inodeSummary[j].RWvolumeId == rwvid;
	     j++) {
	    VolumeId vid = inodeSummary[j].volumeId;
	    struct VolumeSummary *tsp;
	    /* Scan volume list (from partition root directory) looking for the
	     * current rw volume number in the volume list from the inode scan.
	     * If there is one here that is not in the inode volume list,
	     * delete it now. */
	    for (; vsp < esp && (vsp->header.parent < rwvid); vsp++) {
		if (vsp->fileName)
		    DeleteExtraVolumeHeaderFile(vsp);
	    }
	    /* Now match up the volume summary info from the root directory with the
	     * entry in the volume list obtained from scanning inodes */
	    inodeSummary[j].volSummary = NULL;
	    for (tsp = vsp; tsp < esp && (tsp->header.parent == rwvid); tsp++) {
		if (tsp->header.id == vid) {
		    inodeSummary[j].volSummary = tsp;
		    tsp->fileName = 0;
		    break;
		}
	    }
	}
	/* Salvage the group of volumes (several read-only + 1 read/write)
	 * starting with the current read-only volume we're looking at.
	 */
	SalvageVolumeGroup(&inodeSummary[i], j - i);
    }

    /* Delete any additional volumes that were listed in the partition but which didn't have any corresponding inodes */
    for (; vsp < esp; vsp++) {
	if (vsp->fileName)
	    DeleteExtraVolumeHeaderFile(vsp);
    }

    if (!singleVolumeNumber)	/* Remove the FORCESALVAGE file */
	RemoveTheForce(fileSysPath);

    if (!Testing && singleVolumeNumber) {
	AskOnline(singleVolumeNumber, fileSysPartition->name);

	/* Step through the volumeSummary list and set all volumes on-line.
	 * The volumes were taken off-line in GetVolumeSummary.
	 */
	for (j = 0; j < nVolumes; j++) {
	    AskOnline(volumeSummaryp[j].header.id, fileSysPartition->name);
	}
    } else {
	if (!Showmode)
	    Log("SALVAGING OF PARTITION %s%s COMPLETED\n",
		fileSysPartition->name, (Testing ? " (READONLY mode)" : ""));
    }

    close(inodeFd);		/* SalvageVolumeGroup was the last which needed it. */
}

void
DeleteExtraVolumeHeaderFile(register struct VolumeSummary *vsp)
{
    if (!Showmode)
	Log("The volume header file %s is not associated with any actual data (%sdeleted)\n", vsp->fileName, (Testing ? "would have been " : ""));
    if (!Testing)
	unlink(vsp->fileName);
    vsp->fileName = 0;
}

CompareInodes(const void *_p1, const void *_p2)
{
    register const struct ViceInodeInfo *p1 = _p1;
    register const struct ViceInodeInfo *p2 = _p2;
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
CountVolumeInodes(register struct ViceInodeInfo *ip, int maxInodes,
		  register struct InodeSummary *summary)
{
    int volume = ip->u.vnode.volumeId;
    int rwvolume = volume;
    register n, nSpecial;
    register Unique maxunique;
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
OnlyOneVolume(struct ViceInodeInfo *inodeinfo, VolumeId singleVolumeNumber, void *rock)
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
GetInodeSummary(char *path, VolumeId singleVolumeNumber)
{
    struct afs_stat status;
    int forceSal, err;
    struct ViceInodeInfo *ip;
    struct InodeSummary summary;
    char summaryFileName[50];
    FILE *summaryFile;
#ifdef AFS_NT40_ENV
    char *dev = fileSysPath;
    char *wpath = fileSysPath;
#else
    char *dev = fileSysDeviceName;
    char *wpath = filesysfulldev;
#endif
    char *part = fileSysPath;
    char *tdir;

    /* This file used to come from vfsck; cobble it up ourselves now... */
    if ((err =
	 ListViceInodes(dev, fileSysPath, path,
			singleVolumeNumber ? OnlyOneVolume : 0,
			singleVolumeNumber, &forceSal, forceR, wpath, NULL)) < 0) {
	if (err == -2) {
	    Log("*** I/O error %d when writing a tmp inode file %s; Not salvaged %s ***\nIncrease space on partition or use '-tmpdir'\n", errno, path, dev);
	    return -1;
	}
	unlink(path);
	Abort("Unable to get inodes for \"%s\"; not salvaged\n", dev);
    }
    if (forceSal && !ForceSalvage) {
	Log("***Forced salvage of all volumes on this partition***\n");
	ForceSalvage = 1;
    }
    inodeFd = afs_open(path, O_RDWR);
    if (inodeFd == -1 || afs_fstat(inodeFd, &status) == -1) {
	unlink(path);
	Abort("No inode description file for \"%s\"; not salvaged\n", dev);
    }
    tdir = (tmpdir ? tmpdir : part);
#ifdef AFS_NT40_ENV
    (void)_putenv("TMP=");	/* If "TMP" is set, then that overrides tdir. */
    (void)strcpy(summaryFileName, _tempnam(tdir, "salvage.temp"));
#else
    (void)afs_snprintf(summaryFileName, sizeof summaryFileName,
		       "%s/salvage.temp.%d", tdir, getpid());
#endif
    summaryFile = afs_fopen(summaryFileName, "a+");
    if (summaryFile == NULL) {
	close(inodeFd);
	unlink(path);
	Abort("Unable to create inode summary file\n");
    }
    if (!canfork || debug || Fork() == 0) {
	int nInodes;
	unsigned long st_size=(unsigned long) status.st_size;
	nInodes = st_size / sizeof(struct ViceInodeInfo);
	if (nInodes == 0) {
	    fclose(summaryFile);
	    close(inodeFd);
	    unlink(summaryFileName);
	    if (!singleVolumeNumber)	/* Remove the FORCESALVAGE file */
		RemoveTheForce(fileSysPath);
	    else {
		struct VolumeSummary *vsp;
		int i;

		GetVolumeSummary(singleVolumeNumber);

		for (i = 0, vsp = volumeSummaryp; i < nVolumes; i++) {
		    if (vsp->fileName)
			DeleteExtraVolumeHeaderFile(vsp);
		}
	    }
	    Log("%s vice inodes on %s; not salvaged\n",
		singleVolumeNumber ? "No applicable" : "No", dev);
	    return -1;
	}
	ip = (struct ViceInodeInfo *)malloc(nInodes*sizeof(struct ViceInodeInfo));
	if (ip == NULL) {
	    fclose(summaryFile);
	    close(inodeFd);
	    unlink(path);
	    unlink(summaryFileName);
	    Abort
		("Unable to allocate enough space to read inode table; %s not salvaged\n",
		 dev);
	}
	if (read(inodeFd, ip, st_size) != st_size) {
	    fclose(summaryFile);
	    close(inodeFd);
	    unlink(path);
	    unlink(summaryFileName);
	    Abort("Unable to read inode table; %s not salvaged\n", dev);
	}
	qsort(ip, nInodes, sizeof(struct ViceInodeInfo), CompareInodes);
	if (afs_lseek(inodeFd, 0, SEEK_SET) == -1
	    || write(inodeFd, ip, st_size) != st_size) {
	    fclose(summaryFile);
	    close(inodeFd);
	    unlink(path);
	    unlink(summaryFileName);
	    Abort("Unable to rewrite inode table; %s not salvaged\n", dev);
	}
	summary.index = 0;
	while (nInodes) {
	    CountVolumeInodes(ip, nInodes, &summary);
	    if (fwrite(&summary, sizeof(summary), 1, summaryFile) != 1) {
		Log("Difficulty writing summary file (errno = %d); %s not salvaged\n", errno, dev);
		fclose(summaryFile);
		close(inodeFd);
		return -1;
	    }
	    summary.index += (summary.nInodes);
	    nInodes -= summary.nInodes;
	    ip += summary.nInodes;
	}
	/* Following fflush is not fclose, because if it was debug mode would not work */
	if (fflush(summaryFile) == EOF || fsync(fileno(summaryFile)) == -1) {
	    Log("Unable to write summary file (errno = %d); %s not salvaged\n", errno, dev);
	    fclose(summaryFile);
	    close(inodeFd);
	    return -1;
	}
	if (canfork && !debug) {
	    ShowLog = 0;
	    Exit(0);
	}
    } else {
	if (Wait("Inode summary") == -1) {
	    fclose(summaryFile);
	    close(inodeFd);
	    unlink(path);
	    unlink(summaryFileName);
	    Exit(1);		/* salvage of this partition aborted */
	}
    }
    assert(afs_fstat(fileno(summaryFile), &status) != -1);
    if (status.st_size != 0) {
	int ret;
	unsigned long st_status=(unsigned long)status.st_size;
	inodeSummary = (struct InodeSummary *)malloc(st_status);
	assert(inodeSummary != NULL);
	/* For GNU we need to do lseek to get the file pointer moved. */
	assert(afs_lseek(fileno(summaryFile), 0, SEEK_SET) == 0);
	ret = read(fileno(summaryFile), inodeSummary, st_status);
	assert(ret == st_status);
    }
    nVolumesInInodeFile =(unsigned long)(status.st_size) / sizeof(struct InodeSummary);
    Log("%d nVolumesInInodeFile %d \n",nVolumesInInodeFile,(unsigned long)(status.st_size));
    fclose(summaryFile);
    close(inodeFd);
    unlink(summaryFileName);
    return 0;
}

/* Comparison routine for volume sort.
   This is setup so that a read-write volume comes immediately before
   any read-only clones of that volume */
int
CompareVolumes(const void *_p1, const void *_p2)
{
    register const struct VolumeSummary *p1 = _p1;
    register const struct VolumeSummary *p2 = _p2;
    if (p1->header.parent != p2->header.parent)
	return p1->header.parent < p2->header.parent ? -1 : 1;
    if (p1->header.id == p1->header.parent)	/* p1 is rw volume */
	return -1;
    if (p2->header.id == p2->header.parent)	/* p2 is rw volume */
	return 1;
    return p1->header.id < p2->header.id ? -1 : 1;	/* Both read-only */
}

void
GetVolumeSummary(VolumeId singleVolumeNumber)
{
    DIR *dirp;
    afs_int32 nvols = 0;
    struct VolumeSummary *vsp, vs;
    struct VolumeDiskHeader diskHeader;
    struct dirent *dp;

    /* Get headers from volume directory */
    if (chdir(fileSysPath) == -1 || (dirp = opendir(".")) == NULL)
	Abort("Can't read directory %s; not salvaged\n", fileSysPath);
    if (!singleVolumeNumber) {
	while ((dp = readdir(dirp))) {
	    char *p = dp->d_name;
	    p = strrchr(dp->d_name, '.');
	    if (p != NULL && strcmp(p, VHDREXT) == 0) {
		int fd;
		if ((fd = afs_open(dp->d_name, O_RDONLY)) != -1
		    && read(fd, (char *)&diskHeader, sizeof(diskHeader))
		    == sizeof(diskHeader)
		    && diskHeader.stamp.magic == VOLUMEHEADERMAGIC) {
		    DiskToVolumeHeader(&vs.header, &diskHeader);
		    nvols++;
		}
		close(fd);
	    }
	}
#ifdef AFS_NT40_ENV
	closedir(dirp);
	dirp = opendir(".");	/* No rewinddir for NT */
#else
	rewinddir(dirp);
#endif
	if (!nvols)
	    nvols = 1;
	volumeSummaryp =
	    (struct VolumeSummary *)malloc(nvols *
					   sizeof(struct VolumeSummary));
    } else
	volumeSummaryp =
	    (struct VolumeSummary *)malloc(20 * sizeof(struct VolumeSummary));
    assert(volumeSummaryp != NULL);

    nVolumes = 0;
    vsp = volumeSummaryp;
    while ((dp = readdir(dirp))) {
	char *p = dp->d_name;
	p = strrchr(dp->d_name, '.');
	if (p != NULL && strcmp(p, VHDREXT) == 0) {
	    int error = 0;
	    int fd;
	    if ((fd = afs_open(dp->d_name, O_RDONLY)) == -1
		|| read(fd, &diskHeader, sizeof(diskHeader))
		!= sizeof(diskHeader)
		|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
		error = 1;
	    }
	    close(fd);
	    if (error) {
		if (!singleVolumeNumber) {
		    if (!Showmode)
			Log("%s/%s is not a legitimate volume header file; %sdeleted\n", fileSysPathName, dp->d_name, (Testing ? "it would have been " : ""));
		    if (!Testing)
			unlink(dp->d_name);
		}
	    } else {
		char nameShouldBe[64];
		DiskToVolumeHeader(&vsp->header, &diskHeader);
		if (singleVolumeNumber && vsp->header.id == singleVolumeNumber
		    && vsp->header.parent != singleVolumeNumber) {
		    Log("%u is a read-only volume; not salvaged\n",
			singleVolumeNumber);
		    Exit(1);
		}
		if (!singleVolumeNumber
		    || (vsp->header.id == singleVolumeNumber
			|| vsp->header.parent == singleVolumeNumber)) {
		    (void)afs_snprintf(nameShouldBe, sizeof nameShouldBe,
				       VFORMAT, vsp->header.id);
		    if (singleVolumeNumber)
			AskOffline(vsp->header.id);
		    if (strcmp(nameShouldBe, dp->d_name)) {
			if (!Showmode)
			    Log("Volume header file %s is incorrectly named; %sdeleted (it will be recreated later, if necessary)\n", dp->d_name, (Testing ? "it would have been " : ""));
			if (!Testing)
			    unlink(dp->d_name);
		    } else {
			vsp->fileName = ToString(dp->d_name);
			nVolumes++;
			vsp++;
		    }
		}
	    }
	    close(fd);
	}
    }
    closedir(dirp);
    qsort(volumeSummaryp, nVolumes, sizeof(struct VolumeSummary),
	  CompareVolumes);
}

/* Find the link table. This should be associated with the RW volume or, if
 * a RO only site, then the RO volume. For now, be cautious and hunt carefully.
 */
Inode
FindLinkHandle(register struct InodeSummary *isp, int nVols,
	       struct ViceInodeInfo *allInodes)
{
    int i, j;
    struct ViceInodeInfo *ip;

    for (i = 0; i < nVols; i++) {
	ip = allInodes + isp[i].index;
	for (j = 0; j < isp[i].nSpecialInodes; j++) {
	    if (ip[j].u.special.type == VI_LINKTABLE)
		return ip[j].inodeNumber;
	}
    }
    return (Inode) - 1;
}

int
CreateLinkTable(register struct InodeSummary *isp, Inode ino)
{
    struct versionStamp version;
    FdHandle_t *fdP;

    if (!VALID_INO(ino))
	ino =
	    IH_CREATE(NULL, fileSysDevice, fileSysPath, 0, isp->volumeId,
		      INODESPECIAL, VI_LINKTABLE, isp->RWvolumeId);
    if (!VALID_INO(ino))
	Abort
	    ("Unable to allocate link table inode for volume %u (error = %d)\n",
	     isp->RWvolumeId, errno);
    IH_INIT(VGLinkH, fileSysDevice, isp->RWvolumeId, ino);
    fdP = IH_OPEN(VGLinkH);
    if (fdP == NULL)
	Abort("Can't open link table for volume %u (error = %d)\n",
	      isp->RWvolumeId, errno);

    if (FDH_TRUNC(fdP, sizeof(version) + sizeof(short)) < 0)
	Abort("Can't truncate link table for volume %u (error = %d)\n",
	      isp->RWvolumeId, errno);

    version.magic = LINKTABLEMAGIC;
    version.version = LINKTABLEVERSION;

    if (FDH_WRITE(fdP, (char *)&version, sizeof(version))
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
    DoSalvageVolumeGroup(parms->svgp_inodeSummaryp, parms->svgp_count);
    return NULL;
}

void
SalvageVolumeGroup(register struct InodeSummary *isp, int nVols)
{
    pthread_t tid;
    pthread_attr_t tattr;
    int code;
    SVGParms_t parms;

    /* Initialize per volume global variables, even if later code does so */
    VolumeChanged = 0;
    VGLinkH = NULL;
    VGLinkH_cnt = 0;
    memset(&VolInfo, 0, sizeof(VolInfo));

    parms.svgp_inodeSummaryp = isp;
    parms.svgp_count = nVols;
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
DoSalvageVolumeGroup(register struct InodeSummary *isp, int nVols)
{
    struct ViceInodeInfo *inodes, *allInodes, *ip;
    int i, totalInodes, size, salvageTo;
    int haveRWvolume;
    int check;
    Inode ino;
    int dec_VGLinkH = 0;
    int VGLinkH_p1;
    FdHandle_t *fdP = NULL;

    VGLinkH_cnt = 0;
    haveRWvolume = (isp->volumeId == isp->RWvolumeId
		    && isp->nSpecialInodes > 0);
    if ((!ShowMounts) || (ShowMounts && !haveRWvolume)) {
	if (!ForceSalvage && QuickCheck(isp, nVols))
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
    assert(afs_lseek
	   (inodeFd, isp->index * sizeof(struct ViceInodeInfo),
	    SEEK_SET) != -1);
    assert(read(inodeFd, inodes, size) == size);

    /* Don't try to salvage a read write volume if there isn't one on this
     * partition */
    salvageTo = haveRWvolume ? 0 : 1;

#ifdef AFS_NAMEI_ENV
    ino = FindLinkHandle(isp, nVols, allInodes);
    if (VALID_INO(ino)) {
	IH_INIT(VGLinkH, fileSysDevice, isp->RWvolumeId, ino);
	fdP = IH_OPEN(VGLinkH);
    }
    if (!VALID_INO(ino) || fdP == NULL) {
	Log("%s link table for volume %u.\n",
	    Testing ? "Would have recreated" : "Recreating", isp->RWvolumeId);
	if (Testing) {
	    IH_INIT(VGLinkH, fileSysDevice, -1, -1);
	} else {
            int i, j;
            struct ViceInodeInfo *ip;
	    CreateLinkTable(isp, ino);
	    fdP = IH_OPEN(VGLinkH);
            /* Sync fake 1 link counts to the link table, now that it exists */
            if (fdP) {
            	for (i = 0; i < nVols; i++) {
            		ip = allInodes + isp[i].index;
		         for (j = isp[i].nSpecialInodes; j < isp[i].nInodes; j++) {
				 namei_SetLinkCount(fdP, ip[j].inodeNumber, 1, 1);
		    }
            	}
	    }
	}
    }
    if (fdP)
	FDH_REALLYCLOSE(fdP);
#else
    IH_INIT(VGLinkH, fileSysDevice, -1, -1);
#endif

    /* Salvage in reverse order--read/write volume last; this way any
     * Inodes not referenced by the time we salvage the read/write volume
     * can be picked up by the read/write volume */
    /* ACTUALLY, that's not done right now--the inodes just vanish */
    for (i = nVols - 1; i >= salvageTo; i--) {
	int rw = (i == 0);
	struct InodeSummary *lisp = &isp[i];
#ifdef AFS_NAMEI_ENV
	/* If only the RO is present on this partition, the link table
	 * shows up as a RW volume special file. Need to make sure the
	 * salvager doesn't try to salvage the non-existent RW.
	 */
	if (rw && nVols > 1 && isp[i].nSpecialInodes == 1) {
	    /* If this only special inode is the link table, continue */
	    if (inodes->u.special.type == VI_LINKTABLE) {
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
	    if (SalvageVolumeHeaderFile(lisp, allInodes, rw, check, &deleteMe)
		== -1) {
		MaybeZapVolume(lisp, "Volume header", deleteMe, check);
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
	    if (SalvageVnodes(isp, lisp, allInodes, check) == -1) {
		MaybeZapVolume(lisp, "Vnode index", 0, check);
		break;
	    }
	}
    }

    /* Fix actual inode counts */
    if (!Showmode) {
	Log("totalInodes %d\n",totalInodes);
	for (ip = inodes; totalInodes; ip++, totalInodes--) {
	    static int TraceBadLinkCounts = 0;
#ifdef AFS_NAMEI_ENV
	    if (VGLinkH->ih_ino == ip->inodeNumber) {
		dec_VGLinkH = ip->linkCount - VGLinkH_cnt;
		VGLinkH_p1 = ip->u.param[0];
		continue;	/* Deal with this last. */
	    }
#endif
	    if (ip->linkCount != 0 && TraceBadLinkCounts) {
		TraceBadLinkCounts--;	/* Limit reports, per volume */
		Log("#### DEBUG #### Link count incorrect by %d; inode %s, size %llu, p=(%u,%u,%u,%u)\n", ip->linkCount, PrintInode(NULL, ip->inodeNumber), (afs_uintmax_t) ip->byteCount, ip->u.param[0], ip->u.param[1], ip->u.param[2], ip->u.param[3]);
	    }
	    while (ip->linkCount > 0) {
		/* below used to assert, not break */
		if (!Testing) {
		    if (IH_DEC(VGLinkH, ip->inodeNumber, ip->u.param[0])) {
			Log("idec failed. inode %s errno %d\n",
			    PrintInode(NULL, ip->inodeNumber), errno);
			break;
		    }
		}
		ip->linkCount--;
	    }
	    while (ip->linkCount < 0) {
		/* these used to be asserts */
		if (!Testing) {
		    if (IH_INC(VGLinkH, ip->inodeNumber, ip->u.param[0])) {
			Log("iinc failed. inode %s errno %d\n",
			    PrintInode(NULL, ip->inodeNumber), errno);
			break;
		    }
		}
		ip->linkCount++;
	    }
	}
#ifdef AFS_NAMEI_ENV
	while (dec_VGLinkH > 0) {
	    if (IH_DEC(VGLinkH, VGLinkH->ih_ino, VGLinkH_p1) < 0) {
		Log("idec failed on link table, errno = %d\n", errno);
	    }
	    dec_VGLinkH--;
	}
	while (dec_VGLinkH < 0) {
	    if (IH_INC(VGLinkH, VGLinkH->ih_ino, VGLinkH_p1) < 0) {
		Log("iinc failed on link table, errno = %d\n", errno);
	    }
	    dec_VGLinkH++;
	}
#endif
    }
    free(inodes);
    /* Directory consistency checks on the rw volume */
    if (haveRWvolume)
	SalvageVolume(isp, VGLinkH);
    IH_RELEASE(VGLinkH);

    if (canfork && !debug) {
	ShowLog = 0;
	Exit(0);
    }
}

int
QuickCheck(register struct InodeSummary *isp, int nVols)
{
    /* Check headers BEFORE forking */
    register int i;
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
	IH_INIT(h, fileSysDevice, vs->header.parent, vs->header.volumeInfo);
	if (IH_IREAD(h, 0, (char *)&volHeader, sizeof(volHeader))
	    == sizeof(volHeader)
	    && volHeader.stamp.magic == VOLUMEINFOMAGIC
	    && volHeader.dontSalvage == DONT_SALVAGE
	    && volHeader.needsSalvaged == 0 && volHeader.destroyMe == 0) {
	    if (volHeader.inUse == 1) {
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
SalvageVolumeHeaderFile(register struct InodeSummary *isp,
			register struct ViceInodeInfo *inodes, int RW,
			int check, int *deleteMe)
{
    int headerFd = 0;
    int i;
    register struct ViceInodeInfo *ip;
    int allinodesobsolete = 1;
    struct VolumeDiskHeader diskHeader;

    if (deleteMe)
	*deleteMe = 0;
    memset(&tempHeader, 0, sizeof(tempHeader));
    tempHeader.stamp.magic = VOLUMEHEADERMAGIC;
    tempHeader.stamp.version = VOLUMEHEADERVERSION;
    tempHeader.id = isp->volumeId;
    tempHeader.parent = isp->RWvolumeId;
    /* Check for duplicates (inodes are sorted by type field) */
    for (i = 0; i < isp->nSpecialInodes - 1; i++) {
	ip = &inodes[isp->index + i];
	if (ip->u.special.type == (ip + 1)->u.special.type) {
	    if (!Showmode)
		Log("Duplicate special inodes in volume header; salvage of volume %u aborted\n", isp->volumeId);
	    return -1;
	}
    }
    for (i = 0; i < isp->nSpecialInodes; i++) {
	ip = &inodes[isp->index + i];
	if (ip->u.special.type <= 0 || ip->u.special.type > MAXINODETYPE) {
	    if (check) {
		Log("Rubbish header inode\n");
		return -1;
	    }
	    Log("Rubbish header inode; deleted\n");
	} else if (!stuff[ip->u.special.type - 1].obsolete) {
	    *(stuff[ip->u.special.type - 1].inode) = ip->inodeNumber;
	    if (!check && ip->u.special.type != VI_LINKTABLE)
		ip->linkCount--;	/* Keep the inode around */
	    allinodesobsolete = 0;
	}
    }

    if (allinodesobsolete) {
	if (deleteMe)
	    *deleteMe = 1;
	return -1;
    }

    if (!check)
	VGLinkH_cnt++;		/* one for every header. */

    if (!RW && !check && isp->volSummary) {
	ClearROInUseBit(isp->volSummary);
	return 0;
    }

    for (i = 0; i < MAXINODETYPE; i++) {
	if (stuff[i].inodeType == VI_LINKTABLE) {
	    /* Gross hack: SalvageHeader does a bcmp on the volume header.
	     * And we may have recreated the link table earlier, so set the
	     * RW header as well.
	     */
	    if (VALID_INO(VGLinkH->ih_ino)) {
		*stuff[i].inode = VGLinkH->ih_ino;
	    }
	    continue;
	}
	if (SalvageHeader(&stuff[i], isp, check, deleteMe) == -1 && check)
	    return -1;
    }

    if (isp->volSummary == NULL) {
	char name[64];
	(void)afs_snprintf(name, sizeof name, VFORMAT, isp->volumeId);
	if (check) {
	    Log("No header file for volume %u\n", isp->volumeId);
	    return -1;
	}
	if (!Showmode)
	    Log("No header file for volume %u; %screating %s/%s\n",
		isp->volumeId, (Testing ? "it would have been " : ""),
		fileSysPathName, name);
	headerFd = afs_open(name, O_RDWR | O_CREAT | O_TRUNC, 0644);
	assert(headerFd != -1);
	isp->volSummary = (struct VolumeSummary *)
	    malloc(sizeof(struct VolumeSummary));
	isp->volSummary->fileName = ToString(name);
    } else {
	char name[64];
	/* hack: these two fields are obsolete... */
	isp->volSummary->header.volumeAcl = 0;
	isp->volSummary->header.volumeMountTable = 0;

	if (memcmp
	    (&isp->volSummary->header, &tempHeader,
	     sizeof(struct VolumeHeader))) {
	    /* We often remove the name before calling us, so we make a fake one up */
	    if (isp->volSummary->fileName) {
		strcpy(name, isp->volSummary->fileName);
	    } else {
		(void)afs_snprintf(name, sizeof name, VFORMAT, isp->volumeId);
		isp->volSummary->fileName = ToString(name);
	    }

	    Log("Header file %s is damaged or no longer valid%s\n", name,
		(check ? "" : "; repairing"));
	    if (check)
		return -1;

	    headerFd = afs_open(name, O_RDWR | O_TRUNC, 0644);
	    assert(headerFd != -1);
	}
    }
    if (headerFd) {
	memcpy(&isp->volSummary->header, &tempHeader,
	       sizeof(struct VolumeHeader));
	if (Testing) {
	    if (!Showmode)
		Log("It would have written a new header file for volume %u\n",
		    isp->volumeId);
	} else {
	    VolumeHeaderToDisk(&diskHeader, &tempHeader);
	    if (write(headerFd, &diskHeader, sizeof(struct VolumeDiskHeader))
		!= sizeof(struct VolumeDiskHeader)) {
		Log("Couldn't rewrite volume header file!\n");
		close(headerFd);
		return -1;
	    }
	}
	close(headerFd);
    }
    IH_INIT(isp->volSummary->volumeInfoHandle, fileSysDevice, isp->RWvolumeId,
	    isp->volSummary->header.volumeInfo);
    return 0;
}

int
SalvageHeader(register struct stuff *sp, struct InodeSummary *isp, int check,
	      int *deleteMe)
{
    union {
	VolumeDiskData volumeInfo;
	struct versionStamp fileHeader;
    } header;
    IHandle_t *specH;
    int recreate = 0;
    afs_int32 code;
    FdHandle_t *fdP;

    if (sp->obsolete)
	return 0;
#ifndef AFS_NAMEI_ENV
    if (sp->inodeType == VI_LINKTABLE)
	return 0;
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
		IH_CREATE(NULL, fileSysDevice, fileSysPath, 0, isp->volumeId,
			  INODESPECIAL, sp->inodeType, isp->RWvolumeId);
	    if (!VALID_INO(*(sp->inode)))
		Abort
		    ("Unable to allocate inode (%s) for volume header (error = %d)\n",
		     sp->description, errno);
	}
	recreate = 1;
    }

    IH_INIT(specH, fileSysDevice, isp->RWvolumeId, *(sp->inode));
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
	&& (FDH_READ(fdP, (char *)&header, sp->size) != sp->size
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
    }
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
	code = FDH_TRUNC(fdP, 0);
	if (code == -1)
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
	    if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
		Abort
		    ("Unable to seek to beginning of volume header file (%s) (errno = %d)\n",
		     sp->description, errno);
	    }
	    code =
		FDH_WRITE(fdP, (char *)&header.volumeInfo,
			  sizeof(header.volumeInfo));
	    if (code != sizeof(header.volumeInfo)) {
		if (code < 0)
		    Abort
			("Unable to write volume header file (%s) (errno = %d)\n",
			 sp->description, errno);
		Abort("Unable to write entire volume header file (%s)\n",
		      sp->description);
	    }
	} else {
	    if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
		Abort
		    ("Unable to seek to beginning of volume header file (%s) (errno = %d)\n",
		     sp->description, errno);
	    }
	    code = FDH_WRITE(fdP, (char *)&sp->stamp, sizeof(sp->stamp));
	    if (code != sizeof(sp->stamp)) {
		if (code < 0)
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
	VolInfo = header.volumeInfo;
	if (check) {
	    char update[25];
	    if (VolInfo.updateDate) {
		strcpy(update, TimeStamp(VolInfo.updateDate, 0));
		if (!Showmode)
		    Log("%s (%u) %supdated %s\n", VolInfo.name, VolInfo.id,
			(Testing ? "it would have been " : ""), update);
	    } else {
		strcpy(update, TimeStamp(VolInfo.creationDate, 0));
		if (!Showmode)
		    Log("%s (%u) not updated (created %s)\n", VolInfo.name,
			VolInfo.id, update);
	    }

	}
    }

    return 0;
}

int
SalvageVnodes(register struct InodeSummary *rwIsp,
	      register struct InodeSummary *thisIsp,
	      register struct ViceInodeInfo *inodes, int check)
{
    int ilarge, ismall, ioffset, RW, nInodes;
    ioffset = rwIsp->index + rwIsp->nSpecialInodes;	/* first inode */
    if (Showmode)
	return 0;
    RW = (rwIsp == thisIsp);
    nInodes = (rwIsp->nInodes - rwIsp->nSpecialInodes);
    ismall =
	SalvageIndex(thisIsp->volSummary->header.smallVnodeIndex, vSmall, RW,
		     &inodes[ioffset], nInodes, thisIsp->volSummary, check);
    if (check && ismall == -1)
	return -1;
    ilarge =
	SalvageIndex(thisIsp->volSummary->header.largeVnodeIndex, vLarge, RW,
		     &inodes[ioffset], nInodes, thisIsp->volSummary, check);
    return (ilarge == 0 && ismall == 0 ? 0 : -1);
}

int
SalvageIndex(Inode ino, VnodeClass class, int RW,
	     register struct ViceInodeInfo *ip, int nInodes,
	     struct VolumeSummary *volSummary, int check)
{
    VolumeId volumeNumber;
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    int err = 0;
    StreamHandle_t *file;
    struct VnodeClassInfo *vcp;
    afs_sfsize_t size;
    afs_fsize_t vnodeLength;
    int vnodeIndex, nVnodes;
    afs_ino_str_t stmp1, stmp2;
    IHandle_t *handle;
    FdHandle_t *fdP;

    volumeNumber = volSummary->header.id;
    IH_INIT(handle, fileSysDevice, volSummary->header.parent, ino);
    fdP = IH_OPEN(handle);
    assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    assert(file != NULL);
    vcp = &VnodeClassInfo[class];
    size = OS_SIZE(fdP->fd_fd);
    assert(size != -1);
    nVnodes = (size / vcp->diskSize) - 1;
    if (nVnodes > 0) {
	assert((nVnodes + 1) * vcp->diskSize == size);
	assert(STREAM_SEEK(file, vcp->diskSize, 0) == 0);
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
		Log("OWNER IS ROOT %s %u dir %u vnode %u author %u owner %u mode %o\n", VolInfo.name, volumeNumber, vnode->parent, vnodeNumber, vnode->author, vnode->owner, vnode->modeBits);
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
		    register struct ViceInodeInfo *lip = ip;
		    register lnInodes = nInodes;
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
		    if (VNDISK_GET_INO(vnode) != 0
			|| vnode->type == vDirectory) {
			/* No matching inode--get rid of the vnode */
			if (check) {
			    if (VNDISK_GET_INO(vnode)) {
				if (!Showmode) {
				    Log("Vnode %d (unique %u): corresponding inode %s is missing\n", vnodeNumber, vnode->uniquifier, PrintInode(NULL, VNDISK_GET_INO(vnode)));
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
				Log("Vnode %d (unique %u): corresponding inode %s is missing; vnode deleted, vnode mod time=%s", vnodeNumber, vnode->uniquifier, PrintInode(NULL, VNDISK_GET_INO(vnode)), ctime((time_t *) & (vnode->serverModifyTime)));
			    }
			} else {
			    if (!Showmode)
				Log("Vnode %d (unique %u): bad directory vnode (no inode number listed); vnode deleted, vnode mod time=%s", vnodeNumber, vnode->uniquifier, ctime((time_t *) & (vnode->serverModifyTime)));
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
	    assert(!(vnodeChanged && check));
	    if (vnodeChanged && !Testing) {
		assert(IH_IWRITE
		       (handle, vnodeIndexOffset(vcp, vnodeNumber),
			(char *)vnode, vcp->diskSize)
		       == vcp->diskSize);
		VolumeChanged = 1;	/* For break call back */
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
CheckVnodeNumber(VnodeId vnodeNumber)
{
    VnodeClass class;
    struct VnodeInfo *vip;
    int offset;

    class = vnodeIdToClass(vnodeNumber);
    vip = &vnodeInfo[class];
    offset = vnodeIdToBitNumber(vnodeNumber);
    return (offset >= vip->nVnodes ? NULL : &vip->vnodes[offset]);
}

void
CopyOnWrite(register struct DirSummary *dir)
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
	IH_IREAD(vnodeInfo[vLarge].handle,
		 vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		 sizeof(vnode));
    assert(code == sizeof(vnode));
    oldinode = VNDISK_GET_INO(&vnode);
    /* Increment the version number by a whole lot to avoid problems with
     * clients that were promised new version numbers--but the file server
     * crashed before the versions were written to disk.
     */
    newinode =
	IH_CREATE(dir->ds_linkH, fileSysDevice, fileSysPath, 0, dir->rwVid,
		  dir->vnodeNumber, vnode.uniquifier, vnode.dataVersion +=
		  200);
    assert(VALID_INO(newinode));
    assert(CopyInode(fileSysDevice, oldinode, newinode, dir->rwVid) == 0);
    vnode.cloned = 0;
    VNDISK_SET_INO(&vnode, newinode);
    code =
	IH_IWRITE(vnodeInfo[vLarge].handle,
		  vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		  sizeof(vnode));
    assert(code == sizeof(vnode));

    SetSalvageDirHandle(&dir->dirHandle, dir->dirHandle.dirh_handle->ih_vid,
			fileSysDevice, newinode);
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
CopyAndSalvage(register struct DirSummary *dir)
{
    struct VnodeDiskObject vnode;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[vLarge];
    Inode oldinode, newinode;
    DirHandle newdir;
    afs_int32 code;
    afs_sfsize_t lcode;
    afs_int32 parentUnique = 1;
    struct VnodeEssence *vnodeEssence;

    if (Testing)
	return;
    Log("Salvaging directory %u...\n", dir->vnodeNumber);
    lcode =
	IH_IREAD(vnodeInfo[vLarge].handle,
		 vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		 sizeof(vnode));
    assert(lcode == sizeof(vnode));
    oldinode = VNDISK_GET_INO(&vnode);
    /* Increment the version number by a whole lot to avoid problems with
     * clients that were promised new version numbers--but the file server
     * crashed before the versions were written to disk.
     */
    newinode =
	IH_CREATE(dir->ds_linkH, fileSysDevice, fileSysPath, 0, dir->rwVid,
		  dir->vnodeNumber, vnode.uniquifier, vnode.dataVersion +=
		  200);
    assert(VALID_INO(newinode));
    SetSalvageDirHandle(&newdir, dir->rwVid, fileSysDevice, newinode);

    /* Assign . and .. vnode numbers from dir and vnode.parent. 
     * The uniquifier for . is in the vnode.
     * The uniquifier for .. might be set to a bogus value of 1 and 
     * the salvager will later clean it up.
     */
    if (vnode.parent && (vnodeEssence = CheckVnodeNumber(vnode.parent))) {
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
	assert(1 == 2);
    }
    Log("Checking the results of the directory salvage...\n");
    if (!DirOK(&newdir)) {
	Log("Directory salvage failed!!!; restoring old version of the directory.\n");
	code = IH_DEC(dir->ds_linkH, newinode, dir->rwVid);
	assert(code == 0);
	assert(1 == 2);
    }
    vnode.cloned = 0;
    VNDISK_SET_INO(&vnode, newinode);
    VNDISK_SET_LEN(&vnode, Length(&newdir));
    lcode =
	IH_IWRITE(vnodeInfo[vLarge].handle,
		  vnodeIndexOffset(vcp, dir->vnodeNumber), (char *)&vnode,
		  sizeof(vnode));
    assert(lcode == sizeof(vnode));
#ifdef AFS_NT40_ENV
    nt_sync(fileSysDevice);
#else
    sync();			/* this is slow, but hopefully rarely called.  We don't have
				 * an open FD on the file itself to fsync.
				 */
#endif
    code = IH_DEC(dir->ds_linkH, oldinode, dir->rwVid);
    assert(code == 0);
    dir->dirHandle = newdir;
}

void
JudgeEntry(struct DirSummary *dir, char *name, VnodeId vnodeNumber,
	   Unique unique)
{
    struct VnodeEssence *vnodeEssence;
    afs_int32 dirOrphaned, todelete;

    dirOrphaned = IsVnodeOrphaned(dir->vnodeNumber);

    vnodeEssence = CheckVnodeNumber(vnodeNumber);
    if (vnodeEssence == NULL) {
	if (!Showmode) {
	    Log("dir vnode %u: invalid entry deleted: %s/%s (vnode %u, unique %u)\n", dir->vnodeNumber, dir->name ? dir->name : "??", name, vnodeNumber, unique);
	}
	if (!Testing) {
	    CopyOnWrite(dir);
	    assert(Delete(&dir->dirHandle, name) == 0);
	}
	return;
    }
#ifdef AFS_AIX_ENV
#ifndef AFS_NAMEI_ENV
    /* On AIX machines, don't allow entries to point to inode 0. That is a special 
     * mount inode for the partition. If this inode were deleted, it would crash
     * the machine.
     */
    if (vnodeEssence->InodeNumber == 0) {
	Log("dir vnode %d: invalid entry: %s/%s has no inode (vnode %d, unique %d)%s\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, (Testing ? "-- would have deleted" : " -- deleted"));
	if (!Testing) {
	    CopyOnWrite(dir);
	    assert(Delete(&dir->dirHandle, name) == 0);
	}
	return;
    }
#endif
#endif

    if (!(vnodeNumber & 1) && !Showmode
	&& !(vnodeEssence->count || vnodeEssence->unique
	     || vnodeEssence->modeBits)) {
	Log("dir vnode %u: invalid entry: %s/%s (vnode %u, unique %u)%s\n",
	    dir->vnodeNumber, (dir->name ? dir->name : "??"), name,
	    vnodeNumber, unique,
	    ((!unique) ? (Testing ? "-- would have deleted" : " -- deleted") :
	     ""));
	if (!unique) {
	    if (!Testing) {
		CopyOnWrite(dir);
		assert(Delete(&dir->dirHandle, name) == 0);
	    }
	    return;
	}
    }

    /* Check if the Uniquifiers match. If not, change the directory entry
     * so its unique matches the vnode unique. Delete if the unique is zero
     * or if the directory is orphaned.
     */
    if (!vnodeEssence->unique || (vnodeEssence->unique) != unique) {
	if (!vnodeEssence->unique
	    && ((strcmp(name, "..") == 0) || (strcmp(name, ".") == 0))) {
	    /* This is an orphaned directory. Don't delete the . or ..
	     * entry. Otherwise, it will get created in the next 
	     * salvage and deleted again here. So Just skip it.
	     */
	    return;
	}

	todelete = ((!vnodeEssence->unique || dirOrphaned) ? 1 : 0);

	if (!Showmode) {
	    Log("dir vnode %u: %s/%s (vnode %u): unique changed from %u to %u %s\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, vnodeEssence->unique, (!todelete ? "" : (Testing ? "-- would have deleted" : "-- deleted")));
	}
	if (!Testing) {
	    ViceFid fid;
	    fid.Vnode = vnodeNumber;
	    fid.Unique = vnodeEssence->unique;
	    CopyOnWrite(dir);
	    assert(Delete(&dir->dirHandle, name) == 0);
	    if (!todelete)
		assert(Create(&dir->dirHandle, name, &fid) == 0);
	}
	if (todelete)
	    return;		/* no need to continue */
    }

    if (strcmp(name, ".") == 0) {
	if (dir->vnodeNumber != vnodeNumber || (dir->unique != unique)) {
	    ViceFid fid;
	    if (!Showmode)
		Log("directory vnode %u.%u: bad '.' entry (was %u.%u); fixed\n", dir->vnodeNumber, dir->unique, vnodeNumber, unique);
	    if (!Testing) {
		CopyOnWrite(dir);
		assert(Delete(&dir->dirHandle, ".") == 0);
		fid.Vnode = dir->vnodeNumber;
		fid.Unique = dir->unique;
		assert(Create(&dir->dirHandle, ".", &fid) == 0);
	    }

	    vnodeNumber = fid.Vnode;	/* Get the new Essence */
	    unique = fid.Unique;
	    vnodeEssence = CheckVnodeNumber(vnodeNumber);
	}
	dir->haveDot = 1;
    } else if (strcmp(name, "..") == 0) {
	ViceFid pa;
	if (dir->parent) {
	    struct VnodeEssence *dotdot;
	    pa.Vnode = dir->parent;
	    dotdot = CheckVnodeNumber(pa.Vnode);
	    assert(dotdot != NULL);	/* XXX Should not be assert */
	    pa.Unique = dotdot->unique;
	} else {
	    pa.Vnode = dir->vnodeNumber;
	    pa.Unique = dir->unique;
	}
	if ((pa.Vnode != vnodeNumber) || (pa.Unique != unique)) {
	    if (!Showmode)
		Log("directory vnode %u.%u: bad '..' entry (was %u.%u); fixed\n", dir->vnodeNumber, dir->unique, vnodeNumber, unique);
	    if (!Testing) {
		CopyOnWrite(dir);
		assert(Delete(&dir->dirHandle, "..") == 0);
		assert(Create(&dir->dirHandle, "..", &pa) == 0);
	    }

	    vnodeNumber = pa.Vnode;	/* Get the new Essence */
	    unique = pa.Unique;
	    vnodeEssence = CheckVnodeNumber(vnodeNumber);
	}
	dir->haveDotDot = 1;
    } else if (strncmp(name, ".__afs", 6) == 0) {
	if (!Showmode) {
	    Log("dir vnode %u: special old unlink-while-referenced file %s %s deleted (vnode %u)\n", dir->vnodeNumber, name, (Testing ? "would have been" : "is"), vnodeNumber);
	}
	if (!Testing) {
	    CopyOnWrite(dir);
	    assert(Delete(&dir->dirHandle, name) == 0);
	}
	vnodeEssence->claimed = 0;	/* Not claimed: Orphaned */
	vnodeEssence->todelete = 1;	/* Will later delete vnode and decr inode */
	return;
    } else {
	if (ShowSuid && (vnodeEssence->modeBits & 06000))
	    Log("FOUND suid/sgid file: %s/%s (%u.%u %05o) author %u (vnode %u dir %u)\n", dir->name ? dir->name : "??", name, vnodeEssence->owner, vnodeEssence->group, vnodeEssence->modeBits, vnodeEssence->author, vnodeNumber, dir->vnodeNumber);
	if (ShowMounts && (vnodeEssence->type == vSymlink)
	    && !(vnodeEssence->modeBits & 0111)) {
	    int code, size;
	    char buf[1024];
	    IHandle_t *ihP;
	    FdHandle_t *fdP;

	    IH_INIT(ihP, fileSysDevice, dir->dirHandle.dirh_handle->ih_vid,
		    vnodeEssence->InodeNumber);
	    fdP = IH_OPEN(ihP);
	    assert(fdP != NULL);
	    size = FDH_SIZE(fdP);
	    assert(size != -1);
	    memset(buf, 0, 1024);
	    if (size > 1024)
		size = 1024;
	    code = FDH_READ(fdP, buf, size);
	    assert(code == size);
	    Log("In volume %u (%s) found mountpoint %s/%s to '%s'\n",
		dir->dirHandle.dirh_handle->ih_vid, dir->vname,
		dir->name ? dir->name : "??", name, buf);
	    FDH_REALLYCLOSE(fdP);
	    IH_RELEASE(ihP);
	}
	if (ShowRootFiles && vnodeEssence->owner == 0 && vnodeNumber != 1)
	    Log("FOUND root file: %s/%s (%u.%u %05o) author %u (vnode %u dir %u)\n", dir->name ? dir->name : "??", name, vnodeEssence->owner, vnodeEssence->group, vnodeEssence->modeBits, vnodeEssence->author, vnodeNumber, dir->vnodeNumber);
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
		    Log("dir vnode %u: %s/%s (vnode %u, unique %u) -- parent vnode %schanged from %u to %u\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, (Testing ? "would have been " : ""), vnodeEssence->parent, dir->vnodeNumber);
		}
		vnodeEssence->parent = dir->vnodeNumber;
		vnodeEssence->changed = 1;
	    } else {
		/* Vnode was claimed by another directory */
		if (!Showmode) {
		    if (dirOrphaned) {
			Log("dir vnode %u: %s/%s parent vnode is %u (vnode %u, unique %u) -- %sdeleted\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeEssence->parent, vnodeNumber, unique, (Testing ? "would have been " : ""));
		    } else if (vnodeNumber == 1) {
			Log("dir vnode %d: %s/%s is invalid (vnode %d, unique %d) -- %sdeleted\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeNumber, unique, (Testing ? "would have been " : ""));
		    } else {
			Log("dir vnode %u: %s/%s already claimed by directory vnode %u (vnode %u, unique %u) -- %sdeleted\n", dir->vnodeNumber, (dir->name ? dir->name : "??"), name, vnodeEssence->parent, vnodeNumber, unique, (Testing ? "would have been " : ""));
		    }
		}
		if (!Testing) {
		    CopyOnWrite(dir);
		    assert(Delete(&dir->dirHandle, name) == 0);
		}
		return;
	    }
	}
	/* This directory claims the vnode */
	vnodeEssence->claimed = 1;
    }
    vnodeEssence->count--;
}

void
DistilVnodeEssence(VolumeId rwVId, VnodeClass class, Inode ino, Unique * maxu)
{
    register struct VnodeInfo *vip = &vnodeInfo[class];
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    char buf[SIZEOF_LARGEDISKVNODE];
    struct VnodeDiskObject *vnode = (struct VnodeDiskObject *)buf;
    afs_sfsize_t size;
    StreamHandle_t *file;
    int vnodeIndex;
    int nVnodes;
    FdHandle_t *fdP;

    IH_INIT(vip->handle, fileSysDevice, rwVId, ino);
    fdP = IH_OPEN(vip->handle);
    assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r+");
    assert(file != NULL);
    size = OS_SIZE(fdP->fd_fd);
    assert(size != -1);
    vip->nVnodes = (size / vcp->diskSize) - 1;
    if (vip->nVnodes > 0) {
	assert((vip->nVnodes + 1) * vcp->diskSize == size);
	assert(STREAM_SEEK(file, vcp->diskSize, 0) == 0);
	assert((vip->vnodes = (struct VnodeEssence *)
		calloc(vip->nVnodes, sizeof(struct VnodeEssence))) != NULL);
	if (class == vLarge) {
	    assert((vip->inodes = (Inode *)
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
	    register struct VnodeEssence *vep = &vip->vnodes[vnodeIndex];
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
		assert(class == vLarge);
		vip->inodes[vnodeIndex] = VNDISK_GET_INO(vnode);
	    }
	}
    }
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
}

static char *
GetDirName(VnodeId vnode, struct VnodeEssence *vp, char *path)
{
    struct VnodeEssence *parentvp;

    if (vnode == 1) {
	strcpy(path, ".");
	return path;
    }
    if (vp->parent && vp->name && (parentvp = CheckVnodeNumber(vp->parent))
	&& GetDirName(vp->parent, parentvp, path)) {
	strcat(path, "/");
	strcat(path, vp->name);
	return path;
    }
    return 0;
}

/* To determine if a vnode is orhpaned or not, the vnode and all its parent
 * vnodes must be "claimed". The vep->claimed flag is set in JudgeEntry().
 */
static int
IsVnodeOrphaned(VnodeId vnode)
{
    struct VnodeEssence *vep;

    if (vnode == 0)
	return (1);		/* Vnode zero does not exist */
    if (vnode == 1)
	return (0);		/* The root dir vnode is always claimed */
    vep = CheckVnodeNumber(vnode);	/* Get the vnode essence */
    if (!vep || !vep->claimed)
	return (1);		/* Vnode is not claimed - it is orphaned */

    return (IsVnodeOrphaned(vep->parent));
}

void
SalvageDir(char *name, VolumeId rwVid, struct VnodeInfo *dirVnodeInfo,
	   IHandle_t * alinkH, int i, struct DirSummary *rootdir,
	   int *rootdirfound)
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
	parent = CheckVnodeNumber(dirVnodeInfo->vnodes[i].parent);
	if (parent && parent->salvaged == 0)
	    SalvageDir(name, rwVid, dirVnodeInfo, alinkH,
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
    SetSalvageDirHandle(&dir.dirHandle, dir.rwVid, fileSysDevice,
			dirVnodeInfo->inodes[i]);

    dirok = ((RebuildDirs && !Testing) ? 0 : DirOK(&dir.dirHandle));
    if (!dirok) {
	if (!RebuildDirs) {
	    Log("Directory bad, vnode %u; %s...\n", dir.vnodeNumber,
		(Testing ? "skipping" : "salvaging"));
	}
	if (!Testing) {
	    CopyAndSalvage(&dir);
	    dirok = 1;
	}
    }
    dirHandle = dir.dirHandle;

    dir.name =
	GetDirName(bitNumberToVnodeNumber(i, vLarge),
		   &dirVnodeInfo->vnodes[i], path);

    if (dirok) {
	/* If enumeration failed for random reasons, we will probably delete
	 * too much stuff, so we guard against this instead.
	 */
	assert(EnumerateDir(&dirHandle, JudgeEntry, &dir) == 0);
    }

    /* Delete the old directory if it was copied in order to salvage.
     * CopyOnWrite has written the new inode # to the disk, but we still
     * have the old one in our local structure here.  Thus, we idec the
     * local dude.
     */
    DFlush();
    if (dir.copied && !Testing) {
	code = IH_DEC(dir.ds_linkH, dirHandle.dirh_handle->ih_ino, rwVid);
	assert(code == 0);
	dirVnodeInfo->inodes[i] = dir.dirHandle.dirh_inode;
    }

    /* Remember rootdir DirSummary _after_ it has been judged */
    if (dir.vnodeNumber == 1 && dir.unique == 1) {
	memcpy(rootdir, &dir, sizeof(struct DirSummary));
	*rootdirfound = 1;
    }

    return;
}

int
SalvageVolume(register struct InodeSummary *rwIsp, IHandle_t * alinkH)
{
    /* This routine, for now, will only be called for read-write volumes */
    int i, j, code;
    int BlocksInVolume = 0, FilesInVolume = 0;
    register VnodeClass class;
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
    ViceFid pa;
    VnodeId LFVnode, ThisVnode;
    Unique LFUnique, ThisUnique;
    char npath[128];

    vid = rwIsp->volSummary->header.id;
    IH_INIT(h, fileSysDevice, vid, rwIsp->volSummary->header.volumeInfo);
    nBytes = IH_IREAD(h, 0, (char *)&volHeader, sizeof(volHeader));
    assert(nBytes == sizeof(volHeader));
    assert(volHeader.stamp.magic == VOLUMEINFOMAGIC);
    assert(volHeader.destroyMe != DESTROY_ME);
    /* (should not have gotten this far with DESTROY_ME flag still set!) */

    DistilVnodeEssence(vid, vLarge, rwIsp->volSummary->header.largeVnodeIndex,
		       &maxunique);
    DistilVnodeEssence(vid, vSmall, rwIsp->volSummary->header.smallVnodeIndex,
		       &maxunique);

    dirVnodeInfo = &vnodeInfo[vLarge];
    for (i = 0; i < dirVnodeInfo->nVnodes; i++) {
	SalvageDir(volHeader.name, vid, dirVnodeInfo, alinkH, i, &rootdir,
		   &rootdirfound);
    }
    if (Showmode) {
	IH_RELEASE(h);
	return 0;
    }

    /* Parse each vnode looking for orphaned vnodes and
     * connect them to the tree as orphaned (if requested).
     */
    oldrootdir = rootdir;
    for (class = 0; class < nVNODECLASSES; class++) {
	for (v = 0; v < vnodeInfo[class].nVnodes; v++) {
	    vep = &(vnodeInfo[class].vnodes[v]);
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
		if (vnodeInfo[vLarge].vnodes[pv].unique != 0)
		    vnodeInfo[vLarge].vnodes[pv].count++;
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
		    ViceFid pa;
		    DirHandle dh;

		    /* Remove and recreate the ".." entry in this orphaned directory */
		    SetSalvageDirHandle(&dh, vid, fileSysDevice,
					vnodeInfo[class].inodes[v]);
		    pa.Vnode = LFVnode;
		    pa.Unique = LFUnique;
		    assert(Delete(&dh, "..") == 0);
		    assert(Create(&dh, "..", &pa) == 0);

		    /* The original parent's link count was decremented above.
		     * Here we increment the new parent's link count.
		     */
		    pv = vnodeIdToBitNumber(LFVnode);
		    vnodeInfo[vLarge].vnodes[pv].count--;

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

		    CopyOnWrite(&rootdir);
		    code = Create(&rootdir.dirHandle, npath, &pa);
		    if (!code)
			break;

		    ThisUnique += 50;	/* Try creating a different file */
		}
		assert(code == 0);
		Log("Attaching orphaned %s to volume's root dir as %s\n",
		    ((class == vLarge) ? "directory" : "file"), npath);
	    }
	}			/* for each vnode in the class */
    }				/* for each class of vnode */

    /* Delete the old rootinode directory if the rootdir was CopyOnWrite */
    DFlush();
    if (!oldrootdir.copied && rootdir.copied) {
	code =
	    IH_DEC(oldrootdir.ds_linkH, oldrootdir.dirHandle.dirh_inode,
		   oldrootdir.rwVid);
	assert(code == 0);
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
	int nVnodes = vnodeInfo[class].nVnodes;
	struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
	struct VnodeEssence *vnodes = vnodeInfo[class].vnodes;
	FilesInVolume += vnodeInfo[class].nAllocatedVnodes;
	BlocksInVolume += vnodeInfo[class].volumeBlockCount;
	for (i = 0; i < nVnodes; i++) {
	    register struct VnodeEssence *vnp = &vnodes[i];
	    VnodeId vnodeNumber = bitNumberToVnodeNumber(i, class);

	    /* If the vnode is good but is unclaimed (not listed in
	     * any directory entries), then it is orphaned.
	     */
	    orphaned = -1;
	    if ((vnp->type != 0) && (orphaned = IsVnodeOrphaned(vnodeNumber))) {
		vnp->claimed = 0;	/* Makes IsVnodeOrphaned calls faster */
		vnp->changed = 1;
	    }

	    if (vnp->changed || vnp->count) {
		int oldCount;
		int code;
		nBytes =
		    IH_IREAD(vnodeInfo[class].handle,
			     vnodeIndexOffset(vcp, vnodeNumber),
			     (char *)&vnode, sizeof(vnode));
		assert(nBytes == sizeof(vnode));

		vnode.parent = vnp->parent;
		oldCount = vnode.linkCount;
		vnode.linkCount = vnode.linkCount - vnp->count;

		if (orphaned == -1)
		    orphaned = IsVnodeOrphaned(vnodeNumber);
		if (orphaned) {
		    if (!vnp->todelete) {
			/* Orphans should have already been attached (if requested) */
			assert(orphans != ORPH_ATTACH);
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
			    assert(code == 0);
			}
			memset(&vnode, 0, sizeof(vnode));
		    }
		} else if (vnp->count) {
		    if (!Showmode) {
			Log("Vnode %u: link count incorrect (was %d, %s %d)\n", vnodeNumber, oldCount, (Testing ? "would have changed to" : "now"), vnode.linkCount);
		    }
		}

		vnode.dataVersion++;
		if (!Testing) {
		    nBytes =
			IH_IWRITE(vnodeInfo[class].handle,
				  vnodeIndexOffset(vcp, vnodeNumber),
				  (char *)&vnode, sizeof(vnode));
		    assert(nBytes == sizeof(vnode));
		}
		VolumeChanged = 1;
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
	register struct VnodeInfo *vip = &vnodeInfo[class];
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

    /* Turn off the inUse bit; the volume's been salvaged! */
    volHeader.inUse = 0;	/* clear flag indicating inUse@last crash */
    volHeader.needsSalvaged = 0;	/* clear 'damaged' flag */
    volHeader.inService = 1;	/* allow service again */
    volHeader.needsCallback = (VolumeChanged != 0);
    volHeader.dontSalvage = DONT_SALVAGE;
    VolumeChanged = 0;
    if (!Testing) {
	nBytes = IH_IWRITE(h, 0, (char *)&volHeader, sizeof(volHeader));
	assert(nBytes == sizeof(volHeader));
    }
    if (!Showmode) {
	Log("%sSalvaged %s (%u): %d files, %d blocks\n",
	    (Testing ? "It would have " : ""), volHeader.name, volHeader.id,
	    FilesInVolume, BlocksInVolume);
    }
    IH_RELEASE(vnodeInfo[vSmall].handle);
    IH_RELEASE(vnodeInfo[vLarge].handle);
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
    assert(nBytes == sizeof(volHeader));
    assert(volHeader.stamp.magic == VOLUMEINFOMAGIC);
    volHeader.inUse = 0;
    volHeader.needsSalvaged = 0;
    volHeader.inService = 1;
    volHeader.dontSalvage = DONT_SALVAGE;
    if (!Testing) {
	nBytes = IH_IWRITE(h, 0, (char *)&volHeader, sizeof(volHeader));
	assert(nBytes == sizeof(volHeader));
    }
}

/* MaybeZapVolume
 * Possible delete the volume.
 *
 * deleteMe - Always do so, only a partial volume.
 */
void
MaybeZapVolume(register struct InodeSummary *isp, char *message, int deleteMe,
	       int check)
{
    if (readOnly(isp) || deleteMe) {
	if (isp->volSummary && isp->volSummary->fileName) {
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
	    if (!Testing)
		unlink(isp->volSummary->fileName);
	}
    } else if (!check) {
	Log("%s salvage was unsuccessful: read-write volume %u\n", message,
	    isp->volumeId);
	Abort("Salvage of volume %u aborted\n", isp->volumeId);
    }
}


void
AskOffline(VolumeId volumeId)
{
    if (FSYNC_askfs(volumeId, NULL, FSYNC_OFF, FSYNC_SALVAGE) == FSYNC_DENIED) {
	Log("AskOffline:  file server denied offline request; a general salvage is required.\n");
	Abort("Salvage aborted\n");
    }
}

void
AskOnline(VolumeId volumeId, char *partition)
{
    if (FSYNC_askfs(volumeId, partition, FSYNC_ON, 0) == FSYNC_DENIED) {
	Log("AskOnline:  file server denied online request to volume %u partition %s\n", volumeId, partition);
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
    register int n = 0;

    IH_INIT(srcH, device, rwvolume, inode1);
    srcFdP = IH_OPEN(srcH);
    assert(srcFdP != NULL);
    IH_INIT(destH, device, rwvolume, inode2);
    destFdP = IH_OPEN(destH);
    assert(n != -1);
    while ((n = FDH_READ(srcFdP, buf, sizeof(buf))) > 0)
	assert(FDH_WRITE(destFdP, buf, n) == n);
    assert(n == 0);
    FDH_REALLYCLOSE(srcFdP);
    FDH_REALLYCLOSE(destFdP);
    IH_RELEASE(srcH);
    IH_RELEASE(destH);
    return 0;
}

void
PrintInodeList(void)
{
    register struct ViceInodeInfo *ip;
    struct ViceInodeInfo *buf;
    struct afs_stat status;
    register nInodes;

    assert(afs_fstat(inodeFd, &status) == 0);
    buf = (struct ViceInodeInfo *)malloc(status.st_size);
    assert(buf != NULL);
    nInodes = status.st_size / sizeof(struct ViceInodeInfo);
    assert(read(inodeFd, buf, status.st_size) == status.st_size);
    for (ip = buf; nInodes--; ip++) {
	Log("Inode:%s, linkCount=%d, size=%#llx, p=(%u,%u,%u,%u)\n",
	    PrintInode(NULL, ip->inodeNumber), ip->linkCount,
	    (afs_uintmax_t) ip->byteCount, ip->u.param[0], ip->u.param[1],
	    ip->u.param[2], ip->u.param[3]);
    }
    free(buf);
}

void
PrintInodeSummary(void)
{
    int i;
    struct InodeSummary *isp;

    for (i = 0; i < nVolumesInInodeFile; i++) {
	isp = &inodeSummary[i];
	Log("VID:%u, RW:%u, index:%d, nInodes:%d, nSpecialInodes:%d, maxUniquifier:%u, volSummary\n", isp->volumeId, isp->RWvolumeId, isp->index, isp->nInodes, isp->nSpecialInodes, isp->maxUniquifier);
    }
}

void
PrintVolumeSummary(void)
{
    int i;
    struct VolumeSummary *vsp;

    for (i = 0, vsp = volumeSummaryp; i < nVolumes; vsp++, i++) {
	Log("fileName:%s, header, wouldNeedCallback\n", vsp->fileName);
    }
}

int
Fork(void)
{
    int f;
#ifdef AFS_NT40_ENV
    f = 0;
    assert(0);			/* Fork is never executed in the NT code path */
#else
    f = fork();
    assert(f >= 0);
#endif
    return f;
}

void
Exit(code)
     int code;
{
    if (ShowLog)
	showlog();
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
    assert(pid != -1);
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
	(void)strftime(timestamp, 20, "%m/%d/%Y %T", lt);
    else
	(void)strftime(timestamp, 20, "%m/%d/%Y %H:%M", lt);
    return timestamp;
}

void
CheckLogFile(void)
{
    char oldSlvgLog[AFSDIR_PATH_MAX];

#ifndef AFS_NT40_ENV
    if (useSyslog) {
	ShowLog = 0;
	return;
    }
#endif

    strcpy(oldSlvgLog, AFSDIR_SERVER_SLVGLOG_FILEPATH);
    strcat(oldSlvgLog, ".old");
    if (!logFile) {
	renamefile(AFSDIR_SERVER_SLVGLOG_FILEPATH, oldSlvgLog);
	logFile = afs_fopen(AFSDIR_SERVER_SLVGLOG_FILEPATH, "a");

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
TimeStampLogFile(void)
{
    char stampSlvgLog[AFSDIR_PATH_MAX];
    struct tm *lt;
    time_t now;

    now = time(0);
    lt = localtime(&now);
    (void)afs_snprintf(stampSlvgLog, sizeof stampSlvgLog,
		       "%s.%04d-%02d-%02d.%02d:%02d:%02d",
		       AFSDIR_SERVER_SLVGLOG_FILEPATH, lt->tm_year + 1900,
		       lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min,
		       lt->tm_sec);

    /* try to link the logfile to a timestamped filename */
    /* if it fails, oh well, nothing we can do */
    link(AFSDIR_SERVER_SLVGLOG_FILEPATH, stampSlvgLog);
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

    rewind(logFile);
    fclose(logFile);

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
    {
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
    {
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
ToString(char *s)
{
    register char *p;
    p = (char *)malloc(strlen(s) + 1);
    assert(p != NULL);
    strcpy(p, s);
    return p;

}

/* Remove the FORCESALVAGE file */
void
RemoveTheForce(char *path)
{
    if (!Testing && ForceSalvage) {
	if (chdir(path) == 0)
	    unlink("FORCESALVAGE");
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

    assert(chdir(path) != -1);

    return (afs_stat("FORCESALVAGE", &force) == 0);
}
#else
/*
 * UseTheForceLuke -	see if we can use the force
 *
 * NOTE:
 *	The VRMIX fsck will not muck with the filesystem it is supposedly
 *	fixing and create a "FORCESAVAGE" file (by design).  Instead, we
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
