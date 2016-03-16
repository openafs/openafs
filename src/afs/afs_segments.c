/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * --------------------- Required definitions ---------------------
 */
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/afs_osidnlc.h"

afs_uint32 afs_stampValue = 0;

/*
 * afs_StoreMini
 *
 * Description:
 *	Send a truncation request to a FileServer.
 *
 * Parameters:
 *	xxx : description
 *
 * Environment:
 *	We're write-locked upon entry.
 */

static int
afs_StoreMini(struct vcache *avc, struct vrequest *areq)
{
    struct afs_conn *tc;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutStatus;
    struct AFSVolSync tsync;
    afs_int32 code;
    struct rx_call *tcall;
    struct rx_connection *rxconn;
    afs_size_t tlen, xlen = 0;
    XSTATS_DECLS;
    AFS_STATCNT(afs_StoreMini);
    afs_Trace2(afs_iclSetp, CM_TRACE_STOREMINI, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->f.m.Length);
    tlen = avc->f.m.Length;
    if (avc->f.truncPos < tlen)
	tlen = avc->f.truncPos;
    avc->f.truncPos = AFS_NOTRUNC;
    avc->f.states &= ~CExtendedFile;
    memset(&InStatus, 0, sizeof(InStatus));

    do {
	tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
	if (tc) {
#ifdef AFS_64BIT_CLIENT
	  retry:
#endif
	    RX_AFS_GUNLOCK();
	    tcall = rx_NewCall(rxconn);
	    RX_AFS_GLOCK();
	    /* Set the client mod time since we always want the file
	     * to have the client's mod time and not the server's one
	     * (to avoid problems with make, etc.) It almost always
	     * works fine with standard afs because them server/client
	     * times are in sync and more importantly this storemini
	     * it's a special call that would typically be followed by
	     * the proper store-data or store-status calls.
	     */
	    InStatus.Mask = AFS_SETMODTIME;
	    InStatus.ClientModTime = avc->f.m.Date;
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STOREDATA);
	    afs_Trace4(afs_iclSetp, CM_TRACE_STOREDATA64, ICL_TYPE_FID,
		       &avc->f.fid.Fid, ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(avc->f.m.Length), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(xlen), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(tlen));
	    RX_AFS_GUNLOCK();
#ifdef AFS_64BIT_CLIENT
	    if (!afs_serverHasNo64Bit(tc)) {
		code =
		    StartRXAFS_StoreData64(tcall,
					   (struct AFSFid *)&avc->f.fid.Fid,
					   &InStatus, avc->f.m.Length,
					   (afs_size_t) 0, tlen);
	    } else {
		afs_int32 l1, l2;
		l1 = avc->f.m.Length;
		l2 = tlen;
		if ((avc->f.m.Length > 0x7fffffff) ||
		    (tlen > 0x7fffffff) ||
		    ((0x7fffffff - tlen) < avc->f.m.Length)) {
		    code = EFBIG;
		    goto error;
		}
		code =
		    StartRXAFS_StoreData(tcall,
					 (struct AFSFid *)&avc->f.fid.Fid,
					 &InStatus, l1, 0, l2);
	    }
#else /* AFS_64BIT_CLIENT */
	    code =
		StartRXAFS_StoreData(tcall, (struct AFSFid *)&avc->f.fid.Fid,
				     &InStatus, avc->f.m.Length, 0, tlen);
#endif /* AFS_64BIT_CLIENT */
	    if (code == 0) {
		code = EndRXAFS_StoreData(tcall, &OutStatus, &tsync);
	    }
#ifdef AFS_64BIT_CLIENT
	error:
#endif
	    code = rx_EndCall(tcall, code);
	    RX_AFS_GLOCK();
	    XSTATS_END_TIME;
#ifdef AFS_64BIT_CLIENT
	    if (code == RXGEN_OPCODE && !afs_serverHasNo64Bit(tc)) {
		afs_serverSetNo64Bit(tc);
		goto retry;
	    }
#endif /* AFS_64BIT_CLIENT */
	} else
	    code = -1;
    } while (afs_Analyze
	     (tc, rxconn, code, &avc->f.fid, areq, AFS_STATS_FS_RPCIDX_STOREDATA,
	      SHARED_LOCK, NULL));

    if (code == 0)
	afs_ProcessFS(avc, &OutStatus, areq);

    return code;
}				/*afs_StoreMini */

/*
 * afs_StoreAllSegments
 *
 * Description:
 *	Stores all modified segments back to server
 *
 * Parameters:
 *	avc  : Pointer to vcache entry.
 *	areq : Pointer to request structure.
 *
 * Environment:
 *	Called with avc write-locked.
 */
#if defined (AFS_HPUX_ENV)
int NCHUNKSATONCE = 3;
#else
int NCHUNKSATONCE = 64;
#endif
int afs_dvhack = 0;


int
afs_StoreAllSegments(struct vcache *avc, struct vrequest *areq,
		     int sync)
{
    struct dcache *tdc;
    afs_int32 code = 0;
    afs_int32 index;
    afs_int32 origCBs, foreign = 0;
    int hash;
    afs_hyper_t newDV, oldDV;	/* DV when we start, and finish, respectively */
    struct dcache **dcList;
    unsigned int i, j, minj, moredata, high, off;
    afs_size_t maxStoredLength;	/* highest offset we've written to server. */
    int safety, marineronce = 0;

    AFS_STATCNT(afs_StoreAllSegments);

    hash = DVHash(&avc->f.fid);
    foreign = (avc->f.states & CForeign);
    dcList = (struct dcache **)osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    afs_Trace2(afs_iclSetp, CM_TRACE_STOREALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length));
#if !defined(AFS_AIX32_ENV) && !defined(AFS_SGI65_ENV)
    /* In the aix vm implementation we need to do the vm_writep even
     * on the memcache case since that's we adjust the file's size
     * and finish flushing partial vm pages.
     */
    if ((cacheDiskType != AFS_FCACHE_TYPE_MEM) ||
	(sync & AFS_VMSYNC_INVAL) || (sync & AFS_VMSYNC) ||
	(sync & AFS_LASTSTORE))
#endif /* !AFS_AIX32_ENV && !AFS_SGI65_ENV */
    {
	/* If we're not diskless, reading a file may stress the VM
	 * system enough to cause a pageout, and this vnode would be
	 * locked when the pageout occurs.  We can prevent this problem
	 * by making sure all dirty pages are already flushed.  We don't
	 * do this when diskless because reading a diskless (i.e.
	 * memory-resident) chunk doesn't require using new VM, and we
	 * also don't want to dump more dirty data into a diskless cache,
	 * since they're smaller, and we might exceed its available
	 * space.
	 */
#if	defined(AFS_SUN5_ENV)
	if (sync & AFS_VMSYNC_INVAL)	/* invalidate VM pages */
	    osi_VM_TryToSmush(avc, CRED(), 1);
	else
#endif
	    osi_VM_StoreAllSegments(avc);
    }
    if (AFS_IS_DISCONNECTED && !AFS_IN_SYNC) {
	/* This will probably make someone sad ... */
	/*printf("Net down in afs_StoreSegments\n");*/
	return ENETDOWN;
    }

    /*
     * Can't do this earlier because osi_VM_StoreAllSegments drops locks
     * and can indirectly do some stores that increase the DV.
     */
    hset(oldDV, avc->f.m.DataVersion);
    hset(newDV, avc->f.m.DataVersion);

    ConvertWToSLock(&avc->lock);

    /*
     * Subsequent code expects a sorted list, and it expects all the
     * chunks in the list to be contiguous, so we need a sort and a
     * while loop in here, too - but this will work for a first pass...
     * 92.10.05 - OK, there's a sort in here now.  It's kind of a modified
     *            bin sort, I guess.  Chunk numbers start with 0
     *
     * - Have to get a write lock on xdcache because GetDSlot might need it (if
     *   the chunk doesn't have a dcache struct).
     *   This seems like overkill in most cases.
     * - I'm not sure that it's safe to do "index = .hvNextp", then unlock
     *   xdcache, then relock xdcache and try to use index.  It is done
     *   a lot elsewhere in the CM, but I'm not buying that argument.
     * - should be able to check IFDataMod without doing the GetDSlot (just
     *   hold afs_xdcache).  That way, it's easy to do this without the
     *   writelock on afs_xdcache, and we save unneccessary disk
     *   operations. I don't think that works, 'cuz the next pointers
     *   are still on disk.
     */
    origCBs = afs_allCBs;

    maxStoredLength = 0;
    minj = 0;

    do {
	memset(dcList, 0, NCHUNKSATONCE * sizeof(struct dcache *));
	high = 0;
	moredata = FALSE;

	/* lock and start over from beginning of hash chain
	 * in order to avoid a race condition. */
	ObtainWriteLock(&afs_xdcache, 284);
	index = afs_dvhashTbl[hash];

	for (j = 0; index != NULLIDX;) {
	    if ((afs_indexFlags[index] & IFDataMod)
		&& (afs_indexUnique[index] == avc->f.fid.Fid.Unique)) {
		tdc = afs_GetValidDSlot(index);	/* refcount+1. */
		if (!tdc) {
		    ReleaseWriteLock(&afs_xdcache);
		    code = EIO;
		    goto done;
		}
		ReleaseReadLock(&tdc->tlock);
		if (!FidCmp(&tdc->f.fid, &avc->f.fid) && tdc->f.chunk >= minj) {
		    off = tdc->f.chunk - minj;
		    if (off < NCHUNKSATONCE) {
			if (dcList[off])
			    osi_Panic("dclist slot already in use!");
			if (afs_mariner && !marineronce) {
			    /* first chunk only */
			    afs_MarinerLog("store$Storing", avc);
			    marineronce++;
			}
			dcList[off] = tdc;
			if (off > high)
			    high = off;
			j++;
			/* DCLOCKXXX: chunkBytes is protected by tdc->lock which we
			 * can't grab here, due to lock ordering with afs_xdcache.
			 * So, disable this shortcut for now.  -- kolya 2001-10-13
			 */
			/* shortcut: big win for little files */
			/* tlen -= tdc->f.chunkBytes;
			 * if (tlen <= 0)
			 *    break;
			 */
		    } else {
			moredata = TRUE;
			afs_PutDCache(tdc);
			if (j == NCHUNKSATONCE)
			    break;
		    }
		} else {
		    afs_PutDCache(tdc);
		}
	    }
	    index = afs_dvnextTbl[index];
	}
	ReleaseWriteLock(&afs_xdcache);

	/* this guy writes chunks, puts back dcache structs, and bumps newDV */
	/* "moredata" just says "there are more dirty chunks yet to come".
	 */
	if (j) {
	    code =
		afs_CacheStoreVCache(dcList, avc, areq, sync,
				   minj, high, moredata,
				   &newDV, &maxStoredLength);
	    /* Release any zero-length dcache entries in our interval
	     * that we locked but didn't store back above.
	     */
	    for (j = 0; j <= high; j++) {
		tdc = dcList[j];
		if (tdc) {
		    osi_Assert(tdc->f.chunkBytes == 0);
		    ReleaseSharedLock(&tdc->lock);
		    afs_PutDCache(tdc);
		}
	    }
	}
	/* if (j) */
	minj += NCHUNKSATONCE;
    } while (!code && moredata);

 done:
    UpgradeSToWLock(&avc->lock, 29);

    /* send a trivial truncation store if did nothing else */
    if (code == 0) {
	/*
	 * Call StoreMini if we haven't written enough data to extend the
	 * file at the fileserver to the client's notion of the file length.
	 */
	if ((avc->f.truncPos != AFS_NOTRUNC)
	    || ((avc->f.states & CExtendedFile)
		&& (maxStoredLength < avc->f.m.Length))) {
	    code = afs_StoreMini(avc, areq);
	    if (code == 0)
		hadd32(newDV, 1);	/* just bumped here, too */
	}
	avc->f.states &= ~CExtendedFile;
    }

    /*
     * Finally, turn off DWriting, turn on DFEntryMod,
     * update f.versionNo.
     * A lot of this could be integrated into the loop above
     */
    if (!code) {
	afs_hyper_t h_unset;
	hones(h_unset);

	minj = 0;

	do {
	    moredata = FALSE;
	    memset(dcList, 0,
		   NCHUNKSATONCE * sizeof(struct dcache *));

	    /* overkill, but it gets the lock in case GetDSlot needs it */
	    ObtainWriteLock(&afs_xdcache, 285);

	    for (j = 0, safety = 0, index = afs_dvhashTbl[hash];
		 index != NULLIDX && safety < afs_cacheFiles + 2;
	         index = afs_dvnextTbl[index]) {

		if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
		    tdc = afs_GetValidDSlot(index);
		    if (!tdc) {
			/* This is okay; since manipulating the dcaches at this
			 * point is best-effort. We only get a dcache here to
			 * increment the dv and turn off DWriting. If we were
			 * supposed to do that for a dcache, but could not
			 * due to an I/O error, it just means the dv won't
			 * be updated so we don't be able to use that cached
			 * chunk in the future. That's inefficient, but not
			 * an error. */
			continue;
		    }
		    ReleaseReadLock(&tdc->tlock);

		    if (!FidCmp(&tdc->f.fid, &avc->f.fid)
			&& tdc->f.chunk >= minj) {
			off = tdc->f.chunk - minj;
			if (off < NCHUNKSATONCE) {
			    /* this is the file, and the correct chunk range */
			    if (j >= NCHUNKSATONCE)
				osi_Panic
				    ("Too many dcache entries in range\n");
			    dcList[j++] = tdc;
			} else {
			    moredata = TRUE;
			    afs_PutDCache(tdc);
			    if (j == NCHUNKSATONCE)
				break;
			}
		    } else {
			afs_PutDCache(tdc);
		    }
		}
	    }
	    ReleaseWriteLock(&afs_xdcache);

	    for (i = 0; i < j; i++) {
		/* Iterate over the dcache entries we collected above */
		tdc = dcList[i];
		ObtainSharedLock(&tdc->lock, 677);

		/* was code here to clear IFDataMod, but it should only be done
		 * in storedcache and storealldcache.
		 */
		/* Only increase DV if we had up-to-date data to start with.
		 * Otherwise, we could be falsely upgrading an old chunk
		 * (that we never read) into one labelled with the current
		 * DV #.  Also note that we check that no intervening stores
		 * occurred, otherwise we might mislabel cache information
		 * for a chunk that we didn't store this time
		 */
		/* Don't update the version number if it's not yet set. */
		if (!hsame(tdc->f.versionNo, h_unset)
		    && hcmp(tdc->f.versionNo, oldDV) >= 0) {

		    if ((!(afs_dvhack || foreign)
			 && hsame(avc->f.m.DataVersion, newDV))
			|| ((afs_dvhack || foreign)
			    && (origCBs == afs_allCBs))) {
			/* no error, this is the DV */

			UpgradeSToWLock(&tdc->lock, 678);
			hset(tdc->f.versionNo, avc->f.m.DataVersion);
			tdc->dflags |= DFEntryMod;
			/* DWriting may not have gotten cleared above, if all
			 * we did was a StoreMini */
			tdc->f.states &= ~DWriting;
			ConvertWToSLock(&tdc->lock);
		    }
		}

		ReleaseSharedLock(&tdc->lock);
		afs_PutDCache(tdc);
	    }

	    minj += NCHUNKSATONCE;

	} while (moredata);
    }

    if (code) {
	/*
	 * Invalidate chunks after an error for ccores files since
	 * afs_inactive won't be called for these and they won't be
	 * invalidated. Also discard data if it's a permanent error from the
	 * fileserver.
	 */
	if (areq->permWriteError || (avc->f.states & CCore)) {
	    afs_InvalidateAllSegments(avc);
	}
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_STOREALLDONE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->f.m.Length, ICL_TYPE_INT32, code);
    /* would like a Trace5, but it doesn't exist... */
    afs_Trace3(afs_iclSetp, CM_TRACE_AVCLOCKER, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->lock.wait_states, ICL_TYPE_INT32,
	       avc->lock.excl_locked);
    afs_Trace4(afs_iclSetp, CM_TRACE_AVCLOCKEE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->lock.wait_states, ICL_TYPE_INT32,
	       avc->lock.readers_reading, ICL_TYPE_INT32,
	       avc->lock.num_waiting);

    /*
     * Finally, if updated DataVersion matches newDV, we did all of the
     * stores.  If mapDV indicates that the page cache was flushed up
     * to when we started the store, then we can relabel them as flushed
     * as recently as newDV.
     * Turn off CDirty bit because the stored data is now in sync with server.
     */
    if (code == 0 && hcmp(avc->mapDV, oldDV) >= 0) {
	if ((!(afs_dvhack || foreign) && hsame(avc->f.m.DataVersion, newDV))
	    || ((afs_dvhack || foreign) && (origCBs == afs_allCBs))) {
	    hset(avc->mapDV, newDV);
	    avc->f.states &= ~CDirty;
	}
    }
    osi_FreeLargeSpace(dcList);

    /* If not the final write a temporary error is ok. */
    if (code && !areq->permWriteError && !(sync & AFS_LASTSTORE))
	code = 0;

    return code;

}				/*afs_StoreAllSegments (new 03/02/94) */


/*
 * afs_InvalidateAllSegments
 *
 * Description:
 *	Invalidates all chunks for a given file
 *
 * Parameters:
 *	avc      : Pointer to vcache entry.
 *
 * Environment:
 *	For example, called after an error has been detected.  Called
 *	with avc write-locked, and afs_xdcache unheld.
 */

int
afs_InvalidateAllSegments(struct vcache *avc)
{
    struct dcache *tdc;
    afs_int32 hash;
    afs_int32 index;
    struct dcache **dcList;
    int i, dcListMax, dcListCount;

    AFS_STATCNT(afs_InvalidateAllSegments);
    afs_Trace2(afs_iclSetp, CM_TRACE_INVALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length));
    hash = DVHash(&avc->f.fid);
    avc->f.truncPos = AFS_NOTRUNC;	/* don't truncate later */
    avc->f.states &= ~CExtendedFile;	/* not any more */
    ObtainWriteLock(&afs_xcbhash, 459);
    afs_DequeueCallback(avc);
    avc->f.states &= ~(CStatd | CDirty);	/* mark status information as bad, too */
    ReleaseWriteLock(&afs_xcbhash);
    if (avc->f.fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	osi_dnlc_purgedp(avc);
    /* Blow away pages; for now, only for Solaris */
#if	(defined(AFS_SUN5_ENV))
    if (WriteLocked(&avc->lock))
	osi_ReleaseVM(avc, (afs_ucred_t *)0);
#endif
    /*
     * Block out others from screwing with this table; is a read lock
     * sufficient?
     */
    ObtainWriteLock(&afs_xdcache, 286);
    dcListMax = 0;

    for (index = afs_dvhashTbl[hash]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
	    tdc = afs_GetValidDSlot(index);
	    if (!tdc) {
		/* In the case of fatal errors during stores, we MUST
		 * invalidate all of the relevant chunks. Otherwise, the chunks
		 * will be left with the 'new' data that was never successfully
		 * written to the server, but the DV in the dcache is still the
		 * old DV. So, we may indefintely serve serve applications data
		 * that is not actually in the file on the fileserver. If we
		 * cannot afs_GetValidDSlot the appropriate entries, currently
		 * there is no way to ensure the dcache is invalidated. So for
		 * now, to avoid risking serving bad data from the cache, panic
		 * instead. */
		osi_Panic("afs_InvalidateAllSegments tdc count");
	    }
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->f.fid))
		dcListMax++;
	    afs_PutDCache(tdc);
	}
	index = afs_dvnextTbl[index];
    }

    dcList = osi_Alloc(dcListMax * sizeof(struct dcache *));
    dcListCount = 0;

    for (index = afs_dvhashTbl[hash]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
	    tdc = afs_GetValidDSlot(index);
	    if (!tdc) {
		/* We cannot proceed after getting this error; we risk serving
		 * incorrect data to applications. So panic instead. See the
		 * above comment next to the previous afs_GetValidDSlot call
		 * for details. */
		osi_Panic("afs_InvalidateAllSegments tdc store");
	    }
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->f.fid)) {
		/* same file? we'll zap it */
		if (afs_indexFlags[index] & IFDataMod) {
		    afs_stats_cmperf.cacheCurrDirtyChunks--;
		    /* don't write it back */
		    afs_indexFlags[index] &= ~IFDataMod;
		}
		afs_indexFlags[index] &= ~IFAnyPages;
		if (dcListCount < dcListMax)
		    dcList[dcListCount++] = tdc;
		else
		    afs_PutDCache(tdc);
	    } else {
		afs_PutDCache(tdc);
	    }
	}
	index = afs_dvnextTbl[index];
    }
    ReleaseWriteLock(&afs_xdcache);

    for (i = 0; i < dcListCount; i++) {
	tdc = dcList[i];

	ObtainWriteLock(&tdc->lock, 679);
	ZapDCE(tdc);
	if (vType(avc) == VDIR)
	    DZap(tdc);
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
    }

    osi_Free(dcList, dcListMax * sizeof(struct dcache *));

    return 0;
}

/*!
 *
 * Extend a cache file
 *
 * \param avc pointer to vcache to extend data for
 * \param alen Length to extend file to
 * \param areq
 *
 * \note avc must be write locked. May release and reobtain avc and GLOCK
 */
int
afs_ExtendSegments(struct vcache *avc, afs_size_t alen, struct vrequest *areq)
{
    afs_size_t offset, toAdd;
    struct osi_file *tfile;
    afs_int32 code = 0;
    struct dcache *tdc;
    void *zeros;

    zeros = afs_osi_Alloc(AFS_PAGESIZE);
    if (zeros == NULL)
	return ENOMEM;
    memset(zeros, 0, AFS_PAGESIZE);

    while (avc->f.m.Length < alen) {
        tdc = afs_ObtainDCacheForWriting(avc, avc->f.m.Length, alen - avc->f.m.Length, areq, 0);
        if (!tdc) {
	    code = EIO;
	    break;
        }

	toAdd = alen - avc->f.m.Length;

        offset = avc->f.m.Length - AFS_CHUNKTOBASE(tdc->f.chunk);
	if (offset + toAdd > AFS_CHUNKTOSIZE(tdc->f.chunk)) {
	    toAdd = AFS_CHUNKTOSIZE(tdc->f.chunk) - offset;
	}
        tfile = afs_CFileOpen(&tdc->f.inode);
	while(tdc->validPos < avc->f.m.Length + toAdd) {
	     afs_size_t towrite;

	     towrite = (avc->f.m.Length + toAdd) - tdc->validPos;
	     if (towrite > AFS_PAGESIZE) towrite = AFS_PAGESIZE;

	     code = afs_CFileWrite(tfile,
			           tdc->validPos - AFS_CHUNKTOBASE(tdc->f.chunk),
				   zeros, towrite);
	     tdc->validPos += towrite;
	}
	afs_CFileClose(tfile);
	afs_AdjustSize(tdc, offset + toAdd );
	avc->f.m.Length += toAdd;
	ReleaseWriteLock(&tdc->lock);
	afs_PutDCache(tdc);
    }

    afs_osi_Free(zeros, AFS_PAGESIZE);
    return code;
}

/*
 * afs_TruncateAllSegments
 *
 * Description:
 *	Truncate a cache file.
 *
 * Parameters:
 *	avc  : Ptr to vcache entry to truncate.
 *	alen : Number of bytes to make the file.
 *	areq : Ptr to request structure.
 *
 * Environment:
 *	Called with avc write-locked; in VFS40 systems, pvnLock is also
 *	held.
 */
int
afs_TruncateAllSegments(struct vcache *avc, afs_size_t alen,
			struct vrequest *areq, afs_ucred_t *acred)
{
    struct dcache *tdc;
    afs_int32 code;
    afs_int32 index;
    afs_size_t newSize;

    int dcCount, dcPos;
    struct dcache **tdcArray = NULL;

    AFS_STATCNT(afs_TruncateAllSegments);
    avc->f.m.Date = osi_Time();
    afs_Trace3(afs_iclSetp, CM_TRACE_TRUNCALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length),
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(alen));
    if (alen >= avc->f.m.Length) {
	/*
	 * Special speedup since Sun's vm extends the file this way;
	 * we've never written to the file thus we can just set the new
	 * length and avoid the needless calls below.
	 * Also used for ftruncate calls which can extend the file.
	 * To completely minimize the possible extra StoreMini RPC, we really
	 * should keep the ExtendedPos as well and clear this flag if we
	 * truncate below that value before we store the file back.
	 */
	avc->f.states |= CExtendedFile;
	avc->f.m.Length = alen;
	return 0;
    }
#if	(defined(AFS_SUN5_ENV))

    /* Zero unused portion of last page */
    osi_VM_PreTruncate(avc, alen, acred);

#endif

#if	(defined(AFS_SUN5_ENV))
    ObtainWriteLock(&avc->vlock, 546);
    avc->activeV++;		/* Block new getpages */
    ReleaseWriteLock(&avc->vlock);
#endif

    ReleaseWriteLock(&avc->lock);
    AFS_GUNLOCK();

    /* Flush pages beyond end-of-file. */
    osi_VM_Truncate(avc, alen, acred);

    AFS_GLOCK();
    ObtainWriteLock(&avc->lock, 79);

    avc->f.m.Length = alen;

    if (alen < avc->f.truncPos)
	avc->f.truncPos = alen;
    code = DVHash(&avc->f.fid);

    /* block out others from screwing with this table */
    ObtainWriteLock(&afs_xdcache, 287);

    dcCount = 0;
    for (index = afs_dvhashTbl[code]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
	    tdc = afs_GetValidDSlot(index);
	    if (!tdc) {
		ReleaseWriteLock(&afs_xdcache);
		code = EIO;
		goto done;
	    }
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->f.fid))
		dcCount++;
	    afs_PutDCache(tdc);
	}
	index = afs_dvnextTbl[index];
    }

    /* Now allocate space where we can save those dcache entries, and
     * do a second pass over them..  Since we're holding xdcache, it
     * shouldn't be changing.
     */
    tdcArray = osi_Alloc(dcCount * sizeof(struct dcache *));
    dcPos = 0;

    for (index = afs_dvhashTbl[code]; index != NULLIDX; index = afs_dvnextTbl[index]) {
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
	    tdc = afs_GetValidDSlot(index);
	    if (!tdc) {
		/* make sure we put back all of the tdcArray members before
		 * bailing out */
		/* remember, the last valid tdc is at dcPos-1, so start at
		 * dcPos-1, not at dcPos itself. */
		for (dcPos = dcPos - 1; dcPos >= 0; dcPos--) {
		    tdc = tdcArray[dcPos];
		    afs_PutDCache(tdc);
		}
		code = EIO;
		goto done;
	    }
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->f.fid)) {
		/* same file, and modified, we'll store it back */
		if (dcPos < dcCount) {
		    tdcArray[dcPos++] = tdc;
		} else {
		    afs_PutDCache(tdc);
		}
	    } else {
		afs_PutDCache(tdc);
	    }
	}
    }

    ReleaseWriteLock(&afs_xdcache);

    /* Now we loop over the array of dcache entries and truncate them */
    for (index = 0; index < dcPos; index++) {
	struct osi_file *tfile;

	tdc = tdcArray[index];

	newSize = alen - AFS_CHUNKTOBASE(tdc->f.chunk);
	if (newSize < 0)
	    newSize = 0;
	ObtainSharedLock(&tdc->lock, 672);
	if (newSize < tdc->f.chunkBytes && newSize < MAX_AFS_UINT32) {
	    UpgradeSToWLock(&tdc->lock, 673);
	    tdc->f.states |= DWriting;
	    tfile = afs_CFileOpen(&tdc->f.inode);
	    afs_CFileTruncate(tfile, (afs_int32)newSize);
	    afs_CFileClose(tfile);
	    afs_AdjustSize(tdc, (afs_int32)newSize);
	    if (alen < tdc->validPos) {
                if (alen < AFS_CHUNKTOBASE(tdc->f.chunk))
                    tdc->validPos = 0;
                else
                    tdc->validPos = alen;
            }
	    ConvertWToSLock(&tdc->lock);
	}
	ReleaseSharedLock(&tdc->lock);
	afs_PutDCache(tdc);
    }

    code = 0;

 done:
    if (tdcArray) {
	osi_Free(tdcArray, dcCount * sizeof(struct dcache *));
    }
#if	(defined(AFS_SUN5_ENV))
    ObtainWriteLock(&avc->vlock, 547);
    if (--avc->activeV == 0 && (avc->vstates & VRevokeWait)) {
	avc->vstates &= ~VRevokeWait;
	afs_osi_Wakeup((char *)&avc->vstates);
    }
    ReleaseWriteLock(&avc->vlock);
#endif

    return code;
}
