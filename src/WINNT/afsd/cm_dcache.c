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

#include <windows.h>
#include <winsock2.h>
#include <nb30.h>
#include <string.h>
#include <stdlib.h>
#include <osi.h>

#include "afsd.h"

#ifdef DEBUG
extern void afsi_log(char *pattern, ...);
#endif

#ifdef AFS_FREELANCE_CLIENT
extern osi_mutex_t cm_Freelance_Lock;
#endif

#define USE_RX_IOVEC 1

/* we can access connp->serverp without holding a lock because that
   never changes since the connection is made. */
#define SERVERHAS64BIT(connp) (!((connp)->serverp->flags & CM_SERVERFLAG_NO64BIT))
#define SET_SERVERHASNO64BIT(connp) (cm_SetServerNo64Bit((connp)->serverp, TRUE))

/* functions called back from the buffer package when reading or writing data,
 * or when holding or releasing a vnode pointer.
 */
long cm_BufWrite(void *vscp, osi_hyper_t *offsetp, long length, long flags,
                 cm_user_t *userp, cm_req_t *reqp)
{
    /*
     * store the data back from this buffer; the buffer is locked and held,
     * but the vnode involved may or may not be locked depending on whether
     * or not the CM_BUF_WRITE_SCP_LOCKED flag is set.
     */
    long code, code1;
    cm_scache_t *scp = vscp;
    afs_int32 nbytes;
    afs_int32 save_nbytes;
    long temp;
    AFSFetchStatus outStatus;
    AFSStoreStatus inStatus;
    osi_hyper_t thyper;
    AFSVolSync volSync;
    AFSFid tfid;
    struct rx_call *rxcallp;
    struct rx_connection *rxconnp;
    osi_queueData_t *qdp;
    cm_buf_t *bufp;
    afs_uint32 wbytes;
    char *bufferp;
    cm_conn_t *connp;
    osi_hyper_t truncPos;
    cm_bulkIO_t biod;		/* bulk IO descriptor */
    int require_64bit_ops = 0;
    int call_was_64bit = 0;
    int scp_locked = flags & CM_BUF_WRITE_SCP_LOCKED;

    osi_assertx(userp != NULL, "null cm_user_t");
    osi_assertx(scp != NULL, "null cm_scache_t");

    memset(&volSync, 0, sizeof(volSync));

    /*
     * now, the buffer may or may not be filled with good data (buf_GetNewLocked
     * drops lots of locks, and may indeed return a properly initialized
     * buffer, although more likely it will just return a new, empty, buffer.
     */
    if (!scp_locked)
        lock_ObtainWrite(&scp->rw);
    if (scp->flags & CM_SCACHEFLAG_DELETED) {
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
	return CM_ERROR_NOSUCHFILE;
    }

    cm_AFSFidFromFid(&tfid, &scp->fid);

    /* Serialize StoreData RPC's; for rationale see cm_scache.c */
    (void) cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_STOREDATA_EXCL);

    code = cm_SetupStoreBIOD(scp, offsetp, length, &biod, userp, reqp);
    if (code) {
        osi_Log1(afsd_logp, "cm_SetupStoreBIOD code %x", code);
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
        return code;
    }

    if (biod.length == 0) {
        osi_Log0(afsd_logp, "cm_SetupStoreBIOD length 0");
        cm_ReleaseBIOD(&biod, 1, 0, 1);	/* should be a NOOP */
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
        return 0;
    }

    /* prepare the output status for the store */
    _InterlockedOr(&scp->mask, CM_SCACHEMASK_CLIENTMODTIME);
    cm_StatusFromAttr(&inStatus, scp, NULL);
    truncPos = scp->length;
    if ((scp->mask & CM_SCACHEMASK_TRUNCPOS)
         && LargeIntegerLessThan(scp->truncPos, truncPos)) {
        truncPos = scp->truncPos;
        _InterlockedAnd(&scp->mask, ~CM_SCACHEMASK_TRUNCPOS);
    }

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

    InterlockedIncrement(&scp->activeRPCs);
    lock_ReleaseWrite(&scp->rw);

    /* now we're ready to do the store operation */
    save_nbytes = nbytes;
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
                     scp, biod.offset.HighPart, biod.offset.LowPart, nbytes);
            osi_Log2(afsd_logp, "... truncPos 0x%x:%08x",  truncPos.HighPart, truncPos.LowPart);

            code = StartRXAFS_StoreData64(rxcallp, &tfid, &inStatus,
                                          biod.offset.QuadPart,
                                          nbytes,
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
                         scp, biod.offset.HighPart, biod.offset.LowPart, nbytes);

                code = StartRXAFS_StoreData(rxcallp, &tfid, &inStatus,
                                            biod.offset.LowPart, nbytes, truncPos.LowPart);
		if (code)
		    osi_Log1(afsd_logp, "CALL StartRXAFS_StoreData FAILURE, code 0x%x", code);
		else
		    osi_Log0(afsd_logp, "CALL StartRXAFS_StoreData SUCCESS");
            }
        }

        if (code == 0) {
            afs_uint32 buf_offset = 0, bytes_copied = 0;

            /* write the data from the the list of buffers */
            qdp = NULL;
            while(nbytes > 0) {
#ifdef USE_RX_IOVEC
                struct iovec tiov[RX_MAXIOVECS];
                afs_int32 tnio, vlen, vbytes, iov, voffset;
                afs_uint32 vleft;

                vbytes = rx_WritevAlloc(rxcallp, tiov, &tnio, RX_MAXIOVECS, nbytes);
                if (vbytes <= 0) {
                    code = RX_PROTOCOL_ERROR;
                    break;
                }

                for ( iov = voffset = vlen = 0;
                      vlen < vbytes && iov < tnio; vlen += wbytes) {
                    if (qdp == NULL) {
                        qdp = biod.bufListEndp;
                        buf_offset = offsetp->LowPart % cm_data.buf_blockSize;
                    } else if (buf_offset == cm_data.buf_blockSize) {
                        qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                        buf_offset = 0;
                    }

                    osi_assertx(qdp != NULL, "null osi_queueData_t");
                    bufp = osi_GetQData(qdp);
                    bufferp = bufp->datap + buf_offset;
                    wbytes = vbytes - vlen;
                    if (wbytes > cm_data.buf_blockSize - buf_offset)
                        wbytes = cm_data.buf_blockSize - buf_offset;

                    vleft = tiov[iov].iov_len - voffset;
                    while (wbytes > vleft && iov < tnio) {
                        memcpy(tiov[iov].iov_base + voffset, bufferp, vleft);
                        bytes_copied += vleft;
                        vlen += vleft;
                        wbytes -= vleft;
                        bufferp += vleft;
                        buf_offset += vleft;

                        iov++;
                        voffset = 0;
                        vleft = tiov[iov].iov_len;
                    }

                    if (iov < tnio) {
                        memcpy(tiov[iov].iov_base + voffset, bufferp, wbytes);
                        bytes_copied += wbytes;
                        if (tiov[iov].iov_len == voffset + wbytes) {
                            iov++;
                            voffset = 0;
                            vleft = (iov < tnio) ? tiov[iov].iov_len : 0;
                        } else {
                            voffset += wbytes;
                            vleft -= wbytes;
                        }
                        bufferp += wbytes;
                        buf_offset += wbytes;
                    } else {
                        voffset = vleft = 0;
                    }
                }

                osi_assertx(iov == tnio, "incorrect iov count");
                osi_assertx(vlen == vbytes, "bytes_copied != vbytes");
                osi_assertx(bufp->offset.QuadPart + buf_offset == biod.offset.QuadPart + bytes_copied,
                            "begin and end offsets don't match");

                temp = rx_Writev(rxcallp, tiov, tnio, vbytes);
                if (temp != vbytes) {
                    osi_Log3(afsd_logp, "rx_Writev failed bp 0x%p, %d != %d", bufp, temp, vbytes);
                    code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
                    break;
                }

                osi_Log2(afsd_logp, "rx_Writev succeeded iov offset 0x%x, wrote 0x%x",
                         (unsigned long)(bufp->offset.QuadPart + buf_offset - vbytes), vbytes);
                nbytes -= vbytes;
#else /* USE_RX_IOVEC */
                if (qdp == NULL) {
                    qdp = biod.bufListEndp;
                    buf_offset = offsetp->LowPart % cm_data.buf_blockSize;
                } else {
                    qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                    buf_offset = 0;
                }

                osi_assertx(qdp != NULL, "null osi_queueData_t");
                bufp = osi_GetQData(qdp);
                bufferp = bufp->datap + buf_offset;
                wbytes = nbytes;
                if (wbytes > cm_data.buf_blockSize - buf_offset)
                    wbytes = cm_data.buf_blockSize - buf_offset;

                /* write out wbytes of data from bufferp */
                temp = rx_Write(rxcallp, bufferp, wbytes);
                if (temp != wbytes) {
                    osi_Log3(afsd_logp, "rx_Write failed bp 0x%p, %d != %d", bufp, temp, wbytes);
                    code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
                    break;
                } else {
                    osi_Log2(afsd_logp, "rx_Write succeeded bp 0x%p written %d", bufp, temp);
                }
                nbytes -= wbytes;
#endif /* USE_RX_IOVEC */
            }	/* while more bytes to write */
        }	/* if RPC started successfully */

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
            qdp = NULL;
            nbytes = save_nbytes;
            goto retry;
        }

        /* Prefer StoreData error over rx_EndCall error */
        if (code1 != 0)
            code = code1;
    } while (cm_Analyze(connp, userp, reqp, &scp->fid, 1, &volSync, NULL, NULL, code));

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

        cm_MergeStatus(NULL, scp, &outStatus, &volSync, userp, reqp, CM_MERGEFLAG_STOREDATA);
    } else {
        InterlockedDecrement(&scp->activeRPCs);
        if (code == CM_ERROR_SPACE)
            _InterlockedOr(&scp->flags, CM_SCACHEFLAG_OUTOFSPACE);
        else if (code == CM_ERROR_QUOTA)
            _InterlockedOr(&scp->flags, CM_SCACHEFLAG_OVERQUOTA);
    }

    cm_ReleaseBIOD(&biod, 1, code, 1);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);

    if (!scp_locked)
        lock_ReleaseWrite(&scp->rw);

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
    long code, code1;
    osi_hyper_t truncPos;
    cm_conn_t *connp;
    struct rx_call *rxcallp;
    struct rx_connection *rxconnp;
    int require_64bit_ops = 0;
    int call_was_64bit = 0;

    memset(&volSync, 0, sizeof(volSync));
    memset(&inStatus, 0, sizeof(inStatus));

    osi_Log2(afsd_logp, "cm_StoreMini scp 0x%p userp 0x%p", scp, userp);

    /* Serialize StoreData RPC's; for rationale see cm_scache.c */
    (void) cm_SyncOp(scp, NULL, userp, reqp, 0,
                     CM_SCACHESYNC_STOREDATA_EXCL);

    /* prepare the output status for the store */
    inStatus.Mask = AFS_SETMODTIME;
    inStatus.ClientModTime = scp->clientModTime;
    _InterlockedAnd(&scp->mask, ~CM_SCACHEMASK_CLIENTMODTIME);

    /* calculate truncation position */
    truncPos = scp->length;
    if ((scp->mask & CM_SCACHEMASK_TRUNCPOS)
        && LargeIntegerLessThan(scp->truncPos, truncPos))
        truncPos = scp->truncPos;
    _InterlockedAnd(&scp->mask, ~CM_SCACHEMASK_TRUNCPOS);

    if (LargeIntegerGreaterThan(truncPos,
                                ConvertLongToLargeInteger(LONG_MAX))) {

        require_64bit_ops = 1;
    }

    InterlockedIncrement(&scp->activeRPCs);
    lock_ReleaseWrite(&scp->rw);

    cm_AFSFidFromFid(&tfid, &scp->fid);

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

            osi_Log3(afsd_logp, "CALL StartRXAFS_StoreData64 scp 0x%p, truncPos 0x%x:%08x",
                      scp, truncPos.HighPart, truncPos.LowPart);

            code = StartRXAFS_StoreData64(rxcallp, &tfid, &inStatus,
                                          0, 0, truncPos.QuadPart);
	    if (code)
		osi_Log1(afsd_logp, "CALL StartRXAFS_StoreData64 FAILURE, code 0x%x", code);
	    else
		osi_Log0(afsd_logp, "CALL StartRXAFS_StoreData64 SUCCESS");
        } else {
            call_was_64bit = 0;

            if (require_64bit_ops) {
                code = CM_ERROR_TOOBIG;
            } else {
                osi_Log3(afsd_logp, "CALL StartRXAFS_StoreData scp 0x%p, truncPos 0x%x:%08x",
                          scp, truncPos.HighPart, truncPos.LowPart);

                code = StartRXAFS_StoreData(rxcallp, &tfid, &inStatus,
                                            0, 0, truncPos.LowPart);
                if (code)
                    osi_Log1(afsd_logp, "CALL StartRXAFS_StoreData FAILURE, code 0x%x", code);
                else
                    osi_Log0(afsd_logp, "CALL StartRXAFS_StoreData SUCCESS");
            }
        }

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
                    osi_Log2(afsd_logp, "EndRXAFS_StoreData FAILURE scp 0x%p code %lX", scp, code);
		else
		    osi_Log0(afsd_logp, "EndRXAFS_StoreData SUCCESS");
            }
        }
        code1 = rx_EndCall(rxcallp, code);

        if ((code == RXGEN_OPCODE || code1 == RXGEN_OPCODE) && SERVERHAS64BIT(connp)) {
            SET_SERVERHASNO64BIT(connp);
            goto retry;
        }

        /* prefer StoreData error over rx_EndCall error */
        if (code == 0 && code1 != 0)
            code = code1;
    } while (cm_Analyze(connp, userp, reqp, &scp->fid, 1, &volSync, NULL, NULL, code));
    code = cm_MapRPCError(code, reqp);

    /* now, clean up our state */
    lock_ObtainWrite(&scp->rw);

    if (code == 0) {
        osi_hyper_t t;
        /*
         * For explanation of handling of CM_SCACHEMASK_LENGTH,
         * see cm_BufWrite().
         */
        if (call_was_64bit) {
            t.HighPart = outStatus.Length_hi;
            t.LowPart = outStatus.Length;
        } else {
            t = ConvertLongToLargeInteger(outStatus.Length);
        }

        if (LargeIntegerGreaterThanOrEqualTo(t, scp->length))
            _InterlockedAnd(&scp->mask, ~CM_SCACHEMASK_LENGTH);
        cm_MergeStatus(NULL, scp, &outStatus, &volSync, userp, reqp, CM_MERGEFLAG_STOREDATA);
    } else {
        InterlockedDecrement(&scp->activeRPCs);
    }
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_STOREDATA_EXCL);

    return code;
}

long cm_BufRead(cm_buf_t *bufp, long nbytes, long *bytesReadp, cm_user_t *userp)
{
    *bytesReadp = 0;

    /* now return a code that means that I/O is done */
    return 0;
}

/*
 * stabilize scache entry with CM_SCACHESYNC_SETSIZE.  This prevents any new
 * data buffers to be allocated, new data to be fetched from the file server,
 * and writes to be accepted from the application but permits dirty buffers
 * to be written to the file server.
 *
 * Stabilize uses cm_SyncOp to maintain the cm_scache_t in this stable state
 * instead of holding the rwlock exclusively.  This permits background stores
 * to be performed in parallel and in particular allow FlushFile to be
 * implemented without violating the locking hierarchy.
 */
long cm_BufStabilize(void *vscp, cm_user_t *userp, cm_req_t *reqp)
{
    cm_scache_t *scp = vscp;
    long code;

    lock_ObtainWrite(&scp->rw);
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_SETSIZE);
    lock_ReleaseWrite(&scp->rw);

    return code;
}

/* undoes the work that cm_BufStabilize does: releases lock so things can change again */
long cm_BufUnstabilize(void *vscp, cm_user_t *userp)
{
    cm_scache_t *scp = vscp;

    lock_ObtainWrite(&scp->rw);
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_SETSIZE);

    lock_ReleaseWrite(&scp->rw);

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
    if (bufp->dataVersion <= scp->dataVersion && bufp->dataVersion >= scp->bufDataVersionLow)
        return 1;
    if (bufp->offset.QuadPart >= scp->serverLength.QuadPart)
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

/*
 * used when deciding whether to do a background fetch or not.
 * call with scp->rw write-locked.
 */
afs_int32
cm_CheckFetchRange(cm_scache_t *scp, osi_hyper_t *startBasep, osi_hyper_t *length,
                        cm_user_t *userp, cm_req_t *reqp, osi_hyper_t *realBasep)
{
    osi_hyper_t tbase;
    osi_hyper_t tlength;
    osi_hyper_t tblocksize;
    long code;
    cm_buf_t *bp;
    int stop;

    /* now scan all buffers in the range, looking for any that look like
     * they need work.
     */
    tbase = *startBasep;
    tlength = *length;
    tblocksize = ConvertLongToLargeInteger(cm_data.buf_blockSize);
    stop = 0;
    while (LargeIntegerGreaterThanZero(tlength)) {
        /* get callback so we can do a meaningful dataVersion comparison */
        code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                         CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (code)
            return code;

        if (LargeIntegerGreaterThanOrEqualTo(tbase, scp->length)) {
            /* we're past the end of file */
            break;
        }

        bp = buf_Find(&scp->fid, &tbase);
        /* We cheat slightly by not locking the bp mutex. */
        if (bp) {
            if ((bp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING)) == 0
                 && (bp->dataVersion < scp->bufDataVersionLow || bp->dataVersion > scp->dataVersion))
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

        tbase = LargeIntegerAdd(tbase, tblocksize);
        tlength = LargeIntegerSubtract(tlength,  tblocksize);
    }

    /* if we get here, either everything is fine or 'stop' stopped us at a
     * particular buffer in the range that definitely needs to be fetched.
     */
    if (stop == 0) {
        /* return non-zero code since realBasep won't be valid */
        code = -1;
    }
    else {
        /* successfully found a page that will need fetching */
        *realBasep = tbase;
        code = 0;
    }
    return code;
}

afs_int32
cm_BkgStore(cm_scache_t *scp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4,
	    cm_user_t *userp, cm_req_t *reqp)
{
    osi_hyper_t toffset;
    long length;
    long code = 0;

    if (scp->flags & CM_SCACHEFLAG_DELETED) {
	osi_Log4(afsd_logp, "Skipping BKG store - Deleted scp 0x%p, offset 0x%x:%08x, length 0x%x", scp, p2, p1, p3);
    } else {
	/* Retries will be performed by the BkgDaemon thread if appropriate */
        afs_uint32 req_flags = reqp->flags;
	reqp->flags |= CM_REQ_NORETRY;

	toffset.LowPart = p1;
	toffset.HighPart = p2;
	length = p3;

	osi_Log4(afsd_logp, "Starting BKG store scp 0x%p, offset 0x%x:%08x, length 0x%x", scp, p2, p1, p3);

	code = cm_BufWrite(scp, &toffset, length, /* flags */ 0, userp, reqp);

	osi_Log4(afsd_logp, "Finished BKG store scp 0x%p, offset 0x%x:%08x, code 0x%x", scp, p2, p1, code);

        reqp->flags = req_flags;
    }

    /*
     * Keep the following list synchronized with the
     * error code list in cm_BkgDaemon
     */
    switch ( code ) {
    case CM_ERROR_TIMEDOUT: /* or server restarting */
    case CM_ERROR_RETRY:
    case CM_ERROR_WOULDBLOCK:
    case CM_ERROR_ALLBUSY:
    case CM_ERROR_ALLDOWN:
    case CM_ERROR_ALLOFFLINE:
    case CM_ERROR_PARTIALWRITE:
        break;  /* cm_BkgDaemon will re-insert the request in the queue */
    case 0:
    default:
        lock_ObtainWrite(&scp->rw);
        cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_ASYNCSTORE);
        lock_ReleaseWrite(&scp->rw);
    }

    return code;
}

/* Called with scp locked */
void cm_ClearPrefetchFlag(long code, cm_scache_t *scp, osi_hyper_t *base, osi_hyper_t *length)
{
    osi_hyper_t end;

    if (code == 0) {
        end =  LargeIntegerAdd(*base, *length);
        if (LargeIntegerGreaterThan(*base, scp->prefetch.base))
            scp->prefetch.base = *base;
        if (LargeIntegerGreaterThan(end, scp->prefetch.end))
            scp->prefetch.end = end;
    }
    _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_PREFETCHING);
}

/* do the prefetch.  if the prefetch fails, return 0 (success)
 * because there is no harm done.  */
afs_int32
cm_BkgPrefetch(cm_scache_t *scp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4,
	       cm_user_t *userp, cm_req_t *reqp)
{
    osi_hyper_t length;
    osi_hyper_t base;
    osi_hyper_t offset;
    osi_hyper_t end;
    osi_hyper_t fetched;
    osi_hyper_t tblocksize;
    afs_int32 code;
    int rxheld = 0;
    cm_buf_t *bp = NULL;
    afs_uint32 req_flags;

    /* Retries will be performed by the BkgDaemon thread if appropriate */
    req_flags = reqp->flags;
    reqp->flags |= CM_REQ_NORETRY;

    fetched.LowPart = 0;
    fetched.HighPart = 0;
    tblocksize = ConvertLongToLargeInteger(cm_data.buf_blockSize);
    base.LowPart = p1;
    base.HighPart = p2;
    length.LowPart = p3;
    length.HighPart = p4;

    end = LargeIntegerAdd(base, length);

    osi_Log5(afsd_logp, "Starting BKG prefetch scp 0x%p offset 0x%x:%x length 0x%x:%x",
             scp, p2, p1, p4, p3);

    for ( code = 0, offset = base;
          code == 0 && LargeIntegerLessThan(offset, end);
          offset = LargeIntegerAdd(offset, tblocksize) )
    {
        if (rxheld) {
            lock_ReleaseWrite(&scp->rw);
            rxheld = 0;
        }

        code = buf_Get(scp, &offset, reqp, &bp);
        if (code)
            break;

        if (bp->cmFlags & CM_BUF_CMFETCHING) {
            /* skip this buffer as another thread is already fetching it */
            if (!rxheld) {
                lock_ObtainWrite(&scp->rw);
                rxheld = 1;
            }
            buf_Release(bp);
            bp = NULL;
            continue;
        }

        if (!rxheld) {
            lock_ObtainWrite(&scp->rw);
            rxheld = 1;
        }

        code = cm_GetBuffer(scp, bp, NULL, userp, reqp);
        if (code == 0)
            fetched = LargeIntegerAdd(fetched, tblocksize);
        buf_Release(bp);
    }

    if (!rxheld) {
        lock_ObtainWrite(&scp->rw);
        rxheld = 1;
    }

    cm_ClearPrefetchFlag(LargeIntegerGreaterThanZero(fetched) ? 0 : code,
                         scp, &base, &fetched);

    /* wakeup anyone who is waiting */
    if (scp->flags & CM_SCACHEFLAG_WAITING) {
        osi_Log1(afsd_logp, "CM BkgPrefetch Waking scp 0x%p", scp);
        osi_Wakeup((LONG_PTR) &scp->flags);
    }
    lock_ReleaseWrite(&scp->rw);

    osi_Log4(afsd_logp, "Ending BKG prefetch scp 0x%p code 0x%x fetched 0x%x:%x",
             scp, code, fetched.HighPart, fetched.LowPart);

    reqp->flags = req_flags;
    return code;
}

/* a read was issued to offsetp, and we have to determine whether we should
 * do a prefetch of the next chunk.
 */
void cm_ConsiderPrefetch(cm_scache_t *scp, osi_hyper_t *offsetp, afs_uint32 count,
                         cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    int  rwheld = 0;
    osi_hyper_t realBase;
    osi_hyper_t readBase;
    osi_hyper_t readLength;
    osi_hyper_t readEnd;
    osi_hyper_t tblocksize;		/* a long long temp variable */

    tblocksize = ConvertLongToLargeInteger(cm_data.buf_blockSize);

    readBase = *offsetp;
    /* round up to chunk boundary */
    readBase.LowPart += (cm_chunkSize-1);
    readBase.LowPart &= (-cm_chunkSize);

    readLength = ConvertLongToLargeInteger(count);

    lock_ObtainWrite(&scp->rw);
    rwheld = 1;
    if ((scp->flags & CM_SCACHEFLAG_PREFETCHING)
         || LargeIntegerLessThanOrEqualTo(readBase, scp->prefetch.base)) {
        lock_ReleaseWrite(&scp->rw);
        return;
    }
    _InterlockedOr(&scp->flags, CM_SCACHEFLAG_PREFETCHING);

    /* start the scan at the latter of the end of this read or
     * the end of the last fetched region.
     */
    if (LargeIntegerGreaterThan(scp->prefetch.end, readBase))
        readBase = scp->prefetch.end;

    code = cm_CheckFetchRange(scp, &readBase, &readLength, userp, reqp,
                              &realBase);
    if (code) {
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_PREFETCHING);
        lock_ReleaseWrite(&scp->rw);
        return;	/* can't find something to prefetch */
    }

    readEnd = LargeIntegerAdd(realBase, readLength);

    if (rwheld)
        lock_ReleaseWrite(&scp->rw);

    osi_Log2(afsd_logp, "BKG Prefetch request scp 0x%p, base 0x%x",
             scp, realBase.LowPart);

    cm_QueueBKGRequest(scp, cm_BkgPrefetch,
                       realBase.LowPart, realBase.HighPart,
                       readLength.LowPart, readLength.HighPart,
                       userp, reqp);
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
    biop->scp = scp;			/* do not hold; held by caller */
    biop->offset = *inOffsetp;
    biop->length = 0;
    biop->bufListp = NULL;
    biop->bufListEndp = NULL;
    biop->reserved = 0;

    /* reserve a chunk's worth of buffers */
    lock_ReleaseWrite(&scp->rw);
    buf_ReserveBuffers(cm_chunkSize / cm_data.buf_blockSize);
    lock_ObtainWrite(&scp->rw);

    bufp = NULL;
    for (temp = 0; temp < inSize; temp += cm_data.buf_blockSize) {
        thyper = ConvertLongToLargeInteger(temp);
        tbase = LargeIntegerAdd(*inOffsetp, thyper);

        bufp = buf_Find(&scp->fid, &tbase);
        if (bufp) {
            /* get buffer mutex and scp mutex safely */
            lock_ReleaseWrite(&scp->rw);
            lock_ObtainMutex(&bufp->mx);

            /*
             * if the buffer is actively involved in I/O
             * we wait for the I/O to complete.
             */
            if (bufp->flags & (CM_BUF_WRITING|CM_BUF_READING))
                buf_WaitIO(scp, bufp);

            lock_ObtainWrite(&scp->rw);
            flags = CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_STOREDATA | CM_SCACHESYNC_BUFLOCKED;
            code = cm_SyncOp(scp, bufp, userp, reqp, 0, flags);
            if (code) {
                lock_ReleaseMutex(&bufp->mx);
                buf_Release(bufp);
                bufp = NULL;
                buf_UnreserveBuffers(cm_chunkSize / cm_data.buf_blockSize);
                return code;
            }

            /* if the buffer is dirty, we're done */
            if (bufp->flags & CM_BUF_DIRTY) {
                osi_assertx(!(bufp->flags & CM_BUF_WRITING),
                            "WRITING w/o CMSTORING in SetupStoreBIOD");
                _InterlockedOr(&bufp->flags, CM_BUF_WRITING);
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

    /* compute the window surrounding firstModOffset of size cm_chunkSize */
    scanStart = firstModOffset;
    scanStart.LowPart &= (-cm_chunkSize);
    thyper = ConvertLongToLargeInteger(cm_chunkSize);
    scanEnd = LargeIntegerAdd(scanStart, thyper);

    flags = CM_SCACHESYNC_GETSTATUS
        | CM_SCACHESYNC_STOREDATA
        | CM_SCACHESYNC_BUFLOCKED;

    /* start by looking backwards until scanStart */
    /* hyper version of cm_data.buf_blockSize */
    thyper = ConvertLongToLargeInteger(cm_data.buf_blockSize);
    tbase = LargeIntegerSubtract(firstModOffset, thyper);
    while(LargeIntegerGreaterThanOrEqualTo(tbase, scanStart)) {
        /* see if we can find the buffer */
        bufp = buf_Find(&scp->fid, &tbase);
        if (!bufp)
            break;

        /* try to lock it, and quit if we can't (simplifies locking) */
        lock_ReleaseWrite(&scp->rw);
        code = lock_TryMutex(&bufp->mx);
        lock_ObtainWrite(&scp->rw);
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
    /* hyper version of cm_data.buf_blockSize */
    thyper = ConvertLongToLargeInteger(cm_data.buf_blockSize);
    tbase = LargeIntegerAdd(firstModOffset, thyper);
    while(LargeIntegerLessThan(tbase, scanEnd)) {
        /* see if we can find the buffer */
        bufp = buf_Find(&scp->fid, &tbase);
        if (!bufp)
            break;

        /* try to lock it, and quit if we can't (simplifies locking) */
        lock_ReleaseWrite(&scp->rw);
        code = lock_TryMutex(&bufp->mx);
        lock_ObtainWrite(&scp->rw);
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
			cm_bulkIO_t *biop, cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_buf_t *tbp;
    osi_hyper_t tblocksize;		/* a long long temp variable */
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

    tblocksize = ConvertLongToLargeInteger(cm_data.buf_blockSize);

    biop->scp = scp;			/* do not hold; held by caller */
    biop->reqp = reqp;
    biop->offset = *offsetp;
    /* null out the list of buffers */
    biop->bufListp = biop->bufListEndp = NULL;
    biop->reserved = 0;

    /* first lookup the file's length, so we know when to stop */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code)
        return code;

    /* copy out size, since it may change */
    fileSize = scp->serverLength;

    lock_ReleaseWrite(&scp->rw);

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

    /* first hold all buffers, since we can't hold any locks in buf_Get */
    while (1) {
        /* stop at chunk boundary */
        if (collected >= cm_chunkSize)
            break;

        /* see if the next page would be past EOF */
        if (LargeIntegerGreaterThanOrEqualTo(pageBase, fileSize))
            break;

        code = buf_Get(scp, &pageBase, reqp, &tbp);
        if (code) {
            lock_ObtainWrite(&scp->rw);
            cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
            return code;
        }

        buf_Release(tbp);
        tbp = NULL;

        pageBase = LargeIntegerAdd(tblocksize, pageBase);
        collected += cm_data.buf_blockSize;
    }

    /* reserve a chunk's worth of buffers if possible */
    reserving = buf_TryReserveBuffers(cm_chunkSize / cm_data.buf_blockSize);

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

        tbp = buf_Find(&scp->fid, &pageBase);
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
        pageBase = LargeIntegerAdd(tblocksize, pageBase);
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
        lock_ObtainWrite(&scp->rw);

        /* don't bother fetching over data that is already current */
        if (tbp->dataVersion <= scp->dataVersion && tbp->dataVersion >= scp->bufDataVersionLow) {
            /* we don't need this buffer, since it is current */
            lock_ReleaseWrite(&scp->rw);
            lock_ReleaseMutex(&tbp->mx);
            break;
        }

        flags = CM_SCACHESYNC_FETCHDATA | CM_SCACHESYNC_BUFLOCKED;
        if (!isFirst)
            flags |= CM_SCACHESYNC_NOWAIT;

        /* wait for the buffer to serialize, if required.  Doesn't
         * release the scp or buffer lock(s) if NOWAIT is specified.
         */
        code = cm_SyncOp(scp, tbp, userp, reqp, 0, flags);
        if (code) {
            lock_ReleaseWrite(&scp->rw);
            lock_ReleaseMutex(&tbp->mx);
            break;
        }

        /* don't fetch over dirty buffers */
        if (tbp->flags & CM_BUF_DIRTY) {
            cm_SyncOpDone(scp, tbp, flags);
            lock_ReleaseWrite(&scp->rw);
            lock_ReleaseMutex(&tbp->mx);
            break;
        }

        /* Release locks */
        lock_ReleaseWrite(&scp->rw);
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
        tbp = NULL;
    }

    /* Caller expects this */
    lock_ObtainWrite(&scp->rw);

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
void cm_ReleaseBIOD(cm_bulkIO_t *biop, int isStore, long code, int scp_locked)
{
    cm_scache_t *scp;		/* do not release; not held in biop */
    cm_buf_t *bufp;
    osi_queueData_t *qdp;
    osi_queueData_t *nqdp;
    int flags;

    /* Give back reserved buffers */
    if (biop->reserved)
        buf_UnreserveBuffers(cm_chunkSize / cm_data.buf_blockSize);

    if (isStore)
        flags = CM_SCACHESYNC_STOREDATA;
    else
        flags = CM_SCACHESYNC_FETCHDATA;

    scp = biop->scp;
    if (biop->bufListp) {
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
            if (scp_locked)
                lock_ReleaseWrite(&scp->rw);
	    lock_ObtainMutex(&bufp->mx);
	    lock_ObtainWrite(&scp->rw);
	    cm_SyncOpDone(scp, bufp, flags);

	    /* turn off writing and wakeup users */
	    if (isStore) {
		if (bufp->flags & CM_BUF_WAITING) {
		    osi_Log2(afsd_logp, "cm_ReleaseBIOD Waking [scp 0x%p] bp 0x%p", scp, bufp);
		    osi_Wakeup((LONG_PTR) bufp);
		}
		if (code) {
		    _InterlockedAnd(&bufp->flags, ~CM_BUF_WRITING);
                    switch (code) {
                    case CM_ERROR_NOSUCHFILE:
                    case CM_ERROR_BADFD:
                    case CM_ERROR_NOACCESS:
                    case CM_ERROR_QUOTA:
                    case CM_ERROR_SPACE:
                    case CM_ERROR_TOOBIG:
                    case CM_ERROR_READONLY:
                    case CM_ERROR_NOSUCHPATH:
                        /*
                         * Apply the fatal error to this buffer.
                         */
                        _InterlockedAnd(&bufp->flags, ~CM_BUF_DIRTY);
                        _InterlockedOr(&bufp->flags, CM_BUF_ERROR);
                        bufp->dirty_length = 0;
                        bufp->error = code;
                        bufp->dataVersion = CM_BUF_VERSION_BAD;
                        bufp->dirtyCounter++;
                        break;
                    case CM_ERROR_TIMEDOUT:
                    case CM_ERROR_ALLDOWN:
                    case CM_ERROR_ALLBUSY:
                    case CM_ERROR_ALLOFFLINE:
                    case CM_ERROR_CLOCKSKEW:
                    default:
                        /* do not mark the buffer in error state but do
                        * not attempt to complete the rest either.
                        */
                        break;
                    }
		} else {
		    _InterlockedAnd(&bufp->flags, ~(CM_BUF_WRITING | CM_BUF_DIRTY));
                    bufp->dirty_offset = bufp->dirty_length = 0;
                }
	    }

            if (!scp_locked)
                lock_ReleaseWrite(&scp->rw);
	    lock_ReleaseMutex(&bufp->mx);
	    buf_Release(bufp);
	    bufp = NULL;
	}
    } else {
	if (!scp_locked)
            lock_ObtainWrite(&scp->rw);
	cm_SyncOpDone(scp, NULL, flags);
        if (!scp_locked)
            lock_ReleaseWrite(&scp->rw);
    }

    /* clean things out */
    biop->bufListp = NULL;
    biop->bufListEndp = NULL;
}

static int
cm_CloneStatus(cm_scache_t *scp, cm_user_t *userp, int scp_locked,
               AFSFetchStatus *afsStatusp, AFSVolSync *volSyncp)
{
    // setup the status based upon the scp data
    afsStatusp->InterfaceVersion = 0x1;
    switch (scp->fileType) {
    case CM_SCACHETYPE_FILE:
        afsStatusp->FileType = File;
        break;
    case CM_SCACHETYPE_DIRECTORY:
        afsStatusp->FileType = Directory;
        break;
    case CM_SCACHETYPE_MOUNTPOINT:
        afsStatusp->FileType = SymbolicLink;
        break;
    case CM_SCACHETYPE_SYMLINK:
    case CM_SCACHETYPE_DFSLINK:
        afsStatusp->FileType = SymbolicLink;
        break;
    default:
        afsStatusp->FileType = -1;    /* an invalid value */
    }
    afsStatusp->LinkCount = scp->linkCount;
    afsStatusp->Length = scp->length.LowPart;
    afsStatusp->DataVersion = (afs_uint32)(scp->dataVersion & MAX_AFS_UINT32);
    afsStatusp->Author = 0x1;
    afsStatusp->Owner = scp->owner;
    if (!scp_locked) {
        lock_ObtainWrite(&scp->rw);
        scp_locked = 1;
    }
    if (cm_FindACLCache(scp, userp, &afsStatusp->CallerAccess))
        afsStatusp->CallerAccess = scp->anyAccess;
    afsStatusp->AnonymousAccess = scp->anyAccess;
    afsStatusp->UnixModeBits = scp->unixModeBits;
    afsStatusp->ParentVnode = scp->parentVnode;
    afsStatusp->ParentUnique = scp->parentUnique;
    afsStatusp->ResidencyMask = 0;
    afsStatusp->ClientModTime = scp->clientModTime;
    afsStatusp->ServerModTime = scp->serverModTime;
    afsStatusp->Group = scp->group;
    afsStatusp->SyncCounter = 0;
    afsStatusp->dataVersionHigh = (afs_uint32)(scp->dataVersion >> 32);
    afsStatusp->lockCount = 0;
    afsStatusp->Length_hi = scp->length.HighPart;
    afsStatusp->errorCode = 0;

    volSyncp->spare1 = scp->volumeCreationDate;

    return scp_locked;
}

/* Fetch a buffer.  Called with scp locked.
 * The scp is locked on return.
 */
long cm_GetBuffer(cm_scache_t *scp, cm_buf_t *bufp, int *cpffp, cm_user_t *userp,
                  cm_req_t *reqp)
{
    long code=0, code1=0;
    afs_uint32 nbytes;			/* bytes in transfer */
    afs_uint32 nbytes_hi = 0;            /* high-order 32 bits of bytes in transfer */
    afs_uint64 length_found = 0;
    long rbytes;			/* bytes in rx_Read call */
    long temp;
    AFSFetchStatus afsStatus;
    AFSCallBack callback;
    AFSVolSync volSync;
    char *bufferp;
    afs_uint32 buffer_offset;
    cm_buf_t *tbufp;		        /* buf we're filling */
    osi_queueData_t *qdp;		/* q element we're scanning */
    AFSFid tfid;
    struct rx_call *rxcallp;
    struct rx_connection *rxconnp;
    cm_bulkIO_t biod;		/* bulk IO descriptor */
    cm_conn_t *connp;
    int getroot;
    afs_int32 t1,t2;
    int require_64bit_ops = 0;
    int call_was_64bit = 0;
    int fs_fetchdata_offset_bug = 0;
    int first_read = 1;
    int scp_locked = 1;

    memset(&volSync, 0, sizeof(volSync));

    /* now, the buffer may or may not be filled with good data (buf_GetNewLocked
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

    if (cm_HaveCallback(scp) && bufp->dataVersion <= scp->dataVersion && bufp->dataVersion >= scp->bufDataVersionLow) {
        /* We already have this buffer don't do extra work */
        return 0;
    }

    cm_AFSFidFromFid(&tfid, &scp->fid);

    code = cm_SetupFetchBIOD(scp, &bufp->offset, &biod, userp, reqp);
    if (code) {
        /* couldn't even get the first page setup properly */
        osi_Log1(afsd_logp, "GetBuffer: SetupFetchBIOD failure code %d", code);
        return code;
    }

    /* once we get here, we have the callback in place, we know that no one
     * is fetching the data now.  Check one last time that we still have
     * the wrong data, and then fetch it if we're still wrong.
     *
     * We can lose a race condition and end up with biod.length zero, in
     * which case we just retry.
     */
    if (bufp->dataVersion <= scp->dataVersion && bufp->dataVersion >= scp->bufDataVersionLow || biod.length == 0) {
        if ((bufp->dataVersion == CM_BUF_VERSION_BAD || bufp->dataVersion < scp->bufDataVersionLow) &&
             LargeIntegerGreaterThanOrEqualTo(bufp->offset, scp->serverLength))
        {
            osi_Log4(afsd_logp, "Bad DVs 0x%x != (0x%x -> 0x%x) or length 0x%x",
                     bufp->dataVersion, scp->bufDataVersionLow, scp->dataVersion, biod.length);

            if (bufp->dataVersion == CM_BUF_VERSION_BAD)
                memset(bufp->datap, 0, cm_data.buf_blockSize);
            bufp->dataVersion = scp->dataVersion;
        }
        cm_ReleaseBIOD(&biod, 0, 0, 1);
        return 0;
    } else if ((bufp->dataVersion == CM_BUF_VERSION_BAD || bufp->dataVersion < scp->bufDataVersionLow)
                && (scp->mask & CM_SCACHEMASK_TRUNCPOS) &&
                LargeIntegerGreaterThanOrEqualTo(bufp->offset, scp->truncPos)) {
        memset(bufp->datap, 0, cm_data.buf_blockSize);
        bufp->dataVersion = scp->dataVersion;
        cm_ReleaseBIOD(&biod, 0, 0, 1);
        return 0;
    }

    InterlockedIncrement(&scp->activeRPCs);
    lock_ReleaseWrite(&scp->rw);
    scp_locked = 0;

    if (LargeIntegerGreaterThan(LargeIntegerAdd(biod.offset,
                                                ConvertLongToLargeInteger(biod.length)),
                                ConvertLongToLargeInteger(LONG_MAX))) {
        require_64bit_ops = 1;
    }

    osi_Log2(afsd_logp, "cm_GetBuffer: fetching data scp %p bufp %p", scp, bufp);
    osi_Log3(afsd_logp, "cm_GetBuffer: fetching data scpDV 0x%x scpDVLow 0x%x bufDV 0x%x",
             scp->dataVersion, scp->bufDataVersionLow, bufp->dataVersion);

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
        afsStatus.DataVersion = (afs_uint32)(cm_data.fakeDirVersion & 0xFFFFFFFF);
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
        afsStatus.dataVersionHigh = (afs_uint32)(cm_data.fakeDirVersion >> 32);
        afsStatus.lockCount = 0;
        afsStatus.Length_hi = 0;
        afsStatus.errorCode = 0;
	memset(&volSync, 0, sizeof(volSync));

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
            if (t2> (afs_int32)cm_data.buf_blockSize)
                t2=cm_data.buf_blockSize;
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

    /*
     * if the requested offset is greater than the file length,
     * the file server will return zero bytes of data and the
     * current status for the file which we already have since
     * we have just obtained a callback.  Instead, we can avoid
     * the network round trip by allocating zeroed buffers and
     * faking the status info.
     */
    if (biod.offset.QuadPart >= scp->length.QuadPart) {
        osi_Log5(afsd_logp, "SKIP FetchData64 scp 0x%p, off 0x%x:%08x > length 0x%x:%08x",
                 scp, biod.offset.HighPart, biod.offset.LowPart,
                 scp->length.HighPart, scp->length.LowPart);

        /* Clone the current status info */
        scp_locked = cm_CloneStatus(scp, userp, scp_locked, &afsStatus, &volSync);

        /* status info complete, fill pages with zeros */
        for (qdp = biod.bufListEndp;
             qdp;
             qdp = (osi_queueData_t *) osi_QPrev(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            bufferp=tbufp->datap;
            memset(bufferp, 0, cm_data.buf_blockSize);
        }

        /* no need to contact the file server */
        goto fetchingcompleted;
    }

    if (scp_locked) {
        lock_ReleaseWrite(&scp->rw);
        scp_locked = 0;
    }

    /* now make the call */
    do {
        code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
        if (code)
            continue;

        rxconnp = cm_GetRxConn(connp);
        rxcallp = rx_NewCall(rxconnp);
        rx_PutConnection(rxconnp);

        nbytes = nbytes_hi = 0;

        if (SERVERHAS64BIT(connp)) {
            call_was_64bit = 1;

            osi_Log4(afsd_logp, "CALL FetchData64 scp 0x%p, off 0x%x:%08x, size 0x%x",
                     scp, biod.offset.HighPart, biod.offset.LowPart, biod.length);

            code = StartRXAFS_FetchData64(rxcallp, &tfid, biod.offset.QuadPart, biod.length);

            if (code == 0) {
                temp = rx_Read32(rxcallp, &nbytes_hi);
                if (temp == sizeof(afs_int32)) {
                    nbytes_hi = ntohl(nbytes_hi);
                } else {
                    nbytes_hi = 0;
		    code = rxcallp->error;
                    code1 = rx_EndCall(rxcallp, code);
                    rxcallp = NULL;
                }
            }
        } else {
            call_was_64bit = 0;
        }

        if (code == RXGEN_OPCODE || !SERVERHAS64BIT(connp)) {
            if (require_64bit_ops) {
                osi_Log0(afsd_logp, "Skipping FetchData.  Operation requires FetchData64");
                code = CM_ERROR_TOOBIG;
            } else {
                if (!rxcallp) {
                    rxconnp = cm_GetRxConn(connp);
                    rxcallp = rx_NewCall(rxconnp);
                    rx_PutConnection(rxconnp);
                }

                osi_Log3(afsd_logp, "CALL FetchData scp 0x%p, off 0x%x, size 0x%x",
                         scp, biod.offset.LowPart, biod.length);

                code = StartRXAFS_FetchData(rxcallp, &tfid, biod.offset.LowPart,
                                            biod.length);

                SET_SERVERHASNO64BIT(connp);
            }
        }

        if (code == 0) {
            temp  = rx_Read32(rxcallp, &nbytes);
            if (temp == sizeof(afs_int32)) {
                nbytes = ntohl(nbytes);
                FillInt64(length_found, nbytes_hi, nbytes);
                if (length_found > biod.length) {
                    /*
                     * prior to 1.4.12 and 1.5.65 the file server would return
                     * (filesize - offset) if the requested offset was greater than
                     * the filesize.  The correct return value would have been zero.
                     * Force a retry by returning an RX_PROTOCOL_ERROR.  If the cause
                     * is a race between two RPCs issues by this cache manager, the
                     * correct thing will happen the second time.
                     */
                    osi_Log0(afsd_logp, "cm_GetBuffer length_found > biod.length");
                    fs_fetchdata_offset_bug = 1;
                }
            } else {
                osi_Log1(afsd_logp, "cm_GetBuffer rx_Read32 returns %d != 4", temp);
                code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
            }
        }
        /* for the moment, nbytes_hi will always be 0 if code == 0
           because biod.length is a 32-bit quantity. */

        if (code == 0) {
            qdp = biod.bufListEndp;
            if (qdp) {
                tbufp = osi_GetQData(qdp);
                bufferp = tbufp->datap;
                buffer_offset = 0;
            }
            else
                bufferp = NULL;

            /* fill length_found of data from the pipe into the pages.
             * When we stop, qdp will point at the last page we're
             * dealing with, and bufferp will tell us where we
             * stopped.  We'll need this info below when we clear
             * the remainder of the last page out (and potentially
             * clear later pages out, if we fetch past EOF).
             */
            while (length_found > 0) {
#ifdef USE_RX_IOVEC
                struct iovec tiov[RX_MAXIOVECS];
                afs_int32 tnio, iov, iov_offset;

                temp = rx_Readv(rxcallp, tiov, &tnio, RX_MAXIOVECS, length_found);
                osi_Log1(afsd_logp, "cm_GetBuffer rx_Readv returns %d", temp);
                if (temp != length_found && temp < cm_data.buf_blockSize) {
                    /*
                     * If the file server returned (filesize - offset),
                     * then the first rx_Read will return zero octets of data.
                     * If it does, do not treat it as an error.  Correct the
                     * length_found and continue as if the file server said
                     * it was sending us zero octets of data.
                     */
                    if (fs_fetchdata_offset_bug && first_read)
                        length_found = 0;
                    else
                        code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
                    break;
                }

                iov = 0;
                iov_offset = 0;
                rbytes = temp;

                while (rbytes > 0) {
                    afs_int32 len;

                    osi_assertx(bufferp != NULL, "null cm_buf_t");

                    len = MIN(tiov[iov].iov_len - iov_offset, cm_data.buf_blockSize - buffer_offset);
                    memcpy(bufferp + buffer_offset, tiov[iov].iov_base + iov_offset, len);
                    iov_offset += len;
                    buffer_offset += len;
                    rbytes -= len;

                    if (iov_offset == tiov[iov].iov_len) {
                        iov++;
                        iov_offset = 0;
                    }

                    if (buffer_offset == cm_data.buf_blockSize) {
                        /* allow read-while-fetching.
                        * if this is the last buffer, clear the
                        * PREFETCHING flag, so the reader waiting for
                        * this buffer will start a prefetch.
                        */
                        _InterlockedOr(&tbufp->cmFlags, CM_BUF_CMFULLYFETCHED);
                        lock_ObtainWrite(&scp->rw);
                        if (scp->flags & CM_SCACHEFLAG_WAITING) {
                            osi_Log1(afsd_logp, "CM GetBuffer Waking scp 0x%p", scp);
                            osi_Wakeup((LONG_PTR) &scp->flags);
                        }
                        if (cpffp && !*cpffp && !osi_QPrev(&qdp->q)) {
                            osi_hyper_t tlength = ConvertLongToLargeInteger(biod.length);
                            *cpffp = 1;
                            cm_ClearPrefetchFlag(0, scp, &biod.offset, &tlength);
                        }
                        lock_ReleaseWrite(&scp->rw);

                        /* Advance the buffer */
                        qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                        if (qdp) {
                            tbufp = osi_GetQData(qdp);
                            bufferp = tbufp->datap;
                            buffer_offset = 0;
                        }
                        else
                            bufferp = NULL;
                    }
                }

                length_found -= temp;
#else /* USE_RX_IOVEC */
                /* assert that there are still more buffers;
                 * our check above for length_found being less than
                 * biod.length should ensure this.
                 */
                osi_assertx(bufferp != NULL, "null cm_buf_t");

                /* read rbytes of data */
                rbytes = (afs_uint32)(length_found > cm_data.buf_blockSize ? cm_data.buf_blockSize : length_found);
                temp = rx_Read(rxcallp, bufferp, rbytes);
                if (temp < rbytes) {
                    /*
                     * If the file server returned (filesize - offset),
                     * then the first rx_Read will return zero octets of data.
                     * If it does, do not treat it as an error.  Correct the
                     * length_found and continue as if the file server said
                     * it was sending us zero octets of data.
                     */
                    if (fs_fetchdata_offset_bug && first_read)
                        length_found = 0;
                    else
                        code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
                    break;
                }
                first_read = 0;

                /* allow read-while-fetching.
                 * if this is the last buffer, clear the
                 * PREFETCHING flag, so the reader waiting for
                 * this buffer will start a prefetch.
                 */
                _InterlockedOr(&tbufp->cmFlags, CM_BUF_CMFULLYFETCHED);
                lock_ObtainWrite(&scp->rw);
                if (scp->flags & CM_SCACHEFLAG_WAITING) {
                    osi_Log1(afsd_logp, "CM GetBuffer Waking scp 0x%p", scp);
                    osi_Wakeup((LONG_PTR) &scp->flags);
                }
                if (cpffp && !*cpffp && !osi_QPrev(&qdp->q)) {
                    osi_hyper_t tlength = ConvertLongToLargeInteger(biod.length);
                    *cpffp = 1;
                    cm_ClearPrefetchFlag(0, scp, &biod.offset, &tlength);
                }
                lock_ReleaseWrite(&scp->rw);

                /* and adjust counters */
                length_found -= temp;

                /* and move to the next buffer */
                if (length_found != 0) {
                    qdp = (osi_queueData_t *) osi_QPrev(&qdp->q);
                    if (qdp) {
                        tbufp = osi_GetQData(qdp);
                        bufferp = tbufp->datap;
                    }
                    else
                        bufferp = NULL;
                } else
                    bufferp += temp;
#endif /* USE_RX_IOVEC */
            }

            /* zero out remainder of last pages, in case we are
             * fetching past EOF.  We were fetching an integral #
             * of pages, but stopped, potentially in the middle of
             * a page.  Zero the remainder of that page, and then
             * all of the rest of the pages.
             */
#ifdef USE_RX_IOVEC
            rbytes = cm_data.buf_blockSize - buffer_offset;
            bufferp = tbufp->datap + buffer_offset;
#else /* USE_RX_IOVEC */
            /* bytes fetched */
	    osi_assertx((bufferp - tbufp->datap) < LONG_MAX, "data >= LONG_MAX");
            rbytes = (long) (bufferp - tbufp->datap);

            /* bytes left to zero */
            rbytes = cm_data.buf_blockSize - rbytes;
#endif /* USE_RX_IOVEC */
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
            if (call_was_64bit)
                code = EndRXAFS_FetchData64(rxcallp, &afsStatus, &callback, &volSync);
            else
                code = EndRXAFS_FetchData(rxcallp, &afsStatus, &callback, &volSync);
        } else {
            if (call_was_64bit)
                osi_Log1(afsd_logp, "CALL EndRXAFS_FetchData64 skipped due to error %d", code);
            else
                osi_Log1(afsd_logp, "CALL EndRXAFS_FetchData skipped due to error %d", code);
        }

        if (rxcallp)
            code1 = rx_EndCall(rxcallp, code);

        if (code1 == RXKADUNKNOWNKEY)
            osi_Log0(afsd_logp, "CALL EndCall returns RXKADUNKNOWNKEY");

        /* If we are avoiding a file server bug, ignore the error state */
        if (fs_fetchdata_offset_bug && first_read && length_found == 0 && code == -451) {
            /* Clone the current status info and clear the error state */
            scp_locked = cm_CloneStatus(scp, userp, scp_locked, &afsStatus, &volSync);
            if (scp_locked) {
                lock_ReleaseWrite(&scp->rw);
                scp_locked = 0;
            }
            code = 0;
        /* Prefer the error value from FetchData over rx_EndCall */
        } else if (code == 0 && code1 != 0)
            code = code1;
        osi_Log0(afsd_logp, "CALL FetchData DONE");

    } while (cm_Analyze(connp, userp, reqp, &scp->fid, 0, &volSync, NULL, NULL, code));

  fetchingcompleted:
    code = cm_MapRPCError(code, reqp);

    if (!scp_locked)
        lock_ObtainWrite(&scp->rw);

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
            tbufp->dataVersion = afsStatus.dataVersionHigh;
            tbufp->dataVersion <<= 32;
            tbufp->dataVersion |= afsStatus.DataVersion;

#ifdef DISKCACHE95
            /* write buffer out to disk cache */
            diskcache_Update(tbufp->dcp, tbufp->datap, cm_data.buf_blockSize,
                              tbufp->dataVersion);
#endif /* DISKCACHE95 */
        }
    }

    if (code == 0)
        cm_MergeStatus(NULL, scp, &afsStatus, &volSync, userp, reqp, CM_MERGEFLAG_FETCHDATA);
    else
        InterlockedDecrement(&scp->activeRPCs);

    /* release scatter/gather I/O structure (buffers, locks) */
    cm_ReleaseBIOD(&biod, 0, code, 1);

    return code;
}

/*
 * Similar to cm_GetBuffer but doesn't use an allocated cm_buf_t object.
 * Instead the data is read from the file server and copied directly into
 * a provided buffer.  Called with scp locked. The scp is locked on return.
 */
long cm_GetData(cm_scache_t *scp, osi_hyper_t *offsetp, char *datap, int data_length,
                cm_user_t *userp, cm_req_t *reqp)
{
    long code=0, code1=0;
    afs_uint32 nbytes;			/* bytes in transfer */
    afs_uint32 nbytes_hi = 0;           /* high-order 32 bits of bytes in transfer */
    afs_uint64 length_found = 0;
    char *bufferp = datap;
    afs_uint32 buffer_offset = 0;
    long rbytes;			/* bytes in rx_Read call */
    long temp;
    AFSFetchStatus afsStatus;
    AFSCallBack callback;
    AFSVolSync volSync;
    AFSFid tfid;
    struct rx_call *rxcallp;
    struct rx_connection *rxconnp;
    cm_conn_t *connp;
    int getroot;
    afs_int32 t1,t2;
    int require_64bit_ops = 0;
    int call_was_64bit = 0;
    int fs_fetchdata_offset_bug = 0;
    int first_read = 1;
    int scp_locked = 1;

    memset(&volSync, 0, sizeof(volSync));

    /* now, the buffer may or may not be filled with good data (buf_GetNewLocked
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

    if (LargeIntegerGreaterThan(LargeIntegerAdd(*offsetp,
                                                ConvertLongToLargeInteger(data_length)),
                                ConvertLongToLargeInteger(LONG_MAX))) {
        require_64bit_ops = 1;
    }

    InterlockedIncrement(&scp->activeRPCs);
    osi_Log2(afsd_logp, "cm_GetData: fetching data scp %p DV 0x%x", scp, scp->dataVersion);

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
        afsStatus.DataVersion = (afs_uint32)(cm_data.fakeDirVersion & 0xFFFFFFFF);
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
        afsStatus.dataVersionHigh = (afs_uint32)(cm_data.fakeDirVersion >> 32);
        afsStatus.lockCount = 0;
        afsStatus.Length_hi = 0;
        afsStatus.errorCode = 0;
	memset(&volSync, 0, sizeof(volSync));

        // once we're done setting up the status info,
        // we just fill the buffer pages with fakedata
        // from cm_FakeRootDir. Extra pages are set to
        // 0.

        lock_ObtainMutex(&cm_Freelance_Lock);
        t1 = offsetp->LowPart;
        memset(datap, 0, data_length);
        t2 = cm_fakeDirSize - t1;
        if (t2 > data_length)
            t2 = data_length;
        if (t2 > 0)
            memcpy(datap, cm_FakeRootDir+t1, t2);
        lock_ReleaseMutex(&cm_Freelance_Lock);

        // once we're done, we skip over the part of the
        // code that does the ACTUAL fetching of data for
        // real files

        goto fetchingcompleted;
    }

#endif /* AFS_FREELANCE_CLIENT */

    if (scp_locked) {
        lock_ReleaseWrite(&scp->rw);
        scp_locked = 0;
    }

    /* now make the call */
    do {
        code = cm_ConnFromFID(&scp->fid, userp, reqp, &connp);
        if (code)
            continue;

        rxconnp = cm_GetRxConn(connp);
        rxcallp = rx_NewCall(rxconnp);
        rx_PutConnection(rxconnp);

        nbytes = nbytes_hi = 0;

        if (SERVERHAS64BIT(connp)) {
            call_was_64bit = 1;

            osi_Log4(afsd_logp, "CALL FetchData64 scp 0x%p, off 0x%x:%08x, size 0x%x",
                     scp, offsetp->HighPart, offsetp->LowPart, data_length);

            code = StartRXAFS_FetchData64(rxcallp, &tfid, offsetp->QuadPart, data_length);

            if (code == 0) {
                temp = rx_Read32(rxcallp, &nbytes_hi);
                if (temp == sizeof(afs_int32)) {
                    nbytes_hi = ntohl(nbytes_hi);
                } else {
                    nbytes_hi = 0;
		    code = rxcallp->error;
                    code1 = rx_EndCall(rxcallp, code);
                    rxcallp = NULL;
                }
            }
        } else {
            call_was_64bit = 0;
        }

        if (code == RXGEN_OPCODE || !SERVERHAS64BIT(connp)) {
            if (require_64bit_ops) {
                osi_Log0(afsd_logp, "Skipping FetchData.  Operation requires FetchData64");
                code = CM_ERROR_TOOBIG;
            } else {
                if (!rxcallp) {
                    rxconnp = cm_GetRxConn(connp);
                    rxcallp = rx_NewCall(rxconnp);
                    rx_PutConnection(rxconnp);
                }

                osi_Log3(afsd_logp, "CALL FetchData scp 0x%p, off 0x%x, size 0x%x",
                         scp, offsetp->LowPart, data_length);

                code = StartRXAFS_FetchData(rxcallp, &tfid, offsetp->LowPart, data_length);

                SET_SERVERHASNO64BIT(connp);
            }
        }

        if (code == 0) {
            temp  = rx_Read32(rxcallp, &nbytes);
            if (temp == sizeof(afs_int32)) {
                nbytes = ntohl(nbytes);
                FillInt64(length_found, nbytes_hi, nbytes);
                if (length_found > data_length) {
                    /*
                     * prior to 1.4.12 and 1.5.65 the file server would return
                     * (filesize - offset) if the requested offset was greater than
                     * the filesize.  The correct return value would have been zero.
                     * Force a retry by returning an RX_PROTOCOL_ERROR.  If the cause
                     * is a race between two RPCs issues by this cache manager, the
                     * correct thing will happen the second time.
                     */
                    osi_Log0(afsd_logp, "cm_GetData length_found > data_length");
                    fs_fetchdata_offset_bug = 1;
                }
            } else {
                osi_Log1(afsd_logp, "cm_GetData rx_Read32 returns %d != 4", temp);
                code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
            }
        }
        /* for the moment, nbytes_hi will always be 0 if code == 0
           because data_length is a 32-bit quantity. */

        if (code == 0) {
            /* fill length_found of data from the pipe into the pages.
             * When we stop, qdp will point at the last page we're
             * dealing with, and bufferp will tell us where we
             * stopped.  We'll need this info below when we clear
             * the remainder of the last page out (and potentially
             * clear later pages out, if we fetch past EOF).
             */
            while (length_found > 0) {
#ifdef USE_RX_IOVEC
                struct iovec tiov[RX_MAXIOVECS];
                afs_int32 tnio, iov, iov_offset;

                temp = rx_Readv(rxcallp, tiov, &tnio, RX_MAXIOVECS, length_found);
                osi_Log1(afsd_logp, "cm_GetData rx_Readv returns %d", temp);
                if (temp != length_found && temp < data_length) {
                    /*
                     * If the file server returned (filesize - offset),
                     * then the first rx_Read will return zero octets of data.
                     * If it does, do not treat it as an error.  Correct the
                     * length_found and continue as if the file server said
                     * it was sending us zero octets of data.
                     */
                    if (fs_fetchdata_offset_bug && first_read)
                        length_found = 0;
                    else
                        code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
                    break;
                }

                iov = 0;
                iov_offset = 0;
                rbytes = temp;

                while (rbytes > 0) {
                    afs_int32 len;

                    osi_assertx(bufferp != NULL, "null cm_buf_t");

                    len = MIN(tiov[iov].iov_len - iov_offset, data_length - buffer_offset);
                    memcpy(bufferp + buffer_offset, tiov[iov].iov_base + iov_offset, len);
                    iov_offset += len;
                    buffer_offset += len;
                    rbytes -= len;

                    if (iov_offset == tiov[iov].iov_len) {
                        iov++;
                        iov_offset = 0;
                    }
                }

                length_found -= temp;
#else /* USE_RX_IOVEC */
                /* assert that there are still more buffers;
                 * our check above for length_found being less than
                 * data_length should ensure this.
                 */
                osi_assertx(bufferp != NULL, "null cm_buf_t");

                /* read rbytes of data */
                rbytes = (afs_uint32)(length_found > data_length ? data_length : length_found);
                temp = rx_Read(rxcallp, bufferp, rbytes);
                if (temp < rbytes) {
                    /*
                     * If the file server returned (filesize - offset),
                     * then the first rx_Read will return zero octets of data.
                     * If it does, do not treat it as an error.  Correct the
                     * length_found and continue as if the file server said
                     * it was sending us zero octets of data.
                     */
                    if (fs_fetchdata_offset_bug && first_read)
                        length_found = 0;
                    else
                        code = (rxcallp->error < 0) ? rxcallp->error : RX_PROTOCOL_ERROR;
                    break;
                }
                first_read = 0;

                /* and adjust counters */
                length_found -= temp;
#endif /* USE_RX_IOVEC */
            }

            /* zero out remainder of last pages, in case we are
             * fetching past EOF.  We were fetching an integral #
             * of pages, but stopped, potentially in the middle of
             * a page.  Zero the remainder of that page, and then
             * all of the rest of the pages.
             */
#ifdef USE_RX_IOVEC
            rbytes = data_length - buffer_offset;
            bufferp = datap + buffer_offset;
#else /* USE_RX_IOVEC */
            /* bytes fetched */
	    osi_assertx((bufferp - datap) < LONG_MAX, "data >= LONG_MAX");
            rbytes = (long) (bufferp - datap);

            /* bytes left to zero */
            rbytes = data_length - rbytes;
#endif /* USE_RX_IOVEC */
            if (rbytes != 0)
                memset(bufferp, 0, rbytes);
        }

        if (code == 0) {
            if (call_was_64bit)
                code = EndRXAFS_FetchData64(rxcallp, &afsStatus, &callback, &volSync);
            else
                code = EndRXAFS_FetchData(rxcallp, &afsStatus, &callback, &volSync);
        } else {
            if (call_was_64bit)
                osi_Log1(afsd_logp, "CALL EndRXAFS_FetchData64 skipped due to error %d", code);
            else
                osi_Log1(afsd_logp, "CALL EndRXAFS_FetchData skipped due to error %d", code);
        }

        if (rxcallp)
            code1 = rx_EndCall(rxcallp, code);

        if (code1 == RXKADUNKNOWNKEY)
            osi_Log0(afsd_logp, "CALL EndCall returns RXKADUNKNOWNKEY");

        /* If we are avoiding a file server bug, ignore the error state */
        if (fs_fetchdata_offset_bug && first_read && length_found == 0 && code == -451) {
            /* Clone the current status info and clear the error state */
            scp_locked = cm_CloneStatus(scp, userp, scp_locked, &afsStatus, &volSync);
            if (scp_locked) {
                lock_ReleaseWrite(&scp->rw);
                scp_locked = 0;
            }
            code = 0;
        /* Prefer the error value from FetchData over rx_EndCall */
        } else if (code == 0 && code1 != 0)
            code = code1;
        osi_Log0(afsd_logp, "CALL FetchData DONE");

    } while (cm_Analyze(connp, userp, reqp, &scp->fid, 0, &volSync, NULL, NULL, code));

  fetchingcompleted:
    code = cm_MapRPCError(code, reqp);

    if (!scp_locked)
        lock_ObtainWrite(&scp->rw);

    if (code == 0)
        cm_MergeStatus(NULL, scp, &afsStatus, &volSync, userp, reqp, CM_MERGEFLAG_FETCHDATA);
    else
        InterlockedDecrement(&scp->activeRPCs);

    return code;
}
