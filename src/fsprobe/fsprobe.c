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
 *	Implementation of the AFS FileServer probe facility.
 *
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <pthread.h>
#include <afs/cellconfig.h>
#include <afs/afsint.h>
#include <afs/afsutil.h>
#include <afs/volser.h>
#include <afs/volser_prototypes.h>
#define FSINT_COMMON_XG
#include <afs/afscbint.h>

#include "fsprobe.h"		/*Interface for this module */

/*
 * Exported variables.
 */
int fsprobe_numServers;		/*Num servers connected */
struct fsprobe_ConnectionInfo *fsprobe_ConnInfo;	/*Ptr to connection array */
struct fsprobe_ProbeResults fsprobe_Results;	/*Latest probe results */
int fsprobe_ProbeFreqInSecs;	/*Probe freq. in seconds */

/*
 * Private globals.
 */
static int fsprobe_initflag = 0;	/*Was init routine called? */
static int fsprobe_debug = 0;	/*Debugging output enabled? */
static int (*fsprobe_Handler) (void);	/*Probe handler routine */
static pthread_t fsprobe_thread;	/*Probe thread */
static int fsprobe_statsBytes;	/*Num bytes in stats block */
static int fsprobe_probeOKBytes;	/*Num bytes in probeOK block */
static opr_mutex_t fsprobe_force_lock;	/*Lock to force probe */
static opr_cv_t fsprobe_force_cv;	/*Condvar to force probe */

/*------------------------------------------------------------------------
 * [private] fsprobe_CleanupInit
 *
 * Description:
 *	Set up for recovery after an error in initialization (i.e.,
 *	during a call to fsprobe_Init.
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
fsprobe_CleanupInit(void)
{				/*fsprobe_CleanupInit */

    afs_int32 code;		/*Return code from callback stubs */
    struct rx_call *rxcall;	/*Bogus param */
    AFSCBFids *Fids_Array;	/*Bogus param */
    AFSCBs *CallBack_Array;	/*Bogus param */
    struct interfaceAddr *interfaceAddr;	/*Bogus param */

    fsprobe_ConnInfo = (struct fsprobe_ConnectionInfo *)0;
    memset(&fsprobe_Results, 0, sizeof(struct fsprobe_ProbeResults));

    rxcall = (struct rx_call *)0;
    Fids_Array = (AFSCBFids *) 0;
    CallBack_Array = (AFSCBs *) 0;
    interfaceAddr = NULL;

    code = SRXAFSCB_CallBack(rxcall, Fids_Array, CallBack_Array);
    if (code)
	return (code);
    code = SRXAFSCB_InitCallBackState2(rxcall, interfaceAddr);
    if (code)
	return (code);
    code = SRXAFSCB_Probe(rxcall);
    return (code);

}				/*fsprobe_CleanupInit */


/*------------------------------------------------------------------------
 * [exported] fsprobe_Cleanup
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
 *		with the fsprobe connection array.
 *
 * Environment:
 *	fsprobe_numServers should be properly set.  We don't do anything
 *	unless fsprobe_Init() has already been called.
 *
 * Side Effects:
 *	Shuts down Rx connections gracefully, frees allocated space
 *	(if so directed).
 *------------------------------------------------------------------------*/

int
fsprobe_Cleanup(int a_releaseMem)
{				/*fsprobe_Cleanup */

    static char rn[] = "fsprobe_Cleanup";	/*Routine name */
    int code;			/*Return code */
    int conn_idx;		/*Current connection index */
    struct fsprobe_ConnectionInfo *curr_conn;	/*Ptr to fsprobe connection */

    /*
     * Assume the best, but check the worst.
     */
    if (!fsprobe_initflag) {
	fprintf(stderr, "[%s] Refused; module not initialized\n", rn);
	return (-1);
    } else
	code = 0;

    /*
     * Take care of all Rx connections first.  Check to see that the
     * server count is a legal value.
     */
    if (fsprobe_numServers <= 0) {
	fprintf(stderr,
		"[%s] Illegal number of servers to clean up (fsprobe_numServers = %d)\n",
		rn, fsprobe_numServers);
	code = -1;
    } else {
	if (fsprobe_ConnInfo != (struct fsprobe_ConnectionInfo *)0) {
	    /*
	     * The fsprobe connection structure array exists.  Go through it
	     * and close up any Rx connections it holds.
	     */
	    curr_conn = fsprobe_ConnInfo;
	    for (conn_idx = 0; conn_idx < fsprobe_numServers; conn_idx++) {
		if (curr_conn->rxconn != (struct rx_connection *)0) {
		    rx_DestroyConnection(curr_conn->rxconn);
		    curr_conn->rxconn = (struct rx_connection *)0;
		}
		if (curr_conn->rxVolconn != (struct rx_connection *)0) {
		    rx_DestroyConnection(curr_conn->rxVolconn);
		    curr_conn->rxVolconn = (struct rx_connection *)0;
		}
		curr_conn++;
	    }			/*for each fsprobe connection */
	}			/*fsprobe connection structure exists */
    }				/*Legal number of servers */

    /*
     * Now, release all the space we've allocated, if asked to.
     */
    if (a_releaseMem) {
	if (fsprobe_ConnInfo != (struct fsprobe_ConnectionInfo *)0)
	    free(fsprobe_ConnInfo);
	if (fsprobe_Results.stats != NULL)
	    free(fsprobe_Results.stats);
	if (fsprobe_Results.probeOK != (int *)0)
	    free(fsprobe_Results.probeOK);
    }

    /*
     * Return the news, whatever it is.
     */
    return (code);

}				/*fsprobe_Cleanup */

/*------------------------------------------------------------------------
 * [private] fsprobe_LWP
 *
 * Description:
 *	This thread iterates over the server connections and gathers up
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
 *	Started by fsprobe_Init(), uses global sturctures.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/
static void *
fsprobe_LWP(void *unused)
{				/*fsprobe_LWP */

    static char rn[] = "fsprobe_LWP";	/*Routine name */
    afs_int32 code;	/*Results of calls */
    struct timeval tv;		/*Time structure */
    struct timespec wait;	/*Time to wait */
    int conn_idx;		/*Connection index */
    struct fsprobe_ConnectionInfo *curr_conn;	/*Current connection */
    struct ProbeViceStatistics *curr_stats;	/*Current stats region */
    int *curr_probeOK;		/*Current probeOK field */
    ViceStatistics64 stats64;      /*Current stats region */
    stats64.ViceStatistics64_val = malloc(STATS64_VERSION *
					  sizeof(afs_uint64));
    while (1) {			/*Service loop */
	/*
	 * Iterate through the server connections, gathering data.
	 * Don't forget to bump the probe count and zero the statistics
	 * areas before calling the servers.
	 */
	if (fsprobe_debug)
	    fprintf(stderr,
		    "[%s] Waking up, collecting data from %d connected servers\n",
		    rn, fsprobe_numServers);
	curr_conn = fsprobe_ConnInfo;
	curr_stats = fsprobe_Results.stats;
	curr_probeOK = fsprobe_Results.probeOK;
	fsprobe_Results.probeNum++;
	memset(fsprobe_Results.stats, 0, fsprobe_statsBytes);
	memset(fsprobe_Results.probeOK, 0, fsprobe_probeOKBytes);

	for (conn_idx = 0; conn_idx < fsprobe_numServers; conn_idx++) {
	    /*
	     * Grab the statistics for the current FileServer, if the
	     * connection is valid.
	     */
	    if (fsprobe_debug)
		fprintf(stderr, "[%s] Contacting server %s\n", rn,
			curr_conn->hostName);
	    if (curr_conn->rxconn != (struct rx_connection *)0) {
		if (fsprobe_debug)
		    fprintf(stderr,
			    "[%s] Connection valid, calling RXAFS_GetStatistics\n",
			    rn);
		*curr_probeOK =
		    RXAFS_GetStatistics64(curr_conn->rxconn, STATS64_VERSION, &stats64);
		if (*curr_probeOK == RXGEN_OPCODE)
		    *curr_probeOK =
			RXAFS_GetStatistics(curr_conn->rxconn, (ViceStatistics *)curr_stats);
		else if (*curr_probeOK == 0) {
		    curr_stats->CurrentTime = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_CURRENTTIME]);
		    curr_stats->BootTime = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_BOOTTIME]);
		    curr_stats->StartTime = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_STARTTIME]);
		    curr_stats->CurrentConnections = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_CURRENTCONNECTIONS]);
		    curr_stats->TotalFetchs = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_TOTALFETCHES]);
		    curr_stats->TotalStores = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_TOTALSTORES]);
		    curr_stats->WorkStations = RoundInt64ToInt32(stats64.ViceStatistics64_val[STATS64_WORKSTATIONS]);
		}
	    }

	    /*Valid Rx connection */
	    /*
	     * Call the Volume Server too to get additional stats
	     */
	    if (fsprobe_debug)
		fprintf(stderr, "[%s] Contacting volume server %s\n", rn,
			curr_conn->hostName);
	    if (curr_conn->rxVolconn != (struct rx_connection *)0) {
		int i, code;
		char pname[10];
		struct diskPartition partition;
		struct diskPartition64 *partition64p =
		    malloc(sizeof(struct diskPartition64));

		if (fsprobe_debug)
		    fprintf(stderr,
			    "[%s] Connection valid, calling RXAFS_GetStatistics\n",
			    rn);
		for (i = 0; i < curr_conn->partCnt; i++) {
		    if (curr_conn->partList.partFlags[i] & PARTVALID) {
			MapPartIdIntoName(curr_conn->partList.partId[i],
					  pname);
			code =
			    AFSVolPartitionInfo64(curr_conn->rxVolconn, pname,
						  partition64p);

			if (!code) {
			    curr_stats->Disk[i].BlocksAvailable =
				RoundInt64ToInt31(partition64p->free);
			    curr_stats->Disk[i].TotalBlocks =
				RoundInt64ToInt31(partition64p->minFree);
			    strcpy(curr_stats->Disk[i].Name, pname);
			}
			if (code == RXGEN_OPCODE) {
			    code =
				AFSVolPartitionInfo(curr_conn->rxVolconn,
						    pname, &partition);
			    if (!code) {
				curr_stats->Disk[i].BlocksAvailable =
				    partition.free;
				curr_stats->Disk[i].TotalBlocks =
				    partition.minFree;
				strcpy(curr_stats->Disk[i].Name, pname);
			    }
			}
			if (code) {
			    fprintf(stderr,
				    "Could not get information on server %s partition %s\n",
				    curr_conn->hostName, pname);
			}
		    }
		}
		free(partition64p);
	    }


	    /*
	     * Advance the fsprobe connection pointer & stats pointer.
	     */
	    curr_conn++;
	    curr_stats++;
	    curr_probeOK++;

	}			/*For each fsprobe connection */

	/*
	 * All (valid) connections have been probed.  Now, call the
	 * associated handler function.  The handler does not take
	 * any explicit parameters, rather gets to the goodies via
	 * some of the objects exported by this module.
	 */
	if (fsprobe_debug)
	    fprintf(stderr,
		    "[%s] Polling complete, calling associated handler routine.\n",
		    rn);
	code = fsprobe_Handler();
	if (code)
	    fprintf(stderr, "[%s] Handler routine returned error code %d\n",
		    rn, code);

	/*
	 * Fall asleep for the prescribed number of seconds or wakeup
	 * sooner if forced.
	 */
	gettimeofday(&tv, NULL);
	wait.tv_sec = tv.tv_sec + fsprobe_ProbeFreqInSecs;
	wait.tv_nsec = tv.tv_usec * 1000;
	opr_mutex_enter(&fsprobe_force_lock);
	code = opr_cv_timedwait(&fsprobe_force_cv, &fsprobe_force_lock, &wait);
	opr_mutex_exit(&fsprobe_force_lock);
    }				/*Service loop */
    AFS_UNREACHED(free(stats64.ViceStatistics64_val));
    AFS_UNREACHED(return(NULL));
}				/*fsprobe_LWP */

/*list all the partitions on <aserver> */
static int newvolserver = 0;

int
XListPartitions(struct rx_connection *aconn, struct partList *ptrPartList,
	       	afs_int32 *cntp)
{
    struct pIDs partIds;
    struct partEntries partEnts;
    int i, j = 0, code;

    *cntp = 0;
    if (newvolserver == 1) {
	for (i = 0; i < 26; i++)
	    partIds.partIds[i] = -1;
      tryold:
	code = AFSVolListPartitions(aconn, &partIds);
	if (!code) {
	    for (i = 0; i < 26; i++) {
		if ((partIds.partIds[i]) != -1) {
		    ptrPartList->partId[j] = partIds.partIds[i];
		    ptrPartList->partFlags[j] = PARTVALID;
		    j++;
		} else
		    ptrPartList->partFlags[i] = 0;
	    }
	    *cntp = j;
	}
	goto out;
    }
    partEnts.partEntries_len = 0;
    partEnts.partEntries_val = NULL;
    code = AFSVolXListPartitions(aconn, &partEnts);
    if (!newvolserver) {
	if (code == RXGEN_OPCODE) {
	    newvolserver = 1;	/* Doesn't support new interface */
	    goto tryold;
	} else if (!code) {
	    newvolserver = 2;
	}
    }
    if (!code) {
	*cntp = partEnts.partEntries_len;
	if (*cntp > VOLMAXPARTS) {
	    fprintf(stderr,
		    "Warning: number of partitions on the server too high %d (process only %d)\n",
		    *cntp, VOLMAXPARTS);
	    *cntp = VOLMAXPARTS;
	}
	for (i = 0; i < *cntp; i++) {
	    ptrPartList->partId[i] = partEnts.partEntries_val[i];
	    ptrPartList->partFlags[i] = PARTVALID;
	}
	free(partEnts.partEntries_val);
    }
  out:
    if (code)
	fprintf(stderr,
		"Could not fetch the list of partitions from the server\n");
    return code;
}


/*------------------------------------------------------------------------
 * [exported] fsprobe_Init
 *
 * Description:
 *	Initialize the fsprobe module: set up Rx connections to the
 *	given set of servers, start up the probe and callback threads,
 *	and associate the routine to be called when a probe completes.
 *
 * Arguments:
 *	int a_numServers		  : Num. servers to connect to.
 *	struct sockaddr_in *a_socketArray : Array of server sockets.
 *	int a_ProbeFreqInSecs		  : Probe frequency in seconds.
 *	int (*a_ProbeHandler)()		  : Ptr to probe handler fcn.
 *	int a_debug;			  : Turn debugging output on?
 *
 * Returns:
 *	0 on success,
 *	-2 for (at least one) connection error,
 *	thread process creation code, if it failed,
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
fsprobe_Init(int a_numServers, struct sockaddr_in *a_socketArray,
	     int a_ProbeFreqInSecs, int (*a_ProbeHandler)(void),
	     int a_debug)
{				/*fsprobe_Init */

    static char rn[] = "fsprobe_Init";	/*Routine name */
    afs_int32 code;	/*Return value */
    static struct rx_securityClass *CBsecobj;	/*Callback security object */
    struct rx_securityClass *secobj;	/*Client security object */
    struct rx_service *rxsrv_afsserver;	/*Server for AFS */
    int arg_errfound;		/*Argument error found? */
    int curr_srv;		/*Current server idx */
    struct fsprobe_ConnectionInfo *curr_conn;	/*Ptr to current conn */
    char *hostNameFound;	/*Ptr to returned host name */
    int conn_err;		/*Connection error? */

    /*
     * If we've already been called, snicker at the bozo, gently
     * remind him of his doubtful heritage, and return success.
     */
    if (fsprobe_initflag) {
	fprintf(stderr, "[%s] Called multiple times!\n", rn);
	return (0);
    } else
	fsprobe_initflag = 1;

    opr_mutex_init(&fsprobe_force_lock);
    opr_cv_init(&fsprobe_force_cv);

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
    if (arg_errfound)
	return (-1);

    /*
     * Record our passed-in info.
     */
    fsprobe_debug = a_debug;
    fsprobe_numServers = a_numServers;
    fsprobe_Handler = a_ProbeHandler;
    fsprobe_ProbeFreqInSecs = a_ProbeFreqInSecs;

    /*
     * Get ready in case we have to do a cleanup - basically, zero
     * everything out.
     */
    fsprobe_CleanupInit();

    /*
     * Allocate the necessary data structures and initialize everything
     * else.
     */
    fsprobe_ConnInfo = (struct fsprobe_ConnectionInfo *)
	malloc(a_numServers * sizeof(struct fsprobe_ConnectionInfo));
    if (fsprobe_ConnInfo == (struct fsprobe_ConnectionInfo *)0) {
	fprintf(stderr,
		"[%s] Can't allocate %d connection info structs (%"AFS_SIZET_FMT" bytes)\n",
		rn, a_numServers,
		(a_numServers * sizeof(struct fsprobe_ConnectionInfo)));
	return (-1);		/*No cleanup needs to be done yet */
    }

    fsprobe_statsBytes = a_numServers * sizeof(struct ProbeViceStatistics);
    fsprobe_Results.stats = (struct ProbeViceStatistics *)
	malloc(fsprobe_statsBytes);
    if (fsprobe_Results.stats == NULL) {
	fprintf(stderr,
		"[%s] Can't allocate %d statistics structs (%d bytes)\n", rn,
		a_numServers, fsprobe_statsBytes);
	fsprobe_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    } else if (fsprobe_debug)
	fprintf(stderr, "[%s] fsprobe_Results.stats allocated (%d bytes)\n",
		rn, fsprobe_statsBytes);

    fsprobe_probeOKBytes = a_numServers * sizeof(int);
    fsprobe_Results.probeOK = malloc(fsprobe_probeOKBytes);
    if (fsprobe_Results.probeOK == (int *)0) {
	fprintf(stderr,
		"[%s] Can't allocate %d probeOK array entries (%d bytes)\n",
		rn, a_numServers, fsprobe_probeOKBytes);
	fsprobe_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    } else if (fsprobe_debug)
	fprintf(stderr, "[%s] fsprobe_Results.probeOK allocated (%d bytes)\n",
		rn, fsprobe_probeOKBytes);

    fsprobe_Results.probeNum = 0;
    fsprobe_Results.probeTime = 0;
    memset(fsprobe_Results.stats, 0,
	   (a_numServers * sizeof(struct ProbeViceStatistics)));

    /*
     * Initialize the Rx subsystem, just in case nobody's done it.
     */
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Initializing Rx\n", rn);
    code = rx_Init(0);
    if (code) {
	fprintf(stderr, "[%s] Fatal error in rx_Init()\n", rn);
	return (-1);
    }
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Rx initialized.\n", rn);

    /*
     * Create a null Rx server security object, to be used by the
     * Callback listener.
     */
    CBsecobj = rxnull_NewServerSecurityObject();
    if (CBsecobj == (struct rx_securityClass *)0) {
	fprintf(stderr,
		"[%s] Can't create null security object for the callback listener.\n",
		rn);
	fsprobe_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Callback server security object created\n", rn);

    /*
     * Create a null Rx client security object, to be used by the
     * probe thread.
     */
    secobj = rxnull_NewClientSecurityObject();
    if (secobj == (struct rx_securityClass *)0) {
	fprintf(stderr,
		"[%s] Can't create client security object for probe thread.\n",
		rn);
	fsprobe_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Probe thread client security object created\n",
		rn);

    curr_conn = fsprobe_ConnInfo;
    conn_err = 0;
    for (curr_srv = 0; curr_srv < a_numServers; curr_srv++) {
	/*
	 * Copy in the socket info for the current server, resolve its
	 * printable name if possible.
	 */
	if (fsprobe_debug) {
	    fprintf(stderr, "[%s] Copying in the following socket info:\n",
		    rn);
	    fprintf(stderr, "[%s] IP addr 0x%x, port %d\n", rn,
		    (a_socketArray + curr_srv)->sin_addr.s_addr,
		    (a_socketArray + curr_srv)->sin_port);
	}
	memcpy(&(curr_conn->skt), a_socketArray + curr_srv,
	       sizeof(struct sockaddr_in));

	hostNameFound =
	    hostutil_GetNameByINet(curr_conn->skt.sin_addr.s_addr);
	if (hostNameFound == NULL) {
	    fprintf(stderr,
		    "[%s] Can't map Internet address %u to a string name\n",
		    rn, curr_conn->skt.sin_addr.s_addr);
	    curr_conn->hostName[0] = '\0';
	} else {
	    strcpy(curr_conn->hostName, hostNameFound);
	    if (fsprobe_debug)
		fprintf(stderr, "[%s] Host name for server index %d is %s\n",
			rn, curr_srv, curr_conn->hostName);
	}

	/*
	 * Make an Rx connection to the current server.
	 */
	if (fsprobe_debug)
	    fprintf(stderr,
		    "[%s] Connecting to srv idx %d, IP addr 0x%x, port %d, service 1\n",
		    rn, curr_srv, curr_conn->skt.sin_addr.s_addr,
		    curr_conn->skt.sin_port);
	curr_conn->rxconn = rx_NewConnection(curr_conn->skt.sin_addr.s_addr,	/*Server addr */
					     curr_conn->skt.sin_port,	/*Server port */
					     1,	/*AFS service num */
					     secobj,	/*Security object */
					     0);	/*Number of above */
	if (curr_conn->rxconn == (struct rx_connection *)0) {
	    fprintf(stderr,
		    "[%s] Can't create Rx connection to server %s (%u)\n",
		    rn, curr_conn->hostName, curr_conn->skt.sin_addr.s_addr);
	    conn_err = 1;
	}
	if (fsprobe_debug)
	    fprintf(stderr, "[%s] New connection at %p\n", rn,
		    curr_conn->rxconn);

	/*
	 * Make an Rx connection to the current volume server.
	 */
	if (fsprobe_debug)
	    fprintf(stderr,
		    "[%s] Connecting to srv idx %d, IP addr 0x%x, port %d, service 1\n",
		    rn, curr_srv, curr_conn->skt.sin_addr.s_addr,
		    htons(7005));
	curr_conn->rxVolconn = rx_NewConnection(curr_conn->skt.sin_addr.s_addr,	/*Server addr */
						htons(AFSCONF_VOLUMEPORT),	/*Volume Server port */
						VOLSERVICE_ID,	/*AFS service num */
						secobj,	/*Security object */
						0);	/*Number of above */
	if (curr_conn->rxVolconn == (struct rx_connection *)0) {
	    fprintf(stderr,
		    "[%s] Can't create Rx connection to volume server %s (%u)\n",
		    rn, curr_conn->hostName, curr_conn->skt.sin_addr.s_addr);
	    conn_err = 1;
	} else {
	    int i, cnt;

	    memset(&curr_conn->partList, 0, sizeof(struct partList));
	    curr_conn->partCnt = 0;
	    i = XListPartitions(curr_conn->rxVolconn, &curr_conn->partList,
				&cnt);
	    if (!i) {
		curr_conn->partCnt = cnt;
	    }
	}
	if (fsprobe_debug)
	    fprintf(stderr, "[%s] New connection at %p\n", rn,
		    curr_conn->rxVolconn);


	/*
	 * Bump the current fsprobe connection to set up.
	 */
	curr_conn++;

    }				/*for curr_srv */

    /*
     * Create the AFS callback service (listener).
     */
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Creating AFS callback listener\n", rn);
    rxsrv_afsserver = rx_NewService(0,	/*Use default port */
				    1,	/*Service ID */
				    "afs",	/*Service name */
				    &CBsecobj,	/*Ptr to security object(s) */
				    1,	/*Number of security objects */
				    RXAFSCB_ExecuteRequest);	/*Dispatcher */
    if (rxsrv_afsserver == (struct rx_service *)0) {
	fprintf(stderr, "[%s] Can't create callback Rx service/listener\n",
		rn);
	fsprobe_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (-1);
    }
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Callback listener created\n", rn);

    /*
     * Start up the AFS callback service.
     */
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Starting up callback listener.\n", rn);
    rx_StartServer(0 /*Don't donate yourself to thread pool */ );

    /*
     * Start up the probe thread.
     */
    if (fsprobe_debug)
	fprintf(stderr, "[%s] Creating the probe thread\n", rn);
    code = pthread_create(&fsprobe_thread, NULL, fsprobe_LWP, NULL);
    if (code) {
	fprintf(stderr, "[%s] Can't create fsprobe thread!  Error is %d\n", rn,
		code);
	fsprobe_Cleanup(1);	/*Delete already-malloc'ed areas */
	return (code);
    }

    /*
     * Return the final results.
     */
    if (conn_err)
	return (-2);
    else
	return (0);

}				/*fsprobe_Init */


/*------------------------------------------------------------------------
 * [exported] fsprobe_ForceProbeNow
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
fsprobe_ForceProbeNow(void)
{				/*fsprobe_ForceProbeNow */

    static char rn[] = "fsprobe_ForceProbeNow";	/*Routine name */

    /*
     * There isn't a prayer unless we've been initialized.
     */
    if (!fsprobe_initflag) {
	fprintf(stderr, "[%s] Must call fsprobe_Init first!\n", rn);
	return (-1);
    }

    /*
     * Kick the sucker in the side.
     */
    opr_mutex_enter(&fsprobe_force_lock);
    opr_cv_signal(&fsprobe_force_cv);
    opr_mutex_exit(&fsprobe_force_lock);

    /*
     * We did it, so report the happy news.
     */
    return (0);

}				/*fsprobe_ForceProbeNow */

/*------------------------------------------------------------------------
 * [exported] fsprobe_Wait
 *
 * Description:
 *	Wait for the collection to complete.
 *
 * Arguments:
 *    int sleep_secs : time to wait in seconds. 0 means sleep forever.
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
fsprobe_Wait(int sleep_secs)
{
    int code;
    struct timeval tv;

    if (sleep_secs == 0) {
	while (1) {
	    tv.tv_sec = 30;
	    tv.tv_usec = 0;
	    code = select(0, 0, 0, 0, &tv);
	    if (code < 0)
		break;
	}
    } else {
	tv.tv_sec = sleep_secs;
	tv.tv_usec = 0;
	code = select(0, 0, 0, 0, &tv);
    }
    return code;
}
