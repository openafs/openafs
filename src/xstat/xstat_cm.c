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
 *	Implementation of the client side of the AFS Cache Manager
 *	extended statistics facility.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <afs/opr.h>

#include "xstat_cm.h"		/*Interface for this module */
#include <pthread.h>

#include <afs/afsutil.h>

/*
 * Exported variables.
 */
int xstat_cm_numServers;	/*Num connected servers */
struct xstat_cm_ConnectionInfo
 *xstat_cm_ConnInfo;		/*Ptr to connection array */
struct xstat_cm_ProbeResults xstat_cm_Results;	/*Latest probe results */

afs_int32 xstat_cmData[AFSCB_MAX_XSTAT_LONGS];	/*Buffer for collected data */

/*
 * Private globals.
 */
static int xstat_cm_ProbeFreqInSecs;	/*Probe freq. in seconds */
static int xstat_cm_initflag = 0;	/*Was init routine called? */
static int xstat_cm_debug = 0;	/*Debugging output enabled? */
static int xstat_cm_oneShot = 0;	/*One-shot operation? */
static int (*xstat_cm_Handler) (void);	/*Probe handler routine */
static pthread_t xstat_cm_thread;	/*Probe thread */
static int xstat_cm_numCollections;	/*Number of desired collections */
static afs_int32 *xstat_cm_collIDP;	/*Ptr to collection IDs desired */
static opr_mutex_t xstat_cm_force_lock;	/*Lock to wakeup probe */
static opr_cv_t xstat_cm_force_cv;	/*Condvar to wakeup probe */


/*------------------------------------------------------------------------
 * [private] xstat_cm_CleanupInit
 *
 * Description:
 *	Set up for recovery after an error in initialization (i.e.,
 *	during a call to xstat_cm_Init.
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
xstat_cm_CleanupInit(void)
{
    xstat_cm_ConnInfo = (struct xstat_cm_ConnectionInfo *)0;
    xstat_cm_Results.probeNum = 0;
    xstat_cm_Results.probeTime = 0;
    xstat_cm_Results.connP = (struct xstat_cm_ConnectionInfo *)0;
    xstat_cm_Results.collectionNumber = 0;
    xstat_cm_Results.data.AFSCB_CollData_len = AFSCB_MAX_XSTAT_LONGS;
    xstat_cm_Results.data.AFSCB_CollData_val = (afs_int32 *) xstat_cmData;
    xstat_cm_Results.probeOK = 0;

    return (0);
}


/*------------------------------------------------------------------------
 * [exported] xstat_cm_Cleanup
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
 *		with the xstat_cm connection array.
 *
 * Environment:
 *	xstat_cm_numServers should be properly set.  We don't do anything
 *	unless xstat_cm_Init() has already been called.
 *
 * Side Effects:
 *	Shuts down Rx connections gracefully, frees allocated space
 *	(if so directed).
 *------------------------------------------------------------------------*/

int
xstat_cm_Cleanup(int a_releaseMem)
{
    static char rn[] = "xstat_cm_Cleanup";	/*Routine name */
    int code;			/*Return code */
    int conn_idx;		/*Current connection index */
    struct xstat_cm_ConnectionInfo *curr_conn;	/*Ptr to xstat_cm connection */

    /*
     * Assume the best, but check the worst.
     */
    if (!xstat_cm_initflag) {
	fprintf(stderr, "[%s] Refused; module not initialized\n", rn);
	return (-1);
    } else
	code = 0;

    /*
     * Take care of all Rx connections first.  Check to see that the
     * server count is a legal value.
     */
    if (xstat_cm_numServers <= 0) {
	fprintf(stderr,
		"[%s] Illegal number of servers (xstat_cm_numServers = %d)\n",
		rn, xstat_cm_numServers);
	code = -1;
    } else {
	if (xstat_cm_ConnInfo != (struct xstat_cm_ConnectionInfo *)0) {
	    /*
	     * The xstat_cm connection structure array exists.  Go through
	     * it and close up any Rx connections it holds.
	     */
	    curr_conn = xstat_cm_ConnInfo;
	    for (conn_idx = 0; conn_idx < xstat_cm_numServers; conn_idx++) {
		if (curr_conn->rxconn != (struct rx_connection *)0) {
		    rx_DestroyConnection(curr_conn->rxconn);
		    curr_conn->rxconn = (struct rx_connection *)0;
		}
		curr_conn++;
	    }			/*for each xstat_cm connection */
	}			/*xstat_cm connection structure exists */
    }				/*Legal number of servers */

    /*
     * If asked to, release the space we've allocated.
     */
    if (a_releaseMem) {
	if (xstat_cm_ConnInfo != (struct xstat_cm_ConnectionInfo *)0)
	    free(xstat_cm_ConnInfo);
    }

    /*
     * Return the news, whatever it is.
     */
    return (code);
}


/*------------------------------------------------------------------------
 * [private] xstat_cm_LWP
 *
 * Description:
 *	This thread iterates over the server connections and gathers up
 *	the desired statistics from each one on a regular basis, for
 *	all known data collections.  The associated handler function
 *	is called each time a new data collection is received.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Started by xstat_cm_Init(), uses global structures and the
 *	global private xstat_cm_oneShot variable.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/
static void *
xstat_cm_LWP(void *unused)
{
    static char rn[] = "xstat_cm_LWP";	/*Routine name */
    afs_int32 code;	/*Results of calls */
    struct timeval tv;		/*Time structure */
    struct timespec wait;	/*Time to wait */
    int conn_idx;		/*Connection index */
    struct xstat_cm_ConnectionInfo *curr_conn;	/*Current connection */
    afs_int32 srvVersionNumber;	/*Xstat version # */
    afs_int32 clientVersionNumber;	/*Client xstat version */
    afs_int32 numColls;		/*Number of collections to get */
    afs_int32 *currCollIDP;	/*Curr collection ID desired */

    /*
     * Set up some numbers we'll need.
     */
    clientVersionNumber = AFSCB_XSTAT_VERSION;

    while (1) {			/*Service loop */
	/*
	 * Iterate through the server connections, gathering data.
	 * Don't forget to bump the probe count and zero the statistics
	 * areas before calling the servers.
	 */
	if (xstat_cm_debug)
	    printf("[%s] Waking up, getting data from %d server(s)\n", rn,
		   xstat_cm_numServers);
	curr_conn = xstat_cm_ConnInfo;
	xstat_cm_Results.probeNum++;

	for (conn_idx = 0; conn_idx < xstat_cm_numServers; conn_idx++) {
	    /*
	     * Grab the statistics for the current Cache Manager, if the
	     * connection is valid.
	     */
	    if (xstat_cm_debug)
		printf("[%s] Getting collections from Cache Manager '%s'\n",
		       rn, curr_conn->hostName);
	    if (curr_conn->rxconn != (struct rx_connection *)0) {
		if (xstat_cm_debug)
		    printf("[%s] Connection OK, calling RXAFSCB_GetXStats\n",
			   rn);

		/*
		 * Probe the given CM for each desired collection.
		 */
		currCollIDP = xstat_cm_collIDP;
		for (numColls = 0; numColls < xstat_cm_numCollections;
		     numColls++, currCollIDP++) {
		    /*
		     * Initialize the per-probe values.
		     */
		    if (xstat_cm_debug)
			printf("[%s] Asking for data collection %d\n", rn,
			       *currCollIDP);
		    xstat_cm_Results.collectionNumber = *currCollIDP;
		    xstat_cm_Results.data.AFSCB_CollData_len =
			AFSCB_MAX_XSTAT_LONGS;
		    memset(xstat_cm_Results.data.AFSCB_CollData_val, 0,
			   AFSCB_MAX_XSTAT_LONGS * 4);

		    xstat_cm_Results.connP = curr_conn;

		    if (xstat_cm_debug) {
			printf
			    ("%s: Calling RXAFSCB_GetXStats, conn=%p, clientVersionNumber=%d, collectionNumber=%d, srvVersionNumberP=%p, timeP=%p, dataP=%p\n",
			     rn, curr_conn->rxconn, clientVersionNumber,
			     *currCollIDP, &srvVersionNumber,
			     &(xstat_cm_Results.probeTime),
			     &(xstat_cm_Results.data));
			printf("%s: [bufflen=%d, buffer at %p]\n", rn,
			       xstat_cm_Results.data.AFSCB_CollData_len,
			       xstat_cm_Results.data.AFSCB_CollData_val);
		    }

		    xstat_cm_Results.probeOK =
			RXAFSCB_GetXStats(curr_conn->rxconn,
					  clientVersionNumber, *currCollIDP,
					  &srvVersionNumber,
					  &(xstat_cm_Results.probeTime),
					  &(xstat_cm_Results.data));

		    /*
		     * Now that we (may) have the data for this connection,
		     * call the associated handler function.  The handler
		     * does not take any explicit parameters, but rather
		     * gets to the goodies via some of the objects exported
		     * by this module.
		     */
		    if (xstat_cm_debug)
			printf("[%s] Calling handler routine.\n", rn);
		    code = xstat_cm_Handler();
		    if (code)
			fprintf(stderr,
				"[%s] Handler routine got error code %d\n",
				rn, code);
		}		/*For each collection */
	    }

	    /*Valid Rx connection */
	    /*
	     * Advance the xstat_cm connection pointer.
	     */
	    curr_conn++;

	}			/*For each xstat_cm connection */

	/*
	 * All (valid) connections have been probed.  Fall asleep for the
	 * prescribed number of seconds, unless we're a one-shot.  In
	 * that case, we need to signal our caller that we're done.
	 */
	if (xstat_cm_debug)
	    printf("[%s] Polling complete for probe round %d.\n", rn,
		   xstat_cm_Results.probeNum);

	if (xstat_cm_oneShot) {
	    /*
	     * One-shot execution desired.
	     */
	    break;		/*from the perpetual while loop */
	} /*One-shot execution */
	else {
	    /*
	     * Continuous execution desired.  Sleep for the required
	     * number of seconds or wakeup sooner if forced.
	     */
	    if (xstat_cm_debug)
		printf("[%s] Falling asleep for %d seconds\n", rn,
		       xstat_cm_ProbeFreqInSecs);
	    gettimeofday(&tv, NULL);
	    wait.tv_sec = tv.tv_sec + xstat_cm_ProbeFreqInSecs;
	    wait.tv_nsec = tv.tv_usec * 1000;
	    opr_mutex_enter(&xstat_cm_force_lock);
	    code = opr_cv_timedwait(&xstat_cm_force_cv, &xstat_cm_force_lock, &wait);
	    opr_mutex_exit(&xstat_cm_force_lock);
	}			/*Continuous execution */
    }				/*Service loop */
    return NULL;
}


/*------------------------------------------------------------------------
 * [exported] xstat_cm_Init
 *
 * Description:
 *	Initialize the xstat_cm module: set up Rx connections to the
 *	given set of Cache Managers, start up the probe thread, and
 *	associate the routine to be called when a probe completes.
 *	Also, let it know which collections you're interested in.
 *
 * Arguments:
 *	int a_numServers		  : Num. servers to connect to.
 *	struct sockaddr_in *a_socketArray : Array of server sockets.
 *	int a_ProbeFreqInSecs		  : Probe frequency in seconds.
 *	int (*a_ProbeHandler)()		  : Ptr to probe handler fcn.
 *	int a_flags;			  : Various flags.
 *	int a_numCollections		  : Number of collections desired.
 *	afs_int32 *a_collIDP			  : Ptr to collection IDs.
 *
 * Returns:
 *	0 on success,
 *	-2 for (at least one) connection error,
 *	thread process creation code, if it failed,
 *	-1 for other fatal errors.
 *
 * Environment:
 *	*** MUST BE THE FIRST ROUTINE CALLED FROM THIS PACKAGE ***
 *
 * Side Effects:
 *	Sets up just about everything.
 *------------------------------------------------------------------------*/

int
xstat_cm_Init(int a_numServers, struct sockaddr_in *a_socketArray,
	      int a_ProbeFreqInSecs, int (*a_ProbeHandler) (void), int a_flags,
	      int a_numCollections, afs_int32 * a_collIDP)
{

    static char rn[] = "xstat_cm_Init";	/*Routine name */
    afs_int32 code;	/*Return value */
    struct rx_securityClass *secobj;	/*Client security object */
    int arg_errfound;		/*Argument error found? */
    int curr_srv;		/*Current server idx */
    struct xstat_cm_ConnectionInfo *curr_conn;	/*Ptr to current conn */
    char *hostNameFound;	/*Ptr to returned host name */
    int conn_err;		/*Connection error? */
    int collIDBytes;		/*Num bytes in coll ID array */
    char hoststr[16];

    /*
     * If we've already been called, snicker at the bozo, gently
     * remind him of his doubtful heritage, and return success.
     */
    if (xstat_cm_initflag) {
	fprintf(stderr, "[%s] Called multiple times!\n", rn);
	return (0);
    } else
	xstat_cm_initflag = 1;

    opr_mutex_init(&xstat_cm_force_lock);
    opr_cv_init(&xstat_cm_force_cv);

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
    if (a_ProbeHandler == NULL) {
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
    xstat_cm_debug = (a_flags & XSTAT_CM_INITFLAG_DEBUGGING);
    xstat_cm_oneShot = (a_flags & XSTAT_CM_INITFLAG_ONE_SHOT);
    xstat_cm_numServers = a_numServers;
    xstat_cm_Handler = a_ProbeHandler;
    xstat_cm_ProbeFreqInSecs = a_ProbeFreqInSecs;
    xstat_cm_numCollections = a_numCollections;
    collIDBytes = xstat_cm_numCollections * sizeof(afs_int32);
    xstat_cm_collIDP = malloc(collIDBytes);
    memcpy(xstat_cm_collIDP, a_collIDP, collIDBytes);
    if (xstat_cm_debug) {
	printf("[%s] Asking for %d collection(s): ", rn,
	       xstat_cm_numCollections);
	for (curr_srv = 0; curr_srv < xstat_cm_numCollections; curr_srv++)
	    printf("%d ", *(xstat_cm_collIDP + curr_srv));
	printf("\n");
    }

    /*
     * Get ready in case we have to do a cleanup - basically, zero
     * everything out.
     */
    code = xstat_cm_CleanupInit();
    if (code)
	return (code);

    /*
     * Allocate the necessary data structures and initialize everything
     * else.
     */
    xstat_cm_ConnInfo = malloc(a_numServers
			       * sizeof(struct xstat_cm_ConnectionInfo));
    if (xstat_cm_ConnInfo == (struct xstat_cm_ConnectionInfo *)0) {
	fprintf(stderr,
		"[%s] Can't allocate %d connection info structs (%" AFS_SIZET_FMT " bytes)\n",
		rn, a_numServers,
		(a_numServers * sizeof(struct xstat_cm_ConnectionInfo)));
	return (-1);		/*No cleanup needs to be done yet */
    }

    /*
     * Initialize the Rx subsystem, just in case nobody's done it.
     */
    if (xstat_cm_debug)
	printf("[%s] Initializing Rx on port 0\n", rn);
    code = rx_Init(htons(0));
    if (code) {
	fprintf(stderr, "[%s] Fatal error in rx_Init(), error=%d\n", rn,
		code);
	return (-1);
    }

    if (xstat_cm_debug)
	printf("[%s] Rx initialized on port 0\n", rn);

    /*
     * Create a null Rx client security object, to be used by the
     * probe thread.
     */
    secobj = rxnull_NewClientSecurityObject();
    if (secobj == (struct rx_securityClass *)0) {
	fprintf(stderr,
		"[%s] Can't create probe thread client security object.\n", rn);
	xstat_cm_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (xstat_cm_debug)
	printf("[%s] Probe thread client security object created\n", rn);

    curr_conn = xstat_cm_ConnInfo;
    conn_err = 0;
    for (curr_srv = 0; curr_srv < a_numServers; curr_srv++) {
	/*
	 * Copy in the socket info for the current server, resolve its
	 * printable name if possible.
	 */
	if (xstat_cm_debug) {
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
	    if (xstat_cm_debug)
		printf("[%s] Host name for server index %d is %s\n", rn,
		       curr_srv, curr_conn->hostName);
	}

	/*
	 * Make an Rx connection to the current server.
	 */
	if (xstat_cm_debug)
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
	if (xstat_cm_debug)
	    printf("[%s] New connection at %p\n", rn, curr_conn->rxconn);

	/*
	 * Bump the current xstat_cm connection to set up.
	 */
	curr_conn++;

    }				/*for curr_srv */

    /*
     * Start up the probe thread.
     */
    if (xstat_cm_debug)
	printf("[%s] Creating the probe thread\n", rn);
    code = pthread_create(&xstat_cm_thread, NULL, xstat_cm_LWP, NULL);
    if (code) {
	fprintf(stderr, "[%s] Can't create xstat_cm thread!  Error is %d\n", rn,
		code);
	xstat_cm_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (code);
    }

    /*
     * Return the final results.
     */
    if (conn_err)
	return (-2);
    else
	return (0);
}


/*------------------------------------------------------------------------
 * [exported] xstat_cm_ForceProbeNow
 *
 * Description:
 *	Wake up the probe thread, forcing it to execute a probe immediately.
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
xstat_cm_ForceProbeNow(void)
{
    static char rn[] = "xstat_cm_ForceProbeNow";	/*Routine name */

    /*
     * There isn't a prayer unless we've been initialized.
     */
    if (!xstat_cm_initflag) {
	fprintf(stderr, "[%s] Must call xstat_cm_Init first!\n", rn);
	return (-1);
    }

    /*
     * Kick the sucker in the side.
     */
    opr_mutex_enter(&xstat_cm_force_lock);
    opr_cv_signal(&xstat_cm_force_cv);
    opr_mutex_exit(&xstat_cm_force_lock);

    /*
     * We did it, so report the happy news.
     */
    return (0);
}

/*
 * Wait for the collection to complete. Returns after one cycle if running in
 * one-shot mode, otherwise wait for a given amount of time.
 *
 * Args:
 *    int sleep_secs : time to wait in seconds when running
 *                     in continuous mode. 0 means wait forever.
 *
 * Returns:
 *    0 on success
 */
int
xstat_cm_Wait(int sleep_secs)
{
    static char rn[] = "xstat_cm_Wait";	/*Routine name */
    int code;
    struct timeval tv;		/*Time structure */

    if (xstat_cm_oneShot) {
	/*
	 * One-shot operation; just wait for the collection to be done.
	 */
	if (xstat_cm_debug)
	    printf("[%s] Calling pthread_join()\n", rn);
	code = pthread_join(xstat_cm_thread, NULL);
	if (xstat_cm_debug)
	    printf("[%s] Returned from pthread_join()\n", rn);
	if (code) {
	    fprintf(stderr,
		    "[%s] Error %d encountered by pthread_join()\n",
		    rn, code);
	}
    } else if (sleep_secs == 0) {
	/* Sleep forever. */
	if (xstat_cm_debug)
	    fprintf(stderr, "[%s] going to sleep ...\n", rn);
	while (1) {
	    code = select(0,	/*Num fds */
			  0,	/*Descriptors ready for reading */
			  0,	/*Descriptors ready for writing */
			  0,	/*Descriptors with exceptional conditions */
			  NULL);	/* NULL timeout means "forever" */
	    if (code < 0) {
		fprintf(stderr, "[%s] select() error %d\n", rn, errno);
		break;
	    }
	}
    } else {
	/* Let's just fall asleep while.  */
	if (xstat_cm_debug)
	    printf
		("xstat_cm service started, main thread sleeping for %d secs.\n",
		 sleep_secs);
	tv.tv_sec = sleep_secs;
	tv.tv_usec = 0;
	code = select(0,	/*Num fds */
		      0,	/*Descriptors ready for reading */
		      0,	/*Descriptors ready for writing */
		      0,	/*Descriptors with exceptional conditions */
		      &tv);	/*Timeout structure */
	if (code < 0)
	    fprintf(stderr, "[%s] select() error: %d\n", rn, errno);
    }
    return code;
}
