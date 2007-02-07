/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CM_SCACHE_H_ENV__
#define __CM_SCACHE_H_ENV__ 1

#ifdef DJGPP
#include "largeint95.h"
#endif /* DJGPP */

#define MOUNTPOINTLEN   1024

typedef struct cm_fid {
	unsigned long cell;
        unsigned long volume;
        unsigned long vnode;
        unsigned long unique;
} cm_fid_t;

/* Key used for byte range locking.  Each unique key identifies a
   unique client per cm_scache_t for the purpose of locking. */
typedef afs_uint64 cm_key_t;

typedef struct cm_range {
    afs_int64 offset;
    afs_int64 length;
} cm_range_t;

/* forward dcls */
struct cm_scache;
typedef struct cm_scache cm_scache_t;

typedef struct cm_file_lock {
    osi_queue_t q;              /* list of all locks [protected by
                                   cm_scacheLock] */
    osi_queue_t fileq;		/* per-file list of locks [protected
                                   by scp->mx]*/
    
    cm_user_t *userp;           /* The user to which this lock belongs
                                   to [immutable; held] */
    cm_scache_t *scp;           /* The scache to which this lock
                                   applies to [immutable; held] */
#ifdef DEBUG
    cm_fid_t   fid;
#endif

    cm_range_t range;           /* Range for the lock [immutable] */
    cm_key_t key;               /* Key for the lock [immutable] */
    unsigned char lockType;     /* LockRead or LockWrite [immutable] */
    unsigned char flags;        /* combination of CM_FILELOCK_FLAG__*
                                 * [protected by cm_scacheLock] */
    time_t lastUpdate;          /* time of last assertion with
                                 * server. [protected by
                                 * cm_scacheLock] */
} cm_file_lock_t;

#define CM_FILELOCK_FLAG_DELETED         0x01
#define CM_FILELOCK_FLAG_LOST            0x02

/* the following are mutually exclusive */
#define CM_FILELOCK_FLAG_WAITLOCK        0x04
#define CM_FILELOCK_FLAG_WAITUNLOCK      0x0C

/* the following is used to indicate that there are no server side
   locks associated with this lock.  This is true for locks obtained
   against files in RO volumes as well as files residing on servers
   that disable client side byte range locking. */
#define CM_FILELOCK_FLAG_CLIENTONLY      0x10

#define CM_FLSHARE_OFFSET_HIGH           0x01000000
#define CM_FLSHARE_OFFSET_LOW            0x00000000
#define CM_FLSHARE_LENGTH_HIGH           0x00000000
#define CM_FLSHARE_LENGTH_LOW            0x00000001

typedef struct cm_prefetch {		/* last region scanned for prefetching */
	osi_hyper_t base;		/* start of region */
        osi_hyper_t end;		/* first char past region */
} cm_prefetch_t;

#define CM_SCACHE_MAGIC ('S' | 'C'<<8 | 'A'<<16 | 'C'<<24)

typedef struct cm_scache {
    osi_queue_t q;              	/* lru queue; cm_scacheLock */
    afs_uint32      magic;
    struct cm_scache *nextp;		/* next in hash; cm_scacheLock */
    cm_fid_t fid;
    afs_uint32 flags;			/* flags; locked by mx */

    /* synchronization stuff */
    osi_mutex_t mx;			/* mutex for this structure */
    osi_rwlock_t bufCreateLock;		/* read-locked during buffer creation;
                                         * write-locked to prevent buffers from
					 * being created during a truncate op, etc.
					 */
    afs_uint32 refCount;		/* reference count; cm_scacheLock */
    osi_queueData_t *bufReadsp;		/* queue of buffers being read */
    osi_queueData_t *bufWritesp;	/* queue of buffers being written */

    /* parent info for ACLs */
    afs_uint32 parentVnode;		/* parent vnode for ACL callbacks */
    afs_uint32 parentUnique;		/* for ACL callbacks */

    /* local modification stat */
    afs_uint32 mask;			/* for clientModTime, length and
                                         * truncPos */

    /* file status */
    afs_uint32 fileType;		/* file type */
    time_t clientModTime;	        /* mtime */
    time_t serverModTime;	        /* at server, for concurrent call
                                         * comparisons */
    osi_hyper_t length;			/* file length */
    cm_prefetch_t prefetch;		/* prefetch info structure */
    afs_uint32 unixModeBits;		/* unix protection mode bits */
    afs_uint32 linkCount;		/* link count */
    afs_uint32 dataVersion;		/* data version */
    afs_uint32 owner; 			/* file owner */
    afs_uint32 group;			/* file owning group */
    cm_user_t *creator;			/* user, if new file */

    /* pseudo file status */
    osi_hyper_t serverLength;		/* length known to server */

    /* aux file status */
    osi_hyper_t truncPos;		/* file size to truncate to before
                                         * storing data */

    /* symlink and mount point info */
    char mountPointStringp[MOUNTPOINTLEN];	/* the string stored in a mount point;
                                                 * first char is type, then vol name.
                                         * If this is a normal symlink, we store
					 * the link contents here.
                                         */
    cm_fid_t  mountRootFid;	        /* mounted on root */
    time_t    mountRootGen;	        /* time to update mountRootFidp? */
    cm_fid_t  dotdotFid;		/* parent of volume root */

    /* callback info */
    struct cm_server *cbServerp;	/* server granting callback */
    time_t cbExpires;			/* time callback expires */

    /* access cache */
    long anyAccess;			/* anonymous user's access */
    struct cm_aclent *randomACLp;	/* access cache entries */

    /* file locks */
    afs_int32    serverLock;    /* current lock we have acquired on
                                 * this file.  One of (-1), LockRead
                                 * or LockWrite. [protected by
                                 * scp->mx].  In the future, this
                                 * should be replaced by a queue of
                                 * cm_server_lock_t objects which keep
                                 * track of lock type, the user for
                                 * whom the lock was obtained, the
                                 * dataVersion at the time the lock
                                 * was asserted last, lastRefreshCycle
                                 * and lateUpdateTime.
                                 */
    unsigned long lastRefreshCycle; /* protected with cm_scacheLock
                                     * for all scaches. */
    afs_uint32 lockDataVersion; /* dataVersion of the scp at the time
                                   the server lock for the scp was
                                   asserted for this lock the last
                                   time. */
    osi_queue_t *fileLocksH;    /* queue of locks (head) */
    osi_queue_t *fileLocksT;    /* queue of locks (tail) */

    afs_uint32   sharedLocks;   /* number of shared locks on
                                 * ::fileLocks.  This count does not
                                 * include locks which have
                                 * CM_FILELOCK_FLAG_CLIENTONLY set. */

    afs_uint32   exclusiveLocks; /* number of exclusive locks on
                                  * ::fileLocks.  This count does not
                                  * include locks which have
                                  * CM_FILELOCK_FLAG_CLIENTONLY set.
                                  */

    afs_uint32   clientLocks;   /* number of locks on ::fileLocks that
                                   have CM_FILELOCK_FLAG_CLIENTONLY
                                   set. */
	
    /* volume info */
    struct cm_volume *volp;		/* volume info; held reference */

    /* bulk stat progress */
    osi_hyper_t bulkStatProgress;	/* track bulk stats of large dirs */

    /* open state */
    afs_uint16 openReads;		/* open for reading */
    afs_uint16 openWrites;		/* open for writing */
    afs_uint16 openShares;		/* open for read excl */
    afs_uint16 openExcls;		/* open for exclusives */

    /* syncop state */
    afs_uint32 waitCount;           /* number of threads waiting */
    afs_uint32 waitRequests;        /* num of thread wait requests */
} cm_scache_t;

/* mask field - tell what has been modified */
#define CM_SCACHEMASK_CLIENTMODTIME	1	/* client mod time */
#define CM_SCACHEMASK_LENGTH		2	/* length */
#define CM_SCACHEMASK_TRUNCPOS		4	/* truncation position */

/* fileType values */
#define CM_SCACHETYPE_FILE		1	/* a file */
#define CM_SCACHETYPE_DIRECTORY		2	/* a dir */
#define CM_SCACHETYPE_SYMLINK		3	/* a symbolic link */
#define CM_SCACHETYPE_MOUNTPOINT	4	/* a mount point */
#define CM_SCACHETYPE_DFSLINK           5       /* a Microsoft Dfs link */
#define CM_SCACHETYPE_INVALID           99      /* an invalid link */

/* flag bits */
#define CM_SCACHEFLAG_STATD             0x01    /* status info is valid */
#define CM_SCACHEFLAG_DELETED           0x02    /* file has been deleted */
#define CM_SCACHEFLAG_CALLBACK          0x04    /* have a valid callback */
#define CM_SCACHEFLAG_STORING           0x08    /* status being stored back */
#define CM_SCACHEFLAG_FETCHING		0x10	/* status being fetched */
#define CM_SCACHEFLAG_SIZESTORING	0x20	/* status being stored that
						 * changes the data; typically,
						 * this is a truncate op. */
#define CM_SCACHEFLAG_INHASH		0x40	/* in the hash table */
#define CM_SCACHEFLAG_BULKSTATTING	0x80	/* doing a bulk stat */
#define CM_SCACHEFLAG_WAITING		0x200	/* waiting for fetch/store
						 * state to change */
#define CM_SCACHEFLAG_PURERO		0x400	/* read-only (not even backup);
						 * for mount point eval */
#define CM_SCACHEFLAG_RO		0x800	/* read-only
						 * (can't do write ops) */
#define CM_SCACHEFLAG_GETCALLBACK	0x1000	/* we're getting a callback */
#define CM_SCACHEFLAG_DATASTORING	0x2000	/* data being stored */
#define CM_SCACHEFLAG_PREFETCHING	0x4000	/* somebody is prefetching */
#define CM_SCACHEFLAG_OVERQUOTA		0x8000	/* over quota */
#define CM_SCACHEFLAG_OUTOFSPACE	0x10000	/* out of space */
#define CM_SCACHEFLAG_ASYNCSTORING	0x20000	/* scheduled to store back */
#define CM_SCACHEFLAG_LOCKING		0x40000	/* setting/clearing file lock */
#define CM_SCACHEFLAG_WATCHED		0x80000	/* directory being watched */
#define CM_SCACHEFLAG_WATCHEDSUBTREE	0x100000 /* dir subtree being watched */
#define CM_SCACHEFLAG_ANYWATCH \
			(CM_SCACHEFLAG_WATCHED | CM_SCACHEFLAG_WATCHEDSUBTREE)

#define CM_SCACHEFLAG_EACCESS           0x200000 /* Bulk Stat returned EACCES */
#define CM_SCACHEFLAG_SMB_FID	        0x400000

/* sync flags for calls to the server.  The CM_SCACHEFLAG_FETCHING,
 * CM_SCACHEFLAG_STORING and CM_SCACHEFLAG_SIZESTORING flags correspond to the
 * below, except for FETCHDATA and STOREDATA, which correspond to non-null
 * buffers in bufReadsp and bufWritesp.
 * These flags correspond to individual RPCs that we may be making, and at most
 * one can be set in any one call to SyncOp.
 */
#define CM_SCACHESYNC_FETCHSTATUS           0x01        /* fetching status info */
#define CM_SCACHESYNC_STORESTATUS           0x02        /* storing status info */
#define CM_SCACHESYNC_FETCHDATA             0x04        /* fetch data */
#define CM_SCACHESYNC_STOREDATA             0x08        /* store data */
#define CM_SCACHESYNC_STORESIZE		0x10	/* store new file size */
#define CM_SCACHESYNC_GETCALLBACK	0x20	/* fetching a callback */
#define CM_SCACHESYNC_STOREDATA_EXCL	0x40	/* store data */
#define CM_SCACHESYNC_ASYNCSTORE	0x80	/* schedule data store */
#define CM_SCACHESYNC_LOCK		0x100	/* set/clear file lock */

/* sync flags for calls within the client; there are no corresponding flags
 * in the scache entry, because we hold the scache entry locked during the
 * operations below.
 */
#define CM_SCACHESYNC_GETSTATUS		0x1000	/* read the status */
#define CM_SCACHESYNC_SETSTATUS		0x2000	/* e.g. utimes */
#define CM_SCACHESYNC_READ		0x4000	/* read data from a chunk */
#define CM_SCACHESYNC_WRITE		0x8000	/* write data to a chunk */
#define CM_SCACHESYNC_SETSIZE		0x10000	/* shrink the size of a file,
						 * e.g. truncate */
#define CM_SCACHESYNC_NEEDCALLBACK	0x20000	/* need a callback on the file */
#define CM_SCACHESYNC_CHECKRIGHTS	0x40000	/* check that user has desired
						 * access rights */
#define CM_SCACHESYNC_BUFLOCKED		0x80000	/* the buffer is locked */
#define CM_SCACHESYNC_NOWAIT		0x100000/* don't wait for the state,
						 * just fail */
#define CM_SCACHESYNC_FORCECB		0x200000/* when calling cm_GetCallback()
                                                 * set the force flag */

/* flags for cm_RecycleSCache	*/
#define CM_SCACHE_RECYCLEFLAG_DESTROY_BUFFERS 	0x1

/* flags for cm_MergeStatus */
#define CM_MERGEFLAG_FORCE		1	/* check mtime before merging;
						 * used to see if we're merging
						 * in old info.
                                                 */
#define CM_MERGEFLAG_STOREDATA		2	/* Merge due to storedata op */

/* hash define.  Must not include the cell, since the callback revocation code
 * doesn't necessarily know the cell in the case of a multihomed server
 * contacting us from a mystery address.
 */
#define CM_SCACHE_HASH(fidp)	(((unsigned long)	\
				   ((fidp)->volume +	\
				    (fidp)->vnode +	\
				    (fidp)->unique))	\
					% cm_data.hashTableSize)

#include "cm_conn.h"
#include "cm_buf.h"

extern void cm_InitSCache(int, long);

#ifdef DEBUG_REFCOUNT
extern long cm_GetSCacheDbg(cm_fid_t *, cm_scache_t **, struct cm_user *,
	struct cm_req *, char *, long);

#define cm_GetSCache(a,b,c,d)  cm_GetSCacheDbg(a,b,c,d,__FILE__,__LINE__)
#else
extern long cm_GetSCache(cm_fid_t *, cm_scache_t **, struct cm_user *,
	struct cm_req *);
#endif

extern cm_scache_t *cm_GetNewSCache(void);

extern int cm_FidCmp(cm_fid_t *, cm_fid_t *);

extern long cm_SyncOp(cm_scache_t *, struct cm_buf *, struct cm_user *,
	struct cm_req *, afs_uint32, afs_uint32);

extern void cm_SyncOpDone(cm_scache_t *, struct cm_buf *, afs_uint32);

extern void cm_MergeStatus(cm_scache_t * dscp, cm_scache_t * scp, 
			   struct AFSFetchStatus * statusp, 
			   struct AFSVolSync * volsyncp,
			   struct cm_user *userp, 
			   afs_uint32 flags);

extern void cm_AFSFidFromFid(struct AFSFid *, cm_fid_t *);

#ifdef DEBUG_REFCOUNT
extern void cm_HoldSCacheNoLockDbg(cm_scache_t *, char *, long);

extern void cm_HoldSCacheDbg(cm_scache_t *, char *, long);

extern void cm_ReleaseSCacheNoLockDbg(cm_scache_t *, char *, long);

extern void cm_ReleaseSCacheDbg(cm_scache_t *, char *, long);

#define cm_HoldSCacheNoLock(scp)    cm_HoldSCacheNoLockDbg(scp, __FILE__, __LINE__)
#define cm_HoldSCache(scp)          cm_HoldSCacheDbg(scp, __FILE__, __LINE__)
#define cm_ReleaseSCacheNoLock(scp) cm_ReleaseSCacheNoLockDbg(scp, __FILE__, __LINE__)
#define cm_ReleaseSCache(scp)       cm_ReleaseSCacheDbg(scp, __FILE__, __LINE__)
#else
extern void cm_HoldSCacheNoLock(cm_scache_t *);

extern void cm_HoldSCache(cm_scache_t *);

extern void cm_ReleaseSCacheNoLock(cm_scache_t *);

extern void cm_ReleaseSCache(cm_scache_t *);
#endif
extern cm_scache_t *cm_FindSCache(cm_fid_t *fidp);

extern cm_scache_t *cm_FindSCacheParent(cm_scache_t *);

extern osi_rwlock_t cm_scacheLock;

extern osi_queue_t *cm_allFileLocks;

extern osi_queue_t *cm_freeFileLocks;

extern unsigned long cm_lockRefreshCycle;

extern void cm_DiscardSCache(cm_scache_t *scp);

extern int cm_FindFileType(cm_fid_t *fidp);

extern long cm_ValidateSCache(void);

extern long cm_ShutdownSCache(void);

extern long cm_RecycleSCache(cm_scache_t *scp, afs_int32 flags);

extern void cm_RemoveSCacheFromHashTable(cm_scache_t *scp);
#endif /*  __CM_SCACHE_H_ENV__ */
