/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#ifndef OPENAFS_WINNT_AFSD_BUF_H
#define OPENAFS_WINNT_AFSD_BUF_H 1

#include <osi.h>
#include <opr/jhash.h>

#ifdef DISKCACHE95
#include "cm_diskcache.h"
#endif /* DISKCACHE95 */

/* default # of buffers if not changed */
#define CM_BUF_BUFFERS	100

/* default buffer size */
#define CM_BUF_BLOCKSIZE CM_CONFIGDEFAULT_BLOCKSIZE

/* cache type */
#define CM_BUF_CACHETYPE_FILE 1
#define CM_BUF_CACHETYPE_VIRTUAL 2
extern int buf_cacheType;

#define BUF_HASH(fidp, offsetp) \
    (opr_jhash((uint32_t *)(offsetp), 2, (fidp)->hash) & (cm_data.buf_hashSize - 1))

#define BUF_FILEHASH(fidp) ((fidp)->hash & (cm_data.buf_hashSize - 1))

#define CM_BUF_MAGIC    ('B' | 'U' <<8 | 'F'<<16 | 'F'<<24)

#define CM_BUF_VERSION_BAD 0xFFFFFFFFFFFFFFFF

/* represents a single buffer */
typedef struct cm_buf {
    osi_queue_t    q;		/* queue: buf_freeList and buf_redirList */
    afs_uint32     qFlags;	/* queue/hash state flags - buf_globalLock */
    afs_uint32     magic;
    struct cm_buf *allp;	/* next in all list */
    struct cm_buf *hashp;	/* hash bucket pointer */
    struct cm_buf *fileHashp;   /* file hash bucket pointer */
    struct cm_buf *fileHashBackp;	/* file hash bucket back pointer */
				/*
				 * The file hash chain is doubly linked, since
				 * these chains can get rather long.  The
				 * regular hash chain is only singly linked,
				 * since the chains should be short if the
				 * hash function is good and if there are
				 * enough buckets for the size of the cache.
				 */
    struct cm_buf *dirtyp;	/* next in the dirty list */
    osi_mutex_t mx;		/* mutex protecting structure except refcount */
    afs_int32 refCount;	        /* reference count (buf_globalLock) */
    afs_uint32 dirtyCounter;	/* bumped at each dirty->clean transition */
    osi_hyper_t offset;	        /* offset */
    cm_fid_t fid;		/* file ID */
    char *datap;		/* data in this buffer */
    afs_uint32 flags;		/* flags we're using - mx */
    afs_uint32 error;	        /* last error code, if CM_BUF_ERROR is set */
    cm_user_t *userp;	        /* user who wrote to the buffer last */

    /* fields added for the CM; locked by scp->mx */
    afs_uint64 dataVersion;	/* data version of this page */
    afs_uint32 cmFlags;		/* flags for cm */

    /* syncop state */
    afs_uint32 waitCount;       /* number of threads waiting */
    afs_uint32 waitRequests;    /* num of thread wait requests */

    afs_uint32 dirty_offset;    /* offset from beginning of buffer containing dirty bytes */
    afs_uint32 dirty_length;    /* number of dirty bytes within the buffer */

#ifdef DISKCACHE95
    cm_diskcache_t *dcp;        /* diskcache structure */
#endif /* DISKCACHE95 */
#ifdef DEBUG
    cm_scache_t *scp;		/* for debugging, the scache object belonging to */
                                /* the fid at the time of fid assignment. */
#else
    void * dummy;
#endif

    /* redirector state - protected by buf_globalLock */
    osi_queue_t redirq;         /* queue: cm_scache_t redirList */
    time_t      redirLastAccess;/* last time redir accessed the buffer */
    time_t      redirReleaseRequested;

    unsigned char md5cksum[16]; /* md5 checksum of the block pointed to by datap */
} cm_buf_t;

#define redirq_to_cm_buf_t(q) ((q) ? (cm_buf_t *)((char *) (q) - offsetof(cm_buf_t, redirq)) : NULL)

/* values for cmFlags */
#define CM_BUF_CMFETCHING	1	/* fetching this buffer */
#define CM_BUF_CMSTORING	2	/* storing this buffer */
#define CM_BUF_CMFULLYFETCHED	4	/* read-while-fetching optimization */
#define CM_BUF_CMWRITING        8       /* writing to this buffer */
/* waiting is done based on scp->flags.  Removing bits from cmFlags
   should be followed by waking the scp. */

/* values for qFlags */
#define CM_BUF_QINHASH	1	/* in the hash table */
#define CM_BUF_QINLRU	2	/* in lru queue (aka free list) */
#define CM_BUF_QINDL    4       /* in the dirty list */
#define CM_BUF_QREDIR   8       /* buffer held by the redirector */

/* values for flags */
#define CM_BUF_READING	1	/* now reading buffer from the disk */
#define CM_BUF_WRITING	2	/* now writing buffer to the disk */
#define CM_BUF_DIRTY	8	/* buffer is dirty */
#define CM_BUF_ERROR	0x20	/* something went wrong on delayed write */
#define CM_BUF_WAITING	0x40	/* someone's waiting for a flag to change */
#define CM_BUF_EOF	0x80	/* read 0 bytes; used for detecting EOF */

typedef struct cm_buf_ops {
    long (*Writep)(void *vscp, osi_hyper_t *offsetp,
                   long length, long flags,
                   struct cm_user *userp,
                   struct cm_req *reqp);
    long (*Readp)(cm_buf_t *bufp, long length,
                  long *bytesReadp, struct cm_user *userp);
    long (*Stabilizep)(void *vscp, struct cm_user *userp, struct cm_req *reqp);
    long (*Unstabilizep)(void *vscp, struct cm_user *userp);
} cm_buf_ops_t;

#define CM_BUF_WRITE_SCP_LOCKED 0x1

/* global locks */
extern osi_rwlock_t buf_globalLock;

extern long buf_Init(int newFile, cm_buf_ops_t *, afs_uint64 nbuffers);

extern void buf_Shutdown(void);

#ifdef DEBUG_REFCOUNT
extern void buf_ReleaseDbg(cm_buf_t *, char *, long);

extern void buf_HoldDbg(cm_buf_t *, char *, long);

extern void buf_ReleaseLockedDbg(cm_buf_t *, afs_uint32, char *, long);

extern void buf_HoldLockedDbg(cm_buf_t *, char *, long);

#define buf_Release(bufp) buf_ReleaseDbg(bufp, __FILE__, __LINE__)
#define buf_Hold(bufp)    buf_HoldDbg(bufp, __FILE__, __LINE__)
#define buf_ReleaseLocked(bufp, lock) buf_ReleaseLockedDbg(bufp, lock, __FILE__, __LINE__)
#define buf_HoldLocked(bufp) buf_HoldLockedDbg(bufp, __FILE__, __LINE__)
#else
extern void buf_Release(cm_buf_t *);

extern void buf_Hold(cm_buf_t *);

extern void buf_ReleaseLocked(cm_buf_t *, afs_uint32);

extern void buf_HoldLocked(cm_buf_t *);
#endif

extern void buf_WaitIO(cm_scache_t *, cm_buf_t *);

extern cm_buf_t *buf_FindLocked(struct cm_fid *, osi_hyper_t *);

extern cm_buf_t *buf_Find(struct cm_fid *, osi_hyper_t *);

extern cm_buf_t *buf_FindAllLocked(struct cm_fid *, osi_hyper_t *, afs_uint32 flags);

extern cm_buf_t *buf_FindAll(struct cm_fid *, osi_hyper_t *, afs_uint32 flags);

extern long buf_GetNewLocked(struct cm_scache *, osi_hyper_t *, cm_req_t *, cm_buf_t **);

extern long buf_Get(struct cm_scache *, osi_hyper_t *, cm_req_t *, cm_buf_t **);

extern afs_uint32 buf_CleanLocked(cm_scache_t *, cm_buf_t *, cm_req_t *, afs_uint32 flags, afs_uint32 *);

extern afs_uint32 buf_Clean(cm_scache_t *, cm_buf_t *, cm_req_t *, afs_uint32 flags, afs_uint32 *);

extern void buf_CleanWait(cm_scache_t *, cm_buf_t *, afs_uint32 locked);

extern void buf_SetDirty(cm_buf_t *, cm_req_t *, afs_uint32 offset, afs_uint32 length, cm_user_t *);

extern long buf_CleanAndReset(void);

extern void buf_ReserveBuffers(afs_uint64);

extern afs_uint64 buf_TryReserveBuffers(afs_uint64);

extern void buf_UnreserveBuffers(afs_uint64);

#ifdef TESTING
extern void buf_ValidateBufQueues(void);
#endif /* TESTING */

extern osi_log_t *buf_logp;

extern long buf_Truncate(struct cm_scache *scp, cm_user_t *userp,
	cm_req_t *reqp, osi_hyper_t *sizep);

extern long buf_CleanVnode(struct cm_scache *scp, cm_user_t *userp,
	cm_req_t *reqp);

extern long buf_FlushCleanPages(cm_scache_t *scp, cm_user_t *userp,
	cm_req_t *reqp);

extern long buf_SetNBuffers(afs_uint64 nbuffers);

extern long buf_ValidateBuffers(void);

extern void buf_ForceTrace(BOOL flush);

extern long buf_DirtyBuffersExist(cm_fid_t * fidp);

extern long buf_CleanDirtyBuffers(cm_scache_t *scp);

extern long buf_InvalidateBuffers(cm_scache_t * scp);

extern long buf_RDRBuffersExist(cm_fid_t *fidp);

extern long buf_ClearRDRFlag(cm_scache_t *scp, char * reason);

extern long buf_ForceDataVersion(cm_scache_t * scp, afs_uint64 fromVersion, afs_uint64 toVersion);

extern int cm_DumpBufHashTable(FILE *outputFile, char *cookie, int lock);

extern void buf_ComputeCheckSum(cm_buf_t *bp);

extern int  buf_ValidateCheckSum(cm_buf_t *bp);

extern const char *buf_HexCheckSum(cm_buf_t * bp);

extern afs_uint32
buf_RDRShakeSomeExtentsFree(cm_req_t *reqp, afs_uint32 oneFid, afs_uint32 minage);

extern afs_uint32
buf_RDRShakeAnExtentFree(cm_buf_t *bufp, cm_req_t *reqp);

extern afs_uint32
buf_RDRShakeFileExtentsFree(cm_scache_t *scp, cm_req_t *reqp);

extern void
buf_InsertToRedirQueue(cm_scache_t *scp, cm_buf_t *bufp);

extern void
buf_RemoveFromRedirQueue(cm_scache_t *scp, cm_buf_t *bufp);

extern void
buf_MoveToHeadOfRedirQueue(cm_scache_t *scp, cm_buf_t *bufp);

#ifdef _M_IX86
#define buf_IncrementRedirCount()  InterlockedIncrement(&cm_data.buf_redirCount)
#define buf_DecrementRedirCount()  InterlockedDecrement(&cm_data.buf_redirCount)
#define buf_IncrementFreeCount()   InterlockedIncrement(&cm_data.buf_freeCount)
#define buf_DecrementFreeCount()   InterlockedDecrement(&cm_data.buf_freeCount)
#define buf_IncrementUsedCount()   InterlockedIncrement(&cm_data.buf_usedCount)
#define buf_DecrementUsedCount()   InterlockedDecrement(&cm_data.buf_usedCount)
#else
#define buf_IncrementRedirCount()  InterlockedIncrement64(&cm_data.buf_redirCount)
#define buf_DecrementRedirCount()  InterlockedDecrement64(&cm_data.buf_redirCount)
#define buf_IncrementFreeCount()   InterlockedIncrement64(&cm_data.buf_freeCount)
#define buf_DecrementFreeCount()   InterlockedDecrement64(&cm_data.buf_freeCount)
#define buf_IncrementUsedCount()   InterlockedIncrement64(&cm_data.buf_usedCount)
#define buf_DecrementUsedCount()   InterlockedDecrement64(&cm_data.buf_usedCount)
#endif

/* error codes */
#define CM_BUF_EXISTS	1	/* buffer exists, and shouldn't */

#endif /* OPENAFS_WINNT_AFSD_BUF_H */
