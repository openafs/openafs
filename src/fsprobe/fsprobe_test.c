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
 *	Test of the fsprobe module.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <fsprobe.h>		/*Interface for fsprobe module */

/*
  * External routines that don't have explicit include file definitions.
  */
extern struct hostent *hostutil_GetHostByName();

/*------------------------------------------------------------------------
 * FS_Handler
 *
 * Description:
 *	Handler routine passed to the fsprobe module.  This handler is
 *	called immediately after a poll of all the FileServers has taken
 *	place.  All it needs to know is exported by the fsprobe module,
 *	namely the data structure where the probe results are stored.
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
{				/*FS_Handler */

    static char rn[] = "FS_Handler";	/*Routine name */
    struct ProbeViceStatistics *curr_stats;	/*Ptr to current stats */
    int curr_srvidx, i;		/*Current server index */
    int *curr_probeOK;		/*Ptr to current probeOK value */

    printf("[%s] Called; results of poll %d are:\n", rn,
	   fsprobe_Results.probeNum);
    curr_stats = fsprobe_Results.stats;
    curr_probeOK = fsprobe_Results.probeOK;

    for (curr_srvidx = 0; curr_srvidx < 3; curr_srvidx++) {
	printf("\nServer %s:\n", (fsprobe_ConnInfo + curr_srvidx)->hostName);
	printf("\tValue of probeOK for this server: %d\n", *curr_probeOK);
	printf("\tCurrentMsgNumber:\t%d\n", curr_stats->CurrentMsgNumber);
	printf("\tOldestMsgNumber:\t%d\n", curr_stats->OldestMsgNumber);
	printf("\tCurrentTime:\t%d\n", curr_stats->CurrentTime);
	printf("\tBootTime:\t%d\n", curr_stats->BootTime);
	printf("\tStartTime:\t%d\n", curr_stats->StartTime);
	printf("\tCurrentConnections:\t%d\n", curr_stats->CurrentConnections);
	printf("\tTotalViceCalls:\t%d\n", curr_stats->TotalViceCalls);
	printf("\tTotalFetchs:\t%d\n", curr_stats->TotalFetchs);
	printf("\tFetchDatas:\t%d\n", curr_stats->FetchDatas);
	printf("\tFetchedBytes:\t%d\n", curr_stats->FetchedBytes);
	printf("\tFetchDataRate:\t%d\n", curr_stats->FetchDataRate);
	printf("\tTotalStores:\t%d\n", curr_stats->TotalStores);
	printf("\tStoreDatas:\t%d\n", curr_stats->StoreDatas);
	printf("\tStoredBytes:\t%d\n", curr_stats->StoredBytes);
	printf("\tStoreDataRate:\t%d\n", curr_stats->StoreDataRate);
	printf("\tTotalRPCBytesSent:\t%d\n", curr_stats->TotalRPCBytesSent);
	printf("\tTotalRPCBytesReceived:\t%d\n",
	       curr_stats->TotalRPCBytesReceived);
	printf("\tTotalRPCPacketsSent:\t%d\n",
	       curr_stats->TotalRPCPacketsSent);
	printf("\tTotalRPCPacketsReceived:\t%d\n",
	       curr_stats->TotalRPCPacketsReceived);
	printf("\tTotalRPCPacketsLost:\t%d\n",
	       curr_stats->TotalRPCPacketsLost);
	printf("\tTotalRPCBogusPackets:\t%d\n",
	       curr_stats->TotalRPCBogusPackets);
	printf("\tSystemCPU:\t%d\n", curr_stats->SystemCPU);
	printf("\tUserCPU:\t%d\n", curr_stats->UserCPU);
	printf("\tNiceCPU:\t%d\n", curr_stats->NiceCPU);
	printf("\tIdleCPU:\t%d\n", curr_stats->IdleCPU);
	printf("\tTotalIO:\t%d\n", curr_stats->TotalIO);
	printf("\tActiveVM:\t%d\n", curr_stats->ActiveVM);
	printf("\tTotalVM:\t%d\n", curr_stats->TotalVM);
	printf("\tEtherNetTotalErrors:\t%d\n",
	       curr_stats->EtherNetTotalErrors);
	printf("\tEtherNetTotalWrites:\t%d\n",
	       curr_stats->EtherNetTotalWrites);
	printf("\tEtherNetTotalInterupts:\t%d\n",
	       curr_stats->EtherNetTotalInterupts);
	printf("\tEtherNetGoodReads:\t%d\n", curr_stats->EtherNetGoodReads);
	printf("\tEtherNetTotalBytesWritten:\t%d\n",
	       curr_stats->EtherNetTotalBytesWritten);
	printf("\tEtherNetTotalBytesRead:\t%d\n",
	       curr_stats->EtherNetTotalBytesRead);
	printf("\tProcessSize:\t%d\n", curr_stats->ProcessSize);
	printf("\tWorkStations:\t%d\n", curr_stats->WorkStations);
	printf("\tActiveWorkStations:\t%d\n", curr_stats->ActiveWorkStations);
	printf("\tSpare1:\t%d\n", curr_stats->Spare1);
	printf("\tSpare2:\t%d\n", curr_stats->Spare2);
	printf("\tSpare3:\t%d\n", curr_stats->Spare3);
	printf("\tSpare4:\t%d\n", curr_stats->Spare4);
	printf("\tSpare5:\t%d\n", curr_stats->Spare5);
	printf("\tSpare6:\t%d\n", curr_stats->Spare6);
	printf("\tSpare7:\t%d\n", curr_stats->Spare7);
	printf("\tSpare8:\t%d\n", curr_stats->Spare8);

	for (i = 0; i < 26; i++) {
	    printf("\tDisk %d: blocks avail=%d, ttl blocks=%d, name=%s\n", i,
		   curr_stats->Disk[i].BlocksAvailable,
		   curr_stats->Disk[i].TotalBlocks, curr_stats->Disk[i].Name);
	}
	/*
	 * Bump out statistics pointer to the next server's region.  Ditto
	 * for probeOK;
	 */
	curr_stats++;
	curr_probeOK++;

    }				/*For each server */

    /*
     * Return the happy news.
     */
    return (0);

}				/*FS_Handler */

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;

{				/*Main routine */

    static char rn[] = "fsprobe_test";	/*Routine name */
    register afs_int32 code;	/*Return code */
    struct sockaddr_in FSSktArray[3];	/*socket array */
    struct hostent *he;		/*Host entry */
    struct timeval tv;		/*Time structure */
    int sleep_secs;		/*Number of seconds to sleep */

#ifdef AFS_RXK5
    initialize_RXK5_error_table();
#endif

    printf("\n\nTest of the fsprobe facility.\n\n");

    /*
     * Fill in the socket array for bigbird, vice1, and vice2.
     */
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
    FSSktArray[0].sin_family = AF_INET;		/*Internet family */
#else
    FSSktArray[0].sin_family = htons(AF_INET);	/*Internet family */
#endif
    FSSktArray[0].sin_port = htons(7000);	/*FileServer port */
    he = hostutil_GetHostByName("servername1");
    if (he == NULL) {
	fprintf(stderr, "[%s] Can't get host info for servername1\n", rn);
	exit(-1);
    }
    memcpy(&(FSSktArray[0].sin_addr.s_addr), he->h_addr, 4);

#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
    FSSktArray[1].sin_family = AF_INET;		/*Internet address family */
#else
    FSSktArray[1].sin_family = htons(AF_INET);	/*Internet address family */
#endif
    FSSktArray[1].sin_port = htons(7000);	/*FileServer port */
    he = hostutil_GetHostByName("servername2");
    if (he == NULL) {
	fprintf(stderr, "[%s] Can't get host info for servername2\n", rn);
	exit(-1);
    }
    memcpy(&(FSSktArray[1].sin_addr.s_addr), he->h_addr, 4);

#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
    FSSktArray[2].sin_family = AF_INET;		/*Internet address family */
#else
    FSSktArray[2].sin_family = htons(AF_INET);	/*Internet address family */
#endif
    FSSktArray[2].sin_port = htons(7000);	/*FileServer port */
    he = hostutil_GetHostByName("servername3");
    if (he == NULL) {
	fprintf(stderr, "[%s] Can't get host info for servername3\n", rn);
	exit(-1);
    }
    memcpy(&(FSSktArray[2].sin_addr.s_addr), he->h_addr, 4);

    printf("Sockets for the 3 AFS FileServers to be probed:\n");
    printf("\t Host servername1: IP addr 0x%lx, port %d\n",
	   FSSktArray[0].sin_addr.s_addr, FSSktArray[0].sin_port);
    printf("\t Host servername2: IP addr 0x%lx, port %d\n",
	   FSSktArray[1].sin_addr.s_addr, FSSktArray[1].sin_port);
    printf("\t Host servername3: IP addr 0x%lx, port %d\n",
	   FSSktArray[2].sin_addr.s_addr, FSSktArray[2].sin_port);

    /*
     * Crank up the FileServer prober, then sit back and have fun.
     */
    printf("Starting up the fsprobe service\n");
    code = fsprobe_Init(3,	/*Num servers */
			FSSktArray,	/*FileServer socket array */
			30,	/*Probe every 30 seconds */
			FS_Handler,	/*Handler routine */
			1);	/*Turn debugging output on */
    if (code) {
	fprintf(stderr, "[%s] Error returned by fsprobe_Init: %d\n", rn,
		code);
	fsprobe_Cleanup(1);	/*Get rid of malloc'ed structures */
	exit(-1);
    }
    sleep_secs = 60 * 10;	/*Allow 10 minutes of data collection */
    printf
	("Fsprobe service started, main thread sleeping for %d seconds...\n",
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
	fprintf(stderr, "[%s] IOMGR_Select() returned non-zero value: %d\n",
		rn, code);
    }

    /*
     * We're all done.  Clean up, put the last nail in Rx, then
     * exit happily.
     */
    printf("Yawn, main thread just woke up.  Cleaning things out...\n");
    code = fsprobe_Cleanup(1);	/*Get rid of malloc'ed data */
    rx_Finalize();
    exit(0);

}				/*Main routine */
