/* 
 * Copyright (C) 1998, 1989 Transarc Corporation - All rights reserved
 *
 * (C) COPYRIGHT IBM CORPORATION 1987, 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 *
 *
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <rx/rx.h>

#include "afsd.h"

long cm_daemonCheckInterval = 30;
long cm_daemonTokenCheckInterval = 180;

osi_rwlock_t cm_daemonLock;

long cm_bkgQueueCount;		/* # of queued requests */

int cm_bkgWaitingForCount;	/* true if someone's waiting for cm_bkgQueueCount to drop */

cm_bkgRequest_t *cm_bkgListp;		/* first elt in the list of requests */
cm_bkgRequest_t *cm_bkgListEndp;	/* last elt in the list of requests */

void cm_BkgDaemon(long parm)
{
	cm_bkgRequest_t *rp;

	lock_ObtainWrite(&cm_daemonLock);
	while(1) {
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
}

void cm_QueueBKGRequest(cm_scache_t *scp, cm_bkgProc_t *procp, long p1, long p2, long p3, long p4,
	cm_user_t *userp)
{
	cm_bkgRequest_t *rp;
        
        rp = malloc(sizeof(*rp));
        memset(rp, 0, sizeof(*rp));
        
        rp->scp = scp;
        cm_HoldSCache(scp);
        rp->userp = userp;
        cm_HoldUser(userp);
        rp->procp = procp;
        rp->p1 = p1;
        rp->p2 = p2;
        rp->p3 = p3;
        rp->p4 = p4;
        
        lock_ObtainWrite(&cm_daemonLock);
	cm_bkgQueueCount++;
        osi_QAdd((osi_queue_t **) &cm_bkgListp, &rp->q);
        if (!cm_bkgListEndp) cm_bkgListEndp = rp;
        lock_ReleaseWrite(&cm_daemonLock);
        
        osi_Wakeup((long) &cm_bkgListp);
}

/* periodic check daemon */
void cm_Daemon(long parm)
{
        long now;
	long lastLockCheck;
        long lastVolCheck;
        long lastCBExpirationCheck;
	long lastDownServerCheck;
	long lastUpServerCheck;
	long lastTokenCacheCheck;
	char thostName[200];
	long code;
	struct hostent *thp;

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
	memcpy(&code, thp->h_addr_list[0], 4);
	srand(ntohl(code));

	now = osi_Time();
	lastVolCheck = now - 1800 + (rand() % 3600);
        lastCBExpirationCheck = now - 60 + (rand() % 60);
	lastLockCheck = now - 60 + (rand() % 60);
	lastDownServerCheck = now - cm_daemonCheckInterval/2 + (rand() % cm_daemonCheckInterval);
	lastUpServerCheck = now - 1800 + (rand() % 3600);
	lastTokenCacheCheck = now - cm_daemonTokenCheckInterval/2 + (rand() % cm_daemonTokenCheckInterval);
	
        while (1) {
		Sleep(30 * 1000);		/* sleep 30 seconds */
                
		/* find out what time it is */
		now = osi_Time();

		/* check down servers */
		if (now > lastDownServerCheck + cm_daemonCheckInterval) {
			lastDownServerCheck = now;
			cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS, NULL);
		}

		/* check up servers */
		if (now > lastUpServerCheck + 3600) {
			lastUpServerCheck = now;
			cm_CheckServers(CM_FLAG_CHECKUPSERVERS, NULL);
		}

                if (now > lastVolCheck + 3600) {
			lastVolCheck = now;
                        cm_CheckVolumes();
                }
                
                if (now > lastCBExpirationCheck + 60) {
			lastCBExpirationCheck = now;
                        cm_CheckCBExpiration();
                }
                
                if (now > lastLockCheck + 60) {
			lastLockCheck = now;
                        cm_CheckLocks();
                }

		if (now > lastTokenCacheCheck + cm_daemonTokenCheckInterval) {
		        lastTokenCacheCheck = now;
			cm_CheckTokenCache(now);
		}
        }
}

void cm_InitDaemon(int nDaemons)
{
	static osi_once_t once;
        long pid;
        HANDLE phandle;
        int i;
        
        if (osi_Once(&once)) {
		lock_InitializeRWLock(&cm_daemonLock, "cm_daemonLock");
		osi_EndOnce(&once);
                
                /* creating pinging daemon */
		phandle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
	                (LPTHREAD_START_ROUTINE) cm_Daemon, 0, 0, &pid);
		osi_assert(phandle != NULL);

		CloseHandle(phandle);
		for(i=0; i < nDaemons; i++) {
			phandle = CreateThread((SECURITY_ATTRIBUTES *) 0, 0,
		                (LPTHREAD_START_ROUTINE) cm_BkgDaemon, 0, 0, &pid);
			osi_assert(phandle != NULL);
			CloseHandle(phandle);
		}
        }
}
