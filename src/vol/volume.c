/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2005-2008 Sine Nomine Associates
 */

/* 1/1/89: NB:  this stuff is all going to be replaced.  Don't take it too seriously */
/*

	System:		VICE-TWO
	Module:		volume.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <rx/xdr.h>
#include <afs/afsint.h>
#include <ctype.h>
#ifndef AFS_NT40_ENV
#include <sys/param.h>
#if !defined(AFS_SGI_ENV)
#ifdef	AFS_OSF_ENV
#include <ufs/fs.h>
#else /* AFS_OSF_ENV */
#ifdef AFS_VFSINCL_ENV
#define VFS
#ifdef	AFS_SUN5_ENV
#include <sys/fs/ufs_fs.h>
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#else
#include <ufs/fs.h>
#endif
#endif
#else /* AFS_VFSINCL_ENV */
#if !defined(AFS_AIX_ENV) && !defined(AFS_LINUX20_ENV) && !defined(AFS_XBSD_ENV)
#include <sys/fs.h>
#endif
#endif /* AFS_VFSINCL_ENV */
#endif /* AFS_OSF_ENV */
#endif /* AFS_SGI_ENV */
#endif /* AFS_NT40_ENV */
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <fcntl.h>
#else
#include <sys/file.h>
#endif
#include <dirent.h>
#ifdef	AFS_AIX_ENV
#include <sys/vfs.h>
#include <fcntl.h>
#else
#ifdef	AFS_HPUX_ENV
#include <fcntl.h>
#include <mntent.h>
#else
#if	defined(AFS_SUN_ENV) || defined(AFS_SUN5_ENV)
#ifdef	AFS_SUN5_ENV
#include <sys/mnttab.h>
#include <sys/mntent.h>
#else
#include <mntent.h>
#endif
#else
#ifndef AFS_NT40_ENV
#if defined(AFS_SGI_ENV)
#include <fcntl.h>
#include <mntent.h>

#else
#ifndef AFS_LINUX20_ENV
#include <fstab.h>		/* Need to find in libc 5, present in libc 6 */
#endif
#endif
#endif /* AFS_SGI_ENV */
#endif
#endif /* AFS_HPUX_ENV */
#endif
#ifndef AFS_NT40_ENV
#include <netdb.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <setjmp.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#endif /* AFS_NT40_ENV */
#if defined(AFS_SUN5_ENV) || defined(AFS_NT40_ENV) || defined(AFS_LINUX20_ENV)
#include <string.h>
#else
#include <strings.h>
#endif

#include "nfs.h"
#include <afs/errors.h>
#include "lock.h"
#include "lwp.h"
#include <afs/afssyscalls.h>
#include "ihandle.h"
#include <afs/afsutil.h>
#ifdef AFS_NT40_ENV
#include <io.h>
#endif
#include "daemon_com.h"
#include "fssync.h"
#include "salvsync.h"
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#include "volume_inline.h"
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include "afs/assert.h"
#endif /* AFS_PTHREAD_ENV */
#include "vutils.h"
#ifndef AFS_NT40_ENV
#include <dir/dir.h>
#include <unistd.h>
#endif

#if !defined(offsetof)
#include <stddef.h>
#endif

#ifdef O_LARGEFILE
#define afs_stat	stat64
#define afs_fstat	fstat64
#define afs_open	open64
#else /* !O_LARGEFILE */
#define afs_stat	stat
#define afs_fstat	fstat
#define afs_open	open
#endif /* !O_LARGEFILE */

#ifdef AFS_PTHREAD_ENV
pthread_mutex_t vol_glock_mutex;
pthread_mutex_t vol_trans_mutex;
pthread_cond_t vol_put_volume_cond;
pthread_cond_t vol_sleep_cond;
int vol_attach_threads = 1;
#endif /* AFS_PTHREAD_ENV */

#ifdef AFS_DEMAND_ATTACH_FS
pthread_mutex_t vol_salvsync_mutex;
#endif /* AFS_DEMAND_ATTACH_FS */

#ifdef	AFS_OSF_ENV
extern void *calloc(), *realloc();
#endif

/*@printflike@*/ extern void Log(const char *format, ...);

/* Forward declarations */
static Volume *attach2(Error * ec, VolId vid, char *path,
		       register struct VolumeHeader *header,
		       struct DiskPartition *partp, Volume * vp, 
		       int isbusy, int mode);
static void ReallyFreeVolume(Volume * vp);
#ifdef AFS_DEMAND_ATTACH_FS
static void FreeVolume(Volume * vp);
#else /* !AFS_DEMAND_ATTACH_FS */
#define FreeVolume(vp) ReallyFreeVolume(vp)
static void VScanUpdateList(void);
#endif /* !AFS_DEMAND_ATTACH_FS */
static void VInitVolumeHeaderCache(afs_uint32 howMany);
static int GetVolumeHeader(register Volume * vp);
static void ReleaseVolumeHeader(register struct volHeader *hd);
static void FreeVolumeHeader(register Volume * vp);
static void AddVolumeToHashTable(register Volume * vp, int hashid);
static void DeleteVolumeFromHashTable(register Volume * vp);
static int VHold(Volume * vp);
static int VHold_r(Volume * vp);
static void VGetBitmap_r(Error * ec, Volume * vp, VnodeClass class);
static void GetVolumePath(Error * ec, VolId volumeId, char **partitionp,
			  char **namep);
static void VReleaseVolumeHandles_r(Volume * vp);
static void VCloseVolumeHandles_r(Volume * vp);
static void LoadVolumeHeader(Error * ec, Volume * vp);
static int VCheckOffline(register Volume * vp);
static int VCheckDetach(register Volume * vp);
static Volume * GetVolume(Error * ec, Error * client_ec, VolId volumeId, Volume * hint, int flags);
static int VolumeExternalName_r(VolumeId volumeId, char * name, size_t len);

int LogLevel;			/* Vice loglevel--not defined as extern so that it will be
				 * defined when not linked with vice, XXXX */
ProgramType programType;	/* The type of program using the package */

/* extended volume package statistics */
VolPkgStats VStats;

#ifdef VOL_LOCK_DEBUG
pthread_t vol_glock_holder = 0;
#endif


#define VOLUME_BITMAP_GROWSIZE	16	/* bytes, => 128vnodes */
					/* Must be a multiple of 4 (1 word) !! */

/* this parameter needs to be tunable at runtime.
 * 128 was really inadequate for largish servers -- at 16384 volumes this
 * puts average chain length at 128, thus an average 65 deref's to find a volptr.
 * talk about bad spatial locality...
 *
 * an AVL or splay tree might work a lot better, but we'll just increase
 * the default hash table size for now
 */
#define DEFAULT_VOLUME_HASH_SIZE 256   /* Must be a power of 2!! */
#define DEFAULT_VOLUME_HASH_MASK (DEFAULT_VOLUME_HASH_SIZE-1)
#define VOLUME_HASH(volumeId) (volumeId&(VolumeHashTable.Mask))

/*
 * turn volume hash chains into partially ordered lists.
 * when the threshold is exceeded between two adjacent elements,
 * perform a chain rebalancing operation.
 *
 * keep the threshold high in order to keep cache line invalidates
 * low "enough" on SMPs
 */
#define VOLUME_HASH_REORDER_THRESHOLD 200

/*
 * when possible, don't just reorder single elements, but reorder
 * entire chains of elements at once.  a chain of elements that
 * exceed the element previous to the pivot by at least CHAIN_THRESH 
 * accesses are moved in front of the chain whose elements have at
 * least CHAIN_THRESH less accesses than the pivot element
 */
#define VOLUME_HASH_REORDER_CHAIN_THRESH (VOLUME_HASH_REORDER_THRESHOLD / 2)

#include "rx/rx_queue.h"


VolumeHashTable_t VolumeHashTable = {
    DEFAULT_VOLUME_HASH_SIZE,
    DEFAULT_VOLUME_HASH_MASK,
    NULL
};


static void VInitVolumeHash(void);


#ifndef AFS_HAVE_FFS
/* This macro is used where an ffs() call does not exist. Was in util/ffs.c */
ffs(x)
{
    afs_int32 ffs_i;
    afs_int32 ffs_tmp = x;
    if (ffs_tmp == 0)
	return (-1);
    else
	for (ffs_i = 1;; ffs_i++) {
	    if (ffs_tmp & 1)
		return (ffs_i);
	    else
		ffs_tmp >>= 1;
	}
}
#endif /* !AFS_HAVE_FFS */

#ifdef AFS_PTHREAD_ENV
typedef struct diskpartition_queue_t {
    struct rx_queue queue;
    struct DiskPartition * diskP;
} diskpartition_queue_t;
typedef struct vinitvolumepackage_thread_t {
    struct rx_queue queue;
    pthread_cond_t thread_done_cv;
    int n_threads_complete;
} vinitvolumepackage_thread_t;
static void * VInitVolumePackageThread(void * args);
#endif /* AFS_PTHREAD_ENV */

static int VAttachVolumesByPartition(struct DiskPartition *diskP, 
				     int * nAttached, int * nUnattached);


#ifdef AFS_DEMAND_ATTACH_FS
/* demand attach fileserver extensions */

/* XXX
 * in the future we will support serialization of VLRU state into the fs_state
 * disk dumps
 *
 * these structures are the beginning of that effort
 */
struct VLRU_DiskHeader {
    struct versionStamp stamp;            /* magic and structure version number */
    afs_uint32 mtime;                     /* time of dump to disk */
    afs_uint32 num_records;               /* number of VLRU_DiskEntry records */
};

struct VLRU_DiskEntry {
    afs_uint32 vid;                       /* volume ID */
    afs_uint32 idx;                       /* generation */
    afs_uint32 last_get;                  /* timestamp of last get */
};

struct VLRU_StartupQueue {
    struct VLRU_DiskEntry * entry;
    int num_entries;
    int next_idx;
};

typedef struct vshutdown_thread_t {
    struct rx_queue q;
    pthread_mutex_t lock;
    pthread_cond_t cv;
    pthread_cond_t master_cv;
    int n_threads;
    int n_threads_complete;
    int vol_remaining;
    int schedule_version;
    int pass;
    byte n_parts;
    byte n_parts_done_pass;
    byte part_thread_target[VOLMAXPARTS+1];
    byte part_done_pass[VOLMAXPARTS+1];
    struct rx_queue * part_pass_head[VOLMAXPARTS+1];
    int stats[4][VOLMAXPARTS+1];
} vshutdown_thread_t;
static void * VShutdownThread(void * args);


static Volume * VAttachVolumeByVp_r(Error * ec, Volume * vp, int mode);
static int VCheckFree(Volume * vp);

/* VByP List */
static void AddVolumeToVByPList_r(Volume * vp);
static void DeleteVolumeFromVByPList_r(Volume * vp);
static void VVByPListBeginExclusive_r(struct DiskPartition * dp);
static void VVByPListEndExclusive_r(struct DiskPartition * dp);
static void VVByPListWait_r(struct DiskPartition * dp);

/* online salvager */
static int VCheckSalvage(register Volume * vp);
static int VUpdateSalvagePriority_r(Volume * vp);
static int VScheduleSalvage_r(Volume * vp);
static int VCancelSalvage_r(Volume * vp, int reason);

/* Volume hash table */
static void VReorderHash_r(VolumeHashChainHead * head, Volume * pp, Volume * vp);
static void VHashBeginExclusive_r(VolumeHashChainHead * head);
static void VHashEndExclusive_r(VolumeHashChainHead * head);
static void VHashWait_r(VolumeHashChainHead * head);

/* shutdown */
static int ShutdownVByPForPass_r(struct DiskPartition * dp, int pass);
static int ShutdownVolumeWalk_r(struct DiskPartition * dp, int pass,
				struct rx_queue ** idx);
static void ShutdownController(vshutdown_thread_t * params);
static void ShutdownCreateSchedule(vshutdown_thread_t * params);

/* VLRU */
static void VLRU_ComputeConstants(void);
static void VInitVLRU(void);
static void VLRU_Init_Node_r(volatile Volume * vp);
static void VLRU_Add_r(volatile Volume * vp);
static void VLRU_Delete_r(volatile Volume * vp);
static void VLRU_UpdateAccess_r(volatile Volume * vp);
static void * VLRU_ScannerThread(void * args);
static void VLRU_Scan_r(int idx);
static void VLRU_Promote_r(int idx);
static void VLRU_Demote_r(int idx);
static void VLRU_SwitchQueues(volatile Volume * vp, int new_idx, int append);

/* soft detach */
static int VCheckSoftDetach(volatile Volume * vp, afs_uint32 thresh);
static int VCheckSoftDetachCandidate(volatile Volume * vp, afs_uint32 thresh);
static int VSoftDetachVolume_r(volatile Volume * vp, afs_uint32 thresh);
#endif /* AFS_DEMAND_ATTACH_FS */


struct Lock vol_listLock;	/* Lock obtained when listing volumes:  
				 * prevents a volume from being missed 
				 * if the volume is attached during a 
				 * list volumes */


static int TimeZoneCorrection;	/* Number of seconds west of GMT */

/* Common message used when the volume goes off line */
char *VSalvageMessage =
    "Files in this volume are currently unavailable; call operations";

int VInit;			/* 0 - uninitialized,
				 * 1 - initialized but not all volumes have been attached,
				 * 2 - initialized and all volumes have been attached,
				 * 3 - initialized, all volumes have been attached, and
				 * VConnectFS() has completed. */


bit32 VolumeCacheCheck;		/* Incremented everytime a volume goes on line--
				 * used to stamp volume headers and in-core
				 * vnodes.  When the volume goes on-line the
				 * vnode will be invalidated
				 * access only with VOL_LOCK held */




/***************************************************/
/* Startup routines                                */
/***************************************************/

int
VInitVolumePackage(ProgramType pt, afs_uint32 nLargeVnodes, afs_uint32 nSmallVnodes,
		   int connect, afs_uint32 volcache)
{
    int errors = 0;		/* Number of errors while finding vice partitions. */
    struct timeval tv;
    struct timezone tz;

    programType = pt;

    memset(&VStats, 0, sizeof(VStats));
    VStats.hdr_cache_size = 200;

    VInitPartitionPackage();
    VInitVolumeHash();
#ifdef AFS_DEMAND_ATTACH_FS
    if (programType == fileServer) {
	VInitVLRU();
    } else {
	VLRU_SetOptions(VLRU_SET_ENABLED, 0);
    }
#endif

#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&vol_glock_mutex, NULL) == 0);
    assert(pthread_mutex_init(&vol_trans_mutex, NULL) == 0);
    assert(pthread_cond_init(&vol_put_volume_cond, NULL) == 0);
    assert(pthread_cond_init(&vol_sleep_cond, NULL) == 0);
#else /* AFS_PTHREAD_ENV */
    IOMGR_Initialize();
#endif /* AFS_PTHREAD_ENV */
    Lock_Init(&vol_listLock);

    srandom(time(0));		/* For VGetVolumeInfo */
    gettimeofday(&tv, &tz);
    TimeZoneCorrection = tz.tz_minuteswest * 60;

#ifdef AFS_DEMAND_ATTACH_FS
    assert(pthread_mutex_init(&vol_salvsync_mutex, NULL) == 0);
#endif /* AFS_DEMAND_ATTACH_FS */

    /* Ok, we have done enough initialization that fileserver can 
     * start accepting calls, even though the volumes may not be 
     * available just yet.
     */
    VInit = 1;

#if defined(AFS_DEMAND_ATTACH_FS) && defined(SALVSYNC_BUILD_SERVER)
    if (programType == salvageServer) {
	SALVSYNC_salvInit();
    }
#endif /* AFS_DEMAND_ATTACH_FS */
#ifdef FSSYNC_BUILD_SERVER
    if (programType == fileServer) {
	FSYNC_fsInit();
    }
#endif
#if defined(AFS_DEMAND_ATTACH_FS) && defined(SALVSYNC_BUILD_CLIENT)
    if (programType == fileServer) {
	/* establish a connection to the salvager at this point */
	assert(VConnectSALV() != 0);
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    if (volcache > VStats.hdr_cache_size)
	VStats.hdr_cache_size = volcache;
    VInitVolumeHeaderCache(VStats.hdr_cache_size);

    VInitVnodes(vLarge, nLargeVnodes);
    VInitVnodes(vSmall, nSmallVnodes);


    errors = VAttachPartitions();
    if (errors)
	return -1;

    if (programType == fileServer) {
	struct DiskPartition *diskP;
#ifdef AFS_PTHREAD_ENV
	struct vinitvolumepackage_thread_t params;
	struct diskpartition_queue_t * dpq;
	int i, threads, parts;
	pthread_t tid;
	pthread_attr_t attrs;

	assert(pthread_cond_init(&params.thread_done_cv,NULL) == 0);
	queue_Init(&params);
	params.n_threads_complete = 0;

	/* create partition work queue */
	for (parts=0, diskP = DiskPartitionList; diskP; diskP = diskP->next, parts++) {
	    dpq = (diskpartition_queue_t *) malloc(sizeof(struct diskpartition_queue_t));
	    assert(dpq != NULL);
	    dpq->diskP = diskP;
	    queue_Append(&params,dpq);
	}

	threads = MIN(parts, vol_attach_threads);

	if (threads > 1) {
	    /* spawn off a bunch of initialization threads */
	    assert(pthread_attr_init(&attrs) == 0);
	    assert(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);

	    Log("VInitVolumePackage: beginning parallel fileserver startup\n");
#ifdef AFS_DEMAND_ATTACH_FS
	    Log("VInitVolumePackage: using %d threads to pre-attach volumes on %d partitions\n",
		threads, parts);
#else /* AFS_DEMAND_ATTACH_FS */
	    Log("VInitVolumePackage: using %d threads to attach volumes on %d partitions\n",
		threads, parts);
#endif /* AFS_DEMAND_ATTACH_FS */

	    VOL_LOCK;
	    for (i=0; i < threads; i++) {
		assert(pthread_create
		       (&tid, &attrs, &VInitVolumePackageThread,
			&params) == 0);
	    }

	    while(params.n_threads_complete < threads) {
		VOL_CV_WAIT(&params.thread_done_cv);
	    }
	    VOL_UNLOCK;

	    assert(pthread_attr_destroy(&attrs) == 0);
	} else {
	    /* if we're only going to run one init thread, don't bother creating
	     * another LWP */
	    Log("VInitVolumePackage: beginning single-threaded fileserver startup\n");
#ifdef AFS_DEMAND_ATTACH_FS
	    Log("VInitVolumePackage: using 1 thread to pre-attach volumes on %d partition(s)\n",
		parts);
#else /* AFS_DEMAND_ATTACH_FS */
	    Log("VInitVolumePackage: using 1 thread to attach volumes on %d partition(s)\n",
		parts);
#endif /* AFS_DEMAND_ATTACH_FS */

	    VInitVolumePackageThread(&params);
	}

	assert(pthread_cond_destroy(&params.thread_done_cv) == 0);

#else /* AFS_PTHREAD_ENV */
	DIR *dirp;
	struct dirent *dp;

	/* Attach all the volumes in this partition */
	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    int nAttached = 0, nUnattached = 0;
	    assert(VAttachVolumesByPartition(diskP, &nAttached, &nUnattached) == 0);
	}
#endif /* AFS_PTHREAD_ENV */
    }

    VInit = 2;			/* Initialized, and all volumes have been attached */
#ifdef FSSYNC_BUILD_CLIENT
    if (programType == volumeUtility && connect) {
	if (!VConnectFS()) {
	    Log("Unable to connect to file server; aborted\n");
	    exit(1);
	}
    }
#ifdef AFS_DEMAND_ATTACH_FS
    else if (programType == salvageServer) {
	if (!VConnectFS()) {
	    Log("Unable to connect to file server; aborted\n");
	    exit(1);
	}
    }
#endif /* AFS_DEMAND_ATTACH_FS */
#endif /* FSSYNC_BUILD_CLIENT */
    return 0;
}

#ifdef AFS_PTHREAD_ENV
static void *
VInitVolumePackageThread(void * args) {
    int errors = 0;		/* Number of errors while finding vice partitions. */

    DIR *dirp;
    struct dirent *dp;
    struct DiskPartition *diskP;
    struct vinitvolumepackage_thread_t * params;
    struct diskpartition_queue_t * dpq;

    params = (vinitvolumepackage_thread_t *) args;


    VOL_LOCK;
    /* Attach all the volumes in this partition */
    while (queue_IsNotEmpty(params)) {
        int nAttached = 0, nUnattached = 0;

        dpq = queue_First(params,diskpartition_queue_t);
	queue_Remove(dpq);
	VOL_UNLOCK;
	diskP = dpq->diskP;
	free(dpq);

	assert(VAttachVolumesByPartition(diskP, &nAttached, &nUnattached) == 0);

	VOL_LOCK;
    }

    params->n_threads_complete++;
    pthread_cond_signal(&params->thread_done_cv);
    VOL_UNLOCK;
    return NULL;
}
#endif /* AFS_PTHREAD_ENV */

/*
 * attach all volumes on a given disk partition
 */
static int
VAttachVolumesByPartition(struct DiskPartition *diskP, int * nAttached, int * nUnattached)
{
  DIR * dirp;
  struct dirent * dp;
  int ret = 0;

  Log("Partition %s: attaching volumes\n", diskP->name);
  dirp = opendir(VPartitionPath(diskP));
  if (!dirp) {
    Log("opendir on Partition %s failed!\n", diskP->name);
    return 1;
  }

  while ((dp = readdir(dirp))) {
    char *p;
    p = strrchr(dp->d_name, '.');
    if (p != NULL && strcmp(p, VHDREXT) == 0) {
      Error error;
      Volume *vp;
#ifdef AFS_DEMAND_ATTACH_FS
      vp = VPreAttachVolumeByName(&error, diskP->name, dp->d_name);
#else /* AFS_DEMAND_ATTACH_FS */
      vp = VAttachVolumeByName(&error, diskP->name, dp->d_name,
			       V_VOLUPD);
#endif /* AFS_DEMAND_ATTACH_FS */
      (*(vp ? nAttached : nUnattached))++;
      if (error == VOFFLINE)
	Log("Volume %d stays offline (/vice/offline/%s exists)\n", VolumeNumber(dp->d_name), dp->d_name);
      else if (LogLevel >= 5) {
	Log("Partition %s: attached volume %d (%s)\n",
	    diskP->name, VolumeNumber(dp->d_name),
	    dp->d_name);
      }
#if !defined(AFS_DEMAND_ATTACH_FS)
      if (vp) {
	VPutVolume(vp);
      }
#endif /* AFS_DEMAND_ATTACH_FS */
    }
  }

  Log("Partition %s: attached %d volumes; %d volumes not attached\n", diskP->name, *nAttached, *nUnattached);
  closedir(dirp);
  return ret;
}


/***************************************************/
/* Shutdown routines                               */
/***************************************************/

/*
 * demand attach fs
 * highly multithreaded volume package shutdown
 *
 * with the demand attach fileserver extensions,
 * VShutdown has been modified to be multithreaded.
 * In order to achieve optimal use of many threads,
 * the shutdown code involves one control thread and
 * n shutdown worker threads.  The control thread
 * periodically examines the number of volumes available
 * for shutdown on each partition, and produces a worker
 * thread allocation schedule.  The idea is to eliminate
 * redundant scheduling computation on the workers by
 * having a single master scheduler.
 *
 * The scheduler's objectives are:
 * (1) fairness
 *   each partition with volumes remaining gets allocated
 *   at least 1 thread (assuming sufficient threads)
 * (2) performance
 *   threads are allocated proportional to the number of
 *   volumes remaining to be offlined.  This ensures that
 *   the OS I/O scheduler has many requests to elevator
 *   seek on partitions that will (presumably) take the
 *   longest amount of time (from now) to finish shutdown
 * (3) keep threads busy
 *   when there are extra threads, they are assigned to
 *   partitions using a simple round-robin algorithm
 *
 * In the future, we may wish to add the ability to adapt
 * to the relative performance patterns of each disk
 * partition.
 *
 *
 * demand attach fs
 * multi-step shutdown process
 *
 * demand attach shutdown is a four-step process. Each
 * shutdown "pass" shuts down increasingly more difficult
 * volumes.  The main purpose is to achieve better cache
 * utilization during shutdown.
 *
 * pass 0
 *   shutdown volumes in the unattached, pre-attached
 *   and error states
 * pass 1
 *   shutdown attached volumes with cached volume headers
 * pass 2
 *   shutdown all volumes in non-exclusive states
 * pass 3
 *   shutdown all remaining volumes
 */

void
VShutdown_r(void)
{
    int i;
    register Volume *vp, *np;
    register afs_int32 code;
#ifdef AFS_DEMAND_ATTACH_FS
    struct DiskPartition * diskP;
    struct diskpartition_queue_t * dpq;
    vshutdown_thread_t params;
    pthread_t tid;
    pthread_attr_t attrs;

    memset(&params, 0, sizeof(vshutdown_thread_t));

    for (params.n_parts=0, diskP = DiskPartitionList;
	 diskP; diskP = diskP->next, params.n_parts++);

    Log("VShutdown:  shutting down on-line volumes on %d partition%s...\n", 
	params.n_parts, params.n_parts > 1 ? "s" : "");

    if (vol_attach_threads > 1) {
	/* prepare for parallel shutdown */
	params.n_threads = vol_attach_threads;
	assert(pthread_mutex_init(&params.lock, NULL) == 0);
	assert(pthread_cond_init(&params.cv, NULL) == 0);
	assert(pthread_cond_init(&params.master_cv, NULL) == 0);
	assert(pthread_attr_init(&attrs) == 0);
	assert(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);
	queue_Init(&params);

	/* setup the basic partition information structures for
	 * parallel shutdown */
	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    /* XXX debug */
	    struct rx_queue * qp, * nqp;
	    Volume * vp;
	    int count = 0;

	    VVByPListWait_r(diskP);
	    VVByPListBeginExclusive_r(diskP);

	    /* XXX debug */
	    for (queue_Scan(&diskP->vol_list, qp, nqp, rx_queue)) {
		vp = (Volume *)((char *)qp - offsetof(Volume, vol_list));
		if (vp->header)
		    count++;
	    }
	    Log("VShutdown: partition %s has %d volumes with attached headers\n",
		VPartitionPath(diskP), count);
		

	    /* build up the pass 0 shutdown work queue */
	    dpq = (struct diskpartition_queue_t *) malloc(sizeof(struct diskpartition_queue_t));
	    assert(dpq != NULL);
	    dpq->diskP = diskP;
	    queue_Prepend(&params, dpq);

	    params.part_pass_head[diskP->device] = queue_First(&diskP->vol_list, rx_queue);
	}

	Log("VShutdown:  beginning parallel fileserver shutdown\n");
	Log("VShutdown:  using %d threads to offline volumes on %d partition%s\n",
	    vol_attach_threads, params.n_parts, params.n_parts > 1 ? "s" : "" );

	/* do pass 0 shutdown */
	assert(pthread_mutex_lock(&params.lock) == 0);
	for (i=0; i < params.n_threads; i++) {
	    assert(pthread_create
		   (&tid, &attrs, &VShutdownThread,
		    &params) == 0);
	}
	
	/* wait for all the pass 0 shutdowns to complete */
	while (params.n_threads_complete < params.n_threads) {
	    assert(pthread_cond_wait(&params.master_cv, &params.lock) == 0);
	}
	params.n_threads_complete = 0;
	params.pass = 1;
	assert(pthread_cond_broadcast(&params.cv) == 0);
	assert(pthread_mutex_unlock(&params.lock) == 0);

	Log("VShutdown:  pass 0 completed using the 1 thread per partition algorithm\n");
	Log("VShutdown:  starting passes 1 through 3 using finely-granular mp-fast algorithm\n");

	/* run the parallel shutdown scheduler. it will drop the glock internally */
	ShutdownController(&params);
	
	/* wait for all the workers to finish pass 3 and terminate */
	while (params.pass < 4) {
	    VOL_CV_WAIT(&params.cv);
	}
	
	assert(pthread_attr_destroy(&attrs) == 0);
	assert(pthread_cond_destroy(&params.cv) == 0);
	assert(pthread_cond_destroy(&params.master_cv) == 0);
	assert(pthread_mutex_destroy(&params.lock) == 0);

	/* drop the VByPList exclusive reservations */
	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    VVByPListEndExclusive_r(diskP);
	    Log("VShutdown:  %s stats : (pass[0]=%d, pass[1]=%d, pass[2]=%d, pass[3]=%d)\n",
		VPartitionPath(diskP),
		params.stats[0][diskP->device],
		params.stats[1][diskP->device],
		params.stats[2][diskP->device],
		params.stats[3][diskP->device]);
	}

	Log("VShutdown:  shutdown finished using %d threads\n", params.n_threads);
    } else {
	/* if we're only going to run one shutdown thread, don't bother creating
	 * another LWP */
	Log("VShutdown:  beginning single-threaded fileserver shutdown\n");

	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    VShutdownByPartition_r(diskP);
	}
    }

    Log("VShutdown:  complete.\n");
#else /* AFS_DEMAND_ATTACH_FS */
    Log("VShutdown:  shutting down on-line volumes...\n");
    for (i = 0; i < VolumeHashTable.Size; i++) {
	/* try to hold first volume in the hash table */
	for (queue_Scan(&VolumeHashTable.Table[i],vp,np,Volume)) {
	    code = VHold_r(vp);
	    if (code == 0) {
		if (LogLevel >= 5)
		    Log("VShutdown:  Attempting to take volume %u offline.\n",
			vp->hashid);
		
		/* next, take the volume offline (drops reference count) */
		VOffline_r(vp, "File server was shut down");
	    }
	}
    }
    Log("VShutdown:  complete.\n");
#endif /* AFS_DEMAND_ATTACH_FS */
}

void
VShutdown(void)
{
    VOL_LOCK;
    VShutdown_r();
    VOL_UNLOCK;
}

#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * shutdown control thread
 */
static void
ShutdownController(vshutdown_thread_t * params)
{
    /* XXX debug */
    struct DiskPartition * diskP;
    Device id;
    vshutdown_thread_t shadow;

    ShutdownCreateSchedule(params);

    while ((params->pass < 4) &&
	   (params->n_threads_complete < params->n_threads)) {
	/* recompute schedule once per second */

	memcpy(&shadow, params, sizeof(vshutdown_thread_t));

	VOL_UNLOCK;
	/* XXX debug */
	Log("ShutdownController:  schedule version=%d, vol_remaining=%d, pass=%d\n",
	    shadow.schedule_version, shadow.vol_remaining, shadow.pass);
	Log("ShutdownController:  n_threads_complete=%d, n_parts_done_pass=%d\n",
	    shadow.n_threads_complete, shadow.n_parts_done_pass);
	for (diskP = DiskPartitionList; diskP; diskP=diskP->next) {
	    id = diskP->device;
	    Log("ShutdownController:  part[%d] : (len=%d, thread_target=%d, done_pass=%d, pass_head=%p)\n",
		id, 
		diskP->vol_list.len,
		shadow.part_thread_target[id], 
		shadow.part_done_pass[id], 
		shadow.part_pass_head[id]);
	}

	sleep(1);
	VOL_LOCK;

	ShutdownCreateSchedule(params);
    }
}

/* create the shutdown thread work schedule.
 * this scheduler tries to implement fairness
 * by allocating at least 1 thread to each 
 * partition with volumes to be shutdown,
 * and then it attempts to allocate remaining
 * threads based upon the amount of work left
 */
static void
ShutdownCreateSchedule(vshutdown_thread_t * params)
{
    struct DiskPartition * diskP;
    int sum, thr_workload, thr_left;
    int part_residue[VOLMAXPARTS+1];
    Device id;

    /* compute the total number of outstanding volumes */
    sum = 0;
    for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	sum += diskP->vol_list.len;
    }
    
    params->schedule_version++;
    params->vol_remaining = sum;

    if (!sum)
	return;

    /* compute average per-thread workload */
    thr_workload = sum / params->n_threads;
    if (sum % params->n_threads)
	thr_workload++;

    thr_left = params->n_threads;
    memset(&part_residue, 0, sizeof(part_residue));

    /* for fairness, give every partition with volumes remaining
     * at least one thread */
    for (diskP = DiskPartitionList; diskP && thr_left; diskP = diskP->next) {
	id = diskP->device;
	if (diskP->vol_list.len) {
	    params->part_thread_target[id] = 1;
	    thr_left--;
	} else {
	    params->part_thread_target[id] = 0;
	}
    }

    if (thr_left && thr_workload) {
	/* compute length-weighted workloads */
	int delta;

	for (diskP = DiskPartitionList; diskP && thr_left; diskP = diskP->next) {
	    id = diskP->device;
	    delta = (diskP->vol_list.len / thr_workload) -
		params->part_thread_target[id];
	    if (delta < 0) {
		continue;
	    }
	    if (delta < thr_left) {
		params->part_thread_target[id] += delta;
		thr_left -= delta;
	    } else {
		params->part_thread_target[id] += thr_left;
		thr_left = 0;
		break;
	    }
	}
    }

    if (thr_left) {
	/* try to assign any leftover threads to partitions that
	 * had volume lengths closer to needing thread_target+1 */
	int max_residue, max_id;

	/* compute the residues */
	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    id = diskP->device;
	    part_residue[id] = diskP->vol_list.len - 
		(params->part_thread_target[id] * thr_workload);
	}

	/* now try to allocate remaining threads to partitions with the
	 * highest residues */
	while (thr_left) {
	    max_residue = 0;
	    for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
		id = diskP->device;
		if (part_residue[id] > max_residue) {
		    max_residue = part_residue[id];
		    max_id = id;
		}
	    }

	    if (!max_residue) {
		break;
	    }

	    params->part_thread_target[max_id]++;
	    thr_left--;
	    part_residue[max_id] = 0;
	}
    }

    if (thr_left) {
	/* punt and give any remaining threads equally to each partition */
	int alloc;
	if (thr_left >= params->n_parts) {
	    alloc = thr_left / params->n_parts;
	    for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
		id = diskP->device;
		params->part_thread_target[id] += alloc;
		thr_left -= alloc;
	    }
	}

	/* finish off the last of the threads */
	for (diskP = DiskPartitionList; thr_left && diskP; diskP = diskP->next) {
	    id = diskP->device;
	    params->part_thread_target[id]++;
	    thr_left--;
	}
    }
}

/* worker thread for parallel shutdown */
static void *
VShutdownThread(void * args)
{
    struct rx_queue *qp;
    Volume * vp;
    vshutdown_thread_t * params;
    int part, code, found, pass, schedule_version_save, count;
    struct DiskPartition *diskP;
    struct diskpartition_queue_t * dpq;
    Device id;

    params = (vshutdown_thread_t *) args;

    /* acquire the shutdown pass 0 lock */
    assert(pthread_mutex_lock(&params->lock) == 0);

    /* if there's still pass 0 work to be done,
     * get a work entry, and do a pass 0 shutdown */
    if (queue_IsNotEmpty(params)) {
	dpq = queue_First(params, diskpartition_queue_t);
	queue_Remove(dpq);
	assert(pthread_mutex_unlock(&params->lock) == 0);
	diskP = dpq->diskP;
	free(dpq);
	id = diskP->device;

	count = 0;
	while (ShutdownVolumeWalk_r(diskP, 0, &params->part_pass_head[id]))
	    count++;
	params->stats[0][diskP->device] = count;
	assert(pthread_mutex_lock(&params->lock) == 0);
    }

    params->n_threads_complete++;
    if (params->n_threads_complete == params->n_threads) {
      /* notify control thread that all workers have completed pass 0 */
      assert(pthread_cond_signal(&params->master_cv) == 0);
    }
    while (params->pass == 0) {
      assert(pthread_cond_wait(&params->cv, &params->lock) == 0);
    }

    /* switch locks */
    assert(pthread_mutex_unlock(&params->lock) == 0);
    VOL_LOCK;

    pass = params->pass;
    assert(pass > 0);

    /* now escalate through the more complicated shutdowns */
    while (pass <= 3) {
	schedule_version_save = params->schedule_version;
	found = 0;
	/* find a disk partition to work on */
	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    id = diskP->device;
	    if (params->part_thread_target[id] && !params->part_done_pass[id]) {
		params->part_thread_target[id]--;
		found = 1;
		break;
	    }
	}
	
	if (!found) {
	    /* hmm. for some reason the controller thread couldn't find anything for 
	     * us to do. let's see if there's anything we can do */
	    for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
		id = diskP->device;
		if (diskP->vol_list.len && !params->part_done_pass[id]) {
		    found = 1;
		    break;
		} else if (!params->part_done_pass[id]) {
		    params->part_done_pass[id] = 1;
		    params->n_parts_done_pass++;
		    if (pass == 3) {
			Log("VShutdown:  done shutting down volumes on partition %s.\n",
			    VPartitionPath(diskP));
		    }
		}
	    }
	}
	
	/* do work on this partition until either the controller
	 * creates a new schedule, or we run out of things to do
	 * on this partition */
	if (found) {
	    count = 0;
	    while (!params->part_done_pass[id] &&
		   (schedule_version_save == params->schedule_version)) {
		/* ShutdownVolumeWalk_r will drop the glock internally */
		if (!ShutdownVolumeWalk_r(diskP, pass, &params->part_pass_head[id])) {
		    if (!params->part_done_pass[id]) {
			params->part_done_pass[id] = 1;
			params->n_parts_done_pass++;
			if (pass == 3) {
			    Log("VShutdown:  done shutting down volumes on partition %s.\n",
				VPartitionPath(diskP));
			}
		    }
		    break;
		}
		count++;
	    }

	    params->stats[pass][id] += count;
	} else {
	    /* ok, everyone is done this pass, proceed */

	    /* barrier lock */
	    params->n_threads_complete++;
	    while (params->pass == pass) {
		if (params->n_threads_complete == params->n_threads) {
		    /* we are the last thread to complete, so we will
		     * reinitialize worker pool state for the next pass */
		    params->n_threads_complete = 0;
		    params->n_parts_done_pass = 0;
		    params->pass++;
		    for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
			id = diskP->device;
			params->part_done_pass[id] = 0;
			params->part_pass_head[id] = queue_First(&diskP->vol_list, rx_queue);
		    }

		    /* compute a new thread schedule before releasing all the workers */
		    ShutdownCreateSchedule(params);

		    /* wake up all the workers */
		    assert(pthread_cond_broadcast(&params->cv) == 0);

		    VOL_UNLOCK;
		    Log("VShutdown:  pass %d completed using %d threads on %d partitions\n",
			pass, params->n_threads, params->n_parts);
		    VOL_LOCK;
		} else {
		    VOL_CV_WAIT(&params->cv);
		}
	    }
	    pass = params->pass;
	}
	
	/* for fairness */
	VOL_UNLOCK;
	pthread_yield();
	VOL_LOCK;
    }

    VOL_UNLOCK;

    return NULL;
}

/* shut down all volumes on a given disk partition 
 *
 * note that this function will not allow mp-fast
 * shutdown of a partition */
int
VShutdownByPartition_r(struct DiskPartition * dp)
{
    int pass, retVal;
    int pass_stats[4];
    int total;

    /* wait for other exclusive ops to finish */
    VVByPListWait_r(dp);

    /* begin exclusive access */
    VVByPListBeginExclusive_r(dp);

    /* pick the low-hanging fruit first,
     * then do the complicated ones last 
     * (has the advantage of keeping
     *  in-use volumes up until the bitter end) */
    for (pass = 0, total=0; pass < 4; pass++) {
	pass_stats[pass] = ShutdownVByPForPass_r(dp, pass);
	total += pass_stats[pass];
    }

    /* end exclusive access */
    VVByPListEndExclusive_r(dp);

    Log("VShutdownByPartition:  shut down %d volumes on %s (pass[0]=%d, pass[1]=%d, pass[2]=%d, pass[3]=%d)\n",
	total, VPartitionPath(dp), pass_stats[0], pass_stats[1], pass_stats[2], pass_stats[3]);

    return retVal;
}

/* internal shutdown functionality
 *
 * for multi-pass shutdown:
 * 0 to only "shutdown" {pre,un}attached and error state volumes
 * 1 to also shutdown attached volumes w/ volume header loaded
 * 2 to also shutdown attached volumes w/o volume header loaded
 * 3 to also shutdown exclusive state volumes 
 *
 * caller MUST hold exclusive access on the hash chain
 * because we drop vol_glock_mutex internally
 * 
 * this function is reentrant for passes 1--3 
 * (e.g. multiple threads can cooperate to 
 *  shutdown a partition mp-fast)
 *
 * pass 0 is not scaleable because the volume state data is
 * synchronized by vol_glock mutex, and the locking overhead
 * is too high to drop the lock long enough to do linked list
 * traversal
 */
static int
ShutdownVByPForPass_r(struct DiskPartition * dp, int pass)
{
    struct rx_queue * q = queue_First(&dp->vol_list, rx_queue);
    register int i = 0;

    while (ShutdownVolumeWalk_r(dp, pass, &q))
	i++;

    return i;
}

/* conditionally shutdown one volume on partition dp
 * returns 1 if a volume was shutdown in this pass,
 * 0 otherwise */
static int
ShutdownVolumeWalk_r(struct DiskPartition * dp, int pass,
		     struct rx_queue ** idx)
{
    struct rx_queue *qp, *nqp;
    Volume * vp;

    qp = *idx;

    for (queue_ScanFrom(&dp->vol_list, qp, qp, nqp, rx_queue)) {
	vp = (Volume *) (((char *)qp) - offsetof(Volume, vol_list));
	
	switch (pass) {
	case 0:
	    if ((V_attachState(vp) != VOL_STATE_UNATTACHED) &&
		(V_attachState(vp) != VOL_STATE_ERROR) &&
		(V_attachState(vp) != VOL_STATE_PREATTACHED)) {
		break;
	    }
	case 1:
	    if ((V_attachState(vp) == VOL_STATE_ATTACHED) &&
		(vp->header == NULL)) {
		break;
	    }
	case 2:
	    if (VIsExclusiveState(V_attachState(vp))) {
		break;
	    }
	case 3:
	    *idx = nqp;
	    DeleteVolumeFromVByPList_r(vp);
	    VShutdownVolume_r(vp);
	    vp = NULL;
	    return 1;
	}
    }

    return 0;
}

/*
 * shutdown a specific volume
 */
/* caller MUST NOT hold a heavyweight ref on vp */
int
VShutdownVolume_r(Volume * vp)
{
    int code;

    VCreateReservation_r(vp);

    if (LogLevel >= 5) {
	Log("VShutdownVolume_r:  vid=%u, device=%d, state=%hu\n",
	    vp->hashid, vp->partition->device, V_attachState(vp));
    }

    /* wait for other blocking ops to finish */
    VWaitExclusiveState_r(vp);

    assert(VIsValidState(V_attachState(vp)));
    
    switch(V_attachState(vp)) {
    case VOL_STATE_SALVAGING:
	/* make sure salvager knows we don't want
	 * the volume back */
	VCancelSalvage_r(vp, SALVSYNC_SHUTDOWN);
    case VOL_STATE_PREATTACHED:
    case VOL_STATE_ERROR:
	VChangeState_r(vp, VOL_STATE_UNATTACHED);
    case VOL_STATE_UNATTACHED:
	break;
    case VOL_STATE_GOING_OFFLINE:
    case VOL_STATE_SHUTTING_DOWN:
    case VOL_STATE_ATTACHED:
	code = VHold_r(vp);
	if (!code) {
	    if (LogLevel >= 5)
		Log("VShutdown:  Attempting to take volume %u offline.\n",
		    vp->hashid);

	    /* take the volume offline (drops reference count) */
	    VOffline_r(vp, "File server was shut down");
	}
	break;
    }
    
    VCancelReservation_r(vp);
    vp = NULL;
    return 0;
}
#endif /* AFS_DEMAND_ATTACH_FS */


/***************************************************/
/* Header I/O routines                             */
/***************************************************/

/* open a descriptor for the inode (h),
 * read in an on-disk structure into buffer (to) of size (size),
 * verify versionstamp in structure has magic (magic) and
 * optionally verify version (version) if (version) is nonzero
 */
static void
ReadHeader(Error * ec, IHandle_t * h, char *to, int size, bit32 magic,
	   bit32 version)
{
    struct versionStamp *vsn;
    FdHandle_t *fdP;

    *ec = 0;
    if (h == NULL) {
	*ec = VSALVAGE;
	return;
    }

    fdP = IH_OPEN(h);
    if (fdP == NULL) {
	*ec = VSALVAGE;
	return;
    }

    if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
	*ec = VSALVAGE;
	FDH_REALLYCLOSE(fdP);
	return;
    }
    vsn = (struct versionStamp *)to;
    if (FDH_READ(fdP, to, size) != size || vsn->magic != magic) {
	*ec = VSALVAGE;
	FDH_REALLYCLOSE(fdP);
	return;
    }
    FDH_CLOSE(fdP);

    /* Check is conditional, in case caller wants to inspect version himself */
    if (version && vsn->version != version) {
	*ec = VSALVAGE;
    }
}

void
WriteVolumeHeader_r(Error * ec, Volume * vp)
{
    IHandle_t *h = V_diskDataHandle(vp);
    FdHandle_t *fdP;

    *ec = 0;

    fdP = IH_OPEN(h);
    if (fdP == NULL) {
	*ec = VSALVAGE;
	return;
    }
    if (FDH_SEEK(fdP, 0, SEEK_SET) < 0) {
	*ec = VSALVAGE;
	FDH_REALLYCLOSE(fdP);
	return;
    }
    if (FDH_WRITE(fdP, (char *)&V_disk(vp), sizeof(V_disk(vp)))
	!= sizeof(V_disk(vp))) {
	*ec = VSALVAGE;
	FDH_REALLYCLOSE(fdP);
	return;
    }
    FDH_CLOSE(fdP);
}

/* VolumeHeaderToDisk
 * Allows for storing 64 bit inode numbers in on-disk volume header
 * file.
 */
/* convert in-memory representation of a volume header to the
 * on-disk representation of a volume header */
void
VolumeHeaderToDisk(VolumeDiskHeader_t * dh, VolumeHeader_t * h)
{

    memset((char *)dh, 0, sizeof(VolumeDiskHeader_t));
    dh->stamp = h->stamp;
    dh->id = h->id;
    dh->parent = h->parent;

#ifdef AFS_64BIT_IOPS_ENV
    dh->volumeInfo_lo = (afs_int32) h->volumeInfo & 0xffffffff;
    dh->volumeInfo_hi = (afs_int32) (h->volumeInfo >> 32) & 0xffffffff;
    dh->smallVnodeIndex_lo = (afs_int32) h->smallVnodeIndex & 0xffffffff;
    dh->smallVnodeIndex_hi =
	(afs_int32) (h->smallVnodeIndex >> 32) & 0xffffffff;
    dh->largeVnodeIndex_lo = (afs_int32) h->largeVnodeIndex & 0xffffffff;
    dh->largeVnodeIndex_hi =
	(afs_int32) (h->largeVnodeIndex >> 32) & 0xffffffff;
    dh->linkTable_lo = (afs_int32) h->linkTable & 0xffffffff;
    dh->linkTable_hi = (afs_int32) (h->linkTable >> 32) & 0xffffffff;
#else
    dh->volumeInfo_lo = h->volumeInfo;
    dh->smallVnodeIndex_lo = h->smallVnodeIndex;
    dh->largeVnodeIndex_lo = h->largeVnodeIndex;
    dh->linkTable_lo = h->linkTable;
#endif
}

/* DiskToVolumeHeader
 * Converts an on-disk representation of a volume header to
 * the in-memory representation of a volume header.
 *
 * Makes the assumption that AFS has *always* 
 * zero'd the volume header file so that high parts of inode
 * numbers are 0 in older (SGI EFS) volume header files.
 */
void
DiskToVolumeHeader(VolumeHeader_t * h, VolumeDiskHeader_t * dh)
{
    memset((char *)h, 0, sizeof(VolumeHeader_t));
    h->stamp = dh->stamp;
    h->id = dh->id;
    h->parent = dh->parent;

#ifdef AFS_64BIT_IOPS_ENV
    h->volumeInfo =
	(Inode) dh->volumeInfo_lo | ((Inode) dh->volumeInfo_hi << 32);

    h->smallVnodeIndex =
	(Inode) dh->smallVnodeIndex_lo | ((Inode) dh->
					  smallVnodeIndex_hi << 32);

    h->largeVnodeIndex =
	(Inode) dh->largeVnodeIndex_lo | ((Inode) dh->
					  largeVnodeIndex_hi << 32);
    h->linkTable =
	(Inode) dh->linkTable_lo | ((Inode) dh->linkTable_hi << 32);
#else
    h->volumeInfo = dh->volumeInfo_lo;
    h->smallVnodeIndex = dh->smallVnodeIndex_lo;
    h->largeVnodeIndex = dh->largeVnodeIndex_lo;
    h->linkTable = dh->linkTable_lo;
#endif
}


/***************************************************/
/* Volume Attachment routines                      */
/***************************************************/

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * pre-attach a volume given its path.
 *
 * @param[out] ec         outbound error code
 * @param[in]  partition  partition path string
 * @param[in]  name       volume id string
 *
 * @return volume object pointer
 *
 * @note A pre-attached volume will only have its partition
 *       and hashid fields initialized.  At first call to 
 *       VGetVolume, the volume will be fully attached.
 *
 */
Volume *
VPreAttachVolumeByName(Error * ec, char *partition, char *name)
{
    Volume * vp;
    VOL_LOCK;
    vp = VPreAttachVolumeByName_r(ec, partition, name);
    VOL_UNLOCK;
    return vp;
}

/**
 * pre-attach a volume given its path.
 *
 * @param[out] ec         outbound error code
 * @param[in]  partition  path to vice partition
 * @param[in]  name       volume id string
 *
 * @return volume object pointer
 *
 * @pre VOL_LOCK held
 *
 * @internal volume package internal use only.
 */
Volume *
VPreAttachVolumeByName_r(Error * ec, char *partition, char *name)
{
    return VPreAttachVolumeById_r(ec, 
				  partition,
				  VolumeNumber(name));
}

/**
 * pre-attach a volume given its path and numeric volume id.
 *
 * @param[out] ec          error code return
 * @param[in]  partition   path to vice partition
 * @param[in]  volumeId    numeric volume id
 *
 * @return volume object pointer
 *
 * @pre VOL_LOCK held
 *
 * @internal volume package internal use only.
 */
Volume *
VPreAttachVolumeById_r(Error * ec, 
		       char * partition,
		       VolId volumeId)
{
    Volume *vp;
    struct DiskPartition *partp;

    *ec = 0;

    assert(programType == fileServer);

    if (!(partp = VGetPartition_r(partition, 0))) {
	*ec = VNOVOL;
	Log("VPreAttachVolumeById_r:  Error getting partition (%s)\n", partition);
	return NULL;
    }

    vp = VLookupVolume_r(ec, volumeId, NULL);
    if (*ec) {
	return NULL;
    }

    return VPreAttachVolumeByVp_r(ec, partp, vp, volumeId);
}

/**
 * preattach a volume.
 *
 * @param[out] ec     outbound error code
 * @param[in]  partp  pointer to partition object
 * @param[in]  vp     pointer to volume object
 * @param[in]  vid    volume id
 *
 * @return volume object pointer
 *
 * @pre VOL_LOCK is held.
 *
 * @warning Returned volume object pointer does not have to
 *          equal the pointer passed in as argument vp.  There
 *          are potential race conditions which can result in
 *          the pointers having different values.  It is up to
 *          the caller to make sure that references are handled
 *          properly in this case.
 *
 * @note If there is already a volume object registered with
 *       the same volume id, its pointer MUST be passed as 
 *       argument vp.  Failure to do so will result in a silent
 *       failure to preattach.
 *
 * @internal volume package internal use only.
 */
Volume * 
VPreAttachVolumeByVp_r(Error * ec, 
		       struct DiskPartition * partp, 
		       Volume * vp,
		       VolId vid)
{
    Volume *nvp = NULL;

    *ec = 0;

    /* check to see if pre-attach already happened */
    if (vp && 
	(V_attachState(vp) != VOL_STATE_UNATTACHED) && 
	!VIsErrorState(V_attachState(vp)) &&
	((V_attachState(vp) != VOL_STATE_PREATTACHED) ||
	 vp->pending_vol_op == NULL)) {
	/*
	 * pre-attach is a no-op in all but the following cases:
	 *
	 *   - volume is unattached
	 *   - volume is in an error state
	 *   - volume is pre-attached with a pending volume operation
	 *     (e.g. vos move between two partitions on same server)
	 */
	goto done;
    } else if (vp) {
	/* we're re-attaching a volume; clear out some old state */
	memset(&vp->salvage, 0, sizeof(struct VolumeOnlineSalvage));

	if (V_partition(vp) != partp) {
	    /* XXX potential race */
	    DeleteVolumeFromVByPList_r(vp);
	}
    } else {
	/* if we need to allocate a new Volume struct,
	 * go ahead and drop the vol glock, otherwise
	 * do the basic setup synchronised, as it's
	 * probably not worth dropping the lock */
	VOL_UNLOCK;

	/* allocate the volume structure */
	vp = nvp = (Volume *) malloc(sizeof(Volume));
	assert(vp != NULL);
	memset(vp, 0, sizeof(Volume));
	queue_Init(&vp->vnode_list);
	assert(pthread_cond_init(&V_attachCV(vp), NULL) == 0);
    }

    /* link the volume with its associated vice partition */
    vp->device = partp->device;
    vp->partition = partp;
    vp->hashid = vid;

    /* if we dropped the lock, reacquire the lock,
     * check for pre-attach races, and then add
     * the volume to the hash table */
    if (nvp) {
	VOL_LOCK;
	nvp = VLookupVolume_r(ec, vid, NULL);
	if (*ec) {
	    free(vp);
	    vp = NULL;
	    goto done;
	} else if (nvp) { /* race detected */
	    free(vp);
	    vp = nvp;
	    goto done;
	} else {
	  /* hack to make up for VChangeState_r() decrementing 
	   * the old state counter */
	  VStats.state_levels[0]++;
	}
    }

    /* put pre-attached volume onto the hash table
     * and bring it up to the pre-attached state */
    AddVolumeToHashTable(vp, vp->hashid);
    AddVolumeToVByPList_r(vp);
    VLRU_Init_Node_r(vp);
    VChangeState_r(vp, VOL_STATE_PREATTACHED);

    if (LogLevel >= 5)
	Log("VPreAttachVolumeByVp_r:  volume %u pre-attached\n", vp->hashid);

  done:
    if (*ec)
	return NULL;
    else
	return vp;
}
#endif /* AFS_DEMAND_ATTACH_FS */

/* Attach an existing volume, given its pathname, and return a
   pointer to the volume header information.  The volume also
   normally goes online at this time.  An offline volume
   must be reattached to make it go online */
Volume *
VAttachVolumeByName(Error * ec, char *partition, char *name, int mode)
{
    Volume *retVal;
    VOL_LOCK;
    retVal = VAttachVolumeByName_r(ec, partition, name, mode);
    VOL_UNLOCK;
    return retVal;
}

Volume *
VAttachVolumeByName_r(Error * ec, char *partition, char *name, int mode)
{
    register Volume *vp = NULL, *svp = NULL;
    int fd, n;
    struct afs_stat status;
    struct VolumeDiskHeader diskHeader;
    struct VolumeHeader iheader;
    struct DiskPartition *partp;
    char path[64];
    int isbusy = 0;
    VolId volumeId;
#ifdef AFS_DEMAND_ATTACH_FS
    VolumeStats stats_save;
#endif /* AFS_DEMAND_ATTACH_FS */

    *ec = 0;
   
    volumeId = VolumeNumber(name);

    if (!(partp = VGetPartition_r(partition, 0))) {
	*ec = VNOVOL;
	Log("VAttachVolume: Error getting partition (%s)\n", partition);
	goto done;
    }

    if (programType == volumeUtility) {
	assert(VInit == 3);
	VLockPartition_r(partition);
    } else if (programType == fileServer) {
#ifdef AFS_DEMAND_ATTACH_FS
	/* lookup the volume in the hash table */
	vp = VLookupVolume_r(ec, volumeId, NULL);
	if (*ec) {
	    return NULL;
	}

	if (vp) {
	    /* save any counters that are supposed to
	     * be monotonically increasing over the
	     * lifetime of the fileserver */
	    memcpy(&stats_save, &vp->stats, sizeof(VolumeStats));
	} else {
	    memset(&stats_save, 0, sizeof(VolumeStats));
	}

	/* if there's something in the hash table, and it's not
	 * in the pre-attach state, then we may need to detach
	 * it before proceeding */
	if (vp && (V_attachState(vp) != VOL_STATE_PREATTACHED)) {
	    VCreateReservation_r(vp);
	    VWaitExclusiveState_r(vp);

	    /* at this point state must be one of:
	     *   UNATTACHED,
	     *   ATTACHED,
	     *   SHUTTING_DOWN,
	     *   GOING_OFFLINE,
	     *   SALVAGING,
	     *   ERROR
	     */

	    if (vp->specialStatus == VBUSY)
		isbusy = 1;
	    
	    /* if it's already attached, see if we can return it */
	    if (V_attachState(vp) == VOL_STATE_ATTACHED) {
		VGetVolumeByVp_r(ec, vp);
		if (V_inUse(vp)) {
		    VCancelReservation_r(vp);
		    return vp;
		}

		/* otherwise, we need to detach, and attempt to re-attach */
		VDetachVolume_r(ec, vp);
		if (*ec) {
		    Log("VAttachVolume: Error detaching old volume instance (%s)\n", name);
		}
	    } else {
		/* if it isn't fully attached, delete from the hash tables,
		   and let the refcounter handle the rest */
		DeleteVolumeFromHashTable(vp);
		DeleteVolumeFromVByPList_r(vp);
	    }

	    VCancelReservation_r(vp);
	    vp = NULL;
	}

	/* pre-attach volume if it hasn't been done yet */
	if (!vp || 
	    (V_attachState(vp) == VOL_STATE_UNATTACHED) ||
	    (V_attachState(vp) == VOL_STATE_ERROR)) {
	    svp = vp;
	    vp = VPreAttachVolumeByVp_r(ec, partp, vp, volumeId);
	    if (*ec) {
		return NULL;
	    }
	}

	assert(vp != NULL);

	/* handle pre-attach races 
	 *
	 * multiple threads can race to pre-attach a volume,
	 * but we can't let them race beyond that
	 * 
	 * our solution is to let the first thread to bring
	 * the volume into an exclusive state win; the other
	 * threads just wait until it finishes bringing the
	 * volume online, and then they do a vgetvolumebyvp
	 */
	if (svp && (svp != vp)) {
	    /* wait for other exclusive ops to finish */
	    VCreateReservation_r(vp);
	    VWaitExclusiveState_r(vp);

	    /* get a heavyweight ref, kill the lightweight ref, and return */
	    VGetVolumeByVp_r(ec, vp);
	    VCancelReservation_r(vp);
	    return vp;
	}

	/* at this point, we are chosen as the thread to do
	 * demand attachment for this volume. all other threads
	 * doing a getvolume on vp->hashid will block until we finish */

	/* make sure any old header cache entries are invalidated
	 * before proceeding */
	FreeVolumeHeader(vp);

	VChangeState_r(vp, VOL_STATE_ATTACHING);

	/* restore any saved counters */
	memcpy(&vp->stats, &stats_save, sizeof(VolumeStats));
#else /* AFS_DEMAND_ATTACH_FS */
	vp = VGetVolume_r(ec, volumeId);
	if (vp) {
	    if (V_inUse(vp))
		return vp;
	    if (vp->specialStatus == VBUSY)
		isbusy = 1;
	    VDetachVolume_r(ec, vp);
	    if (*ec) {
		Log("VAttachVolume: Error detaching volume (%s)\n", name);
	    }
	    vp = NULL;
	}
#endif /* AFS_DEMAND_ATTACH_FS */
    }

    *ec = 0;
    strcpy(path, VPartitionPath(partp));

    VOL_UNLOCK;

    strcat(path, "/");
    strcat(path, name);
    if ((fd = afs_open(path, O_RDONLY)) == -1 || afs_fstat(fd, &status) == -1) {
	Log("VAttachVolume: Failed to open %s (errno %d)\n", path, errno);
	if (fd > -1)
	    close(fd);
	*ec = VNOVOL;
	VOL_LOCK;
	goto done;
    }
    n = read(fd, &diskHeader, sizeof(diskHeader));
    close(fd);
    if (n != sizeof(diskHeader)
	|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	Log("VAttachVolume: Error reading volume header %s\n", path);
	*ec = VSALVAGE;
	VOL_LOCK;
	goto done;
    }
    if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	Log("VAttachVolume: Volume %s, version number is incorrect; volume needs salvaged\n", path);
	*ec = VSALVAGE;
	VOL_LOCK;
	goto done;
    }

    DiskToVolumeHeader(&iheader, &diskHeader);
#ifdef FSSYNC_BUILD_CLIENT
    if (programType == volumeUtility && mode != V_SECRETLY && mode != V_PEEK) {
        VOL_LOCK;
	if (FSYNC_VolOp(iheader.id, partition, FSYNC_VOL_NEEDVOLUME, mode, NULL)
	    != SYNC_OK) {
	    Log("VAttachVolume: attach of volume %u apparently denied by file server\n", iheader.id);
	    *ec = VNOVOL;	/* XXXX */
	    goto done;
	}
	VOL_UNLOCK;
    }
#endif

    if (!vp) {
      vp = (Volume *) calloc(1, sizeof(Volume));
      assert(vp != NULL);
      vp->device = partp->device;
      vp->partition = partp;
      queue_Init(&vp->vnode_list);
#ifdef AFS_DEMAND_ATTACH_FS
      assert(pthread_cond_init(&V_attachCV(vp), NULL) == 0);
#endif /* AFS_DEMAND_ATTACH_FS */
    }

    /* attach2 is entered without any locks, and returns
     * with vol_glock_mutex held */
    vp = attach2(ec, volumeId, path, &iheader, partp, vp, isbusy, mode);

    if (programType == volumeUtility && vp) {
#ifdef AFS_DEMAND_ATTACH_FS
	/* for dafs, we should tell the fileserver, except for V_PEEK
         * where we know it is not necessary */
	if (mode == V_PEEK) {
	    vp->needsPutBack = 0;
	} else {
	    vp->needsPutBack = 1;
	}
#else /* !AFS_DEMAND_ATTACH_FS */
	/* duplicate computation in fssync.c about whether the server
	 * takes the volume offline or not.  If the volume isn't
	 * offline, we must not return it when we detach the volume,
	 * or the server will abort */
	if (mode == V_READONLY || mode == V_PEEK
	    || (!VolumeWriteable(vp) && (mode == V_CLONE || mode == V_DUMP)))
	    vp->needsPutBack = 0;
	else
	    vp->needsPutBack = 1;
#endif /* !AFS_DEMAND_ATTACH_FS */
    }
    /* OK, there's a problem here, but one that I don't know how to
     * fix right now, and that I don't think should arise often.
     * Basically, we should only put back this volume to the server if
     * it was given to us by the server, but since we don't have a vp,
     * we can't run the VolumeWriteable function to find out as we do
     * above when computing vp->needsPutBack.  So we send it back, but
     * there's a path in VAttachVolume on the server which may abort
     * if this volume doesn't have a header.  Should be pretty rare
     * for all of that to happen, but if it does, probably the right
     * fix is for the server to allow the return of readonly volumes
     * that it doesn't think are really checked out. */
#ifdef FSSYNC_BUILD_CLIENT
    if (programType == volumeUtility && vp == NULL &&
	mode != V_SECRETLY && mode != V_PEEK) {
	FSYNC_VolOp(iheader.id, partition, FSYNC_VOL_ON, 0, NULL);
    } else 
#endif
    if (programType == fileServer && vp) {
#ifdef AFS_DEMAND_ATTACH_FS
	/* 
	 * we can get here in cases where we don't "own"
	 * the volume (e.g. volume owned by a utility).
	 * short circuit around potential disk header races.
	 */
	if (V_attachState(vp) != VOL_STATE_ATTACHED) {
	    goto done;
	}
#endif
	V_needsCallback(vp) = 0;
#ifdef	notdef
	if (VInit >= 2 && V_BreakVolumeCallbacks) {
	    Log("VAttachVolume: Volume %u was changed externally; breaking callbacks\n", V_id(vp));
	    (*V_BreakVolumeCallbacks) (V_id(vp));
	}
#endif
	VUpdateVolume_r(ec, vp, 0);
	if (*ec) {
	    Log("VAttachVolume: Error updating volume\n");
	    if (vp)
		VPutVolume_r(vp);
	    goto done;
	}
	if (VolumeWriteable(vp) && V_dontSalvage(vp) == 0) {
#ifndef AFS_DEMAND_ATTACH_FS
	    /* This is a hack: by temporarily setting the incore
	     * dontSalvage flag ON, the volume will be put back on the
	     * Update list (with dontSalvage OFF again).  It will then
	     * come back in N minutes with DONT_SALVAGE eventually
	     * set.  This is the way that volumes that have never had
	     * it set get it set; or that volumes that have been
	     * offline without DONT SALVAGE having been set also
	     * eventually get it set */
	    V_dontSalvage(vp) = DONT_SALVAGE;
#endif /* !AFS_DEMAND_ATTACH_FS */
	    VAddToVolumeUpdateList_r(ec, vp);
	    if (*ec) {
		Log("VAttachVolume: Error adding volume to update list\n");
		if (vp)
		    VPutVolume_r(vp);
		goto done;
	    }
	}
	if (LogLevel)
	    Log("VOnline:  volume %u (%s) attached and online\n", V_id(vp),
		V_name(vp));
    }

  done:
    if (programType == volumeUtility) {
	VUnlockPartition_r(partition);
    }
    if (*ec) {
#ifdef AFS_DEMAND_ATTACH_FS
	/* attach failed; make sure we're in error state */
	if (vp && !VIsErrorState(V_attachState(vp))) {
	    VChangeState_r(vp, VOL_STATE_ERROR);
	}
#endif /* AFS_DEMAND_ATTACH_FS */
	return NULL;
    } else {
	return vp;
    }
}

#ifdef AFS_DEMAND_ATTACH_FS
/* VAttachVolumeByVp_r
 *
 * finish attaching a volume that is
 * in a less than fully attached state
 */
/* caller MUST hold a ref count on vp */
static Volume *
VAttachVolumeByVp_r(Error * ec, Volume * vp, int mode)
{
    char name[VMAXPATHLEN];
    int fd, n, reserve = 0;
    struct afs_stat status;
    struct VolumeDiskHeader diskHeader;
    struct VolumeHeader iheader;
    struct DiskPartition *partp;
    char path[64];
    int isbusy = 0;
    VolId volumeId;
    Volume * nvp;
    VolumeStats stats_save;
    *ec = 0;

    /* volume utility should never call AttachByVp */
    assert(programType == fileServer);
   
    volumeId = vp->hashid;
    partp = vp->partition;
    VolumeExternalName_r(volumeId, name, sizeof(name));


    /* if another thread is performing a blocking op, wait */
    VWaitExclusiveState_r(vp);

    memcpy(&stats_save, &vp->stats, sizeof(VolumeStats));

    /* if it's already attached, see if we can return it */
    if (V_attachState(vp) == VOL_STATE_ATTACHED) {
	VGetVolumeByVp_r(ec, vp);
	if (V_inUse(vp)) {
	    return vp;
	} else {
	    if (vp->specialStatus == VBUSY)
		isbusy = 1;
	    VDetachVolume_r(ec, vp);
	    if (*ec) {
		Log("VAttachVolume: Error detaching volume (%s)\n", name);
	    }
	    vp = NULL;
	}
    }

    /* pre-attach volume if it hasn't been done yet */
    if (!vp || 
	(V_attachState(vp) == VOL_STATE_UNATTACHED) ||
	(V_attachState(vp) == VOL_STATE_ERROR)) {
	nvp = VPreAttachVolumeByVp_r(ec, partp, vp, volumeId);
	if (*ec) {
	    return NULL;
	}
	if (nvp != vp) {
	    reserve = 1;
	    VCreateReservation_r(nvp);
	    vp = nvp;
	}
    }
    
    assert(vp != NULL);
    VChangeState_r(vp, VOL_STATE_ATTACHING);

    /* restore monotonically increasing stats */
    memcpy(&vp->stats, &stats_save, sizeof(VolumeStats));

    *ec = 0;


    /* compute path to disk header, 
     * read in header, 
     * and verify magic and version stamps */
    strcpy(path, VPartitionPath(partp));

    VOL_UNLOCK;

    strcat(path, "/");
    strcat(path, name);
    if ((fd = afs_open(path, O_RDONLY)) == -1 || afs_fstat(fd, &status) == -1) {
	Log("VAttachVolume: Failed to open %s (errno %d)\n", path, errno);
	if (fd > -1)
	    close(fd);
	*ec = VNOVOL;
	VOL_LOCK;
	goto done;
    }
    n = read(fd, &diskHeader, sizeof(diskHeader));
    close(fd);
    if (n != sizeof(diskHeader)
	|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	Log("VAttachVolume: Error reading volume header %s\n", path);
	*ec = VSALVAGE;
	VOL_LOCK;
	goto done;
    }
    if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	Log("VAttachVolume: Volume %s, version number is incorrect; volume needs salvaged\n", path);
	*ec = VSALVAGE;
	VOL_LOCK;
	goto done;
    }

    /* convert on-disk header format to in-memory header format */
    DiskToVolumeHeader(&iheader, &diskHeader);

    /* do volume attach
     *
     * NOTE: attach2 is entered without any locks, and returns
     * with vol_glock_mutex held */
    vp = attach2(ec, volumeId, path, &iheader, partp, vp, isbusy, mode);

    /*
     * the event that an error was encountered, or
     * the volume was not brought to an attached state
     * for any reason, skip to the end.  We cannot
     * safely call VUpdateVolume unless we "own" it.
     */
    if (*ec || 
	(vp == NULL) ||
	(V_attachState(vp) != VOL_STATE_ATTACHED)) {
	goto done;
    }

    V_needsCallback(vp) = 0;
    VUpdateVolume_r(ec, vp, 0);
    if (*ec) {
	Log("VAttachVolume: Error updating volume %u\n", vp->hashid);
	VPutVolume_r(vp);
	goto done;
    }
    if (VolumeWriteable(vp) && V_dontSalvage(vp) == 0) {
#ifndef AFS_DEMAND_ATTACH_FS
	/* This is a hack: by temporarily setting the incore
	 * dontSalvage flag ON, the volume will be put back on the
	 * Update list (with dontSalvage OFF again).  It will then
	 * come back in N minutes with DONT_SALVAGE eventually
	 * set.  This is the way that volumes that have never had
	 * it set get it set; or that volumes that have been
	 * offline without DONT SALVAGE having been set also
	 * eventually get it set */
	V_dontSalvage(vp) = DONT_SALVAGE;
#endif /* !AFS_DEMAND_ATTACH_FS */
	VAddToVolumeUpdateList_r(ec, vp);
	if (*ec) {
	    Log("VAttachVolume: Error adding volume %u to update list\n", vp->hashid);
	    if (vp)
		VPutVolume_r(vp);
	    goto done;
	}
    }
    if (LogLevel)
	Log("VOnline:  volume %u (%s) attached and online\n", V_id(vp),
	    V_name(vp));
  done:
    if (reserve) {
	VCancelReservation_r(nvp);
	reserve = 0;
    }
    if (*ec && (*ec != VOFFLINE) && (*ec != VSALVAGE)) {
	if (vp && !VIsErrorState(V_attachState(vp))) {
	    VChangeState_r(vp, VOL_STATE_ERROR);
	}
	return NULL;
    } else {
	return vp;
    }
}
#endif /* AFS_DEMAND_ATTACH_FS */

/*
 * called without any locks held
 * returns with vol_glock_mutex held
 */
private Volume * 
attach2(Error * ec, VolId volumeId, char *path, register struct VolumeHeader * header,
	struct DiskPartition * partp, register Volume * vp, int isbusy, int mode)
{
    vp->specialStatus = (byte) (isbusy ? VBUSY : 0);
    IH_INIT(vp->vnodeIndex[vLarge].handle, partp->device, header->parent,
	    header->largeVnodeIndex);
    IH_INIT(vp->vnodeIndex[vSmall].handle, partp->device, header->parent,
	    header->smallVnodeIndex);
    IH_INIT(vp->diskDataHandle, partp->device, header->parent,
	    header->volumeInfo);
    IH_INIT(vp->linkHandle, partp->device, header->parent, header->linkTable);
    vp->shuttingDown = 0;
    vp->goingOffline = 0;
    vp->nUsers = 1;
#ifdef AFS_DEMAND_ATTACH_FS
    vp->stats.last_attach = FT_ApproxTime();
    vp->stats.attaches++;
#endif

    VOL_LOCK;
    IncUInt64(&VStats.attaches);
    vp->cacheCheck = ++VolumeCacheCheck;
    /* just in case this ever rolls over */
    if (!vp->cacheCheck)
	vp->cacheCheck = ++VolumeCacheCheck;
    GetVolumeHeader(vp);
    VOL_UNLOCK;

#if defined(AFS_DEMAND_ATTACH_FS) && defined(FSSYNC_BUILD_CLIENT)
    /* demand attach changes the V_PEEK mechanism
     *
     * we can now suck the current disk data structure over
     * the fssync interface without going to disk
     *
     * (technically, we don't need to restrict this feature
     *  to demand attach fileservers.  However, I'm trying
     *  to limit the number of common code changes)
     */
    if (programType != fileServer && mode == V_PEEK) {
	SYNC_response res;
	res.payload.len = sizeof(VolumeDiskData);
	res.payload.buf = &vp->header->diskstuff;

	if (FSYNC_VolOp(volumeId,
			VPartitionPath(partp),
			FSYNC_VOL_QUERY_HDR,
			FSYNC_WHATEVER,
			&res) == SYNC_OK) {
	    goto disk_header_loaded;
	}
    }
#endif /* AFS_DEMAND_ATTACH_FS && FSSYNC_BUILD_CLIENT */
    (void)ReadHeader(ec, V_diskDataHandle(vp), (char *)&V_disk(vp),
		     sizeof(V_disk(vp)), VOLUMEINFOMAGIC, VOLUMEINFOVERSION);

#ifdef AFS_DEMAND_ATTACH_FS
    /* update stats */
    VOL_LOCK;
    IncUInt64(&VStats.hdr_loads);
    IncUInt64(&vp->stats.hdr_loads);
    VOL_UNLOCK;
#endif /* AFS_DEMAND_ATTACH_FS */
    
    if (*ec) {
	Log("VAttachVolume: Error reading diskDataHandle vol header %s; error=%u\n", path, *ec);
    }

 disk_header_loaded:

#ifdef AFS_DEMAND_ATTACH_FS
    if (!*ec) {

	/* check for pending volume operations */
	if (vp->pending_vol_op) {
	    /* see if the pending volume op requires exclusive access */
	    if (!VVolOpLeaveOnline_r(vp, vp->pending_vol_op)) {
		/* mark the volume down */
		*ec = VOFFLINE;
		VChangeState_r(vp, VOL_STATE_UNATTACHED);
		if (V_offlineMessage(vp)[0] == '\0')
		    strlcpy(V_offlineMessage(vp),
			    "A volume utility is running.", 
			    sizeof(V_offlineMessage(vp)));
		V_offlineMessage(vp)[sizeof(V_offlineMessage(vp)) - 1] = '\0';

		/* check to see if we should set the specialStatus flag */
		if (VVolOpSetVBusy_r(vp, vp->pending_vol_op)) {
		    vp->specialStatus = VBUSY;
		}
	    }
	}

	V_attachFlags(vp) |= VOL_HDR_LOADED;
	vp->stats.last_hdr_load = vp->stats.last_attach;
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    if (!*ec) {
	struct IndexFileHeader iHead;

#if OPENAFS_VOL_STATS
	/*
	 * We just read in the diskstuff part of the header.  If the detailed
	 * volume stats area has not yet been initialized, we should bzero the
	 * area and mark it as initialized.
	 */
	if (!(V_stat_initialized(vp))) {
	    memset((char *)(V_stat_area(vp)), 0, VOL_STATS_BYTES);
	    V_stat_initialized(vp) = 1;
	}
#endif /* OPENAFS_VOL_STATS */

	(void)ReadHeader(ec, vp->vnodeIndex[vSmall].handle,
			 (char *)&iHead, sizeof(iHead),
			 SMALLINDEXMAGIC, SMALLINDEXVERSION);

	if (*ec) {
	    Log("VAttachVolume: Error reading smallVnode vol header %s; error=%u\n", path, *ec);
	}
    }

    if (!*ec) {
	struct IndexFileHeader iHead;

	(void)ReadHeader(ec, vp->vnodeIndex[vLarge].handle,
			 (char *)&iHead, sizeof(iHead),
			 LARGEINDEXMAGIC, LARGEINDEXVERSION);

	if (*ec) {
	    Log("VAttachVolume: Error reading largeVnode vol header %s; error=%u\n", path, *ec);
	}
    }

#ifdef AFS_NAMEI_ENV
    if (!*ec) {
	struct versionStamp stamp;

	(void)ReadHeader(ec, V_linkHandle(vp), (char *)&stamp,
			 sizeof(stamp), LINKTABLEMAGIC, LINKTABLEVERSION);

	if (*ec) {
	    Log("VAttachVolume: Error reading namei vol header %s; error=%u\n", path, *ec);
	}
    }
#endif /* AFS_NAMEI_ENV */

#if defined(AFS_DEMAND_ATTACH_FS)
    if (*ec && ((*ec != VOFFLINE) || (V_attachState(vp) != VOL_STATE_UNATTACHED))) {
        VOL_LOCK;
	if (programType == fileServer) {
	    VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, VOL_SALVAGE_INVALIDATE_HEADER);
	    vp->nUsers = 0;
	} else {
	    Log("VAttachVolume: Error attaching volume %s; volume needs salvage; error=%u\n", path, *ec);
	    FreeVolume(vp);
	    *ec = VSALVAGE;
	}
	return NULL;
    } else if (*ec) {
	/* volume operation in progress */
	VOL_LOCK;
	return NULL;
    }
#else /* AFS_DEMAND_ATTACH_FS */
    if (*ec) {
	Log("VAttachVolume: Error attaching volume %s; volume needs salvage; error=%u\n", path, *ec);
        VOL_LOCK;
	FreeVolume(vp);
	return NULL;
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    if (V_needsSalvaged(vp)) {
	if (vp->specialStatus)
	    vp->specialStatus = 0;
        VOL_LOCK;
#if defined(AFS_DEMAND_ATTACH_FS)
	if (programType == fileServer) {
	    VRequestSalvage_r(ec, vp, SALVSYNC_NEEDED, VOL_SALVAGE_INVALIDATE_HEADER);
	    vp->nUsers = 0;
	} else {
	    Log("VAttachVolume: volume salvage flag is ON for %s; volume needs salvage\n", path);
	    FreeVolume(vp);
	    *ec = VSALVAGE;
	}
#else /* AFS_DEMAND_ATTACH_FS */
	FreeVolume(vp);
	*ec = VSALVAGE;
#endif /* AFS_DEMAND_ATTACH_FS */
	return NULL;
    }

    VOL_LOCK;
    if (programType == fileServer) {
#ifndef FAST_RESTART
	if (V_inUse(vp) && VolumeWriteable(vp)) {
	    if (!V_needsSalvaged(vp)) {
		V_needsSalvaged(vp) = 1;
		VUpdateVolume_r(ec, vp, 0);
	    }
#if defined(AFS_DEMAND_ATTACH_FS)
	    VRequestSalvage_r(ec, vp, SALVSYNC_NEEDED, VOL_SALVAGE_INVALIDATE_HEADER);
	    vp->nUsers = 0;
#else /* AFS_DEMAND_ATTACH_FS */
	    Log("VAttachVolume: volume %s needs to be salvaged; not attached.\n", path);
	    FreeVolume(vp);
	    *ec = VSALVAGE;
#endif /* AFS_DEMAND_ATTACH_FS */
	    return NULL;
	}
#endif /* FAST_RESTART */

	if (V_destroyMe(vp) == DESTROY_ME) {
#if defined(AFS_DEMAND_ATTACH_FS)
	    /* schedule a salvage so the volume goes away on disk */
	    VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, VOL_SALVAGE_INVALIDATE_HEADER);
	    VChangeState_r(vp, VOL_STATE_ERROR);
	    vp->nUsers = 0;
#endif /* AFS_DEMAND_ATTACH_FS */
	    FreeVolume(vp);
	    Log("VAttachVolume: volume %s is junk; it should be destroyed at next salvage\n", path);
	    *ec = VNOVOL;
	    return NULL;
	}
    }

    vp->nextVnodeUnique = V_uniquifier(vp);
    vp->vnodeIndex[vSmall].bitmap = vp->vnodeIndex[vLarge].bitmap = NULL;
#ifndef BITMAP_LATER
    if (programType == fileServer && VolumeWriteable(vp)) {
	int i;
	for (i = 0; i < nVNODECLASSES; i++) {
	    VGetBitmap_r(ec, vp, i);
	    if (*ec) {
#ifdef AFS_DEMAND_ATTACH_FS
		VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, VOL_SALVAGE_INVALIDATE_HEADER);
		vp->nUsers = 0;
#else /* AFS_DEMAND_ATTACH_FS */
		FreeVolume(vp);
#endif /* AFS_DEMAND_ATTACH_FS */
		Log("VAttachVolume: error getting bitmap for volume (%s)\n",
		    path);
		return NULL;
	    }
	}
    }
#endif /* BITMAP_LATER */

    if (programType == fileServer) {
	if (vp->specialStatus)
	    vp->specialStatus = 0;
	if (V_blessed(vp) && V_inService(vp) && !V_needsSalvaged(vp)) {
	    V_inUse(vp) = 1;
	    V_offlineMessage(vp)[0] = '\0';
	}
    }

    AddVolumeToHashTable(vp, V_id(vp));
#ifdef AFS_DEMAND_ATTACH_FS
    AddVolumeToVByPList_r(vp);
    VLRU_Add_r(vp);
    if ((programType != fileServer) ||
	V_inUse(vp)) {
	VChangeState_r(vp, VOL_STATE_ATTACHED);
    } else {
	VChangeState_r(vp, VOL_STATE_UNATTACHED);
    }
#endif
    return vp;
}

/* Attach an existing volume.
   The volume also normally goes online at this time.
   An offline volume must be reattached to make it go online.
 */

Volume *
VAttachVolume(Error * ec, VolumeId volumeId, int mode)
{
    Volume *retVal;
    VOL_LOCK;
    retVal = VAttachVolume_r(ec, volumeId, mode);
    VOL_UNLOCK;
    return retVal;
}

Volume *
VAttachVolume_r(Error * ec, VolumeId volumeId, int mode)
{
    char *part, *name;
    GetVolumePath(ec, volumeId, &part, &name);
    if (*ec) {
	register Volume *vp;
	Error error;
	vp = VGetVolume_r(&error, volumeId);
	if (vp) {
	    assert(V_inUse(vp) == 0);
	    VDetachVolume_r(ec, vp);
	}
	return NULL;
    }
    return VAttachVolumeByName_r(ec, part, name, mode);
}

/* Increment a reference count to a volume, sans context swaps.  Requires
 * possibly reading the volume header in from the disk, since there's
 * an invariant in the volume package that nUsers>0 ==> vp->header is valid.
 *
 * N.B. This call can fail if we can't read in the header!!  In this case
 * we still guarantee we won't context swap, but the ref count won't be
 * incremented (otherwise we'd violate the invariant).
 */
/* NOTE: with the demand attach fileserver extensions, the global lock
 * is dropped within VHold */
#ifdef AFS_DEMAND_ATTACH_FS
static int
VHold_r(register Volume * vp)
{
    Error error;

    VCreateReservation_r(vp);
    VWaitExclusiveState_r(vp);

    LoadVolumeHeader(&error, vp);
    if (error) {
	VCancelReservation_r(vp);
	return error;
    }
    vp->nUsers++;
    VCancelReservation_r(vp);
    return 0;
}
#else /* AFS_DEMAND_ATTACH_FS */
static int
VHold_r(register Volume * vp)
{
    Error error;

    LoadVolumeHeader(&error, vp);
    if (error)
	return error;
    vp->nUsers++;
    return 0;
}
#endif /* AFS_DEMAND_ATTACH_FS */

static int
VHold(register Volume * vp)
{
    int retVal;
    VOL_LOCK;
    retVal = VHold_r(vp);
    VOL_UNLOCK;
    return retVal;
}


/***************************************************/
/* get and put volume routines                     */
/***************************************************/

/**
 * put back a heavyweight reference to a volume object.
 *
 * @param[in] vp  volume object pointer
 *
 * @pre VOL_LOCK held
 *
 * @post heavyweight volume reference put back.
 *       depending on state, volume may have been taken offline,
 *       detached, salvaged, freed, etc.
 *
 * @internal volume package internal use only
 */
void
VPutVolume_r(register Volume * vp)
{
    assert(--vp->nUsers >= 0);
    if (vp->nUsers == 0) {
	VCheckOffline(vp);
	ReleaseVolumeHeader(vp->header);
#ifdef AFS_DEMAND_ATTACH_FS
	if (!VCheckDetach(vp)) {
	    VCheckSalvage(vp);
	    VCheckFree(vp);
	}
#else /* AFS_DEMAND_ATTACH_FS */
	VCheckDetach(vp);
#endif /* AFS_DEMAND_ATTACH_FS */
    }
}

void
VPutVolume(register Volume * vp)
{
    VOL_LOCK;
    VPutVolume_r(vp);
    VOL_UNLOCK;
}


/* Get a pointer to an attached volume.  The pointer is returned regardless
   of whether or not the volume is in service or on/off line.  An error
   code, however, is returned with an indication of the volume's status */
Volume *
VGetVolume(Error * ec, Error * client_ec, VolId volumeId)
{
    Volume *retVal;
    VOL_LOCK;
    retVal = GetVolume(ec, client_ec, volumeId, NULL, 0);
    VOL_UNLOCK;
    return retVal;
}

Volume *
VGetVolume_r(Error * ec, VolId volumeId)
{
    return GetVolume(ec, NULL, volumeId, NULL, 0);
}

/* try to get a volume we've previously looked up */
/* for demand attach fs, caller MUST NOT hold a ref count on vp */
Volume * 
VGetVolumeByVp_r(Error * ec, Volume * vp)
{
    return GetVolume(ec, NULL, vp->hashid, vp, 0);
}

/* private interface for getting a volume handle
 * volumeId must be provided.
 * hint is an optional parameter to speed up hash lookups
 * flags is not used at this time
 */
/* for demand attach fs, caller MUST NOT hold a ref count on hint */
static Volume *
GetVolume(Error * ec, Error * client_ec, VolId volumeId, Volume * hint, int flags)
{
    Volume *vp = hint;
    /* pull this profiling/debugging code out of regular builds */
#ifdef notdef
#define VGET_CTR_INC(x) x++
    unsigned short V0 = 0, V1 = 0, V2 = 0, V3 = 0, V5 = 0, V6 =
	0, V7 = 0, V8 = 0, V9 = 0;
    unsigned short V10 = 0, V11 = 0, V12 = 0, V13 = 0, V14 = 0, V15 = 0;
#else
#define VGET_CTR_INC(x)
#endif
#ifdef AFS_DEMAND_ATTACH_FS
    Volume *avp, * rvp = hint;
#endif

#ifdef AFS_DEMAND_ATTACH_FS
    if (rvp) {
	VCreateReservation_r(rvp);
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    for (;;) {
	*ec = 0;
	if (client_ec)
	    *client_ec = 0;
	VGET_CTR_INC(V0);

	vp = VLookupVolume_r(ec, volumeId, vp);
	if (*ec) {
	    vp = NULL;
	    break;
	}

#ifdef AFS_DEMAND_ATTACH_FS
	if (rvp && (rvp != vp)) {
	    /* break reservation on old vp */
	    VCancelReservation_r(rvp);
	    rvp = NULL;
	}
#endif /* AFS_DEMAND_ATTACH_FS */

	if (!vp) {
	    VGET_CTR_INC(V1);
	    if (VInit < 2) {
		VGET_CTR_INC(V2);
		/* Until we have reached an initialization level of 2
		 * we don't know whether this volume exists or not.
		 * We can't sleep and retry later because before a volume
		 * is attached, the caller tries to get it first.  Just
		 * return VOFFLINE and the caller can choose whether to
		 * retry the command or not. */
		*ec = VOFFLINE;
		break;
	    }

	    *ec = VNOVOL;
	    break;
	}

	VGET_CTR_INC(V3);
	IncUInt64(&VStats.hdr_gets);
	
#ifdef AFS_DEMAND_ATTACH_FS
	/* block if someone else is performing an exclusive op on this volume */
	if (rvp != vp) {
	    rvp = vp;
	    VCreateReservation_r(rvp);
	}
	VWaitExclusiveState_r(vp);

	/* short circuit with VNOVOL in the following circumstances:
	 *
	 *   VOL_STATE_ERROR
	 *   VOL_STATE_SHUTTING_DOWN
	 */
	if ((V_attachState(vp) == VOL_STATE_ERROR) ||
	    (V_attachState(vp) == VOL_STATE_SHUTTING_DOWN)) {
	    *ec = VNOVOL;
	    vp = NULL;
	    break;
	}

	/*
	 * short circuit with VOFFLINE in the following circumstances:
	 *
	 *   VOL_STATE_UNATTACHED
	 */
       if (V_attachState(vp) == VOL_STATE_UNATTACHED) {
           *ec = VOFFLINE;
           vp = NULL;
           break;
       }

	/* allowable states:
	 *   UNATTACHED
	 *   PREATTACHED
	 *   ATTACHED
	 *   GOING_OFFLINE
	 *   SALVAGING
	 */

	if (vp->salvage.requested) {
	    VUpdateSalvagePriority_r(vp);
	}

	if (V_attachState(vp) == VOL_STATE_PREATTACHED) {
	    avp = VAttachVolumeByVp_r(ec, vp, 0);
	    if (avp) {
		if (vp != avp) {
		    /* VAttachVolumeByVp_r can return a pointer
		     * != the vp passed to it under certain
		     * conditions; make sure we don't leak
		     * reservations if that happens */
		    vp = avp;
		    VCancelReservation_r(rvp);
		    rvp = avp;
		    VCreateReservation_r(rvp);
		}
		VPutVolume_r(avp);
	    }
	    if (*ec) {
		int endloop = 0;
		switch (*ec) {
		case VSALVAGING:
		    break;
		case VOFFLINE:
		    if (!vp->pending_vol_op) {
			endloop = 1;
		    }
		    break;
		default:
		    *ec = VNOVOL;
		    endloop = 1;
		}
		if (endloop) {
		    vp = NULL;
		    break;
		}
	    }
	}

	if ((V_attachState(vp) == VOL_STATE_SALVAGING) ||
	    (*ec == VSALVAGING)) {
	    if (client_ec) {
		/* see CheckVnode() in afsfileprocs.c for an explanation
		 * of this error code logic */
		afs_uint32 now = FT_ApproxTime();
		if ((vp->stats.last_salvage + (10 * 60)) >= now) {
		    *client_ec = VBUSY;
		} else {
		    *client_ec = VRESTARTING;
		}
	    }
	    *ec = VSALVAGING;
	    vp = NULL;
	    break;
	}
#endif

	LoadVolumeHeader(ec, vp);
	if (*ec) {
	    VGET_CTR_INC(V6);
	    /* Only log the error if it was a totally unexpected error.  Simply
	     * a missing inode is likely to be caused by the volume being deleted */
	    if (errno != ENXIO || LogLevel)
		Log("Volume %u: couldn't reread volume header\n",
		    vp->hashid);
#ifdef AFS_DEMAND_ATTACH_FS
	    if (programType == fileServer) {
		VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, VOL_SALVAGE_INVALIDATE_HEADER);
	    } else {
		FreeVolume(vp);
		vp = NULL;
	    }
#else /* AFS_DEMAND_ATTACH_FS */
	    FreeVolume(vp);
	    vp = NULL;
#endif /* AFS_DEMAND_ATTACH_FS */
	    break;
	}

#ifdef AFS_DEMAND_ATTACH_FS
	/*
	 * this test MUST happen after the volume header is loaded
	 */
	if (vp->pending_vol_op && !VVolOpLeaveOnline_r(vp, vp->pending_vol_op)) {
	    if (client_ec) {
		/* see CheckVnode() in afsfileprocs.c for an explanation
		 * of this error code logic */
		afs_uint32 now = FT_ApproxTime();
		if ((vp->stats.last_vol_op + (10 * 60)) >= now) {
		    *client_ec = VBUSY;
		} else {
		    *client_ec = VRESTARTING;
		}
	    }
	    *ec = VOFFLINE;
	    ReleaseVolumeHeader(vp->header);
	    vp = NULL;
	    break;
	}
#endif /* AFS_DEMAND_ATTACH_FS */
	
	VGET_CTR_INC(V7);
	if (vp->shuttingDown) {
	    VGET_CTR_INC(V8);
	    *ec = VNOVOL;
	    vp = NULL;
	    break;
	}

	if (programType == fileServer) {
	    VGET_CTR_INC(V9);
	    if (vp->goingOffline) {
		VGET_CTR_INC(V10);
#ifdef AFS_DEMAND_ATTACH_FS
		/* wait for the volume to go offline */
		if (V_attachState(vp) == VOL_STATE_GOING_OFFLINE) {
		    VWaitStateChange_r(vp);
		}
#elif defined(AFS_PTHREAD_ENV)
		VOL_CV_WAIT(&vol_put_volume_cond);
#else /* AFS_PTHREAD_ENV */
		LWP_WaitProcess(VPutVolume);
#endif /* AFS_PTHREAD_ENV */
		continue;
	    }
	    if (vp->specialStatus) {
		VGET_CTR_INC(V11);
		*ec = vp->specialStatus;
	    } else if (V_inService(vp) == 0 || V_blessed(vp) == 0) {
		VGET_CTR_INC(V12);
		*ec = VNOVOL;
	    } else if (V_inUse(vp) == 0) {
		VGET_CTR_INC(V13);
		*ec = VOFFLINE;
	    } else {
		VGET_CTR_INC(V14);
	    }
	}
	break;
    }
    VGET_CTR_INC(V15);

#ifdef AFS_DEMAND_ATTACH_FS
    /* if no error, bump nUsers */
    if (vp) {
	vp->nUsers++;
	VLRU_UpdateAccess_r(vp);
    }
    if (rvp) {
	VCancelReservation_r(rvp);
	rvp = NULL;
    }
    if (client_ec && !*client_ec) {
	*client_ec = *ec;
    }
#else /* AFS_DEMAND_ATTACH_FS */
    /* if no error, bump nUsers */
    if (vp) {
	vp->nUsers++;
    }
    if (client_ec) {
	*client_ec = *ec;
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    assert(vp || *ec);
    return vp;
}


/***************************************************/
/* Volume offline/detach routines                  */
/***************************************************/

/* caller MUST hold a heavyweight ref on vp */
#ifdef AFS_DEMAND_ATTACH_FS
void
VTakeOffline_r(register Volume * vp)
{
    Error error;

    assert(vp->nUsers > 0);
    assert(programType == fileServer);

    VCreateReservation_r(vp);
    VWaitExclusiveState_r(vp);

    vp->goingOffline = 1;
    V_needsSalvaged(vp) = 1;

    VRequestSalvage_r(&error, vp, SALVSYNC_ERROR, 0);
    VCancelReservation_r(vp);
}
#else /* AFS_DEMAND_ATTACH_FS */
void
VTakeOffline_r(register Volume * vp)
{
    assert(vp->nUsers > 0);
    assert(programType == fileServer);

    vp->goingOffline = 1;
    V_needsSalvaged(vp) = 1;
}
#endif /* AFS_DEMAND_ATTACH_FS */

void
VTakeOffline(register Volume * vp)
{
    VOL_LOCK;
    VTakeOffline_r(vp);
    VOL_UNLOCK;
}

/**
 * force a volume offline.
 *
 * @param[in] vp     volume object pointer
 * @param[in] flags  flags (see note below)
 *
 * @note the flag VOL_FORCEOFF_NOUPDATE is a recursion control flag
 *       used when VUpdateVolume_r needs to call VForceOffline_r
 *       (which in turn would normally call VUpdateVolume_r)
 *
 * @see VUpdateVolume_r
 *
 * @pre VOL_LOCK must be held.
 *      for DAFS, caller must hold ref.
 *
 * @note for DAFS, it _is safe_ to call this function from an
 *       exclusive state
 *
 * @post needsSalvaged flag is set.
 *       for DAFS, salvage is requested.
 *       no further references to the volume through the volume 
 *       package will be honored.
 *       all file descriptor and vnode caches are invalidated.
 *
 * @warning this is a heavy-handed interface.  it results in
 *          a volume going offline regardless of the current 
 *          reference count state.
 *
 * @internal  volume package internal use only
 */
void
VForceOffline_r(Volume * vp, int flags)
{
    Error error;
    if (!V_inUse(vp)) {
#ifdef AFS_DEMAND_ATTACH_FS
	VChangeState_r(vp, VOL_STATE_ERROR);
#endif
	return;
    }

    strcpy(V_offlineMessage(vp),
	   "Forced offline due to internal error: volume needs to be salvaged");
    Log("Volume %u forced offline:  it needs salvaging!\n", V_id(vp));

    V_inUse(vp) = 0;
    vp->goingOffline = 0;
    V_needsSalvaged(vp) = 1;
    if (!(flags & VOL_FORCEOFF_NOUPDATE)) {
	VUpdateVolume_r(&error, vp, VOL_UPDATE_NOFORCEOFF);
    }

#ifdef AFS_DEMAND_ATTACH_FS
    VRequestSalvage_r(&error, vp, SALVSYNC_ERROR, VOL_SALVAGE_INVALIDATE_HEADER);
#endif /* AFS_DEMAND_ATTACH_FS */

#ifdef AFS_PTHREAD_ENV
    assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
#else /* AFS_PTHREAD_ENV */
    LWP_NoYieldSignal(VPutVolume);
#endif /* AFS_PTHREAD_ENV */

    VReleaseVolumeHandles_r(vp);
}

/**
 * force a volume offline.
 *
 * @param[in] vp  volume object pointer
 *
 * @see VForceOffline_r
 */
void
VForceOffline(Volume * vp)
{
    VOL_LOCK;
    VForceOffline_r(vp, 0);
    VOL_UNLOCK;
}

/* The opposite of VAttachVolume.  The volume header is written to disk, with
   the inUse bit turned off.  A copy of the header is maintained in memory,
   however (which is why this is VOffline, not VDetach).
 */
void
VOffline_r(Volume * vp, char *message)
{
    Error error;
    VolumeId vid = V_id(vp);

    assert(programType != volumeUtility);
    if (!V_inUse(vp)) {
	VPutVolume_r(vp);
	return;
    }
    if (V_offlineMessage(vp)[0] == '\0')
	strncpy(V_offlineMessage(vp), message, sizeof(V_offlineMessage(vp)));
    V_offlineMessage(vp)[sizeof(V_offlineMessage(vp)) - 1] = '\0';

    vp->goingOffline = 1;
#ifdef AFS_DEMAND_ATTACH_FS
    VChangeState_r(vp, VOL_STATE_GOING_OFFLINE);
    VCreateReservation_r(vp);
    VPutVolume_r(vp);

    /* wait for the volume to go offline */
    if (V_attachState(vp) == VOL_STATE_GOING_OFFLINE) {
	VWaitStateChange_r(vp);
    }
    VCancelReservation_r(vp);
#else /* AFS_DEMAND_ATTACH_FS */
    VPutVolume_r(vp);
    vp = VGetVolume_r(&error, vid);	/* Wait for it to go offline */
    if (vp)			/* In case it was reattached... */
	VPutVolume_r(vp);
#endif /* AFS_DEMAND_ATTACH_FS */
}

void
VOffline(Volume * vp, char *message)
{
    VOL_LOCK;
    VOffline_r(vp, message);
    VOL_UNLOCK;
}

/* This gets used for the most part by utility routines that don't want
 * to keep all the volume headers around.  Generally, the file server won't
 * call this routine, because then the offline message in the volume header
 * (or other information) won't be available to clients. For NAMEI, also
 * close the file handles.  However, the fileserver does call this during
 * an attach following a volume operation.
 */
void
VDetachVolume_r(Error * ec, Volume * vp)
{
    VolumeId volume;
    struct DiskPartition *tpartp;
    int notifyServer, useDone = FSYNC_VOL_ON;

    *ec = 0;			/* always "succeeds" */
    if (programType == volumeUtility) {
	notifyServer = vp->needsPutBack;
	if (V_destroyMe(vp) == DESTROY_ME)
	    useDone = FSYNC_VOL_DONE;
#ifdef AFS_DEMAND_ATTACH_FS
	else if (!V_blessed(vp) || !V_inService(vp))
	    useDone = FSYNC_VOL_LEAVE_OFF;
#endif
    }
    tpartp = vp->partition;
    volume = V_id(vp);
    DeleteVolumeFromHashTable(vp);
    vp->shuttingDown = 1;
#ifdef AFS_DEMAND_ATTACH_FS
    DeleteVolumeFromVByPList_r(vp);
    VLRU_Delete_r(vp);
    VChangeState_r(vp, VOL_STATE_SHUTTING_DOWN);
#endif /* AFS_DEMAND_ATTACH_FS */
    VPutVolume_r(vp);
    /* Will be detached sometime in the future--this is OK since volume is offline */

    /* XXX the following code should really be moved to VCheckDetach() since the volume
     * is not technically detached until the refcounts reach zero
     */
#ifdef FSSYNC_BUILD_CLIENT
    if (programType == volumeUtility && notifyServer) {
	/* 
	 * Note:  The server is not notified in the case of a bogus volume 
	 * explicitly to make it possible to create a volume, do a partial 
	 * restore, then abort the operation without ever putting the volume 
	 * online.  This is essential in the case of a volume move operation 
	 * between two partitions on the same server.  In that case, there 
	 * would be two instances of the same volume, one of them bogus, 
	 * which the file server would attempt to put on line 
	 */
	FSYNC_VolOp(volume, tpartp->name, useDone, 0, NULL);
	/* XXX this code path is only hit by volume utilities, thus
	 * V_BreakVolumeCallbacks will always be NULL.  if we really
	 * want to break callbacks in this path we need to use FSYNC_VolOp() */
#ifdef notdef
	/* Dettaching it so break all callbacks on it */
	if (V_BreakVolumeCallbacks) {
	    Log("volume %u detached; breaking all call backs\n", volume);
	    (*V_BreakVolumeCallbacks) (volume);
	}
#endif
    }
#endif /* FSSYNC_BUILD_CLIENT */
}

void
VDetachVolume(Error * ec, Volume * vp)
{
    VOL_LOCK;
    VDetachVolume_r(ec, vp);
    VOL_UNLOCK;
}


/***************************************************/
/* Volume fd/inode handle closing routines         */
/***************************************************/

/* For VDetachVolume, we close all cached file descriptors, but keep
 * the Inode handles in case we need to read from a busy volume.
 */
/* for demand attach, caller MUST hold ref count on vp */
static void
VCloseVolumeHandles_r(Volume * vp)
{
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;

    state_save = VChangeState_r(vp, VOL_STATE_OFFLINING);
#endif

    /* demand attach fs
     *
     * XXX need to investigate whether we can perform
     * DFlushVolume outside of vol_glock_mutex... 
     *
     * VCloseVnodeFiles_r drops the glock internally */
    DFlushVolume(V_id(vp));
    VCloseVnodeFiles_r(vp);

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_UNLOCK;
#endif

    /* Too time consuming and unnecessary for the volserver */
    if (programType != volumeUtility) {
	IH_CONDSYNC(vp->vnodeIndex[vLarge].handle);
	IH_CONDSYNC(vp->vnodeIndex[vSmall].handle);
	IH_CONDSYNC(vp->diskDataHandle);
#ifdef AFS_NT40_ENV
	IH_CONDSYNC(vp->linkHandle);
#endif /* AFS_NT40_ENV */
    }

    IH_REALLYCLOSE(vp->vnodeIndex[vLarge].handle);
    IH_REALLYCLOSE(vp->vnodeIndex[vSmall].handle);
    IH_REALLYCLOSE(vp->diskDataHandle);
    IH_REALLYCLOSE(vp->linkHandle);

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_LOCK;
    VChangeState_r(vp, state_save);
#endif
}

/* For both VForceOffline and VOffline, we close all relevant handles.
 * For VOffline, if we re-attach the volume, the files may possible be
 * different than before. 
 */
/* for demand attach, caller MUST hold a ref count on vp */
static void
VReleaseVolumeHandles_r(Volume * vp)
{
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;

    state_save = VChangeState_r(vp, VOL_STATE_DETACHING);
#endif

    /* XXX need to investigate whether we can perform
     * DFlushVolume outside of vol_glock_mutex... */
    DFlushVolume(V_id(vp));

    VReleaseVnodeFiles_r(vp); /* releases the glock internally */

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_UNLOCK;
#endif

    /* Too time consuming and unnecessary for the volserver */
    if (programType != volumeUtility) {
	IH_CONDSYNC(vp->vnodeIndex[vLarge].handle);
	IH_CONDSYNC(vp->vnodeIndex[vSmall].handle);
	IH_CONDSYNC(vp->diskDataHandle);
#ifdef AFS_NT40_ENV
	IH_CONDSYNC(vp->linkHandle);
#endif /* AFS_NT40_ENV */
    }

    IH_RELEASE(vp->vnodeIndex[vLarge].handle);
    IH_RELEASE(vp->vnodeIndex[vSmall].handle);
    IH_RELEASE(vp->diskDataHandle);
    IH_RELEASE(vp->linkHandle);

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_LOCK;
    VChangeState_r(vp, state_save);
#endif
}


/***************************************************/
/* Volume write and fsync routines                 */
/***************************************************/

void
VUpdateVolume_r(Error * ec, Volume * vp, int flags)
{
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;

    if (flags & VOL_UPDATE_WAIT) {
	VCreateReservation_r(vp);
	VWaitExclusiveState_r(vp);
    }
#endif

    *ec = 0;
    if (programType == fileServer)
	V_uniquifier(vp) =
	    (V_inUse(vp) ? V_nextVnodeUnique(vp) +
	     200 : V_nextVnodeUnique(vp));

#ifdef AFS_DEMAND_ATTACH_FS
    state_save = VChangeState_r(vp, VOL_STATE_UPDATING);
    VOL_UNLOCK;
#endif

    WriteVolumeHeader_r(ec, vp);

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_LOCK;
    VChangeState_r(vp, state_save);
    if (flags & VOL_UPDATE_WAIT) {
	VCancelReservation_r(vp);
    }
#endif

    if (*ec) {
	Log("VUpdateVolume: error updating volume header, volume %u (%s)\n",
	    V_id(vp), V_name(vp));
	/* try to update on-disk header, 
	 * while preventing infinite recursion */
	if (!(flags & VOL_UPDATE_NOFORCEOFF)) {
	    VForceOffline_r(vp, VOL_FORCEOFF_NOUPDATE);
	}
    }
}

void
VUpdateVolume(Error * ec, Volume * vp)
{
    VOL_LOCK;
    VUpdateVolume_r(ec, vp, VOL_UPDATE_WAIT);
    VOL_UNLOCK;
}

void
VSyncVolume_r(Error * ec, Volume * vp, int flags)
{
    FdHandle_t *fdP;
    int code;
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;
#endif

    if (flags & VOL_SYNC_WAIT) {
	VUpdateVolume_r(ec, vp, VOL_UPDATE_WAIT);
    } else {
	VUpdateVolume_r(ec, vp, 0);
    }
    if (!*ec) {
#ifdef AFS_DEMAND_ATTACH_FS
	state_save = VChangeState_r(vp, VOL_STATE_UPDATING);
	VOL_UNLOCK;
#endif
	fdP = IH_OPEN(V_diskDataHandle(vp));
	assert(fdP != NULL);
	code = FDH_SYNC(fdP);
	assert(code == 0);
	FDH_CLOSE(fdP);
#ifdef AFS_DEMAND_ATTACH_FS
	VOL_LOCK;
	VChangeState_r(vp, state_save);
#endif
    }
}

void
VSyncVolume(Error * ec, Volume * vp)
{
    VOL_LOCK;
    VSyncVolume_r(ec, vp, VOL_SYNC_WAIT);
    VOL_UNLOCK;
}


/***************************************************/
/* Volume dealloaction routines                    */
/***************************************************/

#ifdef AFS_DEMAND_ATTACH_FS
static void
FreeVolume(Volume * vp)
{
    /* free the heap space, iff it's safe.
     * otherwise, pull it out of the hash table, so it
     * will get deallocated when all refs to it go away */
    if (!VCheckFree(vp)) {
	DeleteVolumeFromHashTable(vp);
	DeleteVolumeFromVByPList_r(vp);

	/* make sure we invalidate the header cache entry */
	FreeVolumeHeader(vp);
    }
}
#endif /* AFS_DEMAND_ATTACH_FS */

static void
ReallyFreeVolume(Volume * vp)
{
    int i;
    if (!vp)
	return;
#ifdef AFS_DEMAND_ATTACH_FS
    /* debug */
    VChangeState_r(vp, VOL_STATE_FREED);
    if (vp->pending_vol_op)
	free(vp->pending_vol_op);
#endif /* AFS_DEMAND_ATTACH_FS */
    for (i = 0; i < nVNODECLASSES; i++)
	if (vp->vnodeIndex[i].bitmap)
	    free(vp->vnodeIndex[i].bitmap);
    FreeVolumeHeader(vp);
#ifndef AFS_DEMAND_ATTACH_FS
    DeleteVolumeFromHashTable(vp);
#endif /* AFS_DEMAND_ATTACH_FS */
    free(vp);
}

/* check to see if we should shutdown this volume
 * returns 1 if volume was freed, 0 otherwise */
#ifdef AFS_DEMAND_ATTACH_FS
static int
VCheckDetach(register Volume * vp)
{
    int ret = 0;

    if (vp->nUsers || vp->nWaiters)
	return ret;

    if (vp->shuttingDown) {
	ret = 1;
	VReleaseVolumeHandles_r(vp);
	VCheckSalvage(vp);
	ReallyFreeVolume(vp);
	if (programType == fileServer) {
	    assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
	}
    }
    return ret;
}
#else /* AFS_DEMAND_ATTACH_FS */
static int
VCheckDetach(register Volume * vp)
{
    int ret = 0;

    if (vp->nUsers)
	return ret;

    if (vp->shuttingDown) {
	ret = 1;
	VReleaseVolumeHandles_r(vp);
	ReallyFreeVolume(vp);
	if (programType == fileServer) {
#if defined(AFS_PTHREAD_ENV)
	    assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
#else /* AFS_PTHREAD_ENV */
	    LWP_NoYieldSignal(VPutVolume);
#endif /* AFS_PTHREAD_ENV */
	}
    }
    return ret;
}
#endif /* AFS_DEMAND_ATTACH_FS */

/* check to see if we should offline this volume
 * return 1 if volume went offline, 0 otherwise */
#ifdef AFS_DEMAND_ATTACH_FS
static int
VCheckOffline(register Volume * vp)
{
    Volume * rvp = NULL;
    int ret = 0;

    if (vp->goingOffline && !vp->nUsers) {
	Error error;
	assert(programType == fileServer);
	assert((V_attachState(vp) != VOL_STATE_ATTACHED) &&
	       (V_attachState(vp) != VOL_STATE_FREED) &&
	       (V_attachState(vp) != VOL_STATE_PREATTACHED) &&
	       (V_attachState(vp) != VOL_STATE_UNATTACHED));

	/* valid states:
	 *
	 * VOL_STATE_GOING_OFFLINE
	 * VOL_STATE_SHUTTING_DOWN
	 * VIsErrorState(V_attachState(vp))
	 * VIsExclusiveState(V_attachState(vp))
	 */

	VCreateReservation_r(vp);
	VChangeState_r(vp, VOL_STATE_OFFLINING);

	ret = 1;
	/* must clear the goingOffline flag before we drop the glock */
	vp->goingOffline = 0;
	V_inUse(vp) = 0;

	VLRU_Delete_r(vp);

	/* perform async operations */
	VUpdateVolume_r(&error, vp, 0);
	VCloseVolumeHandles_r(vp);

	if (LogLevel) {
	    Log("VOffline: Volume %u (%s) is now offline", V_id(vp),
		V_name(vp));
	    if (V_offlineMessage(vp)[0])
		Log(" (%s)", V_offlineMessage(vp));
	    Log("\n");
	}

	/* invalidate the volume header cache entry */
	FreeVolumeHeader(vp);

	/* if nothing changed state to error or salvaging,
	 * drop state to unattached */
	if (!VIsErrorState(V_attachState(vp))) {
	    VChangeState_r(vp, VOL_STATE_UNATTACHED);
	}
	VCancelReservation_r(vp);
	/* no usage of vp is safe beyond this point */
    }
    return ret;
}
#else /* AFS_DEMAND_ATTACH_FS */
static int
VCheckOffline(register Volume * vp)
{
    Volume * rvp = NULL;
    int ret = 0;

    if (vp->goingOffline && !vp->nUsers) {
	Error error;
	assert(programType == fileServer);

	ret = 1;
	vp->goingOffline = 0;
	V_inUse(vp) = 0;
	VUpdateVolume_r(&error, vp, 0);
	VCloseVolumeHandles_r(vp);
	if (LogLevel) {
	    Log("VOffline: Volume %u (%s) is now offline", V_id(vp),
		V_name(vp));
	    if (V_offlineMessage(vp)[0])
		Log(" (%s)", V_offlineMessage(vp));
	    Log("\n");
	}
	FreeVolumeHeader(vp);
#ifdef AFS_PTHREAD_ENV
	assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
#else /* AFS_PTHREAD_ENV */
	LWP_NoYieldSignal(VPutVolume);
#endif /* AFS_PTHREAD_ENV */
    }
    return ret;
}
#endif /* AFS_DEMAND_ATTACH_FS */

/***************************************************/
/* demand attach fs ref counting routines          */
/***************************************************/

#ifdef AFS_DEMAND_ATTACH_FS
/* the following two functions handle reference counting for
 * asynchronous operations on volume structs.
 *
 * their purpose is to prevent a VDetachVolume or VShutdown
 * from free()ing the Volume struct during an async i/o op */

/* register with the async volume op ref counter */
/* VCreateReservation_r moved into inline code header because it 
 * is now needed in vnode.c -- tkeiser 11/20/2007 
 */

/**
 * decrement volume-package internal refcount.
 *
 * @param vp  volume object pointer
 *
 * @internal volume package internal use only
 *
 * @pre 
 *    @arg VOL_LOCK is held
 *    @arg lightweight refcount held
 *
 * @post volume waiters refcount is decremented; volume may
 *       have been deallocated/shutdown/offlined/salvaged/
 *       whatever during the process
 *
 * @warning once you have tossed your last reference (you can acquire
 *          lightweight refs recursively) it is NOT SAFE to reference
 *          a volume object pointer ever again
 *
 * @see VCreateReservation_r
 *
 * @note DEMAND_ATTACH_FS only
 */
void
VCancelReservation_r(Volume * vp)
{
    assert(--vp->nWaiters >= 0);
    if (vp->nWaiters == 0) {
	VCheckOffline(vp);
	if (!VCheckDetach(vp)) {
	    VCheckSalvage(vp);
	    VCheckFree(vp);
	}
    }
}

/* check to see if we should free this volume now
 * return 1 if volume was freed, 0 otherwise */
static int
VCheckFree(Volume * vp)
{
    int ret = 0;
    if ((vp->nUsers == 0) &&
	(vp->nWaiters == 0) &&
	!(V_attachFlags(vp) & (VOL_IN_HASH | 
			       VOL_ON_VBYP_LIST | 
			       VOL_IS_BUSY |
			       VOL_ON_VLRU))) {
	ReallyFreeVolume(vp);
	ret = 1;
    }
    return ret;
}
#endif /* AFS_DEMAND_ATTACH_FS */


/***************************************************/
/* online volume operations routines               */
/***************************************************/

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * register a volume operation on a given volume.
 *
 * @param[in] vp       volume object
 * @param[in] vopinfo  volume operation info object
 *
 * @pre VOL_LOCK is held
 *
 * @post volume operation info object attached to volume object.
 *       volume operation statistics updated.
 *
 * @note by "attached" we mean a copy of the passed in object is made
 *
 * @internal volume package internal use only
 */
int
VRegisterVolOp_r(Volume * vp, FSSYNC_VolOp_info * vopinfo)
{
    FSSYNC_VolOp_info * info;

    /* attach a vol op info node to the volume struct */
    info = (FSSYNC_VolOp_info *) malloc(sizeof(FSSYNC_VolOp_info));
    assert(info != NULL);
    memcpy(info, vopinfo, sizeof(FSSYNC_VolOp_info));
    vp->pending_vol_op = info;

    /* update stats */
    vp->stats.last_vol_op = FT_ApproxTime();
    vp->stats.vol_ops++;
    IncUInt64(&VStats.vol_ops);

    return 0;
}

/**
 * deregister the volume operation attached to this volume.
 *
 * @param[in] vp  volume object pointer
 *
 * @pre VOL_LOCK is held
 *
 * @post the volume operation info object is detached from the volume object
 *
 * @internal volume package internal use only
 */
int
VDeregisterVolOp_r(Volume * vp)
{
    if (vp->pending_vol_op) {
	free(vp->pending_vol_op);
	vp->pending_vol_op = NULL;
    }
    return 0;
}
#endif /* AFS_DEMAND_ATTACH_FS */

/**
 * determine whether it is safe to leave a volume online during
 * the volume operation described by the vopinfo object.
 *
 * @param[in] vp        volume object
 * @param[in] vopinfo   volume operation info object
 *
 * @return whether it is safe to leave volume online
 *    @retval 0  it is NOT SAFE to leave the volume online
 *    @retval 1  it is safe to leave the volume online during the operation
 *
 * @pre
 *    @arg VOL_LOCK is held
 *    @arg disk header attached to vp (heavyweight ref on vp will guarantee
 *         this condition is met)
 *
 * @internal volume package internal use only
 */
int
VVolOpLeaveOnline_r(Volume * vp, FSSYNC_VolOp_info * vopinfo)
{
    return (vopinfo->com.command == FSYNC_VOL_NEEDVOLUME &&
	    (vopinfo->com.reason == V_READONLY ||
	     (!VolumeWriteable(vp) &&
	      (vopinfo->com.reason == V_CLONE ||
	       vopinfo->com.reason == V_DUMP))));
}

/**
 * determine whether VBUSY should be set during this volume operation.
 *
 * @param[in] vp        volume object
 * @param[in] vopinfo   volume operation info object
 *
 * @return whether VBUSY should be set
 *   @retval 0  VBUSY does NOT need to be set
 *   @retval 1  VBUSY SHOULD be set
 *
 * @pre VOL_LOCK is held
 *
 * @internal volume package internal use only
 */
int
VVolOpSetVBusy_r(Volume * vp, FSSYNC_VolOp_info * vopinfo)
{
    return (vopinfo->com.command == FSYNC_VOL_NEEDVOLUME &&
	    (vopinfo->com.reason == V_CLONE ||
	     vopinfo->com.reason == V_DUMP));
}


/***************************************************/
/* online salvager routines                        */
/***************************************************/
#if defined(AFS_DEMAND_ATTACH_FS)
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
 * check whether a salvage needs to be performed on this volume.
 *
 * @param[in] vp   pointer to volume object
 *
 * @return status code
 *    @retval 0 no salvage scheduled
 *    @retval 1 a salvage has been scheduled with the salvageserver
 *
 * @pre VOL_LOCK is held
 *
 * @post if salvage request flag is set and nUsers and nWaiters are zero,
 *       then a salvage will be requested
 *
 * @note this is one of the event handlers called by VCancelReservation_r
 *
 * @see VCancelReservation_r
 *
 * @internal volume package internal use only.
 */
static int
VCheckSalvage(register Volume * vp)
{
    int ret = 0;
#ifdef SALVSYNC_BUILD_CLIENT
    if (vp->nUsers || vp->nWaiters)
	return ret;
    if (vp->salvage.requested) {
	VScheduleSalvage_r(vp);
	ret = 1;
    }
#endif /* SALVSYNC_BUILD_CLIENT */
    return ret;
}

/**
 * request volume salvage.
 *
 * @param[out] ec      computed client error code
 * @param[in]  vp      volume object pointer
 * @param[in]  reason  reason code (passed to salvageserver via SALVSYNC)
 * @param[in]  flags   see flags note below
 *
 * @note flags:
 *       VOL_SALVAGE_INVALIDATE_HEADER causes volume header cache entry 
 *                                     to be invalidated.
 *
 * @pre VOL_LOCK is held.
 *
 * @post volume state is changed.
 *       for fileserver, salvage will be requested once refcount reaches zero.
 *
 * @return operation status code
 *   @retval 0  volume salvage will occur
 *   @retval 1  volume salvage could not be scheduled
 *
 * @note DAFS fileserver only
 *
 * @note this call does not synchronously schedule a volume salvage.  rather,
 *       it sets volume state so that when volume refcounts reach zero, a
 *       volume salvage will occur.  by "refcounts", we mean both nUsers and 
 *       nWaiters must be zero.
 *
 * @internal volume package internal use only.
 */
int
VRequestSalvage_r(Error * ec, Volume * vp, int reason, int flags)
{
    int code = 0;
    /*
     * for DAFS volume utilities, transition to error state
     * (at some point in the future, we should consider
     *  making volser talk to salsrv)
     */
    if (programType != fileServer) {
	VChangeState_r(vp, VOL_STATE_ERROR);
	*ec = VSALVAGE;
	return 1;
    }

    if (!vp->salvage.requested) {
	vp->salvage.requested = 1;
	vp->salvage.reason = reason;
	vp->stats.last_salvage = FT_ApproxTime();
	if (flags & VOL_SALVAGE_INVALIDATE_HEADER) {
	    /* XXX this should likely be changed to FreeVolumeHeader() */
	    ReleaseVolumeHeader(vp->header);
	}
	if (vp->stats.salvages < SALVAGE_COUNT_MAX) {
	    VChangeState_r(vp, VOL_STATE_SALVAGING);
	    *ec = VSALVAGING;
	} else {
	    Log("VRequestSalvage: volume %u online salvaged too many times; forced offline.\n", vp->hashid);
	    VChangeState_r(vp, VOL_STATE_ERROR);
	    *ec = VSALVAGE;
	    code = 1;
	}
    }
    return code;
}

/**
 * update salvageserver scheduling priority for a volume.
 *
 * @param[in] vp  pointer to volume object
 *
 * @return operation status
 *   @retval 0  success
 *   @retval 1  request denied, or SALVSYNC communications failure
 *
 * @pre VOL_LOCK is held.
 *
 * @post in-core salvage priority counter is incremented.  if at least
 *       SALVAGE_PRIO_UPDATE_INTERVAL seconds have elapsed since the
 *       last SALVSYNC_RAISEPRIO request, we contact the salvageserver
 *       to update its priority queue.  if no salvage is scheduled,
 *       this function is a no-op.
 *
 * @note DAFS fileserver only
 *
 * @note this should be called whenever a VGetVolume fails due to a 
 *       pending salvage request
 *
 * @todo should set exclusive state and drop glock around salvsync call
 *
 * @internal volume package internal use only.
 */
static int
VUpdateSalvagePriority_r(Volume * vp)
{
    int code, ret=0;
    afs_uint32 now;

#ifdef SALVSYNC_BUILD_CLIENT
    vp->salvage.prio++;
    now = FT_ApproxTime();

    /* update the salvageserver priority queue occasionally so that
     * frequently requested volumes get moved to the head of the queue 
     */
    if ((vp->salvage.scheduled) &&
	(vp->stats.last_salvage_req < (now-SALVAGE_PRIO_UPDATE_INTERVAL))) {
	code = SALVSYNC_SalvageVolume(vp->hashid,
				      VPartitionPath(vp->partition),
				      SALVSYNC_RAISEPRIO,
				      vp->salvage.reason,
				      vp->salvage.prio,
				      NULL);
	vp->stats.last_salvage_req = now;
	if (code != SYNC_OK) {
	    ret = 1;
	}
    }
#endif /* SALVSYNC_BUILD_CLIENT */
    return ret;
}


/**
 * schedule a salvage with the salvage server.
 *
 * @param[in] vp  pointer to volume object
 *
 * @return operation status
 *    @retval 0 salvage scheduled successfully
 *    @retval 1 salvage not scheduled, or SALVSYNC com error
 *
 * @pre 
 *    @arg VOL_LOCK is held.
 *    @arg nUsers and nWaiters should be zero.
 *
 * @post salvageserver is sent a salvage request
 *
 * @note DAFS fileserver only
 *
 * @internal volume package internal use only.
 */
static int
VScheduleSalvage_r(Volume * vp)
{
    int code, ret=0;
#ifdef SALVSYNC_BUILD_CLIENT
    VolState state_save;
    char partName[16];

    if (vp->nWaiters || vp->nUsers) {
	return 1;
    }

    /* prevent endless salvage,attach,salvage,attach,... loops */
    if (vp->stats.salvages >= SALVAGE_COUNT_MAX)
	return 1;

    if (!vp->salvage.scheduled) {
	/* if we haven't previously scheduled a salvage, do so now 
	 *
	 * set the volume to an exclusive state and drop the lock
	 * around the SALVSYNC call
	 *
	 * note that we do NOT acquire a reservation here -- doing so
	 * could result in unbounded recursion
	 */
	strlcpy(partName, VPartitionPath(vp->partition), sizeof(partName));
	state_save = VChangeState_r(vp, VOL_STATE_SALVSYNC_REQ);
	V_attachFlags(vp) |= VOL_IS_BUSY;
	VOL_UNLOCK;

	/* can't use V_id() since there's no guarantee
	 * we have the disk data header at this point */
	code = SALVSYNC_SalvageVolume(vp->hashid,
				      partName,
				      SALVSYNC_SALVAGE,
				      vp->salvage.reason,
				      vp->salvage.prio,
				      NULL);
	VOL_LOCK;
	VChangeState_r(vp, state_save);
	V_attachFlags(vp) &= ~(VOL_IS_BUSY);

	if (code == SYNC_OK) {
	    vp->salvage.scheduled = 1;
	    vp->stats.salvages++;
	    vp->stats.last_salvage_req = FT_ApproxTime();
	    IncUInt64(&VStats.salvages);
	} else {
	    ret = 1;
	    switch(code) {
	    case SYNC_BAD_COMMAND:
	    case SYNC_COM_ERROR:
		break;
	    case SYNC_DENIED:
		Log("VScheduleSalvage_r:  SALVSYNC request denied\n");
		break;
	    default:
		Log("VScheduleSalvage_r:  SALVSYNC unknown protocol error\n");
		break;
	    }
	}
    }
#endif /* SALVSYNC_BUILD_CLIENT */
    return ret;
}

/**
 * ask salvageserver to cancel a scheduled salvage operation.
 *
 * @param[in] vp      pointer to volume object
 * @param[in] reason  SALVSYNC protocol reason code
 *
 * @return operation status
 *    @retval 0 success
 *    @retval 1 request failed
 *
 * @pre VOL_LOCK is held.
 *
 * @post salvageserver is sent a request to cancel the volume salvage
 *
 * @todo should set exclusive state and drop glock around salvsync call
 *
 * @internal volume package internal use only.
 */
static int
VCancelSalvage_r(Volume * vp, int reason)
{
    int code, ret = 0;

#ifdef SALVSYNC_BUILD_CLIENT
    if (vp->salvage.scheduled) {
	code = SALVSYNC_SalvageVolume(vp->hashid,
				      VPartitionPath(vp->partition),
				      SALVSYNC_CANCEL,
				      reason,
				      0,
				      NULL);
	if (code == SYNC_OK) {
	    vp->salvage.scheduled = 0;
	} else {
	    ret = 1;
	}
    }
#endif /* SALVSYNC_BUILD_CLIENT */
    return ret;
}


#ifdef SALVSYNC_BUILD_CLIENT
/**
 * connect to the salvageserver SYNC service.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @post connection to salvageserver SYNC service established
 *
 * @see VConnectSALV_r
 * @see VDisconnectSALV
 * @see VReconnectSALV
 */
int
VConnectSALV(void)
{
    int retVal;
    VOL_LOCK;
    retVal = VConnectSALV_r();
    VOL_UNLOCK;
    return retVal;
}

/**
 * connect to the salvageserver SYNC service.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre VOL_LOCK is held.
 *
 * @post connection to salvageserver SYNC service established
 *
 * @see VConnectSALV
 * @see VDisconnectSALV_r
 * @see VReconnectSALV_r
 * @see SALVSYNC_clientInit
 *
 * @internal volume package internal use only.
 */
int
VConnectSALV_r(void)
{
    return SALVSYNC_clientInit();
}

/**
 * disconnect from the salvageserver SYNC service.
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre client should have a live connection to the salvageserver
 *
 * @post connection to salvageserver SYNC service destroyed
 *
 * @see VDisconnectSALV_r
 * @see VConnectSALV
 * @see VReconnectSALV
 */
int
VDisconnectSALV(void)
{
    int retVal;
    VOL_LOCK;
    VDisconnectSALV_r();
    VOL_UNLOCK;
    return retVal;
}

/**
 * disconnect from the salvageserver SYNC service.
 *
 * @return operation status
 *    @retval 0 success
 *
 * @pre 
 *    @arg VOL_LOCK is held.
 *    @arg client should have a live connection to the salvageserver.
 *
 * @post connection to salvageserver SYNC service destroyed
 *
 * @see VDisconnectSALV
 * @see VConnectSALV_r
 * @see VReconnectSALV_r
 * @see SALVSYNC_clientFinis
 *
 * @internal volume package internal use only.
 */
int
VDisconnectSALV_r(void)
{ 
    return SALVSYNC_clientFinis();
}

/**
 * disconnect and then re-connect to the salvageserver SYNC service.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre client should have a live connection to the salvageserver
 *
 * @post old connection is dropped, and a new one is established
 *
 * @see VConnectSALV
 * @see VDisconnectSALV
 * @see VReconnectSALV_r
 */
int
VReconnectSALV(void)
{
    int retVal;
    VOL_LOCK;
    retVal = VReconnectSALV_r();
    VOL_UNLOCK;
    return retVal;
}

/**
 * disconnect and then re-connect to the salvageserver SYNC service.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre 
 *    @arg VOL_LOCK is held.
 *    @arg client should have a live connection to the salvageserver.
 *
 * @post old connection is dropped, and a new one is established
 *
 * @see VConnectSALV_r
 * @see VDisconnectSALV
 * @see VReconnectSALV
 * @see SALVSYNC_clientReconnect
 *
 * @internal volume package internal use only.
 */
int
VReconnectSALV_r(void)
{
    return SALVSYNC_clientReconnect();
}
#endif /* SALVSYNC_BUILD_CLIENT */
#endif /* AFS_DEMAND_ATTACH_FS */


/***************************************************/
/* FSSYNC routines                                 */
/***************************************************/

/* This must be called by any volume utility which needs to run while the
   file server is also running.  This is separated from VInitVolumePackage so
   that a utility can fork--and each of the children can independently
   initialize communication with the file server */
#ifdef FSSYNC_BUILD_CLIENT
/**
 * connect to the fileserver SYNC service.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre 
 *    @arg VInit must equal 2.
 *    @arg Program Type must not be fileserver or salvager.
 *
 * @post connection to fileserver SYNC service established
 *
 * @see VConnectFS_r
 * @see VDisconnectFS
 * @see VChildProcReconnectFS
 */
int
VConnectFS(void)
{
    int retVal;
    VOL_LOCK;
    retVal = VConnectFS_r();
    VOL_UNLOCK;
    return retVal;
}

/**
 * connect to the fileserver SYNC service.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre 
 *    @arg VInit must equal 2.
 *    @arg Program Type must not be fileserver or salvager.
 *    @arg VOL_LOCK is held.
 *
 * @post connection to fileserver SYNC service established
 *
 * @see VConnectFS
 * @see VDisconnectFS_r
 * @see VChildProcReconnectFS_r
 *
 * @internal volume package internal use only.
 */
int
VConnectFS_r(void)
{
    int rc;
    assert((VInit == 2) && 
	   (programType != fileServer) &&
	   (programType != salvager));
    rc = FSYNC_clientInit();
    if (rc)
	VInit = 3;
    return rc;
}

/**
 * disconnect from the fileserver SYNC service.
 *
 * @pre 
 *    @arg client should have a live connection to the fileserver.
 *    @arg VOL_LOCK is held.
 *    @arg Program Type must not be fileserver or salvager.
 *
 * @post connection to fileserver SYNC service destroyed
 *
 * @see VDisconnectFS
 * @see VConnectFS_r
 * @see VChildProcReconnectFS_r
 *
 * @internal volume package internal use only.
 */
void
VDisconnectFS_r(void)
{
    assert((programType != fileServer) &&
	   (programType != salvager));
    FSYNC_clientFinis();
    VInit = 2;
}

/**
 * disconnect from the fileserver SYNC service.
 *
 * @pre
 *    @arg client should have a live connection to the fileserver.
 *    @arg Program Type must not be fileserver or salvager.
 *
 * @post connection to fileserver SYNC service destroyed
 *
 * @see VDisconnectFS_r
 * @see VConnectFS
 * @see VChildProcReconnectFS
 */
void
VDisconnectFS(void)
{
    VOL_LOCK;
    VDisconnectFS_r();
    VOL_UNLOCK;
}

/**
 * connect to the fileserver SYNC service from a child process following a fork.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre
 *    @arg VOL_LOCK is held.
 *    @arg current FSYNC handle is shared with a parent process
 *
 * @post current FSYNC handle is discarded and a new connection to the
 *       fileserver SYNC service is established
 *
 * @see VChildProcReconnectFS
 * @see VConnectFS_r
 * @see VDisconnectFS_r
 *
 * @internal volume package internal use only.
 */
int
VChildProcReconnectFS_r(void)
{
    return FSYNC_clientChildProcReconnect();
}

/**
 * connect to the fileserver SYNC service from a child process following a fork.
 *
 * @return operation status
 *    @retval 0 failure
 *    @retval 1 success
 *
 * @pre current FSYNC handle is shared with a parent process
 *
 * @post current FSYNC handle is discarded and a new connection to the
 *       fileserver SYNC service is established
 *
 * @see VChildProcReconnectFS_r
 * @see VConnectFS
 * @see VDisconnectFS
 */
int
VChildProcReconnectFS(void)
{
    int ret;
    VOL_LOCK;
    ret = VChildProcReconnectFS_r();
    VOL_UNLOCK;
    return ret;
}
#endif /* FSSYNC_BUILD_CLIENT */


/***************************************************/
/* volume bitmap routines                          */
/***************************************************/

/*
 * For demand attach fs, flags parameter controls
 * locking behavior.  If (flags & VOL_ALLOC_BITMAP_WAIT)
 * is set, then this function will create a reservation
 * and block on any other exclusive operations.  Otherwise,
 * this function assumes the caller already has exclusive
 * access to vp, and we just change the volume state.
 */
VnodeId
VAllocBitmapEntry_r(Error * ec, Volume * vp, 
		    struct vnodeIndex *index, int flags)
{
    VnodeId ret;
    register byte *bp, *ep;
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;
#endif /* AFS_DEMAND_ATTACH_FS */

    *ec = 0;

    /* This test is probably redundant */
    if (!VolumeWriteable(vp)) {
	*ec = (bit32) VREADONLY;
	return 0;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    if (flags & VOL_ALLOC_BITMAP_WAIT) {
	VCreateReservation_r(vp);
	VWaitExclusiveState_r(vp);
    }
    state_save = VChangeState_r(vp, VOL_STATE_GET_BITMAP);
#endif /* AFS_DEMAND_ATTACH_FS */

#ifdef BITMAP_LATER
    if ((programType == fileServer) && !index->bitmap) {
	int i;
#ifndef AFS_DEMAND_ATTACH_FS
	/* demand attach fs uses the volume state to avoid races.
	 * specialStatus field is not used at all */
	int wasVBUSY = 0;
	if (vp->specialStatus == VBUSY) {
	    if (vp->goingOffline) {	/* vos dump waiting for the volume to
					 * go offline. We probably come here
					 * from AddNewReadableResidency */
		wasVBUSY = 1;
	    } else {
		while (vp->specialStatus == VBUSY) {
#ifdef AFS_PTHREAD_ENV
		    VOL_UNLOCK;
		    sleep(2);
		    VOL_LOCK;
#else /* !AFS_PTHREAD_ENV */
		    IOMGR_Sleep(2);
#endif /* !AFS_PTHREAD_ENV */
		}
	    }
	}
#endif /* !AFS_DEMAND_ATTACH_FS */

	if (!index->bitmap) {
#ifndef AFS_DEMAND_ATTACH_FS
	    vp->specialStatus = VBUSY;	/* Stop anyone else from using it. */
#endif /* AFS_DEMAND_ATTACH_FS */
	    for (i = 0; i < nVNODECLASSES; i++) {
		VGetBitmap_r(ec, vp, i);
		if (*ec) {
#ifdef AFS_DEMAND_ATTACH_FS
		    VRequestSalvage_r(ec, vp, SALVSYNC_ERROR, VOL_SALVAGE_INVALIDATE_HEADER);
#else /* AFS_DEMAND_ATTACH_FS */
		    DeleteVolumeFromHashTable(vp);
		    vp->shuttingDown = 1;	/* Let who has it free it. */
		    vp->specialStatus = 0;
#endif /* AFS_DEMAND_ATTACH_FS */
		    ret = NULL;
		    goto done;
		}
	    }
#ifndef AFS_DEMAND_ATTACH_FS
	    if (!wasVBUSY)
		vp->specialStatus = 0;	/* Allow others to have access. */
#endif /* AFS_DEMAND_ATTACH_FS */
	}
    }
#endif /* BITMAP_LATER */

#ifdef AFS_DEMAND_ATTACH_FS
    VOL_UNLOCK;
#endif /* AFS_DEMAND_ATTACH_FS */
    bp = index->bitmap + index->bitmapOffset;
    ep = index->bitmap + index->bitmapSize;
    while (bp < ep) {
	if ((*(bit32 *) bp) != (bit32) 0xffffffff) {
	    int o;
	    index->bitmapOffset = (afs_uint32) (bp - index->bitmap);
	    while (*bp == 0xff)
		bp++;
	    o = ffs(~*bp) - 1;	/* ffs is documented in BSTRING(3) */
	    *bp |= (1 << o);
	    ret = (VnodeId) ((bp - index->bitmap) * 8 + o);
#ifdef AFS_DEMAND_ATTACH_FS
	    VOL_LOCK;
#endif /* AFS_DEMAND_ATTACH_FS */
	    goto done;
	}
	bp += sizeof(bit32) /* i.e. 4 */ ;
    }
    /* No bit map entry--must grow bitmap */
    bp = (byte *)
	realloc(index->bitmap, index->bitmapSize + VOLUME_BITMAP_GROWSIZE);
    assert(bp != NULL);
    index->bitmap = bp;
    bp += index->bitmapSize;
    memset(bp, 0, VOLUME_BITMAP_GROWSIZE);
    index->bitmapOffset = index->bitmapSize;
    index->bitmapSize += VOLUME_BITMAP_GROWSIZE;
    *bp = 1;
    ret = index->bitmapOffset * 8;
#ifdef AFS_DEMAND_ATTACH_FS
    VOL_LOCK;
#endif /* AFS_DEMAND_ATTACH_FS */

 done:
#ifdef AFS_DEMAND_ATTACH_FS
    VChangeState_r(vp, state_save);
    if (flags & VOL_ALLOC_BITMAP_WAIT) {
	VCancelReservation_r(vp);
    }
#endif /* AFS_DEMAND_ATTACH_FS */
    return ret;
}

VnodeId
VAllocBitmapEntry(Error * ec, Volume * vp, register struct vnodeIndex * index)
{
    VnodeId retVal;
    VOL_LOCK;
    retVal = VAllocBitmapEntry_r(ec, vp, index, VOL_ALLOC_BITMAP_WAIT);
    VOL_UNLOCK;
    return retVal;
}

void
VFreeBitMapEntry_r(Error * ec, register struct vnodeIndex *index,
		   unsigned bitNumber)
{
    unsigned int offset;

    *ec = 0;
#ifdef BITMAP_LATER
    if (!index->bitmap)
	return;
#endif /* BITMAP_LATER */
    offset = bitNumber >> 3;
    if (offset >= index->bitmapSize) {
	*ec = VNOVNODE;
	return;
    }
    if (offset < index->bitmapOffset)
	index->bitmapOffset = offset & ~3;	/* Truncate to nearest bit32 */
    *(index->bitmap + offset) &= ~(1 << (bitNumber & 0x7));
}

void
VFreeBitMapEntry(Error * ec, register struct vnodeIndex *index,
		 unsigned bitNumber)
{
    VOL_LOCK;
    VFreeBitMapEntry_r(ec, index, bitNumber);
    VOL_UNLOCK;
}

/* this function will drop the glock internally.
 * for old pthread fileservers, this is safe thanks to vbusy.
 *
 * for demand attach fs, caller must have already called
 * VCreateReservation_r and VWaitExclusiveState_r */
static void
VGetBitmap_r(Error * ec, Volume * vp, VnodeClass class)
{
    StreamHandle_t *file;
    int nVnodes;
    int size;
    struct VnodeClassInfo *vcp = &VnodeClassInfo[class];
    struct vnodeIndex *vip = &vp->vnodeIndex[class];
    struct VnodeDiskObject *vnode;
    unsigned int unique = 0;
    FdHandle_t *fdP;
#ifdef BITMAP_LATER
    byte *BitMap = 0;
#endif /* BITMAP_LATER */
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;
#endif /* AFS_DEMAND_ATTACH_FS */

    *ec = 0;

#ifdef AFS_DEMAND_ATTACH_FS
    state_save = VChangeState_r(vp, VOL_STATE_GET_BITMAP);
#endif /* AFS_DEMAND_ATTACH_FS */
    VOL_UNLOCK;

    fdP = IH_OPEN(vip->handle);
    assert(fdP != NULL);
    file = FDH_FDOPEN(fdP, "r");
    assert(file != NULL);
    vnode = (VnodeDiskObject *) malloc(vcp->diskSize);
    assert(vnode != NULL);
    size = OS_SIZE(fdP->fd_fd);
    assert(size != -1);
    nVnodes = (size <= vcp->diskSize ? 0 : size - vcp->diskSize)
	>> vcp->logSize;
    vip->bitmapSize = ((nVnodes / 8) + 10) / 4 * 4;	/* The 10 is a little extra so
							 * a few files can be created in this volume,
							 * the whole thing is rounded up to nearest 4
							 * bytes, because the bit map allocator likes
							 * it that way */
#ifdef BITMAP_LATER
    BitMap = (byte *) calloc(1, vip->bitmapSize);
    assert(BitMap != NULL);
#else /* BITMAP_LATER */
    vip->bitmap = (byte *) calloc(1, vip->bitmapSize);
    assert(vip->bitmap != NULL);
    vip->bitmapOffset = 0;
#endif /* BITMAP_LATER */
    if (STREAM_SEEK(file, vcp->diskSize, 0) != -1) {
	int bitNumber = 0;
	for (bitNumber = 0; bitNumber < nVnodes + 100; bitNumber++) {
	    if (STREAM_READ(vnode, vcp->diskSize, 1, file) != 1)
		break;
	    if (vnode->type != vNull) {
		if (vnode->vnodeMagic != vcp->magic) {
		    Log("GetBitmap: addled vnode index in volume %s; volume needs salvage\n", V_name(vp));
		    *ec = VSALVAGE;
		    break;
		}
#ifdef BITMAP_LATER
		*(BitMap + (bitNumber >> 3)) |= (1 << (bitNumber & 0x7));
#else /* BITMAP_LATER */
		*(vip->bitmap + (bitNumber >> 3)) |= (1 << (bitNumber & 0x7));
#endif /* BITMAP_LATER */
		if (unique <= vnode->uniquifier)
		    unique = vnode->uniquifier + 1;
	    }
#ifndef AFS_PTHREAD_ENV
	    if ((bitNumber & 0x00ff) == 0x0ff) {	/* every 256 iterations */
		IOMGR_Poll();
	    }
#endif /* !AFS_PTHREAD_ENV */
	}
    }
    if (vp->nextVnodeUnique < unique) {
	Log("GetBitmap: bad volume uniquifier for volume %s; volume needs salvage\n", V_name(vp));
	*ec = VSALVAGE;
    }
    /* Paranoia, partly justified--I think fclose after fdopen
     * doesn't seem to close fd.  In any event, the documentation
     * doesn't specify, so it's safer to close it twice.
     */
    STREAM_CLOSE(file);
    FDH_CLOSE(fdP);
    free(vnode);

    VOL_LOCK;
#ifdef BITMAP_LATER
    /* There may have been a racing condition with some other thread, both
     * creating the bitmaps for this volume. If the other thread was faster
     * the pointer to bitmap should already be filled and we can free ours.
     */
    if (vip->bitmap == NULL) {
	vip->bitmap = BitMap;
	vip->bitmapOffset = 0;
    } else
	free((byte *) BitMap);
#endif /* BITMAP_LATER */
#ifdef AFS_DEMAND_ATTACH_FS
    VChangeState_r(vp, state_save);
#endif /* AFS_DEMAND_ATTACH_FS */
}


/***************************************************/
/* Volume Path and Volume Number utility routines  */
/***************************************************/

/**
 * find the first occurrence of a volume header file and return the path.
 *
 * @param[out] ec          outbound error code
 * @param[in]  volumeId    volume id to find
 * @param[out] partitionp  pointer to disk partition path string
 * @param[out] namep       pointer to volume header file name string
 *
 * @post path to first occurrence of volume header is returned in partitionp
 *       and namep, or ec is set accordingly.
 *
 * @warning this function is NOT re-entrant -- partitionp and namep point to
 *          static data segments
 *
 * @note if a volume utility inadvertently leaves behind a stale volume header
 *       on a vice partition, it is possible for callers to get the wrong one,
 *       depending on the order of the disk partition linked list.
 *
 * @internal volume package internal use only.
 */
static void
GetVolumePath(Error * ec, VolId volumeId, char **partitionp, char **namep)
{
    static char partition[VMAXPATHLEN], name[VMAXPATHLEN];
    char path[VMAXPATHLEN];
    int found = 0;
    struct DiskPartition *dp;

    *ec = 0;
    name[0] = '/';
    (void)afs_snprintf(&name[1], (sizeof name) - 1, VFORMAT, volumeId);
    for (dp = DiskPartitionList; dp; dp = dp->next) {
	struct afs_stat status;
	strcpy(path, VPartitionPath(dp));
	strcat(path, name);
	if (afs_stat(path, &status) == 0) {
	    strcpy(partition, dp->name);
	    found = 1;
	    break;
	}
    }
    if (!found) {
	*ec = VNOVOL;
	*partitionp = *namep = NULL;
    } else {
	*partitionp = partition;
	*namep = name;
    }
}

/**
 * extract a volume number from a volume header filename string.
 *
 * @param[in] name  volume header filename string
 *
 * @return volume number
 *
 * @note the string must be of the form VFORMAT.  the only permissible
 *       deviation is a leading '/' character.
 *
 * @see VFORMAT
 */
int
VolumeNumber(char *name)
{
    if (*name == '/')
	name++;
    return atoi(name + 1);
}

/**
 * compute the volume header filename.
 *
 * @param[in] volumeId
 *
 * @return volume header filename
 *
 * @post volume header filename string is constructed
 *
 * @warning this function is NOT re-entrant -- the returned string is
 *          stored in a static char array.  see VolumeExternalName_r
 *          for a re-entrant equivalent.
 *
 * @see VolumeExternalName_r
 *
 * @deprecated due to the above re-entrancy warning, this interface should
 *             be considered deprecated.  Please use VolumeExternalName_r
 *             in its stead.
 */
char *
VolumeExternalName(VolumeId volumeId)
{
    static char name[VMAXPATHLEN];
    (void)afs_snprintf(name, sizeof name, VFORMAT, volumeId);
    return name;
}

/**
 * compute the volume header filename.
 *
 * @param[in]     volumeId
 * @param[inout]  name       array in which to store filename
 * @param[in]     len        length of name array
 *
 * @return result code from afs_snprintf
 *
 * @see VolumeExternalName
 * @see afs_snprintf
 *
 * @note re-entrant equivalent of VolumeExternalName
 *
 * @internal volume package internal use only.
 */
static int
VolumeExternalName_r(VolumeId volumeId, char * name, size_t len)
{
    return afs_snprintf(name, len, VFORMAT, volumeId);
}


/***************************************************/
/* Volume Usage Statistics routines                */
/***************************************************/

#if OPENAFS_VOL_STATS
#define OneDay	(86400)		/* 24 hours' worth of seconds */
#else
#define OneDay	(24*60*60)	/* 24 hours */
#endif /* OPENAFS_VOL_STATS */

#define Midnight(date) ((date-TimeZoneCorrection)/OneDay*OneDay+TimeZoneCorrection)

/*------------------------------------------------------------------------
 * [export] VAdjustVolumeStatistics
 *
 * Description:
 *	If we've passed midnight, we need to update all the day use
 *	statistics as well as zeroing the detailed volume statistics
 *	(if we are implementing them).
 *
 * Arguments:
 *	vp : Pointer to the volume structure describing the lucky
 *		volume being considered for update.
 *
 * Returns:
 *	0 (always!)
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As described.
 *------------------------------------------------------------------------*/

int
VAdjustVolumeStatistics_r(register Volume * vp)
{
    unsigned int now = FT_ApproxTime();

    if (now - V_dayUseDate(vp) > OneDay) {
	register int ndays, i;

	ndays = (now - V_dayUseDate(vp)) / OneDay;
	for (i = 6; i > ndays - 1; i--)
	    V_weekUse(vp)[i] = V_weekUse(vp)[i - ndays];
	for (i = 0; i < ndays - 1 && i < 7; i++)
	    V_weekUse(vp)[i] = 0;
	if (ndays <= 7)
	    V_weekUse(vp)[ndays - 1] = V_dayUse(vp);
	V_dayUse(vp) = 0;
	V_dayUseDate(vp) = Midnight(now);

#if OPENAFS_VOL_STATS
	/*
	 * All we need to do is bzero the entire VOL_STATS_BYTES of
	 * the detailed volume statistics area.
	 */
	memset((char *)(V_stat_area(vp)), 0, VOL_STATS_BYTES);
#endif /* OPENAFS_VOL_STATS */
    }

    /*It's been more than a day of collection */
    /*
     * Always return happily.
     */
    return (0);
}				/*VAdjustVolumeStatistics */

int
VAdjustVolumeStatistics(register Volume * vp)
{
    int retVal;
    VOL_LOCK;
    retVal = VAdjustVolumeStatistics_r(vp);
    VOL_UNLOCK;
    return retVal;
}

void
VBumpVolumeUsage_r(register Volume * vp)
{
    unsigned int now = FT_ApproxTime();
    if (now - V_dayUseDate(vp) > OneDay)
	VAdjustVolumeStatistics_r(vp);
    /*
     * Save the volume header image to disk after every 128 bumps to dayUse.
     */
    if ((V_dayUse(vp)++ & 127) == 0) {
	Error error;
	VUpdateVolume_r(&error, vp, VOL_UPDATE_WAIT);
    }
}

void
VBumpVolumeUsage(register Volume * vp)
{
    VOL_LOCK;
    VBumpVolumeUsage_r(vp);
    VOL_UNLOCK;
}

void
VSetDiskUsage_r(void)
{
#ifndef AFS_DEMAND_ATTACH_FS
    static int FifteenMinuteCounter = 0;
#endif

    while (VInit < 2) {
	/* NOTE: Don't attempt to access the partitions list until the
	 * initialization level indicates that all volumes are attached,
	 * which implies that all partitions are initialized. */
#ifdef AFS_PTHREAD_ENV
	sleep(10);
#else /* AFS_PTHREAD_ENV */
	IOMGR_Sleep(10);
#endif /* AFS_PTHREAD_ENV */
    }

    VResetDiskUsage_r();

#ifndef AFS_DEMAND_ATTACH_FS
    if (++FifteenMinuteCounter == 3) {
	FifteenMinuteCounter = 0;
	VScanUpdateList();
    }
#endif /* !AFS_DEMAND_ATTACH_FS */
}

void
VSetDiskUsage(void)
{
    VOL_LOCK;
    VSetDiskUsage_r();
    VOL_UNLOCK;
}


/***************************************************/
/* Volume Update List routines                     */
/***************************************************/

/* The number of minutes that a volume hasn't been updated before the
 * "Dont salvage" flag in the volume header will be turned on */
#define SALVAGE_INTERVAL	(10*60)

/*
 * demand attach fs
 *
 * volume update list functionality has been moved into the VLRU
 * the DONT_SALVAGE flag is now set during VLRU demotion
 */

#ifndef AFS_DEMAND_ATTACH_FS
static VolumeId *UpdateList = NULL;	/* Pointer to array of Volume ID's */
static int nUpdatedVolumes = 0;	        /* Updated with entry in UpdateList, salvage after crash flag on */
static int updateSize = 0;		/* number of entries possible */
#define UPDATE_LIST_SIZE 128	        /* initial size increment (must be a power of 2!) */
#endif /* !AFS_DEMAND_ATTACH_FS */

void
VAddToVolumeUpdateList_r(Error * ec, Volume * vp)
{
    *ec = 0;
    vp->updateTime = FT_ApproxTime();
    if (V_dontSalvage(vp) == 0)
	return;
    V_dontSalvage(vp) = 0;
    VSyncVolume_r(ec, vp, 0);
#ifdef AFS_DEMAND_ATTACH_FS
    V_attachFlags(vp) &= ~(VOL_HDR_DONTSALV);
#else /* !AFS_DEMAND_ATTACH_FS */
    if (*ec)
	return;
    if (UpdateList == NULL) {
	updateSize = UPDATE_LIST_SIZE;
	UpdateList = (VolumeId *) malloc(sizeof(VolumeId) * updateSize);
    } else {
	if (nUpdatedVolumes == updateSize) {
	    updateSize <<= 1;
	    if (updateSize > 524288) {
		Log("warning: there is likely a bug in the volume update scanner\n");
		return;
	    }
	    UpdateList =
		(VolumeId *) realloc(UpdateList,
				     sizeof(VolumeId) * updateSize);
	}
    }
    assert(UpdateList != NULL);
    UpdateList[nUpdatedVolumes++] = V_id(vp);
#endif /* !AFS_DEMAND_ATTACH_FS */
}

#ifndef AFS_DEMAND_ATTACH_FS
static void
VScanUpdateList(void)
{
    register int i, gap;
    register Volume *vp;
    Error error;
    afs_uint32 now = FT_ApproxTime();
    /* Be careful with this code, since it works with interleaved calls to AddToVolumeUpdateList */
    for (i = gap = 0; i < nUpdatedVolumes; i++) {
	if (gap)
	    UpdateList[i - gap] = UpdateList[i];

	/* XXX this routine needlessly messes up the Volume LRU by
	 * breaking the LRU temporal-locality assumptions.....
	 * we should use a special volume header allocator here */
	vp = VGetVolume_r(&error, UpdateList[i - gap] = UpdateList[i]);
	if (error) {
	    gap++;
	} else if (vp->nUsers == 1 && now - vp->updateTime > SALVAGE_INTERVAL) {
	    V_dontSalvage(vp) = DONT_SALVAGE;
	    VUpdateVolume_r(&error, vp, 0);	/* No need to fsync--not critical */
	    gap++;
	}

	if (vp) {
	    VPutVolume_r(vp);
	}

#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();
#endif /* !AFS_PTHREAD_ENV */
    }
    nUpdatedVolumes -= gap;
}
#endif /* !AFS_DEMAND_ATTACH_FS */


/***************************************************/
/* Volume LRU routines                             */
/***************************************************/

/* demand attach fs
 * volume LRU
 *
 * with demand attach fs, we attempt to soft detach(1)
 * volumes which have not been accessed in a long time
 * in order to speed up fileserver shutdown
 *
 * (1) by soft detach we mean a process very similar
 *     to VOffline, except the final state of the 
 *     Volume will be VOL_STATE_PREATTACHED, instead
 *     of the usual VOL_STATE_UNATTACHED
 */
#ifdef AFS_DEMAND_ATTACH_FS

/* implementation is reminiscent of a generational GC
 *
 * queue 0 is newly attached volumes. this queue is
 * sorted by attach timestamp
 *
 * queue 1 is volumes that have been around a bit
 * longer than queue 0. this queue is sorted by
 * attach timestamp
 *
 * queue 2 is volumes tha have been around the longest.
 * this queue is unsorted
 *
 * queue 3 is volumes that have been marked as
 * candidates for soft detachment. this queue is
 * unsorted
 */
#define VLRU_GENERATIONS  3   /**< number of generations in VLRU */
#define VLRU_QUEUES       5   /**< total number of VLRU queues */

/**
 * definition of a VLRU queue.
 */
struct VLRU_q {
    volatile struct rx_queue q;
    volatile int len;
    volatile int busy;
    pthread_cond_t cv;
};

/**
 * main VLRU data structure.
 */
struct VLRU {
    struct VLRU_q q[VLRU_QUEUES];   /**< VLRU queues */

    /* VLRU config */
    /** time interval (in seconds) between promotion passes for
     *  each young generation queue. */
    afs_uint32 promotion_interval[VLRU_GENERATIONS-1];

    /** time interval (in seconds) between soft detach candidate
     *  scans for each generation queue.
     *
     *  scan_interval[VLRU_QUEUE_CANDIDATE] defines how frequently
     *  we perform a soft detach pass. */
    afs_uint32 scan_interval[VLRU_GENERATIONS+1];

    /* scheduler state */
    int next_idx;                                       /**< next queue to receive attention */
    afs_uint32 last_promotion[VLRU_GENERATIONS-1];      /**< timestamp of last promotion scan */
    afs_uint32 last_scan[VLRU_GENERATIONS+1];           /**< timestamp of last detach scan */

    int scanner_state;                                  /**< state of scanner thread */
    pthread_cond_t cv;                                  /**< state transition CV */
};

/** global VLRU state */
static struct VLRU volume_LRU;

/**
 * defined states for VLRU scanner thread.
 */
typedef enum {
    VLRU_SCANNER_STATE_OFFLINE        = 0,    /**< vlru scanner thread is offline */
    VLRU_SCANNER_STATE_ONLINE         = 1,    /**< vlru scanner thread is online */
    VLRU_SCANNER_STATE_SHUTTING_DOWN  = 2,    /**< vlru scanner thread is shutting down */
    VLRU_SCANNER_STATE_PAUSING        = 3,    /**< vlru scanner thread is getting ready to pause */
    VLRU_SCANNER_STATE_PAUSED         = 4     /**< vlru scanner thread is paused */
} vlru_thread_state_t;

/* vlru disk data header stuff */
#define VLRU_DISK_MAGIC      0x7a8b9cad        /**< vlru disk entry magic number */
#define VLRU_DISK_VERSION    1                 /**< vlru disk entry version number */

/** vlru default expiration time (for eventual fs state serialization of vlru data) */
#define VLRU_DUMP_EXPIRATION_TIME   (60*60*24*7)  /* expire vlru data after 1 week */


/** minimum volume inactivity (in seconds) before a volume becomes eligible for
 *  soft detachment. */
static afs_uint32 VLRU_offline_thresh = VLRU_DEFAULT_OFFLINE_THRESH;

/** time interval (in seconds) between VLRU scanner thread soft detach passes. */
static afs_uint32 VLRU_offline_interval = VLRU_DEFAULT_OFFLINE_INTERVAL;

/** maximum number of volumes to soft detach in a VLRU soft detach pass. */
static afs_uint32 VLRU_offline_max = VLRU_DEFAULT_OFFLINE_MAX;

/** VLRU control flag.  non-zero value implies VLRU subsystem is activated. */
static afs_uint32 VLRU_enabled = 1;

/* queue synchronization routines */
static void VLRU_BeginExclusive_r(struct VLRU_q * q);
static void VLRU_EndExclusive_r(struct VLRU_q * q);
static void VLRU_Wait_r(struct VLRU_q * q);

/**
 * set VLRU subsystem tunable parameters.
 *
 * @param[in] option  tunable option to modify
 * @param[in] val     new value for tunable parameter
 *
 * @pre @c VInitVolumePackage has not yet been called.
 *
 * @post tunable parameter is modified
 *
 * @note DAFS only
 *
 * @note valid option parameters are:
 *    @arg @c VLRU_SET_THRESH 
 *         set the period of inactivity after which
 *         volumes are eligible for soft detachment
 *    @arg @c VLRU_SET_INTERVAL 
 *         set the time interval between calls
 *         to the volume LRU "garbage collector"
 *    @arg @c VLRU_SET_MAX 
 *         set the max number of volumes to deallocate
 *         in one GC pass
 */
void
VLRU_SetOptions(int option, afs_uint32 val)
{
    if (option == VLRU_SET_THRESH) {
	VLRU_offline_thresh = val;
    } else if (option == VLRU_SET_INTERVAL) {
	VLRU_offline_interval = val;
    } else if (option == VLRU_SET_MAX) {
	VLRU_offline_max = val;
    } else if (option == VLRU_SET_ENABLED) {
	VLRU_enabled = val;
    }
    VLRU_ComputeConstants();
}

/**
 * compute VLRU internal timing parameters.
 *
 * @post VLRU scanner thread internal timing parameters are computed
 *
 * @note computes internal timing parameters based upon user-modifiable 
 *       tunable parameters.
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VLRU_ComputeConstants(void)
{
    afs_uint32 factor = VLRU_offline_thresh / VLRU_offline_interval;

    /* compute the candidate scan interval */
    volume_LRU.scan_interval[VLRU_QUEUE_CANDIDATE] = VLRU_offline_interval;

    /* compute the promotion intervals */
    volume_LRU.promotion_interval[VLRU_QUEUE_NEW] = VLRU_offline_thresh * 2;
    volume_LRU.promotion_interval[VLRU_QUEUE_MID] = VLRU_offline_thresh * 4;

    if (factor > 16) {
	/* compute the gen 0 scan interval */
	volume_LRU.scan_interval[VLRU_QUEUE_NEW] = VLRU_offline_thresh / 8;
    } else {
	/* compute the gen 0 scan interval */
	volume_LRU.scan_interval[VLRU_QUEUE_NEW] = VLRU_offline_interval * 2;
    }
}

/**
 * initialize VLRU subsystem.
 *
 * @pre this function has not yet been called
 *
 * @post VLRU subsystem is initialized and VLRU scanner thread is starting
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VInitVLRU(void)
{
    pthread_t tid;
    pthread_attr_t attrs;
    int i;

    if (!VLRU_enabled) {
	Log("VLRU: disabled\n");
	return;
    }

    /* initialize each of the VLRU queues */
    for (i = 0; i < VLRU_QUEUES; i++) {
	queue_Init(&volume_LRU.q[i]);
	volume_LRU.q[i].len = 0;
	volume_LRU.q[i].busy = 0;
	assert(pthread_cond_init(&volume_LRU.q[i].cv, NULL) == 0);
    }

    /* setup the timing constants */
    VLRU_ComputeConstants();

    /* XXX put inside LogLevel check? */
    Log("VLRU: starting scanner with the following configuration parameters:\n");
    Log("VLRU:  offlining volumes after minimum of %d seconds of inactivity\n", VLRU_offline_thresh);
    Log("VLRU:  running VLRU soft detach pass every %d seconds\n", VLRU_offline_interval);
    Log("VLRU:  taking up to %d volumes offline per pass\n", VLRU_offline_max);
    Log("VLRU:  scanning generation 0 for inactive volumes every %d seconds\n", volume_LRU.scan_interval[0]);
    Log("VLRU:  scanning for promotion/demotion between generations 0 and 1 every %d seconds\n", volume_LRU.promotion_interval[0]);
    Log("VLRU:  scanning for promotion/demotion between generations 1 and 2 every %d seconds\n", volume_LRU.promotion_interval[1]);

    /* start up the VLRU scanner */
    volume_LRU.scanner_state = VLRU_SCANNER_STATE_OFFLINE;
    if (programType == fileServer) {
	assert(pthread_cond_init(&volume_LRU.cv, NULL) == 0);
	assert(pthread_attr_init(&attrs) == 0);
	assert(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);
	assert(pthread_create(&tid, &attrs, &VLRU_ScannerThread, NULL) == 0);
    }
}

/**
 * initialize the VLRU-related fields of a newly allocated volume object.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre
 *    @arg @c VOL_LOCK is held.
 *    @arg volume object is not on a VLRU queue.
 *
 * @post VLRU fields are initialized to indicate that volume object is not
 *       currently registered with the VLRU subsystem
 *
 * @note DAFS only
 *
 * @internal volume package interal use only.
 */
static void
VLRU_Init_Node_r(volatile Volume * vp)
{
    if (!VLRU_enabled)
	return;

    assert(queue_IsNotOnQueue(&vp->vlru));
    vp->vlru.idx = VLRU_QUEUE_INVALID;
}

/**
 * add a volume object to a VLRU queue.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre
 *    @arg @c VOL_LOCK is held.
 *    @arg caller MUST hold a lightweight ref on @p vp.
 *    @arg caller MUST NOT hold exclusive ownership of the VLRU queue.
 *
 * @post the volume object is added to the appropriate VLRU queue
 *
 * @note if @c vp->vlru.idx contains the index of a valid VLRU queue,
 *       then the volume is added to that queue.  Otherwise, the value
 *       @c VLRU_QUEUE_NEW is stored into @c vp->vlru.idx and the
 *       volume is added to the NEW generation queue.
 *
 * @note @c VOL_LOCK may be dropped internally
 *
 * @note Volume state is temporarily set to @c VOL_STATE_VLRU_ADD
 *       during the add operation, and is restored to the previous
 *       state prior to return.
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VLRU_Add_r(volatile Volume * vp)
{
    int idx;
    VolState state_save;

    if (!VLRU_enabled)
	return;

    if (queue_IsOnQueue(&vp->vlru))
	return;

    state_save = VChangeState_r(vp, VOL_STATE_VLRU_ADD);

    idx = vp->vlru.idx;
    if ((idx < 0) || (idx >= VLRU_QUEUE_INVALID)) {
	idx = VLRU_QUEUE_NEW;
    }

    VLRU_Wait_r(&volume_LRU.q[idx]);

    /* repeat check since VLRU_Wait_r may have dropped
     * the glock */
    if (queue_IsNotOnQueue(&vp->vlru)) {
	vp->vlru.idx = idx;
	queue_Prepend(&volume_LRU.q[idx], &vp->vlru);
	volume_LRU.q[idx].len++;
	V_attachFlags(vp) |= VOL_ON_VLRU;
	vp->stats.last_promote = FT_ApproxTime();
    }

    VChangeState_r(vp, state_save);
}

/**
 * delete a volume object from a VLRU queue.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre
 *    @arg @c VOL_LOCK is held.
 *    @arg caller MUST hold a lightweight ref on @p vp.
 *    @arg caller MUST NOT hold exclusive ownership of the VLRU queue.
 *
 * @post volume object is removed from the VLRU queue
 *
 * @note @c VOL_LOCK may be dropped internally
 *
 * @note DAFS only
 *
 * @todo We should probably set volume state to something exlcusive 
 *       (as @c VLRU_Add_r does) prior to dropping @c VOL_LOCK.
 *
 * @internal volume package internal use only.
 */
static void
VLRU_Delete_r(volatile Volume * vp)
{
    int idx;

    if (!VLRU_enabled)
	return;

    if (queue_IsNotOnQueue(&vp->vlru))
	return;

    /* handle races */
    do {
      idx = vp->vlru.idx;
      if (idx == VLRU_QUEUE_INVALID)
	  return;
      VLRU_Wait_r(&volume_LRU.q[idx]);
    } while (idx != vp->vlru.idx);

    /* now remove from the VLRU and update 
     * the appropriate counter */
    queue_Remove(&vp->vlru);
    volume_LRU.q[idx].len--;
    vp->vlru.idx = VLRU_QUEUE_INVALID;
    V_attachFlags(vp) &= ~(VOL_ON_VLRU);
}

/**
 * tell the VLRU subsystem that a volume was just accessed.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre
 *    @arg @c VOL_LOCK is held
 *    @arg caller MUST hold a lightweight ref on @p vp
 *    @arg caller MUST NOT hold exclusive ownership of any VLRU queue
 *
 * @post volume VLRU access statistics are updated.  If the volume was on
 *       the VLRU soft detach candidate queue, it is moved to the NEW
 *       generation queue.
 *
 * @note @c VOL_LOCK may be dropped internally
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VLRU_UpdateAccess_r(volatile Volume * vp)
{
    afs_uint32 live_interval;
    Volume * rvp = NULL;

    if (!VLRU_enabled)
	return;

    if (queue_IsNotOnQueue(&vp->vlru))
	return;

    assert(V_attachFlags(vp) & VOL_ON_VLRU);

    /* update the access timestamp */
    vp->stats.last_get = FT_ApproxTime();

    /*
     * if the volume is on the soft detach candidate
     * list, we need to safely move it back to a
     * regular generation.  this has to be done
     * carefully so we don't race against the scanner
     * thread.
     */

    /* if this volume is on the soft detach candidate queue,
     * then grab exclusive access to the necessary queues */
    if (vp->vlru.idx == VLRU_QUEUE_CANDIDATE) {
	rvp = vp;
	VCreateReservation_r(rvp);

	VLRU_Wait_r(&volume_LRU.q[VLRU_QUEUE_NEW]);
	VLRU_BeginExclusive_r(&volume_LRU.q[VLRU_QUEUE_NEW]);
	VLRU_Wait_r(&volume_LRU.q[VLRU_QUEUE_CANDIDATE]);
	VLRU_BeginExclusive_r(&volume_LRU.q[VLRU_QUEUE_CANDIDATE]);
    }

    /* make sure multiple threads don't race to update */
    if (vp->vlru.idx == VLRU_QUEUE_CANDIDATE) {
	VLRU_SwitchQueues(vp, VLRU_QUEUE_NEW, 1);
    }

    if (rvp) {
      VLRU_EndExclusive_r(&volume_LRU.q[VLRU_QUEUE_CANDIDATE]);
      VLRU_EndExclusive_r(&volume_LRU.q[VLRU_QUEUE_NEW]);
      VCancelReservation_r(rvp);
    }
}

/**
 * switch a volume between two VLRU queues.
 *
 * @param[in] vp       pointer to volume object
 * @param[in] new_idx  index of VLRU queue onto which the volume will be moved
 * @param[in] append   controls whether the volume will be appended or 
 *                     prepended to the queue.  A nonzero value means it will
 *                     be appended; zero means it will be prepended.
 *
 * @pre The new (and old, if applicable) queue(s) must either be owned 
 *      exclusively by the calling thread for asynchronous manipulation,
 *      or the queue(s) must be quiescent and VOL_LOCK must be held.
 *      Please see VLRU_BeginExclusive_r, VLRU_EndExclusive_r and VLRU_Wait_r
 *      for further details of the queue asynchronous processing mechanism.
 *
 * @post If the volume object was already on a VLRU queue, it is
 *       removed from the queue.  Depending on the value of the append
 *       parameter, the volume object is either appended or prepended
 *       to the VLRU queue referenced by the new_idx parameter.
 *
 * @note DAFS only
 *
 * @see VLRU_BeginExclusive_r
 * @see VLRU_EndExclusive_r
 * @see VLRU_Wait_r
 *
 * @internal volume package internal use only.
 */
static void
VLRU_SwitchQueues(volatile Volume * vp, int new_idx, int append)
{
    if (queue_IsNotOnQueue(&vp->vlru))
	return;

    queue_Remove(&vp->vlru);
    volume_LRU.q[vp->vlru.idx].len--;
    
    /* put the volume back on the correct generational queue */
    if (append) {
	queue_Append(&volume_LRU.q[new_idx], &vp->vlru);
    } else {
	queue_Prepend(&volume_LRU.q[new_idx], &vp->vlru);
    }

    volume_LRU.q[new_idx].len++;
    vp->vlru.idx = new_idx;
}

/**
 * VLRU background thread.
 *
 * The VLRU Scanner Thread is responsible for periodically scanning through
 * each VLRU queue looking for volumes which should be moved to another
 * queue, or soft detached.
 *
 * @param[in] args  unused thread arguments parameter
 *
 * @return unused thread return value
 *    @retval NULL always
 *
 * @internal volume package internal use only.
 */
static void *
VLRU_ScannerThread(void * args)
{
    afs_uint32 now, min_delay, delay;
    afs_uint32 next_scan[VLRU_GENERATIONS];
    afs_uint32 next_promotion[VLRU_GENERATIONS];
    int i, min_idx, min_op, overdue, state;

    /* set t=0 for promotion cycle to be 
     * fileserver startup */
    now = FT_ApproxTime();
    for (i=0; i < VLRU_GENERATIONS-1; i++) {
	volume_LRU.last_promotion[i] = now;
    }

    /* don't start the scanner until VLRU_offline_thresh
     * plus a small delay for VInitVolumePackage to finish
     * has gone by */

    sleep(VLRU_offline_thresh + 60);

    /* set t=0 for scan cycle to be now */
    now = FT_ApproxTime();
    for (i=0; i < VLRU_GENERATIONS+1; i++) {
	volume_LRU.last_scan[i] = now;
    }

    VOL_LOCK;
    if (volume_LRU.scanner_state == VLRU_SCANNER_STATE_OFFLINE) {
	volume_LRU.scanner_state = VLRU_SCANNER_STATE_ONLINE;
    }

    while ((state = volume_LRU.scanner_state) != VLRU_SCANNER_STATE_SHUTTING_DOWN) {
	/* check to see if we've been asked to pause */
	if (volume_LRU.scanner_state == VLRU_SCANNER_STATE_PAUSING) {
	    volume_LRU.scanner_state = VLRU_SCANNER_STATE_PAUSED;
	    assert(pthread_cond_broadcast(&volume_LRU.cv) == 0);
	    do {
		VOL_CV_WAIT(&volume_LRU.cv);
	    } while (volume_LRU.scanner_state == VLRU_SCANNER_STATE_PAUSED);
	}

	/* scheduling can happen outside the glock */
	VOL_UNLOCK;

	/* figure out what is next on the schedule */

	/* figure out a potential schedule for the new generation first */
	overdue = 0;
	min_delay = volume_LRU.scan_interval[0] + volume_LRU.last_scan[0] - now;
	min_idx = 0;
	min_op = 0;
	if (min_delay > volume_LRU.scan_interval[0]) {
	    /* unsigned overflow -- we're overdue to run this scan */
	    min_delay = 0;
	    overdue = 1;
	}

	/* if we're not overdue for gen 0, figure out schedule for candidate gen */
	if (!overdue) {
	    i = VLRU_QUEUE_CANDIDATE;
	    delay = volume_LRU.scan_interval[i] + volume_LRU.last_scan[i] - now;
	    if (delay < min_delay) {
		min_delay = delay;
		min_idx = i;
	    }
	    if (delay > volume_LRU.scan_interval[i]) {
		/* unsigned overflow -- we're overdue to run this scan */
		min_delay = 0;
		min_idx = i;
		overdue = 1;
		break;
	    }
	}

	/* if we're still not overdue for something, figure out schedules for promotions */
	for (i=0; !overdue && i < VLRU_GENERATIONS-1; i++) {
	    delay = volume_LRU.promotion_interval[i] + volume_LRU.last_promotion[i] - now;
	    if (delay < min_delay) {
		min_delay = delay;
		min_idx = i;
		min_op = 1;
	    }
	    if (delay > volume_LRU.promotion_interval[i]) {
		/* unsigned overflow -- we're overdue to run this promotion */
		min_delay = 0;
		min_idx = i;
		min_op = 1;
		overdue = 1;
		break;
	    }
	}

	/* sleep as needed */
	if (min_delay) {
	    sleep(min_delay);
	}

	/* do whatever is next */
	VOL_LOCK;
	if (min_op) {
	    VLRU_Promote_r(min_idx);
	    VLRU_Demote_r(min_idx+1);
	} else {
	    VLRU_Scan_r(min_idx);
	}
	now = FT_ApproxTime();
    }

    Log("VLRU scanner asked to go offline (scanner_state=%d)\n", state);

    /* signal that scanner is down */
    volume_LRU.scanner_state = VLRU_SCANNER_STATE_OFFLINE;
    assert(pthread_cond_broadcast(&volume_LRU.cv) == 0);
    VOL_UNLOCK;
    return NULL;
}

/**
 * promote volumes from one VLRU generation to the next.
 *
 * This routine scans a VLRU generation looking for volumes which are
 * eligible to be promoted to the next generation.  All volumes which
 * meet the eligibility requirement are promoted.
 *
 * Promotion eligibility is based upon meeting both of the following
 * requirements:
 *
 *    @arg The volume has been accessed since the last promotion:
 *         @c (vp->stats.last_get >= vp->stats.last_promote)
 *    @arg The last promotion occurred at least 
 *         @c volume_LRU.promotion_interval[idx] seconds ago
 *
 * As a performance optimization, promotions are "globbed".  In other
 * words, we promote arbitrarily large contiguous sublists of elements
 * as one operation.  
 *
 * @param[in] idx  VLRU queue index to scan
 *
 * @note DAFS only
 *
 * @internal VLRU internal use only.
 */
static void
VLRU_Promote_r(int idx)
{
    int len, chaining, promote;
    afs_uint32 now, thresh;
    struct rx_queue *qp, *nqp;
    Volume * vp, *start, *end;

    /* get exclusive access to two chains, and drop the glock */
    VLRU_Wait_r(&volume_LRU.q[idx]);
    VLRU_BeginExclusive_r(&volume_LRU.q[idx]);
    VLRU_Wait_r(&volume_LRU.q[idx+1]);
    VLRU_BeginExclusive_r(&volume_LRU.q[idx+1]);
    VOL_UNLOCK;

    thresh = volume_LRU.promotion_interval[idx];
    now = FT_ApproxTime();

    len = chaining = 0;
    for (queue_ScanBackwards(&volume_LRU.q[idx], qp, nqp, rx_queue)) {
	vp = (Volume *)((char *)qp - offsetof(Volume, vlru));
	promote = (((vp->stats.last_promote + thresh) <= now) &&
		   (vp->stats.last_get >= vp->stats.last_promote));

	if (chaining) {
	    if (promote) {
		vp->vlru.idx++;
		len++;
		start = vp;
	    } else {
		/* promote and prepend chain */
		queue_MoveChainAfter(&volume_LRU.q[idx+1], &start->vlru, &end->vlru);
		chaining = 0;
	    }
	} else {
	    if (promote) {
		vp->vlru.idx++;
		len++;
		chaining = 1;
		start = end = vp;
	    }
	}
    }

    if (chaining) {
	/* promote and prepend */
	queue_MoveChainAfter(&volume_LRU.q[idx+1], &start->vlru, &end->vlru);
    }

    if (len) {
	volume_LRU.q[idx].len -= len;
	volume_LRU.q[idx+1].len += len;
    }

    /* release exclusive access to the two chains */
    VOL_LOCK;
    volume_LRU.last_promotion[idx] = now;
    VLRU_EndExclusive_r(&volume_LRU.q[idx+1]);
    VLRU_EndExclusive_r(&volume_LRU.q[idx]);
}

/* run the demotions */
static void
VLRU_Demote_r(int idx)
{
    Error ec;
    int len, chaining, demote;
    afs_uint32 now, thresh;
    struct rx_queue *qp, *nqp;
    Volume * vp, *start, *end;
    Volume ** salv_flag_vec = NULL;
    int salv_vec_offset = 0;

    assert(idx == VLRU_QUEUE_MID || idx == VLRU_QUEUE_OLD);

    /* get exclusive access to two chains, and drop the glock */
    VLRU_Wait_r(&volume_LRU.q[idx-1]);
    VLRU_BeginExclusive_r(&volume_LRU.q[idx-1]);
    VLRU_Wait_r(&volume_LRU.q[idx]);
    VLRU_BeginExclusive_r(&volume_LRU.q[idx]);
    VOL_UNLOCK;

    /* no big deal if this allocation fails */
    if (volume_LRU.q[idx].len) {
	salv_flag_vec = (Volume **) malloc(volume_LRU.q[idx].len * sizeof(Volume *));
    }

    now = FT_ApproxTime();
    thresh = volume_LRU.promotion_interval[idx-1];

    len = chaining = 0;
    for (queue_ScanBackwards(&volume_LRU.q[idx], qp, nqp, rx_queue)) {
	vp = (Volume *)((char *)qp - offsetof(Volume, vlru));
	demote = (((vp->stats.last_promote + thresh) <= now) &&
		  (vp->stats.last_get < (now - thresh)));

	/* we now do volume update list DONT_SALVAGE flag setting during
	 * demotion passes */
	if (salv_flag_vec &&
	    !(V_attachFlags(vp) & VOL_HDR_DONTSALV) &&
	    demote && 
	    (vp->updateTime < (now - SALVAGE_INTERVAL)) &&
	    (V_attachState(vp) == VOL_STATE_ATTACHED)) {
	    salv_flag_vec[salv_vec_offset++] = vp;
	    VCreateReservation_r(vp);
	}

	if (chaining) {
	    if (demote) {
		vp->vlru.idx--;
		len++;
		start = vp;
	    } else {
		/* demote and append chain */
		queue_MoveChainBefore(&volume_LRU.q[idx-1], &start->vlru, &end->vlru);
		chaining = 0;
	    }
	} else {
	    if (demote) {
		vp->vlru.idx--;
		len++;
		chaining = 1;
		start = end = vp;
	    }
	}
    }

    if (chaining) {
	queue_MoveChainBefore(&volume_LRU.q[idx-1], &start->vlru, &end->vlru);
    }

    if (len) {
	volume_LRU.q[idx].len -= len;
	volume_LRU.q[idx-1].len += len;
    }

    /* release exclusive access to the two chains */
    VOL_LOCK;
    VLRU_EndExclusive_r(&volume_LRU.q[idx]);
    VLRU_EndExclusive_r(&volume_LRU.q[idx-1]);

    /* now go back and set the DONT_SALVAGE flags as appropriate */
    if (salv_flag_vec) {
	int i;
	for (i = 0; i < salv_vec_offset; i++) {
	    vp = salv_flag_vec[i];
	    if (!(V_attachFlags(vp) & VOL_HDR_DONTSALV) &&
		(vp->updateTime < (now - SALVAGE_INTERVAL)) &&
		(V_attachState(vp) == VOL_STATE_ATTACHED)) {
		ec = VHold_r(vp);
		if (!ec) {
		    V_attachFlags(vp) |= VOL_HDR_DONTSALV;
		    V_dontSalvage(vp) = DONT_SALVAGE;
		    VUpdateVolume_r(&ec, vp, 0);
		    VPutVolume_r(vp);
		}
	    }
	    VCancelReservation_r(vp);
	}
	free(salv_flag_vec);
    }
}

/* run a pass of the VLRU GC scanner */
static void
VLRU_Scan_r(int idx)
{
    afs_uint32 now, thresh;
    struct rx_queue *qp, *nqp;
    volatile Volume * vp;
    int i, locked = 1;

    assert(idx == VLRU_QUEUE_NEW || idx == VLRU_QUEUE_CANDIDATE);

    /* gain exclusive access to the idx VLRU */
    VLRU_Wait_r(&volume_LRU.q[idx]);
    VLRU_BeginExclusive_r(&volume_LRU.q[idx]);

    if (idx != VLRU_QUEUE_CANDIDATE) {
	/* gain exclusive access to the candidate VLRU */
	VLRU_Wait_r(&volume_LRU.q[VLRU_QUEUE_CANDIDATE]);
	VLRU_BeginExclusive_r(&volume_LRU.q[VLRU_QUEUE_CANDIDATE]);
    }

    now = FT_ApproxTime();
    thresh = now - VLRU_offline_thresh;

    /* perform candidate selection and soft detaching */
    if (idx == VLRU_QUEUE_CANDIDATE) {
	/* soft detach some volumes from the candidate pool */
	VOL_UNLOCK;
	locked = 0;

	for (i=0,queue_ScanBackwards(&volume_LRU.q[idx], qp, nqp, rx_queue)) {
	    vp = (Volume *)((char *)qp - offsetof(Volume, vlru));
	    if (i >= VLRU_offline_max) {
		break;
	    }
	    /* check timestamp to see if it's a candidate for soft detaching */
	    if (vp->stats.last_get <= thresh) {
		VOL_LOCK;
		if (VCheckSoftDetach(vp, thresh))
		    i++;
		VOL_UNLOCK;
	    }
	}
    } else {
	/* scan for volumes to become soft detach candidates */
	for (i=1,queue_ScanBackwards(&volume_LRU.q[idx], qp, nqp, rx_queue),i++) {
	    vp = (Volume *)((char *)qp - offsetof(Volume, vlru));

	    /* check timestamp to see if it's a candidate for soft detaching */
	    if (vp->stats.last_get <= thresh) {
		VCheckSoftDetachCandidate(vp, thresh);
	    }

	    if (!(i&0x7f)) {   /* lock coarsening optimization */
		VOL_UNLOCK;
		pthread_yield();
		VOL_LOCK;
	    }
	}
    }

    /* relinquish exclusive access to the VLRU chains */
    if (!locked) {
	VOL_LOCK;
    }
    volume_LRU.last_scan[idx] = now;
    if (idx != VLRU_QUEUE_CANDIDATE) {
	VLRU_EndExclusive_r(&volume_LRU.q[VLRU_QUEUE_CANDIDATE]);
    }
    VLRU_EndExclusive_r(&volume_LRU.q[idx]);
}

/* check whether volume is safe to soft detach
 * caller MUST NOT hold a ref count on vp */
static int
VCheckSoftDetach(volatile Volume * vp, afs_uint32 thresh)
{
    int ret=0;

    if (vp->nUsers || vp->nWaiters)
	return 0;

    if (vp->stats.last_get <= thresh) {
	ret = VSoftDetachVolume_r(vp, thresh);
    }

    return ret;
}

/* check whether volume should be made a 
 * soft detach candidate */
static int
VCheckSoftDetachCandidate(volatile Volume * vp, afs_uint32 thresh)
{
    int idx, ret = 0;
    if (vp->nUsers || vp->nWaiters)
	return 0;

    idx = vp->vlru.idx;

    assert(idx == VLRU_QUEUE_NEW);

    if (vp->stats.last_get <= thresh) {
	/* move to candidate pool */
	queue_Remove(&vp->vlru);
	volume_LRU.q[VLRU_QUEUE_NEW].len--;
	queue_Prepend(&volume_LRU.q[VLRU_QUEUE_CANDIDATE], &vp->vlru);
	vp->vlru.idx = VLRU_QUEUE_CANDIDATE;
	volume_LRU.q[VLRU_QUEUE_CANDIDATE].len++;
	ret = 1;
    }

    return ret;
}


/* begin exclusive access on VLRU */
static void
VLRU_BeginExclusive_r(struct VLRU_q * q)
{
    assert(q->busy == 0);
    q->busy = 1;
}

/* end exclusive access on VLRU */
static void
VLRU_EndExclusive_r(struct VLRU_q * q)
{
    assert(q->busy);
    q->busy = 0;
    assert(pthread_cond_broadcast(&q->cv) == 0);
}

/* wait for another thread to end exclusive access on VLRU */
static void
VLRU_Wait_r(struct VLRU_q * q)
{
    while(q->busy) {
	VOL_CV_WAIT(&q->cv);
    }
}

/* demand attach fs
 * volume soft detach
 *
 * caller MUST NOT hold a ref count on vp */
static int
VSoftDetachVolume_r(volatile Volume * vp, afs_uint32 thresh)
{
    afs_uint32 ts_save;
    int ret = 0;

    assert(vp->vlru.idx == VLRU_QUEUE_CANDIDATE);

    ts_save = vp->stats.last_get;
    if (ts_save > thresh)
	return 0;

    if (vp->nUsers || vp->nWaiters)
	return 0;

    if (VIsExclusiveState(V_attachState(vp))) {
	return 0;
    }

    switch (V_attachState(vp)) {
    case VOL_STATE_UNATTACHED:
    case VOL_STATE_PREATTACHED:
    case VOL_STATE_ERROR:
    case VOL_STATE_GOING_OFFLINE:
    case VOL_STATE_SHUTTING_DOWN:
    case VOL_STATE_SALVAGING:
	volume_LRU.q[vp->vlru.idx].len--;

	/* create and cancel a reservation to
	 * give the volume an opportunity to
	 * be deallocated */
	VCreateReservation_r(vp);
	queue_Remove(&vp->vlru);
	vp->vlru.idx = VLRU_QUEUE_INVALID;
	V_attachFlags(vp) &= ~(VOL_ON_VLRU);
	VCancelReservation_r(vp);
	return 0;
    }

    /* hold the volume and take it offline.
     * no need for reservations, as VHold_r
     * takes care of that internally. */
    if (VHold_r(vp) == 0) {
	/* vhold drops the glock, so now we should
	 * check to make sure we aren't racing against
	 * other threads.  if we are racing, offlining vp
	 * would be wasteful, and block the scanner for a while 
	 */
	if (vp->nWaiters || 
	    (vp->nUsers > 1) ||
	    (vp->shuttingDown) ||
	    (vp->goingOffline) ||
	    (vp->stats.last_get != ts_save)) {
	    /* looks like we're racing someone else. bail */
	    VPutVolume_r(vp);
	    vp = NULL;
	} else {
	    /* pull it off the VLRU */
	    assert(vp->vlru.idx == VLRU_QUEUE_CANDIDATE);
	    volume_LRU.q[VLRU_QUEUE_CANDIDATE].len--;
	    queue_Remove(&vp->vlru);
	    vp->vlru.idx = VLRU_QUEUE_INVALID;
	    V_attachFlags(vp) &= ~(VOL_ON_VLRU);

	    /* take if offline */
	    VOffline_r(vp, "volume has been soft detached");

	    /* invalidate the volume header cache */
	    FreeVolumeHeader(vp);

	    /* update stats */
	    IncUInt64(&VStats.soft_detaches);
	    vp->stats.soft_detaches++;

	    /* put in pre-attached state so demand
	     * attacher can work on it */
	    VChangeState_r(vp, VOL_STATE_PREATTACHED);
	    ret = 1;
	}
    }
    return ret;
}
#endif /* AFS_DEMAND_ATTACH_FS */


/***************************************************/
/* Volume Header Cache routines                    */
/***************************************************/

/** 
 * volume header cache.
 */
struct volume_hdr_LRU_t volume_hdr_LRU;

/**
 * initialize the volume header cache.
 *
 * @param[in] howMany  number of header cache entries to preallocate
 *
 * @pre VOL_LOCK held.  Function has never been called before.
 *
 * @post howMany cache entries are allocated, initialized, and added 
 *       to the LRU list.  Header cache statistics are initialized.
 *
 * @note only applicable to fileServer program type.  Should only be
 *       called once during volume package initialization.
 *
 * @internal volume package internal use only.
 */
static void
VInitVolumeHeaderCache(afs_uint32 howMany)
{
    register struct volHeader *hp;
    if (programType != fileServer)
	return;
    queue_Init(&volume_hdr_LRU);
    volume_hdr_LRU.stats.free = 0;
    volume_hdr_LRU.stats.used = howMany;
    volume_hdr_LRU.stats.attached = 0;
    hp = (struct volHeader *)(calloc(howMany, sizeof(struct volHeader)));
    while (howMany--)
	ReleaseVolumeHeader(hp++);
}

/**
 * get a volume header and attach it to the volume object.
 *
 * @param[in] vp  pointer to volume object
 *
 * @return cache entry status
 *    @retval 0  volume header was newly attached; cache data is invalid
 *    @retval 1  volume header was previously attached; cache data is valid
 *
 * @pre VOL_LOCK held.  For DAFS, lightweight ref must be held on volume object.
 *
 * @post volume header attached to volume object.  if necessary, header cache 
 *       entry on LRU is synchronized to disk.  Header is removed from LRU list.
 *
 * @note VOL_LOCK may be dropped
 *
 * @warning this interface does not load header data from disk.  it merely
 *          attaches a header object to the volume object, and may sync the old
 *          header cache data out to disk in the process.
 *
 * @internal volume package internal use only.
 */
static int
GetVolumeHeader(register Volume * vp)
{
    Error error;
    register struct volHeader *hd;
    int old;
    static int everLogged = 0;

#ifdef AFS_DEMAND_ATTACH_FS
    VolState vp_save, back_save;

    /* XXX debug 9/19/05 we've apparently got
     * a ref counting bug somewhere that's
     * breaking the nUsers == 0 => header on LRU
     * assumption */
    if (vp->header && queue_IsNotOnQueue(vp->header)) {
	Log("nUsers == 0, but header not on LRU\n");
	return 1;
    }
#endif

    old = (vp->header != NULL);	/* old == volume already has a header */

    if (programType != fileServer) {
	/* for volume utilities, we allocate volHeaders as needed */
	if (!vp->header) {
	    hd = (struct volHeader *)calloc(1, sizeof(*vp->header));
	    assert(hd != NULL);
	    vp->header = hd;
	    hd->back = vp;
#ifdef AFS_DEMAND_ATTACH_FS
	    V_attachFlags(vp) |= VOL_HDR_ATTACHED;
#endif
	}
    } else {
	/* for the fileserver, we keep a volume header cache */
	if (old) {
	    /* the header we previously dropped in the lru is
	     * still available. pull it off the lru and return */
	    hd = vp->header;
	    queue_Remove(hd);
	    assert(hd->back == vp);
	} else {
	    /* we need to grab a new element off the LRU */
	    if (queue_IsNotEmpty(&volume_hdr_LRU)) {
		/* grab an element and pull off of LRU */
		hd = queue_First(&volume_hdr_LRU, volHeader);
		queue_Remove(hd);
	    } else {
		/* LRU is empty, so allocate a new volHeader 
		 * this is probably indicative of a leak, so let the user know */
		hd = (struct volHeader *)calloc(1, sizeof(struct volHeader));
		assert(hd != NULL);
		if (!everLogged) {
		    Log("****Allocated more volume headers, probably leak****\n");
		    everLogged = 1;
		}
		volume_hdr_LRU.stats.free++;
	    }
	    if (hd->back) {
		/* this header used to belong to someone else. 
		 * we'll need to check if the header needs to
		 * be sync'd out to disk */

#ifdef AFS_DEMAND_ATTACH_FS
		/* if hd->back were in an exclusive state, then
		 * its volHeader would not be on the LRU... */
		assert(!VIsExclusiveState(V_attachState(hd->back)));
#endif

		if (hd->diskstuff.inUse) {
		    /* volume was in use, so we'll need to sync
		     * its header to disk */

#ifdef AFS_DEMAND_ATTACH_FS
		    back_save = VChangeState_r(hd->back, VOL_STATE_UPDATING);
		    vp_save = VChangeState_r(vp, VOL_STATE_HDR_ATTACHING);
		    VCreateReservation_r(hd->back);
		    VOL_UNLOCK;
#endif

		    WriteVolumeHeader_r(&error, hd->back);
		    /* Ignore errors; catch them later */

#ifdef AFS_DEMAND_ATTACH_FS
		    VOL_LOCK;
#endif
		}

		hd->back->header = NULL;
#ifdef AFS_DEMAND_ATTACH_FS
		V_attachFlags(hd->back) &= ~(VOL_HDR_ATTACHED | VOL_HDR_LOADED | VOL_HDR_IN_LRU);

		if (hd->diskstuff.inUse) {
		    VChangeState_r(hd->back, back_save);
		    VCancelReservation_r(hd->back);
		    VChangeState_r(vp, vp_save);
		}
#endif
	    } else {
		volume_hdr_LRU.stats.attached++;
	    }
	    hd->back = vp;
	    vp->header = hd;
#ifdef AFS_DEMAND_ATTACH_FS
	    V_attachFlags(vp) |= VOL_HDR_ATTACHED;
#endif
	}
	volume_hdr_LRU.stats.free--;
	volume_hdr_LRU.stats.used++;
    }
    IncUInt64(&VStats.hdr_gets);
#ifdef AFS_DEMAND_ATTACH_FS
    IncUInt64(&vp->stats.hdr_gets);
    vp->stats.last_hdr_get = FT_ApproxTime();
#endif
    return old;
}


/**
 * make sure volume header is attached and contains valid cache data.
 *
 * @param[out] ec  outbound error code
 * @param[in]  vp  pointer to volume object
 *
 * @pre VOL_LOCK held.  For DAFS, lightweight ref held on vp.
 *
 * @post header cache entry attached, and loaded with valid data, or
 *       *ec is nonzero, and the header is released back into the LRU.
 *
 * @internal volume package internal use only.
 */
static void
LoadVolumeHeader(Error * ec, Volume * vp)
{
#ifdef AFS_DEMAND_ATTACH_FS
    VolState state_save;
    afs_uint32 now;
    *ec = 0;

    if (vp->nUsers == 0 && !GetVolumeHeader(vp)) {
	IncUInt64(&VStats.hdr_loads);
	state_save = VChangeState_r(vp, VOL_STATE_HDR_LOADING);
	VOL_UNLOCK;

	ReadHeader(ec, V_diskDataHandle(vp), (char *)&V_disk(vp),
		   sizeof(V_disk(vp)), VOLUMEINFOMAGIC,
		   VOLUMEINFOVERSION);
	IncUInt64(&vp->stats.hdr_loads);
	now = FT_ApproxTime();

	VOL_LOCK;
	if (!*ec) {
	    V_attachFlags(vp) |= VOL_HDR_LOADED;
	    vp->stats.last_hdr_load = now;
	}
	VChangeState_r(vp, state_save);
    }
#else /* AFS_DEMAND_ATTACH_FS */
    *ec = 0;
    if (vp->nUsers == 0 && !GetVolumeHeader(vp)) {
	IncUInt64(&VStats.hdr_loads);

	ReadHeader(ec, V_diskDataHandle(vp), (char *)&V_disk(vp),
		   sizeof(V_disk(vp)), VOLUMEINFOMAGIC,
		   VOLUMEINFOVERSION);
    }
#endif /* AFS_DEMAND_ATTACH_FS */
    if (*ec) {
	/* maintain (nUsers==0) => header in LRU invariant */
	ReleaseVolumeHeader(vp->header);
    }
}

/**
 * release a header cache entry back into the LRU list.
 *
 * @param[in] hd  pointer to volume header cache object
 *
 * @pre VOL_LOCK held.
 *
 * @post header cache object appended onto end of LRU list.
 *
 * @note only applicable to fileServer program type.
 *
 * @note used to place a header cache entry back into the
 *       LRU pool without invalidating it as a cache entry.
 *
 * @internal volume package internal use only.
 */
static void
ReleaseVolumeHeader(register struct volHeader *hd)
{
    if (programType != fileServer)
	return;
    if (!hd || queue_IsOnQueue(hd))	/* no header, or header already released */
	return;
    queue_Append(&volume_hdr_LRU, hd);
#ifdef AFS_DEMAND_ATTACH_FS
    if (hd->back) {
	V_attachFlags(hd->back) |= VOL_HDR_IN_LRU;
    }
#endif
    volume_hdr_LRU.stats.free++;
    volume_hdr_LRU.stats.used--;
}

/**
 * free/invalidate a volume header cache entry.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre VOL_LOCK is held.
 *
 * @post For fileserver, header cache entry is returned to LRU, and it is
 *       invalidated as a cache entry.  For volume utilities, the header
 *       cache entry is freed.
 *
 * @note For fileserver, this should be utilized instead of ReleaseVolumeHeader
 *       whenever it is necessary to invalidate the header cache entry.
 *
 * @see ReleaseVolumeHeader
 *
 * @internal volume package internal use only.
 */
static void
FreeVolumeHeader(register Volume * vp)
{
    register struct volHeader *hd = vp->header;
    if (!hd)
	return;
    if (programType == fileServer) {
	ReleaseVolumeHeader(hd);
	hd->back = NULL;
    } else {
	free(hd);
    }
#ifdef AFS_DEMAND_ATTACH_FS
    V_attachFlags(vp) &= ~(VOL_HDR_ATTACHED | VOL_HDR_IN_LRU | VOL_HDR_LOADED);
#endif
    volume_hdr_LRU.stats.attached--;
    vp->header = NULL;
}


/***************************************************/
/* Volume Hash Table routines                      */
/***************************************************/

/**
 * set size of volume object hash table.
 *
 * @param[in] logsize   log(2) of desired hash table size
 *
 * @return operation status
 *    @retval 0 success
 *    @retval -1 failure
 *
 * @pre MUST be called prior to VInitVolumePackage
 *
 * @post Volume Hash Table will have 2^logsize buckets
 */
int 
VSetVolHashSize(int logsize)
{
    /* 64 to 16384 hash buckets seems like a reasonable range */
    if ((logsize < 6 ) || (logsize > 14)) {
        return -1;
    }
    
    if (!VInit) {
        VolumeHashTable.Size = 1 << logsize;
        VolumeHashTable.Mask = VolumeHashTable.Size - 1;
    } else {
	/* we can't yet support runtime modification of this
	 * parameter. we'll need a configuration rwlock to
	 * make runtime modification feasible.... */
	return -1;
    }
    return 0;
}

/**
 * initialize dynamic data structures for volume hash table.
 *
 * @post hash table is allocated, and fields are initialized.
 *
 * @internal volume package internal use only.
 */
static void
VInitVolumeHash(void)
{
    register int i;

    VolumeHashTable.Table = (VolumeHashChainHead *) calloc(VolumeHashTable.Size, 
							   sizeof(VolumeHashChainHead));
    assert(VolumeHashTable.Table != NULL);
    
    for (i=0; i < VolumeHashTable.Size; i++) {
	queue_Init(&VolumeHashTable.Table[i]);
#ifdef AFS_DEMAND_ATTACH_FS
	assert(pthread_cond_init(&VolumeHashTable.Table[i].chain_busy_cv, NULL) == 0);
#endif /* AFS_DEMAND_ATTACH_FS */
    }
}

/**
 * add a volume object to the hash table.
 *
 * @param[in] vp      pointer to volume object
 * @param[in] hashid  hash of volume id
 *
 * @pre VOL_LOCK is held.  For DAFS, caller must hold a lightweight
 *      reference on vp.
 *
 * @post volume is added to hash chain.
 *
 * @internal volume package internal use only.
 *
 * @note For DAFS, VOL_LOCK may be dropped in order to wait for an
 *       asynchronous hash chain reordering to finish.
 */
static void
AddVolumeToHashTable(register Volume * vp, int hashid)
{
    VolumeHashChainHead * head;

    if (queue_IsOnQueue(vp))
	return;

    head = &VolumeHashTable.Table[VOLUME_HASH(hashid)];

#ifdef AFS_DEMAND_ATTACH_FS
    /* wait for the hash chain to become available */
    VHashWait_r(head);

    V_attachFlags(vp) |= VOL_IN_HASH;
    vp->chainCacheCheck = ++head->cacheCheck;
#endif /* AFS_DEMAND_ATTACH_FS */

    head->len++;
    vp->hashid = hashid;
    queue_Append(head, vp);
    vp->vnodeHashOffset = VolumeHashOffset_r();
}

/**
 * delete a volume object from the hash table.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre VOL_LOCK is held.  For DAFS, caller must hold a lightweight
 *      reference on vp.
 *
 * @post volume is removed from hash chain.
 *
 * @internal volume package internal use only.
 *
 * @note For DAFS, VOL_LOCK may be dropped in order to wait for an
 *       asynchronous hash chain reordering to finish.
 */
static void
DeleteVolumeFromHashTable(register Volume * vp)
{
    VolumeHashChainHead * head;

    if (!queue_IsOnQueue(vp))
	return;

    head = &VolumeHashTable.Table[VOLUME_HASH(vp->hashid)];

#ifdef AFS_DEMAND_ATTACH_FS
    /* wait for the hash chain to become available */
    VHashWait_r(head);

    V_attachFlags(vp) &= ~(VOL_IN_HASH);
    head->cacheCheck++;
#endif /* AFS_DEMAND_ATTACH_FS */

    head->len--;
    queue_Remove(vp);
    /* do NOT reset hashid to zero, as the online
     * salvager package may need to know the volume id
     * after the volume is removed from the hash */
}

/**
 * lookup a volume object in the hash table given a volume id.
 *
 * @param[out] ec        error code return
 * @param[in]  volumeId  volume id
 * @param[in]  hint      volume object which we believe could be the correct 
                         mapping
 *
 * @return volume object pointer
 *    @retval NULL  no such volume id is registered with the hash table.
 *
 * @pre VOL_LOCK is held.  For DAFS, caller must hold a lightweight 
        ref on hint.
 *
 * @post volume object with the given id is returned.  volume object and 
 *       hash chain access statistics are updated.  hash chain may have 
 *       been reordered.
 *
 * @note For DAFS, VOL_LOCK may be dropped in order to wait for an 
 *       asynchronous hash chain reordering operation to finish, or 
 *       in order for us to perform an asynchronous chain reordering.
 *
 * @note Hash chain reorderings occur when the access count for the 
 *       volume object being looked up exceeds the sum of the previous 
 *       node's (the node ahead of it in the hash chain linked list) 
 *       access count plus the constant VOLUME_HASH_REORDER_THRESHOLD.
 *
 * @note For DAFS, the hint parameter allows us to short-circuit if the 
 *       cacheCheck fields match between the hash chain head and the 
 *       hint volume object.
 */
Volume *
VLookupVolume_r(Error * ec, VolId volumeId, Volume * hint)
{
    register int looks = 0;
    Volume * vp, *np, *pp;
    VolumeHashChainHead * head;
    *ec = 0;

    head = &VolumeHashTable.Table[VOLUME_HASH(volumeId)];

#ifdef AFS_DEMAND_ATTACH_FS
    /* wait for the hash chain to become available */
    VHashWait_r(head);

    /* check to see if we can short circuit without walking the hash chain */
    if (hint && (hint->chainCacheCheck == head->cacheCheck)) {
	IncUInt64(&hint->stats.hash_short_circuits);
	return hint;
    }
#endif /* AFS_DEMAND_ATTACH_FS */

    /* someday we need to either do per-chain locks, RWlocks,
     * or both for volhash access. 
     * (and move to a data structure with better cache locality) */

    /* search the chain for this volume id */
    for(queue_Scan(head, vp, np, Volume)) {
	looks++;
	if ((vp->hashid == volumeId)) {
	    break;
	}
    }

    if (queue_IsEnd(head, vp)) {
	vp = NULL;
    }

#ifdef AFS_DEMAND_ATTACH_FS
    /* update hash chain statistics */
    {
	afs_uint64 lks;
	FillInt64(lks, 0, looks);
	AddUInt64(head->looks, lks, &head->looks);
	AddUInt64(VStats.hash_looks, lks, &VStats.hash_looks);
	IncUInt64(&head->gets);
    }

    if (vp) {
	afs_uint64 thresh;
	IncUInt64(&vp->stats.hash_lookups);

	/* for demand attach fileserver, we permit occasional hash chain reordering
	 * so that frequently looked up volumes move towards the head of the chain */
	pp = queue_Prev(vp, Volume);
	if (!queue_IsEnd(head, pp)) {
	    FillInt64(thresh, 0, VOLUME_HASH_REORDER_THRESHOLD);
	    AddUInt64(thresh, pp->stats.hash_lookups, &thresh);
	    if (GEInt64(vp->stats.hash_lookups, thresh)) {
		VReorderHash_r(head, pp, vp);
	    }
	}

	/* update the short-circuit cache check */
	vp->chainCacheCheck = head->cacheCheck;
    }
#endif /* AFS_DEMAND_ATTACH_FS */    

    return vp;
}

#ifdef AFS_DEMAND_ATTACH_FS
/* perform volume hash chain reordering.
 *
 * advance a subchain beginning at vp ahead of
 * the adjacent subchain ending at pp */
static void
VReorderHash_r(VolumeHashChainHead * head, Volume * pp, Volume * vp)
{
    Volume *tp, *np, *lp;
    afs_uint64 move_thresh;

    /* this should never be called if the chain is already busy, so
     * no need to wait for other exclusive chain ops to finish */

    /* this is a rather heavy set of operations,
     * so let's set the chain busy flag and drop
     * the vol_glock */
    VHashBeginExclusive_r(head);
    VOL_UNLOCK;

    /* scan forward in the chain from vp looking for the last element
     * in the chain we want to advance */
    FillInt64(move_thresh, 0, VOLUME_HASH_REORDER_CHAIN_THRESH);
    AddUInt64(move_thresh, pp->stats.hash_lookups, &move_thresh);
    for(queue_ScanFrom(head, vp, tp, np, Volume)) {
	if (LTInt64(tp->stats.hash_lookups, move_thresh)) {
	    break;
	}
    }
    lp = queue_Prev(tp, Volume);

    /* scan backwards from pp to determine where to splice and
     * insert the subchain we're advancing */
    for(queue_ScanBackwardsFrom(head, pp, tp, np, Volume)) {
	if (GTInt64(tp->stats.hash_lookups, move_thresh)) {
	    break;
	}
    }
    tp = queue_Next(tp, Volume);

    /* rebalance chain(vp,...,lp) ahead of chain(tp,...,pp) */
    queue_MoveChainBefore(tp,vp,lp);

    VOL_LOCK;
    IncUInt64(&VStats.hash_reorders);
    head->cacheCheck++;
    IncUInt64(&head->reorders);

    /* wake up any threads waiting for the hash chain */
    VHashEndExclusive_r(head);
}


/* demand-attach fs volume hash
 * asynchronous exclusive operations */

/**
 * begin an asynchronous exclusive operation on a volume hash chain.
 *
 * @param[in] head   pointer to volume hash chain head object
 *
 * @pre VOL_LOCK held.  hash chain is quiescent.
 *
 * @post hash chain marked busy.
 *
 * @note this interface is used in conjunction with VHashEndExclusive_r and
 *       VHashWait_r to perform asynchronous (wrt VOL_LOCK) operations on a
 *       volume hash chain.  Its main use case is hash chain reordering, which
 *       has the potential to be a highly latent operation.
 *
 * @see VHashEndExclusive_r
 * @see VHashWait_r
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VHashBeginExclusive_r(VolumeHashChainHead * head)
{
    assert(head->busy == 0);
    head->busy = 1;
}

/**
 * relinquish exclusive ownership of a volume hash chain.
 *
 * @param[in] head   pointer to volume hash chain head object
 *
 * @pre VOL_LOCK held.  thread owns the hash chain exclusively.
 *
 * @post hash chain is marked quiescent.  threads awaiting use of
 *       chain are awakened.
 *
 * @see VHashBeginExclusive_r
 * @see VHashWait_r
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VHashEndExclusive_r(VolumeHashChainHead * head)
{
    assert(head->busy);
    head->busy = 0;
    assert(pthread_cond_broadcast(&head->chain_busy_cv) == 0);
}

/**
 * wait for all asynchronous operations on a hash chain to complete.
 *
 * @param[in] head   pointer to volume hash chain head object
 *
 * @pre VOL_LOCK held.
 *
 * @post hash chain object is quiescent.
 *
 * @see VHashBeginExclusive_r
 * @see VHashEndExclusive_r
 *
 * @note DAFS only
 *
 * @note This interface should be called before any attempt to
 *       traverse the hash chain.  It is permissible for a thread
 *       to gain exclusive access to the chain, and then perform
 *       latent operations on the chain asynchronously wrt the 
 *       VOL_LOCK.
 *
 * @warning if waiting is necessary, VOL_LOCK is dropped
 *
 * @internal volume package internal use only.
 */
static void
VHashWait_r(VolumeHashChainHead * head)
{
    while (head->busy) {
	VOL_CV_WAIT(&head->chain_busy_cv);
    }
}
#endif /* AFS_DEMAND_ATTACH_FS */


/***************************************************/
/* Volume by Partition List routines               */
/***************************************************/

/*
 * demand attach fileserver adds a
 * linked list of volumes to each
 * partition object, thus allowing
 * for quick enumeration of all
 * volumes on a partition
 */

#ifdef AFS_DEMAND_ATTACH_FS
/**
 * add a volume to its disk partition VByPList.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre either the disk partition VByPList is owned exclusively
 *      by the calling thread, or the list is quiescent and
 *      VOL_LOCK is held.
 *
 * @post volume is added to disk partition VByPList
 *
 * @note DAFS only
 *
 * @warning it is the caller's responsibility to ensure list
 *          quiescence.
 *
 * @see VVByPListWait_r
 * @see VVByPListBeginExclusive_r
 * @see VVByPListEndExclusive_r
 *
 * @internal volume package internal use only.
 */
static void
AddVolumeToVByPList_r(Volume * vp)
{
    if (queue_IsNotOnQueue(&vp->vol_list)) {
	queue_Append(&vp->partition->vol_list, &vp->vol_list);
	V_attachFlags(vp) |= VOL_ON_VBYP_LIST;
	vp->partition->vol_list.len++;
    }
}

/**
 * delete a volume from its disk partition VByPList.
 *
 * @param[in] vp  pointer to volume object
 *
 * @pre either the disk partition VByPList is owned exclusively
 *      by the calling thread, or the list is quiescent and
 *      VOL_LOCK is held.
 *
 * @post volume is removed from the disk partition VByPList
 *
 * @note DAFS only
 *
 * @warning it is the caller's responsibility to ensure list
 *          quiescence.
 *
 * @see VVByPListWait_r
 * @see VVByPListBeginExclusive_r
 * @see VVByPListEndExclusive_r
 *
 * @internal volume package internal use only.
 */
static void
DeleteVolumeFromVByPList_r(Volume * vp)
{
    if (queue_IsOnQueue(&vp->vol_list)) {
	queue_Remove(&vp->vol_list);
	V_attachFlags(vp) &= ~(VOL_ON_VBYP_LIST);
	vp->partition->vol_list.len--;
    }
}

/**
 * begin an asynchronous exclusive operation on a VByPList.
 *
 * @param[in] dp   pointer to disk partition object
 *
 * @pre VOL_LOCK held.  VByPList is quiescent.
 *
 * @post VByPList marked busy.
 *
 * @note this interface is used in conjunction with VVByPListEndExclusive_r and
 *       VVByPListWait_r to perform asynchronous (wrt VOL_LOCK) operations on a
 *       VByPList.
 *
 * @see VVByPListEndExclusive_r
 * @see VVByPListWait_r
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
/* take exclusive control over the list */
static void
VVByPListBeginExclusive_r(struct DiskPartition * dp)
{
    assert(dp->vol_list.busy == 0);
    dp->vol_list.busy = 1;
}

/**
 * relinquish exclusive ownership of a VByPList.
 *
 * @param[in] dp   pointer to disk partition object
 *
 * @pre VOL_LOCK held.  thread owns the VByPList exclusively.
 *
 * @post VByPList is marked quiescent.  threads awaiting use of
 *       the list are awakened.
 *
 * @see VVByPListBeginExclusive_r
 * @see VVByPListWait_r
 *
 * @note DAFS only
 *
 * @internal volume package internal use only.
 */
static void
VVByPListEndExclusive_r(struct DiskPartition * dp)
{
    assert(dp->vol_list.busy);
    dp->vol_list.busy = 0;
    assert(pthread_cond_broadcast(&dp->vol_list.cv) == 0);
}

/**
 * wait for all asynchronous operations on a VByPList to complete.
 *
 * @param[in] dp  pointer to disk partition object
 *
 * @pre VOL_LOCK is held.
 *
 * @post disk partition's VByP list is quiescent
 *
 * @note DAFS only
 *
 * @note This interface should be called before any attempt to
 *       traverse the VByPList.  It is permissible for a thread
 *       to gain exclusive access to the list, and then perform
 *       latent operations on the list asynchronously wrt the 
 *       VOL_LOCK.
 *
 * @warning if waiting is necessary, VOL_LOCK is dropped
 *
 * @see VVByPListEndExclusive_r
 * @see VVByPListBeginExclusive_r
 *
 * @internal volume package internal use only.
 */
static void
VVByPListWait_r(struct DiskPartition * dp)
{
    while (dp->vol_list.busy) {
	VOL_CV_WAIT(&dp->vol_list.cv);
    }
}
#endif /* AFS_DEMAND_ATTACH_FS */

/***************************************************/
/* Volume Cache Statistics routines                */
/***************************************************/

void
VPrintCacheStats_r(void)
{
    afs_uint32 get_hi, get_lo, load_hi, load_lo;
    register struct VnodeClassInfo *vcp;
    vcp = &VnodeClassInfo[vLarge];
    Log("Large vnode cache, %d entries, %d allocs, %d gets (%d reads), %d writes\n", vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    vcp = &VnodeClassInfo[vSmall];
    Log("Small vnode cache,%d entries, %d allocs, %d gets (%d reads), %d writes\n", vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    SplitInt64(VStats.hdr_gets, get_hi, get_lo);
    SplitInt64(VStats.hdr_loads, load_hi, load_lo);
    Log("Volume header cache, %d entries, %d gets, %d replacements\n",
	VStats.hdr_cache_size, get_lo, load_lo);
}

void
VPrintCacheStats(void)
{
    VOL_LOCK;
    VPrintCacheStats_r();
    VOL_UNLOCK;
}

#ifdef AFS_DEMAND_ATTACH_FS
static double
UInt64ToDouble(afs_uint64 * x)
{
    static double c32 = 4.0 * 1.073741824 * 1000000000.0;
    afs_uint32 h, l;
    SplitInt64(*x, h, l);
    return (((double)h) * c32) + ((double) l);
}

static char *
DoubleToPrintable(double x, char * buf, int len)
{
    static double billion = 1000000000.0;
    afs_uint32 y[3];

    y[0] = (afs_uint32) (x / (billion * billion));
    y[1] = (afs_uint32) ((x - (((double)y[0]) * billion * billion)) / billion);
    y[2] = (afs_uint32) (x - ((((double)y[0]) * billion * billion) + (((double)y[1]) * billion)));

    if (y[0]) {
	snprintf(buf, len, "%d%09d%09d", y[0], y[1], y[2]);
    } else if (y[1]) {
	snprintf(buf, len, "%d%09d", y[1], y[2]);
    } else {
	snprintf(buf, len, "%d", y[2]);
    }
    buf[len-1] = '\0';
    return buf;
}

void
VPrintExtendedCacheStats_r(int flags)
{
    int i, j;
    struct stats {
	double min;
	double max;
	double sum;
	double avg;
    };
    struct stats looks, gets, reorders, len;
    struct stats ch_looks, ch_gets, ch_reorders;
    char pr_buf[4][32];
    VolumeHashChainHead *head;
    Volume *vp, *np;

    /* zero out stats */
    memset(&looks, 0, sizeof(struct stats));
    memset(&gets, 0, sizeof(struct stats));
    memset(&reorders, 0, sizeof(struct stats));
    memset(&len, 0, sizeof(struct stats));
    memset(&ch_looks, 0, sizeof(struct stats));
    memset(&ch_gets, 0, sizeof(struct stats));
    memset(&ch_reorders, 0, sizeof(struct stats));

    for (i = 0; i < VolumeHashTable.Size; i++) {
	head = &VolumeHashTable.Table[i];

	VHashWait_r(head);
	VHashBeginExclusive_r(head);
	VOL_UNLOCK;

	ch_looks.sum    = UInt64ToDouble(&head->looks);
	ch_gets.sum     = UInt64ToDouble(&head->gets);
	ch_reorders.sum = UInt64ToDouble(&head->reorders);

	/* update global statistics */
	{
	    looks.sum    += ch_looks.sum;
	    gets.sum     += ch_gets.sum;
	    reorders.sum += ch_reorders.sum;
	    len.sum      += (double)head->len;
	    
	    if (i == 0) {
		len.min      = (double) head->len;
		len.max      = (double) head->len;
		looks.min    = ch_looks.sum;
		looks.max    = ch_looks.sum;
		gets.min     = ch_gets.sum;
		gets.max     = ch_gets.sum;
		reorders.min = ch_reorders.sum;
		reorders.max = ch_reorders.sum;
	    } else {
		if (((double)head->len) < len.min)
		    len.min = (double) head->len;
		if (((double)head->len) > len.max)
		    len.max = (double) head->len;
		if (ch_looks.sum < looks.min)
		    looks.min = ch_looks.sum;
		else if (ch_looks.sum > looks.max)
		    looks.max = ch_looks.sum;
		if (ch_gets.sum < gets.min)
		    gets.min = ch_gets.sum;
		else if (ch_gets.sum > gets.max)
		    gets.max = ch_gets.sum;
		if (ch_reorders.sum < reorders.min)
		    reorders.min = ch_reorders.sum;
		else if (ch_reorders.sum > reorders.max)
		    reorders.max = ch_reorders.sum;
	    }
	}

	if ((flags & VOL_STATS_PER_CHAIN2) && queue_IsNotEmpty(head)) {
	    /* compute detailed per-chain stats */
	    struct stats hdr_loads, hdr_gets;
	    double v_looks, v_loads, v_gets;

	    /* initialize stats with data from first element in chain */
	    vp = queue_First(head, Volume);
	    v_looks = UInt64ToDouble(&vp->stats.hash_lookups);
	    v_loads = UInt64ToDouble(&vp->stats.hdr_loads);
	    v_gets  = UInt64ToDouble(&vp->stats.hdr_gets);
	    ch_gets.min = ch_gets.max = v_looks;
	    hdr_loads.min = hdr_loads.max = v_loads;
	    hdr_gets.min = hdr_gets.max = v_gets;
	    hdr_loads.sum = hdr_gets.sum = 0;

	    vp = queue_Next(vp, Volume);

	    /* pull in stats from remaining elements in chain */
	    for (queue_ScanFrom(head, vp, vp, np, Volume)) {
		v_looks = UInt64ToDouble(&vp->stats.hash_lookups);
		v_loads = UInt64ToDouble(&vp->stats.hdr_loads);
		v_gets  = UInt64ToDouble(&vp->stats.hdr_gets);

		hdr_loads.sum += v_loads;
		hdr_gets.sum += v_gets;

		if (v_looks < ch_gets.min)
		    ch_gets.min = v_looks;
		else if (v_looks > ch_gets.max)
		    ch_gets.max = v_looks;

		if (v_loads < hdr_loads.min)
		    hdr_loads.min = v_loads;
		else if (v_loads > hdr_loads.max)
		    hdr_loads.max = v_loads;

		if (v_gets < hdr_gets.min)
		    hdr_gets.min = v_gets;
		else if (v_gets > hdr_gets.max)
		    hdr_gets.max = v_gets;
	    }

	    /* compute per-chain averages */
	    ch_gets.avg = ch_gets.sum / ((double)head->len);
	    hdr_loads.avg = hdr_loads.sum / ((double)head->len);
	    hdr_gets.avg = hdr_gets.sum / ((double)head->len);

	    /* dump per-chain stats */
	    Log("Volume hash chain %d : len=%d, looks=%s, reorders=%s\n",
		i, head->len, 
		DoubleToPrintable(ch_looks.sum, pr_buf[0], sizeof(pr_buf[0])),
		DoubleToPrintable(ch_reorders.sum, pr_buf[1], sizeof(pr_buf[1])));
	    Log("\tVolume gets : min=%s, max=%s, avg=%s, total=%s\n",
		DoubleToPrintable(ch_gets.min, pr_buf[0], sizeof(pr_buf[0])),
		DoubleToPrintable(ch_gets.max, pr_buf[1], sizeof(pr_buf[1])),
		DoubleToPrintable(ch_gets.avg, pr_buf[2], sizeof(pr_buf[2])),
		DoubleToPrintable(ch_gets.sum, pr_buf[3], sizeof(pr_buf[3])));
	    Log("\tHDR gets : min=%s, max=%s, avg=%s, total=%s\n",
		DoubleToPrintable(hdr_gets.min, pr_buf[0], sizeof(pr_buf[0])),
		DoubleToPrintable(hdr_gets.max, pr_buf[1], sizeof(pr_buf[1])),
		DoubleToPrintable(hdr_gets.avg, pr_buf[2], sizeof(pr_buf[2])),
		DoubleToPrintable(hdr_gets.sum, pr_buf[3], sizeof(pr_buf[3])));
	    Log("\tHDR loads : min=%s, max=%s, avg=%s, total=%s\n",
		DoubleToPrintable(hdr_loads.min, pr_buf[0], sizeof(pr_buf[0])),
		DoubleToPrintable(hdr_loads.max, pr_buf[1], sizeof(pr_buf[1])),
		DoubleToPrintable(hdr_loads.avg, pr_buf[2], sizeof(pr_buf[2])),
		DoubleToPrintable(hdr_loads.sum, pr_buf[3], sizeof(pr_buf[3])));
	} else if (flags & VOL_STATS_PER_CHAIN) {
	    /* dump simple per-chain stats */
	    Log("Volume hash chain %d : len=%d, looks=%s, gets=%s, reorders=%s\n",
		i, head->len, 
		DoubleToPrintable(ch_looks.sum, pr_buf[0], sizeof(pr_buf[0])),
		DoubleToPrintable(ch_gets.sum, pr_buf[1], sizeof(pr_buf[1])),
		DoubleToPrintable(ch_reorders.sum, pr_buf[2], sizeof(pr_buf[2])));
	}

	VOL_LOCK;
	VHashEndExclusive_r(head);
    }

    VOL_UNLOCK;

    /* compute global averages */
    len.avg      = len.sum      / ((double)VolumeHashTable.Size);
    looks.avg    = looks.sum    / ((double)VolumeHashTable.Size);
    gets.avg     = gets.sum     / ((double)VolumeHashTable.Size);
    reorders.avg = reorders.sum / ((double)VolumeHashTable.Size);

    /* dump global stats */
    Log("Volume hash summary: %d buckets\n", VolumeHashTable.Size);
    Log(" chain length : min=%s, max=%s, avg=%s, total=%s\n",
	DoubleToPrintable(len.min, pr_buf[0], sizeof(pr_buf[0])),
	DoubleToPrintable(len.max, pr_buf[1], sizeof(pr_buf[1])),
	DoubleToPrintable(len.avg, pr_buf[2], sizeof(pr_buf[2])),
	DoubleToPrintable(len.sum, pr_buf[3], sizeof(pr_buf[3])));
    Log(" looks : min=%s, max=%s, avg=%s, total=%s\n",
	DoubleToPrintable(looks.min, pr_buf[0], sizeof(pr_buf[0])),
	DoubleToPrintable(looks.max, pr_buf[1], sizeof(pr_buf[1])),
	DoubleToPrintable(looks.avg, pr_buf[2], sizeof(pr_buf[2])),
	DoubleToPrintable(looks.sum, pr_buf[3], sizeof(pr_buf[3])));
    Log(" gets : min=%s, max=%s, avg=%s, total=%s\n",
	DoubleToPrintable(gets.min, pr_buf[0], sizeof(pr_buf[0])),
	DoubleToPrintable(gets.max, pr_buf[1], sizeof(pr_buf[1])),
	DoubleToPrintable(gets.avg, pr_buf[2], sizeof(pr_buf[2])),
	DoubleToPrintable(gets.sum, pr_buf[3], sizeof(pr_buf[3])));
    Log(" reorders : min=%s, max=%s, avg=%s, total=%s\n",
	DoubleToPrintable(reorders.min, pr_buf[0], sizeof(pr_buf[0])),
	DoubleToPrintable(reorders.max, pr_buf[1], sizeof(pr_buf[1])),
	DoubleToPrintable(reorders.avg, pr_buf[2], sizeof(pr_buf[2])),
	DoubleToPrintable(reorders.sum, pr_buf[3], sizeof(pr_buf[3])));

    /* print extended disk related statistics */
    {
	struct DiskPartition * diskP;
	afs_uint32 vol_count[VOLMAXPARTS+1];
	byte part_exists[VOLMAXPARTS+1];
	Device id;
	int i;

	memset(vol_count, 0, sizeof(vol_count));
	memset(part_exists, 0, sizeof(part_exists));

	VOL_LOCK;

	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    id = diskP->index;
	    vol_count[id] = diskP->vol_list.len;
	    part_exists[id] = 1;
	}

	VOL_UNLOCK;
	for (i = 0; i <= VOLMAXPARTS; i++) {
	    if (part_exists[i]) {
		diskP = VGetPartitionById_r(i, 0);
		if (diskP) {
		    Log("Partition %s has %d online volumes\n", 
			VPartitionPath(diskP), diskP->vol_list.len);
		}
	    }
	}
	VOL_LOCK;
    }

}

void
VPrintExtendedCacheStats(int flags)
{
    VOL_LOCK;
    VPrintExtendedCacheStats_r(flags);
    VOL_UNLOCK;
}
#endif /* AFS_DEMAND_ATTACH_FS */
