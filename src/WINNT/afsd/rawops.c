/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi.h>
#include "afsd.h"

#include "afsdifs.h"

#define CM_BUF_SIZE		4096
long buf_bufferSize = CM_BUF_SIZE;

long ReadData(cm_scache_t *scp, osi_hyper_t offset, long count, char *op,
              cm_user_t *userp, long *readp)
{
    //osi_hyper_t offset;
    long code;
    cm_buf_t *bufferp;
    osi_hyper_t fileLength;
    osi_hyper_t thyper;
    osi_hyper_t lastByte;
    osi_hyper_t bufferOffset;
    long bufIndex, nbytes;
    int sequential = 0;
    cm_req_t req;

    cm_InitReq(&req);

    bufferp = NULL;

    lock_ObtainMutex(&scp->mx);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) 
        goto done;

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* now we have the entry locked, look up the length */
    fileLength = scp->length;

    /* adjust count down so that it won't go past EOF */
    thyper.LowPart = count;
    thyper.HighPart = 0;
    thyper = LargeIntegerAdd(offset, thyper);	/* where read should end */
    lastByte = thyper;
    if (LargeIntegerGreaterThan(thyper, fileLength)) {
        /* we'd read past EOF, so just stop at fileLength bytes.
        * Start by computing how many bytes remain in the file.
        */
        thyper = LargeIntegerSubtract(fileLength, offset);

        /* if we are past EOF, read 0 bytes */
        if (LargeIntegerLessThanZero(thyper))
            count = 0;
        else
            count = thyper.LowPart;
    }       

    *readp = count;

    /* now, copy the data one buffer at a time,
     * until we've filled the request packet
     */
    while (1) {
        /* if we've copied all the data requested, we're done */
        if (count <= 0) break;

        /* otherwise, load up a buffer of data */
        thyper.HighPart = offset.HighPart;
        thyper.LowPart = offset.LowPart & ~(buf_bufferSize-1);
        if (!bufferp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
            /* wrong buffer */
            if (bufferp) {
                buf_Release(bufferp);
                bufferp = NULL;
            }
            lock_ReleaseMutex(&scp->mx);

            lock_ObtainRead(&scp->bufCreateLock);
            code = buf_Get(scp, &thyper, &bufferp);
            lock_ReleaseRead(&scp->bufCreateLock);

            lock_ObtainMutex(&scp->mx);
            if (code) goto done;
            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                code = cm_SyncOp(scp, bufferp, userp, &req, 0,
                                  CM_SCACHESYNC_NEEDCALLBACK
                                  | CM_SCACHESYNC_READ);
                if (code) 
                    goto done;

		cm_SyncOpDone(scp, bufferp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);

                if (cm_HaveBuffer(scp, bufferp, 0)) break;

                /* otherwise, load the buffer and try again */
                code = cm_GetBuffer(scp, bufferp, NULL, userp, &req);
                if (code) break;
            }
            if (code) {
                buf_Release(bufferp);
                bufferp = NULL;
                goto done;
            }
        }	/* if (wrong buffer) ... */

        /* now we have the right buffer loaded.  Copy out the
         * data from here to the user's buffer.
         */
        bufIndex = offset.LowPart & (buf_bufferSize - 1);

        /* and figure out how many bytes we want from this buffer */
        nbytes = buf_bufferSize - bufIndex;	/* what remains in buffer */
        if (nbytes > count) nbytes = count;	/* don't go past EOF */

        /* now copy the data */
        memcpy(op, bufferp->datap + bufIndex, nbytes);

        /* adjust counters, pointers, etc. */
        op += nbytes;
        count -= nbytes;
        thyper.LowPart = nbytes;
        thyper.HighPart = 0;
        offset = LargeIntegerAdd(thyper, offset);
    } /* while 1 */

  done:
    lock_ReleaseMutex(&scp->mx);
    //lock_ReleaseMutex(&fidp->mx);
    if (bufferp) 
        buf_Release(bufferp);

    if (code == 0 && sequential)
        cm_ConsiderPrefetch(scp, &lastByte, userp, &req);

    return code;
}       


long WriteData(cm_scache_t *scp, osi_hyper_t offset, long count, char *op,
               cm_user_t *userp, long *writtenp)
{
    long code = 0;
    long written = 0;
    osi_hyper_t fileLength;	/* file's length at start of write */
    osi_hyper_t minLength;	/* don't read past this */
    long nbytes;		/* # of bytes to transfer this iteration */
    cm_buf_t *bufferp;
    osi_hyper_t thyper;		/* hyper tmp variable */
    osi_hyper_t bufferOffset;
    long bufIndex;			/* index in buffer where our data is */
    int doWriteBack;
    osi_hyper_t writeBackOffset;	/* offset of region to write back when
    * I/O is done */
    DWORD filter = 0;
    cm_req_t req;

    cm_InitReq(&req);

    bufferp = NULL;
    doWriteBack = 0;

    lock_ObtainMutex(&scp->mx);

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK
                      | CM_SCACHESYNC_SETSTATUS
                      | CM_SCACHESYNC_GETSTATUS);
    if (code) 
        goto done;
    
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_SETSTATUS | CM_SCACHESYNC_GETSTATUS);

#if 0
    /* make sure we have a writable FD */
    if (!(fidp->flags & SMB_FID_OPENWRITE)) {
    code = CM_ERROR_BADFDOP;
    goto done;
    }
#endif
	
    /* now we have the entry locked, look up the length */
    fileLength = scp->length;
    minLength = fileLength;
    if (LargeIntegerGreaterThan(minLength, scp->serverLength))
        minLength = scp->serverLength;

    /* adjust file length if we extend past EOF */
    thyper.LowPart = count;
    thyper.HighPart = 0;
    thyper = LargeIntegerAdd(offset, thyper);	/* where write should end */
    if (LargeIntegerGreaterThan(thyper, fileLength)) {
        /* we'd write past EOF, so extend the file */
        scp->mask |= CM_SCACHEMASK_LENGTH;
        scp->length = thyper;
        filter |= (FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
    } else
        filter |= FILE_NOTIFY_CHANGE_LAST_WRITE;

    /* now, if the new position (thyper) and the old (offset) are in
     * different storeback windows, remember to store back the previous
     * storeback window when we're done with the write.
     */
    if ((thyper.LowPart & (-cm_chunkSize)) !=
         (offset.LowPart & (-cm_chunkSize))) {
        /* they're different */
        doWriteBack = 1;
        writeBackOffset.HighPart = offset.HighPart;
        writeBackOffset.LowPart = offset.LowPart & (-cm_chunkSize);
    }

    *writtenp = count;

    /* now, copy the data one buffer at a time, until we've filled the
     * request packet */
    while (1) {
        /* if we've copied all the data requested, we're done */
        if (count <= 0) break;

        /* handle over quota or out of space */
        if (scp->flags & (CM_SCACHEFLAG_OVERQUOTA | CM_SCACHEFLAG_OUTOFSPACE)) {
            *writtenp = written;
            break;
        }

        /* otherwise, load up a buffer of data */
        thyper.HighPart = offset.HighPart;
        thyper.LowPart = offset.LowPart & ~(buf_bufferSize-1);
        if (!bufferp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
            /* wrong buffer */
            if (bufferp) {
                lock_ReleaseMutex(&bufferp->mx);
                buf_Release(bufferp);
                bufferp = NULL;
            }	
            lock_ReleaseMutex(&scp->mx);

            lock_ObtainRead(&scp->bufCreateLock);
            code = buf_Get(scp, &thyper, &bufferp);
            lock_ReleaseRead(&scp->bufCreateLock);

            lock_ObtainMutex(&bufferp->mx);
            lock_ObtainMutex(&scp->mx);
            if (code) 
                goto done;

            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                code = cm_SyncOp(scp, bufferp, userp, &req, 0,
                                  CM_SCACHESYNC_NEEDCALLBACK
                                  | CM_SCACHESYNC_WRITE
                                  | CM_SCACHESYNC_BUFLOCKED);
                if (code) 
                    goto done;
                       
		cm_SyncOpDone(scp, bufferp, 
			       CM_SCACHESYNC_NEEDCALLBACK 
			       | CM_SCACHESYNC_WRITE 
			       | CM_SCACHESYNC_BUFLOCKED);

                /* If we're overwriting the entire buffer, or
                 * if we're writing at or past EOF, mark the
                 * buffer as current so we don't call
                 * cm_GetBuffer.  This skips the fetch from the
                 * server in those cases where we're going to 
                 * obliterate all the data in the buffer anyway,
                 * or in those cases where there is no useful
                 * data at the server to start with.
                 *
                 * Use minLength instead of scp->length, since
                 * the latter has already been updated by this
                 * call.
                 */
                if (LargeIntegerGreaterThanOrEqualTo(bufferp->offset, minLength) ||
                     LargeIntegerEqualTo(offset, bufferp->offset) &&
                     (count >= buf_bufferSize ||
                       LargeIntegerGreaterThanOrEqualTo(LargeIntegerAdd(offset, ConvertLongToLargeInteger(count)), minLength))) {
                    if (count < buf_bufferSize
                         && bufferp->dataVersion == -1)
                        memset(bufferp->datap, 0,
                                buf_bufferSize);
                    bufferp->dataVersion = scp->dataVersion;
                }

                if (cm_HaveBuffer(scp, bufferp, 1)) break;

                /* otherwise, load the buffer and try again */
                lock_ReleaseMutex(&bufferp->mx);
                code = cm_GetBuffer(scp, bufferp, NULL, userp,
                                     &req);
                lock_ReleaseMutex(&scp->mx);
                lock_ObtainMutex(&bufferp->mx);
                lock_ObtainMutex(&scp->mx);
                if (code) 
                    break;
            }
            if (code) {
                lock_ReleaseMutex(&bufferp->mx);
                buf_Release(bufferp);
                bufferp = NULL;
                goto done;
            }
        }	/* if (wrong buffer) ... */

        /* now we have the right buffer loaded.  Copy out the
         * data from here to the user's buffer.
         */
        bufIndex = offset.LowPart & (buf_bufferSize - 1);

        /* and figure out how many bytes we want from this buffer */
        nbytes = buf_bufferSize - bufIndex;	/* what remains in buffer */
        if (nbytes > count) 
            nbytes = count;	/* don't go past end of request */

        /* now copy the data */
	memcpy(bufferp->datap + bufIndex, op, nbytes);
        buf_SetDirty(bufferp);

        /* and record the last writer */
        if (bufferp->userp != userp) {
            cm_HoldUser(userp);
            if (bufferp->userp) 
                cm_ReleaseUser(bufferp->userp);
            bufferp->userp = userp;
        }

        /* adjust counters, pointers, etc. */
        op += nbytes;
        count -= nbytes;
        written += nbytes;
        thyper.LowPart = nbytes;
        thyper.HighPart = 0;
        offset = LargeIntegerAdd(thyper, offset);
    } /* while 1 */

  done:
    lock_ReleaseMutex(&scp->mx);
    if (bufferp) {
        lock_ReleaseMutex(&bufferp->mx);
        buf_Release(bufferp);
    }

#if 0
    if (code == 0 /* && filter != 0 && (fidp->flags & SMB_FID_NTOPEN)
        && (fidp->NTopen_dscp->flags & CM_SCACHEFLAG_ANYWATCH)*/) {
        smb_NotifyChange(FILE_ACTION_MODIFIED, filter,
                         fidp->NTopen_dscp, fidp->NTopen_pathp,
                         NULL, TRUE);
    }
#endif

    if (code == 0 && doWriteBack) {
        lock_ObtainMutex(&scp->mx);
        cm_SyncOp(scp, NULL, userp, &req, 0, CM_SCACHESYNC_ASYNCSTORE);
        lock_ReleaseMutex(&scp->mx);
        cm_QueueBKGRequest(scp, cm_BkgStore, writeBackOffset.LowPart,
                            writeBackOffset.HighPart, cm_chunkSize, 0, userp);
    }   

    /* cm_SyncOpDone is called when cm_BkgStore completes */
    return code;
}


