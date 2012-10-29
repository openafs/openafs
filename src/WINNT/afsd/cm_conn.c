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
#include <string.h>
#include <malloc.h>
#include <osi.h>
#include "afsd.h"
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <afs/unified_afs.h>
#include <afs/vlserver.h>
#include <WINNT/afsreg.h>

osi_rwlock_t cm_connLock;

DWORD RDRtimeout = CM_CONN_DEFAULTRDRTIMEOUT;
unsigned short ConnDeadtimeout = CM_CONN_CONNDEADTIME;
unsigned short HardDeadtimeout = CM_CONN_HARDDEADTIME;
unsigned short IdleDeadtimeout = CM_CONN_IDLEDEADTIME;
unsigned short ReplicaIdleDeadtimeout = CM_CONN_IDLEDEADTIME_REP;
unsigned short NatPingInterval = CM_CONN_NATPINGINTERVAL;

#define LANMAN_WKS_PARAM_KEY "SYSTEM\\CurrentControlSet\\Services\\lanmanworkstation\\parameters"
#define LANMAN_WKS_SESSION_TIMEOUT "SessTimeout"
#define LANMAN_WKS_EXT_SESSION_TIMEOUT "ExtendedSessTimeout"

afs_uint32 cryptall = 0;
afs_uint32 cm_anonvldb = 0;
afs_uint32 rx_pmtu_discovery = 0;

void cm_PutConn(cm_conn_t *connp)
{
    afs_int32 refCount = InterlockedDecrement(&connp->refCount);
    osi_assertx(refCount >= 0, "cm_conn_t refcount underflow");
}

void cm_InitConn(void)
{
    static osi_once_t once;
    long code;
    DWORD dwValue;
    DWORD dummyLen;
    HKEY parmKey;

    if (osi_Once(&once)) {
	lock_InitializeRWLock(&cm_connLock, "connection global lock",
                               LOCK_HIERARCHY_CONN_GLOBAL);

        /* keisa - read timeout value for lanmanworkstation  service.
         * jaltman - as per
         *   http://support.microsoft.com:80/support/kb/articles/Q102/0/67.asp&NoWebContent=1
         * the SessTimeout is a minimum timeout not a maximum timeout.  Therefore,
         * I believe that the default should not be short.  Instead, we should wait until
         * RX times out before reporting a timeout to the SMB client.
         */
	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, LANMAN_WKS_PARAM_KEY,
			    0, KEY_QUERY_VALUE, &parmKey);
	if (code == ERROR_SUCCESS)
        {
            BOOL extTimeouts = msftSMBRedirectorSupportsExtendedTimeouts();

            if (extTimeouts) {
                /*
                 * The default value is 1000 seconds.  However, this default
                 * will not apply to "\\AFS" unless "AFS" is listed in
                 * ServersWithExtendedSessTimeout which we will add when we
                 * create the ExtendedSessTimeout value in smb_Init()
                 */
                dummyLen = sizeof(DWORD);
                code = RegQueryValueEx(parmKey,
                                       LANMAN_WKS_EXT_SESSION_TIMEOUT,
                                        NULL, NULL,
                                        (BYTE *) &dwValue, &dummyLen);
                if (code == ERROR_SUCCESS) {
                    RDRtimeout = dwValue;
                    afsi_log("lanmanworkstation : ExtSessTimeout %u", RDRtimeout);
                }
            }
            if (!extTimeouts || code != ERROR_SUCCESS) {
                dummyLen = sizeof(DWORD);
                code = RegQueryValueEx(parmKey,
                                       LANMAN_WKS_SESSION_TIMEOUT,
                                       NULL, NULL,
                                       (BYTE *) &dwValue, &dummyLen);
                if (code == ERROR_SUCCESS) {
                    RDRtimeout = dwValue;
                    afsi_log("lanmanworkstation : SessTimeout %u", RDRtimeout);
                }
            }
	    RegCloseKey(parmKey);
        }

	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
			     0, KEY_QUERY_VALUE, &parmKey);
	if (code == ERROR_SUCCESS) {
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "ConnDeadTimeout", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS) {
                ConnDeadtimeout = (unsigned short)dwValue;
                afsi_log("ConnDeadTimeout is %d", ConnDeadtimeout);
            }
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "HardDeadTimeout", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS) {
                HardDeadtimeout = (unsigned short)dwValue;
                afsi_log("HardDeadTimeout is %d", HardDeadtimeout);
            }
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "IdleDeadTimeout", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS) {
                IdleDeadtimeout = (unsigned short)dwValue;
                afsi_log("IdleDeadTimeout is %d", IdleDeadtimeout);
            }
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "ReplicaIdleDeadTimeout", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS) {
                ReplicaIdleDeadtimeout = (unsigned short)dwValue;
                afsi_log("ReplicaIdleDeadTimeout is %d", ReplicaIdleDeadtimeout);
            }
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "NatPingInterval", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS) {
                NatPingInterval = (unsigned short)dwValue;
            }
            afsi_log("NatPingInterval is %d", NatPingInterval);
            RegCloseKey(parmKey);
	}

        /*
         * If these values were not set via cpp macro or obtained
         * from the registry, we use a value that is derived from
         * the smb redirector session timeout (RDRtimeout).
         *
         * The UNIX cache manager uses 120 seconds for the hard dead
         * timeout and 1200 seconds for the connection and idle timeouts.
         *
         * We base our values on those while making sure we leave
         * enough time for overhead.
         *
         * To further complicate matters we need to take into account
         * file server hard dead timeouts as they affect the length
         * of time it takes the file server to give up when attempting
         * to break callbacks to unresponsive clients.  The file
         * server hard dead timeout is 120 seconds.
         */
        afsi_log("lanmanworkstation : SessTimeout %u", RDRtimeout);
        if (ConnDeadtimeout == 0) {
            ConnDeadtimeout = (unsigned short) ((RDRtimeout / 2) < 50 ? (RDRtimeout / 2) : 50);
            afsi_log("ConnDeadTimeout is %d", ConnDeadtimeout);
        }
        if (HardDeadtimeout == 0) {
            HardDeadtimeout = (unsigned short) (RDRtimeout > 125 ? 120 : (RDRtimeout - 5));
            afsi_log("HardDeadTimeout is %d", HardDeadtimeout);
        }
        if (IdleDeadtimeout == 0) {
            IdleDeadtimeout = 10 * (unsigned short) HardDeadtimeout;
            afsi_log("IdleDeadTimeout is %d", IdleDeadtimeout);
        }
        if (ReplicaIdleDeadtimeout == 0) {
            ReplicaIdleDeadtimeout = (unsigned short) HardDeadtimeout;
            afsi_log("ReplicaIdleDeadTimeout is %d", ReplicaIdleDeadtimeout);
        }
	osi_EndOnce(&once);
    }
}

void cm_InitReq(cm_req_t *reqp)
{
	memset(reqp, 0, sizeof(cm_req_t));
	reqp->startTime = GetTickCount();
}

static long cm_GetServerList(struct cm_fid *fidp, struct cm_user *userp,
	struct cm_req *reqp, afs_uint32 *replicated, cm_serverRef_t ***serversppp)
{
    long code;
    cm_volume_t *volp = NULL;
    cm_vol_state_t *volstatep = NULL;
    cm_cell_t *cellp = NULL;

    if (!fidp) {
        *serversppp = NULL;
        return CM_ERROR_INVAL;
    }

    cellp = cm_FindCellByID(fidp->cell, 0);
    if (!cellp)
        return CM_ERROR_NOSUCHCELL;

    code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp, CM_GETVOL_FLAG_CREATE, &volp);
    if (code)
        return code;

    volstatep = cm_VolumeStateByID(volp, fidp->volume);
    *replicated = (volstatep->flags & CM_VOL_STATE_FLAG_REPLICATED);
    *serversppp = cm_GetVolServers(volp, fidp->volume, userp, reqp);

    lock_ObtainRead(&cm_volumeLock);
    cm_PutVolume(volp);
    lock_ReleaseRead(&cm_volumeLock);
    return (*serversppp ? 0 : CM_ERROR_NOSUCHVOLUME);
}

void
cm_SetServerBusyStatus(cm_serverRef_t *serversp, cm_server_t *serverp)
{
    cm_serverRef_t *tsrp;

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (tsrp->status == srv_deleted)
            continue;
        if (cm_ServerEqual(tsrp->server, serverp) && tsrp->status == srv_not_busy) {
            tsrp->status = srv_busy;
            break;
        }
    }
    lock_ReleaseWrite(&cm_serverLock);
}

void
cm_ResetServerBusyStatus(cm_serverRef_t *serversp)
{
    cm_serverRef_t *tsrp;

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (tsrp->status == srv_deleted)
            continue;
        if (tsrp->status == srv_busy) {
            tsrp->status = srv_not_busy;
        }
    }
    lock_ReleaseWrite(&cm_serverLock);
}

/*
 * Analyze the error return from an RPC.  Determine whether or not to retry,
 * and if we're going to retry, determine whether failover is appropriate,
 * and whether timed backoff is appropriate.
 *
 * If the error code is from cm_ConnFromFID() or friends, it will be a CM_ERROR code.
 * Otherwise it will be an RPC code.  This may be a UNIX code (e.g. EDQUOT), or
 * it may be an RX code, or it may be a special code (e.g. VNOVOL), or it may
 * be a security code (e.g. RXKADEXPIRED).
 *
 * If the error code is from cm_ConnFromFID() or friends, connp will be NULL.
 *
 * For VLDB calls, fidp will be NULL.
 *
 * volSyncp and/or cbrp may also be NULL.
 */
int
cm_Analyze(cm_conn_t *connp,
           cm_user_t *userp,
           cm_req_t *reqp,
           struct cm_fid *fidp,
           afs_uint32 storeOp,
           AFSVolSync *volSyncp,
           cm_serverRef_t * serversp,
           cm_callbackRequest_t *cbrp,
           long errorCode)
{
    cm_server_t *serverp = NULL;
    cm_serverRef_t **serverspp = NULL;
    cm_serverRef_t *tsrp;
    cm_cell_t  *cellp = NULL;
    cm_ucell_t *ucellp;
    cm_volume_t * volp = NULL;
    cm_vol_state_t *statep = NULL;
    cm_scache_t * scp = NULL;
    afs_uint32 replicated;
    int retry = 0;
    int free_svr_list = 0;
    int dead_session;
    long timeUsed, timeLeft;
    long code;
    char addr[16]="unknown";
    int forcing_new = 0;
    int location_updated = 0;
    char *format;
    DWORD msgID;

    osi_Log2(afsd_logp, "cm_Analyze connp 0x%p, code 0x%x",
             connp, errorCode);

    /* no locking required, since connp->serverp never changes after
     * creation */
    dead_session = (userp->cellInfop == NULL);
    if (connp)
        serverp = connp->serverp;

    /* Update callback pointer */
    if (cbrp && serverp && errorCode == 0) {
        if (cbrp->serverp) {
            if ( cbrp->serverp != serverp ) {
                lock_ObtainWrite(&cm_serverLock);
                cm_PutServerNoLock(cbrp->serverp);
                cm_GetServerNoLock(serverp);
                lock_ReleaseWrite(&cm_serverLock);
            }
        } else {
            cm_GetServer(serverp);
        }
        lock_ObtainWrite(&cm_callbackLock);
        cbrp->serverp = serverp;
        lock_ReleaseWrite(&cm_callbackLock);
    }

    /* if timeout - check that it did not exceed the HardDead timeout
     * and retry */

    /* timeleft - get it from reqp the same way as cm_ConnByMServers does */
    timeUsed = (GetTickCount() - reqp->startTime) / 1000;
    timeLeft = HardDeadtimeout - timeUsed;

    /* get a pointer to the cell */
    if (errorCode) {
        if (cellp == NULL && serverp)
            cellp = serverp->cellp;
        if (cellp == NULL && serversp) {
            struct cm_serverRef * refp;
            for ( refp=serversp ; cellp == NULL && refp != NULL; refp=refp->next) {
                if (refp->status == srv_deleted)
                    continue;
                if ( refp->server )
                    cellp = refp->server->cellp;
            }
        }
        if (cellp == NULL && fidp) {
            cellp = cm_FindCellByID(fidp->cell, 0);
        }
    }

    if (errorCode == CM_ERROR_TIMEDOUT) {
        if ( timeLeft > 5 ) {
            thrd_Sleep(3000);
            cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS, cellp);
            retry = 1;
        }
    }

    else if (errorCode == UAEWOULDBLOCK || errorCode == EWOULDBLOCK ||
              errorCode == UAEAGAIN || errorCode == EAGAIN) {
	osi_Log0(afsd_logp, "cm_Analyze passed EWOULDBLOCK or EAGAIN.");
        if ( timeLeft > 5 ) {
            thrd_Sleep(1000);
            retry = 1;
        }
    }

    /* if there is nosuchvolume, then we have a situation in which a
     * previously known volume no longer has a set of servers
     * associated with it.  Either the volume has moved or
     * the volume has been deleted.  Try to find a new server list
     * until the timeout period expires.
     */
    else if (errorCode == CM_ERROR_NOSUCHVOLUME) {
	osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_NOSUCHVOLUME.");
        /*
         * The VNOVOL or VL_NOENT error has already been translated
         * to CM_ERROR_NOSUCHVOLUME.  There is nothing for us to do.
         */
    }

    else if (errorCode == CM_ERROR_ALLDOWN) {
	/* Servers marked DOWN will be restored by the background daemon
	 * thread as they become available.  The volume status is
         * updated as the server state changes.
	 */
        if (fidp) {
            osi_Log2(afsd_logp, "cm_Analyze passed CM_ERROR_DOWN (FS cell %s vol 0x%x)",
                      cellp->name, fidp->volume);
            msgID = MSG_ALL_SERVERS_DOWN;
            format = "All servers are unreachable when accessing cell %s volume %d.";
	    LogEvent(EVENTLOG_WARNING_TYPE, msgID, cellp->name, fidp->volume);
        } else {
            osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_ALLDOWN (VL Server)");
        }
    }

    else if (errorCode == CM_ERROR_ALLOFFLINE) {
        /* Volume instances marked offline will be restored by the
         * background daemon thread as they become available
         */
        if (fidp) {
            osi_Log2(afsd_logp, "cm_Analyze passed CM_ERROR_ALLOFFLINE (FS cell %s vol 0x%x)",
                      cellp->name, fidp->volume);
            msgID = MSG_ALL_SERVERS_OFFLINE;
            format = "All servers are offline when accessing cell %s volume %d.";
	    LogEvent(EVENTLOG_WARNING_TYPE, msgID, cellp->name, fidp->volume);

            if (!serversp) {
                code = cm_GetServerList(fidp, userp, reqp, &replicated, &serverspp);
                if (code == 0) {
                    serversp = *serverspp;
                    free_svr_list = 1;
                }
            }
            cm_ResetServerBusyStatus(serversp);
            if (free_svr_list) {
                cm_FreeServerList(serverspp, 0);
                free_svr_list = 0;
                serversp = NULL;
            }

            code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp,
                                      CM_GETVOL_FLAG_NO_LRU_UPDATE,
                                      &volp);
            if (code == 0) {
                /*
                 * Do not perform a cm_CheckOfflineVolume() if cm_Analyze()
                 * was called by cm_CheckOfflineVolumeState().
                 */
                if (!(reqp->flags & CM_REQ_OFFLINE_VOL_CHK) && timeLeft > 7) {
                    thrd_Sleep(5000);

                    /* cm_CheckOfflineVolume() resets the serverRef state */
                    if (cm_CheckOfflineVolume(volp, fidp->volume))
                        retry = 1;
                } else {
                    cm_UpdateVolumeStatus(volp, fidp->volume);
                }
                lock_ObtainRead(&cm_volumeLock);
                cm_PutVolume(volp);
                lock_ReleaseRead(&cm_volumeLock);
                volp = NULL;
            }
        } else {
            osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_ALLOFFLINE (VL Server)");
        }
    }
    else if (errorCode == CM_ERROR_ALLBUSY) {
        /* Volumes that are busy cannot be determined to be non-busy
         * without actually attempting to access them.
         */
        if (fidp) { /* File Server query */
            osi_Log2(afsd_logp, "cm_Analyze passed CM_ERROR_ALLBUSY (FS cell %s vol 0x%x)",
                     cellp->name, fidp->volume);
            msgID = MSG_ALL_SERVERS_BUSY;
            format = "All servers are busy when accessing cell %s volume %d.";
	    LogEvent(EVENTLOG_WARNING_TYPE, msgID, cellp->name, fidp->volume);

            if (!serversp) {
                code = cm_GetServerList(fidp, userp, reqp, &replicated, &serverspp);
                if (code == 0) {
                    serversp = *serverspp;
                    free_svr_list = 1;
                }
            }
            cm_ResetServerBusyStatus(serversp);
            if (free_svr_list) {
                cm_FreeServerList(serverspp, 0);
                free_svr_list = 0;
                serversp = NULL;
            }

            code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp,
                                     CM_GETVOL_FLAG_NO_LRU_UPDATE,
                                     &volp);
            if (code == 0) {
                if (timeLeft > 7) {
                    thrd_Sleep(5000);
                    statep = cm_VolumeStateByID(volp, fidp->volume);
                    retry = 1;
                }
                cm_UpdateVolumeStatus(volp, fidp->volume);

                lock_ObtainRead(&cm_volumeLock);
                cm_PutVolume(volp);
                lock_ReleaseRead(&cm_volumeLock);
                volp = NULL;
            }
        } else {    /* VL Server query */
            osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_ALLBUSY (VL Server).");

            if (timeLeft > 7) {
                thrd_Sleep(5000);

                if (serversp) {
                    cm_ResetServerBusyStatus(serversp);
                    retry = 1;
                }
            }
        }
    }

    /* special codes:  VBUSY and VRESTARTING */
    else if (errorCode == VBUSY || errorCode == VRESTARTING) {
        if (!serversp && fidp) {
            code = cm_GetServerList(fidp, userp, reqp, &replicated, &serverspp);
            if (code == 0) {
                serversp = *serverspp;
                free_svr_list = 1;
            }
        }

        switch ( errorCode ) {
        case VBUSY:
	    msgID = MSG_SERVER_REPORTS_VBUSY;
            format = "Server %s reported busy when accessing volume %d in cell %s.";
            break;
        case VRESTARTING:
	    msgID = MSG_SERVER_REPORTS_VRESTARTING;
            format = "Server %s reported restarting when accessing volume %d in cell %s.";
            break;
        }

        if (serverp && fidp) {
            /* Log server being offline for this volume */
            sprintf(addr, "%d.%d.%d.%d",
                   ((serverp->addr.sin_addr.s_addr & 0xff)),
                   ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                   ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                   ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

	    osi_Log3(afsd_logp, format, osi_LogSaveString(afsd_logp,addr), fidp->volume, cellp->name);
	    LogEvent(EVENTLOG_WARNING_TYPE, msgID, addr, fidp->volume, cellp->name);
        }

        cm_SetServerBusyStatus(serversp, serverp);

        if (fidp) { /* File Server query */
            code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp,
                                      CM_GETVOL_FLAG_NO_LRU_UPDATE,
                                      &volp);
            if (code == 0) {
                statep = cm_VolumeStateByID(volp, fidp->volume);

                if (statep)
                    cm_UpdateVolumeStatus(volp, statep->ID);

                lock_ObtainRead(&cm_volumeLock);
                cm_PutVolume(volp);
                lock_ReleaseRead(&cm_volumeLock);
                volp = NULL;
            }
        }

        if (free_svr_list) {
            cm_FreeServerList(serverspp, 0);
            serversp = NULL;
            free_svr_list = 0;
        }
        retry = 1;
    }

    /* special codes:  missing volumes */
    else if (errorCode == VNOVOL || errorCode == VMOVED || errorCode == VOFFLINE ||
             errorCode == VSALVAGE || errorCode == VIO)
    {
        /* In case of timeout */
        reqp->volumeError = errorCode;

        switch ( errorCode ) {
        case VNOVOL:
	    msgID = MSG_SERVER_REPORTS_VNOVOL;
            format = "Server %s reported volume %d in cell %s as not attached (may have been moved or deleted).";
            break;
        case VMOVED:
	    msgID = MSG_SERVER_REPORTS_VMOVED;
            format = "Server %s reported volume %d in cell %s as moved.";
            break;
        case VOFFLINE:
	    msgID = MSG_SERVER_REPORTS_VOFFLINE;
            format = "Server %s reported volume %d in cell %s as offline.";
            break;
        case VSALVAGE:
	    msgID = MSG_SERVER_REPORTS_VSALVAGE;
            format = "Server %s reported volume %d in cell %s as needs salvage.";
            break;
        case VIO:
	    msgID = MSG_SERVER_REPORTS_VIO;
            format = "Server %s reported volume %d in cell %s as temporarily unaccessible.";
            break;
        }

        if (fidp) { /* File Server query */
            if (serverp) {
                /* Log server being unavailable for this volume */
                sprintf(addr, "%d.%d.%d.%d",
                         ((serverp->addr.sin_addr.s_addr & 0xff)),
                         ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                         ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                         ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

                osi_Log3(afsd_logp, format, osi_LogSaveString(afsd_logp,addr), fidp->volume, cellp->name);
                LogEvent(EVENTLOG_WARNING_TYPE, msgID, addr, fidp->volume, cellp->name);
            }

            code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp,
                                      CM_GETVOL_FLAG_NO_LRU_UPDATE,
                                      &volp);
            if (code == 0)
                statep = cm_VolumeStateByID(volp, fidp->volume);

            if ((errorCode == VMOVED || errorCode == VNOVOL || errorCode == VOFFLINE) &&
                !(reqp->flags & CM_REQ_VOLUME_UPDATED))
            {
                code = cm_ForceUpdateVolume(fidp, userp, reqp);
                if (code == 0)
                    location_updated = 1;

                /* Even if the update fails, there might still be another replica */

                reqp->flags |= CM_REQ_VOLUME_UPDATED;
                osi_Log3(afsd_logp, "cm_Analyze called cm_ForceUpdateVolume cell %u vol %u code 0x%x",
                        fidp->cell, fidp->volume, code);
            }

            if (statep) {
                cm_UpdateVolumeStatus(volp, statep->ID);
                osi_Log3(afsd_logp, "cm_Analyze NewVolState cell %u vol %u state %u",
                         fidp->cell, fidp->volume, statep->state);
            }

            if (volp) {
                lock_ObtainRead(&cm_volumeLock);
                cm_PutVolume(volp);
                lock_ReleaseRead(&cm_volumeLock);
                volp = NULL;
            }

            /*
             * Mark server offline for this volume or delete the volume
             * from the server list if it was moved or is not present.
             */
            if (!serversp || location_updated) {
                code = cm_GetServerList(fidp, userp, reqp, &replicated, &serverspp);
                if (code == 0) {
                    serversp = *serverspp;
                    free_svr_list = 1;
                }
            }
        }

        lock_ObtainWrite(&cm_serverLock);
        for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
            if (tsrp->status == srv_deleted)
                continue;

            sprintf(addr, "%d.%d.%d.%d",
                     ((tsrp->server->addr.sin_addr.s_addr & 0xff)),
                     ((tsrp->server->addr.sin_addr.s_addr & 0xff00)>> 8),
                     ((tsrp->server->addr.sin_addr.s_addr & 0xff0000)>> 16),
                     ((tsrp->server->addr.sin_addr.s_addr & 0xff000000)>> 24));

            if (cm_ServerEqual(tsrp->server, serverp)) {
                /* REDIRECT */
                switch (errorCode) {
                case VMOVED:
                    osi_Log2(afsd_logp, "volume %u moved from server %s",
                             fidp->volume, osi_LogSaveString(afsd_logp,addr));
                    tsrp->status = srv_deleted;
                    if (fidp)
                        cm_RemoveVolumeFromServer(serverp, fidp->volume);
                    break;
                case VNOVOL:
                    /*
                     * The 1.6.0 and 1.6.1 file servers send transient VNOVOL errors which
                     * are no indicative of the volume not being present.  For example,
                     * VNOVOL can be sent during a transition to a VBUSY state prior to
                     * salvaging or when cloning a .backup volume instance.  As a result
                     * the cache manager must attempt at least one retry when a VNOVOL is
                     * receive but there are no changes to the volume location information.
                     */
                    if (reqp->vnovolError > 0 && cm_ServerEqual(reqp->errorServp, serverp)) {
                        osi_Log2(afsd_logp, "volume %u not present on server %s",
                                  fidp->volume, osi_LogSaveString(afsd_logp,addr));
                        tsrp->status = srv_deleted;
                        if (fidp)
                            cm_RemoveVolumeFromServer(serverp, fidp->volume);
                    } else {
                        osi_Log2(afsd_logp, "VNOVOL received for volume %u from server %s",
                                 fidp->volume, osi_LogSaveString(afsd_logp,addr));
                        if (replicated) {
                            cm_SetServerBusyStatus(serversp, serverp);
                        } else {
                            Sleep(2000);
                        }
                    }
                    break;
                case VOFFLINE:
                    osi_Log2(afsd_logp, "VOFFLINE received for volume %u from server %s",
                              fidp->volume, osi_LogSaveString(afsd_logp,addr));
                    tsrp->status = srv_offline;
                    break;
                default:
                    osi_Log3(afsd_logp, "volume %u exists on server %s with status %u",
                             fidp->volume, osi_LogSaveString(afsd_logp,addr), tsrp->status);
                }
            }
        }
        lock_ReleaseWrite(&cm_serverLock);

        /* Remember that the VNOVOL error occurred */
        if (errorCode == VNOVOL) {
            reqp->errorServp = serverp;
            reqp->vnovolError++;
        }

        /* Free the server list before cm_ForceUpdateVolume is called */
        if (free_svr_list) {
            cm_FreeServerList(serverspp, 0);
            serversp = NULL;
            free_svr_list = 0;
        }

        if ( timeLeft > 2 )
            retry = 1;
    } else if ( errorCode == VNOVNODE ) {
	if ( fidp ) {
	    osi_Log4(afsd_logp, "cm_Analyze passed VNOVNODE cell %u vol %u vn %u uniq %u.",
		      fidp->cell, fidp->volume, fidp->vnode, fidp->unique);

	    scp = cm_FindSCache(fidp);
	    if (scp) {
		cm_scache_t *pscp = NULL;

		if (scp->fileType != CM_SCACHETYPE_DIRECTORY)
		    pscp = cm_FindSCacheParent(scp);

		lock_ObtainWrite(&scp->rw);
		scp->flags |= CM_SCACHEFLAG_DELETED;
		lock_ObtainWrite(&cm_scacheLock);
                cm_AdjustScacheLRU(scp);
                cm_RemoveSCacheFromHashTable(scp);
		lock_ReleaseWrite(&cm_scacheLock);
                cm_LockMarkSCacheLost(scp);
		lock_ReleaseWrite(&scp->rw);
		cm_ReleaseSCache(scp);

 		if (pscp) {
		    if (cm_HaveCallback(pscp)) {
 			lock_ObtainWrite(&pscp->rw);
 			cm_DiscardSCache(pscp);
 			lock_ReleaseWrite(&pscp->rw);
 		    }
 		    cm_ReleaseSCache(pscp);
 		}
	    }
	} else {
	    osi_Log0(afsd_logp, "cm_Analyze passed VNOVNODE unknown fid.");
	}
    }

    /* RX codes */
    else if (errorCode == RX_CALL_TIMEOUT) {
        /* RPC took longer than hardDeadTime or the server
         * reported idle for longer than idleDeadTime
         * don't mark server as down but don't retry
         * this is to prevent the SMB session from timing out
         * In addition, we log an event to the event log
         */

        if (serverp) {
            sprintf(addr, "%d.%d.%d.%d",
                    ((serverp->addr.sin_addr.s_addr & 0xff)),
                    ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                    ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                    ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

            LogEvent(EVENTLOG_WARNING_TYPE, MSG_RX_HARD_DEAD_TIME_EXCEEDED, addr);
            osi_Log1(afsd_logp, "cm_Analyze: hardDeadTime or idleDeadtime exceeded addr[%s]",
                     osi_LogSaveString(afsd_logp,addr));
            reqp->errorServp = serverp;
            reqp->idleError++;
        }

        if (fidp && storeOp)
            scp = cm_FindSCache(fidp);
        if (scp) {
            if (cm_HaveCallback(scp)) {
                lock_ObtainWrite(&scp->rw);
                cm_DiscardSCache(scp);
                lock_ReleaseWrite(&scp->rw);

                /*
                * We really should notify the redirector that we discarded
                * the status information but doing so in this case is not
                * safe as it can result in a deadlock with extent release
                * processing.
                */
            }
            cm_ReleaseSCache(scp);
        }

        if (timeLeft > 2) {
            if (!fidp) { /* vldb */
                retry = 1;
            } else { /* file */
                cm_volume_t *volp = cm_GetVolumeByFID(fidp);
                if (volp) {
                    if (fidp->volume == cm_GetROVolumeID(volp))
                        retry = 1;
                    cm_PutVolume(volp);
                }
            }
        }
    }
    else if (errorCode == RX_MSGSIZE) {
        /*
         * RPC failed because a transmitted rx packet
         * may have grown larger than the path mtu.
         * Force a retry and the Rx library will try
         * with a smaller mtu size.
         */

        if (serverp)
            sprintf(addr, "%d.%d.%d.%d",
                    ((serverp->addr.sin_addr.s_addr & 0xff)),
                    ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                    ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                    ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

        LogEvent(EVENTLOG_WARNING_TYPE, MSG_RX_MSGSIZE_EXCEEDED, addr);
        osi_Log1(afsd_logp, "cm_Analyze: Path MTU may have been exceeded addr[%s]",
                 osi_LogSaveString(afsd_logp,addr));

        retry = 1;
    }
    else if (errorCode == RX_CALL_BUSY) {
        /*
         * RPC failed because the selected call channel
         * is currently busy on the server.  Unconditionally
         * retry the request so an alternate call channel can be used.
         */
        if (serverp)
            sprintf(addr, "%d.%d.%d.%d",
                    ((serverp->addr.sin_addr.s_addr & 0xff)),
                    ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                    ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                    ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

        LogEvent(EVENTLOG_WARNING_TYPE, MSG_RX_BUSY_CALL_CHANNEL, addr);
        osi_Log1(afsd_logp, "cm_Analyze: Retry RPC due to busy call channel addr[%s]",
                 osi_LogSaveString(afsd_logp,addr));
        retry = 1;
    }
    else if (errorCode == VNOSERVICE) {
        /*
         * The server did not service the RPC.
         * If this was a file server RPC it means that for at
         * least the file server's idle dead timeout period the
         * file server did not receive any new data packets from
         * client.
         *
         * The RPC was not serviced so it can be retried and any
         * existing status information is still valid.
         */
        if (fidp) {
            if (serverp)
                sprintf(addr, "%d.%d.%d.%d",
                        ((serverp->addr.sin_addr.s_addr & 0xff)),
                        ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                        ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                        ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

            LogEvent(EVENTLOG_WARNING_TYPE, MSG_SERVER_REPORTS_VNOSERVICE,
                     addr, fidp->volume, cellp->name);
            osi_Log3(afsd_logp, "Server %s reported rpc to volume %d in cell %s as not serviced.",
                     osi_LogSaveString(afsd_logp,addr), fidp->volume, cellp->name);
        }

        if (timeLeft > 2)
            retry = 1;
    }
    else if (errorCode == RX_CALL_IDLE) {
        /*
         * RPC failed because the server failed to respond with data
         * within the idle dead timeout period.  This could be for a variety
         * of reasons:
         *  1. The server could have a bad partition such as a failed
         *     disk or iSCSI target and all I/O to that partition is
         *     blocking on the server and will never complete.
         *
         *  2. The server vnode may be locked by another client request
         *     that is taking a very long time.
         *
         *  3. The server may have a very long queue of requests
         *     pending and is unable to process this request.
         *
         *  4. The server could be malicious and is performing a denial
         *     of service attack against the client.
         *
         * If this is a request against a .readonly with alternate sites
         * the server should be marked down for this request and the
         * client should fail over to another server.  If this is a
         * request against a single source, the client may retry once.
         */
        if (serverp)
            sprintf(addr, "%d.%d.%d.%d",
                    ((serverp->addr.sin_addr.s_addr & 0xff)),
                    ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                    ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                    ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

        if (fidp) {
            code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp,
                                     CM_GETVOL_FLAG_NO_LRU_UPDATE,
                                     &volp);
            if (code == 0) {
                statep = cm_VolumeStateByID(volp, fidp->volume);

                if (statep)
                    replicated = (statep->flags & CM_VOL_STATE_FLAG_REPLICATED);

                lock_ObtainRead(&cm_volumeLock);
                cm_PutVolume(volp);
                lock_ReleaseRead(&cm_volumeLock);
                volp = NULL;
            }

            if (storeOp)
                scp = cm_FindSCache(fidp);
	    if (scp) {
                if (cm_HaveCallback(scp)) {
                    lock_ObtainWrite(&scp->rw);
                    cm_DiscardSCache(scp);
                    lock_ReleaseWrite(&scp->rw);

                    /*
                     * We really should notify the redirector that we discarded
                     * the status information but doing so in this case is not
                     * safe as it can result in a deadlock with extent release
                     * processing.
                     */
                }
                cm_ReleaseSCache(scp);
            }
        }

        if (replicated && serverp) {
            reqp->errorServp = serverp;
            reqp->tokenError = errorCode;

            if (timeLeft > 2)
                retry = 1;
        }

        LogEvent(EVENTLOG_WARNING_TYPE, MSG_RX_IDLE_DEAD_TIMEOUT, addr, retry);
        osi_Log2(afsd_logp, "cm_Analyze: RPC failed due to idle dead timeout addr[%s] retry=%u",
                 osi_LogSaveString(afsd_logp,addr), retry);
    }
    else if (errorCode == RX_CALL_DEAD) {
        /* mark server as down */
        if (serverp)
            sprintf(addr, "%d.%d.%d.%d",
                    ((serverp->addr.sin_addr.s_addr & 0xff)),
                    ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                    ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                    ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

        osi_Log2(afsd_logp, "cm_Analyze: Rx Call Dead addr[%s] forcedNew[%s]",
                 osi_LogSaveString(afsd_logp,addr),
                 (reqp->flags & CM_REQ_NEW_CONN_FORCED ? "yes" : "no"));

        if (serverp) {
            if ((reqp->flags & CM_REQ_NEW_CONN_FORCED)) {
                lock_ObtainMutex(&serverp->mx);
                if (!(serverp->flags & CM_SERVERFLAG_DOWN)) {
                    _InterlockedOr(&serverp->flags, CM_SERVERFLAG_DOWN);
                    serverp->downTime = time(NULL);
                }
                lock_ReleaseMutex(&serverp->mx);
            } else {
                reqp->flags |= CM_REQ_NEW_CONN_FORCED;
                forcing_new = 1;
                cm_ForceNewConnections(serverp);
            }
        }

        if (fidp && storeOp)
            scp = cm_FindSCache(fidp);
        if (scp) {
            if (cm_HaveCallback(scp)) {
                lock_ObtainWrite(&scp->rw);
                cm_DiscardSCache(scp);
                lock_ReleaseWrite(&scp->rw);

                /*
                * We really should notify the redirector that we discarded
                * the status information but doing so in this case is not
                * safe as it can result in a deadlock with extent release
                * processing.
                */
            }
            cm_ReleaseSCache(scp);
        }

        if ( timeLeft > 2 )
            retry = 1;
    }
    else if (errorCode >= -64 && errorCode < 0) {
        /* mark server as down */
        if (serverp)
            sprintf(addr, "%d.%d.%d.%d",
                    ((serverp->addr.sin_addr.s_addr & 0xff)),
                    ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                    ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                    ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24));

        osi_Log3(afsd_logp, "cm_Analyze: Rx Misc Error[%d] addr[%s] forcedNew[%s]",
                 errorCode,
                 osi_LogSaveString(afsd_logp,addr),
                 (reqp->flags & CM_REQ_NEW_CONN_FORCED ? "yes" : "no"));

        if (serverp) {
            if (reqp->flags & CM_REQ_NEW_CONN_FORCED) {
                reqp->errorServp = serverp;
                reqp->tokenError = errorCode;
            } else {
                reqp->flags |= CM_REQ_NEW_CONN_FORCED;
                forcing_new = 1;
                cm_ForceNewConnections(serverp);
            }
        }
        if ( timeLeft > 2 )
            retry = 1;
    }
    else if (errorCode == RXKADEXPIRED) {
        osi_Log1(afsd_logp, "cm_Analyze: rxkad error code 0x%x (RXKADEXPIRED)",
                 errorCode);
        if (!dead_session) {
            lock_ObtainMutex(&userp->mx);
            ucellp = cm_GetUCell(userp, serverp->cellp);
            if (ucellp->ticketp) {
                free(ucellp->ticketp);
                ucellp->ticketp = NULL;
            }
            _InterlockedAnd(&ucellp->flags, ~CM_UCELLFLAG_RXKAD);
            ucellp->gen++;
            lock_ReleaseMutex(&userp->mx);
            if ( timeLeft > 2 )
                retry = 1;
        }
    } else if (errorCode >= ERROR_TABLE_BASE_RXK && errorCode < ERROR_TABLE_BASE_RXK + 256) {
        char * s = "unknown error";
        switch ( errorCode ) {
        case RXKADINCONSISTENCY: s = "RXKADINCONSISTENCY"; break;
        case RXKADPACKETSHORT  : s = "RXKADPACKETSHORT";   break;
        case RXKADLEVELFAIL    : s = "RXKADLEVELFAIL";     break;
        case RXKADTICKETLEN    : s = "RXKADTICKETLEN";     break;
        case RXKADOUTOFSEQUENCE: s = "RXKADOUTOFSEQUENCE"; break;
        case RXKADNOAUTH       : s = "RXKADNOAUTH";        break;
        case RXKADBADKEY       : s = "RXKADBADKEY";        break;
        case RXKADBADTICKET    : s = "RXKADBADTICKET";     break;
        case RXKADUNKNOWNKEY   : s = "RXKADUNKNOWNKEY";    break;
        case RXKADEXPIRED      : s = "RXKADEXPIRED";       break;
        case RXKADSEALEDINCON  : s = "RXKADSEALEDINCON";   break;
        case RXKADDATALEN      : s = "RXKADDATALEN";       break;
        case RXKADILLEGALLEVEL : s = "RXKADILLEGALLEVEL";  break;
        }
        osi_Log2(afsd_logp, "cm_Analyze: rxkad error code 0x%x (%s)",
                  errorCode, s);

        if (serverp) {
            reqp->errorServp = serverp;
            reqp->tokenError = errorCode;
            retry = 1;
        }
    } else if (errorCode >= ERROR_TABLE_BASE_U && errorCode < ERROR_TABLE_BASE_U + 256) {
        /*
         * We received a ubik error.  its possible that the server we are
         * communicating with has a corrupted database or is partitioned
         * from the rest of the servers and another server might be able
         * to answer our query.  Therefore, we will retry the request
         * and force the use of another server.
         */
        if (serverp) {
            reqp->errorServp = serverp;
            reqp->tokenError = errorCode;
            retry = 1;
        }
    } else if (errorCode == VICECONNBAD || errorCode == VICETOKENDEAD) {
	cm_ForceNewConnections(serverp);
        if ( timeLeft > 2 )
            retry = 1;
    } else {
        if (errorCode) {
            char * s = "unknown error";
            switch ( errorCode ) {
            case VSALVAGE          : s = "VSALVAGE";           break;
            case VNOVNODE          : s = "VNOVNODE";           break;
            case VNOVOL            : s = "VNOVOL";             break;
            case VVOLEXISTS        : s = "VVOLEXISTS";         break;
            case VNOSERVICE        : s = "VNOSERVICE";         break;
            case VOFFLINE          : s = "VOFFLINE";           break;
            case VONLINE           : s = "VONLINE";            break;
            case VDISKFULL         : s = "VDISKFULL";          break;
            case VOVERQUOTA        : s = "VOVERQUOTA";         break;
            case VBUSY             : s = "VBUSY";              break;
            case VMOVED            : s = "VMOVED";             break;
            case VIO               : s = "VIO";                break;
            case VRESTRICTED       : s = "VRESTRICTED";        break;
            case VRESTARTING       : s = "VRESTARTING";        break;
            case VREADONLY         : s = "VREADONLY";          break;
            case EAGAIN            : s = "EAGAIN";             break;
	    case UAEAGAIN          : s = "UAEAGAIN";	       break;
            case EINVAL            : s = "EINVAL";             break;
	    case UAEINVAL          : s = "UAEINVAL";	       break;
            case EACCES            : s = "EACCES";             break;
	    case UAEACCES 	   : s = "UAEACCES";           break;
	    case ENOENT            : s = "ENOENT"; 	       break;
	    case UAENOENT          : s = "UAENOENT";           break;
            case EEXIST            : s = "EEXIST";             break;
            case UAEEXIST          : s = "UAEEXIST";           break;
	    case VICECONNBAD	   : s = "VICECONNBAD";	       break;
	    case VICETOKENDEAD     : s = "VICETOKENDEAD";      break;
            case WSAEWOULDBLOCK    : s = "WSAEWOULDBLOCK";     break;
            case UAEWOULDBLOCK     : s = "UAEWOULDBLOCK";      break;
            case VL_IDEXIST        : s = "VL_IDEXIST";         break;
            case VL_IO             : s = "VL_IO";              break;
            case VL_NAMEEXIST      : s = "VL_NAMEEXIST";       break;
            case VL_CREATEFAIL     : s = "VL_CREATEFAIL";      break;
            case VL_NOENT          : s = "VL_NOENT";           break;
            case VL_EMPTY          : s = "VL_EMPTY";           break;
            case VL_ENTDELETED     : s = "VL_ENTDELETED";      break;
            case VL_BADNAME        : s = "VL_BADNAME";         break;
            case VL_BADINDEX       : s = "VL_BADINDEX";        break;
            case VL_BADVOLTYPE     : s = "VL_BADVOLTYPE";      break;
            case VL_BADSERVER      : s = "VL_BADSERVER";       break;
            case VL_BADPARTITION   : s = "VL_BADPARTITION";    break;
            case VL_REPSFULL       : s = "VL_REPSFULL";        break;
            case VL_NOREPSERVER    : s = "VL_NOREPSERVER";     break;
            case VL_DUPREPSERVER   : s = "VL_DUPREPSERVER";    break;
            case VL_RWNOTFOUND     : s = "VL_RWNOTFOUND";      break;
            case VL_BADREFCOUNT    : s = "VL_BADREFCOUNT";     break;
            case VL_SIZEEXCEEDED   : s = "VL_SIZEEXCEEDED";    break;
            case VL_BADENTRY       : s = "VL_BADENTRY";        break;
            case VL_BADVOLIDBUMP   : s = "VL_BADVOLIDBUMP";    break;
            case VL_IDALREADYHASHED: s = "VL_IDALREADYHASHED"; break;
            case VL_ENTRYLOCKED    : s = "VL_ENTRYLOCKED";     break;
            case VL_BADVOLOPER     : s = "VL_BADVOLOPER";      break;
            case VL_BADRELLOCKTYPE : s = "VL_BADRELLOCKTYPE";  break;
            case VL_RERELEASE      : s = "VL_RERELEASE";       break;
            case VL_BADSERVERFLAG  : s = "VL_BADSERVERFLAG";   break;
            case VL_PERM           : s = "VL_PERM";            break;
            case VL_NOMEM          : s = "VL_NOMEM";           break;
            case VL_BADVERSION     : s = "VL_BADVERSION";      break;
            case VL_INDEXERANGE    : s = "VL_INDEXERANGE";     break;
            case VL_MULTIPADDR     : s = "VL_MULTIPADDR";      break;
            case VL_BADMASK        : s = "VL_BADMASK";         break;
	    case CM_ERROR_NOSUCHCELL	    : s = "CM_ERROR_NOSUCHCELL";         break;
	    case CM_ERROR_NOSUCHVOLUME	    : s = "CM_ERROR_NOSUCHVOLUME";       break;
	    case CM_ERROR_TIMEDOUT	    : s = "CM_ERROR_TIMEDOUT";           break;
	    case CM_ERROR_RETRY		    : s = "CM_ERROR_RETRY";              break;
	    case CM_ERROR_NOACCESS	    : s = "CM_ERROR_NOACCESS";           break;
	    case CM_ERROR_NOSUCHFILE	    : s = "CM_ERROR_NOSUCHFILE";         break;
	    case CM_ERROR_STOPNOW	    : s = "CM_ERROR_STOPNOW";            break;
	    case CM_ERROR_TOOBIG	    : s = "CM_ERROR_TOOBIG";             break;
	    case CM_ERROR_INVAL		    : s = "CM_ERROR_INVAL";              break;
	    case CM_ERROR_BADFD		    : s = "CM_ERROR_BADFD";              break;
	    case CM_ERROR_BADFDOP	    : s = "CM_ERROR_BADFDOP";            break;
	    case CM_ERROR_EXISTS	    : s = "CM_ERROR_EXISTS";             break;
	    case CM_ERROR_CROSSDEVLINK	    : s = "CM_ERROR_CROSSDEVLINK";       break;
	    case CM_ERROR_BADOP		    : s = "CM_ERROR_BADOP";              break;
	    case CM_ERROR_BADPASSWORD       : s = "CM_ERROR_BADPASSWORD";        break;
	    case CM_ERROR_NOTDIR	    : s = "CM_ERROR_NOTDIR";             break;
	    case CM_ERROR_ISDIR		    : s = "CM_ERROR_ISDIR";              break;
	    case CM_ERROR_READONLY	    : s = "CM_ERROR_READONLY";           break;
	    case CM_ERROR_WOULDBLOCK	    : s = "CM_ERROR_WOULDBLOCK";         break;
	    case CM_ERROR_QUOTA		    : s = "CM_ERROR_QUOTA";              break;
	    case CM_ERROR_SPACE		    : s = "CM_ERROR_SPACE";              break;
	    case CM_ERROR_BADSHARENAME	    : s = "CM_ERROR_BADSHARENAME";       break;
	    case CM_ERROR_BADTID	    : s = "CM_ERROR_BADTID";             break;
	    case CM_ERROR_UNKNOWN	    : s = "CM_ERROR_UNKNOWN";            break;
	    case CM_ERROR_NOMORETOKENS	    : s = "CM_ERROR_NOMORETOKENS";       break;
	    case CM_ERROR_NOTEMPTY	    : s = "CM_ERROR_NOTEMPTY";           break;
	    case CM_ERROR_USESTD	    : s = "CM_ERROR_USESTD";             break;
	    case CM_ERROR_REMOTECONN	    : s = "CM_ERROR_REMOTECONN";         break;
	    case CM_ERROR_ATSYS		    : s = "CM_ERROR_ATSYS";              break;
	    case CM_ERROR_NOSUCHPATH	    : s = "CM_ERROR_NOSUCHPATH";         break;
	    case CM_ERROR_CLOCKSKEW	    : s = "CM_ERROR_CLOCKSKEW";          break;
	    case CM_ERROR_BADSMB	    : s = "CM_ERROR_BADSMB";             break;
	    case CM_ERROR_ALLBUSY	    : s = "CM_ERROR_ALLBUSY";            break;
	    case CM_ERROR_NOFILES	    : s = "CM_ERROR_NOFILES";            break;
	    case CM_ERROR_PARTIALWRITE	    : s = "CM_ERROR_PARTIALWRITE";       break;
	    case CM_ERROR_NOIPC		    : s = "CM_ERROR_NOIPC";              break;
	    case CM_ERROR_BADNTFILENAME	    : s = "CM_ERROR_BADNTFILENAME";      break;
	    case CM_ERROR_BUFFERTOOSMALL    : s = "CM_ERROR_BUFFERTOOSMALL";     break;
	    case CM_ERROR_RENAME_IDENTICAL  : s = "CM_ERROR_RENAME_IDENTICAL";   break;
	    case CM_ERROR_ALLOFFLINE        : s = "CM_ERROR_ALLOFFLINE";         break;
	    case CM_ERROR_AMBIGUOUS_FILENAME: s = "CM_ERROR_AMBIGUOUS_FILENAME"; break;
	    case CM_ERROR_BADLOGONTYPE	    : s = "CM_ERROR_BADLOGONTYPE";       break;
	    case CM_ERROR_GSSCONTINUE       : s = "CM_ERROR_GSSCONTINUE";        break;
	    case CM_ERROR_TIDIPC            : s = "CM_ERROR_TIDIPC";             break;
	    case CM_ERROR_TOO_MANY_SYMLINKS : s = "CM_ERROR_TOO_MANY_SYMLINKS";  break;
	    case CM_ERROR_PATH_NOT_COVERED  : s = "CM_ERROR_PATH_NOT_COVERED";   break;
	    case CM_ERROR_LOCK_CONFLICT     : s = "CM_ERROR_LOCK_CONFLICT";      break;
	    case CM_ERROR_SHARING_VIOLATION : s = "CM_ERROR_SHARING_VIOLATION";  break;
	    case CM_ERROR_ALLDOWN           : s = "CM_ERROR_ALLDOWN";            break;
	    case CM_ERROR_TOOFEWBUFS	    : s = "CM_ERROR_TOOFEWBUFS";         break;
	    case CM_ERROR_TOOMANYBUFS	    : s = "CM_ERROR_TOOMANYBUFS";        break;
            }
            osi_Log2(afsd_logp, "cm_Analyze: ignoring error code 0x%x (%s)",
                     errorCode, s);
            retry = 0;
        }
    }

    /* If not allowed to retry, don't */
    if (!forcing_new && (reqp->flags & CM_REQ_NORETRY) &&
        (errorCode != RX_MSGSIZE && errorCode != RX_CALL_BUSY))
	retry = 0;
    else if (retry && dead_session)
        retry = 0;

    /* drop this on the way out */
    if (connp)
        cm_PutConn(connp);

    /*
     * clear the volume updated flag if we succeed.
     * this way the flag will not prevent a subsequent volume
     * from being updated if necessary.
     */
    if (errorCode == 0)
    {
        reqp->flags &= ~CM_REQ_VOLUME_UPDATED;
    }

    if ( serversp &&
         errorCode != VBUSY &&
         errorCode != VRESTARTING &&
         errorCode != CM_ERROR_ALLBUSY)
    {
        cm_ResetServerBusyStatus(serversp);
    }

    /* retry until we fail to find a connection */
    return retry;
}

long cm_ConnByMServers(cm_serverRef_t *serversp, afs_uint32 replicated, cm_user_t *usersp,
                       cm_req_t *reqp, cm_conn_t **connpp)
{
    long code;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    long firstError = 0;
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;
#ifdef SET_RX_TIMEOUTS_TO_TIMELEFT
    long timeUsed, timeLeft, hardTimeLeft;
#endif
    *connpp = NULL;

    if (serversp == NULL) {
	osi_Log1(afsd_logp, "cm_ConnByMServers returning 0x%x", CM_ERROR_ALLDOWN);
	return CM_ERROR_ALLDOWN;
    }

#ifdef SET_RX_TIMEOUTS_TO_TIMELEFT
    timeUsed = (GetTickCount() - reqp->startTime) / 1000;

    /* leave 5 seconds margin of safety */
    timeLeft =  ConnDeadtimeout - timeUsed - 5;
    hardTimeLeft = HardDeadtimeout - timeUsed - 5;
#endif

    lock_ObtainRead(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        if (tsrp->status == srv_deleted)
            continue;

        tsp = tsrp->server;
        if (reqp->errorServp) {
            /*
             * search the list until we find the server
             * that failed last time.  When we find it
             * clear the error, skip it and try the one
             * in the list.
             */
            if (tsp == reqp->errorServp)
                reqp->errorServp = NULL;
            continue;
        }
        if (tsp) {
            cm_GetServerNoLock(tsp);
            lock_ReleaseRead(&cm_serverLock);
            if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                allDown = 0;
                if (tsrp->status == srv_busy) {
                    allOffline = 0;
                    someBusy = 1;
                } else if (tsrp->status == srv_offline) {
                    allBusy = 0;
                    someOffline = 1;
                } else {
                    allOffline = 0;
                    allBusy = 0;
                    code = cm_ConnByServer(tsp, usersp, replicated, connpp);
                    if (code == 0) {        /* cm_CBS only returns 0 */
                        cm_PutServer(tsp);
#ifdef SET_RX_TIMEOUTS_TO_TIMELEFT
                        /* Set RPC timeout */
                        if (timeLeft > ConnDeadtimeout)
                            timeLeft = ConnDeadtimeout;

                        if (hardTimeLeft > HardDeadtimeout)
                            hardTimeLeft = HardDeadtimeout;

                        lock_ObtainMutex(&(*connpp)->mx);
                        rx_SetConnDeadTime((*connpp)->rxconnp, timeLeft);
                        rx_SetConnHardDeadTime((*connpp)->rxconnp, (u_short) hardTimeLeft);
                        lock_ReleaseMutex(&(*connpp)->mx);
#endif
                        return 0;
                    }

                    /* therefore, this code is never executed */
                    if (firstError == 0)
                        firstError = code;
                }
            }
            lock_ObtainRead(&cm_serverLock);
            cm_PutServerNoLock(tsp);
        }
    }
    lock_ReleaseRead(&cm_serverLock);

    if (firstError == 0) {
        if (allDown) {
            firstError = (reqp->tokenError ? reqp->tokenError :
                          (reqp->idleError ? RX_CALL_TIMEOUT : CM_ERROR_ALLDOWN));
            /*
             * if we experienced either a token error or and idle dead time error
             * and now all of the servers are down, we have either tried them
             * all or lost connectivity.  Clear the error we are returning so
             * we will not return it indefinitely if the request is retried.
             */
            reqp->idleError = reqp->tokenError = 0;
        } else if (allBusy) {
            firstError = CM_ERROR_ALLBUSY;
	} else if (allOffline || (someBusy && someOffline)) {
	    firstError = CM_ERROR_ALLOFFLINE;
        } else {
            osi_Log0(afsd_logp, "cm_ConnByMServers returning impossible error TIMEDOUT");
            firstError = CM_ERROR_TIMEDOUT;
        }
    }

    osi_Log1(afsd_logp, "cm_ConnByMServers returning 0x%x", firstError);
    return firstError;
}

/* called with a held server to GC all bad connections hanging off of the server */
void cm_GCConnections(cm_server_t *serverp)
{
    cm_conn_t *tcp;
    cm_conn_t **lcpp;
    cm_user_t *userp;

    lock_ObtainWrite(&cm_connLock);
    lcpp = &serverp->connsp;
    for (tcp = *lcpp; tcp; tcp = *lcpp) {
        userp = tcp->userp;
        if (userp && tcp->refCount == 0 && (userp->vcRefs == 0)) {
            /* do the deletion of this guy */
            cm_PutServer(tcp->serverp);
            cm_ReleaseUser(userp);
            *lcpp = tcp->nextp;
            rx_SetConnSecondsUntilNatPing(tcp->rxconnp, 0);
            rx_DestroyConnection(tcp->rxconnp);
            lock_FinalizeMutex(&tcp->mx);
            free(tcp);
        }
        else {
            /* just advance to the next */
            lcpp = &tcp->nextp;
        }
    }
    lock_ReleaseWrite(&cm_connLock);
}

static void cm_NewRXConnection(cm_conn_t *tcp, cm_ucell_t *ucellp,
                               cm_server_t *serverp, afs_uint32 replicated)
{
    unsigned short port;
    int serviceID;
    int secIndex;
    struct rx_securityClass *secObjp;

    port = serverp->addr.sin_port;
    switch (serverp->type) {
    case CM_SERVER_VLDB:
        if (port == 0)
            port = htons(7003);
        serviceID = 52;
        break;
    case CM_SERVER_FILE:
        if (port == 0)
            port = htons(7000);
        serviceID = 1;
        break;
    default:
        osi_panic("unknown server type", __FILE__, __LINE__);
    }

    if (ucellp->flags & CM_UCELLFLAG_RXKAD) {
        secIndex = 2;
        switch (cryptall) {
        case 0:
            tcp->cryptlevel = rxkad_clear;
            break;
        case 2:
            tcp->cryptlevel = rxkad_auth;
            break;
        default:
            tcp->cryptlevel = rxkad_crypt;
        }
        secObjp = rxkad_NewClientSecurityObject(tcp->cryptlevel,
                                                &ucellp->sessionKey, ucellp->kvno,
                                                ucellp->ticketLen, ucellp->ticketp);
    } else {
        /* normal auth */
        secIndex = 0;
        tcp->cryptlevel = rxkad_clear;
        secObjp = rxnull_NewClientSecurityObject();
    }
    osi_assertx(secObjp != NULL, "null rx_securityClass");
    tcp->rxconnp = rx_NewConnection(serverp->addr.sin_addr.s_addr,
                                    port,
                                    serviceID,
                                    secObjp,
                                    secIndex);
    rx_SetConnDeadTime(tcp->rxconnp, ConnDeadtimeout);
    rx_SetConnHardDeadTime(tcp->rxconnp, HardDeadtimeout);

    /*
     * Setting idle dead timeout to a non-zero value activates RX_CALL_IDLE errors
     */
    if (replicated) {
        tcp->flags &= CM_CONN_FLAG_REPLICATION;
        rx_SetConnIdleDeadTime(tcp->rxconnp, ReplicaIdleDeadtimeout);
    } else {
        rx_SetConnIdleDeadTime(tcp->rxconnp, IdleDeadtimeout);
    }

    /*
     * Let the Rx library know that we can auto-retry if an
     * RX_MSGSIZE error is returned.
     */
    if (rx_pmtu_discovery)
        rx_SetMsgsizeRetryErr(tcp->rxconnp, RX_MSGSIZE);

    /*
     * Attempt to limit NAT pings to the anonymous file server connections.
     * Only file servers implement client callbacks and we only need one ping
     * to be sent to each server.
     */
    if (NatPingInterval && serverp->type == CM_SERVER_FILE &&
         (ucellp->flags & CM_UCELLFLAG_ROOTUSER)) {
        rx_SetConnSecondsUntilNatPing(tcp->rxconnp, NatPingInterval);
    }

    tcp->ucgen = ucellp->gen;
    if (secObjp)
        rxs_Release(secObjp);   /* Decrement the initial refCount */
}

long cm_ConnByServer(cm_server_t *serverp, cm_user_t *userp, afs_uint32 replicated, cm_conn_t **connpp)
{
    cm_conn_t *tcp;
    cm_ucell_t *ucellp;

    *connpp = NULL;

    if (cm_anonvldb && serverp->type == CM_SERVER_VLDB)
        userp = cm_rootUserp;

    lock_ObtainMutex(&userp->mx);
    lock_ObtainRead(&cm_connLock);
    for (tcp = serverp->connsp; tcp; tcp=tcp->nextp) {
        if (tcp->userp == userp &&
            (replicated && (tcp->flags & CM_CONN_FLAG_REPLICATION) ||
             !replicated && !(tcp->flags & CM_CONN_FLAG_REPLICATION)))
            break;
    }

    /* find ucell structure */
    ucellp = cm_GetUCell(userp, serverp->cellp);
    if (!tcp) {
        lock_ConvertRToW(&cm_connLock);
        for (tcp = serverp->connsp; tcp; tcp=tcp->nextp) {
            if (tcp->userp == userp)
                break;
        }
        if (tcp) {
            lock_ReleaseWrite(&cm_connLock);
            goto haveconn;
        }
        cm_GetServer(serverp);
        tcp = malloc(sizeof(*tcp));
        memset(tcp, 0, sizeof(*tcp));
        cm_HoldUser(userp);
        tcp->userp = userp;
        lock_InitializeMutex(&tcp->mx, "cm_conn_t mutex", LOCK_HIERARCHY_CONN);
        tcp->serverp = serverp;
        tcp->cryptlevel = rxkad_clear;
        cm_NewRXConnection(tcp, ucellp, serverp, replicated);
        tcp->refCount = 1;
        tcp->nextp = serverp->connsp;
        serverp->connsp = tcp;
        lock_ReleaseWrite(&cm_connLock);
        lock_ReleaseMutex(&userp->mx);
    } else {
        lock_ReleaseRead(&cm_connLock);
      haveconn:
        lock_ReleaseMutex(&userp->mx);
        InterlockedIncrement(&tcp->refCount);

        lock_ObtainMutex(&tcp->mx);
        if ((tcp->flags & CM_CONN_FLAG_FORCE_NEW) ||
            (tcp->ucgen < ucellp->gen) ||
            (tcp->cryptlevel != (ucellp->flags & CM_UCELLFLAG_RXKAD ? (cryptall == 1 ? rxkad_crypt : (cryptall == 2 ? rxkad_auth : rxkad_clear)) : rxkad_clear)))
        {
            if (tcp->ucgen < ucellp->gen)
                osi_Log0(afsd_logp, "cm_ConnByServer replace connection due to token update");
            else
                osi_Log0(afsd_logp, "cm_ConnByServer replace connection due to crypt change");
            tcp->flags &= ~CM_CONN_FLAG_FORCE_NEW;
            rx_SetConnSecondsUntilNatPing(tcp->rxconnp, 0);
            rx_DestroyConnection(tcp->rxconnp);
            cm_NewRXConnection(tcp, ucellp, serverp, replicated);
        }
        lock_ReleaseMutex(&tcp->mx);
    }

    /* return this pointer to our caller */
    osi_Log1(afsd_logp, "cm_ConnByServer returning conn 0x%p", tcp);
    *connpp = tcp;

    return 0;
}

long cm_ServerAvailable(struct cm_fid *fidp, struct cm_user *userp)
{
    long code;
    cm_req_t req;
    cm_serverRef_t **serverspp;
    cm_serverRef_t *tsrp;
    cm_server_t *tsp;
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;
    afs_uint32 replicated;

    cm_InitReq(&req);

    code = cm_GetServerList(fidp, userp, &req, &replicated, &serverspp);
    if (code)
        return 0;

    lock_ObtainRead(&cm_serverLock);
    for (tsrp = *serverspp; tsrp; tsrp=tsrp->next) {
        if (tsrp->status == srv_deleted)
            continue;
        tsp = tsrp->server;
        if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
	    allDown = 0;
            if (tsrp->status == srv_busy) {
		allOffline = 0;
                someBusy = 1;
            } else if (tsrp->status == srv_offline) {
		allBusy = 0;
		someOffline = 1;
            } else {
		allOffline = 0;
                allBusy = 0;
            }
        }
    }
    lock_ReleaseRead(&cm_serverLock);
    cm_FreeServerList(serverspp, 0);

    if (allDown)
	return 0;
    else if (allBusy)
	return 0;
    else if (allOffline || (someBusy && someOffline))
	return 0;
    else
	return 1;
}

/*
 * The returned cm_conn_t ** object is released in the subsequent call
 * to cm_Analyze().
 */
long cm_ConnFromFID(struct cm_fid *fidp, struct cm_user *userp, cm_req_t *reqp,
                    cm_conn_t **connpp)
{
    long code;
    cm_serverRef_t **serverspp;
    afs_uint32 replicated;

    *connpp = NULL;

    code = cm_GetServerList(fidp, userp, reqp, &replicated, &serverspp);
    if (code)
        return code;

    code = cm_ConnByMServers(*serverspp, replicated, userp, reqp, connpp);
    cm_FreeServerList(serverspp, 0);
    return code;
}


long cm_ConnFromVolume(struct cm_volume *volp, unsigned long volid, struct cm_user *userp, cm_req_t *reqp,
                       cm_conn_t **connpp)
{
    long code;
    cm_serverRef_t **serverspp;
    afs_uint32 replicated;
    cm_vol_state_t * volstatep;

    *connpp = NULL;

    volstatep = cm_VolumeStateByID(volp, volid);
    replicated = (volstatep->flags & CM_VOL_STATE_FLAG_REPLICATED);
    serverspp = cm_GetVolServers(volp, volid, userp, reqp);

    code = cm_ConnByMServers(*serverspp, replicated, userp, reqp, connpp);
    cm_FreeServerList(serverspp, 0);
    return code;
}


extern struct rx_connection *
cm_GetRxConn(cm_conn_t *connp)
{
    struct rx_connection * rxconnp;
    lock_ObtainMutex(&connp->mx);
    rxconnp = connp->rxconnp;
    rx_GetConnection(rxconnp);
    lock_ReleaseMutex(&connp->mx);
    return rxconnp;
}

void cm_ForceNewConnections(cm_server_t *serverp)
{
    cm_conn_t *tcp;

    lock_ObtainWrite(&cm_connLock);
    for (tcp = serverp->connsp; tcp; tcp=tcp->nextp) {
	lock_ObtainMutex(&tcp->mx);
	tcp->flags |= CM_CONN_FLAG_FORCE_NEW;
	lock_ReleaseMutex(&tcp->mx);
    }
    lock_ReleaseWrite(&cm_connLock);
}
