/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2006-2008 Sine Nomine Associates
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
#include "daemon_com.h"
#include "fssync.h"

#if 0
/** turn this on if you suspect a volume package locking bug */
#define VOL_LOCK_DEBUG 1
#endif

#ifdef VOL_LOCK_DEBUG
#define VOL_LOCK_ASSERT_HELD \
    osi_Assert(vol_glock_holder == pthread_self())
#define VOL_LOCK_ASSERT_UNHELD \
    osi_Assert(vol_glock_holder == 0)
#define _VOL_LOCK_SET_HELD \
    vol_glock_holder = pthread_self()
#define _VOL_LOCK_SET_UNHELD \
    vol_glock_holder = 0
#define VOL_LOCK_DBG_CV_WAIT_END \
    do { \
        VOL_LOCK_ASSERT_UNHELD; \
        _VOL_LOCK_SET_HELD; \
    } while(0)
#define VOL_LOCK_DBG_CV_WAIT_BEGIN \
    do { \
         VOL_LOCK_ASSERT_HELD; \
         _VOL_LOCK_SET_UNHELD; \
    } while(0)
#else
#define VOL_LOCK_ASSERT_HELD
#define VOL_LOCK_ASSERT_UNHELD
#define VOL_LOCK_DBG_CV_WAIT_BEGIN
#define VOL_LOCK_DBG_CV_WAIT_END
#endif


#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
extern pthread_mutex_t vol_glock_mutex;
extern pthread_mutex_t vol_trans_mutex;
extern pthread_cond_t vol_put_volume_cond;
extern pthread_cond_t vol_sleep_cond;
extern pthread_cond_t vol_vinit_cond;
extern ih_init_params vol_io_params;
extern int vol_attach_threads;
#ifdef VOL_LOCK_DEBUG
extern pthread_t vol_glock_holder;
#define VOL_LOCK \
    do { \
	MUTEX_ENTER(&vol_glock_mutex); \
	VOL_LOCK_ASSERT_UNHELD; \
	_VOL_LOCK_SET_HELD; \
    } while (0)
#define VOL_UNLOCK \
    do { \
        VOL_LOCK_ASSERT_HELD; \
	_VOL_LOCK_SET_UNHELD; \
	MUTEX_EXIT(&vol_glock_mutex); \
    } while (0)
#define VOL_CV_WAIT(cv) \
    do { \
        VOL_LOCK_DBG_CV_WAIT_BEGIN; \
	CV_WAIT((cv), &vol_glock_mutex); \
        VOL_LOCK_DBG_CV_WAIT_END; \
    } while (0)
#else /* !VOL_LOCK_DEBUG */
#define VOL_LOCK MUTEX_ENTER(&vol_glock_mutex)
#define VOL_UNLOCK MUTEX_EXIT(&vol_glock_mutex)
#define VOL_CV_WAIT(cv) CV_WAIT((cv), &vol_glock_mutex)
#endif /* !VOL_LOCK_DEBUG */

#define VSALVSYNC_LOCK MUTEX_ENTER(&vol_salvsync_mutex)
#define VSALVSYNC_UNLOCK MUTEX_EXIT(&vol_salvsync_mutex)
#define VTRANS_LOCK MUTEX_ENTER(&vol_trans_mutex)
#define VTRANS_UNLOCK MUTEX_EXIT(&vol_trans_mutex)
#else /* AFS_PTHREAD_ENV */
#define VOL_LOCK
#define VOL_UNLOCK
#define VSALVSYNC_LOCK
#define VSALVSYNC_UNLOCK
#define VTRANS_LOCK
#define VTRANS_UNLOCK
#endif /* AFS_PTHREAD_ENV */

/**
 * volume package program type enumeration.
 */
typedef enum {
    fileServer          = 1,    /**< the fileserver process */
    volumeUtility       = 2,    /**< any miscellaneous volume utility */
    salvager            = 3,    /**< standalone whole-partition salvager */
    salvageServer       = 4,    /**< dafs online salvager */
    debugUtility        = 5,    /**< fssync-debug or similar utility */
    volumeServer        = 6,    /**< the volserver process */
    volumeSalvager      = 7     /**< the standalone single-volume salvager */
} ProgramType;
extern ProgramType programType;	/* The type of program using the package */

/* Some initialization parameters for the volume package */
/* Add new initialization parameters here */
extern int (*V_BreakVolumeCallbacks) (VolumeId);
extern int (*vol_PollProc) (void);

#define	DOPOLL	((vol_PollProc)? (*vol_PollProc)() : 0)

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * variable error return code based upon programType and DAFS presence
 */
#define DAFS_VSALVAGE   ((programType == fileServer) ? VSALVAGING : VSALVAGE)
#else
#define DAFS_VSALVAGE   (VSALVAGE)
#endif

struct versionStamp {		/* Version stamp for critical volume files */
    bit32 magic;		/* Magic number */
    bit32 version;		/* Version number of this file, or software
				 * that created this file */
};

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * demand attach volume state enumeration.
 *
 * @note values must be contiguous in order for VIsValidState() to work correctly
 */
typedef enum {
    VOL_STATE_UNATTACHED        = 0,    /**< volume is unattached */
    VOL_STATE_PREATTACHED       = 1,    /**< volume has been pre-attached */
    VOL_STATE_ATTACHING         = 2,    /**< volume is transitioning to fully attached */
    VOL_STATE_ATTACHED          = 3,    /**< volume has been fully attached */
    VOL_STATE_UPDATING          = 4,    /**< volume is updating on-disk structures */
    VOL_STATE_GET_BITMAP        = 5,    /**< volume is getting bitmap entries */
    VOL_STATE_HDR_LOADING       = 6,    /**< volume is loading disk header */
    VOL_STATE_HDR_ATTACHING     = 7,    /**< volume is getting a header from the LRU */
    VOL_STATE_SHUTTING_DOWN     = 8,    /**< volume is shutting down */
    VOL_STATE_GOING_OFFLINE     = 9,    /**< volume is going offline */
    VOL_STATE_OFFLINING         = 10,   /**< volume is transitioning to offline */
    VOL_STATE_DETACHING         = 11,   /**< volume is transitioning to detached */
    VOL_STATE_SALVSYNC_REQ      = 12,   /**< volume is blocked on a salvsync request */
    VOL_STATE_SALVAGING         = 13,   /**< volume is being salvaged */
    VOL_STATE_ERROR             = 14,   /**< volume is in an error state */
    VOL_STATE_VNODE_ALLOC       = 15,   /**< volume is busy allocating a new vnode */
    VOL_STATE_VNODE_GET         = 16,   /**< volume is busy getting vnode disk data */
    VOL_STATE_VNODE_CLOSE       = 17,   /**< volume is busy closing vnodes */
    VOL_STATE_VNODE_RELEASE     = 18,   /**< volume is busy releasing vnodes */
    VOL_STATE_VLRU_ADD          = 19,   /**< volume is busy being added to a VLRU queue */
    VOL_STATE_DELETED           = 20,   /**< volume has been deleted by the volserver */
    VOL_STATE_SALVAGE_REQ       = 21,   /**< volume has been requested to be salvaged,
                                         *   but is waiting for other users to go away
                                         *   so it can be offlined */
    VOL_STATE_SCANNING_RXCALLS  = 22,   /**< volume is scanning vp->rx_call_list
                                         *   to interrupt RX calls */
    /* please add new states directly above this line */
    VOL_STATE_FREED             = 23,   /**< debugging aid */
    VOL_STATE_COUNT             = 24    /**< total number of valid states */
} VolState;

/**
 * V_attachFlags bits.
 */
enum VolFlags {
    VOL_HDR_ATTACHED      = 0x1,     /**< volume header is attached to Volume struct */
    VOL_HDR_LOADED        = 0x2,     /**< volume header contents are valid */
    VOL_HDR_IN_LRU        = 0x4,     /**< volume header is in LRU */
    VOL_IN_HASH           = 0x8,     /**< volume is in hash table */
    VOL_ON_VBYP_LIST      = 0x10,    /**< volume is on VByP list */
    VOL_IS_BUSY           = 0x20,    /**< volume is not to be free()d */
    VOL_ON_VLRU           = 0x40,    /**< volume is on the VLRU */
    VOL_HDR_DONTSALV      = 0x80,    /**< volume header DONTSALVAGE flag is set */
    VOL_LOCKED            = 0x100    /**< volume is disk-locked (@see VLockVolumeNB) */
};

/* VPrintExtendedCacheStats flags */
#define VOL_STATS_PER_CHAIN   0x1  /**< compute simple per-chain stats */
#define VOL_STATS_PER_CHAIN2  0x2  /**< compute per-chain stats that require scanning
				    *   every element of the chain */

/* VLRU_SetOptions options */
#define VLRU_SET_THRESH       1
#define VLRU_SET_INTERVAL     2
#define VLRU_SET_MAX          3
#define VLRU_SET_ENABLED      4

/**
 * VLRU queue names.
 */
typedef enum {
    VLRU_QUEUE_NEW        = 0,  /**< LRU queue for new volumes */
    VLRU_QUEUE_MID        = 1,  /**< survivor generation */
    VLRU_QUEUE_OLD        = 2,  /**< old generation */
    VLRU_QUEUE_CANDIDATE  = 3,  /**< soft detach candidate pool */
    VLRU_QUEUE_HELD       = 4,  /*   volumes which are not allowed
				 *   to be soft detached */
    VLRU_QUEUE_INVALID    = 5   /**< invalid queue id */
} VLRUQueueName;

/* default scanner timing parameters */
#define VLRU_DEFAULT_OFFLINE_THRESH (60*60*2) /* 2 hours */
#define VLRU_DEFAULT_OFFLINE_INTERVAL (60*2) /* 2 minutes */
#define VLRU_DEFAULT_OFFLINE_MAX 8 /* 8 volumes */


/**
 * DAFS thread-specific options structure
 */
typedef struct VThreadOptions {
     int disallow_salvsync;     /**< whether or not salvsync calls are allowed
				 *   on this thread (deadlock prevention). */
} VThreadOptions_t;
extern pthread_key_t VThread_key;
extern VThreadOptions_t VThread_defaults;

#endif /* AFS_DEMAND_ATTACH_FS */

typedef struct VolumePackageOptions {
    afs_uint32 nLargeVnodes;      /**< size of large vnode cache */
    afs_uint32 nSmallVnodes;      /**< size of small vnode cache */
    afs_uint32 volcache;          /**< size of volume header cache */

    afs_int32 canScheduleSalvage; /**< can we schedule salvages? (DAFS) */
				  /* (if 'no', we will just error out if we
                                   * find a bad vol) */
    afs_int32 canUseFSSYNC;       /**< can we use the FSSYNC channel? */
    afs_int32 canUseSALVSYNC;     /**< can we use the SALVSYNC channel? (DAFS) */
    afs_int32 unsafe_attach;      /**< can we bypass checking the inUse vol
                                   *   header on attach? */
    void (*interrupt_rxcall) (struct rx_call *call, afs_int32 error);
                                  /**< callback to interrupt RX calls accessing
                                   *   a going-offline volume */
    afs_int32 offline_timeout;    /**< how long (in seconds) to wait before
                                   *   interrupting RX calls accessing a
                                   *   going-offline volume. -1 disables,
                                   *   0 means immediately. */
    afs_int32 offline_shutdown_timeout;
                                  /**< how long (in seconds) to wait before
                                   *   interrupting RX calls accessing a
                                   *   going-offline volume during shutdown.
                                   *   -1 disables, 0 means immediately.
                                   *   Note that the timeout time is calculated
                                   *   once, when we encounter the first going-
                                   *   offline volume during shutdown. So if we
                                   *   encounter multiple going-offline volumes
                                   *   during shutdown, we will still only wait
                                   *   for this amount of time in total, not e.g.
                                   *   for each going-offline volume encountered. */
    afs_int32 usage_threshold;    /*< number of accesses before writing volume header */
    afs_int32 usage_rate_limit;   /*< minimum number of seconds before writing volume
                                   *  header, after usage_threshold is exceeded */
} VolumePackageOptions;

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

/**
 * global volume package stats.
 */
typedef struct VolPkgStats {
#ifdef AFS_DEMAND_ATTACH_FS
    /*
     * demand attach fs
     * extended volume package statistics
     */

    /* levels */
    afs_uint32 state_levels[VOL_STATE_COUNT]; /**< volume state transition counters */

    /* counters */
    afs_uint64 hash_looks;           /**< number of hash chain element traversals */
    afs_uint64 hash_reorders;        /**< number of hash chain reorders */
    afs_uint64 salvages;             /**< online salvages since fileserver start */
    afs_uint64 vol_ops;              /**< volume operations since fileserver start */
#endif /* AFS_DEMAND_ATTACH_FS */

    afs_uint64 hdr_loads;            /**< header loads from disk */
    afs_uint64 hdr_gets;             /**< header pulls out of LRU */
    afs_uint64 attaches;             /**< volume attaches since fileserver start */
    afs_uint64 soft_detaches;        /**< soft detach ops since fileserver start */

    /* configuration parameters */
    afs_uint32 hdr_cache_size;       /**< size of volume header cache */
} VolPkgStats;
extern VolPkgStats VStats;

/*
 * volume header cache supporting structures
 */
struct volume_hdr_LRU_stats {
    afs_uint32 free;
    afs_uint32 used;
    afs_uint32 attached;
};

struct volume_hdr_LRU_t {
    struct rx_queue lru;
    struct volume_hdr_LRU_stats stats;
};
extern struct volume_hdr_LRU_t volume_hdr_LRU;

/*
 * volume hash chain supporting structures
 */
typedef struct VolumeHashChainHead {
    struct rx_queue queue;
    int len;
    /* someday we could put a per-chain lock here... */
#ifdef AFS_DEMAND_ATTACH_FS
    int busy;
    int cacheCheck;

    /* per-chain statistics */
    afs_uint64 looks;
    afs_uint64 gets;
    afs_uint64 reorders;

    pthread_cond_t chain_busy_cv;
#endif /* AFS_DEMAND_ATTACH_FS */
} VolumeHashChainHead;

typedef struct VolumeHashTable {
    int Size;
    int Mask;
    VolumeHashChainHead * Table;
} VolumeHashTable_t;
extern VolumeHashTable_t VolumeHashTable;

struct VolumeHashChainStats {
    afs_int32 table_size;
    afs_int32 chain_len;
#ifdef AFS_DEMAND_ATTACH_FS
    afs_int32 chain_cacheCheck;
    afs_int32 chain_busy;
    afs_uint64 chain_looks;
    afs_uint64 chain_gets;
    afs_uint64 chain_reorders;
#endif
};


#ifdef AFS_DEMAND_ATTACH_FS
/**
 * DAFS extended per-volume statistics.
 *
 * @note this data lives across the entire
 *       lifetime of the fileserver process
 */
typedef struct VolumeStats {
    /* counters */
    afs_uint64 hash_lookups;         /**< hash table lookups */
    afs_uint64 hash_short_circuits;  /**< short circuited hash lookups (due to cacheCheck) */
    afs_uint64 hdr_loads;            /**< header loads from disk */
    afs_uint64 hdr_gets;             /**< header pulls out of LRU */
    afs_uint16 attaches;             /**< attaches of this volume since fileserver start */
    afs_uint16 soft_detaches;        /**< soft detaches of this volume */
    afs_uint16 salvages;             /**< online salvages since fileserver start */
    afs_uint16 vol_ops;              /**< volume operations since fileserver start */

    /* timestamps */
    afs_uint32 last_attach;      /**< unix timestamp of last VAttach */
    afs_uint32 last_get;         /**< unix timestamp of last VGet/VHold */
    afs_uint32 last_promote;     /**< unix timestamp of last VLRU promote/demote */
    afs_uint32 last_hdr_get;     /**< unix timestamp of last GetVolumeHeader() */
    afs_uint32 last_hdr_load;    /**< unix timestamp of last LoadVolumeHeader() */
    afs_uint32 last_salvage;     /**< unix timestamp of last initiation of an online salvage */
    afs_uint32 last_salvage_req; /**< unix timestamp of last SALVSYNC request */
    afs_uint32 last_vol_op;      /**< unix timestamp of last volume operation */
} VolumeStats;


#define SALVAGE_PRIO_UPDATE_INTERVAL 3      /**< number of seconds between prio updates */
#define SALVAGE_COUNT_MAX 16                /**< number of online salvages we
					     *   allow before moving the volume
					     *   into a permanent error state
					     *
					     *   once this threshold is reached,
					     *   the operator will have to manually
					     *   issue a 'bos salvage' to bring
					     *   the volume back online
					     */

/**
 * DAFS online salvager state.
 */
typedef struct VolumeOnlineSalvage {
    afs_uint32 prio;            /**< number of VGetVolume's since salvage requested */
    int reason;                 /**< reason for requesting online salvage */
    byte requested;             /**< flag specifying that salvage should be scheduled */
    byte scheduled;             /**< flag specifying whether online salvage scheduled */
    byte scheduling;            /**< if nonzero, this volume has entered
                                 *   VCheckSalvage(), so if we recurse into
                                 *   VCheckSalvage() with this set, exit immediately
                                 *   to avoid recursing forever */
    byte reserved[1];           /**< padding */
} VolumeOnlineSalvage;

/**
 * DAFS Volume LRU state.
 */
typedef struct VolumeVLRUState {
    struct rx_queue lru;        /**< VLRU queue for this generation */
    VLRUQueueName idx;          /**< VLRU generation index */
} VolumeVLRUState;
#endif /* AFS_DEMAND_ATTACH_FS */

/**
 * node for a volume's rx_call_list.
 */
struct VCallByVol {
    struct rx_queue q;
    struct rx_call *call;
};

typedef struct Volume {
    struct rx_queue q;          /* Volume hash chain pointers */
    VolumeId hashid;		/* Volume number -- for hash table lookup */
    struct volHeader *header;	/* Cached disk data */
    Device device;		/* Unix device for the volume */
    struct DiskPartition64
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
#define VOL_PUTBACK 1
#define VOL_PUTBACK_DELETE 2
    byte needsPutBack;		/* For a volume utility, this flag is set to VOL_PUTBACK if we
				 * need to give the volume back when we detach it.  The server has
				 * certain modes where it doesn't detach the volume, and
				 * if we give it back spuriously, the server aborts. If set to
				 * VOL_PUTBACK_DELETE, it indicates that we need to tell the
				 * fileserver that the volume is gone entirely, instead of just
				 * giving the volume back to the fileserver. This field
				 * is meaningless on the file server */
    byte specialStatus;		/* An error code to return on VGetVolume: the
				 * volume is unavailable for the reason quoted,
				 * currently VBUSY or VMOVED */
    afs_uint32 checkoutMode;    /* for volume utilities, mode number for current checkout */
    afs_uint32 updateTime;	/* Time that this volume was put on the updated
				 * volume list--the list of volumes that will be
				 * salvaged should the file server crash */
    struct rx_queue vnode_list; /**< linked list of cached vnodes for this volume */
    struct rx_queue rx_call_list; /**< linked list of split RX calls using this
                                   *   volume (fileserver only) */
#ifdef AFS_DEMAND_ATTACH_FS
    VolState attach_state;      /* what stage of attachment has been completed */
    afs_uint32 attach_flags;    /* flags related to attachment state */
    pthread_cond_t attach_cv;   /* state change condition variable */
    short nWaiters;             /* volume package internal ref count */
    int chainCacheCheck;        /* Volume hash chain cache check */
    struct rx_queue vol_list;   /* per-partition volume list (VByPList) */

    VolumeOnlineSalvage salvage;  /* online salvager state */
    VolumeStats stats;            /* per-volume statistics */
    VolumeVLRUState vlru;         /* state specific to the VLRU */
    FSSYNC_VolOp_info * pending_vol_op;  /* fssync command info for any pending vol ops */
#endif /* AFS_DEMAND_ATTACH_FS */
    int usage_bumps_outstanding; /**< to rate limit the usage update i/o by accesses */
    int usage_bumps_next_write;  /**< to rate limit the usage update i/o by time */
} Volume;

struct volHeader {
    struct rx_queue lru;
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
#define V_checkoutMode(vp)      ((vp)->checkoutMode)
#ifdef AFS_DEMAND_ATTACH_FS
#define V_attachState(vp)       ((vp)->attach_state)
#define V_attachFlags(vp)       ((vp)->attach_flags)
#define V_attachCV(vp)          ((vp)->attach_cv)
#endif /* AFS_DEMAND_ATTACH_FS */

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
extern Volume *VGetVolume(Error * ec, Error * client_ec, VolId volumeId);
extern Volume *VGetVolumeWithCall(Error * ec, Error * client_ec, VolId volumeId,
                                  const struct timespec *ts, struct VCallByVol *cbv);
extern Volume *VGetVolume_r(Error * ec, VolId volumeId);
extern void VPutVolume(Volume *);
extern void VPutVolumeWithCall(Volume *vp, struct VCallByVol *cbv);
extern void VPutVolume_r(Volume *);
extern void VOffline(Volume * vp, char *message);
extern void VOffline_r(Volume * vp, char *message);
extern int VConnectFS(void);
extern int VConnectFS_r(void);
extern void VDisconnectFS(void);
extern void VDisconnectFS_r(void);
extern int VChildProcReconnectFS(void);
extern Volume *VAttachVolume(Error * ec, VolumeId volumeId, int mode);
extern Volume *VAttachVolume_r(Error * ec, VolumeId volumeId, int mode);
extern Volume *VCreateVolume(Error * ec, char *partname, VolId volumeId,
			     VolId parentId);
extern Volume *VCreateVolume_r(Error * ec, char *partname, VolId volumeId,
			       VolId parentId);
extern int VAllocBitmapEntry(Error * ec, Volume * vp,
			     struct vnodeIndex *index);
extern int VAllocBitmapEntry_r(Error * ec, Volume * vp,
			       struct vnodeIndex *index, int flags);
extern void VFreeBitMapEntry(Error * ec, Volume *vp, struct vnodeIndex *index,
			     unsigned bitNumber);
extern void VFreeBitMapEntry_r(Error * ec, Volume *vp, struct vnodeIndex *index,
			       unsigned bitNumber, int flags);
extern int VolumeNumber(char *name);
extern char *VolumeExternalName(VolumeId volumeId);
extern int VolumeExternalName_r(VolumeId volumeId, char *name, size_t len);
extern Volume *VAttachVolumeByName(Error * ec, char *partition, char *name,
				   int mode);
extern Volume *VAttachVolumeByName_r(Error * ec, char *partition, char *name,
				     int mode);
extern void VShutdown(void);
extern void VSetTranquil(void);
extern void VUpdateVolume(Error * ec, Volume * vp);
extern void VUpdateVolume_r(Error * ec, Volume * vp, int flags);
extern void VAddToVolumeUpdateList(Error * ec, Volume * vp);
extern void VAddToVolumeUpdateList_r(Error * ec, Volume * vp);
extern void VDetachVolume(Error * ec, Volume * vp);
extern void VDetachVolume_r(Error * ec, Volume * vp);
extern void VForceOffline(Volume * vp);
extern void VForceOffline_r(Volume * vp, int flags);
extern void VBumpVolumeUsage(Volume * vp);
extern void VBumpVolumeUsage_r(Volume * vp);
extern void VSetDiskUsage(void);
extern void VPrintCacheStats(void);
extern void VReleaseVnodeFiles_r(Volume * vp);
extern void VCloseVnodeFiles_r(Volume * vp);
extern struct DiskPartition64 *VGetPartition(char *name, int abortp);
extern struct DiskPartition64 *VGetPartition_r(char *name, int abortp);
extern void VOptDefaults(ProgramType pt, VolumePackageOptions * opts);
extern int VInitVolumePackage2(ProgramType pt, VolumePackageOptions * opts);
extern int VInitAttachVolumes(ProgramType pt);
extern void DiskToVolumeHeader(VolumeHeader_t * h, VolumeDiskHeader_t * dh);
extern void VolumeHeaderToDisk(VolumeDiskHeader_t * dh, VolumeHeader_t * h);
extern void AssignVolumeName(VolumeDiskData * vol, char *name, char *ext);
extern void VTakeOffline_r(Volume * vp);
extern void VTakeOffline(Volume * vp);
extern Volume * VLookupVolume_r(Error * ec, VolId volumeId, Volume * hint);
extern void VGetVolumePath(Error * ec, VolId volumeId, char **partitionp,
			   char **namep);
extern char *vol_DevName(dev_t adev, char *wpath);
extern afs_int32 VIsGoingOffline(struct Volume *vp);

struct VLockFile;
extern void VLockFileInit(struct VLockFile *lf, const char *path);
extern void VLockFileReinit(struct VLockFile *lf);
extern int VLockFileLock(struct VLockFile *lf, afs_uint32 offset,
                         int locktype, int nonblock);
extern void VLockFileUnlock(struct VLockFile *lf, afs_uint32 offset);

#ifdef AFS_DEMAND_ATTACH_FS
extern Volume *VPreAttachVolumeByName(Error * ec, char *partition, char *name);
extern Volume *VPreAttachVolumeByName_r(Error * ec, char *partition, char *name);
extern Volume *VPreAttachVolumeById_r(Error * ec, char * partition,
				      VolId volumeId);
extern Volume *VPreAttachVolumeByVp_r(Error * ec, struct DiskPartition64 * partp,
				      Volume * vp, VolId volume_id);
extern Volume *VGetVolumeByVp_r(Error * ec, Volume * vp);
extern int VShutdownByPartition_r(struct DiskPartition64 * dp);
extern int VShutdownVolume_r(Volume * vp);
extern int VConnectSALV(void);
extern int VConnectSALV_r(void);
extern int VReconnectSALV(void);
extern int VReconnectSALV_r(void);
extern int VDisconnectSALV(void);
extern int VDisconnectSALV_r(void);
extern void VPrintExtendedCacheStats(int flags);
extern void VPrintExtendedCacheStats_r(int flags);
extern void VLRU_SetOptions(int option, afs_uint32 val);
extern int VSetVolHashSize(int logsize);
extern int VRequestSalvage_r(Error * ec, Volume * vp, int reason, int flags);
extern int VUpdateSalvagePriority_r(Volume * vp);
extern int VRegisterVolOp_r(Volume * vp, FSSYNC_VolOp_info * vopinfo);
extern int VDeregisterVolOp_r(Volume * vp);
extern void VCancelReservation_r(Volume * vp);
extern int VChildProcReconnectFS_r(void);
extern void VOfflineForVolOp_r(Error *ec, Volume *vp, char *message);
#endif /* AFS_DEMAND_ATTACH_FS */

#ifdef AFS_DEMAND_ATTACH_FS
struct VDiskLock;
extern void VDiskLockInit(struct VDiskLock *dl, struct VLockFile *lf,
                          afs_uint32 offset);
extern int VGetDiskLock(struct VDiskLock *dl, int locktype, int nonblock);
extern void VReleaseDiskLock(struct VDiskLock *dl, int locktype);
#endif /* AFS_DEMAND_ATTACH_FS */
extern int VVolOpLeaveOnline_r(Volume * vp, FSSYNC_VolOp_info * vopinfo);
extern int VVolOpLeaveOnlineNoHeader_r(Volume * vp, FSSYNC_VolOp_info * vopinfo);
extern int VVolOpSetVBusy_r(Volume * vp, FSSYNC_VolOp_info * vopinfo);

extern void VPurgeVolume(Error * ec, Volume * vp);

extern afs_int32 VCanScheduleSalvage(void);
extern afs_int32 VCanUseFSSYNC(void);
extern afs_int32 VCanUseSALVSYNC(void);
extern afs_int32 VCanUnsafeAttach(void);
extern afs_int32 VReadVolumeDiskHeader(VolumeId volid,
				       struct DiskPartition64 * dp,
				       VolumeDiskHeader_t * hdr);
extern afs_int32 VWriteVolumeDiskHeader(VolumeDiskHeader_t * hdr,
					struct DiskPartition64 * dp);
extern afs_int32 VCreateVolumeDiskHeader(VolumeDiskHeader_t * hdr,
					 struct DiskPartition64 * dp);
extern afs_int32 VDestroyVolumeDiskHeader(struct DiskPartition64 * dp,
					  VolumeId volid, VolumeId parent);

/**
 * VWalkVolumeHeaders header callback.
 *
 * @param[in] dp   disk partition
 * @param[in] name full path to the .vol header file
 * @param[in] hdr  the header data that was read from the .vol header
 * @param[in] last 1 if this is the last attempt to read the vol header, 0
 *                 otherwise. DAFS VWalkVolumeHeaders will retry reading the
 *                 header once, if a non-fatal error occurs when reading the
 *                 header, or if this function returns a positive error code.
 *                 So, if there is a problem, this function will be called
 *                 first with last=0, then with last=1, then the error function
 *                 callback will be called. For non-DAFS, this is always 1.
 * @param[in] rock the rock passed to VWalkVolumeHeaders
 *
 * @return operation status
 *  @retval 0 success
 *  @retval negative a fatal error that should stop the walk immediately
 *  @retval positive an error with the volume header was encountered; the walk
 *          should continue, but the error function should be called on this
 *          header
 *
 * @see VWalkVolumeHeaders
 */
typedef int (*VWalkVolFunc)(struct DiskPartition64 *dp, const char *name,
                            struct VolumeDiskHeader *hdr, int last,
                            void *rock);
/**
 * VWalkVolumeHeaders error callback.
 *
 * This is called from VWalkVolumeHeaders when an invalid or otherwise
 * problematic volume header is encountered. It is typically implemented as a
 * wrapper to unlink the .vol file.
 *
 * @param[in] dp   disk partition
 * @param[in] name full path to the .vol header file
 * @param[in] hdr  header read in from the .vol file, or NULL if it could not
 *                 be read
 * @param[in] rock rock passed to VWalkVolumeHeaders
 *
 * @see VWalkVolumeHeaders
 */
typedef void (*VWalkErrFunc)(struct DiskPartition64 *dp, const char *name,
                             struct VolumeDiskHeader *hdr, void *rock);
extern int VWalkVolumeHeaders(struct DiskPartition64 *dp, const char *partpath,
                              VWalkVolFunc volfunc, VWalkErrFunc errfunc,
                              void *rock);

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



/* VUpdateVolume_r flags */
#define VOL_UPDATE_WAIT          0x1  /* for demand attach, wait for other exclusive ops to end */
#define VOL_UPDATE_NOFORCEOFF    0x2  /* don't force offline on failure. this is to prevent
				       * infinite recursion between vupdate and vforceoff */

/* VForceOffline_r flags */
#define VOL_FORCEOFF_NOUPDATE    0x1  /* don't force update on forceoff. this is to prevent
				       * infinite recursion between vupdate and vforceoff */

/* VSyncVolume_r flags */
#define VOL_SYNC_WAIT            0x1  /* for demand attach, wait for other exclusive ops to end */

/* VAllocBitmapEntry_r flags */
#define VOL_ALLOC_BITMAP_WAIT    0x1  /* for demand attach, wait for other exclusive ops to end */

/* VFreeBitMapEntry_r flags */
#define VOL_FREE_BITMAP_WAIT     0x1  /* for demand attach, wait for other exclusive ops to end */

/* VRequestSalvage_r flags */
#define VOL_SALVAGE_NO_OFFLINE        0x1 /* we do not need to wait to offline the volume; it has
                                           * not been fully attached */


#if	defined(NEARINODE_HINT)
#define V_pref(vp,nearInode)  nearInodeHash(V_id(vp),(nearInode)); (nearInode) %= V_partition(vp)->f_files
#else
#define V_pref(vp,nearInode)   nearInode = 0
#endif /* NEARINODE_HINT */

hdr_static_inline(unsigned int)
afs_printable_VolumeId_u(VolumeId d) { return (unsigned int) d; }

hdr_static_inline(unsigned int)
afs_printable_VnodeId_u(VnodeId d) { return (unsigned int) d; }

#endif /* __volume_h */
