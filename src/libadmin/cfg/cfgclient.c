/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* This file implements configuration functions in the following categories:
 *   cfg_Client*()       - perform minimally necessary client configuration.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <afs/stds.h>

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>
#include <afs/afs_utilAdmin.h>

#include <afs/dirpath.h>
#include <afs/cellconfig.h>

#ifdef AFS_NT40_ENV
#include <windows.h>
#include <WINNT/afsreg.h>
#include <WINNT/afssw.h>
#include <cellservdb.h>
#endif

#include "cfginternal.h"
#include "afs_cfgAdmin.h"


/* Local declarations and definitions */

#define CSDB_OP_ADD  0		/* add a client CellServDB entry */
#define CSDB_OP_REM  1		/* remove a client CellServDB entry */

static int
  ClientCellServDbUpdate(int updateOp, void *hostHandle, const char *cellName,
			 const char *dbentry, afs_status_p st);

static int
  CacheManagerStart(unsigned timeout, afs_status_p st);

static int
  CacheManagerStop(unsigned timeout, afs_status_p st);




/* ---------------- Exported AFS Client functions ------------------ */


/*
 * cfg_ClientQueryStatus() -- Query status of static client configuration
 *     on host, i.e., status of required configuration files, etc.
 *     Upon successful completion *configStP is set to the client
 *     configuration status, with a value of zero (0) indicating that
 *     the configuration is valid.
 *
 *     If client configuration is not valid then *cellNameP is set to NULL;
 *     otherwise, *cellNameP is an allocated buffer containing client cell.
 *
 *     If client software (cache-manager) is not installed then *versionP is
 *     undefined; otherwise *versionP is 34 for 3.4, 35 for 3.5, etc.
 *
 *     Note: Client configuration is checked even if the client software
 *           is not installed.  This is useful for tools that require
 *           client configuration information but NOT the actual
 *           client (cache-manager); for example, the AFS Server Manager.
 */
int ADMINAPI
cfg_ClientQueryStatus(const char *hostName,	/* name of host */
		      short *isInstalledP,	/* client software installed */
		      unsigned *versionP,	/* client software version */
		      afs_status_p configStP,	/* client config status */
		      char **cellNameP,	/* client's cell */
		      afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    afs_status_t clientSt = 0;
    char *clientCellName = NULL;
    short cmInstalled = 0;
    unsigned cmVersion = 0;

    /* validate parameters */

    if (hostName == NULL || *hostName == '\0') {
	tst = ADMCFGHOSTNAMENULL;
    } else if (strlen(hostName) > (MAXHOSTCHARS - 1)) {
	tst = ADMCFGHOSTNAMETOOLONG;
    } else if (isInstalledP == NULL) {
	tst = ADMCFGINSTALLEDFLAGPNULL;
    } else if (versionP == NULL) {
	tst = ADMCFGVERSIONPNULL;
    } else if (configStP == NULL) {
	tst = ADMCFGCONFIGSTATUSPNULL;
    } else if (cellNameP == NULL) {
	tst = ADMCFGCELLNAMEPNULL;
    }

    /* remote configuration not yet supported; hostName must be local host */

    if (tst == 0) {
	short isLocal;

	if (!cfgutil_HostNameIsLocal(hostName, &isLocal, &tst2)) {
	    tst = tst2;
	} else if (!isLocal) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* determine if client software (CM) is installed and if so what version */

#ifdef AFS_NT40_ENV
    /* Windows - cache manager is a service */
    if (tst == 0) {
	DWORD svcState;

	if (!cfgutil_WindowsServiceQuery
	    (AFSREG_CLT_SVC_NAME, &svcState, &tst2)) {
	    /* CM not installed, or insufficient privilege to check */
	    if (tst2 == ADMNOPRIV) {
		tst = tst2;
	    } else {
		cmInstalled = 0;
	    }
	} else {
	    /* CM installed, get version */
	    unsigned major, minor, patch;

	    cmInstalled = 1;

	    if (afssw_GetClientVersion(&major, &minor, &patch)) {
		/* failed to retrieve version information */
		if (errno == EACCES) {
		    tst = ADMNOPRIV;
		} else {
		    tst = ADMCFGCLIENTVERSIONNOTREAD;
		}
	    } else {
		cmVersion = (major * 10) + minor;
	    }
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */


    /* check static client configuration; not necessary that client
     * software (CM) be installed for this information to be valid and useable.
     */

    if (tst == 0) {
	struct afsconf_dir *confdir;

	if ((confdir = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH)) == NULL) {
	    /* the client configuration appears to be missing/invalid */
	    clientSt = ADMCFGCLIENTBASICINFOINVALID;
	} else {
	    struct afsconf_entry *cellentry;

	    if (confdir->cellName == NULL || *confdir->cellName == '\0') {
		/* no cell set for client */
		clientSt = ADMCFGCLIENTNOTINCELL;
	    } else {
		for (cellentry = confdir->entries; cellentry != NULL;
		     cellentry = cellentry->next) {
		    if (!strcasecmp
			(confdir->cellName, cellentry->cellInfo.name)) {
			break;
		    }
		}

		if (cellentry == NULL) {
		    clientSt = ADMCFGCLIENTCELLNOTINDB;
		} else if (cellentry->cellInfo.numServers <= 0) {
		    clientSt = ADMCFGCLIENTCELLHASNODBENTRIES;
		}
	    }

	    if (tst == 0 && clientSt == 0) {
		/* everything looks good; malloc cell name buffer to return */
		clientCellName =
		    (char *)malloc(strlen(cellentry->cellInfo.name) + 1);
		if (clientCellName == NULL) {
		    tst = ADMNOMEM;
		} else {
		    strcpy(clientCellName, cellentry->cellInfo.name);
		}
	    }

	    (void)afsconf_Close(confdir);
	}
    }

    /* return result of query */

    if (tst == 0) {
	/* return client status and cell name */
	*isInstalledP = cmInstalled;
	*versionP = cmVersion;
	*configStP = clientSt;

	if (clientSt == 0) {
	    *cellNameP = clientCellName;
	} else {
	    *cellNameP = NULL;
	}
    } else {
	/* indicate failure */
	rc = 0;

	/* free cell name if allocated before failure */
	if (clientCellName != NULL) {
	    free(clientCellName);
	}
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_ClientSetCell() -- Define default client cell for host.
 *
 *     The cellDbHosts argument is a multistring containing the names of
 *     the existing database servers already configured in the cell; this
 *     multistring list can be obtained via cfg_CellServDbEnumerate().
 *     If configuring the first server in a new cell then the cellDbHosts
 *     list contains only the name of that host.
 *
 *     Warning: client (cache-manager) should be stopped prior to setting cell.
 */
int ADMINAPI
cfg_ClientSetCell(void *hostHandle,	/* host config handle */
		  const char *cellName,	/* cell name */
		  const char *cellDbHosts,	/* cell database hosts */
		  afs_status_p st)
{				/* completion status */
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;

    /* validate parameters */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (cellName == NULL || *cellName == '\0') {
	tst = ADMCFGCELLNAMENULL;
    } else if (strlen(cellName) > (MAXCELLCHARS - 1)) {
	tst = ADMCFGCELLNAMETOOLONG;
    } else if (!cfgutil_HostHandleCellNameCompatible(cfg_host, cellName)) {
	tst = ADMCFGCELLNAMECONFLICT;
    } else if (cellDbHosts == NULL || *cellDbHosts == '\0') {
	tst = ADMCFGCELLDBHOSTSNULL;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* define cell database hosts */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	CELLSERVDB clientDb;

	if (!CSDB_ReadFile(&clientDb, AFSDIR_CLIENT_CELLSERVDB_FILEPATH)) {
	    tst = ADMCFGCLIENTCELLSERVDBNOTREAD;
	} else {
	    CELLDBLINE *cellLinep = CSDB_FindCell(&clientDb, cellName);

	    if (cellLinep != NULL) {
		/* cell entry exists; remove host entries */
		if (!CSDB_RemoveCellServers(&clientDb, cellLinep)) {
		    /* should never happen */
		    tst = ADMCFGCLIENTCELLSERVDBEDITFAILED;
		}
	    } else {
		/* cell entry does not exist; add it */
		cellLinep = CSDB_AddCell(&clientDb, cellName, NULL, NULL);

		if (cellLinep == NULL) {
		    tst = ADMNOMEM;
		}
	    }

	    if (tst == 0) {
		/* add new host entries to cell */
		const char *dbHost = cellDbHosts;
		int dbHostCount = 0;

		while (*dbHost != '\0' && tst == 0) {
		    size_t dbHostLen = strlen(dbHost);
		    const char *dbHostAddrStr;

		    if (dbHostLen > (MAXHOSTCHARS - 1)) {
			tst = ADMCFGHOSTNAMETOOLONG;
		    } else if (dbHostCount >= MAXHOSTSPERCELL) {
			tst = ADMCFGCELLDBHOSTCOUNTTOOLARGE;
		    } else
			if (!cfgutil_HostNameGetAddressString
			    (dbHost, &dbHostAddrStr, &tst2)) {
			tst = tst2;
		    } else
			if (CSDB_AddCellServer
			    (&clientDb, cellLinep, dbHostAddrStr,
			     dbHost) == NULL) {
			tst = ADMNOMEM;
		    } else {
			dbHostCount++;
			dbHost += dbHostLen + 1;
		    }
		}

		if (tst == 0) {
		    /* edit successful; write CellServDB */
		    if (!CSDB_WriteFile(&clientDb)) {
			tst = ADMCFGCLIENTCELLSERVDBNOTWRITTEN;
		    }
		}
	    }

	    CSDB_FreeFile(&clientDb);
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */


    /* define default client cell */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	if (afssw_SetClientCellName(cellName)) {
	    /* failed to set cell name in registry (ThisCell equivalent) */
	    if (errno == EACCES) {
		tst = ADMNOPRIV;
	    } else {
		tst = ADMCFGCLIENTTHISCELLNOTWRITTEN;
	    }
	}
    }
#else
    if (tst == 0) {
	/* function not yet implemented for Unix */
	tst = ADMCFGNOTSUPPORTED;
    }
#endif /* AFS_NT40_ENV */


    /* help any underlying packages adjust to cell change */

    if (tst == 0) {
	int rc;

	if ((rc = ka_CellConfig(AFSDIR_CLIENT_ETC_DIRPATH)) != 0) {
	    tst = rc;
	}
    }

    if (tst != 0) {
	rc = 0;
    }
    if (st != NULL) {
	*st = tst;
    }
    return rc;
}


/*
 * cfg_ClientCellServDbAdd() -- Add entry to client CellServDB on host.
 */
int ADMINAPI
cfg_ClientCellServDbAdd(void *hostHandle,	/* host config handle */
			const char *cellName,	/* cell name */
			const char *dbentry,	/* cell database entry */
			afs_status_p st)
{				/* completion status */
    return ClientCellServDbUpdate(CSDB_OP_ADD, hostHandle, cellName, dbentry,
				  st);
}


/*
 * cfg_ClientCellServDbRemove() -- Remove entry from client CellServDB
 *     on host.
 */
int ADMINAPI
cfg_ClientCellServDbRemove(void *hostHandle,	/* host config handle */
			   const char *cellName,	/* cell name */
			   const char *dbentry,	/* cell database entry */
			   afs_status_p st)
{				/* completion status */
    return ClientCellServDbUpdate(CSDB_OP_REM, hostHandle, cellName, dbentry,
				  st);
}


/*
 * cfg_ClientStop() -- Stop the client (cache manager) on host.
 *
 *     Timeout is the maximum time (in seconds) to wait for client to stop.
 */
int ADMINAPI
cfg_ClientStop(void *hostHandle,	/* host config handle */
	       unsigned int timeout,	/* timeout in seconds */
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

    /* stop client */

    if (tst == 0) {
	if (!CacheManagerStop(timeout, &tst2)) {
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
 * cfg_ClientStart() -- Start the client (cache manager) on host.
 *
 *     Timeout is the maximum time (in seconds) to wait for client to start.
 */
int ADMINAPI
cfg_ClientStart(void *hostHandle,	/* host config handle */
		unsigned int timeout,	/* timeout in seconds */
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

    /* start client */

    if (tst == 0) {
	if (!CacheManagerStart(timeout, &tst2)) {
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



/* ---------------- Local functions ------------------ */


/*
 * ClientCellServDbUpdate() -- add or remove a client CellServDB entry.
 *
 *     Common function implementing cfg_ClientCellServDb{Add/Remove}().
 */
static int
ClientCellServDbUpdate(int updateOp, void *hostHandle, const char *cellName,
		       const char *dbentry, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;
    cfg_host_p cfg_host = (cfg_host_p) hostHandle;
    char dbentryFull[MAXHOSTCHARS];

    /* validate parameters and resolve dbentry to fully qualified name */

    if (!cfgutil_HostHandleValidate(cfg_host, &tst2)) {
	tst = tst2;
    } else if (cellName == NULL || *cellName == '\0') {
	tst = ADMCFGCELLNAMENULL;
    } else if (strlen(cellName) > (MAXCELLCHARS - 1)) {
	tst = ADMCFGCELLNAMETOOLONG;
    } else if (dbentry == NULL || *dbentry == '\0') {
	tst = ADMCFGHOSTNAMENULL;
    } else if (strlen(dbentry) > (MAXHOSTCHARS - 1)) {
	tst = ADMCFGHOSTNAMETOOLONG;
    } else if (!cfgutil_HostNameGetFull(dbentry, dbentryFull, &tst2)) {
	tst = tst2;
    }

    /* remote configuration not yet supported in this function */

    if (tst == 0) {
	if (!cfg_host->is_local) {
	    tst = ADMCFGNOTSUPPORTED;
	}
    }

    /* modify local client CellServDB entry for specified cell */

#ifdef AFS_NT40_ENV
    if (tst == 0) {
	CELLSERVDB clientDb;

	if (!CSDB_ReadFile(&clientDb, AFSDIR_CLIENT_CELLSERVDB_FILEPATH)) {
	    tst = ADMCFGCLIENTCELLSERVDBNOTREAD;
	} else {
	    CELLDBLINE *cellLinep = CSDB_FindCell(&clientDb, cellName);
	    CELLDBLINE *serverLinep = NULL;
	    int serverLineCount = 0;

	    if (cellLinep != NULL) {
		/* found cellName, now find server to add/remove */
		CELLDBLINE *workingLinep;

		for (workingLinep = cellLinep->pNext; workingLinep != NULL;
		     workingLinep = workingLinep->pNext) {
		    CELLDBLINEINFO lineInfo;

		    if (!CSDB_CrackLine(&lineInfo, workingLinep->szLine)) {
			/* not a server (or cell) line; perhaps a comment */
			continue;
		    } else if (lineInfo.szCell[0] != '\0') {
			/* hit a new cell line */
			break;
		    } else {
			/* found a server line; check if is host of interest */
			short isValid;
			int dbentryAddr = ntohl(lineInfo.ipServer);

			serverLineCount++;

			if (!cfgutil_HostAddressIsValid
			    (dbentryFull, dbentryAddr, &isValid, &tst2)) {
			    tst = tst2;
			    break;
			} else if (isValid) {
			    /* found server of interest */
			    serverLinep = workingLinep;
			    break;
			}
		    }
		}
	    }

	    if (tst == 0) {
		if (updateOp == CSDB_OP_ADD && serverLinep == NULL) {
		    if (cellLinep == NULL) {
			cellLinep =
			    CSDB_AddCell(&clientDb, cellName, NULL, NULL);
		    }

		    if (cellLinep == NULL) {
			tst = ADMNOMEM;
		    } else if (serverLineCount >= MAXHOSTSPERCELL) {
			tst = ADMCFGCLIENTCELLSERVDBNOSPACE;
		    } else {
			const char *dbentryAddrStr;

			if (!cfgutil_HostNameGetAddressString
			    (dbentryFull, &dbentryAddrStr, &tst2)) {
			    tst = tst2;
			} else {
			    serverLinep =
				CSDB_AddCellServer(&clientDb, cellLinep,
						   dbentryAddrStr,
						   dbentryFull);
			    if (serverLinep == NULL) {
				tst = ADMNOMEM;
			    }
			}
		    }
		} else if (updateOp == CSDB_OP_REM && serverLinep != NULL) {
		    (void)CSDB_RemoveLine(&clientDb, serverLinep);
		}

		if (tst == 0) {
		    if (!CSDB_WriteFile(&clientDb)) {
			tst = ADMCFGCLIENTCELLSERVDBNOTWRITTEN;
		    }
		}
	    }

	    CSDB_FreeFile(&clientDb);
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
 * CacheManagerStart() -- Start the local AFS cache manager.
 *
 *     Timeout is the maximum time (in seconds) to wait for the CM to start.
 *
 * RETURN CODES: 1 success, 0 failure (st indicates why)
 */
static int
CacheManagerStart(unsigned timeout, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;

#ifdef AFS_NT40_ENV
    /* Windows - cache manager is a service */
    short wasRunning;

    if (!cfgutil_WindowsServiceStart
	(AFSREG_CLT_SVC_NAME, 0, NULL, timeout, &wasRunning, &tst2)) {
	tst = tst2;
    }
#else
    /* function not yet implemented for Unix */
    tst = ADMCFGNOTSUPPORTED;
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
 * CacheManagerStop() -- Stop the local AFS cache manager.
 *
 *     Timeout is the maximum time (in seconds) to wait for the CM to stop.
 *
 * RETURN CODES: 1 success, 0 failure (st indicates why)
 */
static int
CacheManagerStop(unsigned timeout, afs_status_p st)
{
    int rc = 1;
    afs_status_t tst2, tst = 0;

#ifdef AFS_NT40_ENV
    /* Windows - cache manager is a service */
    short wasStopped;

    if (!cfgutil_WindowsServiceStop
	(AFSREG_CLT_SVC_NAME, timeout, &wasStopped, &tst2)) {
	tst = tst2;
    }
#else
    /* function not yet implemented for Unix */
    tst = ADMCFGNOTSUPPORTED;
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
