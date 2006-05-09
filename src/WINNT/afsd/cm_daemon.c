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

#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <netdb.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <rx/rx.h>
#include <rx/rx_prototypes.h>

#include "afsd.h"
#include "afsicf.h"

/* in seconds */
long cm_daemonCheckDownInterval = 180;
long cm_daemonCheckUpInterval = 600;
long cm_daemonCheckVolInterval = 3600;
long cm_daemonCheckCBInterval = 60;
long cm_daemonCheckLockInterval = 60;
long cm_daemonTokenCheckInterval = 180;

osi_rwlock_t cm_daemonLock;

long cm_bkgQueueCount;		/* # of queued requests */

int cm_bkgWaitingForCount;	/* true if someone's waiting for cm_bkgQueueCount to drop */

cm_bkgRequest_t *cm_bkgListp;		/* first elt in the list of requests */
cm_bkgRequest_t *cm_bkgListEndp;	/* last elt in the list of requests */

static int daemon_ShutdownFlag = 0;

#ifndef DJGPP
void cm_IpAddrDaemon(long parm)
{
    extern void smb_CheckVCs(void);

    rx_StartClientThread();

    while (daemon_ShutdownFlag == 0) {
	DWORD Result = NotifyAddrChange(NULL,NULL);
	if (Result == NO_ERROR && daemon_ShutdownFlag == 0) {
	    osi_Log0(afsd_logp, "cm_IpAddrDaemon CheckDownServers");
	    Sleep(2500);
	    cm_ForceNewConnectionsAllServers();
            cm_CheckServers(CM_FLAG_CHECKUPSERVERS | CM_FLAG_CHECKDOWNSERVERS, NULL);
	    smb_CheckVCs();
	}	
    }
}
#endif

void cm_BkgDaemon(long parm)
{
    cm_bkgRequest_t *rp;

    rx_StartClientThread();

    lock_ObtainWrite(&cm_daemonLock);
    while (daemon_ShutdownFlag == 0) {
        if (!cm_bkgListEndp) {
            osi_SleepW((long) &cm_bkgListp, &cm_daemonLock);
            lock_ObtainWrite(&cm_daemonLock);
            continue;
        }
                
        /* we found a request */
        rp = cm_bkgListEndp;
        cm_bkgListEndp = (cm_bkgRequest_t *) osi_QPrev(&rp->q);
        osi_QRemove((osi_queue_t **) &cm_bkgListp, &rp->q);
        osi_assert(cm_bkgQueueCount-- > 0);
        lock_ReleaseWrite(&cm_daemonLock);

        (*rp->procp)(rp->scp, rp->p1, rp->p2, rp->p3, rp->p4, rp->userp);
                
        cm_ReleaseUser(rp->userp);
        cm_ReleaseSCache(rp->scp);
        free(rp);

        lock_ObtainWrite(&cm_daemonLock);
    }
    lock_ReleaseWrite(&cm_daemonLock);
}

void cm_QueueBKGRequest(cm_scache_t *scp, cm_bkgProc_t *procp, long p1, long p2, long p3, long p4,
	cm_user_t *userp)
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

    lock_ObtainWrite(&cm_daemonLock);
    cm_bkgQueueCount++;
    osi_QAdd((osi_queue_t **) &cm_bkgListp, &rp->q);
    if (!cm_bkgListEndp) 
        cm_bkgListEndp = rp;
    lock_ReleaseWrite(&cm_daemonLock);

    osi_Wakeup((long) &cm_bkgListp);
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
    svc = OpenService(scm, "SharedAccess", SERVICE_QUERY_CONFIG);
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
    if (!flag)
        goto free_pConfig;

    /* Is it autostart? */
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

/* periodic check daemon */
void cm_Daemon(long parm)
{
    unsigned long now;
    unsigned long lastLockCheck;
    unsigned long lastVolCheck;
    unsigned long lastCBExpirationCheck;
    unsigned long lastDownServerCheck;
    unsigned long lastUpServerCheck;
    unsigned long lastTokenCacheCheck;
    char thostName[200];
    unsigned long code;
    struct hostent *thp;
    HMODULE hHookDll;
    int configureFirewall = IsWindowsFirewallPresent();

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

    now = osi_Time();
    lastVolCheck = now - cm_daemonCheckVolInterval/2 + (rand() % cm_daemonCheckVolInterval);
    lastCBExpirationCheck = now - cm_daemonCheckCBInterval/2 + (rand() % cm_daemonCheckCBInterval);
    lastLockCheck = now - cm_daemonCheckLockInterval/2 + (rand() % cm_daemonCheckLockInterval);
    lastDownServerCheck = now - cm_daemonCheckDownInterval/2 + (rand() % cm_daemonCheckDownInterval);
    lastUpServerCheck = now - cm_daemonCheckUpInterval/2 + (rand() % cm_daemonCheckUpInterval);
    lastTokenCacheCheck = now - cm_daemonTokenCheckInterval/2 + (rand() % cm_daemonTokenCheckInterval);

    while (daemon_ShutdownFlag == 0) {
	thrd_Sleep(30 * 1000);		/* sleep 30 seconds */
        if (daemon_ShutdownFlag == 1)
            return;

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

        /* check down servers */
        if (now > lastDownServerCheck + cm_daemonCheckDownInterval) {
            lastDownServerCheck = now;
	    osi_Log0(afsd_logp, "cm_Daemon CheckDownServers");
            cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS, NULL);
	    now = osi_Time();
        }

        /* check up servers */
        if (now > lastUpServerCheck + cm_daemonCheckUpInterval) {
            lastUpServerCheck = now;
	    osi_Log0(afsd_logp, "cm_Daemon CheckUpServers");
            cm_CheckServers(CM_FLAG_CHECKUPSERVERS, NULL);
	    now = osi_Time();
        }

        if (now > lastVolCheck + cm_daemonCheckVolInterval) {
            lastVolCheck = now;
            cm_CheckVolumes();
	    now = osi_Time();
        }

        if (now > lastCBExpirationCheck + cm_daemonCheckCBInterval) {
            lastCBExpirationCheck = now;
            cm_CheckCBExpiration();
	    now = osi_Time();
        }

        if (now > lastLockCheck + cm_daemonCheckLockInterval) {
            lastLockCheck = now;
            cm_CheckLocks();
	    now = osi_Time();
        }

        if (now > lastTokenCacheCheck + cm_daemonTokenCheckInterval) {
            lastTokenCacheCheck = now;
            cm_CheckTokenCache(now);
	    now = osi_Time();
        }

        /* allow an exit to be called prior to stopping the service */
        hHookDll = LoadLibrary(AFSD_HOOK_DLL);
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
        }
    }
}       

void cm_DaemonShutdown(void)
{
    daemon_ShutdownFlag = 1;
}

void cm_InitDaemon(int nDaemons)
{
    static osi_once_t once;
    long pid;
    thread_t phandle;
    int i;
        
    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_daemonLock, "cm_daemonLock");
        osi_EndOnce(&once);

#ifndef DJGPP
	/* creating IP Address Change monitor daemon */
        phandle = thrd_Create((SecurityAttrib) 0, 0,
                               (ThreadFunc) cm_IpAddrDaemon, 0, 0, &pid, "cm_IpAddrDaemon");
        osi_assert(phandle != NULL);
        thrd_CloseHandle(phandle);
#endif /* DJGPP */

        /* creating pinging daemon */
        phandle = thrd_Create((SecurityAttrib) 0, 0,
                               (ThreadFunc) cm_Daemon, 0, 0, &pid, "cm_Daemon");
        osi_assert(phandle != NULL);
        thrd_CloseHandle(phandle);

	for(i=0; i < nDaemons; i++) {
            phandle = thrd_Create((SecurityAttrib) 0, 0,
                                   (ThreadFunc) cm_BkgDaemon, 0, 0, &pid,
                                   "cm_BkgDaemon");
            osi_assert(phandle != NULL);
            thrd_CloseHandle(phandle);
        }
    }
}
