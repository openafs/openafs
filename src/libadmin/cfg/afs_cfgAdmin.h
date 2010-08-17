/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_CFG_ADMIN_H
#define OPENAFS_CFG_ADMIN_H

#include <afs/param.h>
#include <afs/afs_Admin.h>


/* INTRODUCTION:
 *
 * This API provides a mechanism for configuring AFS server processes.
 * The API is designed to support both local and remote configuration,
 * though remote configuration may not be supported in the library
 * implementation.
 *
 * Functions in the API are partitioned into logical categories:
 *   cfg_Host*()         - manipulate static server configuration information.
 *   cfg_Client*()       - perform minimally necessary client configuration.
 *   cfg_CellServDb*()   - manage the cell-wide server CellServDb database.
 *   cfg_BosServer*()    - configure the BOS server.
 *   cfg_DbServers*()    - configure the database servers.
 *   cfg_FileServer*()   - configure the fileserver.
 *   cfg_UpdateServer*() - configure the update server.
 *   cfg_UpdateClient*() - configure update clients.
 *
 * Within a given function category, there may exist convenience wrappers
 * that do not begin with the common function prefix for that category.
 * Also provided are utility functions that complement the above; most
 * of these are deallocator functions.
 *
 * USAGE:
 *
 * General usage proceeds as follows:
 *     - static server configuration is set via the cfg_Host*() functions.
 *     - static client configuration is set via the cfg_Client*() functions.
 *     - server processes are dynamically configured via the functions
 *       in each server category; for configuring database servers, the
 *       server CellServDb is manipulated via the cfg_CellServDb*() functions.
 *
 * Detailed usage information is provided elsewhere.
 *
 * NOTES:
 *
 * 1) The intent is to implement all functions to be idempotent (i.e., so
 *    that they can be called more than once w/o adverse side effects).
 * 2) As functions are implemented, specific error codes of interest
 *    to users (e.g., GUI tools) will be documented so that appropriate
 *    action can be taken on failure.
 */



/* ----------- Type and constant declarations --------- */


/* AFS partition table entry */
typedef struct {
    char *partitionName;	/* vice partition name */
    char *deviceName;		/* device path */
} cfg_partitionEntry_t;

/* Status callback invoked by functions that update the CellServDB. */
typedef struct {
    char *fsDbHost;		/* host on which CellServDB update was attempted */
    afs_status_t status;	/* update completion status */
} cfg_cellServDbStatus_t;

typedef void

  (ADMINAPI * cfg_cellServDbUpdateCallBack_t) (void *callBackId,
					       cfg_cellServDbStatus_t *
					       statusItemP,
					       afs_status_t status);

/* BOS instance names used to configure database servers */
ADMINEXPORT extern const char *cfg_kaserverBosName;
ADMINEXPORT extern const char *cfg_ptserverBosName;
ADMINEXPORT extern const char *cfg_vlserverBosName;
ADMINEXPORT extern const char *cfg_buserverBosName;


/* Database server status information */
typedef struct {
    short inCellServDb;		/* host in its own server CellServDB */
    short isKaserver;		/* authentication server configured */
    short isPtserver;		/* protection server configured */
    short isVlserver;		/* volume location server configured */
    short isBuserver;		/* backup server configured */

    /*  isStdDb = (inCellServDb && isKaserver && isPtserver && isVlserver)
     *  isBkDbP = (inCellServDb && isBuserver)
     */
} cfg_dbServersStatus_t;


/* BOS instance name used to configure file server */
ADMINEXPORT extern const char *cfg_fileserverBosName;

/* BOS instance name used to configure update server */
ADMINEXPORT extern const char *cfg_upserverBosName;

/* BOS instance prefix used to configure all update clients;  full instance
 * name is the concatenation of this prefix and a specified suffix.
 */
ADMINEXPORT extern const char *cfg_upclientBosNamePrefix;

/* Default BOS instance suffix used to configure System Control client */
ADMINEXPORT extern const char *cfg_upclientSysBosSuffix;

/* Default BOS instance suffix used to configure Binary Distribution client */
ADMINEXPORT extern const char *cfg_upclientBinBosSuffix;



/* ---------------- Server Host ------------------ */


extern int ADMINAPI cfg_HostQueryStatus(const char *hostName,
					afs_status_p configStP,
					char **cellNameP, afs_status_p st);

extern int ADMINAPI cfg_HostOpen(void *cellHandle, const char *hostName,
				 void **hostHandleP, afs_status_p st);

extern int ADMINAPI cfg_HostClose(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_HostSetCell(void *hostHandle, const char *cellName,
				    const char *cellDbHosts, afs_status_p st);

extern int ADMINAPI cfg_HostSetAfsPrincipal(void *hostHandle, short isFirst,
					    const char *passwd,
					    afs_status_p st);

extern int ADMINAPI cfg_HostSetAdminPrincipal(void *hostHandle, short isFirst,
					      char *admin,
					      const char *passwd,
					      unsigned int afsUid,
					      afs_status_p st);

extern int ADMINAPI cfg_HostInvalidate(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_HostPartitionTableEnumerate(void *hostHandle,
						    cfg_partitionEntry_t **
						    tablePP, int *nEntriesP,
						    afs_status_p st);

extern int ADMINAPI cfg_HostPartitionTableAddEntry(void *hostHandle,
						   const char *partName,
						   const char *devName,
						   afs_status_p st);

extern int ADMINAPI cfg_HostPartitionTableRemoveEntry(void *hostHandle,
						      const char *partName,
						      afs_status_p st);

extern int ADMINAPI cfg_HostPartitionNameValid(const char *partName,
					       short *isValidP,
					       afs_status_p st);

extern int ADMINAPI cfg_HostDeviceNameValid(const char *devName,
					    short *isValidP, afs_status_p st);




/* ---------------- AFS Client ------------------ */


extern int ADMINAPI cfg_ClientQueryStatus(const char *hostName,
					  short *isInstalledP,
					  unsigned *versionP,
					  afs_status_p configStP,
					  char **cellNameP, afs_status_p st);

extern int ADMINAPI cfg_ClientSetCell(void *hostHandle, const char *cellName,
				      const char *cellDbHosts,
				      afs_status_p st);

extern int ADMINAPI cfg_ClientCellServDbAdd(void *hostHandle,
					    const char *cellName,
					    const char *dbentry,
					    afs_status_p st);

extern int ADMINAPI cfg_ClientCellServDbRemove(void *hostHandle,
					       const char *cellName,
					       const char *dbentry,
					       afs_status_p st);

extern int ADMINAPI cfg_ClientStop(void *hostHandle, unsigned int timeout,
				   afs_status_p st);

extern int ADMINAPI cfg_ClientStart(void *hostHandle, unsigned int timeout,
				    afs_status_p st);



/* ---------------- CellServDB ------------------ */


extern int ADMINAPI cfg_CellServDbAddHost(void *hostHandle,
					  const char *sysControlHost,
					  cfg_cellServDbUpdateCallBack_t
					  callBack, void *callBackId,
					  int *maxUpdates, afs_status_p st);

extern int ADMINAPI cfg_CellServDbRemoveHost(void *hostHandle,
					     const char *sysControlHost,
					     cfg_cellServDbUpdateCallBack_t
					     callBack, void *callBackId,
					     int *maxUpdates,
					     afs_status_p st);

extern int ADMINAPI cfg_CellServDbEnumerate(const char *fsDbHost,
					    char **cellName,
					    char **cellDbHosts,
					    afs_status_p st);



/* ---------------- BOS Server  ------------------ */


extern int ADMINAPI cfg_BosServerStart(void *hostHandle, short noAuth,
				       unsigned int timeout, afs_status_p st);

extern int ADMINAPI cfg_BosServerStop(void *hostHandle, unsigned int timeout,
				      afs_status_p st);

extern int ADMINAPI cfg_BosServerQueryStatus(void *hostHandle,
					     short *isStartedP,
					     short *isBosProcP,
					     afs_status_p st);



/* ---------------- Database Servers ------------------ */


extern int ADMINAPI cfg_AuthServerStart(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_DbServersStart(void *hostHandle, short startBkDb,
				       afs_status_p st);

extern int ADMINAPI cfg_DbServersStop(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_DbServersQueryStatus(void *hostHandle,
					     short *isStdDbP, short *isBkDbP,
					     cfg_dbServersStatus_t * detailsP,
					     afs_status_p st);

extern int ADMINAPI cfg_DbServersRestartAll(void *hostHandle,
					    afs_status_p st);

extern int ADMINAPI cfg_DbServersWaitForQuorum(void *hostHandle,
					       unsigned int timeout,
					       afs_status_p st);

extern int ADMINAPI cfg_DbServersStopAllBackup(void *hostHandle,
					       afs_status_p st);



/* ---------------- File Server ------------------ */


extern int ADMINAPI cfg_FileServerStart(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_FileServerStop(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_FileServerQueryStatus(void *hostHandle, short *isFsP,
					      afs_status_p st);



/* ---------------- Update Server ------------------ */


extern int ADMINAPI cfg_UpdateServerStart(void *hostHandle,
					  const char *exportClear,
					  const char *exportCrypt,
					  afs_status_p st);

extern int ADMINAPI cfg_UpdateServerStop(void *hostHandle, afs_status_p st);

extern int ADMINAPI cfg_UpdateServerQueryStatus(void *hostHandle,
						short *isUpserverP,
						short *isSysCtrlP,
						short *isBinDistP,
						afs_status_p st);

extern int ADMINAPI cfg_SysBinServerStart(void *hostHandle, short makeSysCtrl,
					  short makeBinDist, afs_status_p st);



/* ---------------- Update Client ------------------ */


extern int ADMINAPI cfg_UpdateClientStart(void *hostHandle,
					  const char *bosSuffix,
					  const char *upserver, short crypt,
					  const char *import,
					  unsigned int frequency,
					  afs_status_p st);

extern int ADMINAPI cfg_UpdateClientStop(void *hostHandle,
					 const char *bosSuffix,
					 afs_status_p st);

extern int ADMINAPI cfg_UpdateClientStopAll(void *hostHandle,
					    afs_status_p st);

extern int ADMINAPI cfg_UpdateClientQueryStatus(void *hostHandle,
						short *isUpclientP,
						short *isSysP, short *isBinP,
						afs_status_p st);

extern int ADMINAPI cfg_SysControlClientStart(void *hostHandle,
					      const char *upserver,
					      afs_status_p st);

extern int ADMINAPI cfg_BinDistClientStart(void *hostHandle,
					   const char *upserver,
					   afs_status_p st);



/* ---------------- Utilities ------------------ */


extern int ADMINAPI cfg_StringDeallocate(char *stringDataP, afs_status_p st);

extern int ADMINAPI cfg_PartitionListDeallocate(cfg_partitionEntry_t *
						partitionListDataP,
						afs_status_p st);

extern int ADMINAPI cfg_CellServDbStatusDeallocate(cfg_cellServDbStatus_t *
						   statusItempP,
						   afs_status_p st);

#endif /* OPENAFS_CFG_ADMIN_H */
