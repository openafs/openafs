/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef OPENAFS_UTIL_ADMIN_H
#define OPENAFS_UTIL_ADMIN_H

#include <afs/afs_Admin.h>
#include <afs/afs_AdminErrors.h>

#define UTIL_MAX_DATABASE_SERVER_NAME 64
#define UTIL_MAX_CELL_NAME_LEN 256
#define UTIL_MAX_CELL_HOSTS 8
#define UTIL_MAX_RXDEBUG_VERSION_LEN 64


typedef struct util_databaseServerEntry {
    int serverAddress;
    char serverName[UTIL_MAX_DATABASE_SERVER_NAME];
} util_databaseServerEntry_t, *util_databaseServerEntry_p;

extern int ADMINAPI util_AdminErrorCodeTranslate(afs_status_t errorCode,
						 int langId,
						 const char **errorTextP,
						 afs_status_p st);

extern int ADMINAPI util_DatabaseServerGetBegin(const char *cellName,
						void **iterationIdP,
						afs_status_p st);

extern int ADMINAPI util_DatabaseServerGetNext(const void *iterationId,
					       util_databaseServerEntry_p
					       serverP, afs_status_p st);

extern int ADMINAPI util_DatabaseServerGetDone(const void *iterationId,
					       afs_status_p st);

extern int ADMINAPI util_AdminServerAddressGetFromName(const char *serverName,
						       int *serverAddress,
						       afs_status_p st);

extern int ADMINAPI CellHandleIsValid(const void *cellHandle,
				      afs_status_p st);

struct rpcStats;
extern int ADMINAPI util_RPCStatsGetBegin(struct rx_connection *conn,
					  int (*rpc) (struct rx_connection *,
						      afs_uint32, afs_uint32 *,
						      afs_uint32 *, afs_uint32 *,
						      afs_uint32 *,
						      struct rpcStats *),
					  void **iterationIdP,
					  afs_status_p st);

extern int ADMINAPI util_RPCStatsGetNext(const void *iterationId,
					 afs_RPCStats_p stats,
					 afs_status_p st);

extern int ADMINAPI util_RPCStatsGetDone(const void *iterationId,
					 afs_status_p st);

extern int ADMINAPI util_RPCStatsStateGet(struct rx_connection *conn,
					  int (*rpc) (struct rx_connection *,
						      afs_RPCStatsState_p),
					  afs_RPCStatsState_p state,
					  afs_status_p st);

extern int ADMINAPI util_RPCStatsStateEnable(struct rx_connection *conn,
					     int (*rpc) (struct rx_connection *),
					     afs_status_p st);

extern int ADMINAPI util_RPCStatsStateDisable(struct rx_connection *conn,
					      int (*rpc) (struct rx_connection *),
					      afs_status_p st);

extern int ADMINAPI util_RPCStatsClear(struct rx_connection *conn,
				       int (*rpc) (struct rx_connection *,
					           afs_RPCStatsClearFlag_t),
				       afs_RPCStatsClearFlag_t flag,
				       afs_status_p st);

extern int ADMINAPI util_RPCStatsVersionGet(struct rx_connection *conn,
					    afs_RPCStatsVersion_p version,
					    afs_status_p st);

typedef struct afs_CMServerPref {
    afs_int32 ipAddr;
    afs_int32 ipRank;
} afs_CMServerPref_t, *afs_CMServerPref_p;

extern int ADMINAPI util_CMGetServerPrefsBegin(struct rx_connection *conn,
					       void **iterationIdP,
					       afs_status_p st);

extern int ADMINAPI util_CMGetServerPrefsNext(const void *iterationId,
					      afs_CMServerPref_p prefs,
					      afs_status_p st);

extern int ADMINAPI util_CMGetServerPrefsDone(const void *iterationId,
					      afs_status_p st);

typedef struct afs_CMListCell {
    char cellname[UTIL_MAX_CELL_NAME_LEN];
    afs_int32 serverAddr[UTIL_MAX_CELL_HOSTS];
} afs_CMListCell_t, *afs_CMListCell_p;

extern int ADMINAPI util_CMListCellsBegin(struct rx_connection *conn,
					  void **iterationIdP,
					  afs_status_p st);

extern int ADMINAPI util_CMListCellsNext(const void *iterationId,
					 afs_CMListCell_p prefs,
					 afs_status_p st);

extern int ADMINAPI util_CMListCellsDone(const void *iterationId,
					 afs_status_p st);

typedef char afs_CMCellName_t[UTIL_MAX_CELL_NAME_LEN], *afs_CMCellName_p;

extern int ADMINAPI util_CMLocalCell(struct rx_connection *conn,
				     afs_CMCellName_p cellName,
				     afs_status_p st);

extern int ADMINAPI util_CMClientConfig(struct rx_connection *conn,
					afs_ClientConfig_p config,
					afs_status_p st);

typedef char rxdebugVersion_t[UTIL_MAX_RXDEBUG_VERSION_LEN],
    *rxdebugVersion_p;

extern int ADMINAPI util_RXDebugVersion(rxdebugHandle_p handle,
					rxdebugVersion_p version,
					afs_status_p st);

extern int ADMINAPI util_RXDebugSupportedStats(rxdebugHandle_p handle,
					       afs_uint32 * supportedStats,
					       afs_status_p st);

extern int ADMINAPI util_RXDebugBasicStats(rxdebugHandle_p handle,
					   struct rx_debugStats *stats,
					   afs_status_p st);

extern int ADMINAPI util_RXDebugRxStats(rxdebugHandle_p handle,
					struct rx_statistics *stats,
					afs_uint32 * supportedStats,
					afs_status_p st);

extern int ADMINAPI util_RXDebugConnectionsBegin(rxdebugHandle_p handle,
						 int allconns,
						 void **iterationIdP,
						 afs_status_p st);

extern int ADMINAPI util_RXDebugConnectionsNext(const void *iterationId,
						struct rx_debugConn *conn,
						afs_uint32 * supportedValues,
						afs_status_p st);

extern int ADMINAPI util_RXDebugConnectionsDone(const void *iterationId,
						afs_status_p st);

extern int ADMINAPI util_RXDebugPeersBegin(rxdebugHandle_p handle,
					   void **iterationIdP,
					   afs_status_p st);

extern int ADMINAPI util_RXDebugPeersNext(const void *iterationId,
					  struct rx_debugPeer *peer,
					  afs_uint32 * supportedValues,
					  afs_status_p st);

extern int ADMINAPI util_RXDebugPeersDone(const void *iterationId,
					  afs_status_p st);

#endif /* OPENAFS_UTIL_ADMIN_H */
