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
#include "../afs/param.h"       /*Should be always first*/
#include "../afs/sysincludes.h" /*Standard vendor system headers*/
#include "../afs/afsincludes.h" /*AFS-based standard headers*/
#include "../afs/afs_stats.h"  /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/afs_osidnlc.h"



/* Imported variables */
extern afs_rwlock_t afs_xserver;
extern afs_rwlock_t afs_xdcache;
extern afs_rwlock_t afs_xcbhash;
extern afs_lock_t afs_ftf;
extern struct server *afs_servers[NSERVERS];
extern afs_int32 afs_dhashsize;
extern afs_int32 *afs_dvhashTbl;
extern unsigned char *afs_indexFlags;	/*(only one) Is there data there?*/
extern int cacheDiskType;

afs_uint32 afs_stampValue=0;

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

int afs_StoreMini(avc, areq)
    register struct vcache *avc;
    struct vrequest *areq;

{ /*afs_StoreMini*/
    register struct conn *tc;
    struct AFSStoreStatus InStatus;
    struct AFSFetchStatus OutStatus;
    struct AFSVolSync tsync;
    register afs_int32 code;
    register struct rx_call *tcall;
    afs_int32 tlen;
    XSTATS_DECLS

    AFS_STATCNT(afs_StoreMini);
    afs_Trace2(afs_iclSetp, CM_TRACE_STOREMINI, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length);
    tlen = avc->m.Length;
    if (avc->truncPos < tlen) tlen = avc->truncPos;
    avc->truncPos = AFS_NOTRUNC;
    avc->states &= ~CExtendedFile;

    do {
	tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	if (tc) {
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    tcall = rx_NewCall(tc->id);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
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
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = StartRXAFS_StoreData(tcall,
					(struct AFSFid *)&avc->fid.Fid,
					&InStatus, avc->m.Length, 0, tlen);
	    if (code == 0) {
		code = EndRXAFS_StoreData(tcall, &OutStatus, &tsync);
	    }
	    code = rx_EndCall(tcall, code);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	    XSTATS_END_TIME;
	}
	else code = -1;
    } while
	(afs_Analyze(tc, code, &avc->fid, areq,
		     AFS_STATS_FS_RPCIDX_STOREDATA,
		     SHARED_LOCK, (struct cell *)0));

    if (code == 0) {
	afs_ProcessFS(avc, &OutStatus, areq);
    }
    else {
	/* blew it away */
	afs_InvalidateAllSegments(avc, 1);
    }
    return code;

} /*afs_StoreMini*/

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
#if defined (AFS_HPUX_ENV) || defined(AFS_ULTRIX_ENV)
int NCHUNKSATONCE = 3;
#else
int NCHUNKSATONCE = 64 ;
#endif
int afs_dvhack=0;


afs_StoreAllSegments(avc, areq, sync)
    register struct vcache *avc;
    struct vrequest *areq;
    int sync;

{ /*afs_StoreAllSegments*/
    register struct dcache *tdc;
    register afs_int32 code=0;
    register afs_int32 index;
    register afs_int32 origCBs, foreign=0;
    int hash, stored;
    afs_hyper_t newDV, oldDV;	/* DV when we start, and finish, respectively */
    struct dcache **dcList, **dclist;
    unsigned int i, j, minj, maxj, moredata, high, off;
    unsigned long tlen;
    int safety;
    int maxStoredLength; /* highest offset we've written to server. */
#ifndef AFS_NOSTATS
    struct afs_stats_xferData *xferP;	/* Ptr to this op's xfer struct */
    osi_timeval_t  xferStartTime,	/*FS xfer start time*/
                   xferStopTime;	/*FS xfer stop time*/
    afs_int32 bytesToXfer;			/* # bytes to xfer*/
    afs_int32 bytesXferred;			/* # bytes actually xferred*/
#endif /* AFS_NOSTATS */


    AFS_STATCNT(afs_StoreAllSegments);
 
    hset(oldDV, avc->m.DataVersion);
    hset(newDV, avc->m.DataVersion);
    hash = DVHash(&avc->fid);
    foreign = (avc->states & CForeign);
    dcList = (struct dcache **) osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    afs_Trace2(afs_iclSetp, CM_TRACE_STOREALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length);
#ifndef AFS_AIX32_ENV
    /* In the aix vm implementation we need to do the vm_writep even
     * on the memcache case since that's we adjust the file's size
     * and finish flushing partial vm pages.
     */
    if (cacheDiskType != AFS_FCACHE_TYPE_MEM) 
#endif /* AFS_AIX32_ENV */
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
	if ( sync & AFS_VMSYNC_INVAL) /* invalidate VM pages */
	    osi_VM_TryToSmush(avc, CRED() , 1 );
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

  retry:
    maxStoredLength = 0;
    tlen = avc->m.Length;
    minj = 0 ; 

    do {
      bzero ((char *)dcList, NCHUNKSATONCE * sizeof(struct dcache *));
      high = 0;
      moredata = FALSE;

      /* lock and start over from beginning of hash chain 
       * in order to avoid a race condition. */
      MObtainWriteLock(&afs_xdcache,284);  
      index = afs_dvhashTbl[hash];
    
      for(j=0; index != NULLIDX;) {
	if ((afs_indexFlags[index] & IFDataMod) &&
	    (afs_indexUnique[index] == avc->fid.Fid.Unique)) {
	  tdc = afs_GetDSlot(index, 0);  /* refcount+1. */
	  if (!FidCmp( &tdc->f.fid, &avc->fid ) && tdc->f.chunk >= minj ) {

	    off = tdc->f.chunk - minj;
	    if (off < NCHUNKSATONCE) {
	      if ( dcList[ off ] )
		osi_Panic("dclist slot already in use!");
	      dcList[ off ] = tdc;
	      if (off > high) 
		high = off;
	      tlen -= tdc->f.chunkBytes; /* shortcut: big win for little files */
	      j++;
	      if (tlen <= 0)
		break;
	    }
	    else {
	      moredata = TRUE;
	      lockedPutDCache(tdc);
	      if (j == NCHUNKSATONCE)
		break;
	    }
	  } else {
	    lockedPutDCache(tdc);
	  }
	}
	index = afs_dvnextTbl[index];
      }
    
      MReleaseWriteLock(&afs_xdcache);
      /* this guy writes chunks, puts back dcache structs, and bumps newDV */
      /* "moredata" just says "there are more dirty chunks yet to come".
       */
      if (j) {
	static afs_uint32 lp1 = 10000, lp2 = 10000;
	struct AFSStoreStatus InStatus;
	afs_uint32 base, bytes, nchunks;
	int nomore;
	unsigned int first;
	int *shouldwake;
	struct conn * tc;
	struct osi_file * tfile;
	struct rx_call * tcall;
	extern int afs_defaultAsynchrony;
	XSTATS_DECLS

	for (bytes = 0, j = 0; !code && j<=high; j++) {
	  if (dcList[j]) {
	    if (!bytes)
	      first = j;
	    bytes += dcList[j]->f.chunkBytes;
	    if ((dcList[j]->f.chunkBytes < afs_OtherCSize)
		&& (dcList[j]->f.chunk - minj < high)
		&& dcList[j+1]) {
		int sbytes = afs_OtherCSize - dcList[j]->f.chunkBytes;
		bytes += sbytes;



	    }
	  }	    
	  if (bytes && (j==high || !dcList[j+1])) {
	    /* base = AFS_CHUNKTOBASE(dcList[first]->f.chunk); */
	    base = AFS_CHUNKTOBASE(first + minj) ;
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
	    nchunks = 1+j-first;
	    nomore = !(moredata || (j!=high));
	    InStatus.ClientModTime = avc->m.Date;
	    InStatus.Mask = AFS_SETMODTIME;
	    if (sync & AFS_SYNC) {
		InStatus.Mask |= AFS_FSYNC;
	    }
	    tlen = lmin(avc->m.Length, avc->truncPos);

	    do {
		stored = 0;
		tc = afs_Conn(&avc->fid, areq);
		if (tc) {
#ifdef RX_ENABLE_LOCKS
		    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		    tcall = rx_NewCall(tc->id);
		    code = StartRXAFS_StoreData(tcall, (struct AFSFid *) &avc->fid.Fid,
						&InStatus, base, bytes, tlen);
#ifdef RX_ENABLE_LOCKS
		    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		} else {
		    code = -1;
		    tcall = NULL;
		}
		if (!code) {
		    XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_STOREDATA);
		    avc->truncPos = AFS_NOTRUNC;
		}
		for (i = 0; i<nchunks && !code;i++) {
		    tdc = dclist[i];
		    if (!tdc) {
			afs_warn("afs: missing dcache!\n");
			storeallmissing++;
			continue; /* panic? */
		    }
  		    shouldwake = 0;
  		    if (nomore) {
 		       if (avc->asynchrony == -1) {
 			  if (afs_defaultAsynchrony > (bytes-stored)) {
 			     shouldwake = &nomore;
 			  }
 		       } else if ((afs_uint32)avc->asynchrony >= (bytes-stored)) {
 			  shouldwake = &nomore;
  		       }
  		    }
		    tfile = afs_CFileOpen(tdc->f.inode);
#ifndef AFS_NOSTATS
		    xferP = &(afs_stats_cmfullperf.rpc.fsXferTimes[AFS_STATS_FS_XFERIDX_STOREDATA]);
		    osi_GetuTime(&xferStartTime);

		    code = afs_CacheStoreProc(tcall, tfile, tdc->f.chunkBytes,
					      avc, shouldwake, &bytesToXfer,
					      &bytesXferred);

		    osi_GetuTime(&xferStopTime);
		    (xferP->numXfers)++;
		    if (!code) {
			(xferP->numSuccesses)++;
			afs_stats_XferSumBytes[AFS_STATS_FS_XFERIDX_STOREDATA] += bytesXferred;
			(xferP->sumBytes) += (afs_stats_XferSumBytes[AFS_STATS_FS_XFERIDX_STOREDATA] >> 10);
			afs_stats_XferSumBytes[AFS_STATS_FS_XFERIDX_STOREDATA] &= 0x3FF;
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
		        else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET1)
			        (xferP->count[1])++;
		        else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET2)
			        (xferP->count[2])++;
		        else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET3)
			        (xferP->count[3])++;
		        else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET4)
			        (xferP->count[4])++;
		        else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET5)
                                (xferP->count[5])++;
			else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET6)
			        (xferP->count[6])++;
			else
			    if (bytesToXfer <= AFS_STATS_MAXBYTES_BUCKET7)
			        (xferP->count[7])++;
			else
			    (xferP->count[8])++;

		        afs_stats_GetDiff(elapsedTime, xferStartTime, xferStopTime);
		        afs_stats_AddTo((xferP->sumTime), elapsedTime);
                        afs_stats_SquareAddTo((xferP->sqrTime), elapsedTime);
		        if (afs_stats_TimeLessThan(elapsedTime, (xferP->minTime))) {
			   afs_stats_TimeAssign((xferP->minTime), elapsedTime);
		        }
		        if (afs_stats_TimeGreaterThan(elapsedTime, (xferP->maxTime))) {
			   afs_stats_TimeAssign((xferP->maxTime), elapsedTime);
		        }
		      }
#else
		    code = afs_CacheStoreProc(tcall, tfile, tdc->f.chunkBytes, avc, 
					      shouldwake, &lp1, &lp2);
#endif /* AFS_NOSTATS */
		    afs_CFileClose(tfile);
		    if ((tdc->f.chunkBytes < afs_OtherCSize) && 
			(i < (nchunks-1))) {
                       int bsent, tlen, tlen1=0, sbytes = afs_OtherCSize - tdc->f.chunkBytes;
                       char *tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
 
                       while (sbytes > 0) {
                           tlen = (sbytes > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : sbytes);
                           bzero(tbuffer, tlen);
#ifdef RX_ENABLE_LOCKS
			   AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
			   bsent = rx_Write(tcall, tbuffer, tlen);
#ifdef RX_ENABLE_LOCKS
			   AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */

                           if (bsent != tlen) {
                               code = -33;     /* XXX */
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
		    struct AFSFetchStatus OutStatus;
		    struct AFSVolSync tsync;
#ifdef RX_ENABLE_LOCKS
		    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		    code = EndRXAFS_StoreData(tcall, &OutStatus, &tsync);
#ifdef RX_ENABLE_LOCKS
		    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		    hadd32(newDV, 1);
		    XSTATS_END_TIME;
      
		    /* Now copy out return params */
		    UpgradeSToWLock(&avc->lock,28);    /* keep out others for a while */
		    if (!code) {  /* must wait til RPC completes to be sure of this info */
			afs_ProcessFS(avc, &OutStatus, areq);
			/* Keep last (max) size of file on server to see if
			 * we need to call afs_StoreMini to extend the file.
			 */
			if (!moredata)
			    maxStoredLength = OutStatus.Length;

		    }
		    ConvertWToSLock(&avc->lock);
		}
		if (tcall) {
#ifdef RX_ENABLE_LOCKS
		    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		    code = rx_EndCall(tcall, code, avc, base);  
#ifdef RX_ENABLE_LOCKS
		    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		}
	    } while (afs_Analyze(tc, code, &avc->fid, areq,
				 AFS_STATS_FS_RPCIDX_STOREDATA,
				 SHARED_LOCK, (struct cell *)0));

	    /* put back all remaining locked dcache entries */  
	    for (i=0; i<nchunks; i++) {
		tdc = dclist[i];
		if (!code) {
		    if (afs_indexFlags[tdc->index] & IFDataMod) {
			afs_indexFlags[tdc->index] &= ~IFDataMod;
			afs_stats_cmperf.cacheCurrDirtyChunks--;
			afs_indexFlags[tdc->index] &= ~IFDirtyPages;
                        if ( sync & AFS_VMSYNC_INVAL )
                        {
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
		tdc->f.states &= ~DWriting;  /* correct?*/
		tdc->flags |= DFEntryMod;
		lockedPutDCache(tdc);
	    }

	    if (code) {
		for (j++; j<=high; j++)
		    if ( dcList[j] )
		    	lockedPutDCache(dcList[j]);
	    }

	    afs_Trace2(afs_iclSetp, CM_TRACE_STOREALLDCDONE,
		       ICL_TYPE_POINTER, avc, ICL_TYPE_INT32, code);
	    bytes = 0;
	  }
	}
      } /* if (j) */

    minj += NCHUNKSATONCE;
    } while ( !code && moredata ); 
    
    UpgradeSToWLock(&avc->lock,29);

  /* send a trivial truncation store if did nothing else */
  if (code == 0) {
    /*
     * Call StoreMini if we haven't written enough data to extend the
     * file at the fileserver to the client's notion of the file length.
     */
    if ((avc->truncPos != AFS_NOTRUNC) ||
	((avc->states & CExtendedFile) && (maxStoredLength < avc->m.Length))) {
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
      MObtainWriteLock(&afs_xdcache,285);  /* overkill, but it gets the 
					* lock in case GetDSlot needs it */
      for(safety = 0, index = afs_dvhashTbl[hash]; 
	  index != NULLIDX && safety < afs_cacheFiles+2;) {

	if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	  tdc = afs_GetDSlot(index, 0);

	  if (!FidCmp(&tdc->f.fid, &avc->fid)) {
	    /* this is the file */
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
	    if (code == 0 && (!hsame(tdc->f.versionNo, h_unset))
	        && (hcmp(tdc->f.versionNo, oldDV) >= 0)) {
	      if ((!(afs_dvhack || foreign) && hsame(avc->m.DataVersion, newDV))
		  || ((afs_dvhack || foreign) && (origCBs == afs_allCBs)) ) {
		  /* no error, this is the DV */
	        hset(tdc->f.versionNo, avc->m.DataVersion);
	        tdc->flags |= DFEntryMod;
	      }
	    }
	  }
	  lockedPutDCache(tdc);
	}
	index = afs_dvnextTbl[index];
      }
      MReleaseWriteLock(&afs_xdcache);
    }

    if (code) {
	/*
	 * Invalidate chunks after an error for ccores files since
	 * afs_inactive won't be called for these and they won't be
	 * invalidated. Also discard data if it's a permanent error from the
	 * fileserver.
	 */
	if (areq->permWriteError || (avc->states & (CCore1 | CCore))) {
	    afs_InvalidateAllSegments(avc, 1);
	}
    }
    afs_Trace3(afs_iclSetp, CM_TRACE_STOREALLDONE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length, ICL_TYPE_INT32, code);
    /* would like a Trace5, but it doesn't exist...*/
    afs_Trace3(afs_iclSetp, CM_TRACE_AVCLOCKER, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->lock.wait_states, 
	       ICL_TYPE_INT32, avc->lock.excl_locked);
    afs_Trace4(afs_iclSetp, CM_TRACE_AVCLOCKEE, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->lock.wait_states, 
	       ICL_TYPE_INT32, avc->lock.readers_reading, 
	       ICL_TYPE_INT32, avc->lock.num_waiting );
  
    /*
     * Finally, if updated DataVersion matches newDV, we did all of the
     * stores.  If mapDV indicates that the page cache was flushed up
     * to when we started the store, then we can relabel them as flushed
     * as recently as newDV.
     * Turn off CDirty bit because the stored data is now in sync with server.
     */
    if (code == 0 && hcmp(avc->mapDV, oldDV) >= 0) {
      if ((!(afs_dvhack || foreign) && hsame(avc->m.DataVersion, newDV))
	  || ((afs_dvhack || foreign) && (origCBs == afs_allCBs)) ) {
	  hset(avc->mapDV, newDV);
	  avc->states &= ~CDirty;
      }
    }
    osi_FreeLargeSpace(dcList);

    /* If not the final write a temporary error is ok. */
    if (code && !areq->permWriteError && !(sync & AFS_LASTSTORE))
	code = 0;

    return code;
  
} /*afs_StoreAllSegments (new 03/02/94)*/


/*
 * afs_InvalidateAllSegments
 *
 * Description:
 *	Invalidates all chunks for a given file
 *
 * Parameters:
 *	avc      : Pointer to vcache entry.
 *	asetLock : If true, we are to set the afs_xdcache lock; otherwise,
 *		   the caller has already done it.
 *
 * Environment:
 *	For example, called after an error has been detected.  Called
 *	with avc write-locked.
 */
   
afs_InvalidateAllSegments(avc, asetLock)
    struct vcache *avc;
    int asetLock;

{ /*afs_InvalidateAllSegments*/

    struct dcache *tdc;
    afs_int32 hash;
    afs_int32 index;

    AFS_STATCNT(afs_InvalidateAllSegments);
    afs_Trace2(afs_iclSetp, CM_TRACE_INVALL, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length);
    hash = DVHash(&avc->fid);
    avc->truncPos = AFS_NOTRUNC;  /* don't truncate later */
    avc->states &= ~CExtendedFile; /* not any more */
    ObtainWriteLock(&afs_xcbhash, 459);
    afs_DequeueCallback(avc);
    avc->states &= ~(CStatd|CDirty);	  /* mark status information as bad, too */
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
    if (asetLock) MObtainWriteLock(&afs_xdcache,286);
    for(index = afs_dvhashTbl[hash]; index != NULLIDX;) {
      if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	tdc = afs_GetDSlot(index, 0);
	if (!FidCmp(&tdc->f.fid, &avc->fid)) {
	    /* same file? we'll zap it */
	    if (afs_indexFlags[index] & IFDataMod) {
		afs_stats_cmperf.cacheCurrDirtyChunks--;
		/* don't write it back */
		afs_indexFlags[index] &= ~IFDataMod;
	      }
	    afs_indexFlags[index] &= ~IFAnyPages;
	    ZapDCE(tdc);
	    if (vType(avc) == VDIR) {
		DZap(&tdc->f.inode);
	    }
	}
	lockedPutDCache(tdc);
      }
      index = afs_dvnextTbl[index];
    }
    if (asetLock) MReleaseWriteLock(&afs_xdcache);
    return 0;

} /*afs_InvalidateAllSegments*/


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
afs_TruncateAllSegments(avc, alen, areq, acred)
    afs_int32 alen;
    register struct vcache *avc;
    struct vrequest *areq;
    struct AFS_UCRED *acred;
{ /*afs_TruncateAllSegments*/

    register struct dcache *tdc;
    register afs_int32 code;
    register afs_int32 index;
    afs_int32 newSize;

    AFS_STATCNT(afs_TruncateAllSegments);
    avc->m.Date = osi_Time();
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
	afs_Trace3(afs_iclSetp, CM_TRACE_TRUNCALL1, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_INT32, avc->m.Length, ICL_TYPE_INT32, alen);
	return 0;
    }

    afs_Trace3(afs_iclSetp, CM_TRACE_TRUNCALL2, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length, ICL_TYPE_INT32, alen);

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
    ObtainWriteLock(&avc->lock,79);

    avc->m.Length = alen;
    
    if (alen < avc->truncPos) avc->truncPos = alen;
    code = DVHash(&avc->fid);
    /* block out others from screwing with this table */
    MObtainWriteLock(&afs_xdcache,287);
    for(index = afs_dvhashTbl[code]; index != NULLIDX;) {
      if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	tdc = afs_GetDSlot(index, 0);
	if (!FidCmp(&tdc->f.fid, &avc->fid)) {
	    /* same file, and modified, we'll store it back */
	    newSize = alen - AFS_CHUNKTOBASE(tdc->f.chunk);
	    if (newSize < 0) newSize = 0;
	    if (newSize < tdc->f.chunkBytes) {
		register struct osi_file *tfile;
		tfile = afs_CFileOpen(tdc->f.inode);
		afs_CFileTruncate(tfile, newSize);
		afs_CFileClose(tfile);
		afs_AdjustSize(tdc, newSize);
	    }
	}
	lockedPutDCache(tdc);
      }
      index = afs_dvnextTbl[index];
    }
#if	(defined(AFS_SUN5_ENV))
    ObtainWriteLock(&avc->vlock, 547);
    if (--avc->activeV == 0 && (avc->vstates & VRevokeWait)) {
	avc->vstates &= ~VRevokeWait;
	afs_osi_Wakeup((char *)&avc->vstates);
    }
    ReleaseWriteLock(&avc->vlock);
#endif
    MReleaseWriteLock(&afs_xdcache);
    return 0;

} /*afs_TruncateAllSegments*/


