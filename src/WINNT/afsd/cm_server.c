/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <nb30.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "afsd.h"
#include <WINNT\syscfg.h>
#include <WINNT/afsreg.h>
#include <osi.h>
#include <rx/rx.h>
#include <math.h>

osi_rwlock_t cm_serverLock;
osi_rwlock_t cm_syscfgLock;

cm_server_t *cm_allServersp;
afs_uint32   cm_numFileServers = 0;
afs_uint32   cm_numVldbServers = 0;

void
cm_ForceNewConnectionsAllServers(void)
{
    cm_server_t *tsp;

    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        cm_GetServerNoLock(tsp);
        lock_ReleaseRead(&cm_serverLock);
	cm_ForceNewConnections(tsp);
        lock_ObtainRead(&cm_serverLock);
        cm_PutServerNoLock(tsp);
    }
    lock_ReleaseRead(&cm_serverLock);
}

/*
 * lock_ObtainMutex must be held prior to calling
 * this function.
 */
afs_int32
cm_RankServer(cm_server_t * tsp)
{
    afs_int32 code = 0; /* start with "success" */
    struct rx_debugPeer tpeer;
    afs_uint16 port;
    afs_uint16 newRank;

    switch(tsp->type) {
	case CM_SERVER_VLDB:
	    port = htons(7003);
	    break;
	case CM_SERVER_FILE:
	    port = htons(7000);
	    break;
	default:
	    return -1;
    }

    code = rx_GetLocalPeers(tsp->addr.sin_addr.s_addr, port, &tpeer);

    /*check if rx_GetLocalPeers succeeded and if there is data for tsp */
    if(code == 0 && (tpeer.rtt == 0 && tpeer.rtt_dev == 0))
	code = -1;

    if(code == 0) {
	if((tsp->flags & CM_SERVERFLAG_PREF_SET))
	    newRank = tsp->adminRank +
                ((int)(623 * log(tpeer.rtt) / 10) * 10 + 5);
	else /* rank has not been set by admin, derive rank from rtt */
	    newRank = (int)(7200 * log(tpeer.rtt) / 5000) * 5000 + 5000;

	newRank += (rand() & 0x000f); /* randomize */

        if (abs(newRank - tsp->ipRank) > 0xf) {
            tsp->ipRank = newRank;

            lock_ReleaseMutex(&tsp->mx);
            switch (tsp->type) {
            case CM_SERVER_FILE:
                /*
                 * find volumes which might have RO copy
                 * on server and change the ordering of
                 * their RO list
                 */
                cm_ChangeRankVolume(tsp);
                break;
            case CM_SERVER_VLDB:
                /* set preferences for an existing vlserver */
                cm_ChangeRankCellVLServer(tsp);
                break;
            }
            lock_ObtainMutex(&tsp->mx);
        }
    }

    return code;
}

void
cm_PingServer(cm_server_t *tsp)
{
    long code;
    int wasDown = 0;
    cm_conn_t *connp;
    struct rx_connection * rxconnp;
    Capabilities caps = {0, 0};
    char hoststr[16];
    cm_req_t req;

    lock_ObtainMutex(&tsp->mx);
    if (tsp->flags & CM_SERVERFLAG_PINGING) {
	tsp->waitCount++;
	osi_SleepM((LONG_PTR)tsp, &tsp->mx);
	lock_ObtainMutex(&tsp->mx);
	tsp->waitCount--;
	if (tsp->waitCount == 0)
	    _InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_PINGING);
	else
	    osi_Wakeup((LONG_PTR)tsp);
	lock_ReleaseMutex(&tsp->mx);
	return;
    }
    _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PINGING);
    wasDown = tsp->flags & CM_SERVERFLAG_DOWN;
    afs_inet_ntoa_r(tsp->addr.sin_addr.S_un.S_addr, hoststr);
    lock_ReleaseMutex(&tsp->mx);

    code = cm_ConnByServer(tsp, cm_rootUserp, FALSE, &connp);
    if (code == 0) {
	/* now call the appropriate ping call.  Drop the timeout if
	* the server is known to be down, so that we don't waste a
	* lot of time retiming out down servers.
	*/

	osi_Log4(afsd_logp, "cm_PingServer server %s (%s) was %s with caps 0x%x",
		  osi_LogSaveString(afsd_logp, hoststr),
		  tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
		  wasDown ? "down" : "up",
		  tsp->capabilities);

        rxconnp = cm_GetRxConn(connp);
	if (wasDown)
	    rx_SetConnDeadTime(rxconnp, 10);
	if (tsp->type == CM_SERVER_VLDB) {
	    code = VL_ProbeServer(rxconnp);
	}
	else {
	    /* file server */
	    code = RXAFS_GetCapabilities(rxconnp, &caps);
	}
	if (wasDown)
	    rx_SetConnDeadTime(rxconnp, ConnDeadtimeout);
        rx_PutConnection(rxconnp);
	cm_PutConn(connp);
    }	/* got an unauthenticated connection to this server */

    lock_ObtainMutex(&tsp->mx);
    if (code >= 0 || code == RXGEN_OPCODE || code == RX_CALL_BUSY) {
	/* mark server as up */
	_InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_DOWN);
        tsp->downTime = 0;

	/* we currently handle 32-bits of capabilities */
	if (code != RXGEN_OPCODE && code != RX_CALL_BUSY &&
            caps.Capabilities_len > 0) {
	    tsp->capabilities = caps.Capabilities_val[0];
	    xdr_free((xdrproc_t) xdr_Capabilities, &caps);
	    caps.Capabilities_len = 0;
	    caps.Capabilities_val = 0;
	} else {
	    tsp->capabilities = 0;
	}

	osi_Log3(afsd_logp, "cm_PingServer server %s (%s) is up with caps 0x%x",
		  osi_LogSaveString(afsd_logp, hoststr),
		  tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
		  tsp->capabilities);

        /* Now update the volume status if necessary */
        if (wasDown) {
            cm_server_vols_t * tsrvp;
            cm_volume_t * volp;
            int i;

            for (tsrvp = tsp->vols; tsrvp; tsrvp = tsrvp->nextp) {
                for (i=0; i<NUM_SERVER_VOLS; i++) {
                    if (tsrvp->ids[i] != 0) {
                        cm_InitReq(&req);

                        lock_ReleaseMutex(&tsp->mx);
                        code = cm_FindVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
                                                &req, CM_GETVOL_FLAG_NO_LRU_UPDATE, &volp);
                        lock_ObtainMutex(&tsp->mx);
                        if (code == 0) {
                            cm_UpdateVolumeStatus(volp, tsrvp->ids[i]);
                            cm_PutVolume(volp);
                        }
                    }
                }
            }
        }
    } else {
	/* mark server as down */
        if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_DOWN);
            tsp->downTime = time(NULL);
        }
	if (code != VRESTARTING) {
            lock_ReleaseMutex(&tsp->mx);
	    cm_ForceNewConnections(tsp);
            lock_ObtainMutex(&tsp->mx);
        }
	osi_Log3(afsd_logp, "cm_PingServer server %s (%s) is down with caps 0x%x",
		  osi_LogSaveString(afsd_logp, hoststr),
		  tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
		  tsp->capabilities);

        /* Now update the volume status if necessary */
        if (!wasDown) {
            cm_server_vols_t * tsrvp;
            cm_volume_t * volp;
            int i;

            for (tsrvp = tsp->vols; tsrvp; tsrvp = tsrvp->nextp) {
                for (i=0; i<NUM_SERVER_VOLS; i++) {
                    if (tsrvp->ids[i] != 0) {
                        cm_InitReq(&req);

                        lock_ReleaseMutex(&tsp->mx);
                        code = cm_FindVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
                                                &req, CM_GETVOL_FLAG_NO_LRU_UPDATE, &volp);
                        lock_ObtainMutex(&tsp->mx);
                        if (code == 0) {
                            cm_UpdateVolumeStatus(volp, tsrvp->ids[i]);
                            cm_PutVolume(volp);
                        }
                    }
                }
            }
        }
    }

    if (tsp->waitCount == 0)
	_InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_PINGING);
    else
	osi_Wakeup((LONG_PTR)tsp);
    lock_ReleaseMutex(&tsp->mx);
}

void
cm_RankUpServers()
{
    cm_server_t * tsp;

    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
	cm_GetServerNoLock(tsp);
	lock_ReleaseRead(&cm_serverLock);

	lock_ObtainMutex(&tsp->mx);

        /* if the server is not down, rank the server */
        if(!(tsp->flags & CM_SERVERFLAG_DOWN))
	   cm_RankServer(tsp);

	lock_ReleaseMutex(&tsp->mx);

	lock_ObtainRead(&cm_serverLock);
	cm_PutServerNoLock(tsp);
    }
    lock_ReleaseRead(&cm_serverLock);
}

static void cm_CheckServersSingular(afs_uint32 flags, cm_cell_t *cellp)
{
    /* ping all file servers, up or down, with unauthenticated connection,
     * to find out whether we have all our callbacks from the server still.
     * Also, ping down VLDBs.
     */
    cm_server_t *tsp;
    int doPing;
    int isDown;
    int isFS;
    int isVLDB;

    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        cm_GetServerNoLock(tsp);
        lock_ReleaseRead(&cm_serverLock);

        /* now process the server */
        lock_ObtainMutex(&tsp->mx);

        doPing = 0;
        isDown = tsp->flags & CM_SERVERFLAG_DOWN;
        isFS   = tsp->type == CM_SERVER_FILE;
        isVLDB = tsp->type == CM_SERVER_VLDB;

        /* only do the ping if the cell matches the requested cell, or we're
         * matching all cells (cellp == NULL), and if we've requested to ping
         * this type of {up, down} servers.
         */
        if ((cellp == NULL || cellp == tsp->cellp) &&
             ((isDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
               (!isDown && (flags & CM_FLAG_CHECKUPSERVERS))) &&
             ((!(flags & CM_FLAG_CHECKVLDBSERVERS) ||
               isVLDB && (flags & CM_FLAG_CHECKVLDBSERVERS)) &&
              (!(flags & CM_FLAG_CHECKFILESERVERS) ||
                 isFS && (flags & CM_FLAG_CHECKFILESERVERS)))) {
            doPing = 1;
        }	/* we're supposed to check this up/down server */
        lock_ReleaseMutex(&tsp->mx);

        /* at this point, we've adjusted the server state, so do the ping and
         * adjust things.
         */
        if (doPing)
	    cm_PingServer(tsp);

        /* also, run the GC function for connections on all of the
         * server's connections.
         */
        cm_GCConnections(tsp);

        lock_ObtainRead(&cm_serverLock);
        cm_PutServerNoLock(tsp);
    }
    lock_ReleaseRead(&cm_serverLock);
}

static void cm_CheckServersMulti(afs_uint32 flags, cm_cell_t *cellp)
{
    /*
     * The goal of this function is to probe simultaneously
     * probe all of the up/down servers (vldb/file) as
     * specified by flags in the minimum number of RPCs.
     * Effectively that means use one multi_RXAFS_GetCapabilities()
     * followed by possibly one multi_RXAFS_GetTime() and
     * one multi_VL_ProbeServer().
     *
     * To make this work we must construct the list of vldb
     * and file servers that are to be probed as well as the
     * associated data structures.
     */

    int srvAddrCount = 0;
    struct srvAddr **addrs = NULL;
    cm_conn_t **conns = NULL;
    struct rx_connection **rxconns = NULL;
    cm_req_t req;
    afs_int32 i, nconns = 0, maxconns;
    afs_int32 *conntimer, *results;
    Capabilities *caps = NULL;
    cm_server_t ** serversp, *tsp;
    afs_uint32 isDown, wasDown;
    afs_uint32 code;
    time_t start, *deltas;
    char hoststr[16];

    cm_InitReq(&req);
    maxconns = max(cm_numFileServers,cm_numVldbServers);
    if (maxconns == 0)
        return;

    conns = (cm_conn_t **)malloc(maxconns * sizeof(cm_conn_t *));
    rxconns = (struct rx_connection **)malloc(maxconns * sizeof(struct rx_connection *));
    conntimer = (afs_int32 *)malloc(maxconns * sizeof (afs_int32));
    deltas = (time_t *)malloc(maxconns * sizeof (time_t));
    results = (afs_int32 *)malloc(maxconns * sizeof (afs_int32));
    serversp = (cm_server_t **)malloc(maxconns * sizeof(cm_server_t *));
    caps = (Capabilities *)malloc(maxconns * sizeof(Capabilities));

    memset(caps, 0, maxconns * sizeof(Capabilities));

    if ((flags & CM_FLAG_CHECKFILESERVERS) ||
        !(flags & (CM_FLAG_CHECKFILESERVERS|CM_FLAG_CHECKVLDBSERVERS)))
    {
        lock_ObtainRead(&cm_serverLock);
        for (nconns=0, tsp = cm_allServersp; tsp && nconns < maxconns; tsp = tsp->allNextp) {
            if (tsp->type != CM_SERVER_FILE ||
                tsp->cellp == NULL ||           /* SetPref only */
                cellp && cellp != tsp->cellp)
                continue;

            cm_GetServerNoLock(tsp);
            lock_ReleaseRead(&cm_serverLock);

            lock_ObtainMutex(&tsp->mx);
            isDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if ((tsp->flags & CM_SERVERFLAG_PINGING) ||
                !((isDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
                   (!isDown && (flags & CM_FLAG_CHECKUPSERVERS)))) {
                lock_ReleaseMutex(&tsp->mx);
                lock_ObtainRead(&cm_serverLock);
                cm_PutServerNoLock(tsp);
                continue;
            }

            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PINGING);
            lock_ReleaseMutex(&tsp->mx);

            serversp[nconns] = tsp;
            code = cm_ConnByServer(tsp, cm_rootUserp, FALSE, &conns[nconns]);
            if (code) {
                lock_ObtainRead(&cm_serverLock);
                cm_PutServerNoLock(tsp);
                continue;
            }
            lock_ObtainRead(&cm_serverLock);
            rxconns[nconns] = cm_GetRxConn(conns[nconns]);
            if (conntimer[nconns] = (isDown ? 1 : 0))
                rx_SetConnDeadTime(rxconns[nconns], 10);

            nconns++;
        }
        lock_ReleaseRead(&cm_serverLock);

        if (nconns) {
            /* Perform the multi call */
            start = time(NULL);
            multi_Rx(rxconns,nconns)
            {
                multi_RXAFS_GetCapabilities(&caps[multi_i]);
                results[multi_i]=multi_error;
            } multi_End;
        }

        /* Process results of servers that support RXAFS_GetCapabilities */
        for (i=0; i<nconns; i++) {
            if (conntimer[i])
                rx_SetConnDeadTime(rxconns[i], ConnDeadtimeout);
            rx_PutConnection(rxconns[i]);
            cm_PutConn(conns[i]);

            tsp = serversp[i];
            cm_GCConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            wasDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if (results[i] >= 0 || results[i] == RXGEN_OPCODE ||
                results[i] == RX_CALL_BUSY)  {
                /* mark server as up */
                _InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_DOWN);
                tsp->downTime = 0;

                /* we currently handle 32-bits of capabilities */
                if (results[i] != RXGEN_OPCODE && results[i] != RX_CALL_BUSY &&
                    caps[i].Capabilities_len > 0) {
                    tsp->capabilities = caps[i].Capabilities_val[0];
                    xdr_free((xdrproc_t) xdr_Capabilities, &caps[i]);
                    caps[i].Capabilities_len = 0;
                    caps[i].Capabilities_val = 0;
                } else {
                    tsp->capabilities = 0;
                }

                afs_inet_ntoa_r(tsp->addr.sin_addr.S_un.S_addr, hoststr);
                osi_Log3(afsd_logp, "cm_MultiPingServer server %s (%s) is up with caps 0x%x",
                          osi_LogSaveString(afsd_logp, hoststr),
                          tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
                          tsp->capabilities);

                /* Now update the volume status if necessary */
                if (wasDown) {
                    cm_server_vols_t * tsrvp;
                    cm_volume_t * volp;
                    int i;

                    for (tsrvp = tsp->vols; tsrvp; tsrvp = tsrvp->nextp) {
                        for (i=0; i<NUM_SERVER_VOLS; i++) {
                            if (tsrvp->ids[i] != 0) {
                                cm_InitReq(&req);

                                lock_ReleaseMutex(&tsp->mx);
                                code = cm_FindVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
                                                         &req, CM_GETVOL_FLAG_NO_LRU_UPDATE, &volp);
                                lock_ObtainMutex(&tsp->mx);
                                if (code == 0) {
                                    cm_UpdateVolumeStatus(volp, tsrvp->ids[i]);
                                    cm_PutVolume(volp);
                                }
                            }
                        }
                    }
                }
            } else {
                /* mark server as down */
                if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                    _InterlockedOr(&tsp->flags, CM_SERVERFLAG_DOWN);
                    tsp->downTime = time(NULL);
                }
                if (code != VRESTARTING) {
                    lock_ReleaseMutex(&tsp->mx);
                    cm_ForceNewConnections(tsp);
                    lock_ObtainMutex(&tsp->mx);
                }
                afs_inet_ntoa_r(tsp->addr.sin_addr.S_un.S_addr, hoststr);
                osi_Log3(afsd_logp, "cm_MultiPingServer server %s (%s) is down with caps 0x%x",
                          osi_LogSaveString(afsd_logp, hoststr),
                          tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
                          tsp->capabilities);

                /* Now update the volume status if necessary */
                if (!wasDown) {
                    cm_server_vols_t * tsrvp;
                    cm_volume_t * volp;
                    int i;

                    for (tsrvp = tsp->vols; tsrvp; tsrvp = tsrvp->nextp) {
                        for (i=0; i<NUM_SERVER_VOLS; i++) {
                            if (tsrvp->ids[i] != 0) {
                                cm_InitReq(&req);

                                lock_ReleaseMutex(&tsp->mx);
                                code = cm_FindVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
                                                         &req, CM_GETVOL_FLAG_NO_LRU_UPDATE, &volp);
                                lock_ObtainMutex(&tsp->mx);
                                if (code == 0) {
                                    cm_UpdateVolumeStatus(volp, tsrvp->ids[i]);
                                    cm_PutVolume(volp);
                                }
                            }
                        }
                    }
                }
            }

            if (tsp->waitCount == 0)
                _InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_PINGING);
            else
                osi_Wakeup((LONG_PTR)tsp);

            lock_ReleaseMutex(&tsp->mx);

            cm_PutServer(tsp);
        }
    }

    if ((flags & CM_FLAG_CHECKVLDBSERVERS) ||
        !(flags & (CM_FLAG_CHECKFILESERVERS|CM_FLAG_CHECKVLDBSERVERS)))
    {
        lock_ObtainRead(&cm_serverLock);
        for (nconns=0, tsp = cm_allServersp; tsp && nconns < maxconns; tsp = tsp->allNextp) {
            if (tsp->type != CM_SERVER_VLDB ||
                tsp->cellp == NULL ||           /* SetPref only */
                cellp && cellp != tsp->cellp)
                continue;

            cm_GetServerNoLock(tsp);
            lock_ReleaseRead(&cm_serverLock);

            lock_ObtainMutex(&tsp->mx);
            isDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if ((tsp->flags & CM_SERVERFLAG_PINGING) ||
                !((isDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
                   (!isDown && (flags & CM_FLAG_CHECKUPSERVERS)))) {
                lock_ReleaseMutex(&tsp->mx);
                lock_ObtainRead(&cm_serverLock);
                cm_PutServerNoLock(tsp);
                continue;
            }

            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_PINGING);
            lock_ReleaseMutex(&tsp->mx);

            serversp[nconns] = tsp;
            code = cm_ConnByServer(tsp, cm_rootUserp, FALSE, &conns[nconns]);
            if (code) {
                lock_ObtainRead(&cm_serverLock);
                cm_PutServerNoLock(tsp);
                continue;
            }
            lock_ObtainRead(&cm_serverLock);
            rxconns[nconns] = cm_GetRxConn(conns[nconns]);
            conntimer[nconns] = (isDown ? 1 : 0);
            if (isDown)
                rx_SetConnDeadTime(rxconns[nconns], 10);

            nconns++;
        }
        lock_ReleaseRead(&cm_serverLock);

        if (nconns) {
            /* Perform the multi call */
            start = time(NULL);
            multi_Rx(rxconns,nconns)
            {
                multi_VL_ProbeServer();
                results[multi_i]=multi_error;
            } multi_End;
        }

        /* Process results of servers that support VL_ProbeServer */
        for (i=0; i<nconns; i++) {
            if (conntimer[i])
                rx_SetConnDeadTime(rxconns[i], ConnDeadtimeout);
            rx_PutConnection(rxconns[i]);
            cm_PutConn(conns[i]);

            tsp = serversp[i];
            cm_GCConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            wasDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if (results[i] >= 0 || results[i] == RX_CALL_BUSY)  {
                /* mark server as up */
                _InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_DOWN);
                tsp->downTime = 0;
                tsp->capabilities = 0;

                afs_inet_ntoa_r(tsp->addr.sin_addr.S_un.S_addr, hoststr);
                osi_Log3(afsd_logp, "cm_MultiPingServer server %s (%s) is up with caps 0x%x",
                          osi_LogSaveString(afsd_logp, hoststr),
                          tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
                          tsp->capabilities);
            } else {
                /* mark server as down */
                if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                    _InterlockedOr(&tsp->flags, CM_SERVERFLAG_DOWN);
                    tsp->downTime = time(NULL);
                }
                if (code != VRESTARTING) {
                    lock_ReleaseMutex(&tsp->mx);
                    cm_ForceNewConnections(tsp);
                    lock_ObtainMutex(&tsp->mx);
                }
                afs_inet_ntoa_r(tsp->addr.sin_addr.S_un.S_addr, hoststr);
                osi_Log3(afsd_logp, "cm_MultiPingServer server %s (%s) is down with caps 0x%x",
                          osi_LogSaveString(afsd_logp, hoststr),
                          tsp->type == CM_SERVER_VLDB ? "vldb" : "file",
                          tsp->capabilities);
            }

            if (tsp->waitCount == 0)
                _InterlockedAnd(&tsp->flags, ~CM_SERVERFLAG_PINGING);
            else
                osi_Wakeup((LONG_PTR)tsp);

            lock_ReleaseMutex(&tsp->mx);

            cm_PutServer(tsp);
        }
    }

    free(conns);
    free(rxconns);
    free(conntimer);
    free(deltas);
    free(results);
    free(serversp);
    free(caps);
}

void cm_CheckServers(afs_uint32 flags, cm_cell_t *cellp)
{
    DWORD code;
    HKEY parmKey;
    DWORD dummyLen;
    DWORD multi = 1;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code == ERROR_SUCCESS) {
        dummyLen = sizeof(multi);
        code = RegQueryValueEx(parmKey, "MultiCheckServers", NULL, NULL,
                                (BYTE *) &multi, &dummyLen);
        RegCloseKey (parmKey);
    }

    if (multi)
        cm_CheckServersMulti(flags, cellp);
    else
        cm_CheckServersSingular(flags, cellp);
}

void cm_InitServer(void)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_serverLock, "cm_serverLock", LOCK_HIERARCHY_SERVER_GLOBAL);
        lock_InitializeRWLock(&cm_syscfgLock, "cm_syscfgLock", LOCK_HIERARCHY_SYSCFG_GLOBAL);
        osi_EndOnce(&once);
    }
}

/* Protected by cm_syscfgLock (rw) */
int cm_noIPAddr;         /* number of client network interfaces */
int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */
int cm_LanAdapterChangeDetected = 1;

void cm_SetLanAdapterChangeDetected(void)
{
    lock_ObtainWrite(&cm_syscfgLock);
    cm_LanAdapterChangeDetected = 1;
    lock_ReleaseWrite(&cm_syscfgLock);
}

void cm_GetServer(cm_server_t *serverp)
{
    lock_ObtainRead(&cm_serverLock);
    InterlockedIncrement(&serverp->refCount);
    lock_ReleaseRead(&cm_serverLock);
}

void cm_GetServerNoLock(cm_server_t *serverp)
{
    InterlockedIncrement(&serverp->refCount);
}

void cm_PutServer(cm_server_t *serverp)
{
    afs_int32 refCount;
    lock_ObtainRead(&cm_serverLock);
    refCount = InterlockedDecrement(&serverp->refCount);
    osi_assertx(refCount >= 0, "cm_server_t refCount underflow");
    lock_ReleaseRead(&cm_serverLock);
}

void cm_PutServerNoLock(cm_server_t *serverp)
{
    afs_int32 refCount = InterlockedDecrement(&serverp->refCount);
    osi_assertx(refCount >= 0, "cm_server_t refCount underflow");
}

void cm_SetServerNo64Bit(cm_server_t * serverp, int no64bit)
{
    lock_ObtainMutex(&serverp->mx);
    if (no64bit)
        _InterlockedOr(&serverp->flags, CM_SERVERFLAG_NO64BIT);
    else
        _InterlockedAnd(&serverp->flags, ~CM_SERVERFLAG_NO64BIT);
    lock_ReleaseMutex(&serverp->mx);
}

void cm_SetServerNoInlineBulk(cm_server_t * serverp, int no)
{
    lock_ObtainMutex(&serverp->mx);
    if (no)
        _InterlockedOr(&serverp->flags, CM_SERVERFLAG_NOINLINEBULK);
    else
        _InterlockedAnd(&serverp->flags, ~CM_SERVERFLAG_NOINLINEBULK);
    lock_ReleaseMutex(&serverp->mx);
}

void cm_SetServerPrefs(cm_server_t * serverp)
{
    unsigned long	serverAddr; 	/* in host byte order */
    unsigned long	myAddr, myNet, mySubnet;/* in host byte order */
    unsigned long	netMask;
    int 		i;
    long code;
    int writeLock = 0;

    lock_ObtainRead(&cm_syscfgLock);
    if (cm_LanAdapterChangeDetected) {
        lock_ConvertRToW(&cm_syscfgLock);
        writeLock = 1;
        if (cm_LanAdapterChangeDetected) {
            /* get network related info */
            cm_noIPAddr = CM_MAXINTERFACE_ADDR;
            code = syscfg_GetIFInfo(&cm_noIPAddr,
                                     cm_IPAddr, cm_SubnetMask,
                                     cm_NetMtu, cm_NetFlags);
            cm_LanAdapterChangeDetected = 0;
        }
        lock_ConvertWToR(&cm_syscfgLock);
    }

    serverAddr = ntohl(serverp->addr.sin_addr.s_addr);
    serverp->ipRank  = CM_IPRANK_LOW;	/* default settings */

    for ( i=0; i < cm_noIPAddr; i++)
    {
	/* loop through all the client's IP address and compare
	** each of them against the server's IP address */

	myAddr = cm_IPAddr[i];
	if ( IN_CLASSA(myAddr) )
	    netMask = IN_CLASSA_NET;
	else if ( IN_CLASSB(myAddr) )
	    netMask = IN_CLASSB_NET;
	else if ( IN_CLASSC(myAddr) )
	    netMask = IN_CLASSC_NET;
	else
	    netMask = 0;

	myNet    =  myAddr & netMask;
	mySubnet =  myAddr & cm_SubnetMask[i];

	if ( (serverAddr & netMask) == myNet )
	{
	    if ( (serverAddr & cm_SubnetMask[i]) == mySubnet)
	    {
		if ( serverAddr == myAddr )
		    serverp->ipRank = min(serverp->ipRank,
					   CM_IPRANK_TOP);/* same machine */
		else serverp->ipRank = min(serverp->ipRank,
					    CM_IPRANK_HI); /* same subnet */
	    }
	    else serverp->ipRank = min(serverp->ipRank,CM_IPRANK_MED);
	    /* same net */
	}
    } /* and of for loop */

    /* random between 0..15*/
    serverp->ipRank += (rand() % 0x000f);
    lock_ReleaseRead(&cm_syscfgLock);
}

cm_server_t *cm_NewServer(struct sockaddr_in *socketp, int type, cm_cell_t *cellp, afsUUID *uuidp, afs_uint32 flags) {
    cm_server_t *tsp;

    osi_assertx(socketp->sin_family == AF_INET, "unexpected socket family");

    lock_ObtainWrite(&cm_serverLock); 	/* get server lock */
    tsp = cm_FindServer(socketp, type, TRUE);
    if (tsp) {
        /* we might have found a server created by set server prefs */
        if (uuidp && !afs_uuid_is_nil(uuidp) &&
            !(tsp->flags & CM_SERVERFLAG_UUID))
        {
            tsp->uuid = *uuidp;
            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_UUID);
        }
        lock_ReleaseWrite(&cm_serverLock);
        return tsp;
    }

    tsp = malloc(sizeof(*tsp));
    if (tsp) {
        memset(tsp, 0, sizeof(*tsp));
        tsp->type = type;
        tsp->cellp = cellp;
        if (uuidp && !afs_uuid_is_nil(uuidp)) {
            tsp->uuid = *uuidp;
            _InterlockedOr(&tsp->flags, CM_SERVERFLAG_UUID);
        }
        tsp->refCount = 1;
        lock_InitializeMutex(&tsp->mx, "cm_server_t mutex", LOCK_HIERARCHY_SERVER);
        tsp->addr = *socketp;

        cm_SetServerPrefs(tsp);

        tsp->allNextp = cm_allServersp;
        cm_allServersp = tsp;

        switch (type) {
        case CM_SERVER_VLDB:
            cm_numVldbServers++;
            break;
        case CM_SERVER_FILE:
            cm_numFileServers++;
            break;
        }
    }
    lock_ReleaseWrite(&cm_serverLock); 	/* release server lock */

    if (!(flags & CM_FLAG_NOPROBE) && tsp) {
        _InterlockedOr(&tsp->flags, CM_SERVERFLAG_DOWN);	/* assume down; ping will mark up if available */
        cm_PingServer(tsp);	                                /* Obtain Capabilities and check up/down state */
    }

    return tsp;
}

cm_server_t *
cm_FindServerByIP(afs_uint32 ipaddr, unsigned short port, int type, int locked)
{
    cm_server_t *tsp;

    if (!locked)
        lock_ObtainRead(&cm_serverLock);

    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        if (tsp->type == type &&
            tsp->addr.sin_addr.S_un.S_addr == ipaddr &&
            (tsp->addr.sin_port == port || tsp->addr.sin_port == 0))
            break;
    }

    /* bump ref count if we found the server */
    if (tsp)
        cm_GetServerNoLock(tsp);

    if (!locked)
        lock_ReleaseRead(&cm_serverLock);

    return tsp;
}

cm_server_t *
cm_FindServerByUuid(afsUUID *serverUuid, int type, int locked)
{
    cm_server_t *tsp;

    if (!locked)
        lock_ObtainRead(&cm_serverLock);

    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        if (tsp->type == type && !afs_uuid_equal(&tsp->uuid, serverUuid))
            break;
    }

    /* bump ref count if we found the server */
    if (tsp)
        cm_GetServerNoLock(tsp);

    if (!locked)
        lock_ReleaseRead(&cm_serverLock);

    return tsp;
}

/* find a server based on its properties */
cm_server_t *cm_FindServer(struct sockaddr_in *addrp, int type, int locked)
{
    osi_assertx(addrp->sin_family == AF_INET, "unexpected socket value");

    return cm_FindServerByIP(addrp->sin_addr.s_addr, addrp->sin_port, type, locked);
}

cm_server_vols_t *cm_NewServerVols(void) {
    cm_server_vols_t *tsvp;

    tsvp = malloc(sizeof(*tsvp));
    if (tsvp)
        memset(tsvp, 0, sizeof(*tsvp));

    return tsvp;
}

/*
 * cm_NewServerRef() returns with the allocated cm_serverRef_t
 * with a refCount of 1.
 */
cm_serverRef_t *cm_NewServerRef(cm_server_t *serverp, afs_uint32 volID)
{
    cm_serverRef_t *tsrp;
    cm_server_vols_t **tsrvpp = NULL;
    afs_uint32 *slotp = NULL;
    int found = 0;

    cm_GetServer(serverp);
    tsrp = malloc(sizeof(*tsrp));
    tsrp->server = serverp;
    tsrp->status = srv_not_busy;
    tsrp->next = NULL;
    tsrp->volID = volID;
    tsrp->refCount = 1;

    /* if we have a non-zero volID, we need to add it to the list
     * of volumes maintained by the server.  There are two phases:
     * (1) see if the volID is already in the list and (2) insert
     * it into the first empty slot if it is not.
     */
    if (volID) {
        lock_ObtainMutex(&serverp->mx);

        tsrvpp = &serverp->vols;
        while (*tsrvpp) {
            int i;

            for (i=0; i<NUM_SERVER_VOLS; i++) {
                if ((*tsrvpp)->ids[i] == volID) {
                    found = 1;
                    break;
                } else if (!slotp && (*tsrvpp)->ids[i] == 0) {
                    slotp = &(*tsrvpp)->ids[i];
                }
            }

            if (found)
                break;

            tsrvpp = &(*tsrvpp)->nextp;
        }

        if (!found) {
            if (slotp) {
                *slotp = volID;
            } else {
                /* if we didn't find an empty slot in a current
                 * page we must need a new page */
                *tsrvpp = cm_NewServerVols();
                if (*tsrvpp)
                    (*tsrvpp)->ids[0] = volID;
            }
        }

        lock_ReleaseMutex(&serverp->mx);
    }

    return tsrp;
}

void cm_GetServerRef(cm_serverRef_t *tsrp, int locked)
{
    afs_int32 refCount;

    if (!locked)
        lock_ObtainRead(&cm_serverLock);
    refCount = InterlockedIncrement(&tsrp->refCount);
    if (!locked)
        lock_ReleaseRead(&cm_serverLock);
}

afs_int32 cm_PutServerRef(cm_serverRef_t *tsrp, int locked)
{
    afs_int32 refCount;

    if (!locked)
        lock_ObtainRead(&cm_serverLock);
    refCount = InterlockedDecrement(&tsrp->refCount);
    osi_assertx(refCount >= 0, "cm_serverRef_t refCount underflow");

    if (!locked)
        lock_ReleaseRead(&cm_serverLock);

    return refCount;
}



LONG_PTR cm_ChecksumServerList(cm_serverRef_t *serversp)
{
    LONG_PTR sum = 0;
    int first = 1;
    cm_serverRef_t *tsrp;

    lock_ObtainRead(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (tsrp->status == srv_deleted)
            continue;
        if (first)
            first = 0;
        else
            sum <<= 1;
        sum ^= (LONG_PTR) tsrp->server;
    }

    lock_ReleaseRead(&cm_serverLock);
    return sum;
}

/*
** Insert a server into the server list keeping the list sorted in
** ascending order of ipRank.
**
** The refCount of the cm_serverRef_t is not altered.
*/
void cm_InsertServerList(cm_serverRef_t** list, cm_serverRef_t* element)
{
    cm_serverRef_t	*current;
    unsigned short ipRank;

    lock_ObtainWrite(&cm_serverLock);
    /*
     * Since we are grabbing the serverLock exclusively remove any
     * deleted serverRef objects with a zero refcount before
     * inserting the new item.
     */
    if (*list) {
        cm_serverRef_t  **currentp = list;
        cm_serverRef_t  **nextp = NULL;
        cm_serverRef_t  * next = NULL;

        for (currentp = list; *currentp; currentp = nextp)
        {
            nextp = &(*currentp)->next;
            if ((*currentp)->refCount == 0 &&
                (*currentp)->status == srv_deleted) {
                next = *nextp;

                if ((*currentp)->volID)
                    cm_RemoveVolumeFromServer((*currentp)->server, (*currentp)->volID);
                cm_FreeServer((*currentp)->server);
                free(*currentp);
                nextp = &next;
            }
        }
    }

    /* insertion into empty list  or at the beginning of the list */
    if (!(*list))
    {
        element->next = NULL;
        *list = element;
        goto done;
    }

    /*
     * Now that deleted entries have been removed and we know that the
     * list was not empty, look for duplicates.  If the element we are
     * inserting already exists, discard it.
     */
    for ( current = *list; current; current = current->next)
    {
        cm_server_t * server1 = current->server;
        cm_server_t * server2 = element->server;

        if (current->status == srv_deleted)
            continue;

        if (server1->type != server2->type)
            continue;

        if (server1->addr.sin_addr.s_addr != server2->addr.sin_addr.s_addr)
            continue;

        if ((server1->flags & CM_SERVERFLAG_UUID) != (server2->flags & CM_SERVERFLAG_UUID))
            continue;

        if ((server1->flags & CM_SERVERFLAG_UUID) &&
            !afs_uuid_equal(&server1->uuid, &server2->uuid))
            continue;

        /* we must have a match, discard the new element */
        free(element);
        goto done;
    }

    ipRank = element->server->ipRank;

	/* insertion at the beginning of the list */
    if ((*list)->server->ipRank > ipRank)
    {
        element->next = *list;
        *list = element;
        goto done;
    }

    /* find appropriate place to insert */
    for ( current = *list; current->next; current = current->next)
    {
        if ( current->next->server->ipRank > ipRank )
            break;
    }
    element->next = current->next;
    current->next = element;

  done:
    lock_ReleaseWrite(&cm_serverLock);
}
/*
** Re-sort the server list with the modified rank
** returns 0 if element was changed successfully.
** returns 1 if  list remained unchanged.
*/
long cm_ChangeRankServer(cm_serverRef_t** list, cm_server_t*	server)
{
    cm_serverRef_t  **current;
    cm_serverRef_t   *element;

    lock_ObtainWrite(&cm_serverLock);
    current=list;
    element=0;

    /* if there is max of one element in the list, nothing to sort */
    if ( (!*current) || !((*current)->next)  ) {
        lock_ReleaseWrite(&cm_serverLock);
        return 1;		/* list unchanged: return success */
    }

    /* if the server is on the list, delete it from list */
    while ( *current )
    {
        if ( (*current)->server == server)
        {
            element = (*current);
            *current = element->next; /* delete it */
            break;
        }
        current = & ( (*current)->next);
    }
    lock_ReleaseWrite(&cm_serverLock);

    /* if this volume is not replicated on this server  */
    if (!element)
        return 1;	/* server is not on list */

    /* re-insert deleted element into the list with modified rank*/
    cm_InsertServerList(list, element);

    return 0;
}
/*
** If there are more than one server on the list and the first n servers on
** the list have the same rank( n>1), then randomise among the first n servers.
*/
void cm_RandomizeServer(cm_serverRef_t** list)
{
    int 		count, picked;
    cm_serverRef_t*	tsrp, *lastTsrp;
    unsigned short	lowestRank;

    lock_ObtainWrite(&cm_serverLock);
    tsrp = *list;

    /* an empty list or a list with only one element */
    if ( !tsrp || ! tsrp->next ) {
        lock_ReleaseWrite(&cm_serverLock);
        return ;
    }

    /* count the number of servers with the lowest rank */
    lowestRank = tsrp->server->ipRank;
    for ( count=1, tsrp=tsrp->next; tsrp; tsrp=tsrp->next)
    {
        if ( tsrp->server->ipRank != lowestRank)
            break;
        else
            count++;
    }

    /* if there is only one server with the lowest rank, we are done */
    if ( count <= 1 ) {
        lock_ReleaseWrite(&cm_serverLock);
        return ;
    }

    picked = rand() % count;
    if ( !picked ) {
        lock_ReleaseWrite(&cm_serverLock);
        return ;
    }

    tsrp = *list;
    while (--picked >= 0)
    {
        lastTsrp = tsrp;
        tsrp = tsrp->next;
    }
    lastTsrp->next = tsrp->next;  /* delete random element from list*/
    tsrp->next     = *list; /* insert element at the beginning of list */
    *list          = tsrp;
    lock_ReleaseWrite(&cm_serverLock);
}

/* call cm_FreeServer while holding a write lock on cm_serverLock */
void cm_FreeServer(cm_server_t* serverp)
{
    cm_server_vols_t * tsrvp, *nextp;
    int delserver = 0;

    cm_PutServerNoLock(serverp);
    if (serverp->refCount == 0)
    {
        /*
         * we need to check to ensure that all of the connections
         * for this server have a 0 refCount; otherwise, they will
         * not be garbage collected
         *
         * must drop the cm_serverLock because cm_GCConnections
         * obtains the cm_connLock and that comes first in the
         * lock hierarchy.
         */
        lock_ReleaseWrite(&cm_serverLock);
        cm_GCConnections(serverp);  /* connsp */
        lock_ObtainWrite(&cm_serverLock);
    }


    /*
     * Once we have the cm_serverLock locked check to make
     * sure the refCount is still zero before removing the
     * server entirely.
     */
    if (serverp->refCount == 0) {
	if (!(serverp->flags & CM_SERVERFLAG_PREF_SET)) {
            switch (serverp->type) {
            case CM_SERVER_VLDB:
                cm_numVldbServers--;
                break;
            case CM_SERVER_FILE:
                cm_numFileServers--;
                break;
            }

	    lock_FinalizeMutex(&serverp->mx);
	    if ( cm_allServersp == serverp )
		cm_allServersp = serverp->allNextp;
	    else {
		cm_server_t *tsp;

		for(tsp = cm_allServersp; tsp->allNextp; tsp=tsp->allNextp) {
		    if ( tsp->allNextp == serverp ) {
			tsp->allNextp = serverp->allNextp;
			break;
		    }
		}
            }

            /* free the volid list */
            for ( tsrvp = serverp->vols; tsrvp; tsrvp = nextp) {
                nextp = tsrvp->nextp;
                free(tsrvp);
            }

	    free(serverp);
        }
    }
}

/* Called with cm_serverLock write locked */
void cm_RemoveVolumeFromServer(cm_server_t * serverp, afs_uint32 volID)
{
    cm_server_vols_t * tsrvp;
    int i;

    if (volID == 0)
        return;

    for (tsrvp = serverp->vols; tsrvp; tsrvp = tsrvp->nextp) {
        for (i=0; i<NUM_SERVER_VOLS; i++) {
            if (tsrvp->ids[i] == volID) {
                tsrvp->ids[i] = 0;;
                break;
            }
        }
    }
}

void cm_FreeServerList(cm_serverRef_t** list, afs_uint32 flags)
{
    cm_serverRef_t  **current;
    cm_serverRef_t  **nextp;
    cm_serverRef_t  * next;
    afs_int32         refCount;

    lock_ObtainWrite(&cm_serverLock);
    current = list;
    nextp = 0;
    next = 0;

    if (*list == NULL)
        goto done;

    while (*current)
    {
        nextp = &(*current)->next;
        refCount = cm_PutServerRef(*current, TRUE);
        if (refCount == 0) {
            next = *nextp;

            if ((*current)->volID)
                cm_RemoveVolumeFromServer((*current)->server, (*current)->volID);
            cm_FreeServer((*current)->server);
            free(*current);
            *current = next;
        } else {
            if (flags & CM_FREESERVERLIST_DELETE) {
                (*current)->status = srv_deleted;
                if ((*current)->volID)
                    cm_RemoveVolumeFromServer((*current)->server, (*current)->volID);
            }
            current = nextp;
        }
    }

  done:

    lock_ReleaseWrite(&cm_serverLock);
}

/* dump all servers to a file.
 * cookie is used to identify this batch for easy parsing,
 * and it a string provided by a caller
 */
int cm_DumpServers(FILE *outputFile, char *cookie, int lock)
{
    int zilch;
    cm_server_t *tsp;
    char output[1024];
    char uuidstr[128];
    char hoststr[16];

    if (lock)
        lock_ObtainRead(&cm_serverLock);

    sprintf(output,
            "%s - dumping servers - cm_numFileServers=%d, cm_numVldbServers=%d\r\n",
            cookie, cm_numFileServers, cm_numVldbServers);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    for (tsp = cm_allServersp; tsp; tsp=tsp->allNextp)
    {
        char * type;
        char * down;

        switch (tsp->type) {
        case CM_SERVER_VLDB:
            type = "vldb";
            break;
        case CM_SERVER_FILE:
            type = "file";
            break;
        default:
            type = "unknown";
        }

        afsUUID_to_string(&tsp->uuid, uuidstr, sizeof(uuidstr));
        afs_inet_ntoa_r(tsp->addr.sin_addr.s_addr, hoststr);
        down = ctime(&tsp->downTime);
        down[strlen(down)-1] = '\0';

        sprintf(output,
                 "%s - tsp=0x%p cell=%s addr=%-15s port=%u uuid=%s type=%s caps=0x%x "
                 "flags=0x%x waitCount=%u rank=%u downTime=\"%s\" refCount=%u\r\n",
                 cookie, tsp, tsp->cellp ? tsp->cellp->name : "", hoststr,
                 ntohs(tsp->addr.sin_port), uuidstr, type,
                 tsp->capabilities, tsp->flags, tsp->waitCount, tsp->ipRank,
                 (tsp->flags & CM_SERVERFLAG_DOWN) ?  down : "up",
                 tsp->refCount);
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    }
    sprintf(output, "%s - Done dumping servers.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    if (lock)
	lock_ReleaseRead(&cm_serverLock);

    return (0);
}

/*
 * Determine if two servers are in fact the same.
 *
 * Returns 1 if they match, 0 if they do not
 */
int cm_ServerEqual(cm_server_t *srv1, cm_server_t *srv2)
{
    RPC_STATUS status;

    if (srv1 == NULL || srv2 == NULL)
        return 0;

    if (srv1 == srv2)
        return 1;

    if (srv1->flags & CM_SERVERFLAG_UUID) {
        if (!(srv2->flags & CM_SERVERFLAG_UUID))
            return 0;

        /* Both support UUID */
        if (UuidEqual((UUID *)&srv1->uuid, (UUID *)&srv2->uuid, &status))
            return 1;
    } else {
        if (srv1->flags & CM_SERVERFLAG_UUID)
            return 0;

        /* Neither support UUID so perform an addr/port comparison */
        if ( srv1->addr.sin_family == srv2->addr.sin_family &&
             srv1->addr.sin_addr.s_addr == srv2->addr.sin_addr.s_addr &&
             srv1->addr.sin_port == srv2->addr.sin_port )
            return 1;
    }

    return 0;
}

