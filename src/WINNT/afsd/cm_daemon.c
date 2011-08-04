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
#include <iphlpapi.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "afsd.h"
#include "smb.h"

#include <rx/rx.h>
#include <rx/rx_prototypes.h>
#include <WINNT/afsreg.h>

#include "afsicf.h"

/* in seconds */
long cm_daemonCheckDownInterval  = 180;
long cm_daemonCheckUpInterval    = 240;
long cm_daemonCheckVolInterval   = 3600;
long cm_daemonCheckCBInterval    = 60;
long cm_daemonCheckVolCBInterval = 0;
long cm_daemonCheckLockInterval  = 60;
long cm_daemonTokenCheckInterval = 180;
long cm_daemonCheckOfflineVolInterval = 600;
long cm_daemonPerformanceTuningInterval = 0;
long cm_daemonRankServerInterval = 600;

osi_rwlock_t cm_daemonLock;

long cm_bkgQueueCount;		/* # of queued requests */

int cm_bkgWaitingForCount;	/* true if someone's waiting for cm_bkgQueueCount to drop */

cm_bkgRequest_t *cm_bkgListp;		/* first elt in the list of requests */
cm_bkgRequest_t *cm_bkgListEndp;	/* last elt in the list of requests */

extern int powerStateSuspended;
int daemon_ShutdownFlag = 0;
static int cm_nDaemons = 0;
static time_t lastIPAddrChange = 0;

static EVENT_HANDLE cm_Daemon_ShutdownEvent = NULL;
static EVENT_HANDLE cm_IPAddrDaemon_ShutdownEvent = NULL;
static EVENT_HANDLE cm_BkgDaemon_ShutdownEvent[CM_MAX_DAEMONS] =
       {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

void cm_IpAddrDaemon(long parm)
{
    extern void smb_CheckVCs(void);
    char * name = "cm_IPAddrDaemon_ShutdownEvent";

    cm_IPAddrDaemon_ShutdownEvent = thrd_CreateEvent(NULL, FALSE, FALSE, name);
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", name);

    rx_StartClientThread();

    while (daemon_ShutdownFlag == 0) {
	DWORD Result;

        thrd_SetEvent(cm_IPAddrDaemon_ShutdownEvent);
        Result = NotifyAddrChange(NULL,NULL);
        if (Result == NO_ERROR && daemon_ShutdownFlag == 0) {
            lastIPAddrChange = osi_Time();
            smb_SetLanAdapterChangeDetected();
            cm_SetLanAdapterChangeDetected();
            thrd_ResetEvent(cm_IPAddrDaemon_ShutdownEvent);
	}
    }

    thrd_SetEvent(cm_IPAddrDaemon_ShutdownEvent);
}

void cm_BkgDaemon(void * parm)
{
    cm_bkgRequest_t *rp;
    afs_int32 code;
    char name[32] = "";
    long daemonID = (long)(LONG_PTR)parm;

    snprintf(name, sizeof(name), "cm_BkgDaemon_ShutdownEvent%u", daemonID);

    cm_BkgDaemon_ShutdownEvent[daemonID] = thrd_CreateEvent(NULL, FALSE, FALSE, name);
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", name);

    rx_StartClientThread();

    lock_ObtainWrite(&cm_daemonLock);
    while (daemon_ShutdownFlag == 0) {
        if (powerStateSuspended) {
            Sleep(1000);
            continue;
        }
        if (!cm_bkgListEndp) {
            osi_SleepW((LONG_PTR)&cm_bkgListp, &cm_daemonLock);
            lock_ObtainWrite(&cm_daemonLock);
            continue;
        }

        /* we found a request */
        for (rp = cm_bkgListEndp; rp; rp = (cm_bkgRequest_t *) osi_QPrev(&rp->q))
	{
	    if (cm_ServerAvailable(&rp->scp->fid, rp->userp) ||
                rp->scp->flags & CM_SCACHEFLAG_DELETED)
		break;
	}
	if (rp == NULL) {
	    /* we couldn't find a request that we could process at the current time */
	    lock_ReleaseWrite(&cm_daemonLock);
	    Sleep(1000);
	    lock_ObtainWrite(&cm_daemonLock);
	    continue;
	}

        osi_QRemoveHT((osi_queue_t **) &cm_bkgListp, (osi_queue_t **) &cm_bkgListEndp, &rp->q);
        osi_assertx(cm_bkgQueueCount-- > 0, "cm_bkgQueueCount 0");
        lock_ReleaseWrite(&cm_daemonLock);

	osi_Log1(afsd_logp,"cm_BkgDaemon processing request 0x%p", rp);

        if (rp->scp->flags & CM_SCACHEFLAG_DELETED) {
            osi_Log1(afsd_logp,"cm_BkgDaemon DELETED scp 0x%x",rp->scp);
            code = CM_ERROR_BADFD;
        } else {
#ifdef DEBUG_REFCOUNT
            osi_Log2(afsd_logp,"cm_BkgDaemon (before) scp 0x%x ref %d",rp->scp, rp->scp->refCount);
#endif
            code = (*rp->procp)(rp->scp, rp->p1, rp->p2, rp->p3, rp->p4, rp->userp, &rp->req);
#ifdef DEBUG_REFCOUNT
            osi_Log2(afsd_logp,"cm_BkgDaemon (after) scp 0x%x ref %d",rp->scp, rp->scp->refCount);
#endif
        }

        /*
         * Keep the following list synchronized with the
         * error code list in cm_BkgStore.
         * cm_SyncOpDone(CM_SCACHESYNC_ASYNCSTORE) will be called there unless
         * one of these errors has occurred.
         */
        switch ( code ) {
        case CM_ERROR_TIMEDOUT:	/* or server restarting */
        case CM_ERROR_RETRY:
        case CM_ERROR_WOULDBLOCK:
        case CM_ERROR_ALLBUSY:
        case CM_ERROR_ALLDOWN:
        case CM_ERROR_ALLOFFLINE:
        case CM_ERROR_PARTIALWRITE:
            if (rp->procp == cm_BkgStore) {
                osi_Log2(afsd_logp,
                         "cm_BkgDaemon re-queueing failed request 0x%p code 0x%x",
                         rp, code);
                lock_ObtainWrite(&cm_daemonLock);
                cm_bkgQueueCount++;
                osi_QAddT((osi_queue_t **) &cm_bkgListp, (osi_queue_t **)&cm_bkgListEndp, &rp->q);
                break;
            } /* otherwise fall through */
        case 0:  /* success */
        default: /* other error */
            if (code == 0) {
                osi_Log1(afsd_logp,"cm_BkgDaemon SUCCESS: request 0x%p", rp);
            } else {
                osi_Log2(afsd_logp,"cm_BkgDaemon FAILED: request dropped 0x%p code 0x%x",
                         rp, code);
            }
            cm_ReleaseUser(rp->userp);
            cm_ReleaseSCache(rp->scp);
            free(rp);
            lock_ObtainWrite(&cm_daemonLock);
        }
    }
    lock_ReleaseWrite(&cm_daemonLock);
    thrd_SetEvent(cm_BkgDaemon_ShutdownEvent[daemonID]);
}

void cm_QueueBKGRequest(cm_scache_t *scp, cm_bkgProc_t *procp, afs_uint32 p1, afs_uint32 p2, afs_uint32 p3, afs_uint32 p4,
	cm_user_t *userp, cm_req_t *reqp)
{
    cm_bkgRequest_t *rp;

    rp = malloc(sizeof(*rp));
    memset(rp, 0, sizeof(*rp));

    cm_HoldSCache(scp);
    rp->scp = scp;
    cm_HoldUser(userp);
    rp->userp = userp;
    rp->procp = procp;
    rp->p1 = p1;
    rp->p2 = p2;
    rp->p3 = p3;
    rp->p4 = p4;
    rp->req = *reqp;

    lock_ObtainWrite(&cm_daemonLock);
    cm_bkgQueueCount++;
    osi_QAdd((osi_queue_t **) &cm_bkgListp, &rp->q);
    if (!cm_bkgListEndp)
        cm_bkgListEndp = rp;
    lock_ReleaseWrite(&cm_daemonLock);

    osi_Wakeup((LONG_PTR) &cm_bkgListp);
}

static int
IsWindowsFirewallPresent(void)
{
    SC_HANDLE scm;
    SC_HANDLE svc;
    BOOLEAN flag;
    BOOLEAN result = FALSE;
    LPQUERY_SERVICE_CONFIG pConfig = NULL;
    DWORD BufSize;
    LONG status;

    /* Open services manager */
    scm = OpenSCManager(NULL, NULL, GENERIC_READ);
    if (!scm) return FALSE;

    /* Open Windows Firewall service */
    svc = OpenService(scm, "MpsSvc", SERVICE_QUERY_CONFIG);
    if (!svc) {
	afsi_log("MpsSvc Service could not be opened for query: 0x%x", GetLastError());
	svc = OpenService(scm, "SharedAccess", SERVICE_QUERY_CONFIG);
	if (!svc)
	    afsi_log("SharedAccess Service could not be opened for query: 0x%x", GetLastError());
    }
    if (!svc)
        goto close_scm;

    /* Query Windows Firewall service config, first just to get buffer size */
    /* Expected to fail, so don't test return value */
    (void) QueryServiceConfig(svc, NULL, 0, &BufSize);
    status = GetLastError();
    if (status != ERROR_INSUFFICIENT_BUFFER)
        goto close_svc;

    /* Allocate buffer */
    pConfig = (LPQUERY_SERVICE_CONFIG)GlobalAlloc(GMEM_FIXED,BufSize);
    if (!pConfig)
        goto close_svc;

    /* Query Windows Firewall service config, this time for real */
    flag = QueryServiceConfig(svc, pConfig, BufSize, &BufSize);
    if (!flag) {
	afsi_log("QueryServiceConfig failed: 0x%x", GetLastError());
        goto free_pConfig;
    }

    /* Is it autostart? */
    afsi_log("AutoStart 0x%x", pConfig->dwStartType);
    if (pConfig->dwStartType < SERVICE_DEMAND_START)
        result = TRUE;

  free_pConfig:
    GlobalFree(pConfig);
  close_svc:
    CloseServiceHandle(svc);
  close_scm:
    CloseServiceHandle(scm);

    return result;
}

void
cm_DaemonCheckInit(void)
{
    HKEY parmKey;
    DWORD dummyLen;
    DWORD dummy;
    DWORD code;

    code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
                         0, KEY_QUERY_VALUE, &parmKey);
    if (code)
	return;

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckDownInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckDownInterval = dummy;
    afsi_log("daemonCheckDownInterval is %d", cm_daemonCheckDownInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckUpInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckUpInterval = dummy;
    afsi_log("daemonCheckUpInterval is %d", cm_daemonCheckUpInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckVolInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckVolInterval = dummy;
    afsi_log("daemonCheckVolInterval is %d", cm_daemonCheckVolInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckCBInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckCBInterval = dummy;
    afsi_log("daemonCheckCBInterval is %d", cm_daemonCheckCBInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckVolCBInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckVolCBInterval = dummy;
    afsi_log("daemonCheckVolCBInterval is %d", cm_daemonCheckVolCBInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckLockInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckLockInterval = dummy;
    afsi_log("daemonCheckLockInterval is %d", cm_daemonCheckLockInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckTokenInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonTokenCheckInterval = dummy;
    afsi_log("daemonCheckTokenInterval is %d", cm_daemonTokenCheckInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonCheckOfflineVolInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonCheckOfflineVolInterval = dummy;
    afsi_log("daemonCheckOfflineVolInterval is %d", cm_daemonCheckOfflineVolInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonPerformanceTuningInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS)
	cm_daemonPerformanceTuningInterval = dummy;
    afsi_log("daemonPerformanceTuningInterval is %d", cm_daemonPerformanceTuningInterval);

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonRankServerInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonRankServerInterval = dummy;
    afsi_log("daemonRankServerInterval is %d", cm_daemonRankServerInterval);

    RegCloseKey(parmKey);

    if (cm_daemonPerformanceTuningInterval)
        cm_PerformanceTuningInit();
}

/* periodic check daemon */
void cm_Daemon(long parm)
{
    time_t now;
    time_t lastLockCheck;
    time_t lastVolCheck;
    time_t lastCBExpirationCheck;
    time_t lastVolCBRenewalCheck;
    time_t lastDownServerCheck;
    time_t lastUpServerCheck;
    time_t lastTokenCacheCheck;
    time_t lastBusyVolCheck;
    time_t lastPerformanceCheck;
    time_t lastServerRankCheck;
    char thostName[200];
    unsigned long code;
    struct hostent *thp;
    HMODULE hHookDll;
    char * name = "cm_Daemon_ShutdownEvent";
    int configureFirewall = IsWindowsFirewallPresent();
    int bAddrChangeCheck = 0;

    cm_Daemon_ShutdownEvent = thrd_CreateEvent(NULL, FALSE, FALSE, name);
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", name);

    if (!configureFirewall) {
	afsi_log("No Windows Firewall detected");
    }

    if (cm_freelanceEnabled && cm_freelanceImportCellServDB)
        cm_FreelanceImportCellServDB();

    /* ping all file servers, up or down, with unauthenticated connection,
     * to find out whether we have all our callbacks from the server still.
     * Also, ping down VLDBs.
     */
    /*
     * Seed the random number generator with our own address, so that
     * clients starting at the same time don't all do vol checks at the
     * same time.
     */
    gethostname(thostName, sizeof(thostName));
    thp = gethostbyname(thostName);
    if (thp == NULL)    /* In djgpp, gethostname returns the netbios
                           name of the machine.  gethostbyname will fail
                           looking this up if it differs from DNS name. */
        code = 0;
    else
        memcpy(&code, thp->h_addr_list[0], 4);

    srand(ntohl(code));

    cm_DaemonCheckInit();

    now = osi_Time();
    lastVolCheck = now - cm_daemonCheckVolInterval/2 + (rand() % cm_daemonCheckVolInterval);
    lastCBExpirationCheck = now - cm_daemonCheckCBInterval/2 + (rand() % cm_daemonCheckCBInterval);
    if (cm_daemonCheckVolCBInterval)
        lastVolCBRenewalCheck = now - cm_daemonCheckVolCBInterval/2 + (rand() % cm_daemonCheckVolCBInterval);
    lastLockCheck = now - cm_daemonCheckLockInterval/2 + (rand() % cm_daemonCheckLockInterval);
    lastDownServerCheck = now - cm_daemonCheckDownInterval/2 + (rand() % cm_daemonCheckDownInterval);
    lastUpServerCheck = now - cm_daemonCheckUpInterval/2 + (rand() % cm_daemonCheckUpInterval);
    lastTokenCacheCheck = now - cm_daemonTokenCheckInterval/2 + (rand() % cm_daemonTokenCheckInterval);
    if (cm_daemonCheckOfflineVolInterval)
        lastBusyVolCheck = now - cm_daemonCheckOfflineVolInterval/2 * (rand() % cm_daemonCheckOfflineVolInterval);
    if (cm_daemonPerformanceTuningInterval)
        lastPerformanceCheck = now - cm_daemonPerformanceTuningInterval/2 * (rand() % cm_daemonPerformanceTuningInterval);
    lastServerRankCheck = now - cm_daemonRankServerInterval/2 * (rand() % cm_daemonRankServerInterval);

    while (daemon_ShutdownFlag == 0) {
        if (powerStateSuspended) {
            Sleep(1000);
            continue;
        }
	/* check to see if the listener threads halted due to network
	 * disconnect or other issues.  If so, attempt to restart them.
	 */
	smb_RestartListeners(0);

        if (daemon_ShutdownFlag == 1)
            break;

        if (configureFirewall) {
	    /* Open Microsoft Firewall to allow in port 7001 */
	    switch (icf_CheckAndAddAFSPorts(AFS_PORTSET_CLIENT)) {
	    case 0:
		afsi_log("Windows Firewall Configuration succeeded");
		configureFirewall = 0;
		break;
	    case 1:
		afsi_log("Invalid Windows Firewall Port Set");
		break;
	    case 2:
		afsi_log("Unable to open Windows Firewall Profile");
		break;
	    case 3:
		afsi_log("Unable to create/modify Windows Firewall Port entries");
		break;
	    default:
		afsi_log("Unknown Windows Firewall Configuration error");
	    }
	}

        /* find out what time it is */
        now = osi_Time();

        /* Determine whether an address change took place that we need to respond to */
        if (bAddrChangeCheck)
            bAddrChangeCheck = 0;

        if (lastIPAddrChange != 0 && lastIPAddrChange + 2500 < now) {
            bAddrChangeCheck = 1;
            lastIPAddrChange = 0;
        }

        /* check down servers */
        if ((bAddrChangeCheck || now > lastDownServerCheck + cm_daemonCheckDownInterval) &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastDownServerCheck = now;
	    osi_Log0(afsd_logp, "cm_Daemon CheckDownServers");
            cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS, NULL);
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (bAddrChangeCheck &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0)
            cm_ForceNewConnectionsAllServers();

        /* check up servers */
        if ((bAddrChangeCheck || now > lastUpServerCheck + cm_daemonCheckUpInterval) &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastUpServerCheck = now;
	    osi_Log0(afsd_logp, "cm_Daemon CheckUpServers");
            cm_CheckServers(CM_FLAG_CHECKUPSERVERS, NULL);
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (bAddrChangeCheck &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            smb_CheckVCs();
            cm_VolStatus_Network_Addr_Change();
        }

        /*
         * Once every five minutes inspect the volume list and enforce
         * the volume location expiration time.
         */
        if (now > lastVolCheck + 300 &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastVolCheck = now;
            cm_RefreshVolumes(cm_daemonCheckVolInterval);
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

	/* Rank all up servers */
	if ((now > lastServerRankCheck + cm_daemonRankServerInterval) &&
	    daemon_ShutdownFlag == 0 &&
	    powerStateSuspended == 0) {
	    lastServerRankCheck = now;
	    osi_Log0(afsd_logp, "cm_Daemon RankServer");
	    cm_RankUpServers();
	    if(daemon_ShutdownFlag == 1)
		break;
	    now = osi_Time();
	}

        if (cm_daemonCheckVolCBInterval &&
            now > lastVolCBRenewalCheck + cm_daemonCheckVolCBInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastVolCBRenewalCheck = now;
            cm_VolumeRenewROCallbacks();
            if (daemon_ShutdownFlag == 1)
                break;
            now = osi_Time();
        }

        if ((bAddrChangeCheck || (cm_daemonCheckOfflineVolInterval &&
                                  now > lastBusyVolCheck + cm_daemonCheckOfflineVolInterval)) &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastBusyVolCheck = now;
            cm_CheckOfflineVolumes();
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (now > lastCBExpirationCheck + cm_daemonCheckCBInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastCBExpirationCheck = now;
            cm_CheckCBExpiration();
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (now > lastLockCheck + cm_daemonCheckLockInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastLockCheck = now;
            cm_CheckLocks();
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (now > lastTokenCacheCheck + cm_daemonTokenCheckInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastTokenCacheCheck = now;
            cm_CheckTokenCache(now);
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        /* allow an exit to be called prior to stopping the service */
        hHookDll = cm_LoadAfsdHookLib();
        if (hHookDll)
        {
            BOOL hookRc = TRUE;
            AfsdDaemonHook daemonHook = ( AfsdDaemonHook ) GetProcAddress(hHookDll, AFSD_DAEMON_HOOK);
            if (daemonHook)
            {
                hookRc = daemonHook();
            }
            FreeLibrary(hHookDll);
            hHookDll = NULL;

            if (hookRc == FALSE)
            {
                SetEvent(WaitToTerminate);
            }

            if (daemon_ShutdownFlag == 1) {
                break;
            }
	    now = osi_Time();
        }

        if (cm_daemonPerformanceTuningInterval &&
            now > lastPerformanceCheck + cm_daemonPerformanceTuningInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastPerformanceCheck = now;
            cm_PerformanceTuningCheck();
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        thrd_Sleep(10000);		/* sleep 10 seconds */
    }
    thrd_SetEvent(cm_Daemon_ShutdownEvent);
}

void cm_DaemonShutdown(void)
{
    int i;
    DWORD code;

    daemon_ShutdownFlag = 1;
    osi_Wakeup((LONG_PTR) &cm_bkgListp);

    /* wait for shutdown */
    if (cm_Daemon_ShutdownEvent)
        code = thrd_WaitForSingleObject_Event(cm_Daemon_ShutdownEvent, INFINITE);

    for ( i=0; i<cm_nDaemons; i++) {
        if (cm_BkgDaemon_ShutdownEvent[i])
            code = thrd_WaitForSingleObject_Event(cm_BkgDaemon_ShutdownEvent[i], INFINITE);
    }

    if (cm_IPAddrDaemon_ShutdownEvent)
        code = thrd_WaitForSingleObject_Event(cm_IPAddrDaemon_ShutdownEvent, INFINITE);
}

void cm_InitDaemon(int nDaemons)
{
    static osi_once_t once;
    long pid;
    thread_t phandle;
    int i;

    cm_nDaemons = (nDaemons > CM_MAX_DAEMONS) ? CM_MAX_DAEMONS : nDaemons;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_daemonLock, "cm_daemonLock",
                               LOCK_HIERARCHY_DAEMON_GLOBAL);
        osi_EndOnce(&once);

	/* creating IP Address Change monitor daemon */
        phandle = thrd_Create((SecurityAttrib) 0, 0,
                               (ThreadFunc) cm_IpAddrDaemon, 0, 0, &pid, "cm_IpAddrDaemon");
        osi_assertx(phandle != NULL, "cm_IpAddrDaemon thread creation failure");
        thrd_CloseHandle(phandle);

        /* creating pinging daemon */
        phandle = thrd_Create((SecurityAttrib) 0, 0,
                               (ThreadFunc) cm_Daemon, 0, 0, &pid, "cm_Daemon");
        osi_assertx(phandle != NULL, "cm_Daemon thread creation failure");
        thrd_CloseHandle(phandle);

	for(i=0; i < cm_nDaemons; i++) {
            phandle = thrd_Create((SecurityAttrib) 0, 0,
                                   (ThreadFunc) cm_BkgDaemon, (LPVOID)(LONG_PTR)i, 0, &pid,
                                   "cm_BkgDaemon");
            osi_assertx(phandle != NULL, "cm_BkgDaemon thread creation failure");
            thrd_CloseHandle(phandle);
        }
    }
}
