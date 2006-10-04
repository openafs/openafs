/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#include <nb30.h>
#endif /* !DJGPP */
#ifdef COMMENT
#include <malloc.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <osi.h>

#include "afsd.h"

#ifdef DEBUG
extern void afsi_log(char *pattern, ...);
#endif

osi_mutex_t cm_bufGetMutex;
#ifdef AFS_FREELANCE_CLIENT
extern osi_mutex_t cm_Freelance_Lock;
#endif

#ifdef AFS_LARGEFILES
/* we can access connp->serverp without holding a lock because that
   never changes since the connection is made. */
#define SERVERHAS64BIT(connp) (!((connp)->serverp->flags & CM_SERVERFLAG_NO64BIT))
#define SET_SERVERHASNO64BIT(connp) (cm_SetServerNo64Bit((connp)->serverp, TRUE))
#else
#define SERVERHAS64BIT(connp) (FALSE)
#define SET_SERVERHASNO64BIT(connp) (FALSE)
#endif

/* functions called back from the buffer package when reading or writing data,
 * or when holding or releasing a vnode pointer.
 */
long cm_BufWrite(void *vfidp, osi_hyper_t *offsetp, long length, long flags,
                 cm_user_t *userp, cm_req_t *reqp)
{
    /* store the data back from this buffer; the buffer is locked and held,
     * but the vnode involved isn't locked, yet.  It is held by its
     * reference from the buffer, which won't change until the buffer is
     * released by our caller.  Thus, we don't have to worry about holding
     * bufp->scp.
     */
    long code;
    cm_fid_t *fidp = vfidp;
    cm_scache_t *scp;
    long nbytes;
    long temp;
    AFSFetchStatus outStatus;
    AFSStoreStatus inStatus;
    osi_hyper_t thyper;
    AFSVolSync volSync;
    AFSFid tfid;
    struct rx_call *callp;
    struct rx_connection *rxconnp;
    osi_queueData_t *qdp;
    cm_buf_t *bufp;
    long wbytes;
    char *bufferp;
    cm_conn_t *connp;
    osi_hyper_t truncPos;
    cm_bulkIO_t biod;		/* bulk IO descriptor */
    int require_64bit_ops = 0;

    osi_assert(userp != NULL);

    /* now, the buffer may or may not be filled with good data (buf_GetNew
     * drops lots of locks, and may indeed return a properly initialized
     * buffer, although more likely it will just return a new, empty, buffer.
     */
    scp = cm_FindSCache(fidp);
    if (scp == NULL) {
        return CM_ERROR_NOSUCHFILE;	/* shouldn't happen */
    }

    cm_AFSFidFromFid(&tfid, fidp);

    lock_ObtainMutex(&scp->mx);
    if (scp->flags & CM_SCACHEFLAG_DELETED) {
	lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
	return CM_ERROR_NOSUCHFILE;
    }

    code = cm_SetupStoreBIOD(scp, offsetp, length, &biod, userp, reqp);
    if (code) {
        osi_Log1(afsd_logp, "cm_SetupStoreBIOD code %x", code);
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        return code;
    }

    if (biod.length == 0) {
        osi_Log0(afsd_logp, "cm_SetupStoreBIOD length 0");
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseBIOD(&biod, 1);	/* should be a NOOP */
        cm_ReleaseSCache(scp);
        return 0;
    }

    /* Serialize StoreData RPC's; for rationale see cm_scache.c */
    (void) cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA_EXCL);

    /* prepare the output status for the store */
    scp->mask |= CM_SCACHEMASK_CLIENTMODTIME;
    cm_StatusFromAttr(&inStatus, scp, NULL);
    truncPos = scp->length;
    if ((scp->mask & CM_SCACHEMASK_TRUNCPOS)
        && LargeIntegerLessThan(scp->truncPos, truncPos))
        truncPos = scp->truncPos;
	scp->mask &= ~CM_SCACHEMASK_TRUNCPOS;
                
    /* compute how many bytes to write from this buffer */
    thyper = LargeIntegerSubtract(scp->length, biod.offset);
    if (LargeIntegerLessThanZero(thyper)) {
        /* entire buffer is past EOF */
        nbytes = 0;
    }
    else {
        /* otherwise write out part of buffer before EOF, but not
         * more than bufferSize bytes.
         */
        if (LargeIntegerGreaterThan(thyper,
                                    ConvertLongToLargeInteger(biod.length))) {
            nbytes = biod.length;
        } else {
            /* if thyper is less than or equal to biod.length, then we
               can safely assume that the value fits in a long. */
            nbytes = thyper.LowPart;
        }
    }

    if (LargeIntegerGreaterThan(LargeIntegerAdd(biod.offset,
                                                 ConvertLongToLargeInteger(nbytes)),
                                 ConvertLongToLargeInteger(LONG_MAX)) ||
         LargeIntegerGreaterThan(truncPos,
                                 ConvertLongToLargeInteger(LONG_MAX))) {
        require_64bit_ops = 1;
    }
        
    lock_ReleaseMutex(&scp->mx);

    /* now we're ready to do the store operation */
    do {
        code = cm_Conn(&scp->fid, userp, reqp, &connp);
        if (code) 
            continue;

    retry:
        rxconnp = cm_GetRxConn(connp);
        callp = rx_NewCall(rxconnp);
        rx_PutConnection(rxconnp);

#ifdef AFS_LARGEFILES
        if (SERVERHAS64BIT(connp)) {
            osi_Log4(afsd_logp, "CALL StoreData64 scp 0x%p, offset 0x%x:%08x, length 0x%x",
                     scp, biod.offset.HighPart, biod.offset.LowPart, nbytes);

            code = StartRXAFS_StoreData64(callp, &tfid, &inStatus,
                                          biod.offset.QuadPart,
                                          nbytes,
                                          truncPos.QuadPart);
        } else {

            if (require_64bit_ops) {
                osi_Log0(afsd_logp, "Skipping StoreData.  The operation requires StoreData64");
                code = CM_ERROR_TOOBIG;
            } else {
                osi_Log4(afsd_logp, "CALL StoreData scp 0x%p, offset 0x%x:%08x, length 0x%x",
                         scp, biod.offset.HighPart, biod.offset.LowPart, nbytes);

                code = StartRXAFS_StoreData(callp, &tfid, &inStatus,
                                            biod.offset.LowPart, nbytes, truncPos.LowPart);
            }
        }
#else
        osi_Log4(afsd_logp, "CALL StoreData scp 0x%p, offset 0x%x:%08x, length 0x%x",
                 scp, biod.offset.HighPart, biod.offset.LowPart, nbytes);

        code = StartRXAFS_StoreData(callp, &tfid, &inStatus,
                                    biod.offset.LowPart, nbytes, truncPos.LowPart);
#endif

        if (code == 0) {
            /* write the data from the the list of buffers */
            qdp = NULL;
            while(nbytes > 0) {
                if (qdp == NULL)
                    qdp = biod.bufListEndp;
                else
                    qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                osi_assert(qdp != NULL);
                bufp = osi_GetQData(qdp);
                bufferp = bufp->datap;
                wbytes = nbytes;
                if (wbytes > cm_data.buf_blockSize) 
                    wbytes = cm_data.buf_blockSize;

                /* write out wbytes of data from bufferp */
                temp = rx_Write(callp, bufferp, wbytes);
                if (temp != wbytes) {
                    osi_Log3(afsd_logp, "rx_Write failed bp 0x%p, %d != %d",bufp,temp,wbytes);
                    code = -1;
                    break;
                } else {
                    osi_Log2(afsd_logp, "rx_Write succeeded bp 0x%p, %d",bufp,temp);
                }       
                nbytes -= wbytes;
            }	/* while more bytes to write */
        }		/* if RPC started successfully */
        else {
            osi_Log2(afsd_logp, "StartRXAFS_StoreData?? scp 0x%p failed (%lX)",scp,code);
        }

        if (code == 0) {
            if (SERVERHAS64BIT(connp)) {
                code = EndRXAFS_StoreData64(callp, &outStatus, &volSync);
                if (code)
                    osi_Log2(afsd_logp, "EndRXAFS_StoreData64 scp 0x%p failed (%lX)", scp, code);
            } else {
                code = EndRXAFS_StoreData(callp, &outStatus, &volSync);
                if (code)
                    osi_Log2(afsd_logp, "EndRXAFS_StoreData scp 0x%p failed (%lX)",scp,code);
            }
        }

        code = rx_EndCall(callp, code);

#ifdef AFS_LARGEFILES
        if (code == RXGEN_OPCODE && SERVERHAS64BIT(connp)) {
            SET_SERVERHASNO64BIT(connp);
            goto retry;
        }
#endif
                
    } while (cm_Analyze(connp, userp, reqp, &scp->fid, &volSync, NULL, NULL, code));

    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log2(afsd_logp, "CALL StoreData FAILURE scp 0x%p, code 0x%x", scp, code);
    else
        osi_Log1(afsd_logp, "CALL StoreData SUCCESS scp 0x%p", scp);

    /* now, clean up our state */
    lock_ObtainMutex(&scp->mx);

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);

    if (code == 0) {
        osi_hyper_t t;
        /* now, here's something a little tricky: in AFS 3, a dirty
         * length can't be directly stored, instead, a dirty chunk is
         * stored that sets the file's size (by writing and by using
         * the truncate-first option in the store call).
         *
         * At this point, we've just finished a store, and so the trunc
         * pos field is clean.  If the file's size at the server is at
         * least as big as we think it should be, then we turn off the
         * length dirty bit, since all the other dirty buffers must
         * precede this one in the file.
         *
         * The file's desired size shouldn't be smaller than what's
         * stored at the server now, since we just did the trunc pos
         * store.
         *
         * We have to turn off the length dirty bit as soon as we can,
         * so that we see updates made by other machines.
         */

        if (SERVERHAS64BIT(connp)) {
            t.LowPart = outStatus.Length;
            t.HighPart = outStatus.Length_hi;
        } else {
            t = ConvertLongToLargeInteger(outStatus.Length);
        }

        if (LargeIntegerGreaterThanOrEqualTo(t, scp->length))
            scp->mask &= ~CM_SCACHEMASK_LENGTH;

        cm_MergeStatus(scp, &outStatus, &volSync, userp, 0);
    } else {
        if (code == CM_ERROR_SPACE)
            scp->flags |= CM_SCACHEFLAG_OUTOFSPACE;
        else if (code == CM_ERROR_QUOTA)
            scp->flags |= CM_SCACHEFLAG_OVERQUOTA;
    }
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseBIOD(&biod, 1);
    cm_ReleaseSCache(scp);

    return code;
}

/*
 * Truncate the file, by sending a StoreData RPC with zero length.
 *
 * Called with scp locked.  Releases and re-obtains the lock.
 */
long cm_StoreMini(cm_scache_t *scp, cm_user_t *userp, cm_req_t *reqp)
{
    AFSFetchStatus outStatus;
    AFSStoreStatus inStatus;
    AFSVolSync volSync;
    AFSFid tfid;
    long code;
    osi_hyper_t truncPos;
    cm_conn_t *connp;
    struct rx_call *callp;
    struct rx_connection *rxconnp;
    int require_64bit_ops = 0;

    /* Serialize StoreData RPC's; for rationale see cm_scache.c */
    (void) cm_SyncOp(scp, NULL, userp, reqp, 0,
                     CM_SCACHESYNC_STOREDATA_EXCL);

    /* prepare the output status for the store */
    inStatus.Mask = AFS_SETMODTIME;
    inStatus.ClientModTime = scp->clientModTime;
    scp->mask &= ~CM_SCACHEMASK_CLIENTMODTIME;

    /* calculate truncation position */
    truncPos = scp->length;
    if ((scp->mask & CM_SCACHEMASK_TRUNCPOS)
        && LargeIntegerLessThan(scp->truncPos, truncPos))
        truncPos = scp->truncPos;
    scp->mask &= ~CM_SCACHEMASK_TRUNCPOS;

    if (LargeIntegerGreaterThan(truncPos,
                                ConvertLongToLargeInteger(LONG_MAX))) {

        require_64bit_ops = 1;
    }

    lock_ReleaseMutex(&scp->mx);

    cm_AFSFidFromFid(&tfid, &scp->fid);

    /* now we're ready to do the store operation */
    do {
        code = cm_Conn(&scp->fid, userp, reqp, &connp);
        if (code) 
            continue;

    retry:	
        rxconnp = cm_GetRxConn(connp);
        callp = rx_NewCall(rxconnp);
        rx_PutConnection(rxconnp);

#ifdef AFS_LARGEFILES
        if (SERVERHAS64BIT(connp)) {
            code = StartRXAFS_StoreData64(callp, &tfid, &inStatus,
                                          0, 0, truncPos.QuadPart);
        } else {
            if (require_64bit_ops) {
                code = CM_ERROR_TOOBIG;
            } else {
                code = StartRXAFS_StoreData(callp, &tfid, &inStatus,
                                            0, 0, truncPos.LowPart);
            }
        }
#else
        code = StartRXAFS_StoreData(callp, &tfid, &inStatus,
                                    0, 0, truncPos.LowPart);
#endif

        if (code == 0) {
            if (SERVERHAS64BIT(connp))
                code = EndRXAFS_StoreData64(callp, &outStatus, &volSync);
            else
                code = EndRXAFS_StoreData(callp, &outStatus, &volSync);
        }
        code = rx_EndCall(callp, code);

#ifdef AFS_LARGEFILES
        if (code == RXGEN_OPCODE && SERVERHAS64BIT(connp)) {
            SET_SERVERHASNO64BIT(connp);
            goto retry;
        }
#endif

    } while (cm_Analyze(connp, userp, reqp, &scp->fid, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);
        
    /* now, clean up our state */
    lock_ObtainMutex(&scp->mx);

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);

    if (code == 0) {
        osi_hyper_t t;
        /*
         * For explanation of handling of CM_SCACHEMASK_LENGTH,
         * see cm_BufWrite().
         */
        if (SERVERHAS64BIT(connp)) {
            t.HighPart = outStatus.Length_hi;
            t.LowPart = outStatus.Length;
        } else {
            t = ConvertLongToLargeInteger(outStatus.Length);
        }

        if (LargeIntegerGreaterThanOrEqualTo(t, scp->length))
            scp->mask &= ~CM_SCACHEMASK_LENGTH;
        cm_MergeStatus(scp, &outStatus, &volSync, userp, 0);
    }

    return code;
}

long cm_BufRead(cm_buf_t *bufp, long nbytes, long *bytesReadp, cm_user_t *userp)
{
    *bytesReadp = cm_data.buf_blockSize;

    /* now return a code that means that I/O is done */
    return 0;
}

/* stabilize scache entry, and return with it locked so 
 * it stays stable.
 */
long cm_BufStabilize(void *parmp, cm_user_t *userp, cm_req_t *reqp)
{
    cm_scache_t *scp;
    long code;

    scp = parmp;
        
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0, 
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_SETSIZE);
    if (code) {
        lock_ReleaseMutex(&scp->mx);
        return code;
    }
        
    return 0;
}

/* undoes the work that cm_BufStabilize does: releases lock so things can change again */
long cm_BufUnstabilize(void *parmp, cm_user_t *userp)
{
    cm_scache_t *scp;
        
    scp = parmp;
        
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_SETSIZE);

    lock_ReleaseMutex(&scp->mx);
        
    /* always succeeds */
    return 0;
}

cm_buf_ops_t cm_bufOps = {
    cm_BufWrite,
    cm_BufRead,
    cm_BufStabilize,
    cm_BufUnstabilize
};

long cm_ValidateDCache(void)
{
    return buf_ValidateBuffers();
}

long cm_ShutdownDCache(void)
{
    return 0;
}

int cm_InitDCache(int newFile, long chunkSize, afs_uint64 nbuffers)
{
    lock_InitializeMutex(&cm_bufGetMutex, "buf_Get mutex");
    return buf_Init(newFile, &cm_bufOps, nbuffers);
}

/* check to see if we have an up-to-date buffer.  The buffer must have
 * previously been obtained by calling buf_Get.
 *
 * Make sure we have a callback, and that the dataversion matches.
 *
 * Scp must be locked.
 *
 * Bufp *may* be locked.
 */
int cm_HaveBuffer(cm_scache_t *scp, cm_buf_t *bufp, int isBufLocked)
{
    int code;
    if (!cm_HaveCallback(scp))
        return 0;
    if ((bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMFULLYFETCHED)) == (CM_BUF_CMFETCHING | CM_BUF_CMFULLYFETCHED))
        return 1;
    if (bufp->dataVersion == scp->dataVersion)
        return 1;
    if (!isBufLocked) {
        code = lock_TryMutex(&bufp->mx);
        if (code == 0) {
            /* don't have the lock, and can't lock it, then
             * return failure.
             */
            return 0;
        }
    }

    /* remember dirty flag for later */
    code = bufp->flags & CM_BUF_DIRTY;

    /* release lock if we obtained it here */
    if (!isBufLocked) 
        lock_ReleaseMutex(&bufp->mx);

    /* if buffer was dirty, buffer is acceptable for use */
    if (code) 
        return 1;
    else 
        return 0;
}

/* used when deciding whether to do a prefetch or not */
long cm_CheckFetchRange(cm_scache_t *scp, osi_hyper_t *startBasep, long length,
                        cm_user_t *up, cm_req_t *reqp, osi_hyper_t *realBasep)
{
    osi_hyper_t toffset;
    osi_hyper_t tbase;
    long code;
    cm_buf_t *bp;
    int stop;
        
    /* now scan all buffers in the range, looking for any that look like
     * they need work.
     */
    tbase = *startBasep;
    stop = 0;
    lock_ObtainMutex(&scp->mx);
    while(length > 0) {
        /* get callback so we can do a meaningful dataVersion comparison */
        code = cm_SyncOp(scp, NULL, up, reqp, 0,
                         CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code) {
            scp->flags &= ~CM_SCACHEFLAG_PREFETCHING;
            lock_ReleaseMutex(&scp->mx);
            return code;
        }
                
        if (LargeIntegerGreaterThanOrEqualTo(tbase, scp->length)) {
            /* we're past the end of file */
            break;
        }

        bp = buf_Find(scp, &tbase);
        /* We cheat slightly by not locking the bp mutex. */
        if (bp) {
            if ((bp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING)) == 0
                 && bp->dataVersion != scp->dataVersion)
                stop = 1;
            buf_Release(bp);
	    bp = NULL;
        }
        else 
            stop = 1;

        /* if this buffer is essentially guaranteed to require a fetch,
         * break out here and return this position.
         */
        if (stop) 
            break;
                
        toffset.LowPart = cm_data.buf_blockSize;
        toffset.HighPart = 0;
        tbase = LargeIntegerAdd(toffset, tbase);
        length -= cm_data.buf_blockSize;
    }
        
    /* if we get here, either everything is fine or stop stopped us at a
     * particular buffer in the range that definitely needs to be fetched.
     */
    if (stop == 0) {
        /* return non-zero code since realBasep won't be valid */
        scp->flags &= ~CM_SCACHEFLAG_PREFETCHING;
        code = -1;
    }   
    else {
        /* successfully found a page that will need fetching */
        *realBasep = tbase;
        code = 0;
    }
    lock_ReleaseMutex(&scp->mx);
    return code;
}

void cm_BkgStore(cm_scache_t *scp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4,
                 cm_user_t *userp)
{
    osi_hyper_t toffset;
    long length;
    cm_req_t req;
    long code;

    if (scp->flags & CM_SCACHEFLAG_DELETED) {
	osi_Log4(afsd_logp, "Skipping BKG store - Deleted scp 0x%p, offset 0x%x:%08x, length 0x%x", scp, p2, p1, p3);
    } else {
	cm_InitReq(&req);
#ifdef NO_BKG_RETRIES
	req.flags |= CM_REQ_NORETRY;
#endif

	toffset.LowPart = p1;
	toffset.HighPart = p2;
	length = p3;

	osi_Log4(afsd_logp, "Starting BKG store scp 0x%p, offset 0x%x:%08x, length 0x%x", scp, p2, p1, p3);

	code = cm_BufWrite(&scp->fid, &toffset, length, /* flags */ 0, userp, &req);
    }

    lock_ObtainMutex(&scp->mx);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_ASYNCSTORE);
    lock_ReleaseMutex(&scp->mx);
}

/* Called with scp locked */
void cm_ClearPrefetchFlag(long code, cm_scache_t *scp, osi_hyper_t *base)
{
    osi_hyper_t thyper;

    if (code == 0) {
        thyper.LowPart = cm_chunkSize;
        thyper.HighPart = 0;
        thyper =  LargeIntegerAdd(*base, thyper);
        thyper.LowPart &= (-cm_chunkSize);
        if (LargeIntegerGreaterThan(*base, scp->prefetch.base))
            scp->prefetch.base = *base;
        if (LargeIntegerGreaterThan(thyper, scp->prefetch.end))
            scp->prefetch.end = thyper;
    }
    scp->flags &= ~CM_SCACHEFLAG_PREFETCHING;
}

/* do the prefetch */
void cm_BkgPrefetch(cm_scache_t *scp, long p1, long p2, long p3, long p4,
                    cm_user_t *userp)
{
    long length;
    osi_hyper_t base;
    long code;
    cm_buf_t *bp;
    int cpff = 0;			/* cleared prefetch flag */
    cm_req_t req;

    cm_InitReq(&req);
    req.flags |= CM_REQ_NORETRY;
        
    base.LowPart = p1;
    base.HighPart = p2;
    length = p3;
        
    osi_Log2(afsd_logp, "Starting BKG prefetch scp 0x%p, base 0x%x", scp, p1);

    code = buf_Get(scp, &base, &bp);

    lock_ObtainMutex(&scp->mx);

    if (code || (bp->cmFlags & CM_BUF_CMFETCHING)) {
        scp->flags &= ~CM_SCACHEFLAG_PREFETCHING;
        lock_ReleaseMutex(&scp->mx);
        return;
    }

    code = cm_GetBuffer(scp, bp, &cpff, userp, &req);
    if (!cpff) 
        cm_ClearPrefetchFlag(code, scp, &base);
    lock_ReleaseMutex(&scp->mx);
    buf_Release(bp);
    return;
}

/* a read was issued to offsetp, and we have to determine whether we should
 * do a prefetch.
 */
void cm_ConsiderPrefetch(cm_scache_t *scp, osi_hyper_t *offsetp,
                         cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    osi_hyper_t realBase;
    osi_hyper_t readBase;
        
    readBase = *offsetp;
    /* round up to chunk boundary */
    readBase.LowPart += (cm_chunkSize-1);
    readBase.LowPart &= (-cm_chunkSize);

    lock_ObtainMutex(&scp->mx);
    if ((scp->flags & CM_SCACHEFLAG_PREFETCHING)
         || LargeIntegerLessThanOrEqualTo(readBase, scp->prefetch.base)) {
        lock_ReleaseMutex(&scp->mx);
        return;
    }
    scp->flags |= CM_SCACHEFLAG_PREFETCHING;

    /* start the scan at the latter of the end of this read or
     * the end of the last fetched region.
     */
    if (LargeIntegerGreaterThan(scp->prefetch.end, readBase))
        readBase = scp->prefetch.end;

    lock_ReleaseMutex(&scp->mx);

    code = cm_CheckFetchRange(scp, &readBase, cm_chunkSize, userp, reqp,
                              &realBase);
    if (code) 
        return;	/* can't find something to prefetch */

    osi_Log2(afsd_logp, "BKG Prefetch request scp 0x%p, base 0x%x",
             scp, realBase.LowPart);

    cm_QueueBKGRequest(scp, cm_BkgPrefetch, realBase.LowPart,
                       realBase.HighPart, cm_chunkSize, 0, userp);
}

/* scp must be locked; temporarily unlocked during processing.
 * If returns 0, returns buffers held in biop, and with
 * CM_BUF_CMSTORING set.
 *
 * Caller *must* set CM_BUF_WRITING and reset the over.hEvent field if the
 * buffer is ever unlocked before CM_BUF_DIRTY is cleared.  And if
 * CM_BUF_WRITING is ever viewed by anyone, then it must be cleared, sleepers
 * must be woken, and the event must be set when the I/O is done.  All of this
 * is required so that buf_WaitIO synchronizes properly with the buffer as it
 * is being written out.
 */
long cm_SetupStoreBIOD(cm_scache_t *scp, osi_hyper_t *inOffsetp, long inSize,
                       cm_bulkIO_t *biop, cm_user_t *userp, cm_req_t *reqp)
{
    cm_buf_t *bufp;
    osi_queueData_t *qdp;
    osi_hyper_t thyper;
    osi_hyper_t tbase;
    osi_hyper_t scanStart;		/* where to start scan for dirty pages */
    osi_hyper_t scanEnd;		/* where to stop scan for dirty pages */
    osi_hyper_t firstModOffset;	/* offset of first modified page in range */
    long temp;
    long code;
    long flags;			/* flags to cm_SyncOp */
        
    /* clear things out */
    biop->scp = scp;		/* don't hold */
    biop->offset = *inOffsetp;
    biop->length = 0;
    biop->bufListp = NULL;
    biop->bufListEndp = NULL;
    biop->reserved = 0;

    /* reserve a chunk's worth of buffers */
    lock_ReleaseMutex(&scp->mx);
    buf_ReserveBuffers(cm_chunkSize / cm_data.buf_blockSize);
    lock_ObtainMutex(&scp->mx);

    bufp = NULL;
    for (temp = 0; temp < inSize; temp += cm_data.buf_blockSize) {
        thyper.HighPart = 0;
        thyper.LowPart = temp;
        tbase = LargeIntegerAdd(*inOffsetp, thyper);

        bufp = buf_Find(scp, &tbase);
        if (bufp) {
            /* get buffer mutex and scp mutex safely */
            lock_ReleaseMutex(&scp->mx);
            lock_ObtainMutex(&bufp->mx);
            lock_ObtainMutex(&scp->mx);

            flags = CM_SCACHESYNC_NEEDCALLBACK
                | CM_SCACHESYNC_GETSTATUS
                    | CM_SCACHESYNC_STOREDATA
                        | CM_SCACHESYNC_BUFLOCKED;
            code = cm_SyncOp(scp, bufp, userp, reqp, 0, flags); 
            if (code) {
                lock_ReleaseMutex(&bufp->mx);
                buf_Release(bufp);
                buf_UnreserveBuffers(cm_chunkSize / cm_data.buf_blockSize);
                return code;
            }   
                        
            /* if the buffer is dirty, we're done */
            if (bufp->flags & CM_BUF_DIRTY) {
                osi_assertx(!(bufp->flags & CM_BUF_WRITING),
                            "WRITING w/o CMSTORING in SetupStoreBIOD");
                bufp->flags |= CM_BUF_WRITING;
                break;
            }

            /* this buffer is clean, so there's no reason to process it */
            cm_SyncOpDone(scp, bufp, flags);
            lock_ReleaseMutex(&bufp->mx);
            buf_Release(bufp);
	    bufp = NULL;
        }       
    }

    biop->reserved = 1;
        
    /* if we get here, if bufp is null, we didn't find any dirty buffers
     * that weren't already being stored back, so we just quit now.
     */
    if (!bufp) {
        return 0;
    }

    /* don't need buffer mutex any more */
    lock_ReleaseMutex(&bufp->mx);
        
    /* put this element in the list */
    qdp = osi_QDAlloc();
    osi_SetQData(qdp, bufp);
    /* don't have to hold bufp, since held by buf_Find above */
    osi_QAddH((osi_queue_t **) &biop->bufListp,
              (osi_queue_t **) &biop->bufListEndp,
              &qdp->q);
    biop->length = cm_data.buf_blockSize;
    firstModOffset = bufp->offset;
    biop->offset = firstModOffset;
    bufp = NULL;	/* this buffer and reference added to the queue */

    /* compute the window surrounding *inOffsetp of size cm_chunkSize */
    scanStart = *inOffsetp;
    scanStart.LowPart &= (-cm_chunkSize);
    thyper.LowPart = cm_chunkSize;
    thyper.HighPart = 0;
    scanEnd = LargeIntegerAdd(scanStart, thyper);

    flags = CM_SCACHESYNC_NEEDCALLBACK
        | CM_SCACHESYNC_GETSTATUS
        | CM_SCACHESYNC_STOREDATA
        | CM_SCACHESYNC_BUFLOCKED
        | CM_SCACHESYNC_NOWAIT;

    /* start by looking backwards until scanStart */
    thyper.HighPart = 0;		/* hyper version of cm_data.buf_blockSize */
    thyper.LowPart = cm_data.buf_blockSize;
    tbase = LargeIntegerSubtract(firstModOffset, thyper);
    while(LargeIntegerGreaterThanOrEqualTo(tbase, scanStart)) {
        /* see if we can find the buffer */
        bufp = buf_Find(scp, &tbase);
        if (!bufp) 
            break;

        /* try to lock it, and quit if we can't (simplifies locking) */
        lock_ReleaseMutex(&scp->mx);
        code = lock_TryMutex(&bufp->mx);
        lock_ObtainMutex(&scp->mx);
        if (code == 0) {
            buf_Release(bufp);
	    bufp = NULL;
            break;
        }
                
        code = cm_SyncOp(scp, bufp, userp, reqp, 0, flags);
        if (code) {
            lock_ReleaseMutex(&bufp->mx);
            buf_Release(bufp);
	    bufp = NULL;
            break;
        }
                
        if (!(bufp->flags & CM_BUF_DIRTY)) {
            /* buffer is clean, so we shouldn't add it */
            cm_SyncOpDone(scp, bufp, flags);
            lock_ReleaseMutex(&bufp->mx);
            buf_Release(bufp);
	    bufp = NULL;
            break;
        }

        /* don't need buffer mutex any more */
        lock_ReleaseMutex(&bufp->mx);

        /* we have a dirty buffer ready for storing.  Add it to the tail
         * of the list, since it immediately precedes all of the disk
         * addresses we've already collected.
         */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, bufp);
        /* no buf_hold necessary, since we have it held from buf_Find */
        osi_QAddT((osi_queue_t **) &biop->bufListp,
                  (osi_queue_t **) &biop->bufListEndp,
                  &qdp->q);
	bufp = NULL;		/* added to the queue */

        /* update biod info describing the transfer */
        biop->offset = LargeIntegerSubtract(biop->offset, thyper);
        biop->length += cm_data.buf_blockSize;

        /* update loop pointer */
        tbase = LargeIntegerSubtract(tbase, thyper);
    }	/* while loop looking for pages preceding the one we found */

    /* now, find later dirty, contiguous pages, and add them to the list */
    thyper.HighPart = 0;		/* hyper version of cm_data.buf_blockSize */
    thyper.LowPart = cm_data.buf_blockSize;
    tbase = LargeIntegerAdd(firstModOffset, thyper);
    while(LargeIntegerLessThan(tbase, scanEnd)) {
        /* see if we can find the buffer */
        bufp = buf_Find(scp, &tbase);
        if (!bufp) 
            break;

        /* try to lock it, and quit if we can't (simplifies locking) */
        lock_ReleaseMutex(&scp->mx);
        code = lock_TryMutex(&bufp->mx);
        lock_ObtainMutex(&scp->mx);
        if (code == 0) {
            buf_Release(bufp);
	    bufp = NULL;
            break;
        }

        code = cm_SyncOp(scp, bufp, userp, reqp, 0, flags);
        if (code) {
            lock_ReleaseMutex(&bufp->mx);
            buf_Release(bufp);
	    bufp = NULL;
            break;
        }
                
        if (!(bufp->flags & CM_BUF_DIRTY)) {
            /* buffer is clean, so we shouldn't add it */
            cm_SyncOpDone(scp, bufp, flags);
            lock_ReleaseMutex(&bufp->mx);
            buf_Release(bufp);
	    bufp = NULL;
            break;
        }

        /* don't need buffer mutex any more */
        lock_ReleaseMutex(&bufp->mx);

        /* we have a dirty buffer ready for storing.  Add it to the head
         * of the list, since it immediately follows all of the disk
         * addresses we've already collected.
         */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, bufp);
        /* no buf_hold necessary, since we have it held from buf_Find */
        osi_QAddH((osi_queue_t **) &biop->bufListp,
                  (osi_queue_t **) &biop->bufListEndp,
                  &qdp->q);
	bufp = NULL;

        /* update biod info describing the transfer */
        biop->length += cm_data.buf_blockSize;
                
        /* update loop pointer */
        tbase = LargeIntegerAdd(tbase, thyper);
    }	/* while loop looking for pages following the first page we found */
	
    /* finally, we're done */
    return 0;
}

/* scp must be locked; temporarily unlocked during processing.
 * If returns 0, returns buffers held in biop, and with
 * CM_BUF_CMFETCHING flags set.
 * If an error is returned, we don't return any buffers.
 */
long cm_SetupFetchBIOD(cm_scache_t *scp, osi_hyper_t *offsetp,
			cm_bulkIO_t *biop, cm_user_t *up, cm_req_t *reqp)
{
    long code;
    cm_buf_t *tbp;
    osi_hyper_t toffset;		/* a long long temp variable */
    osi_hyper_t pageBase;		/* base offset we're looking at */
    osi_queueData_t *qdp;		/* one temp queue structure */
    osi_queueData_t *tqdp;		/* another temp queue structure */
    long collected;			/* how many bytes have been collected */
    int isFirst;
    long flags;
    osi_hyper_t fileSize;		/* the # of bytes in the file */
    osi_queueData_t *heldBufListp;	/* we hold all buffers in this list */
    osi_queueData_t *heldBufListEndp;	/* first one */
    int reserving;

    biop->scp = scp;
    biop->offset = *offsetp;
    /* null out the list of buffers */
    biop->bufListp = biop->bufListEndp = NULL;
    biop->reserved = 0;

    /* first lookup the file's length, so we know when to stop */
    code = cm_SyncOp(scp, NULL, up, reqp, 0, 
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) 
        return code;
        
    /* copy out size, since it may change */
    fileSize = scp->serverLength;
        
    lock_ReleaseMutex(&scp->mx);

    pageBase = *offsetp;
    collected = pageBase.LowPart & (cm_chunkSize - 1);
    heldBufListp = NULL;
    heldBufListEndp = NULL;

    /*
     * Obtaining buffers can cause dirty buffers to be recycled, which
     * can cause a storeback, so cannot be done while we have buffers
     * reserved.
     *
     * To get around this, we get buffers twice.  Before reserving buffers,
     * we obtain and release each one individually.  After reserving
     * buffers, we try to obtain them again, but only by lookup, not by
     * recycling.  If a buffer has gone away while we were waiting for
     * the others, we just use whatever buffers we already have.
     *
     * On entry to this function, we are already holding a buffer, so we
     * can't wait for reservation.  So we call buf_TryReserveBuffers()
     * instead.  Not only that, we can't really even call buf_Get(), for
     * the same reason.  We can't avoid that, though.  To avoid deadlock
     * we allow only one thread to be executing the buf_Get()-buf_Release()
     * sequence at a time.
     */

    // lock_ObtainMutex(&cm_bufGetMutex);
    /* first hold all buffers, since we can't hold any locks in buf_Get */
    while (1) {
        /* stop at chunk boundary */
        if (collected >= cm_chunkSize) 
            break;
                
        /* see if the next page would be past EOF */
        if (LargeIntegerGreaterThanOrEqualTo(pageBase, fileSize)) 
            break;

        code = buf_Get(scp, &pageBase, &tbp);
        if (code) {
            //lock_ReleaseMutex(&cm_bufGetMutex);
            lock_ObtainMutex(&scp->mx);
            cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            return code;
        }
                
        buf_Release(tbp);

        toffset.HighPart = 0;
        toffset.LowPart = cm_data.buf_blockSize;
        pageBase = LargeIntegerAdd(toffset, pageBase);
        collected += cm_data.buf_blockSize;
    }

    /* reserve a chunk's worth of buffers if possible */
    reserving = buf_TryReserveBuffers(cm_chunkSize / cm_data.buf_blockSize);

    // lock_ReleaseMutex(&cm_bufGetMutex);

    pageBase = *offsetp;
    collected = pageBase.LowPart & (cm_chunkSize - 1);

    /* now hold all buffers, if they are still there */
    while (1) {
        /* stop at chunk boundary */
        if (collected >= cm_chunkSize) 
            break;
                
        /* see if the next page would be past EOF */
        if (LargeIntegerGreaterThanOrEqualTo(pageBase, fileSize)) 
            break;

        tbp = buf_Find(scp, &pageBase);
        if (!tbp) 
            break;

        /* add the buffer to the list */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, tbp);
        osi_QAddH((osi_queue_t **)&heldBufListp, 
		  (osi_queue_t **)&heldBufListEndp, 
		  &qdp->q);
        /* leave tbp held (from buf_Get) */

        if (!reserving) 
            break;

        collected += cm_data.buf_blockSize;
        toffset.HighPart = 0;
        toffset.LowPart = cm_data.buf_blockSize;
        pageBase = LargeIntegerAdd(toffset, pageBase);
    }

    /* look at each buffer, adding it into the list if it looks idle and
     * filled with old data.  One special case: wait for idle if it is the
     * first buffer since we really need that one for our caller to make
     * any progress.
     */
    isFirst = 1;
    collected = 0;		/* now count how many we'll really use */
    for (tqdp = heldBufListEndp;
        tqdp;
          tqdp = (osi_queueData_t *) osi_QPrev(&tqdp->q)) {
        /* get a ptr to the held buffer */
        tbp = osi_GetQData(tqdp);
        pageBase = tbp->offset;

        /* now lock the buffer lock */
        lock_ObtainMutex(&tbp->mx);
        lock_ObtainMutex(&scp->mx);

        /* don't bother fetching over data that is already current */
        if (tbp->dataVersion == scp->dataVersion) {
            /* we don't need this buffer, since it is current */
            lock_ReleaseMutex(&scp->mx);
            lock_ReleaseMutex(&tbp->mx);
            break;
        }

        flags = CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_FETCHDATA
            | CM_SCACHESYNC_BUFLOCKED;
        if (!isFirst) 
            flags |= CM_SCACHESYNC_NOWAIT;

        /* wait for the buffer to serialize, if required.  Doesn't
         * release the scp or buffer lock(s) if NOWAIT is specified.
         */
        code = cm_SyncOp(scp, tbp, up, reqp, 0, flags);
        if (code) {
            lock_ReleaseMutex(&scp->mx);
            lock_ReleaseMutex(&tbp->mx);
            break;
        }
                
        /* don't fetch over dirty buffers */
        if (tbp->flags & CM_BUF_DIRTY) {
            cm_SyncOpDone(scp, tbp, flags);
            lock_ReleaseMutex(&scp->mx);
            lock_ReleaseMutex(&tbp->mx);
            break;
        }

        /* Release locks */
        lock_ReleaseMutex(&scp->mx);
        lock_ReleaseMutex(&tbp->mx);

        /* add the buffer to the list */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, tbp);
        osi_QAddH((osi_queue_t **)&biop->bufListp, 
		  (osi_queue_t **)&biop->bufListEndp, 
		  &qdp->q);
        buf_Hold(tbp);

        /* from now on, a failure just stops our collection process, but
         * we still do the I/O to whatever we've already managed to collect.
         */
        isFirst = 0;
        collected += cm_data.buf_blockSize;
    }
        
    /* now, we've held in biop->bufListp all the buffer's we're really
     * interested in.  We also have holds left from heldBufListp, and we
     * now release those holds on the buffers.
     */
    for (qdp = heldBufListp; qdp; qdp = tqdp) {
        tqdp = (osi_queueData_t *) osi_QNext(&qdp->q);
        tbp = osi_GetQData(qdp);
	osi_QRemoveHT((osi_queue_t **) &heldBufListp,
		      (osi_queue_t **) &heldBufListEndp,
		      &qdp->q);
        osi_QDFree(qdp);
        buf_Release(tbp);
    }

    /* Caller expects this */
    lock_ObtainMutex(&scp->mx);
 
    /* if we got a failure setting up the first buffer, then we don't have
     * any side effects yet, and we also have failed an operation that the
     * caller requires to make any progress.  Give up now.
     */
    if (code && isFirst) {
        buf_UnreserveBuffers(cm_chunkSize / cm_data.buf_blockSize);
        return code;
    }
        
    /* otherwise, we're still OK, and should just return the I/O setup we've
     * got.
     */
    biop->length = collected;
    biop->reserved = reserving;
    return 0;
}

/* release a bulk I/O structure that was setup by cm_SetupFetchBIOD or by
 * cm_SetupStoreBIOD
 */
void cm_ReleaseBIOD(cm_bulkIO_t *biop, int isStore)
{
    cm_scache_t *scp;
    cm_buf_t *bufp;
    osi_queueData_t *qdp;
    osi_queueData_t *nqdp;
    int flags;

    /* Give back reserved buffers */
    if (biop->reserved)
        buf_UnreserveBuffers(cm_chunkSize / cm_data.buf_blockSize);
        
    flags = CM_SCACHESYNC_NEEDCALLBACK;
    if (isStore)
        flags |= CM_SCACHESYNC_STOREDATA;
    else
        flags |= CM_SCACHESYNC_FETCHDATA;

    scp = biop->scp;
    for(qdp = biop->bufListp; qdp; qdp = nqdp) {
        /* lookup next guy first, since we're going to free this one */
        nqdp = (osi_queueData_t *) osi_QNext(&qdp->q);
                
        /* extract buffer and free queue data */
        bufp = osi_GetQData(qdp);
	osi_QRemoveHT((osi_queue_t **) &biop->bufListp,
		      (osi_queue_t **) &biop->bufListEndp,
		      &qdp->q);
        osi_QDFree(qdp);

        /* now, mark I/O as done, unlock the buffer and release it */
        lock_ObtainMutex(&bufp->mx);
        lock_ObtainMutex(&scp->mx);
        cm_SyncOpDone(scp, bufp, flags);
                
        /* turn off writing and wakeup users */
        if (isStore) {
            if (bufp->flags & CM_BUF_WAITING) {
                osi_Log2(afsd_logp, "cm_ReleaseBIOD Waking [scp 0x%p] bp 0x%p", scp, bufp);
                osi_Wakeup((LONG_PTR) bufp);
            }
            bufp->flags &= ~(CM_BUF_WRITING | CM_BUF_DIRTY);
        }

        lock_ReleaseMutex(&scp->mx);
        lock_ReleaseMutex(&bufp->mx);
        buf_Release(bufp);
	bufp = NULL;
    }

    /* clean things out */
    biop->bufListp = NULL;
    biop->bufListEndp = NULL;
}   

/* Fetch a buffer.  Called with scp locked.
 * The scp is locked on return.
 */
long cm_GetBuffer(cm_scache_t *scp, cm_buf_t *bufp, int *cpffp, cm_user_t *up,
                  cm_req_t *reqp)
{
    long code;
    afs_int32 nbytes;			/* bytes in transfer */
    afs_int32 nbytes_hi = 0;            /* high-order 32 bits of bytes in transfer */
    afs_int64 length_found = 0;
    long rbytes;			/* bytes in rx_Read call */
    long temp;
    AFSFetchStatus afsStatus;
    AFSCallBack callback;
    AFSVolSync volSync;
    char *bufferp;
    cm_buf_t *tbufp;		        /* buf we're filling */
    osi_queueData_t *qdp;		/* q element we're scanning */
    AFSFid tfid;
    struct rx_call *callp;
    struct rx_connection *rxconnp;
    cm_bulkIO_t biod;		/* bulk IO descriptor */
    cm_conn_t *connp;
    int getroot;
    long t1, t2;
    int require_64bit_ops = 0;

    /* now, the buffer may or may not be filled with good data (buf_GetNew
     * drops lots of locks, and may indeed return a properly initialized
     * buffer, although more likely it will just return a new, empty, buffer.
     */

#ifdef AFS_FREELANCE_CLIENT

    // yj: if they're trying to get the /afs directory, we need to
    // handle it differently, since it's local rather than on any
    // server

    getroot = (scp==cm_data.rootSCachep);
    if (getroot)
        osi_Log1(afsd_logp,"GetBuffer returns cm_data.rootSCachep=%x",cm_data.rootSCachep);
#endif

    cm_AFSFidFromFid(&tfid, &scp->fid);

    code = cm_SetupFetchBIOD(scp, &bufp->offset, &biod, up, reqp);
    if (code) {
        /* couldn't even get the first page setup properly */
        osi_Log1(afsd_logp, "SetupFetchBIOD failure code %d", code);
        return code;
    }

    /* once we get here, we have the callback in place, we know that no one
     * is fetching the data now.  Check one last time that we still have
     * the wrong data, and then fetch it if we're still wrong.
     *
     * We can lose a race condition and end up with biod.length zero, in
     * which case we just retry.
     */
    if (bufp->dataVersion == scp->dataVersion || biod.length == 0) {
        osi_Log3(afsd_logp, "Bad DVs %d, %d or length 0x%x",
                 bufp->dataVersion, scp->dataVersion, biod.length);
        if ((bufp->dataVersion == -1
             || bufp->dataVersion < scp->dataVersion)
             && LargeIntegerGreaterThanOrEqualTo(bufp->offset,
                                                 scp->serverLength)) {
            if (bufp->dataVersion == -1)
                memset(bufp->datap, 0, cm_data.buf_blockSize);
            bufp->dataVersion = scp->dataVersion;
        }
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseBIOD(&biod, 0);
        lock_ObtainMutex(&scp->mx);
        return 0;
    }

    lock_ReleaseMutex(&scp->mx);

    if (LargeIntegerGreaterThan(LargeIntegerAdd(biod.offset,
                                                ConvertLongToLargeInteger(biod.length)),
                                ConvertLongToLargeInteger(LONG_MAX))) {
        require_64bit_ops = 1;
    }

#ifdef DISKCACHE95
    DPRINTF("cm_GetBuffer: fetching data scpDV=%d bufDV=%d scp=%x bp=%x dcp=%x\n",
            scp->dataVersion, bufp->dataVersion, scp, bufp, bufp->dcp);
#endif /* DISKCACHE95 */

#ifdef AFS_FREELANCE_CLIENT

    // yj code
    // if getroot then we don't need to make any calls
    // just return fake data
	
    if (cm_freelanceEnabled && getroot) {
        // setup the fake status			
        afsStatus.InterfaceVersion = 0x1;
        afsStatus.FileType = 0x2;
        afsStatus.LinkCount = scp->linkCount;
        afsStatus.Length = cm_fakeDirSize;
        afsStatus.DataVersion = cm_data.fakeDirVersion;
        afsStatus.Author = 0x1;
        afsStatus.Owner = 0x0;
        afsStatus.CallerAccess = 0x9;
        afsStatus.AnonymousAccess = 0x9;
        afsStatus.UnixModeBits = 0x1ff;
        afsStatus.ParentVnode = 0x1;
        afsStatus.ParentUnique = 0x1;
        afsStatus.ResidencyMask = 0;
        afsStatus.ClientModTime = (afs_uint32)FakeFreelanceModTime;
        afsStatus.ServerModTime = (afs_uint32)FakeFreelanceModTime;
        afsStatus.Group = 0;
        afsStatus.SyncCounter = 0;
        afsStatus.dataVersionHigh = 0;
        afsStatus.lockCount = 0;
        afsStatus.Length_hi = 0;
        afsStatus.errorCode = 0;
	
        // once we're done setting up the status info,
        // we just fill the buffer pages with fakedata
        // from cm_FakeRootDir. Extra pages are set to
        // 0. 
		
        lock_ObtainMutex(&cm_Freelance_Lock);
        t1 = bufp->offset.LowPart;
        qdp = biod.bufListEndp;
        while (qdp) {
            tbufp = osi_GetQData(qdp);
            bufferp=tbufp->datap;
            memset(bufferp, 0, cm_data.buf_blockSize);
            t2 = cm_fakeDirSize - t1;
            if (t2>cm_data.buf_blockSize) t2=cm_data.buf_blockSize;
            if (t2 > 0) {
                memcpy(bufferp, cm_FakeRootDir+t1, t2);
            } else {
                t2 = 0;
            }
            t1+=t2;
            qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);

        }
        lock_ReleaseMutex(&cm_Freelance_Lock);
	
        // once we're done, we skip over the part of the
        // code that does the ACTUAL fetching of data for
        // real files

        goto fetchingcompleted;
    }

#endif /* AFS_FREELANCE_CLIENT */

	/* now make the call */
    do {
        code = cm_Conn(&scp->fid, up, reqp, &connp);
        if (code) 
            continue;

        rxconnp = cm_GetRxConn(connp);
        callp = rx_NewCall(rxconnp);
        rx_PutConnection(rxconnp);

#ifdef AFS_LARGEFILES
        nbytes = nbytes_hi = 0;

        if (SERVERHAS64BIT(connp)) {
            osi_Log4(afsd_logp, "CALL FetchData64 scp 0x%p, off 0x%x:%08x, size 0x%x",
                     scp, biod.offset.HighPart, biod.offset.LowPart, biod.length);

            code = StartRXAFS_FetchData64(callp, &tfid, biod.offset.QuadPart, biod.length);

            if (code == 0) {
                temp = rx_Read(callp, (char *) &nbytes_hi, sizeof(afs_int32));
                if (temp == sizeof(afs_int32)) {
                    nbytes_hi = ntohl(nbytes_hi);
                } else {
                    nbytes_hi = 0;
		    code = callp->error;
                    rx_EndCall(callp, code);
                    callp = NULL;
                }
            }
        }

        if (code == RXGEN_OPCODE || !SERVERHAS64BIT(connp)) {
            if (require_64bit_ops) {
                osi_Log0(afsd_logp, "Skipping FetchData.  Operation requires FetchData64");
                code = CM_ERROR_TOOBIG;
            } else {
                if (!callp) {
                    rxconnp = cm_GetRxConn(connp);
                    callp = rx_NewCall(rxconnp);
                    rx_PutConnection(rxconnp);
                }

                osi_Log3(afsd_logp, "CALL FetchData scp 0x%p, off 0x%x, size 0x%x",
                         scp, biod.offset.LowPart, biod.length);

                code = StartRXAFS_FetchData(callp, &tfid, biod.offset.LowPart,
                                            biod.length);

                SET_SERVERHASNO64BIT(connp);
            }
        }

        if (code == 0) {
            temp  = rx_Read(callp, (char *)&nbytes, sizeof(afs_int32));
            if (temp == sizeof(afs_int32)) {
                nbytes = ntohl(nbytes);
                FillInt64(length_found, nbytes_hi, nbytes);
                if (length_found > biod.length) 
                    code = (callp->error < 0) ? callp->error : -1;
            } else {
                code = (callp->error < 0) ? callp->error : -1;
            }
        }
        /* for the moment, nbytes_hi will always be 0 if code == 0
           because biod.length is a 32-bit quantity. */
#else
        osi_Log3(afsd_logp, "CALL FetchData scp 0x%p, off 0x%x, size 0x%x",
                 scp, biod.offset.LowPart, biod.length);

        code = StartRXAFS_FetchData(callp, &tfid, biod.offset.LowPart,
                                    biod.length);

        /* now copy the data out of the pipe and put it in the buffer */
        if (code == 0) {
            temp  = rx_Read(callp, (char *)&nbytes, sizeof(afs_int32));
            if (temp == sizeof(afs_int32)) {
                nbytes = ntohl(nbytes);
                if (nbytes > biod.length) 
                    code = (callp->error < 0) ? callp->error : -1;
            }
            else 
                code = (callp->error < 0) ? callp->error : -1;
        }
#endif

        if (code == 0) {
            qdp = biod.bufListEndp;
            if (qdp) {
                tbufp = osi_GetQData(qdp);
                bufferp = tbufp->datap;
            }
            else 
                bufferp = NULL;
            /* fill nbytes of data from the pipe into the pages.
             * When we stop, qdp will point at the last page we're
             * dealing with, and bufferp will tell us where we
             * stopped.  We'll need this info below when we clear
             * the remainder of the last page out (and potentially
             * clear later pages out, if we fetch past EOF).
             */
            while (nbytes > 0) {
                /* assert that there are still more buffers;
                 * our check above for nbytes being less than
                 * biod.length should ensure this.
                 */
                osi_assert(bufferp != NULL);

                /* read rbytes of data */
                rbytes = (nbytes > cm_data.buf_blockSize? cm_data.buf_blockSize : nbytes);
                temp = rx_Read(callp, bufferp, rbytes);
                if (temp < rbytes) {
                    code = (callp->error < 0) ? callp->error : -1;
                    break;
                }

                /* allow read-while-fetching.
                 * if this is the last buffer, clear the
                 * PREFETCHING flag, so the reader waiting for
                 * this buffer will start a prefetch.
                 */
                tbufp->cmFlags |= CM_BUF_CMFULLYFETCHED;
                lock_ObtainMutex(&scp->mx);
                if (scp->flags & CM_SCACHEFLAG_WAITING) {
                    osi_Log1(afsd_logp, "CM GetBuffer Waking scp 0x%p", scp);
                    osi_Wakeup((LONG_PTR) &scp->flags);
                }
                if (cpffp && !*cpffp && !osi_QPrev(&qdp->q)) {
                    *cpffp = 1;
                    cm_ClearPrefetchFlag(0, scp, &biod.offset);
                }
                lock_ReleaseMutex(&scp->mx);

                /* and adjust counters */
                nbytes -= temp;

                /* and move to the next buffer */
                if (nbytes != 0) {
                    qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                    if (qdp) {
                        tbufp = osi_GetQData(qdp);
                        bufferp = tbufp->datap;
                    }
                    else 
                        bufferp = NULL;
                } else 
                    bufferp += temp;
            }

            /* zero out remainder of last pages, in case we are
             * fetching past EOF.  We were fetching an integral #
             * of pages, but stopped, potentially in the middle of
             * a page.  Zero the remainder of that page, and then
             * all of the rest of the pages.
             */
            /* bytes fetched */
	    osi_assert((bufferp - tbufp->datap) < LONG_MAX);
            rbytes = (long) (bufferp - tbufp->datap);

            /* bytes left to zero */
            rbytes = cm_data.buf_blockSize - rbytes;
            while(qdp) {
                if (rbytes != 0)
                    memset(bufferp, 0, rbytes);
                qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                if (qdp == NULL) 
                    break;
                tbufp = osi_GetQData(qdp);
                bufferp = tbufp->datap;
                /* bytes to clear in this page */
                rbytes = cm_data.buf_blockSize;
            }   
        }

        if (code == 0) {
            if (SERVERHAS64BIT(connp))
                code = EndRXAFS_FetchData64(callp, &afsStatus, &callback, &volSync);
            else
                code = EndRXAFS_FetchData(callp, &afsStatus, &callback, &volSync);
        } else {
            if (SERVERHAS64BIT(connp))
                osi_Log1(afsd_logp, "CALL EndRXAFS_FetchData64 skipped due to error %d", code);
            else
                osi_Log1(afsd_logp, "CALL EndRXAFS_FetchData skipped due to error %d", code);
        }

        if (callp)
            code = rx_EndCall(callp, code);

        if (code == RXKADUNKNOWNKEY)
            osi_Log0(afsd_logp, "CALL EndCall returns RXKADUNKNOWNKEY");

        osi_Log0(afsd_logp, "CALL FetchData DONE");

    } while (cm_Analyze(connp, up, reqp, &scp->fid, &volSync, NULL, NULL, code));

  fetchingcompleted:
    code = cm_MapRPCError(code, reqp);

    lock_ObtainMutex(&scp->mx);
    
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_FETCHSTATUS);

    /* we know that no one else has changed the buffer, since we still have
     * the fetching flag on the buffers, and we have the scp locked again.
     * Copy in the version # into the buffer if we got code 0 back from the
     * read.
     */
    if (code == 0) {
        for(qdp = biod.bufListp;
             qdp;
             qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            tbufp->dataVersion = afsStatus.DataVersion;

#ifdef DISKCACHE95
            /* write buffer out to disk cache */
            diskcache_Update(tbufp->dcp, tbufp->datap, cm_data.buf_blockSize,
                              tbufp->dataVersion);
#endif /* DISKCACHE95 */
        }
    }

    /* release scatter/gather I/O structure (buffers, locks) */
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseBIOD(&biod, 0);
    lock_ObtainMutex(&scp->mx);

    if (code == 0) 
        cm_MergeStatus(scp, &afsStatus, &volSync, up, 0);
    
    return code;
}
