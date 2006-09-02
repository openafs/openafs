/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
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
    ("$Header: /cvs/openafs/src/vol/volume.c,v 1.35.2.8 2006/08/24 20:21:49 shadow Exp $");

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
#ifdef AFS_SGI_EFS_IOPS_ENV
#define ROOTINO EFS_ROOTINO
#include <sys/fs/efs.h>
#include "sgiefs/efs.h"		/* until 5.1 release */
#endif


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
#include "vnode.h"
#include "volume.h"
#include "partition.h"
#ifdef AFS_PTHREAD_ENV
#include <assert.h>
#else /* AFS_PTHREAD_ENV */
#include "afs/assert.h"
#endif /* AFS_PTHREAD_ENV */
#include "vutils.h"
#include "fssync.h"
#ifndef AFS_NT40_ENV
#include <unistd.h>
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
pthread_mutex_t vol_attach_mutex;
pthread_mutex_t vol_fsync_mutex;
pthread_mutex_t vol_trans_mutex;
pthread_cond_t vol_put_volume_cond;
pthread_cond_t vol_sleep_cond;
int vol_attach_threads = 1;
#endif /* AFS_PTHREAD_ENV */

#ifdef	AFS_OSF_ENV
extern void *calloc(), *realloc();
#endif

/*@printflike@*/ extern void Log(const char *format, ...);

/* Forward declarations */
static Volume *attach2(Error * ec, char *path,
		       register struct VolumeHeader *header,
		       struct DiskPartition *partp, int isbusy);
static void FreeVolume(Volume * vp);
static void VScanUpdateList(void);
static void InitLRU(int howMany);
static int GetVolumeHeader(register Volume * vp);
static void ReleaseVolumeHeader(register struct volHeader *hd);
static void FreeVolumeHeader(register Volume * vp);
static void AddVolumeToHashTable(register Volume * vp, int hashid);
static void DeleteVolumeFromHashTable(register Volume * vp);
static int VHold(Volume * vp);
static int VHold_r(Volume * vp);
static void GetBitmap(Error * ec, Volume * vp, VnodeClass class);
static void GetVolumePath(Error * ec, VolId volumeId, char **partitionp,
			  char **namep);
static void VReleaseVolumeHandles_r(Volume * vp);
static void VCloseVolumeHandles_r(Volume * vp);

int LogLevel;			/* Vice loglevel--not defined as extern so that it will be
				 * defined when not linked with vice, XXXX */
ProgramType programType;	/* The type of program using the package */

#define VOLUME_BITMAP_GROWSIZE	16	/* bytes, => 128vnodes */
					/* Must be a multiple of 4 (1 word) !! */
#define VOLUME_HASH_TABLE_SIZE 128	/* Must be a power of 2!! */
#define VOLUME_HASH(volumeId) (volumeId&(VOLUME_HASH_TABLE_SIZE-1))
private Volume *VolumeHashTable[VOLUME_HASH_TABLE_SIZE];

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
#include "rx/rx_queue.h"
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

struct Lock vol_listLock;	/* Lock obtained when listing volumes:  prevents a volume from being missed if the volume is attached during a list volumes */

extern struct Lock FSYNC_handler_lock;

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

int VolumeCacheSize = 200, VolumeGets = 0, VolumeReplacements = 0, Vlooks = 0;


int
VInitVolumePackage(ProgramType pt, int nLargeVnodes, int nSmallVnodes,
		   int connect, int volcache)
{
    int errors = 0;		/* Number of errors while finding vice partitions. */
    struct timeval tv;
    struct timezone tz;

    programType = pt;

#ifdef AFS_PTHREAD_ENV
    assert(pthread_mutex_init(&vol_glock_mutex, NULL) == 0);
    assert(pthread_mutex_init(&vol_attach_mutex, NULL) == 0);
    assert(pthread_mutex_init(&vol_fsync_mutex, NULL) == 0);
    assert(pthread_mutex_init(&vol_trans_mutex, NULL) == 0);
    assert(pthread_cond_init(&vol_put_volume_cond, NULL) == 0);
    assert(pthread_cond_init(&vol_sleep_cond, NULL) == 0);
#else /* AFS_PTHREAD_ENV */
    IOMGR_Initialize();
#endif /* AFS_PTHREAD_ENV */
    Lock_Init(&vol_listLock);
    Lock_Init(&FSYNC_handler_lock);

    srandom(time(0));		/* For VGetVolumeInfo */
    gettimeofday(&tv, &tz);
    TimeZoneCorrection = tz.tz_minuteswest * 60;

    /* Ok, we have done enough initialization that fileserver can 
     * start accepting calls, even though the volumes may not be 
     * available just yet.
     */
    VInit = 1;

    if (programType == fileServer) {
	/* File server or "stand" */
	FSYNC_fsInit();
    }

    if (volcache > VolumeCacheSize)
	VolumeCacheSize = volcache;
    InitLRU(VolumeCacheSize);

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
	int i, len;
	pthread_t tid;
	pthread_attr_t attrs;

	assert(pthread_cond_init(&params.thread_done_cv,NULL) == 0);
	queue_Init(&params);
	params.n_threads_complete = 0;

	/* create partition work queue */
	for (len=0, diskP = DiskPartitionList; diskP; diskP = diskP->next, len++) {
	    dpq = (diskpartition_queue_t *) malloc(sizeof(struct diskpartition_queue_t));
	    assert(dpq != NULL);
	    dpq->diskP = diskP;
	    queue_Prepend(&params,dpq);
	}

	assert(pthread_attr_init(&attrs) == 0);
	assert(pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED) == 0);

	len = MIN(len, vol_attach_threads);
	
	VOL_LOCK;
	for (i=0; i < len; i++) {
	    assert(pthread_create
		   (&tid, &attrs, &VInitVolumePackageThread,
		    &params) == 0);
	}

	while(params.n_threads_complete < len) {
  	    pthread_cond_wait(&params.thread_done_cv,&vol_glock_mutex);
	}
	VOL_UNLOCK;

	assert(pthread_cond_destroy(&params.thread_done_cv) == 0);

#else /* AFS_PTHREAD_ENV */
	DIR *dirp;
	struct dirent *dp;

	/* Attach all the volumes in this partition */
	for (diskP = DiskPartitionList; diskP; diskP = diskP->next) {
	    int nAttached = 0, nUnattached = 0;
	    Log("Partition %s: attaching volumes\n", diskP->name);
	    dirp = opendir(VPartitionPath(diskP));
	    assert(dirp);
	    while ((dp = readdir(dirp))) {
		char *p;
		p = strrchr(dp->d_name, '.');
		if (p != NULL && strcmp(p, VHDREXT) == 0) {
		    Error error;
		    Volume *vp;
		    vp = VAttachVolumeByName(&error, diskP->name, dp->d_name,
					     V_VOLUPD);
		    (*(vp ? &nAttached : &nUnattached))++;
		    if (error == VOFFLINE)
			Log("Volume %d stays offline (/vice/offline/%s exists)\n", VolumeNumber(dp->d_name), dp->d_name);
		    else if (LogLevel >= 5) {
			Log("Partition %s: attached volume %d (%s)\n",
			    diskP->name, VolumeNumber(dp->d_name),
			    dp->d_name);
		    }
		    if (vp) {
			VPutVolume(vp);
		    }
		}
	    }
	    Log("Partition %s: attached %d volumes; %d volumes not attached\n", diskP->name, nAttached, nUnattached);
	    closedir(dirp);
	}
#endif /* AFS_PTHREAD_ENV */
    }

    VInit = 2;			/* Initialized, and all volumes have been attached */
    if (programType == volumeUtility && connect) {
	if (!VConnectFS()) {
	    Log("Unable to connect to file server; aborted\n");
	    Lock_Destroy(&FSYNC_handler_lock);
	    exit(1);
	}
    }
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

	Log("Partition %s: attaching volumes\n", diskP->name);
	dirp = opendir(VPartitionPath(diskP));
	assert(dirp);
	while ((dp = readdir(dirp))) {
	    char *p;
	    p = strrchr(dp->d_name, '.');
	    if (p != NULL && strcmp(p, VHDREXT) == 0) {
	        Error error;
		Volume *vp;
		vp = VAttachVolumeByName(&error, diskP->name, dp->d_name,
					 V_VOLUPD);
		(*(vp ? &nAttached : &nUnattached))++;
		if (error == VOFFLINE)
		    Log("Volume %d stays offline (/vice/offline/%s exists)\n", VolumeNumber(dp->d_name), dp->d_name);
		else if (LogLevel >= 5) {
		    Log("Partition %s: attached volume %d (%s)\n",
			diskP->name, VolumeNumber(dp->d_name),
			dp->d_name);
		}
		if (vp) {
		    VPutVolume(vp);
		}
	    }
	}
	Log("Partition %s: attached %d volumes; %d volumes not attached\n", diskP->name, nAttached, nUnattached);
	closedir(dirp);
	VOL_LOCK;
    }

    params->n_threads_complete++;
    pthread_cond_signal(&params->thread_done_cv);
    VOL_UNLOCK;
    return NULL;
}
#endif /* AFS_PTHREAD_ENV */

/* This must be called by any volume utility which needs to run while the
   file server is also running.  This is separated from VInitVolumePackage so
   that a utility can fork--and each of the children can independently
   initialize communication with the file server */
int
VConnectFS(void)
{
    int retVal;
    VOL_LOCK;
    retVal = VConnectFS_r();
    VOL_UNLOCK;
    return retVal;
}

int
VConnectFS_r(void)
{
    int rc;
    assert(VInit == 2 && programType == volumeUtility);
    rc = FSYNC_clientInit();
    if (rc)
	VInit = 3;
    return rc;
}

void
VDisconnectFS_r(void)
{
    assert(programType == volumeUtility);
    FSYNC_clientFinis();
    VInit = 2;
}

void
VDisconnectFS(void)
{
    VOL_LOCK;
    VDisconnectFS_r();
    VOL_UNLOCK;
}

void
VShutdown_r(void)
{
    int i;
    register Volume *vp, *np;
    register afs_int32 code;

    Log("VShutdown:  shutting down on-line volumes...\n");
    for (i = 0; i < VOLUME_HASH_TABLE_SIZE; i++) {
	/* try to hold first volume in the hash table */
	for (vp = VolumeHashTable[i]; vp; vp = vp->hashNext) {
	    code = VHold_r(vp);
	    if (code == 0)
		break;		/* got it */
	    /* otherwise we go around again, trying another volume */
	}
	while (vp) {
	    if (LogLevel >= 5)
		Log("VShutdown:  Attempting to take volume %u offline.\n",
		    vp->hashid);
	    /* first compute np before releasing vp, in case vp disappears
	     * after releasing.  Hold it, so it doesn't disapear.  If we
	     * can't hold it, try the next one in the chain.  Invariant
	     * at the top of this loop is that vp is held (has extra ref count).
	     */
	    for (np = vp->hashNext; np; np = np->hashNext) {
		code = VHold_r(np);
		if (code == 0)
		    break;	/* got it */
	    }
	    /* next, take the volume offline (drops reference count) */
	    VOffline_r(vp, "File server was shut down");
	    vp = np;		/* next guy to try */
	}
    }
    Log("VShutdown:  complete.\n");
}

void
VShutdown(void)
{
    VOL_LOCK;
    VShutdown_r();
    VOL_UNLOCK;
}


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

/* VolumeHeaderToDisk
 * Allows for storing 64 bit inode numbers in on-disk volume header
 * file.
 */
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
 * Reads volume header file from disk, convering 64 bit inodes
 * if required. Makes the assumption that AFS has *always* 
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


void
WriteVolumeHeader_r(ec, vp)
     Error *ec;
     Volume *vp;
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

/* Attach an existing volume, given its pathname, and return a
   pointer to the volume header information.  The volume also
   normally goes online at this time.  An offline volume
   must be reattached to make it go online */
Volume *
VAttachVolumeByName(Error * ec, char *partition, char *name, int mode)
{
    Volume *retVal;
    VATTACH_LOCK;
    VOL_LOCK;
    retVal = VAttachVolumeByName_r(ec, partition, name, mode);
    VOL_UNLOCK;
    VATTACH_UNLOCK;
    return retVal;
}

Volume *
VAttachVolumeByName_r(Error * ec, char *partition, char *name, int mode)
{
    register Volume *vp;
    int fd, n;
    struct afs_stat status;
    struct VolumeDiskHeader diskHeader;
    struct VolumeHeader iheader;
    struct DiskPartition *partp;
    char path[64];
    int isbusy = 0;
    *ec = 0;
    if (programType == volumeUtility) {
	assert(VInit == 3);
	VLockPartition_r(partition);
    }
    if (programType == fileServer) {
	vp = VGetVolume_r(ec, VolumeNumber(name));
	if (vp) {
	    if (V_inUse(vp))
		return vp;
	    if (vp->specialStatus == VBUSY)
		isbusy = 1;
	    VDetachVolume_r(ec, vp);
	    if (*ec) {
		Log("VAttachVolume: Error detaching volume (%s)\n", name);
	    }
	}
    }

    if (!(partp = VGetPartition_r(partition, 0))) {
	*ec = VNOVOL;
	Log("VAttachVolume: Error getting partition (%s)\n", partition);
	goto done;
    }

    *ec = 0;
    strcpy(path, VPartitionPath(partp));
    strcat(path, "/");
    strcat(path, name);
    VOL_UNLOCK;
    if ((fd = afs_open(path, O_RDONLY)) == -1 || afs_fstat(fd, &status) == -1) {
	Log("VAttachVolume: Failed to open %s (errno %d)\n", path, errno);
	if (fd > -1)
	    close(fd);
	VOL_LOCK;
	*ec = VNOVOL;
	goto done;
    }
    n = read(fd, &diskHeader, sizeof(diskHeader));
    close(fd);
    VOL_LOCK;
    if (n != sizeof(diskHeader)
	|| diskHeader.stamp.magic != VOLUMEHEADERMAGIC) {
	Log("VAttachVolume: Error reading volume header %s\n", path);
	*ec = VSALVAGE;
	goto done;
    }
    if (diskHeader.stamp.version != VOLUMEHEADERVERSION) {
	Log("VAttachVolume: Volume %s, version number is incorrect; volume needs salvaged\n", path);
	*ec = VSALVAGE;
	goto done;
    }

    DiskToVolumeHeader(&iheader, &diskHeader);
    if (programType == volumeUtility && mode != V_SECRETLY && mode != V_PEEK) {
	if (FSYNC_askfs(iheader.id, partition, FSYNC_NEEDVOLUME, mode)
	    == FSYNC_DENIED) {
	    Log("VAttachVolume: attach of volume %u apparently denied by file server\n", iheader.id);
	    *ec = VNOVOL;	/* XXXX */
	    goto done;
	}
    }

    vp = attach2(ec, path, &iheader, partp, isbusy);
    if (programType == volumeUtility && vp) {
	/* duplicate computation in fssync.c about whether the server
	 * takes the volume offline or not.  If the volume isn't
	 * offline, we must not return it when we detach the volume,
	 * or the server will abort */
	if (mode == V_READONLY || mode == V_PEEK
	    || (!VolumeWriteable(vp) && (mode == V_CLONE || mode == V_DUMP)))
	    vp->needsPutBack = 0;
	else
	    vp->needsPutBack = 1;
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
    if (programType == volumeUtility && vp == NULL &&
	mode != V_SECRETLY && mode != V_PEEK) {
	FSYNC_askfs(iheader.id, partition, FSYNC_ON, 0);
    } else if (programType == fileServer && vp) {
	V_needsCallback(vp) = 0;
#ifdef	notdef
	if (VInit >= 2 && V_BreakVolumeCallbacks) {
	    Log("VAttachVolume: Volume %u was changed externally; breaking callbacks\n", V_id(vp));
	    (*V_BreakVolumeCallbacks) (V_id(vp));
	}
#endif
	VUpdateVolume_r(ec, vp);
	if (*ec) {
	    Log("VAttachVolume: Error updating volume\n");
	    if (vp)
		VPutVolume_r(vp);
	    goto done;
	}
	if (VolumeWriteable(vp) && V_dontSalvage(vp) == 0) {
	    /* This is a hack: by temporarily settint the incore
	     * dontSalvage flag ON, the volume will be put back on the
	     * Update list (with dontSalvage OFF again).  It will then
	     * come back in N minutes with DONT_SALVAGE eventually
	     * set.  This is the way that volumes that have never had
	     * it set get it set; or that volumes that have been
	     * offline without DONT SALVAGE having been set also
	     * eventually get it set */
	    V_dontSalvage(vp) = DONT_SALVAGE;
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
    if (*ec)
	return NULL;
    else
	return vp;
}

private Volume *
attach2(Error * ec, char *path, register struct VolumeHeader * header,
	struct DiskPartition * partp, int isbusy)
{
    register Volume *vp;

    VOL_UNLOCK;

    vp = (Volume *) calloc(1, sizeof(Volume));
    assert(vp != NULL);
    vp->specialStatus = (byte) (isbusy ? VBUSY : 0);
    vp->device = partp->device;
    vp->partition = partp;
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

    VOL_LOCK;
    vp->cacheCheck = ++VolumeCacheCheck;
    /* just in case this ever rolls over */
    if (!vp->cacheCheck)
	vp->cacheCheck = ++VolumeCacheCheck;
    GetVolumeHeader(vp);
    VOL_UNLOCK;

    (void)ReadHeader(ec, V_diskDataHandle(vp), (char *)&V_disk(vp),
		     sizeof(V_disk(vp)), VOLUMEINFOMAGIC, VOLUMEINFOVERSION);

    VOL_LOCK;
    if (*ec) {
	Log("VAttachVolume: Error reading diskDataHandle vol header %s; error=%u\n", path, *ec);
    }
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
	VOL_UNLOCK;
	(void)ReadHeader(ec, vp->vnodeIndex[vSmall].handle,
			 (char *)&iHead, sizeof(iHead),
			 SMALLINDEXMAGIC, SMALLINDEXVERSION);
	VOL_LOCK;
	if (*ec) {
	    Log("VAttachVolume: Error reading smallVnode vol header %s; error=%u\n", path, *ec);
	}
    }
    if (!*ec) {
	struct IndexFileHeader iHead;
	VOL_UNLOCK;
	(void)ReadHeader(ec, vp->vnodeIndex[vLarge].handle,
			 (char *)&iHead, sizeof(iHead),
			 LARGEINDEXMAGIC, LARGEINDEXVERSION);
	VOL_LOCK;
	if (*ec) {
	    Log("VAttachVolume: Error reading largeVnode vol header %s; error=%u\n", path, *ec);
	}
    }
#ifdef AFS_NAMEI_ENV
    if (!*ec) {
	struct versionStamp stamp;
	VOL_UNLOCK;
	(void)ReadHeader(ec, V_linkHandle(vp), (char *)&stamp,
			 sizeof(stamp), LINKTABLEMAGIC, LINKTABLEVERSION);
	VOL_LOCK;
	if (*ec) {
	    Log("VAttachVolume: Error reading namei vol header %s; error=%u\n", path, *ec);
	}
    }
#endif
    if (*ec) {
	Log("VAttachVolume: Error attaching volume %s; volume needs salvage; error=%u\n", path, *ec);
	FreeVolume(vp);
	return NULL;
    }
    if (V_needsSalvaged(vp)) {
	if (vp->specialStatus)
	    vp->specialStatus = 0;
	Log("VAttachVolume: volume salvage flag is ON for %s; volume needs salvage\n", path);
	*ec = VSALVAGE;
	FreeVolume(vp);
	return NULL;
    }
    if (programType == fileServer) {
#ifndef FAST_RESTART
	if (V_inUse(vp) && VolumeWriteable(vp)) {
	    if (!V_needsSalvaged(vp)) {
		V_needsSalvaged(vp) = 1;
		VUpdateVolume_r(ec, vp);
	    }
	    FreeVolume(vp);
	    Log("VAttachVolume: volume %s needs to be salvaged; not attached.\n", path);
	    *ec = VSALVAGE;
	    return NULL;
	}
#endif /* FAST_RESTART */
	if (V_destroyMe(vp) == DESTROY_ME) {
	    FreeVolume(vp);
	    Log("VAttachVolume: volume %s is junk; it should be destroyed at next salvage\n", path);
	    *ec = VNOVOL;
	    return NULL;
	}
    }

    AddVolumeToHashTable(vp, V_id(vp));
    vp->nextVnodeUnique = V_uniquifier(vp);
    vp->vnodeIndex[vSmall].bitmap = vp->vnodeIndex[vLarge].bitmap = NULL;
#ifndef BITMAP_LATER
    if (programType == fileServer && VolumeWriteable(vp)) {
	int i;
	for (i = 0; i < nVNODECLASSES; i++) {
	    VOL_UNLOCK;
	    GetBitmap(ec, vp, i);
	    VOL_LOCK;
	    if (*ec) {
		FreeVolume(vp);
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
    VATTACH_LOCK;
    VOL_LOCK;
    retVal = VAttachVolume_r(ec, volumeId, mode);
    VOL_UNLOCK;
    VATTACH_UNLOCK;
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
static int
VHold_r(register Volume * vp)
{
    Error error;

    if (vp->nUsers == 0 && !GetVolumeHeader(vp)) {
	VolumeReplacements++;
	ReadHeader(&error, V_diskDataHandle(vp), (char *)&V_disk(vp),
		   sizeof(V_disk(vp)), VOLUMEINFOMAGIC, VOLUMEINFOVERSION);
	if (error)
	    return error;
    }
    vp->nUsers++;
    return 0;
}

static int
VHold(register Volume * vp)
{
    int retVal;
    VOL_LOCK;
    retVal = VHold_r(vp);
    VOL_UNLOCK;
    return retVal;
}

void
VTakeOffline_r(register Volume * vp)
{
    assert(vp->nUsers > 0);
    assert(programType == fileServer);
    vp->goingOffline = 1;
    V_needsSalvaged(vp) = 1;
}

void
VTakeOffline(register Volume * vp)
{
    VOL_LOCK;
    VTakeOffline_r(vp);
    VOL_UNLOCK;
}

void
VPutVolume_r(register Volume * vp)
{
    assert(--vp->nUsers >= 0);
    if (vp->nUsers == 0) {
	ReleaseVolumeHeader(vp->header);
	if (vp->goingOffline) {
	    Error error;
	    assert(programType == fileServer);
	    vp->goingOffline = 0;
	    V_inUse(vp) = 0;
	    VUpdateVolume_r(&error, vp);
	    VCloseVolumeHandles_r(vp);
	    if (LogLevel) {
		Log("VOffline: Volume %u (%s) is now offline", V_id(vp),
		    V_name(vp));
		if (V_offlineMessage(vp)[0])
		    Log(" (%s)", V_offlineMessage(vp));
		Log("\n");
	    }
#ifdef AFS_PTHREAD_ENV
	    assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
#else /* AFS_PTHREAD_ENV */
	    LWP_NoYieldSignal(VPutVolume);
#endif /* AFS_PTHREAD_ENV */
	}
	if (vp->shuttingDown) {
	    VReleaseVolumeHandles_r(vp);
	    FreeVolume(vp);
	    if (programType == fileServer)
#ifdef AFS_PTHREAD_ENV
		assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
#else /* AFS_PTHREAD_ENV */
		LWP_NoYieldSignal(VPutVolume);
#endif /* AFS_PTHREAD_ENV */
	}
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
VGetVolume(Error * ec, VolId volumeId)
{
    Volume *retVal;
    VOL_LOCK;
    retVal = VGetVolume_r(ec, volumeId);
    VOL_UNLOCK;
    return retVal;
}

Volume *
VGetVolume_r(Error * ec, VolId volumeId)
{
    Volume *vp;
    unsigned short V0 = 0, V1 = 0, V2 = 0, V3 = 0, V4 = 0, V5 = 0, V6 =
	0, V7 = 0, V8 = 0, V9 = 0;
    unsigned short V10 = 0, V11 = 0, V12 = 0, V13 = 0, V14 = 0, V15 = 0;

    for (;;) {
	*ec = 0;
	V0++;
	for (vp = VolumeHashTable[VOLUME_HASH(volumeId)];
	     vp && vp->hashid != volumeId; vp = vp->hashNext)
	    Vlooks++;

	if (!vp) {
	    V1++;
	    if (VInit < 2) {
		V2++;
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

	V3++;
	VolumeGets++;
	if (vp->nUsers == 0 && !GetVolumeHeader(vp)) {
	    V5++;
	    VolumeReplacements++;
	    ReadHeader(ec, V_diskDataHandle(vp), (char *)&V_disk(vp),
		       sizeof(V_disk(vp)), VOLUMEINFOMAGIC,
		       VOLUMEINFOVERSION);
	    if (*ec) {
		V6++;
		/* Only log the error if it was a totally unexpected error.  Simply
		 * a missing inode is likely to be caused by the volume being deleted */
		if (errno != ENXIO || LogLevel)
		    Log("Volume %u: couldn't reread volume header\n",
			vp->hashid);
		FreeVolume(vp);
		vp = NULL;
		break;
	    }
	}
	V7++;
	if (vp->shuttingDown) {
	    V8++;
	    *ec = VNOVOL;
	    vp = NULL;
	    break;
	}
	if (programType == fileServer) {
	    V9++;
	    if (vp->goingOffline) {
		V10++;
#ifdef AFS_PTHREAD_ENV
		pthread_cond_wait(&vol_put_volume_cond, &vol_glock_mutex);
#else /* AFS_PTHREAD_ENV */
		LWP_WaitProcess(VPutVolume);
#endif /* AFS_PTHREAD_ENV */
		continue;
	    }
	    if (vp->specialStatus) {
		V11++;
		*ec = vp->specialStatus;
	    } else if (V_inService(vp) == 0 || V_blessed(vp) == 0) {
		V12++;
		*ec = VNOVOL;
	    } else if (V_inUse(vp) == 0) {
		V13++;
		*ec = VOFFLINE;
	    } else {
		V14++;
	    }
	}
	break;
    }
    V15++;
    /* if no error, bump nUsers */
    if (vp)
	vp->nUsers++;

    assert(vp || *ec);
    return vp;
}


/* For both VForceOffline and VOffline, we close all relevant handles.
 * For VOffline, if we re-attach the volume, the files may possible be
 * different than before. 
 */
static void
VReleaseVolumeHandles_r(Volume * vp)
{
    DFlushVolume(V_id(vp));
    VReleaseVnodeFiles_r(vp);

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
}

/* Force the volume offline, set the salvage flag.  No further references to
 * the volume through the volume package will be honored. */
void
VForceOffline_r(Volume * vp)
{
    Error error;
    if (!V_inUse(vp))
	return;
    strcpy(V_offlineMessage(vp),
	   "Forced offline due to internal error: volume needs to be salvaged");
    Log("Volume %u forced offline:  it needs salvaging!\n", V_id(vp));
    V_inUse(vp) = 0;
    vp->goingOffline = 0;
    V_needsSalvaged(vp) = 1;
    VUpdateVolume_r(&error, vp);
#ifdef AFS_PTHREAD_ENV
    assert(pthread_cond_broadcast(&vol_put_volume_cond) == 0);
#else /* AFS_PTHREAD_ENV */
    LWP_NoYieldSignal(VPutVolume);
#endif /* AFS_PTHREAD_ENV */

    VReleaseVolumeHandles_r(vp);

}

void
VForceOffline(Volume * vp)
{
    VOL_LOCK;
    VForceOffline_r(vp);
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
    VPutVolume_r(vp);
    vp = VGetVolume_r(&error, vid);	/* Wait for it to go offline */
    if (vp)			/* In case it was reattached... */
	VPutVolume_r(vp);
}

void
VOffline(Volume * vp, char *message)
{
    VOL_LOCK;
    VOffline_r(vp, message);
    VOL_UNLOCK;
}

/* For VDetachVolume, we close all cached file descriptors, but keep
 * the Inode handles in case we need to read from a busy volume.
 */
static void
VCloseVolumeHandles_r(Volume * vp)
{
    DFlushVolume(V_id(vp));
    VCloseVnodeFiles_r(vp);

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
}

/* This gets used for the most part by utility routines that don't want
 * to keep all the volume headers around.  Generally, the file server won't
 * call this routine, because then the offline message in the volume header
 * (or other information) will still be available to clients. For NAMEI, also
 * close the file handles.
 */
void
VDetachVolume_r(Error * ec, Volume * vp)
{
    VolumeId volume;
    struct DiskPartition *tpartp;
    int notifyServer, useDone;

    *ec = 0;			/* always "succeeds" */
    if (programType == volumeUtility) {
	notifyServer = vp->needsPutBack;
	useDone = (V_destroyMe(vp) == DESTROY_ME);
    }
    tpartp = vp->partition;
    volume = V_id(vp);
    DeleteVolumeFromHashTable(vp);
    vp->shuttingDown = 1;
    VPutVolume_r(vp);
    /* Will be detached sometime in the future--this is OK since volume is offline */

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
	if (useDone)
	    /* don't put online */
	    FSYNC_askfs(volume, tpartp->name, FSYNC_DONE, 0);
	else {
	    /* fs can use it again */
	    FSYNC_askfs(volume, tpartp->name, FSYNC_ON, 0);
	    /* Dettaching it so break all callbacks on it */
	    if (V_BreakVolumeCallbacks) {
		Log("volume %u detached; breaking all call backs\n", volume);
		(*V_BreakVolumeCallbacks) (volume);
	    }
	}
    }
}

void
VDetachVolume(Error * ec, Volume * vp)
{
    VOL_LOCK;
    VDetachVolume_r(ec, vp);
    VOL_UNLOCK;
}


VnodeId
VAllocBitmapEntry_r(Error * ec, Volume * vp, register struct vnodeIndex
		    *index)
{
    register byte *bp, *ep;
    *ec = 0;
    /* This test is probably redundant */
    if (!VolumeWriteable(vp)) {
	*ec = (bit32) VREADONLY;
	return 0;
    }
#ifdef BITMAP_LATER
    if ((programType == fileServer) && !index->bitmap) {
	int i;
	int wasVBUSY = 0;
	if (vp->specialStatus == VBUSY) {
	    if (vp->goingOffline) {	/* vos dump waiting for the volume to
					 * go offline. We probably come here
					 * from AddNewReadableResidency */
		wasVBUSY = 1;
	    } else {
		VOL_UNLOCK;
		while (vp->specialStatus == VBUSY)
#ifdef AFS_PTHREAD_ENV
		    sleep(2);
#else /* AFS_PTHREAD_ENV */
		    IOMGR_Sleep(2);
#endif /* AFS_PTHREAD_ENV */
		VOL_LOCK;
	    }
	}
	if (!index->bitmap) {
	    vp->specialStatus = VBUSY;	/* Stop anyone else from using it. */
	    for (i = 0; i < nVNODECLASSES; i++) {
		VOL_UNLOCK;
		GetBitmap(ec, vp, i);
		VOL_LOCK;
		if (*ec) {
		    vp->specialStatus = 0;
		    vp->shuttingDown = 1;	/* Let who has it free it. */
		    return NULL;
		}
	    }
	    if (!wasVBUSY)
		vp->specialStatus = 0;	/* Allow others to have access. */
	}
    }
#endif /* BITMAP_LATER */
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
	    return (VnodeId) ((bp - index->bitmap) * 8 + o);
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
    return index->bitmapOffset * 8;
}

VnodeId
VAllocBitmapEntry(Error * ec, Volume * vp, register struct vnodeIndex * index)
{
    VnodeId retVal;
    VOL_LOCK;
    retVal = VAllocBitmapEntry_r(ec, vp, index);
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

void
VUpdateVolume_r(Error * ec, Volume * vp)
{
    *ec = 0;
    if (programType == fileServer)
	V_uniquifier(vp) =
	    (V_inUse(vp) ? V_nextVnodeUnique(vp) +
	     200 : V_nextVnodeUnique(vp));
    /*printf("Writing volume header for '%s'\n", V_name(vp)); */
    WriteVolumeHeader_r(ec, vp);
    if (*ec) {
	Log("VUpdateVolume: error updating volume header, volume %u (%s)\n",
	    V_id(vp), V_name(vp));
	VForceOffline_r(vp);
    }
}

void
VUpdateVolume(Error * ec, Volume * vp)
{
    VOL_LOCK;
    VUpdateVolume_r(ec, vp);
    VOL_UNLOCK;
}

void
VSyncVolume_r(Error * ec, Volume * vp)
{
    FdHandle_t *fdP;
    VUpdateVolume_r(ec, vp);
    if (!ec) {
	int code;
	fdP = IH_OPEN(V_diskDataHandle(vp));
	assert(fdP != NULL);
	code = FDH_SYNC(fdP);
	assert(code == 0);
	FDH_CLOSE(fdP);
    }
}

void
VSyncVolume(Error * ec, Volume * vp)
{
    VOL_LOCK;
    VSyncVolume_r(ec, vp);
    VOL_UNLOCK;
}

static void
FreeVolume(Volume * vp)
{
    int i;
    if (!vp)
	return;
    for (i = 0; i < nVNODECLASSES; i++)
	if (vp->vnodeIndex[i].bitmap)
	    free(vp->vnodeIndex[i].bitmap);
    FreeVolumeHeader(vp);
    DeleteVolumeFromHashTable(vp);
    free(vp);
}

static void
GetBitmap(Error * ec, Volume * vp, VnodeClass class)
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

    *ec = 0;

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
}

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

int
VolumeNumber(char *name)
{
    if (*name == '/')
	name++;
    return atoi(name + 1);
}

char *
VolumeExternalName(VolumeId volumeId)
{
    static char name[VMAXPATHLEN];
    (void)afs_snprintf(name, sizeof name, VFORMAT, volumeId);
    return name;
}

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
	register ndays, i;

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
	VUpdateVolume_r(&error, vp);
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
    static int FifteenMinuteCounter = 0;

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
    if (++FifteenMinuteCounter == 3) {
	FifteenMinuteCounter = 0;
	VScanUpdateList();
    }
}

void
VSetDiskUsage(void)
{
    VOL_LOCK;
    VSetDiskUsage_r();
    VOL_UNLOCK;
}

/* The number of minutes that a volume hasn't been updated before the
 * "Dont salvage" flag in the volume header will be turned on */
#define SALVAGE_INTERVAL	(10*60)

static VolumeId *UpdateList;	/* Pointer to array of Volume ID's */
static int nUpdatedVolumes;	/* Updated with entry in UpdateList, salvage after crash flag on */
static int updateSize;		/* number of entries possible */
#define UPDATE_LIST_SIZE 100	/* size increment */

void
VAddToVolumeUpdateList_r(Error * ec, Volume * vp)
{
    *ec = 0;
    vp->updateTime = FT_ApproxTime();
    if (V_dontSalvage(vp) == 0)
	return;
    V_dontSalvage(vp) = 0;
    VSyncVolume_r(ec, vp);
    if (*ec)
	return;
    if (!UpdateList) {
	updateSize = UPDATE_LIST_SIZE;
	UpdateList = (VolumeId *) malloc(sizeof(VolumeId) * updateSize);
    } else {
	if (nUpdatedVolumes == updateSize) {
	    updateSize += UPDATE_LIST_SIZE;
	    UpdateList =
		(VolumeId *) realloc(UpdateList,
				     sizeof(VolumeId) * updateSize);
	}
    }
    assert(UpdateList != NULL);
    UpdateList[nUpdatedVolumes++] = V_id(vp);
}

static void
VScanUpdateList(void)
{
    register int i, gap;
    register Volume *vp;
    Error error;
    afs_uint32 now = FT_ApproxTime();
    /* Be careful with this code, since it works with interleaved calls to AddToVolumeUpdateList */
    for (i = gap = 0; i < nUpdatedVolumes; i++) {
	vp = VGetVolume_r(&error, UpdateList[i - gap] = UpdateList[i]);
	if (error) {
	    gap++;
	} else if (vp->nUsers == 1 && now - vp->updateTime > SALVAGE_INTERVAL) {
	    V_dontSalvage(vp) = DONT_SALVAGE;
	    VUpdateVolume_r(&error, vp);	/* No need to fsync--not critical */
	    gap++;
	}
	if (vp)
	    VPutVolume_r(vp);
#ifndef AFS_PTHREAD_ENV
	IOMGR_Poll();
#endif /* !AFS_PTHREAD_ENV */
    }
    nUpdatedVolumes -= gap;
}

/***************************************************/
/* Add on routines to manage a volume header cache */
/***************************************************/

static struct volHeader *volumeLRU;

/* Allocate a bunch of headers; string them together */
static void
InitLRU(int howMany)
{
    register struct volHeader *hp;
    if (programType != fileServer)
	return;
    hp = (struct volHeader *)(calloc(howMany, sizeof(struct volHeader)));
    while (howMany--)
	ReleaseVolumeHeader(hp++);
}

/* Get a volume header from the LRU list; update the old one if necessary */
/* Returns 1 if there was already a header, which is removed from the LRU list */
static int
GetVolumeHeader(register Volume * vp)
{
    Error error;
    register struct volHeader *hd;
    int old;
    static int everLogged = 0;

    old = (vp->header != NULL);	/* old == volume already has a header */
    if (programType != fileServer) {
	if (!vp->header) {
	    hd = (struct volHeader *)calloc(1, sizeof(*vp->header));
	    assert(hd != NULL);
	    vp->header = hd;
	    hd->back = vp;
	}
    } else {
	if (old) {
	    hd = vp->header;
	    if (volumeLRU == hd)
		volumeLRU = hd->next;
	    assert(hd->back == vp);
	} else {
	    if (volumeLRU)
		/* not currently in use and least recently used */
		hd = volumeLRU->prev;
	    else {
		hd = (struct volHeader *)calloc(1, sizeof(*vp->header));
		/* make it look like single elt LRU */
		hd->prev = hd->next = hd;
		if (!everLogged) {
		    Log("****Allocated more volume headers, probably leak****\n");
		    everLogged = 1;
		}
	    }
	    if (hd->back) {
		if (hd->diskstuff.inUse) {
		    WriteVolumeHeader_r(&error, hd->back);
		    /* Ignore errors; catch them later */
		}
		hd->back->header = 0;
	    }
	    hd->back = vp;
	    vp->header = hd;
	}
	if (hd->next) {		/* hd->next != 0 --> in LRU chain (we zero it later) */
	    hd->prev->next = hd->next;	/* pull hd out of LRU list */
	    hd->next->prev = hd->prev;	/* if hd only element, this is noop */
	}
	hd->next = hd->prev = 0;
	/* if not in LRU chain, next test won't be true */
	if (hd == volumeLRU)	/* last header item, turn into empty list */
	    volumeLRU = NULL;
    }
    return old;
}

/* Put it at the top of the LRU chain */
static void
ReleaseVolumeHeader(register struct volHeader *hd)
{
    if (programType != fileServer)
	return;
    if (!hd || hd->next)	/* no header, or header already released */
	return;
    if (!volumeLRU) {
	hd->next = hd->prev = hd;
    } else {
	hd->prev = volumeLRU->prev;
	hd->next = volumeLRU;
	hd->prev->next = hd->next->prev = hd;
    }
    volumeLRU = hd;
}

static void
FreeVolumeHeader(register Volume * vp)
{
    register struct volHeader *hd = vp->header;
    if (!hd)
	return;
    if (programType == fileServer) {
	ReleaseVolumeHeader(hd);
	hd->back = 0;
    } else {
	free(hd);
    }
    vp->header = 0;
}


/***************************************************/
/* Routines to add volume to hash chain, delete it */
/***************************************************/

static void
AddVolumeToHashTable(register Volume * vp, int hashid)
{
    int hash = VOLUME_HASH(hashid);
    vp->hashid = hashid;
    vp->hashNext = VolumeHashTable[hash];
    VolumeHashTable[hash] = vp;
    vp->vnodeHashOffset = VolumeHashOffset_r();
}

static void
DeleteVolumeFromHashTable(register Volume * vp)
{
    int hash = VOLUME_HASH(vp->hashid);
    if (VolumeHashTable[hash] == vp)
	VolumeHashTable[hash] = vp->hashNext;
    else {
	Volume *tvp = VolumeHashTable[hash];
	if (tvp == NULL)
	    return;
	while (tvp->hashNext && tvp->hashNext != vp)
	    tvp = tvp->hashNext;
	if (tvp->hashNext == NULL)
	    return;
	tvp->hashNext = vp->hashNext;
    }
    vp->hashid = 0;
}

void
VPrintCacheStats_r(void)
{
    register struct VnodeClassInfo *vcp;
    vcp = &VnodeClassInfo[vLarge];
    Log("Large vnode cache, %d entries, %d allocs, %d gets (%d reads), %d writes\n", vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    vcp = &VnodeClassInfo[vSmall];
    Log("Small vnode cache,%d entries, %d allocs, %d gets (%d reads), %d writes\n", vcp->cacheSize, vcp->allocs, vcp->gets, vcp->reads, vcp->writes);
    Log("Volume header cache, %d entries, %d gets, %d replacements\n",
	VolumeCacheSize, VolumeGets, VolumeReplacements);
}

void
VPrintCacheStats(void)
{
    VOL_LOCK;
    VPrintCacheStats_r();
    VOL_UNLOCK;
}

