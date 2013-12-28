/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 *      Module:		vol-salvage.h
 */

#ifndef __vol_salvage_h_
#define __vol_salvage_h_

#define SalvageVersion "2.4"

#include "salvage.h"
#include "volinodes.h"

/* salvager data structures */
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
};
#define readOnly(isp)	((isp)->volumeId != (isp)->RWvolumeId)

struct VolumeSummary {		/* Volume summary an entry for each
				 * volume in a volume directory.
				 * Assumption: one volume directory per
				 * partition */
    struct VolumeHeader header;
    /* volume number, rw volume number, inode
     * numbers of each major component of
     * the volume */
    IHandle_t *volumeInfoHandle;
    char deleted;               /* did we delete this volume? */
    byte wouldNeedCallback;	/* set if the file server should issue
				 * call backs for all the files in this volume when
				 * the volume goes back on line */
    byte unused;                /* is this volume 'extra'? i.e. not referenced
                                 * by anything? */
};

struct VnodeInfo {
    IHandle_t *handle;		/* Inode containing this index */
    afs_sfsize_t nVnodes;	/* Total number of vnodes in index */
    afs_sfsize_t nAllocatedVnodes;	/* Total number actually used */
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
};

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

struct SalvInfo;

#define ORPH_IGNORE 0
#define ORPH_REMOVE 1
#define ORPH_ATTACH 2


/* command line options */
extern int debug;			/* -d flag */
extern int Testing;		        /* -n flag */
extern int ListInodeOption;		/* -i flag */
extern int ShowRootFiles;		/* -r flag */
extern int RebuildDirs;		        /* -sal flag */
extern int Parallel;		        /* -para X flag */
extern int PartsPerDisk;		/* Salvage up to 8 partitions on same disk sequentially */
extern int forceR;			/* -b flag */
extern int ShowLog;		        /* -showlog flag */
extern int ShowSuid;		        /* -showsuid flag */
extern int ShowMounts;		        /* -showmounts flag */
extern int orphans;	                /* -orphans option */
extern int Showmode;

#ifndef AFS_NT40_ENV
extern int useSyslog;		        /* -syslog flag */
extern int useSyslogFacility;	        /* -syslogfacility option */
#endif

#define	MAXPARALLEL	32

extern int OKToZap;			/* -o flag */
extern int ForceSalvage;		/* If salvage should occur despite the DONT_SALVAGE flag
					 * in the volume header */


#define ROOTINODE	2	/* Root inode of a 4.2 Unix file system
				 * partition */



extern char * tmpdir;
extern FILE *logFile;	        /* one of {/usr/afs/logs,/vice/file}/SalvageLog */


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
extern childJob_t myjob;

extern int nt_SalvagePartition(char *partName, int jobn);
extern int nt_SetupPartitionSalvage(void *datap, int len);

typedef struct {
    struct InodeSummary *svgp_inodeSummaryp;
    int svgp_count;
    struct SalvInfo *svgp_salvinfo;
} SVGParms_t;
#endif /* AFS_NT40_ENV */

extern int canfork;


/* prototypes */
extern void Exit(int code) AFS_NORETURN;
extern int Fork(void);
extern int Wait(char *prog);
extern char *ToString(const char *s);
extern int AskDAFS(void);
extern void AskOffline(struct SalvInfo *salvinfo, VolumeId volumeId);
extern void AskOnline(struct SalvInfo *salvinfo, VolumeId volumeId);
extern void AskDelete(struct SalvInfo *salvinfo, VolumeId volumeId);
extern void CheckLogFile(char * log_path);
#ifndef AFS_NT40_ENV
extern void TimeStampLogFile(char * log_path);
#endif
extern void ClearROInUseBit(struct VolumeSummary *summary);
extern void CopyAndSalvage(struct SalvInfo *salvinfo, struct DirSummary *dir);
extern int CopyInode(Device device, Inode inode1, Inode inode2, int rwvolume);
extern void CopyOnWrite(struct SalvInfo *salvinfo, struct DirSummary *dir);
extern void CountVolumeInodes(register struct ViceInodeInfo *ip, int maxInodes,
		       register struct InodeSummary *summary);
extern void DeleteExtraVolumeHeaderFile(struct SalvInfo *salvinfo,
                                        struct VolumeSummary *vsp);
extern void DistilVnodeEssence(struct SalvInfo *salvinfo, VolumeId vid,
                               VnodeClass class, Inode ino, Unique * maxu);
extern int GetInodeSummary(struct SalvInfo *salvinfo, FILE *inodeFile,
                           VolumeId singleVolumeNumber);
extern int GetVolumeSummary(struct SalvInfo *salvinfo,
			    VolumeId singleVolumeNumber);
extern int JudgeEntry(void *dirVal, char *name, afs_int32 vnodeNumber,
		      afs_int32 unique);
extern void MaybeZapVolume(struct SalvInfo *salvinfo, struct InodeSummary *isp,
                           char *message, int deleteMe, int check);
extern void ObtainSalvageLock(void);
extern void ObtainSharedSalvageLock(void);
extern void PrintInodeList(struct SalvInfo *salvinfo);
extern void PrintInodeSummary(struct SalvInfo *salvinfo);
extern int QuickCheck(struct SalvInfo *salvinfo, struct InodeSummary *isp,
                      int nVols);
extern void RemoveTheForce(char *path);
extern void SalvageDir(struct SalvInfo *salvinfo, char *name, VolumeId rwVid,
		       struct VnodeInfo *dirVnodeInfo, IHandle_t * alinkH,
		       int i, struct DirSummary *rootdir, int *rootdirfound);
extern void SalvageFileSysParallel(struct DiskPartition64 *partP);
extern void SalvageFileSys(struct DiskPartition64 *partP, VolumeId singleVolumeNumber);
extern void SalvageFileSys1(struct DiskPartition64 *partP,
			    VolumeId singleVolumeNumber);
extern int SalvageHeader(struct SalvInfo *salvinfo, struct afs_inode_info *sp,
                        struct InodeSummary *isp, int check, int *deleteMe);
extern int SalvageIndex(struct SalvInfo *salvinfo, Inode ino, VnodeClass class,
                        int RW, struct ViceInodeInfo *ip, int nInodes,
                        struct VolumeSummary *volSummary, int check);
extern int SalvageVnodes(struct SalvInfo *salvinfo, struct InodeSummary *rwIsp,
                        struct InodeSummary *thisIsp,
                        struct ViceInodeInfo *inodes, int check);
extern int SalvageVolume(struct SalvInfo *salvinfo, struct InodeSummary *rwIsp,
                         IHandle_t * alinkH);
extern void DoSalvageVolumeGroup(struct SalvInfo *salvinfo,
                                 struct InodeSummary *isp, int nVols);
#ifdef AFS_NT40_ENV
extern void SalvageVolumeGroup(struct SalvInfo *salvinfo, struct InodeSummary *isp, int nVols);
#else
#define SalvageVolumeGroup DoSalvageVolumeGroup
#endif
extern int SalvageVolumeHeaderFile(struct SalvInfo *salvinfo,
                                   struct InodeSummary *isp,
                                   struct ViceInodeInfo *inodes, int RW,
                                   int check, int *deleteMe);
extern void showlog(void);
extern int UseTheForceLuke(char *path);



#endif /* __vol_salvage_h_ */
