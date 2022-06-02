/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

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
long cm_daemonRDRShakeExtentsInterval = 0;
long cm_daemonAfsdHookReloadInterval = 0;
long cm_daemonEAccesCheckInterval = 1800;

typedef struct daemon_state {
    osi_rwlock_t lock;
    afs_uint32   queueCount;
    cm_bkgRequest_t *head;
    cm_bkgRequest_t *tail;
    afs_uint64   completeCount;
    afs_uint64   retryCount;
    afs_uint64   errorCount;
} daemon_state_t;

daemon_state_t *cm_daemons = NULL;
int cm_nDaemons = 0;

extern int powerStateSuspended;
int daemon_ShutdownFlag = 0;
static time_t lastIPAddrChange = 0;

static EVENT_HANDLE cm_Daemon_ShutdownEvent = NULL;
static EVENT_HANDLE cm_LockDaemon_ShutdownEvent = NULL;
static EVENT_HANDLE cm_IPAddrDaemon_ShutdownEvent = NULL;
static EVENT_HANDLE cm_BkgDaemon_ShutdownEvent[CM_MAX_DAEMONS] =
       {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

void * cm_IpAddrDaemon(void * vparm)
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
            if (smb_Enabled)
                smb_SetLanAdapterChangeDetected();
            cm_SetLanAdapterChangeDetected();
            thrd_ResetEvent(cm_IPAddrDaemon_ShutdownEvent);

            cm_ServerClearRPCStats();
	}
    }

    thrd_SetEvent(cm_IPAddrDaemon_ShutdownEvent);
    pthread_exit(NULL);
    return NULL;
}

afs_int32 cm_RequestWillBlock(cm_bkgRequest_t *rp)
{
    afs_int32 willBlock = 0;

    if (rp->procp == cm_BkgStore) {
        /*
         * If the datastoring flag is set, it means that another
         * thread is already performing an exclusive store operation
         * on this file.  The exclusive state will be cleared once
         * the file server locks the vnode.  Therefore, at most two
         * threads can be actively involved in storing data at a time
         * on a file.
         */
        lock_ObtainRead(&rp->scp->rw);
        willBlock = (rp->scp->flags & CM_SCACHEFLAG_DATASTORING);
        lock_ReleaseRead(&rp->scp->rw);
    }
    else if (rp->procp == RDR_BkgFetch || rp->procp == cm_BkgPrefetch) {
        /*
         * Attempt to determine if there is a conflict on the requested
         * range of the file.  If the first in the range does not exist
         * in the cache assume there is no conflict.  If the buffer does
         * exist, check to see if an I/O operation is in progress
         * by using the writing and reading flags as an indicator.
         */
        cm_buf_t *bufp = NULL;
        rock_BkgFetch_t *rockp = (rock_BkgFetch_t *)rp->rockp;

        bufp = buf_Find(&rp->scp->fid, &rockp->base);
        if (bufp) {
            willBlock = (bufp->flags & (CM_BUF_WRITING|CM_BUF_READING));
            buf_Release(bufp);
        }
    }

    return willBlock;
}

void * cm_BkgDaemon(void * vparm)
{
    cm_bkgRequest_t *rp;
    afs_int32 code;
    char name[32] = "";
    long daemonID = (long)(LONG_PTR)vparm;

    snprintf(name, sizeof(name), "cm_BkgDaemon_ShutdownEvent%u", daemonID);

    cm_BkgDaemon_ShutdownEvent[daemonID] = thrd_CreateEvent(NULL, FALSE, FALSE, name);
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", name);

    rx_StartClientThread();

    lock_ObtainWrite(&cm_daemons[daemonID].lock);
    while (daemon_ShutdownFlag == 0) {
        int willBlock = 0;

        if (powerStateSuspended) {
            Sleep(1000);
            continue;
        }
        if (!cm_daemons[daemonID].tail) {
            osi_SleepW((LONG_PTR)&cm_daemons[daemonID].head, &cm_daemons[daemonID].lock);
            lock_ObtainWrite(&cm_daemons[daemonID].lock);
            continue;
        }

        /* we found a request */
        for (rp = cm_daemons[daemonID].tail; rp; rp = (cm_bkgRequest_t *) osi_QPrev(&rp->q))
	{
            if (rp->scp->flags & CM_SCACHEFLAG_DELETED)
		break;

            /*
             * If the request has active I/O such that this worker would
             * be forced to block, leave the request in the queue and move
             * on to one that might be available for servicing.
             */
            if (cm_RequestWillBlock(rp)) {
                willBlock++;
                continue;
            }

            if (cm_ServerAvailable(&rp->scp->fid, rp->userp))
                break;
	}

        if (rp == NULL) {
	    /*
             * Couldn't find a request that we could process at the
             * current time.  If there were requests that would cause
             * the worker to block, sleep for 25ms so it can promptly
             * respond when it is available.  Otherwise, sleep for 1s.
             *
             * This polling cycle needs to be replaced with a proper
             * producer/consumer dynamic worker pool.
             */
            osi_Log2(afsd_logp,"cm_BkgDaemon[%u] sleeping %dms all tasks would block",
                     daemonID, willBlock ? 100 : 1000);

	    lock_ReleaseWrite(&cm_daemons[daemonID].lock);
	    Sleep(willBlock ? 100 : 1000);
	    lock_ObtainWrite(&cm_daemons[daemonID].lock);
	    continue;
	}

        osi_QRemoveHT((osi_queue_t **) &cm_daemons[daemonID].head, (osi_queue_t **) &cm_daemons[daemonID].tail, &rp->q);
        osi_assertx(cm_daemons[daemonID].queueCount-- > 0, "cm_bkgQueueCount 0");
        lock_ReleaseWrite(&cm_daemons[daemonID].lock);

	osi_Log2(afsd_logp,"cm_BkgDaemon[%u] processing request 0x%p", daemonID, rp);

        if (rp->scp->flags & CM_SCACHEFLAG_DELETED) {
            osi_Log2(afsd_logp,"cm_BkgDaemon[%u] DELETED scp 0x%x", daemonID, rp->scp);
            code = CM_ERROR_BADFD;
            if (rp->procp == cm_BkgDirectWrite) {
                cm_BkgDirectWriteDone(rp->scp, rp->rockp, code);
            }
        } else {
#ifdef DEBUG_REFCOUNT
            osi_Log3(afsd_logp,"cm_BkgDaemon[%u] (before) scp 0x%x ref %d", daemonID, rp->scp, rp->scp->refCount);
#endif
            code = (*rp->procp)(rp->scp, rp->rockp, rp->userp, &rp->req);
#ifdef DEBUG_REFCOUNT
            osi_Log3(afsd_logp,"cm_BkgDaemon[%u] (after) scp 0x%x ref %d", daemonID, rp->scp, rp->scp->refCount);
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
            if (rp->procp == cm_BkgStore ||
                rp->procp == cm_BkgDirectWrite ||
                rp->procp == RDR_BkgFetch) {
                osi_Log3(afsd_logp,
                         "cm_BkgDaemon[%u] re-queueing failed request 0x%p code 0x%x",
                         daemonID, rp, code);
                lock_ObtainWrite(&cm_daemons[daemonID].lock);
                cm_daemons[daemonID].queueCount++;
                cm_daemons[daemonID].retryCount++;
                osi_QAddT((osi_queue_t **) &cm_daemons[daemonID].head, (osi_queue_t **)&cm_daemons[daemonID].tail, &rp->q);
                break;
            }
            AFS_FALLTHROUGH;
        case 0:  /* success */
        default: /* other error */
            if (code == 0) {
                osi_Log2(afsd_logp,"cm_BkgDaemon[%u] SUCCESS: request 0x%p", daemonID, rp);
                cm_daemons[daemonID].completeCount++;
            } else {
                osi_Log3(afsd_logp,"cm_BkgDaemon[%u] FAILED: request dropped 0x%p code 0x%x",
                         daemonID, rp, code);
                cm_daemons[daemonID].errorCount++;
            }
            cm_ReleaseUser(rp->userp);
            cm_ReleaseSCache(rp->scp);
            free(rp->rockp);
            free(rp);
            lock_ObtainWrite(&cm_daemons[daemonID].lock);
        }
    }
    lock_ReleaseWrite(&cm_daemons[daemonID].lock);
    thrd_SetEvent(cm_BkgDaemon_ShutdownEvent[daemonID]);
    pthread_exit(NULL);
    return NULL;
}

int cm_QueueBKGRequest(cm_scache_t *scp, cm_bkgProc_t *procp, void *rockp,
                        cm_user_t *userp, cm_req_t *reqp)
{
    cm_bkgRequest_t *rp, *rpq;
    afs_uint32 daemonID;
    int duplicate = 0;

    rp = malloc(sizeof(*rp));
    memset(rp, 0, sizeof(*rp));

    cm_HoldSCache(scp);
    rp->scp = scp;
    cm_HoldUser(userp);
    rp->userp = userp;
    rp->procp = procp;
    rp->rockp = rockp;
    rp->req = *reqp;

    /* Use separate queues for fetch and store operations */
    daemonID = scp->fid.hash % (cm_nDaemons/2) * 2;
    if (procp == cm_BkgStore ||
        procp == cm_BkgDirectWrite)
        daemonID++;

    /* Check to see if this is a duplicate request */
    lock_ObtainWrite(&cm_daemons[daemonID].lock);
    if ( procp == cm_BkgStore || procp == RDR_BkgFetch || procp == cm_BkgPrefetch ) {
        for (rpq = cm_daemons[daemonID].head; rpq; rpq = (cm_bkgRequest_t *) osi_QNext(&rpq->q))
        {
            if ( rpq->procp == procp &&
                 rpq->scp == scp &&
                 rpq->userp == userp)
            {
                if (rp->procp == cm_BkgStore) {
                    rock_BkgStore_t *rock1p = (rock_BkgStore_t *)rp->rockp;
                    rock_BkgStore_t *rock2p = (rock_BkgStore_t *)rpq->rockp;

                    duplicate = (memcmp(rock1p, rock2p, sizeof(*rock1p)) == 0);
                }
                else if (rp->procp == RDR_BkgFetch || rp->procp == cm_BkgPrefetch) {
                    rock_BkgFetch_t *rock1p = (rock_BkgFetch_t *)rp->rockp;
                    rock_BkgFetch_t *rock2p = (rock_BkgFetch_t *)rpq->rockp;

                    duplicate = (memcmp(rock1p, rock2p, sizeof(*rock1p)) == 0);
                }

                if (duplicate) {
                    /* found a duplicate; update request with latest info */
                    break;
                }
            }
        }
    }

    if (!duplicate) {
        cm_daemons[daemonID].queueCount++;
        osi_QAddH((osi_queue_t **) &cm_daemons[daemonID].head, (osi_queue_t **)&cm_daemons[daemonID].tail, &rp->q);
    }
    lock_ReleaseWrite(&cm_daemons[daemonID].lock);

    if (duplicate) {
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        free(rp->rockp);
        free(rp);
        return -1;
    } else {
        osi_Wakeup((LONG_PTR) &cm_daemons[daemonID].head);
        return 0;
    }
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
    code = RegQueryValueEx(parmKey, "daemonRDRShakeExtentsInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonRDRShakeExtentsInterval = dummy;
    afsi_log("daemonRDRShakeExtentsInterval is %d", cm_daemonRDRShakeExtentsInterval);

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

    dummyLen = sizeof(DWORD);
    code = RegQueryValueEx(parmKey, "daemonAfsdHookReloadInterval", NULL, NULL,
			    (BYTE *) &dummy, &dummyLen);
    if (code == ERROR_SUCCESS && dummy)
	cm_daemonAfsdHookReloadInterval = dummy;
    afsi_log("daemonAfsdHookReloadInterval is %d", cm_daemonAfsdHookReloadInterval);

    RegCloseKey(parmKey);

    if (cm_daemonPerformanceTuningInterval)
        cm_PerformanceTuningInit();
}

/* periodic lock check daemon */
void * cm_LockDaemon(void * vparm)
{
    time_t now;
    time_t lastLockCheck;
    char * name = "cm_LockDaemon_ShutdownEvent";

    cm_LockDaemon_ShutdownEvent = thrd_CreateEvent(NULL, FALSE, FALSE, name);
    if ( GetLastError() == ERROR_ALREADY_EXISTS )
        afsi_log("Event Object Already Exists: %s", name);

    now = osi_Time();
    lastLockCheck = now - cm_daemonCheckLockInterval/2 + (rand() % cm_daemonCheckLockInterval);

    while (daemon_ShutdownFlag == 0) {
        if (powerStateSuspended) {
            Sleep(1000);
            continue;
        }

        now = osi_Time();

        if (now > lastLockCheck + cm_daemonCheckLockInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastLockCheck = now;
            cm_CheckLocks();
            if (daemon_ShutdownFlag == 1)
                break;
        }

        thrd_Sleep(1000);		/* sleep 1 second */
    }
    thrd_SetEvent(cm_LockDaemon_ShutdownEvent);
    pthread_exit(NULL);
    return NULL;
}

/* periodic check daemon */
void * cm_Daemon(void *vparm)
{
    time_t now;
    time_t lastVolCheck;
    time_t lastCBExpirationCheck;
    time_t lastVolCBRenewalCheck;
    time_t lastDownServerCheck;
    time_t lastUpServerCheck;
    time_t lastTokenCacheCheck;
    time_t lastBusyVolCheck;
    time_t lastPerformanceCheck;
    time_t lastServerRankCheck;
    time_t lastRDRShakeExtents;
    time_t lastAfsdHookReload;
    time_t lastEAccesCheck;
    char thostName[200];
    unsigned long code;
    struct hostent *thp;
    HMODULE hHookDll = NULL;
    AfsdDaemonHook daemonHook = NULL;
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
    lastDownServerCheck = now - cm_daemonCheckDownInterval/2 + (rand() % cm_daemonCheckDownInterval);
    lastUpServerCheck = now - cm_daemonCheckUpInterval/2 + (rand() % cm_daemonCheckUpInterval);
    lastTokenCacheCheck = now - cm_daemonTokenCheckInterval/2 + (rand() % cm_daemonTokenCheckInterval);
    if (cm_daemonCheckOfflineVolInterval)
        lastBusyVolCheck = now - cm_daemonCheckOfflineVolInterval/2 * (rand() % cm_daemonCheckOfflineVolInterval);
    if (cm_daemonPerformanceTuningInterval)
        lastPerformanceCheck = now - cm_daemonPerformanceTuningInterval/2 * (rand() % cm_daemonPerformanceTuningInterval);
    lastServerRankCheck = now - cm_daemonRankServerInterval/2 * (rand() % cm_daemonRankServerInterval);
    if (cm_daemonRDRShakeExtentsInterval)
        lastRDRShakeExtents = now - cm_daemonRDRShakeExtentsInterval/2 * (rand() % cm_daemonRDRShakeExtentsInterval);
    if (cm_daemonAfsdHookReloadInterval)
        lastAfsdHookReload = now;
    lastEAccesCheck = now;

    hHookDll = cm_LoadAfsdHookLib();
    if (hHookDll)
        daemonHook = ( AfsdDaemonHook ) GetProcAddress(hHookDll, AFSD_DAEMON_HOOK);

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
	    switch (icf_CheckAndAddAFSPorts(cm_callbackport)) {
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
            powerStateSuspended == 0) {
            cm_ForceNewConnectionsAllServers();
        }

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

        if (now > lastTokenCacheCheck + cm_daemonTokenCheckInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            lastTokenCacheCheck = now;
            cm_CheckTokenCache(now);
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (now > lastEAccesCheck + cm_daemonEAccesCheckInterval &&
             daemon_ShutdownFlag == 0 &&
             powerStateSuspended == 0) {
            lastEAccesCheck = now;
            cm_EAccesClearOutdatedEntries();
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        if (cm_daemonRDRShakeExtentsInterval &&
            now > lastRDRShakeExtents + cm_daemonRDRShakeExtentsInterval &&
            daemon_ShutdownFlag == 0 &&
            powerStateSuspended == 0) {
            cm_req_t req;
            cm_InitReq(&req);
            lastRDRShakeExtents = now;
            if (cm_data.buf_redirCount > cm_data.buf_freeCount)
                buf_RDRShakeSomeExtentsFree(&req, FALSE, 10 /* seconds */);
            if (daemon_ShutdownFlag == 1)
                break;
	    now = osi_Time();
        }

        /* allow an exit to be called prior to stopping the service */
        if (cm_daemonAfsdHookReloadInterval &&
            lastAfsdHookReload != 0 && lastAfsdHookReload < now) {
            if (hHookDll) {
                FreeLibrary(hHookDll);
                hHookDll = NULL;
                daemonHook = NULL;
            }

            hHookDll = cm_LoadAfsdHookLib();
            if (hHookDll)
                daemonHook = ( AfsdDaemonHook ) GetProcAddress(hHookDll, AFSD_DAEMON_HOOK);
        }

        if (daemonHook)
        {
            BOOL hookRc = daemonHook();

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

        /*
         * sleep .5 seconds.  if the thread blocks for a long time
         * we risk not being able to close the cache before Windows
         * kills our process during system shutdown.
         */
        thrd_Sleep(500);
    }

    if (hHookDll) {
        FreeLibrary(hHookDll);
    }

    thrd_SetEvent(cm_Daemon_ShutdownEvent);
    pthread_exit(NULL);
    return NULL;
}

void cm_DaemonShutdown(void)
{
    int i;
    DWORD code;

    daemon_ShutdownFlag = 1;

    /* wait for shutdown */
    for ( i=0; i<cm_nDaemons; i++) {
        osi_Wakeup((LONG_PTR) &cm_daemons[i].head);
        if (cm_BkgDaemon_ShutdownEvent[i])
            code = thrd_WaitForSingleObject_Event(cm_BkgDaemon_ShutdownEvent[i], INFINITE);
    }

    if (cm_Daemon_ShutdownEvent)
        code = thrd_WaitForSingleObject_Event(cm_Daemon_ShutdownEvent, INFINITE);

    if (cm_LockDaemon_ShutdownEvent)
        code = thrd_WaitForSingleObject_Event(cm_LockDaemon_ShutdownEvent, INFINITE);
}

void cm_InitDaemon(int nDaemons)
{
    static osi_once_t once;
    pthread_t phandle;
    pthread_attr_t tattr;
    int pstatus;
    int i;

    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

    if (nDaemons > CM_MAX_DAEMONS)
        cm_nDaemons = CM_MAX_DAEMONS;
    else if (nDaemons < CM_MIN_DAEMONS)
        cm_nDaemons = CM_MIN_DAEMONS;
    else
        cm_nDaemons = (nDaemons / 2) * 2; /* must be divisible by two */

    if (osi_Once(&once)) {
	/* creating IP Address Change monitor daemon */
        pstatus = pthread_create(&phandle, &tattr, cm_IpAddrDaemon, 0);
        osi_assertx(pstatus == 0, "cm_IpAddrDaemon thread creation failure");

        /* creating pinging daemon */
        pstatus = pthread_create(&phandle, &tattr, cm_Daemon, 0);
        osi_assertx(pstatus == 0, "cm_Daemon thread creation failure");

        pstatus = pthread_create(&phandle, &tattr, cm_LockDaemon, 0);
        osi_assertx(pstatus == 0, "cm_LockDaemon thread creation failure");

        cm_daemons = malloc(nDaemons * sizeof(daemon_state_t));

	for(i=0; i < cm_nDaemons; i++) {
            lock_InitializeRWLock(&cm_daemons[i].lock, "cm_daemonLock",
                                  LOCK_HIERARCHY_DAEMON_GLOBAL);
            cm_daemons[i].head = cm_daemons[i].tail = NULL;
            cm_daemons[i].queueCount=0;
            cm_daemons[i].completeCount=0;
            cm_daemons[i].retryCount=0;
            cm_daemons[i].errorCount=0;
            pstatus = pthread_create(&phandle, &tattr, cm_BkgDaemon, (LPVOID)(LONG_PTR)i);
            osi_assertx(pstatus == 0, "cm_BkgDaemon thread creation failure");
        }
        osi_EndOnce(&once);
    }

    pthread_attr_destroy(&tattr);
}
