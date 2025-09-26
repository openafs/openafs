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

/*
	System:		VICE-TWO
	Module:		partition.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#ifndef AFS_VOL_PARTITION_H
#define AFS_VOL_PARTITION_H

#include <afs/param.h>
#include "nfs.h"
#if	defined(AFS_HPUX_ENV)
#define	AFS_DSKDEV	"/dev/dsk"
#define	AFS_RDSKDEV	"/dev/rdsk/"
#define	AFS_LVOLDEV	"/dev/vg0"
#define	AFS_ACVOLDEV	"/dev/ac"
#define	AFS_RACVOLDEV	"/dev/ac/r"
#else
#define	AFS_DSKDEV	"/dev"
#define	AFS_RDSKDEV	"/dev/r"
#endif

#include <afs/afs_lock.h>
#ifdef AFS_DEMAND_ATTACH_FS
# include <pthread.h>
#endif

/* All Vice partitions on a server will have the following name prefix */
#define VICE_PARTITION_PREFIX	"/vicep"
#define VICE_PREFIX_SIZE	(sizeof(VICE_PARTITION_PREFIX)-1)

/* If a file by this name exists in a /vicepX directory, it means that
 * this directory should be used as an AFS partition even if it's not
 * on a separate partition (for instance if it's part of a large /).
 * This feature only works with the NAMEI fileserver.
 */
#ifdef AFS_NAMEI_ENV
#define VICE_ALWAYSATTACH_FILE	"AlwaysAttach"
#endif

/* If a file by this name exists in a /vicepX directory, it means that
 * this directory should NOT be used as an AFS partition.
 */
#define VICE_NEVERATTACH_FILE	"NeverAttach"

/**
 * abstraction for files used for file-locking.
 */
struct VLockFile {
    FD_t fd;                /**< fd holding the lock(s) */
    char *path;             /**< path to the lock file */
    int refcount;           /**< how many locks we have on the file */

#ifdef AFS_PTHREAD_ENV
    pthread_mutex_t mutex;  /**< lock for the VLockFile struct */
#endif /* AFS_PTHREAD_ENV */
};

#ifdef AFS_DEMAND_ATTACH_FS
/*
 * flag bits for 'flags' in struct VDiskLock.
 */
#define VDISKLOCK_ACQUIRING  0x1   /**< is someone waiting for an fs lock? */
#define VDISKLOCK_ACQUIRED   0x2   /**< we have an fs lock */

/**
 * on-disk locking mechanism.
 */
struct VDiskLock {
    struct VLockFile *lockfile; /**< file holding the locks */
    afs_uint32 offset;          /**< what offset we lock in the file */

    struct Lock rwlock;         /**< rw lock for inter-thread locking */
    pthread_mutex_t mutex;      /**< lock for the DiskLock object itself */
    pthread_cond_t cv;          /**< cond var for 'acquiring' changes */

    int lockers;                /**< # of callers that have this locked; */

    unsigned int flags;         /**< see above for flag bits */
};
#endif /* AFS_DEMAND_ATTACH_FS */


/* For NT, the roles of "name" and "devName" are no longer reversed. 
 * That is, "name" refers to the canonical name (/vicep) style and
 * "devName" refers to drive name.
 *
 * The NT version of VInitPartition does the intial setup. There is an NT
 * variant for VGetPartition as well. Also, the VolPartitionInfo RPC does
 * a swap before sending the data out on the wire.
 */
struct DiskPartition64 {
    struct DiskPartition64 *next;
    char *name;			/* Mounted partition name */
    char *devName;		/* Device mounted on */
    Device device;		/* device number */
    afs_int32 index;            /* partition index (0<=x<=VOLMAXPARTS) */
    FD_t lock_fd;		/* File descriptor of this partition if locked; otherwise -1;
				 * Not used by the file server */
    afs_int64 free;		/* Total number of blocks (1K) presumed
				 * available on this partition (accounting
				 * for the minfree parameter for the
				 * partition).  This is adjusted
				 * approximately by the sizes of files
				 * and directories read/written, and
				 * periodically the superblock is read and
				 * this is recomputed.  This number can
				 * be negative, if the partition starts
				 * out too full */
    afs_int64 totalUsable;	/* Total number of blocks available on this
				 * partition, taking into account the minfree
				 * parameter for the partition (see the
				 * 4.2bsd command tunefs, but note that the
				 * bug mentioned there--that the superblock
				 * is not reread--does not apply here.  The
				 * superblock is re-read periodically by
				 * VSetPartitionDiskUsage().) */
    afs_int64 minFree;		/* Number blocks to be kept free, as last read
				 * from the superblock */
    int flags;
    afs_int64 f_files;		/* total number of files in this partition */
#ifdef AFS_DEMAND_ATTACH_FS
    struct {
	struct rx_queue head;   /* list of volumes on this partition (VByPList) */
	afs_uint32 len;         /* length of volume list */
	int busy;               /* asynch vol list op in progress */
	pthread_cond_t cv;      /* vol_list.busy change cond var */
    } vol_list;
    struct VLockFile headerLockFile;
    struct VDiskLock headerLock; /* lock for the collective headers on the partition */

    struct VLockFile volLockFile; /* lock file for individual volume locks */
#endif /* AFS_DEMAND_ATTACH_FS */
};

struct DiskPartitionStats64 {
    afs_int64 free;
    afs_int64 totalUsable;
    afs_int64 minFree;
    afs_int64 f_files;
#ifdef AFS_DEMAND_ATTACH_FS
    afs_int32 vol_list_len;
#endif
};

#define	PART_DONTUPDATE	1
#define PART_DUPLICATE  2	/* NT - used if we find more than one partition
				 * using the same drive. Will be dumped before
				 * all partitions attached.
				 */

#ifdef AFS_NT40_ENV
#include <WINNT/vptab.h>
#endif


struct Volume;			/* Potentially forward definition */

extern struct DiskPartition64 *DiskPartitionList;
extern struct DiskPartition64 *VGetPartition(char * name, int abortp);
extern struct DiskPartition64 *VGetPartition_r(char * name, int abortp);
#ifdef AFS_DEMAND_ATTACH_FS
extern struct DiskPartition64 *VGetPartitionById(afs_int32 index, int abortp);
extern struct DiskPartition64 *VGetPartitionById_r(afs_int32 index, int abortp);
extern int VPartHeaderLock(struct DiskPartition64 *dp, int locktype);
extern void VPartHeaderUnlock(struct DiskPartition64 *dp, int locktype);
#endif
extern int VAttachPartitions(void);
extern void VLockPartition(char *name);
extern void VLockPartition_r(char *name);
extern void VUnlockPartition(char *name);
extern void VUnlockPartition_r(char *name);
extern void VResetDiskUsage(void);
extern void VResetDiskUsage_r(void);
extern void VSetPartitionDiskUsage(struct DiskPartition64 *dp);
extern void VSetPartitionDiskUsage_r(struct DiskPartition64 *dp);
extern char *VPartitionPath(struct DiskPartition64 *p);
extern void VAdjustDiskUsage(Error * ec, struct Volume *vp,
			     afs_sfsize_t blocks, afs_sfsize_t checkBlocks);
extern int VDiskUsage(struct Volume *vp, afs_sfsize_t blocks);
extern void VPrintDiskStats(void);
extern int VInitPartitionPackage(void);

#endif /* AFS_VOL_PARTITION_H */
