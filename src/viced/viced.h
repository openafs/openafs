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

/*  file.h	- include file for the File Server			*/
/*									*/
/*  Date: 5/1/85							*/
/*									*/
/************************************************************************/
/*
 * Revision 2.2  90/08/29  15:12:11
 * Cleanups.
 * 
 * Revision 2.1  90/08/07  19:46:16
 * Start with clean version to sync test and dev trees.
 * */

#ifndef _AFS_VICED_VICED_H
#define _AFS_VICED_VICED_H

#include <afs/afssyscalls.h>
#include <afs/afsutil.h>
#include "fs_stats.h"		/*Defs for xstat-based statistics */

#define AFS_HARDDEADTIME	120

typedef struct DirHandle {
    /* device+inode+vid are low level disk addressing + validity check */
    /* vid+vnode+unique+cacheCheck are to guarantee validity of cached copy */
    /* ***NOTE*** size of this stucture must not exceed size in buffer
     * package (dir/buffer.c. Also, dir/buffer uses the first int as a
     * hash into the page hash table.
     * ***NOTE*** The volume, device and inode numbers used to compare
     * fids are copied out of the handle to allow the handle to be reused
     * while pages for the old fid are still in the buffer cache.
     */
    int dirh_vid;
    int dirh_dev;
    Inode dirh_ino;
    VnodeId dirh_vnode;
    afs_int32 dirh_cacheCheck;
    Unique dirh_unique;
    IHandle_t *dirh_handle;
} DirHandle;



#define MAXCNTRS (AFS_HIGHEST_OPCODE+1)

#define MAXCONSOLE 5
#define CONSOLENAME "opcons"
#define NEWCONNECT "NEWCONNECT"
#define TOTAL 0
#define FETCHDATAOP 30
#define FETCHDATA 31
#define FETCHD1 32
#define FETCHD2 33
#define FETCHD3 34
#define FETCHD4 35
#define FETCHD5 36
#define FETCHTIME 37
#define STOREDATAOP 40
#define STOREDATA 41
#define STORED1 42
#define STORED2 43
#define STORED3 44
#define STORED4 45
#define STORED5 46
#define STORETIME 47
/* N.B.: the biggest "opcode" here must not be as big as VICELOWEST_OPCODE */


#define SIZE1 1024
#define SIZE2 SIZE1*8
#define SIZE3 SIZE2*8
#define SIZE4 SIZE3*8

#define BIGTIME	(0x7FFFFFFF)	/* Should be max u_int, rather than max int */

struct AFSCallStatistics {
    /* References to AFS interface calls */
    afs_uint32 FetchData;
    afs_uint32 FetchACL;
    afs_uint32 FetchStatus;
    afs_uint32 StoreData;
    afs_uint32 StoreACL;
    afs_uint32 StoreStatus;
    afs_uint32 RemoveFile;
    afs_uint32 CreateFile;
    afs_uint32 Rename;
    afs_uint32 Symlink;
    afs_uint32 Link;
    afs_uint32 MakeDir;
    afs_uint32 RemoveDir;
    afs_uint32 SetLock;
    afs_uint32 ExtendLock;
    afs_uint32 ReleaseLock;
    afs_uint32 GetStatistics;
    afs_uint32 GiveUpCallBacks;
    afs_uint32 GetVolumeInfo;
    afs_uint32 GetVolumeStatus;
    afs_uint32 SetVolumeStatus;
    afs_uint32 GetRootVolume;
    afs_uint32 CheckToken;
    afs_uint32 GetTime;
    afs_uint32 GetCapabilities;

    /* General Fetch/Store Stats */
    afs_uint32 TotalCalls;
    afs_uint32 TotalFetchedBytes;
    afs_uint32 AccumFetchTime;
    afs_uint32 FetchSize1;
    afs_uint32 FetchSize2;
    afs_uint32 FetchSize3;
    afs_uint32 FetchSize4;
    afs_uint32 FetchSize5;
    afs_uint32 TotalStoredBytes;
    afs_uint32 AccumStoreTime;
    afs_uint32 StoreSize1;
    afs_uint32 StoreSize2;
    afs_uint32 StoreSize3;
    afs_uint32 StoreSize4;
    afs_uint32 StoreSize5;
};

struct AFSDisk {
    afs_int32 BlocksAvailable;
    afs_int32 TotalBlocks;
    DiskName Name;
};

#define	AFS_MSTATSPARES	8
#define	AFS_MSTATDISKS	10
struct AFSStatistics {
    afs_uint32 CurrentMsgNumber;
    afs_uint32 OldestMsgNumber;
    afs_uint32 CurrentTime;
    afs_uint32 BootTime;
    afs_uint32 StartTime;
    afs_int32 CurrentConnections;
    afs_uint32 TotalAFSCalls;
    afs_uint32 TotalFetchs;
    afs_uint32 FetchDatas;
    afs_uint32 FetchedBytes;
    afs_int32 FetchDataRate;
    afs_uint32 TotalStores;
    afs_uint32 StoreDatas;
    afs_uint32 StoredBytes;
    afs_int32 StoreDataRate;
    afs_uint32 TotalRPCBytesSent;
    afs_uint32 TotalRPCBytesReceived;
    afs_uint32 TotalRPCPacketsSent;
    afs_uint32 TotalRPCPacketsReceived;
    afs_uint32 TotalRPCPacketsLost;
    afs_uint32 TotalRPCBogusPackets;
    afs_int32 SystemCPU;
    afs_int32 UserCPU;
    afs_int32 NiceCPU;
    afs_int32 IdleCPU;
    afs_int32 TotalIO;
    afs_int32 ActiveVM;
    afs_int32 TotalVM;
    afs_int32 EtherNetTotalErrors;
    afs_int32 EtherNetTotalWrites;
    afs_int32 EtherNetTotalInterupts;
    afs_int32 EtherNetGoodReads;
    afs_int32 EtherNetTotalBytesWritten;
    afs_int32 EtherNetTotalBytesRead;
    afs_int32 ProcessSize;
    afs_int32 WorkStations;
    afs_int32 ActiveWorkStations;
    afs_int32 Spares[AFS_MSTATSPARES];
    struct AFSDisk Disks[AFS_MSTATDISKS];
};

extern int busyonrst;
extern int saneacls;

#define RESTART_ORDINARY 1
#define RESTART_FAST 2
#define RESTART_SAFE 3

#define DONTPANIC 0
#define PANIC 1

#define MAX_FILESERVER_THREAD	128	/* max number of threads in fileserver, subject to system limits */

#define FILESERVER_HELPER_THREADS 7	/* Listner, IOMGR, FiveMinute, 
					 * HostCheck, Signal, min 2 for RXSTATS */
#ifdef AFS_PTHREAD_ENV
#include <pthread.h>
#include <assert.h>
extern pthread_mutex_t fileproc_glock_mutex;
#define FS_LOCK \
    assert(pthread_mutex_lock(&fileproc_glock_mutex) == 0)
#define FS_UNLOCK \
    assert(pthread_mutex_unlock(&fileproc_glock_mutex) == 0)
extern pthread_mutex_t fsync_glock_mutex;
#define FSYNC_LOCK \
    assert(pthread_mutex_lock(&fsync_glock_mutex) == 0)
#define FSYNC_UNLOCK \
    assert(pthread_mutex_unlock(&fsync_glock_mutex) == 0)
#else /* AFS_PTHREAD_ENV */
#define FS_LOCK
#define FS_UNLOCK
#define FSYNC_LOCK
#define FSYNC_UNLOCK
#endif /* AFS_PTHREAD_ENV */


#ifdef AFS_DEMAND_ATTACH_FS
/*
 * demand attach fs
 * fileserver mode support
 */
struct fs_state {
    volatile int mode;
    volatile byte FiveMinuteLWP_tranquil;      /* five minute check thread is shutdown or sleeping */
    volatile byte HostCheckLWP_tranquil;       /* host check thread is shutdown or sleeping */
    volatile byte FsyncCheckLWP_tranquil;      /* fsync check thread is shutdown or sleeping */
    volatile byte salvsync_fatal_error;        /* fatal error with salvsync comm */

    /* some command-line options we use in 
     * various places
     *
     * these fields are immutable once we
     * go multithreaded */
    struct {
	byte fs_state_save;
	byte fs_state_restore;
	byte fs_state_verify_before_save;
	byte fs_state_verify_after_restore;
    } options;

    pthread_cond_t worker_done_cv;
    pthread_rwlock_t state_lock;
};

extern struct fs_state fs_state;

/* this lock is defined to be directly above FS_LOCK in the locking hierarchy */
#define FS_STATE_RDLOCK  assert(pthread_rwlock_rdlock(&fs_state.state_lock) == 0)
#define FS_STATE_WRLOCK  assert(pthread_rwlock_wrlock(&fs_state.state_lock) == 0)
#define FS_STATE_UNLOCK  assert(pthread_rwlock_unlock(&fs_state.state_lock) == 0)

#define FS_MODE_NORMAL    0
#define FS_MODE_SHUTDOWN  1
#endif /* AFS_DEMAND_ATTACH_FS */


#endif /* _AFS_VICED_VICED_H */
