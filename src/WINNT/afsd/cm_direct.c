/*
 * Copyright (c) 2012 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - Neither the name of Your File System, Inc nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission from
 *   Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <nb30.h>
#include <string.h>
#include <stdlib.h>
#include <osi.h>

#include "afsd.h"

/*
 * cm_DirectWrite is used to write the contents of one contiguous
 * buffer to the file server.  The input buffer must not be a
 * cm_buf_t.data field.  The data is written to the file server without
 * locking any buffers.  The cm_scache object is protected
 * by cm_SyncOp( CM_SCACHESYNC_STOREDATA_EXCL) and the resulting
 * AFSFetchStatus is merged.
 */

static afs_int32
int_DirectWrite( IN cm_scache_t *scp,
                 IN osi_hyper_t *offsetp,
                 IN afs_uint32   length,
                 IN afs_uint32   flags,
                 IN cm_user_t   *userp,
                 IN cm_req_t    *reqp,
                 IN void        *memoryRegionp,
                 OUT afs_uint32 *bytesWritten)
{
    long code, code1;
    long temp;
    AFSFetchStatus outStatus;
    AFSStoreStatus inStatus;
    AFSVolSync volSync;
    AFSFid tfid;
    struct rx_call *rxcallp;
    struct rx_connection *rxconnp;
    cm_conn_t *connp;
    osi_hyper_t truncPos;
    int require_64bit_ops = 0;
    int call_was_64bit = 0;
    int scp_locked = !!(flags & CM_DIRECT_SCP_LOCKED);
    afs_uint32 written = 0;

    osi_assertx(userp != NULL, "null cm_user_t");

    memset(&volSync, 0, sizeof(volSync));
    if (bytesWritten)
        *bytesWritten = 0;

    cm_AFSFidFromFid(&tfid, &scp->fid);

    if (!scp_locked)
        lock_ObtainWrite(&scp->rw);

    /* prepare the output status for the store */
    _InterlockedOr(&scp->mask, CM_SCACHEMASK_CLIENTMODTIME);
    cm_StatusFromAttr(&inStatus, scp, NULL);
    truncPos = scp->length;
    if ((scp->mask & CM_SCACHEMASK_TRUNCPOS)
         && LargeIntegerLessThan(scp->truncPos, truncPos)) {
        truncPos = scp->truncPos;
        _InterlockedAnd(&scp->mask, ~CM_SCACHEMASK_TRUNCPOS);
    }

    InterlockedIncrement(&scp->activeRPCs);
    lock_ReleaseWrite(&scp->rw);

    /* now we're ready to do the store operation */
    do {
        code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
        if (code)
            continue;

    retry:
        rxconnp = cm_GetRxConn(connp);
        rxcallp = rx_NewCall(rxconnp);
        rx_PutConnection(rxconnp);

        if (SERVERHAS64BIT(connp)) {
            call_was_64bit = 1;

            osi_Log4(afsd_logp, "CALL StartRXAFS_StoreData64 scp 0x%p, offset 0x%x:%08x, length 0x%x",
                     scp, offsetp->HighPart, offsetp->LowPart, length);
            osi_Log2(afsd_logp, "... truncPos 0x%x:%08x",  truncPos.HighPart, truncPos.LowPart);

            code = StartRXAFS_StoreData64(rxcallp, &tfid, &inStatus,
                                          offsetp->QuadPart,
                                          length,
                                          truncPos.QuadPart);
	    if (code)
		osi_Log1(afsd_logp, "CALL StartRXAFS_StoreData64 FAILURE, code 0x%x", code);
	    else
		osi_Log0(afsd_logp, "CALL StartRXAFS_StoreData64 SUCCESS");
        } else {
            call_was_64bit = 0;

            if (require_64bit_ops) {
                osi_Log0(afsd_logp, "Skipping StartRXAFS_StoreData.  The operation requires large file support in the server.");
                code = CM_ERROR_TOOBIG;
            } else {
                osi_Log4(afsd_logp, "CALL StartRXAFS_StoreData scp 0x%p, offset 0x%x:%08x, length 0x%x",
                         scp, offsetp->HighPart, offsetp->LowPart, length);
                osi_Log1(afsd_logp, "... truncPos 0x%08x",  truncPos.LowPart);

                code = StartRXAFS_StoreData(rxcallp, &tfid, &inStatus,
                                            offsetp->LowPart, length, truncPos.LowPart);
		if (code)
		    osi_Log1(afsd_logp, "CALL StartRXAFS_StoreData FAILURE, code 0x%x", code);
		else
		    osi_Log0(afsd_logp, "CALL StartRXAFS_StoreData SUCCESS");
            }
        }

        /* Write the data */
        if (code == 0) {
            temp = rx_Write(rxcallp, memoryRegionp, length);
            if (temp != length) {
                osi_Log2(afsd_logp, "rx_Write failed %d != %d", temp, length);
                code = (rx_Error(rxcallp) < 0) ? rx_Error(rxcallp) : RX_PROTOCOL_ERROR;
                break;
            } else {
                osi_Log1(afsd_logp, "rx_Write succeeded written %d", temp);
                written += temp;
            }
        }

        /* End the call */
        if (code == 0) {
            if (call_was_64bit) {
                code = EndRXAFS_StoreData64(rxcallp, &outStatus, &volSync);
                if (code)
                    osi_Log2(afsd_logp, "EndRXAFS_StoreData64 FAILURE scp 0x%p code %lX", scp, code);
		else
		    osi_Log0(afsd_logp, "EndRXAFS_StoreData64 SUCCESS");
            } else {
                code = EndRXAFS_StoreData(rxcallp, &outStatus, &volSync);
                if (code)
                    osi_Log2(afsd_logp, "EndRXAFS_StoreData FAILURE scp 0x%p code %lX",scp,code);
		else
		    osi_Log0(afsd_logp, "EndRXAFS_StoreData SUCCESS");
            }
        }

        code1 = rx_EndCall(rxcallp, code);

        if ((code == RXGEN_OPCODE || code1 == RXGEN_OPCODE) && SERVERHAS64BIT(connp)) {
            SET_SERVERHASNO64BIT(connp);
            goto retry;
        }

        /* Prefer StoreData error over rx_EndCall error */
        if (code1 != 0)
            code = code1;
    } while (cm_Analyze(connp, userp, reqp, &scp->fid, NULL, 1, &outStatus, &volSync, NULL, NULL, code));

    code = cm_MapRPCError(code, reqp);

    if (code)
        osi_Log2(afsd_logp, "CALL StoreData FAILURE scp 0x%p, code 0x%x", scp, code);
    else
        osi_Log1(afsd_logp, "CALL StoreData SUCCESS scp 0x%p", scp);

    /* now, clean up our state */
    lock_ObtainWrite(&scp->rw);

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

        if (call_was_64bit) {
            t.LowPart = outStatus.Length;
            t.HighPart = outStatus.Length_hi;
        } else {
            t = ConvertLongToLargeInteger(outStatus.Length);
        }

        if (LargeIntegerGreaterThanOrEqualTo(t, scp->length))
            _InterlockedAnd(&scp->mask, ~CM_SCACHEMASK_LENGTH);

        cm_MergeStatus(NULL, scp, &outStatus, &volSync, userp, reqp,
                       CM_MERGEFLAG_STOREDATA | CM_MERGEFLAG_CACHE_BYPASS);
    } else {
        InterlockedDecrement(&scp->activeRPCs);
        if (code == CM_ERROR_SPACE)
            _InterlockedOr(&scp->flags, CM_SCACHEFLAG_OUTOFSPACE);
        else if (code == CM_ERROR_QUOTA)
            _InterlockedOr(&scp->flags, CM_SCACHEFLAG_OVERQUOTA);
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);

    if (bytesWritten)
        *bytesWritten = written;

    if (!scp_locked)
        lock_ReleaseWrite(&scp->rw);

    return code;
}

afs_int32
cm_DirectWrite( IN cm_scache_t *scp,
                IN osi_hyper_t *offsetp,
                IN afs_uint32   length,
                IN afs_uint32   flags,
                IN cm_user_t   *userp,
                IN cm_req_t    *reqp,
                IN void        *memoryRegionp,
                OUT afs_uint32 *bytesWritten)
{
    rock_BkgDirectWrite_t *rockp = NULL;
    int scp_locked = !!(flags & CM_DIRECT_SCP_LOCKED);
    afs_int32 code;

    if (!scp_locked)
        lock_ObtainWrite(&scp->rw);

    if (scp->flags & CM_SCACHEFLAG_DELETED) {
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
        return CM_ERROR_BADFD;
    }

    rockp = malloc(sizeof(*rockp));
    if (!rockp) {
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
        return ENOMEM;
    }

    rockp->memoryRegion = malloc(length);
    if (rockp->memoryRegion == NULL) {
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
        free(rockp);
        return ENOMEM;
    }

    /* Serialize StoreData RPC's; for rationale see cm_scache.c */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA_EXCL | CM_SCACHESYNC_ASYNCSTORE);
    if (code) {
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
        free(rockp->memoryRegion);
        free(rockp);
        return ENOMEM;
    }

    /* cannot hold scp->rw when calling cm_QueueBkGRequest. */
    lock_ReleaseWrite(&scp->rw);
    memcpy(rockp->memoryRegion, memoryRegionp, length);
    rockp->offset = *offsetp;
    rockp->length = length;
    rockp->bypass_cache = TRUE;

    cm_QueueBKGRequest(scp, cm_BkgDirectWrite, rockp, userp, reqp);

    *bytesWritten = length;     /* must lie */
    if (scp_locked)
        lock_ObtainWrite(&scp->rw);

    return code;
}

void
cm_BkgDirectWriteDone( cm_scache_t *scp, void *vrockp, afs_int32 code)
{
    rock_BkgDirectWrite_t *rockp = ((rock_BkgDirectWrite_t *)vrockp);

    lock_ObtainWrite(&scp->rw);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL | CM_SCACHESYNC_ASYNCSTORE);
    lock_ReleaseWrite(&scp->rw);
    free(rockp->memoryRegion);
    rockp->memoryRegion = NULL;
}

afs_int32
cm_BkgDirectWrite( cm_scache_t *scp, void *vrockp, struct cm_user *userp, cm_req_t *reqp)
{
    rock_BkgDirectWrite_t *rockp = ((rock_BkgDirectWrite_t *)vrockp);
    afs_uint32 flags = 0;
    afs_uint32 bytesWritten;
    afs_int32  code;

    osi_assertx(rockp->memoryRegion, "memoryRegion is NULL");

    code = int_DirectWrite(scp, &rockp->offset, rockp->length,
                           flags, userp, reqp,
                           rockp->memoryRegion, &bytesWritten);

    switch ( code ) {
    case CM_ERROR_TIMEDOUT:	/* or server restarting */
    case CM_ERROR_RETRY:
    case CM_ERROR_WOULDBLOCK:
    case CM_ERROR_ALLBUSY:
    case CM_ERROR_ALLDOWN:
    case CM_ERROR_ALLOFFLINE:
    case CM_ERROR_PARTIALWRITE:
        /* do nothing; cm_BkgDaemon will retry the request */
        break;
    default:
        lock_ObtainWrite(&scp->rw);
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_ASYNCSTORE);
        lock_ReleaseWrite(&scp->rw);
        free(rockp->memoryRegion);
        rockp->memoryRegion = NULL;
        break;
    }
    return code;
}

/*
 * cm_SetupDirectStoreBIOD differs from cm_SetupStoreBIOD in that it
 * doesn't worry about whether or not the cm_buf_t is dirty or not.  Nor
 * does it concern itself with chunk size.  All of the cm_buf_t objects
 * that overlap the requested range must be held.
 *
 * scp must be locked; temporarily unlocked during processing.
 * If returns 0, returns buffers held in biop, and with
 * CM_BUF_CMSTORING set.
 *
 * Caller *must* set CM_BUF_WRITING and reset the over.hEvent field if the
 * buffer is ever unlocked before CM_BUF_DIRTY is cleared.  And if
 * CM_BUF_WRITING is ever viewed by anyone, then it must be cleared, sleepers
 * must be woken, and the event must be set when the I/O is done.  All of this
 * is required so that buf_WaitIO synchronizes properly with the buffer as it
 * is being written out.
 *
 * Not currently used but want to make sure the code does not rot.
 */
afs_int32
cm_SetupDirectStoreBIOD(cm_scache_t *scp, osi_hyper_t *inOffsetp, afs_uint32 inSize,
                        cm_bulkIO_t *biop, cm_user_t *userp, cm_req_t *reqp)
{
    cm_buf_t *bufp;
    osi_queueData_t *qdp;
    osi_hyper_t thyper;
    osi_hyper_t tbase;
    osi_hyper_t scanStart;	/* where to start scan for dirty pages */
    osi_hyper_t scanEnd;	/* where to stop scan for dirty pages */
    long code;
    long flags;			/* flags to cm_SyncOp */

    /* clear things out */
    biop->scp = scp;		/* do not hold; held by caller */
    biop->userp = userp;        /* do not hold; held by caller */
    biop->reqp = reqp;
    biop->offset = *inOffsetp;
    biop->length = 0;
    biop->bufListp = NULL;
    biop->bufListEndp = NULL;
    biop->reserved = 0;

    /*
     * reserve enough buffers to cover the full range.
     * drop the cm_scache.rw lock because buf_ReserveBuffers()
     * can sleep if there is insufficient room.
     */
    lock_ReleaseWrite(&scp->rw);
    biop->reserved = 1 + inSize / cm_data.buf_blockSize;
    buf_ReserveBuffers(biop->reserved);

    /*
     * This pass is intended to ensure that a cm_buf_t object
     * is allocated for each block of the direct store operation.
     * No effort is going to be made to ensure that the blocks are
     * populated with current data.  Blocks that are not current and
     * are not fully overwritten by the direct store data will not
     * be cached.
     */

    lock_ObtainWrite(&scp->bufCreateLock);

    /*
     * Compute the offset of the first buffer.
     */
    tbase = *inOffsetp;
    tbase.LowPart -= tbase.LowPart % cm_data.buf_blockSize;

    /*
     * If the first buffer cannot be obtained, return an error
     * immediately.  There is no clean up to be performed.
     */
    code = buf_Get(scp, &tbase, reqp, BUF_GET_FLAG_BUFCREATE_LOCKED, &bufp);
    if (code) {
        lock_ReleaseRead(&scp->bufCreateLock);
        buf_UnreserveBuffers(biop->reserved);
        lock_ObtainWrite(&scp->rw);
        return code;
    }

    /* get buffer mutex and scp mutex safely */
    lock_ObtainMutex(&bufp->mx);

    /*
     * if the buffer is actively involved in I/O
     * we wait for the I/O to complete.
     */
    if (bufp->flags & (CM_BUF_WRITING|CM_BUF_READING))
        buf_WaitIO(scp, bufp);

    lock_ObtainWrite(&scp->rw);
    flags = CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS |
            CM_SCACHESYNC_STOREDATA | CM_SCACHESYNC_BUFLOCKED;
    code = cm_SyncOp(scp, bufp, userp, reqp, 0, flags);
    if (code) {
        lock_ReleaseMutex(&bufp->mx);
        buf_Release(bufp);
        buf_UnreserveBuffers(biop->reserved);
        return code;
    }
    cm_SyncOpDone(scp, bufp, flags);
    lock_ReleaseMutex(&bufp->mx);

    /*
     * Add the first buffer into the BIOD list.
     */
    qdp = osi_QDAlloc();
    if (qdp == NULL) {
        buf_Release(bufp);
        buf_UnreserveBuffers(1 + inSize / cm_data.buf_blockSize);
        return ENOMEM;
    }
    osi_SetQData(qdp, bufp);

    if ( cm_verifyData )
        buf_ComputeCheckSum(bufp);

    /* don't have to hold bufp, since held by buf_Get above */
    osi_QAddH((osi_queue_t **) &biop->bufListp,
              (osi_queue_t **) &biop->bufListEndp,
              &qdp->q);
    biop->length = cm_data.buf_blockSize - (afs_uint32)(inOffsetp->QuadPart % cm_data.buf_blockSize);

    if (biop->length < inSize) {
        /* scan for the rest of the buffers */
        thyper = ConvertLongToLargeInteger(biop->length);
        scanStart = LargeIntegerAdd(bufp->offset, thyper);
        thyper = ConvertLongToLargeInteger(inSize);
        scanEnd = LargeIntegerAdd(*inOffsetp, thyper);

        flags = CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_STOREDATA | CM_SCACHESYNC_BUFLOCKED;
        lock_ReleaseWrite(&scp->rw);

        for ( tbase = scanStart, thyper = ConvertLongToLargeInteger(cm_data.buf_blockSize);
              LargeIntegerLessThan(tbase, scanEnd);
              tbase = LargeIntegerAdd(tbase, thyper))
        {
            code = buf_Get(scp, &tbase, reqp, BUF_GET_FLAG_BUFCREATE_LOCKED, &bufp);
            if (code) {
                /* Must tear down biod */
                goto error;
            }

            lock_ObtainMutex(&bufp->mx);
            /*
            * if the buffer is actively involved in I/O
            * we wait for the I/O to complete.
            */
            if (bufp->flags & (CM_BUF_WRITING|CM_BUF_READING))
                buf_WaitIO(scp, bufp);

            lock_ObtainWrite(&scp->rw);
            code = cm_SyncOp(scp, bufp, userp, reqp, 0, flags);
            lock_ReleaseWrite(&scp->rw);
            lock_ReleaseMutex(&bufp->mx);
            if (code) {
                buf_Release(bufp);
                goto error;
            }

            /*
             * Add the buffer into the BIOD list.
             */
            qdp = osi_QDAlloc();
            if (qdp == NULL) {
                buf_Release(bufp);
                code = ENOMEM;
                goto error;
            }
            osi_SetQData(qdp, bufp);

            if ( cm_verifyData )
                buf_ComputeCheckSum(bufp);

            /* don't have to hold bufp, since held by buf_Get above */
            osi_QAddH( (osi_queue_t **) &biop->bufListp,
                       (osi_queue_t **) &biop->bufListEndp,
                       &qdp->q);
            biop->length += cm_data.buf_blockSize;
            bufp = NULL;	/* this buffer and reference added to the queue */
        }

        /* update biod info describing the transfer */
        if (biop->length > inSize)
            biop->length = inSize;

        lock_ObtainWrite(&scp->rw);
    }

    /* finally, we're done */
    lock_ReleaseWrite(&scp->bufCreateLock);
    return 0;

  error:
    lock_ReleaseWrite(&scp->bufCreateLock);
    /* tear down biod and clear buffer reservation */
    cm_ReleaseBIOD(biop, TRUE, code, FALSE);
    lock_ObtainWrite(&scp->rw);
    return code;
}
