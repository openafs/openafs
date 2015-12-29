/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements:
 * afs_FlushVCache
 * afs_AllocCBR
 * afs_FreeCBR
 * afs_FlushVCBs
 * afs_QueueVCB
 * afs_RemoveVCB
 * afs_NewVCache
 * afs_FlushActiveVcaches
 * afs_VerifyVCache2
 * afs_WriteVCache
 * afs_WriteVCacheDiscon
 * afs_SimpleVStat
 * afs_ProcessFS
 * TellALittleWhiteLie
 * afs_RemoteLookup
 * afs_GetVCache
 * afs_LookupVCache
 * afs_GetRootVCache
 * afs_UpdateStatus
 * afs_FetchStatus
 * afs_StuffVcache
 * afs_PutVCache
 * afs_FindVCache
 * afs_NFSFindVCache
 * afs_vcacheInit
 * shutdown_vcache
 *
 */
#include <afsconfig.h>
#include "afs/param.h"

#include "afs/sysincludes.h"   /*Standard vendor system headers */
#include "afsincludes.h"       /*AFS-based standard headers */
#include "afs/afs_stats.h"
#include "afs/afs_cbqueue.h"
#include "afs/afs_osidnlc.h"

afs_int32 afs_maxvcount = 0;	/* max number of vcache entries */
afs_int32 afs_vcount = 0;	/* number of vcache in use now */

#ifdef AFS_SGI_ENV
int afsvnumbers = 0;
#endif

#ifdef AFS_SGI64_ENV
char *makesname();
#endif /* AFS_SGI64_ENV */

/* Exported variables */
afs_rwlock_t afs_xvcdirty;	/*Lock: discon vcache dirty list mgmt */
afs_rwlock_t afs_xvcache;	/*Lock: alloc new stat cache entries */
afs_rwlock_t afs_xvreclaim;	/*Lock: entries reclaimed, not on free list */
afs_lock_t afs_xvcb;		/*Lock: fids on which there are callbacks */
#if !defined(AFS_LINUX22_ENV)
static struct vcache *freeVCList;	/*Free list for stat cache entries */
struct vcache *ReclaimedVCList;	/*Reclaimed list for stat entries */
static struct vcache *Initial_freeVCList;	/*Initial list for above */
#endif
struct afs_q VLRU;		/*vcache LRU */
afs_int32 vcachegen = 0;
unsigned int afs_paniconwarn = 0;
struct vcache *afs_vhashT[VCSIZE];
struct afs_q afs_vhashTV[VCSIZE];
static struct afs_cbr *afs_cbrHashT[CBRSIZE];
afs_int32 afs_bulkStatsLost;
int afs_norefpanic = 0;


/* Disk backed vcache definitions
 * Both protected by xvcache */
static int afs_nextVcacheSlot = 0;
static struct afs_slotlist *afs_freeSlotList = NULL;

/* Forward declarations */
static afs_int32 afs_QueueVCB(struct vcache *avc, int *slept);


/*
 * The PFlush algorithm makes use of the fact that Fid.Unique is not used in
 * below hash algorithms.  Change it if need be so that flushing algorithm
 * doesn't move things from one hash chain to another.
 */
/* Don't hash on the cell; our callback-breaking code sometimes fails to compute
 * the cell correctly, and only scans one hash bucket. */
int VCHash(struct VenusFid *fid)
{
    return opr_jhash_int2(fid->Fid.Volume, fid->Fid.Vnode, 0) &
	opr_jhash_mask(VCSIZEBITS);
}
/* Hash only on volume to speed up volume callbacks. */
int VCHashV(struct VenusFid *fid)
{
    return opr_jhash_int(fid->Fid.Vnode, 0) & opr_jhash_mask(VCSIZEBITS);
}

/*!
 * Generate an index into the hash table for a given Fid.
 * \param fid
 * \return The hash value.
 */
static int
afs_HashCBRFid(struct AFSFid *fid)
{
    return (fid->Volume + fid->Vnode + fid->Unique) % CBRSIZE;
}

/*!
 * Insert a CBR entry into the hash table.
 * Must be called with afs_xvcb held.
 * \param cbr
 * \return
 */
static void
afs_InsertHashCBR(struct afs_cbr *cbr)
{
    int slot = afs_HashCBRFid(&cbr->fid);

    cbr->hash_next = afs_cbrHashT[slot];
    if (afs_cbrHashT[slot])
	afs_cbrHashT[slot]->hash_pprev = &cbr->hash_next;

    cbr->hash_pprev = &afs_cbrHashT[slot];
    afs_cbrHashT[slot] = cbr;
}

/*!
 *
 * Flush the given vcache entry.
 *
 * Environment:
 *	afs_xvcache lock must be held for writing upon entry to
 *	prevent people from changing the vrefCount field, and to
 *      protect the lruq and hnext fields.
 * LOCK: afs_FlushVCache afs_xvcache W
 * REFCNT: vcache ref count must be zero on entry except for osf1
 * RACE: lock is dropped and reobtained, permitting race in caller
 *
 * \param avc Pointer to vcache entry to flush.
 * \param slept Pointer to int to set 1 if we sleep/drop locks, 0 if we don't.
 *
 */
int
afs_FlushVCache(struct vcache *avc, int *slept)
{				/*afs_FlushVCache */

    afs_int32 i, code;
    struct vcache **uvc, *wvc;

    /* NOTE: We must have nothing drop afs_xvcache until we have removed all
     * possible references to this vcache. This means all hash tables, queues,
     * DNLC, etc. */

    *slept = 0;
    AFS_STATCNT(afs_FlushVCache);
    afs_Trace2(afs_iclSetp, CM_TRACE_FLUSHV, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->f.states);

    code = osi_VM_FlushVCache(avc);
    if (code)
	goto bad;

    if (avc->f.states & CVFlushed) {
	code = EBUSY;
	goto bad;
    }
#if !defined(AFS_LINUX22_ENV)
    if (avc->nextfree || !avc->vlruq.prev || !avc->vlruq.next) {	/* qv afs.h */
	refpanic("LRU vs. Free inconsistency");
    }
#endif
    avc->f.states |= CVFlushed;
    /* pull the entry out of the lruq and put it on the free list */
    QRemove(&avc->vlruq);

    /* keep track of # of files that we bulk stat'd, but never used
     * before they got recycled.
     */
    if (avc->f.states & CBulkStat)
	afs_bulkStatsLost++;
    vcachegen++;
    /* remove entry from the hash chain */
    i = VCHash(&avc->f.fid);
    uvc = &afs_vhashT[i];
    for (wvc = *uvc; wvc; uvc = &wvc->hnext, wvc = *uvc) {
	if (avc == wvc) {
	    *uvc = avc->hnext;
	    avc->hnext = NULL;
	    break;
	}
    }

    /* remove entry from the volume hash table */
    QRemove(&avc->vhashq);

#if defined(AFS_LINUX26_ENV)
    {
	struct pagewriter *pw, *store;
	struct list_head tofree;

	INIT_LIST_HEAD(&tofree);
	spin_lock(&avc->pagewriter_lock);
	list_for_each_entry_safe(pw, store, &avc->pagewriters, link) {
	    list_del(&pw->link);
	    /* afs_osi_Free may sleep so we need to defer it */
	    list_add_tail(&pw->link, &tofree);
	}
	spin_unlock(&avc->pagewriter_lock);
	list_for_each_entry_safe(pw, store, &tofree, link) {
	    list_del(&pw->link);
	    afs_osi_Free(pw, sizeof(struct pagewriter));
	}
    }
#endif

    if (avc->mvid.target_root)
	osi_FreeSmallSpace(avc->mvid.target_root);
    avc->mvid.target_root = NULL;
    if (avc->linkData) {
	afs_osi_Free(avc->linkData, strlen(avc->linkData) + 1);
	avc->linkData = NULL;
    }
#if defined(AFS_XBSD_ENV) || defined(AFS_DARWIN_ENV)
    /* OK, there are no internal vrefCounts, so there shouldn't
     * be any more refs here. */
    if (avc->v) {
#ifdef AFS_DARWIN80_ENV
	vnode_clearfsnode(AFSTOV(avc));
        vnode_removefsref(AFSTOV(avc));
#else
	avc->v->v_data = NULL;	/* remove from vnode */
#endif
	AFSTOV(avc) = NULL;             /* also drop the ptr to vnode */
    }
#endif
#ifdef AFS_SUN510_ENV
    /* As we use private vnodes, cleanup is up to us */
    vn_reinit(AFSTOV(avc));
#endif
    afs_FreeAllAxs(&(avc->Access));
    ObtainWriteLock(&afs_xcbhash, 460);
    afs_DequeueCallback(avc);	/* remove it from queued callbacks list */
    avc->f.states &= ~(CStatd | CUnique);
    ReleaseWriteLock(&afs_xcbhash);
    if ((avc->f.states & CForeign) || (avc->f.fid.Fid.Vnode & 1))
	osi_dnlc_purgedp(avc);	/* if it (could be) a directory */
    else
	osi_dnlc_purgevp(avc);

    /* By this point, the vcache has been removed from all global structures
     * via which someone could try to use the vcache. It is okay to drop
     * afs_xvcache at this point (if *slept is set). */

    if (afs_shuttingdown == AFS_RUNNING)
	afs_QueueVCB(avc, slept);

    /*
     * Next, keep track of which vnodes we've deleted for create's
     * optimistic synchronization algorithm
     */
    afs_allZaps++;
    if (avc->f.fid.Fid.Vnode & 1)
	afs_oddZaps++;
    else
	afs_evenZaps++;

    afs_vcount--;
#if !defined(AFS_LINUX22_ENV)
    /* put the entry in the free list */
    avc->nextfree = freeVCList;
    freeVCList = avc;
    if (avc->vlruq.prev || avc->vlruq.next) {
	refpanic("LRU vs. Free inconsistency");
    }
    avc->f.states |= CVFlushed;
#else
    /* This should put it back on the vnode free list since usecount is 1 */
    vSetType(avc, VREG);
    if (VREFCOUNT_GT(avc,0)) {
	AFS_RELE(AFSTOV(avc));
	afs_stats_cmperf.vcacheXAllocs--;
    } else {
	if (afs_norefpanic) {
	    afs_warn("flush vc refcnt < 1");
	    afs_norefpanic++;
	} else
	    osi_Panic("flush vc refcnt < 1");
    }
#endif /* AFS_LINUX22_ENV */
    return 0;

  bad:
    return code;
}				/*afs_FlushVCache */

#ifndef AFS_SGI_ENV
/*!
 *  The core of the inactive vnode op for all but IRIX.
 *
 * \param avc
 * \param acred
 */
void
afs_InactiveVCache(struct vcache *avc, afs_ucred_t *acred)
{
    AFS_STATCNT(afs_inactive);
    if (avc->f.states & CDirty) {
	/* we can't keep trying to push back dirty data forever.  Give up. */
	afs_InvalidateAllSegments(avc);	/* turns off dirty bit */
    }
    avc->f.states &= ~CMAPPED;	/* mainly used by SunOS 4.0.x */
    avc->f.states &= ~CDirty;	/* Turn it off */
    if (avc->f.states & CUnlinked) {
	if (CheckLock(&afs_xvcache) || CheckLock(&afs_xdcache)) {
	    avc->f.states |= CUnlinkedDel;
	    return;
	}
	afs_remunlink(avc, 1);	/* ignore any return code */
    }

}
#endif

/*!
 *   Allocate a callback return structure from the
 * free list and return it.
 *
 * Environment: The alloc and free routines are both called with the afs_xvcb lock
 * held, so we don't have to worry about blocking in osi_Alloc.
 *
 * \return The allocated afs_cbr.
 */
static struct afs_cbr *afs_cbrSpace = 0;
/* if alloc limit below changes, fix me! */
static struct afs_cbr *afs_cbrHeads[16];
struct afs_cbr *
afs_AllocCBR(void)
{
    struct afs_cbr *tsp;
    int i;

    while (!afs_cbrSpace) {
	if (afs_stats_cmperf.CallBackAlloced >= sizeof(afs_cbrHeads)/sizeof(afs_cbrHeads[0])) {
	    /* don't allocate more than 16 * AFS_NCBRS for now */
	    afs_FlushVCBs(0);
	    afs_stats_cmperf.CallBackFlushes++;
	} else {
	    /* try allocating */
	    tsp = afs_osi_Alloc(AFS_NCBRS * sizeof(struct afs_cbr));
	    osi_Assert(tsp != NULL);
	    for (i = 0; i < AFS_NCBRS - 1; i++) {
		tsp[i].next = &tsp[i + 1];
	    }
	    tsp[AFS_NCBRS - 1].next = 0;
	    afs_cbrSpace = tsp;
	    afs_cbrHeads[afs_stats_cmperf.CallBackAlloced] = tsp;
	    afs_stats_cmperf.CallBackAlloced++;
	}
    }
    tsp = afs_cbrSpace;
    afs_cbrSpace = tsp->next;
    return tsp;
}

/*!
 * Free a callback return structure, removing it from all lists.
 *
 * Environment: the xvcb lock is held over these calls.
 *
 * \param asp The address of the structure to free.
 *
 * \rerurn 0
 */
int
afs_FreeCBR(struct afs_cbr *asp)
{
    *(asp->pprev) = asp->next;
    if (asp->next)
	asp->next->pprev = asp->pprev;

    *(asp->hash_pprev) = asp->hash_next;
    if (asp->hash_next)
	asp->hash_next->hash_pprev = asp->hash_pprev;

    asp->next = afs_cbrSpace;
    afs_cbrSpace = asp;
    return 0;
}

static void
FlushAllVCBs(int nconns, struct rx_connection **rxconns,
	     struct afs_conn **conns)
{
    afs_int32 *results;
    afs_int32 i;

    results = afs_osi_Alloc(nconns * sizeof (afs_int32));
    osi_Assert(results != NULL);

    AFS_GUNLOCK();
    multi_Rx(rxconns,nconns)
    {
        multi_RXAFS_GiveUpAllCallBacks();
        results[multi_i] = multi_error;
    } multi_End;
    AFS_GLOCK();

    /*
     * Freeing the CBR will unlink it from the server's CBR list
     * do it here, not in the loop, because a dynamic CBR will call
     * into the memory management routines.
     */
    for ( i = 0 ; i < nconns ; i++ ) {
	if (results[i] == 0) {
	    /* Unchain all of them */
	    while (conns[i]->parent->srvr->server->cbrs)
		afs_FreeCBR(conns[i]->parent->srvr->server->cbrs);
	}
    }
    afs_osi_Free(results, nconns * sizeof(afs_int32));
}

/*!
 *   Flush all queued callbacks to all servers.
 *
 * Environment: holds xvcb lock over RPC to guard against race conditions
 *	when a new callback is granted for the same file later on.
 *
 * \return 0 for success.
 */
afs_int32
afs_FlushVCBs(afs_int32 lockit)
{
    struct AFSFid *tfids;
    struct AFSCallBack callBacks[1];
    struct AFSCBFids fidArray;
    struct AFSCBs cbArray;
    afs_int32 code;
    struct afs_cbr *tcbrp;
    int tcount;
    struct server *tsp;
    int i;
    struct vrequest *treq = NULL;
    struct afs_conn *tc;
    int safety1, safety2, safety3;
    XSTATS_DECLS;

    if (AFS_IS_DISCONNECTED)
	return ENETDOWN;

    if ((code = afs_CreateReq(&treq, afs_osi_credp)))
	return code;
    treq->flags |= O_NONBLOCK;
    tfids = afs_osi_Alloc(sizeof(struct AFSFid) * AFS_MAXCBRSCALL);
    osi_Assert(tfids != NULL);

    if (lockit)
	ObtainWriteLock(&afs_xvcb, 273);
    /*
     * Shutting down.
     * First, attempt a multi across everything, all addresses
     * for all servers we know of.
     */

    if (lockit == 2)
	afs_LoopServers(AFS_LS_ALL, NULL, 0, FlushAllVCBs, NULL);

    ObtainReadLock(&afs_xserver);
    for (i = 0; i < NSERVERS; i++) {
	for (safety1 = 0, tsp = afs_servers[i];
	     tsp && safety1 < afs_totalServers + 10;
	     tsp = tsp->next, safety1++) {
	    /* don't have any */
	    if (tsp->cbrs == (struct afs_cbr *)0)
		continue;

	    /* otherwise, grab a block of AFS_MAXCBRSCALL from the list
	     * and make an RPC, over and over again.
	     */
	    tcount = 0;		/* number found so far */
	    for (safety2 = 0; safety2 < afs_cacheStats; safety2++) {
		if (tcount >= AFS_MAXCBRSCALL || !tsp->cbrs) {
		    struct rx_connection *rxconn;
		    /* if buffer is full, or we've queued all we're going
		     * to from this server, we should flush out the
		     * callbacks.
		     */
		    fidArray.AFSCBFids_len = tcount;
		    fidArray.AFSCBFids_val = (struct AFSFid *)tfids;
		    cbArray.AFSCBs_len = 1;
		    cbArray.AFSCBs_val = callBacks;
		    memset(&callBacks[0], 0, sizeof(callBacks[0]));
		    callBacks[0].CallBackType = CB_EXCLUSIVE;
		    for (safety3 = 0; safety3 < AFS_MAXHOSTS * 2; safety3++) {
			tc = afs_ConnByHost(tsp, tsp->cell->fsport,
					    tsp->cell->cellNum, treq, 0,
					    SHARED_LOCK, 0, &rxconn);
			if (tc) {
			    XSTATS_START_TIME
				(AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS);
			    RX_AFS_GUNLOCK();
			    code =
				RXAFS_GiveUpCallBacks(rxconn, &fidArray,
						      &cbArray);
			    RX_AFS_GLOCK();
			    XSTATS_END_TIME;
			} else
			    code = -1;
			if (!afs_Analyze
			    (tc, rxconn, code, 0, treq,
			     AFS_STATS_FS_RPCIDX_GIVEUPCALLBACKS, SHARED_LOCK,
			     tsp->cell)) {
			    break;
			}
		    }
		    /* ignore return code, since callbacks may have
		     * been returned anyway, we shouldn't leave them
		     * around to be returned again.
		     *
		     * Next, see if we are done with this server, and if so,
		     * break to deal with the next one.
		     */
		    if (!tsp->cbrs)
			break;
		    tcount = 0;
		}
		/* if to flush full buffer */
		/* if we make it here, we have an entry at the head of cbrs,
		 * which we should copy to the file ID array and then free.
		 */
		tcbrp = tsp->cbrs;
		tfids[tcount++] = tcbrp->fid;

		/* Freeing the CBR will unlink it from the server's CBR list */
		afs_FreeCBR(tcbrp);
	    }			/* while loop for this one server */
	    if (safety2 > afs_cacheStats) {
		afs_warn("possible internal error afs_flushVCBs (%d)\n",
			 safety2);
	    }
	}			/* for loop for this hash chain */
    }				/* loop through all hash chains */
    if (safety1 > afs_totalServers + 2) {
	afs_warn
	    ("AFS internal error (afs_flushVCBs) (%d > %d), continuing...\n",
	     safety1, afs_totalServers + 2);
	if (afs_paniconwarn)
	    osi_Panic("afs_flushVCBS safety1");
    }

    ReleaseReadLock(&afs_xserver);
    if (lockit)
	ReleaseWriteLock(&afs_xvcb);
    afs_osi_Free(tfids, sizeof(struct AFSFid) * AFS_MAXCBRSCALL);
    afs_DestroyReq(treq);
    return 0;
}

/*!
 *  Queue a callback on the given fid.
 *
 * Environment:
 *	Locks the xvcb lock.
 *	Called when the xvcache lock is already held.
 * RACE: afs_xvcache may be dropped and reacquired
 *
 * \param avc vcache entry
 * \param slep Set to 1 if we dropped afs_xvcache
 * \return 1 if queued, 0 otherwise
 */

static afs_int32
afs_QueueVCB(struct vcache *avc, int *slept)
{
    int queued = 0;
    struct server *tsp;
    struct afs_cbr *tcbp;
    int reacquire = 0;

    AFS_STATCNT(afs_QueueVCB);

    ObtainWriteLock(&afs_xvcb, 274);

    /* we can't really give back callbacks on RO files, since the
     * server only tracks them on a per-volume basis, and we don't
     * know whether we still have some other files from the same
     * volume. */
    if (!((avc->f.states & CRO) == 0 && avc->callback)) {
        goto done;
    }

    /* The callback is really just a struct server ptr. */
    tsp = (struct server *)(avc->callback);

    if (!afs_cbrSpace) {
	/* If we don't have CBR space, AllocCBR may block or hit the net for
	 * clearing up CBRs. Hitting the net may involve a fileserver
	 * needing to contact us, so we must drop xvcache so we don't block
	 * those requests from going through. */
	reacquire = *slept = 1;
	ReleaseWriteLock(&afs_xvcache);
    }

    /* we now have a pointer to the server, so we just allocate
     * a queue entry and queue it.
     */
    tcbp = afs_AllocCBR();
    tcbp->fid = avc->f.fid.Fid;

    tcbp->next = tsp->cbrs;
    if (tsp->cbrs)
	tsp->cbrs->pprev = &tcbp->next;

    tsp->cbrs = tcbp;
    tcbp->pprev = &tsp->cbrs;

    afs_InsertHashCBR(tcbp);
    queued = 1;

 done:
    /* now release locks and return */
    ReleaseWriteLock(&afs_xvcb);

    if (reacquire) {
	/* make sure this is after dropping xvcb, for locking order */
	ObtainWriteLock(&afs_xvcache, 279);
    }
    return queued;
}


/*!
 *   Remove a queued callback for a given Fid.
 *
 * Environment:
 *	Locks xvcb and xserver locks.
 *	Typically called with xdcache, xvcache and/or individual vcache
 *	entries locked.
 *
 * \param afid The fid we want cleansed of queued callbacks.
 *
 */

void
afs_RemoveVCB(struct VenusFid *afid)
{
    int slot;
    struct afs_cbr *cbr, *ncbr;

    AFS_STATCNT(afs_RemoveVCB);
    ObtainWriteLock(&afs_xvcb, 275);

    slot = afs_HashCBRFid(&afid->Fid);
    ncbr = afs_cbrHashT[slot];

    while (ncbr) {
	cbr = ncbr;
	ncbr = cbr->hash_next;

	if (afid->Fid.Volume == cbr->fid.Volume &&
	    afid->Fid.Vnode == cbr->fid.Vnode &&
	    afid->Fid.Unique == cbr->fid.Unique) {
	    afs_FreeCBR(cbr);
	}
    }

    ReleaseWriteLock(&afs_xvcb);
}

void
afs_FlushReclaimedVcaches(void)
{
#if !defined(AFS_LINUX22_ENV)
    struct vcache *tvc;
    int code, fv_slept;
    struct vcache *tmpReclaimedVCList = NULL;

    ObtainWriteLock(&afs_xvreclaim, 76);
    while (ReclaimedVCList) {
	tvc = ReclaimedVCList;	/* take from free list */
	ReclaimedVCList = tvc->nextfree;
	tvc->nextfree = NULL;
	code = afs_FlushVCache(tvc, &fv_slept);
	if (code) {
	    /* Ok, so, if we got code != 0, uh, wtf do we do? */
	    /* Probably, build a temporary list and then put all back when we
	       get to the end of the list */
	    /* This is actually really crappy, but we need to not leak these.
	       We probably need a way to be smarter about this. */
	    tvc->nextfree = tmpReclaimedVCList;
	    tmpReclaimedVCList = tvc;
	    /* printf("Reclaim list flush %lx failed: %d\n", (unsigned long) tvc, code); */
	}
        if (tvc->f.states & (CVInit
#ifdef AFS_DARWIN80_ENV
			  | CDeadVnode
#endif
           )) {
	   tvc->f.states &= ~(CVInit
#ifdef AFS_DARWIN80_ENV
			    | CDeadVnode
#endif
	   );
	   afs_osi_Wakeup(&tvc->f.states);
	}
    }
    if (tmpReclaimedVCList)
	ReclaimedVCList = tmpReclaimedVCList;

    ReleaseWriteLock(&afs_xvreclaim);
#endif
}

void
afs_PostPopulateVCache(struct vcache *avc, struct VenusFid *afid, int seq)
{
    /*
     * The proper value for mvstat (for root fids) is setup by the caller.
     */
    avc->mvstat = AFS_MVSTAT_FILE;
    if (afid->Fid.Vnode == 1 && afid->Fid.Unique == 1)
	avc->mvstat = AFS_MVSTAT_ROOT;

    if (afs_globalVFS == 0)
	osi_Panic("afs globalvfs");

    osi_PostPopulateVCache(avc);

    avc->dchint = NULL;
    osi_dnlc_purgedp(avc);	/* this may be overkill */
    memset(&(avc->callsort), 0, sizeof(struct afs_q));
    avc->slocks = NULL;
    avc->f.states &=~ CVInit;
    if (seq) {
	avc->f.states |= CBulkFetching;
	avc->f.m.Length = seq;
    }
    afs_osi_Wakeup(&avc->f.states);
}

int
afs_ShakeLooseVCaches(afs_int32 anumber)
{
    afs_int32 i, loop;
    struct vcache *tvc;
    struct afs_q *tq, *uq;
    int fv_slept, defersleep = 0;
    int limit;
    afs_int32 target = anumber;

    loop = 0;

 retry:
    i = 0;
    limit = afs_vcount;
    for (tq = VLRU.prev; tq != &VLRU && anumber > 0; tq = uq) {
	tvc = QTOV(tq);
	uq = QPrev(tq);
	if (tvc->f.states & CVFlushed) {
	    refpanic("CVFlushed on VLRU");
	} else if (i++ > limit) {
	    afs_warn("afs_ShakeLooseVCaches: i %d limit %d afs_vcount %d afs_maxvcount %d\n",
	             (int)i, limit, (int)afs_vcount, (int)afs_maxvcount);
	    refpanic("Found too many AFS vnodes on VLRU (VLRU cycle?)");
	} else if (QNext(uq) != tq) {
	    refpanic("VLRU inconsistent");
	} else if (tvc->f.states & CVInit) {
	    continue;
	}

	fv_slept = 0;
	if (osi_TryEvictVCache(tvc, &fv_slept, defersleep))
	    anumber--;

	if (fv_slept) {
	    if (loop++ > 100)
		break;
	    goto retry;	/* start over - may have raced. */
	}
	if (uq == &VLRU) {
	    if (anumber && !defersleep) {
		defersleep = 1;
		goto retry;
	    }
	    break;
	}
    }
    if (!afsd_dynamic_vcaches && anumber == target) {
	afs_warn("afs_ShakeLooseVCaches: warning none freed, using %d of %d\n",
	       afs_vcount, afs_maxvcount);
    }

    return 0;
}

/* Alloc new vnode. */

static struct vcache *
afs_AllocVCache(void)
{
    struct vcache *tvc;

    tvc = osi_NewVnode();

    afs_vcount++;

    /* track the peak */
    if (afsd_dynamic_vcaches && afs_maxvcount < afs_vcount) {
	afs_maxvcount = afs_vcount;
	/*printf("peak vnodes: %d\n", afs_maxvcount);*/
    }

    afs_stats_cmperf.vcacheXAllocs++;	/* count in case we have a leak */

    /* If we create a new inode, we either give it a new slot number,
     * or if one's available, use a slot number from the slot free list
     */
    if (afs_freeSlotList != NULL) {
       struct afs_slotlist *tmp;

       tvc->diskSlot = afs_freeSlotList->slot;
       tmp = afs_freeSlotList;
       afs_freeSlotList = tmp->next;
       afs_osi_Free(tmp, sizeof(struct afs_slotlist));
    }  else {
       tvc->diskSlot = afs_nextVcacheSlot++;
    }

    return tvc;
}

/* Pre populate a newly allocated vcache. On platforms where the actual
 * vnode is attached to the vcache, this function is called before attachment,
 * therefore it cannot perform any actions on the vnode itself */

static void
afs_PrePopulateVCache(struct vcache *avc, struct VenusFid *afid,
		      struct server *serverp) {

    afs_uint32 slot;
    slot = avc->diskSlot;

    osi_PrePopulateVCache(avc);

    avc->diskSlot = slot;
    QZero(&avc->metadirty);

    AFS_RWLOCK_INIT(&avc->lock, "vcache lock");

    memset(&avc->mvid, 0, sizeof(avc->mvid));
    avc->linkData = NULL;
    avc->cbExpires = 0;
    avc->opens = 0;
    avc->execsOrWriters = 0;
    avc->flockCount = 0;
    avc->f.states = CVInit;
    avc->last_looker = 0;
    avc->f.fid = *afid;
    avc->asynchrony = -1;
    avc->vc_error = 0;

    hzero(avc->mapDV);
    avc->f.truncPos = AFS_NOTRUNC;   /* don't truncate until we need to */
    hzero(avc->f.m.DataVersion);     /* in case we copy it into flushDV */
    avc->Access = NULL;
    avc->callback = serverp;         /* to minimize chance that clear
				      * request is lost */

#if defined(AFS_CACHE_BYPASS)
    avc->cachingStates = 0;
    avc->cachingTransitions = 0;
#endif
}

void
afs_FlushAllVCaches(void)
{
    int i;
    struct vcache *tvc, *nvc;

    ObtainWriteLock(&afs_xvcache, 867);

 retry:
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = nvc) {
	    int slept;

	    nvc = tvc->hnext;
	    if (afs_FlushVCache(tvc, &slept)) {
		afs_warn("Failed to flush vcache 0x%lx\n", (unsigned long)(uintptrsz)tvc);
	    }
	    if (slept) {
		goto retry;
	    }
	}
    }

    ReleaseWriteLock(&afs_xvcache);
}

/*!
 *   This routine is responsible for allocating a new cache entry
 * from the free list.  It formats the cache entry and inserts it
 * into the appropriate hash tables.  It must be called with
 * afs_xvcache write-locked so as to prevent several processes from
 * trying to create a new cache entry simultaneously.
 *
 * LOCK: afs_NewVCache  afs_xvcache W
 *
 * \param afid The file id of the file whose cache entry is being created.
 *
 * \return The new vcache struct.
 */

static_inline struct vcache *
afs_NewVCache_int(struct VenusFid *afid, struct server *serverp, int seq)
{
    struct vcache *tvc;
    afs_int32 i, j;
    afs_int32 anumber = VCACHE_FREE;

    AFS_STATCNT(afs_NewVCache);

    afs_FlushReclaimedVcaches();

#if defined(AFS_LINUX22_ENV)
    if(!afsd_dynamic_vcaches && afs_vcount >= afs_maxvcount) {
	afs_ShakeLooseVCaches(anumber);
	if (afs_vcount >= afs_maxvcount) {
	    afs_warn("afs_NewVCache - none freed\n");
	    return NULL;
	}
    }
    tvc = afs_AllocVCache();
#else /* AFS_LINUX22_ENV */
    /* pull out a free cache entry */
    if (!freeVCList) {
	afs_ShakeLooseVCaches(anumber);
    }

    if (!freeVCList) {
	tvc = afs_AllocVCache();
    } else {
	tvc = freeVCList;	/* take from free list */
	freeVCList = tvc->nextfree;
	tvc->nextfree = NULL;
	afs_vcount++; /* balanced by FlushVCache */
    } /* end of if (!freeVCList) */

#endif /* AFS_LINUX22_ENV */

#if defined(AFS_XBSD_ENV) || defined(AFS_DARWIN_ENV)
    if (tvc->v)
	panic("afs_NewVCache(): free vcache with vnode attached");
#endif

    /* Populate the vcache with as much as we can. */
    afs_PrePopulateVCache(tvc, afid, serverp);

    /* Thread the vcache onto the VLRU */

    i = VCHash(afid);
    j = VCHashV(afid);

    tvc->hnext = afs_vhashT[i];
    afs_vhashT[i] = tvc;
    QAdd(&afs_vhashTV[j], &tvc->vhashq);

    if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
        refpanic("NewVCache VLRU inconsistent");
    }
    QAdd(&VLRU, &tvc->vlruq);   /* put in lruq */
    if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
        refpanic("NewVCache VLRU inconsistent2");
    }
    if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
        refpanic("NewVCache VLRU inconsistent3");
    }
    if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
        refpanic("NewVCache VLRU inconsistent4");
    }
    vcachegen++;

    /* it should now be safe to drop the xvcache lock - so attach an inode
     * to this vcache, where necessary */
    osi_AttachVnode(tvc, seq);

    /* Get a reference count to hold this vcache for the VLRUQ. Note that
     * we have to do this after attaching the vnode, because the reference
     * count may be held in the vnode itself */

#if defined(AFS_LINUX22_ENV)
    /* Hold it for the LRU (should make count 2) */
    AFS_FAST_HOLD(tvc);
#elif !(defined (AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV))
    VREFCOUNT_SET(tvc, 1);	/* us */
#endif

#if defined (AFS_FBSD_ENV)
    if (tvc->f.states & CVInit)
#endif
    afs_PostPopulateVCache(tvc, afid, seq);

    return tvc;
}				/*afs_NewVCache */


struct vcache *
afs_NewVCache(struct VenusFid *afid, struct server *serverp)
{
    return afs_NewVCache_int(afid, serverp, 0);
}

struct vcache *
afs_NewBulkVCache(struct VenusFid *afid, struct server *serverp, int seq)
{
    return afs_NewVCache_int(afid, serverp, seq);
}

/*!
 * ???
 *
 * LOCK: afs_FlushActiveVcaches afs_xvcache N
 *
 * \param doflocks : Do we handle flocks?
 */
void
afs_FlushActiveVcaches(afs_int32 doflocks)
{
    struct vcache *tvc;
    int i;
    struct afs_conn *tc;
    afs_int32 code;
    afs_ucred_t *cred = NULL;
    struct vrequest *treq = NULL;
    struct AFSVolSync tsync;
    int didCore;
    XSTATS_DECLS;
    AFS_STATCNT(afs_FlushActiveVcaches);

    code = afs_CreateReq(&treq, afs_osi_credp);
    if (code) {
	afs_warn("unable to alloc treq\n");
	return;
    }

    ObtainReadLock(&afs_xvcache);
    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
            if (tvc->f.states & CVInit) continue;
#ifdef AFS_DARWIN80_ENV
            if (tvc->f.states & CDeadVnode &&
                (tvc->f.states & (CCore|CUnlinkedDel) ||
                 tvc->flockCount)) panic("Dead vnode has core/unlinkedel/flock");
#endif
	    if (doflocks && tvc->flockCount != 0) {
		struct rx_connection *rxconn;
		/* if this entry has an flock, send a keep-alive call out */
		osi_vnhold(tvc, 0);
		ReleaseReadLock(&afs_xvcache);
		ObtainWriteLock(&tvc->lock, 51);
		do {
		    code = afs_InitReq(treq, afs_osi_credp);
		    if (code) {
			code = -1;
			break; /* shutting down: do not try to extend the lock */
		    }
		    treq->flags |= O_NONBLOCK;

		    tc = afs_Conn(&tvc->f.fid, treq, SHARED_LOCK, &rxconn);
		    if (tc) {
			XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_EXTENDLOCK);
			RX_AFS_GUNLOCK();
			code =
			    RXAFS_ExtendLock(rxconn,
					     (struct AFSFid *)&tvc->f.fid.Fid,
					     &tsync);
			RX_AFS_GLOCK();
			XSTATS_END_TIME;
		    } else
			code = -1;
		} while (afs_Analyze
			 (tc, rxconn, code, &tvc->f.fid, treq,
			  AFS_STATS_FS_RPCIDX_EXTENDLOCK, SHARED_LOCK, NULL));

		ReleaseWriteLock(&tvc->lock);
#ifdef AFS_DARWIN80_ENV
                AFS_FAST_RELE(tvc);
                ObtainReadLock(&afs_xvcache);
#else
                ObtainReadLock(&afs_xvcache);
                AFS_FAST_RELE(tvc);
#endif
	    }
	    didCore = 0;
	    if ((tvc->f.states & CCore) || (tvc->f.states & CUnlinkedDel)) {
		/*
		 * Don't let it evaporate in case someone else is in
		 * this code.  Also, drop the afs_xvcache lock while
		 * getting vcache locks.
		 */
		osi_vnhold(tvc, 0);
		ReleaseReadLock(&afs_xvcache);
#if defined(AFS_SGI_ENV)
		/*
		 * That's because if we come in via the CUnlinkedDel bit state path we'll be have 0 refcnt
		 */
		osi_Assert(VREFCOUNT_GT(tvc,0));
		AFS_RWLOCK((vnode_t *) tvc, VRWLOCK_WRITE);
#endif
		ObtainWriteLock(&tvc->lock, 52);
		if (tvc->f.states & CCore) {
		    tvc->f.states &= ~CCore;
		    /* XXXX Find better place-holder for cred XXXX */
		    cred = (afs_ucred_t *)tvc->linkData;
		    tvc->linkData = NULL;	/* XXX */
		    code = afs_InitReq(treq, cred);
		    afs_Trace2(afs_iclSetp, CM_TRACE_ACTCCORE,
			       ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32,
			       tvc->execsOrWriters);
		    if (!code) {  /* avoid store when shutting down */
		        code = afs_StoreOnLastReference(tvc, treq);
		    }
		    ReleaseWriteLock(&tvc->lock);
		    hzero(tvc->flushDV);
		    osi_FlushText(tvc);
		    didCore = 1;
		    if (code && code != VNOVNODE) {
			afs_StoreWarn(code, tvc->f.fid.Fid.Volume,
				      /* /dev/console */ 1);
		    }
		} else if (tvc->f.states & CUnlinkedDel) {
		    /*
		     * Ignore errors
		     */
		    ReleaseWriteLock(&tvc->lock);
#if defined(AFS_SGI_ENV)
		    AFS_RWUNLOCK((vnode_t *) tvc, VRWLOCK_WRITE);
#endif
		    afs_remunlink(tvc, 0);
#if defined(AFS_SGI_ENV)
		    AFS_RWLOCK((vnode_t *) tvc, VRWLOCK_WRITE);
#endif
		} else {
		    /* lost (or won, perhaps) the race condition */
		    ReleaseWriteLock(&tvc->lock);
		}
#if defined(AFS_SGI_ENV)
		AFS_RWUNLOCK((vnode_t *) tvc, VRWLOCK_WRITE);
#endif
#ifdef AFS_DARWIN80_ENV
		AFS_FAST_RELE(tvc);
		if (didCore) {
		    AFS_RELE(AFSTOV(tvc));
		    /* Matches write code setting CCore flag */
		    crfree(cred);
		}
		ObtainReadLock(&afs_xvcache);
#else
		ObtainReadLock(&afs_xvcache);
		AFS_FAST_RELE(tvc);
		if (didCore) {
		    AFS_RELE(AFSTOV(tvc));
		    /* Matches write code setting CCore flag */
		    crfree(cred);
		}
#endif
	    }
	}
    }
    ReleaseReadLock(&afs_xvcache);
    afs_DestroyReq(treq);
}



/*!
 *   Make sure a cache entry is up-to-date status-wise.
 *
 * NOTE: everywhere that calls this can potentially be sped up
 *       by checking CStatd first, and avoiding doing the InitReq
 *       if this is up-to-date.
 *
 *  Anymore, the only places that call this KNOW already that the
 *  vcache is not up-to-date, so we don't screw around.
 *
 * \param avc  : Ptr to vcache entry to verify.
 * \param areq : ???
 */

/*!
 *
 *   Make sure a cache entry is up-to-date status-wise.
 *
 *   NOTE: everywhere that calls this can potentially be sped up
 *       by checking CStatd first, and avoiding doing the InitReq
 *       if this is up-to-date.
 *
 *   Anymore, the only places that call this KNOW already that the
 * vcache is not up-to-date, so we don't screw around.
 *
 * \param avc Pointer to vcache entry to verify.
 * \param areq
 *
 * \return 0 for success or other error codes.
 */
int
afs_VerifyVCache2(struct vcache *avc, struct vrequest *areq)
{
    struct vcache *tvc;

    AFS_STATCNT(afs_VerifyVCache);

    /* otherwise we must fetch the status info */

    ObtainWriteLock(&avc->lock, 53);
    if (avc->f.states & CStatd) {
	ReleaseWriteLock(&avc->lock);
	return 0;
    }
    ObtainWriteLock(&afs_xcbhash, 461);
    avc->f.states &= ~(CStatd | CUnique);
    avc->callback = NULL;
    afs_DequeueCallback(avc);
    ReleaseWriteLock(&afs_xcbhash);
    ReleaseWriteLock(&avc->lock);

    /* since we've been called back, or the callback has expired,
     * it's possible that the contents of this directory, or this
     * file's name have changed, thus invalidating the dnlc contents.
     */
    if ((avc->f.states & CForeign) || (avc->f.fid.Fid.Vnode & 1))
	osi_dnlc_purgedp(avc);
    else
	osi_dnlc_purgevp(avc);

    /* fetch the status info */
    tvc = afs_GetVCache(&avc->f.fid, areq, NULL, avc);
    if (!tvc)
	return EIO;
    /* Put it back; caller has already incremented vrefCount */
    afs_PutVCache(tvc);
    return 0;

}				/*afs_VerifyVCache */


/*!
 * Simple copy of stat info into cache.
 *
 * Callers:as of 1992-04-29, only called by WriteVCache
 *
 * \param avc   Ptr to vcache entry involved.
 * \param astat Ptr to stat info to copy.
 *
 */
static void
afs_SimpleVStat(struct vcache *avc,
		struct AFSFetchStatus *astat, struct vrequest *areq)
{
    afs_size_t length;
    AFS_STATCNT(afs_SimpleVStat);

#ifdef AFS_64BIT_CLIENT
	FillInt64(length, astat->Length_hi, astat->Length);
#else /* AFS_64BIT_CLIENT */
	length = astat->Length;
#endif /* AFS_64BIT_CLIENT */

#if defined(AFS_SGI_ENV)
    if ((avc->execsOrWriters <= 0) && !afs_DirtyPages(avc)
	&& !AFS_VN_MAPPED((vnode_t *) avc)) {
	osi_Assert((valusema(&avc->vc_rwlock) <= 0)
		   && (OSI_GET_LOCKID() == avc->vc_rwlockid));
	if (length < avc->f.m.Length) {
	    vnode_t *vp = (vnode_t *) avc;

	    osi_Assert(WriteLocked(&avc->lock));
	    ReleaseWriteLock(&avc->lock);
	    AFS_GUNLOCK();
	    PTOSSVP(vp, (off_t) length, (off_t) MAXLONG);
	    AFS_GLOCK();
	    ObtainWriteLock(&avc->lock, 67);
	}
    }
#endif

    if (!afs_DirtyPages(avc)) {
	/* if actively writing the file, don't fetch over this value */
	afs_Trace3(afs_iclSetp, CM_TRACE_SIMPLEVSTAT, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length),
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(length));
	avc->f.m.Length = length;
	avc->f.m.Date = astat->ClientModTime;
    }
    avc->f.m.Owner = astat->Owner;
    avc->f.m.Group = astat->Group;
    avc->f.m.Mode = astat->UnixModeBits;
    if (vType(avc) == VREG) {
	avc->f.m.Mode |= S_IFREG;
    } else if (vType(avc) == VDIR) {
	avc->f.m.Mode |= S_IFDIR;
    } else if (vType(avc) == VLNK) {
	avc->f.m.Mode |= S_IFLNK;
	if ((avc->f.m.Mode & 0111) == 0)
	    avc->mvstat = AFS_MVSTAT_MTPT;
    }
    if (avc->f.states & CForeign) {
	struct axscache *ac;
	avc->f.anyAccess = astat->AnonymousAccess;
#ifdef badidea
	if ((astat->CallerAccess & ~astat->AnonymousAccess))
	    /*   USED TO SAY :
	     * Caller has at least one bit not covered by anonymous, and
	     * thus may have interesting rights.
	     *
	     * HOWEVER, this is a really bad idea, because any access query
	     * for bits which aren't covered by anonymous, on behalf of a user
	     * who doesn't have any special rights, will result in an answer of
	     * the form "I don't know, lets make a FetchStatus RPC and find out!"
	     * It's an especially bad idea under Ultrix, since (due to the lack of
	     * a proper access() call) it must perform several afs_access() calls
	     * in order to create magic mode bits that vary according to who makes
	     * the call.  In other words, _every_ stat() generates a test for
	     * writeability...
	     */
#endif /* badidea */
	    if (avc->Access && (ac = afs_FindAxs(avc->Access, areq->uid)))
		ac->axess = astat->CallerAccess;
	    else		/* not found, add a new one if possible */
		afs_AddAxs(avc->Access, areq->uid, astat->CallerAccess);
    }

}				/*afs_SimpleVStat */


/*!
 * Store the status info *only* back to the server for a
 * fid/vrequest.
 *
 * Environment: Must be called with a shared lock held on the vnode.
 *
 * \param avc Ptr to the vcache entry.
 * \param astatus Ptr to the status info to store.
 * \param areq Ptr to the associated vrequest.
 *
 * \return Operation status.
 */

int
afs_WriteVCache(struct vcache *avc,
		struct AFSStoreStatus *astatus,
		struct vrequest *areq)
{
    afs_int32 code;
    struct afs_conn *tc;
    struct AFSFetchStatus OutStatus;
    struct AFSVolSync tsync;
    struct rx_connection *rxconn;
    XSTATS_DECLS;
    AFS_STATCNT(afs_WriteVCache);
    afs_Trace2(afs_iclSetp, CM_TRACE_WVCACHE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length));
    do {
	tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STORESTATUS);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_StoreStatus(rxconn, (struct AFSFid *)&avc->f.fid.Fid,
				  astatus, &OutStatus, &tsync);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_STORESTATUS,
	      SHARED_LOCK, NULL));

    UpgradeSToWLock(&avc->lock, 20);
    if (code == 0) {
	/* success, do the changes locally */
	afs_SimpleVStat(avc, &OutStatus, areq);
	/*
	 * Update the date, too.  SimpleVStat didn't do this, since
	 * it thought we were doing this after fetching new status
	 * over a file being written.
	 */
	avc->f.m.Date = OutStatus.ClientModTime;
    } else {
	/* failure, set up to check with server next time */
	ObtainWriteLock(&afs_xcbhash, 462);
	afs_DequeueCallback(avc);
	avc->f.states &= ~(CStatd | CUnique);	/* turn off stat valid flag */
	ReleaseWriteLock(&afs_xcbhash);
	if ((avc->f.states & CForeign) || (avc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(avc);	/* if it (could be) a directory */
    }
    ConvertWToSLock(&avc->lock);
    return code;

}				/*afs_WriteVCache */

/*!
 * Store status info only locally, set the proper disconnection flags
 * and add to dirty list.
 *
 * \param avc The vcache to be written locally.
 * \param astatus Get attr fields from local store.
 * \param attrs This one is only of the vs_size.
 *
 * \note Must be called with a shared lock on the vnode
 */
int
afs_WriteVCacheDiscon(struct vcache *avc,
		      struct AFSStoreStatus *astatus,
		      struct vattr *attrs)
{
    afs_int32 code = 0;
    afs_int32 flags = 0;

    UpgradeSToWLock(&avc->lock, 700);

    if (!astatus->Mask) {

	return code;

    } else {

    	/* Set attributes. */
    	if (astatus->Mask & AFS_SETMODTIME) {
		avc->f.m.Date = astatus->ClientModTime;
		flags |= VDisconSetTime;
	}

	if (astatus->Mask & AFS_SETOWNER) {
	    /* printf("Not allowed yet. \n"); */
	    /*avc->f.m.Owner = astatus->Owner;*/
	}

	if (astatus->Mask & AFS_SETGROUP) {
	    /* printf("Not allowed yet. \n"); */
	    /*avc->f.m.Group =  astatus->Group;*/
	}

	if (astatus->Mask & AFS_SETMODE) {
		avc->f.m.Mode = astatus->UnixModeBits;

#if 0 	/* XXX: Leaving this out, so it doesn't mess up the file type flag.*/

		if (vType(avc) == VREG) {
			avc->f.m.Mode |= S_IFREG;
		} else if (vType(avc) == VDIR) {
			avc->f.m.Mode |= S_IFDIR;
		} else if (vType(avc) == VLNK) {
			avc->f.m.Mode |= S_IFLNK;
			if ((avc->f.m.Mode & 0111) == 0)
				avc->mvstat = AFS_MVSTAT_MTPT;
		}
#endif
		flags |= VDisconSetMode;
	 } 		/* if(astatus.Mask & AFS_SETMODE) */

     } 			/* if (!astatus->Mask) */

     if (attrs->va_size > 0) {
     	/* XXX: Do I need more checks? */
     	/* Truncation operation. */
     	flags |= VDisconTrunc;
     }

    if (flags)
	afs_DisconAddDirty(avc, flags, 1);

    /* XXX: How about the rest of the fields? */

    ConvertWToSLock(&avc->lock);

    return code;
}

/*!
 * Copy astat block into vcache info
 *
 * \note This code may get dataversion and length out of sync if the file has
 * been modified.  This is less than ideal.  I haven't thought about it sufficiently
 * to be certain that it is adequate.
 *
 * \note Environment: Must be called under a write lock
 *
 * \param avc  Ptr to vcache entry.
 * \param astat Ptr to stat block to copy in.
 * \param areq Ptr to associated request.
 */
void
afs_ProcessFS(struct vcache *avc,
	      struct AFSFetchStatus *astat, struct vrequest *areq)
{
    afs_size_t length;
    AFS_STATCNT(afs_ProcessFS);

#ifdef AFS_64BIT_CLIENT
    FillInt64(length, astat->Length_hi, astat->Length);
#else /* AFS_64BIT_CLIENT */
    length = astat->Length;
#endif /* AFS_64BIT_CLIENT */
    /* WARNING: afs_DoBulkStat uses the Length field to store a sequence
     * number for each bulk status request. Under no circumstances
     * should afs_DoBulkStat store a sequence number if the new
     * length will be ignored when afs_ProcessFS is called with
     * new stats. If you change the following conditional then you
     * also need to change the conditional in afs_DoBulkStat.  */
#ifdef AFS_SGI_ENV
    if ((avc->execsOrWriters <= 0) && !afs_DirtyPages(avc)
	&& !AFS_VN_MAPPED((vnode_t *) avc)) {
#else
    if ((avc->execsOrWriters <= 0) && !afs_DirtyPages(avc)) {
#endif
	/* if we're writing or mapping this file, don't fetch over these
	 *  values.
	 */
	afs_Trace3(afs_iclSetp, CM_TRACE_PROCESSFS, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length),
		   ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(length));
	avc->f.m.Length = length;
	avc->f.m.Date = astat->ClientModTime;
    }
    hset64(avc->f.m.DataVersion, astat->dataVersionHigh, astat->DataVersion);
    avc->f.m.Owner = astat->Owner;
    avc->f.m.Mode = astat->UnixModeBits;
    avc->f.m.Group = astat->Group;
    avc->f.m.LinkCount = astat->LinkCount;
    if (astat->FileType == File) {
	vSetType(avc, VREG);
	avc->f.m.Mode |= S_IFREG;
    } else if (astat->FileType == Directory) {
	vSetType(avc, VDIR);
	avc->f.m.Mode |= S_IFDIR;
    } else if (astat->FileType == SymbolicLink) {
	if (afs_fakestat_enable && (avc->f.m.Mode & 0111) == 0) {
	    vSetType(avc, VDIR);
	    avc->f.m.Mode |= S_IFDIR;
	} else {
	    vSetType(avc, VLNK);
	    avc->f.m.Mode |= S_IFLNK;
	}
	if ((avc->f.m.Mode & 0111) == 0) {
	    avc->mvstat = AFS_MVSTAT_MTPT;
	}
    }
    avc->f.anyAccess = astat->AnonymousAccess;
#ifdef badidea
    if ((astat->CallerAccess & ~astat->AnonymousAccess))
	/*   USED TO SAY :
	 * Caller has at least one bit not covered by anonymous, and
	 * thus may have interesting rights.
	 *
	 * HOWEVER, this is a really bad idea, because any access query
	 * for bits which aren't covered by anonymous, on behalf of a user
	 * who doesn't have any special rights, will result in an answer of
	 * the form "I don't know, lets make a FetchStatus RPC and find out!"
	 * It's an especially bad idea under Ultrix, since (due to the lack of
	 * a proper access() call) it must perform several afs_access() calls
	 * in order to create magic mode bits that vary according to who makes
	 * the call.  In other words, _every_ stat() generates a test for
	 * writeability...
	 */
#endif /* badidea */
    {
	struct axscache *ac;
	if (avc->Access && (ac = afs_FindAxs(avc->Access, areq->uid)))
	    ac->axess = astat->CallerAccess;
	else			/* not found, add a new one if possible */
	    afs_AddAxs(avc->Access, areq->uid, astat->CallerAccess);
    }
}				/*afs_ProcessFS */


/*!
 * Get fid from server.
 *
 * \param afid
 * \param areq Request to be passed on.
 * \param name Name of ?? to lookup.
 * \param OutStatus Fetch status.
 * \param CallBackp
 * \param serverp
 * \param tsyncp
 *
 * \return Success status of operation.
 */
int
afs_RemoteLookup(struct VenusFid *afid, struct vrequest *areq,
		 char *name, struct VenusFid *nfid,
		 struct AFSFetchStatus *OutStatusp,
		 struct AFSCallBack *CallBackp, struct server **serverp,
		 struct AFSVolSync *tsyncp)
{
    afs_int32 code;
    struct afs_conn *tc;
    struct rx_connection *rxconn;
    struct AFSFetchStatus OutDirStatus;
    XSTATS_DECLS;
    if (!name)
	name = "";		/* XXX */
    do {
	tc = afs_Conn(afid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
	    if (serverp)
		*serverp = tc->parent->srvr->server;
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_XLOOKUP);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_Lookup(rxconn, (struct AFSFid *)&afid->Fid, name,
			     (struct AFSFid *)&nfid->Fid, OutStatusp,
			     &OutDirStatus, CallBackp, tsyncp);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, afid, areq, AFS_STATS_FS_RPCIDX_XLOOKUP, SHARED_LOCK,
	      NULL));

    return code;
}


/*!
 * afs_GetVCache
 *
 * Given a file id and a vrequest structure, fetch the status
 * information associated with the file.
 *
 * \param afid File ID.
 * \param areq Ptr to associated vrequest structure, specifying the
 *  user whose authentication tokens will be used.
 * \param avc Caller may already have a vcache for this file, which is
 *  already held.
 *
 * \note Environment:
 *	The cache entry is returned with an increased vrefCount field.
 *	The entry must be discarded by calling afs_PutVCache when you
 *	are through using the pointer to the cache entry.
 *
 *	You should not hold any locks when calling this function, except
 *	locks on other vcache entries.  If you lock more than one vcache
 *	entry simultaneously, you should lock them in this order:
 *
 *	    1. Lock all files first, then directories.
 *	    2.  Within a particular type, lock entries in Fid.Vnode order.
 *
 *	This locking hierarchy is convenient because it allows locking
 *	of a parent dir cache entry, given a file (to check its access
 *	control list).  It also allows renames to be handled easily by
 *	locking directories in a constant order.
 *
 * \note NB.  NewVCache -> FlushVCache presently (4/10/95) drops the xvcache lock.
 *
 * \note Might have a vcache structure already, which must
 *  already be held by the caller
 */
struct vcache *
afs_GetVCache(struct VenusFid *afid, struct vrequest *areq,
	      afs_int32 * cached, struct vcache *avc)
{

    afs_int32 code, newvcache = 0;
    struct vcache *tvc;
    struct volume *tvp;
    afs_int32 retry;

    AFS_STATCNT(afs_GetVCache);

    if (cached)
	*cached = 0;		/* Init just in case */

#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
  loop:
#endif

    ObtainSharedLock(&afs_xvcache, 5);

    tvc = afs_FindVCache(afid, &retry, DO_STATS | DO_VLRU | IS_SLOCK);
    if (tvc && retry) {
#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
	ReleaseSharedLock(&afs_xvcache);
	spunlock_psema(tvc->v.v_lock, retry, &tvc->v.v_sync, PINOD);
	goto loop;
#endif
    }
    if (tvc) {
	if (cached)
	    *cached = 1;
	osi_Assert((tvc->f.states & CVInit) == 0);
	/* If we are in readdir, return the vnode even if not statd */
	if ((tvc->f.states & CStatd) || afs_InReadDir(tvc)) {
	    ReleaseSharedLock(&afs_xvcache);
	    return tvc;
	}
    } else {
	UpgradeSToWLock(&afs_xvcache, 21);

	/* no cache entry, better grab one */
	tvc = afs_NewVCache(afid, NULL);
	newvcache = 1;

	ConvertWToSLock(&afs_xvcache);
	if (tvc == NULL)
	{
		ReleaseSharedLock(&afs_xvcache);
		return NULL;
	}

	afs_stats_cmperf.vcacheMisses++;
    }

    ReleaseSharedLock(&afs_xvcache);

    ObtainWriteLock(&tvc->lock, 54);

    if (tvc->f.states & CStatd) {
	ReleaseWriteLock(&tvc->lock);
	return tvc;
    }
#ifdef AFS_DARWIN80_ENV
/* Darwin 8.0 only has bufs in nfs, so we shouldn't have to worry about them.
   What about ubc? */
#else
#if defined(AFS_DARWIN_ENV) || defined(AFS_FBSD_ENV)
    /*
     * XXX - I really don't like this.  Should try to understand better.
     * It seems that sometimes, when we get called, we already hold the
     * lock on the vnode (e.g., from afs_getattr via afs_VerifyVCache).
     * We can't drop the vnode lock, because that could result in a race.
     * Sometimes, though, we get here and don't hold the vnode lock.
     * I hate code paths that sometimes hold locks and sometimes don't.
     * In any event, the dodge we use here is to check whether the vnode
     * is locked, and if it isn't, then we gain and drop it around the call
     * to vinvalbuf; otherwise, we leave it alone.
     */
    {
	struct vnode *vp = AFSTOV(tvc);
	int iheldthelock;

#if defined(AFS_DARWIN_ENV)
	iheldthelock = VOP_ISLOCKED(vp);
	if (!iheldthelock)
	    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, current_proc());
	/* this is messy. we can call fsync which will try to reobtain this */
	if (VTOAFS(vp) == tvc)
	  ReleaseWriteLock(&tvc->lock);
	if (UBCINFOEXISTS(vp)) {
	  vinvalbuf(vp, V_SAVE, &afs_osi_cred, current_proc(), PINOD, 0);
	}
	if (VTOAFS(vp) == tvc)
	  ObtainWriteLock(&tvc->lock, 954);
	if (!iheldthelock)
	    VOP_UNLOCK(vp, LK_EXCLUSIVE, current_proc());
#elif defined(AFS_FBSD80_ENV)
	iheldthelock = VOP_ISLOCKED(vp);
	if (!iheldthelock) {
	    /* nosleep/sleep lock order reversal */
	    int glocked = ISAFS_GLOCK();
	    if (glocked)
		AFS_GUNLOCK();
	    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	    if (glocked)
		AFS_GLOCK();
	}
	vinvalbuf(vp, V_SAVE, PINOD, 0); /* changed late in 8.0-CURRENT */
	if (!iheldthelock)
	    VOP_UNLOCK(vp, 0);
#elif defined(AFS_FBSD60_ENV)
	iheldthelock = VOP_ISLOCKED(vp, curthread);
	if (!iheldthelock)
	    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
	AFS_GUNLOCK();
	vinvalbuf(vp, V_SAVE, curthread, PINOD, 0);
	AFS_GLOCK();
	if (!iheldthelock)
	    VOP_UNLOCK(vp, LK_EXCLUSIVE, curthread);
#elif defined(AFS_FBSD_ENV)
	iheldthelock = VOP_ISLOCKED(vp, curthread);
	if (!iheldthelock)
	    vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
	vinvalbuf(vp, V_SAVE, osi_curcred(), curthread, PINOD, 0);
	if (!iheldthelock)
	    VOP_UNLOCK(vp, LK_EXCLUSIVE, curthread);
#elif defined(AFS_OBSD_ENV)
	iheldthelock = VOP_ISLOCKED(vp, curproc);
	if (!iheldthelock)
	    VOP_LOCK(vp, LK_EXCLUSIVE | LK_RETRY, curproc);
	uvm_vnp_uncache(vp);
	if (!iheldthelock)
	    VOP_UNLOCK(vp, 0, curproc);
#elif defined(AFS_NBSD40_ENV)
	iheldthelock = VOP_ISLOCKED(vp);
	if (!iheldthelock) {
	    VOP_LOCK(vp, LK_EXCLUSIVE | LK_RETRY);
	}
	uvm_vnp_uncache(vp);
	if (!iheldthelock)
	    VOP_UNLOCK(vp, 0);
#endif
    }
#endif
#endif

    ObtainWriteLock(&afs_xcbhash, 464);
    tvc->f.states &= ~CUnique;
    tvc->callback = 0;
    afs_DequeueCallback(tvc);
    ReleaseWriteLock(&afs_xcbhash);

    /* It is always appropriate to throw away all the access rights? */
    afs_FreeAllAxs(&(tvc->Access));
    tvp = afs_GetVolume(afid, areq, READ_LOCK);	/* copy useful per-volume info */
    if (tvp) {
	if ((tvp->states & VForeign)) {
	    if (newvcache)
		tvc->f.states |= CForeign;
	    if (newvcache && (tvp->rootVnode == afid->Fid.Vnode)
		&& (tvp->rootUnique == afid->Fid.Unique)) {
		tvc->mvstat = AFS_MVSTAT_ROOT;
	    }
	}
	if (tvp->states & VRO)
	    tvc->f.states |= CRO;
	if (tvp->states & VBackup)
	    tvc->f.states |= CBackup;
	/* now copy ".." entry back out of volume structure, if necessary */
	if (tvc->mvstat == AFS_MVSTAT_ROOT && tvp->dotdot.Fid.Volume != 0) {
	    if (!tvc->mvid.parent)
		tvc->mvid.parent = (struct VenusFid *)
		    osi_AllocSmallSpace(sizeof(struct VenusFid));
	    *tvc->mvid.parent = tvp->dotdot;
	}
	afs_PutVolume(tvp, READ_LOCK);
    }

    /* stat the file */
    afs_RemoveVCB(afid);
    {
	struct AFSFetchStatus OutStatus;

	if (afs_DynrootNewVnode(tvc, &OutStatus)) {
	    afs_ProcessFS(tvc, &OutStatus, areq);
	    tvc->f.states |= CStatd | CUnique;
	    tvc->f.parent.vnode  = OutStatus.ParentVnode;
	    tvc->f.parent.unique = OutStatus.ParentUnique;
	    code = 0;
	} else {

	    if (AFS_IS_DISCONNECTED) {
		/* Nothing to do otherwise...*/
		code = ENETDOWN;
		/* printf("Network is down in afs_GetCache"); */
	    } else
	        code = afs_FetchStatus(tvc, afid, areq, &OutStatus);

	    /* For the NFS translator's benefit, make sure
	     * non-directory vnodes always have their parent FID set
	     * correctly, even when created as a result of decoding an
	     * NFS filehandle.  It would be nice to also do this for
	     * directories, but we can't because the fileserver fills
	     * in the FID of the directory itself instead of that of
	     * its parent.
	     */
            if (!code && OutStatus.FileType != Directory &&
		!tvc->f.parent.vnode) {
		tvc->f.parent.vnode  = OutStatus.ParentVnode;
		tvc->f.parent.unique = OutStatus.ParentUnique;
		/* XXX - SXW - It's conceivable we should mark ourselves
		 *             as dirty again here, incase we've been raced
		 *             out of the FetchStatus call.
		 */
            }
	}
    }

    if (code) {
	ReleaseWriteLock(&tvc->lock);

	afs_PutVCache(tvc);
	return NULL;
    }

    ReleaseWriteLock(&tvc->lock);
    return tvc;

}				/*afs_GetVCache */



/*!
 * Lookup a vcache by fid. Look inside the cache first, if not
 * there, lookup the file on the server, and then get it's fresh
 * cache entry.
 *
 * \param afid
 * \param areq
 * \param cached Is element cached? If NULL, don't answer.
 * \param adp
 * \param aname
 *
 * \return The found element or NULL.
 */
struct vcache *
afs_LookupVCache(struct VenusFid *afid, struct vrequest *areq,
		 afs_int32 * cached, struct vcache *adp, char *aname)
{
    afs_int32 code, now, newvcache = 0;
    struct VenusFid nfid;
    struct vcache *tvc;
    struct volume *tvp;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct server *serverp = 0;
    afs_int32 origCBs;
    afs_int32 retry;

    AFS_STATCNT(afs_GetVCache);
    if (cached)
	*cached = 0;		/* Init just in case */

#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
  loop1:
#endif

    ObtainReadLock(&afs_xvcache);
    tvc = afs_FindVCache(afid, &retry, DO_STATS /* no vlru */ );

    if (tvc) {
	ReleaseReadLock(&afs_xvcache);
	if (retry) {
#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
	    spunlock_psema(tvc->v.v_lock, retry, &tvc->v.v_sync, PINOD);
	    goto loop1;
#endif
	}
	ObtainReadLock(&tvc->lock);

	if (tvc->f.states & CStatd) {
	    if (cached) {
		*cached = 1;
	    }
	    ReleaseReadLock(&tvc->lock);
	    return tvc;
	}
	tvc->f.states &= ~CUnique;

	ReleaseReadLock(&tvc->lock);
	afs_PutVCache(tvc);
	ObtainReadLock(&afs_xvcache);
    }
    /* if (tvc) */
    ReleaseReadLock(&afs_xvcache);

    /* lookup the file */
    nfid = *afid;
    now = osi_Time();
    origCBs = afs_allCBs;	/* if anything changes, we don't have a cb */

    if (AFS_IS_DISCONNECTED) {
	/* printf("Network is down in afs_LookupVcache\n"); */
        code = ENETDOWN;
    } else
        code =
	    afs_RemoteLookup(&adp->f.fid, areq, aname, &nfid, &OutStatus,
	                     &CallBack, &serverp, &tsync);

#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
  loop2:
#endif

    ObtainSharedLock(&afs_xvcache, 6);
    tvc = afs_FindVCache(&nfid, &retry, DO_VLRU | IS_SLOCK/* no xstats now */ );
    if (tvc && retry) {
#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
	ReleaseSharedLock(&afs_xvcache);
	spunlock_psema(tvc->v.v_lock, retry, &tvc->v.v_sync, PINOD);
	goto loop2;
#endif
    }

    if (!tvc) {
	/* no cache entry, better grab one */
	UpgradeSToWLock(&afs_xvcache, 22);
	tvc = afs_NewVCache(&nfid, serverp);
	newvcache = 1;
	ConvertWToSLock(&afs_xvcache);
	if (!tvc)
	{
		ReleaseSharedLock(&afs_xvcache);
		return NULL;
	}
    }

    ReleaseSharedLock(&afs_xvcache);
    ObtainWriteLock(&tvc->lock, 55);

    /* It is always appropriate to throw away all the access rights? */
    afs_FreeAllAxs(&(tvc->Access));
    tvp = afs_GetVolume(afid, areq, READ_LOCK);	/* copy useful per-vol info */
    if (tvp) {
	if ((tvp->states & VForeign)) {
	    if (newvcache)
		tvc->f.states |= CForeign;
	    if (newvcache && (tvp->rootVnode == afid->Fid.Vnode)
		&& (tvp->rootUnique == afid->Fid.Unique))
		tvc->mvstat = AFS_MVSTAT_ROOT;
	}
	if (tvp->states & VRO)
	    tvc->f.states |= CRO;
	if (tvp->states & VBackup)
	    tvc->f.states |= CBackup;
	/* now copy ".." entry back out of volume structure, if necessary */
	if (tvc->mvstat == AFS_MVSTAT_ROOT && tvp->dotdot.Fid.Volume != 0) {
	    if (!tvc->mvid.parent)
		tvc->mvid.parent = (struct VenusFid *)
		    osi_AllocSmallSpace(sizeof(struct VenusFid));
	    *tvc->mvid.parent = tvp->dotdot;
	}
    }

    if (code) {
	ObtainWriteLock(&afs_xcbhash, 465);
	afs_DequeueCallback(tvc);
	tvc->f.states &= ~(CStatd | CUnique);
	ReleaseWriteLock(&afs_xcbhash);
	if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */
	if (tvp)
	    afs_PutVolume(tvp, READ_LOCK);
	ReleaseWriteLock(&tvc->lock);
	afs_PutVCache(tvc);
	return NULL;
    }

    ObtainWriteLock(&afs_xcbhash, 466);
    if (origCBs == afs_allCBs) {
	if (CallBack.ExpirationTime) {
	    tvc->callback = serverp;
	    tvc->cbExpires = CallBack.ExpirationTime + now;
	    tvc->f.states |= CStatd | CUnique;
	    tvc->f.states &= ~CBulkFetching;
	    afs_QueueCallback(tvc, CBHash(CallBack.ExpirationTime), tvp);
	} else if (tvc->f.states & CRO) {
	    /* adapt gives us an hour. */
	    tvc->cbExpires = 3600 + osi_Time();
	     /*XXX*/ tvc->f.states |= CStatd | CUnique;
	    tvc->f.states &= ~CBulkFetching;
	    afs_QueueCallback(tvc, CBHash(3600), tvp);
	} else {
	    tvc->callback = NULL;
	    afs_DequeueCallback(tvc);
	    tvc->f.states &= ~(CStatd | CUnique);
	    if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
		osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */
	}
    } else {
	afs_DequeueCallback(tvc);
	tvc->f.states &= ~CStatd;
	tvc->f.states &= ~CUnique;
	tvc->callback = NULL;
	if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */
    }
    ReleaseWriteLock(&afs_xcbhash);
    if (tvp)
	afs_PutVolume(tvp, READ_LOCK);
    afs_ProcessFS(tvc, &OutStatus, areq);

    ReleaseWriteLock(&tvc->lock);
    return tvc;

}

struct vcache *
afs_GetRootVCache(struct VenusFid *afid, struct vrequest *areq,
		  afs_int32 * cached, struct volume *tvolp)
{
    afs_int32 code = 0, i, newvcache = 0, haveStatus = 0;
    afs_int32 getNewFid = 0;
    afs_uint32 start;
    struct VenusFid nfid;
    struct vcache *tvc;
    struct server *serverp = 0;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    int origCBs = 0;
#ifdef AFS_DARWIN80_ENV
    vnode_t tvp;
#endif

    start = osi_Time();

  newmtpt:
    if (!tvolp->rootVnode || getNewFid) {
	struct VenusFid tfid;

	tfid = *afid;
	tfid.Fid.Vnode = 0;	/* Means get rootfid of volume */
	origCBs = afs_allCBs;	/* ignore InitCallBackState */
	code =
	    afs_RemoteLookup(&tfid, areq, NULL, &nfid, &OutStatus, &CallBack,
			     &serverp, &tsync);
	if (code) {
	    return NULL;
	}
/*	ReleaseReadLock(&tvolp->lock);		 */
	ObtainWriteLock(&tvolp->lock, 56);
	tvolp->rootVnode = afid->Fid.Vnode = nfid.Fid.Vnode;
	tvolp->rootUnique = afid->Fid.Unique = nfid.Fid.Unique;
	ReleaseWriteLock(&tvolp->lock);
/*	ObtainReadLock(&tvolp->lock);*/
	haveStatus = 1;
    } else {
	afid->Fid.Vnode = tvolp->rootVnode;
	afid->Fid.Unique = tvolp->rootUnique;
    }

 rootvc_loop:
    ObtainSharedLock(&afs_xvcache, 7);
    i = VCHash(afid);
    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	if (!FidCmp(&(tvc->f.fid), afid)) {
            if (tvc->f.states & CVInit) {
		ReleaseSharedLock(&afs_xvcache);
		afs_osi_Sleep(&tvc->f.states);
		goto rootvc_loop;
            }
#ifdef AFS_DARWIN80_ENV
	    if (tvc->f.states & CDeadVnode) {
		ReleaseSharedLock(&afs_xvcache);
		afs_osi_Sleep(&tvc->f.states);
		goto rootvc_loop;
	    }
	    tvp = AFSTOV(tvc);
	    if (vnode_get(tvp))       /* this bumps ref count */
	        continue;
	    if (vnode_ref(tvp)) {
		AFS_GUNLOCK();
		/* AFSTOV(tvc) may be NULL */
		vnode_put(tvp);
		AFS_GLOCK();
	        continue;
	    }
#endif
	    break;
	}
    }

    if (!haveStatus && (!tvc || !(tvc->f.states & CStatd))) {
	/* Mount point no longer stat'd or unknown. FID may have changed. */
	getNewFid = 1;
	ReleaseSharedLock(&afs_xvcache);
#ifdef AFS_DARWIN80_ENV
        if (tvc) {
            AFS_GUNLOCK();
            vnode_put(AFSTOV(tvc));
            vnode_rele(AFSTOV(tvc));
            AFS_GLOCK();
        }
#endif
        tvc = NULL;
	goto newmtpt;
    }

    if (!tvc) {
	UpgradeSToWLock(&afs_xvcache, 23);
	/* no cache entry, better grab one */
	tvc = afs_NewVCache(afid, NULL);
	if (!tvc)
	{
		ReleaseWriteLock(&afs_xvcache);
		return NULL;
	}
	newvcache = 1;
	afs_stats_cmperf.vcacheMisses++;
    } else {
	if (cached)
	    *cached = 1;
	afs_stats_cmperf.vcacheHits++;
#if	defined(AFS_DARWIN80_ENV)
	/* we already bumped the ref count in the for loop above */
#else /* AFS_DARWIN80_ENV */
	osi_vnhold(tvc, 0);
#endif
	UpgradeSToWLock(&afs_xvcache, 24);
	if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
	    refpanic("GRVC VLRU inconsistent0");
	}
	if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
	    refpanic("GRVC VLRU inconsistent1");
	}
	if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
	    refpanic("GRVC VLRU inconsistent2");
	}
	QRemove(&tvc->vlruq);	/* move to lruq head */
	QAdd(&VLRU, &tvc->vlruq);
	if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
	    refpanic("GRVC VLRU inconsistent3");
	}
	if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
	    refpanic("GRVC VLRU inconsistent4");
	}
	if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
	    refpanic("GRVC VLRU inconsistent5");
	}
	vcachegen++;
    }

    ReleaseWriteLock(&afs_xvcache);

    if (tvc->f.states & CStatd) {
	return tvc;
    } else {

	ObtainReadLock(&tvc->lock);
	tvc->f.states &= ~CUnique;
	tvc->callback = NULL;	/* redundant, perhaps */
	ReleaseReadLock(&tvc->lock);
    }

    ObtainWriteLock(&tvc->lock, 57);

    /* It is always appropriate to throw away all the access rights? */
    afs_FreeAllAxs(&(tvc->Access));

    if (newvcache)
	tvc->f.states |= CForeign;
    if (tvolp->states & VRO)
	tvc->f.states |= CRO;
    if (tvolp->states & VBackup)
	tvc->f.states |= CBackup;
    /* now copy ".." entry back out of volume structure, if necessary */
    if (newvcache && (tvolp->rootVnode == afid->Fid.Vnode)
	&& (tvolp->rootUnique == afid->Fid.Unique)) {
	tvc->mvstat = AFS_MVSTAT_ROOT;
    }
    if (tvc->mvstat == AFS_MVSTAT_ROOT && tvolp->dotdot.Fid.Volume != 0) {
	if (!tvc->mvid.parent)
	    tvc->mvid.parent = (struct VenusFid *)
		osi_AllocSmallSpace(sizeof(struct VenusFid));
	*tvc->mvid.parent = tvolp->dotdot;
    }

    /* stat the file */
    afs_RemoveVCB(afid);

    if (!haveStatus) {
	struct VenusFid tfid;

	tfid = *afid;
	tfid.Fid.Vnode = 0;	/* Means get rootfid of volume */
	origCBs = afs_allCBs;	/* ignore InitCallBackState */
	code =
	    afs_RemoteLookup(&tfid, areq, NULL, &nfid, &OutStatus, &CallBack,
			     &serverp, &tsync);
    }

    if (code) {
	ObtainWriteLock(&afs_xcbhash, 467);
	afs_DequeueCallback(tvc);
	tvc->callback = NULL;
	tvc->f.states &= ~(CStatd | CUnique);
	ReleaseWriteLock(&afs_xcbhash);
	if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */
	ReleaseWriteLock(&tvc->lock);
	afs_PutVCache(tvc);
	return NULL;
    }

    ObtainWriteLock(&afs_xcbhash, 468);
    if (origCBs == afs_allCBs) {
	tvc->f.states |= CTruth;
	tvc->callback = serverp;
	if (CallBack.ExpirationTime != 0) {
	    tvc->cbExpires = CallBack.ExpirationTime + start;
	    tvc->f.states |= CStatd;
	    tvc->f.states &= ~CBulkFetching;
	    afs_QueueCallback(tvc, CBHash(CallBack.ExpirationTime), tvolp);
	} else if (tvc->f.states & CRO) {
	    /* adapt gives us an hour. */
	    tvc->cbExpires = 3600 + osi_Time();
	     /*XXX*/ tvc->f.states |= CStatd;
	    tvc->f.states &= ~CBulkFetching;
	    afs_QueueCallback(tvc, CBHash(3600), tvolp);
	}
    } else {
	afs_DequeueCallback(tvc);
	tvc->callback = NULL;
	tvc->f.states &= ~(CStatd | CUnique);
	if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */
    }
    ReleaseWriteLock(&afs_xcbhash);
    afs_ProcessFS(tvc, &OutStatus, areq);

    ReleaseWriteLock(&tvc->lock);
    return tvc;
}


/*!
 * Update callback status and (sometimes) attributes of a vnode.
 * Called after doing a fetch status RPC. Whilst disconnected, attributes
 * shouldn't be written to the vcache here.
 *
 * \param avc
 * \param afid
 * \param areq
 * \param Outsp Server status after rpc call.
 * \param acb Callback for this vnode.
 *
 * \note The vcache must be write locked.
 */
void
afs_UpdateStatus(struct vcache *avc, struct VenusFid *afid,
		 struct vrequest *areq, struct AFSFetchStatus *Outsp,
		 struct AFSCallBack *acb, afs_uint32 start)
{
    struct volume *volp;

    if (!AFS_IN_SYNC)
	/* Dont write status in vcache if resyncing after a disconnection. */
	afs_ProcessFS(avc, Outsp, areq);

    volp = afs_GetVolume(afid, areq, READ_LOCK);
    ObtainWriteLock(&afs_xcbhash, 469);
    avc->f.states |= CTruth;
    if (avc->callback /* check for race */ ) {
	if (acb->ExpirationTime != 0) {
	    avc->cbExpires = acb->ExpirationTime + start;
	    avc->f.states |= CStatd;
	    avc->f.states &= ~CBulkFetching;
	    afs_QueueCallback(avc, CBHash(acb->ExpirationTime), volp);
    	} else if (avc->f.states & CRO) {
	    /* ordinary callback on a read-only volume -- AFS 3.2 style */
	    avc->cbExpires = 3600 + start;
	    avc->f.states |= CStatd;
	    avc->f.states &= ~CBulkFetching;
	    afs_QueueCallback(avc, CBHash(3600), volp);
    	} else {
	    afs_DequeueCallback(avc);
	    avc->callback = NULL;
	    avc->f.states &= ~(CStatd | CUnique);
	    if ((avc->f.states & CForeign) || (avc->f.fid.Fid.Vnode & 1))
	    	osi_dnlc_purgedp(avc);	/* if it (could be) a directory */
    	}
    } else {
    	afs_DequeueCallback(avc);
    	avc->callback = NULL;
    	avc->f.states &= ~(CStatd | CUnique);
    	if ((avc->f.states & CForeign) || (avc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(avc);	/* if it (could be) a directory */
    }
    ReleaseWriteLock(&afs_xcbhash);
    if (volp)
    	afs_PutVolume(volp, READ_LOCK);
}

void
afs_BadFetchStatus(struct afs_conn *tc)
{
    int addr = ntohl(tc->parent->srvr->sa_ip);
    afs_warn("afs: Invalid AFSFetchStatus from server %u.%u.%u.%u\n",
             (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
             (addr) & 0xff);
    afs_warn("afs: This suggests the server may be sending bad data that "
             "can lead to availability issues or data corruption. The "
             "issue has been avoided for now, but it may not always be "
             "detectable. Please upgrade the server if possible.\n");
}

/**
 * Check if a given AFSFetchStatus structure is sane.
 *
 * @param[in] tc The server from which we received the status
 * @param[in] status The status we received
 *
 * @return whether the given structure is valid or not
 *  @retval 0 the structure is fine
 *  @retval nonzero the structure looks like garbage; act as if we received
 *                  the returned error code from the server
 */
int
afs_CheckFetchStatus(struct afs_conn *tc, struct AFSFetchStatus *status)
{
    if (status->errorCode ||
        status->InterfaceVersion != 1 ||
        !(status->FileType > Invalid && status->FileType <= SymbolicLink) ||
        status->ParentVnode == 0 || status->ParentUnique == 0) {

	afs_warn("afs: FetchStatus ec %u iv %u ft %u pv %u pu %u\n",
	         (unsigned)status->errorCode, (unsigned)status->InterfaceVersion,
	         (unsigned)status->FileType, (unsigned)status->ParentVnode,
	         (unsigned)status->ParentUnique);
	afs_BadFetchStatus(tc);

	return VBUSY;
    }
    return 0;
}

/*!
 * Must be called with avc write-locked
 * don't absolutely have to invalidate the hint unless the dv has
 * changed, but be sure to get it right else there will be consistency bugs.
 */
afs_int32
afs_FetchStatus(struct vcache * avc, struct VenusFid * afid,
		struct vrequest * areq, struct AFSFetchStatus * Outsp)
{
    int code;
    afs_uint32 start = 0;
    struct afs_conn *tc;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct rx_connection *rxconn;
    XSTATS_DECLS;
    do {
	tc = afs_Conn(afid, areq, SHARED_LOCK, &rxconn);
	avc->dchint = NULL;	/* invalidate hints */
	if (tc) {
	    avc->callback = tc->parent->srvr->server;
	    start = osi_Time();
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHSTATUS);
	    RX_AFS_GUNLOCK();
	    code =
		RXAFS_FetchStatus(rxconn, (struct AFSFid *)&afid->Fid, Outsp,
				  &CallBack, &tsync);
	    RX_AFS_GLOCK();

	    XSTATS_END_TIME;

	    if (code == 0) {
		code = afs_CheckFetchStatus(tc, Outsp);
	    }

	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, afid, areq, AFS_STATS_FS_RPCIDX_FETCHSTATUS,
	      SHARED_LOCK, NULL));

    if (!code) {
	afs_UpdateStatus(avc, afid, areq, Outsp, &CallBack, start);
    } else {
	/* used to undo the local callback, but that's too extreme.
	 * There are plenty of good reasons that fetchstatus might return
	 * an error, such as EPERM.  If we have the vnode cached, statd,
	 * with callback, might as well keep track of the fact that we
	 * don't have access...
	 */
	if (code == EPERM || code == EACCES) {
	    struct axscache *ac;
	    if (avc->Access && (ac = afs_FindAxs(avc->Access, areq->uid)))
		ac->axess = 0;
	    else		/* not found, add a new one if possible */
		afs_AddAxs(avc->Access, areq->uid, 0);
	}
    }
    return code;
}

#if 0
/*
 * afs_StuffVcache
 *
 * Description:
 *	Stuff some information into the vcache for the given file.
 *
 * Parameters:
 *	afid	  : File in question.
 *	OutStatus : Fetch status on the file.
 *	CallBack  : Callback info.
 *	tc	  : RPC connection involved.
 *	areq	  : vrequest involved.
 *
 * Environment:
 *	Nothing interesting.
 */
void
afs_StuffVcache(struct VenusFid *afid,
		struct AFSFetchStatus *OutStatus,
		struct AFSCallBack *CallBack, struct afs_conn *tc,
		struct vrequest *areq)
{
    afs_int32 code, i, newvcache = 0;
    struct vcache *tvc;
    struct AFSVolSync tsync;
    struct volume *tvp;
    struct axscache *ac;
    afs_int32 retry;

    AFS_STATCNT(afs_StuffVcache);
#ifdef IFS_VCACHECOUNT
    ifs_gvcachecall++;
#endif

  loop:
    ObtainSharedLock(&afs_xvcache, 8);

    tvc = afs_FindVCache(afid, &retry, DO_VLRU| IS_SLOCK /* no stats */ );
    if (tvc && retry) {
#if	defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
	ReleaseSharedLock(&afs_xvcache);
	spunlock_psema(tvc->v.v_lock, retry, &tvc->v.v_sync, PINOD);
	goto loop;
#endif
    }

    if (!tvc) {
	/* no cache entry, better grab one */
	UpgradeSToWLock(&afs_xvcache, 25);
	tvc = afs_NewVCache(afid, NULL);
	newvcache = 1;
	ConvertWToSLock(&afs_xvcache);
	if (!tvc)
	{
		ReleaseSharedLock(&afs_xvcache);
		return NULL;
	}
    }

    ReleaseSharedLock(&afs_xvcache);
    ObtainWriteLock(&tvc->lock, 58);

    tvc->f.states &= ~CStatd;
    if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
	osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */

    /* Is it always appropriate to throw away all the access rights? */
    afs_FreeAllAxs(&(tvc->Access));

    /*Copy useful per-volume info */
    tvp = afs_GetVolume(afid, areq, READ_LOCK);
    if (tvp) {
	if (newvcache && (tvp->states & VForeign))
	    tvc->f.states |= CForeign;
	if (tvp->states & VRO)
	    tvc->f.states |= CRO;
	if (tvp->states & VBackup)
	    tvc->f.states |= CBackup;
	/*
	 * Now, copy ".." entry back out of volume structure, if
	 * necessary
	 */
	if (tvc->mvstat == AFS_MVSTAT_ROOT && tvp->dotdot.Fid.Volume != 0) {
	    if (!tvc->mvid.parent)
		tvc->mvid.parent = (struct VenusFid *)
		    osi_AllocSmallSpace(sizeof(struct VenusFid));
	    *tvc->mvid.parent = tvp->dotdot;
	}
    }
    /* store the stat on the file */
    afs_RemoveVCB(afid);
    afs_ProcessFS(tvc, OutStatus, areq);
    tvc->callback = tc->srvr->server;

    /* we use osi_Time twice below.  Ideally, we would use the time at which
     * the FetchStatus call began, instead, but we don't have it here.  So we
     * make do with "now".  In the CRO case, it doesn't really matter. In
     * the other case, we hope that the difference between "now" and when the
     * call actually began execution on the server won't be larger than the
     * padding which the server keeps.  Subtract 1 second anyway, to be on
     * the safe side.  Can't subtract more because we don't know how big
     * ExpirationTime is.  Possible consistency problems may arise if the call
     * timeout period becomes longer than the server's expiration padding.  */
    ObtainWriteLock(&afs_xcbhash, 470);
    if (CallBack->ExpirationTime != 0) {
	tvc->cbExpires = CallBack->ExpirationTime + osi_Time() - 1;
	tvc->f.states |= CStatd;
	tvc->f.states &= ~CBulkFetching;
	afs_QueueCallback(tvc, CBHash(CallBack->ExpirationTime), tvp);
    } else if (tvc->f.states & CRO) {
	/* old-fashioned AFS 3.2 style */
	tvc->cbExpires = 3600 + osi_Time();
	 /*XXX*/ tvc->f.states |= CStatd;
	tvc->f.states &= ~CBulkFetching;
	afs_QueueCallback(tvc, CBHash(3600), tvp);
    } else {
	afs_DequeueCallback(tvc);
	tvc->callback = NULL;
	tvc->f.states &= ~(CStatd | CUnique);
	if ((tvc->f.states & CForeign) || (tvc->f.fid.Fid.Vnode & 1))
	    osi_dnlc_purgedp(tvc);	/* if it (could be) a directory */
    }
    ReleaseWriteLock(&afs_xcbhash);
    if (tvp)
	afs_PutVolume(tvp, READ_LOCK);

    /* look in per-pag cache */
    if (tvc->Access && (ac = afs_FindAxs(tvc->Access, areq->uid)))
	ac->axess = OutStatus->CallerAccess;	/* substitute pags */
    else			/* not found, add a new one if possible */
	afs_AddAxs(tvc->Access, areq->uid, OutStatus->CallerAccess);

    ReleaseWriteLock(&tvc->lock);
    afs_Trace4(afs_iclSetp, CM_TRACE_STUFFVCACHE, ICL_TYPE_POINTER, tvc,
	       ICL_TYPE_POINTER, tvc->callback, ICL_TYPE_INT32,
	       tvc->cbExpires, ICL_TYPE_INT32, tvc->cbExpires - osi_Time());
    /*
     * Release ref count... hope this guy stays around...
     */
    afs_PutVCache(tvc);
}				/*afs_StuffVcache */
#endif

/*!
 * Decrements the reference count on a cache entry.
 *
 * \param avc Pointer to the cache entry to decrement.
 *
 * \note Environment: Nothing interesting.
 */
void
afs_PutVCache(struct vcache *avc)
{
    AFS_STATCNT(afs_PutVCache);
#ifdef AFS_DARWIN80_ENV
    vnode_put(AFSTOV(avc));
    AFS_FAST_RELE(avc);
#else
    /*
     * Can we use a read lock here?
     */
    ObtainReadLock(&afs_xvcache);
    AFS_FAST_RELE(avc);
    ReleaseReadLock(&afs_xvcache);
#endif
}				/*afs_PutVCache */


/*!
 * Reset a vcache entry, so local contents are ignored, and the
 * server will be reconsulted next time the vcache is used
 *
 * \param avc Pointer to the cache entry to reset
 * \param acred
 * \param skipdnlc  skip the dnlc purge for this vnode
 *
 * \note avc must be write locked on entry
 *
 * \note The caller should purge the dnlc when skipdnlc is set.
 */
void
afs_ResetVCache(struct vcache *avc, afs_ucred_t *acred, afs_int32 skipdnlc)
{
    ObtainWriteLock(&afs_xcbhash, 456);
    afs_DequeueCallback(avc);
    avc->f.states &= ~(CStatd | CDirty);    /* next reference will re-stat */
    ReleaseWriteLock(&afs_xcbhash);
    /* now find the disk cache entries */
    afs_TryToSmush(avc, acred, 1);
    if (!skipdnlc) {
	osi_dnlc_purgedp(avc);
    }
    if (avc->linkData && !(avc->f.states & CCore)) {
	afs_osi_Free(avc->linkData, strlen(avc->linkData) + 1);
	avc->linkData = NULL;
    }
}

/*!
 * Sleepa when searching for a vcache. Releases all the pending locks,
 * sleeps then obtains the previously released locks.
 *
 * \param vcache Enter sleep state.
 * \param flag Determines what locks to use.
 *
 * \return
 */
static void
findvc_sleep(struct vcache *avc, int flag)
{
    if (flag & IS_SLOCK) {
	    ReleaseSharedLock(&afs_xvcache);
    } else {
	if (flag & IS_WLOCK) {
	    ReleaseWriteLock(&afs_xvcache);
	} else {
	    ReleaseReadLock(&afs_xvcache);
	}
    }
    afs_osi_Sleep(&avc->f.states);
    if (flag & IS_SLOCK) {
	    ObtainSharedLock(&afs_xvcache, 341);
    } else {
	if (flag & IS_WLOCK) {
	    ObtainWriteLock(&afs_xvcache, 343);
	} else {
	    ObtainReadLock(&afs_xvcache);
	}
    }
}

/*!
 * Add a reference on an existing vcache entry.
 *
 * \param tvc Pointer to the vcache.
 *
 * \note Environment: Must be called with at least one reference from
 * elsewhere on the vcache, even if that reference will be dropped.
 * The global lock is required.
 *
 * \return 0 on success, -1 on failure.
 */

int
afs_RefVCache(struct vcache *tvc)
{
#ifdef AFS_DARWIN80_ENV
    vnode_t tvp;
#endif

    /* AFS_STATCNT(afs_RefVCache); */

#ifdef  AFS_DARWIN80_ENV
    tvp = AFSTOV(tvc);
    if (vnode_get(tvp))
	return -1;
    if (vnode_ref(tvp)) {
	AFS_GUNLOCK();
	/* AFSTOV(tvc) may be NULL */
	vnode_put(tvp);
	AFS_GLOCK();
	return -1;
    }
#else
	osi_vnhold(tvc, 0);
#endif
    return 0;
}				/*afs_RefVCache */

/*!
 * Find a vcache entry given a fid.
 *
 * \param afid Pointer to the fid whose cache entry we desire.
 * \param retry (SGI-specific) tell the caller to drop the lock on xvcache,
 *  unlock the vnode, and try again.
 * \param flag Bit 1 to specify whether to compute hit statistics.  Not
 *  set if FindVCache is called as part of internal bookkeeping.
 *
 * \note Environment: Must be called with the afs_xvcache lock at least held at
 * the read level.  In order to do the VLRU adjustment, the xvcache lock
 * must be shared-- we upgrade it here.
 */

struct vcache *
afs_FindVCache(struct VenusFid *afid, afs_int32 * retry, afs_int32 flag)
{

    struct vcache *tvc;
    afs_int32 i;
#ifdef AFS_DARWIN80_ENV
    struct vcache *deadvc = NULL, *livevc = NULL;
    vnode_t tvp;
#endif

    AFS_STATCNT(afs_FindVCache);

 findloop:
    i = VCHash(afid);
    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	if (FidMatches(afid, tvc)) {
            if (tvc->f.states & CVInit) {
		findvc_sleep(tvc, flag);
		goto findloop;
	    }
#ifdef  AFS_DARWIN80_ENV
            if (tvc->f.states & CDeadVnode) {
		findvc_sleep(tvc, flag);
		goto findloop;
            }
#endif
	    break;
	}
    }

    /* should I have a read lock on the vnode here? */
    if (tvc) {
	if (retry)
	    *retry = 0;
#if defined(AFS_DARWIN80_ENV)
	tvp = AFSTOV(tvc);
	if (vnode_get(tvp))
	    tvp = NULL;
	if (tvp && vnode_ref(tvp)) {
	    AFS_GUNLOCK();
	    /* AFSTOV(tvc) may be NULL */
	    vnode_put(tvp);
	    AFS_GLOCK();
	    tvp = NULL;
	}
	if (!tvp) {
	    tvc = NULL;
	    return tvc;
	}
#elif defined(AFS_DARWIN_ENV)
	tvc->f.states |= CUBCinit;
	AFS_GUNLOCK();
	if (UBCINFOMISSING(AFSTOV(tvc)) ||
	    UBCINFORECLAIMED(AFSTOV(tvc))) {
	  ubc_info_init(AFSTOV(tvc));
	}
	AFS_GLOCK();
	tvc->f.states &= ~CUBCinit;
#else
	osi_vnhold(tvc, retry);	/* already held, above */
	if (retry && *retry)
	    return 0;
#endif
	/*
	 * only move to front of vlru if we have proper vcache locking)
	 */
	if (flag & DO_VLRU) {
	    if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
		refpanic("FindVC VLRU inconsistent1");
	    }
	    if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
		refpanic("FindVC VLRU inconsistent1");
	    }
	    if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
		refpanic("FindVC VLRU inconsistent2");
	    }
	    UpgradeSToWLock(&afs_xvcache, 26);
	    QRemove(&tvc->vlruq);
	    QAdd(&VLRU, &tvc->vlruq);
	    ConvertWToSLock(&afs_xvcache);
	    if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
		refpanic("FindVC VLRU inconsistent1");
	    }
	    if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
		refpanic("FindVC VLRU inconsistent2");
	    }
	    if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
		refpanic("FindVC VLRU inconsistent3");
	    }
	}
	vcachegen++;
    }

    if (flag & DO_STATS) {
	if (tvc)
	    afs_stats_cmperf.vcacheHits++;
	else
	    afs_stats_cmperf.vcacheMisses++;
	if (afs_IsPrimaryCellNum(afid->Cell))
	    afs_stats_cmperf.vlocalAccesses++;
	else
	    afs_stats_cmperf.vremoteAccesses++;
    }
    return tvc;
}				/*afs_FindVCache */

/*!
 * Find a vcache entry given a fid. Does a wildcard match on what we
 * have for the fid. If more than one entry, don't return anything.
 *
 * \param avcp Fill in pointer if we found one and only one.
 * \param afid Pointer to the fid whose cache entry we desire.
 * \param retry (SGI-specific) tell the caller to drop the lock on xvcache,
 *             unlock the vnode, and try again.
 * \param flags bit 1 to specify whether to compute hit statistics.  Not
 *             set if FindVCache is called as part of internal bookkeeping.
 *
 * \note Environment: Must be called with the afs_xvcache lock at least held at
 *  the read level.  In order to do the VLRU adjustment, the xvcache lock
 *  must be shared-- we upgrade it here.
 *
 * \return Number of matches found.
 */

int afs_duplicate_nfs_fids = 0;

afs_int32
afs_NFSFindVCache(struct vcache **avcp, struct VenusFid *afid)
{
    struct vcache *tvc;
    afs_int32 i;
    afs_int32 count = 0;
    struct vcache *found_tvc = NULL;
#ifdef AFS_DARWIN80_ENV
    vnode_t tvp;
#endif

    AFS_STATCNT(afs_FindVCache);

  loop:

    ObtainSharedLock(&afs_xvcache, 331);

    i = VCHash(afid);
    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	/* Match only on what we have.... */
	if (((tvc->f.fid.Fid.Vnode & 0xffff) == afid->Fid.Vnode)
	    && (tvc->f.fid.Fid.Volume == afid->Fid.Volume)
	    && ((tvc->f.fid.Fid.Unique & 0xffffff) == afid->Fid.Unique)
	    && (tvc->f.fid.Cell == afid->Cell)) {
	    if (tvc->f.states & CVInit) {
		ReleaseSharedLock(&afs_xvcache);
		afs_osi_Sleep(&tvc->f.states);
		goto loop;
            }
#ifdef  AFS_DARWIN80_ENV
	    if (tvc->f.states & CDeadVnode) {
		ReleaseSharedLock(&afs_xvcache);
		afs_osi_Sleep(&tvc->f.states);
		goto loop;
	    }
	    tvp = AFSTOV(tvc);
	    if (vnode_get(tvp)) {
		/* This vnode no longer exists. */
		continue;
	    }
	    if (vnode_ref(tvp)) {
		/* This vnode no longer exists. */
		AFS_GUNLOCK();
		/* AFSTOV(tvc) may be NULL */
		vnode_put(tvp);
		AFS_GLOCK();
		continue;
	    }
#endif /* AFS_DARWIN80_ENV */
	    count++;
	    if (found_tvc) {
		/* Duplicates */
		afs_duplicate_nfs_fids++;
		ReleaseSharedLock(&afs_xvcache);
#ifdef AFS_DARWIN80_ENV
                /* Drop our reference counts. */
                vnode_put(AFSTOV(tvc));
                vnode_put(AFSTOV(found_tvc));
#endif
		return count;
	    }
	    found_tvc = tvc;
	}
    }

    tvc = found_tvc;
    /* should I have a read lock on the vnode here? */
    if (tvc) {
#ifndef AFS_DARWIN80_ENV
#if defined(AFS_SGI_ENV) && !defined(AFS_SGI53_ENV)
	afs_int32 retry = 0;
	osi_vnhold(tvc, &retry);
	if (retry) {
	    count = 0;
	    found_tvc = (struct vcache *)0;
	    ReleaseSharedLock(&afs_xvcache);
	    spunlock_psema(tvc->v.v_lock, retry, &tvc->v.v_sync, PINOD);
	    goto loop;
	}
#else
	osi_vnhold(tvc, (int *)0);	/* already held, above */
#endif
#endif
	/*
	 * We obtained the xvcache lock above.
	 */
	if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
	    refpanic("FindVC VLRU inconsistent1");
	}
	if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
	    refpanic("FindVC VLRU inconsistent1");
	}
	if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
	    refpanic("FindVC VLRU inconsistent2");
	}
	UpgradeSToWLock(&afs_xvcache, 568);
	QRemove(&tvc->vlruq);
	QAdd(&VLRU, &tvc->vlruq);
	ConvertWToSLock(&afs_xvcache);
	if ((VLRU.next->prev != &VLRU) || (VLRU.prev->next != &VLRU)) {
	    refpanic("FindVC VLRU inconsistent1");
	}
	if (tvc->vlruq.next->prev != &(tvc->vlruq)) {
	    refpanic("FindVC VLRU inconsistent2");
	}
	if (tvc->vlruq.prev->next != &(tvc->vlruq)) {
	    refpanic("FindVC VLRU inconsistent3");
	}
    }
    vcachegen++;

    if (tvc)
	afs_stats_cmperf.vcacheHits++;
    else
	afs_stats_cmperf.vcacheMisses++;
    if (afs_IsPrimaryCellNum(afid->Cell))
	afs_stats_cmperf.vlocalAccesses++;
    else
	afs_stats_cmperf.vremoteAccesses++;

    *avcp = tvc;		/* May be null */

    ReleaseSharedLock(&afs_xvcache);
    return (tvc ? 1 : 0);

}				/*afs_NFSFindVCache */




/*!
 * Initialize vcache related variables
 *
 * \param astatSize
 */
void
afs_vcacheInit(int astatSize)
{
#if !defined(AFS_LINUX22_ENV)
    struct vcache *tvp;
#endif
    int i;
    if (!afs_maxvcount) {
	afs_maxvcount = astatSize;	/* no particular limit on linux? */
    }
#if !defined(AFS_LINUX22_ENV)
    freeVCList = NULL;
#endif

    AFS_RWLOCK_INIT(&afs_xvcache, "afs_xvcache");
    LOCK_INIT(&afs_xvcb, "afs_xvcb");

#if !defined(AFS_LINUX22_ENV)
    /* Allocate and thread the struct vcache entries */
    tvp = afs_osi_Alloc(astatSize * sizeof(struct vcache));
    osi_Assert(tvp != NULL);
    memset(tvp, 0, sizeof(struct vcache) * astatSize);

    Initial_freeVCList = tvp;
    freeVCList = &(tvp[0]);
    for (i = 0; i < astatSize - 1; i++) {
	tvp[i].nextfree = &(tvp[i + 1]);
    }
    tvp[astatSize - 1].nextfree = NULL;
# ifdef  KERNEL_HAVE_PIN
    pin((char *)tvp, astatSize * sizeof(struct vcache));	/* XXX */
# endif
#endif

#if defined(AFS_SGI_ENV)
    for (i = 0; i < astatSize; i++) {
	char name[METER_NAMSZ];
	struct vcache *tvc = &tvp[i];

	tvc->v.v_number = ++afsvnumbers;
	tvc->vc_rwlockid = OSI_NO_LOCKID;
	initnsema(&tvc->vc_rwlock, 1,
		  makesname(name, "vrw", tvc->v.v_number));
#ifndef	AFS_SGI53_ENV
	initnsema(&tvc->v.v_sync, 0, makesname(name, "vsy", tvc->v.v_number));
#endif
#ifndef AFS_SGI62_ENV
	initnlock(&tvc->v.v_lock, makesname(name, "vlk", tvc->v.v_number));
#endif /* AFS_SGI62_ENV */
    }
#endif
    QInit(&VLRU);
    for(i = 0; i < VCSIZE; ++i)
	QInit(&afs_vhashTV[i]);
}

/*!
 * Shutdown vcache.
 */
void
shutdown_vcache(void)
{
    int i;
    struct afs_cbr *tsp;
    /*
     * XXX We may potentially miss some of the vcaches because if when
     * there are no free vcache entries and all the vcache entries are active
     * ones then we allocate an additional one - admittedly we almost never
     * had that occur.
     */

    {
	struct afs_q *tq, *uq = NULL;
	struct vcache *tvc;
	for (tq = VLRU.prev; tq != &VLRU; tq = uq) {
	    tvc = QTOV(tq);
	    uq = QPrev(tq);
	    if (tvc->mvid.target_root) {
		osi_FreeSmallSpace(tvc->mvid.target_root);
		tvc->mvid.target_root = NULL;
	    }
#ifdef	AFS_AIX_ENV
	    aix_gnode_rele(AFSTOV(tvc));
#endif
	    if (tvc->linkData) {
		afs_osi_Free(tvc->linkData, strlen(tvc->linkData) + 1);
		tvc->linkData = 0;
	    }
	}
	/*
	 * Also free the remaining ones in the Cache
	 */
	for (i = 0; i < VCSIZE; i++) {
	    for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
		if (tvc->mvid.target_root) {
		    osi_FreeSmallSpace(tvc->mvid.target_root);
		    tvc->mvid.target_root = NULL;
		}
#ifdef	AFS_AIX_ENV
		if (tvc->v.v_gnode)
		    afs_osi_Free(tvc->v.v_gnode, sizeof(struct gnode));
#ifdef	AFS_AIX32_ENV
		if (tvc->segid) {
		    AFS_GUNLOCK();
		    vms_delete(tvc->segid);
		    AFS_GLOCK();
		    tvc->segid = tvc->vmh = NULL;
		    if (VREFCOUNT_GT(tvc,0))
			osi_Panic("flushVcache: vm race");
		}
		if (tvc->credp) {
		    crfree(tvc->credp);
		    tvc->credp = NULL;
		}
#endif
#endif
#if	defined(AFS_SUN5_ENV)
		if (tvc->credp) {
		    crfree(tvc->credp);
		    tvc->credp = NULL;
		}
#endif
		if (tvc->linkData) {
		    afs_osi_Free(tvc->linkData, strlen(tvc->linkData) + 1);
		    tvc->linkData = 0;
		}

		if (tvc->Access)
		    afs_FreeAllAxs(&(tvc->Access));
	    }
	    afs_vhashT[i] = 0;
	}
    }
    /*
     * Free any leftover callback queue
     */
    for (i = 0; i < afs_stats_cmperf.CallBackAlloced; i++) {
	tsp = afs_cbrHeads[i];
	afs_cbrHeads[i] = 0;
	afs_osi_Free((char *)tsp, AFS_NCBRS * sizeof(struct afs_cbr));
    }
    afs_cbrSpace = 0;

#if !defined(AFS_LINUX22_ENV)
    afs_osi_Free(Initial_freeVCList, afs_cacheStats * sizeof(struct vcache));

# ifdef  KERNEL_HAVE_PIN
    unpin(Initial_freeVCList, afs_cacheStats * sizeof(struct vcache));
# endif

    freeVCList = Initial_freeVCList = 0;
#endif

    AFS_RWLOCK_INIT(&afs_xvcache, "afs_xvcache");
    LOCK_INIT(&afs_xvcb, "afs_xvcb");
    QInit(&VLRU);
    for(i = 0; i < VCSIZE; ++i)
	QInit(&afs_vhashTV[i]);
}

void
afs_DisconGiveUpCallbacks(void)
{
    int i;
    struct vcache *tvc;
    int nq=0;

    ObtainWriteLock(&afs_xvcache, 1002); /* XXX - should be a unique number */

 retry:
    /* Somehow, walk the set of vcaches, with each one coming out as tvc */
    for (i = 0; i < VCSIZE; i++) {
        for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    int slept = 0;
            if (afs_QueueVCB(tvc, &slept)) {
                tvc->callback = NULL;
                nq++;
            }
	    if (slept) {
		goto retry;
	    }
        }
    }

    ReleaseWriteLock(&afs_xvcache);

    afs_FlushVCBs(2);
}

/*!
 *
 * Clear the Statd flag from all vcaches
 *
 * This function removes the Statd flag from all vcaches. It's used by
 * disconnected mode to tidy up during reconnection
 *
 */
void
afs_ClearAllStatdFlag(void)
{
    int i;
    struct vcache *tvc;

    ObtainWriteLock(&afs_xvcache, 715);

    for (i = 0; i < VCSIZE; i++) {
	for (tvc = afs_vhashT[i]; tvc; tvc = tvc->hnext) {
	    tvc->f.states &= ~(CStatd|CUnique);
	}
    }
    ReleaseWriteLock(&afs_xvcache);
}
