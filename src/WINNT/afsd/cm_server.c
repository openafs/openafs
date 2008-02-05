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
#include <osi.h>
#include <rx/rx.h>

osi_rwlock_t cm_serverLock;

cm_server_t *cm_allServersp;
afs_uint32   cm_numFileServers = 0;
afs_uint32   cm_numVldbServers = 0;

void
cm_ForceNewConnectionsAllServers(void)
{
    cm_server_t *tsp;

    lock_ObtainWrite(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        cm_GetServerNoLock(tsp);
	cm_ForceNewConnections(tsp);
        cm_PutServerNoLock(tsp);
    }
    lock_ReleaseWrite(&cm_serverLock);
}

void 
cm_PingServer(cm_server_t *tsp)
{
    long code;
    int wasDown = 0;
    cm_conn_t *connp;
    struct rx_connection * rxconnp;
    long secs;
    long usecs;
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
	    tsp->flags &= ~CM_SERVERFLAG_PINGING;
	else 
	    osi_Wakeup((LONG_PTR)tsp);
	lock_ReleaseMutex(&tsp->mx);
	return;
    }
    tsp->flags |= CM_SERVERFLAG_PINGING;
    wasDown = tsp->flags & CM_SERVERFLAG_DOWN;
    afs_inet_ntoa_r(tsp->addr.sin_addr.S_un.S_addr, hoststr);
    lock_ReleaseMutex(&tsp->mx);

    code = cm_ConnByServer(tsp, cm_rootUserp, &connp);
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
	    if (code == RXGEN_OPCODE)
		code = RXAFS_GetTime(rxconnp, &secs, &usecs);
	}
	if (wasDown)
	    rx_SetConnDeadTime(rxconnp, ConnDeadtimeout);
        rx_PutConnection(rxconnp);
	cm_PutConn(connp);
    }	/* got an unauthenticated connection to this server */

    lock_ObtainMutex(&tsp->mx);
    if (code >= 0) {
	/* mark server as up */
	tsp->flags &= ~CM_SERVERFLAG_DOWN;
        tsp->downTime = 0;

	/* we currently handle 32-bits of capabilities */
	if (caps.Capabilities_len > 0) {
	    tsp->capabilities = caps.Capabilities_val[0];
	    free(caps.Capabilities_val);
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
                        code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
            tsp->flags |= CM_SERVERFLAG_DOWN;
            tsp->downTime = time(NULL);
        }
	if (code != VRESTARTING)
	    cm_ForceNewConnections(tsp);

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
                        code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
	tsp->flags &= ~CM_SERVERFLAG_PINGING;
    else 
	osi_Wakeup((LONG_PTR)tsp);
    lock_ReleaseMutex(&tsp->mx);
}

#define MULTI_CHECKSERVERS 1
#ifndef MULTI_CHECKSERVERS
void cm_CheckServers(afs_uint32 flags, cm_cell_t *cellp)
{
    /* ping all file servers, up or down, with unauthenticated connection,
     * to find out whether we have all our callbacks from the server still.
     * Also, ping down VLDBs.
     */
    cm_server_t *tsp;
    int doPing;
    int isDown;
    int isFS;

    lock_ObtainWrite(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        cm_GetServerNoLock(tsp);
        lock_ReleaseWrite(&cm_serverLock);

        /* now process the server */
        lock_ObtainMutex(&tsp->mx);

        doPing = 0;
        isDown = tsp->flags & CM_SERVERFLAG_DOWN;
        isFS   = tsp->type == CM_SERVER_FILE;

        /* only do the ping if the cell matches the requested cell, or we're
         * matching all cells (cellp == NULL), and if we've requested to ping
         * this type of {up, down} servers.
         */
        if ((cellp == NULL || cellp == tsp->cellp) &&
             ((isDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
               (!isDown && (flags & CM_FLAG_CHECKUPSERVERS))) &&
             ((!(flags & CM_FLAG_CHECKVLDBSERVERS) || 
               !isFS && (flags & CM_FLAG_CHECKVLDBSERVERS)) &&
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

        lock_ObtainWrite(&cm_serverLock);
        cm_PutServerNoLock(tsp);
    }
    lock_ReleaseWrite(&cm_serverLock);
}       
#else /* MULTI_CHECKSERVERS */
void cm_CheckServers(afs_uint32 flags, cm_cell_t *cellp)
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
    int nconns = 0;
    struct rx_connection **rxconns = NULL;
    cm_req_t req;
    afs_uint32 i, j;
    afs_int32 *conntimer, *results;
    Capabilities *caps = NULL;
    cm_server_t ** serversp, *tsp;
    afs_uint32 isDown, wasDown;
    afs_uint32 code;
    time_t start, end, *deltas;
    afs_int32 secs;
    afs_int32 usecs;
    char hoststr[16];

    cm_InitReq(&req);

    j = max(cm_numFileServers,cm_numVldbServers);
    conns = (cm_conn_t **)malloc(j * sizeof(cm_conn_t *));
    rxconns = (struct rx_connection **)malloc(j * sizeof(struct rx_connection *));
    conntimer = (afs_int32 *)malloc(j * sizeof (afs_int32));
    deltas = (time_t *)malloc(j * sizeof (time_t));
    results = (afs_int32 *)malloc(j * sizeof (afs_int32));
    serversp = (cm_server_t **)malloc(j * sizeof(cm_server_t *));
    caps = (Capabilities *)malloc(j * sizeof(Capabilities));

    memset(caps, 0, j * sizeof(Capabilities));

    if (!(flags & CM_FLAG_CHECKVLDBSERVERS)) {
        lock_ObtainWrite(&cm_serverLock);
        nconns = 0;
        for (nconns=0, tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
            if (tsp->type != CM_SERVER_FILE || 
                tsp->cellp == NULL ||           /* SetPref only */
                cellp && cellp != tsp->cellp)
                continue;

            cm_GetServerNoLock(tsp);
            lock_ReleaseWrite(&cm_serverLock);

            lock_ObtainMutex(&tsp->mx);
            isDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if ((tsp->flags & CM_SERVERFLAG_PINGING) ||
                !((isDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
                   (!isDown && (flags & CM_FLAG_CHECKUPSERVERS)))) {
                lock_ReleaseMutex(&tsp->mx);
                lock_ObtainWrite(&cm_serverLock);
                continue;
            }

            tsp->flags |= CM_SERVERFLAG_PINGING;
            lock_ReleaseMutex(&tsp->mx);

            serversp[nconns] = tsp;
            code = cm_ConnByServer(tsp, cm_rootUserp, &conns[nconns]);
            if (code) {
	            lock_ObtainWrite(&cm_serverLock);
                cm_PutServerNoLock(tsp);
                continue;
            }
            lock_ObtainWrite(&cm_serverLock);
			rxconns[nconns] = cm_GetRxConn(conns[nconns]);
            if (conntimer[nconns] = (isDown ? 1 : 0))
                rx_SetConnDeadTime(rxconns[nconns], 10);

            nconns++;
        }
        lock_ReleaseWrite(&cm_serverLock);

        /* Perform the multi call */
        start = time(NULL);
        multi_Rx(rxconns,nconns)
        {
            multi_RXAFS_GetCapabilities(&caps[multi_i]);
            results[multi_i]=multi_error;
        } multi_End;


        /* Process results of servers that support RXAFS_GetCapabilities */
        for (i=0; i<nconns; i++) {
            /* Leave the servers that did not support GetCapabilities alone */
            if (results[i] == RXGEN_OPCODE)
                continue;

            if (conntimer[i])
                rx_SetConnDeadTime(rxconns[i], ConnDeadtimeout);
            rx_PutConnection(rxconns[i]);
            cm_PutConn(conns[i]);

            tsp = serversp[i];
            cm_GCConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            wasDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if (results[i] >= 0)  {
                /* mark server as up */
                tsp->flags &= ~CM_SERVERFLAG_DOWN;
                tsp->downTime = 0;

                /* we currently handle 32-bits of capabilities */
                if (caps[i].Capabilities_len > 0) {
                    tsp->capabilities = caps[i].Capabilities_val[0];
                    free(caps[i].Capabilities_val);
                    caps[i].Capabilities_len = 0;
                    caps[i].Capabilities_val = 0;
                } else {
                    tsp->capabilities = 0;
                }

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
                                code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
                    tsp->flags |= CM_SERVERFLAG_DOWN;
                    tsp->downTime = time(NULL);
                }
                if (code != VRESTARTING)
                    cm_ForceNewConnections(tsp);

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
                                code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
                tsp->flags &= ~CM_SERVERFLAG_PINGING;
            else 
                osi_Wakeup((LONG_PTR)tsp);
            
            lock_ReleaseMutex(&tsp->mx);

            cm_PutServer(tsp);
        }

        /* 
         * At this point we have handled any responses that did not indicate
         * that RXAFS_GetCapabilities is not supported.
         */
        for ( i=0, j=0; i<nconns; i++) {
            if (results[i] == RXGEN_OPCODE && i != j) {
                conns[j] = conns[i];
                rxconns[j] = rxconns[i];
                serversp[j] = serversp[i];
                j++;
            }
        }
        nconns = j;

        /* Perform the multi call */
        start = time(NULL);
        multi_Rx(rxconns,nconns)
        {
            secs = usecs = 0;
            multi_RXAFS_GetTime(&secs, &usecs);
            end = time(NULL);
            results[multi_i]=multi_error;
            if ((start == end) && !multi_error)
                deltas[multi_i] = end - secs;
        } multi_End;


        /* Process Results of servers that only support RXAFS_GetTime */
        for (i=0; i<nconns; i++) {
            /* Leave the servers that did not support GetCapabilities alone */
            if (conntimer[i])
                rx_SetConnDeadTime(rxconns[i], ConnDeadtimeout);
            rx_PutConnection(rxconns[i]);
            cm_PutConn(conns[i]);

            tsp = serversp[i];
            cm_GCConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            wasDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if (results[i] >= 0)  {
                /* mark server as up */
                tsp->flags &= ~CM_SERVERFLAG_DOWN;
                tsp->downTime = 0;
                tsp->capabilities = 0;

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
                                code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
                    tsp->flags |= CM_SERVERFLAG_DOWN;
                    tsp->downTime = time(NULL);
                }
                if (code != VRESTARTING)
                    cm_ForceNewConnections(tsp);

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
                                code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
                tsp->flags &= ~CM_SERVERFLAG_PINGING;
            else 
                osi_Wakeup((LONG_PTR)tsp);
            
            lock_ReleaseMutex(&tsp->mx);

            cm_PutServer(tsp);
        }
    }

    if (!(flags & CM_FLAG_CHECKFILESERVERS)) {
        lock_ObtainWrite(&cm_serverLock);
        nconns = 0;
        for (nconns=0, tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
            if (tsp->type != CM_SERVER_VLDB ||
                tsp->cellp == NULL ||           /* SetPref only */
                cellp && cellp != tsp->cellp)
                continue;

            cm_GetServerNoLock(tsp);
            lock_ReleaseWrite(&cm_serverLock);

            lock_ObtainMutex(&tsp->mx);
            isDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if ((tsp->flags & CM_SERVERFLAG_PINGING) ||
                !((isDown && (flags & CM_FLAG_CHECKDOWNSERVERS)) ||
                   (!isDown && (flags & CM_FLAG_CHECKUPSERVERS)))) {
                lock_ReleaseMutex(&tsp->mx);
                lock_ObtainWrite(&cm_serverLock);
                continue;
            }

            tsp->flags |= CM_SERVERFLAG_PINGING;
            lock_ReleaseMutex(&tsp->mx);

            serversp[nconns] = tsp;
            code = cm_ConnByServer(tsp, cm_rootUserp, &conns[nconns]);
            if (code) {
	            lock_ObtainWrite(&cm_serverLock);
                cm_PutServerNoLock(tsp);
                continue;
            }
            lock_ObtainWrite(&cm_serverLock);
            rxconns[nconns] = cm_GetRxConn(conns[nconns]);
            if (conntimer[nconns] = (isDown ? 1 : 0))
                rx_SetConnDeadTime(rxconns[nconns], 10);

            nconns++;
        }
        lock_ReleaseWrite(&cm_serverLock);

        /* Perform the multi call */
        start = time(NULL);
        multi_Rx(rxconns,nconns)
        {
            multi_VL_ProbeServer();
            results[multi_i]=multi_error;
        } multi_End;


        /* Process results of servers that support RXAFS_GetCapabilities */
        for (i=0; i<nconns; i++) {
            /* Leave the servers that did not support GetCapabilities alone */
            if (results[i] == RXGEN_OPCODE)
                continue;

            if (conntimer[i])
                rx_SetConnDeadTime(rxconns[i], ConnDeadtimeout);
            rx_PutConnection(rxconns[i]);
            cm_PutConn(conns[i]);

            tsp = serversp[i];
            cm_GCConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            wasDown = tsp->flags & CM_SERVERFLAG_DOWN;

            if (results[i] >= 0)  {
                /* mark server as up */
                tsp->flags &= ~CM_SERVERFLAG_DOWN;
                tsp->downTime = 0;
                tsp->capabilities = 0;

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
                                code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
                    tsp->flags |= CM_SERVERFLAG_DOWN;
                    tsp->downTime = time(NULL);
                }
                if (code != VRESTARTING)
                    cm_ForceNewConnections(tsp);

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
                                code = cm_GetVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
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
                tsp->flags &= ~CM_SERVERFLAG_PINGING;
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
    free(caps);
}
#endif /* MULTI_CHECKSERVERS */

void cm_InitServer(void)
{
    static osi_once_t once;
        
    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_serverLock, "cm_serverLock");
        osi_EndOnce(&once);
    }
}

void cm_GetServer(cm_server_t *serverp)
{
    lock_ObtainWrite(&cm_serverLock);
    serverp->refCount++;
    lock_ReleaseWrite(&cm_serverLock);
}

void cm_GetServerNoLock(cm_server_t *serverp)
{
    serverp->refCount++;
}

void cm_PutServer(cm_server_t *serverp)
{
    lock_ObtainWrite(&cm_serverLock);
    osi_assertx(serverp->refCount-- > 0, "cm_server_t refCount 0");
    lock_ReleaseWrite(&cm_serverLock);
}

void cm_PutServerNoLock(cm_server_t *serverp)
{
    osi_assertx(serverp->refCount-- > 0, "cm_server_t refCount 0");
}

void cm_SetServerNo64Bit(cm_server_t * serverp, int no64bit)
{
    lock_ObtainMutex(&serverp->mx);
    if (no64bit)
        serverp->flags |= CM_SERVERFLAG_NO64BIT;
    else
        serverp->flags &= ~CM_SERVERFLAG_NO64BIT;
    lock_ReleaseMutex(&serverp->mx);
}

void cm_SetServerNoInlineBulk(cm_server_t * serverp, int no)
{
    lock_ObtainMutex(&serverp->mx);
    if (no)
        serverp->flags |= CM_SERVERFLAG_NOINLINEBULK;
    else
        serverp->flags &= ~CM_SERVERFLAG_NOINLINEBULK;
    lock_ReleaseMutex(&serverp->mx);
}

void cm_SetServerPrefs(cm_server_t * serverp)
{
    unsigned long	serverAddr; 	/* in host byte order */
    unsigned long	myAddr, myNet, mySubnet;/* in host byte order */
    unsigned long	netMask;
    int 		i;

    int cm_noIPAddr;         /* number of client network interfaces */
    int cm_IPAddr[CM_MAXINTERFACE_ADDR];    /* client's IP address in host order */
    int cm_SubnetMask[CM_MAXINTERFACE_ADDR];/* client's subnet mask in host order*/
    int cm_NetMtu[CM_MAXINTERFACE_ADDR];    /* client's MTU sizes */
    int cm_NetFlags[CM_MAXINTERFACE_ADDR];  /* network flags */
    long code;

    /* get network related info */
    cm_noIPAddr = CM_MAXINTERFACE_ADDR;
    code = syscfg_GetIFInfo(&cm_noIPAddr,
			    cm_IPAddr, cm_SubnetMask,
			    cm_NetMtu, cm_NetFlags);

    serverAddr = ntohl(serverp->addr.sin_addr.s_addr);
    serverp->ipRank  = CM_IPRANK_LOW;	/* default setings */

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
	/* random between 0..15*/
	serverp->ipRank += min(serverp->ipRank, rand() % 0x000f);
    } /* and of for loop */
}

cm_server_t *cm_NewServer(struct sockaddr_in *socketp, int type, cm_cell_t *cellp, afs_uint32 flags) {
    cm_server_t *tsp;

    osi_assertx(socketp->sin_family == AF_INET, "unexpected socket family");

    tsp = malloc(sizeof(*tsp));
    if (tsp) {
        memset(tsp, 0, sizeof(*tsp));
        tsp->type = type;
        tsp->cellp = cellp;
        tsp->refCount = 1;
        lock_InitializeMutex(&tsp->mx, "cm_server_t mutex");
        tsp->addr = *socketp;

        cm_SetServerPrefs(tsp); 

        lock_ObtainWrite(&cm_serverLock); 	/* get server lock */
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

        lock_ReleaseWrite(&cm_serverLock); 	/* release server lock */

        if ( !(flags & CM_FLAG_NOPROBE) ) {
            tsp->flags = CM_SERVERFLAG_DOWN;	/* assume down; ping will mark up if available */
            cm_PingServer(tsp);	                /* Obtain Capabilities and check up/down state */
        }
    }
    return tsp;
}

cm_server_t *
cm_FindServerByIP(afs_uint32 ipaddr, int type)
{
    cm_server_t *tsp;

    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        if (tsp->type == type &&
            tsp->addr.sin_addr.S_un.S_addr == ipaddr)
            break;
    }
    lock_ReleaseRead(&cm_serverLock);

    return tsp;
}

/* find a server based on its properties */
cm_server_t *cm_FindServer(struct sockaddr_in *addrp, int type)
{
    cm_server_t *tsp;

    osi_assertx(addrp->sin_family == AF_INET, "unexpected socket value");
        
    lock_ObtainWrite(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp=tsp->allNextp) {
        if (tsp->type == type &&
            tsp->addr.sin_addr.s_addr == addrp->sin_addr.s_addr) 
            break;
    }       

    /* bump ref count if we found the server */
    if (tsp) 
        cm_GetServerNoLock(tsp);

    /* drop big table lock */
    lock_ReleaseWrite(&cm_serverLock);
	
    /* return what we found */
    return tsp;
}       

cm_server_vols_t *cm_NewServerVols(void) {
    cm_server_vols_t *tsvp;

    tsvp = malloc(sizeof(*tsvp));
    if (tsvp)
        memset(tsvp, 0, sizeof(*tsvp));

    return tsvp;
}

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

LONG_PTR cm_ChecksumServerList(cm_serverRef_t *serversp)
{
    LONG_PTR sum = 0;
    int first = 1;
    cm_serverRef_t *tsrp;

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (first)
            first = 0;
        else
            sum <<= 1;
        sum ^= (LONG_PTR) tsrp->server;
    }

    lock_ReleaseWrite(&cm_serverLock);
    return sum;
}

/*
** Insert a server into the server list keeping the list sorted in 
** asending order of ipRank. 
** 
** The refCount of the cm_serverRef_t is increased
*/
void cm_InsertServerList(cm_serverRef_t** list, cm_serverRef_t* element)
{
    cm_serverRef_t	*current=*list;
    unsigned short ipRank = element->server->ipRank;

    lock_ObtainWrite(&cm_serverLock);
    element->refCount++;                /* increase refCount */

    /* insertion into empty list  or at the beginning of the list */
    if ( !current || (current->server->ipRank > ipRank) )
    {
        element->next = *list;
        *list = element;
        lock_ReleaseWrite(&cm_serverLock);
        return ;	
    }
	
    while ( current->next ) /* find appropriate place to insert */
    {
        if ( current->next->server->ipRank > ipRank )
            break;
        else current = current->next;
    }
    element->next = current->next;
    current->next = element;
    lock_ReleaseWrite(&cm_serverLock);
}       
/*
** Re-sort the server list with the modified rank
** returns 0 if element was changed successfully. 
** returns 1 if  list remained unchanged.
*/
long cm_ChangeRankServer(cm_serverRef_t** list, cm_server_t*	server)
{
    cm_serverRef_t  **current=list;
    cm_serverRef_t	*element=0;

    /* if there is max of one element in the list, nothing to sort */
    if ( (!*current) || !((*current)->next)  )
        return 1;		/* list unchanged: return success */

    lock_ObtainWrite(&cm_serverLock);
    /* if the server is on the list, delete it from list */
    while ( *current )
    {
        if ( (*current)->server == server)
        {
            element = (*current);
            *current = (*current)->next; /* delete it */
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

    /* reduce refCount which was increased by cm_InsertServerList */
    lock_ObtainWrite(&cm_serverLock);
    element->refCount--;
    lock_ReleaseWrite(&cm_serverLock);
    return 0;
}
/*
** If there are more than one server on the list and the first n servers on 
** the list have the same rank( n>1), then randomise among the first n servers.
*/
void cm_RandomizeServer(cm_serverRef_t** list)
{
    int 		count, picked;
    cm_serverRef_t*	tsrp = *list, *lastTsrp;
    unsigned short	lowestRank;

    /* an empty list or a list with only one element */
    if ( !tsrp || ! tsrp->next )
        return ; 

    lock_ObtainWrite(&cm_serverLock);

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

    cm_PutServerNoLock(serverp);
    if (serverp->refCount == 0)
    {
        /* we need to check to ensure that all of the connections
         * for this server have a 0 refCount; otherwise, they will
         * not be garbage collected 
         */
        cm_GCConnections(serverp);  /* connsp */

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
    cm_serverRef_t  **current = list;
    cm_serverRef_t  **nextp = 0;
    cm_serverRef_t  * next = 0;

    lock_ObtainWrite(&cm_serverLock);

    while (*current)
    {
        nextp = &(*current)->next;
        if (--((*current)->refCount) == 0) {
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
  
    lock_ReleaseWrite(&cm_serverLock);
}
