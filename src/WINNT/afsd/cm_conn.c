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
#endif /* !DJGPP */
#include <string.h>
#include <malloc.h>
#include <osi.h>
#include <rx/rx.h>
#ifndef DJGPP
#include <rx/rxkad.h>
#else
#include <rx/rxkad.h>
#endif

#include "afsd.h"

osi_rwlock_t cm_connLock;

long RDRtimeout = CM_CONN_DEFAULTRDRTIMEOUT;

afs_int32 cryptall = 0;

void cm_PutConn(cm_conn_t *connp)
{
	lock_ObtainWrite(&cm_connLock);
	osi_assert(connp->refCount-- > 0);
	lock_ReleaseWrite(&cm_connLock);
}

void cm_InitConn(void)
{
	static osi_once_t once;
        
        if (osi_Once(&once)) {
		lock_InitializeRWLock(&cm_connLock, "connection global lock");
		osi_EndOnce(&once);
        }
}

void cm_InitReq(cm_req_t *reqp)
{
	memset((char *)reqp, 0, sizeof(cm_req_t));
#ifndef DJGPP
	reqp->startTime = GetCurrentTime();
#else
        gettimeofday(&reqp->startTime, NULL);
#endif
 
}

long cm_GetServerList(struct cm_fid *fidp, struct cm_user *userp,
	struct cm_req *reqp, cm_serverRef_t **serverspp)
{
	long code;
        cm_volume_t *volp = NULL;
        cm_serverRef_t *serversp = NULL;
        cm_cell_t *cellp = NULL;

        if (!fidp) {
		*serverspp = NULL;
		return 0;
	}

	cellp = cm_FindCellByID(fidp->cell);
        if (!cellp) return CM_ERROR_NOSUCHCELL;

        code = cm_GetVolumeByID(cellp, fidp->volume, userp, reqp, &volp);
        if (code) return code;
        
	if (fidp->volume == volp->rwID)
        	serversp = volp->rwServersp;
	else if (fidp->volume == volp->roID)
        	serversp = volp->roServersp;
	else if (fidp->volume == volp->bkID)
        	serversp = volp->bkServersp;
	else
		serversp = NULL;

        cm_PutVolume(volp);
	*serverspp = serversp;
	return 0;
}

/*
 * Analyze the error return from an RPC.  Determine whether or not to retry,
 * and if we're going to retry, determine whether failover is appropriate,
 * and whether timed backoff is appropriate.
 *
 * If the error code is from cm_Conn() or friends, it will be a CM_ERROR code.
 * Otherwise it will be an RPC code.  This may be a UNIX code (e.g. EDQUOT), or
 * it may be an RX code, or it may be a special code (e.g. VNOVOL), or it may
 * be a security code (e.g. RXKADEXPIRED).
 *
 * If the error code is from cm_Conn() or friends, connp will be NULL.
 *
 * For VLDB calls, fidp will be NULL.
 *
 * volSyncp and/or cbrp may also be NULL.
 */
cm_Analyze(cm_conn_t *connp, cm_user_t *userp, cm_req_t *reqp,
	struct cm_fid *fidp,
	AFSVolSync *volSyncp, cm_callbackRequest_t *cbrp, long errorCode)
{
	cm_server_t *serverp;
	cm_serverRef_t *serversp, *tsrp;
	cm_ucell_t *ucellp;
        int retry = 0;
	int dead_session;
        
	osi_Log2(afsd_logp, "cm_Analyze connp 0x%x, code %d",
		 (long) connp, errorCode);

	/* no locking required, since connp->serverp never changes after
	 * creation */
	dead_session = (userp->cellInfop == NULL);
	if (connp)
		serverp = connp->serverp;

	/* Update callback pointer */
        if (cbrp && errorCode == 0) cbrp->serverp = connp->serverp;

	/* If not allowed to retry, don't */
	if (reqp->flags & CM_REQ_NORETRY)
		goto out;

	/* if all servers are busy, mark them non-busy and start over */
	if (errorCode == CM_ERROR_ALLBUSY) {
		cm_GetServerList(fidp, userp, reqp, &serversp);
		for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
			if (tsrp->status == busy)
				tsrp->status = not_busy;
		}
		thrd_Sleep(5000);
		retry = 1;
	}

	/* special codes:  VBUSY and VRESTARTING */
	if (errorCode == VBUSY || errorCode == VRESTARTING) {
		cm_GetServerList(fidp, userp, reqp, &serversp);
		for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
			if (tsrp->server == serverp
			    && tsrp->status == not_busy) {
				tsrp->status = busy;
				break;
			}
		}
		retry = 1;
	}

	/* special codes:  missing volumes */
	if (errorCode == VNOVOL || errorCode == VMOVED || errorCode == VOFFLINE
	    || errorCode == VSALVAGE || errorCode == VNOSERVICE) {
		long oldSum, newSum;
		int same;

		/* Back off to allow move to complete */
		thrd_Sleep(2000);

		/* Update the volume location and see if it changed */
		cm_GetServerList(fidp, userp, reqp, &serversp);
		oldSum = cm_ChecksumServerList(serversp);
		cm_ForceUpdateVolume(fidp, userp, reqp);
		cm_GetServerList(fidp, userp, reqp, &serversp);
		newSum = cm_ChecksumServerList(serversp);
		same = (oldSum == newSum);

		/* mark servers as appropriate */
		for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
			if (tsrp->server == serverp)
				tsrp->status = offline;
			else if (!same)
				tsrp->status = not_busy;
		}
		retry = 1;
	}

	/* RX codes */
	if (errorCode == RX_CALL_TIMEOUT) {
		/* server took longer than hardDeadTime 
		 * don't mark server as down but don't retry
		 * this is to prevent the SMB session from timing out
		 * In addition, we log an event to the event log 
		 */
#ifndef DJGPP
		HANDLE h;
                char *ptbuf[1];
                char s[100];
                h = RegisterEventSource(NULL, AFS_DAEMON_EVENT_NAME);
                sprintf(s, "cm_Analyze: HardDeadTime exceeded.");
                ptbuf[0] = s;
                ReportEvent(h, EVENTLOG_WARNING_TYPE, 0, 1009, NULL,
                        1, 0, ptbuf, NULL);
                DeregisterEventSource(h);
#endif /* !DJGPP */
	  
		retry = 0;
	  	osi_Log0(afsd_logp, "cm_Analyze: hardDeadTime exceeded");
	}
	else if (errorCode >= -64 && errorCode < 0) {
		/* mark server as down */
		lock_ObtainMutex(&serverp->mx);
                serverp->flags |= CM_SERVERFLAG_DOWN;
		lock_ReleaseMutex(&serverp->mx);
                retry = 1;
        }

	if (errorCode == RXKADEXPIRED && !dead_session) {
		lock_ObtainMutex(&userp->mx);
		ucellp = cm_GetUCell(userp, serverp->cellp);
		if (ucellp->ticketp) {
			free(ucellp->ticketp);
			ucellp->ticketp = NULL;
		}
		ucellp->flags &= ~CM_UCELLFLAG_RXKAD;
		ucellp->gen++;
		lock_ReleaseMutex(&userp->mx);
		retry = 1;
	}

	if (retry && dead_session)
		retry = 0;
 
out:
	/* drop this on the way out */
	if (connp)
		cm_PutConn(connp);

	/* retry until we fail to find a connection */
        return retry;
}

long cm_ConnByMServers(cm_serverRef_t *serversp, cm_user_t *usersp,
	cm_req_t *reqp, cm_conn_t **connpp)
{
	long code;
	cm_serverRef_t *tsrp;
        cm_server_t *tsp;
        long firstError = 0;
	int someBusy = 0, someOffline = 0;
	long timeUsed, timeLeft, hardTimeLeft;
#ifdef DJGPP
        struct timeval now;
#endif /* DJGPP */        

        *connpp = NULL;

#ifndef DJGPP
	timeUsed = (GetCurrentTime() - reqp->startTime) / 1000;
#else
        gettimeofday(&now, NULL);
        timeUsed = sub_time(now, reqp->startTime) / 1000;
#endif
        
	/* leave 5 seconds margin of safety */
	timeLeft = RDRtimeout - timeUsed - 5;
	hardTimeLeft = timeLeft;

	/* Time enough to do an RPC? */
	if (timeLeft < 1) {
		return CM_ERROR_TIMEDOUT;
	}

	lock_ObtainWrite(&cm_serverLock);

        for(tsrp = serversp; tsrp; tsrp=tsrp->next) {
		tsp = tsrp->server;
		tsp->refCount++;
                lock_ReleaseWrite(&cm_serverLock);
		if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
			if (tsrp->status == busy)
				someBusy = 1;
			else if (tsrp->status == offline)
				someOffline = 1;
			else {
				code = cm_ConnByServer(tsp, usersp, connpp);
				if (code == 0) {
					cm_PutServer(tsp);
					/* Set RPC timeout */
					if (timeLeft > CM_CONN_CONNDEADTIME)
						timeLeft = CM_CONN_CONNDEADTIME;

					if (hardTimeLeft > CM_CONN_HARDDEADTIME) 
					        hardTimeLeft = CM_CONN_HARDDEADTIME;

					lock_ObtainMutex(&(*connpp)->mx);
					rx_SetConnDeadTime((*connpp)->callp,
							   timeLeft);
					rx_SetConnHardDeadTime((*connpp)->callp, 
							       (u_short) hardTimeLeft);
					lock_ReleaseMutex(&(*connpp)->mx);

                        		return 0;
				}
				if (firstError == 0) firstError = code;
			}
                }
                lock_ObtainWrite(&cm_serverLock);
                osi_assert(tsp->refCount-- > 0);
        }

	lock_ReleaseWrite(&cm_serverLock);
	if (firstError == 0) {
		if (someBusy) firstError = CM_ERROR_ALLBUSY;
		else if (someOffline) firstError = CM_ERROR_NOSUCHVOLUME;
		else firstError = CM_ERROR_TIMEDOUT;
	}
	osi_Log1(afsd_logp, "cm_ConnByMServers returning %x", firstError);
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
	for(tcp = *lcpp; tcp; tcp = *lcpp) {
		userp = tcp->userp;
		if (userp && tcp->refCount == 0 && (userp->vcRefs == 0)) {
			/* do the deletion of this guy */
                        cm_ReleaseUser(userp);
                        *lcpp = tcp->nextp;
			rx_DestroyConnection(tcp->callp);
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
	cm_server_t *serverp)
{
        unsigned short port;
        int serviceID;
        int secIndex;
        struct rx_securityClass *secObjp;
	afs_int32 level;

	if (serverp->type == CM_SERVER_VLDB) {
		port = htons(7003);
                serviceID = 52;
        }
        else {
		osi_assert(serverp->type == CM_SERVER_FILE);
                port = htons(7000);
                serviceID = 1;
        }
	if (ucellp->flags & CM_UCELLFLAG_RXKAD) {
		secIndex = 2;
		if (cryptall) {
			level = rxkad_crypt;
			tcp->cryptlevel = rxkad_crypt;
		} else {
			level = rxkad_clear;
		}
                secObjp = rxkad_NewClientSecurityObject(level,
			&ucellp->sessionKey, ucellp->kvno,
			ucellp->ticketLen, ucellp->ticketp);
        }
        else {
		/* normal auth */
                secIndex = 0;
                secObjp = rxnull_NewClientSecurityObject();
        }
	osi_assert(secObjp != NULL);
        tcp->callp = rx_NewConnection(serverp->addr.sin_addr.s_addr,
               	port,
        	serviceID,
		secObjp,
                secIndex);
	rx_SetConnDeadTime(tcp->callp, CM_CONN_CONNDEADTIME);
	rx_SetConnHardDeadTime(tcp->callp, CM_CONN_HARDDEADTIME);
	tcp->ucgen = ucellp->gen;
}

long cm_ConnByServer(cm_server_t *serverp, cm_user_t *userp, cm_conn_t **connpp)
{
	cm_conn_t *tcp;
        cm_ucell_t *ucellp;

	lock_ObtainMutex(&userp->mx);
	lock_ObtainWrite(&cm_connLock);
	for(tcp = serverp->connsp; tcp; tcp=tcp->nextp) {
		if (tcp->userp == userp) break;
        }
	/* find ucell structure */
        ucellp = cm_GetUCell(userp, serverp->cellp);
	if (!tcp) {
		tcp = malloc(sizeof(*tcp));
                memset(tcp, 0, sizeof(*tcp));
                tcp->nextp = serverp->connsp;
                serverp->connsp = tcp;
                tcp->userp = userp;
                cm_HoldUser(userp);
                lock_InitializeMutex(&tcp->mx, "cm_conn_t mutex");
                tcp->serverp = serverp;
		tcp->cryptlevel = rxkad_clear;
		cm_NewRXConnection(tcp, ucellp, serverp);
		tcp->refCount = 1;
        }
	else {
		if ((tcp->ucgen < ucellp->gen) || (tcp->cryptlevel != cryptall))
		{
			rx_DestroyConnection(tcp->callp);
			cm_NewRXConnection(tcp, ucellp, serverp);
		}
        	tcp->refCount++;
	}
	lock_ReleaseWrite(&cm_connLock);
        lock_ReleaseMutex(&userp->mx);

	/* return this pointer to our caller */
        osi_Log1(afsd_logp, "cm_ConnByServer returning conn 0x%x", (long) tcp);
	*connpp = tcp;

        return 0;
}

long cm_Conn(struct cm_fid *fidp, struct cm_user *userp, cm_req_t *reqp,
	cm_conn_t **connpp)
{
	long code;

	cm_serverRef_t *serversp;

	code = cm_GetServerList(fidp, userp, reqp, &serversp);
	if (code) {
		*connpp = NULL;
		return code;
	}

	code = cm_ConnByMServers(serversp, userp, reqp, connpp);
        return code;
}
