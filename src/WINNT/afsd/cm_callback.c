/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/afs_args.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif /* !DJGPP */
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include <osi.h>

#include "afsd.h"

/*extern void afsi_log(char *pattern, ...);*/

/* read/write lock for all global storage in this module */
osi_rwlock_t cm_callbackLock;

#ifdef AFS_FREELANCE_CLIENT
extern osi_mutex_t cm_Freelance_Lock;
#endif

/* count of # of callback breaking messages received by this CM so far.  We use
 * this count in determining whether there have been any callback breaks that
 * apply to a call that returned a new callback.  If the counter doesn't
 * increase during a call, then we know that no callbacks were broken during
 * that call, and thus that the callback that was just returned is still valid.
 */
long cm_callbackCount;

/* count of number of RPCs potentially returning a callback executing now.
 * When this counter hits zero, we can clear out the racing revokes list, since
 * at that time, we know that none of the just-executed callback revokes will
 * apply to any future call that returns a callback (since the latter hasn't
 * even started execution yet).
 */
long cm_activeCallbackGrantingCalls;

/* list of callbacks that have been broken recently.  If a call returning a
 * callback is executing and a callback revoke runs immediately after it at the
 * server, the revoke may end up being processed before the response to the
 * original callback granting call.  We detect this by keeping a list of
 * callback revokes that have been received since we *started* the callback
 * granting call, and discarding any callbacks received for the same file ID,
 * even if the callback revoke was received before the callback grant.
 */
cm_racingRevokes_t *cm_racingRevokesp;

/* record a (potentially) racing revoke for this file ID; null means for all
 * file IDs, and is used by InitCallBackState.
 *
 * The cancelFlags describe whether we're just discarding callbacks for the same
 * file ID, the same volume, or all from the same server.
 *
 * Called with no locks held.
 */
void cm_RecordRacingRevoke(cm_fid_t *fidp, long cancelFlags)
{
	cm_racingRevokes_t *rp;

	lock_ObtainWrite(&cm_callbackLock);

    osi_Log3(afsd_logp, "RecordRacingRevoke Volume %d Flags %lX activeCalls %d",
             fidp->volume, cancelFlags, cm_activeCallbackGrantingCalls);

	if (cm_activeCallbackGrantingCalls > 0) {
		rp = malloc(sizeof(*rp));
	        memset(rp, 0, sizeof(*rp));
	        osi_QAdd((osi_queue_t **) &cm_racingRevokesp, &rp->q);
                rp->flags |= (cancelFlags & CM_RACINGFLAG_ALL);
		if (fidp) rp->fid = *fidp;
                rp->callbackCount = ++cm_callbackCount;
	}
	lock_ReleaseWrite(&cm_callbackLock);
}

/*
 * When we lose a callback, may have to send change notification replies.
 */
void cm_CallbackNotifyChange(cm_scache_t *scp)
{
    osi_Log2(afsd_logp, "CallbackNotifyChange FileType %d Flags %lX",
              scp->fileType, scp->flags);

	if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
		if (scp->flags & CM_SCACHEFLAG_ANYWATCH)
			smb_NotifyChange(0,
			 FILE_NOTIFY_GENERIC_DIRECTORY_FILTER,
			 scp, NULL, NULL, TRUE);
	} else {
		cm_fid_t tfid;
		cm_scache_t *dscp;

		tfid.cell = scp->fid.cell;
		tfid.volume = scp->fid.volume;
		tfid.vnode = scp->parentVnode;
		tfid.unique = scp->parentUnique;
		dscp = cm_FindSCache(&tfid);
		if (dscp &&
			dscp->flags & CM_SCACHEFLAG_ANYWATCH)
			smb_NotifyChange(0,
			 FILE_NOTIFY_GENERIC_FILE_FILTER,
			 dscp, NULL, NULL, TRUE);
		if (dscp) cm_ReleaseSCache(dscp);
	}
}

/* called with no locks held for every file ID that is revoked directly by
 * a callback revoke call.  Does not have to handle volume callback breaks,
 * since those have already been split out.
 *
 * The callp parameter is currently unused.
 */
void cm_RevokeCallback(struct rx_call *callp, AFSFid *fidp)
{
	cm_fid_t tfid;
        cm_scache_t *scp;
        long hash;
        
	/* don't bother setting cell, since we won't be checking it (to aid
         * in working with multi-homed servers: we don't know the cell if we
         * don't recognize the IP address).
         */
	tfid.cell = 0;
        tfid.volume = fidp->Volume;
        tfid.vnode = fidp->Vnode;
        tfid.unique = fidp->Unique;
        hash = CM_SCACHE_HASH(&tfid);

    osi_Log3(afsd_logp, "RevokeCallback vol %d vn %d un %d",
		 fidp->Volume, fidp->Vnode, fidp->Unique);
        
	/* do this first, so that if we're executing a callback granting call
         * at this moment, we kill it before it can be merged in.  Otherwise,
         * it could complete while we're doing the scan below, and get missed
         * by both the scan and by this code.
         */
	cm_RecordRacingRevoke(&tfid, 0);

	lock_ObtainWrite(&cm_scacheLock);
	/* do all in the hash bucket, since we don't know how many we'll find with
         * varying cells.
         */
        for(scp = cm_hashTablep[hash]; scp; scp=scp->nextp) {
		if (scp->fid.volume == tfid.volume &&
                	scp->fid.vnode == tfid.vnode &&
                        scp->fid.unique == tfid.unique) {
			scp->refCount++;
			lock_ReleaseWrite(&cm_scacheLock);
            osi_Log1(afsd_logp, "Discarding SCache scp %x", scp);
                        lock_ObtainMutex(&scp->mx);
			cm_DiscardSCache(scp);
                        lock_ReleaseMutex(&scp->mx);
			cm_CallbackNotifyChange(scp);
                        lock_ObtainWrite(&cm_scacheLock);
                        scp->refCount--;
		}
        }
	lock_ReleaseWrite(&cm_scacheLock);
}

/* called to revoke a volume callback, which is typically issued when a volume
 * is moved from one server to another.
 *
 * Called with no locks held.
 */
void cm_RevokeVolumeCallback(struct rx_call *callp, AFSFid *fidp)
{
	long hash;
        cm_scache_t *scp;
        cm_fid_t tfid;

    osi_Log1(afsd_logp, "RevokeVolumeCallback %d", fidp->Volume);

	/* do this first, so that if we're executing a callback granting call
         * at this moment, we kill it before it can be merged in.  Otherwise,
         * it could complete while we're doing the scan below, and get missed
         * by both the scan and by this code.
         */
	tfid.cell = tfid.vnode = tfid.unique = 0;
        tfid.volume = fidp->Volume;
        cm_RecordRacingRevoke(&tfid, CM_RACINGFLAG_CANCELVOL);


        lock_ObtainWrite(&cm_scacheLock);
	for(hash = 0; hash < cm_hashTableSize; hash++) {
		for(scp=cm_hashTablep[hash]; scp; scp=scp->nextp) {
			if (scp->fid.volume == fidp->Volume) {
				scp->refCount++;
	                        lock_ReleaseWrite(&cm_scacheLock);
	                        lock_ObtainMutex(&scp->mx);
                osi_Log1(afsd_logp, "Discarding SCache scp %x", scp);
				cm_DiscardSCache(scp);
				lock_ReleaseMutex(&scp->mx);
				cm_CallbackNotifyChange(scp);
	                        lock_ObtainWrite(&cm_scacheLock);
	                        scp->refCount--;
	                }
		}	/* search one hash bucket */
	}	/* search all hash buckets */
        
        lock_ReleaseWrite(&cm_scacheLock);
}

/* handle incoming RPC callback breaking message.
 * Called with no locks held.
 */
SRXAFSCB_CallBack(struct rx_call *callp, AFSCBFids *fidsArrayp, AFSCBs *cbsArrayp)
{
        int i;
        AFSFid *tfidp;
        
    osi_Log0(afsd_logp, "SRXAFSCB_CallBack");

        for(i=0; i < (long) fidsArrayp->AFSCBFids_len; i++) {
		tfidp = &fidsArrayp->AFSCBFids_val[i];
                
        if (tfidp->Volume == 0)
            continue;   /* means don't do anything */
                else if (tfidp->Vnode == 0)
                	cm_RevokeVolumeCallback(callp, tfidp);
        else
            cm_RevokeCallback(callp, tfidp);
        }

	return 0;
}

/* called with no locks by RPC system when a server indicates that it has never
 * heard from us, or for other reasons has had to discard callbacks from us
 * without telling us, e.g. a network partition.
 */
SRXAFSCB_InitCallBackState(struct rx_call *callp)
{
    struct sockaddr_in taddr;
    cm_server_t *tsp;
    cm_scache_t *scp;
    int hash;
    int discarded;

    osi_Log0(afsd_logp, "SRXAFSCB_InitCallBackState");

    if ((rx_ConnectionOf(callp)) && (rx_PeerOf(rx_ConnectionOf(callp)))) {
	taddr.sin_family = AF_INET;
	taddr.sin_addr.s_addr = rx_HostOf(rx_PeerOf(rx_ConnectionOf(callp)));

	tsp = cm_FindServer(&taddr, CM_SERVER_FILE);

	osi_Log1(afsd_logp, "Init Callback State server %x", tsp);
	
	/* record the callback in the racing revokes structure.  This
	 * shouldn't be necessary, since we shouldn't be making callback
	 * granting calls while we're going to get an initstate call,
	 * but there probably are some obscure races, so better safe
	 * than sorry.
	 *
	 * We do this first since we don't hold the cm_scacheLock and vnode
	 * locks over the entire callback scan operation below.  The
	 * big loop below is guaranteed to hit any callback already
	 * processed.  The call to RecordRacingRevoke is guaranteed
	 * to kill any callback that is currently being returned.
	 * Anything that sneaks past both must start
	 * after the call to RecordRacingRevoke.
	 */
	cm_RecordRacingRevoke(NULL, CM_RACINGFLAG_CANCELALL);
	
	/* now search all vnodes looking for guys with this callback, if we
	 * found it, or guys with any callbacks, if we didn't find the server
	 * (that's how multihomed machines will appear and how we'll handle
	 * them, albeit a little inefficiently).  That is, we're discarding all
	 * callbacks from all hosts if we get an initstate call from an unknown
	 * host.  Since these calls are rare, and multihomed servers
	 * are "rare," hopefully this won't be a problem.
	 */
	lock_ObtainWrite(&cm_scacheLock);
	for(hash = 0; hash < cm_hashTableSize; hash++) {
		for(scp=cm_hashTablep[hash]; scp; scp=scp->nextp) {
			scp->refCount++;
                        lock_ReleaseWrite(&cm_scacheLock);
                        lock_ObtainMutex(&scp->mx);
			discarded = 0;
			if (scp->cbServerp != NULL) {
				/* we have a callback, now decide if we should clear it */
				if (scp->cbServerp == tsp || tsp == NULL) {
                        osi_Log1(afsd_logp, "Discarding SCache scp %x", scp);
					cm_DiscardSCache(scp);
					discarded = 1;
				}
			}
			lock_ReleaseMutex(&scp->mx);
			if (discarded)
				cm_CallbackNotifyChange(scp);
                        lock_ObtainWrite(&cm_scacheLock);
                        scp->refCount--;
		}	/* search one hash bucket */
	}	/* search all hash buckets */
	
	lock_ReleaseWrite(&cm_scacheLock);
	
	/* we're done with the server structure */
	if (tsp) cm_PutServer(tsp);
    }

    return 0;
}

/* just returns if we're up */
SRXAFSCB_Probe(struct rx_call *callp)
{
    osi_Log0(afsd_logp, "SRXAFSCB_Probe - not implemented");
	return 0;
}

/* debug interface: not implemented */
SRXAFSCB_GetCE64(struct rx_call *callp, long index, AFSDBCacheEntry *cep)
{
    /* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_GetCE64 - not implemented");
    return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_GetLock(struct rx_call *callp, long index, AFSDBLock *lockp)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_GetLock - not implemented");
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_GetCE(struct rx_call *callp, long index, AFSDBCacheEntry *cep)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_GetCE - not implemented");
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_XStatsVersion(struct rx_call *callp, long *vp)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_XStatsVersion - not implemented");
	*vp = -1;
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_GetXStats(struct rx_call *callp, long cvn, long coln, long *srvp, long *timep,
	AFSCB_CollData *datap)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_GetXStats - not implemented");
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_InitCallBackState2(struct rx_call *callp, struct interfaceAddr* addr)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_InitCallBackState2 - not implemented");
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_WhoAreYou(struct rx_call *callp, struct interfaceAddr* addr)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_WhoAreYou - not implemented");
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_InitCallBackState3(struct rx_call *callp, afsUUID* serverUuid)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_InitCallBackState3 - not implemented");
	return RXGEN_OPCODE;
}

/* debug interface: not implemented */
SRXAFSCB_ProbeUuid(struct rx_call *callp, afsUUID* clientUuid)
{
	/* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_ProbeUuid - not implemented");
	return RXGEN_OPCODE;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetServerPrefs
 *
 * Description:
 *      Routine to list server preferences used by this client.
 *
 * Arguments:
 *      a_call  : Ptr to Rx call on which this request came in.
 *      a_index : Input server index
 *      a_srvr_addr  : Output server address (0xffffffff on last server)
 *      a_srvr_rank  : Output server rank
 *
 * Returns:
 *      0 on success
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetServerPrefs(
    struct rx_call *a_call,
    afs_int32 a_index,
    afs_int32 *a_srvr_addr,
    afs_int32 *a_srvr_rank)
{
    osi_Log0(afsd_logp, "SRXAFSCB_GetServerPrefs - not implemented");

    *a_srvr_addr = 0xffffffff;
    *a_srvr_rank = 0xffffffff;
    return 0;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCellServDB
 *
 * Description:
 *      Routine to list cells configured for this client
 *
 * Arguments:
 *      a_call  : Ptr to Rx call on which this request came in.
 *      a_index : Input cell index
 *      a_name  : Output cell name ("" on last cell)
 *      a_hosts : Output cell database servers
 *
 * Returns:
 *      0 on success
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetCellServDB(
    struct rx_call *a_call,
    afs_int32 a_index,
    char **a_name,
    serverList *a_hosts)
{
    char *t_name;

    osi_Log0(afsd_logp, "SRXAFSCB_GetCellServDB - not implemented");

    t_name = (char *)malloc(AFSNAMEMAX);
    t_name[0] = '\0';
    *a_name = t_name;
    a_hosts->serverList_len = 0;
    return 0;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetLocalCell
 *
 * Description:
 *      Routine to return name of client's local cell
 *
 * Arguments:
 *      a_call  : Ptr to Rx call on which this request came in.
 *      a_name  : Output cell name
 *
 * Returns:
 *      0 on success
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetLocalCell(
    struct rx_call *a_call,
    char **a_name)
{
    char *t_name;

    osi_Log0(afsd_logp, "SRXAFSCB_GetLocalCell");

    if (cm_rootCellp) {
	t_name = (char *)malloc(strlen(cm_rootCellp->namep)+1);
        strcpy(t_name, cm_rootCellp->namep);
    } else {
	t_name = (char *)malloc(1);
	t_name[0] = '\0';
    }
    *a_name = t_name;
    return 0;
}


/*
 * afs_MarshallCacheConfig - marshall client cache configuration
 *
 * PARAMETERS
 *
 * IN callerVersion - the rpc stat version of the caller.
 *
 * IN config - client cache configuration.
 *
 * OUT ptr - buffer where configuration is marshalled.
 *
 * RETURN CODES
 *
 * Returns void.
 */
static void afs_MarshallCacheConfig(
    afs_uint32 callerVersion,
    cm_initparams_v1 *config,
    afs_uint32 *ptr)
{
    /*
     * We currently only support version 1.
     */
    *(ptr++) = config->nChunkFiles;
    *(ptr++) = config->nStatCaches;
    *(ptr++) = config->nDataCaches;
    *(ptr++) = config->nVolumeCaches;
    *(ptr++) = config->firstChunkSize;
    *(ptr++) = config->otherChunkSize;
    *(ptr++) = config->cacheSize;
    *(ptr++) = config->setTime;
    *(ptr++) = config->memCache;

}
 

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetCacheConfig
 *
 * Description:
 *	Routine to return parameters used to initialize client cache.
 *      Client may request any format version. Server may not return
 *      format version greater than version requested by client.
 *
 * Arguments:
 *	a_call:        Ptr to Rx call on which this request came in.
 *	callerVersion: Data format version desired by the client.
 *	serverVersion: Data format version of output data.
 *      configCount:   Number bytes allocated for output data.
 *      config:        Client cache configuration.
 *
 * Returns:
 *	0 on success
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	As advertised.
 *------------------------------------------------------------------------*/

int SRXAFSCB_GetCacheConfig(a_call, callerVersion, serverVersion,
			    configCount, config)
struct rx_call *a_call;
afs_uint32 callerVersion;
afs_uint32 *serverVersion;
afs_uint32 *configCount;
cacheConfig *config;
{
    afs_uint32 *t_config;
    size_t allocsize;
    extern cm_initparams_v1 cm_initParams;

    osi_Log0(afsd_logp, "SRXAFSCB_GetCacheConfig - version 1 only");

    /*
     * Currently only support version 1
     */
    allocsize = sizeof(cm_initparams_v1);
    t_config = (afs_uint32 *)malloc(allocsize);

    afs_MarshallCacheConfig(callerVersion, &cm_initParams, t_config);

    *serverVersion = AFS_CLIENT_RETRIEVAL_FIRST_EDITION;
    *configCount = allocsize;
    config->cacheConfig_val = t_config;
    config->cacheConfig_len = allocsize/sizeof(afs_uint32);

    return 0;
}

/* called by afsd without any locks to initialize this module */
void cm_InitCallback(void)
{
	lock_InitializeRWLock(&cm_callbackLock, "cm_callbackLock");
        cm_activeCallbackGrantingCalls = 0;
}

/* called with locked scp; tells us whether we've got a callback.
 * Expirations are checked by a background daemon so as to make
 * this function as inexpensive as possible
 */
int cm_HaveCallback(cm_scache_t *scp)
{
#ifdef AFS_FREELANCE_CLIENT
    // yj: we handle callbacks specially for callbacks on the root directory
    // Since it's local, we almost always say that we have callback on it
    // The only time we send back a 0 is if we're need to initialize or
    // reinitialize the fake directory

    // There are 2 state variables cm_fakeGettingCallback and cm_fakeDirCallback
    // cm_fakeGettingCallback is 1 if we're in the process of initialization and
    // hence should return false. it's 0 otherwise
    // cm_fakeDirCallback is 0 if we haven't loaded the fake directory, it's 1
    // if the fake directory is loaded and this is the first time cm_HaveCallback
    // is called since then. We return false in this case to allow cm_GetCallback
    // to be called because cm_GetCallback has some initialization work to do.
    // If cm_fakeDirCallback is 2, then it means that the fake directory is in
    // good shape and we simply return true, provided no change is detected.
  int fdc, fgc;

    if (cm_freelanceEnabled && 
         scp->fid.cell==AFS_FAKE_ROOT_CELL_ID && scp->fid.volume==AFS_FAKE_ROOT_VOL_ID) {
        /* if it's something on /afs */
        if (!(scp->fid.vnode==0x1 && scp->fid.unique==0x1)) {
            /* if it's not root.afs */
	    return 1;
        }

	    lock_ObtainMutex(&cm_Freelance_Lock);
	    fdc = cm_fakeDirCallback;
	    fgc = cm_fakeGettingCallback;
	    lock_ReleaseMutex(&cm_Freelance_Lock);
	    
	    if (fdc==1) {	// first call since init
		return 0;
	    } else if (fdc==2 && !fgc) { 	// we're in good shape
		if (cm_getLocalMountPointChange()) {	// check for changes
		    cm_clearLocalMountPointChange(); // clear the changefile
            lock_ReleaseMutex(&scp->mx);      // this is re-locked in reInitLocalMountPoints
		    cm_reInitLocalMountPoints();	// start reinit
            lock_ObtainMutex(&scp->mx);      // now get the lock back 
		    return 0;
		}
		return 1;			// no change
	    }
	    return 0;
	}
#endif

    if (scp->cbServerp != NULL)
	return 1;
    else 
        return 0;
}

/* need to detect a broken callback that races with our obtaining a callback.
 * Need to be able to do this even if we don't know the file ID of the file
 * we're breaking the callback on at the time we start the acquisition of the
 * callback (as in the case where we are creating a file).
 *
 * So, we start by writing down the count of the # of callbacks we've received
 * so far, and bumping a global counter of the # of callback granting calls
 * outstanding (all done under cm_callbackLock).
 *
 * When we're back from the call, we look at all of the callback revokes with
 * counter numbers greater than the one we recorded in our caller's structure,
 * and replay those that are higher than when we started the call.
 * 
 * We free all the structures in the queue when the count of the # of outstanding
 * callback-granting calls drops to zero.
 *
 * We call this function with the scp locked, too, but in its current implementation,
 * this knowledge is not used.
 */
void cm_StartCallbackGrantingCall(cm_scache_t *scp, cm_callbackRequest_t *cbrp)
{
	lock_ObtainWrite(&cm_callbackLock);
	cbrp->callbackCount = cm_callbackCount;
        cm_activeCallbackGrantingCalls++;
        cbrp->startTime = osi_Time();
        cbrp->serverp = NULL;
	lock_ReleaseWrite(&cm_callbackLock);
}

/* Called at the end of a callback-granting call, to remove the callback
 * info from the scache entry, if necessary.
 *
 * Called with scp locked, so we can discard the callbacks easily with
 * this locking hierarchy.
 */
void cm_EndCallbackGrantingCall(cm_scache_t *scp, cm_callbackRequest_t *cbrp,
	AFSCallBack *cbp, long flags)
{
	cm_racingRevokes_t *revp;		/* where we are */
	cm_racingRevokes_t *nrevp;		/* where we'll be next */
        int freeFlag;
    cm_server_t * serverp = 0;

	lock_ObtainWrite(&cm_callbackLock);
	if (flags & CM_CALLBACK_MAINTAINCOUNT) {
        	osi_assert(cm_activeCallbackGrantingCalls > 0);
	}
	else {
		osi_assert(cm_activeCallbackGrantingCalls-- > 0);
	}
    if (cm_activeCallbackGrantingCalls == 0) 
        freeFlag = 1;
    else 
        freeFlag = 0;

	/* record the callback; we'll clear it below if we really lose it */
    if (cbrp) {
	if (scp) {
            if (scp->cbServerp != cbrp->serverp) {
                serverp = scp->cbServerp;
            }
	        scp->cbServerp = cbrp->serverp;
	        scp->cbExpires = cbrp->startTime + cbp->ExpirationTime;
        } else {
            serverp = cbrp->serverp;
        }
        cbrp->serverp = NULL;
	}

	/* a callback was actually revoked during our granting call, so
	 * run down the list of revoked fids, looking for ours.
	 * If activeCallbackGrantingCalls is zero, free the elements, too.
	 *
         * May need to go through entire list just to do the freeing.
	 */
	for(revp = cm_racingRevokesp; revp; revp = nrevp) {
		nrevp = (cm_racingRevokes_t *) osi_QNext(&revp->q);
		/* if this callback came in later than when we started the
                 * callback-granting call, and if this fid is the right fid,
                 * then clear the callback.
                 */
        if (scp && cbrp && cbrp->callbackCount != cm_callbackCount
                       	&& revp->callbackCount > cbrp->callbackCount
             && (( scp->fid.volume == revp->fid.volume &&
                                 scp->fid.vnode == revp->fid.vnode &&
                                 scp->fid.unique == revp->fid.unique)
                            ||
                                ((revp->flags & CM_RACINGFLAG_CANCELVOL) &&
                                 scp->fid.volume == revp->fid.volume)
                            ||
                            	(revp->flags & CM_RACINGFLAG_CANCELALL))) {
			/* this one matches */
			osi_Log4(afsd_logp,
			"Racing revoke scp %x old cbc %d rev cbc %d cur cbc %d",
				 scp,
				 cbrp->callbackCount, revp->callbackCount,
				 cm_callbackCount);
			cm_DiscardSCache(scp);
			/*
			 * Since we don't have a callback to preserve, it's
			 * OK to drop the lock and re-obtain it.
			 */
			lock_ReleaseMutex(&scp->mx);
			cm_CallbackNotifyChange(scp);
			lock_ObtainMutex(&scp->mx);
                }
                if (freeFlag) free(revp);
        }

	/* if we freed the list, zap the pointer to it */
	if (freeFlag) cm_racingRevokesp = NULL;

	lock_ReleaseWrite(&cm_callbackLock);

    if ( serverp ) {
        lock_ObtainWrite(&cm_serverLock);
        cm_FreeServer(serverp);
        lock_ReleaseWrite(&cm_serverLock);
    }
}

/* if flags is 1, we want to force the code to make one call, anyway.
 * called with locked scp; returns with same.
 */
long cm_GetCallback(cm_scache_t *scp, struct cm_user *userp,
	struct cm_req *reqp, long flags)
{
	long code;
    cm_conn_t *connp;
    AFSFetchStatus afsStatus;
    AFSVolSync volSync;
    AFSCallBack callback;
    AFSFid tfid;
    cm_callbackRequest_t cbr;
    int mustCall;
    long sflags;
    cm_fid_t sfid;

    osi_Log2(afsd_logp, "GetCallback scp %x flags %lX", scp, flags);

#ifdef AFS_FREELANCE_CLIENT
	// The case where a callback is needed on /afs is handled
	// specially. We need to fetch the status by calling
	// cm_MergeStatus and mark that cm_fakeDirCallback is 2
	if (cm_freelanceEnabled) {
        if (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
             scp->fid.volume==AFS_FAKE_ROOT_VOL_ID &&
             scp->fid.unique==0x1 &&
             scp->fid.vnode==0x1) {
            
            // Start by indicating that we're in the process
            // of fetching the callback
            lock_ObtainMutex(&cm_Freelance_Lock);
            osi_Log0(afsd_logp,"cm_getGetCallback fakeGettingCallback=1");
            cm_fakeGettingCallback = 1;
            lock_ReleaseMutex(&cm_Freelance_Lock);

            // Fetch the status info 
            cm_MergeStatus(scp, &afsStatus, &volSync, userp, 0);

            // Indicate that the callback is not done
            lock_ObtainMutex(&cm_Freelance_Lock);
            osi_Log0(afsd_logp,"cm_getGetCallback fakeDirCallback=2");
            cm_fakeDirCallback = 2;

            // Indicate that we're no longer fetching the callback
            osi_Log0(afsd_logp,"cm_getGetCallback fakeGettingCallback=0");
            cm_fakeGettingCallback = 0;
            lock_ReleaseMutex(&cm_Freelance_Lock);

            return 0;
        }

        if (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID && scp->fid.volume==AFS_FAKE_ROOT_VOL_ID) {
            osi_Log0(afsd_logp,"cm_getcallback should NEVER EVER get here... ");
        }
    }
#endif /* AFS_FREELANCE_CLIENT */
	
	mustCall = (flags & 1);
	cm_AFSFidFromFid(&tfid, &scp->fid);
	while (1) {
		if (!mustCall && cm_HaveCallback(scp)) return 0;

        /* turn off mustCall, since it has now forced us past the check above */
        mustCall = 0;

        /* otherwise, we have to make an RPC to get the status */
		sflags = CM_SCACHESYNC_FETCHSTATUS | CM_SCACHESYNC_GETCALLBACK;
        cm_SyncOp(scp, NULL, NULL, NULL, 0, sflags);
        cm_StartCallbackGrantingCall(scp, &cbr);
        sfid = scp->fid;
		lock_ReleaseMutex(&scp->mx);
		
		/* now make the RPC */
		osi_Log1(afsd_logp, "CALL FetchStatus vp %x", (long) scp);
        do {
			code = cm_Conn(&sfid, userp, reqp, &connp);
            if (code) continue;
		
            code = RXAFS_FetchStatus(connp->callp, &tfid,
                                     &afsStatus, &callback, &volSync);

		} while (cm_Analyze(connp, userp, reqp, &sfid, &volSync, NULL,
                            &cbr, code));
        code = cm_MapRPCError(code, reqp);
		osi_Log0(afsd_logp, "CALL FetchStatus DONE");

		lock_ObtainMutex(&scp->mx);
        cm_SyncOpDone(scp, NULL, sflags);
		if (code == 0) {
            cm_EndCallbackGrantingCall(scp, &cbr, &callback, 0);
            cm_MergeStatus(scp, &afsStatus, &volSync, userp, 0);
		}   
        else
            cm_EndCallbackGrantingCall(NULL, &cbr, NULL, 0);

        /* now check to see if we got an error */
        if (code) return code;
    }
}

/* called periodically by cm_daemon to shut down use of expired callbacks */
void cm_CheckCBExpiration(void)
{
	int i;
        cm_scache_t *scp;
        long now;
        
    osi_Log0(afsd_logp, "CheckCBExpiration");

	now = osi_Time();
	lock_ObtainWrite(&cm_scacheLock);
        for(i=0; i<cm_hashTableSize; i++) {
		for(scp = cm_hashTablep[i]; scp; scp=scp->nextp) {
			scp->refCount++;
			lock_ReleaseWrite(&cm_scacheLock);
			lock_ObtainMutex(&scp->mx);
            if (scp->cbExpires > 0 && (scp->cbServerp == NULL || now > scp->cbExpires)) {
                osi_Log1(afsd_logp, "Callback Expiration Discarding SCache scp %x", scp);
				cm_DiscardSCache(scp);
                        }
			lock_ReleaseMutex(&scp->mx);
			lock_ObtainWrite(&cm_scacheLock);
                        osi_assert(scp->refCount-- > 0);
                }
        }
        lock_ReleaseWrite(&cm_scacheLock);
}

/* debug interface: not implemented */
int SRXAFSCB_GetCellByNum(struct rx_call *a_call, afs_int32 a_cellnum,
			  char **a_name, serverList *a_hosts)
{
    /* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_GetCellByNum - not implemented");
    return RXGEN_OPCODE;
}

/* debug interface: not implemented */
int SRXAFSCB_TellMeAboutYourself(struct rx_call *a_call, afs_int32 a_cellnum,
                          char **a_name, serverList *a_hosts)
{
    /* XXXX */
    osi_Log0(afsd_logp, "SRXAFSCB_TellMeAboutYourself - not implemented");
    return RXGEN_OPCODE;
}
