/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* statistics-gathering package */

#include "../afs/param.h"       /*Should be always first*/
#include "../afs/sysincludes.h" /*Standard vendor system headers*/
#include "../afs/afsincludes.h" /*AFS-based standard headers*/
#include "afs_stats.h"

struct afs_CMStats afs_cmstats;
struct afs_stats_CMPerf afs_stats_cmperf;
struct afs_stats_CMFullPerf afs_stats_cmfullperf;
afs_int32 afs_stats_XferSumBytes[AFS_STATS_NUM_FS_XFER_OPS];



/*
 * afs_InitStats
 *
 * Description:
 *	Initialize all of the CM statistics structures.
 *
 * Parameters:
 *	None.
 *
 * Environment:
 *	This routine should only be called once, at initialization time.
 */
void afs_InitStats()

{ /*afs_InitStats*/

    struct afs_stats_opTimingData *opTimeP;	/*Ptr to curr timing struct*/
    struct afs_stats_xferData *xferP;		/*Ptr to curr xfer struct*/
    int currIdx;				/*Current index*/

    /*
     * First step is to zero everything out.
     */
    bzero((char *)(&afs_cmstats), sizeof(struct afs_CMStats));
    bzero((char *)(&afs_stats_cmperf), sizeof(struct afs_stats_CMPerf));
    bzero((char *)(&afs_stats_cmfullperf),
	  sizeof(struct afs_stats_CMFullPerf));

    /*
     * Some fields really should be non-zero at the start, so set 'em up.
     */
    afs_stats_cmperf.srvNumBuckets = NSERVERS;

    opTimeP = &(afs_stats_cmfullperf.rpc.fsRPCTimes[0]);
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++, opTimeP++)
	opTimeP->minTime.tv_sec = 999999;

    opTimeP = &(afs_stats_cmfullperf.rpc.cmRPCTimes[0]);
    for (currIdx = 0; currIdx < AFS_STATS_NUM_CM_RPC_OPS; currIdx++, opTimeP++)
	opTimeP->minTime.tv_sec = 999999;

    xferP = &(afs_stats_cmfullperf.rpc.fsXferTimes[0]);
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_XFER_OPS; currIdx++, xferP++) {
	xferP->minTime.tv_sec = 999999;
	xferP->minBytes = 999999999;
    }
    
} /*afs_InitStats*/



afs_GetCMStat(ptr, size)
     char **ptr; /* memory area */
     unsigned *size; /* # of bytes */
  {
#ifndef AFS_NOSTATS
      AFS_STATCNT(afs_GetCMStat);
      *ptr = (char *)&afs_cmstats;
      *size = sizeof(afs_cmstats);
#endif /* AFS_NOSTATS */
  }

afs_AddToMean(oldMean, newValue)
     struct afs_MeanStats *oldMean;
     afs_int32 newValue;
  {
      AFS_STATCNT(afs_AddToMean);
  }
