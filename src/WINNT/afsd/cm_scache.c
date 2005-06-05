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
#include <nb30.h>
#endif /* !DJGPP */
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <osi.h>

#include "afsd.h"

/*extern void afsi_log(char *pattern, ...);*/

extern osi_hyper_t hzero;

/* File locks */
osi_queue_t *cm_allFileLocks;

/* lock for globals */
osi_rwlock_t cm_scacheLock;

/* Dummy scache entry for use with pioctl fids */
cm_scache_t cm_fakeSCache;

#ifdef AFS_FREELANCE_CLIENT
extern osi_mutex_t cm_Freelance_Lock;
#endif

/* must be called with cm_scacheLock write-locked! */
void cm_AdjustLRU(cm_scache_t *scp)
{
    if (scp == cm_data.scacheLRULastp)
        cm_data.scacheLRULastp = (cm_scache_t *) osi_QPrev(&scp->q);
    osi_QRemove((osi_queue_t **) &cm_data.scacheLRUFirstp, &scp->q);
    osi_QAdd((osi_queue_t **) &cm_data.scacheLRUFirstp, &scp->q);
    if (!cm_data.scacheLRULastp) 
        cm_data.scacheLRULastp = scp;
}

/* called with cm_scacheLock write-locked; find a vnode to recycle.
 * Can allocate a new one if desperate, or if below quota (cm_data.maxSCaches).
 */
cm_scache_t *cm_GetNewSCache(void)
{
    cm_scache_t *scp;
    int i;
    cm_scache_t **lscpp;
    cm_scache_t *tscp;

    if (cm_data.currentSCaches >= cm_data.maxSCaches) {
        for (scp = cm_data.scacheLRULastp;
              scp;
              scp = (cm_scache_t *) osi_QPrev(&scp->q)) {
            if (scp->refCount == 0) 
                break;
        }
                
        if (scp) {
            osi_assert(scp >= cm_data.scacheBaseAddress && scp < (cm_scache_t *)cm_data.hashTablep);
            /* we found an entry, so return it */
            if (scp->flags & CM_SCACHEFLAG_INHASH) {
                /* hash it out first */
                i = CM_SCACHE_HASH(&scp->fid);
                for (lscpp = &cm_data.hashTablep[i], tscp = cm_data.hashTablep[i];
                      tscp;
                      lscpp = &tscp->nextp, tscp = tscp->nextp) {
                    if (tscp == scp) {
                        *lscpp = scp->nextp;
                        scp->flags &= ~CM_SCACHEFLAG_INHASH;
                        break;
                    }
                }
                osi_assertx(tscp, "afsd: scache hash screwup");
            }

            /* look for things that shouldn't still be set */
            osi_assert(scp->bufWritesp == NULL);
            osi_assert(scp->bufReadsp == NULL);

            /* invalidate so next merge works fine;
            * also initialize some flags */
            scp->flags &= ~(CM_SCACHEFLAG_STATD
                             | CM_SCACHEFLAG_RO
                             | CM_SCACHEFLAG_PURERO
                             | CM_SCACHEFLAG_OVERQUOTA
                             | CM_SCACHEFLAG_OUTOFSPACE);
            scp->serverModTime = 0;
            scp->dataVersion = 0;
            scp->bulkStatProgress = hzero;
            scp->waitCount = 0;

            scp->fid.vnode = 0;
            scp->fid.volume = 0;
            scp->fid.unique = 0;
            scp->fid.cell = 0;

            /* discard callback */
            if (scp->cbServerp) {
                cm_PutServer(scp->cbServerp);
                scp->cbServerp = NULL;
            }
            scp->cbExpires = 0;

            /* remove from dnlc */
            cm_dnlcPurgedp(scp);
            cm_dnlcPurgevp(scp);

            /* discard cached status; if non-zero, Close
             * tried to store this to server but failed */
            scp->mask = 0;

            /* drop held volume ref */
            if (scp->volp) {
                cm_PutVolume(scp->volp);
                scp->volp = NULL;
            }

            /* discard symlink info */
            scp->mountPointStringp[0] = 0;
            memset(&scp->mountRootFid, 0, sizeof(cm_fid_t));
            memset(&scp->dotdotFid, 0, sizeof(cm_fid_t));

            /* not locked, but there can be no references to this guy
             * while we hold the global refcount lock.
             */
            cm_FreeAllACLEnts(scp);

            /* now remove from the LRU queue and put it back at the
             * head of the LRU queue.
             */
            cm_AdjustLRU(scp);

            /* and we're done */
            return scp;
        }
    }
        
    /* if we get here, we should allocate a new scache entry.  We either are below
     * quota or we have a leak and need to allocate a new one to avoid panicing.
     */
    scp = cm_data.scacheBaseAddress + cm_data.currentSCaches;
    osi_assert(scp >= cm_data.scacheBaseAddress && scp < (cm_scache_t *)cm_data.hashTablep);
    memset(scp, 0, sizeof(cm_scache_t));
    scp->magic = CM_SCACHE_MAGIC;
    lock_InitializeMutex(&scp->mx, "cm_scache_t mutex");
    lock_InitializeRWLock(&scp->bufCreateLock, "cm_scache_t bufCreateLock");

    /* and put it in the LRU queue */
    osi_QAdd((osi_queue_t **) &cm_data.scacheLRUFirstp, &scp->q);
    if (!cm_data.scacheLRULastp) 
        cm_data.scacheLRULastp = scp;
    cm_data.currentSCaches++;
    cm_dnlcPurgedp(scp); /* make doubly sure that this is not in dnlc */
    cm_dnlcPurgevp(scp); 
    return scp;
}       

/* like strcmp, only for fids */
int cm_FidCmp(cm_fid_t *ap, cm_fid_t *bp)
{
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
        cm_data.fakeSCache.cbServerp = (struct cm_server *)(-1);
        /* can leave clientModTime at 0 */
        cm_data.fakeSCache.fileType = CM_SCACHETYPE_FILE;
        cm_data.fakeSCache.unixModeBits = 0777;
        cm_data.fakeSCache.length.LowPart = 1000;
        cm_data.fakeSCache.linkCount = 1;
        cm_data.fakeSCache.refCount = 1;
    }
    lock_InitializeMutex(&cm_data.fakeSCache.mx, "cm_scache_t mutex");
}       

long
cm_ValidateSCache(void)
{
    cm_scache_t * scp;
    long i;

    for ( scp = cm_data.scacheLRUFirstp; scp;
          scp = (cm_scache_t *) osi_QNext(&scp->q) ) {
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
        if (scp->volp && scp->volp->magic != CM_VOLUME_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->volp->magic != CM_VOLUME_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->volp->magic != CM_VOLUME_MAGIC\n");
            return -4;
        }
    }

    for ( scp = cm_data.scacheLRULastp; scp;
          scp = (cm_scache_t *) osi_QPrev(&scp->q) ) {
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
        if (scp->volp && scp->volp->magic != CM_VOLUME_MAGIC) {
            afsi_log("cm_ValidateSCache failure: scp->volp->magic != CM_VOLUME_MAGIC");
            fprintf(stderr, "cm_ValidateSCache failure: scp->volp->magic != CM_VOLUME_MAGIC\n");
            return -8;
        }
    }

    for ( i=0; i < cm_data.hashTableSize; i++ ) {
        for ( scp = cm_data.hashTablep[i]; scp; scp = scp->nextp ) {
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
            if (scp->volp && scp->volp->magic != CM_VOLUME_MAGIC) {
                afsi_log("cm_ValidateSCache failure: scp->volp->magic != CM_VOLUME_MAGIC");
                fprintf(stderr, "cm_ValidateSCache failure: scp->volp->magic != CM_VOLUME_MAGIC\n");
                return -12;
            }
        }
    }

    return cm_dnlcValidate();
}

long
cm_ShutdownSCache(void)
{
    cm_scache_t * scp;

    for ( scp = cm_data.scacheLRULastp; scp;
          scp = (cm_scache_t *) osi_QPrev(&scp->q) ) {
        if (scp->randomACLp) {
            lock_ObtainMutex(&scp->mx);
            cm_FreeAllACLEnts(scp);
            lock_ReleaseMutex(&scp->mx);
        }
        lock_FinalizeMutex(&scp->mx);
        lock_FinalizeRWLock(&scp->bufCreateLock);
    }

    return cm_dnlcShutdown();
}

void cm_InitSCache(int newFile, long maxSCaches)
{
    static osi_once_t once;
        
    if (osi_Once(&once)) {
        lock_InitializeRWLock(&cm_scacheLock, "cm_scacheLock");
        if ( newFile ) {
            memset(cm_data.hashTablep, 0, sizeof(cm_scache_t *) * cm_data.hashTableSize);
            cm_data.currentSCaches = 0;
            cm_data.maxSCaches = maxSCaches;
            cm_data.scacheLRUFirstp = cm_data.scacheLRULastp = NULL;
        } else {
            cm_scache_t * scp;

            for ( scp = cm_data.scacheLRULastp; scp;
                  scp = (cm_scache_t *) osi_QPrev(&scp->q) ) {
                lock_InitializeMutex(&scp->mx, "cm_scache_t mutex");
                lock_InitializeRWLock(&scp->bufCreateLock, "cm_scache_t bufCreateLock");

                scp->cbServerp = NULL;
                scp->cbExpires = 0;
                scp->fileLocks = NULL;
                scp->openReads = 0;
                scp->openWrites = 0;
                scp->openShares = 0;
                scp->openExcls = 0;
                scp->waitCount = 0;
                scp->flags &= ~CM_SCACHEFLAG_WAITING;
            }
        }
        cm_allFileLocks = NULL;
        cm_fakeSCacheInit(newFile);
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
        
    osi_assert(fidp->cell != 0);

    lock_ObtainWrite(&cm_scacheLock);
    for (scp=cm_data.hashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
            cm_HoldSCacheNoLock(scp);
            cm_AdjustLRU(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            return scp;
        }
    }
    lock_ReleaseWrite(&cm_scacheLock);
    return NULL;
}

long cm_GetSCache(cm_fid_t *fidp, cm_scache_t **outScpp, cm_user_t *userp,
                  cm_req_t *reqp)
{
    long hash;
    cm_scache_t *scp;
    long code;
    cm_volume_t *volp = 0;
    cm_cell_t *cellp;
    char* mp = 0;
    int special; // yj: boolean variable to test if file is on root.afs
    int isRoot;
    extern cm_fid_t cm_rootFid;
        
    hash = CM_SCACHE_HASH(fidp);
        
    osi_assert(fidp->cell != 0);

    if (fidp->cell== cm_data.rootFid.cell && 
         fidp->volume==cm_data.rootFid.volume &&
         fidp->vnode==0x0 && fidp->unique==0x0)
    {
        osi_Log0(afsd_logp,"cm_getSCache called with root cell/volume and vnode=0 and unique=0");
    }

    // yj: check if we have the scp, if so, we don't need
    // to do anything else
    lock_ObtainWrite(&cm_scacheLock);
    for (scp=cm_data.hashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
            cm_HoldSCacheNoLock(scp);
            *outScpp = scp;
            cm_AdjustLRU(scp);
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
    special = (fidp->cell==AFS_FAKE_ROOT_CELL_ID && 
               fidp->volume==AFS_FAKE_ROOT_VOL_ID &&
               !(fidp->vnode==0x1 && fidp->unique==0x1));
    isRoot = (fidp->cell==AFS_FAKE_ROOT_CELL_ID && 
              fidp->volume==AFS_FAKE_ROOT_VOL_ID &&
              fidp->vnode==0x1 && fidp->unique==0x1);
    if (cm_freelanceEnabled && isRoot) {
        osi_Log0(afsd_logp,"cm_getSCache Freelance and isRoot");
        /* freelance: if we are trying to get the root scp for the first
         * time, we will just put in a place holder entry. 
         */
        volp = NULL;
    }
	  
    if (cm_freelanceEnabled && special) {
        osi_Log0(afsd_logp,"cm_getSCache Freelance and special");
        if (fidp->vnode > 1 && fidp->vnode <= cm_noLocalMountPoints + 2) {
	    lock_ObtainMutex(&cm_Freelance_Lock);
            mp =(cm_localMountPoints+fidp->vnode-2)->mountPointStringp;
            lock_ReleaseMutex(&cm_Freelance_Lock);
        } else {
            mp = "";
        }
        scp = cm_GetNewSCache();
		
        scp->fid = *fidp;
        scp->volp = cm_data.rootSCachep->volp;
        scp->dotdotFid.cell=AFS_FAKE_ROOT_CELL_ID;
        scp->dotdotFid.volume=AFS_FAKE_ROOT_VOL_ID;
        scp->dotdotFid.unique=1;
        scp->dotdotFid.vnode=1;
        scp->flags |= (CM_SCACHEFLAG_PURERO | CM_SCACHEFLAG_RO);
        scp->nextp=cm_data.hashTablep[hash];
        cm_data.hashTablep[hash]=scp;
        scp->flags |= CM_SCACHEFLAG_INHASH;
        scp->refCount = 1;
        if (fidp->vnode > 1 && fidp->vnode <= cm_noLocalMountPoints + 2)
            scp->fileType = (cm_localMountPoints+fidp->vnode-2)->fileType;
        else 
            scp->fileType = CM_SCACHETYPE_INVALID;

        lock_ObtainMutex(&cm_Freelance_Lock);
        scp->length.LowPart = strlen(mp)+4;
        strncpy(scp->mountPointStringp,mp,MOUNTPOINTLEN);
        scp->mountPointStringp[MOUNTPOINTLEN-1] = '\0';
        lock_ReleaseMutex(&cm_Freelance_Lock);

        scp->owner=0x0;
        scp->unixModeBits=0x1ff;
        scp->clientModTime=FakeFreelanceModTime;
        scp->serverModTime=FakeFreelanceModTime;
        scp->parentUnique = 0x1;
        scp->parentVnode=0x1;
        scp->group=0;
        scp->dataVersion=cm_data.fakeDirVersion;
        *outScpp = scp;
        lock_ReleaseWrite(&cm_scacheLock);
        /*afsi_log("   getscache done");*/
        return 0;
    }
    // end of yj code
#endif /* AFS_FREELANCE_CLIENT */

    /* otherwise, we need to find the volume */
    if (!cm_freelanceEnabled || !isRoot) {
        lock_ReleaseWrite(&cm_scacheLock);	/* for perf. reasons */
        cellp = cm_FindCellByID(fidp->cell);
        if (!cellp) 
            return CM_ERROR_NOSUCHCELL;

        code = cm_GetVolumeByID(cellp, fidp->volume, userp, reqp, &volp);
        if (code) 
            return code;
        lock_ObtainWrite(&cm_scacheLock);
    }
        
    /* otherwise, we have the volume, now reverify that the scp doesn't
     * exist, and proceed.
     */
    for (scp=cm_data.hashTablep[hash]; scp; scp=scp->nextp) {
        if (cm_FidCmp(fidp, &scp->fid) == 0) {
            cm_HoldSCacheNoLock(scp);
            osi_assert(scp->volp == volp);
            cm_AdjustLRU(scp);
            lock_ReleaseWrite(&cm_scacheLock);
            if (volp)
                cm_PutVolume(volp);
            *outScpp = scp;
            return 0;
        }
    }
        
    /* now, if we don't have the fid, recycle something */
    scp = cm_GetNewSCache();
    osi_assert(!(scp->flags & CM_SCACHEFLAG_INHASH));
    scp->fid = *fidp;
    scp->volp = volp;	/* a held reference */

    if (!cm_freelanceEnabled || !isRoot) {
        /* if this scache entry represents a volume root then we need 
         * to copy the dotdotFipd from the volume structure where the 
         * "master" copy is stored (defect 11489)
         */
        if (scp->fid.vnode == 1 && scp->fid.unique == 1) {
	    scp->dotdotFid = volp->dotdotFid;
        }
	  
        if (volp->roID == fidp->volume)
	    scp->flags |= (CM_SCACHEFLAG_PURERO | CM_SCACHEFLAG_RO);
        else if (volp->bkID == fidp->volume)
	    scp->flags |= CM_SCACHEFLAG_RO;
    }
    scp->nextp = cm_data.hashTablep[hash];
    cm_data.hashTablep[hash] = scp;
    scp->flags |= CM_SCACHEFLAG_INHASH;
    scp->refCount = 1;

    /* XXX - The following fields in the cm_scache are 
     * uninitialized:
     *   fileType
     *   parentVnode
     *   parentUnique
     */
    lock_ReleaseWrite(&cm_scacheLock);
        
    /* now we have a held scache entry; just return it */
    *outScpp = scp;
    return 0;
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
long cm_SyncOp(cm_scache_t *scp, cm_buf_t *bufp, cm_user_t *up, cm_req_t *reqp,
               long rights, long flags)
{
    osi_queueData_t *qdp;
    long code;
    cm_buf_t *tbufp;
    long outRights;
    int bufLocked;

    /* lookup this first */
    bufLocked = flags & CM_SCACHESYNC_BUFLOCKED;

    /* some minor assertions */
    if (flags & (CM_SCACHESYNC_STOREDATA | CM_SCACHESYNC_FETCHDATA
                  | CM_SCACHESYNC_READ | CM_SCACHESYNC_WRITE
                  | CM_SCACHESYNC_SETSIZE)) {
        if (bufp) {
            osi_assert(bufp->refCount > 0);
            /*
               osi_assert(cm_FidCmp(&bufp->fid, &scp->fid) == 0);
             */
        }
    }
    else osi_assert(bufp == NULL);

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
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING
                               | CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING|STORING|SIZESTORING|GETCALLBACK want FETCHSTATUS", scp);
                goto sleep;
            }
        }
        if (flags & (CM_SCACHESYNC_STORESIZE | CM_SCACHESYNC_STORESTATUS
                      | CM_SCACHESYNC_SETSIZE | CM_SCACHESYNC_GETCALLBACK)) {
            /* if we're going to make an RPC to change the status, make sure
             * that no one is bringing in or sending out the status.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING |
                              CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING|STORING|SIZESTORING|GETCALLBACK want STORESIZE|STORESTATUS|SETSIZE|GETCALLBACK", scp);
                goto sleep;
            }
            if (scp->bufReadsp || scp->bufWritesp) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is bufRead|bufWrite want STORESIZE|STORESTATUS|SETSIZE|GETCALLBACK", scp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_FETCHDATA) {
            /* if we're bringing in a new chunk of data, make sure that
             * nothing is happening to that chunk, and that we aren't
             * changing the basic file status info, either.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING
                               | CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING|STORING|SIZESTORING|GETCALLBACK want FETCHDATA", scp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING))) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%x bufp 0x%x is BUF_CMFETCHING|BUF_CMSTORING want FETCHDATA", scp, bufp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_STOREDATA) {
            /* same as fetch data */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING
                               | CM_SCACHEFLAG_SIZESTORING | CM_SCACHEFLAG_GETCALLBACK)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING|STORING|SIZESTORING|GETCALLBACK want STOREDATA", scp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING))) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%x bufp 0x%x is BUF_CMFETCHING|BUF_CMSTORING want STOREDATA", scp, bufp);
                goto sleep;
            }
        }

        if (flags & CM_SCACHESYNC_STOREDATA_EXCL) {
            /* Don't allow concurrent StoreData RPC's */
            if (scp->flags & CM_SCACHEFLAG_DATASTORING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is DATASTORING want STOREDATA_EXCL", scp);
                goto sleep;
            }
        }

        if (flags & CM_SCACHESYNC_ASYNCSTORE) {
            /* Don't allow more than one BKG store request */
            if (scp->flags & CM_SCACHEFLAG_ASYNCSTORING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is ASYNCSTORING want ASYNCSTORE", scp);
                goto sleep;
            }
        }

        if (flags & CM_SCACHESYNC_LOCK) {
            /* Don't allow concurrent fiddling with lock lists */
            if (scp->flags & CM_SCACHEFLAG_LOCKING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is LOCKING want LOCK", scp);
                goto sleep;
            }
        }

        /* now the operations that don't correspond to making RPCs */
        if (flags & CM_SCACHESYNC_GETSTATUS) {
            /* we can use the status that's here, if we're not
             * bringing in new status.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING want GETSTATUS", scp);
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
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING | CM_SCACHEFLAG_SIZESTORING)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING|STORING|SIZESTORING want SETSTATUS", scp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_READ) {
            /* we're going to read the data, make sure that the
             * status is available, and that the data is here.  It
             * is OK to read while storing the data back.
             */
            if (scp->flags & CM_SCACHEFLAG_FETCHING) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING want READ", scp);
                goto sleep;
            }
            if (bufp && ((bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMFULLYFETCHED)) == CM_BUF_CMFETCHING)) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%x bufp 0x%x is BUF_CMFETCHING want READ", scp, bufp);
                goto sleep;
            }
        }
        if (flags & CM_SCACHESYNC_WRITE) {
            /* don't write unless the status is stable and the chunk
             * is stable.
             */
            if (scp->flags & (CM_SCACHEFLAG_FETCHING | CM_SCACHEFLAG_STORING
                               | CM_SCACHEFLAG_SIZESTORING)) {
                osi_Log1(afsd_logp, "CM SyncOp scp 0x%x is FETCHING|STORING|SIZESTORING want WRITE", scp);
                goto sleep;
            }
            if (bufp && (bufp->cmFlags & (CM_BUF_CMFETCHING | CM_BUF_CMSTORING))) {
                osi_Log2(afsd_logp, "CM SyncOp scp 0x%x bufp 0x%x is BUF_CMFETCHING|BUF_CMSTORING want WRITE", scp, bufp);
                goto sleep;
            }
        }

        // yj: modified this so that callback only checked if we're
        // not checking something on /afs
        /* fix the conditional to match the one in cm_HaveCallback */
        if (  (flags & CM_SCACHESYNC_NEEDCALLBACK)
#ifdef AFS_FREELANCE_CLIENT
             && (!cm_freelanceEnabled || 
                  !(scp->fid.vnode==0x1 && scp->fid.unique==0x1) ||
                  scp->fid.cell!=AFS_FAKE_ROOT_CELL_ID ||
                  scp->fid.volume!=AFS_FAKE_ROOT_VOL_ID ||
                  cm_fakeDirCallback < 2)
#endif /* AFS_FREELANCE_CLIENT */
             ) {
            if (!cm_HaveCallback(scp)) {
                osi_Log1(afsd_logp, "CM SyncOp getting callback on scp %x",
                          (long) scp);
                if (bufLocked) lock_ReleaseMutex(&bufp->mx);
                code = cm_GetCallback(scp, up, reqp, 0);
                if (bufLocked) {
                    lock_ReleaseMutex(&scp->mx);
                    lock_ObtainMutex(&bufp->mx);
                    lock_ObtainMutex(&scp->mx);
                }
                if (code) 
                    return code;
                continue;
            }
        }

        if (rights) {
            /* can't check access rights without a callback */
            osi_assert(flags & CM_SCACHESYNC_NEEDCALLBACK);

            if ((rights & PRSFS_WRITE) && (scp->flags & CM_SCACHEFLAG_RO))
                return CM_ERROR_READONLY;

            if (cm_HaveAccessRights(scp, up, rights, &outRights)) {
                if (~outRights & rights) return CM_ERROR_NOACCESS;
            }
            else {
                /* we don't know the required access rights */
                if (bufLocked) lock_ReleaseMutex(&bufp->mx);
                code = cm_GetAccessRights(scp, up, reqp);
                if (code) 
                    return code;
                if (bufLocked) {
                    lock_ReleaseMutex(&scp->mx);
                    lock_ObtainMutex(&bufp->mx);
                    lock_ObtainMutex(&scp->mx);
                }
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

        /* wait here, then try again */
        osi_Log1(afsd_logp, "CM SyncOp sleeping scp 0x%x", scp);
        if ( scp->flags & CM_SCACHEFLAG_WAITING ) {
            scp->waitCount++;
            scp->waitRequests++;
            osi_Log3(afsd_logp, "CM SyncOp CM_SCACHEFLAG_WAITING already set for 0x%x; %d threads; %d requests", 
                     scp, scp->waitCount, scp->waitRequests);
        } else {
            osi_Log1(afsd_logp, "CM SyncOp CM_SCACHEFLAG_WAITING set for 0x%x", scp);
            scp->flags |= CM_SCACHEFLAG_WAITING;
            scp->waitCount = scp->waitRequests = 1;
        }
        if (bufLocked) 
            lock_ReleaseMutex(&bufp->mx);
        osi_SleepM((long) &scp->flags, &scp->mx);
        if (bufLocked) 
            lock_ObtainMutex(&bufp->mx);
        lock_ObtainMutex(&scp->mx);
        scp->waitCount--;
        osi_Log3(afsd_logp, "CM SyncOp woke! scp 0x%x; still waiting %d threads of %d requests", 
                 scp, scp->waitCount, scp->waitRequests);
        if (scp->waitCount == 0) {
            osi_Log1(afsd_logp, "CM SyncOp CM_SCACHEFLAG_WAITING reset for 0x%x", scp);
            scp->flags &= ~CM_SCACHEFLAG_WAITING;
            scp->waitRequests = 0;
        }
    } /* big while loop */
        
    /* now, update the recorded state for RPC-type calls */
    if (flags & CM_SCACHESYNC_FETCHSTATUS)
        scp->flags |= CM_SCACHEFLAG_FETCHING;
    if (flags & CM_SCACHESYNC_STORESTATUS)
        scp->flags |= CM_SCACHEFLAG_STORING;
    if (flags & CM_SCACHESYNC_STORESIZE)
        scp->flags |= CM_SCACHEFLAG_SIZESTORING;
    if (flags & CM_SCACHESYNC_GETCALLBACK)
        scp->flags |= CM_SCACHEFLAG_GETCALLBACK;
    if (flags & CM_SCACHESYNC_STOREDATA_EXCL)
        scp->flags |= CM_SCACHEFLAG_DATASTORING;
    if (flags & CM_SCACHESYNC_ASYNCSTORE)
        scp->flags |= CM_SCACHEFLAG_ASYNCSTORING;
    if (flags & CM_SCACHESYNC_LOCK)
        scp->flags |= CM_SCACHEFLAG_LOCKING;

    /* now update the buffer pointer */
    if (flags & CM_SCACHESYNC_FETCHDATA) {
        /* ensure that the buffer isn't already in the I/O list */
        if (bufp) {
            for(qdp = scp->bufReadsp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
                tbufp = osi_GetQData(qdp);
                osi_assert(tbufp != bufp);
            }
        }

        /* queue a held reference to the buffer in the "reading" I/O list */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, bufp);
        if (bufp) {
            buf_Hold(bufp);
            bufp->cmFlags |= CM_BUF_CMFETCHING;
        }
        osi_QAdd((osi_queue_t **) &scp->bufReadsp, &qdp->q);
    }

    if (flags & CM_SCACHESYNC_STOREDATA) {
        /* ensure that the buffer isn't already in the I/O list */
        if (bufp) {
            for(qdp = scp->bufWritesp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
                tbufp = osi_GetQData(qdp);
                osi_assert(tbufp != bufp);
            }
        }

        /* queue a held reference to the buffer in the "writing" I/O list */
        qdp = osi_QDAlloc();
        osi_SetQData(qdp, bufp);
        if (bufp) {
            buf_Hold(bufp);
            bufp->cmFlags |= CM_BUF_CMSTORING;
        }
        osi_QAdd((osi_queue_t **) &scp->bufWritesp, &qdp->q);
    }

    return 0;
}

/* for those syncops that setup for RPCs.
 * Called with scache locked.
 */
void cm_SyncOpDone(cm_scache_t *scp, cm_buf_t *bufp, long flags)
{
    osi_queueData_t *qdp;
    cm_buf_t *tbufp;

    /* now, update the recorded state for RPC-type calls */
    if (flags & CM_SCACHESYNC_FETCHSTATUS)
        scp->flags &= ~CM_SCACHEFLAG_FETCHING;
    if (flags & CM_SCACHESYNC_STORESTATUS)
        scp->flags &= ~CM_SCACHEFLAG_STORING;
    if (flags & CM_SCACHESYNC_STORESIZE)
        scp->flags &= ~CM_SCACHEFLAG_SIZESTORING;
    if (flags & CM_SCACHESYNC_GETCALLBACK)
        scp->flags &= ~CM_SCACHEFLAG_GETCALLBACK;
    if (flags & CM_SCACHESYNC_STOREDATA_EXCL)
        scp->flags &= ~CM_SCACHEFLAG_DATASTORING;
    if (flags & CM_SCACHESYNC_ASYNCSTORE)
        scp->flags &= ~CM_SCACHEFLAG_ASYNCSTORING;
    if (flags & CM_SCACHESYNC_LOCK)
        scp->flags &= ~CM_SCACHEFLAG_LOCKING;

    /* now update the buffer pointer */
    if (flags & CM_SCACHESYNC_FETCHDATA) {
        /* ensure that the buffer isn't already in the I/O list */
        for(qdp = scp->bufReadsp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            if (tbufp == bufp) break;
        }
        osi_assert(qdp != NULL);
        osi_assert(osi_GetQData(qdp) == bufp);
        osi_QRemove((osi_queue_t **) &scp->bufReadsp, &qdp->q);
        osi_QDFree(qdp);
        if (bufp) {
            bufp->cmFlags &= ~(CM_BUF_CMFETCHING | CM_BUF_CMFULLYFETCHED);
            if (bufp->flags & CM_BUF_WAITING) {
                osi_Log2(afsd_logp, "CM SyncOpDone Waking [scp 0x%x] bufp 0x%x", scp, bufp);
                osi_Wakeup((long) &bufp);
            }
            buf_Release(bufp);
        }
    }

    /* now update the buffer pointer */
    if (flags & CM_SCACHESYNC_STOREDATA) {
        /* ensure that the buffer isn't already in the I/O list */
        for(qdp = scp->bufWritesp; qdp; qdp = (osi_queueData_t *) osi_QNext(&qdp->q)) {
            tbufp = osi_GetQData(qdp);
            if (tbufp == bufp) break;
        }
        osi_assert(qdp != NULL);
        osi_assert(osi_GetQData(qdp) == bufp);
        osi_QRemove((osi_queue_t **) &scp->bufWritesp, &qdp->q);
        osi_QDFree(qdp);
        if (bufp) {
            bufp->cmFlags &= ~CM_BUF_CMSTORING;
            if (bufp->flags & CM_BUF_WAITING) {
                osi_Log2(afsd_logp, "CM SyncOpDone Waking [scp 0x%x] bufp 0x%x", scp, bufp);
                osi_Wakeup((long) &bufp);
            }
            buf_Release(bufp);
        }
    }

    /* and wakeup anyone who is waiting */
    if (scp->flags & CM_SCACHEFLAG_WAITING) {
        osi_Log1(afsd_logp, "CM SyncOpDone Waking scp 0x%x", scp);
        osi_Wakeup((long) &scp->flags);
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
void cm_MergeStatus(cm_scache_t *scp, AFSFetchStatus *statusp, AFSVolSync *volp,
                    cm_user_t *userp, int flags)
{
    // yj: i want to create some fake status for the /afs directory and the
    // entries under that directory
#ifdef AFS_FREELANCE_CLIENT
    if (cm_freelanceEnabled && scp == cm_data.rootSCachep) {
        osi_Log0(afsd_logp,"cm_MergeStatus Freelance cm_data.rootSCachep");
        statusp->InterfaceVersion = 0x1;
        statusp->FileType = CM_SCACHETYPE_DIRECTORY;
        statusp->LinkCount = scp->linkCount;
        statusp->Length = cm_fakeDirSize;
        statusp->DataVersion = cm_data.fakeDirVersion;
        statusp->Author = 0x1;
        statusp->Owner = 0x0;
        statusp->CallerAccess = 0x9;
        statusp->AnonymousAccess = 0x9;
        statusp->UnixModeBits = 0x1ff;
        statusp->ParentVnode = 0x1;
        statusp->ParentUnique = 0x1;
        statusp->ResidencyMask = 0;
        statusp->ClientModTime = FakeFreelanceModTime;
        statusp->ServerModTime = FakeFreelanceModTime;
        statusp->Group = 0;
        statusp->SyncCounter = 0;
        statusp->dataVersionHigh = 0;
    }
#endif /* AFS_FREELANCE_CLIENT */

    if (!(flags & CM_MERGEFLAG_FORCE)
         && statusp->DataVersion < (unsigned long) scp->dataVersion) {
        struct cm_cell *cellp;
        struct cm_volume *volp;

        cellp = cm_FindCellByID(scp->fid.cell);
        cm_GetVolumeByID(cellp, scp->fid.volume, userp,
                          (cm_req_t *) NULL, &volp);
        if (scp->cbServerp)
            osi_Log2(afsd_logp, "old data from server %x volume %s",
                      scp->cbServerp->addr.sin_addr.s_addr,
                      volp->namep);
        osi_Log3(afsd_logp, "Bad merge, scp %x, scp dv %d, RPC dv %d",
                  scp, scp->dataVersion, statusp->DataVersion);
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
            return;
    }       
    scp->serverModTime = statusp->ServerModTime;

    if (!(scp->mask & CM_SCACHEMASK_CLIENTMODTIME)) {
        scp->clientModTime = statusp->ClientModTime;
    }
    if (!(scp->mask & CM_SCACHEMASK_LENGTH)) {
        scp->length.LowPart = statusp->Length;
        scp->length.HighPart = 0;
    }

    scp->serverLength.LowPart = statusp->Length;
    scp->serverLength.HighPart = 0;

    scp->linkCount = statusp->LinkCount;
    scp->dataVersion = statusp->DataVersion;
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
        osi_Log1(afsd_logp, "Merge, Invalid File Type, scp %x", scp);
        scp->fileType = 0;	/* invalid */
    }
    /* and other stuff */
    scp->parentVnode = statusp->ParentVnode;
    scp->parentUnique = statusp->ParentUnique;
        
    /* and merge in the private acl cache info, if this is more than the public
     * info; merge in the public stuff in any case.
     */
    scp->anyAccess = statusp->AnonymousAccess;

    if (userp != NULL) {
        cm_AddACLCache(scp, userp, statusp->CallerAccess);
    }
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
    lock_AssertMutex(&scp->mx);
    if (scp->cbServerp) {
        cm_PutServer(scp->cbServerp);
	scp->cbServerp = NULL;
    }
    scp->cbExpires = 0;
    cm_dnlcPurgedp(scp);
    cm_dnlcPurgevp(scp);
    cm_FreeAllACLEnts(scp);
}

void cm_AFSFidFromFid(AFSFid *afsFidp, cm_fid_t *fidp)
{
    afsFidp->Volume = fidp->volume;
    afsFidp->Vnode = fidp->vnode;
    afsFidp->Unique = fidp->unique;
}       

void cm_HoldSCacheNoLock(cm_scache_t *scp)
{
    osi_assert(scp != 0);
    osi_assert(scp->refCount >= 0);
    scp->refCount++;
}

void cm_HoldSCache(cm_scache_t *scp)
{
    osi_assert(scp != 0);
    lock_ObtainWrite(&cm_scacheLock);
    osi_assert(scp->refCount >= 0);
    scp->refCount++;
    lock_ReleaseWrite(&cm_scacheLock);
}

void cm_ReleaseSCacheNoLock(cm_scache_t *scp)
{
    osi_assert(scp != 0);
    osi_assert(scp->refCount-- >= 0);
}

void cm_ReleaseSCache(cm_scache_t *scp)
{
    osi_assert(scp != 0);
    lock_ObtainWrite(&cm_scacheLock);
    osi_assert(scp->refCount != 0);
    scp->refCount--;
    lock_ReleaseWrite(&cm_scacheLock);
}

/* just look for the scp entry to get filetype */
/* doesn't need to be perfectly accurate, so locking doesn't matter too much */
int cm_FindFileType(cm_fid_t *fidp)
{
    long hash;
    cm_scache_t *scp;
        
    hash = CM_SCACHE_HASH(fidp);
        
    osi_assert(fidp->cell != 0);

    lock_ObtainWrite(&cm_scacheLock);
    for (scp=cm_data.hashTablep[hash]; scp; scp=scp->nextp) {
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
    char output[1024];
    int i;
  
    if (lock)
        lock_ObtainRead(&cm_scacheLock);
  
    sprintf(output, "%s - dumping scache - cm_data.currentSCaches=%d, cm_data.maxSCaches=%d\n", cookie, cm_data.currentSCaches, cm_data.maxSCaches);
    WriteFile(outputFile, output, strlen(output), &zilch, NULL);
  
    for (scp = cm_data.scacheLRULastp; scp; scp = (cm_scache_t *) osi_QPrev(&scp->q)) 
    {
        if (scp->refCount != 0)
        {
            sprintf(output, "%s fid (cell=%d, volume=%d, vnode=%d, unique=%d) refCount=%u\n", 
                    cookie, scp->fid.cell, scp->fid.volume, scp->fid.vnode, scp->fid.unique, 
                    scp->refCount);
            WriteFile(outputFile, output, strlen(output), &zilch, NULL);
        }
    }
  
    sprintf(output, "%s - dumping cm_data.hashTable - cm_data.hashTableSize=%d\n", cookie, cm_data.hashTableSize);
    WriteFile(outputFile, output, strlen(output), &zilch, NULL);
  
    for (i = 0; i < cm_data.hashTableSize; i++)
    {
        for(scp = cm_data.hashTablep[i]; scp; scp=scp->nextp) 
        {
            if (scp->refCount != 0)
            {
                sprintf(output, "%s scp=0x%08X, hash=%d, fid (cell=%d, volume=%d, vnode=%d, unique=%d) refCount=%u\n", 
                         cookie, (void *)scp, i, scp->fid.cell, scp->fid.volume, scp->fid.vnode, 
                         scp->fid.unique, scp->refCount);
                WriteFile(outputFile, output, strlen(output), &zilch, NULL);
            }
        }
    }

    sprintf(output, "%s - Done dumping scache.\n", cookie);
    WriteFile(outputFile, output, strlen(output), &zilch, NULL);
  
    if (lock)
        lock_ReleaseRead(&cm_scacheLock);       
    return (0);     
}

