/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __fsprobe_h
#define	__fsprobe_h  1

/*------------------------------------------------------------------------
 * fsprobe.h
 *
 * Interface to the AFS FileServer probe facility.  With the routines
 * defined here, the importer can gather statistics from the given set
 * of FileServers at regular intervals, or force immediate collection.
 *
 *------------------------------------------------------------------------*/

#include <sys/types.h>		/*Basic system types */
#include <netinet/in.h>		/*Internet definitions */
#include <netdb.h>		/*Network database library */
#include <sys/socket.h>		/*Socket definitions */
#include <rx/rx.h>		/*Rx definitions */
#include <afs/afsint.h>		/*AFS FileServer interface */
#include <afs/volser.h>
#include <afs/volint.h>

struct ProbeViceStatistics {
    afs_uint32 CurrentMsgNumber;
    afs_uint32 OldestMsgNumber;
    afs_uint32 CurrentTime;
    afs_uint32 BootTime;
    afs_uint32 StartTime;
    afs_int32 CurrentConnections;
    afs_uint32 TotalViceCalls;
    afs_uint32 TotalFetchs;
    afs_uint32 FetchDatas;
    afs_uint32 FetchedBytes;
    afs_int32 FetchDataRate;
    afs_uint32 TotalStores;
    afs_uint32 StoreDatas;
    afs_uint32 StoredBytes;
    afs_int32 StoreDataRate;
    afs_uint32 TotalRPCBytesSent;
    afs_uint32 TotalRPCBytesReceived;
    afs_uint32 TotalRPCPacketsSent;
    afs_uint32 TotalRPCPacketsReceived;
    afs_uint32 TotalRPCPacketsLost;
    afs_uint32 TotalRPCBogusPackets;
    afs_int32 SystemCPU;
    afs_int32 UserCPU;
    afs_int32 NiceCPU;
    afs_int32 IdleCPU;
    afs_int32 TotalIO;
    afs_int32 ActiveVM;
    afs_int32 TotalVM;
    afs_int32 EtherNetTotalErrors;
    afs_int32 EtherNetTotalWrites;
    afs_int32 EtherNetTotalInterupts;
    afs_int32 EtherNetGoodReads;
    afs_int32 EtherNetTotalBytesWritten;
    afs_int32 EtherNetTotalBytesRead;
    afs_int32 ProcessSize;
    afs_int32 WorkStations;
    afs_int32 ActiveWorkStations;
    afs_int32 Spare1;
    afs_int32 Spare2;
    afs_int32 Spare3;
    afs_int32 Spare4;
    afs_int32 Spare5;
    afs_int32 Spare6;
    afs_int32 Spare7;
    afs_int32 Spare8;
    ViceDisk Disk[VOLMAXPARTS];
};


/*
  * Connection information per FileServer host being probed.
  */
struct fsprobe_ConnectionInfo {
    struct sockaddr_in skt;	/*Socket info */
    struct rx_connection *rxconn;	/*Rx connection */
    struct rx_connection *rxVolconn;	/*Rx connection to Vol server */
    struct partList partList;	/*Server part list */
    afs_int32 partCnt;		/*# of parts */
    char hostName[256];		/*Computed hostname */
};

/*
  * The results of a probe of the given set of FileServers.  The ith
  * entry in the stats array corresponds to the ith connected server.
  */
struct fsprobe_ProbeResults {
    int probeNum;		/*Probe number */
    afs_int32 probeTime;	/*Time probe initiated */
    struct ProbeViceStatistics *stats;	/*Ptr to stats array for servers */
    int *probeOK;		/*Array: was latest probe successful? */
};

extern int fsprobe_numServers;	/*# servers connected */
extern struct fsprobe_ConnectionInfo *fsprobe_ConnInfo;	/*Ptr to connections */
extern int numCollections;	/*Num data collections */
extern struct fsprobe_ProbeResults fsprobe_Results;	/*Latest probe results */

extern int fsprobe_Init(int, struct sockaddr_in *, int, int (*)(void), int );
    /*
     * Summary:
     *    Initialize the fsprobe module: set up Rx connections to the
     *    given set of servers, start up the probe and callback LWPs,
     *    and associate the routine to be called when a probe completes.
     *
     * Args:
     *    int a_numServers                  : Num. servers to connect to.
     *    struct sockaddr_in *a_socketArray : Array of server sockets.
     *    int a_ProbeFreqInSecs             : Probe frequency in seconds.
     *    int (*a_ProbeHandler)()           : Ptr to probe handler fcn.
     *    int a_debug                       : Turn debugging output on?
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int fsprobe_ForceProbeNow(void);
    /*
     * Summary:
     *    Force an immediate probe to the connected FileServers.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int fsprobe_Cleanup(int);
    /*
     * Summary:
     *    Clean up our memory and connection state.
     *
     * Args:
     *    int a_releaseMem : Should we free up malloc'ed areas?
     *
     * Returns:
     *    0 on total success,
     *    -1 if the module was never initialized, or there was a problem
     *          with the fsprobe connection array.
     */

#endif /* __fsprobe_h */
