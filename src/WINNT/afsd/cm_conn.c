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
#include <WINNT/afsreg.h>

osi_rwlock_t cm_connLock;

long RDRtimeout = CM_CONN_DEFAULTRDRTIMEOUT;
unsigned short ConnDeadtimeout = CM_CONN_CONNDEADTIME;
unsigned short HardDeadtimeout = CM_CONN_HARDDEADTIME;

#define LANMAN_WKS_PARAM_KEY "SYSTEM\\CurrentControlSet\\Services\\lanmanworkstation\\parameters"
#define LANMAN_WKS_SESSION_TIMEOUT "SessTimeout"

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
    long code;
    DWORD dwValue;
    DWORD dummyLen;
    HKEY parmKey;
        
    if (osi_Once(&once)) {
	lock_InitializeRWLock(&cm_connLock, "connection global lock");

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
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, LANMAN_WKS_SESSION_TIMEOUT, NULL, NULL, 
				   (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS)
                RDRtimeout = dwValue;
	    RegCloseKey(parmKey);
        }

	code = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY,
			     0, KEY_QUERY_VALUE, &parmKey);
	if (code == ERROR_SUCCESS) {
	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "ConnDeadTimeout", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS)
                ConnDeadtimeout = (unsigned short)dwValue;

	    dummyLen = sizeof(DWORD);
	    code = RegQueryValueEx(parmKey, "HardDeadTimeout", NULL, NULL,
				    (BYTE *) &dwValue, &dummyLen);
	    if (code == ERROR_SUCCESS)
                HardDeadtimeout = (unsigned short)dwValue;
	    afsi_log("HardDeadTimeout is %d", HardDeadtimeout);
	    RegCloseKey(parmKey);
	}

	afsi_log("lanmanworkstation : SessTimeout %d", RDRtimeout);
	if (ConnDeadtimeout == 0)
	    ConnDeadtimeout = (unsigned short) (RDRtimeout / 2);
	afsi_log("ConnDeadTimeout is %d", ConnDeadtimeout);
	if (HardDeadtimeout == 0)
	    HardDeadtimeout = (unsigned short) RDRtimeout;
	afsi_log("HardDeadTimeout is %d", HardDeadtimeout);

	osi_EndOnce(&once);
    }
}

void cm_InitReq(cm_req_t *reqp)
{
	memset((char *)reqp, 0, sizeof(cm_req_t));
	reqp->startTime = GetTickCount();
}

static long cm_GetServerList(struct cm_fid *fidp, struct cm_user *userp,
	struct cm_req *reqp, cm_serverRef_t ***serversppp)
{
    long code;
    cm_volume_t *volp = NULL;
    cm_cell_t *cellp = NULL;

    if (!fidp) {
        *serversppp = NULL;
        return 0;
    }

    cellp = cm_FindCellByID(fidp->cell);
    if (!cellp) return CM_ERROR_NOSUCHCELL;

    code = cm_GetVolumeByID(cellp, fidp->volume, userp, reqp, &volp);
    if (code) return code;
    
    *serversppp = cm_GetVolServers(volp, fidp->volume);

    cm_PutVolume(volp);
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
int
cm_Analyze(cm_conn_t *connp, cm_user_t *userp, cm_req_t *reqp,
           struct cm_fid *fidp, 
           AFSVolSync *volSyncp, 
           cm_serverRef_t * serversp,
           cm_callbackRequest_t *cbrp, long errorCode)
{
    cm_server_t *serverp = NULL;
    cm_serverRef_t **serverspp = NULL;
    cm_serverRef_t *tsrp;
    cm_cell_t  *cellp = NULL;
    cm_ucell_t *ucellp;
    int retry = 0;
    int free_svr_list = 0;
    int dead_session;
    long timeUsed, timeLeft;
    long code;
    char addr[16];

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

    /* If not allowed to retry, don't */
    if (reqp->flags & CM_REQ_NORETRY)
        goto out;

    /* if timeout - check that it did not exceed the HardDead timeout
     * and retry */
    
    /* timeleft - get if from reqp the same way as cmXonnByMServers does */
    timeUsed = (GetTickCount() - reqp->startTime) / 1000;
	    
    /* leave 5 seconds margin for sleep */
    timeLeft = HardDeadtimeout - timeUsed;

    if (errorCode == CM_ERROR_TIMEDOUT) {
        if (timeLeft > 5 ) {
            thrd_Sleep(3000);
            if (cellp == NULL && serverp)
                cellp = serverp->cellp;
            if (cellp == NULL && serversp) {
                struct cm_serverRef * refp;
                for ( refp=serversp ; cellp == NULL && refp != NULL; refp=refp->next) {
                    if ( refp->server )
                        cellp = refp->server->cellp;
                }
            }
            cm_CheckServers(CM_FLAG_CHECKDOWNSERVERS, cellp);
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
        if (timeLeft > 7) {
            thrd_Sleep(5000);
            
            retry = 1;

            if (fidp != NULL)   /* Not a VLDB call */
                cm_ForceUpdateVolume(fidp, userp, reqp);
        }
    }

    else if (errorCode == CM_ERROR_ALLDOWN) {
	osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_ALLDOWN.");
	/* Servers marked DOWN will be restored by the background daemon
	 * thread as they become available.
	 */
    }

    else if (errorCode == CM_ERROR_ALLOFFLINE) {
        if (timeLeft > 7) {
            osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_ALLOFFLINE.");
            thrd_Sleep(5000);
            
	    if (fidp) {	/* Not a VLDB call */
		if (!serversp) {
		    code = cm_GetServerList(fidp, userp, reqp, &serverspp);
		    if (code == 0) {
			serversp = *serverspp;
			free_svr_list = 1;
		    }
		}
		if (serversp) {
		    lock_ObtainWrite(&cm_serverLock);
		    for (tsrp = serversp; tsrp; tsrp=tsrp->next)
			tsrp->status = not_busy;
		    lock_ReleaseWrite(&cm_serverLock);
		    if (free_svr_list) {
			cm_FreeServerList(&serversp);
			*serverspp = serversp;
		    }
		    retry = 1;
		}

                cm_ForceUpdateVolume(fidp, userp, reqp);
	    } else { /* VLDB call */
		if (serversp) {
		    lock_ObtainWrite(&cm_serverLock);
		    for (tsrp = serversp; tsrp; tsrp=tsrp->next)
			tsrp->status = not_busy;
		    lock_ReleaseWrite(&cm_serverLock);
		    if (free_svr_list) {
			cm_FreeServerList(&serversp);
			*serverspp = serversp;
		    }
		}
	    }	
        }
    }

    /* if all servers are busy, mark them non-busy and start over */
    else if (errorCode == CM_ERROR_ALLBUSY) {
	osi_Log0(afsd_logp, "cm_Analyze passed CM_ERROR_ALLBUSY.");
        if (timeLeft > 7) {
            thrd_Sleep(5000);
            if (!serversp) {
                code = cm_GetServerList(fidp, userp, reqp, &serverspp);
                if (code == 0) {
                    serversp = *serverspp;
                    free_svr_list = 1;
                }
            }
            lock_ObtainWrite(&cm_serverLock);
            for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
                if (tsrp->status == busy)
                    tsrp->status = not_busy;
            }
            lock_ReleaseWrite(&cm_serverLock);
            if (free_svr_list) {
                cm_FreeServerList(&serversp);
                *serverspp = serversp;
            }
            retry = 1;
        }
    }

    /* special codes:  VBUSY and VRESTARTING */
    else if (errorCode == VBUSY || errorCode == VRESTARTING) {
        if (!serversp) {
            code = cm_GetServerList(fidp, userp, reqp, &serverspp);
            if (code == 0) {
                serversp = *serverspp;
                free_svr_list = 1;
            }
        }
        lock_ObtainWrite(&cm_serverLock);
        for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
            if (tsrp->server == serverp
                 && tsrp->status == not_busy) {
                tsrp->status = busy;
                break;
            }
        }
        lock_ReleaseWrite(&cm_serverLock);
        if (free_svr_list) {
            cm_FreeServerList(&serversp);
            *serverspp = serversp;
        }
        retry = 1;
    }

    /* special codes:  missing volumes */
    else if (errorCode == VNOVOL || errorCode == VMOVED || errorCode == VOFFLINE ||
             errorCode == VSALVAGE || errorCode == VNOSERVICE || errorCode == VIO) 
    {       
        char addr[16];
        char *format;
	DWORD msgID;
        switch ( errorCode ) {
        case VNOVOL:
	    msgID = MSG_SERVER_REPORTS_VNOVOL;
            format = "Server %s reported volume %d as not attached.";
            break;
        case VMOVED:
	    msgID = MSG_SERVER_REPORTS_VMOVED;
            format = "Server %s reported volume %d as moved.";
            break;
        case VOFFLINE:
	    msgID = MSG_SERVER_REPORTS_VOFFLINE;
            format = "Server %s reported volume %d as offline.";
            break;
        case VSALVAGE:
	    msgID = MSG_SERVER_REPORTS_VSALVAGE;
            format = "Server %s reported volume %d as needs salvage.";
            break;
        case VNOSERVICE:
	    msgID = MSG_SERVER_REPORTS_VNOSERVICE;
            format = "Server %s reported volume %d as not in service.";
            break;
        case VIO:
	    msgID = MSG_SERVER_REPORTS_VIO;
            format = "Server %s reported volume %d as temporarily unaccessible.";
            break;
        }

        /* Log server being offline for this volume */
        sprintf(addr, "%d.%d.%d.%d", 
                 ((serverp->addr.sin_addr.s_addr & 0xff)),
                 ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                 ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                 ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24)); 

	osi_Log2(afsd_logp, format, osi_LogSaveString(afsd_logp,addr), fidp->volume);
	LogEvent(EVENTLOG_WARNING_TYPE, msgID, addr, fidp->volume);

        /* Mark server offline for this volume */
        if (!serversp) {
            code = cm_GetServerList(fidp, userp, reqp, &serverspp);
            if (code == 0) {
                serversp = *serverspp;
                free_svr_list = 1;
            }
        }
        for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
            if (tsrp->server == serverp)
                tsrp->status = offline;
        }   
        if (free_svr_list) {
            cm_FreeServerList(&serversp);
            *serverspp = serversp;
        }
        if ( timeLeft > 2 )
            retry = 1;
    } else if ( errorCode == VNOVNODE ) {
	if ( fidp ) {
	    cm_scache_t * scp;
	    osi_Log4(afsd_logp, "cm_Analyze passed VNOVNODE cell %u vol %u vn %u uniq %u.",
		      fidp->cell, fidp->volume, fidp->vnode, fidp->unique);

	    scp = cm_FindSCache(fidp);
	    if (scp) {
		cm_scache_t *pscp = NULL;

		if (scp->fileType != CM_SCACHETYPE_DIRECTORY)
		    pscp = cm_FindSCacheParent(scp);


		lock_ObtainMutex(&scp->mx);
		lock_ObtainWrite(&cm_scacheLock);
		cm_RemoveSCacheFromHashTable(scp);
		lock_ReleaseWrite(&cm_scacheLock);
		scp->flags |= CM_SCACHEFLAG_DELETED;
		lock_ReleaseMutex(&scp->mx);
		cm_ReleaseSCache(scp);

 		if (pscp) {
		    if (cm_HaveCallback(pscp)) {
 			lock_ObtainMutex(&pscp->mx);
 			cm_DiscardSCache(pscp);
 			lock_ReleaseMutex(&pscp->mx);
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
        /* server took longer than hardDeadTime 
         * don't mark server as down but don't retry
         * this is to prevent the SMB session from timing out
         * In addition, we log an event to the event log 
         */

        /* Log server being offline for this volume */
        sprintf(addr, "%d.%d.%d.%d", 
                 ((serverp->addr.sin_addr.s_addr & 0xff)),
                 ((serverp->addr.sin_addr.s_addr & 0xff00)>> 8),
                 ((serverp->addr.sin_addr.s_addr & 0xff0000)>> 16),
                 ((serverp->addr.sin_addr.s_addr & 0xff000000)>> 24)); 

	LogEvent(EVENTLOG_WARNING_TYPE, MSG_RX_HARD_DEAD_TIME_EXCEEDED, addr);
	  
        retry = 0;
        osi_Log1(afsd_logp, "cm_Analyze: hardDeadTime exceeded addr[%s]",
		 osi_LogSaveString(afsd_logp,addr));
    }
    else if (errorCode >= -64 && errorCode < 0) {
        /* mark server as down */
        lock_ObtainMutex(&serverp->mx);
        serverp->flags |= CM_SERVERFLAG_DOWN;
        lock_ReleaseMutex(&serverp->mx);
	cm_ForceNewConnections(serverp);
        if ( timeLeft > 2 )
            retry = 1;
    }
    else if (errorCode == RXKADEXPIRED || errorCode == RXKADBADTICKET) {
        if (!dead_session) {
            lock_ObtainMutex(&userp->mx);
            ucellp = cm_GetUCell(userp, serverp->cellp);
            if (ucellp->ticketp) {
                free(ucellp->ticketp);
                ucellp->ticketp = NULL;
            }
            ucellp->flags &= ~CM_UCELLFLAG_RXKAD;
            ucellp->gen++;
            lock_ReleaseMutex(&userp->mx);
            if ( timeLeft > 2 )
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
	    case VICECONNBAD	   : s = "VICECONNBAD";	       break;
	    case VICETOKENDEAD     : s = "VICETOKENDEAD";      break;
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
    int someBusy = 0, someOffline = 0, allOffline = 1, allBusy = 1, allDown = 1;
    long timeUsed, timeLeft, hardTimeLeft;

    if (serversp == NULL) {
	osi_Log1(afsd_logp, "cm_ConnByMServers returning 0x%x", CM_ERROR_NOSUCHVOLUME);
	return CM_ERROR_NOSUCHVOLUME;
    }

    *connpp = NULL;

    timeUsed = (GetTickCount() - reqp->startTime) / 1000;
        
    /* leave 5 seconds margin of safety */
    timeLeft =  ConnDeadtimeout - timeUsed - 5;
    hardTimeLeft = HardDeadtimeout - timeUsed - 5;

    lock_ObtainWrite(&cm_serverLock);
    for (tsrp = serversp; tsrp; tsrp=tsrp->next) {
        tsp = tsrp->server;
        cm_GetServerNoLock(tsp);
        lock_ReleaseWrite(&cm_serverLock);
        if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
	    allDown = 0;
            if (tsrp->status == busy) {
		allOffline = 0;
                someBusy = 1;
            } else if (tsrp->status == offline) {
		allBusy = 0;
		someOffline = 1;
            } else {
		allOffline = 0;
                allBusy = 0;
                code = cm_ConnByServer(tsp, usersp, connpp);
                if (code == 0) {        /* cm_CBS only returns 0 */
                    cm_PutServer(tsp);
                    /* Set RPC timeout */
                    if (timeLeft > ConnDeadtimeout)
                        timeLeft = ConnDeadtimeout;

                    if (hardTimeLeft > HardDeadtimeout) 
                        hardTimeLeft = HardDeadtimeout;

                    lock_ObtainMutex(&(*connpp)->mx);
                    rx_SetConnDeadTime((*connpp)->callp, timeLeft);
                    rx_SetConnHardDeadTime((*connpp)->callp, (u_short) hardTimeLeft);
                    lock_ReleaseMutex(&(*connpp)->mx);
                    return 0;
                }
                
                /* therefore, this code is never executed */
                if (firstError == 0)
                    firstError = code;
            }
        }
        lock_ObtainWrite(&cm_serverLock);
        cm_PutServerNoLock(tsp);
    }   
    lock_ReleaseWrite(&cm_serverLock);

    if (firstError == 0) {
        if (allDown) 
            firstError = CM_ERROR_ALLDOWN;
        else if (allBusy) 
            firstError = CM_ERROR_ALLBUSY;
	else if (allOffline || (someBusy && someOffline))
	    firstError = CM_ERROR_ALLOFFLINE;
        else {
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
            tcp->cryptlevel = rxkad_crypt;
        } else {
            tcp->cryptlevel = rxkad_clear;
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
    osi_assert(secObjp != NULL);
    tcp->callp = rx_NewConnection(serverp->addr.sin_addr.s_addr,
                                  port,
                                  serviceID,
                                  secObjp,
                                  secIndex);
    rx_SetConnDeadTime(tcp->callp, ConnDeadtimeout);
    rx_SetConnHardDeadTime(tcp->callp, HardDeadtimeout);
    tcp->ucgen = ucellp->gen;
    if (secObjp)
        rxs_Release(secObjp);   /* Decrement the initial refCount */
}

long cm_ConnByServer(cm_server_t *serverp, cm_user_t *userp, cm_conn_t **connpp)
{
    cm_conn_t *tcp;
    cm_ucell_t *ucellp;

    lock_ObtainMutex(&userp->mx);
    lock_ObtainWrite(&cm_connLock);
    for (tcp = serverp->connsp; tcp; tcp=tcp->nextp) {
        if (tcp->userp == userp) 
            break;
    }
    
    /* find ucell structure */
    ucellp = cm_GetUCell(userp, serverp->cellp);
    if (!tcp) {
        cm_GetServer(serverp);
        tcp = malloc(sizeof(*tcp));
        memset(tcp, 0, sizeof(*tcp));
        tcp->nextp = serverp->connsp;
        serverp->connsp = tcp;
        cm_HoldUser(userp);
        tcp->userp = userp;
        lock_InitializeMutex(&tcp->mx, "cm_conn_t mutex");
        lock_ObtainMutex(&tcp->mx);
        tcp->serverp = serverp;
        tcp->cryptlevel = rxkad_clear;
        cm_NewRXConnection(tcp, ucellp, serverp);
        tcp->refCount = 1;
        lock_ReleaseMutex(&tcp->mx);
    } else {
        if ((tcp->flags & CM_CONN_FLAG_FORCE_NEW) ||
            (tcp->ucgen < ucellp->gen) ||
            (tcp->cryptlevel != (cryptall ? (ucellp->flags & CM_UCELLFLAG_RXKAD ? rxkad_crypt : rxkad_clear) : rxkad_clear)))
        {
            if (tcp->ucgen < ucellp->gen)
                osi_Log0(afsd_logp, "cm_ConnByServer replace connection due to token update");
            else
                osi_Log0(afsd_logp, "cm_ConnByServer replace connection due to crypt change");
            lock_ObtainMutex(&tcp->mx);
	    tcp->flags &= ~CM_CONN_FLAG_FORCE_NEW;
            rx_DestroyConnection(tcp->callp);
            cm_NewRXConnection(tcp, ucellp, serverp);
            lock_ReleaseMutex(&tcp->mx);
        }
        tcp->refCount++;
    }
    lock_ReleaseWrite(&cm_connLock);
    lock_ReleaseMutex(&userp->mx);

    /* return this pointer to our caller */
    osi_Log1(afsd_logp, "cm_ConnByServer returning conn 0x%p", tcp);
    *connpp = tcp;

    return 0;
}

long cm_Conn(struct cm_fid *fidp, struct cm_user *userp, cm_req_t *reqp,
             cm_conn_t **connpp)
{
    long code;

    cm_serverRef_t **serverspp;

    code = cm_GetServerList(fidp, userp, reqp, &serverspp);
    if (code) {
        *connpp = NULL;
        return code;
    }

    code = cm_ConnByMServers(*serverspp, userp, reqp, connpp);
    cm_FreeServerList(serverspp);
    return code;
}

extern struct rx_connection * 
cm_GetRxConn(cm_conn_t *connp)
{
    struct rx_connection * rxconn;
    lock_ObtainMutex(&connp->mx);
    rxconn = connp->callp;
    rx_GetConnection(rxconn);
    lock_ReleaseMutex(&connp->mx);
    return rxconn;
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
