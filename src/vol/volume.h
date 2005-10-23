/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	System:		VICE-TWO
	Module:		volume.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef __volume_h
#define	__volume_h  1

#include <afs/afssyscalls.h>
#include "voldefs.h"
#include "ihandle.h"
#define VolumeWriteable(vp)		(V_type(vp)==readwriteVolume)
#define VolumeWriteable2(vol)		(vol.type == readwriteVolume)
typedef bit32 FileOffset;	/* Offset in this file */
#define Date afs_uint32

#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#include <pthread.h>
extern pthread_mutex_t vol_glock_mutex;
extern pthread_mutex_t vol_attach_mutex;
extern pthread_mutex_t vol_fsync_mutex;
extern pthread_mutex_t vol_trans_mutex;
extern pthread_cond_t vol_put_volume_cond;
extern pthread_cond_t vol_sleep_cond;
extern int vol_attach_threads;
/* this lock has been deprecated */
#define VATTACH_LOCK
#define VATTACH_UNLOCK
#define VOL_LOCK \
    assert(pthread_mutex_lock(&vol_glock_mutex) == 0)
#define VOL_UNLOCK \
    assert(pthread_mutex_unlock(&vol_glock_mutex) == 0)
#define VFSYNC_LOCK \
    assert(pthread_mutex_lock(&vol_fsync_mutex) == 0)
#define VFSYNC_UNLOCK \
    assert(pthread_mutex_unlock(&vol_fsync_mutex) == 0)
#define VTRANS_LOCK \
    assert(pthread_mutex_lock(&vol_trans_mutex) == 0)
#define VTRANS_UNLOCK \
    assert(pthread_mutex_unlock(&vol_trans_mutex) == 0)
#else /* AFS_PTHREAD_ENV */
#define VATTACH_LOCK
#define VATTACH_UNLOCK
#define VOL_LOCK
#define VOL_UNLOCK
#define VFSYNC_LOCK
#define VFSYNC_UNLOCK
#define VTRANS_LOCK
#define VTRANS_UNLOCK
#endif /* AFS_PTHREAD_ENV */

typedef enum { fileServer, volumeUtility, salvager } ProgramType;
extern ProgramType programType;	/* The type of program using the package */

/* Some initialization parameters for the volume package */
/* Add new initialization parameters here */
extern int (*V_BreakVolumeCallbacks) ();
extern int (*vol_PollProc) ();
#define	DOPOLL	((vol_PollProc)? (*vol_PollProc)() : 0)

struct versionStamp {		/* Version stamp for critical volume files */
    bit32 magic;		/* Magic number */
    bit32 version;		/* Version number of this file, or software
				 * that created this file */
};

/* Magic numbers and version stamps for each type of file */
#define VOLUMEHEADERMAGIC	((bit32)0x88a1bb3c)
#define VOLUMEINFOMAGIC		((bit32)0x78a1b2c5)
#define	SMALLINDEXMAGIC		0x99776655
#define LARGEINDEXMAGIC		0x88664433
#define	MOUNTMAGIC		0x9a8b7c6d
#define ACLMAGIC		0x88877712
#define LINKTABLEMAGIC		0x99877712

#define VOLUMEHEADERVERSION	1
#define VOLUMEINFOVERSION	1
#define	SMALLINDEXVERSION	1
#define	LARGEINDEXVERSION	1
#define	MOUNTVERSION		1
#define ACLVERSION		1
#define LINKTABLEVERSION	1

/*
 * Define whether we are keeping detailed statistics on volume dealings.
 */
#define OPENAFS_VOL_STATS	1

#if OPENAFS_VOL_STATS
/*
 * Define various indices and counts used in keeping volume-level statistics.
 */
#define VOL_STATS_NUM_RWINFO_FIELDS 4

#define VOL_STATS_SAME_NET	0	/*Within same site (total) */
#define VOL_STATS_SAME_NET_AUTH 1	/*Within same site (authenticated);
					 * (must be 1 more than above) */
#define VOL_STATS_DIFF_NET	2	/*From external site (total) */
#define VOL_STATS_DIFF_NET_AUTH 3	/*From external site (authenticated)
					 * (must be 1 more than above) */

#define VOL_STATS_NUM_TIME_RANGES 6

#define VOL_STATS_TIME_CAP_0	    60	/*60 seconds */
#define VOL_STATS_TIME_CAP_1	   600	/*10 minutes, in seconds */
#define VOL_STATS_TIME_CAP_2	  3600	/*1 hour, in seconds */
#define VOL_STATS_TIME_CAP_3	 86400	/*1 day, in seconds */
#define VOL_STATS_TIME_CAP_4	604800	/*1 week, in seconds */

#define VOL_STATS_NUM_TIME_FIELDS 6

#define VOL_STATS_TIME_IDX_0	0	/*0 secs to 60 secs */
#define VOL_STATS_TIME_IDX_1	1	/*1 min to 10 mins */
#define VOL_STATS_TIME_IDX_2	2	/*10 mins to 60 mins */
#define VOL_STATS_TIME_IDX_3	3	/*1 hr to 24 hrs */
#define VOL_STATS_TIME_IDX_4	4	/*1 day to 7 days */
#define VOL_STATS_TIME_IDX_5	5	/*Greater than 1 week */
#endif /* OPENAFS_VOL_STATS */

/* Volume header.  This is the contents of the named file representing
 * the volume.  Read-only by the file server!
 */
typedef struct VolumeHeader {
    struct versionStamp stamp;	/* Must be first field */
    VolumeId id;		/* Volume number */
    VolumeId parent;		/* Read-write volume number (or this volume
				 * number if this is a read-write volume) */
    Inode volumeInfo;
    Inode smallVnodeIndex;
    Inode largeVnodeIndex;
    Inode volumeAcl;
    Inode volumeMountTable;
    Inode linkTable;
} VolumeHeader_t;


typedef struct VolumeDiskHeader {
    struct versionStamp stamp;	/* Must be first field */
    VolumeId id;		/* Volume number */
    VolumeId parent;		/* Read-write volume number (or this volume
				 * number if this is a read-write volume) */
    afs_int32 volumeInfo_lo;
    afs_int32 smallVnodeIndex_lo;
    afs_int32 largeVnodeIndex_lo;
    afs_int32 volumeAcl_lo;
    afs_int32 volumeMountTable_lo;
    afs_int32 volumeInfo_hi;
    afs_int32 smallVnodeIndex_hi;
    afs_int32 largeVnodeIndex_hi;
    afs_int32 volumeAcl_hi;
    afs_int32 volumeMountTable_hi;
    afs_int32 linkTable_lo;
    afs_int32 linkTable_hi;
    /* If you add fields, add them before here and reduce the size of  array */
    bit32 reserved[3];
} VolumeDiskHeader_t;

/* A vnode index file header */
struct IndexFileHeader {
    struct versionStamp stamp;
};


/******************************************************************************/
/* Volume Data which is stored on disk and can also be maintained in memory.  */
/******************************************************************************/
typedef struct VolumeDiskData {
    struct versionStamp stamp;	/* Must be first field */
    VolumeId id;		/* Volume id--unique over all systems */
#define VNAMESIZE 32		/* including 0 byte */
    char name[VNAMESIZE];	/* Unofficial name for the volume */
    byte inUse;			/* Volume is being used (perhaps it is online),
				 * or the system crashed while it was used */
    byte inService;		/* Volume in service, not necessarily on line
				 * This bit is set by an operator/system
				 * programmer.  Manually taking a volume offline
				 * always clears the inService bit. Taking
				 * it out of service also takes it offline */
    byte blessed;		/* Volume is administratively blessed with
				 * the ability to go on line.  Set by a system
				 * administrator. Clearing this bit will
				 * take the volume offline */
    byte needsSalvaged;		/* Volume needs salvaged--an unrecoverable
				 * error occured to the volume.  Note:  a volume
				 * may still require salvage even if this
				 * flag isn't set--e.g. if a system crash
				 * occurred while the volume was on line. */
    bit32 uniquifier;		/* Next vnode uniquifier for this volume */
    int type;			/* */
    VolId parentId;		/* Id of parent, if type==readonly */
    VolId cloneId;		/* Latest read-only clone, if type==readwrite,
				 * 0 if the volume has never been cloned.  Note: the
				 * indicated volume does not necessarily exist (it
				 * may have been deleted since cloning). */
    VolId backupId;		/* Latest backup copy of this read write volume */
    VolId restoredFromId;	/* The id in the dump this volume was restored from--used simply
				 * to make sure that an incremental dump is not restored on top
				 * of something inappropriate:  Note:  this field itself is NEVER
				 * dumped!!! */
    byte needsCallback;		/* Set by the salvager if anything was changed
				 * about the volume.  Note:  this is not set by
				 * clone/makebackups when setting the copy-on-write
				 * flag in directories; this flag is not seen by
				 * the clients. */
#define DESTROY_ME	0xD3
    byte destroyMe;		/* If this is set to DESTROY_ME, then the salvager should destroy
				 * this volume; it is bogus (left over from an aborted  volume move,
				 * for example).  Note:  if this flag is on, then inService should
				 * be OFF--only the salvager checks this flag */
#ifdef ALPHA_DUX40_ENV
#define DONT_SALVAGE	0xE6
#else				/* ALPHA_DUX40_ENV */
#define DONT_SALVAGE	0xE5
#endif				/* ALPHA_DUX40_ENV */
    byte dontSalvage;		/* If this is on, then don't bother salvaging this volume */
    byte reserveb3;

    bit32 reserved1[6];


    /* Administrative stuff */
    int maxquota;		/* Quota maximum, 1K blocks */
    int minquota;		/* Quota minimum, 1K blocks */
    int maxfiles;		/* Maximum number of files (i.e. inodes) */
    bit32 accountNumber;	/* Uninterpreted account number */
    bit32 owner;		/* The person administratively responsible
				 * for this volume */
    int reserved2[8];		/* Other administrative constraints */

    /* Resource usage & statistics */
    int filecount;		/* Actual number of files */
    int diskused;		/* Actual disk space used, 1K blocks */
    int dayUse;			/* Metric for today's usage of this volume so far */
    int weekUse[7];		/* Usage of the volume for the last week.
				 * weekUse[0] is for most recent complete 24 hour period
				 * of measurement; week[6] is 7 days ago */
    Date dayUseDate;		/* Date the dayUse statistics refer to; the week use stats
				 * are the preceding 7 days */
    unsigned int volUpdateCounter; /*incremented at every update of volume*/
    int reserved3[10];		/* Other stats here */

    /* Server supplied dates */
    Date creationDate;		/* Creation date for a read/write
				 * volume; cloning date for original copy of
				 * a readonly volume (replicated volumes have
				 * the same creation date) */
    Date accessDate;		/* Last access time by a user, large granularity */
    Date updateDate;		/* Last modification by user */
    Date expirationDate;	/* 0 if it never expires */
    Date backupDate;		/* last time a backup clone was taken */

    /* Time that this copy of this volume was made.  NEVER backed up.  This field is only
     * set when the copy is created */
    Date copyDate;

#if OPENAFS_VOL_STATS
    bit32 stat_initialized;	/*Are the stat fields below set up? */
    bit32 reserved4[7];
#else
    bit32 reserved4[8];
#endif				/* OPENAFS_VOL_STATS */

    /* messages */
#define VMSGSIZE 128
    char offlineMessage[VMSGSIZE];	/* Why the volume is offline */
#if OPENAFS_VOL_STATS
#define VOL_STATS_BYTES 128
    /*
     * Keep per-volume aggregate statistics on type and distance of access,
     * along with authorship info.
     */
    bit32 stat_reads[VOL_STATS_NUM_RWINFO_FIELDS];
    bit32 stat_writes[VOL_STATS_NUM_RWINFO_FIELDS];
    bit32 stat_fileSameAuthor[VOL_STATS_NUM_TIME_FIELDS];
    bit32 stat_fileDiffAuthor[VOL_STATS_NUM_TIME_FIELDS];
    bit32 stat_dirSameAuthor[VOL_STATS_NUM_TIME_FIELDS];
    bit32 stat_dirDiffAuthor[VOL_STATS_NUM_TIME_FIELDS];
#else
    char motd[VMSGSIZE];	/* Volume "message of the day" */
#endif				/* OPENAFS_VOL_STATS */

} VolumeDiskData;


/**************************************/
/* Memory resident volume information */
/**************************************/
typedef struct Volume {
    struct Volume *hashNext;	/* Next in hash resolution table */
    VolumeId hashid;		/* Volume number -- for hash table lookup */
    struct volHeader *header;	/* Cached disk data */
    Device device;		/* Unix device for the volume */
    struct DiskPartition
     *partition;		/* Information about the Unix partition */
    struct vnodeIndex {
	IHandle_t *handle;	/* Unix inode holding this index */
	byte *bitmap;		/* Index bitmap */
	afs_uint32 bitmapSize;	/* length of bitmap, in bytes */
	afs_uint32 bitmapOffset;	/* Which byte address of the first long to
					 * start search from in bitmap */
    } vnodeIndex[nVNODECLASSES];
    IHandle_t *linkHandle;
    Unique nextVnodeUnique;	/* Derived originally from volume uniquifier.
				 * This is the actual next version number to
				 * assign; the uniquifier is bumped by 200 and
				 * and written to disk every 200 file creates
				 * If the volume is shutdown gracefully, the
				 * uniquifier should be rewritten with the
				 * value nextVnodeVersion */
    IHandle_t *diskDataHandle;	/* Unix inode holding general volume info */
    bit16 vnodeHashOffset;	/* Computed by HashOffset function in vnode.h.
				 * Assigned to the volume when initialized. 
				 * Added to vnode number for hash table index */
    byte shuttingDown;		/* This volume is going to be detached */
    byte goingOffline;		/* This volume is going offline */
    bit32 cacheCheck;		/* Online sequence number to be used to invalidate vnode cache entries
				 * that stayed around while a volume was offline */
    short nUsers;		/* Number of users of this volume header */
    byte needsPutBack;		/* For a volume utility, this flag is set if we need
				 * to give the volume back when we detach it.  The server has
				 * certain modes where it doesn't detach the volume, and
				 * if we give it back spuriously, the server aborts.  This field
				 * is meaningless on the file server */
    byte specialStatus;		/* An error code to return on VGetVolume: the
				 * volume is unavailable for the reason quoted,
				 * currently VBUSY or VMOVED */
    afs_uint32 updateTime;	/* Time that this volume was put on the updated
				 * volume list--the list of volumes that will be
				 * salvaged should the file server crash */
} Volume;

struct volHeader {
    struct volHeader *prev, *next;	/* LRU pointers */
    VolumeDiskData diskstuff;	/* General volume info read from disk */
    Volume *back;		/* back pointer to current volume structure */
};

/* These macros are used to export fields within the volume header.  This was added
   to facilitate changing the actual representation */

#define V_device(vp)		((vp)->device)
#define V_partition(vp)		((vp)->partition)
#define V_diskDataHandle(vp)	((vp)->diskDataHandle)
#define V_vnodeIndex(vp)	((vp)->vnodeIndex)
#define V_nextVnodeUnique(vp)	((vp)->nextVnodeUnique)
#define V_linkHandle(vp)	((vp)->linkHandle)

/* N.B. V_id must be this, rather than vp->id, or some programs will break, probably */
#define V_stamp(vp)		((vp)->header->diskstuff.stamp)
#define V_id(vp)		((vp)->header->diskstuff.id)
#define V_name(vp)		((vp)->header->diskstuff.name)
#define V_inUse(vp)		((vp)->header->diskstuff.inUse)
#define V_inService(vp)		((vp)->header->diskstuff.inService)
#define V_blessed(vp)		((vp)->header->diskstuff.blessed)
#define V_needsSalvaged(vp)	((vp)->header->diskstuff.needsSalvaged)
#define V_uniquifier(vp)	((vp)->header->diskstuff.uniquifier)
#define V_type(vp)		((vp)->header->diskstuff.type)
#define V_parentId(vp)		((vp)->header->diskstuff.parentId)
#define V_cloneId(vp)		((vp)->header->diskstuff.cloneId)
#define V_backupId(vp)		((vp)->header->diskstuff.backupId)
#define V_restoredFromId(vp)	((vp)->header->diskstuff.restoredFromId)
#define V_needsCallback(vp)	((vp)->header->diskstuff.needsCallback)
#define V_destroyMe(vp)		((vp)->header->diskstuff.destroyMe)
#define V_dontSalvage(vp)	((vp)->header->diskstuff.dontSalvage)
#define V_maxquota(vp)		((vp)->header->diskstuff.maxquota)
#define V_minquota(vp)		((vp)->header->diskstuff.minquota)
#define V_maxfiles(vp)		((vp)->header->diskstuff.maxfiles)
#define V_accountNumber(vp)	((vp)->header->diskstuff.accountNumber)
#define V_owner(vp)		((vp)->header->diskstuff.owner)
#define V_filecount(vp)		((vp)->header->diskstuff.filecount)
#define V_diskused(vp)		((vp)->header->diskstuff.diskused)
#define V_dayUse(vp)		((vp)->header->diskstuff.dayUse)
#define V_weekUse(vp)		((vp)->header->diskstuff.weekUse)
#define V_dayUseDate(vp)	((vp)->header->diskstuff.dayUseDate)
#define V_creationDate(vp)	((vp)->header->diskstuff.creationDate)
#define V_accessDate(vp)	((vp)->header->diskstuff.accessDate)
#define V_updateDate(vp)	((vp)->header->diskstuff.updateDate)
#define V_expirationDate(vp)	((vp)->header->diskstuff.expirationDate)
#define V_backupDate(vp)	((vp)->header->diskstuff.backupDate)
#define V_copyDate(vp)		((vp)->header->diskstuff.copyDate)
#define V_offlineMessage(vp)	((vp)->header->diskstuff.offlineMessage)
#define V_disk(vp)		((vp)->header->diskstuff)
#define V_motd(vp)		((vp)->header->diskstuff.motd)
#if OPENAFS_VOL_STATS
#define V_stat_initialized(vp)	((vp)->header->diskstuff.stat_initialized)
#define V_stat_area(vp)		(((vp)->header->diskstuff.stat_reads))
#define V_stat_reads(vp, idx)	(((vp)->header->diskstuff.stat_reads)[idx])
#define V_stat_writes(vp, idx)	(((vp)->header->diskstuff.stat_writes)[idx])
#define V_stat_fileSameAuthor(vp, idx) (((vp)->header->diskstuff.stat_fileSameAuthor)[idx])
#define V_stat_fileDiffAuthor(vp, idx) (((vp)->header->diskstuff.stat_fileDiffAuthor)[idx])
#define V_stat_dirSameAuthor(vp, idx)  (((vp)->header->diskstuff.stat_dirSameAuthor)[idx])
#define V_stat_dirDiffAuthor(vp, idx)  (((vp)->header->diskstuff.stat_dirDiffAuthor)[idx])
#endif /* OPENAFS_VOL_STATS */
#define V_volUpCounter(vp)		((vp)->header->diskstuff.volUpdateCounter)

/* File offset computations.  The offset values in the volume header are
   computed with these macros -- when the file is written only!! */
#define VOLUME_MOUNT_TABLE_OFFSET(Volume)	(sizeof (VolumeDiskData))
#define VOLUME_BITMAP_OFFSET(Volume)	\
	(sizeof (VolumeDiskData) + (Volume)->disk.mountTableSize)


extern char *VSalvageMessage;	/* Canonical message when a volume is forced
				 * offline */
extern Volume *VGetVolume(Error * ec, VolId volumeId);
extern Volume *VGetVolume_r(Error * ec, VolId volumeId);
extern void VPutVolume(Volume *);
extern void VPutVolume_r(Volume *);
extern void VOffline(Volume * vp, char *message);
extern void VOffline_r(Volume * vp, char *message);
extern int VConnectFS(void);
extern int VConnectFS_r(void);
extern Volume *VAttachVolume(Error * ec, VolumeId volumeId, int mode);
extern Volume *VAttachVolume_r(Error * ec, VolumeId volumeId, int mode);
extern Volume *VCreateVolume(Error * ec, char *partname, VolId volumeId,
			     VolId parentId);
extern Volume *VCreateVolume_r(Error * ec, char *partname, VolId volumeId,
			       VolId parentId);
extern VnodeId VAllocBitmapEntry(Error * ec, Volume * vp,
				 struct vnodeIndex *index);
extern VnodeId VAllocBitmapEntry_r(Error * ec, Volume * vp,
				   struct vnodeIndex *index);
extern void VFreeBitMapEntry(Error * ec, register struct vnodeIndex *index,
			     unsigned bitNumber);
extern void VFreeBitMapEntry_r(Error * ec, register struct vnodeIndex *index,
			       unsigned bitNumber);
extern int VolumeNumber(char *name);
extern char *VolumeExternalName(VolumeId volumeId);
extern Volume *VAttachVolumeByName(Error * ec, char *partition, char *name,
				   int mode);
extern Volume *VAttachVolumeByName_r(Error * ec, char *partition, char *name,
				     int mode);
extern void VShutdown(void);
extern void VUpdateVolume(Error * ec, Volume * vp);
extern void VUpdateVolume_r(Error * ec, Volume * vp);
extern void VAddToVolumeUpdateList(Error * ec, Volume * vp);
extern void VAddToVolumeUpdateList_r(Error * ec, Volume * vp);
extern void VDetachVolume(Error * ec, Volume * vp);
extern void VDetachVolume_r(Error * ec, Volume * vp);
extern void VForceOffline(Volume * vp);
extern void VForceOffline_r(Volume * vp);
extern void VBumpVolumeUsage(register Volume * vp);
extern void VBumpVolumeUsage_r(register Volume * vp);
extern void VSetDiskUsage(void);
extern void VPrintCacheStats(void);
extern void VReleaseVnodeFiles_r(Volume * vp);
extern void VCloseVnodeFiles_r(Volume * vp);
extern struct DiskPartition *VGetPartition(char *name, int abortp);
extern struct DiskPartition *VGetPartition_r(char *name, int abortp);
extern int VInitVolumePackage(ProgramType pt, int nLargeVnodes,
			      int nSmallVnodes, int connect, int volcache);
extern void DiskToVolumeHeader(VolumeHeader_t * h, VolumeDiskHeader_t * dh);
extern void VolumeHeaderToDisk(VolumeDiskHeader_t * dh, VolumeHeader_t * h);
extern void VTakeOffline_r(register Volume * vp);
extern void VTakeOffline(register Volume * vp);


/* Naive formula relating number of file size to number of 1K blocks in file */
/* Note:  we charge 1 block for 0 length files so the user can't store
   an inifite number of them; for most files, we give him the inode, vnode,
   and indirect block overhead, for FREE! */
#define nBlocks(bytes) ((afs_sfsize_t)((bytes) == 0? 1: (((afs_sfsize_t)(bytes))+1023)/1024))

/* Client process id -- file server sends a Check volumes signal back to the client at this pid */
#define CLIENTPID	"/vice/vol/clientpid"

/* Modes of attachment, for VAttachVolume[ByName] to convey to the file server */
#define	V_READONLY 1		/* Absolutely no updates will be done to the volume */
#define V_CLONE	   2		/* Cloning the volume:  if it is read/write, then directory
				 * version numbers will change.  Header will be updated.  If
				 * the volume is read-only, the file server may continue to
				 * server it; it may also continue to server it in read/write
				 * mode if the writes are deferred */
#define V_VOLUPD   3		/* General update or volume purge is possible.  Volume must
				 * go offline */
#define V_DUMP	   4		/* A dump of the volume is requested; the volume can be served
				 * read-only during this time */
#define V_SECRETLY 5		/* Secret attach of the volume.  This is used to attach a volume
				 * which the file server doesn't know about--and which it shouldn't
				 * know about yet, since the volume has just been created and
				 * is somewhat bogus.  Required to make sure that a file server
				 * never knows about more than one copy of the same volume--when
				 * a volume is moved from one partition to another on a single
				 * server */
#define V_PEEK     6		/* "Peek" at the volume without telling the fileserver.  This is
				 * similar to V_SECRETLY, but read-only.  It is used in cases where
				 * not impacting fileserver performance is more important than
				 * getting the most recent data. */


#if	defined(NEARINODE_HINT)
#define V_pref(vp,nearInode)  nearInodeHash(V_id(vp),(nearInode)); (nearInode) %= V_partition(vp)->f_files
#else
#define V_pref(vp,nearInode)   nearInode = 0
#endif /* NEARINODE_HINT */

#endif /* __volume_h */
