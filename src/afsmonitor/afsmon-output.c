/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Module:	afsmon-output.c
 *		Outputs the xstat probe results to a file
 *		Most of this code is taken from xstat_fs_test.c and
 *		xstat_cm_test.c
 *
 *-------------------------------------------------------------------------*/

#include <stdio.h>
#include <time.h>
#include <afsconfig.h>
#include <afs/param.h>
#include <string.h>

RCSID
    ("$Header$");

#include <afs/xstat_fs.h>
#include <afs/xstat_cm.h>



/* Extern Variables */
extern int afsmon_debug;	/* debugging on ? */
extern FILE *debugFD;		/* debug file FD */
extern char errMsg[256];	/* error message buffer */

extern int afsmon_Exit();	/* exit routine */

static FILE *fs_outFD;		/* fs output file descriptor */
static FILE *cm_outFD;		/* cm output file descriptor */

/* structures used by FS & CM stats print routines */

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
    "GetXStats"
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

/*________________________________________________________________________
				FS STATS ROUTINES 
 *_______________________________________________________________________*/

/*------------------------------------------------------------------------
 * Print_fs_OverallPerfInfo
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
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
Print_fs_OverallPerfInfo(a_ovP)
     struct afs_PerfStats *a_ovP;

{				/*Print_fs_OverallPerfInfo */

    fprintf(fs_outFD, "\t%10d numPerfCalls\n\n", a_ovP->numPerfCalls);


    /*
     * Vnode cache section.
     */
    fprintf(fs_outFD, "\t%10d vcache_L_Entries\n", a_ovP->vcache_L_Entries);
    fprintf(fs_outFD, "\t%10d vcache_L_Allocs\n", a_ovP->vcache_L_Allocs);
    fprintf(fs_outFD, "\t%10d vcache_L_Gets\n", a_ovP->vcache_L_Gets);
    fprintf(fs_outFD, "\t%10d vcache_L_Reads\n", a_ovP->vcache_L_Reads);
    fprintf(fs_outFD, "\t%10d vcache_L_Writes\n\n", a_ovP->vcache_L_Writes);

    fprintf(fs_outFD, "\t%10d vcache_S_Entries\n", a_ovP->vcache_S_Entries);
    fprintf(fs_outFD, "\t%10d vcache_S_Allocs\n", a_ovP->vcache_S_Allocs);
    fprintf(fs_outFD, "\t%10d vcache_S_Gets\n", a_ovP->vcache_S_Gets);
    fprintf(fs_outFD, "\t%10d vcache_S_Reads\n", a_ovP->vcache_S_Reads);
    fprintf(fs_outFD, "\t%10d vcache_S_Writes\n\n", a_ovP->vcache_S_Writes);

    fprintf(fs_outFD, "\t%10d vcache_H_Entries\n", a_ovP->vcache_H_Entries);
    fprintf(fs_outFD, "\t%10d vcache_H_Gets\n", a_ovP->vcache_H_Gets);
    fprintf(fs_outFD, "\t%10d vcache_H_Replacements\n\n",
	    a_ovP->vcache_H_Replacements);

    /*
     * Directory package section.
     */
    fprintf(fs_outFD, "\t%10d dir_Buffers\n", a_ovP->dir_Buffers);
    fprintf(fs_outFD, "\t%10d dir_Calls\n", a_ovP->dir_Calls);
    fprintf(fs_outFD, "\t%10d dir_IOs\n\n", a_ovP->dir_IOs);

    /*
     * Rx section.
     */
    fprintf(fs_outFD, "\t%10d rx_packetRequests\n", a_ovP->rx_packetRequests);
    fprintf(fs_outFD, "\t%10d rx_noPackets_RcvClass\n",
	    a_ovP->rx_noPackets_RcvClass);
    fprintf(fs_outFD, "\t%10d rx_noPackets_SendClass\n",
	    a_ovP->rx_noPackets_SendClass);
    fprintf(fs_outFD, "\t%10d rx_noPackets_SpecialClass\n",
	    a_ovP->rx_noPackets_SpecialClass);
    fprintf(fs_outFD, "\t%10d rx_socketGreedy\n", a_ovP->rx_socketGreedy);
    fprintf(fs_outFD, "\t%10d rx_bogusPacketOnRead\n",
	    a_ovP->rx_bogusPacketOnRead);
    fprintf(fs_outFD, "\t%10d rx_bogusHost\n", a_ovP->rx_bogusHost);
    fprintf(fs_outFD, "\t%10d rx_noPacketOnRead\n", a_ovP->rx_noPacketOnRead);
    fprintf(fs_outFD, "\t%10d rx_noPacketBuffersOnRead\n",
	    a_ovP->rx_noPacketBuffersOnRead);
    fprintf(fs_outFD, "\t%10d rx_selects\n", a_ovP->rx_selects);
    fprintf(fs_outFD, "\t%10d rx_sendSelects\n", a_ovP->rx_sendSelects);
    fprintf(fs_outFD, "\t%10d rx_packetsRead_RcvClass\n",
	    a_ovP->rx_packetsRead_RcvClass);
    fprintf(fs_outFD, "\t%10d rx_packetsRead_SendClass\n",
	    a_ovP->rx_packetsRead_SendClass);
    fprintf(fs_outFD, "\t%10d rx_packetsRead_SpecialClass\n",
	    a_ovP->rx_packetsRead_SpecialClass);
    fprintf(fs_outFD, "\t%10d rx_dataPacketsRead\n",
	    a_ovP->rx_dataPacketsRead);
    fprintf(fs_outFD, "\t%10d rx_ackPacketsRead\n", a_ovP->rx_ackPacketsRead);
    fprintf(fs_outFD, "\t%10d rx_dupPacketsRead\n", a_ovP->rx_dupPacketsRead);
    fprintf(fs_outFD, "\t%10d rx_spuriousPacketsRead\n",
	    a_ovP->rx_spuriousPacketsRead);
    fprintf(fs_outFD, "\t%10d rx_packetsSent_RcvClass\n",
	    a_ovP->rx_packetsSent_RcvClass);
    fprintf(fs_outFD, "\t%10d rx_packetsSent_SendClass\n",
	    a_ovP->rx_packetsSent_SendClass);
    fprintf(fs_outFD, "\t%10d rx_packetsSent_SpecialClass\n",
	    a_ovP->rx_packetsSent_SpecialClass);
    fprintf(fs_outFD, "\t%10d rx_ackPacketsSent\n", a_ovP->rx_ackPacketsSent);
    fprintf(fs_outFD, "\t%10d rx_pingPacketsSent\n",
	    a_ovP->rx_pingPacketsSent);
    fprintf(fs_outFD, "\t%10d rx_abortPacketsSent\n",
	    a_ovP->rx_abortPacketsSent);
    fprintf(fs_outFD, "\t%10d rx_busyPacketsSent\n",
	    a_ovP->rx_busyPacketsSent);
    fprintf(fs_outFD, "\t%10d rx_dataPacketsSent\n",
	    a_ovP->rx_dataPacketsSent);
    fprintf(fs_outFD, "\t%10d rx_dataPacketsReSent\n",
	    a_ovP->rx_dataPacketsReSent);
    fprintf(fs_outFD, "\t%10d rx_dataPacketsPushed\n",
	    a_ovP->rx_dataPacketsPushed);
    fprintf(fs_outFD, "\t%10d rx_ignoreAckedPacket\n",
	    a_ovP->rx_ignoreAckedPacket);
    fprintf(fs_outFD, "\t%10d rx_totalRtt_Sec\n", a_ovP->rx_totalRtt_Sec);
    fprintf(fs_outFD, "\t%10d rx_totalRtt_Usec\n", a_ovP->rx_totalRtt_Usec);
    fprintf(fs_outFD, "\t%10d rx_minRtt_Sec\n", a_ovP->rx_minRtt_Sec);
    fprintf(fs_outFD, "\t%10d rx_minRtt_Usec\n", a_ovP->rx_minRtt_Usec);
    fprintf(fs_outFD, "\t%10d rx_maxRtt_Sec\n", a_ovP->rx_maxRtt_Sec);
    fprintf(fs_outFD, "\t%10d rx_maxRtt_Usec\n", a_ovP->rx_maxRtt_Usec);
    fprintf(fs_outFD, "\t%10d rx_nRttSamples\n", a_ovP->rx_nRttSamples);
    fprintf(fs_outFD, "\t%10d rx_nServerConns\n", a_ovP->rx_nServerConns);
    fprintf(fs_outFD, "\t%10d rx_nClientConns\n", a_ovP->rx_nClientConns);
    fprintf(fs_outFD, "\t%10d rx_nPeerStructs\n", a_ovP->rx_nPeerStructs);
    fprintf(fs_outFD, "\t%10d rx_nCallStructs\n", a_ovP->rx_nCallStructs);
    fprintf(fs_outFD, "\t%10d rx_nFreeCallStructs\n\n",
	    a_ovP->rx_nFreeCallStructs);

    /*
     * Host module fields.
     */
    fprintf(fs_outFD, "\t%10d host_NumHostEntries\n",
	    a_ovP->host_NumHostEntries);
    fprintf(fs_outFD, "\t%10d host_HostBlocks\n", a_ovP->host_HostBlocks);
    fprintf(fs_outFD, "\t%10d host_NonDeletedHosts\n",
	    a_ovP->host_NonDeletedHosts);
    fprintf(fs_outFD, "\t%10d host_HostsInSameNetOrSubnet\n",
	    a_ovP->host_HostsInSameNetOrSubnet);
    fprintf(fs_outFD, "\t%10d host_HostsInDiffSubnet\n",
	    a_ovP->host_HostsInDiffSubnet);
    fprintf(fs_outFD, "\t%10d host_HostsInDiffNetwork\n",
	    a_ovP->host_HostsInDiffNetwork);
    fprintf(fs_outFD, "\t%10d host_NumClients\n", a_ovP->host_NumClients);
    fprintf(fs_outFD, "\t%10d host_ClientBlocks\n\n",
	    a_ovP->host_ClientBlocks);

}				/*Print_fs_OverallPerfInfo */


/*------------------------------------------------------------------------
 * Print_fs_OpTiming
 *
 * Description:
 *	Print out the contents of an RPC op timing structure.
 *
 * Arguments:
 *	a_opIdx   : Index of the AFS operation we're printing number on.
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
Print_fs_OpTiming(a_opIdx, a_opTimeP)
     int a_opIdx;
     struct fs_stats_opTimingData *a_opTimeP;

{				/*Print_fs_OpTiming */

    fprintf(fs_outFD,
	    "%15s: %d ops (%d OK); sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	    fsOpNames[a_opIdx], a_opTimeP->numOps, a_opTimeP->numSuccesses,
	    a_opTimeP->sumTime.tv_sec, a_opTimeP->sumTime.tv_usec,
	    a_opTimeP->minTime.tv_sec, a_opTimeP->minTime.tv_usec,
	    a_opTimeP->maxTime.tv_sec, a_opTimeP->maxTime.tv_usec);

}				/*Print_fs_OpTiming */


/*------------------------------------------------------------------------
 * Print_fs_XferTiming
 *
 * Description:
 *	Print out the contents of a data transfer structure.
 *
 * Arguments:
 *	a_opIdx : Index of the AFS operation we're printing number on.
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
Print_fs_XferTiming(a_opIdx, a_xferP)
     int a_opIdx;
     struct fs_stats_xferData *a_xferP;

{				/*Print_fs_XferTiming */

    fprintf(fs_outFD,
	    "%s: %d xfers (%d OK), time sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	    xferOpNames[a_opIdx], a_xferP->numXfers, a_xferP->numSuccesses,
	    a_xferP->sumTime.tv_sec, a_xferP->sumTime.tv_usec,
	    a_xferP->minTime.tv_sec, a_xferP->minTime.tv_usec,
	    a_xferP->maxTime.tv_sec, a_xferP->maxTime.tv_usec);
    fprintf(fs_outFD, "\t[bytes: sum=%d, min=%d, max=%d]\n",
	    a_xferP->sumBytes, a_xferP->minBytes, a_xferP->maxBytes);
    fprintf(fs_outFD,
	    "\t[buckets: 0: %d, 1: %d, 2: %d, 3: %d, 4: %d, 5: %d 6: %d, 7: %d, 8: %d]\n",
	    a_xferP->count[0], a_xferP->count[1], a_xferP->count[2],
	    a_xferP->count[3], a_xferP->count[4], a_xferP->count[5],
	    a_xferP->count[6], a_xferP->count[7], a_xferP->count[8]);

}				/*Print_fs_XferTiming */


/*------------------------------------------------------------------------
 * Print_fs_DetailedPerfInfo
 *
 * Description:
 *	Print out a set of detailed performance numbers.
 *
 * Arguments:
 *	a_detP : Ptr to detailed perf numbers to print.
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
Print_fs_DetailedPerfInfo(a_detP)
     struct fs_stats_DetailedStats *a_detP;

{				/*Print_fs_DetailedPerfInfo */

    int currIdx;		/*Loop variable */

    fprintf(fs_outFD, "\t%10d epoch\n", a_detP->epoch);

    for (currIdx = 0; currIdx < FS_STATS_NUM_RPC_OPS; currIdx++)
	Print_fs_OpTiming(currIdx, &(a_detP->rpcOpTimes[currIdx]));

    for (currIdx = 0; currIdx < FS_STATS_NUM_XFER_OPS; currIdx++)
	Print_fs_XferTiming(currIdx, &(a_detP->xferOpTimes[currIdx]));

}				/*Print_fs_DetailedPerfInfo */


/*------------------------------------------------------------------------
 * Print_fs_FullPerfInfo
 *
 * Description:
 *	Print out the AFS_XSTATSCOLL_FULL_PERF_INFO collection we just
 *	received.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_fs_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
Print_fs_FullPerfInfo(a_fs_Results)
     struct xstat_fs_ProbeResults *a_fs_Results;	/* ptr to fs results */
{				/*Print_fs_FullPerfInfo */

    static char rn[] = "Print_fs_FullPerfInfo";	/*Routine name */
    static afs_int32 fullPerfLongs = (sizeof(struct fs_stats_FullPerfStats) >> 2);	/*Correct # longs to rcv */
    afs_int32 numLongs;		/*# longwords received */
    struct fs_stats_FullPerfStats *fullPerfP;	/*Ptr to full perf stats */
    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime;

    numLongs = a_fs_Results->data.AFS_CollData_len;
    if (numLongs != fullPerfLongs) {
	fprintf(fs_outFD,
		" ** Data size mismatch in full performance collection!\n");
	fprintf(fs_outFD, " ** Expecting %d, got %d\n", fullPerfLongs,
		numLongs);
	return;
    }

    probeTime = a_fs_Results->probeTime;
    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    fullPerfP = (struct fs_stats_FullPerfStats *)
	(a_fs_Results->data.AFS_CollData_val);

    fprintf(fs_outFD,
	    "AFS_XSTATSCOLL_FULL_PERF_INFO (coll %d) for FS %s\n[Probe %d, %s]\n\n",
	    a_fs_Results->collectionNumber, a_fs_Results->connP->hostName,
	    a_fs_Results->probeNum, printableTime);

    Print_fs_OverallPerfInfo(&(fullPerfP->overall));
    Print_fs_DetailedPerfInfo(&(fullPerfP->det));

}				/*Print_fs_FullPerfInfo */


/*------------------------------------------------------------------------
 * afsmon_fsOutput()
 *
 * Description:
 *	Prints the contents of xstat_fs_Results to an output file. The 
 *	output is either in a compact (longs only) format or a detailed
 *	format giving the names of each of the datums. Output is appended.
 *
 * Arguments:
 *	Name of output file.
 *	Flag to indicate if detailed output is required.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	All the info we need is nestled into xstat_fs_Results.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/
int
afsmon_fsOutput(a_outfile, a_detOutput)
     char *a_outfile;		/* ptr to output file name */
     int a_detOutput;		/* detailed output ? */
{

    static char rn[] = "afsmon_fsOutput";	/* routine name */
    char *printTime;		/* ptr to time string */
    char *hostname;		/* fileserner name */
    afs_int32 numLongs;		/* longwords in result */
    afs_int32 *currLong;	/* ptr to longwords in result */
    int i;
    time_t probeTime;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_outfile= %s, a_detOutput= %d\n",
		rn, a_outfile, a_detOutput);
	fflush(debugFD);
    }

    fs_outFD = fopen(a_outfile, "a");
    if (fs_outFD == (FILE *) 0) {
	sprintf(errMsg, "[ %s ] failed to open output file %s", rn,
		a_outfile);
	afsmon_Exit(1);
    }

    /* get the probe time and strip the \n at the end */
    probeTime = xstat_fs_Results.probeTime;
    printTime = ctime(&probeTime);
    printTime[strlen(printTime) - 1] = '\0';
    hostname = xstat_fs_Results.connP->hostName;

    /* print "time hostname FS" */
    fprintf(fs_outFD, "\n%s %s FS ", printTime, hostname);

    /* if probe failed print -1 and return */
    if (xstat_fs_Results.probeOK) {
	fprintf(fs_outFD, "-1\n");
	fclose(fs_outFD);
	return (0);
    }

    /* print out the probe information as  long words */
    numLongs = xstat_fs_Results.data.AFS_CollData_len;
    currLong = (afs_int32 *) (xstat_fs_Results.data.AFS_CollData_val);

    for (i = 0; i < numLongs; i++) {
	fprintf(fs_outFD, "%d ", *currLong++);
    }
    fprintf(fs_outFD, "\n\n");

    /* print detailed information */
    if (a_detOutput) {
	Print_fs_FullPerfInfo(&xstat_fs_Results);
	fflush(fs_outFD);
    }

    if (fclose(fs_outFD))
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] failed to close %s\n", rn, a_outfile);
	    fflush(debugFD);
	}

    return (0);
}

/*___________________________________________________________________________
			CM STATS
 *__________________________________________________________________________*/



/*------------------------------------------------------------------------
 * Print_cm_UpDownStats
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
Print_cm_UpDownStats(a_upDownP)
     struct afs_stats_SrvUpDownInfo *a_upDownP;	/*Ptr to server up/down info */

{				/*Print_cm_UpDownStats */

    /*
     * First, print the simple values.
     */
    fprintf(cm_outFD, "\t\t%10d numTtlRecords\n", a_upDownP->numTtlRecords);
    fprintf(cm_outFD, "\t\t%10d numUpRecords\n", a_upDownP->numUpRecords);
    fprintf(cm_outFD, "\t\t%10d numDownRecords\n", a_upDownP->numDownRecords);
    fprintf(cm_outFD, "\t\t%10d sumOfRecordAges\n",
	    a_upDownP->sumOfRecordAges);
    fprintf(cm_outFD, "\t\t%10d ageOfYoungestRecord\n",
	    a_upDownP->ageOfYoungestRecord);
    fprintf(cm_outFD, "\t\t%10d ageOfOldestRecord\n",
	    a_upDownP->ageOfOldestRecord);
    fprintf(cm_outFD, "\t\t%10d numDowntimeIncidents\n",
	    a_upDownP->numDowntimeIncidents);
    fprintf(cm_outFD, "\t\t%10d numRecordsNeverDown\n",
	    a_upDownP->numRecordsNeverDown);
    fprintf(cm_outFD, "\t\t%10d maxDowntimesInARecord\n",
	    a_upDownP->maxDowntimesInARecord);
    fprintf(cm_outFD, "\t\t%10d sumOfDowntimes\n", a_upDownP->sumOfDowntimes);
    fprintf(cm_outFD, "\t\t%10d shortestDowntime\n",
	    a_upDownP->shortestDowntime);
    fprintf(cm_outFD, "\t\t%10d longestDowntime\n",
	    a_upDownP->longestDowntime);

    /*
     * Now, print the array values.
     */
    fprintf(cm_outFD, "\t\tDowntime duration distribution:\n");
    fprintf(cm_outFD, "\t\t\t%8d: 0 min .. 10 min\n",
	    a_upDownP->downDurations[0]);
    fprintf(cm_outFD, "\t\t\t%8d: 10 min .. 30 min\n",
	    a_upDownP->downDurations[1]);
    fprintf(cm_outFD, "\t\t\t%8d: 30 min .. 1 hr\n",
	    a_upDownP->downDurations[2]);
    fprintf(cm_outFD, "\t\t\t%8d: 1 hr .. 2 hr\n",
	    a_upDownP->downDurations[3]);
    fprintf(cm_outFD, "\t\t\t%8d: 2 hr .. 4 hr\n",
	    a_upDownP->downDurations[4]);
    fprintf(cm_outFD, "\t\t\t%8d: 4 hr .. 8 hr\n",
	    a_upDownP->downDurations[5]);
    fprintf(cm_outFD, "\t\t\t%8d: > 8 hr\n", a_upDownP->downDurations[6]);

    fprintf(cm_outFD, "\t\tDowntime incident distribution:\n");
    fprintf(cm_outFD, "\t\t\t%8d: 0 times\n", a_upDownP->downIncidents[0]);
    fprintf(cm_outFD, "\t\t\t%8d: 1 time\n", a_upDownP->downIncidents[1]);
    fprintf(cm_outFD, "\t\t\t%8d: 2 .. 5 times\n",
	    a_upDownP->downIncidents[2]);
    fprintf(cm_outFD, "\t\t\t%8d: 6 .. 10 times\n",
	    a_upDownP->downIncidents[3]);
    fprintf(cm_outFD, "\t\t\t%8d: 10 .. 50 times\n",
	    a_upDownP->downIncidents[4]);
    fprintf(cm_outFD, "\t\t\t%8d: > 50 times\n", a_upDownP->downIncidents[5]);

}				/*Print_cm_UpDownStats */


/*------------------------------------------------------------------------
 * Print_cm_OverallPerfInfo
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
Print_cm_OverallPerfInfo(a_ovP)
     struct afs_stats_CMPerf *a_ovP;

{				/*Print_cm_OverallPerfInfo */

    fprintf(cm_outFD, "\t%10d numPerfCalls\n", a_ovP->numPerfCalls);

    fprintf(cm_outFD, "\t%10d epoch\n", a_ovP->epoch);
    fprintf(cm_outFD, "\t%10d numCellsVisible\n", a_ovP->numCellsVisible);
    fprintf(cm_outFD, "\t%10d numCellsContacted\n", a_ovP->numCellsContacted);
    fprintf(cm_outFD, "\t%10d dlocalAccesses\n", a_ovP->dlocalAccesses);
    fprintf(cm_outFD, "\t%10d vlocalAccesses\n", a_ovP->vlocalAccesses);
    fprintf(cm_outFD, "\t%10d dremoteAccesses\n", a_ovP->dremoteAccesses);
    fprintf(cm_outFD, "\t%10d vremoteAccesses\n", a_ovP->vremoteAccesses);
    fprintf(cm_outFD, "\t%10d cacheNumEntries\n", a_ovP->cacheNumEntries);
    fprintf(cm_outFD, "\t%10d cacheBlocksTotal\n", a_ovP->cacheBlocksTotal);
    fprintf(cm_outFD, "\t%10d cacheBlocksInUse\n", a_ovP->cacheBlocksInUse);
    fprintf(cm_outFD, "\t%10d cacheBlocksOrig\n", a_ovP->cacheBlocksOrig);
    fprintf(cm_outFD, "\t%10d cacheMaxDirtyChunks\n",
	    a_ovP->cacheMaxDirtyChunks);
    fprintf(cm_outFD, "\t%10d cacheCurrDirtyChunks\n",
	    a_ovP->cacheCurrDirtyChunks);
    fprintf(cm_outFD, "\t%10d dcacheHits\n", a_ovP->dcacheHits);
    fprintf(cm_outFD, "\t%10d vcacheHits\n", a_ovP->vcacheHits);
    fprintf(cm_outFD, "\t%10d dcacheMisses\n", a_ovP->dcacheMisses);
    fprintf(cm_outFD, "\t%10d vcacheMisses\n", a_ovP->vcacheMisses);
    fprintf(cm_outFD, "\t%10d cacheFilesReused\n", a_ovP->cacheFilesReused);
    fprintf(cm_outFD, "\t%10d vcacheXAllocs\n", a_ovP->vcacheXAllocs);

    fprintf(cm_outFD, "\t%10d bufAlloced\n", a_ovP->bufAlloced);
    fprintf(cm_outFD, "\t%10d bufHits\n", a_ovP->bufHits);
    fprintf(cm_outFD, "\t%10d bufMisses\n", a_ovP->bufMisses);
    fprintf(cm_outFD, "\t%10d bufFlushDirty\n", a_ovP->bufFlushDirty);

    fprintf(cm_outFD, "\t%10d LargeBlocksActive\n", a_ovP->LargeBlocksActive);
    fprintf(cm_outFD, "\t%10d LargeBlocksAlloced\n",
	    a_ovP->LargeBlocksAlloced);
    fprintf(cm_outFD, "\t%10d SmallBlocksActive\n", a_ovP->SmallBlocksActive);
    fprintf(cm_outFD, "\t%10d SmallBlocksAlloced\n",
	    a_ovP->SmallBlocksAlloced);
    fprintf(cm_outFD, "\t%10d OutStandingMemUsage\n",
	    a_ovP->OutStandingMemUsage);
    fprintf(cm_outFD, "\t%10d OutStandingAllocs\n", a_ovP->OutStandingAllocs);
    fprintf(cm_outFD, "\t%10d CallBackAlloced\n", a_ovP->CallBackAlloced);
    fprintf(cm_outFD, "\t%10d CallBackFlushes\n", a_ovP->CallBackFlushes);

    fprintf(cm_outFD, "\t%10d srvRecords\n", a_ovP->srvRecords);
    fprintf(cm_outFD, "\t%10d srvNumBuckets\n", a_ovP->srvNumBuckets);
    fprintf(cm_outFD, "\t%10d srvMaxChainLength\n", a_ovP->srvMaxChainLength);
    fprintf(cm_outFD, "\t%10d srvMaxChainLengthHWM\n",
	    a_ovP->srvMaxChainLengthHWM);
    fprintf(cm_outFD, "\t%10d srvRecordsHWM\n", a_ovP->srvRecordsHWM);

    fprintf(cm_outFD, "\t%10d sysName_ID\n", a_ovP->sysName_ID);

    fprintf(cm_outFD, "\tFile Server up/downtimes, same cell:\n");
    Print_cm_UpDownStats(&(a_ovP->fs_UpDown[0]));

    fprintf(cm_outFD, "\tFile Server up/downtimes, diff cell:\n");
    Print_cm_UpDownStats(&(a_ovP->fs_UpDown[1]));

    fprintf(cm_outFD, "\tVL Server up/downtimes, same cell:\n");
    Print_cm_UpDownStats(&(a_ovP->vl_UpDown[0]));

    fprintf(cm_outFD, "\tVL Server up/downtimes, diff cell:\n");
    Print_cm_UpDownStats(&(a_ovP->vl_UpDown[1]));

}				/*Print_cm_OverallPerfInfo */


/*------------------------------------------------------------------------
 * Print_cm_PerfInfo
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
Print_cm_PerfInfo()
{				/*Print_cm_PerfInfo */

    static char rn[] = "Print_cm_PerfInfo";	/*Routine name */
    static afs_int32 perfLongs = (sizeof(struct afs_stats_CMPerf) >> 2);	/*Correct # longs to rcv */
    afs_int32 numLongs;		/*# longwords received */
    struct afs_stats_CMPerf *perfP;	/*Ptr to performance stats */
    char *printableTime;	/*Ptr to printable time string */
    time_t probeTime;

    numLongs = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numLongs != perfLongs) {
	fprintf(cm_outFD,
		" ** Data size mismatch in performance collection!\n");
	fprintf(cm_outFD, "** Expecting %d, got %d\n", perfLongs, numLongs);
	return;
    }

    probeTime = xstat_cm_Results.probeTime;
    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    perfP = (struct afs_stats_CMPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    fprintf(cm_outFD,
	    "AFSCB_XSTATSCOLL_PERF_INFO (coll %d) for CM %s\n[Probe %d, %s]\n\n",
	    xstat_cm_Results.collectionNumber,
	    xstat_cm_Results.connP->hostName, xstat_cm_Results.probeNum,
	    printableTime);

    Print_cm_OverallPerfInfo(perfP);

}				/*Print_cm_PerfInfo */


/*------------------------------------------------------------------------
 * Print_cm_OpTiming
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
Print_cm_OpTiming(a_opIdx, a_opNames, a_opTimeP)
     int a_opIdx;
     char *a_opNames[];
     struct afs_stats_opTimingData *a_opTimeP;

{				/*Print_cm_OpTiming */

    fprintf(cm_outFD,
	    "%15s: %d ops (%d OK); sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	    a_opNames[a_opIdx], a_opTimeP->numOps, a_opTimeP->numSuccesses,
	    a_opTimeP->sumTime.tv_sec, a_opTimeP->sumTime.tv_usec,
	    a_opTimeP->minTime.tv_sec, a_opTimeP->minTime.tv_usec,
	    a_opTimeP->maxTime.tv_sec, a_opTimeP->maxTime.tv_usec);

}				/*Print_cm_OpTiming */


/*------------------------------------------------------------------------
 * Print_cm_XferTiming
 *
 * Description:
 *	Print out the contents of a data transfer structure.
 *
 * Arguments:
 *	a_opIdx : Index of the AFS operation we're printing number on.
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
Print_cm_XferTiming(a_opIdx, a_opNames, a_xferP)
     int a_opIdx;
     char *a_opNames[];
     struct afs_stats_xferData *a_xferP;

{				/*Print_cm_XferTiming */

    fprintf(cm_outFD,
	    "%s: %d xfers (%d OK), time sum=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	    a_opNames[a_opIdx], a_xferP->numXfers, a_xferP->numSuccesses,
	    a_xferP->sumTime.tv_sec, a_xferP->sumTime.tv_usec,
	    a_xferP->minTime.tv_sec, a_xferP->minTime.tv_usec,
	    a_xferP->maxTime.tv_sec, a_xferP->maxTime.tv_usec);
    fprintf(cm_outFD, "\t[bytes: sum=%d, min=%d, max=%d]\n",
	    a_xferP->sumBytes, a_xferP->minBytes, a_xferP->maxBytes);
    fprintf(cm_outFD,
	    "\t[buckets: 0: %d, 1: %d, 2: %d, 3: %d, 4: %d, 5: %d 6: %d, 7: %d, 8: %d]\n",
	    a_xferP->count[0], a_xferP->count[1], a_xferP->count[2],
	    a_xferP->count[3], a_xferP->count[4], a_xferP->count[5],
	    a_xferP->count[6], a_xferP->count[7], a_xferP->count[8]);

}				/*Print_cm_XferTiming */


/*------------------------------------------------------------------------
 * Print_cm_ErrInfo
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
Print_cm_ErrInfo(a_opIdx, a_opNames, a_opErrP)
     int a_opIdx;
     char *a_opNames[];
     struct afs_stats_RPCErrors *a_opErrP;

{				/*Print_cm_ErrInfo */

    fprintf(cm_outFD,
	    "%15s: %d server, %d network, %d prot, %d vol, %d busies, %d other\n",
	    a_opNames[a_opIdx], a_opErrP->err_Server, a_opErrP->err_Network,
	    a_opErrP->err_Protection, a_opErrP->err_Volume,
	    a_opErrP->err_VolumeBusies, a_opErrP->err_Other);

}				/*Print_cm_ErrInfo */


/*------------------------------------------------------------------------
 * Print_cm_RPCPerfInfo
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
Print_cm_RPCPerfInfo(a_rpcP)
     struct afs_stats_RPCOpInfo *a_rpcP;

{				/*Print_cm_RPCPerfInfo */

    int currIdx;		/*Loop variable */

    /*
     * Print the contents of each of the opcode-related arrays.
     */
    fprintf(cm_outFD, "FS Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	Print_cm_OpTiming(currIdx, fsOpNames, &(a_rpcP->fsRPCTimes[currIdx]));

    fprintf(cm_outFD, "\nError Info:\n-----------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_RPC_OPS; currIdx++)
	Print_cm_ErrInfo(currIdx, fsOpNames, &(a_rpcP->fsRPCErrors[currIdx]));

    fprintf(cm_outFD, "\nTransfer timings:\n-----------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_FS_XFER_OPS; currIdx++)
	Print_cm_XferTiming(currIdx, xferOpNames,
			    &(a_rpcP->fsXferTimes[currIdx]));

    fprintf(cm_outFD, "\nCM Operation Timings:\n---------------------\n");
    for (currIdx = 0; currIdx < AFS_STATS_NUM_CM_RPC_OPS; currIdx++)
	Print_cm_OpTiming(currIdx, cmOpNames, &(a_rpcP->cmRPCTimes[currIdx]));

}				/*Print_cm_RPCPerfInfo */


/*------------------------------------------------------------------------
 * Print_cm_FullPerfInfo
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
Print_cm_FullPerfInfo()
{				/*Print_cm_FullPerfInfo */

    static char rn[] = "Print_cm_FullPerfInfo";	/* routine name */
    struct afs_stats_AuthentInfo *authentP;	/*Ptr to authentication stats */
    struct afs_stats_AccessInfo *accessinfP;	/*Ptr to access stats */
    static afs_int32 fullPerfLongs = (sizeof(struct afs_stats_CMFullPerf) >> 2);	/*Correct #longs */
    afs_int32 numLongs;		/*# longs actually received */
    struct afs_stats_CMFullPerf *fullP;	/*Ptr to full perf info */
    time_t probeTime;
    char *printableTime;	/*Ptr to printable time string */

    numLongs = xstat_cm_Results.data.AFSCB_CollData_len;
    if (numLongs != fullPerfLongs) {
	fprintf(cm_outFD,
		" ** Data size mismatch in performance collection!\n");
	fprintf(cm_outFD, " ** Expecting %d, got %d\n", fullPerfLongs,
		numLongs);
	return;
    }

    probeTime = xstat_cm_Results.probeTime;
    printableTime = ctime(&probeTime);
    printableTime[strlen(printableTime) - 1] = '\0';
    fullP = (struct afs_stats_CMFullPerf *)
	(xstat_cm_Results.data.AFSCB_CollData_val);

    fprintf(cm_outFD,
	    "AFSCB_XSTATSCOLL_FULL_PERF_INFO (coll %d) for CM %s\n[Probe %d, %s]\n\n",
	    xstat_cm_Results.collectionNumber,
	    xstat_cm_Results.connP->hostName, xstat_cm_Results.probeNum,
	    printableTime);

    /*
     * Print the overall numbers first, followed by all of the RPC numbers,
     * then each of the other groupings.
     */
    fprintf(cm_outFD,
	    "Overall Performance Info:\n-------------------------\n");
    Print_cm_OverallPerfInfo(&(fullP->perf));
    fprintf(cm_outFD, "\n");
    Print_cm_RPCPerfInfo(&(fullP->rpc));

    authentP = &(fullP->authent);
    fprintf(cm_outFD, "\nAuthentication info:\n--------------------\n");
    fprintf(cm_outFD,
	    "\t%d PAGS, %d records (%d auth, %d unauth), %d max in PAG, chain max: %d\n",
	    authentP->curr_PAGs, authentP->curr_Records,
	    authentP->curr_AuthRecords, authentP->curr_UnauthRecords,
	    authentP->curr_MaxRecordsInPAG, authentP->curr_LongestChain);
    fprintf(cm_outFD, "\t%d PAG creations, %d tkt updates\n",
	    authentP->PAGCreations, authentP->TicketUpdates);
    fprintf(cm_outFD,
	    "\t[HWMs: %d PAGS, %d records, %d max in PAG, chain max: %d]\n",
	    authentP->HWM_PAGs, authentP->HWM_Records,
	    authentP->HWM_MaxRecordsInPAG, authentP->HWM_LongestChain);

    accessinfP = &(fullP->accessinf);
    fprintf(cm_outFD,
	    "\n[Un]replicated accesses:\n------------------------\n");
    fprintf(cm_outFD,
	    "\t%d unrep, %d rep, %d reps accessed, %d max reps/ref, %d first OK\n\n",
	    accessinfP->unreplicatedRefs, accessinfP->replicatedRefs,
	    accessinfP->numReplicasAccessed, accessinfP->maxReplicasPerRef,
	    accessinfP->refFirstReplicaOK);

    /* There really isn't any authorship info
     * authorP = &(fullP->author); */

}				/*Print_cm_FullPerfInfo */

/*------------------------------------------------------------------------
 * afsmon_cmOutput()
 *
 * Description:
 *	Prints the contents of xstat_cm_Results to an output file. The 
 *	output is either in a compact (longs only) format or a detailed
 *	format giving the names of each of the datums. Output is appended.
 *
 * Arguments:
 *	Name of output file.
 *	Flag to indicate if detailed output is required.
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
int
afsmon_cmOutput(a_outfile, a_detOutput)
     char *a_outfile;		/* ptr to output file name */
     int a_detOutput;		/* detailed output ? */
{

    static char rn[] = "afsmon_cmOutput";	/* routine name */
    char *printTime;		/* ptr to time string */
    char *hostname;		/* fileserner name */
    afs_int32 numLongs;		/* longwords in result */
    afs_int32 *currLong;	/* ptr to longwords in result */
    int i;
    time_t probeTime;

    if (afsmon_debug) {
	fprintf(debugFD, "[ %s ] Called, a_outfile= %s, a_detOutput= %d\n",
		rn, a_outfile, a_detOutput);
	fflush(debugFD);
    }

    /* need to lock this file before writing */
    cm_outFD = fopen(a_outfile, "a");
    if (cm_outFD == (FILE *) 0) {
	sprintf(errMsg, "[ %s ] failed to open output file %s", rn,
		a_outfile);
	afsmon_Exit(1);
    }

    /* get the probe time and strip the \n at the end */
    probeTime = xstat_cm_Results.probeTime;
    printTime = ctime(&probeTime);
    printTime[strlen(printTime) - 1] = '\0';
    hostname = xstat_cm_Results.connP->hostName;

    /* print "time hostname CM" prefix  */
    fprintf(cm_outFD, "\n%s %s CM ", printTime, hostname);

    /* if probe failed print -1 and vanish */
    if (xstat_cm_Results.probeOK) {
	fprintf(cm_outFD, "-1\n");
	fclose(cm_outFD);
	return (0);
    }

    /* print out the probe information as  long words */
    numLongs = xstat_cm_Results.data.AFSCB_CollData_len;
    currLong = (afs_int32 *) (xstat_cm_Results.data.AFSCB_CollData_val);

    for (i = 0; i < numLongs; i++) {
	fprintf(cm_outFD, "%d ", *currLong++);
    }
    fprintf(cm_outFD, "\n\n");

    /* print out detailed statistics */
    if (a_detOutput) {
	Print_cm_FullPerfInfo();
	fflush(cm_outFD);
    }

    if (fclose(cm_outFD))
	if (afsmon_debug) {
	    fprintf(debugFD, "[ %s ] failed to close %s\n", rn, a_outfile);
	    fflush(debugFD);
	}

    return (0);
}
