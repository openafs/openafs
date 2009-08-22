/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file implements configuration functions in the following categories:
 *   cfg_BosServer*()    - configure the BOS server.
 *   cfg_DbServers*()    - configure the database servers.
 *   cfg_FileServer*()   - configure the fileserver.
 *   cfg_UpdateServer*() - configure the update server.
 *   cfg_UpdateClient*() - configure update clients.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <WINNT/afsreg.h>
#endif /* AFS_NT40_ENV */

#include <pthread.h>

#include <rx/rx.h>
#include <rx/rxstat.h>

#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_bosAdmin.h>
#include <afs/afs_clientAdmin.h>
#include <afs/afs_utilAdmin.h>
#include <afs/afs_vosAdmin.h>

#include <afs/dirpath.h>
#include <afs/bnode.h>
#include <afs/cellconfig.h>
#include <afs/bubasics.h>
#include <rx/rx_null.h>

#define UBIK_INTERNALS		/* need "internal" symbols from ubik.h */
#include <ubik.h>

#include "cfginternal.h"
#include "afs_cfgAdmin.h"


/* Local declarations and definitions */

#define KASERVER_BOSNAME "kaserver"
#define KASERVER_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/kaserver"

#define PTSERVER_BOSNAME "ptserver"
#define PTSERVER_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/ptserver"

#define VLSERVER_BOSNAME "vlserver"
#define VLSERVER_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/vlserver"

#define BUSERVER_BOSNAME "buserver"
#define BUSERVER_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/buserver"

#define UPSERVER_BOSNAME "upserver"
#define UPSERVER_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/upserver"

#define UPCLIENT_BOSNAME "upclient"
#define UPCLIENT_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/upclient"

#define FILESERVER_BOSNAME "fs"
#define FILESERVER_EXEPATH  AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/fileserver"
#define VOLSERVER_EXEPATH   AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/volserver"
#define SALVAGER_EXEPATH    AFSDIR_CANONICAL_SERVER_BIN_DIRPATH "/salvager"


static int
  SimpleProcessStart(void *bosHandle, const char *instance,
		     const char *executable, const char *args,
		     afs_status_p st);

static int
  FsProcessStart(void *bosHandle, const char *instance,
		 const char *fileserverExe, const char *volserverExe,
		 const char *salvagerExe, afs_status_p st);

static int
  BosProcessDelete(void *bosHandle, const char *instance, afs_status_p st);

static void
  UpdateCommandParse(char *cmdString, short *hasSysPathP, short *hasBinPathP);

static int
  UbikQuorumCheck(cfg_host_p cfg_host, const char *dbInstance,
		  short *hasQuorum, afs_status_p st);

static int
  UbikVoteStatusFetch(int serverAddr, unsigned short serverPort,
		      short *isSyncSite, short *isWriteReady,
		      afs_status_p st);






/* ---------------- Exported constants ---------------- */

const char *cfg_kaserverBosName = KASERVER_BOSNAME;
const char *cfg_ptserverBosName = PTSERVER_BOSNAME;
const char *cfg_vlserverBosName = VLSERVER_BOSNAME;
const char *cfg_buserverBosName = BUSERVER_BOSNAME;
const char *cfg_fileserverBosName = FILESERVER_BOSNAME;
const char *cfg_upserverBosName = UPSERVER_BOSNAME;

const char *cfg_upclientBosNamePrefix = UPCLIENT_BOSNAME;
const char *cfg_upclientSysBosSuffix = "etc";
const char *cfg_upclientBinBosSuffix = "bin";



/* ---------------- Exported BOS Server functions ------------------ */


/*
 * cfg_BosServerStart() -- Start the BOS server on host.
 *
 *     Timeout is the maximum time, in seconds, to wait for BOS to start.
 */
int ADMINAPI
cfg_BosServerStart(void *hostHandle,	/* host config handle */
		   short noAuth,	/* start in NoAuth mode */
		   unsigned int timeout,	/* timeout (in seconds) */
		   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
    short wasRunning = 0;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* start bosserver (if not already running) */

#ifdef AFS_NT40_ENV
    /* Windows - bosserver is controlled via the BOS control service */
    if (tst == 0) {
	DWORD auxArgc;
	LPCTSTR auxArgBuf[1], *auxArgv;

	if (noAuth) {
	    auxArgc = 1;
	    auxArgv = auxArgBuf;
	    auxArgBuf[0] = "-noauth";
	} else {
	    auxArgc = 0;
	    auxArgv = NULL;
	}

	if (!cfgutil_WindowsServiceStart
	    (AFSREG_SVR_SVC_NAME, auxArgc, auxArgv, timeout, &wasRunning,
	     &tst2)) {
	    /* failed to start BOS control service */
	    tst = tst2;
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */

    /* put bosserver into requested auth mode if was already running */

    if (tst == 0 && wasRunning) {
	if (!cfgutil_HostSetNoAuthFlag(cfg_host, noAuth, &tst2)) {
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
 * cfg_BosServerStop() -- Stop the BOS server on host.
 *
 *     Timeout is the maximum time, in seconds, to wait for BOS to stop.
 */
int ADMINAPI
cfg_BosServerStop(void *hostHandle,	/* host config handle */
		  unsigned int timeout,	/* timeout (in seconds) */
		  afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* stop the bosserver (if running) */

#ifdef AFS_NT40_ENV
    /* Windows - bosserver is controlled via the BOS control service */
    if (tst == 0) {
	short wasStopped;

	if (!cfgutil_WindowsServiceStop
	    (AFSREG_SVR_SVC_NAME, timeout, &wasStopped, &tst2)) {
	    /* failed to stop BOS control service */
	    tst = tst2;
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */

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
 * cfg_BosServerQueryStatus() -- Query status of BOS server on host.
 *
 *     The argument *isBosProcP is set to TRUE only if BOS processes
 *     are currently executing (as opposed to configured but stopped).
 */
int ADMINAPI
cfg_BosServerQueryStatus(void *hostHandle,	/* host config handle */
			 short *isStartedP,	/* BOS server is started */
			 short *isBosProcP,	/* BOS processes running */
			 afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (isStartedP == NULL) {
	tst = ADMCFGSTARTEDFLAGPNULL;
    } else if (isBosProcP == NULL) {
	tst = ADMCFGBOSSERVERPROCSFLAGPNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* determine if bosserver is running */

#ifdef AFS_NT40_ENV
    /* Windows - bosserver is controlled via the BOS control service */
    if (tst == 0) {
	DWORD svcState;

	*isStartedP = *isBosProcP = 0;

	if (!cfgutil_WindowsServiceQuery
	    (AFSREG_SVR_SVC_NAME, &svcState, &tst2)) {
	    tst = tst2;
	} else if (svcState == SERVICE_RUNNING) {
	    *isStartedP = 1;
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */


    /* query status of bos processes */

    if (tst == 0 && *isStartedP) {
	void *procIter;

	*isBosProcP = 0;

	if (!bos_ProcessNameGetBegin(cfg_host->bosHandle, &procIter, &tst2)) {
	    tst = tst2;
	} else {
	    /* iterate over process names, checking status of each */
	    char procName[BOS_MAX_NAME_LEN];
	    bos_ProcessExecutionState_t procState;
	    char procAuxState[BOS_MAX_NAME_LEN];
	    int procDone = 0;

	    while (!procDone) {
		if (!bos_ProcessNameGetNext(procIter, procName, &tst2)) {
		    /* no more processes (or failure) */
		    if (tst2 != ADMITERATORDONE) {
			tst = tst2;
		    }
		    procDone = 1;

		} else
		    if (!bos_ProcessExecutionStateGet
			(cfg_host->bosHandle, procName, &procState,
			 procAuxState, &tst2)) {
		    /* process removed (or failure) */
		    if (tst2 != BZNOENT) {
			tst = tst2;
			procDone = 1;
		    }

		} else {
		    if (procState != BOS_PROCESS_STOPPED) {
			*isBosProcP = 1;
			procDone = 1;
		    }
		}
	    }

	    if (!bos_ProcessNameGetDone(procIter, &tst2)) {
		tst = tst2;
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




/* ---------------- Exported Database Server functions ------------------ */



/*
 * cfg_AuthServerStart() -- Start authentication server on host and wait
 *     for server to be ready to accept requests.
 *
 *     This function is intended to be used when configuring the first
 *     machine in a cell; it enables the AFS server principal to be created
 *     and configured before starting the remaining database servers.
 */
int ADMINAPI
cfg_AuthServerStart(void *hostHandle,	/* host config handle */
		    afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* create and start authentication server instance */

    if (tst == 0) {
	if (!SimpleProcessStart
	    (cfg_host->bosHandle, KASERVER_BOSNAME, KASERVER_EXEPATH, NULL,
	     &tst2)) {
	    tst = tst2;
	}
    }

    /* wait for authentication server to achieve quorum */

    if (tst == 0) {
	time_t timeStart = time(NULL);
	short kaHasQuorum;

	while (1) {
	    if (!UbikQuorumCheck
		(cfg_host, KASERVER_BOSNAME, &kaHasQuorum, &tst2)) {
		tst = tst2;
		break;

	    } else if (kaHasQuorum) {
		/* quorum on authentication server */
		break;

	    } else {
		/* quorum not yet achieved on authentication server */
		if (difftime(time(NULL), timeStart) > 180) {
		    tst = ADMCFGQUORUMWAITTIMEOUT;
		    break;
		} else {
		    cfgutil_Sleep(2);
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
 * cfg_DbServersStart() -- Start the standard (required) database servers,
 *     and optionally the backup database server, on host.
 *
 *     The BOS instance names used are the string constants:
 *         cfg_kaserverBosName - authentication server
 *         cfg_ptserverBosName - protection server
 *         cfg_vlserverBosName - volume location server
 *         cfg_buserverBosName - backup server
 */
int ADMINAPI
cfg_DbServersStart(void *hostHandle,	/* host config handle */
		   short startBkDb,	/* start backup server */
		   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* create and start database server instances */

    if (tst == 0) {
	/* try all regardless of failures; last error code wins */
	if (!SimpleProcessStart
	    (cfg_host->bosHandle, KASERVER_BOSNAME, KASERVER_EXEPATH, NULL,
	     &tst2)) {
	    tst = tst2;
	}
	if (!SimpleProcessStart
	    (cfg_host->bosHandle, PTSERVER_BOSNAME, PTSERVER_EXEPATH, NULL,
	     &tst2)) {
	    tst = tst2;
	}
	if (!SimpleProcessStart
	    (cfg_host->bosHandle, VLSERVER_BOSNAME, VLSERVER_EXEPATH, NULL,
	     &tst2)) {
	    tst = tst2;
	}
	if (startBkDb
	    && !SimpleProcessStart(cfg_host->bosHandle, BUSERVER_BOSNAME,
				   BUSERVER_EXEPATH, NULL, &tst2)) {
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
 * cfg_DbServersStop() -- Stop, and unconfigure, the database servers on host.
 */
int ADMINAPI
cfg_DbServersStop(void *hostHandle,	/* host config handle */
		  afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete database server instances */

    if (tst == 0) {
	/* try all regardless of failures; last error code wins */
	if (!BosProcessDelete(cfg_host->bosHandle, KASERVER_BOSNAME, &tst2)) {
	    tst = tst2;
	}
	if (!BosProcessDelete(cfg_host->bosHandle, PTSERVER_BOSNAME, &tst2)) {
	    tst = tst2;
	}
	if (!BosProcessDelete(cfg_host->bosHandle, VLSERVER_BOSNAME, &tst2)) {
	    tst = tst2;
	}
	if (!BosProcessDelete(cfg_host->bosHandle, BUSERVER_BOSNAME, &tst2)) {
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
 * cfg_DbServersQueryStatus() -- Query status of database servers on host.
 *
 *     If detailed status information is not required, detailsP can be NULL.
 *
 *     If *isStdDbP is FALSE and *isBkDbP is TRUE then the host is in an
 *     inconsistent state; the remaining database servers should be configured.
 */
int ADMINAPI
cfg_DbServersQueryStatus(void *hostHandle,	/* host config handle */
			 short *isStdDbP,	/* std DB servers configured */
			 short *isBkDbP,	/* backup DB server configured */
			 cfg_dbServersStatus_t * detailsP,	/* config details */
			 afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
    short inCellServDb, isKaserver, isPtserver, isVlserver, isBuserver;

    inCellServDb = isKaserver = isPtserver = isVlserver = isBuserver = 0;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (isStdDbP == NULL || isBkDbP == NULL) {
	tst = ADMCFGDBSERVERCONFIGFLAGPNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* query host's server CellServDb to see if it lists itself */

    if (tst == 0) {
	char hostNameAlias[MAXHOSTCHARS];

	if (!cfgutil_HostNameGetCellServDbAlias
	    (cfg_host->hostName, cfg_host->hostName, hostNameAlias, &tst2)) {
	    tst = tst2;
	} else if (*hostNameAlias != '\0') {
	    /* host in its own CellServDb */
	    inCellServDb = 1;
	}
    }

    /* query bosserver to determine what database servers are configured */

    if (tst == 0) {
	bos_ProcessType_t procType;
	bos_ProcessInfo_t procInfo;

	if (bos_ProcessInfoGet
	    (cfg_host->bosHandle, KASERVER_BOSNAME, &procType, &procInfo,
	     &tst2)) {
	    isKaserver = 1;
	} else if (tst2 != BZNOENT) {
	    tst = tst2;
	}

	if (tst == 0) {
	    if (bos_ProcessInfoGet
		(cfg_host->bosHandle, PTSERVER_BOSNAME, &procType, &procInfo,
		 &tst2)) {
		isPtserver = 1;
	    } else if (tst2 != BZNOENT) {
		tst = tst2;
	    }
	}

	if (tst == 0) {
	    if (bos_ProcessInfoGet
		(cfg_host->bosHandle, VLSERVER_BOSNAME, &procType, &procInfo,
		 &tst2)) {
		isVlserver = 1;
	    } else if (tst2 != BZNOENT) {
		tst = tst2;
	    }
	}

	if (tst == 0) {
	    if (bos_ProcessInfoGet
		(cfg_host->bosHandle, BUSERVER_BOSNAME, &procType, &procInfo,
		 &tst2)) {
		isBuserver = 1;
	    } else if (tst2 != BZNOENT) {
		tst = tst2;
	    }
	}
    }

    if (tst == 0) {
	/* success; return results */
	*isStdDbP = (inCellServDb && isKaserver && isPtserver && isVlserver);
	*isBkDbP = (inCellServDb && isBuserver);

	if (detailsP) {
	    detailsP->inCellServDb = inCellServDb;
	    detailsP->isKaserver = isKaserver;
	    detailsP->isPtserver = isPtserver;
	    detailsP->isVlserver = isVlserver;
	    detailsP->isBuserver = isBuserver;
	}
    } else {
	/* indicate failure */
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_DbServersRestartAll() -- Restart all database servers in host's cell.
 */
int ADMINAPI
cfg_DbServersRestartAll(void *hostHandle,	/* host config handle */
			afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* restart all database servers in host's cell */

    if (tst == 0) {
	void *dbIter;

	if (!bos_HostGetBegin(cfg_host->bosHandle, &dbIter, &tst2)) {
	    tst = tst2;
	} else {
	    /* iterate over server CellServDb, restarting db servers */
	    char dbhostName[BOS_MAX_NAME_LEN];
	    void *dbhostHandle;
	    int dbhostDone = 0;

	    while (!dbhostDone) {
		if (!bos_HostGetNext(dbIter, dbhostName, &tst2)) {
		    /* no more entries (or failure) */
		    if (tst2 != ADMITERATORDONE) {
			tst = tst2;
		    }
		    dbhostDone = 1;

		} else
		    if (!bos_ServerOpen
			(cfg_host->cellHandle, dbhostName, &dbhostHandle,
			 &tst2)) {
		    /* failed to get bos handle; note error but keep going */
		    tst = tst2;

		} else {
		    /* restart db servers; note errors, but keep going */
		    if (!bos_ProcessRestart
			(dbhostHandle, KASERVER_BOSNAME, &tst2)) {
			tst = tst2;
		    }
		    if (!bos_ProcessRestart
			(dbhostHandle, PTSERVER_BOSNAME, &tst2)) {
			tst = tst2;
		    }
		    if (!bos_ProcessRestart
			(dbhostHandle, VLSERVER_BOSNAME, &tst2)) {
			tst = tst2;
		    }
		    if (!bos_ProcessRestart
			(dbhostHandle, BUSERVER_BOSNAME, &tst2)) {
			/* may not be running a backup server */
			if (tst2 != BZNOENT) {
			    tst = tst2;
			}
		    }

		    if (!bos_ServerClose(dbhostHandle, &tst2)) {
			tst = tst2;
		    }
		}
	    }

	    if (!bos_HostGetDone(dbIter, &tst2)) {
		tst = tst2;
	    }
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;

	/* should really utilize a callback (or some other mechanism) to
	 * indicate which restarts failed and why.
	 */
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_DbServersWaitForQuorum() -- Wait for database servers in host's cell
 *     to achieve quorum.
 *
 *     Timeout is the maximum time, in seconds, to wait for quorum.
 *
 *     NOTE: Function does not check for backup server quorum since
 *           configuration does not require modifying the backup database.
 */
int ADMINAPI
cfg_DbServersWaitForQuorum(void *hostHandle,	/* host config handle */
			   unsigned int timeout,	/* timeout in sec. */
			   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* wait for the database servers in host's cell to achieve quorum */

    if (tst == 0) {
	time_t timeStart = time(NULL);
	short kaHasQuorum, ptHasQuorum, vlHasQuorum;

	kaHasQuorum = ptHasQuorum = vlHasQuorum = 0;

	while (1) {
	    if (!kaHasQuorum) {
		if (!UbikQuorumCheck
		    (cfg_host, KASERVER_BOSNAME, &kaHasQuorum, &tst2)) {
		    tst = tst2;
		    break;
		}
	    }

	    if (!ptHasQuorum) {
		if (!UbikQuorumCheck
		    (cfg_host, PTSERVER_BOSNAME, &ptHasQuorum, &tst2)) {
		    tst = tst2;
		    break;
		}
	    }

	    if (!vlHasQuorum) {
		if (!UbikQuorumCheck
		    (cfg_host, VLSERVER_BOSNAME, &vlHasQuorum, &tst2)) {
		    tst = tst2;
		    break;
		}
	    }

	    if (kaHasQuorum && ptHasQuorum && vlHasQuorum) {
		/* quorum on all dbservers of interest */
		break;
	    } else {
		/* quorum not yet achieved for one or more dbservers */
		if ((timeout == 0)
		    || (difftime(time(NULL), timeStart) > timeout)) {
		    tst = ADMCFGQUORUMWAITTIMEOUT;
		    break;
		} else {
		    cfgutil_Sleep(10);
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
 * cfg_DbServersStopAllBackup() -- Stop, and unconfigure, all backup servers
 *     in host's cell.
 */
int ADMINAPI
cfg_DbServersStopAllBackup(void *hostHandle,	/* host config handle */
			   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete all backup servers in host's cell */

    if (tst == 0) {
	void *dbIter;

	if (!bos_HostGetBegin(cfg_host->bosHandle, &dbIter, &tst2)) {
	    tst = tst2;
	} else {
	    /* iterate over server CellServDb, unconfiguring backup servers */
	    char dbhostName[BOS_MAX_NAME_LEN];
	    void *dbhostHandle;
	    int dbhostDone = 0;

	    while (!dbhostDone) {
		if (!bos_HostGetNext(dbIter, dbhostName, &tst2)) {
		    /* no more entries (or failure) */
		    if (tst2 != ADMITERATORDONE) {
			tst = tst2;
		    }
		    dbhostDone = 1;

		} else
		    if (!bos_ServerOpen
			(cfg_host->cellHandle, dbhostName, &dbhostHandle,
			 &tst2)) {
		    /* failed to get bos handle; note error but keep going */
		    tst = tst2;

		} else {
		    /* unconfig backup server; note errors, but keep going */
		    if (!BosProcessDelete
			(dbhostHandle, BUSERVER_BOSNAME, &tst2)) {
			tst = tst2;
		    }

		    if (!bos_ServerClose(dbhostHandle, &tst2)) {
			tst = tst2;
		    }
		}
	    }

	    if (!bos_HostGetDone(dbIter, &tst2)) {
		tst = tst2;
	    }
	}
    }

    if (tst != 0) {
	/* indicate failure */
	rc = 0;

	/* should really utilize a callback (or some other mechanism) to
	 * indicate which stops failed and why.
	 */
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}





/* ---------------- Exported File Server functions ------------------ */


/*
 * cfg_FileServerStart() -- Start the file server on host.
 *
 *     The BOS instance name used is the string constant cfg_fileserverBosName.
 */
int ADMINAPI
cfg_FileServerStart(void *hostHandle,	/* host config handle */
		    afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* create and start file server instance */

    if (tst == 0) {
	if (!FsProcessStart
	    (cfg_host->bosHandle, FILESERVER_BOSNAME, FILESERVER_EXEPATH,
	     VOLSERVER_EXEPATH, SALVAGER_EXEPATH, &tst2)) {
	    tst = tst2;
	} else {
	    /* TO BE DONE: need a reliable "is started and ready" check */
	    cfgutil_Sleep(5);
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
 * cfg_FileServerStop() -- Stop, and unconfigure, the file server on host.
 */
int ADMINAPI
cfg_FileServerStop(void *hostHandle,	/* host config handle */
		   afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete file server instance */

    if (tst == 0) {
	if (!BosProcessDelete(cfg_host->bosHandle, FILESERVER_BOSNAME, &tst2)) {
	    tst = tst2;
	} else {
	    /* file server instance deleted; remove its addresses from VLDB */
	    int addrCount, i;
	    afs_int32 *addrList = NULL;

	    /* note: ignore any errors since address removal is optional;
	     * e.g., a common source of errors will be attempting to remove
	     * an address while volumes tied to that address are still listed
	     * in the VLDB (in which case the address is not removed).
	     */
	    if (cfgutil_HostAddressFetchAll
		(cfg_host->hostName, &addrCount, &addrList, &tst2)) {
		for (i = 0; i < addrCount; i++) {
		    (void)vos_FileServerAddressRemove(cfg_host->cellHandle,
						      NULL, addrList[i],
						      &tst2);
		}
			if (addrList) {
				free(addrList);
				addrList = NULL;
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
 * cfg_FileServerQueryStatus() -- Query status of file server on host.
 */
int ADMINAPI
cfg_FileServerQueryStatus(void *hostHandle,	/* host config handle */
			  short *isFsP,	/* file server configured */
			  afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (isFsP == NULL) {
	tst = ADMCFGFILESERVERCONFIGFLAGPNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* query bosserver to determine if fileserver is configured */

    if (tst == 0) {
	bos_ProcessType_t procType;
	bos_ProcessInfo_t procInfo;

	*isFsP = 0;

	if (bos_ProcessInfoGet
	    (cfg_host->bosHandle, FILESERVER_BOSNAME, &procType, &procInfo,
	     &tst2)) {
	    /* instance exists; check type for good measure */
	    if (procType == BOS_PROCESS_FS) {
		*isFsP = 1;
	    }
	} else if (tst2 != BZNOENT) {
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




/* ---------------- Exported Update Server functions ------------------ */



/*
 * cfg_UpdateServerStart() -- Start the Update server on host.
 *
 *     Argument strings exportClear and exportCrypt each specify a set of
 *     space-separated directories to export or are NULL.
 *
 *     The BOS instance name used is the string constant cfg_upserverBosName.
 */
int ADMINAPI
cfg_UpdateServerStart(void *hostHandle,	/* host config handle */
		      const char *exportClear,	/* dirs to export in clear */
		      const char *exportCrypt,	/* dirs to export encrypted */
		      afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete existing update server instance, if any.
     * we do this because the set of exported directores might be changing.
     */

    if (tst == 0) {
	if (!BosProcessDelete(cfg_host->bosHandle, UPSERVER_BOSNAME, &tst2)) {
	    tst = tst2;
	}
    }

    /* create and start update server instance */

    if (tst == 0) {
	char argsBuf[AFSDIR_PATH_MAX], *args;
	size_t argsLen = 0;

	if (exportClear != NULL && *exportClear != '\0') {
	    argsLen += strlen("-clear ") + strlen(exportClear) + 1;
	}
	if (exportCrypt != NULL && *exportCrypt != '\0') {
	    argsLen += strlen("-crypt ") + strlen(exportCrypt) + 1;
	}

	if (argsLen == 0) {
	    args = NULL;
	} else {
	    if (argsLen <= AFSDIR_PATH_MAX) {
		args = argsBuf;
	    } else {
		args = (char *)malloc(argsLen);
	    }

	    if (args == NULL) {
		tst = ADMNOMEM;
	    } else {
		if (exportClear == NULL) {
		    sprintf(args, "-crypt %s", exportCrypt);
		} else if (exportCrypt == NULL) {
		    sprintf(args, "-clear %s", exportClear);
		} else {
		    sprintf(args, "-clear %s -crypt %s", exportClear,
			    exportCrypt);
		}
	    }
	}

	if (tst == 0) {
	    if (!SimpleProcessStart
		(cfg_host->bosHandle, UPSERVER_BOSNAME, UPSERVER_EXEPATH,
		 args, &tst2)) {
		tst = tst2;
	    }

	    if (args != NULL && args != argsBuf) {
			free(args);
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
 * cfg_UpdateServerStop() -- Stop, and unconfigure, the Update server on host.
 */
int ADMINAPI
cfg_UpdateServerStop(void *hostHandle,	/* host config handle */
		     afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete upserver instance */

    if (tst == 0) {
	if (!BosProcessDelete(cfg_host->bosHandle, UPSERVER_BOSNAME, &tst2)) {
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
 * cfg_UpdateServerQueryStatus() -- Query status of Update server on host.
 */
int ADMINAPI
cfg_UpdateServerQueryStatus(void *hostHandle,	/* host config handle */
			    short *isUpserverP,	/* update server configured */
			    short *isSysCtrlP,	/* system control configured */
			    short *isBinDistP,	/* binary dist configured */
			    afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (isUpserverP == NULL || isSysCtrlP == NULL
	       || isBinDistP == NULL) {
	tst = ADMCFGUPSERVERCONFIGFLAGPNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* query bosserver to determine if, and how, upserver is configured */

    if (tst == 0) {
	void *cmdIter;

	*isUpserverP = *isSysCtrlP = *isBinDistP = 0;

	if (!bos_ProcessParameterGetBegin
	    (cfg_host->bosHandle, UPSERVER_BOSNAME, &cmdIter, &tst2)) {
	    tst = tst2;
	} else {
	    char cmdString[BOS_MAX_NAME_LEN];

	    if (!bos_ProcessParameterGetNext(cmdIter, cmdString, &tst2)) {
		/* no upserver instance (or error) */
		if (tst2 != BZNOENT) {
		    tst = tst2;
		}
	    } else {
		/* parse upserver command line to determine how configured */
		short hasSysPath, hasBinPath;

		UpdateCommandParse(cmdString, &hasSysPath, &hasBinPath);

		*isUpserverP = 1;

		if (hasSysPath) {
		    *isSysCtrlP = 1;
		}
		if (hasBinPath) {
		    *isBinDistP = 1;
		}
	    }

	    if (!bos_ProcessParameterGetDone(cmdIter, &tst2)) {
		tst = tst2;
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
 * cfg_SysBinServerStart() -- Start Update server in System Control and/or
 *     Binary Distribution machine configuration on host.
 *
 *     This function is a convenience wrapper for cfg_UpdateServerStart().
 */
int ADMINAPI
cfg_SysBinServerStart(void *hostHandle,	/* host config handle */
		      short makeSysCtrl,	/* config as sys control mach */
		      short makeBinDist,	/* config as binary dist mach */
		      afs_status_p st)
{				/* completion status */
    char *cryptSysDir = NULL;
    char *clearBinDir = NULL;

    if (makeSysCtrl) {
	cryptSysDir = AFSDIR_CANONICAL_SERVER_ETC_DIRPATH;
    }

    if (makeBinDist) {
	clearBinDir = AFSDIR_CANONICAL_SERVER_BIN_DIRPATH;
    }

    return cfg_UpdateServerStart(hostHandle, clearBinDir, cryptSysDir, st);
}



/* ---------------- Exported Update Client functions ------------------ */



/*
 * cfg_UpdateClientStart() -- Start an Update client on host.
 *
 *     Argument string import specifies a set of space-separated directories
 *     to import.  Argument frequency specifies the import interval in
 *     seconds; if the value is zero (0) then the default frequency is used.
 *
 *     The BOS instance name used is the concatenation of the string constant
 *     cfg_upclientBosNamePrefix and the argument string bosSuffix.
 */
int ADMINAPI
cfg_UpdateClientStart(void *hostHandle,	/* host config handle */
		      const char *bosSuffix,	/* BOS instance suffix */
		      const char *upserver,	/* upserver to import from */
		      short crypt,	/* import encrypted */
		      const char *import,	/* dirs to import */
		      unsigned int frequency,	/* import interval in sec. */
		      afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
    char upclientInstance[BOS_MAX_NAME_LEN];

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (bosSuffix == NULL) {
	tst = ADMCFGUPCLIENTSUFFIXNULL;
    } else if ((strlen(UPCLIENT_BOSNAME) + strlen(bosSuffix) + 1) >
	       BOS_MAX_NAME_LEN) {
	tst = ADMCFGUPCLIENTSUFFIXTOOLONG;
    } else if (upserver == NULL || *upserver == '\0') {
	tst = ADMCFGUPCLIENTTARGETSERVERNULL;
    } else if (import == NULL || *import == '\0') {
	tst = ADMCFGUPCLIENTIMPORTDIRNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete existing update client instance, if any.
     * we do this because the set of imported directores might be changing.
     */

    if (tst == 0) {
	sprintf(upclientInstance, "%s%s", UPCLIENT_BOSNAME, bosSuffix);

	if (!BosProcessDelete(cfg_host->bosHandle, upclientInstance, &tst2)) {
	    tst = tst2;
	}
    }

    /* create and start update client instance */

    if (tst == 0) {
	char argsBuf[AFSDIR_PATH_MAX], *args;
	size_t argsLen = 0;

	argsLen += strlen(upserver) + 1;

	if (crypt) {
	    argsLen += strlen("-crypt ");
	} else {
	    argsLen += strlen("-clear ");
	}

	if (frequency != 0) {
	    argsLen += strlen("-t ") + 10 /* max uint */  + 1;
	}

	argsLen += strlen(import) + 1;

	if (argsLen <= AFSDIR_PATH_MAX) {
	    args = argsBuf;
	} else {
	    args = (char *)malloc(argsLen);
	}

	if (args == NULL) {
	    tst = ADMNOMEM;
	} else {
	    /* set up argument buffer */
	    if (crypt) {
		sprintf(args, "%s -crypt ", upserver);
	    } else {
		sprintf(args, "%s -clear ", upserver);
	    }
	    if (frequency != 0) {
		char *strp = args + strlen(args);
		sprintf(strp, "-t %u ", frequency);
	    }
	    strcat(args, import);

	    /* create and start instance */
	    if (!SimpleProcessStart
		(cfg_host->bosHandle, upclientInstance, UPCLIENT_EXEPATH,
		 args, &tst2)) {
		tst = tst2;
	    }

	    if (args != argsBuf) {
			free(args);
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
 * cfg_UpdateClientStop() -- Stop, and unconfigure, an Update client on host.
 */
int ADMINAPI
cfg_UpdateClientStop(void *hostHandle,	/* host config handle */
		     const char *bosSuffix,	/* BOS instance suffix */
		     afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
    char upclientInstance[BOS_MAX_NAME_LEN];

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (bosSuffix == NULL) {
	tst = ADMCFGUPCLIENTSUFFIXNULL;
    } else if ((strlen(UPCLIENT_BOSNAME) + strlen(bosSuffix) + 1) >
	       BOS_MAX_NAME_LEN) {
	tst = ADMCFGUPCLIENTSUFFIXTOOLONG;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* stop and delete specified update client instance */

    if (tst == 0) {
	sprintf(upclientInstance, "%s%s", UPCLIENT_BOSNAME, bosSuffix);

	if (!BosProcessDelete(cfg_host->bosHandle, upclientInstance, &tst2)) {
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
 * cfg_UpdateClientStopAll() -- Stop, and unconfigure, all Update clients
 *     on host.
 */
int ADMINAPI
cfg_UpdateClientStopAll(void *hostHandle,	/* host config handle */
			afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* find, stop, and delete all update client instances */

    if (tst == 0) {
	void *procIter;

	if (!bos_ProcessNameGetBegin(cfg_host->bosHandle, &procIter, &tst2)) {
	    tst = tst2;
	} else {
	    /* iterate over process names, looking for update clients */
	    char procName[BOS_MAX_NAME_LEN];
	    int procDone = 0;

	    while (!procDone) {
		if (!bos_ProcessNameGetNext(procIter, procName, &tst2)) {
		    /* no more processes (or failure) */
		    if (tst2 != ADMITERATORDONE) {
			tst = tst2;
		    }
		    procDone = 1;

		} else
		    if (!strncmp
			(UPCLIENT_BOSNAME, procName,
			 (sizeof(UPCLIENT_BOSNAME) - 1))) {
		    /* upclient instance prefix; assume is upclient */
		    if (!BosProcessDelete
			(cfg_host->bosHandle, procName, &tst2)) {
			tst = tst2;
			procDone = 1;
		    }
		}
	    }

	    if (!bos_ProcessNameGetDone(procIter, &tst2)) {
		tst = tst2;
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
 * cfg_UpdateClientQueryStatus() -- Query status of Update clients on host.
 */
int ADMINAPI
cfg_UpdateClientQueryStatus(void *hostHandle,	/* host config handle */
			    short *isUpclientP,	/* an upclient is configured */
			    short *isSysP,	/* system control client */
			    short *isBinP,	/* binary dist. client */
			    afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters and prepare host handle for bos functions */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (isUpclientP == NULL || isSysP == NULL || isBinP == NULL) {
	tst = ADMCFGUPCLIENTCONFIGFLAGPNULL;
    } else if (!cfgutil_HostHandleBosInit(cfg_host, &tst2)) {
	tst = tst2;
    }

    /* determine if, and how, any upclients are configured */

    if (tst == 0) {
	void *procIter;

	*isUpclientP = *isSysP = *isBinP = 0;

	if (!bos_ProcessNameGetBegin(cfg_host->bosHandle, &procIter, &tst2)) {
	    tst = tst2;
	} else {
	    /* iterate over process names, looking for update clients */
	    char procName[BOS_MAX_NAME_LEN];
	    int procDone = 0;

	    while (!procDone) {
		if (!bos_ProcessNameGetNext(procIter, procName, &tst2)) {
		    /* no more processes (or failure) */
		    if (tst2 != ADMITERATORDONE) {
			tst = tst2;
		    }
		    procDone = 1;

		} else
		    if (!strncmp
			(UPCLIENT_BOSNAME, procName,
			 (sizeof(UPCLIENT_BOSNAME) - 1))) {
		    /* upclient instance prefix; assume is upclient */
		    void *cmdIter;

		    *isUpclientP = 1;

		    if (!bos_ProcessParameterGetBegin
			(cfg_host->bosHandle, procName, &cmdIter, &tst2)) {
			tst = tst2;
		    } else {
			char cmdString[BOS_MAX_NAME_LEN];

			if (!bos_ProcessParameterGetNext
			    (cmdIter, cmdString, &tst2)) {
			    /* instance deleted out from under us (or error) */
			    if (tst2 != BZNOENT) {
				tst = tst2;
			    }
			} else {
			    /* parse command line to determine how config */
			    short hasSysPath, hasBinPath;

			    UpdateCommandParse(cmdString, &hasSysPath,
					       &hasBinPath);

			    if (hasSysPath) {
				*isSysP = 1;
			    }
			    if (hasBinPath) {
				*isBinP = 1;
			    }
			}

			if (!bos_ProcessParameterGetDone(cmdIter, &tst2)) {
			    tst = tst2;
			}
		    }

		    if (tst != 0) {
			procDone = 1;
		    }
		}
	    }			/* while() */

	    if (!bos_ProcessNameGetDone(procIter, &tst2)) {
		tst = tst2;
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
 * cfg_SysControlClientStart() -- Start an Update client in System Control
 *     client configuration on host.
 *
 *     This function is a convenience wrapper for cfg_UpdateClientStart().
 *     The BOS instance suffix used is the constant cfg_upclientSysBosSuffix.
 */
int ADMINAPI
cfg_SysControlClientStart(void *hostHandle,	/* host config handle */
			  const char *upserver,	/* upserver to import from */
			  afs_status_p st)
{				/* completion status */
    return cfg_UpdateClientStart(hostHandle, cfg_upclientSysBosSuffix,
				 upserver, 1 /* crypt */ ,
				 AFSDIR_CANONICAL_SERVER_ETC_DIRPATH,
				 0 /* default frequency */ ,
				 st);
}


/*
 * cfg_BinDistClientStart() -- Start an Update client in Binary Distribution
 *     client configuration on host.
 *
 *     This function is a convenience wrapper for cfg_UpdateClientStart().
 *     The BOS instance suffix used is the constant cfg_upclientBinBosSuffix.
 */
int ADMINAPI
cfg_BinDistClientStart(void *hostHandle,	/* host config handle */
		       const char *upserver,	/* upserver to import from */
		       afs_status_p st)
{				/* completion status */
    return cfg_UpdateClientStart(hostHandle, cfg_upclientBinBosSuffix,
				 upserver, 0 /* crypt */ ,
				 AFSDIR_CANONICAL_SERVER_BIN_DIRPATH,
				 0 /* default frequency */ ,
				 st);
}



/* ---------------- Local functions ------------------ */

/*
 * SimpleProcessStart() -- create and start a simple bosserver instance.
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
SimpleProcessStart(void *bosHandle, const char *instance,
		   const char *executable, const char *args, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    char cmdBuf[AFSDIR_PATH_MAX], *cmd;
    size_t cmdLen;

    cmdLen = strlen(executable) + 1 + (args == NULL ? 0 : (strlen(args) + 1));

    if (cmdLen <= AFSDIR_PATH_MAX) {
	cmd = cmdBuf;
    } else {
	cmd = (char *)malloc(cmdLen);
    }

    if (cmd == NULL) {
	tst = ADMNOMEM;
    } else {
	if (args == NULL) {
	    strcpy(cmd, executable);
	} else {
	    sprintf(cmd, "%s %s", executable, args);
	}

	if (!bos_ProcessCreate
	    (bosHandle, (char *)instance, BOS_PROCESS_SIMPLE, cmd, NULL, NULL, &tst2)
	    && tst2 != BZEXISTS) {
	    /* failed to create instance (and not because existed) */
	    tst = tst2;
	} else
	    if (!bos_ProcessExecutionStateSet
		(bosHandle, (char *)instance, BOS_PROCESS_RUNNING, &tst2)) {
	    /* failed to set instance state to running */
	    tst = tst2;
	}

	if (cmd != cmdBuf) {
	    free(cmd);
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
 * FsProcessStart() -- create and start a fs bosserver instance.
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
FsProcessStart(void *bosHandle, const char *instance,
	       const char *fileserverExe, const char *volserverExe,
	       const char *salvagerExe, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;

    if (!bos_FSProcessCreate
	(bosHandle, (char *)instance, (char *)fileserverExe, (char *)volserverExe, (char *)salvagerExe, NULL,
	 &tst2) && tst2 != BZEXISTS) {
	/* failed to create instance (and not because existed) */
	tst = tst2;
    } else
	if (!bos_ProcessExecutionStateSet
	    (bosHandle, instance, BOS_PROCESS_RUNNING, &tst2)) {
	/* failed to set instance state to running */
	tst = tst2;
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
 * BosProcessDelete() -- stop and delete a bosserver instance, if it exists.
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
BosProcessDelete(void *bosHandle, const char *instance, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;

    if (!bos_ProcessExecutionStateSet
	(bosHandle, instance, BOS_PROCESS_STOPPED, &tst2)) {
	/* failed to set instance state to stopped (or does not exist) */
	if (tst2 != BZNOENT) {
	    tst = tst2;
	}

    } else if (!bos_ProcessAllWaitTransition(bosHandle, &tst2)) {
	/* failed to wait for process to stop */
	tst = tst2;

    } else if (!bos_ProcessDelete(bosHandle, (char *)instance, &tst2)) {
	/* failed to delete instance (or does not exist) */
	if (tst2 != BZNOENT) {
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
 * UpdateCommandParse() -- Parse an upserver or upclient command to determine:
 *         1) if it explicitly exports/imports the system control directory
 *         2) if it explicitly exports/imports the binary directory
 *
 *    NOTE: cmdString altered (made all lower case and forward slashes)
 */
static void
UpdateCommandParse(char *cmdString, short *hasSysPathP, short *hasBinPathP)
{
    char *argp, *dirp;

    *hasSysPathP = *hasBinPathP = 0;

    /* make command string all lower case and forward slashes */

    for (argp = cmdString; *argp != '\0'; argp++) {
	if (isupper(*argp)) {
	    *argp = tolower(*argp);
	} else if (*argp == '\\') {
	    *argp = '/';
	}
    }

    /* find end of update executable path (and hence beginning of arguments */

    argp = cmdString;

    while (isspace(*argp)) {
	argp++;
    }
    while (*argp != '\0' && !isspace(*argp)) {
	argp++;
    }

    /* search for well-known system control directory */

    dirp = strstr(argp, AFSDIR_CANONICAL_SERVER_ETC_DIRPATH);

    if (dirp != NULL) {
	/* check that not a portition of a larger path */
	char oneBefore, oneAfter;
	char twoAfter = 0;

	oneBefore = *(dirp - 1);
	oneAfter = *(dirp + sizeof(AFSDIR_CANONICAL_SERVER_ETC_DIRPATH) - 1);

	if (oneAfter != '\0') {
	    twoAfter = *(dirp + sizeof(AFSDIR_CANONICAL_SERVER_ETC_DIRPATH));
	}

	if (isspace(oneBefore)) {
	    if ((isspace(oneAfter)) || (oneAfter == '\0')
		|| (oneAfter == '/'
		    && (isspace(twoAfter) || twoAfter == '\0'))) {
		*hasSysPathP = 1;
	    }
	}
    }

    /* search for well-known binary directory */

    dirp = strstr(argp, AFSDIR_CANONICAL_SERVER_BIN_DIRPATH);

    if (dirp != NULL) {
	/* check that not a portition of a larger path */
	char oneBefore, oneAfter;
	char twoAfter = 0;

	oneBefore = *(dirp - 1);
	oneAfter = *(dirp + sizeof(AFSDIR_CANONICAL_SERVER_BIN_DIRPATH) - 1);

	if (oneAfter != '\0') {
	    twoAfter = *(dirp + sizeof(AFSDIR_CANONICAL_SERVER_BIN_DIRPATH));
	}

	if (isspace(oneBefore)) {
	    if ((isspace(oneAfter)) || (oneAfter == '\0')
		|| (oneAfter == '/'
		    && (isspace(twoAfter) || twoAfter == '\0'))) {
		*hasBinPathP = 1;
	    }
	}
    }
}



/*
 * UbikQuorumCheck() -- Determine if Ubik has achieved quorum for a specified
 *     database instance in host's cell.
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
UbikQuorumCheck(cfg_host_p cfg_host, const char *dbInstance, short *hasQuorum,
		afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    void *dbIter;

    *hasQuorum = 0;

    if (!bos_HostGetBegin(cfg_host->bosHandle, &dbIter, &tst2)) {
	tst = tst2;
    } else {
	/* iterate over server CellServDb, looking for dbserver sync site */
	char dbhostName[BOS_MAX_NAME_LEN];
	int dbhostAddr;
	unsigned short dbhostPort = 0;
	int dbhostDone = 0;
	int dbhostQueries = 0;

	if (!strcmp(dbInstance, KASERVER_BOSNAME)) {
	    dbhostPort = AFSCONF_KAUTHPORT;
	} else if (!strcmp(dbInstance, PTSERVER_BOSNAME)) {
	    dbhostPort = AFSCONF_PROTPORT;
	} else if (!strcmp(dbInstance, VLSERVER_BOSNAME)) {
	    dbhostPort = AFSCONF_VLDBPORT;
	} else if (!strcmp(dbInstance, BUSERVER_BOSNAME)) {
	    dbhostPort = AFSCONF_BUDBPORT;
	}

	while (!dbhostDone) {
	    if (!bos_HostGetNext(dbIter, dbhostName, &tst2)) {
		/* no more entries (or failure) */
		if (tst2 == ADMITERATORDONE) {
		    if (dbhostQueries == 0) {
			/* consider quorum to have been achieved when no
			 * database servers in cell; otherwise higher-level
			 * functions will timeout and fail.
			 */
			*hasQuorum = 1;
		    }
		} else {
		    tst = tst2;
		}
		dbhostDone = 1;

	    } else
		if (!util_AdminServerAddressGetFromName
		    (dbhostName, &dbhostAddr, &tst2)) {
		tst = tst2;
		dbhostDone = 1;
	    } else {
		short isSyncSite = 0;
	        short isWriteReady = 0;

		/* ignore errors fetching Ubik vote status; there might be
		 * an unreachable dbserver yet a reachable sync site.
		 */
		dbhostQueries++;

		if (UbikVoteStatusFetch
		    (dbhostAddr, dbhostPort, &isSyncSite, &isWriteReady,
		     &tst2)) {
		    /* have quorum if is sync site AND is ready for updates */
		    if (isSyncSite) {
			if (isWriteReady) {
			    *hasQuorum = 1;
			}
			dbhostDone = 1;
		    }
		}
	    }
	}

	if (!bos_HostGetDone(dbIter, &tst2)) {
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
 * UbikVoteStatusFetch() -- Fetch Ubik vote status parameters of interest from
 *     specified server and port.
 *
 * RETURN CODES: 1 success, 0 failure  (st indicates why)
 */
static int
UbikVoteStatusFetch(int serverAddr, unsigned short serverPort,
		    short *isSyncSite, short *isWriteReady, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst = 0;
    struct rx_securityClass *nullSecurity;
    struct rx_connection *serverConn;

    nullSecurity = rxnull_NewClientSecurityObject();	/* never fails */

    if ((serverConn =
	 rx_GetCachedConnection(htonl(serverAddr), htons(serverPort),
				VOTE_SERVICE_ID, nullSecurity, 0)) == NULL) {
	tst = ADMCFGUBIKVOTENOCONNECTION;
    } else {
	int rpcCode;
	struct ubik_debug udebugInfo;

	if ((rpcCode = VOTE_Debug(serverConn, &udebugInfo)) == 0) {
	    /* talking to a 3.5 or later server */
	    *isSyncSite = (udebugInfo.amSyncSite ? 1 : 0);
	    *isWriteReady = 0;

	    if (*isSyncSite) {
		/* as of 3.5 the database is writeable if "labeled" or if all
		 * prior recovery states have been achieved; see defect 9477.
		 */
		if (((udebugInfo.recoveryState & UBIK_RECLABELDB))
		    || ((udebugInfo.recoveryState & UBIK_RECSYNCSITE)
			&& (udebugInfo.recoveryState & UBIK_RECFOUNDDB)
			&& (udebugInfo.recoveryState & UBIK_RECHAVEDB))) {
		    *isWriteReady = 1;
		}
	    }

	} else if (rpcCode == RXGEN_OPCODE) {
	    /* talking to old (pre 3.5) server */
	    struct ubik_debug_old udebugInfo;

	    if ((rpcCode = VOTE_DebugOld(serverConn, &udebugInfo)) == 0) {
		*isSyncSite = (udebugInfo.amSyncSite ? 1 : 0);
		*isWriteReady = 0;

		if (*isSyncSite) {
		    /* pre 3.5 the database is writeable only if "labeled" */
		    if (udebugInfo.recoveryState & UBIK_RECLABELDB) {
			*isWriteReady = 1;
		    }
		}
	    }
	}

	(void)rx_ReleaseCachedConnection(serverConn);

	tst = rpcCode;
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
