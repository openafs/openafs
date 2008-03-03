/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Copyright (C) 1994 Cazamar Systems, Inc. */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <osi.h>
#include <stdio.h>
#include <assert.h>
#include <strsafe.h>
#include <math.h>

#include "afsd.h"
#include "cm_memmap.h"

#ifdef DEBUG
#define TRACE_BUFFER 1
#endif

extern void afsi_log(char *pattern, ...);

/* This module implements the buffer package used by the local transaction
 * system (cm).  It is initialized by calling cm_Init, which calls buf_Init;
 * it must be initalized before any of its main routines are called.
 *
 * Each buffer is hashed into a hash table by file ID and offset, and if its
 * reference count is zero, it is also in a free list.
 *
 * There are two locks involved in buffer processing.  The global lock
 * buf_globalLock protects all of the global variables defined in this module,
 * the reference counts and hash pointers in the actual cm_buf_t structures,
 * and the LRU queue pointers in the buffer structures.
 *
 * The mutexes in the buffer structures protect the remaining fields in the
 * buffers, as well the data itself.
 * 
 * The locking hierarchy here is this:
 * 
 * - resv multiple simul. buffers reservation
 * - lock buffer I/O flags
 * - lock buffer's mutex
 * - lock buf_globalLock
 *
 */

/* global debugging log */
osi_log_t *buf_logp = NULL;

/* Global lock protecting hash tables and free lists */
osi_rwlock_t buf_globalLock;

/* ptr to head of the free list (most recently used) and the
 * tail (the guy to remove first).  We use osi_Q* functions
 * to put stuff in buf_freeListp, and maintain the end
 * pointer manually
 */

/* a pointer to a list of all buffers, just so that we can find them
 * easily for debugging, and for the incr syncer.  Locked under
 * the global lock.
 */

/* defaults setup; these variables may be manually assigned into
 * before calling cm_Init, as a way of changing these defaults.
 */

/* callouts for reading and writing data, etc */
cm_buf_ops_t *cm_buf_opsp;

#ifdef DISKCACHE95
/* for experimental disk caching support in Win95 client */
cm_buf_t *buf_diskFreeListp;
cm_buf_t *buf_diskFreeListEndp;
cm_buf_t *buf_diskAllp;
extern int cm_diskCacheEnabled;
#endif /* DISKCACHE95 */

/* set this to 1 when we are terminating to prevent access attempts */
static int buf_ShutdownFlag = 0;

void buf_HoldLocked(cm_buf_t *bp)
{
    osi_assertx(bp->magic == CM_BUF_MAGIC,"incorrect cm_buf_t magic");
    InterlockedIncrement(&bp->refCount);
}

/* hold a reference to an already held buffer */
void buf_Hold(cm_buf_t *bp)
{
    lock_ObtainRead(&buf_globalLock);
    buf_HoldLocked(bp);
    lock_ReleaseRead(&buf_globalLock);
}

/* code to drop reference count while holding buf_globalLock */
void buf_ReleaseLocked(cm_buf_t *bp, afs_uint32 writeLocked)
{
    afs_int32 refCount;

    if (writeLocked)
        lock_AssertWrite(&buf_globalLock);
    else
        lock_AssertRead(&buf_globalLock);

    /* ensure that we're in the LRU queue if our ref count is 0 */
    osi_assertx(bp->magic == CM_BUF_MAGIC,"incorrect cm_buf_t magic");

    refCount = InterlockedDecrement(&bp->refCount);
#ifdef DEBUG
    if (refCount < 0)
	osi_panic("buf refcount 0",__FILE__,__LINE__);;
#else
    osi_assertx(refCount >= 0, "cm_buf_t refCount == 0");
#endif
    if (refCount == 0) {
        /* 
         * If we are read locked there could be a race condition
         * with buf_Find() so we must obtain a write lock and
         * double check that the refCount is actually zero
         * before we remove the buffer from the LRU queue.
         */
        if (!writeLocked)
            lock_ConvertRToW(&buf_globalLock);

        if (bp->refCount == 0 &&
            !(bp->flags & CM_BUF_INLRU)) {
            osi_QAdd((osi_queue_t **) &cm_data.buf_freeListp, &bp->q);

            /* watch for transition from empty to one element */
            if (!cm_data.buf_freeListEndp)
                cm_data.buf_freeListEndp = cm_data.buf_freeListp;
            bp->flags |= CM_BUF_INLRU;
        }

        if (!writeLocked)
            lock_ConvertWToR(&buf_globalLock);
    }
}       

/* release a buffer.  Buffer must be referenced, but unlocked. */
void buf_Release(cm_buf_t *bp)
{
    afs_int32 refCount;

    /* ensure that we're in the LRU queue if our ref count is 0 */
    osi_assertx(bp->magic == CM_BUF_MAGIC,"incorrect cm_buf_t magic");

    refCount = InterlockedDecrement(&bp->refCount);
#ifdef DEBUG
    if (refCount < 0)
	osi_panic("buf refcount 0",__FILE__,__LINE__);;
#else
    osi_assertx(refCount >= 0, "cm_buf_t refCount == 0");
#endif
    if (refCount == 0) {
        lock_ObtainWrite(&buf_globalLock);
        if (bp->refCount == 0 && 
            !(bp->flags & CM_BUF_INLRU)) {
            osi_QAdd((osi_queue_t **) &cm_data.buf_freeListp, &bp->q);

            /* watch for transition from empty to one element */
            if (!cm_data.buf_freeListEndp)
                cm_data.buf_freeListEndp = cm_data.buf_freeListp;
            bp->flags |= CM_BUF_INLRU;
        }
        lock_ReleaseWrite(&buf_globalLock);
    }
}

/* incremental sync daemon.  Writes all dirty buffers every 5000 ms */
void buf_IncrSyncer(long parm)
{
    cm_buf_t **bpp, *bp;
    long i;				/* counter */
    long wasDirty = 0;
    cm_req_t req;

    while (buf_ShutdownFlag == 0) {
        if (!wasDirty) {
            i = SleepEx(5000, 1);
            if (i != 0) continue;
	}

	wasDirty = 0;

        /* now go through our percentage of the buffers */
        for (bpp = &cm_data.buf_dirtyListp; bp = *bpp; ) {
	    /* all dirty buffers are held when they are added to the
	     * dirty list.  No need for an additional hold.
	     */
            lock_ObtainMutex(&bp->mx);
	    if (bp->flags & CM_BUF_DIRTY) {
		/* start cleaning the buffer; don't touch log pages since
 		 * the log code counts on knowing exactly who is writing
		 * a log page at any given instant.
		 */
		cm_InitReq(&req);
		req.flags |= CM_REQ_NORETRY;
		wasDirty |= buf_CleanAsyncLocked(bp, &req);
	    }

	    /* the buffer may or may not have been dirty
	     * and if dirty may or may not have been cleaned
	     * successfully.  check the dirty flag again.  
	     */
            if (!(bp->flags & CM_BUF_DIRTY)) {
                /* remove the buffer from the dirty list */
                lock_ObtainWrite(&buf_globalLock);
                *bpp = bp->dirtyp;
                bp->dirtyp = NULL;
                if (cm_data.buf_dirtyListp == NULL)
                    cm_data.buf_dirtyListEndp = NULL;
                buf_ReleaseLocked(bp, TRUE);
                lock_ReleaseWrite(&buf_globalLock);
            } else {
                /* advance the pointer so we don't loop forever */
                bpp = &bp->dirtyp;
            }
            lock_ReleaseMutex(&bp->mx);
        }	/* for loop over a bunch of buffers */
    }		/* whole daemon's while loop */
}

long
buf_ValidateBuffers(void)
{
    cm_buf_t * bp, *bpf, *bpa, *bpb;
    afs_uint64 countb = 0, countf = 0, counta = 0;

    if (cm_data.buf_freeListp == NULL && cm_data.buf_freeListEndp != NULL ||
         cm_data.buf_freeListp != NULL && cm_data.buf_freeListEndp == NULL) {
        afsi_log("cm_ValidateBuffers failure: inconsistent free list pointers");
        fprintf(stderr, "cm_ValidateBuffers failure: inconsistent free list pointers\n");
        return -9;                  
    }

    for (bp = cm_data.buf_freeListEndp; bp; bp=(cm_buf_t *) osi_QPrev(&bp->q)) { 
        if (bp->magic != CM_BUF_MAGIC) {
            afsi_log("cm_ValidateBuffers failure: bp->magic != CM_BUF_MAGIC");
            fprintf(stderr, "cm_ValidateBuffers failure: bp->magic != CM_BUF_MAGIC\n");
            return -1;                  
        }
        countb++;                                                                
        bpb = bp;     

        if (countb > cm_data.buf_nbuffers) {
            afsi_log("cm_ValidateBuffers failure: countb > cm_data.buf_nbuffers");
            fprintf(stderr, "cm_ValidateBuffers failure: countb > cm_data.buf_nbuffers\n");
            return -6;	                 
        }
    }

    for (bp = cm_data.buf_freeListp; bp; bp=(cm_buf_t *) osi_QNext(&bp->q)) { 
        if (bp->magic != CM_BUF_MAGIC) {
            afsi_log("cm_ValidateBuffers failure: bp->magic != CM_BUF_MAGIC");
            fprintf(stderr, "cm_ValidateBuffers failure: bp->magic != CM_BUF_MAGIC\n");
            return -2;                  
        }
        countf++;                                                             
        bpf = bp;    

        if (countf > cm_data.buf_nbuffers) {
            afsi_log("cm_ValidateBuffers failure: countf > cm_data.buf_nbuffers");
            fprintf(stderr, "cm_ValidateBuffers failure: countf > cm_data.buf_nbuffers\n");
            return -7;
        }
    }                                                                         
                                                                              
    for (bp = cm_data.buf_allp; bp; bp=bp->allp) {                            
        if (bp->magic != CM_BUF_MAGIC) {
            afsi_log("cm_ValidateBuffers failure: bp->magic != CM_BUF_MAGIC");
            fprintf(stderr, "cm_ValidateBuffers failure: bp->magic != CM_BUF_MAGIC\n");
            return -3;                  
        }
        counta++;                                                             
        bpa = bp;                                                             

        if (counta > cm_data.buf_nbuffers) {
            afsi_log("cm_ValidateBuffers failure: counta > cm_data.buf_nbuffers");
            fprintf(stderr, "cm_ValidateBuffers failure: counta > cm_data.buf_nbuffers\n");
            return -8;	                 
        }
    }                                                                         
                                                                              
    if (countb != countf) {
        afsi_log("cm_ValidateBuffers failure: countb != countf");
        fprintf(stderr, "cm_ValidateBuffers failure: countb != countf\n");
        return -4;         
    }
                                                                              
    if (counta != cm_data.buf_nbuffers) {
        afsi_log("cm_ValidateBuffers failure: counta != cm_data.buf_nbuffers");
        fprintf(stderr, "cm_ValidateBuffers failure: counta != cm_data.buf_nbuffers\n");
        return -5;	                 
    }
                                                                              
    return 0;                                                                 
}

void buf_Shutdown(void)  
{                        
    buf_ShutdownFlag = 1;
}                        

/* initialize the buffer package; called with no locks
 * held during the initialization phase.
 */
long buf_Init(int newFile, cm_buf_ops_t *opsp, afs_uint64 nbuffers)
{
    static osi_once_t once;
    cm_buf_t *bp;
    thread_t phandle;
    long i;
    unsigned long pid;
    char *data;

    if ( newFile ) {
        if (nbuffers) 
            cm_data.buf_nbuffers = nbuffers;

        /* Have to be able to reserve a whole chunk */
        if (((cm_data.buf_nbuffers - 3) * cm_data.buf_blockSize) < cm_chunkSize)
            return CM_ERROR_TOOFEWBUFS;
    }

    /* recall for callouts */
    cm_buf_opsp = opsp;

    if (osi_Once(&once)) {
        /* initialize global locks */
        lock_InitializeRWLock(&buf_globalLock, "Global buffer lock");

        if ( newFile ) {
            /* remember this for those who want to reset it */
            cm_data.buf_nOrigBuffers = cm_data.buf_nbuffers;
 
            /* lower hash size to a prime number */
	    cm_data.buf_hashSize = osi_PrimeLessThan((afs_uint32)(cm_data.buf_nbuffers/7 + 1));
 
            /* create hash table */
            memset((void *)cm_data.buf_scacheHashTablepp, 0, cm_data.buf_hashSize * sizeof(cm_buf_t *));
            
            /* another hash table */
            memset((void *)cm_data.buf_fileHashTablepp, 0, cm_data.buf_hashSize * sizeof(cm_buf_t *));

            /* create buffer headers and put in free list */
            bp = cm_data.bufHeaderBaseAddress;
            data = cm_data.bufDataBaseAddress;
            cm_data.buf_allp = NULL;
            
            for (i=0; i<cm_data.buf_nbuffers; i++) {
                osi_assertx(bp >= cm_data.bufHeaderBaseAddress && bp < (cm_buf_t *)cm_data.bufDataBaseAddress, 
                            "invalid cm_buf_t address");
                osi_assertx(data >= cm_data.bufDataBaseAddress && data < cm_data.bufEndOfData,
                            "invalid cm_buf_t data address");
                
                /* allocate and zero some storage */
                memset(bp, 0, sizeof(cm_buf_t));
                bp->magic = CM_BUF_MAGIC;
                /* thread on list of all buffers */
                bp->allp = cm_data.buf_allp;
                cm_data.buf_allp = bp;
                
                osi_QAdd((osi_queue_t **)&cm_data.buf_freeListp, &bp->q);
                bp->flags |= CM_BUF_INLRU;
                lock_InitializeMutex(&bp->mx, "Buffer mutex");
                
                /* grab appropriate number of bytes from aligned zone */
                bp->datap = data;
                
                /* setup last buffer pointer */
                if (i == 0)
                    cm_data.buf_freeListEndp = bp;
                    
                /* next */
                bp++;
                data += cm_data.buf_blockSize;
            }       
 
            /* none reserved at first */
            cm_data.buf_reservedBufs = 0;
 
            /* just for safety's sake */
            cm_data.buf_maxReservedBufs = cm_data.buf_nbuffers - 3;
        } else {
            bp = cm_data.bufHeaderBaseAddress;
            data = cm_data.bufDataBaseAddress;
            
            for (i=0; i<cm_data.buf_nbuffers; i++) {
                lock_InitializeMutex(&bp->mx, "Buffer mutex");
                bp->userp = NULL;
                bp->waitCount = 0;
                bp->waitRequests = 0;
                bp->flags &= ~CM_BUF_WAITING;
                bp++;
            }       
        }
 
#ifdef TESTING
        buf_ValidateBufQueues();
#endif /* TESTING */

#ifdef TRACE_BUFFER
        /* init the buffer trace log */
        buf_logp = osi_LogCreate("buffer", 1000);
        osi_LogEnable(buf_logp);
#endif

        osi_EndOnce(&once);

        /* and create the incr-syncer */
        phandle = thrd_Create(0, 0,
                               (ThreadFunc) buf_IncrSyncer, 0, 0, &pid,
                               "buf_IncrSyncer");

        osi_assertx(phandle != NULL, "buf: can't create incremental sync proc");
        CloseHandle(phandle);
    }

#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */
    return 0;
}

/* add nbuffers to the buffer pool, if possible.
 * Called with no locks held.
 */
long buf_AddBuffers(afs_uint64 nbuffers)
{
    /* The size of a virtual cache cannot be changed after it has
     * been created.  Subsequent calls to MapViewofFile() with
     * an existing mapping object name would not allow the 
     * object to be resized.  Return failure immediately.
     *
     * A similar problem now occurs with the persistent cache
     * given that the memory mapped file now contains a complex
     * data structure.
     */
    afsi_log("request to add %d buffers to the existing cache of size %d denied",
              nbuffers, cm_data.buf_nbuffers);

    return CM_ERROR_INVAL;
}       

/* interface to set the number of buffers to an exact figure.
 * Called with no locks held.
 */
long buf_SetNBuffers(afs_uint64 nbuffers)
{
    if (nbuffers < 10) 
        return CM_ERROR_INVAL;
    if (nbuffers == cm_data.buf_nbuffers) 
        return 0;
    else if (nbuffers > cm_data.buf_nbuffers)
        return buf_AddBuffers(nbuffers - cm_data.buf_nbuffers);
    else 
        return CM_ERROR_INVAL;
}

/* wait for reading or writing to clear; called with write-locked
 * buffer and unlocked scp and returns with locked buffer.
 */
void buf_WaitIO(cm_scache_t * scp, cm_buf_t *bp)
{
    int release = 0;

    if (scp)
        osi_assertx(scp->magic == CM_SCACHE_MAGIC, "invalid cm_scache_t magic");
    osi_assertx(bp->magic == CM_BUF_MAGIC, "invalid cm_buf_t magic");

    while (1) {
        /* if no IO is happening, we're done */
        if (!(bp->flags & (CM_BUF_READING | CM_BUF_WRITING)))
            break;
		
        /* otherwise I/O is happening, but some other thread is waiting for
         * the I/O already.  Wait for that guy to figure out what happened,
         * and then check again.
         */
        if ( bp->flags & CM_BUF_WAITING ) {
            bp->waitCount++;
            bp->waitRequests++;
            osi_Log1(buf_logp, "buf_WaitIO CM_BUF_WAITING already set for 0x%p", bp);
        } else {
            osi_Log1(buf_logp, "buf_WaitIO CM_BUF_WAITING set for 0x%p", bp);
            bp->flags |= CM_BUF_WAITING;
            bp->waitCount = bp->waitRequests = 1;
        }
        osi_SleepM((LONG_PTR)bp, &bp->mx);

	smb_UpdateServerPriority();

        lock_ObtainMutex(&bp->mx);
        osi_Log1(buf_logp, "buf_WaitIO conflict wait done for 0x%p", bp);
        bp->waitCount--;
        if (bp->waitCount == 0) {
            osi_Log1(buf_logp, "buf_WaitIO CM_BUF_WAITING reset for 0x%p", bp);
            bp->flags &= ~CM_BUF_WAITING;
            bp->waitRequests = 0;
        }

        if ( !scp ) {
            if (scp = cm_FindSCache(&bp->fid))
		 release = 1;
        }
        if ( scp ) {
            lock_ObtainRead(&scp->rw);
            if (scp->flags & CM_SCACHEFLAG_WAITING) {
                osi_Log1(buf_logp, "buf_WaitIO waking scp 0x%p", scp);
                osi_Wakeup((LONG_PTR)&scp->flags);
            }
	    lock_ReleaseRead(&scp->rw);
        }
    }
        
    /* if we get here, the IO is done, but we may have to wakeup people waiting for
     * the I/O to complete.  Do so.
     */
    if (bp->flags & CM_BUF_WAITING) {
        osi_Log1(buf_logp, "buf_WaitIO Waking bp 0x%p", bp);
        osi_Wakeup((LONG_PTR) bp);
    }
    osi_Log1(buf_logp, "WaitIO finished wait for bp 0x%p", bp);

    if (scp && release)
	cm_ReleaseSCache(scp);
}

/* find a buffer, if any, for a particular file ID and offset.  Assumes
 * that buf_globalLock is write locked when called.
 */
cm_buf_t *buf_FindLocked(struct cm_scache *scp, osi_hyper_t *offsetp)
{
    afs_uint32 i;
    cm_buf_t *bp;

    i = BUF_HASH(&scp->fid, offsetp);
    for(bp = cm_data.buf_scacheHashTablepp[i]; bp; bp=bp->hashp) {
        if (cm_FidCmp(&scp->fid, &bp->fid) == 0
             && offsetp->LowPart == bp->offset.LowPart
             && offsetp->HighPart == bp->offset.HighPart) {
            buf_HoldLocked(bp);
            break;
        }
    }
        
    /* return whatever we found, if anything */
    return bp;
}

/* find a buffer with offset *offsetp for vnode *scp.  Called
 * with no locks held.
 */
cm_buf_t *buf_Find(struct cm_scache *scp, osi_hyper_t *offsetp)
{
    cm_buf_t *bp;

    lock_ObtainRead(&buf_globalLock);
    bp = buf_FindLocked(scp, offsetp);
    lock_ReleaseRead(&buf_globalLock);

    return bp;
}       

/* start cleaning I/O on this buffer.  Buffer must be write locked, and is returned
 * write-locked.
 *
 * Makes sure that there's only one person writing this block
 * at any given time, and also ensures that the log is forced sufficiently far,
 * if this buffer contains logged data.
 *
 * Returns non-zero if the buffer was dirty.
 */
long buf_CleanAsyncLocked(cm_buf_t *bp, cm_req_t *reqp)
{
    long code = 0;
    long isdirty = 0;
    cm_scache_t * scp = NULL;
    osi_hyper_t offset;

    osi_assertx(bp->magic == CM_BUF_MAGIC, "invalid cm_buf_t magic");

    while ((bp->flags & CM_BUF_DIRTY) == CM_BUF_DIRTY) {
	isdirty = 1;
        lock_ReleaseMutex(&bp->mx);

	scp = cm_FindSCache(&bp->fid);
	if (scp) {
	    osi_Log2(buf_logp, "buf_CleanAsyncLocked starts I/O on scp 0x%p buf 0x%p", scp, bp);

            offset = bp->offset;
            LargeIntegerAdd(offset, ConvertLongToLargeInteger(bp->dirty_offset));
	    code = (*cm_buf_opsp->Writep)(scp, &offset, 
#if 1
                                           /* we might as well try to write all of the contiguous 
                                            * dirty buffers in one RPC 
                                            */
                                           cm_chunkSize,
#else
                                          bp->dirty_length, 
#endif
                                          0, bp->userp, reqp);
	    osi_Log3(buf_logp, "buf_CleanAsyncLocked I/O on scp 0x%p buf 0x%p, done=%d", scp, bp, code);

	    cm_ReleaseSCache(scp);
	    scp = NULL;
	} else {
	    osi_Log1(buf_logp, "buf_CleanAsyncLocked unable to start I/O - scp not found buf 0x%p", bp);
	    code = CM_ERROR_NOSUCHFILE;
	}    
        
	lock_ObtainMutex(&bp->mx);
	/* if the Write routine returns No Such File, clear the dirty flag
	 * because we aren't going to be able to write this data to the file
	 * server.
	 */
	if (code == CM_ERROR_NOSUCHFILE || code == CM_ERROR_BADFD){
	    bp->flags &= ~CM_BUF_DIRTY;
	    bp->flags |= CM_BUF_ERROR;
            bp->dirty_offset = 0;
            bp->dirty_length = 0;
	    bp->error = code;
	    bp->dataVersion = CM_BUF_VERSION_BAD; /* bad */
	    bp->dirtyCounter++;
	}

#ifdef DISKCACHE95
        /* Disk cache support */
        /* write buffer to disk cache (synchronous for now) */
        diskcache_Update(bp->dcp, bp->datap, cm_data.buf_blockSize, bp->dataVersion);
#endif /* DISKCACHE95 */

	/* if we get here and retries are not permitted 
	 * then we need to exit this loop regardless of 
	 * whether or not we were able to clear the dirty bit
	 */
	if (reqp->flags & CM_REQ_NORETRY)
	    break;
    };

    if (!(bp->flags & CM_BUF_DIRTY)) {
	/* remove buffer from dirty buffer queue */

    }

    /* do logging after call to GetLastError, or else */
        
    /* if someone was waiting for the I/O that just completed or failed,
     * wake them up.
     */
    if (bp->flags & CM_BUF_WAITING) {
        /* turn off flags and wakeup users */
        osi_Log1(buf_logp, "buf_WaitIO Waking bp 0x%p", bp);
        osi_Wakeup((LONG_PTR) bp);
    }
    return isdirty;
}

/* Called with a zero-ref count buffer and with the buf_globalLock write locked.
 * recycles the buffer, and leaves it ready for reuse with a ref count of 1.
 * The buffer must already be clean, and no I/O should be happening to it.
 */
void buf_Recycle(cm_buf_t *bp)
{
    afs_uint32 i;
    cm_buf_t **lbpp;
    cm_buf_t *tbp;
    cm_buf_t *prevBp, *nextBp;

    osi_assertx(bp->magic == CM_BUF_MAGIC, "invalid cm_buf_t magic");

    /* if we get here, we know that the buffer still has a 0 ref count,
     * and that it is clean and has no currently pending I/O.  This is
     * the dude to return.
     * Remember that as long as the ref count is 0, we know that we won't
     * have any lock conflicts, so we can grab the buffer lock out of
     * order in the locking hierarchy.
     */
    osi_Log3( buf_logp, "buf_Recycle recycles 0x%p, off 0x%x:%08x",
              bp, bp->offset.HighPart, bp->offset.LowPart);

    osi_assertx(bp->refCount == 0, "cm_buf_t refcount != 0");
    osi_assertx(!(bp->flags & (CM_BUF_READING | CM_BUF_WRITING | CM_BUF_DIRTY)),
                "incorrect cm_buf_t flags");
    lock_AssertWrite(&buf_globalLock);

    if (bp->flags & CM_BUF_INHASH) {
        /* Remove from hash */

        i = BUF_HASH(&bp->fid, &bp->offset);
        lbpp = &(cm_data.buf_scacheHashTablepp[i]);
        for(tbp = *lbpp; tbp; lbpp = &tbp->hashp, tbp = *lbpp) {
            if (tbp == bp) 
                break;
        }

        /* we better find it */
        osi_assertx(tbp != NULL, "buf_Recycle: hash table screwup");

        *lbpp = bp->hashp;	/* hash out */
        bp->hashp = NULL;

        /* Remove from file hash */

        i = BUF_FILEHASH(&bp->fid);
        prevBp = bp->fileHashBackp;
        bp->fileHashBackp = NULL;
        nextBp = bp->fileHashp;
        bp->fileHashp = NULL;
        if (prevBp)
            prevBp->fileHashp = nextBp;
        else
            cm_data.buf_fileHashTablepp[i] = nextBp;
        if (nextBp)
            nextBp->fileHashBackp = prevBp;

        bp->flags &= ~CM_BUF_INHASH;
    }

    /* make the fid unrecognizable */
    memset(&bp->fid, 0, sizeof(cm_fid_t));
}       

/* recycle a buffer, removing it from the free list, hashing in its new identity
 * and returning it write-locked so that no one can use it.  Called without
 * any locks held, and can return an error if it loses the race condition and 
 * finds that someone else created the desired buffer.
 *
 * If success is returned, the buffer is returned write-locked.
 *
 * May be called with null scp and offsetp, if we're just trying to reclaim some
 * space from the buffer pool.  In that case, the buffer will be returned
 * without being hashed into the hash table.
 */
long buf_GetNewLocked(struct cm_scache *scp, osi_hyper_t *offsetp, cm_buf_t **bufpp)
{
    cm_buf_t *bp;	/* buffer we're dealing with */
    cm_buf_t *nextBp;	/* next buffer in file hash chain */
    afs_uint32 i;	/* temp */
    cm_req_t req;

    cm_InitReq(&req);	/* just in case */

#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */

    while(1) {
      retry:
        lock_ObtainRead(&scp->bufCreateLock);
        lock_ObtainWrite(&buf_globalLock);
        /* check to see if we lost the race */
        if (scp) {
            if (bp = buf_FindLocked(scp, offsetp)) {
		/* Do not call buf_ReleaseLocked() because we 
		 * do not want to allow the buffer to be added
		 * to the free list.
		 */
                bp->refCount--;
                lock_ReleaseWrite(&buf_globalLock);
                lock_ReleaseRead(&scp->bufCreateLock);
                return CM_BUF_EXISTS;
            }
        }

	/* does this fix the problem below?  it's a simple solution. */
	if (!cm_data.buf_freeListEndp)
	{
	    lock_ReleaseWrite(&buf_globalLock);
            lock_ReleaseRead(&scp->bufCreateLock);
	    osi_Log0(afsd_logp, "buf_GetNewLocked: Free Buffer List is empty - sleeping 200ms");
	    Sleep(200);
	    goto retry;
	}

        /* for debugging, assert free list isn't empty, although we
         * really should try waiting for a running tranasction to finish
         * instead of this; or better, we should have a transaction
         * throttler prevent us from entering this situation.
         */
        osi_assertx(cm_data.buf_freeListEndp != NULL, "buf_GetNewLocked: no free buffers");

        /* look at all buffers in free list, some of which may temp.
         * have high refcounts and which then should be skipped,
         * starting cleaning I/O for those which are dirty.  If we find
         * a clean buffer, we rehash it, lock it and return it.
         */
        for(bp = cm_data.buf_freeListEndp; bp; bp=(cm_buf_t *) osi_QPrev(&bp->q)) {
            /* check to see if it really has zero ref count.  This
             * code can bump refcounts, at least, so it may not be
             * zero.
             */
            if (bp->refCount > 0) 
                continue;
                        
            /* we don't have to lock buffer itself, since the ref
             * count is 0 and we know it will stay zero as long as
             * we hold the global lock.
             */

            /* don't recycle someone in our own chunk */
            if (!cm_FidCmp(&bp->fid, &scp->fid)
                 && (bp->offset.LowPart & (-cm_chunkSize))
                 == (offsetp->LowPart & (-cm_chunkSize)))
                continue;

            /* if this page is being filled (!) or cleaned, see if
             * the I/O has completed.  If not, skip it, otherwise
             * do the final processing for the I/O.
             */
            if (bp->flags & (CM_BUF_READING | CM_BUF_WRITING)) {
                /* probably shouldn't do this much work while
                 * holding the big lock?  Watch for contention
                 * here.
                 */
                continue;
            }
                        
            if (bp->flags & CM_BUF_DIRTY) {
                /* if the buffer is dirty, start cleaning it and
                 * move on to the next buffer.  We do this with
                 * just the lock required to minimize contention
                 * on the big lock.
                 */
                buf_HoldLocked(bp);
                lock_ReleaseWrite(&buf_globalLock);
                lock_ReleaseRead(&scp->bufCreateLock);

                /* grab required lock and clean; this only
                 * starts the I/O.  By the time we're back,
                 * it'll still be marked dirty, but it will also
                 * have the WRITING flag set, so we won't get
                 * back here.
                 */
                buf_CleanAsync(bp, &req);

                /* now put it back and go around again */
                buf_Release(bp);
                goto retry;
            }

            /* if we get here, we know that the buffer still has a 0
             * ref count, and that it is clean and has no currently
             * pending I/O.  This is the dude to return.
             * Remember that as long as the ref count is 0, we know
             * that we won't have any lock conflicts, so we can grab
             * the buffer lock out of order in the locking hierarchy.
             */
            buf_Recycle(bp);

            /* clean up junk flags */
            bp->flags &= ~(CM_BUF_EOF | CM_BUF_ERROR);
            bp->dataVersion = CM_BUF_VERSION_BAD;	/* unknown so far */

            /* now hash in as our new buffer, and give it the
             * appropriate label, if requested.
             */
            if (scp) {
                bp->flags |= CM_BUF_INHASH;
                bp->fid = scp->fid;
#ifdef DEBUG
		bp->scp = scp;
#endif
                bp->offset = *offsetp;
                i = BUF_HASH(&scp->fid, offsetp);
                bp->hashp = cm_data.buf_scacheHashTablepp[i];
                cm_data.buf_scacheHashTablepp[i] = bp;
                i = BUF_FILEHASH(&scp->fid);
                nextBp = cm_data.buf_fileHashTablepp[i];
                bp->fileHashp = nextBp;
                bp->fileHashBackp = NULL;
                if (nextBp)
                    nextBp->fileHashBackp = bp;
                cm_data.buf_fileHashTablepp[i] = bp;
            }

            /* we should move it from the lru queue.  It better still be there,
             * since we've held the global (big) lock since we found it there.
             */
            osi_assertx(bp->flags & CM_BUF_INLRU,
                         "buf_GetNewLocked: LRU screwup");

            if (cm_data.buf_freeListEndp == bp) {
                /* we're the last guy in this queue, so maintain it */
                cm_data.buf_freeListEndp = (cm_buf_t *) osi_QPrev(&bp->q);
            }
            osi_QRemove((osi_queue_t **) &cm_data.buf_freeListp, &bp->q);
            bp->flags &= ~CM_BUF_INLRU;

            /* grab the mutex so that people don't use it
             * before the caller fills it with data.  Again, no one	
             * should have been able to get to this dude to lock it.
             */
	    if (!lock_TryMutex(&bp->mx)) {
	    	osi_Log2(afsd_logp, "buf_GetNewLocked bp 0x%p cannot be mutex locked.  refCount %d should be 0",
			 bp, bp->refCount);
		osi_panic("buf_GetNewLocked: TryMutex failed",__FILE__,__LINE__);
	    }

	    /* prepare to return it.  Give it a refcount */
            bp->refCount = 1;
                        
            lock_ReleaseWrite(&buf_globalLock);
            lock_ReleaseRead(&scp->bufCreateLock);
            *bufpp = bp;

#ifdef TESTING
            buf_ValidateBufQueues();
#endif /* TESTING */
            return 0;
        } /* for all buffers in lru queue */
        lock_ReleaseWrite(&buf_globalLock);
        lock_ReleaseRead(&scp->bufCreateLock);
	osi_Log0(afsd_logp, "buf_GetNewLocked: Free Buffer List has no buffers with a zero refcount - sleeping 100ms");
	Sleep(100);		/* give some time for a buffer to be freed */
    }	/* while loop over everything */
    /* not reached */
} /* the proc */

/* get a page, returning it held but unlocked.  Doesn't fill in the page
 * with I/O, since we're going to write the whole thing new.
 */
long buf_GetNew(struct cm_scache *scp, osi_hyper_t *offsetp, cm_buf_t **bufpp)
{
    cm_buf_t *bp;
    long code;
    osi_hyper_t pageOffset;
    int created;

    created = 0;
    pageOffset.HighPart = offsetp->HighPart;
    pageOffset.LowPart = offsetp->LowPart & ~(cm_data.buf_blockSize-1);
    while (1) {
        bp = buf_Find(scp, &pageOffset);
        if (bp) {
            /* lock it and break out */
            lock_ObtainMutex(&bp->mx);
            break;
        }

        /* otherwise, we have to create a page */
        code = buf_GetNewLocked(scp, &pageOffset, &bp);

        /* check if the buffer was created in a race condition branch.
         * If so, go around so we can hold a reference to it. 
         */
        if (code == CM_BUF_EXISTS) 
            continue;

        /* something else went wrong */
        if (code != 0) 
            return code;

        /* otherwise, we have a locked buffer that we just created */
        created = 1;
        break;
    } /* big while loop */

    /* wait for reads */
    if (bp->flags & CM_BUF_READING)
        buf_WaitIO(scp, bp);

    /* once it has been read once, we can unlock it and return it, still
     * with its refcount held.
     */
    lock_ReleaseMutex(&bp->mx);
    *bufpp = bp;
    osi_Log4(buf_logp, "buf_GetNew returning bp 0x%p for scp 0x%p, offset 0x%x:%08x",
              bp, scp, offsetp->HighPart, offsetp->LowPart);
    return 0;
}

/* get a page, returning it held but unlocked.  Make sure it is complete */
/* The scp must be unlocked when passed to this function */
long buf_Get(struct cm_scache *scp, osi_hyper_t *offsetp, cm_buf_t **bufpp)
{
    cm_buf_t *bp;
    long code;
    osi_hyper_t pageOffset;
    unsigned long tcount;
    int created;
    long lcount = 0;
#ifdef DISKCACHE95
    cm_diskcache_t *dcp;
#endif /* DISKCACHE95 */

    created = 0;
    pageOffset.HighPart = offsetp->HighPart;
    pageOffset.LowPart = offsetp->LowPart & ~(cm_data.buf_blockSize-1);
    while (1) {
        lcount++;
#ifdef TESTING
        buf_ValidateBufQueues();
#endif /* TESTING */

        bp = buf_Find(scp, &pageOffset);
        if (bp) {
            /* lock it and break out */
            lock_ObtainMutex(&bp->mx);

#ifdef DISKCACHE95
            /* touch disk chunk to update LRU info */
            diskcache_Touch(bp->dcp);
#endif /* DISKCACHE95 */
            break;
        }

        /* otherwise, we have to create a page */
        code = buf_GetNewLocked(scp, &pageOffset, &bp);
	/* bp->mx is now held */

        /* check if the buffer was created in a race condition branch.
         * If so, go around so we can hold a reference to it. 
         */
        if (code == CM_BUF_EXISTS) 
            continue;

        /* something else went wrong */
        if (code != 0) { 
#ifdef TESTING
            buf_ValidateBufQueues();
#endif /* TESTING */
            return code;
        }
                
        /* otherwise, we have a locked buffer that we just created */
        created = 1;
        break;
    } /* big while loop */

    /* if we get here, we have a locked buffer that may have just been
     * created, in which case it needs to be filled with data.
     */
    if (created) {
        /* load the page; freshly created pages should be idle */
        osi_assertx(!(bp->flags & (CM_BUF_READING | CM_BUF_WRITING)), "incorrect cm_buf_t flags");

        /* start the I/O; may drop lock */
        bp->flags |= CM_BUF_READING;
        code = (*cm_buf_opsp->Readp)(bp, cm_data.buf_blockSize, &tcount, NULL);

#ifdef DISKCACHE95
        code = diskcache_Get(&bp->fid, &bp->offset, bp->datap, cm_data.buf_blockSize, &bp->dataVersion, &tcount, &dcp);
        bp->dcp = dcp;    /* pointer to disk cache struct. */
#endif /* DISKCACHE95 */

        if (code != 0) {
            /* failure or queued */
            if (code != ERROR_IO_PENDING) {
                bp->error = code;
                bp->flags |= CM_BUF_ERROR;
                bp->flags &= ~CM_BUF_READING;
                if (bp->flags & CM_BUF_WAITING) {
                    osi_Log1(buf_logp, "buf_Get Waking bp 0x%p", bp);
                    osi_Wakeup((LONG_PTR) bp);
                }
                lock_ReleaseMutex(&bp->mx);
                buf_Release(bp);
#ifdef TESTING
                buf_ValidateBufQueues();
#endif /* TESTING */
                return code;
            }
        } else {
            /* otherwise, I/O completed instantly and we're done, except
             * for padding the xfr out with 0s and checking for EOF
             */
            if (tcount < (unsigned long) cm_data.buf_blockSize) {
                memset(bp->datap+tcount, 0, cm_data.buf_blockSize - tcount);
                if (tcount == 0)
                    bp->flags |= CM_BUF_EOF;
            }
            bp->flags &= ~CM_BUF_READING;
            if (bp->flags & CM_BUF_WAITING) {
                osi_Log1(buf_logp, "buf_Get Waking bp 0x%p", bp);
                osi_Wakeup((LONG_PTR) bp);
            }
        }

    } /* if created */

    /* wait for reads, either that which we started above, or that someone
     * else started.  We don't care if we return a buffer being cleaned.
     */
    if (bp->flags & CM_BUF_READING)
        buf_WaitIO(scp, bp);

    /* once it has been read once, we can unlock it and return it, still
     * with its refcount held.
     */
    lock_ReleaseMutex(&bp->mx);
    *bufpp = bp;

    /* now remove from queue; will be put in at the head (farthest from
     * being recycled) when we're done in buf_Release.
     */
    lock_ObtainWrite(&buf_globalLock);
    if (bp->flags & CM_BUF_INLRU) {
        if (cm_data.buf_freeListEndp == bp)
            cm_data.buf_freeListEndp = (cm_buf_t *) osi_QPrev(&bp->q);
        osi_QRemove((osi_queue_t **) &cm_data.buf_freeListp, &bp->q);
        bp->flags &= ~CM_BUF_INLRU;
    }
    lock_ReleaseWrite(&buf_globalLock);

    osi_Log4(buf_logp, "buf_Get returning bp 0x%p for scp 0x%p, offset 0x%x:%08x",
              bp, scp, offsetp->HighPart, offsetp->LowPart);
#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */
    return 0;
}

/* count # of elements in the free list;
 * we don't bother doing the proper locking for accessing dataVersion or flags
 * since it is a pain, and this is really just an advisory call.  If you need
 * to do better at some point, rewrite this function.
 */
long buf_CountFreeList(void)
{
    long count;
    cm_buf_t *bufp;

    count = 0;
    lock_ObtainRead(&buf_globalLock);
    for(bufp = cm_data.buf_freeListp; bufp; bufp = (cm_buf_t *) osi_QNext(&bufp->q)) {
        /* if the buffer doesn't have an identity, or if the buffer
         * has been invalidate (by having its DV stomped upon), then
         * count it as free, since it isn't really being utilized.
         */
        if (!(bufp->flags & CM_BUF_INHASH) || bufp->dataVersion <= 0)
            count++;
    }       
    lock_ReleaseRead(&buf_globalLock);
    return count;
}

/* clean a buffer synchronously */
long buf_CleanAsync(cm_buf_t *bp, cm_req_t *reqp)
{
    long code;
    osi_assertx(bp->magic == CM_BUF_MAGIC, "invalid cm_buf_t magic");

    lock_ObtainMutex(&bp->mx);
    code = buf_CleanAsyncLocked(bp, reqp);
    lock_ReleaseMutex(&bp->mx);

    return code;
}       

/* wait for a buffer's cleaning to finish */
void buf_CleanWait(cm_scache_t * scp, cm_buf_t *bp, afs_uint32 locked)
{
    osi_assertx(bp->magic == CM_BUF_MAGIC, "invalid cm_buf_t magic");

    if (!locked)
        lock_ObtainMutex(&bp->mx);
    if (bp->flags & CM_BUF_WRITING) {
        buf_WaitIO(scp, bp);
    }
    if (!locked)
        lock_ReleaseMutex(&bp->mx);
}       

/* set the dirty flag on a buffer, and set associated write-ahead log,
 * if there is one.  Allow one to be added to a buffer, but not changed.
 *
 * The buffer must be locked before calling this routine.
 */
void buf_SetDirty(cm_buf_t *bp, afs_uint32 offset, afs_uint32 length)
{
    osi_assertx(bp->magic == CM_BUF_MAGIC, "invalid cm_buf_t magic");
    osi_assertx(bp->refCount > 0, "cm_buf_t refcount 0");

    if (bp->flags & CM_BUF_DIRTY) {

	osi_Log1(buf_logp, "buf_SetDirty 0x%p already dirty", bp);

        if (bp->dirty_offset <= offset) {
            if (bp->dirty_offset + bp->dirty_length >= offset + length) {
                /* dirty_length remains the same */
            } else {
                bp->dirty_length = offset + length - bp->dirty_offset;
            }
        } else /* bp->dirty_offset > offset */ {
            if (bp->dirty_offset + bp->dirty_length >= offset + length) {
                bp->dirty_length = bp->dirty_offset + bp->dirty_length - offset;
            } else {
                bp->dirty_length = length;
            }
            bp->dirty_offset = offset;
        }
    } else {
	osi_Log1(buf_logp, "buf_SetDirty 0x%p", bp);

        /* set dirty bit */
        bp->flags |= CM_BUF_DIRTY;

        /* and turn off EOF flag, since it has associated data now */
        bp->flags &= ~CM_BUF_EOF;

        bp->dirty_offset = offset;
        bp->dirty_length = length;

        /* and add to the dirty list.  
         * we obtain a hold on the buffer for as long as it remains 
         * in the list.  buffers are only removed from the list by 
         * the buf_IncrSyncer function regardless of when else the
         * dirty flag might be cleared.
         *
         * This should never happen but just in case there is a bug
         * elsewhere, never add to the dirty list if the buffer is 
         * already there.
         */
        lock_ObtainWrite(&buf_globalLock);
        if (bp->dirtyp == NULL && cm_data.buf_dirtyListEndp != bp) {
            buf_HoldLocked(bp);
            if (!cm_data.buf_dirtyListp) {
                cm_data.buf_dirtyListp = cm_data.buf_dirtyListEndp = bp;
            } else {
                cm_data.buf_dirtyListEndp->dirtyp = bp;
                cm_data.buf_dirtyListEndp = bp;
            }
            bp->dirtyp = NULL;
        }
        lock_ReleaseWrite(&buf_globalLock);
    }
}

/* clean all buffers, reset log pointers and invalidate all buffers.
 * Called with no locks held, and returns with same.
 *
 * This function is guaranteed to clean and remove the log ptr of all the
 * buffers that were dirty or had non-zero log ptrs before the call was
 * made.  That's sufficient to clean up any garbage left around by recovery,
 * which is all we're counting on this for; there may be newly created buffers
 * added while we're running, but that should be OK.
 *
 * In an environment where there are no transactions (artificially imposed, for
 * example, when switching the database to raw mode), this function is used to
 * make sure that all updates have been written to the disk.  In that case, we don't
 * really require that we forget the log association between pages and logs, but
 * it also doesn't hurt.  Since raw mode I/O goes through this buffer package, we don't
 * have to worry about invalidating data in the buffers.
 *
 * This function is used at the end of recovery as paranoia to get the recovered
 * database out to disk.  It removes all references to the recovery log and cleans
 * all buffers.
 */
long buf_CleanAndReset(void)
{
    afs_uint32 i;
    cm_buf_t *bp;
    cm_req_t req;

    lock_ObtainRead(&buf_globalLock);
    for(i=0; i<cm_data.buf_hashSize; i++) {
        for(bp = cm_data.buf_scacheHashTablepp[i]; bp; bp = bp->hashp) {
            if ((bp->flags & CM_BUF_DIRTY) == CM_BUF_DIRTY) {
                buf_HoldLocked(bp);
                lock_ReleaseRead(&buf_globalLock);

                /* now no locks are held; clean buffer and go on */
                cm_InitReq(&req);
		req.flags |= CM_REQ_NORETRY;

		buf_CleanAsync(bp, &req);
		buf_CleanWait(NULL, bp, FALSE);

                /* relock and release buffer */
                lock_ObtainRead(&buf_globalLock);
                buf_ReleaseLocked(bp, FALSE);
            } /* dirty */
        } /* over one bucket */
    }	/* for loop over all hash buckets */

    /* release locks */
    lock_ReleaseRead(&buf_globalLock);

#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */
    
    /* and we're done */
    return 0;
}       

/* called without global lock being held, reserves buffers for callers
 * that need more than one held (not locked) at once.
 */
void buf_ReserveBuffers(afs_uint64 nbuffers)
{
    lock_ObtainWrite(&buf_globalLock);
    while (1) {
        if (cm_data.buf_reservedBufs + nbuffers > cm_data.buf_maxReservedBufs) {
            cm_data.buf_reserveWaiting = 1;
            osi_Log1(buf_logp, "buf_ReserveBuffers waiting for %d bufs", nbuffers);
            osi_SleepW((LONG_PTR) &cm_data.buf_reservedBufs, &buf_globalLock);
            lock_ObtainWrite(&buf_globalLock);
        }
        else {
            cm_data.buf_reservedBufs += nbuffers;
            break;
        }
    }
    lock_ReleaseWrite(&buf_globalLock);
}

int buf_TryReserveBuffers(afs_uint64 nbuffers)
{
    int code;

    lock_ObtainWrite(&buf_globalLock);
    if (cm_data.buf_reservedBufs + nbuffers > cm_data.buf_maxReservedBufs) {
        code = 0;
    }
    else {
        cm_data.buf_reservedBufs += nbuffers;
        code = 1;
    }
    lock_ReleaseWrite(&buf_globalLock);
    return code;
}       

/* called without global lock held, releases reservation held by
 * buf_ReserveBuffers.
 */
void buf_UnreserveBuffers(afs_uint64 nbuffers)
{
    lock_ObtainWrite(&buf_globalLock);
    cm_data.buf_reservedBufs -= nbuffers;
    if (cm_data.buf_reserveWaiting) {
        cm_data.buf_reserveWaiting = 0;
        osi_Wakeup((LONG_PTR) &cm_data.buf_reservedBufs);
    }
    lock_ReleaseWrite(&buf_globalLock);
}       

/* truncate the buffers past sizep, zeroing out the page, if we don't
 * end on a page boundary.
 *
 * Requires cm_bufCreateLock to be write locked.
 */
long buf_Truncate(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp,
                   osi_hyper_t *sizep)
{
    cm_buf_t *bufp;
    cm_buf_t *nbufp;			/* next buffer, if didRelease */
    osi_hyper_t bufEnd;
    long code;
    long bufferPos;
    afs_uint32 i;

    /* assert that cm_bufCreateLock is held in write mode */
    lock_AssertWrite(&scp->bufCreateLock);

    i = BUF_FILEHASH(&scp->fid);

    lock_ObtainRead(&buf_globalLock);
    bufp = cm_data.buf_fileHashTablepp[i];
    if (bufp == NULL) {
        lock_ReleaseRead(&buf_globalLock);
        return 0;
    }

    buf_HoldLocked(bufp);
    lock_ReleaseRead(&buf_globalLock);
    while (bufp) {
        lock_ObtainMutex(&bufp->mx);

        bufEnd.HighPart = 0;
        bufEnd.LowPart = cm_data.buf_blockSize;
        bufEnd = LargeIntegerAdd(bufEnd, bufp->offset);

        if (cm_FidCmp(&bufp->fid, &scp->fid) == 0 &&
             LargeIntegerLessThan(*sizep, bufEnd)) {
            buf_WaitIO(scp, bufp);
        }
        lock_ObtainWrite(&scp->rw);
	
        /* make sure we have a callback (so we have the right value for
         * the length), and wait for it to be safe to do a truncate.
         */
        code = cm_SyncOp(scp, bufp, userp, reqp, 0,
                          CM_SCACHESYNC_NEEDCALLBACK
                          | CM_SCACHESYNC_GETSTATUS
                          | CM_SCACHESYNC_SETSIZE
                          | CM_SCACHESYNC_BUFLOCKED);

	
	/* if we succeeded in our locking, and this applies to the right
         * file, and the truncate request overlaps the buffer either
         * totally or partially, then do something.
         */
        if (code == 0 && cm_FidCmp(&bufp->fid, &scp->fid) == 0
             && LargeIntegerLessThan(*sizep, bufEnd)) {


            /* destroy the buffer, turning off its dirty bit, if
             * we're truncating the whole buffer.  Otherwise, set
             * the dirty bit, and clear out the tail of the buffer
             * if we just overlap some.
             */
            if (LargeIntegerLessThanOrEqualTo(*sizep, bufp->offset)) {
                /* truncating the entire page */
                bufp->flags &= ~CM_BUF_DIRTY;
                bufp->dirty_offset = 0;
                bufp->dirty_length = 0;
                bufp->dataVersion = CM_BUF_VERSION_BAD;	/* known bad */
                bufp->dirtyCounter++;
            }
            else {
                /* don't set dirty, since dirty implies
                 * currently up-to-date.  Don't need to do this,
                 * since we'll update the length anyway.
                 *
                 * Zero out remainder of the page, in case we
                 * seek and write past EOF, and make this data
                 * visible again.
                 */
                bufferPos = sizep->LowPart & (cm_data.buf_blockSize - 1);
                osi_assertx(bufferPos != 0, "non-zero bufferPos");
                memset(bufp->datap + bufferPos, 0,
                        cm_data.buf_blockSize - bufferPos);
            }
        }
		
	cm_SyncOpDone( scp, bufp, 
		       CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS
		       | CM_SCACHESYNC_SETSIZE | CM_SCACHESYNC_BUFLOCKED);

        lock_ReleaseWrite(&scp->rw);
        lock_ReleaseMutex(&bufp->mx);
    
	if (!code) {
	    nbufp = bufp->fileHashp;
	    if (nbufp) 
		buf_Hold(nbufp);
	} else {
	    /* This forces the loop to end and the error code
	     * to be returned. */
	    nbufp = NULL;
	}
	buf_Release(bufp);
	bufp = nbufp;
    }

#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */

    /* done */
    return code;
}

long buf_FlushCleanPages(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_buf_t *bp;		/* buffer we're hacking on */
    cm_buf_t *nbp;
    int didRelease;
    afs_uint32 i;

    i = BUF_FILEHASH(&scp->fid);

    code = 0;
    lock_ObtainRead(&buf_globalLock);
    bp = cm_data.buf_fileHashTablepp[i];
    if (bp) 
        buf_HoldLocked(bp);
    lock_ReleaseRead(&buf_globalLock);
    
    for (; bp; bp = nbp) {
        didRelease = 0;	/* haven't released this buffer yet */

        /* clean buffer synchronously */
        if (cm_FidCmp(&bp->fid, &scp->fid) == 0) {
            lock_ObtainMutex(&bp->mx);

            /* start cleaning the buffer, and wait for it to finish */
            buf_CleanAsyncLocked(bp, reqp);
            buf_WaitIO(scp, bp);
            lock_ReleaseMutex(&bp->mx);

            code = (*cm_buf_opsp->Stabilizep)(scp, userp, reqp);
            if (code && code != CM_ERROR_BADFD) 
                goto skip;

            if (code == CM_ERROR_BADFD) {
                /* if the scp's FID is bad its because we received VNOVNODE 
                 * when attempting to FetchStatus before the write.  This
                 * page therefore contains data that can no longer be stored.
                 */
                lock_ObtainMutex(&bp->mx);
                bp->flags &= ~CM_BUF_DIRTY;
                bp->flags |= CM_BUF_ERROR;
                bp->error = CM_ERROR_BADFD;
                bp->dirty_offset = 0;
                bp->dirty_length = 0;
                bp->dataVersion = CM_BUF_VERSION_BAD;	/* known bad */
                bp->dirtyCounter++;
                lock_ReleaseMutex(&bp->mx);
            }

            /* actually, we only know that buffer is clean if ref
             * count is 1, since we don't have buffer itself locked.
             */
            if (!(bp->flags & CM_BUF_DIRTY)) {
                lock_ObtainWrite(&buf_globalLock);
                if (bp->refCount == 1) {	/* bp is held above */
                    nbp = bp->fileHashp;
                    if (nbp) 
                        buf_HoldLocked(nbp);
                    buf_ReleaseLocked(bp, TRUE);
                    didRelease = 1;
                    buf_Recycle(bp);
                }
                lock_ReleaseWrite(&buf_globalLock);
            }

	    if (code == 0)
		(*cm_buf_opsp->Unstabilizep)(scp, userp);
        }

      skip:
        if (!didRelease) {
            lock_ObtainRead(&buf_globalLock);
            nbp = bp->fileHashp;
	    if (nbp)
                buf_HoldLocked(nbp);
            buf_ReleaseLocked(bp, FALSE);
            lock_ReleaseRead(&buf_globalLock);
        }
    }	/* for loop over a bunch of buffers */

#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */

    /* done */
    return code;
}       

/* Must be called with scp->rw held */
long buf_ForceDataVersion(cm_scache_t * scp, afs_uint64 fromVersion, afs_uint64 toVersion)
{
    cm_buf_t * bp;
    afs_uint32 i;
    int found = 0;

    lock_AssertAny(&scp->rw);

    i = BUF_FILEHASH(&scp->fid);

    lock_ObtainRead(&buf_globalLock);

    for (bp = cm_data.buf_fileHashTablepp[i]; bp; bp = bp->fileHashp) {
        if (cm_FidCmp(&bp->fid, &scp->fid) == 0) {
            if (bp->dataVersion == fromVersion) {
                bp->dataVersion = toVersion;
                found = 1;
            }
        }
    }
    lock_ReleaseRead(&buf_globalLock);

    if (found)
        return 0;
    else
        return ENOENT;
}

long buf_CleanVnode(struct cm_scache *scp, cm_user_t *userp, cm_req_t *reqp)
{
    long code = 0;
    long wasDirty = 0;
    cm_buf_t *bp;		/* buffer we're hacking on */
    cm_buf_t *nbp;		/* next one */
    afs_uint32 i;

    i = BUF_FILEHASH(&scp->fid);

    lock_ObtainRead(&buf_globalLock);
    bp = cm_data.buf_fileHashTablepp[i];
    if (bp) 
        buf_HoldLocked(bp);
    lock_ReleaseRead(&buf_globalLock);
    for (; bp; bp = nbp) {
        /* clean buffer synchronously */
        if (cm_FidCmp(&bp->fid, &scp->fid) == 0) {
            lock_ObtainMutex(&bp->mx);
            if (bp->flags & CM_BUF_DIRTY) {
                if (userp) {
                    cm_HoldUser(userp);
                    if (bp->userp) 
                        cm_ReleaseUser(bp->userp);
                    bp->userp = userp;
                }   
                wasDirty = buf_CleanAsyncLocked(bp, reqp);
                buf_CleanWait(scp, bp, TRUE);
                if (bp->flags & CM_BUF_ERROR) {
                    code = bp->error;
                    if (code == 0) 
                        code = -1;
                }
            }
            lock_ReleaseMutex(&bp->mx);
        }

        lock_ObtainRead(&buf_globalLock);
        nbp = bp->fileHashp;
        if (nbp) 
            buf_HoldLocked(nbp);
        buf_ReleaseLocked(bp, FALSE);
        lock_ReleaseRead(&buf_globalLock);
    }	/* for loop over a bunch of buffers */

#ifdef TESTING
    buf_ValidateBufQueues();
#endif /* TESTING */

    /* done */
    return code;
}

#ifdef TESTING
void
buf_ValidateBufQueues(void)
{
    cm_buf_t * bp, *bpb, *bpf, *bpa;
    afs_uint32 countf=0, countb=0, counta=0;

    lock_ObtainRead(&buf_globalLock);
    for (bp = cm_data.buf_freeListEndp; bp; bp=(cm_buf_t *) osi_QPrev(&bp->q)) {
        if (bp->magic != CM_BUF_MAGIC)
            osi_panic("buf magic error",__FILE__,__LINE__);
        countb++;
        bpb = bp;
    }

    for (bp = cm_data.buf_freeListp; bp; bp=(cm_buf_t *) osi_QNext(&bp->q)) {
        if (bp->magic != CM_BUF_MAGIC)
            osi_panic("buf magic error",__FILE__,__LINE__);
        countf++;
        bpf = bp;
    }

    for (bp = cm_data.buf_allp; bp; bp=bp->allp) {
        if (bp->magic != CM_BUF_MAGIC)
            osi_panic("buf magic error",__FILE__,__LINE__);
        counta++;
        bpa = bp;
    }
    lock_ReleaseRead(&buf_globalLock);

    if (countb != countf)
	osi_panic("buf magic error",__FILE__,__LINE__);

    if (counta != cm_data.buf_nbuffers)
	osi_panic("buf magic error",__FILE__,__LINE__);
}
#endif /* TESTING */

/* dump the contents of the buf_scacheHashTablepp. */
int cm_DumpBufHashTable(FILE *outputFile, char *cookie, int lock)
{
    int zilch;
    cm_buf_t *bp;
    char output[1024];
    afs_uint32 i;
  
    if (cm_data.buf_scacheHashTablepp == NULL)
        return -1;

    if (lock)
        lock_ObtainRead(&buf_globalLock);
  
    StringCbPrintfA(output, sizeof(output), "%s - dumping buf_HashTable - buf_hashSize=%d\r\n", 
                    cookie, cm_data.buf_hashSize);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
  
    for (i = 0; i < cm_data.buf_hashSize; i++)
    {
        for (bp = cm_data.buf_scacheHashTablepp[i]; bp; bp=bp->hashp) 
        {
	    StringCbPrintfA(output, sizeof(output), 
			    "%s bp=0x%08X, hash=%d, fid (cell=%d, volume=%d, "
			    "vnode=%d, unique=%d), offset=%x:%08x, dv=%I64d, "
			    "flags=0x%x, cmFlags=0x%x, refCount=%d\r\n",
			     cookie, (void *)bp, i, bp->fid.cell, bp->fid.volume, 
			     bp->fid.vnode, bp->fid.unique, bp->offset.HighPart, 
			     bp->offset.LowPart, bp->dataVersion, bp->flags, 
			     bp->cmFlags, bp->refCount);
	    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
        }
    }
  
    StringCbPrintfA(output, sizeof(output), "%s - Done dumping buf_HashTable.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    StringCbPrintfA(output, sizeof(output), "%s - dumping buf_freeListEndp\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    for(bp = cm_data.buf_freeListEndp; bp; bp=(cm_buf_t *) osi_QPrev(&bp->q)) {
	StringCbPrintfA(output, sizeof(output), 
			 "%s bp=0x%08X, fid (cell=%d, volume=%d, "
			 "vnode=%d, unique=%d), offset=%x:%08x, dv=%I64d, "
			 "flags=0x%x, cmFlags=0x%x, refCount=%d\r\n",
			 cookie, (void *)bp, bp->fid.cell, bp->fid.volume, 
			 bp->fid.vnode, bp->fid.unique, bp->offset.HighPart, 
			 bp->offset.LowPart, bp->dataVersion, bp->flags, 
			 bp->cmFlags, bp->refCount);
	WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    }
    StringCbPrintfA(output, sizeof(output), "%s - Done dumping buf_FreeListEndp.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    StringCbPrintfA(output, sizeof(output), "%s - dumping buf_dirtyListEndp\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    for(bp = cm_data.buf_dirtyListEndp; bp; bp=(cm_buf_t *) osi_QPrev(&bp->q)) {
	StringCbPrintfA(output, sizeof(output), 
			 "%s bp=0x%08X, fid (cell=%d, volume=%d, "
			 "vnode=%d, unique=%d), offset=%x:%08x, dv=%I64d, "
			 "flags=0x%x, cmFlags=0x%x, refCount=%d\r\n",
			 cookie, (void *)bp, bp->fid.cell, bp->fid.volume, 
			 bp->fid.vnode, bp->fid.unique, bp->offset.HighPart, 
			 bp->offset.LowPart, bp->dataVersion, bp->flags, 
			 bp->cmFlags, bp->refCount);
	WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    }
    StringCbPrintfA(output, sizeof(output), "%s - Done dumping buf_dirtyListEndp.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    if (lock)
        lock_ReleaseRead(&buf_globalLock);
    return 0;
}

void buf_ForceTrace(BOOL flush)
{
    HANDLE handle;
    int len;
    char buf[256];

    if (!buf_logp) 
        return;

    len = GetTempPath(sizeof(buf)-10, buf);
    StringCbCopyA(&buf[len], sizeof(buf)-len, "/afs-buffer.log");
    handle = CreateFile(buf, GENERIC_WRITE, FILE_SHARE_READ,
			    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        osi_panic("Cannot create log file", __FILE__, __LINE__);
    }
    osi_LogPrint(buf_logp, handle);
    if (flush)
        FlushFileBuffers(handle);
    CloseHandle(handle);
}

long buf_DirtyBuffersExist(cm_fid_t *fidp)
{
    cm_buf_t *bp;
    afs_uint32 bcount = 0;
    afs_uint32 i;

    i = BUF_FILEHASH(fidp);

    for (bp = cm_data.buf_fileHashTablepp[i]; bp; bp=bp->allp, bcount++) {
	if (!cm_FidCmp(fidp, &bp->fid) && (bp->flags & CM_BUF_DIRTY))
	    return 1;
    }
    return 0;
}

#if 0
long buf_CleanDirtyBuffers(cm_scache_t *scp)
{
    cm_buf_t *bp;
    afs_uint32 bcount = 0;
    cm_fid_t * fidp = &scp->fid;

    for (bp = cm_data.buf_allp; bp; bp=bp->allp, bcount++) {
	if (!cm_FidCmp(fidp, &bp->fid) && (bp->flags & CM_BUF_DIRTY)) {
            buf_Hold(bp);
	    lock_ObtainMutex(&bp->mx);
	    bp->cmFlags &= ~CM_BUF_CMSTORING;
	    bp->flags &= ~CM_BUF_DIRTY;
            bp->dirty_offset = 0;
            bp->dirty_length = 0;
	    bp->flags |= CM_BUF_ERROR;
	    bp->error = VNOVNODE;
	    bp->dataVersion = CM_BUF_VERSION_BAD; /* bad */
	    bp->dirtyCounter++;
	    if (bp->flags & CM_BUF_WAITING) {
		osi_Log2(buf_logp, "BUF CleanDirtyBuffers Waking [scp 0x%x] bp 0x%x", scp, bp);
		osi_Wakeup((long) &bp);
	    }
	    lock_ReleaseMutex(&bp->mx);
	    buf_Release(bp);
	}
    }
    return 0;
}
#endif
