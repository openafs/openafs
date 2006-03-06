/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _xstat_cm_h_
#define	_xstat_cm_h_  1

/*------------------------------------------------------------------------
 * xstat_cm.h
 *
 * Interface to the AFS Cache Manager extended statistics facility.  With
 * the routines defined here, the importer can gather extended statistics
 * from the given group of Cache Managers at regular intervals, or force
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
#include <afs/afscbint.h>	/*AFS CM callback interface */
#define FSINT_COMMON_XG		/* to allow the inclusion of
				 * xstat_cm.h with this file in an application. */
#include <afs/afs_stats.h>	/*AFS statistics interface */

/*
 * ---------------------- Exported definitions ------------------------
 */
/*
 * Define the initialization flags used within the xstat_fs_Init() call.
 *	XSTAT_CM_INITFLAG_DEBUGGING	Turn debugging output on?
 *	XSTAT_CM_INITFLAG_ONE_SHOT	Do a one-shot collection?
 */
#define XSTAT_CM_INITFLAG_DEBUGGING	0x1
#define XSTAT_CM_INITFLAG_ONE_SHOT	0x2


/*
 * ----------------------- Exported structures ------------------------
 */
/*
 * Connection information per Cache Manager host being probed.
 */
struct xstat_cm_ConnectionInfo {
    struct sockaddr_in skt;	/*Socket info */
    struct rx_connection *rxconn;	/*Rx connection */
    char hostName[256];		/*Computed hostname */
};

/*
 * The results of a probe of one of the Cache Managers in the set being
 * watched.
 */
struct xstat_cm_ProbeResults {
    int probeNum;		/*Probe number */
    afs_int32 probeTime;	/*Time probe initiated */
    struct xstat_cm_ConnectionInfo *connP;	/*Connection polled */
    afs_int32 collectionNumber;	/*Collection received */
    AFSCB_CollData data;	/*Ptr to data collected */
    int probeOK;		/*Latest probe successful? */
};

/*
 * ------------------- Externally-visible variables -------------------
 */
extern int xstat_cm_numServers;	/*# connected servers */
extern struct xstat_cm_ConnectionInfo
 *xstat_cm_ConnInfo;		/*Ptr to connections */
extern int numCollections;	/*Num data collections */
extern struct xstat_cm_ProbeResults
  xstat_cm_Results;		/*Latest probe results */
extern char terminationEvent;	/*One-shot termination event */

/*
 * ------------------------ Exported functions ------------------------
 */
extern int xstat_cm_Init();
    /*
     * Summary:
     *    Initialize the xstat_cm module: set up Rx connections to the
     *    given set of Cache Managers, start up the probe LWP, and
     *    associate the routine to be called when a probe completes.
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

extern int xstat_cm_ForceProbeNow();
    /*
     * Summary:
     *    Force an immediate probe to the connected Cache Managers.
     *
     * Args:
     *    None.
     *
     * Returns:
     *    0 on success,
     *    Error value otherwise.
     */

extern int xstat_cm_Cleanup();
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
     *          with the xstat_cm connection array.
     */

#endif /* _xstat_cm_h_ */
