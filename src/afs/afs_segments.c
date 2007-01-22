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

RCSID
    ("$Header$");

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

int
afs_StoreMini(register struct vcache *avc, struct vrequest *areq)
{
    register struct conn *tc;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutStatus;
    struct AFSVolSync tsync;
    register afs_int32 code;
    register struct rx_call *tcall;
    afs_size_t tlen, xlen = 0;
    XSTATS_DECLS;
    AFS_STATCNT(afs_StoreMini);
    afs_Trace2(afs_iclSetp, CM_TRACE_STOREMINI, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length);
    tlen = avc->m.Length;
    if (avc->truncPos < tlen)
	tlen = avc->truncPos;
    avc->truncPos = AFS_NOTRUNC;
    avc->states &= ~CExtendedFile;

    do {
	tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	if (tc) {
	  retry:
	    RX_AFS_GUNLOCK();
	    tcall = rx_NewCall(tc->id);
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
	    InStatus.ClientModTime = avc->m.Date;
	    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STOREDATA);
	    afs_Trace4(afs_iclSetp, CM_TRACE_STOREDATA64, ICL_TYPE_FID,
		       &avc->fid.Fid, ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(avc->m.Length), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(xlen), ICL_TYPE_OFFSET,
		       ICL_HANDLE_OFFSET(tlen));
	    RX_AFS_GUNLOCK();
#ifdef AFS_64BIT_CLIENT
	    if (!afs_serverHasNo64Bit(tc)) {
		code =
		    StartRXAFS_StoreData64(tcall,
					   (struct AFSFid *)&avc->fid.Fid,
					   &InStatus, avc->m.Length,
					   (afs_size_t) 0, tlen);
	    } else {
		afs_int32 l1, l2;
		l1 = avc->m.Length;
		l2 = tlen;
		code =
		    StartRXAFS_StoreData(tcall,
					 (struct AFSFid *)&avc->fid.Fid,
					 &InStatus, l1, 0, l2);
	    }
#else /* AFS_64BIT_CLIENT */
	    code =
		StartRXAFS_StoreData(tcall, (struct AFSFid *)&avc->fid.Fid,
				     &InStatus, avc->m.Length, 0, tlen);
#endif /* AFS_64BIT_CLIENT */
	    if (code == 0) {
		code = EndRXAFS_StoreData(tcall, &OutStatus, &tsync);
	    }
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
	     (tc, code, &avc->fid, areq, AFS_STATS_FS_RPCIDX_STOREDATA,
	      SHARED_LOCK, NULL));

    if (code == 0) {
	afs_ProcessFS(avc, &OutStatus, areq);
    } else {
	/* blew it away */
	afs_InvalidateAllSegments(avc);
    }
    return code;

}				/*afs_StoreMini */

unsigned int storeallmissing = 0;
#define lmin(a,b) (((a) < (b)) ? (a) : (b))
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
afs_StoreAllSegments(register struct vcache *avc, struct vrequest *areq,
		     int sync)
{
    register struct dcache *tdc;
    register afs_int32 code = 0;
    register afs_int32 index;
    register afs_int32 origCBs, foreign = 0;
    int hash, stored;
    afs_hyper_t newDV, oldDV;	/* DV when we start, and finish, respectively */
    struct dcache **dcList, **dclist;
    unsigned int i, j, minj, moredata, high, off;
    afs_size_t tlen;
    afs_size_t maxStoredLength;	/* highest offset we've written to server. */
    int safety;
#ifndef AFS_NOSTATS
    struct afs_stats_xferData *xferP;	/* Ptr to this op's xfer struct */
    afs_timeval_t xferStartTime,	/*FS xfer start time */
      xferStopTime;		/*FS xfer stop time */
    afs_size_t bytesToXfer;	/* # bytes to xfer */
    afs_size_t bytesXferred;	/* # bytes actually xferred */
#endif /* AFS_NOSTATS */


    AFS_STATCNT(afs_StoreAllSegments);

    hset(oldDV, avc->m.DataVersion);
    hset(newDV, avc->m.DataVersion);
    hash = DVHash(&avc->fid);
    foreign = (avc->states & CForeign);
    dcList = (struct dcache **)osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    afs_Trace2(afs_iclSetp, CM_TRACE_STOREALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->m.Length));
#if !defined(AFS_AIX32_ENV) && !defined(AFS_SGI65_ENV)
    /* In the aix vm implementation we need to do the vm_writep even
     * on the memcache case since that's we adjust the file's size
     * and finish flushing partial vm pages.
     */
    if (cacheDiskType != AFS_FCACHE_TYPE_MEM)
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
    tlen = avc->m.Length;
    minj = 0;

    do {
	memset((char *)dcList, 0, NCHUNKSATONCE * sizeof(struct dcache *));
	high = 0;
	moredata = FALSE;

	/* lock and start over from beginning of hash chain 
	 * in order to avoid a race condition. */
	MObtainWriteLock(&afs_xdcache, 284);
	index = afs_dvhashTbl[hash];

	for (j = 0; index != NULLIDX;) {
	    if ((afs_indexFlags[index] & IFDataMod)
		&& (afs_indexUnique[index] == avc->fid.Fid.Unique)) {
		tdc = afs_GetDSlot(index, 0);	/* refcount+1. */
		ReleaseReadLock(&tdc->tlock);
		if (!FidCmp(&tdc->f.fid, &avc->fid) && tdc->f.chunk >= minj) {
		    off = tdc->f.chunk - minj;
		    if (off < NCHUNKSATONCE) {
			if (dcList[off])
			    osi_Panic("dclist slot already in use!");
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
	MReleaseWriteLock(&afs_xdcache);

	/* this guy writes chunks, puts back dcache structs, and bumps newDV */
	/* "moredata" just says "there are more dirty chunks yet to come".
	 */
	if (j) {
#ifdef AFS_NOSTATS
	    static afs_uint32 lp1 = 10000, lp2 = 10000;
#endif
	    struct AFSStoreStatus InStatus;
	    struct AFSFetchStatus OutStatus;
	    int doProcessFS = 0;
	    afs_size_t base, bytes;
	    afs_uint32 nchunks;
	    int nomore;
	    unsigned int first = 0;
	    int *shouldwake;
	    struct conn *tc;
	    struct osi_file *tfile;
	    struct rx_call *tcall;
	    XSTATS_DECLS;
	    for (bytes = 0, j = 0; !code && j <= high; j++) {
		if (dcList[j]) {
		    ObtainSharedLock(&(dcList[j]->lock), 629);
		    if (!bytes)
			first = j;
		    bytes += dcList[j]->f.chunkBytes;
		    if ((dcList[j]->f.chunkBytes < afs_OtherCSize)
			&& (dcList[j]->f.chunk - minj < high)
			&& dcList[j + 1]) {
			int sbytes = afs_OtherCSize - dcList[j]->f.chunkBytes;
			bytes += sbytes;
		    }
		}
		if (bytes && (j == high || !dcList[j + 1])) {
		    /* base = AFS_CHUNKTOBASE(dcList[first]->f.chunk); */
		    base = AFS_CHUNKTOBASE(first + minj);
		    /*
		     * 
		     * take a list of dcache structs and send them all off to the server
		     * the list must be in order, and the chunks contiguous.
		     * Note - there is no locking done by this code currently.  For
		     * safety's sake, xdcache could be locked over the entire call.
		     * However, that pretty well ties up all the threads.  Meantime, all
		     * the chunks _MUST_ have their refcounts bumped.
		     * The writes done before a store back will clear setuid-ness
		     * in cache file.
		     * We can permit CacheStoreProc to wake up the user process IFF we 
		     * are doing the last RPC for this close, ie, storing back the last 
		     * set of contiguous chunks of a file.
		     */

		    dclist = &dcList[first];
		    nchunks = 1 + j - first;
		    nomore = !(moredata || (j != high));
		    InStatus.ClientModTime = avc->m.Date;
		    InStatus.Mask = AFS_SETMODTIME;
		    if (sync & AFS_SYNC) {
			InStatus.Mask |= AFS_FSYNC;
		    }
		    tlen = lmin(avc->m.Length, avc->truncPos);
		    afs_Trace4(afs_iclSetp, CM_TRACE_STOREDATA64,
			       ICL_TYPE_FID, &avc->fid.Fid, ICL_TYPE_OFFSET,
			       ICL_HANDLE_OFFSET(base), ICL_TYPE_OFFSET,
			       ICL_HANDLE_OFFSET(bytes), ICL_TYPE_OFFSET,
			       ICL_HANDLE_OFFSET(tlen));

		    do {
			stored = 0;
			tc = afs_Conn(&avc->fid, areq, 0);
			if (tc) {
			  restart:
			    RX_AFS_GUNLOCK();
			    tcall = rx_NewCall(tc->id);
#ifdef AFS_64BIT_CLIENT
			    if (!afs_serverHasNo64Bit(tc)) {
				code =
				    StartRXAFS_StoreData64(tcall,
							   (struct AFSFid *)
							   &avc->fid.Fid,
							   &InStatus, base,
							   bytes, tlen);
			    } else {
				if (tlen > 0xFFFFFFFF) {
				    code = EFBIG;
				} else {
				    afs_int32 t1, t2, t3;
				    t1 = base;
				    t2 = bytes;
				    t3 = tlen;
				    code =
					StartRXAFS_StoreData(tcall,
							     (struct AFSFid *)
							     &avc->fid.Fid,
							     &InStatus, t1,
							     t2, t3);
				}
			    }
#else /* AFS_64BIT_CLIENT */
			    code =
				StartRXAFS_StoreData(tcall,
						     (struct AFSFid *)&avc->
						     fid.Fid, &InStatus, base,
						     bytes, tlen);
#endif /* AFS_64BIT_CLIENT */
			    RX_AFS_GLOCK();
			} else {
			    code = -1;
			    tcall = NULL;
			}
			if (!code) {
			    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STOREDATA);
			    avc->truncPos = AFS_NOTRUNC;
			}
			for (i = 0; i < nchunks && !code; i++) {
			    tdc = dclist[i];
			    if (!tdc) {
				afs_warn("afs: missing dcache!\n");
				storeallmissing++;
				continue;	/* panic? */
			    }
			    afs_Trace4(afs_iclSetp, CM_TRACE_STOREALL2,
				       ICL_TYPE_POINTER, avc, ICL_TYPE_INT32,
				       tdc->f.chunk, ICL_TYPE_INT32,
				       tdc->index, ICL_TYPE_INT32,
				       tdc->f.inode);
			    shouldwake = 0;
			    if (nomore) {
				if (avc->asynchrony == -1) {
				    if (afs_defaultAsynchrony >
					(bytes - stored)) {
					shouldwake = &nomore;
				    }
				} else if ((afs_uint32) avc->asynchrony >=
					   (bytes - stored)) {
				    shouldwake = &nomore;
				}
			    }
			    tfile = afs_CFileOpen(tdc->f.inode);
#ifndef AFS_NOSTATS
			    xferP =
				&(afs_stats_cmfullperf.rpc.
				  fsXferTimes
				  [AFS_STATS_FS_XFERIDX_STOREDATA]);
			    osi_GetuTime(&xferStartTime);

			    code =
				afs_CacheStoreProc(tcall, tfile,
						   tdc->f.chunkBytes, avc,
						   shouldwake, &bytesToXfer,
						   &bytesXferred);

			    osi_GetuTime(&xferStopTime);
			    (xferP->numXfers)++;
			    if (!code) {
				(xferP->numSuccesses)++;
				afs_stats_XferSumBytes
				    [AFS_STATS_FS_XFERIDX_STOREDATA] +=
				    bytesXferred;
				(xferP->sumBytes) +=
				    (afs_stats_XferSumBytes
				     [AFS_STATS_FS_XFERIDX_STOREDATA] >> 10);
				afs_stats_XferSumBytes
				    [AFS_STATS_FS_XFERIDX_STOREDATA] &= 0x3FF;
				if (bytesXferred < xferP->minBytes)
				    xferP->minBytes = bytesXferred;
				if (bytesXferred > xferP->maxBytes)
				    xferP->maxBytes = bytesXferred;

				/*
				 * Tally the size of the object.  Note: we tally the actual size,
				 * NOT the number of bytes that made it out over the wire.
				 */
				if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET0)
				    (xferP->count[0])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET1)
				    (xferP->count[1])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET2)
				    (xferP->count[2])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET3)
				    (xferP->count[3])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET4)
				    (xferP->count[4])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET5)
				    (xferP->count[5])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET6)
				    (xferP->count[6])++;
				else if (bytesToXfer <=
					 AFS_STATS_MAXBYTES_BUCKET7)
				    (xferP->count[7])++;
				else
				    (xferP->count[8])++;

				afs_stats_GetDiff(elapsedTime, xferStartTime,
						  xferStopTime);
				afs_stats_AddTo((xferP->sumTime),
						elapsedTime);
				afs_stats_SquareAddTo((xferP->sqrTime),
						      elapsedTime);
				if (afs_stats_TimeLessThan
				    (elapsedTime, (xferP->minTime))) {
				    afs_stats_TimeAssign((xferP->minTime),
							 elapsedTime);
				}
				if (afs_stats_TimeGreaterThan
				    (elapsedTime, (xferP->maxTime))) {
				    afs_stats_TimeAssign((xferP->maxTime),
							 elapsedTime);
				}
			    }
#else
			    code =
				afs_CacheStoreProc(tcall, tfile,
						   tdc->f.chunkBytes, avc,
						   shouldwake, &lp1, &lp2);
#endif /* AFS_NOSTATS */
			    afs_CFileClose(tfile);
			    if ((tdc->f.chunkBytes < afs_OtherCSize)
				&& (i < (nchunks - 1)) && code == 0) {
				int bsent, tlen, sbytes =
				    afs_OtherCSize - tdc->f.chunkBytes;
				char *tbuffer =
				    osi_AllocLargeSpace(AFS_LRALLOCSIZ);

				while (sbytes > 0) {
				    tlen =
					(sbytes >
					 AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ :
					 sbytes);
				    memset(tbuffer, 0, tlen);
				    RX_AFS_GUNLOCK();
				    bsent = rx_Write(tcall, tbuffer, tlen);
				    RX_AFS_GLOCK();

				    if (bsent != tlen) {
					code = -33;	/* XXX */
					break;
				    }
				    sbytes -= tlen;
				}
				osi_FreeLargeSpace(tbuffer);
			    }
			    stored += tdc->f.chunkBytes;

			    /* ideally, I'd like to unlock the dcache and turn
			     * off the writing bit here, but that would
			     * require being able to retry StoreAllSegments in
			     * the event of a failure. It only really matters
			     * if user can't read from a 'locked' dcache or
			     * one which has the writing bit turned on. */
			}
			if (!code) {
			    struct AFSVolSync tsync;
			    RX_AFS_GUNLOCK();
			    code =
				EndRXAFS_StoreData(tcall, &OutStatus, &tsync);
			    RX_AFS_GLOCK();
			    hadd32(newDV, 1);
			    XSTATS_END_TIME;
			    if (!code)
				doProcessFS = 1;	/* Flag to run afs_ProcessFS() later on */
			}
			if (tcall) {
			    afs_int32 code2;
			    RX_AFS_GUNLOCK();
			    code2 = rx_EndCall(tcall, code);
			    RX_AFS_GLOCK();
			    if (code2)
				code = code2;
			}
#ifdef AFS_64BIT_CLIENT
			if (code == RXGEN_OPCODE && !afs_serverHasNo64Bit(tc)) {
			    afs_serverSetNo64Bit(tc);
			    goto restart;
			}
#endif /* AFS_64BIT_CLIENT */
		    } while (afs_Analyze
			     (tc, code, &avc->fid, areq,
			      AFS_STATS_FS_RPCIDX_STOREDATA, SHARED_LOCK,
			      NULL));

		    /* put back all remaining locked dcache entries */
		    for (i = 0; i < nchunks; i++) {
			tdc = dclist[i];
			if (!code) {
			    if (afs_indexFlags[tdc->index] & IFDataMod) {
				/*
				 * LOCKXXX -- should hold afs_xdcache(W) when
				 * modifying afs_indexFlags.
				 */
				afs_indexFlags[tdc->index] &= ~IFDataMod;
				afs_stats_cmperf.cacheCurrDirtyChunks--;
				afs_indexFlags[tdc->index] &= ~IFDirtyPages;
				if (sync & AFS_VMSYNC_INVAL) {
				    /* since we have invalidated all the pages of this
				     ** vnode by calling osi_VM_TryToSmush, we can
				     ** safely mark this dcache entry as not having
				     ** any pages. This vnode now becomes eligible for
				     ** reclamation by getDownD.
				     */
				    afs_indexFlags[tdc->index] &= ~IFAnyPages;
				}
			    }
			}
			UpgradeSToWLock(&tdc->lock, 628);
			tdc->f.states &= ~DWriting;	/* correct? */
			tdc->dflags |= DFEntryMod;
			ReleaseWriteLock(&tdc->lock);
			afs_PutDCache(tdc);
			/* Mark the entry as released */
			dclist[i] = NULL;
		    }

		    if (doProcessFS) {
			/* Now copy out return params */
			UpgradeSToWLock(&avc->lock, 28);	/* keep out others for a while */
			afs_ProcessFS(avc, &OutStatus, areq);
			/* Keep last (max) size of file on server to see if
			 * we need to call afs_StoreMini to extend the file.
			 */
			if (!moredata)
			    maxStoredLength = OutStatus.Length;
			ConvertWToSLock(&avc->lock);
			doProcessFS = 0;
		    }

		    if (code) {
			for (j++; j <= high; j++) {
			    if (dcList[j]) {
				ReleaseSharedLock(&(dcList[j]->lock));
				afs_PutDCache(dcList[j]);
				/* Releasing entry */
				dcList[j] = NULL;
			    }
			}
		    }

		    afs_Trace2(afs_iclSetp, CM_TRACE_STOREALLDCDONE,
			       ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code);
		    bytes = 0;
		}
	    }

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

    UpgradeSToWLock(&avc->lock, 29);

    /* send a trivial truncation store if did nothing else */
    if (code == 0) {
	/*
	 * Call StoreMini if we haven't written enough data to extend the
	 * file at the fileserver to the client's notion of the file length.
	 */
	if ((avc->truncPos != AFS_NOTRUNC) || ((avc->states & CExtendedFile)
					       && (maxStoredLength <
						   avc->m.Length))) {
	    code = afs_StoreMini(avc, areq);
	    if (code == 0)
		hadd32(newDV, 1);	/* just bumped here, too */
	}
	avc->states &= ~CExtendedFile;
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
	    memset((char *)dcList, 0,
		   NCHUNKSATONCE * sizeof(struct dcache *));

	    /* overkill, but it gets the lock in case GetDSlot needs it */
	    MObtainWriteLock(&afs_xdcache, 285);

	    for (j = 0, safety = 0, index = afs_dvhashTbl[hash];
		 index != NULLIDX && safety < afs_cacheFiles + 2;) {

		if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
		    tdc = afs_GetDSlot(index, 0);
		    ReleaseReadLock(&tdc->tlock);

		    if (!FidCmp(&tdc->f.fid, &avc->fid)
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

		index = afs_dvnextTbl[index];
	    }
	    MReleaseWriteLock(&afs_xdcache);

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
			 && hsame(avc->m.DataVersion, newDV))
			|| ((afs_dvhack || foreign)
			    && (origCBs == afs_allCBs))) {
			/* no error, this is the DV */

			UpgradeSToWLock(&tdc->lock, 678);
			hset(tdc->f.versionNo, avc->m.DataVersion);
			tdc->dflags |= DFEntryMod;
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
	if (areq->permWriteError || (avc->states & (CCore1 | CCore))) {
	    afs_InvalidateAllSegments(avc);
	}
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_STOREALLDONE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length, ICL_TYPE_INT32, code);
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
	if ((!(afs_dvhack || foreign) && hsame(avc->m.DataVersion, newDV))
	    || ((afs_dvhack || foreign) && (origCBs == afs_allCBs))) {
	    hset(avc->mapDV, newDV);
	    avc->states &= ~CDirty;
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
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->m.Length));
    hash = DVHash(&avc->fid);
    avc->truncPos = AFS_NOTRUNC;	/* don't truncate later */
    avc->states &= ~CExtendedFile;	/* not any more */
    ObtainWriteLock(&afs_xcbhash, 459);
    afs_DequeueCallback(avc);
    avc->states &= ~(CStatd | CDirty);	/* mark status information as bad, too */
    ReleaseWriteLock(&afs_xcbhash);
    if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
	osi_dnlc_purgedp(avc);
    /* Blow away pages; for now, only for Solaris */
#if	(defined(AFS_SUN5_ENV))
    if (WriteLocked(&avc->lock))
	osi_ReleaseVM(avc, (struct AFS_UCRED *)0);
#endif
    /*
     * Block out others from screwing with this table; is a read lock
     * sufficient?
     */
    MObtainWriteLock(&afs_xdcache, 286);
    dcListMax = 0;

    for (index = afs_dvhashTbl[hash]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	    tdc = afs_GetDSlot(index, 0);
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->fid))
		dcListMax++;
	    afs_PutDCache(tdc);
	}
	index = afs_dvnextTbl[index];
    }

    dcList = osi_Alloc(dcListMax * sizeof(struct dcache *));
    dcListCount = 0;

    for (index = afs_dvhashTbl[hash]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	    tdc = afs_GetDSlot(index, 0);
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->fid)) {
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
    MReleaseWriteLock(&afs_xdcache);

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
afs_TruncateAllSegments(register struct vcache *avc, afs_size_t alen,
			struct vrequest *areq, struct AFS_UCRED *acred)
{
    register struct dcache *tdc;
    register afs_int32 code;
    register afs_int32 index;
    afs_int32 newSize;

    int dcCount, dcPos;
    struct dcache **tdcArray;

    AFS_STATCNT(afs_TruncateAllSegments);
    avc->m.Date = osi_Time();
    afs_Trace3(afs_iclSetp, CM_TRACE_TRUNCALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->m.Length),
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(alen));
    if (alen >= avc->m.Length) {
	/*
	 * Special speedup since Sun's vm extends the file this way;
	 * we've never written to the file thus we can just set the new
	 * length and avoid the needless calls below.
	 * Also used for ftruncate calls which can extend the file.
	 * To completely minimize the possible extra StoreMini RPC, we really
	 * should keep the ExtendedPos as well and clear this flag if we
	 * truncate below that value before we store the file back.
	 */
	avc->states |= CExtendedFile;
	avc->m.Length = alen;
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

    avc->m.Length = alen;

    if (alen < avc->truncPos)
	avc->truncPos = alen;
    code = DVHash(&avc->fid);

    /* block out others from screwing with this table */
    MObtainWriteLock(&afs_xdcache, 287);

    dcCount = 0;
    for (index = afs_dvhashTbl[code]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	    tdc = afs_GetDSlot(index, 0);
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->fid))
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

    for (index = afs_dvhashTbl[code]; index != NULLIDX;) {
	if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	    tdc = afs_GetDSlot(index, 0);
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->fid)) {
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
	index = afs_dvnextTbl[index];
    }

    MReleaseWriteLock(&afs_xdcache);

    /* Now we loop over the array of dcache entries and truncate them */
    for (index = 0; index < dcPos; index++) {
	struct osi_file *tfile;

	tdc = tdcArray[index];

	newSize = alen - AFS_CHUNKTOBASE(tdc->f.chunk);
	if (newSize < 0)
	    newSize = 0;
	ObtainSharedLock(&tdc->lock, 672);
	if (newSize < tdc->f.chunkBytes) {
	    UpgradeSToWLock(&tdc->lock, 673);
	    tfile = afs_CFileOpen(tdc->f.inode);
	    afs_CFileTruncate(tfile, newSize);
	    afs_CFileClose(tfile);
	    afs_AdjustSize(tdc, newSize);
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

    osi_Free(tdcArray, dcCount * sizeof(struct dcache *));

#if	(defined(AFS_SUN5_ENV))
    ObtainWriteLock(&avc->vlock, 547);
    if (--avc->activeV == 0 && (avc->vstates & VRevokeWait)) {
	avc->vstates &= ~VRevokeWait;
	afs_osi_Wakeup((char *)&avc->vstates);
    }
    ReleaseWriteLock(&avc->vlock);
#endif
    return 0;
}
