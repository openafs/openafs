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
 *	Implementation of the client side of the AFS File Server extended
 *	statistics facility.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include "xstat_fs.h"		/*Interface for this module */
#include <lwp.h>		/*Lightweight process package */

#include <afs/afsutil.h>
#include <string.h>

#define LWP_STACK_SIZE	(16 * 1024)

/*
 * Routines we need that don't have explicit include file definitions.
 */
extern int RXAFSCB_ExecuteRequest();	/*AFS callback dispatcher */
extern char *hostutil_GetNameByINet();	/*Host parsing utility */

/*
 * Help out the linker by explicitly importing the callback routines
 * File Servers may be lobbing at us.
 */
extern afs_int32 SRXAFSCB_CallBack();
extern afs_int32 SRXAFSCB_InitCallBackState3();
extern afs_int32 SRXAFSCB_Probe();
extern afs_int32 SRXAFSCB_ProbeUUID();
extern afs_int32 SRXAFSCB_GetCE();
extern afs_int32 SRXAFSCB_GetLock();

/*
 * Exported variables.
 */
int xstat_fs_numServers;	/*Num connected servers */
struct xstat_fs_ConnectionInfo
 *xstat_fs_ConnInfo;		/*Ptr to connection array */
int numCollections;		/*Number of data collections */
struct xstat_fs_ProbeResults xstat_fs_Results;	/*Latest probe results */
char terminationEvent;		/*One-shot termination event */

afs_int32 xstat_fsData[AFS_MAX_XSTAT_LONGS];	/*Buffer for collected data */

/*
 * Private globals.
 */
static int xstat_fs_ProbeFreqInSecs;	/*Probe freq. in seconds */
static int xstat_fs_initflag = 0;	/*Was init routine called? */
static int xstat_fs_debug = 0;	/*Debugging output enabled? */
static int xstat_fs_oneShot = 0;	/*One-shot operation? */
static int (*xstat_fs_Handler) ();	/*Probe handler routine */
static PROCESS probeLWP_ID;	/*Probe LWP process ID */
static int xstat_fs_numCollections;	/*Number of desired collections */
static afs_int32 *xstat_fs_collIDP;	/*Ptr to collection IDs desired */

/*
 * We have to pass a port to Rx to start up our callback listener
 * service, but 7001 is already taken up by the Cache Manager.  So,
 * we make up our own.
 */
#define XSTAT_FS_CBPORT	7101


/*------------------------------------------------------------------------
 * [private] xstat_fs_CleanupInit
 *
 * Description:
 *	Set up for recovery after an error in initialization (i.e.,
 *	during a call to xstat_fs_Init.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	This routine is private to the module.
 *
 * Side Effects:
 *	Zeros out basic data structures.
 *------------------------------------------------------------------------*/

static int
xstat_fs_CleanupInit()
{
    afs_int32 code;		/*Return code from callback stubs */
    struct rx_call *rxcall;	/*Bogus param */
    AFSCBFids *Fids_Array;	/*Bogus param */
    AFSCBs *CallBack_Array;	/*Bogus param */

    xstat_fs_ConnInfo = (struct xstat_fs_ConnectionInfo *)0;
    xstat_fs_Results.probeNum = 0;
    xstat_fs_Results.probeTime = 0;
    xstat_fs_Results.connP = (struct xstat_fs_ConnectionInfo *)0;
    xstat_fs_Results.collectionNumber = 0;
    xstat_fs_Results.data.AFS_CollData_len = AFS_MAX_XSTAT_LONGS;
    xstat_fs_Results.data.AFS_CollData_val = (afs_int32 *) xstat_fsData;
    xstat_fs_Results.probeOK = 0;

    rxcall = (struct rx_call *)0;
    Fids_Array = (AFSCBFids *) 0;
    CallBack_Array = (AFSCBs *) 0;

    /*
     * Call each of the callback routines our module provides (in
     * xstat_fs_callback.c) to make sure they're all there.
     */
    code = SRXAFSCB_CallBack(rxcall, Fids_Array, CallBack_Array);
    if (code)
	return (code);
    code = SRXAFSCB_InitCallBackState3(rxcall, (afsUUID *) 0);
    if (code)
	return (code);
    code = SRXAFSCB_Probe(rxcall);
    return (code);
}


/*------------------------------------------------------------------------
 * [exported] xstat_fs_Cleanup
 *
 * Description:
 *	Clean up our memory and connection state.
 *
 * Arguments:
 *	int a_releaseMem : Should we free up malloc'ed areas?
 *
 * Returns:
 *	0 on total success,
 *	-1 if the module was never initialized, or there was a problem
 *		with the xstat_fs connection array.
 *
 * Environment:
 *	xstat_fs_numServers should be properly set.  We don't do anything
 *	unless xstat_fs_Init() has already been called.
 *
 * Side Effects:
 *	Shuts down Rx connections gracefully, frees allocated space
 *	(if so directed).
 *------------------------------------------------------------------------*/

int
xstat_fs_Cleanup(int a_releaseMem)
{
    static char rn[] = "xstat_fs_Cleanup";	/*Routine name */
    int code;			/*Return code */
    int conn_idx;		/*Current connection index */
    struct xstat_fs_ConnectionInfo *curr_conn;	/*Ptr to xstat_fs connection */

    /*
     * Assume the best, but check the worst.
     */
    if (!xstat_fs_initflag) {
	fprintf(stderr, "[%s] Refused; module not initialized\n", rn);
	return (-1);
    } else
	code = 0;

    /*
     * Take care of all Rx connections first.  Check to see that the
     * server count is a legal value.
     */
    if (xstat_fs_numServers <= 0) {
	fprintf(stderr,
		"[%s] Illegal number of servers (xstat_fs_numServers = %d)\n",
		rn, xstat_fs_numServers);
	code = -1;
    } else {
	if (xstat_fs_ConnInfo != (struct xstat_fs_ConnectionInfo *)0) {
	    /*
	     * The xstat_fs connection structure array exists.  Go through
	     * it and close up any Rx connections it holds.
	     */
	    curr_conn = xstat_fs_ConnInfo;
	    for (conn_idx = 0; conn_idx < xstat_fs_numServers; conn_idx++) {
		if (curr_conn->rxconn != (struct rx_connection *)0) {
		    rx_DestroyConnection(curr_conn->rxconn);
		    curr_conn->rxconn = (struct rx_connection *)0;
		}
		curr_conn++;
	    }			/*for each xstat_fs connection */
	}			/*xstat_fs connection structure exists */
    }				/*Legal number of servers */

    /*
     * If asked to, release the space we've allocated.
     */
    if (a_releaseMem) {
	if (xstat_fs_ConnInfo != (struct xstat_fs_ConnectionInfo *)0)
	    free(xstat_fs_ConnInfo);
    }

    /*
     * Return the news, whatever it is.
     */
    return (code);
}


/*------------------------------------------------------------------------
 * [private] xstat_fs_LWP
 *
 * Description:
 *	This LWP iterates over the server connections and gathers up
 *	the desired statistics from each one on a regular basis.  When
 *	the sweep is done, the associated handler function is called
 *	to process the new data.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Started by xstat_fs_Init(), uses global structures and the
 *	global private xstat_fs_oneShot variable.
 *
 * Side Effects:
 *	Nothing interesting.
 *------------------------------------------------------------------------*/

static void
xstat_fs_LWP()
{
    static char rn[] = "xstat_fs_LWP";	/*Routine name */
    register afs_int32 code;	/*Results of calls */
    int oneShotCode;		/*Result of one-shot signal */
    struct timeval tv;		/*Time structure */
    int conn_idx;		/*Connection index */
    struct xstat_fs_ConnectionInfo *curr_conn;	/*Current connection */
    afs_int32 srvVersionNumber;	/*Xstat version # */
    afs_int32 clientVersionNumber;	/*Client xstat version */
    afs_int32 numColls;		/*Number of collections to get */
    afs_int32 *currCollIDP;	/*Curr collection ID desired */

    static afs_int32 xstat_VersionNumber;	/*Version # of server */

    /*
     * Set up some numbers we'll need.
     */
    clientVersionNumber = AFS_XSTAT_VERSION;

    while (1) {			/*Service loop */
	/*
	 * Iterate through the server connections, gathering data.
	 * Don't forget to bump the probe count and zero the statistics
	 * areas before calling the servers.
	 */
	if (xstat_fs_debug)
	    printf("[%s] Waking up, getting data from %d server(s)\n", rn,
		   xstat_fs_numServers);
	curr_conn = xstat_fs_ConnInfo;
	xstat_fs_Results.probeNum++;

	for (conn_idx = 0; conn_idx < xstat_fs_numServers; conn_idx++) {
	    /*
	     * Grab the statistics for the current File Server, if the
	     * connection is valid.
	     */
	    if (xstat_fs_debug)
		printf("[%s] Getting collections from File Server '%s'\n", rn,
		       curr_conn->hostName);
	    if (curr_conn->rxconn != (struct rx_connection *)0) {
		if (xstat_fs_debug)
		    printf("[%s] Connection OK, calling RXAFS_GetXStats\n",
			   rn);

		currCollIDP = xstat_fs_collIDP;
		for (numColls = 0; numColls < xstat_fs_numCollections;
		     numColls++, currCollIDP++) {
		    /*
		     * Initialize the per-probe values.
		     */
		    if (xstat_fs_debug)
			printf("[%s] Asking for data collection %d\n", rn,
			       *currCollIDP);
		    xstat_fs_Results.collectionNumber = *currCollIDP;
		    xstat_fs_Results.data.AFS_CollData_len =
			AFS_MAX_XSTAT_LONGS;
		    memset(xstat_fs_Results.data.AFS_CollData_val, 0,
			   AFS_MAX_XSTAT_LONGS * 4);

		    xstat_fs_Results.connP = curr_conn;

		    if (xstat_fs_debug) {
			printf
			    ("%s: Calling RXAFS_GetXStats, conn=0x%x, clientVersionNumber=%d, collectionNumber=%d, srvVersionNumberP=0x%x, timeP=0x%x, dataP=0x%x\n",
			     rn, curr_conn->rxconn, clientVersionNumber,
			     *currCollIDP, &srvVersionNumber,
			     &(xstat_fs_Results.probeTime),
			     &(xstat_fs_Results.data));
			printf("%s: [bufflen=%d, buffer at 0x%x]\n", rn,
			       xstat_fs_Results.data.AFS_CollData_len,
			       xstat_fs_Results.data.AFS_CollData_val);
		    }

		    xstat_fs_Results.probeOK =
			RXAFS_GetXStats(curr_conn->rxconn,
					clientVersionNumber, *currCollIDP,
					&srvVersionNumber,
					&(xstat_fs_Results.probeTime),
					&(xstat_fs_Results.data));

		    /*
		     * Now that we (may) have the data for this connection,
		     * call the associated handler function.  The handler does
		     * not take any explicit parameters, but rather gets to the
		     * goodies via some of the objects exported by this module.
		     */
		    if (xstat_fs_debug)
			printf("[%s] Calling handler routine.\n", rn);
		    code = xstat_fs_Handler();
		    if (code)
			fprintf(stderr,
				"[%s] Handler returned error code %d\n", rn,
				code);

		}		/*For each collection */
	    }

	    /*Valid Rx connection */
	    /*
	     * Advance the xstat_fs connection pointer.
	     */
	    curr_conn++;

	}			/*For each xstat_fs connection */

	/*
	 * All (valid) connections have been probed.  Fall asleep for the
	 * prescribed number of seconds, unless we're a one-shot.  In
	 * that case, we need to signal our caller that we're done.
	 */
	if (xstat_fs_debug)
	    printf("[%s] Polling complete for probe round %d.\n", rn,
		   xstat_fs_Results.probeNum);

	if (xstat_fs_oneShot) {
	    /*
	     * One-shot execution desired.  Signal our main procedure
	     * that we've finished our collection round.
	     */
	    if (xstat_fs_debug)
		printf("[%s] Signalling main process at 0x%x\n", rn,
		       &terminationEvent);
	    oneShotCode = LWP_SignalProcess(&terminationEvent);
	    if (oneShotCode)
		fprintf(stderr, "[%s] Error %d from LWP_SignalProcess()", rn,
			oneShotCode);
	    break;		/*from the perpetual while loop */
	} /*One-shot execution */
	else {
	    /*
	     * Continuous execution desired.  Sleep for the required
	     * number of seconds.
	     */
	    tv.tv_sec = xstat_fs_ProbeFreqInSecs;
	    tv.tv_usec = 0;
	    if (xstat_fs_debug)
		printf("[%s] Falling asleep for %d seconds\n", rn,
		       xstat_fs_ProbeFreqInSecs);
	    code = IOMGR_Select(0,	/*Num fids */
				0,	/*Descs ready for reading */
				0,	/*Descs ready for writing */
				0,	/*Descs w/exceptional conditions */
				&tv);	/*Ptr to timeout structure */
	    if (code)
		fprintf(stderr, "[%s] IOMGR_Select returned code %d\n", rn,
			code);
	}			/*Continuous execution */
    }				/*Service loop */
}

/*------------------------------------------------------------------------
 * [exported] xstat_fs_Init
 *
 * Description:
 *	Initialize the xstat_fs module: set up Rx connections to the
 *	given set of File Servers, start up the probe and callback LWPs,
 *	and associate the routine to be called when a probe completes.
 *	Also, let it know which collections you're interested in.
 *
 * Arguments:
 *	int a_numServers		  : Num. servers to connect to.
 *	struct sockaddr_in *a_socketArray : Array of server sockets.
 *	int a_ProbeFreqInSecs		  : Probe frequency in seconds.
 *	int (*a_ProbeHandler)()		  : Ptr to probe handler fcn.
 *	int a_flags			  : Various flags.
 *	int a_numCollections		  : Number of collections desired.
 *	afs_int32 *a_collIDP			  : Ptr to collection IDs.
 *
 * Returns:
 *	0 on success,
 *	-2 for (at least one) connection error,
 *	LWP process creation code, if it failed,
 *	-1 for other fatal errors.
 *
 * Environment:
 *	*** MUST BE THE FIRST ROUTINE CALLED FROM THIS PACKAGE ***
 *	Also, the server security object CBsecobj MUST be a static,
 *	since it has to stick around after this routine exits.
 *
 * Side Effects:
 *	Sets up just about everything.
 *------------------------------------------------------------------------*/

int
xstat_fs_Init(int a_numServers, struct sockaddr_in *a_socketArray,
	      int a_ProbeFreqInSecs, int (*a_ProbeHandler) (), int a_flags,
	      int a_numCollections, afs_int32 * a_collIDP)
{
    static char rn[] = "xstat_fs_Init";	/*Routine name */
    register afs_int32 code;	/*Return value */
    static struct rx_securityClass *CBsecobj;	/*Callback security object */
    struct rx_securityClass *secobj;	/*Client security object */
    struct rx_service *rxsrv_afsserver;	/*Server for AFS */
    int arg_errfound;		/*Argument error found? */
    int curr_srv;		/*Current server idx */
    struct xstat_fs_ConnectionInfo *curr_conn;	/*Ptr to current conn */
    char *hostNameFound;	/*Ptr to returned host name */
    int conn_err;		/*Connection error? */
    int PortToUse;		/*Callback port to use */
    int collIDBytes;		/*Num bytes in coll ID array */
    char hoststr[16];

    /*
     * If we've already been called, snicker at the bozo, gently
     * remind him of his doubtful heritage, and return success.
     */
    if (xstat_fs_initflag) {
	fprintf(stderr, "[%s] Called multiple times!\n", rn);
	return (0);
    } else
	xstat_fs_initflag = 1;

    /*
     * Check the parameters for bogosities.
     */
    arg_errfound = 0;
    if (a_numServers <= 0) {
	fprintf(stderr, "[%s] Illegal number of servers: %d\n", rn,
		a_numServers);
	arg_errfound = 1;
    }
    if (a_socketArray == (struct sockaddr_in *)0) {
	fprintf(stderr, "[%s] Null server socket array argument\n", rn);
	arg_errfound = 1;
    }
    if (a_ProbeFreqInSecs <= 0) {
	fprintf(stderr, "[%s] Illegal probe frequency: %d\n", rn,
		a_ProbeFreqInSecs);
	arg_errfound = 1;
    }
    if (a_ProbeHandler == (int (*)())0) {
	fprintf(stderr, "[%s] Null probe handler function argument\n", rn);
	arg_errfound = 1;
    }
    if (a_numCollections <= 0) {
	fprintf(stderr, "[%s] Illegal collection count argument: %d\n", rn,
		a_numServers);
	arg_errfound = 1;
    }
    if (a_collIDP == NULL) {
	fprintf(stderr, "[%s] Null collection ID array argument\n", rn);
	arg_errfound = 1;
    }
    if (arg_errfound)
	return (-1);

    /*
     * Record our passed-in info.
     */
    xstat_fs_debug = (a_flags & XSTAT_FS_INITFLAG_DEBUGGING);
    xstat_fs_oneShot = (a_flags & XSTAT_FS_INITFLAG_ONE_SHOT);
    xstat_fs_numServers = a_numServers;
    xstat_fs_Handler = a_ProbeHandler;
    xstat_fs_ProbeFreqInSecs = a_ProbeFreqInSecs;
    xstat_fs_numCollections = a_numCollections;
    collIDBytes = xstat_fs_numCollections * sizeof(afs_int32);
    xstat_fs_collIDP = (afs_int32 *) (malloc(collIDBytes));
    memcpy(xstat_fs_collIDP, a_collIDP, collIDBytes);
    if (xstat_fs_debug) {
	printf("[%s] Asking for %d collection(s): ", rn,
	       xstat_fs_numCollections);
	for (curr_srv = 0; curr_srv < xstat_fs_numCollections; curr_srv++)
	    printf("%d ", *(xstat_fs_collIDP + curr_srv));
	printf("\n");
    }

    /*
     * Get ready in case we have to do a cleanup - basically, zero
     * everything out.
     */
    code = xstat_fs_CleanupInit();
    if (code)
	return (code);

    /*
     * Allocate the necessary data structures and initialize everything
     * else.
     */
    xstat_fs_ConnInfo = (struct xstat_fs_ConnectionInfo *)
	malloc(a_numServers * sizeof(struct xstat_fs_ConnectionInfo));
    if (xstat_fs_ConnInfo == (struct xstat_fs_ConnectionInfo *)0) {
	fprintf(stderr,
		"[%s] Can't allocate %d connection info structs (%d bytes)\n",
		rn, a_numServers,
		(a_numServers * sizeof(struct xstat_fs_ConnectionInfo)));
	return (-1);		/*No cleanup needs to be done yet */
    }

    /*
     * Initialize the Rx subsystem, just in case nobody's done it.
     */
    if (xstat_fs_debug)
	printf("[%s] Initializing Rx\n", rn);
    PortToUse = XSTAT_FS_CBPORT;

    do {
	code = rx_Init(htons(PortToUse));
	if (code) {
	    if (code == RX_ADDRINUSE) {
		if (xstat_fs_debug)
		    fprintf(stderr,
			    "[%s] Callback port %d in use, advancing\n", rn,
			    PortToUse);
		PortToUse++;
	    } else {
		fprintf(stderr, "[%s] Fatal error in rx_Init()\n", rn);
		return (-1);
	    }
	}
    } while (code);
    if (xstat_fs_debug)
	printf("[%s] Rx initialized on port %d\n", rn, PortToUse);

    /*
     * Create a null Rx server security object, to be used by the
     * Callback listener.
     */
    CBsecobj = (struct rx_securityClass *)
	rxnull_NewServerSecurityObject();
    if (CBsecobj == (struct rx_securityClass *)0) {
	fprintf(stderr,
		"[%s] Can't create callback listener's security object.\n",
		rn);
	xstat_fs_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (xstat_fs_debug)
	printf("[%s] Callback server security object created\n", rn);

    /*
     * Create a null Rx client security object, to be used by the
     * probe LWP.
     */
    secobj = rxnull_NewClientSecurityObject();
    if (secobj == (struct rx_securityClass *)0) {
	fprintf(stderr,
		"[%s] Can't create probe LWP client security object.\n", rn);
	xstat_fs_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (xstat_fs_debug)
	printf("[%s] Probe LWP client security object created\n", rn);

    curr_conn = xstat_fs_ConnInfo;
    conn_err = 0;
    for (curr_srv = 0; curr_srv < a_numServers; curr_srv++) {
	/*
	 * Copy in the socket info for the current server, resolve its
	 * printable name if possible.
	 */
	if (xstat_fs_debug) {
	    char hoststr[16];
	    printf("[%s] Copying in the following socket info:\n", rn);
	    printf("[%s] IP addr %s, port %d\n", rn,
		   afs_inet_ntoa_r((a_socketArray + curr_srv)->sin_addr.s_addr,hoststr),
		   ntohs((a_socketArray + curr_srv)->sin_port));
	}
	memcpy(&(curr_conn->skt), a_socketArray + curr_srv,
	       sizeof(struct sockaddr_in));

	hostNameFound =
	    hostutil_GetNameByINet(curr_conn->skt.sin_addr.s_addr);
	if (hostNameFound == NULL) {
	    fprintf(stderr,
		    "[%s] Can't map Internet address %s to a string name\n",
		    rn, afs_inet_ntoa_r(curr_conn->skt.sin_addr.s_addr,hoststr));
	    curr_conn->hostName[0] = '\0';
	} else {
	    strcpy(curr_conn->hostName, hostNameFound);
	    if (xstat_fs_debug)
		printf("[%s] Host name for server index %d is %s\n", rn,
		       curr_srv, curr_conn->hostName);
	}

	/*
	 * Make an Rx connection to the current server.
	 */
	if (xstat_fs_debug)
	    printf
		("[%s] Connecting to srv idx %d, IP addr %s, port %d, service 1\n",
		 rn, curr_srv, afs_inet_ntoa_r(curr_conn->skt.sin_addr.s_addr,hoststr),
		 ntohs(curr_conn->skt.sin_port));

	curr_conn->rxconn = rx_NewConnection(curr_conn->skt.sin_addr.s_addr,	/*Server addr */
					     curr_conn->skt.sin_port,	/*Server port */
					     1,	/*AFS service # */
					     secobj,	/*Security obj */
					     0);	/*# of above */
	if (curr_conn->rxconn == (struct rx_connection *)0) {
	    fprintf(stderr,
		    "[%s] Can't create Rx connection to server '%s' (%s)\n",
		    rn, curr_conn->hostName, afs_inet_ntoa_r(curr_conn->skt.sin_addr.s_addr,hoststr));
	    conn_err = 1;
	}
	if (xstat_fs_debug)
	    printf("[%s] New connection at 0x%lx\n", rn, curr_conn->rxconn);

	/*
	 * Bump the current xstat_fs connection to set up.
	 */
	curr_conn++;

    }				/*for curr_srv */

    /*
     * Create the AFS callback service (listener).
     */
    if (xstat_fs_debug)
	printf("[%s] Creating AFS callback listener\n", rn);
    rxsrv_afsserver = rx_NewService(0,	/*Use default port */
				    1,	/*Service ID */
				    "afs",	/*Service name */
				    &CBsecobj,	/*Ptr to security object(s) */
				    1,	/*# of security objects */
				    RXAFSCB_ExecuteRequest);	/*Dispatcher */
    if (rxsrv_afsserver == (struct rx_service *)0) {
	fprintf(stderr, "[%s] Can't create callback Rx service/listener\n",
		rn);
	xstat_fs_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (xstat_fs_debug)
	printf("[%s] Callback listener created\n", rn);

    /*
     * Start up the AFS callback service.
     */
    if (xstat_fs_debug)
	printf("[%s] Starting up callback listener.\n", rn);
    rx_StartServer(0);		/*Don't donate yourself to LWP pool */

    /*
     * Start up the probe LWP.
     */
    if (xstat_fs_debug)
	printf("[%s] Creating the probe LWP\n", rn);
    code = LWP_CreateProcess(xstat_fs_LWP,	/*Function to start up */
			     LWP_STACK_SIZE,	/*Stack size in bytes */
			     1,	/*Priority */
			     (void *)0,	/*Parameters */
			     "xstat_fs Worker",	/*Name to use */
			     &probeLWP_ID);	/*Returned LWP process ID */
    if (code) {
	fprintf(stderr, "[%s] Can't create xstat_fs LWP!  Error is %d\n", rn,
		code);
	xstat_fs_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (code);
    }
    if (xstat_fs_debug)
	printf("[%s] Probe LWP process structure located at 0x%x\n", rn,
	       probeLWP_ID);

    /*
     * Return the final results.
     */
    if (conn_err)
	return (-2);
    else
	return (0);
}


/*------------------------------------------------------------------------
 * [exported] xstat_fs_ForceProbeNow
 *
 * Description:
 *	Wake up the probe LWP, forcing it to execute a probe immediately.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	0 on success,
 *	Error value otherwise.
 *
 * Environment:
 *	The module must have been initialized.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int
xstat_fs_ForceProbeNow()
{
    static char rn[] = "xstat_fs_ForceProbeNow";	/*Routine name */

    /*
     * There isn't a prayer unless we've been initialized.
     */
    if (!xstat_fs_initflag) {
	fprintf(stderr, "[%s] Must call xstat_fs_Init first!\n", rn);
	return (-1);
    }

    /*
     * Kick the sucker in the side.
     */
    IOMGR_Cancel(probeLWP_ID);

    /*
     * We did it, so report the happy news.
     */
    return (0);
}
