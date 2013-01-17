/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <osi.h>
#include "afsd.h"

/*
 * called with scp->rw lock held exclusive
 */
afs_int32
raw_ReadData( cm_scache_t *scp, osi_hyper_t *offsetp,
              afs_uint32 length, char *bufferp, afs_uint32 *readp,
              cm_user_t *userp, cm_req_t *reqp)
{
    long code;
    cm_buf_t *bufp = NULL;
    osi_hyper_t fileLength;
    osi_hyper_t thyper;
    osi_hyper_t lastByte;
    osi_hyper_t bufferOffset;
    long bufIndex, nbytes;
    int sequential = 0;

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code)
        goto done;

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* now we have the entry locked, look up the length */
    fileLength = scp->length;

    /* adjust length down so that it won't go past EOF */
    thyper.LowPart = length;
    thyper.HighPart = 0;
    thyper = LargeIntegerAdd(*offsetp, thyper);	/* where read should end */
    lastByte = thyper;
    if (LargeIntegerGreaterThan(thyper, fileLength)) {
        /* we'd read past EOF, so just stop at fileLength bytes.
        * Start by computing how many bytes remain in the file.
        */
        thyper = LargeIntegerSubtract(fileLength, *offsetp);

        /* if we are past EOF, read 0 bytes */
        if (LargeIntegerLessThanZero(thyper))
            length = 0;
        else
            length = thyper.LowPart;
    }

    *readp = length;

    /* now, copy the data one buffer at a time,
     * until we've filled the request packet
     */
    while (1) {
        /* if we've copied all the data requested, we're done */
        if (length <= 0) break;

        /* otherwise, load up a buffer of data */
        thyper.HighPart = offsetp->HighPart;
        thyper.LowPart = offsetp->LowPart & ~(cm_data.blockSize-1);
        if (!bufp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
            /* wrong buffer */
            if (bufp) {
                buf_Release(bufp);
                bufp = NULL;
            }
            lock_ReleaseWrite(&scp->rw);

            code = buf_Get(scp, &thyper, reqp, &bufp);

            lock_ObtainWrite(&scp->rw);
            if (code) goto done;
            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                code = cm_SyncOp(scp, bufp, userp, reqp, 0,
                                 CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);
                if (code)
                    goto done;

		cm_SyncOpDone(scp, bufp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);

                if (cm_HaveBuffer(scp, bufp, 0)) break;

                /* otherwise, load the buffer and try again */
                code = cm_GetBuffer(scp, bufp, NULL, userp, reqp);
                if (code) break;
            }
            if (code) {
                buf_Release(bufp);
                bufp = NULL;
                goto done;
            }
        }	/* if (wrong buffer) ... */

        /* now we have the right buffer loaded.  Copy out the
         * data from here to the user's buffer.
         */
        bufIndex = offsetp->LowPart & (cm_data.blockSize - 1);

        /* and figure out how many bytes we want from this buffer */
        nbytes = cm_data.blockSize - bufIndex;	/* what remains in buffer */
        if (nbytes > length) nbytes = length;	/* don't go past EOF */

        /* now copy the data */
        memcpy(bufferp, bufp->datap + bufIndex, nbytes);

        /* adjust lengthers, pointers, etc. */
        bufferp += nbytes;
        length -= nbytes;
        thyper.LowPart = nbytes;
        thyper.HighPart = 0;
        *offsetp = LargeIntegerAdd(thyper, *offsetp);
    } /* while 1 */

  done:
    if (bufp)
        buf_Release(bufp);

    if (code == 0 && sequential)
        cm_ConsiderPrefetch(scp, &lastByte, *readp, userp, reqp);

    return code;
}


/*
 * called with scp->rw lock held exclusive
 */

afs_uint32
raw_WriteData( cm_scache_t *scp, osi_hyper_t *offsetp, afs_uint32 length, char *bufferp,
               cm_user_t *userp, cm_req_t *reqp, afs_uint32 *writtenp)
{
    long code = 0;
    long written = 0;
    osi_hyper_t fileLength;	/* file's length at start of write */
    osi_hyper_t minLength;	/* don't read past this */
    afs_uint32 nbytes;		/* # of bytes to transfer this iteration */
    cm_buf_t *bufp = NULL;
    osi_hyper_t thyper;		/* hyper tmp variable */
    osi_hyper_t bufferOffset;
    afs_uint32 bufIndex;	/* index in buffer where our data is */
    int doWriteBack = 0;
    osi_hyper_t writeBackOffset;/* offset of region to write back when I/O is done */
    afs_uint32 writeBackLength;
    DWORD filter = 0;

    /* start by looking up the file's end */
    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                     CM_SCACHESYNC_NEEDCALLBACK
                      | CM_SCACHESYNC_SETSTATUS
                      | CM_SCACHESYNC_GETSTATUS);
    if (code)
        goto done;

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_SETSTATUS | CM_SCACHESYNC_GETSTATUS);

    /* now we have the entry locked, look up the length */
    fileLength = scp->length;
    minLength = fileLength;
    if (LargeIntegerGreaterThan(minLength, scp->serverLength))
        minLength = scp->serverLength;

    /* adjust file length if we extend past EOF */
    thyper.LowPart = length;
    thyper.HighPart = 0;
    thyper = LargeIntegerAdd(*offsetp, thyper);	/* where write should end */
    if (LargeIntegerGreaterThan(thyper, fileLength)) {
        /* we'd write past EOF, so extend the file */
        scp->mask |= CM_SCACHEMASK_LENGTH;
        scp->length = thyper;
        filter |= (FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE);
    } else
        filter |= FILE_NOTIFY_CHANGE_LAST_WRITE;

    doWriteBack = 1;
    writeBackOffset = *offsetp;
    writeBackLength = length;

    /* now, copy the data one buffer at a time, until we've filled the
     * request packet */
    while (1) {
        /* if we've copied all the data requested, we're done */
        if (length <= 0) break;

        /* otherwise, load up a buffer of data */
        thyper.HighPart = offsetp->HighPart;
        thyper.LowPart = offsetp->LowPart & ~(cm_data.blockSize-1);
        if (!bufp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
            /* wrong buffer */
            if (bufp) {
                lock_ReleaseMutex(&bufp->mx);
                buf_Release(bufp);
                bufp = NULL;
            }
            lock_ReleaseWrite(&scp->rw);

            code = buf_Get(scp, &thyper, reqp, &bufp);
            if (bufp)
                lock_ObtainMutex(&bufp->mx);

            lock_ObtainWrite(&scp->rw);
            if (code)
                goto done;

            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                code = cm_SyncOp(scp, bufp, userp, reqp, 0,
                                  CM_SCACHESYNC_NEEDCALLBACK
                                  | CM_SCACHESYNC_WRITE
                                  | CM_SCACHESYNC_BUFLOCKED);
                if (code)
                    goto done;

		cm_SyncOpDone(scp, bufp,
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
                if (LargeIntegerGreaterThanOrEqualTo(bufp->offset, minLength) ||
                     LargeIntegerEqualTo(*offsetp, bufp->offset) &&
                     (length >= cm_data.blockSize ||
                       LargeIntegerGreaterThanOrEqualTo(LargeIntegerAdd(*offsetp, ConvertLongToLargeInteger(length)), minLength))) {
                    if (length < cm_data.blockSize
                         && bufp->dataVersion == CM_BUF_VERSION_BAD)
                        memset(bufp->datap, 0,
                                cm_data.blockSize);
                    bufp->dataVersion = scp->dataVersion;
                }

                if (cm_HaveBuffer(scp, bufp, 1)) break;

                /* otherwise, load the buffer and try again */
                lock_ReleaseMutex(&bufp->mx);
                code = cm_GetBuffer(scp, bufp, NULL, userp, reqp);
                lock_ReleaseWrite(&scp->rw);
                lock_ObtainMutex(&bufp->mx);
                lock_ObtainWrite(&scp->rw);
                if (code)
                    break;
            }
            if (code) {
                lock_ReleaseMutex(&bufp->mx);
                buf_Release(bufp);
                bufp = NULL;
                goto done;
            }
        }	/* if (wrong buffer) ... */

        /* now we have the right buffer loaded.  Copy out the
         * data from here to the user's buffer.
         */
        bufIndex = offsetp->LowPart & (cm_data.blockSize - 1);

        /* and figure out how many bytes we want from this buffer */
        nbytes = cm_data.blockSize - bufIndex;	/* what remains in buffer */
        if (nbytes > length)
            nbytes = length;	/* don't go past end of request */

        /* now copy the data */
	memcpy(bufp->datap + bufIndex, bufferp, nbytes);
        buf_SetDirty(bufp, reqp, bufIndex, nbytes, userp);

        /* adjust lengthers, pointers, etc. */
        bufferp += nbytes;
        length -= nbytes;
        written += nbytes;
        thyper.LowPart = nbytes;
        thyper.HighPart = 0;
        *offsetp = LargeIntegerAdd(thyper, *offsetp);
    } /* while 1 */

  done:
    if (writtenp)
        *writtenp = written;

    if (bufp) {
        lock_ReleaseMutex(&bufp->mx);
        buf_Release(bufp);
    }

    if (code == 0 && doWriteBack) {
        rock_BkgStore_t *rockp = malloc(sizeof(*rockp));

        if (rockp) {
            code = cm_SyncOp(scp, NULL, userp, reqp, 0, CM_SCACHESYNC_ASYNCSTORE);
            if (code == 0) {
                rockp->length = writeBackLength;
                rockp->offset = writeBackOffset;

                cm_QueueBKGRequest(scp, cm_BkgStore, rockp, userp, reqp);

                /* rock is freed by cm_BkgDaemon */
            } else {
                free(rockp);
            }
        }
    }

    /* cm_SyncOpDone is called when cm_BkgStore completes */
    return code;
}


