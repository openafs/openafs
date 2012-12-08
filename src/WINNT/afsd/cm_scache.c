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
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <osi.h>

#include "afsd.h"
#include "cm_btree.h"

/*extern void afsi_log(char *pattern, ...);*/

extern osi_hyper_t hzero;

/* File locks */
osi_queue_t *cm_allFileLocks;
osi_queue_t *cm_freeFileLocks;
unsigned long cm_lockRefreshCycle;

/* lock for globals */
osi_rwlock_t cm_scacheLock;

/* Dummy scache entry for use with pioctl fids */
cm_scache_t cm_fakeSCache;

osi_queue_t * cm_allFreeWaiters;        /* protected by cm_scacheLock */

#ifdef AFS_FREELANCE_CLIENT
extern osi_mutex_t cm_Freelance_Lock;
#endif

cm_scache_t *
cm_RootSCachep(cm_user_t *userp, cm_req_t *reqp)
{
    afs_int32 code;

    lock_ObtainWrite(&cm_data.rootSCachep->rw);
    code = cm_SyncOp(cm_data.rootSCachep, NULL, userp, reqp, 0,
                      CM_SCACHESYNC_GETSTATUS | CM_SCACHESYNC_NEEDCALLBACK);
    if (!code)
        cm_SyncOpDone(cm_data.rootSCachep, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    lock_ReleaseWrite(&cm_data.rootSCachep->rw);

    return cm_data.rootSCachep;
}


/* must be called with cm_scacheLock write-locked! */
void cm_AdjustScacheLRU(cm_scache_t *scp)
{
    lock_AssertWrite(&cm_scacheLock);
    if (!(scp->flags & CM_SCACHEFLAG_DELETED)) {
        osi_QRemoveHT((osi_queue_t **) &cm_data.scacheLRUFirstp, (osi_queue_t **) &cm_data.scacheLRULastp, &scp->q);
        osi_QAddH((osi_queue_t **) &cm_data.scacheLRUFirstp, (osi_queue_t **) &cm_data.scacheLRULastp, &scp->q);
    }
}

/* call with cm_scacheLock write-locked and scp rw held */
void cm_RemoveSCacheFromHashTable(cm_scache_t *scp)
{
    cm_scache_t **lscpp;
    cm_scache_t *tscp;
    int i;

    lock_AssertWrite(&cm_scacheLock);
    lock_AssertWrite(&scp->rw);
    if (scp->flags & CM_SCACHEFLAG_INHASH) {
	/* hash it out first */
	i = CM_SCACHE_HASH(&scp->fid);
	for (lscpp = &cm_data.scacheHashTablep[i], tscp = cm_data.scacheHashTablep[i];
	     tscp;
	     lscpp = &tscp->nextp, tscp = tscp->nextp) {
	    if (tscp == scp) {
		*lscpp = scp->nextp;
                scp->nextp = NULL;
		_InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_INHASH);
		break;
	    }
	}
    }
}

/* called with cm_scacheLock and scp write-locked */
void cm_ResetSCacheDirectory(cm_scache_t *scp, afs_int32 dirlock)
{
#ifdef USE_BPLUS
    /* destroy directory Bplus Tree */
    if (scp->dirBplus) {
        LARGE_INTEGER start, end;

        if (!dirlock && !lock_TryWrite(&scp->dirlock)) {
            /*
             * We are not holding the dirlock and obtaining it
             * requires that we drop the scp->rw.  As a result
             * we will leave the dirBplus tree intact but
             * invalidate the version number so that whatever
             * operation is currently active can safely complete
             * but the contents will be ignored on the next
             * directory operation.
             */
            scp->dirDataVersion = CM_SCACHE_VERSION_BAD;
            return;
        }

        QueryPerformanceCounter(&start);
        bplus_free_tree++;
        freeBtree(scp->dirBplus);
        scp->dirBplus = NULL;
        scp->dirDataVersion = CM_SCACHE_VERSION_BAD;
        QueryPerformanceCounter(&end);

        if (!dirlock)
            lock_ReleaseWrite(&scp->dirlock);

        bplus_free_time += (end.QuadPart - start.QuadPart);
    }
#endif
}

/* called with cm_scacheLock and scp write-locked; recycles an existing scp. */
long cm_RecycleSCache(cm_scache_t *scp, afs_int32 flags)
{
    lock_AssertWrite(&cm_scacheLock);
    lock_AssertWrite(&scp->rw);

    if (scp->refCount != 0) {
	return -1;
    }

    if (scp->flags & CM_SCACHEFLAG_SMB_FID) {
	osi_Log1(afsd_logp,"cm_RecycleSCache CM_SCACHEFLAG_SMB_FID detected scp 0x%p", scp);
#ifdef DEBUG
	osi_panic("cm_RecycleSCache CM_SCACHEFLAG_SMB_FID detected",__FILE__,__LINE__);
#endif
	return -1;
    }

    cm_RemoveSCacheFromHashTable(scp);

    /* invalidate so next merge works fine;
     * also initialize some flags */
    scp->fileType = 0;
    _InterlockedAnd(&scp->flags,
                    ~(CM_SCACHEFLAG_STATD
		     | CM_SCACHEFLAG_DELETED
		     | CM_SCACHEFLAG_RO
		     | CM_SCACHEFLAG_PURERO
		     | CM_SCACHEFLAG_OVERQUOTA
		     | CM_SCACHEFLAG_OUTOFSPACE
		     | CM_SCACHEFLAG_EACCESS));
    scp->serverModTime = 0;
    scp->dataVersion = CM_SCACHE_VERSION_BAD;
    scp->bufDataVersionLow = CM_SCACHE_VERSION_BAD;
    scp->bulkStatProgress = hzero;
    scp->waitCount = 0;
    scp->waitQueueT = NULL;

    if (scp->cbServerp) {
        cm_PutServer(scp->cbServerp);
        scp->cbServerp = NULL;
    }
    scp->cbExpires = 0;
    scp->volumeCreationDate = 0;

    scp->fid.vnode = 0;
    scp->fid.volume = 0;
    scp->fid.unique = 0;
    scp->fid.cell = 0;
    scp->fid.hash = 0;

    /* remove from dnlc */
    cm_dnlcPurgedp(scp);
    cm_dnlcPurgevp(scp);

    /* discard cached status; if non-zero, Close
     * tried to store this to server but failed */
    scp->mask = 0;

    /* discard symlink info */
    scp->mountPointStringp[0] = '\0';
    memset(&scp->mountRootFid, 0, sizeof(cm_fid_t));
    memset(&scp->dotdotFid, 0, sizeof(cm_fid_t));

    /* reset locking info */
    scp->fileLocksH = NULL;
    scp->fileLocksT = NULL;
    scp->serverLock = (-1);
    scp->exclusiveLocks = 0;
    scp->sharedLocks = 0;
    scp->lockDataVersion = CM_SCACHE_VERSION_BAD;
    scp->fsLockCount = 0;

    /* not locked, but there can be no references to this guy
     * while we hold the global refcount lock.
     */
    cm_FreeAllACLEnts(scp);

    cm_ResetSCacheDirectory(scp, 0);
    return 0;
}


/*
 * called with cm_scacheLock write-locked; find a vnode to recycle.
 * Can allocate a new one if desperate, or if below quota (cm_data.maxSCaches).
 * returns scp->rw write-locked.
 */
cm_scache_t *cm_GetNewSCache(void)
{
    cm_scache_t *scp;
    int retry = 0;

    lock_AssertWrite(&cm_scacheLock);
    if (cm_data.currentSCaches >= cm_data.maxSCaches) {
	/* There were no deleted scache objects that we could use.  Try to find
	 * one that simply hasn't been used in a while.
	 */
        for ( scp = cm_data.scacheLRULastp;
              scp;
              scp = (cm_scache_t *) osi_QPrev(&scp->q))
        {
            /* It is possible for the refCount to be zero and for there still
             * to be outstanding dirty buffers.  If there are dirty buffers,
             * we must not recycle the scp. */
            if (scp->refCount == 0 && scp->bufReadsp == NULL && scp->bufWritesp == NULL) {
                if (!buf_DirtyBuffersExist(&scp->fid)) {
                    if (!lock_TryWrite(&scp->rw))
                        continue;

                    if (!cm_RecycleSCache(scp, 0)) {
                        /* we found an entry, so return it */
                        /* now remove from the LRU queue and put it back at the
                         * head of the LRU queue.
                         */
                        cm_AdjustScacheLRU(scp);

                        /* and we're done */
                        return scp;
                    }
                    lock_ReleaseWrite(&scp->rw);
                } else {
                    osi_Log1(afsd_logp,"GetNewSCache dirty buffers exist scp 0x%x", scp);
                }
            }
        }
        osi_Log1(afsd_logp, "GetNewSCache all scache entries in use (retry = %d)", retry);

        return NULL;
    }

    /* if we get here, we should allocate a new scache entry.  We either are below
     * quota or we have a leak and need to allocate a new one to avoid panicing.
     */
    scp = cm_data.scacheBaseAddress + cm_data.currentSCaches;
    osi_assertx(scp >= cm_data.scacheBaseAddress && scp < (cm_scache_t *)cm_data.scacheHashTablep,
                "invalid cm_scache_t address");
    memset(scp, 0, sizeof(cm_scache_t));
    scp->magic = CM_SCACHE_MAGIC;
    lock_InitializeRWLock(&scp->rw, "cm_scache_t rw", LOCK_HIERARCHY_SCACHE);
    osi_assertx(lock_TryWrite(&scp->rw), "cm_scache_t rw held after allocation");
    lock_InitializeRWLock(&scp->bufCreateLock, "cm_scache_t bufCreateLock", LOCK_HIERARCHY_SCACHE_BUFCREATE);
#ifdef USE_BPLUS
    lock_InitializeRWLock(&scp->dirlock, "cm_scache_t dirlock", LOCK_HIERARCHY_SCACHE_DIRLOCK);
#endif
    scp->serverLock = -1;

    /* and put it in the LRU queue */
    osi_QAdd((osi_queue_t **) &cm_data.scacheLRUFirstp, &scp->q);
    if (!cm_data.scacheLRULastp)
        cm_data.scacheLRULastp = scp;
    cm_data.currentSCaches++;
    cm_dnlcPurgedp(scp); /* make doubly sure that this is not in dnlc */
    cm_dnlcPurgevp(scp);
    scp->allNextp = cm_data.allSCachesp;
    cm_data.allSCachesp = scp;
    return scp;
}

void cm_SetFid(cm_fid_t *fidp, afs_uint32 cell, afs_uint32 volume, afs_uint32 vnode, afs_uint32 unique)
{
    fidp->cell = cell;
    fidp->volume = volume;
    fidp->vnode = vnode;
    fidp->unique = unique;
    fidp->hash = ((cell & 0xF) << 28) | ((volume & 0x3F) << 22) | ((vnode & 0x7FF) << 11) | (unique & 0x7FF);
}

/* like strcmp, only for fids */
__inline int cm_FidCmp(cm_fid_t *ap, cm_fid_t *bp)
{
    if (ap->hash != bp->hash)
        return 1;
    if (ap->vnode != bp->vnode)
        return 1;
    if (ap->volume != bp->volume)
        return 1;
    if (ap->unique != bp->unique)
        return 1;
    if (ap->cell != bp->cell)
        return 1;
    return 0;
}

void cm_fakeSCacheInit(int newFile)
{
    if ( newFile ) {
        memset(&cm_data.fakeSCache, 0, sizeof(cm_scache_t));
        cm_data.fakeSCache.magic = CM_SCACHE_MAGIC;
        cm_data.fakeSCache.cbServerp = (struct cm_server *)(-1);
        cm_data.fakeSCache.cbExpires = (time_t)-1;
        /* can leave clientModTime at 0 */
        cm_data.fakeSCache.fileType = CM_SCACHETYPE_FILE;
        cm_data.fakeSCache.unixModeBits = 0777;
        cm_data.fakeSCache.length.LowPart = 1000;
        cm_data.fakeSCache.linkCount = 1;
        cm_data.fakeSCache.refCount = 1;
        cm_data.fakeSCache.serverLock = -1;
        cm_data.fakeSCache.dataVersion = CM_SCACHE_VERSION_BAD;
    }
    lock_InitializeRWLock(&cm_data.fakeSCache.rw, "cm_scache_t rw", LOCK_HIERARCHY_SCACHE);
    lock_InitializeRWLock(&cm_data.fakeSCache.bufCreateLock, "cm_scache_t bufCreateLock", LOCK_HIERARCHY_SCACHE_BUFCREATE);
    lock_InitializeRWLock(&cm_data.fakeSCache.dirlock, "cm_scache_t dirlock", LOCK_HIERARCHY_SCACHE_DIRLOCK);
}

long
cm_ValidateSCache(void)
{
    cm_scache_t * scp, *lscp;
    long i;

    if ( cm_data.scacheLRUFirstp == NULL && cm_data.scacheLRULastp != NULL ||
         cm_data.scacheLRUFirstp != NULL && cm_data.scacheLRULastp == NULL) {
        afsi_log("cm_ValidateSCache failure: inconsistent LRU pointers");
        fprintf(stderr, "cm_ValidateSCache failure: inconsistent LRU pointers\n");
        return -17;
    }

    for ( scp = cm_data.scacheLRUFirstp, lscp = NULL, i = 0;
          scp;
          lscp = scp, scp = (cm_scache_t *) osi_QNext(&scp->q), i++ ) {
        if (scp->magic != CM_SCACHE_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->magic != CM_SCACHE_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->magic != CM_SCACHE_MAGIC\n");
            return -1;
        }
        if (scp->nextp && scp->nextp->magic != CM_SCACHE_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->nextp->magic != CM_SCACHE_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->nextp->magic != CM_SCACHE_MAGIC\n");
            return -2;
        }
        if (scp->randomACLp && scp->randomACLp->magic != CM_ACLENT_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->randomACLp->magic != CM_ACLENT_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->randomACLp->magic != CM_ACLENT_MAGIC\n");
            return -3;
        }
        if (i > cm_data.currentSCaches ) {
            afsi_log("cm_ValidateSCache failure: LRU First queue loops");
            fprintf(stderr, "cm_ValidateSCache failure: LUR First queue loops\n");
            return -13;
        }
        if (lscp != (cm_scache_t *) osi_QPrev(&scp->q)) {
            afsi_log("cm_ValidateSCache failure: QPrev(scp) != previous");
            fprintf(stderr, "cm_ValidateSCache failure: QPrev(scp) != previous\n");
            return -15;
        }
    }

    for ( scp = cm_data.scacheLRULastp, lscp = NULL, i = 0; scp;
          lscp = scp, scp = (cm_scache_t *) osi_QPrev(&scp->q), i++ ) {
        if (scp->magic != CM_SCACHE_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->magic != CM_SCACHE_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->magic != CM_SCACHE_MAGIC\n");
            return -5;
        }
        if (scp->nextp && scp->nextp->magic != CM_SCACHE_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->nextp->magic != CM_SCACHE_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->nextp->magic != CM_SCACHE_MAGIC\n");
            return -6;
        }
        if (scp->randomACLp && scp->randomACLp->magic != CM_ACLENT_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->randomACLp->magic != CM_ACLENT_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->randomACLp->magic != CM_ACLENT_MAGIC\n");
            return -7;
        }
        if (i > cm_data.currentSCaches ) {
            afsi_log("cm_ValidateSCache failure: LRU Last queue loops");
            fprintf(stderr, "cm_ValidateSCache failure: LUR Last queue loops\n");
            return -14;
        }
        if (lscp != (cm_scache_t *) osi_QNext(&scp->q)) {
            afsi_log("cm_ValidateSCache failure: QNext(scp) != next");
            fprintf(stderr, "cm_ValidateSCache failure: QNext(scp) != next\n");
            return -16;
        }
    }

    for ( i=0; i < cm_data.scacheHashTableSize; i++ ) {
        for ( scp = cm_data.scacheHashTablep[i]; scp; scp = scp->nextp ) {
            afs_uint32 hash;
            hash = CM_SCACHE_HASH(&scp->fid);
            if (scp->magic != CM_SCACHE_MAGIC) {
                afsi_log("cm_ValidateSCache failure: scp->magic != CM_SCACHE_MAGIC");
                fprintf(stderr, "cm_ValidateSCache failure: scp->magic != CM_SCACHE_MAGIC\n");
                return -9;
            }
            if (scp->nextp && scp->nextp->magic != CM_SCACHE_MAGIC) {
                afsi_log("cm_ValidateSCache failure: scp->nextp->magic != CM_SCACHE_MAGIC");
                fprintf(stderr, "cm_ValidateSCache failure: scp->nextp->magic != CM_SCACHE_MAGIC\n");
                return -10;
            }
            if (scp->randomACLp && scp->randomACLp->magic != CM_ACLENT_MAGIC) {
                afsi_log("cm_ValidateSCache failure: scp->randomACLp->magic != CM_ACLENT_MAGIC");
                fprintf(stderr, "cm_ValidateSCache failure: scp->randomACLp->magic != CM_ACLENT_MAGIC\n");
                return -11;
            }
            if (hash != i) {
                afsi_log("cm_ValidateSCache failure: scp hash != hash index");
                fprintf(stderr, "cm_ValidateSCache failure: scp hash != hash index\n");
                return -13;
            }
        }
    }

    return cm_dnlcValidate();
}

void
cm_SuspendSCache(void)
{
    cm_scache_t * scp;
    time_t now;

    cm_GiveUpAllCallbacksAllServersMulti(TRUE);

    /*
     * After this call all servers are marked down.
     * Do not clear the callbacks, instead change the
     * expiration time so that the callbacks will be expired
     * when the servers are marked back up.  However, we
     * want the callbacks to be preserved as long as the
     * servers are down.  That way if the machine resumes
     * without network, the stat cache item will still be
     * considered valid.
     */
    now = time(NULL);

    lock_ObtainWrite(&cm_scacheLock);
    for ( scp = cm_data.allSCachesp; scp; scp = scp->allNextp ) {
        if (scp->cbServerp) {
            if (scp->flags & CM_SCACHEFLAG_PURERO) {
                cm_volume_t *volp = cm_GetVolumeByFID(&scp->fid);
                if (volp) {
                    if (volp->cbExpiresRO == scp->cbExpires)
                        volp->cbExpiresRO = now+1;
                    cm_PutVolume(volp);
                }
            }
            scp->cbExpires = now+1;
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);
}

long
cm_ShutdownSCache(void)
{
    cm_scache_t * scp, * nextp;

    lock_ObtainWrite(&cm_scacheLock);

    for ( scp = cm_data.allSCachesp; scp;
          scp = nextp ) {
        nextp = scp->allNextp;
        lock_ReleaseWrite(&cm_scacheLock);
#ifdef USE_BPLUS
        lock_ObtainWrite(&scp->dirlock);
#endif
        lock_ObtainWrite(&scp->rw);
        lock_ObtainWrite(&cm_scacheLock);

        if (scp->randomACLp) {
            cm_FreeAllACLEnts(scp);
        }

        if (scp->cbServerp) {
            cm_PutServer(scp->cbServerp);
            scp->cbServerp = NULL;
        }
        scp->cbExpires = 0;
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_CALLBACK);
        lock_ReleaseWrite(&scp->rw);

#ifdef USE_BPLUS
        if (scp->dirBplus)
            freeBtree(scp->dirBplus);
        scp->dirBplus = NULL;
        scp->dirDataVersion = CM_SCACHE_VERSION_BAD;
        lock_ReleaseWrite(&scp->dirlock);
        lock_FinalizeRWLock(&scp->dirlock);
#endif
        lock_FinalizeRWLock(&scp->rw);
        lock_FinalizeRWLock(&scp->bufCreateLock);
    }
    lock_ReleaseWrite(&cm_scacheLock);

    cm_GiveUpAllCallbacksAllServersMulti(FALSE);

    return cm_dnlcShutdown();
}

void cm_InitSCache(int newFile, long maxSCaches)
{
    static osi_once_t once;

    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_scacheLock, "cm_scacheLock", LOCK_HIERARCHY_SCACHE_GLOBAL);
        if ( newFile ) {
            memset(cm_data.scacheHashTablep, 0, sizeof(cm_scache_t *) * cm_data.scacheHashTableSize);
            cm_data.allSCachesp = NULL;
            cm_data.currentSCaches = 0;
            cm_data.maxSCaches = maxSCaches;
            cm_data.scacheLRUFirstp = cm_data.scacheLRULastp = NULL;
        } else {
            cm_scache_t * scp;

            for ( scp = cm_data.allSCachesp; scp;
                  scp = scp->allNextp ) {
                lock_InitializeRWLock(&scp->rw, "cm_scache_t rw", LOCK_HIERARCHY_SCACHE);
                lock_InitializeRWLock(&scp->bufCreateLock, "cm_scache_t bufCreateLock", LOCK_HIERARCHY_SCACHE_BUFCREATE);
#ifdef USE_BPLUS
                lock_InitializeRWLock(&scp->dirlock, "cm_scache_t dirlock", LOCK_HIERARCHY_SCACHE_DIRLOCK);
#endif
                scp->cbServerp = NULL;
                scp->cbExpires = 0;
                scp->volumeCreationDate = 0;
                scp->fileLocksH = NULL;
                scp->fileLocksT = NULL;
                scp->serverLock = (-1);
                scp->lastRefreshCycle = 0;
                scp->exclusiveLocks = 0;
                scp->sharedLocks = 0;
                scp->openReads = 0;
                scp->openWrites = 0;
                scp->openShares = 0;
                scp->openExcls = 0;
                scp->waitCount = 0;
                scp->activeRPCs = 0;
#ifdef USE_BPLUS
                scp->dirBplus = NULL;
                scp->dirDataVersion = CM_SCACHE_VERSION_BAD;
#endif
                scp->waitQueueT = NULL;
                _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_WAITING);
            }
        }
        cm_allFileLocks = NULL;
        cm_freeFileLocks = NULL;
        cm_lockRefreshCycle = 0;
        cm_fakeSCacheInit(newFile);
        cm_allFreeWaiters = NULL;
        cm_dnlcInit(newFile);
        osi_EndOnce(&once);
    }
}

/* version that doesn't bother creating the entry if we don't find it */
cm_scache_t *cm_FindSCache(cm_fid_t *fidp)
{
    long hash;
    cm_scache_t *scp;

    hash = CM_SCACHE_HASH(fidp);

    if (fidp->cell == 0) {
	return NULL;
    }

    lock_ObtainRead(&cm_scacheLock);
    for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
            cm_HoldSCacheNoLock(scp);
            lock_ConvertRToW(&cm_scacheLock);
            cm_AdjustScacheLRU(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            return scp;
        }
    }
    lock_ReleaseRead(&cm_scacheLock);
    return NULL;
}

#ifdef DEBUG_REFCOUNT
long cm_GetSCacheDbg(cm_fid_t *fidp, cm_scache_t **outScpp, cm_user_t *userp,
                  cm_req_t *reqp, char * file, long line)
#else
long cm_GetSCache(cm_fid_t *fidp, cm_scache_t **outScpp, cm_user_t *userp,
                  cm_req_t *reqp)
#endif
{
    long hash;
    cm_scache_t *scp = NULL;
    long code;
    cm_volume_t *volp = NULL;
    cm_cell_t *cellp;
    int special = 0; // yj: boolean variable to test if file is on root.afs
    int isRoot = 0;
    extern cm_fid_t cm_rootFid;

    hash = CM_SCACHE_HASH(fidp);

    if (fidp->cell == 0)
        return CM_ERROR_INVAL;

#ifdef AFS_FREELANCE_CLIENT
    special = (fidp->cell==AFS_FAKE_ROOT_CELL_ID &&
               fidp->volume==AFS_FAKE_ROOT_VOL_ID &&
               !(fidp->vnode==0x1 && fidp->unique==0x1));
    isRoot = (fidp->cell==AFS_FAKE_ROOT_CELL_ID &&
              fidp->volume==AFS_FAKE_ROOT_VOL_ID &&
              fidp->vnode==0x1 && fidp->unique==0x1);
#endif

    // yj: check if we have the scp, if so, we don't need
    // to do anything else
    lock_ObtainWrite(&cm_scacheLock);
    for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
#ifdef DEBUG_REFCOUNT
	    afsi_log("%s:%d cm_GetSCache (1) scp 0x%p ref %d", file, line, scp, scp->refCount);
	    osi_Log1(afsd_logp,"cm_GetSCache (1) scp 0x%p", scp);
#endif
#ifdef AFS_FREELANCE_CLIENT
            if (cm_freelanceEnabled && special &&
                cm_data.fakeDirVersion != scp->dataVersion)
                break;
#endif
            cm_HoldSCacheNoLock(scp);
            *outScpp = scp;
            cm_AdjustScacheLRU(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            return 0;
        }
    }

    // yj: when we get here, it means we don't have an scp
    // so we need to either load it or fake it, depending
    // on whether the file is "special", see below.

    // yj: if we're trying to get an scp for a file that's
    // on root.afs of homecell, we want to handle it specially
    // because we have to fill in the status stuff 'coz we
    // don't want trybulkstat to fill it in for us
#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && isRoot) {
        osi_Log0(afsd_logp,"cm_GetSCache Freelance and isRoot");
        /* freelance: if we are trying to get the root scp for the first
         * time, we will just put in a place holder entry.
         */
        volp = NULL;
    }

    if (cm_freelanceEnabled && special) {
        lock_ReleaseWrite(&cm_scacheLock);
        osi_Log0(afsd_logp,"cm_GetSCache Freelance and special");

        if (cm_getLocalMountPointChange()) {
            cm_clearLocalMountPointChange();
            cm_reInitLocalMountPoints();
        }

        lock_ObtainWrite(&cm_scacheLock);
        if (scp == NULL) {
            scp = cm_GetNewSCache();    /* returns scp->rw held */
            if (scp == NULL) {
                osi_Log0(afsd_logp,"cm_GetSCache unable to obtain *new* scache entry");
                return CM_ERROR_WOULDBLOCK;
            }
        } else {
            lock_ReleaseWrite(&cm_scacheLock);
            lock_ObtainWrite(&scp->rw);
            lock_ObtainWrite(&cm_scacheLock);
        }
        scp->fid = *fidp;
        scp->dotdotFid.cell=AFS_FAKE_ROOT_CELL_ID;
        scp->dotdotFid.volume=AFS_FAKE_ROOT_VOL_ID;
        scp->dotdotFid.unique=1;
        scp->dotdotFid.vnode=1;
        _InterlockedOr(&scp->flags, (CM_SCACHEFLAG_PURERO | CM_SCACHEFLAG_RO));
        if (!(scp->flags & CM_SCACHEFLAG_INHASH)) {
            scp->nextp = cm_data.scacheHashTablep[hash];
            cm_data.scacheHashTablep[hash] = scp;
            _InterlockedOr(&scp->flags, CM_SCACHEFLAG_INHASH);
        }
        scp->refCount = 1;
	osi_Log1(afsd_logp,"cm_GetSCache (freelance) sets refCount to 1 scp 0x%p", scp);

        /* must be called after the scp->fid is set */
        cm_FreelanceFetchMountPointString(scp);
        cm_FreelanceFetchFileType(scp);

        scp->length.LowPart = (DWORD)strlen(scp->mountPointStringp)+4;
        scp->length.HighPart = 0;
        scp->owner=0x0;
        scp->unixModeBits=0777;
        scp->clientModTime=FakeFreelanceModTime;
        scp->serverModTime=FakeFreelanceModTime;
        scp->parentUnique = 0x1;
        scp->parentVnode=0x1;
        scp->group=0;
        scp->dataVersion=cm_data.fakeDirVersion;
        scp->bufDataVersionLow=cm_data.fakeDirVersion;
        scp->lockDataVersion=CM_SCACHE_VERSION_BAD; /* no lock yet */
        scp->fsLockCount=0;
        lock_ReleaseWrite(&scp->rw);
        lock_ReleaseWrite(&cm_scacheLock);
	*outScpp = scp;
#ifdef DEBUG_REFCOUNT
	afsi_log("%s:%d cm_GetSCache (2) scp 0x%p ref %d", file, line, scp, scp->refCount);
	osi_Log1(afsd_logp,"cm_GetSCache (2) scp 0x%p", scp);
#endif
        return 0;
    }
    // end of yj code
#endif /* AFS_FREELANCE_CLIENT */

    /* otherwise, we need to find the volume */
    if (!cm_freelanceEnabled || !isRoot) {
        lock_ReleaseWrite(&cm_scacheLock);	/* for perf. reasons */
        cellp = cm_FindCellByID(fidp->cell, 0);
        if (!cellp)
            return CM_ERROR_NOSUCHCELL;

        code = cm_FindVolumeByID(cellp, fidp->volume, userp, reqp, CM_GETVOL_FLAG_CREATE, &volp);
        if (code)
            return code;
        lock_ObtainWrite(&cm_scacheLock);
    }

    /* otherwise, we have the volume, now reverify that the scp doesn't
     * exist, and proceed.
     */
    for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
#ifdef DEBUG_REFCOUNT
	    afsi_log("%s:%d cm_GetSCache (3) scp 0x%p ref %d", file, line, scp, scp->refCount);
	    osi_Log1(afsd_logp,"cm_GetSCache (3) scp 0x%p", scp);
#endif
            cm_HoldSCacheNoLock(scp);
            cm_AdjustScacheLRU(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            if (volp)
                cm_PutVolume(volp);
            *outScpp = scp;
            return 0;
        }
    }

    /* now, if we don't have the fid, recycle something */
    scp = cm_GetNewSCache();    /* returns scp->rw held */
    if (scp == NULL) {
	osi_Log0(afsd_logp,"cm_GetNewSCache unable to obtain *new* scache entry");
	lock_ReleaseWrite(&cm_scacheLock);
	if (volp)
	    cm_PutVolume(volp);
	return CM_ERROR_WOULDBLOCK;
    }
#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_GetNewSCache returns scp 0x%p flags 0x%x", file, line, scp, scp->flags);
#endif
    osi_Log2(afsd_logp,"cm_GetNewSCache returns scp 0x%p flags 0x%x", scp, scp->flags);

    osi_assertx(!(scp->flags & CM_SCACHEFLAG_INHASH), "CM_SCACHEFLAG_INHASH set");

    scp->fid = *fidp;
    if (!cm_freelanceEnabled || !isRoot) {
        /* if this scache entry represents a volume root then we need
         * to copy the dotdotFipd from the volume structure where the
         * "master" copy is stored (defect 11489)
         */
        if (volp->vol[ROVOL].ID == fidp->volume) {
	    _InterlockedOr(&scp->flags, (CM_SCACHEFLAG_PURERO | CM_SCACHEFLAG_RO));
            if (scp->fid.vnode == 1 && scp->fid.unique == 1)
                scp->dotdotFid = cm_VolumeStateByType(volp, ROVOL)->dotdotFid;
        } else if (volp->vol[BACKVOL].ID == fidp->volume) {
	    _InterlockedOr(&scp->flags, CM_SCACHEFLAG_RO);
            if (scp->fid.vnode == 1 && scp->fid.unique == 1)
                scp->dotdotFid = cm_VolumeStateByType(volp, BACKVOL)->dotdotFid;
        } else {
            if (scp->fid.vnode == 1 && scp->fid.unique == 1)
                scp->dotdotFid = cm_VolumeStateByType(volp, RWVOL)->dotdotFid;
        }
    }
    if (volp)
        cm_PutVolume(volp);
    scp->nextp = cm_data.scacheHashTablep[hash];
    cm_data.scacheHashTablep[hash] = scp;
    _InterlockedOr(&scp->flags, CM_SCACHEFLAG_INHASH);
    lock_ReleaseWrite(&scp->rw);
    scp->refCount = 1;
#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_GetSCache sets refCount to 1 scp 0x%p", file, line, scp);
#endif
    osi_Log1(afsd_logp,"cm_GetSCache sets refCount to 1 scp 0x%p", scp);

    /* XXX - The following fields in the cm_scache are
     * uninitialized:
     *   fileType
     *   parentVnode
     *   parentUnique
     */

    /* now we have a held scache entry; just return it */
    *outScpp = scp;
#ifdef DEBUG_REFCOUNT
    afsi_log("%s:%d cm_GetSCache (4) scp 0x%p ref %d", file, line, scp, scp->refCount);
    osi_Log1(afsd_logp,"cm_GetSCache (4) scp 0x%p", scp);
#endif
    lock_ReleaseWrite(&cm_scacheLock);
    return 0;
}

/* Returns a held reference to the scache's parent
 * if it exists */
cm_scache_t * cm_FindSCacheParent(cm_scache_t * scp)
{
    long code = 0;
    int i;
    cm_fid_t    parent_fid;
    cm_scache_t * pscp = NULL;

    lock_ObtainWrite(&cm_scacheLock);
    cm_SetFid(&parent_fid, scp->fid.cell, scp->fid.volume, scp->parentVnode, scp->parentUnique);

    if (cm_FidCmp(&scp->fid, &parent_fid)) {
	i = CM_SCACHE_HASH(&parent_fid);
	for (pscp = cm_data.scacheHashTablep[i]; pscp; pscp = pscp->nextp) {
	    if (!cm_FidCmp(&pscp->fid, &parent_fid)) {
		cm_HoldSCacheNoLock(pscp);
		break;
	    }
	}
    }

    lock_ReleaseWrite(&cm_scacheLock);

    return pscp;
}

void cm_SyncOpAddToWaitQueue(cm_scache_t * scp, afs_int32 flags, cm_buf_t * bufp)
{
    cm_scache_waiter_t * w;

    lock_ObtainWrite(&cm_scacheLock);
    if (cm_allFreeWaiters == NULL) {
        w = malloc(sizeof(*w));
        memset(w, 0, sizeof(*w));
    } else {
        w = (cm_scache_waiter_t *) cm_allFreeWaiters;
        osi_QRemove(&cm_allFreeWaiters, (osi_queue_t *) w);
    }

    w->threadId = thrd_Current();
    w->scp = scp;
    cm_HoldSCacheNoLock(scp);
    w->flags = flags;
    w->bufp = bufp;

    osi_QAddT(&scp->waitQueueH, &scp->waitQueueT, (osi_queue_t *) w);
    lock_ReleaseWrite(&cm_scacheLock);

    osi_Log2(afsd_logp, "cm_SyncOpAddToWaitQueue : Adding thread to wait queue scp 0x%p w 0x%p", scp, w);
}

int cm_SyncOpCheckContinue(cm_scache_t * scp, afs_int32 flags, cm_buf_t * bufp)
{
    cm_scache_waiter_t * w;
    int this_is_me;

    osi_Log0(afsd_logp, "cm_SyncOpCheckContinue checking for continuation");

    lock_ObtainRead(&cm_scacheLock);
    for (w = (cm_scache_waiter_t *)scp->waitQueueH;
         w;
         w = (cm_scache_waiter_t *)osi_QNext((osi_queue_t *) w)) {
        if (w->flags == flags && w->bufp == bufp) {
            break;
        }
    }

    osi_assertx(w != NULL, "null cm_scache_waiter_t");
    this_is_me = (w->threadId == thrd_Current());
    lock_ReleaseRead(&cm_scacheLock);

    if (!this_is_me) {
        osi_Log1(afsd_logp, "cm_SyncOpCheckContinue MISS: Waiter 0x%p", w);
        return 0;
    }

    osi_Log1(afsd_logp, "cm_SyncOpCheckContinue HIT: Waiter 0x%p", w);

    lock_ObtainWrite(&cm_scacheLock);
    osi_QRemoveHT(&scp->waitQueueH, &scp->waitQueueT, (osi_queue_t *) w);
    cm_ReleaseSCacheNoLock(scp);
    memset(w, 0, sizeof(*w));
    osi_QAdd(&cm_allFreeWaiters, (osi_queue_t *) w);
    lock_ReleaseWrite(&cm_scacheLock);

    return 1;
}


/* synchronize a fetch, store, read, write, fetch status or store status.
 * Called with scache mutex held, and returns with it held, but temporarily
 * drops it during the fetch.
 *
 * At most one flag can be on in flags, if this is an RPC request.
 *
 * Also, if we're fetching or storing data, we must ensure that we have a buffer.
 *
 * There are a lot of weird restrictions here; here's an attempt to explain the
 * rationale for the concurrency restrictions implemented in this function.
 *
 * First, although the file server will break callbacks when *another* machine
 * modifies a file or status block, the client itself is responsible for
 * concurrency control on its own requests.  Callback breaking events are rare,
 * and simply invalidate any concurrent new status info.
 *
 * In the absence of callback breaking messages, we need to know how to
 * synchronize incoming responses describing updates to files.  We synchronize
 * operations that update the data version by comparing the data versions.
 * However, updates that do not update the data, but only the status, can't be
 * synchronized with fetches or stores, since there's nothing to compare
 * to tell which operation executed first at the server.
 *
 * Thus, we can allow multiple ops that change file data, or dir data, and
 * fetches.  However, status storing ops have to be done serially.
 *
 * Furthermore, certain data-changing ops are incompatible: we can't read or
 * write a buffer while doing a truncate.  We can't read and write the same
 * buffer at the same time, or write while fetching or storing, or read while
 * fetching a buffer (this may change).  We can't fetch and store at the same
 * time, either.
 *
 * With respect to status, we can't read and write at the same time, read while
 * fetching, write while fetching or storing, or fetch and store at the same time.
 *
 * We can't allow a get callback RPC to run in concurrently with something that
 * will return updated status, since we could start a call, have the server
 * return status, have another machine make an update to the status (which
 * doesn't change serverModTime), have the original machine get a new callback,
 * and then have the original machine merge in the early, old info from the
 * first call.  At this point, the easiest way to avoid this problem is to have
 * getcallback calls conflict with all others for the same vnode.  Other calls
 * to cm_MergeStatus that aren't associated with calls to cm_SyncOp on the same
 * vnode must be careful not to merge in their status unless they have obtained
 * a callback from the start of their call.
 *
 * Note added 1/23/96
 * Concurrent StoreData RPC's can cause trouble if the file is being extended.
 * Each such RPC passes a FileLength parameter, which the server uses to do
 * pre-truncation if necessary.  So if two RPC's are processed out of order at
 * the server, the one with the smaller FileLength will be processed last,
 * possibly resulting in a bogus truncation.  The simplest way to avoid this
 * is to serialize all StoreData RPC's.  This is the reason we defined
 * CM_SCACHESYNC_STOREDATA_EXCL and CM_SCACHEFLAG_DATASTORING.
 */
long cm_SyncOp(cm_scache_t *scp, cm_buf_t *bufp, cm_user_t *userp, cm_req_t *reqp,
               afs_uint32 rights, afs_uint32 flags)
{
    osi_queueData_t *qdp;
    long code;
    cm_buf_t *tbufp;
    afs_uint32 outRights;
    int bufLocked;
    afs_uint32 sleep_scp_flags = 0;
    afs_uint32 sleep_buf_cmflags = 0;
    afs_uint32 sleep_scp_bufs = 0;
    int wakeupCycle;

    lock_AssertWrite(&scp->rw);

    /* lookup this first */
    bufLocked = flags & CM_SCACHESYNC_BUFLOCKED;

    if (bufp)
        osi_assertx(bufp->refCount > 0, "cm_buf_t refCount 0");


    /* Do the access check.  Now we don't really do the access check
     * atomically, since the caller doesn't expect the parent dir to be
     * returned locked, and that is what we'd have to do to prevent a
     * callback breaking message on the parent due to a setacl call from
     * being processed while we're running.  So, instead, we check things
     * here, and if things look fine with the access, we proceed to finish
     * the rest of this check.  Sort of a hack, but probably good enough.
     */

    while (1) {
        if (flags & CM_SCACHESYNC_FETCHSTATUS) {
            /* if we're bringing in a new status block, ensure that
             * we aren't already doing so, and that no one is
             * changing the status concurrently, either.  We need
             * to do this, even if the status is of a different
             * type, since we don't have the ability to figure out,
             * in the AFS 3 protocols, which status-changing
             * operation ran first, or even which order a read and
             * a write occurred in.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING | CM_SCACHEFLAG_SIZESETTING |
                              CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING|STORING|SIZESETTING|SIZESTORING|GETCALLBACK want FETCHSTATUS", scp);
                goto sleep;
            }
        }
        if (flags & (CM_SCACHESYNC_STORESIZE | CM_SCACHESYNC_STORESTATUS
                      | CM_SCACHESYNC_SETSIZE | CM_SCACHESYNC_GETCALLBACK)) {
            /* if we're going to make an RPC to change the status, make sure
             * that no one is bringing in or sending out the status.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING | CM_SCACHEFLAG_SIZESETTING |
                              CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING|STORING|SIZESETTING|SIZESTORING|GETCALLBACK want STORESIZE|STORESTATUS|SETSIZE|GETCALLBACK", scp);
                goto sleep;
            }
            if ((!bufp || bufp && scp->fileType == CM_SCACHETYPE_FILE) &&
                (scp->bufReadsp || scp->bufWritesp)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is bufRead|bufWrite want STORESIZE|STORESTATUS|SETSIZE|GETCALLBACK", scp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_FETCHDATA) {
            /* if we're bringing in a new chunk of data, make sure that
             * nothing is happening to that chunk, and that we aren't
             * changing the basic file status info, either.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING | CM_SCACHEFLAG_SIZESETTING |
                              CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING|STORING|SIZESETTING|SIZESTORING|GETCALLBACK want FETCHDATA", scp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING | CM_BUF_CMWRITING))) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%p bufp 0x%p is BUF_CMFETCHING|BUF_CMSTORING|BUF_CMWRITING want FETCHDATA", scp, bufp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_STOREDATA) {
            /* same as fetch data */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING
                               | CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING|STORING|SIZESTORING|GETCALLBACK want STOREDATA", scp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING | CM_BUF_CMWRITING))) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%p bufp 0x%p is BUF_CMFETCHING|BUF_CMSTORING|BUF_CMWRITING want STOREDATA", scp, bufp);
                goto sleep;
            }
        }

        if (flags & CM_SCACHESYNC_STOREDATA_EXCL) {
            /* Don't allow concurrent StoreData RPC's */
            if (scp->flags & CM_SCACHEFLAG_DATASTORING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is DATASTORING want STOREDATA_EXCL", scp);
                goto sleep;
            }
        }

        if (flags & CM_SCACHESYNC_ASYNCSTORE) {
            /* Don't allow more than one BKG store request */
            if (scp->flags & CM_SCACHEFLAG_ASYNCSTORING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is ASYNCSTORING want ASYNCSTORE", scp);
                goto sleep;
            }
        }

        if (flags & CM_SCACHESYNC_LOCK) {
            /* Don't allow concurrent fiddling with lock lists */
            if (scp->flags & CM_SCACHEFLAG_LOCKING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is LOCKING want LOCK", scp);
                goto sleep;
            }
        }

        /* now the operations that don't correspond to making RPCs */
        if (flags & CM_SCACHESYNC_GETSTATUS) {
            /* we can use the status that's here, if we're not
             * bringing in new status.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING want GETSTATUS", scp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_SETSTATUS) {
            /* we can make a change to the local status, as long as
             * the status isn't changing now.
             *
             * If we're fetching or storing a chunk of data, we can
             * change the status locally, since the fetch/store
             * operations don't change any of the data that we're
             * changing here.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING |
                              CM_SCACHEFLAG_SIZESETTING | CM_SCACHEFLAG_SIZESTORING)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING|STORING|SIZESETTING|SIZESTORING want SETSTATUS", scp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_READ) {
            /* we're going to read the data, make sure that the
             * status is available, and that the data is here.  It
             * is OK to read while storing the data back.
             */
            if (scp->flags & CM_SCACHEFLAG_FETCHING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING want READ", scp);
                goto sleep;
            }
            if (bufp && ((bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMFULLYFETCHED)) == CM_BUF_CMFETCHING)) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%p bufp 0x%p is BUF_CMFETCHING want READ", scp, bufp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & CM_BUF_CMWRITING)) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%p bufp 0x%p is BUF_CMWRITING want READ", scp, bufp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_WRITE) {
            /* don't write unless the status is stable and the chunk
             * is stable.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING | CM_SCACHEFLAG_SIZESETTING |
                              CM_SCACHEFLAG_SIZESTORING)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%p is FETCHING|STORING|SIZESETTING|SIZESTORING want WRITE", scp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & (CM_BUF_CMFETCHING |
                                          CM_BUF_CMSTORING |
                                          CM_BUF_CMWRITING))) {
                osi_Log3(afsd_logp, "CM SyncOp scp 0x%p bufp 0x%p is %s want WRITE",
                         scp, bufp,
                         ((bufp->cmFlags & CM_BUF_CMFETCHING) ? "CM_BUF_CMFETCHING":
                          ((bufp->cmFlags & CM_BUF_CMSTORING) ? "CM_BUF_CMSTORING" :
                           ((bufp->cmFlags & CM_BUF_CMWRITING) ? "CM_BUF_CMWRITING" :
                            "UNKNOWN!!!"))));
                goto sleep;
            }
        }

        if ((flags & CM_SCACHESYNC_NEEDCALLBACK)) {
            if ((flags & CM_SCACHESYNC_FORCECB) || !cm_HaveCallback(scp)) {
                osi_Log1(afsd_logp, "CM SyncOp getting callback on scp 0x%p",
                          scp);
                if (bufLocked)
		    lock_ReleaseMutex(&bufp->mx);
                code = cm_GetCallback(scp, userp, reqp, (flags & CM_SCACHESYNC_FORCECB)?1:0);
                if (bufLocked) {
                    lock_ReleaseWrite(&scp->rw);
                    lock_ObtainMutex(&bufp->mx);
                    lock_ObtainWrite(&scp->rw);
                }
                if (code)
                    return code;
		flags &= ~CM_SCACHESYNC_FORCECB;	/* only force once */
                continue;
            }
        }

        if (rights) {
            /* can't check access rights without a callback */
            osi_assertx(flags & CM_SCACHESYNC_NEEDCALLBACK, "!CM_SCACHESYNC_NEEDCALLBACK");

            if ((rights & (PRSFS_WRITE|PRSFS_DELETE)) && (scp->flags & CM_SCACHEFLAG_RO))
                return CM_ERROR_READONLY;

            if (cm_HaveAccessRights(scp, userp, rights, &outRights)) {
                if (~outRights & rights)
		    return CM_ERROR_NOACCESS;
            }
            else {
                /* we don't know the required access rights */
                if (bufLocked) lock_ReleaseMutex(&bufp->mx);
                code = cm_GetAccessRights(scp, userp, reqp);
                if (bufLocked) {
                    lock_ReleaseWrite(&scp->rw);
                    lock_ObtainMutex(&bufp->mx);
                    lock_ObtainWrite(&scp->rw);
                }
                if (code)
                    return code;
                continue;
            }
        }

        /* if we get here, we're happy */
        break;

      sleep:
        /* first check if we're not supposed to wait: fail
         * in this case, returning with everything still locked.
         */
        if (flags & CM_SCACHESYNC_NOWAIT)
            return CM_ERROR_WOULDBLOCK;

        /* These are used for minidump debugging */
	sleep_scp_flags = scp->flags;		/* so we know why we slept */
	sleep_buf_cmflags = bufp ? bufp->cmFlags : 0;
	sleep_scp_bufs = (scp->bufReadsp ? 1 : 0) | (scp->bufWritesp ? 2 : 0);

        /* wait here, then try again */
        osi_Log1(afsd_logp, "CM SyncOp sleeping scp 0x%p", scp);
        if ( scp->flags & CM_SCACHEFLAG_WAITING ) {
            scp->waitCount++;
            scp->waitRequests++;
            osi_Log3(afsd_logp, "CM SyncOp CM_SCACHEFLAG_WAITING already set for 0x%p; %d threads; %d requests",
                     scp, scp->waitCount, scp->waitRequests);
        } else {
            osi_Log1(afsd_logp, "CM SyncOp CM_SCACHEFLAG_WAITING set for 0x%p", scp);
            _InterlockedOr(&scp->flags, CM_SCACHEFLAG_WAITING);
            scp->waitCount = scp->waitRequests = 1;
        }

        cm_SyncOpAddToWaitQueue(scp, flags, bufp);
        wakeupCycle = 0;
        do {
            if (bufLocked)
                lock_ReleaseMutex(&bufp->mx);
            osi_SleepW((LONG_PTR) &scp->flags, &scp->rw);
            if (bufLocked)
                lock_ObtainMutex(&bufp->mx);
            lock_ObtainWrite(&scp->rw);
        } while (!cm_SyncOpCheckContinue(scp, flags, bufp));

	cm_UpdateServerPriority();

        scp->waitCount--;
        osi_Log3(afsd_logp, "CM SyncOp woke! scp 0x%p; still waiting %d threads of %d requests",
                 scp, scp->waitCount, scp->waitRequests);
        if (scp->waitCount == 0) {
            osi_Log1(afsd_logp, "CM SyncOp CM_SCACHEFLAG_WAITING reset for 0x%p", scp);
            _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_WAITING);
            scp->waitRequests = 0;
        }
    } /* big while loop */

    /* now, update the recorded state for RPC-type calls */
    if (flags & CM_SCACHESYNC_FETCHSTATUS)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_FETCHING);
    if (flags & CM_SCACHESYNC_STORESTATUS)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_STORING);
    if (flags & CM_SCACHESYNC_SETSIZE)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_SIZESETTING);
    if (flags & CM_SCACHESYNC_STORESIZE)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_SIZESTORING);
    if (flags & CM_SCACHESYNC_GETCALLBACK)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_GETCALLBACK);
    if (flags & CM_SCACHESYNC_STOREDATA_EXCL)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_DATASTORING);
    if (flags & CM_SCACHESYNC_ASYNCSTORE)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_ASYNCSTORING);
    if (flags & CM_SCACHESYNC_LOCK)
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_LOCKING);

    /* now update the buffer pointer */
    if (bufp && (flags & CM_SCACHESYNC_FETCHDATA)) {
        /* ensure that the buffer isn't already in the I/O list */
        for (qdp = scp->bufReadsp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            osi_assertx(tbufp != bufp, "unexpected cm_buf_t value");
        }

        /* queue a held reference to the buffer in the "reading" I/O list */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, bufp);

        buf_Hold(bufp);
        _InterlockedOr(&bufp->cmFlags, CM_BUF_CMFETCHING);
        osi_QAdd((osi_queue_t **) &scp->bufReadsp, &qdp->q);
    }

    if (bufp && (flags & CM_SCACHESYNC_STOREDATA)) {
        osi_assertx(scp->fileType == CM_SCACHETYPE_FILE,
            "attempting to store extents on a non-file object");

        /* ensure that the buffer isn't already in the I/O list */
        for (qdp = scp->bufWritesp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            osi_assertx(tbufp != bufp, "unexpected cm_buf_t value");
        }

        /* queue a held reference to the buffer in the "writing" I/O list */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, bufp);
        buf_Hold(bufp);
        _InterlockedOr(&bufp->cmFlags, CM_BUF_CMSTORING);
        osi_QAdd((osi_queue_t **) &scp->bufWritesp, &qdp->q);
    }

    if (bufp && (flags & CM_SCACHESYNC_WRITE)) {
        /* mark the buffer as being written to. */
        _InterlockedOr(&bufp->cmFlags, CM_BUF_CMWRITING);
    }

    return 0;
}

/* for those syncops that setup for RPCs.
 * Called with scache locked.
 */
void cm_SyncOpDone(cm_scache_t *scp, cm_buf_t *bufp, afs_uint32 flags)
{
    osi_queueData_t *qdp;
    cm_buf_t *tbufp;

    lock_AssertWrite(&scp->rw);

    /* now, update the recorded state for RPC-type calls */
    if (flags & CM_SCACHESYNC_FETCHSTATUS)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_FETCHING);
    if (flags & CM_SCACHESYNC_STORESTATUS)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_STORING);
    if (flags & CM_SCACHESYNC_SETSIZE)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_SIZESETTING);
    if (flags & CM_SCACHESYNC_STORESIZE)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_SIZESTORING);
    if (flags & CM_SCACHESYNC_GETCALLBACK)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_GETCALLBACK);
    if (flags & CM_SCACHESYNC_STOREDATA_EXCL)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_DATASTORING);
    if (flags & CM_SCACHESYNC_ASYNCSTORE)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_ASYNCSTORING);
    if (flags & CM_SCACHESYNC_LOCK)
        _InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_LOCKING);

    /* now update the buffer pointer */
    if (bufp && (flags & CM_SCACHESYNC_FETCHDATA)) {
	int release = 0;

	/* ensure that the buffer is in the I/O list */
        for (qdp = scp->bufReadsp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            if (tbufp == bufp)
		break;
        }
	if (qdp) {
	    osi_QRemove((osi_queue_t **) &scp->bufReadsp, &qdp->q);
	    osi_QDFree(qdp);
	    release = 1;
	}
        _InterlockedAnd(&bufp->cmFlags, ~(CM_BUF_CMFETCHING | CM_BUF_CMFULLYFETCHED));
        if (bufp->flags & CM_BUF_WAITING) {
            osi_Log2(afsd_logp, "CM SyncOpDone FetchData Waking [scp 0x%p] bufp 0x%p", scp, bufp);
            osi_Wakeup((LONG_PTR) &bufp);
        }
        if (release)
	    buf_Release(bufp);
    }

    /* now update the buffer pointer */
    if (bufp && (flags & CM_SCACHESYNC_STOREDATA)) {
	int release = 0;
        /* ensure that the buffer is in the I/O list */
        for (qdp = scp->bufWritesp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            if (tbufp == bufp)
		break;
        }
	if (qdp) {
	    osi_QRemove((osi_queue_t **) &scp->bufWritesp, &qdp->q);
	    osi_QDFree(qdp);
	    release = 1;
	}
        _InterlockedAnd(&bufp->cmFlags, ~CM_BUF_CMSTORING);
        if (bufp->flags & CM_BUF_WAITING) {
            osi_Log2(afsd_logp, "CM SyncOpDone StoreData Waking [scp 0x%p] bufp 0x%p", scp, bufp);
            osi_Wakeup((LONG_PTR) &bufp);
        }
        if (release)
	    buf_Release(bufp);
    }

    if (bufp && (flags & CM_SCACHESYNC_WRITE)) {
        osi_assertx(bufp->cmFlags & CM_BUF_CMWRITING, "!CM_BUF_CMWRITING");
        _InterlockedAnd(&bufp->cmFlags, ~CM_BUF_CMWRITING);
    }

    /* and wakeup anyone who is waiting */
    if (scp->flags & CM_SCACHEFLAG_WAITING) {
        osi_Log3(afsd_logp, "CM SyncOpDone 0x%x Waking scp 0x%p bufp 0x%p", flags, scp, bufp);
        osi_Wakeup((LONG_PTR) &scp->flags);
    }
}

/* merge in a response from an RPC.  The scp must be locked, and the callback
 * is optional.
 *
 * Don't overwrite any status info that is dirty, since we could have a store
 * operation (such as store data) that merges some info in, and we don't want
 * to lose the local updates.  Typically, there aren't many updates we do
 * locally, anyway, probably only mtime.
 *
 * There is probably a bug in here where a chmod (which doesn't change
 * serverModTime) that occurs between two fetches, both of whose responses are
 * handled after the callback breaking is done, but only one of whose calls
 * started before that, can cause old info to be merged from the first call.
 */
void cm_MergeStatus(cm_scache_t *dscp,
		    cm_scache_t *scp, AFSFetchStatus *statusp,
		    AFSVolSync *volsyncp,
                    cm_user_t *userp, cm_req_t *reqp, afs_uint32 flags)
{
    afs_uint64 dataVersion;
    struct cm_volume *volp = NULL;
    struct cm_cell *cellp = NULL;
    int rdr_invalidate = 0;
    afs_uint32 activeRPCs;

    lock_AssertWrite(&scp->rw);

    activeRPCs = 1 + InterlockedDecrement(&scp->activeRPCs);

    // yj: i want to create some fake status for the /afs directory and the
    // entries under that directory
#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && scp->fid.cell==AFS_FAKE_ROOT_CELL_ID &&
         scp->fid.volume==AFS_FAKE_ROOT_VOL_ID) {
        if (scp == cm_data.rootSCachep) {
            osi_Log0(afsd_logp,"cm_MergeStatus Freelance cm_data.rootSCachep");
            statusp->FileType = CM_SCACHETYPE_DIRECTORY;
            statusp->Length = cm_fakeDirSize;
            statusp->Length_hi = 0;
        } else {
            statusp->FileType = scp->fileType;
            statusp->Length = scp->length.LowPart;
            statusp->Length_hi = scp->length.HighPart;
        }
        statusp->InterfaceVersion = 0x1;
        statusp->LinkCount = scp->linkCount;
        statusp->DataVersion = (afs_uint32)(cm_data.fakeDirVersion & 0xFFFFFFFF);
        statusp->Author = 0x1;
        statusp->Owner = 0x0;
        statusp->CallerAccess = 0x9;
        statusp->AnonymousAccess = 0x9;
        statusp->UnixModeBits = 0777;
        statusp->ParentVnode = 0x1;
        statusp->ParentUnique = 0x1;
        statusp->ResidencyMask = 0;
        statusp->ClientModTime = FakeFreelanceModTime;
        statusp->ServerModTime = FakeFreelanceModTime;
        statusp->Group = 0;
        statusp->SyncCounter = 0;
        statusp->dataVersionHigh = (afs_uint32)(cm_data.fakeDirVersion >> 32);
        statusp->lockCount = 0;
        statusp->errorCode = 0;
    }
#endif /* AFS_FREELANCE_CLIENT */

    if (statusp->InterfaceVersion != 0x1) {
        osi_Log2(afsd_logp, "Merge, Failure scp 0x%p Invalid InterfaceVersion %u",
                 scp, statusp->InterfaceVersion);
        return;
    }

    if (statusp->errorCode != 0) {
        _InterlockedOr(&scp->flags, CM_SCACHEFLAG_EACCESS);
	osi_Log2(afsd_logp, "Merge, Failure scp 0x%p code 0x%x", scp, statusp->errorCode);

	scp->fileType = 0;	/* unknown */

	scp->serverModTime = 0;
	scp->clientModTime = 0;
	scp->length.LowPart = 0;
	scp->length.HighPart = 0;
	scp->serverLength.LowPart = 0;
	scp->serverLength.HighPart = 0;
	scp->linkCount = 0;
	scp->owner = 0;
	scp->group = 0;
	scp->unixModeBits = 0;
	scp->anyAccess = 0;
	scp->dataVersion = CM_SCACHE_VERSION_BAD;
        scp->bufDataVersionLow = CM_SCACHE_VERSION_BAD;
        scp->fsLockCount = 0;

	if (dscp) {
            scp->parentVnode = dscp->fid.vnode;
            scp->parentUnique = dscp->fid.unique;
	} else {
            scp->parentVnode = 0;
            scp->parentUnique = 0;
	}
	goto done;
    } else {
	_InterlockedAnd(&scp->flags, ~CM_SCACHEFLAG_EACCESS);
    }

    dataVersion = statusp->dataVersionHigh;
    dataVersion <<= 32;
    dataVersion |= statusp->DataVersion;

    if (!(flags & CM_MERGEFLAG_FORCE) &&
        dataVersion < scp->dataVersion &&
        scp->dataVersion != CM_SCACHE_VERSION_BAD) {

        cellp = cm_FindCellByID(scp->fid.cell, 0);
        if (scp->cbServerp) {
            cm_FindVolumeByID(cellp, scp->fid.volume, userp,
                              reqp, CM_GETVOL_FLAG_CREATE, &volp);
            osi_Log2(afsd_logp, "old data from server %x volume %s",
                      scp->cbServerp->addr.sin_addr.s_addr,
                      volp ? volp->namep : "(unknown)");
        }
        osi_Log3(afsd_logp, "Bad merge, scp 0x%p, scp dv %d, RPC dv %d",
                  scp, scp->dataVersion, dataVersion);
        /* we have a number of data fetch/store operations running
         * concurrently, and we can tell which one executed last at the
         * server by its mtime.
         * Choose the one with the largest mtime, and ignore the rest.
         *
         * These concurrent calls are incompatible with setting the
         * mtime, so we won't have a locally changed mtime here.
         *
         * We could also have ACL info for a different user than usual,
         * in which case we have to do that part of the merge, anyway.
         * We won't have to worry about the info being old, since we
         * won't have concurrent calls
         * that change file status running from this machine.
         *
         * Added 3/17/98:  if we see data version regression on an RO
         * file, it's probably due to a server holding an out-of-date
         * replica, rather than to concurrent RPC's.  Failures to
         * release replicas are now flagged by the volserver, but only
         * since AFS 3.4 5.22, so there are plenty of clients getting
         * out-of-date replicas out there.
         *
         * If we discover an out-of-date replica, by this time it's too
         * late to go to another server and retry.  Also, we can't
         * reject the merge, because then there is no way for
         * GetAccess to do its work, and the caller gets into an
         * infinite loop.  So we just grin and bear it.
         */
        if (!(scp->flags & CM_SCACHEFLAG_RO))
            goto done;
    }

    if (cm_readonlyVolumeVersioning)
        scp->volumeCreationDate = volsyncp->spare1;       /* volume creation date */

    scp->serverModTime = statusp->ServerModTime;

    if (!(scp->mask & CM_SCACHEMASK_CLIENTMODTIME)) {
        scp->clientModTime = statusp->ClientModTime;
    }
    if (!(scp->mask & CM_SCACHEMASK_LENGTH)) {
        scp->length.LowPart = statusp->Length;
        scp->length.HighPart = statusp->Length_hi;
    }

    scp->serverLength.LowPart = statusp->Length;
    scp->serverLength.HighPart = statusp->Length_hi;

    scp->linkCount = statusp->LinkCount;
    scp->owner = statusp->Owner;
    scp->group = statusp->Group;
    scp->unixModeBits = statusp->UnixModeBits & 07777;

    if (statusp->FileType == File)
        scp->fileType = CM_SCACHETYPE_FILE;
    else if (statusp->FileType == Directory)
        scp->fileType = CM_SCACHETYPE_DIRECTORY;
    else if (statusp->FileType == SymbolicLink) {
        if ((scp->unixModeBits & 0111) == 0)
            scp->fileType = CM_SCACHETYPE_MOUNTPOINT;
        else
            scp->fileType = CM_SCACHETYPE_SYMLINK;
    }
    else {
        osi_Log2(afsd_logp, "Merge, Invalid File Type (%d), scp 0x%p", statusp->FileType, scp);
        scp->fileType = CM_SCACHETYPE_INVALID;	/* invalid */
    }
    /* and other stuff */
    scp->parentVnode = statusp->ParentVnode;
    scp->parentUnique = statusp->ParentUnique;
    scp->fsLockCount = statusp->lockCount;

    /* and merge in the private acl cache info, if this is more than the public
     * info; merge in the public stuff in any case.
     */
    scp->anyAccess = statusp->AnonymousAccess;

    if (userp != NULL) {
        cm_AddACLCache(scp, userp, statusp->CallerAccess);
    }

    if (scp->dataVersion != 0 &&
        (!(flags & (CM_MERGEFLAG_DIROP|CM_MERGEFLAG_STOREDATA)) && dataVersion != scp->dataVersion ||
         (flags & (CM_MERGEFLAG_DIROP|CM_MERGEFLAG_STOREDATA)) && dataVersion - scp->dataVersion > activeRPCs)) {
        /*
         * We now know that all of the data buffers that we have associated
         * with this scp are invalid.  Subsequent operations will go faster
         * if the buffers are removed from the hash tables.
         *
         * We do not remove directory buffers if the dataVersion delta is 'activeRPCs' because
         * those version numbers will be updated as part of the directory operation.
         *
         * We do not remove storedata buffers because they will still be valid.
         */
        int i, j;
        cm_buf_t **lbpp;
        cm_buf_t *tbp;
        cm_buf_t *bp, *prevBp, *nextBp;

        lock_ObtainWrite(&buf_globalLock);
        i = BUF_FILEHASH(&scp->fid);
       	for (bp = cm_data.buf_fileHashTablepp[i]; bp; bp=nextBp)
	{
            nextBp = bp->fileHashp;
            /*
             * if the buffer belongs to this stat cache entry
             * and the buffer mutex can be obtained, check the
             * reference count and if it is zero, remove the buffer
             * from the hash tables.  If there are references,
             * the buffer might be updated to the current version
             * so leave it in place.
             */
            if (cm_FidCmp(&scp->fid, &bp->fid) == 0 &&
                 lock_TryMutex(&bp->mx)) {
                if (bp->refCount == 0 &&
                    !(bp->flags & CM_BUF_READING | CM_BUF_WRITING | CM_BUF_DIRTY)) {
                    prevBp = bp->fileHashBackp;
                    bp->fileHashBackp = bp->fileHashp = NULL;
                    if (prevBp)
                        prevBp->fileHashp = nextBp;
                    else
                        cm_data.buf_fileHashTablepp[i] = nextBp;
                    if (nextBp)
                        nextBp->fileHashBackp = prevBp;

                    j = BUF_HASH(&bp->fid, &bp->offset);
                    lbpp = &(cm_data.buf_scacheHashTablepp[j]);
                    for(tbp = *lbpp; tbp; lbpp = &tbp->hashp, tbp = tbp->hashp) {
                        if (tbp == bp)
                            break;
                    }

                    /* we better find it */
                    osi_assertx(tbp != NULL, "cm_MergeStatus: buf_scacheHashTablepp table screwup");

                    *lbpp = bp->hashp;	/* hash out */
                    bp->hashp = NULL;

                    _InterlockedAnd(&bp->qFlags, ~CM_BUF_QINHASH);
                }
                lock_ReleaseMutex(&bp->mx);
            }
	}
        lock_ReleaseWrite(&buf_globalLock);
    }

    /*
     * If the dataVersion has changed, the mountPointStringp must be cleared
     * in order to force a re-evaluation by cm_HandleLink().  The Windows CM
     * does not update a mountpoint or symlink by altering the contents of
     * the file data; but the Unix CM does.
     */
    if (scp->dataVersion != dataVersion && !(flags & CM_MERGEFLAG_FETCHDATA))
        scp->mountPointStringp[0] = '\0';

    /* We maintain a range of buffer dataVersion values which are considered
     * valid.  This avoids the need to update the dataVersion on each buffer
     * object during an uncontested storeData operation.  As a result this
     * merge status no longer has performance characteristics derived from
     * the size of the file.
     *
     * For directory buffers, only current dataVersion values are up to date.
     */
    if (((flags & CM_MERGEFLAG_STOREDATA) && dataVersion - scp->dataVersion > activeRPCs) ||
         (!(flags & CM_MERGEFLAG_STOREDATA) && scp->dataVersion != dataVersion) ||
         scp->bufDataVersionLow == 0 ||
         scp->fileType == CM_SCACHETYPE_DIRECTORY)
        scp->bufDataVersionLow = dataVersion;

    scp->dataVersion = dataVersion;

    /*
     * If someone is waiting for status information, we can wake them up
     * now even though the entity that issued the FetchStatus may not
     * have completed yet.
     */
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_FETCHSTATUS);

    /*
     * We just successfully merged status on the stat cache object.
     * This means that the associated volume must be online.
     */
    if (!volp) {
        if (!cellp)
            cellp = cm_FindCellByID(scp->fid.cell, 0);
        cm_FindVolumeByID(cellp, scp->fid.volume, userp, reqp, 0, &volp);
    }
    if (volp) {
        cm_vol_state_t *statep = cm_VolumeStateByID(volp, scp->fid.volume);
        if (statep->state != vl_online) {
            lock_ObtainWrite(&volp->rw);
            cm_VolumeStatusNotification(volp, statep->ID, statep->state, vl_online);
            statep->state = vl_online;
            lock_ReleaseWrite(&volp->rw);
        }
    }

  done:
    if (volp)
        cm_PutVolume(volp);
}

/* note that our stat cache info is incorrect, so force us eventually
 * to stat the file again.  There may be dirty data associated with
 * this vnode, and we want to preserve that information.
 *
 * This function works by simply simulating a loss of the callback.
 *
 * This function must be called with the scache locked.
 */
void cm_DiscardSCache(cm_scache_t *scp)
{
    lock_AssertWrite(&scp->rw);
    if (scp->cbServerp) {
        cm_PutServer(scp->cbServerp);
	scp->cbServerp = NULL;
    }
    scp->cbExpires = 0;
    scp->volumeCreationDate = 0;
    _InterlockedAnd(&scp->flags, ~(CM_SCACHEFLAG_CALLBACK | CM_SCACHEFLAG_LOCAL));
    cm_dnlcPurgedp(scp);
    cm_dnlcPurgevp(scp);
    cm_FreeAllACLEnts(scp);

    if (scp->fileType == CM_SCACHETYPE_DFSLINK)
        cm_VolStatus_Invalidate_DFS_Mapping(scp);

    /* Force mount points and symlinks to be re-evaluated */
    scp->mountPointStringp[0] = '\0';
}

void cm_AFSFidFromFid(AFSFid *afsFidp, cm_fid_t *fidp)
{
    afsFidp->Volume = fidp->volume;
    afsFidp->Vnode = fidp->vnode;
    afsFidp->Unique = fidp->unique;
}

#ifdef DEBUG_REFCOUNT
void cm_HoldSCacheNoLockDbg(cm_scache_t *scp, char * file, long line)
#else
void cm_HoldSCacheNoLock(cm_scache_t *scp)
#endif
{
    afs_int32 refCount;

    osi_assertx(scp != NULL, "null cm_scache_t");
    lock_AssertAny(&cm_scacheLock);
    refCount = InterlockedIncrement(&scp->refCount);
#ifdef DEBUG_REFCOUNT
    osi_Log2(afsd_logp,"cm_HoldSCacheNoLock scp 0x%p ref %d",scp, refCount);
    afsi_log("%s:%d cm_HoldSCacheNoLock scp 0x%p, ref %d", file, line, scp, refCount);
#endif
}

#ifdef DEBUG_REFCOUNT
void cm_HoldSCacheDbg(cm_scache_t *scp, char * file, long line)
#else
void cm_HoldSCache(cm_scache_t *scp)
#endif
{
    afs_int32 refCount;

    osi_assertx(scp != NULL, "null cm_scache_t");
    lock_ObtainRead(&cm_scacheLock);
    refCount = InterlockedIncrement(&scp->refCount);
#ifdef DEBUG_REFCOUNT
    osi_Log2(afsd_logp,"cm_HoldSCache scp 0x%p ref %d",scp, refCount);
    afsi_log("%s:%d cm_HoldSCache scp 0x%p ref %d", file, line, scp, refCount);
#endif
    lock_ReleaseRead(&cm_scacheLock);
}

#ifdef DEBUG_REFCOUNT
void cm_ReleaseSCacheNoLockDbg(cm_scache_t *scp, char * file, long line)
#else
void cm_ReleaseSCacheNoLock(cm_scache_t *scp)
#endif
{
    afs_int32 refCount;

    osi_assertx(scp != NULL, "null cm_scache_t");
    lock_AssertAny(&cm_scacheLock);

    refCount = InterlockedDecrement(&scp->refCount);
#ifdef DEBUG_REFCOUNT
    if (refCount < 0)
	osi_Log1(afsd_logp,"cm_ReleaseSCacheNoLock about to panic scp 0x%x",scp);
#endif
    osi_assertx(refCount >= 0, "cm_scache_t refCount 0");
#ifdef DEBUG_REFCOUNT
    osi_Log2(afsd_logp,"cm_ReleaseSCacheNoLock scp 0x%p ref %d",scp, refCount);
    afsi_log("%s:%d cm_ReleaseSCacheNoLock scp 0x%p ref %d", file, line, scp, refCount);
#endif

    if (refCount == 0 && (scp->flags & CM_SCACHEFLAG_DELETED)) {
        int deleted = 0;
        long      lockstate;

        lockstate = lock_GetRWLockState(&cm_scacheLock);
        if (lockstate != OSI_RWLOCK_WRITEHELD)
            lock_ReleaseRead(&cm_scacheLock);
        else
            lock_ReleaseWrite(&cm_scacheLock);

        lock_ObtainWrite(&scp->rw);
        if (scp->flags & CM_SCACHEFLAG_DELETED)
            deleted = 1;

        if (refCount == 0 && deleted) {
            lock_ObtainWrite(&cm_scacheLock);
            cm_RecycleSCache(scp, 0);
            if (lockstate != OSI_RWLOCK_WRITEHELD)
                lock_ConvertWToR(&cm_scacheLock);
        } else {
            if (lockstate != OSI_RWLOCK_WRITEHELD)
                lock_ObtainRead(&cm_scacheLock);
            else
                lock_ObtainWrite(&cm_scacheLock);
        }
        lock_ReleaseWrite(&scp->rw);
    }
}

#ifdef DEBUG_REFCOUNT
void cm_ReleaseSCacheDbg(cm_scache_t *scp, char * file, long line)
#else
void cm_ReleaseSCache(cm_scache_t *scp)
#endif
{
    afs_int32 refCount;

    osi_assertx(scp != NULL, "null cm_scache_t");
    lock_ObtainRead(&cm_scacheLock);
    refCount = InterlockedDecrement(&scp->refCount);
#ifdef DEBUG_REFCOUNT
    if (refCount < 0)
	osi_Log1(afsd_logp,"cm_ReleaseSCache about to panic scp 0x%x",scp);
#endif
    osi_assertx(refCount >= 0, "cm_scache_t refCount 0");
#ifdef DEBUG_REFCOUNT
    osi_Log2(afsd_logp,"cm_ReleaseSCache scp 0x%p ref %d",scp, refCount);
    afsi_log("%s:%d cm_ReleaseSCache scp 0x%p ref %d", file, line, scp, refCount);
#endif
    lock_ReleaseRead(&cm_scacheLock);

    if (scp->flags & CM_SCACHEFLAG_DELETED) {
        int deleted = 0;
        lock_ObtainWrite(&scp->rw);
        if (scp->flags & CM_SCACHEFLAG_DELETED)
            deleted = 1;
        if (deleted) {
            lock_ObtainWrite(&cm_scacheLock);
            cm_RecycleSCache(scp, 0);
            lock_ReleaseWrite(&cm_scacheLock);
        }
        lock_ReleaseWrite(&scp->rw);
    }
}

/* just look for the scp entry to get filetype */
/* doesn't need to be perfectly accurate, so locking doesn't matter too much */
int cm_FindFileType(cm_fid_t *fidp)
{
    long hash;
    cm_scache_t *scp;

    hash = CM_SCACHE_HASH(fidp);

    osi_assertx(fidp->cell != 0, "unassigned cell value");

    lock_ObtainWrite(&cm_scacheLock);
    for (scp=cm_data.scacheHashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
            lock_ReleaseWrite(&cm_scacheLock);
            return scp->fileType;
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);
    return 0;
}

/* dump all scp's that have reference count > 0 to a file.
 * cookie is used to identify this batch for easy parsing,
 * and it a string provided by a caller
 */
int cm_DumpSCache(FILE *outputFile, char *cookie, int lock)
{
    int zilch;
    cm_scache_t *scp;
    osi_queue_t *q;
    char output[2048];
    int i;

    if (lock)
        lock_ObtainRead(&cm_scacheLock);

    sprintf(output, "%s - dumping all scache - cm_data.currentSCaches=%d, cm_data.maxSCaches=%d\r\n", cookie, cm_data.currentSCaches, cm_data.maxSCaches);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    for (scp = cm_data.allSCachesp; scp; scp = scp->allNextp)
    {
        time_t t;
        char *srvStr = NULL;
        afs_uint32 srvStrRpc = TRUE;
        char *cbt = NULL;
        char *cdrot = NULL;

        if (scp->cbServerp) {
            if (!((scp->cbServerp->flags & CM_SERVERFLAG_UUID) &&
                UuidToString((UUID *)&scp->cbServerp->uuid, &srvStr) == RPC_S_OK)) {
                afs_asprintf(&srvStr, "%.0I", scp->cbServerp->addr.sin_addr.s_addr);
                srvStrRpc = FALSE;
            }
        }
        if (scp->cbExpires) {
            t = scp->cbExpires;
            cbt = ctime(&t);
            if (cbt) {
                cbt = strdup(cbt);
                cbt[strlen(cbt)-1] = '\0';
            }
        }
        if (scp->volumeCreationDate) {
            t = scp->volumeCreationDate;
            cdrot = ctime(&t);
            if (cdrot) {
                cdrot = strdup(cdrot);
                cdrot[strlen(cdrot)-1] = '\0';
            }
        }
        sprintf(output,
                "%s scp=0x%p, fid (cell=%d, volume=%d, vnode=%d, unique=%d) type=%d dv=%I64d len=0x%I64x "
                "mp='%s' Locks (server=0x%x shared=%d excl=%d clnt=%d) fsLockCount=%d linkCount=%d anyAccess=0x%x "
                "flags=0x%x cbServer='%s' cbExpires='%s' volumeCreationDate='%s' refCount=%u\r\n",
                cookie, scp, scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique,
                scp->fileType, scp->dataVersion, scp->length.QuadPart, scp->mountPointStringp,
                scp->serverLock, scp->sharedLocks, scp->exclusiveLocks, scp->clientLocks, scp->fsLockCount,
                scp->linkCount, scp->anyAccess, scp->flags, srvStr ? srvStr : "<none>", cbt ? cbt : "<none>",
                cdrot ? cdrot : "<none>", scp->refCount);
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

        if (scp->fileLocksH) {
            sprintf(output, "  %s - begin dumping scp locks\r\n", cookie);
            WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

            for (q = scp->fileLocksH; q; q = osi_QNext(q)) {
                cm_file_lock_t * lockp = (cm_file_lock_t *)((char *) q - offsetof(cm_file_lock_t, fileq));
                sprintf(output, "  %s lockp=0x%p scp=0x%p, cm_userp=0x%p offset=0x%I64x len=0x%08I64x type=0x%x "
                        "key=0x%I64x flags=0x%x update=0x%I64u\r\n",
                        cookie, lockp, lockp->scp, lockp->userp, lockp->range.offset, lockp->range.length,
                        lockp->lockType, lockp->key, lockp->flags, (afs_uint64)lockp->lastUpdate);
                WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
            }

            sprintf(output, "  %s - done dumping scp locks\r\n", cookie);
            WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
        }

        if (srvStr) {
            if (srvStrRpc)
                RpcStringFree(&srvStr);
            else
                free(srvStr);
        }
        if (cbt)
            free(cbt);
        if (cdrot)
            free(cdrot);
    }

    sprintf(output, "%s - Done dumping all scache.\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    sprintf(output, "%s - dumping cm_data.scacheHashTable - cm_data.scacheHashTableSize=%d\r\n",
            cookie, cm_data.scacheHashTableSize);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    for (i = 0; i < cm_data.scacheHashTableSize; i++)
    {
        for(scp = cm_data.scacheHashTablep[i]; scp; scp=scp->nextp)
        {
            sprintf(output, "%s scp=0x%p, hash=%d, fid (cell=%d, volume=%d, vnode=%d, unique=%d)\r\n",
                    cookie, scp, i, scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique);
            WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
        }
    }

    sprintf(output, "%s - Done dumping cm_data.scacheHashTable\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    sprintf(output, "%s - begin dumping all file locks\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    for (q = cm_allFileLocks; q; q = osi_QNext(q)) {
        cm_file_lock_t * lockp = (cm_file_lock_t *)q;
        sprintf(output, "%s filelockp=0x%p scp=0x%p, cm_userp=0x%p offset=0x%I64x len=0x%08I64x type=0x%x key=0x%I64x flags=0x%x update=0x%I64u\r\n",
                 cookie, lockp, lockp->scp, lockp->userp, lockp->range.offset, lockp->range.length,
                 lockp->lockType, lockp->key, lockp->flags, (afs_uint64)lockp->lastUpdate);
        WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);
    }

    sprintf(output, "%s - done dumping all file locks\r\n", cookie);
    WriteFile(outputFile, output, (DWORD)strlen(output), &zilch, NULL);

    if (lock)
        lock_ReleaseRead(&cm_scacheLock);
    return (0);
}

