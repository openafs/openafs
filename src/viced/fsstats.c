/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* An abstracted interface for recording fs statistics data */

#include <afsconfig.h>
#include <afs/param.h>

#include <assert.h>
#include <roken.h>

#include <afs/opr.h>
#include <afs/afsint.h>
#include <afs/ihandle.h>
#include <afs/nfs.h>
#include "viced.h"
#include "fs_stats.h"


void
fsstats_StartOp(struct fsstats *stats, int index)
{
    assert(index >= 0 && index < FS_STATS_NUM_RPC_OPS);
    stats->opP = &(afs_FullPerfStats.det.rpcOpTimes[index]);
    FS_LOCK;
    (stats->opP->numOps)++;
    FS_UNLOCK;
    gettimeofday(&stats->opStartTime, NULL);
}

void
fsstats_FinishOp(struct fsstats *stats, int code)
{
    struct timeval opStopTime, elapsedTime;

    gettimeofday(&opStopTime, NULL);
    if (code == 0) {
	FS_LOCK;
	(stats->opP->numSuccesses)++;
	fs_stats_GetDiff(elapsedTime, stats->opStartTime, opStopTime);
	fs_stats_AddTo((stats->opP->sumTime), elapsedTime);
	fs_stats_SquareAddTo((stats->opP->sqrTime), elapsedTime);
	if (fs_stats_TimeLessThan(elapsedTime, (stats->opP->minTime))) {
	    fs_stats_TimeAssign((stats->opP->minTime), elapsedTime);
	}
	if (fs_stats_TimeGreaterThan(elapsedTime, (stats->opP->maxTime))) {
	    fs_stats_TimeAssign((stats->opP->maxTime), elapsedTime);
	}
	FS_UNLOCK;
    }
}

void
fsstats_StartXfer(struct fsstats *stats, int index)
{
    assert(index >= 0 && index < FS_STATS_NUM_XFER_OPS);
    gettimeofday(&stats->xferStartTime, NULL);
    stats->xferP = &(afs_FullPerfStats.det.xferOpTimes[index]);
}

void
fsstats_FinishXfer(struct fsstats *stats, int code,
		   afs_sfsize_t bytesToXfer, afs_sfsize_t bytesXferred,
		   int *remainder)
{
    struct timeval xferStopTime;
    struct timeval elapsedTime;

    /*
     * At this point, the data transfer is done, for good or ill.  Remember
     * when the transfer ended, bump the number of successes/failures, and
     * integrate the transfer size and elapsed time into the stats.  If the
     * operation failed, we jump to the appropriate point.
     */
    gettimeofday(&xferStopTime, 0);
    FS_LOCK;
    (stats->xferP->numXfers)++;
    if (code == 0) {
	(stats->xferP->numSuccesses)++;

	/*
	 * Bump the xfer sum by the number of bytes actually sent, NOT the
	 * target number.
	 */
	*remainder += bytesXferred;
	(stats->xferP->sumBytes) += (*remainder >> 10);
	*remainder &= 0x3FF;
	if (bytesXferred < stats->xferP->minBytes)
	    stats->xferP->minBytes = bytesXferred;
	if (bytesXferred > stats->xferP->maxBytes)
	    stats->xferP->maxBytes = bytesXferred;

	/*
	 * Tally the size of the object.  Note: we tally the actual size,
	 * NOT the number of bytes that made it out over the wire.
	 */
	if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET0)
	    (stats->xferP->count[0])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET1)
	    (stats->xferP->count[1])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET2)
	    (stats->xferP->count[2])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET3)
	    (stats->xferP->count[3])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET4)
	    (stats->xferP->count[4])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET5)
	    (stats->xferP->count[5])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET6)
	    (stats->xferP->count[6])++;
	else if (bytesToXfer <= FS_STATS_MAXBYTES_BUCKET7)
	    (stats->xferP->count[7])++;
	else
	    (stats->xferP->count[8])++;

	fs_stats_GetDiff(elapsedTime, stats->xferStartTime, xferStopTime);
	fs_stats_AddTo((stats->xferP->sumTime), elapsedTime);
	fs_stats_SquareAddTo((stats->xferP->sqrTime), elapsedTime);
	if (fs_stats_TimeLessThan(elapsedTime, (stats->xferP->minTime))) {
	    fs_stats_TimeAssign((stats->xferP->minTime), elapsedTime);
	}
	if (fs_stats_TimeGreaterThan(elapsedTime, (stats->xferP->maxTime))) {
	    fs_stats_TimeAssign((stats->xferP->maxTime), elapsedTime);
	}
    }
    FS_UNLOCK;
}
