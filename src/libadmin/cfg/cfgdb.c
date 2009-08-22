/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file implements configuration functions in the following categories:
 *   cfg_CellServDb*()   - manage the cell-wide server CellServDb database.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

#include <rx/rx.h>
#include <rx/rxstat.h>

#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>

#include <afs/cellconfig.h>
#include <afs/bnode.h>

#include "cfginternal.h"
#include "afs_cfgAdmin.h"


/* Local declarations and definitions */

#define CSDB_OP_ADD  0		/* add a server CellServDB entry */
#define CSDB_OP_REM  1		/* remove a server CellServDB entry */

#define CSDB_WAIT   0		/* wait to begin CellServDB update operations */
#define CSDB_GO     1		/* begin CellServDB update operations */
#define CSDB_ABORT  2		/* abort CellServDB update operations */

#define SERVER_NAME_BLOCK_SIZE  20	/* number of server names in a block */


/* server name iterator */
typedef struct {
    int dbOnly;			/* enumerate database servers only */
    void *iterationId;		/* underlying iteration handle */
} cfg_server_iteration_t;


/* CellServDB update control block */
typedef struct {
    /* control block invariants */
    cfg_host_p cfg_host;	/* host configuration handle */
    int op;			/* CellServDB update operation type */
    char opHostAlias[MAXHOSTCHARS];	/* CellServDB alias for config host */
    cfg_cellServDbUpdateCallBack_t callBack;	/* CellServDB update callback */
    void *callBackId;		/* CellServDB update callback cookie */

    /* control block synchronization objects */
    pthread_cond_t event;	/* disposition change event */
    pthread_mutex_t mutex;	/* protects disposition and workersActive */
    int disposition;		/* wait, go, or abort operation */
    int workersActive;		/* count of active worker threads */
} cfg_csdb_update_ctrl_t;

/* CellServDB update name block */
typedef struct {
    cfg_csdb_update_ctrl_t *ctrl;	/* pointer to common control block */
    int serverCount;		/* number of entries in serverName array */
    char serverName[SERVER_NAME_BLOCK_SIZE][AFS_MAX_SERVER_NAME_LEN];
} cfg_csdb_update_name_t;

/* name block iterator */
typedef struct {
    void *serverIter;		/* server name iterator */
    cfg_csdb_update_ctrl_t *ctrlBlockp;	/* update control block */
    const char *cfgHost;	/* configuration host name */
    const char *sysControlHost;	/* system control host name (if any) */
    short cfgInBlock;		/* configuration host in a name block */
    short sysInBlock;		/* system control host in a name block */
    short serverIterDone;	/* server name enumeration complete */
} cfg_csdb_nameblock_iteration_t;



static int
  CellServDbUpdate(int updateOp, void *hostHandle, const char *sysControlHost,
		   cfg_cellServDbUpdateCallBack_t callBack, void *callBackId,
		   int *maxUpdates, afs_status_p st);

static int
  StartUpdateWorkerThread(cfg_csdb_update_name_t * nameBlockp,
			  afs_status_p st);

static void *UpdateWorkerThread(void *argp);

static int
  CfgHostGetCellServDbAlias(cfg_host_p cfg_host, char *cfgHostAlias,
			    afs_status_p st);

static int
  NameBlockGetBegin(cfg_host_p cfg_host, const char *sysControlHost,
		    cfg_csdb_update_ctrl_t * ctrlBlockp, void **iterationIdP,
		    afs_status_p st);

static int
  NameBlockGetNext(void *iterationId, cfg_csdb_update_name_t * nameBlockp,
		   afs_status_p st);

static int
  NameBlockGetDone(void *iterationId, afs_status_p st);

static int
  ServerNameGetBegin(cfg_host_p cfg_host, short dbOnly, void **iterationIdP,
		     afs_status_p st);

static int
  ServerNameGetNext(void *iterationId, char *serverName, afs_status_p st);

static int
  ServerNameGetDone(void *iterationId, afs_status_p st);



/* ---------------- Exported CellServDB functions ------------------ */


/*
 * cfg_cellServDbUpdateCallBack_t -- prototype for status callback that is
 *     invoked by functions that update the CellServDB.
 *
 *     Implementation provided by library user.
 *
 * ARGUMENTS:
 *     callBackId - user-supplied context identifier; used by caller to
 *         distinguish results from different functions (or different
 *         invocations of the same function) that utilize the same callback.
 *
 *     statusItemP - pointer to status information returned by a function
 *         via this callback; an statusItemP value of NULL indicates
 *         termination of status callbacks for a given function invocation;
 *         the termination callback will not occur until all other callbacks
 *         in the set have completed.
 *
 *     status - if statusItemP is NULL then this argument indicates whether
 *         termination of status callbacks is due to logical completion
 *         or an error.
 *
 * RESTRICTIONS:
 *    The callback thread is not permitted to reenter the configuration
 *    library except to deallocate returned storage.
 */


/*
 * cfg_CellServDbAddHost() -- Add host being configured to server
 *     CellServDB on all fileserver and database machines in cell and to
 *     host's server CellServDB.
 *
 *     If a System Control machine has been configured, sysControlHost must
 *     specify the host name; otherwise, sysControlHost must be NULL.
 */
int ADMINAPI
cfg_CellServDbAddHost(void *hostHandle,	/* host config handle */
		      const char *sysControlHost,	/* sys control host */
		      cfg_cellServDbUpdateCallBack_t callBack, void *callBackId, int *maxUpdates,	/* max servers to update */
		      afs_status_p st)
{				/* completion status */
    return CellServDbUpdate(CSDB_OP_ADD, hostHandle, sysControlHost, callBack,
			    callBackId, maxUpdates, st);
}


/*
 * cfg_CellServDbRemoveHost() -- Remove host being configured from server
 *     CellServDB on all fileserver and database machines in cell and from
 *     host's server CellServDB.
 *
 *     If a System Control machine has been configured, sysControlHost must
 *     specify the host name; otherwise, sysControlHost must be NULL.
 */
int ADMINAPI
cfg_CellServDbRemoveHost(void *hostHandle,	/* host config handle */
			 const char *sysControlHost,	/* sys control host */
			 cfg_cellServDbUpdateCallBack_t callBack, void *callBackId, int *maxUpdates,	/* max servers to update */
			 afs_status_p st)
{				/* completion status */
    return CellServDbUpdate(CSDB_OP_REM, hostHandle, sysControlHost, callBack,
			    callBackId, maxUpdates, st);
}


/*
 * cfg_CellServDbEnumerate() -- Enumerate database machines known to the
 *     specified database or fileserver machine.  Enumeration is returned
 *     as a multistring.
 */
int ADMINAPI
cfg_CellServDbEnumerate(const char *fsDbHost,	/* fileserver or database host */
			char **cellName,	/* cell name for cellDbHosts */
			char **cellDbHosts,	/* cell database hosts */
			afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;

    /* validate parameters */

    if (fsDbHost == NULL || *fsDbHost == '\0') {
	tst = ADMCFGHOSTNAMENULL;
    } else if (cellName == NULL) {
	tst = ADMCFGCELLNAMENULL;
    } else if (cellDbHosts == NULL) {
	tst = ADMCFGCELLDBHOSTSNULL;
    }

    /* enumerate server CellServDB on specified host, along with cell name */

    if (tst == 0) {
	void *cellHandle;
	void *bosHandle;
	char dbhostName[MAXHOSTSPERCELL][BOS_MAX_NAME_LEN];
	char dbhostCell[BOS_MAX_NAME_LEN];
	int dbhostCount = 0;

	if (!afsclient_NullCellOpen(&cellHandle, &tst2)) {
	    tst = tst2;
	} else {
	    if (!bos_ServerOpen(cellHandle, fsDbHost, &bosHandle, &tst2)) {
		tst = tst2;
	    } else {
		void *dbIter;

		if (!bos_HostGetBegin(bosHandle, &dbIter, &tst2)) {
		    tst = tst2;
		} else {
		    for (dbhostCount = 0;; dbhostCount++) {
			char dbhostNameTemp[BOS_MAX_NAME_LEN];

			if (!bos_HostGetNext(dbIter, dbhostNameTemp, &tst2)) {
			    /* no more entries (or failure) */
			    if (tst2 != ADMITERATORDONE) {
				tst = tst2;
			    }
			    break;
			} else if (dbhostCount >= MAXHOSTSPERCELL) {
			    /* more entries than expected */
			    tst = ADMCFGCELLSERVDBTOOMANYENTRIES;
			    break;
			} else {
			    strcpy(dbhostName[dbhostCount], dbhostNameTemp);
			}
		    }

		    if (!bos_HostGetDone(dbIter, &tst2)) {
			tst = tst2;
		    }

		    if (tst == 0) {
			/* got database servers; now get cell name */
			if (!bos_CellGet(bosHandle, dbhostCell, &tst2)) {
			    tst = tst2;
			}
		    }
		}

		if (!bos_ServerClose(bosHandle, &tst2)) {
		    tst = tst2;
		}
	    }

	    if (!afsclient_CellClose(cellHandle, &tst2)) {
		tst = tst2;
	    }
	}

	if (tst == 0) {
	    /* return database hosts to caller */
	    int i;
	    size_t bufSize = 0;

	    for (i = 0; i < dbhostCount; i++) {
		bufSize += strlen(dbhostName[i]) + 1;
	    }
	    bufSize++;		/* end multistring */

	    *cellDbHosts = (char *)malloc(bufSize);

	    if (*cellDbHosts == NULL) {
		tst = ADMNOMEM;
	    } else {
		char *bufp = *cellDbHosts;

		for (i = 0; i < dbhostCount; i++) {
		    strcpy(bufp, dbhostName[i]);
		    bufp += strlen(bufp) + 1;
		}
		*bufp = '\0';
	    }

	    /* return cell name to caller */
	    if (tst == 0) {
		*cellName = (char *)malloc(strlen(dbhostCell) + 1);

		if (*cellName == NULL) {
		    free(*cellDbHosts);
			*cellDbHosts = NULL;
		    tst = ADMNOMEM;
		} else {
		    strcpy(*cellName, dbhostCell);
		}
	    }
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}



/* ---------------- Exported Utility functions ------------------ */


/*
 * cfg_CellServDbStatusDeallocate() -- Deallocate CellServDB update status
 *     record returned by library.
 */
int ADMINAPI
cfg_CellServDbStatusDeallocate(cfg_cellServDbStatus_t * statusItempP,
			       afs_status_p st)
{
	if ( statusItempP )
		free((void *)statusItempP);

    if (st != NULL) {
	*st = 0;
    }
    return 1;
}





/* ---------------- Local functions ------------------ */


/*
 * CellServDbUpdate() -- add or remove a server CellServDB entry.
 *
 *     Common function implementing cfg_CellServDb{Add/Remove}Host().
 */
static int
CellServDbUpdate(int updateOp, void *hostHandle, const char *sysControlHost,
		 cfg_cellServDbUpdateCallBack_t callBack, void *callBackId,
		 int *maxUpdates, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
    char fullSysHostName[MAXHOSTCHARS];

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (sysControlHost != NULL && *sysControlHost == '\0') {
	tst = ADMCFGHOSTNAMENULL;
    } else if (callBack == NULL) {
	tst = ADMCFGCALLBACKNULL;
    } else if (maxUpdates == NULL) {
	tst = ADMCFGUPDATECOUNTNULL;
    }

    /* resolve sys ctrl host to fully qualified name (if extant) */

    if (tst == 0) {
	if (sysControlHost != NULL) {
	    if (!cfgutil_HostNameGetFull
		(sysControlHost, fullSysHostName, &tst2)) {
		tst = tst2;
	    } else {
		sysControlHost = fullSysHostName;
	    }
	}
    }

    /* Update cell-wide server CellServDb as follows:
     *
     *   1) If system control machine is in use then update the following:
     *      system control host + database server hosts + configuration host
     *
     *      Updating the system control machine is theoretically sufficient,
     *      as all server hosts should be getting configuration information
     *      from there.  However, we don't want to have to delay further
     *      configuration until this update occurs (which could be set for
     *      any time interval).  Therefore, we compromise by manually
     *      updating the database server hosts and the host being configured.
     *
     *   2) If no system control machine is in use then update the following:
     *      fileserver hosts + database server hosts + configuration host
     *
     *   General algorithm:
     *     We create a set of server name blocks, with one thread per name
     *     block that is responsible for updating the servers in that block.
     *     All server name blocks share a single control block that stores
     *     common data and coordinates start/abort and cleanup activities.
     *     All threads wait for the start/abort signal before performing
     *     update operations so that this function is atomic.
     */

    if (tst == 0) {
	cfg_csdb_update_ctrl_t *ctrlBlockp = NULL;

	*maxUpdates = 0;

	/* create control block */

	ctrlBlockp = (cfg_csdb_update_ctrl_t *) malloc(sizeof(*ctrlBlockp));

	if (ctrlBlockp == NULL) {
	    tst = ADMNOMEM;
	} else {
	    ctrlBlockp->cfg_host = cfg_host;
	    ctrlBlockp->op = updateOp;
	    ctrlBlockp->callBack = callBack;
	    ctrlBlockp->callBackId = callBackId;
	    ctrlBlockp->disposition = CSDB_WAIT;
	    ctrlBlockp->workersActive = 0;

	    if (pthread_mutex_init(&ctrlBlockp->mutex, NULL)) {
		tst = ADMMUTEXINIT;
	    } else if (pthread_cond_init(&ctrlBlockp->event, NULL)) {
		tst = ADMCONDINIT;
	    } else {
		/* Unfortunately the bosserver adds/removes entries from
		 * the server CellServDB based on a case-sensitive string
		 * comparison, rather than using an address comparison
		 * to handle aliasing.  So we must use the name for the
		 * configuration host exactly as listed in the CellServDB.
		 *
		 * Of course the 3.5 bosserver can and should be modified to
		 * handle aliases, but we still have to deal with down-level
		 * servers in this library.
		 *
		 * To get reasonable performance, the presumption is made
		 * that all server CellServDB are identical.  This way we
		 * can look up the configuration host alias once and use
		 * it everywhere.  If this proves to be insufficient then
		 * this lookup will have to be done for every server to be
		 * updated which will be very costly; such individual lookups
		 * would naturally be handled by the update worker threads.
		 *
		 * A final presumption is that we can just look at the
		 * server CellServDB on the current database servers to
		 * get the configuration host alias.  The only time this
		 * might get us into trouble is in a re-do scenario.
		 */
		if (!CfgHostGetCellServDbAlias
		    (cfg_host, ctrlBlockp->opHostAlias, &tst2)) {
		    tst = tst2;
		} else if (*ctrlBlockp->opHostAlias == '\0') {
		    /* no alias found; go with config host working name */
		    strcpy(ctrlBlockp->opHostAlias, cfg_host->hostName);
		}
	    }

	    if (tst != 0) {
		free(ctrlBlockp);
	    } else {
		/* fill name blocks, handing each to a worker thread */
		void *nameBlockIter = NULL;
		short workersStarted = 0;

		if (!NameBlockGetBegin
		    (cfg_host, sysControlHost, ctrlBlockp, &nameBlockIter,
		     &tst2)) {
		    tst = tst2;
		} else {
		    cfg_csdb_update_name_t *nameBlockp = NULL;
		    short nameBlockDone = 0;

		    while (!nameBlockDone) {
			nameBlockp = ((cfg_csdb_update_name_t *)
				      malloc(sizeof(*nameBlockp)));

			if (nameBlockp == NULL) {
			    tst = ADMNOMEM;
			    nameBlockDone = 1;

			} else
			    if (!NameBlockGetNext
				(nameBlockIter, nameBlockp, &tst2)) {
			    /* no more entries (or failure) */
			    if (tst2 != ADMITERATORDONE) {
				tst = tst2;
			    }
			    free(nameBlockp);
			    nameBlockDone = 1;

			} else {
			    *maxUpdates += nameBlockp->serverCount;

			    if (StartUpdateWorkerThread(nameBlockp, &tst2)) {
				/* increment worker count; lock not required
				 * until workers given start/abort signal.
				 */
				ctrlBlockp->workersActive++;
				workersStarted = 1;
			    } else {
				tst = tst2;
				free(nameBlockp);
				nameBlockDone = 1;
			    }
			}
		    }

		    if (!NameBlockGetDone(nameBlockIter, &tst2)) {
			tst = tst2;
		    }
		}

		if (workersStarted) {
		    /* worker threads started; set disposition and signal */
		    if (pthread_mutex_lock(&ctrlBlockp->mutex)) {
			tst = ADMMUTEXLOCK;
		    } else {
			if (tst == 0) {
			    /* tell workers to proceed with updates */
			    ctrlBlockp->disposition = CSDB_GO;
			} else {
			    /* tell workers to abort */
			    ctrlBlockp->disposition = CSDB_ABORT;
			}

			if (pthread_mutex_unlock(&ctrlBlockp->mutex)) {
			    tst = ADMMUTEXUNLOCK;
			}
			if (pthread_cond_broadcast(&ctrlBlockp->event)) {
			    tst = ADMCONDSIGNAL;
			}
		    }
		} else {
		    /* no worker threads started */
		    free(ctrlBlockp);
		}
	    }
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * StartUpdateWorkerThread() -- start an update worker thread for the
 *     given server name block
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
StartUpdateWorkerThread(cfg_csdb_update_name_t * nameBlockp, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;
    pthread_attr_t tattr;
    pthread_t tid;

    if (pthread_attr_init(&tattr)) {
	tst = ADMTHREADATTRINIT;

    } else if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED)) {
	tst = ADMTHREADATTRSETDETACHSTATE;

    } else
	if (pthread_create
	    (&tid, &tattr, UpdateWorkerThread, (void *)nameBlockp)) {
	tst = ADMTHREADCREATE;
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * UpdateWorkerThread() -- thread for updating CellServDB of servers in
 *     a single name block.
 */
static void *
UpdateWorkerThread(void *argp)
{
    afs_status_t sync_tst = 0;
    cfg_csdb_update_name_t *nameBlockp = (cfg_csdb_update_name_t *) argp;
    int opDisposition;

    /* Pthread mutex and condition variable functions should never fail,
     * but if they do make a best effort attempt to report the problem;
     * will not be able to avoid race conditions in the face of such failures.
     */

    if (pthread_mutex_lock(&nameBlockp->ctrl->mutex)) {
	sync_tst = ADMMUTEXLOCK;
    }

    while ((opDisposition = nameBlockp->ctrl->disposition) == CSDB_WAIT) {
	/* wait for start/abort signal */
	if (pthread_cond_wait
	    (&nameBlockp->ctrl->event, &nameBlockp->ctrl->mutex)) {
	    /* avoid tight loop if condition variable wait fails */
	    cfgutil_Sleep(1);
	}
    }

    if (pthread_mutex_unlock(&nameBlockp->ctrl->mutex)) {
	sync_tst = ADMMUTEXUNLOCK;
    }

    if (opDisposition == CSDB_GO) {
	/* proceed with CellServDB update */
	int i;

	for (i = 0; i < nameBlockp->serverCount; i++) {
	    cfg_cellServDbStatus_t *statusp;

	    /* alloc memory for status information (including host name) */

	    while ((statusp = (cfg_cellServDbStatus_t *)
		    malloc(sizeof(*statusp) + AFS_MAX_SERVER_NAME_LEN)) ==
		   NULL) {
		/* avoid tight loop while waiting for status storage */
		cfgutil_Sleep(1);
	    }

	    statusp->fsDbHost = ((char *)statusp + sizeof(*statusp));

	    /* make update and set status information */

	    strcpy(statusp->fsDbHost, nameBlockp->serverName[i]);

	    if (sync_tst != 0) {
		/* report pthread problem */
		statusp->status = sync_tst;
	    } else {
		/* attempt update and report update status */
		void *bosHandle;
		afs_status_t tst2, tst = 0;

		if (!bos_ServerOpen
		    (nameBlockp->ctrl->cfg_host->cellHandle,
		     nameBlockp->serverName[i], &bosHandle, &tst2)) {
		    tst = tst2;

		} else {
		    char *opHost = nameBlockp->ctrl->opHostAlias;

		    if (nameBlockp->ctrl->op == CSDB_OP_ADD) {
			if (!bos_HostCreate(bosHandle, opHost, &tst2)) {
			    tst = tst2;
			}
		    } else {
			if (!bos_HostDelete(bosHandle, opHost, &tst2)) {
			    if (tst2 != BZNOENT) {
				tst = tst2;
			    }
			}
		    }

		    if (!bos_ServerClose(bosHandle, &tst2)) {
			tst = tst2;
		    }
		}
		statusp->status = tst;
	    }

	    /* make call back to return update status */

	    (*nameBlockp->ctrl->callBack) (nameBlockp->ctrl->callBackId,
					   statusp, 0);
	}
    }

    /* last worker makes termination call back and deallocates control block */

    if (pthread_mutex_lock(&nameBlockp->ctrl->mutex)) {
	sync_tst = ADMMUTEXLOCK;
    }

    nameBlockp->ctrl->workersActive--;

    if (nameBlockp->ctrl->workersActive == 0) {
	if (opDisposition == CSDB_GO) {
	    (*nameBlockp->ctrl->callBack) (nameBlockp->ctrl->callBackId, NULL,
					   sync_tst);
	}
	free(nameBlockp->ctrl);
    }

    if (pthread_mutex_unlock(&nameBlockp->ctrl->mutex)) {
	sync_tst = ADMMUTEXUNLOCK;
    }

    /* all workers deallocate their own name block */
    free(nameBlockp);

    return NULL;
}


/*
 * CfgHostGetCellServDbAlias() -- Get alias for configuration host name
 *     as listed in the server CellServDB.  If no alias is found then
 *     cfgHostAlias is set to the empty string.
 * 
 *     Note: cfgHostAlias is presumed to be a buffer of size MAXHOSTCHARS.
 *           Presumes all server CellServDB are identical.
 *           Only checks server CellServDB of database servers.
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
CfgHostGetCellServDbAlias(cfg_host_p cfg_host, char *cfgHostAlias,
			  afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    void *dbIter;

    if (!util_DatabaseServerGetBegin(cfg_host->cellName, &dbIter, &tst2)) {
	tst = tst2;
    } else {
	util_databaseServerEntry_t dbhostEntry;
	afs_status_t dbhostSt = 0;
	short dbhostDone = 0;
	short dbhostFound = 0;

	while (!dbhostDone) {
	    if (!util_DatabaseServerGetNext(dbIter, &dbhostEntry, &tst2)) {
		/* no more entries (or failure) */
		if (tst2 != ADMITERATORDONE) {
		    tst = tst2;
		}
		dbhostDone = 1;

	    } else
		if (!cfgutil_HostNameGetCellServDbAlias
		    (dbhostEntry.serverName, cfg_host->hostName, cfgHostAlias,
		     &tst2)) {
		/* save failure status but keep trying */
		dbhostSt = tst2;

	    } else if (*cfgHostAlias != '\0') {
		dbhostFound = 1;
		dbhostDone = 1;
	    }
	}

	if (!dbhostFound) {
	    *cfgHostAlias = '\0';

	    if (tst == 0) {
		tst = dbhostSt;
	    }
	}

	if (!util_DatabaseServerGetDone(dbIter, &tst2)) {
	    tst = tst2;
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * NameBlockGetBegin() -- initialize name block iteration
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
NameBlockGetBegin(cfg_host_p cfg_host, const char *sysControlHost,
		  cfg_csdb_update_ctrl_t * ctrlBlockp, void **iterationIdP,
		  afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_csdb_nameblock_iteration_t *nbIterp;

    nbIterp = (cfg_csdb_nameblock_iteration_t *) malloc(sizeof(*nbIterp));

    if (nbIterp == NULL) {
	tst = ADMNOMEM;
    } else {
	short dbOnly = (sysControlHost != NULL);

	nbIterp->ctrlBlockp = ctrlBlockp;
	nbIterp->cfgHost = cfg_host->hostName;
	nbIterp->sysControlHost = sysControlHost;
	nbIterp->cfgInBlock = 0;
	nbIterp->sysInBlock = 0;
	nbIterp->serverIterDone = 0;

	if (!ServerNameGetBegin
	    (cfg_host, dbOnly, &nbIterp->serverIter, &tst2)) {
	    tst = tst2;
	}

	if (tst == 0) {
	    *iterationIdP = nbIterp;
	} else {
	    free(nbIterp);
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * NameBlockGetNext() -- fill next name block
 * 
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
NameBlockGetNext(void *iterationId, cfg_csdb_update_name_t * nameBlockp,
		 afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_csdb_nameblock_iteration_t *nbIterp;
    int i;

    nbIterp = (cfg_csdb_nameblock_iteration_t *) iterationId;

    nameBlockp->ctrl = nbIterp->ctrlBlockp;
    nameBlockp->serverCount = 0;

    for (i = 0; i < SERVER_NAME_BLOCK_SIZE; i++) {
	short nameEntered = 0;

	if (!nbIterp->serverIterDone) {
	    if (ServerNameGetNext
		(nbIterp->serverIter, nameBlockp->serverName[i], &tst2)) {
		/* Got server name; check if matches cfg or sys control host.
		 * Do a simple string compare, rather than making an expensive
		 * cfgutil_HostNameIsAlias() call because it will not cause
		 * any problems to have a duplicate in the list.
		 */
		nameEntered = 1;

		if (!nbIterp->cfgInBlock) {
		    if (!strcasecmp
			(nbIterp->cfgHost, nameBlockp->serverName[i])) {
			nbIterp->cfgInBlock = 1;
		    }
		}

		if (!nbIterp->sysInBlock && nbIterp->sysControlHost != NULL) {
		    if (!strcasecmp
			(nbIterp->sysControlHost,
			 nameBlockp->serverName[i])) {
			nbIterp->sysInBlock = 1;
		    }
		}

	    } else {
		/* no more entries (or failure) */
		if (tst2 == ADMITERATORDONE) {
		    nbIterp->serverIterDone = 1;
		} else {
		    tst = tst2;
		    break;
		}
	    }
	}

	if (!nameEntered) {
	    /* include config host and (possibly) sys control host */
	    if (!nbIterp->cfgInBlock) {
		/* shouldn't be duplicate, but OK if is */
		strcpy(nameBlockp->serverName[i], nbIterp->cfgHost);
		nbIterp->cfgInBlock = 1;
		nameEntered = 1;

	    } else if (!nbIterp->sysInBlock
		       && nbIterp->sysControlHost != NULL) {
		/* shouldn't be duplicate, but OK if is */
		strcpy(nameBlockp->serverName[i], nbIterp->sysControlHost);
		nbIterp->sysInBlock = 1;
		nameEntered = 1;
	    }
	}

	if (nameEntered) {
	    nameBlockp->serverCount++;
	} else {
	    /* no more server names */
	    if (nameBlockp->serverCount == 0) {
		tst = ADMITERATORDONE;
	    }
	    break;
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * NameBlockGetDone() -- finalize name block iteration
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
NameBlockGetDone(void *iterationId, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_csdb_nameblock_iteration_t *nbIterp;

    nbIterp = (cfg_csdb_nameblock_iteration_t *) iterationId;

    if (!ServerNameGetDone(nbIterp->serverIter, &tst2)) {
	tst = tst2;
    }

    free(nbIterp);

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * ServerNameGetBegin() -- begin database server and (optionally) fileserver
 *     name enumeration
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
ServerNameGetBegin(cfg_host_p cfg_host, short dbOnly, void **iterationIdP,
		   afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_server_iteration_t *serverIterp;

    serverIterp = (cfg_server_iteration_t *) malloc(sizeof(*serverIterp));

    if (serverIterp == NULL) {
	tst = ADMNOMEM;
    } else {
	serverIterp->dbOnly = dbOnly;

	if (dbOnly) {
	    if (!util_DatabaseServerGetBegin
		(cfg_host->cellName, &serverIterp->iterationId, &tst2)) {
		tst = tst2;
	    }
	} else {
	    if (!afsclient_AFSServerGetBegin
		(cfg_host->cellHandle, &serverIterp->iterationId, &tst2)) {
		tst = tst2;
	    }
	}

	if (tst == 0) {
	    *iterationIdP = serverIterp;
	} else {
	    free(serverIterp);
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * ServerNameGetNext() -- get next server name
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
ServerNameGetNext(void *iterationId, char *serverName, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_server_iteration_t *serverIterp;

    serverIterp = (cfg_server_iteration_t *) iterationId;

    if (serverIterp->dbOnly) {
	util_databaseServerEntry_t serverEntry;

	if (!util_DatabaseServerGetNext
	    (serverIterp->iterationId, &serverEntry, &tst2)) {
	    tst = tst2;
	} else {
	    strcpy(serverName, serverEntry.serverName);
	}
    } else {
	afs_serverEntry_t serverEntry;

	if (!afsclient_AFSServerGetNext
	    (serverIterp->iterationId, &serverEntry, &tst2)) {
	    tst = tst2;
	} else {
	    strcpy(serverName, serverEntry.serverName);
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * ServerNameGetDone() -- terminate server enumeration
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
ServerNameGetDone(void *iterationId, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_server_iteration_t *serverIterp;

    serverIterp = (cfg_server_iteration_t *) iterationId;

    if (serverIterp->dbOnly) {
	if (!util_DatabaseServerGetDone(serverIterp->iterationId, &tst2)) {
	    tst = tst2;
	}
    } else {
	if (!afsclient_AFSServerGetDone(serverIterp->iterationId, &tst2)) {
	    tst = tst2;
	}
    }

    free(serverIterp);

    if (tst != 0) {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}
