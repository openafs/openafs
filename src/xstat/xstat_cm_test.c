/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Description:
 *	Test of the xstat_cm module.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>


#include "xstat_cm.h"		/*Interface for xstat_cm module */
#include <afs/cmd.h>		/*Command line interpreter */
#include <time.h>
#include <string.h>
#include <afs/afsutil.h>

/*
 * Command line parameter indices.
 *	P_CM_NAMES : List of CacheManager names.
 *	P_COLL_IDS : List of collection IDs to pick up.
 *	P_ONESHOT  : Are we gathering exactly one round of data?
 *	P_DEBUG    : Enable debugging output?
 */
#define P_CM_NAMES 	0
#define P_COLL_IDS 	1
#define P_ONESHOT  	2
#define P_FREQUENCY 	3
#define P_PERIOD 	4
#define P_DEBUG    	5

/*
 * Private globals.
 */
static int debugging_on = 0;	/*Are we debugging? */
static int one_shot = 0;	/*Single round of data collection? */

static char *fsOpNames[] = {
    "FetchData",
    "FetchACL",
    "FetchStatus",
    "StoreData",
    "StoreACL",
    "StoreStatus",
    "RemoveFile",
    "CreateFile",
    "Rename",
    "Symlink",
    "Link",
    "MakeDir",
    "RemoveDir",
    "SetLock",
    "ExtendLock",
    "ReleaseLock",
    "GetStatistics",
    "GiveUpCallbacks",
    "GetVolumeInfo",
    "GetVolumeStatus",
    "SetVolumeStatus",
    "GetRootVolume",
    "CheckToken",
    "GetTime",
    "NGetVolumeInfo",
    "BulkStatus",
    "XStatsVersion",
    "GetXStats",
    "XLookup",
    "ResidencyRpc"
};

static char *cmOpNames[] = {
    "CallBack",
    "InitCallBackState",
    "Probe",
    "GetLock",
    "GetCE",
    "XStatsVersion",
    "GetXStats"
};

static char *xferOpNames[] = {
    "FetchData",
    "StoreData"
};


/*------------------------------------------------------------------------
 * PrintCallInfo
 *
 * Description:
 *	Print out the AFSCB_XSTATSCOLL_PERF_INFO collection we just
 *	received.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_cm_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintCallInfo(void)
{				/*PrintCallInfo */

    int i;		/*Loop variable */
    int numInt32s;		/*# int32words returned */
    afs_int32 *currInt32;	/*Ptr to current afs_int32 value */
    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime = xstat_cm_Results.probeTime;
    /*
     * Just print out the results of the particular probe.
     */
    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    currInt32 = (afs_int32 *) (xstat_cm_Results.data.AFSCB_CollData_val);
    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';

    printf
	("AFSCB_XSTATSCOLL_CALL_INFO (coll %d) for CM %s\n[Poll %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    if (debugging_on)
	printf("\n[%u entries returned at %" AFS_PTR_FMT "]\n\n", numInt32s, currInt32);

    for (i = 0; i < numInt32s; i++)
	printf("%u ", *currInt32++);
    printf("\n");


}				/*PrintCallInfo */

/* Print detailed functional call statistics */

void
print_cmCallStats(void)
{
    char *printableTime;	/*Ptr to printable time string */
    afs_int32 nitems;
    struct afs_CMStats *cmp;
    time_t probeTime = xstat_cm_Results.probeTime;

    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';

    printf
	("AFSCB_XSTATSCOLL_CALL_INFO (coll %d) for CM %s\n[Probe %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    cmp = (struct afs_CMStats *)(xstat_cm_Results.data.AFSCB_CollData_val);
    nitems = xstat_cm_Results.data.AFSCB_CollData_len;

#define AFS_CS(call) \
    if (nitems > 0) { \
        printf("\t%10u %s\n", cmp->callInfo.C_ ## call, #call); \
        nitems--; \
    }

    AFS_CM_CALL_STATS
#undef AFS_CS
}


/*------------------------------------------------------------------------
 * PrintUpDownStats
 *
 * Description:
 *	Print the up/downtime stats for the given class of server records
 *	provided.
 *
 * Arguments:
 *	a_upDownP : Ptr to the server up/down info.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintUpDownStats(struct afs_stats_SrvUpDownInfo *a_upDownP)
{				/*PrintUpDownStats */

    /*
     * First, print the simple values.
     */
    printf("\t\t%10u numTtlRecords\n", a_upDownP->numTtlRecords);
    printf("\t\t%10u numUpRecords\n", a_upDownP->numUpRecords);
    printf("\t\t%10u numDownRecords\n", a_upDownP->numDownRecords);
    printf("\t\t%10u sumOfRecordAges\n", a_upDownP->sumOfRecordAges);
    printf("\t\t%10u ageOfYoungestRecord\n", a_upDownP->ageOfYoungestRecord);
    printf("\t\t%10u ageOfOldestRecord\n", a_upDownP->ageOfOldestRecord);
    printf("\t\t%10u numDowntimeIncidents\n",
	   a_upDownP->numDowntimeIncidents);
    printf("\t\t%10u numRecordsNeverDown\n", a_upDownP->numRecordsNeverDown);
    printf("\t\t%10u maxDowntimesInARecord\n",
	   a_upDownP->maxDowntimesInARecord);
    printf("\t\t%10u sumOfDowntimes\n", a_upDownP->sumOfDowntimes);
    printf("\t\t%10u shortestDowntime\n", a_upDownP->shortestDowntime);
    printf("\t\t%10u longestDowntime\n", a_upDownP->longestDowntime);

    /*
     * Now, print the array values.
     */
    printf("\t\tDowntime duration distribution:\n");
    printf("\t\t\t%8u: 0 min .. 10 min\n", a_upDownP->downDurations[0]);
    printf("\t\t\t%8u: 10 min .. 30 min\n", a_upDownP->downDurations[1]);
    printf("\t\t\t%8u: 30 min .. 1 hr\n", a_upDownP->downDurations[2]);
    printf("\t\t\t%8u: 1 hr .. 2 hr\n", a_upDownP->downDurations[3]);
    printf("\t\t\t%8u: 2 hr .. 4 hr\n", a_upDownP->downDurations[4]);
    printf("\t\t\t%8u: 4 hr .. 8 hr\n", a_upDownP->downDurations[5]);
    printf("\t\t\t%8u: > 8 hr\n", a_upDownP->downDurations[6]);

    printf("\t\tDowntime incident distribution:\n");
    printf("\t\t\t%8u: 0 times\n", a_upDownP->downIncidents[0]);
    printf("\t\t\t%8u: 1 time\n", a_upDownP->downIncidents[1]);
    printf("\t\t\t%8u: 2 .. 5 times\n", a_upDownP->downIncidents[2]);
    printf("\t\t\t%8u: 6 .. 10 times\n", a_upDownP->downIncidents[3]);
    printf("\t\t\t%8u: 10 .. 50 times\n", a_upDownP->downIncidents[4]);
    printf("\t\t\t%8u: > 50 times\n", a_upDownP->downIncidents[5]);

}				/*PrintUpDownStats */


/*------------------------------------------------------------------------
 * PrintOverallPerfInfo
 *
 * Description:
 *	Print out overall performance numbers.
 *
 * Arguments:
 *	a_ovP : Ptr to the overall performance numbers.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_cm_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintOverallPerfInfo(struct afs_stats_CMPerf *a_ovP)
{				/*PrintOverallPerfInfo */

    printf("\t%10u numPerfCalls\n", a_ovP->numPerfCalls);

    printf("\t%10u epoch\n", a_ovP->epoch);
    printf("\t%10u numCellsVisible\n", a_ovP->numCellsVisible);
    printf("\t%10u numCellsContacted\n", a_ovP->numCellsContacted);
    printf("\t%10u dlocalAccesses\n", a_ovP->dlocalAccesses);
    printf("\t%10u vlocalAccesses\n", a_ovP->vlocalAccesses);
    printf("\t%10u dremoteAccesses\n", a_ovP->dremoteAccesses);
    printf("\t%10u vremoteAccesses\n", a_ovP->vremoteAccesses);
    printf("\t%10u cacheNumEntries\n", a_ovP->cacheNumEntries);
    printf("\t%10u cacheBlocksTotal\n", a_ovP->cacheBlocksTotal);
    printf("\t%10u cacheBlocksInUse\n", a_ovP->cacheBlocksInUse);
    printf("\t%10u cacheBlocksOrig\n", a_ovP->cacheBlocksOrig);
    printf("\t%10u cacheMaxDirtyChunks\n", a_ovP->cacheMaxDirtyChunks);
    printf("\t%10u cacheCurrDirtyChunks\n", a_ovP->cacheCurrDirtyChunks);
    printf("\t%10u dcacheHits\n", a_ovP->dcacheHits);
    printf("\t%10u vcacheHits\n", a_ovP->vcacheHits);
    printf("\t%10u dcacheMisses\n", a_ovP->dcacheMisses);
    printf("\t%10u vcacheMisses\n", a_ovP->vcacheMisses);
    printf("\t%10u cacheFilesReused\n", a_ovP->cacheFilesReused);
    printf("\t%10u vcacheXAllocs\n", a_ovP->vcacheXAllocs);
    printf("\t%10u dcacheXAllocs\n", a_ovP->dcacheXAllocs);

    printf("\t%10u bufAlloced\n", a_ovP->bufAlloced);
    printf("\t%10u bufHits\n", a_ovP->bufHits);
    printf("\t%10u bufMisses\n", a_ovP->bufMisses);
    printf("\t%10u bufFlushDirty\n", a_ovP->bufFlushDirty);

    printf("\t%10u LargeBlocksActive\n", a_ovP->LargeBlocksActive);
    printf("\t%10u LargeBlocksAlloced\n", a_ovP->LargeBlocksAlloced);
    printf("\t%10u SmallBlocksActive\n", a_ovP->SmallBlocksActive);
    printf("\t%10u SmallBlocksAlloced\n", a_ovP->SmallBlocksAlloced);
    printf("\t%10u OutStandingMemUsage\n", a_ovP->OutStandingMemUsage);
    printf("\t%10u OutStandingAllocs\n", a_ovP->OutStandingAllocs);
    printf("\t%10u CallBackAlloced\n", a_ovP->CallBackAlloced);
    printf("\t%10u CallBackFlushes\n", a_ovP->CallBackFlushes);
    printf("\t%10u CallBackLoops\n", a_ovP->cbloops);

    printf("\t%10u srvRecords\n", a_ovP->srvRecords);
    printf("\t%10u srvNumBuckets\n", a_ovP->srvNumBuckets);
    printf("\t%10u srvMaxChainLength\n", a_ovP->srvMaxChainLength);
    printf("\t%10u srvMaxChainLengthHWM\n", a_ovP->srvMaxChainLengthHWM);
    printf("\t%10u srvRecordsHWM\n", a_ovP->srvRecordsHWM);

    printf("\t%10u cacheBucket0_Discarded\n",  a_ovP->cacheBucket0_Discarded);
    printf("\t%10u cacheBucket1_Discarded\n",  a_ovP->cacheBucket1_Discarded);
    printf("\t%10u cacheBucket2_Discarded\n",  a_ovP->cacheBucket2_Discarded);

    printf("\t%10u sysName_ID\n", a_ovP->sysName_ID);

    printf("\tFile Server up/downtimes, same cell:\n");
    PrintUpDownStats(&(a_ovP->fs_UpDown[0]));

    printf("\tFile Server up/downtimes, diff cell:\n");
    PrintUpDownStats(&(a_ovP->fs_UpDown[1]));

    printf("\tVL Server up/downtimes, same cell:\n");
    PrintUpDownStats(&(a_ovP->vl_UpDown[0]));

    printf("\tVL Server up/downtimes, diff cell:\n");
    PrintUpDownStats(&(a_ovP->vl_UpDown[1]));

}				/*PrintOverallPerfInfo */


/*------------------------------------------------------------------------
 * PrintPerfInfo
 *
 * Description:
 *	Print out the AFSCB_XSTATSCOLL_PERF_INFO collection we just
 *	received.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_cm_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintPerfInfo(void)
{				/*PrintPerfInfo */

    static afs_int32 perfInt32s = (sizeof(struct afs_stats_CMPerf) >> 2);	/*Correct # int32s to rcv */
    afs_int32 numInt32s;	/*# int32words received */
    struct afs_stats_CMPerf *perfP;	/*Ptr to performance stats */
    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime = xstat_cm_Results.probeTime;

    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numInt32s != perfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %u, got %u\n", perfInt32s, numInt32s);
	printf("** Version mismatch with Cache Manager\n");
	return;
    }

    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    perfP = (struct afs_stats_CMPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    printf
	("AFSCB_XSTATSCOLL_PERF_INFO (coll %d) for CM %s\n[Probe %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    PrintOverallPerfInfo(perfP);

}				/*PrintPerfInfo */


/*------------------------------------------------------------------------
 * PrintOpTiming
 *
 * Description:
 *	Print out the contents of an FS RPC op timing structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing number on.
 *	a_opNames : Ptr to table of operaton names.
 *	a_opTimeP : Ptr to the op timing structure to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintOpTiming(int a_opIdx, char *a_opNames[],
	      struct afs_stats_opTimingData *a_opTimeP)
{				/*PrintOpTiming */

    printf
	("%15s: %u ops (%u OK); sum=%lu.%06lu, sqr=%lu.%06lu, min=%lu.%06lu, max=%lu.%06lu\n",
	 a_opNames[a_opIdx], a_opTimeP->numOps, a_opTimeP->numSuccesses,
	 (long)a_opTimeP->sumTime.tv_sec, (long)a_opTimeP->sumTime.tv_usec,
	 (long)a_opTimeP->sqrTime.tv_sec, (long)a_opTimeP->sqrTime.tv_usec,
	 (long)a_opTimeP->minTime.tv_sec, (long)a_opTimeP->minTime.tv_usec,
	 (long)a_opTimeP->maxTime.tv_sec, (long)a_opTimeP->maxTime.tv_usec);

}				/*PrintOpTiming */


/*------------------------------------------------------------------------
 * PrintXferTiming
 *
 * Description:
 *	Print out the contents of a data transfer structure.
 *
 * Arguments:
 *	a_opIdx : Index of the AFS operation we're printing number on.
 *	a_opNames : Ptr to table of operation names.
 *	a_xferP : Ptr to the data transfer structure to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintXferTiming(int a_opIdx, char *a_opNames[],
		struct afs_stats_xferData *a_xferP)
{				/*PrintXferTiming */

    printf
	("%s: %u xfers (%u OK), time sum=%lu.%06lu, sqr=%lu.%06lu, min=%lu.%06lu, max=%lu.%06lu\n",
	 a_opNames[a_opIdx], a_xferP->numXfers, a_xferP->numSuccesses,
	 (long)a_xferP->sumTime.tv_sec, (long)a_xferP->sumTime.tv_usec,
	 (long)a_xferP->sqrTime.tv_sec, (long)a_xferP->sqrTime.tv_usec,
	 (long)a_xferP->minTime.tv_sec, (long)a_xferP->minTime.tv_usec,
	 (long)a_xferP->maxTime.tv_sec, (long)a_xferP->maxTime.tv_usec);
    printf("\t[bytes: sum=%u, min=%u, max=%u]\n", a_xferP->sumBytes,
	   a_xferP->minBytes, a_xferP->maxBytes);
    printf
	("\t[buckets: 0: %u, 1: %u, 2: %u, 3: %u, 4: %u, 5: %u, 6: %u, 7: %u, 8: %u]\n",
	 a_xferP->count[0], a_xferP->count[1], a_xferP->count[2],
	 a_xferP->count[3], a_xferP->count[4], a_xferP->count[5],
	 a_xferP->count[6], a_xferP->count[7], a_xferP->count[8]);


}				/*PrintXferTiming */


/*------------------------------------------------------------------------
 * PrintErrInfo
 *
 * Description:
 *	Print out the contents of an FS RPC error info structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing.
 *	a_opNames : Ptr to table of operation names.
 *	a_opErrP  : Ptr to the op timing structure to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintErrInfo(int a_opIdx, char *a_opNames[],
	     struct afs_stats_RPCErrors *a_opErrP)
{				/*PrintErrInfo */

    printf
	("%15s: %u server, %u network, %u prot, %u vol, %u busies, %u other\n",
	 a_opNames[a_opIdx], a_opErrP->err_Server, a_opErrP->err_Network,
	 a_opErrP->err_Protection, a_opErrP->err_Volume,
	 a_opErrP->err_VolumeBusies, a_opErrP->err_Other);

}				/*PrintErrInfo */


/*------------------------------------------------------------------------
 * PrintRPCPerfInfo
 *
 * Description:
 *	Print out a set of RPC performance numbers.
 *
 * Arguments:
 *	a_rpcP : Ptr to RPC perf numbers to print.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintRPCPerfInfo(struct afs_stats_RPCOpInfo *a_rpcP)
{				/*PrintRPCPerfInfo */

    int currIdx;		/*Loop variable */

    /*
     * Print the contents of each of the opcode-related arrays.
     */
    printf("FS Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	PrintOpTiming(currIdx, fsOpNames, &(a_rpcP->fsRPCTimes[currIdx]));

    printf("\nError Info:\n-----------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	PrintErrInfo(currIdx, fsOpNames, &(a_rpcP->fsRPCErrors[currIdx]));

    printf("\nTransfer timings:\n-----------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_XFER_OPS; currIdx++)
	PrintXferTiming(currIdx, xferOpNames,
			&(a_rpcP->fsXferTimes[currIdx]));

    printf("\nCM Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_CM_RPC_OPS; currIdx++)
	PrintOpTiming(currIdx, cmOpNames, &(a_rpcP->cmRPCTimes[currIdx]));

}				/*PrintRPCPerfInfo */


/*------------------------------------------------------------------------
 * PrintFullPerfInfo
 *
 * Description:
 *	Print out a set of full performance numbers.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintFullPerfInfo(void)
{				/*PrintFullPerfInfo */

    struct afs_stats_AuthentInfo *authentP;	/*Ptr to authentication stats */
    struct afs_stats_AccessInfo *accessinfP;	/*Ptr to access stats */
    static afs_int32 fullPerfInt32s = (sizeof(struct afs_stats_CMFullPerf) >> 2);	/*Correct #int32s */
    afs_int32 numInt32s;	/*# int32s actually received */
    struct afs_stats_CMFullPerf *fullP;	/*Ptr to full perf info */

    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime = xstat_cm_Results.probeTime;

    numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numInt32s != fullPerfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %u, got %u\n", fullPerfInt32s, numInt32s);
	printf("** Version mismatch with Cache Manager\n");
	return;
    }

    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    fullP = (struct afs_stats_CMFullPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    printf
	("AFSCB_XSTATSCOLL_FULL_PERF_INFO (coll %d) for CM %s\n[Probe %u, %s]\n\n",
	 xstat_cm_Results.collectionNumber, xstat_cm_Results.connP->hostName,
	 xstat_cm_Results.probeNum, printableTime);

    /*
     * Print the overall numbers first, followed by all of the RPC numbers,
     * then each of the other groupings.
     */
    printf("Overall Performance Info:\n-------------------------\n");
    PrintOverallPerfInfo(&(fullP->perf));
    printf("\n");
    PrintRPCPerfInfo(&(fullP->rpc));

    authentP = &(fullP->authent);
    printf("\nAuthentication info:\n--------------------\n");
    printf
	("\t%u PAGS, %u records (%u auth, %u unauth), %u max in PAG, chain max: %u\n",
	 authentP->curr_PAGs, authentP->curr_Records,
	 authentP->curr_AuthRecords, authentP->curr_UnauthRecords,
	 authentP->curr_MaxRecordsInPAG, authentP->curr_LongestChain);
    printf("\t%u PAG creations, %u tkt updates\n", authentP->PAGCreations,
	   authentP->TicketUpdates);
    printf("\t[HWMs: %u PAGS, %u records, %u max in PAG, chain max: %u]\n",
	   authentP->HWM_PAGs, authentP->HWM_Records,
	   authentP->HWM_MaxRecordsInPAG, authentP->HWM_LongestChain);

    accessinfP = &(fullP->accessinf);
    printf("\n[Un]replicated accesses:\n------------------------\n");
    printf
	("\t%u unrep, %u rep, %u reps accessed, %u max reps/ref, %u first OK\n\n",
	 accessinfP->unreplicatedRefs, accessinfP->replicatedRefs,
	 accessinfP->numReplicasAccessed, accessinfP->maxReplicasPerRef,
	 accessinfP->refFirstReplicaOK);

    /* There really isn't any authorship info
     * authorP = &(fullP->author); */

}				/*PrintFullPerfInfo */


/*------------------------------------------------------------------------
 * CM_Handler
 *
 * Description:
 *	Handler routine passed to the xstat_cm module.  This handler is
 *	called immediately after a poll of one of the Cache Managers has
 *	taken place.  All it needs to know is exported by the xstat_cm
 *	module, namely the data structure where the probe results are
 *	stored.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 on success,
 *	-1 otherwise.
 *
 * Environment:
 *	See above.  All we do now is print out what we got.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
CM_Handler(void)
{				/*CM_Handler */

    static char rn[] = "CM_Handler";	/*Routine name */

    printf("\n-----------------------------------------------------------\n");

    /*
     * If the probe failed, there isn't much we can do except gripe.
     */
    if (xstat_cm_Results.probeOK) {
	printf("%s: Probe %u, collection %d to CM on '%s' failed, code=%d\n",
	       rn, xstat_cm_Results.probeNum,
	       xstat_cm_Results.collectionNumber,
	       xstat_cm_Results.connP->hostName, xstat_cm_Results.probeOK);
	return (0);
    }

    if (debugging_on) {
        int i;
        int numInt32s = xstat_cm_Results.data.AFSCB_CollData_len;
        afs_int32 *entry = xstat_cm_Results.data.AFSCB_CollData_val;

        printf("debug: got collection number %d\n", xstat_cm_Results.collectionNumber);
        printf("debug: collection data length is %d\n", numInt32s);
        for (i = 0; i < numInt32s; i++) {
            printf("debug: entry %d %u\n", i, entry[i]);
        }
        printf("\n");
    }

    switch (xstat_cm_Results.collectionNumber) {
    case AFSCB_XSTATSCOLL_CALL_INFO:
	/* Why was this commented out in 3.3 ? */
	/* PrintCallInfo();  */
	print_cmCallStats();
	break;

    case AFSCB_XSTATSCOLL_PERF_INFO:
	/* we will do nothing here */
	/* PrintPerfInfo(); */
	break;

    case AFSCB_XSTATSCOLL_FULL_PERF_INFO:
	PrintFullPerfInfo();
	break;

    default:
	printf("** Unknown collection: %d\n",
	       xstat_cm_Results.collectionNumber);
    }

    /*
     * Return the happy news.
     */
    return (0);

}				/*CM_Handler */


/*------------------------------------------------------------------------
 * CountListItems
 *
 * Description:
 *	Given a pointer to the list of Cache Managers we'll be polling
 *	(or, in fact, any list at all), compute the length of the list.
 *
 * Arguments:
 *	struct cmd_item *a_firstItem : Ptr to first item in list.
 *
 * Returns:
 *	Length of the above list.
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

static int
CountListItems(struct cmd_item *a_firstItem)
{				/*CountListItems */

    int list_len;		/*List length */
    struct cmd_item *curr_item;	/*Ptr to current item */

    list_len = 0;
    curr_item = a_firstItem;

    /*
     * Count 'em up.
     */
    while (curr_item) {
	list_len++;
	curr_item = curr_item->next;
    }

    /*
     * Return our tally.
     */
    return (list_len);

}				/*CountListItems */


/*------------------------------------------------------------------------
 * RunTheTest
 *
 * Description:
 *	Routine called by the command line interpreter to execute the
 *	meat of the program.  We count the number of Cache Managers
 *	to watch, allocate enough space to remember all the connection
 *	info for them, then go for it.
 *
 *
 * Arguments:
 *	a_s : Ptr to the command line syntax descriptor.
 *
 * Returns:
 *	0, but may exit the whole program on an error!
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
RunTheTest(struct cmd_syndesc *a_s, void *arock)
{				/*RunTheTest */

    static char rn[] = "RunTheTest";	/*Routine name */
    int code;			/*Return code */
    int numCMs;			/*# Cache Managers to monitor */
    int numCollIDs;		/*# collections to fetch */
    int currCM;			/*Loop index */
    int currCollIDIdx;		/*Index of current collection ID */
    afs_int32 *collIDP;		/*Ptr to array of collection IDs */
    afs_int32 *currCollIDP;	/*Ptr to current collection ID */
    struct cmd_item *curr_item;	/*Current CM cmd line record */
    struct sockaddr_in *CMSktArray;	/*Cache Manager socket array */
    struct hostent *he;		/*Host entry */
    struct timeval tv;		/*Time structure */
    int sleep_secs;		/*Number of seconds to sleep */
    int initFlags;		/*Flags passed to the init fcn */
    int waitCode;		/*Result of LWP_WaitProcess() */
    int freq;			/*Frequency of polls */
    int period;			/*Time in minutes of data collection */

    /*
     * Are we doing one-shot measurements?
     */
    if (a_s->parms[P_ONESHOT].items != 0)
	one_shot = 1;

    /*
     * Are we doing debugging output?
     */
    if (a_s->parms[P_DEBUG].items != 0)
	debugging_on = 1;

    /*
     * Pull out the number of Cache Managers to watch and the number of
     * collections to get.
     */
    numCMs = CountListItems(a_s->parms[P_CM_NAMES].items);
    numCollIDs = CountListItems(a_s->parms[P_COLL_IDS].items);

    /* Get the polling frequency */
    if (a_s->parms[P_FREQUENCY].items != 0)
	freq = atoi(a_s->parms[P_FREQUENCY].items->data);
    else
	freq = 30;		/* default to 30 seconds */

    /* Get the time duration to run the tests */
    if (a_s->parms[P_PERIOD].items != 0)
	period = atoi(a_s->parms[P_PERIOD].items->data);
    else
	period = 10;		/* default to 10 minutes */

    /*
     * Allocate the socket array.
     */
    if (debugging_on)
	printf("%s: Allocating socket array for %d Cache Manager(s)\n", rn,
	       numCMs);
    CMSktArray = (struct sockaddr_in *)
	malloc(numCMs * sizeof(struct sockaddr_in));
    if (CMSktArray == (struct sockaddr_in *)0) {
	printf("%s: Can't allocate socket array for %d Cache Managers\n", rn,
	       numCMs);
	exit(1);
    }

    /*
     * Fill in the socket array for each of the Cache Managers listed.
     */
    curr_item = a_s->parms[P_CM_NAMES].items;
    for (currCM = 0; currCM < numCMs; currCM++) {
	CMSktArray[currCM].sin_family = AF_INET;
	CMSktArray[currCM].sin_port = htons(7001);	/* Cache Manager port */
	he = hostutil_GetHostByName(curr_item->data);
	if (he == NULL) {
	    fprintf(stderr, "[%s] Can't get host info for '%s'\n", rn,
		    curr_item->data);
	    exit(-1);
	}
	memcpy(&(CMSktArray[currCM].sin_addr.s_addr), he->h_addr, 4);

	/*
	 * Move to the next CM name.
	 */
	curr_item = curr_item->next;

    }				/*Get socket info for each Cache Manager */

    /*
     * Create and fill up the array of desired collection IDs.
     */
    if (debugging_on)
	printf("Allocating %d long(s) for coll ID\n", numCollIDs);
    collIDP = (afs_int32 *) (malloc(numCollIDs * sizeof(afs_int32)));
    currCollIDP = collIDP;
    curr_item = a_s->parms[P_COLL_IDS].items;
    for (currCollIDIdx = 0; currCollIDIdx < numCollIDs; currCollIDIdx++) {
	*currCollIDP = (afs_int32) (atoi(curr_item->data));
	if (debugging_on)
	    printf("CollID at index %d is %d\n", currCollIDIdx, *currCollIDP);
	curr_item = curr_item->next;
	currCollIDP++;
    };

    /*
     * Crank up the Cache Manager prober, then sit back and have fun.
     */
    printf("\nStarting up the xstat_cm service, ");
    initFlags = 0;
    if (debugging_on) {
	initFlags |= XSTAT_CM_INITFLAG_DEBUGGING;
	printf("debugging enabled, ");
    } else
	printf("no debugging, ");
    if (one_shot) {
	initFlags |= XSTAT_CM_INITFLAG_ONE_SHOT;
	printf("one-shot operation\n");
    } else
	printf("continuous operation\n");

    code = xstat_cm_Init(numCMs,	/*Num CMs */
			 CMSktArray,	/*File Server socket array */
			 freq,	/*Probe every 30 seconds */
			 CM_Handler,	/*Handler routine */
			 initFlags,	/*Initialization flags */
			 numCollIDs,	/*Number of collection IDs */
			 collIDP);	/*Ptr to collection ID array */
    if (code) {
	fprintf(stderr, "[%s] Error returned by xstat_cm_Init: %d\n", rn,
		code);
	xstat_cm_Cleanup(1);	/*Get rid of malloc'ed structures */
	exit(-1);
    }

    if (one_shot) {
	/*
	 * One-shot operation; just wait for the collection to be done.
	 */
	if (debugging_on)
	    printf("[%s] Calling LWP_WaitProcess() on event %" AFS_PTR_FMT
		   "\n", rn, &terminationEvent);
	waitCode = LWP_WaitProcess(&terminationEvent);
	if (debugging_on)
	    printf("[%s] Returned from LWP_WaitProcess()\n", rn);
	if (waitCode) {
	    if (debugging_on)
		fprintf(stderr,
			"[%s] Error %d encountered by LWP_WaitProcess()\n",
			rn, waitCode);
	}
    } else {
	/*
	 * Continuous operation.
	 */
	sleep_secs = 60 * period;	/*length of data collection */
	printf
	    ("xstat_cm service started, main thread sleeping for %d secs.\n",
	     sleep_secs);

	/*
	 * Let's just fall asleep for a while, then we'll clean up.
	 */
	tv.tv_sec = sleep_secs;
	tv.tv_usec = 0;
	code = IOMGR_Select(0,	/*Num fds */
			    0,	/*Descriptors ready for reading */
			    0,	/*Descriptors ready for writing */
			    0,	/*Descriptors with exceptional conditions */
			    &tv);	/*Timeout structure */
	if (code) {
	    fprintf(stderr,
		    "[%s] IOMGR_Select() returned non-zero value: %d\n", rn,
		    code);
	}
    }

    /*
     * We're all done.  Clean up, put the last nail in Rx, then
     * exit happily.
     */
    if (debugging_on)
	printf("\nYawn, main thread just woke up.  Cleaning things out...\n");
    code = xstat_cm_Cleanup(1);	/*Get rid of malloc'ed data */
    rx_Finalize();
    return (0);

}				/*RunTheTest */


#include "AFS_component_version_number.c"
int
main(int argc, char **argv)
{				/*Main routine */

    static char rn[] = "xstat_cm_test";	/*Routine name */
    afs_int32 code;	/*Return code */
    struct cmd_syndesc *ts;	/*Ptr to cmd line syntax desc */

    /*
     * Set up the commands we understand.
     */
    ts = cmd_CreateSyntax("initcmd", RunTheTest, NULL, "initialize the program");
    cmd_AddParm(ts, "-cmname", CMD_LIST, CMD_REQUIRED,
		"Cache Manager name(s) to monitor");
    cmd_AddParm(ts, "-collID", CMD_LIST, CMD_REQUIRED,
		"Collection(s) to fetch");
    cmd_AddParm(ts, "-onceonly", CMD_FLAG, CMD_OPTIONAL,
		"Collect results exactly once, then quit");
    cmd_AddParm(ts, "-frequency", CMD_SINGLE, CMD_OPTIONAL,
		"poll frequency, in seconds");
    cmd_AddParm(ts, "-period", CMD_SINGLE, CMD_OPTIONAL,
		"data collection time, in minutes");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL,
		"turn on debugging output");

    /*
     * Parse command-line switches & execute the test, then get the
     * heck out of here.
     */
    code = cmd_Dispatch(argc, argv);
    if (code) {
	fprintf(stderr, "[%s] Call to cmd_Dispatch() failed; code is %d\n",
		rn, code);
    }

    exit(code);

}				/*Main routine */
