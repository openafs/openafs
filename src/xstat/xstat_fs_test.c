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
 *	Test of the xstat_fs module.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "xstat_fs.h"		/*Interface for xstat_fs module */
#include <cmd.h>		/*Command line interpreter */
#include <time.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/*
 * External routines that don't have explicit include file definitions.
 */
extern struct hostent *hostutil_GetHostByName();

/*
 * Command line parameter indices.
 *	P_FS_NAMES : List of FileServer names.
 *	P_COLL_IDS : List of collection IDs to pick up.
 *	P_ONESHOT  : Are we gathering exactly one round of data?
 *	P_DEBUG    : Enable debugging output?
 */
#define P_FS_NAMES	0
#define P_COLL_IDS	1
#define P_ONESHOT	2
#define P_FREQUENCY	3
#define P_PERIOD	4
#define P_DEBUG		5

/*
 * Private globals.
 */
static int debugging_on = 0;	/*Are we debugging? */
static int one_shot = 0;	/*Single round of data collection? */

static char *opNames[] = {
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

static char *xferOpNames[] = {
    "FetchData",
    "StoreData"
};


/*------------------------------------------------------------------------
 * PrintCallInfo
 *
 * Description:
 *	Print out the AFS_XSTATSCOLL_CALL_INFO collection we just
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
PrintCallInfo()
{				/*PrintCallInfo */

    static char rn[] = "PrintCallInfo";	/*Routine name */
    register int i;		/*Loop variable */
    int numInt32s;		/*# int32words returned */
    afs_int32 *currInt32;	/*Ptr to current afs_int32 value */
    char *printableTime;	/*Ptr to printable time string */

    /*
     * Just print out the results of the particular probe.
     */
    numInt32s = xstat_fs_Results.data.AFS_CollData_len;
    currInt32 = (afs_int32 *) (xstat_fs_Results.data.AFS_CollData_val);
    printableTime = ctime((time_t *) & (xstat_fs_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';

    printf("AFS_XSTATSCOLL_CALL_INFO (coll %d) for FS %s\n[Probe %d, %s]\n\n",
	   xstat_fs_Results.collectionNumber,
	   xstat_fs_Results.connP->hostName, xstat_fs_Results.probeNum,
	   printableTime);

    if (debugging_on)
	printf("\n[%d entries returned at 0x%x]\n\n", numInt32s, currInt32);

    for (i = 0; i < numInt32s; i++)
	printf("%d ", *currInt32++);
    fprintf(stderr, "\n");

}				/*PrintCallInfo */


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
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

void
PrintOverallPerfInfo(struct afs_PerfStats *a_ovP)
{
    printf("\t%10d numPerfCalls\n\n", a_ovP->numPerfCalls);

    /*
     * Vnode cache section.
     */
    printf("\t%10d vcache_L_Entries\n", a_ovP->vcache_L_Entries);
    printf("\t%10d vcache_L_Allocs\n", a_ovP->vcache_L_Allocs);
    printf("\t%10d vcache_L_Gets\n", a_ovP->vcache_L_Gets);
    printf("\t%10d vcache_L_Reads\n", a_ovP->vcache_L_Reads);
    printf("\t%10d vcache_L_Writes\n\n", a_ovP->vcache_L_Writes);

    printf("\t%10d vcache_S_Entries\n", a_ovP->vcache_S_Entries);
    printf("\t%10d vcache_S_Allocs\n", a_ovP->vcache_S_Allocs);
    printf("\t%10d vcache_S_Gets\n", a_ovP->vcache_S_Gets);
    printf("\t%10d vcache_S_Reads\n", a_ovP->vcache_S_Reads);
    printf("\t%10d vcache_S_Writes\n\n", a_ovP->vcache_S_Writes);

    printf("\t%10d vcache_H_Entries\n", a_ovP->vcache_H_Entries);
    printf("\t%10d vcache_H_Gets\n", a_ovP->vcache_H_Gets);
    printf("\t%10d vcache_H_Replacements\n\n", a_ovP->vcache_H_Replacements);

    /*
     * Directory package section.
     */
    printf("\t%10d dir_Buffers\n", a_ovP->dir_Buffers);
    printf("\t%10d dir_Calls\n", a_ovP->dir_Calls);
    printf("\t%10d dir_IOs\n\n", a_ovP->dir_IOs);

    /*
     * Rx section.
     */
    printf("\t%10d rx_packetRequests\n", a_ovP->rx_packetRequests);
    printf("\t%10d rx_noPackets_RcvClass\n", a_ovP->rx_noPackets_RcvClass);
    printf("\t%10d rx_noPackets_SendClass\n", a_ovP->rx_noPackets_SendClass);
    printf("\t%10d rx_noPackets_SpecialClass\n",
	   a_ovP->rx_noPackets_SpecialClass);
    printf("\t%10d rx_socketGreedy\n", a_ovP->rx_socketGreedy);
    printf("\t%10d rx_bogusPacketOnRead\n", a_ovP->rx_bogusPacketOnRead);
    printf("\t%10d rx_bogusHost\n", a_ovP->rx_bogusHost);
    printf("\t%10d rx_noPacketOnRead\n", a_ovP->rx_noPacketOnRead);
    printf("\t%10d rx_noPacketBuffersOnRead\n",
	   a_ovP->rx_noPacketBuffersOnRead);
    printf("\t%10d rx_selects\n", a_ovP->rx_selects);
    printf("\t%10d rx_sendSelects\n", a_ovP->rx_sendSelects);
    printf("\t%10d rx_packetsRead_RcvClass\n",
	   a_ovP->rx_packetsRead_RcvClass);
    printf("\t%10d rx_packetsRead_SendClass\n",
	   a_ovP->rx_packetsRead_SendClass);
    printf("\t%10d rx_packetsRead_SpecialClass\n",
	   a_ovP->rx_packetsRead_SpecialClass);
    printf("\t%10d rx_dataPacketsRead\n", a_ovP->rx_dataPacketsRead);
    printf("\t%10d rx_ackPacketsRead\n", a_ovP->rx_ackPacketsRead);
    printf("\t%10d rx_dupPacketsRead\n", a_ovP->rx_dupPacketsRead);
    printf("\t%10d rx_spuriousPacketsRead\n", a_ovP->rx_spuriousPacketsRead);
    printf("\t%10d rx_packetsSent_RcvClass\n",
	   a_ovP->rx_packetsSent_RcvClass);
    printf("\t%10d rx_packetsSent_SendClass\n",
	   a_ovP->rx_packetsSent_SendClass);
    printf("\t%10d rx_packetsSent_SpecialClass\n",
	   a_ovP->rx_packetsSent_SpecialClass);
    printf("\t%10d rx_ackPacketsSent\n", a_ovP->rx_ackPacketsSent);
    printf("\t%10d rx_pingPacketsSent\n", a_ovP->rx_pingPacketsSent);
    printf("\t%10d rx_abortPacketsSent\n", a_ovP->rx_abortPacketsSent);
    printf("\t%10d rx_busyPacketsSent\n", a_ovP->rx_busyPacketsSent);
    printf("\t%10d rx_dataPacketsSent\n", a_ovP->rx_dataPacketsSent);
    printf("\t%10d rx_dataPacketsReSent\n", a_ovP->rx_dataPacketsReSent);
    printf("\t%10d rx_dataPacketsPushed\n", a_ovP->rx_dataPacketsPushed);
    printf("\t%10d rx_ignoreAckedPacket\n", a_ovP->rx_ignoreAckedPacket);
    printf("\t%10d rx_totalRtt_Sec\n", a_ovP->rx_totalRtt_Sec);
    printf("\t%10d rx_totalRtt_Usec\n", a_ovP->rx_totalRtt_Usec);
    printf("\t%10d rx_minRtt_Sec\n", a_ovP->rx_minRtt_Sec);
    printf("\t%10d rx_minRtt_Usec\n", a_ovP->rx_minRtt_Usec);
    printf("\t%10d rx_maxRtt_Sec\n", a_ovP->rx_maxRtt_Sec);
    printf("\t%10d rx_maxRtt_Usec\n", a_ovP->rx_maxRtt_Usec);
    printf("\t%10d rx_nRttSamples\n", a_ovP->rx_nRttSamples);
    printf("\t%10d rx_nServerConns\n", a_ovP->rx_nServerConns);
    printf("\t%10d rx_nClientConns\n", a_ovP->rx_nClientConns);
    printf("\t%10d rx_nPeerStructs\n", a_ovP->rx_nPeerStructs);
    printf("\t%10d rx_nCallStructs\n", a_ovP->rx_nCallStructs);
    printf("\t%10d rx_nFreeCallStructs\n", a_ovP->rx_nFreeCallStructs);
    printf("\t%10d rx_nBusies\n\n", a_ovP->rx_nBusies);

    printf("\t%10d fs_nBusies\n", a_ovP->fs_nBusies);
    printf("\t%10d fs_GetCapabilities\n\n", a_ovP->fs_nGetCaps);
    /*
     * Host module fields.
     */
    printf("\t%10d host_NumHostEntries\n", a_ovP->host_NumHostEntries);
    printf("\t%10d host_HostBlocks\n", a_ovP->host_HostBlocks);
    printf("\t%10d host_NonDeletedHosts\n", a_ovP->host_NonDeletedHosts);
    printf("\t%10d host_HostsInSameNetOrSubnet\n",
	   a_ovP->host_HostsInSameNetOrSubnet);
    printf("\t%10d host_HostsInDiffSubnet\n", a_ovP->host_HostsInDiffSubnet);
    printf("\t%10d host_HostsInDiffNetwork\n",
	   a_ovP->host_HostsInDiffNetwork);
    printf("\t%10d host_NumClients\n", a_ovP->host_NumClients);
    printf("\t%10d host_ClientBlocks\n\n", a_ovP->host_ClientBlocks);

    printf("\t%10d sysname_ID\n", a_ovP->sysname_ID);
}


/*------------------------------------------------------------------------
 * PrintOpTiming
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
PrintOpTiming(int a_opIdx, struct fs_stats_opTimingData *a_opTimeP)
{
    double fSumTime, avg;

    fSumTime =
	((double)(a_opTimeP->sumTime.tv_sec)) +
	(((double)(a_opTimeP->sumTime.tv_usec)) / ((double)(1000000)));
/*    printf("Double sum time is %f\n", fSumTime);*/
    avg = fSumTime / ((double)(a_opTimeP->numSuccesses));

    printf
	("%15s: %d ops (%d OK); sum=%d.%06d, sqr=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	 opNames[a_opIdx], a_opTimeP->numOps, a_opTimeP->numSuccesses,
	 a_opTimeP->sumTime.tv_sec, a_opTimeP->sumTime.tv_usec,
	 a_opTimeP->sqrTime.tv_sec, a_opTimeP->sqrTime.tv_usec,
	 a_opTimeP->minTime.tv_sec, a_opTimeP->minTime.tv_usec,
	 a_opTimeP->maxTime.tv_sec, a_opTimeP->maxTime.tv_usec);
}


/*------------------------------------------------------------------------
 * PrintXferTiming
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
PrintXferTiming(int a_opIdx, struct fs_stats_xferData *a_xferP)
{
    double fSumTime, avg;

    fSumTime =
	((double)(a_xferP->sumTime.tv_sec)) +
	((double)(a_xferP->sumTime.tv_usec)) / ((double)(1000000));

    avg = fSumTime / ((double)(a_xferP->numSuccesses));

    printf
	("%s: %d xfers (%d OK), time sum=%d.%06d, sqr=%d.%06d, min=%d.%06d, max=%d.%06d\n",
	 xferOpNames[a_opIdx], a_xferP->numXfers, a_xferP->numSuccesses,
	 a_xferP->sumTime.tv_sec, a_xferP->sumTime.tv_usec,
	 a_xferP->sqrTime.tv_sec, a_xferP->sqrTime.tv_usec,
	 a_xferP->minTime.tv_sec, a_xferP->minTime.tv_usec,
	 a_xferP->maxTime.tv_sec, a_xferP->maxTime.tv_usec);
    printf("\t[bytes: sum=%lu, min=%d, max=%d]\n", a_xferP->sumBytes,
	   a_xferP->minBytes, a_xferP->maxBytes);
    printf
	("\t[buckets: 0: %d, 1: %d, 2: %d, 3: %d, 4: %d, 5: %d, 6: %d, 7: %d, 8: %d]\n",
	 a_xferP->count[0], a_xferP->count[1], a_xferP->count[2],
	 a_xferP->count[3], a_xferP->count[4], a_xferP->count[5],
	 a_xferP->count[6], a_xferP->count[7], a_xferP->count[8]);
}


/*------------------------------------------------------------------------
 * PrintDetailedPerfInfo
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
PrintDetailedPerfInfo(struct fs_stats_DetailedStats *a_detP)
{
    int currIdx;		/*Loop variable */

    printf("\t%10d epoch\n", a_detP->epoch);

    for (currIdx = 0; currIdx < FS_STATS_NUM_RPC_OPS; currIdx++)
	PrintOpTiming(currIdx, &(a_detP->rpcOpTimes[currIdx]));

    for (currIdx = 0; currIdx < FS_STATS_NUM_XFER_OPS; currIdx++)
	PrintXferTiming(currIdx, &(a_detP->xferOpTimes[currIdx]));
}


/*------------------------------------------------------------------------
 * PrintFullPerfInfo
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
PrintFullPerfInfo()
{

    static char rn[] = "PrintFullPerfInfo";	/*Routine name */
    static afs_int32 fullPerfInt32s = (sizeof(struct fs_stats_FullPerfStats) >> 2);	/*Correct # int32s to rcv */
    afs_int32 numInt32s;	/*# int32words received */
    struct fs_stats_FullPerfStats *fullPerfP;	/*Ptr to full perf stats */
    char *printableTime;	/*Ptr to printable time
				 * string */

    numInt32s = xstat_fs_Results.data.AFS_CollData_len;
    if (numInt32s != fullPerfInt32s) {
	printf("** Data size mismatch in full performance collection!");
	printf("** Expecting %d, got %d\n", fullPerfInt32s, numInt32s);
	return;
    }

    printableTime = ctime((time_t *) & (xstat_fs_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';
    fullPerfP = (struct fs_stats_FullPerfStats *)
	(xstat_fs_Results.data.AFS_CollData_val);

    printf
	("AFS_XSTATSCOLL_FULL_PERF_INFO (coll %d) for FS %s\n[Probe %d, %s]\n\n",
	 xstat_fs_Results.collectionNumber, xstat_fs_Results.connP->hostName,
	 xstat_fs_Results.probeNum, printableTime);

    PrintOverallPerfInfo(&(fullPerfP->overall));
    PrintDetailedPerfInfo(&(fullPerfP->det));
}


/*------------------------------------------------------------------------
 * PrintPerfInfo
 *
 * Description:
 *	Print out the AFS_XSTATSCOLL_PERF_INFO collection we just
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
PrintPerfInfo()
{
    static char rn[] = "PrintPerfInfo";	/*Routine name */
    static afs_int32 perfInt32s = (sizeof(struct afs_PerfStats) >> 2);	/*Correct # int32s to rcv */
    afs_int32 numInt32s;	/*# int32words received */
    struct afs_PerfStats *perfP;	/*Ptr to performance stats */
    char *printableTime;	/*Ptr to printable time string */

    numInt32s = xstat_fs_Results.data.AFS_CollData_len;
    if (numInt32s != perfInt32s) {
	printf("** Data size mismatch in performance collection!");
	printf("** Expecting %d, got %d\n", perfInt32s, numInt32s);
	return;
    }

    printableTime = ctime((time_t *) & (xstat_fs_Results.probeTime));
    printableTime[strlen(printableTime) - 1] = '\0';
    perfP = (struct afs_PerfStats *)
	(xstat_fs_Results.data.AFS_CollData_val);

    printf("AFS_XSTATSCOLL_PERF_INFO (coll %d) for FS %s\n[Probe %d, %s]\n\n",
	   xstat_fs_Results.collectionNumber,
	   xstat_fs_Results.connP->hostName, xstat_fs_Results.probeNum,
	   printableTime);

    PrintOverallPerfInfo(perfP);
}

static char *CbCounterStrings[] = {
    "DeleteFiles",
    "DeleteCallBacks",
    "BreakCallBacks",
    "AddCallBack",
    "GotSomeSpaces",
    "DeleteAllCallBacks",
    "nFEs", "nCBs", "nblks",
    "CBsTimedOut",
    "nbreakers",
    "GSS1", "GSS2", "GSS3", "GSS4", "GSS5"
};


void
PrintCbCounters() {
    int numInt32s = sizeof(CbCounterStrings)/sizeof(char *);
    int i;
    afs_uint32 *val=xstat_fs_Results.data.AFS_CollData_val;

    if (numInt32s > xstat_fs_Results.data.AFS_CollData_len)
	numInt32s = xstat_fs_Results.data.AFS_CollData_len;

    for (i=0; i<numInt32s; i++) {
	printf("\t%10u %s\n", val[i], CbCounterStrings[i]);
    }
}


/*------------------------------------------------------------------------
 * FS_Handler
 *
 * Description:
 *	Handler routine passed to the xstat_fs module.  This handler is
 *	called immediately after a poll of one of the File Servers has
 *	taken place.  All it needs to know is exported by the xstat_fs
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
FS_Handler()
{
    static char rn[] = "FS_Handler";	/*Routine name */

    printf
	("\n------------------------------------------------------------\n");

    /*
     * If the probe failed, there isn't much we can do except gripe.
     */
    if (xstat_fs_Results.probeOK) {
	printf("%s: Probe %d to File Server '%s' failed, code=%d\n", rn,
	       xstat_fs_Results.probeNum, xstat_fs_Results.connP->hostName,
	       xstat_fs_Results.probeOK);
	return (0);
    }

    switch (xstat_fs_Results.collectionNumber) {
    case AFS_XSTATSCOLL_CALL_INFO:
	PrintCallInfo();
	break;

    case AFS_XSTATSCOLL_PERF_INFO:
	PrintPerfInfo();
	break;

    case AFS_XSTATSCOLL_FULL_PERF_INFO:
	PrintFullPerfInfo();
	break;

    case AFS_XSTATSCOLL_CBSTATS:
	PrintCbCounters();
	break;

    default:
	printf("** Unknown collection: %d\n",
	       xstat_fs_Results.collectionNumber);
    }

    /*
     * Return the happy news.
     */
    return (0);
}


/*------------------------------------------------------------------------
 * CountListItems
 *
 * Description:
 *	Given a pointer to the list of File Servers we'll be polling
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
{

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
}


/*------------------------------------------------------------------------
 * RunTheTest
 *
 * Description:
 *	Routine called by the command line interpreter to execute the
 *	meat of the program.  We count the number of File Servers
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
RunTheTest(struct cmd_syndesc *a_s)
{
    static char rn[] = "RunTheTest";	/*Routine name */
    int code;			/*Return code */
    int numFSs;			/*# File Servers to monitor */
    int numCollIDs;		/*# collections to fetch */
    int currFS;			/*Loop index */
    int currCollIDIdx;		/*Index of current collection ID */
    afs_int32 *collIDP;		/*Ptr to array of collection IDs */
    afs_int32 *currCollIDP;	/*Ptr to current collection ID */
    struct cmd_item *curr_item;	/*Current FS cmd line record */
    struct sockaddr_in FSSktArray[20];	/*File Server socket array - FIX! */
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
     * Pull out the number of File Servers to watch and the number of
     * collections to get.
     */
    numFSs = CountListItems(a_s->parms[P_FS_NAMES].items);
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
     * Fill in the socket array for each of the File Servers listed.
     */
    curr_item = a_s->parms[P_FS_NAMES].items;
    for (currFS = 0; currFS < numFSs; currFS++) {
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
	FSSktArray[currFS].sin_family = AF_INET;	/*Internet family */
#else
	FSSktArray[currFS].sin_family = htons(AF_INET);	/*Internet family */
#endif
	FSSktArray[currFS].sin_port = htons(7000);	/*FileServer port */
	he = hostutil_GetHostByName(curr_item->data);
	if (he == NULL) {
	    fprintf(stderr, "[%s] Can't get host info for '%s'\n", rn,
		    curr_item->data);
	    exit(-1);
	}
	memcpy(&(FSSktArray[currFS].sin_addr.s_addr), he->h_addr, 4);

	/*
	 * Move to the next File Server name.
	 */
	curr_item = curr_item->next;

    }				/*Get socket info for each File Server */

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
     * Crank up the File Server prober, then sit back and have fun.
     */
    printf("\nStarting up the xstat_fs service, ");
    initFlags = 0;
    if (debugging_on) {
	initFlags |= XSTAT_FS_INITFLAG_DEBUGGING;
	printf("debugging enabled, ");
    } else
	printf("no debugging, ");
    if (one_shot) {
	initFlags |= XSTAT_FS_INITFLAG_ONE_SHOT;
	printf("one-shot operation\n");
    } else
	printf("continuous operation\n");

    code = xstat_fs_Init(numFSs,	/*Num servers */
			 FSSktArray,	/*File Server socket array */
			 freq,	/*Probe frequency */
			 FS_Handler,	/*Handler routine */
			 initFlags,	/*Initialization flags */
			 numCollIDs,	/*Number of collection IDs */
			 collIDP);	/*Ptr to collection ID array */
    if (code) {
	fprintf(stderr, "[%s] Error returned by xstat_fs_Init: %d\n", rn,
		code);
	xstat_fs_Cleanup(1);	/*Get rid of malloc'ed structures */
	exit(-1);
    }

    if (one_shot) {
	/*
	 * One-shot operation; just wait for the collection to be done.
	 */
	if (debugging_on)
	    printf("[%s] Calling LWP_WaitProcess() on event 0x%x\n", rn,
		   &terminationEvent);
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
	    ("xstat_fs service started, main thread sleeping for %d secs.\n",
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

    code = xstat_fs_Cleanup(1);	/*Get rid of malloc'ed data */
    rx_Finalize();
    return (0);
}


#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    static char rn[] = "xstat_fs_test";	/*Routine name */
    register afs_int32 code;	/*Return code */
    struct cmd_syndesc *ts;	/*Ptr to cmd line syntax desc */

    /*
     * Set up the commands we understand.
     */
    ts = cmd_CreateSyntax("initcmd", RunTheTest, 0, "initialize the program");
    cmd_AddParm(ts, "-fsname", CMD_LIST, CMD_REQUIRED,
		"File Server name(s) to monitor");
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
}
