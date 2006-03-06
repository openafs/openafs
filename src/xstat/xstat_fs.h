/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _xstat_fs_h_
#define	_xstat_fs_h_  1

/*------------------------------------------------------------------------
 * xstat_fs.h
 *
 * Interface to the AFS File Server extended statistics facility.  With
 * the routines defined here, the importer can gather extended statistics
 * from the given group of File Servers at regular intervals, or force
 * immediate collection.
 *
 *------------------------------------------------------------------------*/

#include <sys/types.h>		/*Basic system types */
#ifndef AFS_NT40_ENV
#ifndef	IPPROTO_IP
#include <netinet/in.h>		/*Internet definitions */
#endif

#ifndef _netdb_h_
#define _netdb_h_
#include <netdb.h>		/*Network database library */
#endif

#ifndef _socket_h_
#define _socket_h_
#include <sys/socket.h>		/*Socket definitions */
#endif
#endif /* AFS_NT40_ENV */

#include <rx/rx.h>		/*Rx definitions */
#include <afs/afsint.h>		/*AFS FileServer interface */
#define FSINT_COMMON_XG		/* to allow the inclusion of
				 * xstat_cm.h with this file in an application. */
#include <afs/fs_stats.h>	/*AFS FileServer statistics interface */


/*
 * ---------------------- Exported definitions ------------------------
 */
/*
 * Define the initialization flags used within the xstat_fs_Init() call.
 *	XSTAT_FS_INITFLAG_DEBUGGING	Turn debugging output on?
 *	XSTAT_FS_INITFLAG_ONE_SHOT	Do a one-shot collection?
 */
#define XSTAT_FS_INITFLAG_DEBUGGING	0x1
#define XSTAT_FS_INITFLAG_ONE_SHOT	0x2


/*
 * ----------------------- Exported structures ------------------------
 */
/*
 * Connection information per File Server host being probed.
 */
struct xstat_fs_ConnectionInfo {
    struct sockaddr_in skt;	/*Socket info */
    struct rx_connection *rxconn;	/*Rx connection */
    char hostName[256];		/*Computed hostname */
};

/*
 * The results of a probe of one of the File Servers in the set being
 * watched.
 */
struct xstat_fs_ProbeResults {
    int probeNum;		/*Probe number */
    afs_int32 probeTime;	/*Time probe initiated */
    struct xstat_fs_ConnectionInfo *connP;	/*Connection polled */
    afs_int32 collectionNumber;	/*Collection received */
    AFS_CollData data;		/*Ptr to data collected */
    int probeOK;		/*Latest probe successful? */
};

/*
 * ------------------- Externally-visible variables -------------------
 */
extern int xstat_fs_numServers;	/*# connected servers */
extern struct xstat_fs_ConnectionInfo
 *xstat_fs_ConnInfo;		/*Ptr to connections */
extern int numCollections;	/*Num data collections */
extern struct xstat_fs_ProbeResults
  xstat_fs_Results;		/*Latest probe results */
extern char terminationEvent;	/*One-shot termination event */

/*
 * ------------------------ Exported functions ------------------------
 */
extern int xstat_fs_Init();
    /*
     * Summary:
     *    Initialize the xstat_fs module: set up Rx connections to the
     *    given set of File Servers, start up the probe and callback LWPs,
     *    and associate the routine to be called when a probe completes.
     *    Also, let it know which collections you're interested in.
     *
     * Args:
     *    int a_numServers                  : Num. servers to connect.
     *    struct sockaddr_in *a_socketArray : Array of server sockets.
     *    int a_ProbeFreqInSecs             : Probe frequency in seconds.
     *    int (*a_ProbeHandler)()           : Ptr to probe handler fcn.
     *    int a_flags                       : Various flags.
     *    int a_numCollections              : Number of collections desired.
     *    afs_int32 *a_collIDP              : Ptr to collection IDs.
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int xstat_fs_ForceProbeNow();
    /*
     * Summary:
     *    Force an immediate probe to the connected File Servers.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int xstat_fs_Cleanup();
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
     *          with the xstat_fs connection array.
     */

#endif /* _xstat_fs_h_ */
