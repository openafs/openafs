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

#include <windows.h>
#include <winsock2.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include "afsd.h"
#include "smb.h"
#include <osi.h>
#include <rx_pthread.h>

#include <WINNT/syscfg.h>
#include <WINNT/afsreg.h>

int
SRXAFSCB_InitCallBackState3(struct rx_call *callp, afsUUID* serverUuid);

/* read/write lock for all global storage in this module */
osi_rwlock_t cm_callbackLock;

afs_int32 cm_OfflineROIsValid = 0;

afs_int32 cm_giveUpAllCBs = 0;

afs_int32 cm_shutdown = 0;

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
		fidp ? fidp->volume : 0, cancelFlags, cm_activeCallbackGrantingCalls);

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
 * Do not call with a lock on the scp.
 */
void cm_CallbackNotifyChange(cm_scache_t *scp)
{
    DWORD dwDelay = 0;
    HKEY  hKey;
    DWORD dummyLen;

    /* why does this have to query the registry each time? */
	if (RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                      AFSREG_CLT_OPENAFS_SUBKEY,
                      0,
                      KEY_READ|KEY_QUERY_VALUE,
                      &hKey) == ERROR_SUCCESS) {

        dummyLen = sizeof(DWORD);
        RegQueryValueEx(hKey, "CallBack Notify Change Delay", NULL, NULL,
                        (BYTE *) &dwDelay, &dummyLen);
        RegCloseKey(hKey);
    }

    if (dwDelay > 5000)    /* do not allow a delay of more then 5 seconds */
        dwDelay = 5000;

    osi_Log3(afsd_logp, "CallbackNotifyChange FileType %d Flags %lX Delay %dms",
              scp->fileType, scp->flags, dwDelay);

    if (dwDelay)
        Sleep(dwDelay);

    /* for directories, this sends a change notification on the dir itself */
    if (scp->fileType == CM_SCACHETYPE_DIRECTORY) {
        if (scp->flags & CM_SCACHEFLAG_ANYWATCH)
            smb_NotifyChange(0,
                             FILE_NOTIFY_GENERIC_DIRECTORY_FILTER,
                             scp, NULL, NULL, TRUE);
    } else {
	/* and for files, this sends a change notification on the file's parent dir */
        cm_fid_t tfid;
        cm_scache_t *dscp;

        cm_SetFid(&tfid, scp->fid.cell, scp->fid.volume, scp->parentVnode, scp->parentUnique);
        dscp = cm_FindSCache(&tfid);
        if ( dscp &&
             dscp->flags & CM_SCACHEFLAG_ANYWATCH )
            smb_NotifyChange( 0,
                              FILE_NOTIFY_GENERIC_FILE_FILTER,
                              dscp, NULL, NULL, TRUE);
        if (dscp)
            cm_ReleaseSCache(dscp);
    }
}

/* called with no locks held for every file ID that is revoked directly by
 * a callback revoke call.  Does not have to handle volume callback breaks,
 * since those have already been split out.
 *
 * The callp parameter is currently unused.
 */
void cm_RevokeCallback(struct rx_call *callp, cm_cell_t * cellp, AFSFid *fidp)
{
    cm_fid_t tfid;
    cm_scache_t *scp;
    long hash;

    tfid.cell = cellp ? cellp->cellID : 0;
    tfid.volume = fidp->Volume;
    tfid.vnode = fidp->Vnode;
    tfid.unique = fidp->Unique;
    hash = CM_SCACHE_HASH(&tfid);

    osi_Log3(afsd_logp, "RevokeCallback vol %u vn %u uniq %u",
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
    for (scp = cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
        if (scp->fid.volume == tfid.volume &&
             scp->fid.vnode == tfid.vnode &&
             scp->fid.unique == tfid.unique &&
             (cellp == NULL || scp->fid.cell == cellp->cellID) &&
             cm_HaveCallback(scp))
        {
            cm_HoldSCacheNoLock(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            osi_Log4(afsd_logp, "RevokeCallback Discarding SCache scp 0x%p vol %u vn %u uniq %u",
                     scp, scp->fid.volume, scp->fid.vnode, scp->fid.unique);

            lock_ObtainWrite(&scp->rw);
            cm_DiscardSCache(scp);
            lock_ReleaseWrite(&scp->rw);

            cm_CallbackNotifyChange(scp);

            lock_ObtainWrite(&cm_scacheLock);
            cm_ReleaseSCacheNoLock(scp);
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    osi_Log3(afsd_logp, "RevokeCallback Complete vol %u vn %u uniq %u",
             fidp->Volume, fidp->Vnode, fidp->Unique);
}

static __inline void
cm_callbackDiscardROVolumeByFID(cm_fid_t *fidp)
{
    cm_volume_t *volp = cm_GetVolumeByFID(fidp);
    if (volp) {
        cm_PutVolume(volp);
        if (volp->cbExpiresRO) {
            volp->cbExpiresRO = 0;
            if (volp->cbServerpRO) {
                cm_PutServer(volp->cbServerpRO);
                volp->cbServerpRO = NULL;
            }
            volp->creationDateRO = 0;
        }
    }
}

/* called to revoke a volume callback, which is typically issued when a volume
 * is moved from one server to another.
 *
 * Called with no locks held.
 */
void cm_RevokeVolumeCallback(struct rx_call *callp, cm_cell_t *cellp, AFSFid *fidp)
{
    unsigned long hash;
    cm_scache_t *scp;
    cm_fid_t tfid;

    osi_Log1(afsd_logp, "RevokeVolumeCallback vol %d", fidp->Volume);

    /* do this first, so that if we're executing a callback granting call
     * at this moment, we kill it before it can be merged in.  Otherwise,
     * it could complete while we're doing the scan below, and get missed
     * by both the scan and by this code.
     */
    tfid.cell = cellp ? cellp->cellID : 0;
    tfid.volume = fidp->Volume;
    tfid.vnode = tfid.unique = 0;

    cm_RecordRacingRevoke(&tfid, CM_RACINGFLAG_CANCELVOL);

    lock_ObtainWrite(&cm_scacheLock);
    for (hash = 0; hash < cm_data.scacheHashTableSize; hash++) {
        for(scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
            if (scp->fid.volume == fidp->Volume &&
                (cellp == NULL || scp->fid.cell == cellp->cellID) &&
                 scp->cbExpires > 0 &&
                 scp->cbServerp != NULL) {
                cm_HoldSCacheNoLock(scp);
                lock_ReleaseWrite(&cm_scacheLock);

                lock_ObtainWrite(&scp->rw);
                osi_Log4(afsd_logp, "RevokeVolumeCallback Discarding SCache scp 0x%p vol %u vn %u uniq %u",
                          scp, scp->fid.volume, scp->fid.vnode, scp->fid.unique);
                cm_DiscardSCache(scp);
                lock_ReleaseWrite(&scp->rw);

                cm_CallbackNotifyChange(scp);
                lock_ObtainWrite(&cm_scacheLock);
                cm_ReleaseSCacheNoLock(scp);
                if (scp->flags & CM_SCACHEFLAG_PURERO)
                    cm_callbackDiscardROVolumeByFID(&scp->fid);
            }
        }	/* search one hash bucket */
    }	/* search all hash buckets */

    lock_ReleaseWrite(&cm_scacheLock);

    osi_Log1(afsd_logp, "RevokeVolumeCallback Complete vol %d", fidp->Volume);
}

/*
 * afs_data_pointer_to_int32() - returns least significant afs_int32 of the
 * given data pointer, without triggering "cast truncates pointer"
 * warnings.  We use this where we explicitly don't care whether a
 * pointer is truncated -- it loses information where a pointer is
 * larger than an afs_int32.
 */

static afs_int32
afs_data_pointer_to_int32(const void *p)
{
    union {
        afs_int32 i32[sizeof(void *) / sizeof(afs_int32)];
        const void *p;
    } ip;

    int i32_sub;                /* subscript of least significant afs_int32 in ip.i32[] */

    /* set i32_sub */

    {
        /* used to determine the byte order of the system */

        union {
            char c[sizeof(int) / sizeof(char)];
            int i;
        } ci;

        ci.i = 1;
        if (ci.c[0] == 1) {
            /* little-endian system */
            i32_sub = 0;
        } else {
            /* big-endian system */
            i32_sub = (sizeof ip.i32 / sizeof ip.i32[0]) - 1;
        }
    }

    ip.p = p;
    return ip.i32[i32_sub];
}
/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_CallBack
 *
 * Description:
 *      Routine called by the server-side callback RPC interface to
 *      implement passing in callback information.
 *      table.
 *
 * Arguments:
 *      rx_call    : Ptr to Rx call on which this request came in.
 *      fidsArrayp : Ptr to array of fids involved.
 *      cbsArrayp  : Ptr to matching callback info for the fids.
 *
 * Returns:
 *      0 (always).
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/
/* handle incoming RPC callback breaking message.
 * Called with no locks held.
 */
int
SRXAFSCB_CallBack(struct rx_call *callp, AFSCBFids *fidsArrayp, AFSCBs *cbsArrayp)
{
    int i;
    AFSFid *tfidp;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;
    cm_server_t *tsp = NULL;
    cm_cell_t* cellp = NULL;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);

        tsp = cm_FindServerByIP(host, port, CM_SERVER_FILE, FALSE);
        if (tsp) {
            cellp = tsp->cellp;
            cm_PutServer(tsp);
        }

        if (!cellp)
            osi_Log2(afsd_logp, "SRXAFSCB_CallBack from host 0x%x port %d",
                     ntohl(host),
                     ntohs(port));
        else
            osi_Log3(afsd_logp, "SRXAFSCB_CallBack from host 0x%x port %d for cell %s",
                     ntohl(host),
                     ntohs(port),
                     cellp->name /* does not need to be saved, doesn't change */);
    } else {
        osi_Log0(afsd_logp, "SRXAFSCB_CallBack from unknown host");
    }


    for (i=0; i < (long) fidsArrayp->AFSCBFids_len; i++) {
        tfidp = &fidsArrayp->AFSCBFids_val[i];

        if (tfidp->Volume == 0)
            continue;   /* means don't do anything */
        else if (tfidp->Vnode == 0)
            cm_RevokeVolumeCallback(callp, cellp, tfidp);
        else
            cm_RevokeCallback(callp, cellp, tfidp);
    }
    return 0;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_InitCallBackState
 *
 * Description:
 *      Routine called by the server-side callback RPC interface to
 *      implement clearing all callbacks from this host.
 *
 * Arguments:
 *      rx_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *      0 (always).
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/
/* called with no locks by RPC system when a server indicates that it has never
 * heard from us, or for other reasons has had to discard callbacks from us
 * without telling us, e.g. a network partition.
 */
int
SRXAFSCB_InitCallBackState(struct rx_call *callp)
{
    if (cm_shutdown)
        return 1;

    osi_Log0(afsd_logp, "SRXAFSCB_InitCallBackState ->");

    return SRXAFSCB_InitCallBackState3(callp, NULL);
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_Probe
 *
 * Description:
 *      Routine called by the server-side callback RPC interface to
 *      implement ``probing'' the Cache Manager, just making sure it's
 *      still there.
 *
 * Arguments:
 *      rx_call : Ptr to Rx call on which this request came in.
 *
 * Returns:
 *      0 (always).
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/
int
SRXAFSCB_Probe(struct rx_call *callp)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_Probe from host 0x%x port %d",
              ntohl(host),
              ntohs(port));

    return 0;
}

/*------------------------------------------------------------------------
 * EXPORTED SRXAFSCB_GetLock
 *
 * Description:
 *      Routine called by the server-side callback RPC interface to
 *      implement pulling out the contents of a lock in the lock
 *      table.
 *
 * Arguments:
 *      a_call   : Ptr to Rx call on which this request came in.
 *      a_index  : Index of desired lock.
 *      a_result : Ptr to a buffer for the given lock.
 *
 * Returns:
 *      0 if everything went fine,
 *      1 if we were given a bad index.
 *
 * Environment:
 *      Nothing interesting.
 *
 * Side Effects:
 *      As advertised.
 *------------------------------------------------------------------------*/
/* debug interface */

extern osi_rwlock_t cm_aclLock;
extern osi_rwlock_t buf_globalLock;
extern osi_rwlock_t cm_cellLock;
extern osi_rwlock_t cm_connLock;
extern osi_rwlock_t cm_daemonLock;
extern osi_rwlock_t cm_dnlcLock;
extern osi_rwlock_t cm_scacheLock;
extern osi_rwlock_t cm_serverLock;
extern osi_rwlock_t cm_syscfgLock;
extern osi_rwlock_t cm_userLock;
extern osi_rwlock_t cm_utilsLock;
extern osi_rwlock_t cm_volumeLock;
extern osi_rwlock_t smb_globalLock;
extern osi_rwlock_t smb_rctLock;

extern osi_mutex_t cm_Freelance_Lock;
extern osi_mutex_t cm_Afsdsbmt_Lock;
extern osi_mutex_t tokenEventLock;
extern osi_mutex_t  smb_ListenerLock;
extern osi_mutex_t smb_RawBufLock;
extern osi_mutex_t smb_Dir_Watch_Lock;

#define LOCKTYPE_RW     1
#define LOCKTYPE_MUTEX  2
static struct _ltable {
    char *name;
    char *addr;
    int  type;
} ltable[] = {
    {"cm_scacheLock",    (char*)&cm_scacheLock,         LOCKTYPE_RW},
    {"buf_globalLock",   (char*)&buf_globalLock,        LOCKTYPE_RW},
    {"cm_serverLock",    (char*)&cm_serverLock,         LOCKTYPE_RW},
    {"cm_callbackLock",  (char*)&cm_callbackLock,       LOCKTYPE_RW},
    {"cm_syscfgLock",    (char*)&cm_syscfgLock,         LOCKTYPE_RW},
    {"cm_aclLock",       (char*)&cm_aclLock,            LOCKTYPE_RW},
    {"cm_cellLock",      (char*)&cm_cellLock,           LOCKTYPE_RW},
    {"cm_connLock",      (char*)&cm_connLock,           LOCKTYPE_RW},
    {"cm_userLock",      (char*)&cm_userLock,           LOCKTYPE_RW},
    {"cm_volumeLock",    (char*)&cm_volumeLock,         LOCKTYPE_RW},
    {"cm_daemonLock",    (char*)&cm_daemonLock,         LOCKTYPE_RW},
    {"cm_dnlcLock",      (char*)&cm_dnlcLock,           LOCKTYPE_RW},
    {"cm_utilsLock",     (char*)&cm_utilsLock,          LOCKTYPE_RW},
    {"smb_globalLock",   (char*)&smb_globalLock,        LOCKTYPE_RW},
    {"smb_rctLock",      (char*)&smb_rctLock,           LOCKTYPE_RW},
    {"cm_Freelance_Lock",(char*)&cm_Freelance_Lock,     LOCKTYPE_MUTEX},
    {"cm_Afsdsbmt_Lock", (char*)&cm_Afsdsbmt_Lock,      LOCKTYPE_MUTEX},
    {"tokenEventLock",   (char*)&tokenEventLock,        LOCKTYPE_MUTEX},
    {"smb_ListenerLock", (char*)&smb_ListenerLock,      LOCKTYPE_MUTEX},
    {"smb_RawBufLock",   (char*)&smb_RawBufLock,        LOCKTYPE_MUTEX},
    {"smb_Dir_Watch_Lock",(char*)&smb_Dir_Watch_Lock,   LOCKTYPE_MUTEX}
};

int
SRXAFSCB_GetLock(struct rx_call *callp, long index, AFSDBLock *lockp)
{
    struct _ltable *tl;          /*Ptr to lock table entry */
    osi_rwlock_t  *rwp;
    osi_mutex_t   *mtxp;
    int nentries;               /*Num entries in table */
    int code;                   /*Return code */
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log3(afsd_logp, "SRXAFSCB_GetLock(%d) from host 0x%x port %d",
             index, ntohl(host), ntohs(port));

    nentries = sizeof(ltable) / sizeof(struct _ltable);
    if (index < 0 || index >= nentries) {
        /*
         * Past EOF
         */
        code = 1;
    } else {
        /*
         * Found it - copy out its contents.
         */
        tl = &ltable[index];
        strncpy(lockp->name, tl->name, sizeof(lockp->name));
        lockp->name[sizeof(lockp->name)-1] = '\0';
        lockp->lock.waitStates = 0;
        switch ( tl->type ) {
        case LOCKTYPE_RW:
            rwp = (osi_rwlock_t *)tl->addr;
            lockp->lock.exclLocked = rwp->flags;
            lockp->lock.readersReading = rwp->readers;
            lockp->lock.numWaiting = rwp->waiters;
            break;
        case LOCKTYPE_MUTEX:
            mtxp = (osi_mutex_t *)tl->addr;
            lockp->lock.exclLocked = mtxp->flags;
            lockp->lock.readersReading = 0;
            lockp->lock.numWaiting = mtxp->waiters;
            break;
        }
        lockp->lock.pid_last_reader = 0;
        lockp->lock.pid_writer = 0;
        lockp->lock.src_indicator = 0;
        code = 0;
    }

    return code;
}

/* debug interface */
int
SRXAFSCB_GetCE(struct rx_call *callp, long index, AFSDBCacheEntry *cep)
{
    afs_uint32 i;
    cm_scache_t * scp;
    int code;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetCE from host 0x%x port %d",
             ntohl(host), ntohs(port));

    lock_ObtainRead(&cm_scacheLock);
    for (i = 0; i < cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp) {
            if (index == 0)
                goto searchDone;
            index--;
        }                       /*Zip through current hash chain */
    }                           /*Zip through hash chains */

  searchDone:
    if (scp == NULL) {
        /*Past EOF */
        code = 1;
        goto fcnDone;
    }

    /*
     * Copy out the located entry.
     */
    memset(cep, 0, sizeof(AFSDBCacheEntry));
    cep->addr = afs_data_pointer_to_int32(scp);
    cep->cell = scp->fid.cell;
    cep->netFid.Volume = scp->fid.volume;
    cep->netFid.Vnode = scp->fid.vnode;
    cep->netFid.Unique = scp->fid.unique;
    cep->lock.waitStates = 0;
    cep->lock.exclLocked = scp->rw.flags;
    cep->lock.readersReading = 0;
    cep->lock.numWaiting = scp->rw.waiters;
    cep->lock.pid_last_reader = 0;
    cep->lock.pid_writer = 0;
    cep->lock.src_indicator = 0;
    cep->Length = scp->length.LowPart;
    cep->DataVersion = (afs_uint32)(scp->dataVersion & 0xFFFFFFFF);
    cep->callback = afs_data_pointer_to_int32(scp->cbServerp);
    if (scp->flags & CM_SCACHEFLAG_PURERO) {
        cm_volume_t *volp = cm_GetVolumeByFID(&scp->fid);
        if (volp) {
            cep->cbExpires = volp->cbExpiresRO;
            cm_PutVolume(volp);
        }
    } else {
        /* TODO: deal with time_t below */
        cep->cbExpires = (afs_int32) scp->cbExpires;
    }
    cep->refCount = scp->refCount;
    cep->opens = scp->openReads;
    cep->writers = scp->openWrites;
    switch (scp->fileType) {
    case CM_SCACHETYPE_FILE:
        cep->mvstat = 0;
        break;
    case CM_SCACHETYPE_MOUNTPOINT:
        cep->mvstat = 1;
        break;
    case CM_SCACHETYPE_DIRECTORY:
        if (scp->fid.vnode == 1 && scp->fid.unique == 1)
            cep->mvstat = 2;
        else
            cep->mvstat = 3;
        break;
    case CM_SCACHETYPE_SYMLINK:
        cep->mvstat = 4;
        break;
    case CM_SCACHETYPE_DFSLINK:
        cep->mvstat = 5;
        break;
    case CM_SCACHETYPE_INVALID:
        cep->mvstat = 6;
        break;
    }
    cep->states = 0;
    if (scp->flags & CM_SCACHEFLAG_STATD)
        cep->states |= 1;
    if (scp->flags & CM_SCACHEFLAG_RO || scp->flags & CM_SCACHEFLAG_PURERO)
        cep->states |= 4;
    if (scp->fileType == CM_SCACHETYPE_MOUNTPOINT &&
        scp->mountPointStringp[0])
        cep->states |= 8;
    if (scp->flags & CM_SCACHEFLAG_WAITING)
        cep->states |= 0x40;
    code = 0;

    /*
     * Return our results.
     */
  fcnDone:
    lock_ReleaseRead(&cm_scacheLock);

    return (code);
}

/* debug interface */
int
SRXAFSCB_GetCE64(struct rx_call *callp, long index, AFSDBCacheEntry64 *cep)
{
    afs_uint32 i;
    cm_scache_t * scp;
    int code;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetCE64 from host 0x%x port %d",
             ntohl(host), ntohs(port));

    lock_ObtainRead(&cm_scacheLock);
    for (i = 0; i < cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp) {
            if (index == 0)
                goto searchDone;
            index--;
        }                       /*Zip through current hash chain */
    }                           /*Zip through hash chains */

  searchDone:
    if (scp == NULL) {
        /*Past EOF */
        code = 1;
        goto fcnDone;
    }

    /*
     * Copy out the located entry.
     */
    memset(cep, 0, sizeof(AFSDBCacheEntry64));
    cep->addr = afs_data_pointer_to_int32(scp);
    cep->cell = scp->fid.cell;
    cep->netFid.Volume = scp->fid.volume;
    cep->netFid.Vnode = scp->fid.vnode;
    cep->netFid.Unique = scp->fid.unique;
    cep->lock.waitStates = 0;
    cep->lock.exclLocked = scp->rw.flags;
    cep->lock.readersReading = 0;
    cep->lock.numWaiting = scp->rw.waiters;
    cep->lock.pid_last_reader = 0;
    cep->lock.pid_writer = 0;
    cep->lock.src_indicator = 0;
#if !defined(AFS_64BIT_ENV)
    cep->Length.high = scp->length.HighPart;
    cep->Length.low = scp->length.LowPart;
#else
    cep->Length = (afs_int64) scp->length.QuadPart;
#endif
    cep->DataVersion = (afs_uint32)(scp->dataVersion & 0xFFFFFFFF);
    cep->callback = afs_data_pointer_to_int32(scp->cbServerp);
    if (scp->flags & CM_SCACHEFLAG_PURERO) {
        cm_volume_t *volp = cm_GetVolumeByFID(&scp->fid);
        if (volp) {
            cep->cbExpires = volp->cbExpiresRO;
            cm_PutVolume(volp);
        }
    } else {
        /* TODO: handle time_t */
        cep->cbExpires = (afs_int32) scp->cbExpires;
    }
    cep->refCount = scp->refCount;
    cep->opens = scp->openReads;
    cep->writers = scp->openWrites;
    switch (scp->fileType) {
    case CM_SCACHETYPE_FILE:
        cep->mvstat = 0;
        break;
    case CM_SCACHETYPE_MOUNTPOINT:
        cep->mvstat = 1;
        break;
    case CM_SCACHETYPE_DIRECTORY:
        if (scp->fid.vnode == 1 && scp->fid.unique == 1)
            cep->mvstat = 2;
        else
            cep->mvstat = 3;
        break;
    case CM_SCACHETYPE_SYMLINK:
        cep->mvstat = 4;
        break;
    case CM_SCACHETYPE_DFSLINK:
        cep->mvstat = 5;
        break;
    case CM_SCACHETYPE_INVALID:
        cep->mvstat = 6;
        break;
    }
    cep->states = 0;
    if (scp->flags & CM_SCACHEFLAG_STATD)
        cep->states |= 1;
    if (scp->flags & CM_SCACHEFLAG_RO || scp->flags & CM_SCACHEFLAG_PURERO)
        cep->states |= 4;
    if (scp->fileType == CM_SCACHETYPE_MOUNTPOINT &&
        scp->mountPointStringp[0])
        cep->states |= 8;
    if (scp->flags & CM_SCACHEFLAG_WAITING)
        cep->states |= 0x40;
    code = 0;

    /*
     * Return our results.
     */
  fcnDone:
    lock_ReleaseRead(&cm_scacheLock);

    return (code);
}

/* debug interface: not implemented */
int
SRXAFSCB_XStatsVersion(struct rx_call *callp, long *vp)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_XStatsVersion from host 0x%x port %d - not implemented",
             ntohl(host), ntohs(port));
    *vp = -1;

    return RXGEN_OPCODE;
}

/* debug interface: not implemented */
int
SRXAFSCB_GetXStats(struct rx_call *callp, long cvn, long coln, long *srvp, long *timep,
                   AFSCB_CollData *datap)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetXStats from host 0x%x port %d - not implemented",
             ntohl(host), ntohs(port));

    return RXGEN_OPCODE;
}

int
SRXAFSCB_InitCallBackState2(struct rx_call *callp, struct interfaceAddr* addr)
{
    if (cm_shutdown)
        return 1;

    osi_Log0(afsd_logp, "SRXAFSCB_InitCallBackState2 ->");

    return SRXAFSCB_InitCallBackState3(callp, NULL);
}

/* debug interface */
int
SRXAFSCB_WhoAreYou(struct rx_call *callp, struct interfaceAddr* addr)
{
    int i;
    long code;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_WhoAreYou from host 0x%x port %d",
              ntohl(host),
              ntohs(port));

    lock_ObtainRead(&cm_syscfgLock);
    if (cm_LanAdapterChangeDetected) {
        lock_ConvertRToW(&cm_syscfgLock);
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

    /* return all network interface addresses */
    addr->numberOfInterfaces = cm_noIPAddr;
    addr->uuid = cm_data.Uuid;
    for ( i=0; i < cm_noIPAddr; i++ ) {
        addr->addr_in[i] = cm_IPAddr[i];
        addr->subnetmask[i] = cm_SubnetMask[i];
        addr->mtu[i] = (rx_mtu == -1 || (rx_mtu != -1 && cm_NetMtu[i] < rx_mtu)) ?
            cm_NetMtu[i] : rx_mtu;
    }

    lock_ReleaseRead(&cm_syscfgLock);

    return 0;
}

int
SRXAFSCB_InitCallBackState3(struct rx_call *callp, afsUUID* serverUuid)
{
    char *p = NULL;

    cm_server_t *tsp = NULL;
    cm_scache_t *scp = NULL;
    cm_cell_t* cellp = NULL;
    afs_uint32 hash;
    int discarded;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);

        if (serverUuid) {
            if (UuidToString((UUID *)serverUuid, &p) == RPC_S_OK) {
                osi_Log1(afsd_logp, "SRXAFSCB_InitCallBackState3 Uuid%s ->",osi_LogSaveString(afsd_logp,p));
                RpcStringFree(&p);
            }

            tsp = cm_FindServerByUuid(serverUuid, CM_SERVER_FILE, FALSE);
        }
        if (!tsp)
            tsp = cm_FindServerByIP(host, port, CM_SERVER_FILE, FALSE);
        if (tsp) {
            cellp = tsp->cellp;
            cm_PutServer(tsp);
        }

        if (!cellp)
            osi_Log2(afsd_logp, "SRXAFSCB_InitCallBackState3 from host 0x%x port %d",
                     ntohl(host),
                     ntohs(port));
        else
            osi_Log3(afsd_logp, "SRXAFSCB_InitCallBackState3 from host 0x%x port %d for cell %s",
                     ntohl(host),
                     ntohs(port),
                     cellp->name /* does not need to be saved, doesn't change */);
    } else {
        osi_Log0(afsd_logp, "SRXAFSCB_InitCallBackState3 from unknown host");
    }

    if (connp && peerp) {
	tsp = cm_FindServerByIP(host, port, CM_SERVER_FILE, FALSE);

	osi_Log1(afsd_logp, "InitCallbackState3 server %x", tsp);

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
        if (cellp) {
            cm_fid_t fid;

            fid.cell = cellp->cellID;
            fid.volume = fid.vnode = fid.unique = 0;

            cm_RecordRacingRevoke(&fid, CM_RACINGFLAG_CANCELALL);
        } else {
            cm_RecordRacingRevoke(NULL, CM_RACINGFLAG_CANCELALL);
        }

	/* now search all vnodes looking for guys with this callback, if we
	 * found it, or guys with any callbacks, if we didn't find the server
	 * (that's how multihomed machines will appear and how we'll handle
	 * them, albeit a little inefficiently).  That is, we're discarding all
	 * callbacks from all hosts if we get an initstate call from an unknown
	 * host.  Since these calls are rare, and multihomed servers
	 * are "rare," hopefully this won't be a problem.
	 */
	lock_ObtainWrite(&cm_scacheLock);
	for (hash = 0; hash < cm_data.scacheHashTableSize; hash++) {
            for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
                cm_HoldSCacheNoLock(scp);
                lock_ReleaseWrite(&cm_scacheLock);
                lock_ObtainWrite(&scp->rw);
                discarded = 0;
                if (scp->cbExpires > 0 && scp->cbServerp != NULL) {
                    /* we have a callback, now decide if we should clear it */
                    if (cm_ServerEqual(scp->cbServerp, tsp)) {
                        osi_Log4(afsd_logp, "InitCallbackState3 Discarding SCache scp 0x%p vol %u vn %u uniq %u",
                                  scp, scp->fid.volume, scp->fid.vnode, scp->fid.unique);
                        cm_DiscardSCache(scp);
                        discarded = 1;
                    }
                }
                lock_ReleaseWrite(&scp->rw);
                if (discarded)
                    cm_CallbackNotifyChange(scp);
                lock_ObtainWrite(&cm_scacheLock);
                cm_ReleaseSCacheNoLock(scp);

                if (discarded && (scp->flags & CM_SCACHEFLAG_PURERO))
                    cm_callbackDiscardROVolumeByFID(&scp->fid);

            }	/* search one hash bucket */
	}      	/* search all hash buckets */

	lock_ReleaseWrite(&cm_scacheLock);

	if (tsp) {
	    /* reset the No flags on the server */
	    cm_SetServerNo64Bit(tsp, 0);
	    cm_SetServerNoInlineBulk(tsp, 0);

	    /* we're done with the server structure */
            cm_PutServer(tsp);
	}
    }
    return 0;
}

/* debug interface */
int
SRXAFSCB_ProbeUuid(struct rx_call *callp, afsUUID* clientUuid)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;
    char *p,*q;
    int code = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    if ( !afs_uuid_equal(&cm_data.Uuid, clientUuid) ) {
        UuidToString((UUID *)&cm_data.Uuid, &p);
        UuidToString((UUID *)clientUuid, &q);
        osi_Log4(afsd_logp, "SRXAFSCB_ProbeUuid %s != %s from host 0x%x port %d",
                  osi_LogSaveString(afsd_logp,p),
                  osi_LogSaveString(afsd_logp,q),
                  ntohl(host),
                  ntohs(port));
        RpcStringFree(&p);
        RpcStringFree(&q);

        code = 1;       /* failure */
    } else
        osi_Log2(afsd_logp, "SRXAFSCB_ProbeUuid (success) from host 0x%x port %d",
                  ntohl(host),
                  ntohs(port));

    return code;
}

/* debug interface */
static int
GetCellCommon(afs_int32 a_cellnum, char **a_name, serverList *a_hosts)
{
    afs_int32 sn;
    cm_cell_t * cellp;
    cm_serverRef_t * serverRefp;
    size_t len;

    cellp = cm_FindCellByID(a_cellnum, CM_FLAG_NOPROBE);
    if (!cellp) {
        *a_name = (char *)xdr_alloc(sizeof(char));
        if (*a_name)
            *a_name[0] = '\0';
        return 0;
    }

    lock_ObtainRead(&cm_serverLock);
    len = strlen(cellp->name)+1;
    *a_name = (char *)xdr_alloc(len);
    if (*a_name)
        memcpy(*a_name, cellp->name, len);

    for ( sn = 0, serverRefp = cellp->vlServersp;
          sn < AFSMAXCELLHOSTS && serverRefp;
          sn++, serverRefp = serverRefp->next);

    a_hosts->serverList_len = sn;
    a_hosts->serverList_val = (afs_int32 *)xdr_alloc(sn * sizeof(afs_int32));

    for ( sn = 0, serverRefp = cellp->vlServersp;
          sn < AFSMAXCELLHOSTS && serverRefp;
          sn++, serverRefp = serverRefp->next)
    {
        a_hosts->serverList_val[sn] = ntohl(serverRefp->server->addr.sin_addr.s_addr);
    }

    lock_ReleaseRead(&cm_serverLock);
    return 0;
}

/* debug interface */
int
SRXAFSCB_GetCellByNum(struct rx_call *callp, afs_int32 a_cellnum,
                      char **a_name, serverList *a_hosts)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;
    int rc;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log3(afsd_logp, "SRXAFSCB_GetCellByNum(%d) from host 0x%x port %d",
             a_cellnum, ntohl(host), ntohs(port));

    a_hosts->serverList_val = 0;
    a_hosts->serverList_len = 0;


    rc = GetCellCommon(a_cellnum, a_name, a_hosts);

    return rc;
}

/* debug interface */
int
SRXAFSCB_TellMeAboutYourself( struct rx_call *callp,
                              struct interfaceAddr *addr,
                              Capabilities * capabilities)
{
    int i;
    afs_uint32 *dataBuffP;
    afs_int32 dataBytes;
    long code;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_TellMeAboutYourself from host 0x%x port %d",
              ntohl(host),
              ntohs(port));

    lock_ObtainRead(&cm_syscfgLock);
    if (cm_LanAdapterChangeDetected) {
        lock_ConvertRToW(&cm_syscfgLock);
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

    /* return all network interface addresses */
    addr->numberOfInterfaces = cm_noIPAddr;
    addr->uuid = cm_data.Uuid;
    for ( i=0; i < cm_noIPAddr; i++ ) {
        addr->addr_in[i] = cm_IPAddr[i];
        addr->subnetmask[i] = cm_SubnetMask[i];
        addr->mtu[i] = (rx_mtu == -1 || (rx_mtu != -1 && cm_NetMtu[i] < rx_mtu)) ?
            cm_NetMtu[i] : rx_mtu;
    }
    lock_ReleaseRead(&cm_syscfgLock);

    dataBytes = 1 * sizeof(afs_uint32);
    dataBuffP = (afs_uint32 *) xdr_alloc(dataBytes);
    dataBuffP[0] = CLIENT_CAPABILITY_ERRORTRANS;
    capabilities->Capabilities_len = dataBytes / sizeof(afs_uint32);
    capabilities->Capabilities_val = dataBuffP;

    return 0;
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
    struct rx_call *callp,
    afs_int32 a_index,
    afs_int32 *a_srvr_addr,
    afs_int32 *a_srvr_rank)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetServerPrefs from host 0x%x port %d - not implemented",
              ntohl(host),
              ntohs(port));

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

int SRXAFSCB_GetCellServDB(struct rx_call *callp, afs_int32 index, char **a_name,
                           serverList *a_hosts)
{
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;
    int rc;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetCellServDB from host 0x%x port %d - not implemented",
             ntohl(host), ntohs(port));

#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && index == 0) {
        rc = GetCellCommon(AFS_FAKE_ROOT_CELL_ID, a_name, a_hosts);
    } else
#endif
    {
        rc = GetCellCommon(index+1, a_name, a_hosts);
    }
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

int SRXAFSCB_GetLocalCell(struct rx_call *callp, char **a_name)
{
    char *t_name;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;
    size_t len;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetLocalCell from host 0x%x port %d",
             ntohl(host), ntohs(port));

    if (cm_data.rootCellp) {
        len = strlen(cm_data.rootCellp->name) + 1;
        t_name = (char *)xdr_alloc(len);
        if (t_name)
            memcpy(t_name, cm_data.rootCellp->name, len);
    } else {
	t_name = (char *)xdr_alloc(1);
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

int SRXAFSCB_GetCacheConfig(struct rx_call *callp,
                            afs_uint32 callerVersion,
                            afs_uint32 *serverVersion,
                            afs_uint32 *configCount,
                            cacheConfig *config)
{
    afs_uint32 *t_config;
    size_t allocsize;
    extern cm_initparams_v1 cm_initParams;
    struct rx_connection *connp;
    struct rx_peer *peerp;
    unsigned long host = 0;
    unsigned short port = 0;

    if (cm_shutdown)
        return 1;

    if ((connp = rx_ConnectionOf(callp)) && (peerp = rx_PeerOf(connp))) {
        host = rx_HostOf(peerp);
        port = rx_PortOf(peerp);
    }

    osi_Log2(afsd_logp, "SRXAFSCB_GetCacheConfig from host 0x%x port %d - version 1 only",
             ntohl(host), ntohs(port));

    /*
     * Currently only support version 1
     */
    allocsize = sizeof(cm_initparams_v1);
    t_config = (afs_uint32 *)xdr_alloc(allocsize);

    afs_MarshallCacheConfig(callerVersion, &cm_initParams, t_config);

    *serverVersion = AFS_CLIENT_RETRIEVAL_FIRST_EDITION;
#ifdef DEBUG
#ifndef SIZE_MAX
#define SIZE_MAX UINT_MAX
#endif
    osi_assertx(allocsize < SIZE_MAX, "allocsize >= SIZE_MAX");
#endif
    *configCount = (afs_uint32)allocsize;
    config->cacheConfig_val = t_config;
    config->cacheConfig_len = (*configCount)/sizeof(afs_uint32);

    return 0;
}

/* called by afsd without any locks to initialize this module */
void cm_InitCallback(void)
{
    lock_InitializeRWLock(&cm_callbackLock, "cm_callbackLock", LOCK_HIERARCHY_CALLBACK_GLOBAL);
    cm_activeCallbackGrantingCalls = 0;
}

/* called with locked scp; tells us whether we've got a callback.
 * Expirations are checked by a background daemon so as to make
 * this function as inexpensive as possible
 */
int cm_HaveCallback(cm_scache_t *scp)
{
#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled &&
        scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
        scp->fid.volume==AFS_FAKE_ROOT_VOL_ID) {
        if (cm_getLocalMountPointChange()) {
            cm_clearLocalMountPointChange();
            lock_ReleaseWrite(&scp->rw);
            cm_reInitLocalMountPoints();
            lock_ObtainWrite(&scp->rw);
        }
        return (cm_data.fakeDirVersion == scp->dataVersion);
    }
#endif
    if (scp->cbServerp != NULL)
	return 1;

    if (scp->flags & CM_SCACHEFLAG_PURERO) {
        cm_volume_t *volp = cm_GetVolumeByFID(&scp->fid);
        if (volp) {
            int haveCB = 0;
            if (cm_OfflineROIsValid) {
                switch (cm_GetVolumeStatus(volp, scp->fid.volume)) {
                case vl_offline:
                case vl_alldown:
                case vl_unknown:
                    haveCB = 1;
                    break;
                }
            }
            if (!haveCB &&
                volp->creationDateRO == scp->volumeCreationDate &&
                volp->cbServerpRO != NULL) {
                haveCB = 1;
            }
            cm_PutVolume(volp);
            return haveCB;
        }
    }
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
    cbrp->startTime = time(NULL);
    cbrp->serverp = NULL;
    lock_ReleaseWrite(&cm_callbackLock);
}

/* Called at the end of a callback-granting call, to remove the callback
 * info from the scache entry, if necessary.
 *
 * Called with scp write locked, so we can discard the callbacks easily with
 * this locking hierarchy.
 */
void cm_EndCallbackGrantingCall(cm_scache_t *scp, cm_callbackRequest_t *cbrp,
                                AFSCallBack *cbp, AFSVolSync *volSyncp, long flags)
{
    cm_racingRevokes_t *revp;		/* where we are */
    cm_racingRevokes_t *nrevp;		/* where we'll be next */
    int freeFlag;
    cm_server_t * serverp = NULL;
    int discardScp = 0, discardVolCB = 0;

    lock_ObtainWrite(&cm_callbackLock);
    if (flags & CM_CALLBACK_MAINTAINCOUNT) {
        osi_assertx(cm_activeCallbackGrantingCalls > 0,
                    "CM_CALLBACK_MAINTAINCOUNT && cm_activeCallbackGrantingCalls == 0");
    }
    else {
        osi_assertx(cm_activeCallbackGrantingCalls-- > 0,
                    "!CM_CALLBACK_MAINTAINCOUNT && cm_activeCallbackGrantingCalls == 0");
    }
    if (cm_activeCallbackGrantingCalls == 0)
        freeFlag = 1;
    else
        freeFlag = 0;

    /* record the callback; we'll clear it below if we really lose it */
    if (cbrp) {
	if (scp) {
            if (!cm_ServerEqual(scp->cbServerp, cbrp->serverp)) {
                serverp = scp->cbServerp;
                if (!freeFlag)
                    cm_GetServer(cbrp->serverp);
                scp->cbServerp = cbrp->serverp;
            } else {
                if (freeFlag)
                    serverp = cbrp->serverp;
            }
            scp->cbExpires = cbrp->startTime + cbp->ExpirationTime;
        } else {
            if (freeFlag)
                serverp = cbrp->serverp;
        }
        if (freeFlag)
            cbrp->serverp = NULL;
    }

    /* a callback was actually revoked during our granting call, so
     * run down the list of revoked fids, looking for ours.
     * If activeCallbackGrantingCalls is zero, free the elements, too.
     *
     * May need to go through entire list just to do the freeing.
     */
    for (revp = cm_racingRevokesp; revp; revp = nrevp) {
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
                  ((revp->flags & CM_RACINGFLAG_CANCELALL) &&
                   (revp->fid.cell == 0 || scp->fid.cell == revp->fid.cell)))) {
            /* this one matches */
            osi_Log4(afsd_logp,
                      "Racing revoke scp 0x%p old cbc %d rev cbc %d cur cbc %d",
                      scp,
                      cbrp->callbackCount, revp->callbackCount,
                      cm_callbackCount);
            discardScp = 1;
            if ((scp->flags & CM_SCACHEFLAG_PURERO) &&
                 (revp->flags & CM_RACINGFLAG_ALL))
                cm_callbackDiscardROVolumeByFID(&scp->fid);
        }
        if (freeFlag)
            free(revp);
    }

    /* if we freed the list, zap the pointer to it */
    if (freeFlag)
        cm_racingRevokesp = NULL;

    lock_ReleaseWrite(&cm_callbackLock);

    if ( discardScp ) {
        cm_DiscardSCache(scp);
        lock_ReleaseWrite(&scp->rw);
        cm_CallbackNotifyChange(scp);
        lock_ObtainWrite(&scp->rw);
    } else {
        if (scp && scp->flags & CM_SCACHEFLAG_PURERO) {
            cm_volume_t * volp = cm_GetVolumeByFID(&scp->fid);
            if (volp) {
                if (volSyncp) {
                    lock_ObtainWrite(&cm_scacheLock);
                    volp->cbExpiresRO = scp->cbExpires;
                    volp->creationDateRO = volSyncp->spare1;
                    if (volp->cbServerpRO != scp->cbServerp) {
                        if (volp->cbServerpRO)
                            cm_PutServer(volp->cbServerpRO);
                        cm_GetServer(scp->cbServerp);
                        volp->cbServerpRO = scp->cbServerp;
                    }
                    lock_ReleaseWrite(&cm_scacheLock);
                }
                cm_PutVolume(volp);
            }
        }
    }

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
    long code = 0;
    cm_conn_t *connp = NULL;
    AFSFetchStatus afsStatus;
    AFSVolSync volSync;
    AFSCallBack callback;
    AFSFid tfid;
    cm_callbackRequest_t cbr;
    int mustCall;
    cm_fid_t sfid;
    struct rx_connection * rxconnp = NULL;
    int syncop_done = 0;

    memset(&volSync, 0, sizeof(volSync));

    osi_Log4(afsd_logp, "GetCallback scp 0x%p cell %d vol %d flags %lX",
             scp, scp->fid.cell, scp->fid.volume, flags);

#ifdef AFS_FREELANCE_CLIENT
    // The case where a callback is needed on /afs is handled
    // specially. We need to fetch the status by calling
    // cm_MergeStatus
    if (cm_freelanceEnabled &&
        (scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
         scp->fid.volume==AFS_FAKE_ROOT_VOL_ID)) {

        code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                          CM_SCACHESYNC_FETCHSTATUS | CM_SCACHESYNC_GETCALLBACK);
        if (code)
            goto done;
        syncop_done = 1;

        if (scp->dataVersion != cm_data.fakeDirVersion) {
            memset(&afsStatus, 0, sizeof(afsStatus));
            memset(&volSync, 0, sizeof(volSync));
            InterlockedIncrement(&scp->activeRPCs);

            // Fetch the status info
            cm_MergeStatus(NULL, scp, &afsStatus, &volSync, userp, reqp, 0);
        }
        goto done;
    }
#endif /* AFS_FREELANCE_CLIENT */

    mustCall = (flags & 1);
    cm_AFSFidFromFid(&tfid, &scp->fid);
    while (1) {
        if (!mustCall && cm_HaveCallback(scp))
	    break;

        /* turn off mustCall, since it has now forced us past the check above */
        mustCall = 0;

        /* otherwise, we have to make an RPC to get the status */
	if (!syncop_done) {
	    code = cm_SyncOp(scp, NULL, userp, reqp, 0,
			     CM_SCACHESYNC_FETCHSTATUS | CM_SCACHESYNC_GETCALLBACK);
	    if (code)
		break;
	    syncop_done = 1;
	}
        cm_StartCallbackGrantingCall(scp, &cbr);
        sfid = scp->fid;
        lock_ReleaseWrite(&scp->rw);

        /* now make the RPC */
        InterlockedIncrement(&scp->activeRPCs);
        osi_Log4(afsd_logp, "CALL FetchStatus scp 0x%p vol %u vn %u uniq %u",
                 scp, sfid.volume, sfid.vnode, sfid.unique);
        do {
            code = cm_ConnFromFID(&sfid, userp, reqp, &connp);
            if (code)
                continue;

            rxconnp = cm_GetRxConn(connp);
            code = RXAFS_FetchStatus(rxconnp, &tfid,
                                     &afsStatus, &callback, &volSync);
            rx_PutConnection(rxconnp);

        } while (cm_Analyze(connp, userp, reqp, &sfid, 0, &volSync, NULL,
                            &cbr, code));
        code = cm_MapRPCError(code, reqp);
        if (code)
            osi_Log4(afsd_logp, "CALL FetchStatus FAILURE code 0x%x scp 0x%p vol %u vn %u",
                     code, scp, scp->fid.volume, scp->fid.vnode);
        else
            osi_Log4(afsd_logp, "CALL FetchStatus SUCCESS scp 0x%p vol %u vn %u uniq %u",
                     scp, scp->fid.volume, scp->fid.vnode, scp->fid.unique);

        lock_ObtainWrite(&scp->rw);
        if (code == 0) {
            cm_EndCallbackGrantingCall(scp, &cbr, &callback, &volSync, 0);
            cm_MergeStatus(NULL, scp, &afsStatus, &volSync, userp, reqp, 0);
        } else {
            cm_EndCallbackGrantingCall(NULL, &cbr, NULL, NULL, 0);
            InterlockedDecrement(&scp->activeRPCs);
        }

        /* if we got an error, return to caller */
        if (code)
	    break;
    }

  done:
    if (syncop_done)
	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_FETCHSTATUS | CM_SCACHESYNC_GETCALLBACK);

    if (code) {
	osi_Log2(afsd_logp, "GetCallback Failed code 0x%x scp 0x%p -->",code, scp);
	osi_Log4(afsd_logp, "            cell %u vol %u vn %u uniq %u",
		 scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique);
    } else {
	osi_Log3(afsd_logp, "GetCallback Complete scp 0x%p cell %d vol %d",
		  scp, scp->fid.cell, scp->fid.volume);
    }

    return code;
}


/*
 * cm_CBServersDownTime() returns 1 if the downTime parameter is valid.
 *
 * Servers with multiple interfaces have multiple cm_server_t objects
 * which share the same UUID.  If one interface is down but others are up,
 * the server should not be considered down.  The returned downTime should
 * be the largest non-zero value if down or zero if up.  If the cbServerp
 * is down, it is updated to refer to an interface that is up (if one exists).
 *
 * called with cm_scacheLock held
 */
static long
cm_CBServersDownTime(cm_scache_t *scp, cm_volume_t *volp, time_t * pdownTime)
{
    cm_vol_state_t *statep;
    cm_serverRef_t *tsrp;
    int alldown = 1;
    time_t downTime = 0;
    cm_server_t * upserver = NULL;
    cm_server_t * downserver;

    *pdownTime = 0;

    if (scp->cbServerp == NULL)
        return 1;

    if (!(scp->cbServerp->flags & CM_SERVERFLAG_DOWN))
        return 1;

    statep = cm_VolumeStateByID(volp, scp->fid.volume);
    if (statep) {
        for (tsrp = statep->serversp; tsrp; tsrp=tsrp->next) {
            if (tsrp->status == srv_deleted)
                continue;

            if (!cm_ServerEqual(tsrp->server, scp->cbServerp))
                continue;

            if (!(tsrp->server->flags & CM_SERVERFLAG_DOWN)) {
                alldown = 0;
                if (!upserver) {
                    upserver = tsrp->server;
                    cm_GetServer(upserver);
                }
            }

            if (tsrp->server->downTime > downTime)
                downTime = tsrp->server->downTime;
        }
    } else {
        downTime = scp->cbServerp->downTime;
    }

    /* if the cbServerp does not match the current volume server list
     * we report the callback server as up so the callback can be
     * expired.
     */

    if (alldown) {
        *pdownTime = downTime;
    } else {
        lock_ObtainWrite(&scp->rw);
        downserver = scp->cbServerp;
        scp->cbServerp = upserver;
        lock_ReleaseWrite(&scp->rw);
        cm_PutServer(downserver);
    }
    return 1;
}

/* called periodically by cm_daemon to shut down use of expired callbacks */
void cm_CheckCBExpiration(void)
{
    afs_uint32 i;
    cm_scache_t *scp;
    cm_volume_t *volp;
    enum volstatus volstate;
    time_t now, downTime;

    osi_Log0(afsd_logp, "CheckCBExpiration");

    now = time(NULL);
    lock_ObtainWrite(&cm_scacheLock);
    for (i=0; i<cm_data.scacheHashTableSize; i++) {
        for (scp = cm_data.scacheHashTablep[i]; scp; scp=scp->nextp) {
            volp = NULL;
            cm_HoldSCacheNoLock(scp);
            lock_ReleaseWrite(&cm_scacheLock);

            /*
             * If this is not a PURERO object and there is no callback
             * or it hasn't expired, there is nothing to do
             */
            if (!(scp->flags & CM_SCACHEFLAG_PURERO) &&
                (scp->cbServerp == NULL || scp->cbExpires == 0 || now < scp->cbExpires))
                goto scp_complete;

            /*
             * Determine the volume state and update the callback info
             * to the latest if it is a PURERO object.
             */
            volp = cm_GetVolumeByFID(&scp->fid);
            volstate = vl_unknown;
            downTime = 0;
            if (volp) {
                if ((scp->flags & CM_SCACHEFLAG_PURERO) &&
                    volp->cbExpiresRO > scp->cbExpires && scp->cbExpires > 0)
                {
                    lock_ObtainWrite(&scp->rw);
                    scp->cbExpires = volp->cbExpiresRO;
                    if (volp->cbServerpRO != scp->cbServerp) {
                        if (scp->cbServerp)
                            cm_PutServer(scp->cbServerp);
                        cm_GetServer(volp->cbServerpRO);
                        scp->cbServerp = volp->cbServerpRO;
                    }
                    lock_ReleaseWrite(&scp->rw);
                }
                volstate = cm_GetVolumeStatus(volp, scp->fid.volume);
            }

            /* If there is no callback or it hasn't expired, there is nothing to do */
            if (scp->cbServerp == NULL || scp->cbExpires == 0 || now < scp->cbExpires)
                goto scp_complete;

            /* If the volume is known not to be online, do not expire the callback */
            if (volstate != vl_online)
                goto scp_complete;

            /*
             * If all the servers are down and the callback expired after the
             * issuing server went down, do not expire the callback
             */
            if (cm_CBServersDownTime(scp, volp, &downTime) && downTime && downTime < scp->cbExpires)
                goto scp_complete;

            /* The callback has expired, discard the status info */
            osi_Log4(afsd_logp, "Callback Expiration Discarding SCache scp 0x%p vol %u vn %u uniq %u",
                     scp, scp->fid.volume, scp->fid.vnode, scp->fid.unique);
            lock_ObtainWrite(&scp->rw);
            cm_DiscardSCache(scp);
            lock_ReleaseWrite(&scp->rw);

            cm_CallbackNotifyChange(scp);

          scp_complete:
            if (volp)
                cm_PutVolume(volp);

            lock_ObtainWrite(&cm_scacheLock);
            cm_ReleaseSCacheNoLock(scp);
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);

    osi_Log0(afsd_logp, "CheckCBExpiration Complete");
}


void
cm_GiveUpAllCallbacks(cm_server_t *tsp, afs_int32 markDown)
{
    long code;
    cm_conn_t *connp;
    struct rx_connection * rxconnp;

    if ((tsp->type == CM_SERVER_FILE) && !(tsp->flags & CM_SERVERFLAG_DOWN))
    {
        code = cm_ConnByServer(tsp, cm_rootUserp, FALSE, &connp);
        if (code == 0) {
            rxconnp = cm_GetRxConn(connp);
            rx_SetConnDeadTime(rxconnp, 10);
            code = RXAFS_GiveUpAllCallBacks(rxconnp);
            rx_SetConnDeadTime(rxconnp, ConnDeadtimeout);
            rx_PutConnection(rxconnp);
        }

        if (markDown) {
            cm_server_vols_t * tsrvp;
            cm_volume_t * volp;
            int i;

            cm_ForceNewConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                _InterlockedOr(&tsp->flags, CM_SERVERFLAG_DOWN);
                tsp->downTime = time(NULL);
            }
            /* Now update the volume status */
            for (tsrvp = tsp->vols; tsrvp; tsrvp = tsrvp->nextp) {
                for (i=0; i<NUM_SERVER_VOLS; i++) {
                    if (tsrvp->ids[i] != 0) {
                        cm_req_t req;

                        cm_InitReq(&req);
                        lock_ReleaseMutex(&tsp->mx);
                        code = cm_FindVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
                                                 &req, CM_GETVOL_FLAG_NO_LRU_UPDATE | CM_GETVOL_FLAG_NO_RESET, &volp);
                        lock_ObtainMutex(&tsp->mx);
                        if (code == 0) {
                            cm_UpdateVolumeStatus(volp, tsrvp->ids[i]);
                            cm_PutVolume(volp);
                        }
                    }
                }
            }
            lock_ReleaseMutex(&tsp->mx);
        }
    }
}

void
cm_GiveUpAllCallbacksAllServers(afs_int32 markDown)
{
    cm_server_t *tsp;

    if (!cm_giveUpAllCBs)
        return;

    lock_ObtainRead(&cm_serverLock);
    for (tsp = cm_allServersp; tsp; tsp = tsp->allNextp) {
        cm_GetServerNoLock(tsp);
        lock_ReleaseRead(&cm_serverLock);
        cm_GiveUpAllCallbacks(tsp, markDown);
        lock_ObtainRead(&cm_serverLock);
        cm_PutServerNoLock(tsp);
    }
    lock_ReleaseRead(&cm_serverLock);
}

void
cm_GiveUpAllCallbacksAllServersMulti(afs_int32 markDown)
{
    long code;
    cm_conn_t **conns = NULL;
    struct rx_connection **rxconns = NULL;
    afs_int32 i, nconns = 0, maxconns;
    cm_server_t ** serversp, *tsp;
    afs_int32 *results;
    time_t start, *deltas;

    maxconns = cm_numFileServers;
    if (maxconns == 0)
        return;

    conns = (cm_conn_t **)malloc(maxconns * sizeof(cm_conn_t *));
    rxconns = (struct rx_connection **)malloc(maxconns * sizeof(struct rx_connection *));
    deltas = (time_t *)malloc(maxconns * sizeof (time_t));
    results = (afs_int32 *)malloc(maxconns * sizeof (afs_int32));
    serversp = (cm_server_t **)malloc(maxconns * sizeof(cm_server_t *));

    lock_ObtainRead(&cm_serverLock);
    for (nconns=0, tsp = cm_allServersp; tsp && nconns < maxconns; tsp = tsp->allNextp) {
        if (tsp->type != CM_SERVER_FILE ||
            (tsp->flags & CM_SERVERFLAG_DOWN) ||
            tsp->cellp == NULL          /* SetPrefs only */)
            continue;

        cm_GetServerNoLock(tsp);
        lock_ReleaseRead(&cm_serverLock);

        serversp[nconns] = tsp;
        code = cm_ConnByServer(tsp, cm_rootUserp, FALSE, &conns[nconns]);
        if (code) {
            lock_ObtainRead(&cm_serverLock);
            cm_PutServerNoLock(tsp);
            continue;
        }
        lock_ObtainRead(&cm_serverLock);
        rxconns[nconns] = cm_GetRxConn(conns[nconns]);
        rx_SetConnDeadTime(rxconns[nconns], 10);

        nconns++;
    }
    lock_ReleaseRead(&cm_serverLock);

    if (nconns) {
        /* Perform the multi call */
        start = time(NULL);
        multi_Rx(rxconns,nconns)
        {
            multi_RXAFS_GiveUpAllCallBacks();
            results[multi_i]=multi_error;
        } multi_End;
    }

    /* Process results of servers that support RXAFS_GetCapabilities */
    for (i=0; i<nconns; i++) {
        rx_SetConnDeadTime(rxconns[i], ConnDeadtimeout);
        rx_PutConnection(rxconns[i]);
        cm_PutConn(conns[i]);

        tsp = serversp[i];
        cm_GCConnections(tsp);

        if (markDown) {
            cm_server_vols_t * tsrvp;
            cm_volume_t * volp;
            int i;

            cm_ForceNewConnections(tsp);

            lock_ObtainMutex(&tsp->mx);
            if (!(tsp->flags & CM_SERVERFLAG_DOWN)) {
                _InterlockedOr(&tsp->flags, CM_SERVERFLAG_DOWN);
                tsp->downTime = time(NULL);
            }
            /* Now update the volume status */
            for (tsrvp = tsp->vols; tsrvp; tsrvp = tsrvp->nextp) {
                for (i=0; i<NUM_SERVER_VOLS; i++) {
                    if (tsrvp->ids[i] != 0) {
                        cm_req_t req;

                        cm_InitReq(&req);
                        lock_ReleaseMutex(&tsp->mx);
                        code = cm_FindVolumeByID(tsp->cellp, tsrvp->ids[i], cm_rootUserp,
                                                 &req, CM_GETVOL_FLAG_NO_LRU_UPDATE | CM_GETVOL_FLAG_NO_RESET, &volp);
                        lock_ObtainMutex(&tsp->mx);
                        if (code == 0) {
                            cm_UpdateVolumeStatus(volp, tsrvp->ids[i]);
                            cm_PutVolume(volp);
                        }
                    }
                }
            }
            lock_ReleaseMutex(&tsp->mx);
        }
    }

    free(conns);
    free(rxconns);
    free(deltas);
    free(results);
    free(serversp);
}


