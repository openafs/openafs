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
 */
#include "../afs/param.h"       /*Should be always first*/
#include "../afs/sysincludes.h" /*Standard vendor system headers*/
#include "../afs/afsincludes.h" /*AFS-based standard headers*/
#include "../afs/afs_stats.h"  /* statistics */
#include "../afs/afs_cbqueue.h"
#include "../afs/afs_osidnlc.h"

/* Forward declarations. */
static void afs_GetDownD(int anumber, int *aneedSpace);
static void afs_FreeDiscardedDCache(void);
static void afs_DiscardDCache(struct dcache *);

/* Imported variables */
extern afs_rwlock_t afs_xvcache;
extern afs_rwlock_t afs_xcbhash;
extern afs_int32 afs_mariner;
extern afs_int32 cacheInfoModTime;			/*Last time cache info modified*/


/*
 * --------------------- Exported definitions ---------------------
 */
afs_lock_t afs_xdcache;			/*Lock: alloc new disk cache entries*/
afs_int32 afs_freeDCList;			/*Free list for disk cache entries*/
afs_int32 afs_freeDCCount;			/*Count of elts in freeDCList*/
afs_int32 afs_discardDCList;		/*Discarded disk cache entries*/
afs_int32 afs_discardDCCount;		/*Count of elts in discardDCList*/
struct dcache *afs_freeDSList;		/*Free list for disk slots */
struct dcache *afs_Initial_freeDSList;	/*Initial list for above*/
ino_t cacheInode;			/*Inode for CacheItems file*/
struct osi_file *afs_cacheInodep = 0;	/* file for CacheItems inode */
struct afs_q afs_DLRU;			/*dcache LRU*/
afs_int32 afs_dhashsize = 1024;
afs_int32 *afs_dvhashTbl;			/*Data cache hash table*/
afs_int32 *afs_dchashTbl;			/*Data cache hash table*/
afs_int32 *afs_dvnextTbl;                    /*Dcache hash table links */
afs_int32 *afs_dcnextTbl;                    /*Dcache hash table links */
struct dcache **afs_indexTable;		/*Pointers to dcache entries*/
afs_hyper_t *afs_indexTimes;			/*Dcache entry Access times*/
afs_int32 *afs_indexUnique;                  /*dcache entry Fid.Unique */
unsigned char *afs_indexFlags;		/*(only one) Is there data there?*/
afs_hyper_t afs_indexCounter;			/*Fake time for marking index
					  entries*/
afs_int32 afs_cacheFiles =0;		/*Size of afs_indexTable*/
afs_int32 afs_cacheBlocks;			/*1K blocks in cache*/
afs_int32 afs_cacheStats;			/*Stat entries in cache*/
afs_int32 afs_blocksUsed;			/*Number of blocks in use*/
afs_int32 afs_blocksDiscarded;		/*Blocks freed but not truncated */
afs_int32 afs_fsfragsize = 1023;             /*Underlying Filesystem minimum unit 
					 *of disk allocation usually 1K
					 *this value is (truefrag -1 ) to
					 *save a bunch of subtracts... */

/* The following is used to ensure that new dcache's aren't obtained when
 * the cache is nearly full.
 */
int afs_WaitForCacheDrain = 0;
int afs_TruncateDaemonRunning = 0;
int afs_CacheTooFull = 0;

afs_int32  afs_dcentries;			/* In-memory dcache entries */


int dcacheDisabled = 0;

extern struct dcache *afs_UFSGetDSlot();
extern struct volume *afs_UFSGetVolSlot();
extern int osi_UFSTruncate(), afs_osi_Read(), afs_osi_Write(), osi_UFSClose();
extern int afs_UFSRead(), afs_UFSWrite();
static int afs_UFSCacheFetchProc(), afs_UFSCacheStoreProc();
extern int afs_UFSHandleLink();
struct afs_cacheOps afs_UfsCacheOps = {
    osi_UFSOpen,
    osi_UFSTruncate,
    afs_osi_Read,
    afs_osi_Write,
    osi_UFSClose,
    afs_UFSRead,
    afs_UFSWrite,
    afs_UFSCacheFetchProc,
    afs_UFSCacheStoreProc,
    afs_UFSGetDSlot,
    afs_UFSGetVolSlot,
    afs_UFSHandleLink,
};

extern void *afs_MemCacheOpen();
extern struct dcache *afs_MemGetDSlot();
extern struct volume *afs_MemGetVolSlot();
extern int afs_MemCacheTruncate(), afs_MemReadBlk(), afs_MemWriteBlk(), afs_MemCacheClose();
extern int afs_MemRead(), afs_MemWrite(), afs_MemCacheFetchProc(), afs_MemCacheStoreProc();
extern int afs_MemHandleLink();
struct afs_cacheOps afs_MemCacheOps = {
    afs_MemCacheOpen,
    afs_MemCacheTruncate,
    afs_MemReadBlk,
    afs_MemWriteBlk,
    afs_MemCacheClose,
    afs_MemRead,
    afs_MemWrite,
    afs_MemCacheFetchProc,
    afs_MemCacheStoreProc,
    afs_MemGetDSlot,
    afs_MemGetVolSlot,
    afs_MemHandleLink,
};

int cacheDiskType;			/*Type of backing disk for cache*/
struct afs_cacheOps *afs_cacheType;




/*
 * afs_StoreWarn
 *
 * Description:
 *	Warn about failing to store a file.
 *
 * Parameters:
 *	acode   : Associated error code.
 *	avolume : Volume involved.
 *	aflags  : How to handle the output:
 *			aflags & 1: Print out on console
 *			aflags & 2: Print out on controlling tty
 *
 * Environment:
 *	Call this from close call when vnodeops is RCS unlocked.
 */

void
afs_StoreWarn(acode, avolume, aflags)
    register afs_int32 acode;
    afs_int32 avolume;
    register afs_int32 aflags;

{ /*afs_StoreWarn*/

    static char problem_fmt[] =
	"afs: failed to store file in volume %d (%s)\n";
    static char problem_fmt_w_error[] =
	"afs: failed to store file in volume %d (error %d)\n";
    static char netproblems[] = "network problems";
    static char partfull[]    = "partition full";
    static char overquota[]   = "over quota";
    static char unknownerr[]  = "unknown error";

    AFS_STATCNT(afs_StoreWarn);
    if (acode < 0) {
        /*
	 * Network problems
	 */
        if (aflags & 1)
	    afs_warn(problem_fmt, avolume, netproblems);
        if (aflags & 2)
	    afs_warnuser(problem_fmt, avolume, netproblems);
    }
    else
	if (acode == ENOSPC) {
	    /*
	     * Partition full
	     */
	    if (aflags & 1)
		afs_warn(problem_fmt, avolume, partfull);
	    if (aflags & 2)
		afs_warnuser(problem_fmt, avolume, partfull);
	}
	else
#ifndef	AFS_SUN5_ENV
	    /* EDQUOT doesn't exist on solaris and won't be sent by the server.
	     * Instead ENOSPC will be sent...
	     */
	    if (acode == EDQUOT) {
		/*
		 * Quota exceeded
		 */
		if (aflags & 1)
		    afs_warn(problem_fmt, avolume, overquota);
		if (aflags & 2)
		    afs_warnuser(problem_fmt, avolume, overquota);
	    } else
#endif
	    {
		/*
		 * Unknown error
		 */
		if (aflags & 1)
		    afs_warn(problem_fmt_w_error, avolume, acode);
		if (aflags & 2)
		    afs_warnuser(problem_fmt_w_error, avolume, acode);
	    }
} /*afs_StoreWarn*/

/* Keep statistics on run time for afs_CacheTruncateDaemon. This is a
 * struct so we need only export one symbol for AIX.
 */
struct CTD_stats {
    osi_timeval_t CTD_beforeSleep;
    osi_timeval_t CTD_afterSleep;
    osi_timeval_t CTD_sleepTime;
    osi_timeval_t CTD_runTime;
    int CTD_nSleeps;
} CTD_stats;

u_int afs_min_cache = 0;
void afs_CacheTruncateDaemon() {
    osi_timeval_t CTD_tmpTime;
    u_int counter;
    u_int cb_lowat;
    u_int dc_hiwat = (100-CM_DCACHECOUNTFREEPCT+CM_DCACHEEXTRAPCT)*afs_cacheFiles/100;
    afs_min_cache = (((10 * AFS_CHUNKSIZE(0)) + afs_fsfragsize) & ~afs_fsfragsize)>>10;

    osi_GetuTime(&CTD_stats.CTD_afterSleep);
    afs_TruncateDaemonRunning = 1;
    while (1) {
	cb_lowat =  ((CM_DCACHESPACEFREEPCT-CM_DCACHEEXTRAPCT)
		     * afs_cacheBlocks) / 100;
	MObtainWriteLock(&afs_xdcache,266);
	if (afs_CacheTooFull) {
	  int space_needed, slots_needed;
	    /* if we get woken up, we should try to clean something out */
	    for (counter = 0; counter < 10; counter++) {
	        space_needed = afs_blocksUsed - afs_blocksDiscarded - cb_lowat;
		slots_needed = dc_hiwat - afs_freeDCCount - afs_discardDCCount;
		afs_GetDownD(slots_needed, &space_needed);	
		if ((space_needed <= 0) && (slots_needed <= 0)) {
		    break;
		}
		if (afs_termState == AFSOP_STOP_TRUNCDAEMON)
		    break;
	    }
	    if (!afs_CacheIsTooFull())
		afs_CacheTooFull = 0;
	}
	MReleaseWriteLock(&afs_xdcache);

	/*
	 * This is a defensive check to try to avoid starving threads
	 * that may need the global lock so thay can help free some
	 * cache space. If this thread won't be sleeping or truncating
	 * any cache files then give up the global lock so other
	 * threads get a chance to run.
	 */
	if ((afs_termState!=AFSOP_STOP_TRUNCDAEMON) && afs_CacheTooFull &&
	    (!afs_blocksDiscarded || afs_WaitForCacheDrain)) {
	    afs_osi_Wait(100, 0, 0); /* 100 milliseconds */
	}

	/*
	 * This is where we free the discarded cache elements.
	 */
	while(afs_blocksDiscarded && !afs_WaitForCacheDrain && 
	      (afs_termState!=AFSOP_STOP_TRUNCDAEMON))
	{
	    afs_FreeDiscardedDCache();
	}

	/* See if we need to continue to run. Someone may have
	 * signalled us while we were executing.
	 */
	if (!afs_WaitForCacheDrain && !afs_CacheTooFull &&
			(afs_termState!=AFSOP_STOP_TRUNCDAEMON))
	{
	    /* Collect statistics on truncate daemon. */
	    CTD_stats.CTD_nSleeps++;
	    osi_GetuTime(&CTD_stats.CTD_beforeSleep);
	    afs_stats_GetDiff(CTD_tmpTime, CTD_stats.CTD_afterSleep,
			      CTD_stats.CTD_beforeSleep);
	    afs_stats_AddTo(CTD_stats.CTD_runTime, CTD_tmpTime);

	    afs_TruncateDaemonRunning = 0;
	    afs_osi_Sleep((char *)afs_CacheTruncateDaemon);  
	    afs_TruncateDaemonRunning = 1;

	    osi_GetuTime(&CTD_stats.CTD_afterSleep);
	    afs_stats_GetDiff(CTD_tmpTime, CTD_stats.CTD_beforeSleep,
			      CTD_stats.CTD_afterSleep);
	    afs_stats_AddTo(CTD_stats.CTD_sleepTime, CTD_tmpTime);
	}
	if (afs_termState == AFSOP_STOP_TRUNCDAEMON) {
	    afs_termState = AFSOP_STOP_RXEVENT;
	    afs_osi_Wakeup(&afs_termState);
	    break;
	}
    }
}


/*
 * afs_AdjustSize
 *
 * Description:
 * 	Make adjustment for the new size in the disk cache entry
 *
 * Major Assumptions Here:
 *      Assumes that frag size is an integral power of two, less one,
 *      and that this is a two's complement machine.  I don't
 *      know of any filesystems which violate this assumption...
 *
 * Parameters:
 *	adc	 : Ptr to dcache entry.
 *	anewsize : New size desired.
 */

void
afs_AdjustSize(adc, anewSize)
    register struct dcache *adc;
    register afs_int32 anewSize;

{ /*afs_AdjustSize*/

    register afs_int32 oldSize;

    AFS_STATCNT(afs_AdjustSize);
    adc->flags |= DFEntryMod;
    oldSize = ((adc->f.chunkBytes + afs_fsfragsize)^afs_fsfragsize)>>10;/* round up */
    adc->f.chunkBytes = anewSize;
    anewSize = ((anewSize + afs_fsfragsize)^afs_fsfragsize)>>10;/* round up */
    if (anewSize > oldSize) {
	/* We're growing the file, wakeup the daemon */
	afs_MaybeWakeupTruncateDaemon();
    }
    afs_blocksUsed += (anewSize - oldSize);
    afs_stats_cmperf.cacheBlocksInUse = afs_blocksUsed;	/* XXX */

} /*afs_AdjustSize*/





/*
 * afs_GetDownD
 *
 * Description:
 *	This routine is responsible for moving at least one entry (but up
 *	to some number of them) from the LRU queue to the free queue.
 *
 * Parameters:
 *	anumber	   : Number of entries that should ideally be moved.
 *	aneedSpace : How much space we need (1K blocks);
 *
 * Environment:
 *	The anumber parameter is just a hint; at least one entry MUST be
 *	moved, of we'll panic.  We must be called with afs_xdcache
 *	write-locked.  We should try to satisfy both anumber and aneedspace,
 *      whichever is more demanding - need to do several things:
 *      1.  only grab up to anumber victims if aneedSpace <= 0, not
 *          the whole set of MAXATONCE.
 *      2.  dynamically choose MAXATONCE to reflect severity of
 *          demand: something like (*aneedSpace >> (logChunk - 9)) 
 *  N.B. if we're called with aneedSpace <= 0 and anumber > 0, that
 *  indicates that the cache is not properly configured/tuned or
 *  something. We should be able to automatically correct that problem.
 */

#define	MAXATONCE   16		/* max we can obtain at once */
static void afs_GetDownD(int anumber, int *aneedSpace)
{

    struct dcache *tdc;
    struct VenusFid *afid;
    afs_int32 i, j, k;
    afs_hyper_t vtime;
    int skip, phase;
    register struct vcache *tvc;
    afs_uint32 victims[MAXATONCE];
    struct dcache *victimDCs[MAXATONCE];
    afs_hyper_t victimTimes[MAXATONCE];/* youngest (largest LRU time) first */
    afs_uint32 victimPtr;	    /* next free item in victim arrays */
    afs_hyper_t maxVictimTime;	    /* youngest (largest LRU time) victim */
    afs_uint32 maxVictimPtr;		    /* where it is */
    int discard;

    AFS_STATCNT(afs_GetDownD);
    if (CheckLock(&afs_xdcache) != -1)
	osi_Panic("getdownd nolock");
    /* decrement anumber first for all dudes in free list */
    /* SHOULD always decrement anumber first, even if aneedSpace >0, 
     * because we should try to free space even if anumber <=0 */
    if (!aneedSpace || *aneedSpace <= 0) {
	anumber -= afs_freeDCCount;
	if (anumber <= 0) return;	/* enough already free */
    }
    /* bounds check parameter */
    if (anumber	> MAXATONCE)
	anumber = MAXATONCE;   /* all we can do */

    /*
     * The phase variable manages reclaims.  Set to 0, the first pass,
     * we don't reclaim active entries.  Set to 1, we reclaim even active
     * ones.
     */
    phase = 0;
    for (i = 0; i < afs_cacheFiles; i++)
	/* turn off all flags */
	afs_indexFlags[i] &= ~IFFlag;

    while (anumber > 0 || (aneedSpace && *aneedSpace >0)) {
	/* find oldest entries for reclamation */
	maxVictimPtr = victimPtr = 0;
	hzero(maxVictimTime);
	/* select victims from access time array */
	for (i = 0; i < afs_cacheFiles; i++) {
	    if (afs_indexFlags[i] & (IFDataMod | IFFree | IFDiscarded)) {
	      /* skip if dirty or already free */
	      continue;
	    }
	    tdc = afs_indexTable[i];
	    if (tdc && (tdc->refCount != 0)) {
	      /* Referenced; can't use it! */
	      continue;
	    }
	    hset(vtime, afs_indexTimes[i]);

	    /* if we've already looked at this one, skip it */
	    if (afs_indexFlags[i] & IFFlag) continue;

	    if (victimPtr < MAXATONCE) {
		/* if there's at least one free victim slot left */
		victims[victimPtr] = i;
		hset(victimTimes[victimPtr], vtime);
		if (hcmp(vtime, maxVictimTime) > 0) {
		    hset(maxVictimTime, vtime);
		    maxVictimPtr = victimPtr;
		}
		victimPtr++;
	    }
	    else if (hcmp(vtime, maxVictimTime) < 0) {
		/*
		 * We're older than youngest victim, so we replace at
		 * least one victim
		 */
		/* find youngest (largest LRU) victim */
	        j = maxVictimPtr;
		if (j == victimPtr) osi_Panic("getdownd local");
		victims[j] = i;
		hset(victimTimes[j], vtime);
		/* recompute maxVictimTime */
		hset(maxVictimTime, vtime);
		for(j = 0; j < victimPtr; j++)
		    if (hcmp(maxVictimTime, victimTimes[j]) < 0) {
			hset(maxVictimTime, victimTimes[j]);
			maxVictimPtr = j;
		    }
	    }
	} /* big for loop */

	/* now really reclaim the victims */
	j = 0;	/* flag to track if we actually got any of the victims */
	/* first, hold all the victims, since we're going to release the lock
	 * during the truncate operation.
	 */
	for(i=0; i < victimPtr; i++)
	    victimDCs[i] = afs_GetDSlot(victims[i], 0);
	for(i = 0; i < victimPtr; i++) {
	    /* q is first elt in dcache entry */
	    tdc = victimDCs[i];
	    /* now, since we're dropping the afs_xdcache lock below, we
	     * have to verify, before proceeding, that there are no other
	     * references to this dcache entry, even now.  Note that we
	     * compare with 1, since we bumped it above when we called
	     * afs_GetDSlot to preserve the entry's identity.
	     */
	    if (tdc->refCount == 1) {
		unsigned char chunkFlags;
                afid = &tdc->f.fid;
		/* xdcache is lower than the xvcache lock */
		MReleaseWriteLock(&afs_xdcache);
		MObtainReadLock(&afs_xvcache);
		tvc = afs_FindVCache(afid, 0,0, 0, 0 /* no stats, no vlru */ );
		MReleaseReadLock(&afs_xvcache);
		MObtainWriteLock(&afs_xdcache, 527);
		skip = 0;
		if (tdc->refCount > 1) skip = 1;
		if (tvc) {
		    chunkFlags = afs_indexFlags[tdc->index];
		    if (phase == 0 && osi_Active(tvc)) skip = 1;
                    if (phase > 0 && osi_Active(tvc) && (tvc->states & CDCLock)
                                && (chunkFlags & IFAnyPages)) skip = 1;
		    if (chunkFlags & IFDataMod) skip = 1;
		    afs_Trace4(afs_iclSetp, CM_TRACE_GETDOWND,
			       ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32, skip,
			       ICL_TYPE_INT32,
			       (afs_int32)(chunkFlags & IFDirtyPages),
			       ICL_TYPE_INT32, AFS_CHUNKTOBASE(tdc->f.chunk));

#if	defined(AFS_SUN5_ENV)
		    /*
		     * Now we try to invalidate pages.  We do this only for
		     * Solaris.  For other platforms, it's OK to recycle a
		     * dcache entry out from under a page, because the strategy
		     * function can call afs_GetDCache().
		     */
		    if (!skip && (chunkFlags & IFAnyPages)) {
			int code;

			MReleaseWriteLock(&afs_xdcache);
			MObtainWriteLock(&tvc->vlock, 543);
			if (tvc->multiPage) {
			    skip = 1;
			    goto endmultipage;
			}
			/* block locking pages */
			tvc->vstates |= VPageCleaning;
			/* block getting new pages */
			tvc->activeV++;
			MReleaseWriteLock(&tvc->vlock);
			/* One last recheck */
			MObtainWriteLock(&afs_xdcache, 333);
			chunkFlags = afs_indexFlags[tdc->index];
			if (tdc->refCount > 1
			    || (chunkFlags & IFDataMod)
			    || (osi_Active(tvc) && (tvc->states & CDCLock)
                                && (chunkFlags & IFAnyPages))) {
			    skip = 1;
			    MReleaseWriteLock(&afs_xdcache);
			    goto endputpage;
			}
			MReleaseWriteLock(&afs_xdcache);

			code = osi_VM_GetDownD(tvc, tdc);

			MObtainWriteLock(&afs_xdcache,269);
			/* we actually removed all pages, clean and dirty */
			if (code == 0) {
			    afs_indexFlags[tdc->index] &= ~(IFDirtyPages| IFAnyPages);
			} else
			    skip = 1;
			MReleaseWriteLock(&afs_xdcache);
endputpage:
			MObtainWriteLock(&tvc->vlock, 544);
			if (--tvc->activeV == 0 && (tvc->vstates & VRevokeWait)) {
			    tvc->vstates &= ~VRevokeWait;
			    afs_osi_Wakeup((char *)&tvc->vstates);

			}
			if (tvc->vstates & VPageCleaning) {
			    tvc->vstates &= ~VPageCleaning;
			    afs_osi_Wakeup((char *)&tvc->vstates);
			}
endmultipage:
			MReleaseWriteLock(&tvc->vlock);
		    } else
#endif	/* AFS_SUN5_ENV */
		    {
			MReleaseWriteLock(&afs_xdcache);
		    }

		    AFS_FAST_RELE(tvc);
		    MObtainWriteLock(&afs_xdcache, 528);
		    if (afs_indexFlags[tdc->index] &
			 (IFDataMod | IFDirtyPages | IFAnyPages)) skip = 1;
		    if (tdc->refCount > 1) skip = 1;
		}
#if	defined(AFS_SUN5_ENV)
		else {
		    /* no vnode, so IFDirtyPages is spurious (we don't
		     * sweep dcaches on vnode recycling, so we can have
		     * DIRTYPAGES set even when all pages are gone).  Just
		     * clear the flag.
		     * Hold vcache lock to prevent vnode from being
		     * created while we're clearing IFDirtyPages.
		     */
		    afs_indexFlags[tdc->index] &= ~(IFDirtyPages | IFAnyPages);
		}
#endif
		if (skip) {
		    /* skip this guy and mark him as recently used */
		    afs_indexFlags[tdc->index] |= IFFlag;
		}
		else {
		    /* flush this dude from the data cache and reclaim;
                     * first, make sure no one will care that we damage
                     * it, by removing it from all hash tables.  Then,
                     * melt it down for parts.  Note that any concurrent
                     * (new possibility!) calls to GetDownD won't touch
                     * this guy because his reference count is > 0. */
#ifndef	AFS_DEC_ENV
		    AFS_STATCNT(afs_gget);
#endif
		    afs_HashOutDCache(tdc);
		    if (tdc->f.chunkBytes != 0) {
			discard = 1;
			if (aneedSpace)
			  *aneedSpace -= (tdc->f.chunkBytes + afs_fsfragsize) >> 10;
		    } else {
			discard = 0;
		    }
		    if (discard) {
			afs_DiscardDCache(tdc);
		    } else {
			afs_FreeDCache(tdc);
		    }
		    anumber--;
		    j = 1;	/* we reclaimed at least one victim */
		}
	    }
#ifdef	AFS_SUN5_ENVX
	    afs_PutDCache(tdc);
#else
	    tdc->refCount--;		/* put it back */
#endif
	}
	
	if (phase == 0) {
	    /* Phase is 0 and no one was found, so try phase 1 (ignore
	     * osi_Active flag) */
	    if (j == 0) {
		phase = 1;
		for (i = 0; i < afs_cacheFiles; i++)
		    /* turn off all flags */
		    afs_indexFlags[i] &= ~IFFlag;
	    }
	}
	else {
	    /* found no one in phase 1, we're hosed */
	    if (victimPtr == 0) break;
	}
    } /* big while loop */
    return;

} /*afs_GetDownD*/


/*
 * Description: remove adc from any hash tables that would allow it to be located
 * again by afs_FindDCache or afs_GetDCache.
 *
 * Parameters: adc -- pointer to dcache entry to remove from hash tables.
 *
 * Locks: Must have the afs_xdcache lock write-locked to call this function.
 */
afs_HashOutDCache(adc)
    struct dcache *adc;

{ /*afs_HashOutDCache*/

    int i, us;

#ifndef	AFS_DEC_ENV
    AFS_STATCNT(afs_glink);
#endif
    /* we know this guy's in the LRUQ.  We'll move dude into DCQ below */
    DZap(&adc->f.inode);
    /* if this guy is in the hash table, pull him out */
    if (adc->f.fid.Fid.Volume != 0) {
	/* remove entry from first hash chains */
	i = DCHash(&adc->f.fid, adc->f.chunk);
	us = afs_dchashTbl[i];
	if (us == adc->index) {
	    /* first dude in the list */
	    afs_dchashTbl[i] = afs_dcnextTbl[adc->index];
	}
	else {
	    /* somewhere on the chain */
	    while (us != NULLIDX) {
		if (afs_dcnextTbl[us] == adc->index) {
		    /* found item pointing at the one to delete */
		    afs_dcnextTbl[us] = afs_dcnextTbl[adc->index];
		    break;
		}
		us = afs_dcnextTbl[us];
	    }
	    if (us == NULLIDX) osi_Panic("dcache hc");
	}
	/* remove entry from *other* hash chain */
	i = DVHash(&adc->f.fid);
	us = afs_dvhashTbl[i];
	if (us == adc->index) {
	    /* first dude in the list */
	    afs_dvhashTbl[i] = afs_dvnextTbl[adc->index];
	}
	else {
	    /* somewhere on the chain */
	    while (us != NULLIDX) {
		if (afs_dvnextTbl[us] == adc->index) {
		    /* found item pointing at the one to delete */
		    afs_dvnextTbl[us] = afs_dvnextTbl[adc->index];
		    break;
		}
		us = afs_dvnextTbl[us];
	    }
	    if (us == NULLIDX) osi_Panic("dcache hv");
	}
    }

    /* prevent entry from being found on a reboot (it is already out of
     * the hash table, but after a crash, we just look at fid fields of
     * stable (old) entries).
     */
    adc->f.fid.Fid.Volume = 0;	/* invalid */

    /* mark entry as modified */
    adc->flags |= DFEntryMod;

    /* all done */
    return 0;
} /*afs_HashOutDCache */


/*
 * afs_FlushDCache
 *
 * Description:
 *	Flush the given dcache entry, pulling it from hash chains
 *	and truncating the associated cache file.
 *
 * Arguments:
 *	adc: Ptr to dcache entry to flush.
 *
 * Environment:
 *	This routine must be called with the afs_xdcache lock held
 *	(in write mode)
 */

void
afs_FlushDCache(adc)
register struct dcache *adc;
{ /*afs_FlushDCache*/

    AFS_STATCNT(afs_FlushDCache);
    /*
     * Bump the number of cache files flushed.
     */
    afs_stats_cmperf.cacheFlushes++;

    /* remove from all hash tables */
    afs_HashOutDCache(adc);

    /* Free its space; special case null operation, since truncate operation
     * in UFS is slow even in this case, and this allows us to pre-truncate
     * these files at more convenient times with fewer locks set
     * (see afs_GetDownD).
     */
    if (adc->f.chunkBytes != 0) {
	afs_DiscardDCache(adc);
	afs_MaybeWakeupTruncateDaemon();
    } else {
	afs_FreeDCache(adc);
    }

    if (afs_WaitForCacheDrain) {
	if (afs_blocksUsed <=
	    (CM_CACHESIZEDRAINEDPCT*afs_cacheBlocks)/100) {
	    afs_WaitForCacheDrain = 0;
	    afs_osi_Wakeup(&afs_WaitForCacheDrain);
	}
    }
} /*afs_FlushDCache*/


/*
 * afs_FreeDCache
 *
 * Description: put a dcache entry on the free dcache entry list.
 *
 * Parameters: adc -- dcache entry to free
 *
 * Environment: called with afs_xdcache lock write-locked.
 */
afs_FreeDCache(adc)
register struct dcache *adc; {
    /* Thread on free list, update free list count and mark entry as
     * freed in its indexFlags element.  Also, ensure DCache entry gets
     * written out (set DFEntryMod).
     */

    afs_dvnextTbl[adc->index] = afs_freeDCList;
    afs_freeDCList = adc->index;
    afs_freeDCCount++;
    afs_indexFlags[adc->index] |= IFFree;
    adc->flags |= DFEntryMod;

    if (afs_WaitForCacheDrain) {
	if ((afs_blocksUsed - afs_blocksDiscarded) <=
	    (CM_CACHESIZEDRAINEDPCT*afs_cacheBlocks)/100) {
	    afs_WaitForCacheDrain = 0;
	    afs_osi_Wakeup(&afs_WaitForCacheDrain);
	}
    }
    return 0;
}

/*
 * afs_DiscardDCache
 *
 * Description:
 * 	Discard the cache element by moving it to the discardDCList.
 *      This puts the cache element into a quasi-freed state, where
 *      the space may be reused, but the file has not been truncated.
 *
 * Major Assumptions Here:
 *      Assumes that frag size is an integral power of two, less one,
 *      and that this is a two's complement machine.  I don't
 *      know of any filesystems which violate this assumption...
 *
 * Parameters:
 *	adc	 : Ptr to dcache entry.
 */

static void
afs_DiscardDCache(adc)
    register struct dcache *adc;

{ /*afs_DiscardDCache*/

    register afs_int32 size;

    AFS_STATCNT(afs_DiscardDCache);
    size = ((adc->f.chunkBytes + afs_fsfragsize)^afs_fsfragsize)>>10;/* round up */
    afs_blocksDiscarded += size;
    afs_stats_cmperf.cacheBlocksDiscarded = afs_blocksDiscarded;

    afs_dvnextTbl[adc->index] = afs_discardDCList;
    afs_discardDCList = adc->index;
    afs_discardDCCount++;

    adc->f.fid.Fid.Volume = 0;
    adc->flags |= DFEntryMod;
    afs_indexFlags[adc->index] |= IFDiscarded;

    if (afs_WaitForCacheDrain) {
	if ((afs_blocksUsed - afs_blocksDiscarded) <=
	    (CM_CACHESIZEDRAINEDPCT*afs_cacheBlocks)/100) {
	    afs_WaitForCacheDrain = 0;
	    afs_osi_Wakeup(&afs_WaitForCacheDrain);
	}
    }

} /*afs_DiscardDCache*/

/*
 * afs_FreeDiscardedDCache
 *
 * Description:
 *     Free the next element on the list of discarded cache elements.
 */
static void
afs_FreeDiscardedDCache()
{
    register struct dcache *tdc;
    register struct osi_file *tfile; 
    register afs_int32 size;

    AFS_STATCNT(afs_FreeDiscardedDCache);

    MObtainWriteLock(&afs_xdcache,510);
    if (!afs_blocksDiscarded) {
	MReleaseWriteLock(&afs_xdcache);
	return;
    }

    /*
     * Get an entry from the list of discarded cache elements
     */
    tdc = afs_GetDSlot(afs_discardDCList, 0);
    afs_discardDCList = afs_dvnextTbl[tdc->index];
    afs_dvnextTbl[tdc->index] = NULLIDX;
    afs_discardDCCount--;
    size = ((tdc->f.chunkBytes + afs_fsfragsize)^afs_fsfragsize)>>10;/* round up */
    afs_blocksDiscarded -= size;
    afs_stats_cmperf.cacheBlocksDiscarded = afs_blocksDiscarded;
    MReleaseWriteLock(&afs_xdcache);

    /*
     * Truncate the element to reclaim its space
     */
    tfile = afs_CFileOpen(tdc->f.inode);
    afs_CFileTruncate(tfile, 0);
    afs_CFileClose(tfile);
    afs_AdjustSize(tdc, 0);

    /*
     * Free the element we just truncated
     */
    MObtainWriteLock(&afs_xdcache,511);
    afs_indexFlags[tdc->index] &= ~IFDiscarded;
    afs_FreeDCache(tdc);
    tdc->refCount--;
    MReleaseWriteLock(&afs_xdcache);
}

/*
 * afs_MaybeFreeDiscardedDCache
 *
 * Description:
 *      Free as many entries from the list of discarded cache elements
 *      as we need to get the free space down below CM_WAITFORDRAINPCT (98%).
 *
 * Parameters:
 *      None
 */
afs_MaybeFreeDiscardedDCache()
{

    AFS_STATCNT(afs_MaybeFreeDiscardedDCache);

    while (afs_blocksDiscarded &&
	   (afs_blocksUsed > (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100)) {
	afs_FreeDiscardedDCache();
    }
    return 0;
}

/*
 * afs_GetDownDSlot
 *
 * Description:
 *	Try to free up a certain number of disk slots.
 *
 * Parameters:
 *	anumber : Targeted number of disk slots to free up.
 */
#if defined(AFS_SGI_ENV) && defined(AFS_SGI_SHORTSTACK)
extern SV_TYPE afs_sgibksync;
extern SV_TYPE afs_sgibkwait;
extern lock_t afs_sgibklock;
extern struct dcache *afs_sgibklist;
#endif

static void
afs_GetDownDSlot(anumber)
    int anumber;

{ /*afs_GetDownDSlot*/

    struct afs_q *tq, *nq;
    struct dcache *tdc;
    int ix;
    unsigned int i=0;
    unsigned int cnt;

    AFS_STATCNT(afs_GetDownDSlot);
    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
	osi_Panic("diskless getdowndslot");

    if (CheckLock(&afs_xdcache) != -1)
	osi_Panic("getdowndslot nolock");

    /* decrement anumber first for all dudes in free list */
    for(tdc = afs_freeDSList; tdc; tdc = (struct dcache *)tdc->lruq.next)
	anumber--;
    if (anumber	<= 0)
	return;				/* enough already free */

    for(cnt=0, tq = afs_DLRU.prev; tq != &afs_DLRU && anumber > 0;
	tq = nq, cnt++) {
	tdc = (struct dcache *)	tq;	/* q is first elt in dcache entry */
	nq = QPrev(tq);	/* in case we remove it */
	if (tdc->refCount == 0) {
	    if ((ix=tdc->index) == NULLIDX) osi_Panic("getdowndslot");
	    /* pull the entry out of the lruq and put it on the free list */
	    QRemove(&tdc->lruq);

	    /* write-through if modified */
	    if (tdc->flags & DFEntryMod) {
#if defined(AFS_SGI_ENV) && defined(AFS_SGI_SHORTSTACK)
		/*
		 * ask proxy to do this for us - we don't have the stack space
		 */
	        while (tdc->flags & DFEntryMod) {
		    int s;
		    AFS_GUNLOCK();
		    s = SPLOCK(afs_sgibklock);
		    if (afs_sgibklist == NULL) {
			/* if slot is free, grab it. */
			afs_sgibklist = tdc;
			SV_SIGNAL(&afs_sgibksync);
		    }
		    /* wait for daemon to (start, then) finish. */
		    SP_WAIT(afs_sgibklock, s, &afs_sgibkwait, PINOD);
		    AFS_GLOCK();
		}
#else
		tdc->flags &= ~DFEntryMod;
		afs_WriteDCache(tdc, 1);
#endif
	    }

	    tdc->stamp = 0;
#ifdef IHINT
             if (tdc->ihint) {
                 struct osi_file * f = (struct osi_file *)tdc->ihint;
                 tdc->ihint = 0;
                 afs_UFSClose(f);
		 nihints--;
             }
#endif /* IHINT */


	    /* finally put the entry in the free list */
	    afs_indexTable[ix] = (struct dcache *) 0;
	    afs_indexFlags[ix] &= ~IFEverUsed;
	    tdc->index = NULLIDX;
	    tdc->lruq.next = (struct afs_q *) afs_freeDSList;
	    afs_freeDSList = tdc;
	    anumber--;
	}
    }
} /*afs_GetDownDSlot*/



/*
 * afs_PutDCache
 *
 * Description:
 *	Decrement the reference count on a disk cache entry.
 *
 * Parameters:
 *	ad : Ptr to the dcache entry to decrement.
 *
 * Environment:
 *	Nothing interesting.
 */
afs_PutDCache(ad)
    register struct dcache *ad; 

{ /*afs_PutDCache*/
    AFS_STATCNT(afs_PutDCache);
#ifndef	AFS_SUN5_ENVX
    MObtainWriteLock(&afs_xdcache,276);
#endif
    if (ad->refCount <= 0)
	osi_Panic("putdcache");
    --ad->refCount;
#ifdef	AFS_SUN5_ENVX
    MReleaseWriteLock(&ad->lock);
#else
    MReleaseWriteLock(&afs_xdcache);
#endif
    return 0;

} /*afs_PutDCache*/


/*
 * afs_TryToSmush
 *
 * Description:
 *	Try to discard all data associated with this file from the
 *	cache.
 *
 * Parameters:
 *	avc : Pointer to the cache info for the file.
 *
 * Environment:
 *	Both pvnLock and lock are write held.
 */
void
afs_TryToSmush(avc, acred, sync)
    register struct vcache *avc;
    struct AFS_UCRED *acred;
    int sync;
{ /*afs_TryToSmush*/

    register struct dcache *tdc;
    register int index;
    register int i;
    AFS_STATCNT(afs_TryToSmush);
    afs_Trace2(afs_iclSetp, CM_TRACE_TRYTOSMUSH, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length);
    sync = 1;				/* XX Temp testing XX*/

#if     defined(AFS_SUN5_ENV)
    ObtainWriteLock(&avc->vlock, 573);
    avc->activeV++;     /* block new getpages */
    ReleaseWriteLock(&avc->vlock);
#endif

    /* Flush VM pages */
    osi_VM_TryToSmush(avc, acred, sync);

    /*
     * Get the hash chain containing all dce's for this fid
     */
    i =	DVHash(&avc->fid);
    MObtainWriteLock(&afs_xdcache,277);
    for(index = afs_dvhashTbl[i]; index != NULLIDX; index=i) {
      i = afs_dvnextTbl[index];	/* next pointer this hash table */
      if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	tdc = afs_GetDSlot(index, (struct dcache *)0);
	if (!FidCmp(&tdc->f.fid, &avc->fid)) {
	    if (sync) {
		if ((afs_indexFlags[index] & IFDataMod) == 0 &&
		    tdc->refCount == 1) {
		    afs_FlushDCache(tdc);
		}
	    } else
		afs_indexTable[index] = 0;
	}
	lockedPutDCache(tdc);
      }
    }
#if	defined(AFS_SUN5_ENV)
    ObtainWriteLock(&avc->vlock, 545);
    if (--avc->activeV == 0 && (avc->vstates & VRevokeWait)) {
	avc->vstates &= ~VRevokeWait;
	afs_osi_Wakeup((char *)&avc->vstates);
    }
    ReleaseWriteLock(&avc->vlock);
#endif
    MReleaseWriteLock(&afs_xdcache);
    /*
     * It's treated like a callback so that when we do lookups we'll invalidate the unique bit if any
     * trytoSmush occured during the lookup call
     */
    afs_allCBs++;
} /*afs_TryToSmush*/

/*
 * afs_FindDCache
 *
 * Description:
 *	Given the cached info for a file and a byte offset into the
 *	file, make sure the dcache entry for that file and containing
 *	the given byte is available, returning it to our caller.
 *
 * Parameters:
 *	avc   : Pointer to the (held) vcache entry to look in.
 *	abyte : Which byte we want to get to.
 *
 * Returns:
 *	Pointer to the dcache entry covering the file & desired byte,
 *	or NULL if not found.
 *
 * Environment:
 *	The vcache entry is held upon entry.
 */

struct dcache *afs_FindDCache(avc, abyte)
    register struct vcache *avc;
    afs_int32 abyte;

{ /*afs_FindDCache*/

    afs_int32 chunk;
    register afs_int32 i, index;
    register struct dcache *tdc;

    AFS_STATCNT(afs_FindDCache);
    chunk = AFS_CHUNK(abyte);

    /*
     * Hash on the [fid, chunk] and get the corresponding dcache index
     * after write-locking the dcache.
     */
    i = DCHash(&avc->fid, chunk);
    MObtainWriteLock(&afs_xdcache,278);
    for(index = afs_dchashTbl[i]; index != NULLIDX;) {
      if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	tdc = afs_GetDSlot(index, (struct dcache *)0);
	if (!FidCmp(&tdc->f.fid, &avc->fid) && chunk == tdc->f.chunk) {
	    break;  /* leaving refCount high for caller */
	}
	lockedPutDCache(tdc);
      }
      index = afs_dcnextTbl[index];
    }
    MReleaseWriteLock(&afs_xdcache);
    if (index != NULLIDX) {
	hset(afs_indexTimes[tdc->index], afs_indexCounter);
	hadd32(afs_indexCounter, 1);
	return tdc;
    }
    else
	return(struct dcache *) 0;

} /*afs_FindDCache*/


/*
 * afs_UFSCacheStoreProc
 *
 * Description:
 *	Called upon store.
 *
 * Parameters:
 *	acall : Ptr to the Rx call structure involved.
 *	afile : Ptr to the related file descriptor.
 *	alen  : Size of the file in bytes.
 *	avc   : Ptr to the vcache entry.
 *      shouldWake : is it "safe" to return early from close() ?
 *	abytesToXferP  : Set to the number of bytes to xfer.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *	abytesXferredP : Set to the number of bytes actually xferred.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *
 * Environment:
 *	Nothing interesting.
 */
static int afs_UFSCacheStoreProc(acall, afile, alen, avc, shouldWake,
			      abytesToXferP, abytesXferredP)
     register struct rx_call *acall;
     struct osi_file *afile;
     register afs_int32 alen;
     struct vcache *avc;
     int *shouldWake;
     afs_int32 *abytesToXferP;
     afs_int32 *abytesXferredP;
{ /* afs_UFSCacheStoreProc*/

    afs_int32 code, got;
    register char *tbuffer;
    register int tlen;

    AFS_STATCNT(UFS_CacheStoreProc);

#ifndef AFS_NOSTATS
    /*
     * In this case, alen is *always* the amount of data we'll be trying
     * to ship here.
     */
    (*abytesToXferP)  = alen;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */

    afs_Trace3(afs_iclSetp, CM_TRACE_STOREPROC, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, avc->m.Length, ICL_TYPE_INT32, alen);
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    while (alen > 0) {
	tlen = (alen > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : alen);
	got = afs_osi_Read(afile, -1, tbuffer, tlen);
	if ((got < 0) 
#if	!defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_SGI64_ENV) && !defined(AFS_LINUX20_ENV)
	    || (got != tlen && getuerror())
#endif
	    ) {
	    osi_FreeLargeSpace(tbuffer);
	    return EIO;
	}
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	code = rx_Write(acall, tbuffer, got);  /* writing 0 bytes will
	* push a short packet.  Is that really what we want, just because the
        * data didn't come back from the disk yet?  Let's try it and see. */
#ifdef RX_ENABLE_LOCKS
	AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
#ifndef AFS_NOSTATS
	(*abytesXferredP) += code;
#endif /* AFS_NOSTATS */
	if (code != got) {
	    osi_FreeLargeSpace(tbuffer);
	    return -33;
	}
	alen -= got;
	/*
	 * If file has been locked on server, we can allow the store
	 * to continue.
	 */
	if (shouldWake && *shouldWake && (rx_GetRemoteStatus(acall) & 1)) {
	    *shouldWake = 0;  /* only do this once */
	    afs_wakeup(avc);
	}
    }
    osi_FreeLargeSpace(tbuffer);
    return 0;

} /* afs_UFSCacheStoreProc*/


/*
 * afs_UFSCacheFetchProc
 *
 * Description:
 *	Routine called on fetch; also tells people waiting for data
 *	that more has arrived.
 *
 * Parameters:
 *	acall : Ptr to the Rx call structure.
 *	afile : File descriptor for the cache file.
 *	abase : Base offset to fetch.
 *	adc   : Ptr to the dcache entry for the file.
 *      avc   : Ptr to the vcache entry for the file.
 *	abytesToXferP  : Set to the number of bytes to xfer.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *	abytesXferredP : Set to the number of bytes actually xferred.
 *			 NOTE: This parameter is only used if AFS_NOSTATS
 *				is not defined.
 *
 * Environment:
 *	Nothing interesting.
 */

static int afs_UFSCacheFetchProc(acall, afile, abase, adc, avc,
			      abytesToXferP, abytesXferredP)
    register struct rx_call *acall;
    afs_int32 abase;
    struct dcache *adc;
    struct vcache *avc;
    struct osi_file *afile;
    afs_int32 *abytesToXferP;
    afs_int32 *abytesXferredP;

{ /*UFS_CacheFetchProc*/

    afs_int32 length;
    register afs_int32 code;
    register char *tbuffer;
    register int tlen;
    int moredata;

    AFS_STATCNT(UFS_CacheFetchProc);
    afile->offset = 0;	/* Each time start from the beginning */
#ifndef AFS_NOSTATS
    (*abytesToXferP)  = 0;
    (*abytesXferredP) = 0;
#endif /* AFS_NOSTATS */
    tbuffer = osi_AllocLargeSpace(AFS_LRALLOCSIZ);
    do {
#ifdef RX_ENABLE_LOCKS
	AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	code = rx_Read(acall, (char *)&length, sizeof(afs_int32));
#ifdef RX_ENABLE_LOCKS
	AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
	if (code != sizeof(afs_int32)) {
	    osi_FreeLargeSpace(tbuffer);
	    code = rx_Error(acall);
	    return (code?code:-1);	/* try to return code, not -1 */
	}
	length = ntohl(length);
	/*
	 * The fetch protocol is extended for the AFS/DFS translator
	 * to allow multiple blocks of data, each with its own length,
	 * to be returned. As long as the top bit is set, there are more
	 * blocks expected.
	 *
	 * We do not do this for AFS file servers because they sometimes
	 * return large negative numbers as the transfer size.
	 */
	if (avc->states & CForeign) {
	    moredata = length & 0x80000000;
	    length &= ~0x80000000;
	} else {
	    moredata = 0;
	}
#ifndef AFS_NOSTATS
	(*abytesToXferP) += length;
#endif				/* AFS_NOSTATS */
	while (length > 0) {
	    tlen = (length > AFS_LRALLOCSIZ ? AFS_LRALLOCSIZ : length);
#ifdef RX_ENABLE_LOCKS
	    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
	    code = rx_Read(acall, tbuffer, tlen);
#ifdef RX_ENABLE_LOCKS
	    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
#ifndef AFS_NOSTATS
	    (*abytesXferredP) += code;
#endif				/* AFS_NOSTATS */
	    if (code != tlen) {
		osi_FreeLargeSpace(tbuffer);
		return -34;
	    }
	    code = afs_osi_Write(afile, -1, tbuffer, tlen);
	    if (code != tlen) {
		osi_FreeLargeSpace(tbuffer);
		return EIO;
	    }
	    abase += tlen;
	    length -= tlen;
	    adc->validPos = abase;
	    if (adc->flags & DFWaiting) {
		adc->flags &= ~DFWaiting;
		afs_osi_Wakeup(&adc->validPos);
	    }
	}
    } while (moredata);
    osi_FreeLargeSpace(tbuffer);
    return 0;

} /* afs_UFSCacheFetchProc*/

/*
 * afs_GetDCache
 *
 * Description:
 *	This function is called to obtain a reference to data stored in
 *	the disk cache, locating a chunk of data containing the desired
 *	byte and returning a reference to the disk cache entry, with its
 *	reference count incremented.
 *
 * Parameters:
 * IN:
 *	avc     : Ptr to a vcache entry (unlocked)
 *	abyte   : Byte position in the file desired
 *	areq    : Request structure identifying the requesting user.
 *	aflags  : Settings as follows:
 *			1 : Set locks
 *			2 : Return after creating entry.
 * OUT:
 *	aoffset : Set to the offset within the chunk where the resident
 *		  byte is located.
 *	alen    : Set to the number of bytes of data after the desired
 *		  byte (including the byte itself) which can be read
 *		  from this chunk.
 *
 * Environment:
 *	The vcache entry pointed to by avc is unlocked upon entry.
 */

struct tlocal1 {
    struct AFSVolSync tsync;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
};

/* these fields are protected by the lock on the vcache and luck 
 * on the dcache */
#define updateV2DC(l,v,d,src) { if (l) ObtainWriteLock(&((v)->lock),src);\
    if (hsame((v)->m.DataVersion, (d)->f.versionNo) && (v)->callback) { \
	(v)->quick.dc = (d);                                          \
	(v)->quick.stamp = (d)->stamp = MakeStamp();                  \
	(v)->quick.minLoc = AFS_CHUNKTOBASE((d)->f.chunk);            \
  /* Don't think I need these next two lines forever */       \
	(v)->quick.len = (d)->f.chunkBytes;                           \
	(v)->h1.dchint = (d); }  if(l) ReleaseWriteLock(&((v)->lock)); }

struct dcache *afs_GetDCache(avc, abyte, areq, aoffset, alen, aflags)
    register struct vcache *avc;    /*Held*/
    afs_int32 abyte;
    int	aflags;
    afs_int32 *aoffset, *alen;
    register struct vrequest *areq;

{ /*afs_GetDCache*/

    register afs_int32 i, code, code1, shortcut , adjustsize=0;
    int setLocks;
    afs_int32 index;
    afs_int32 us;
    afs_int32 chunk;
    afs_int32 maxGoodLength;		/* amount of good data at server */
    struct rx_call *tcall;
    afs_int32 Position = 0;
    afs_int32 size;				/* size of segment to transfer */
    struct tlocal1 *tsmall;
    register struct dcache *tdc;
    register struct osi_file *file;
    register struct conn *tc;
    int downDCount = 0;
    XSTATS_DECLS
#ifndef AFS_NOSTATS
    struct afs_stats_xferData *xferP;	/* Ptr to this op's xfer struct */
    osi_timeval_t  xferStartTime,	/*FS xfer start time*/
                   xferStopTime;	/*FS xfer stop time*/
    afs_int32 bytesToXfer;			/* # bytes to xfer*/
    afs_int32 bytesXferred;			/* # bytes actually xferred*/
    struct afs_stats_AccessInfo *accP;	/*Ptr to access record in stats*/
    int fromReplica;			/*Are we reading from a replica?*/
    int numFetchLoops;			/*# times around the fetch/analyze loop*/
#endif /* AFS_NOSTATS */

    AFS_STATCNT(afs_GetDCache);

    if (dcacheDisabled)
	return NULL;

    /*
     * Determine the chunk number and offset within the chunk corresponding
     * to the desired byte.
     */
    if (vType(avc) == VDIR) {
	chunk = 0;
    }
    else {
	chunk = AFS_CHUNK(abyte);
    }

    setLocks = aflags & 1;

    /* come back to here if we waited for the cache to drain. */
 RetryGetDCache:
    shortcut = 0;

    /* check hints first! (might could use bcmp or some such...) */

    if (tdc = avc->h1.dchint) {
	MObtainReadLock(&afs_xdcache);
	if ( (tdc->index != NULLIDX) && !FidCmp(&tdc->f.fid, &avc->fid) &&
	    chunk == tdc->f.chunk &&
	    !(afs_indexFlags[tdc->index] & (IFFree|IFDiscarded))) {
	    /* got the right one.  It might not be the right version, and it 
	     * might be fetching, but it's the right dcache entry.
	     */
	    /* All this code should be integrated better with what follows:
	     * I can save a good bit more time under a write lock if I do..
	     */
	    /* does avc need to be locked? */
	    /* Note that the race labeled LOCKXXX is inconsequential: the xdcache
	     * lock protects both the dcache slots AND the DLRU list.  While 
	     * the slots and hash table and DLRU list all may change in the race,
	     * THIS particular dcache structure cannot be recycled and its LRU 
	     * pointers must still be valid once we get the lock again.  Still
	     * we should either create another lock or invent a new method of
	     * managing dcache structs -- CLOCK or something. */
	    shortcut = 1;	    
#ifdef	AFS_SUN5_ENVX
	    MObtainWriteLock(&tdc->lock,279);
#endif
	    tdc->refCount++;
	    if (hsame(tdc->f.versionNo, avc->m.DataVersion) 
		&& !(tdc->flags & DFFetching)) {
		afs_stats_cmperf.dcacheHits++;
		MReleaseReadLock(&afs_xdcache);

		MObtainWriteLock(&afs_xdcache, 559); /* LOCKXXX */
		QRemove(&tdc->lruq);
		QAdd(&afs_DLRU, &tdc->lruq);
		MReleaseWriteLock(&afs_xdcache);
		goto done;
	    }
#ifdef	AFS_SUN5_ENVX
	   MReleaseWriteLock(&tdc->lock);
#endif
	}
	MReleaseReadLock(&afs_xdcache);
    }
    if (!shortcut)
      {

    /*
     * Hash on the [fid, chunk] and get the corresponding dcache index
     * after write-locking the dcache.
     */
 RetryLookup:
    i = DCHash(&avc->fid, chunk);
    afs_MaybeWakeupTruncateDaemon();	/* check to make sure our space is fine */
    MObtainWriteLock(&afs_xdcache,280);
    us = NULLIDX;
    for(index = afs_dchashTbl[i]; index != NULLIDX;) {
      if (afs_indexUnique[index] == avc->fid.Fid.Unique) {
	tdc = afs_GetDSlot(index, (struct dcache *)0);
	if (!FidCmp(&tdc->f.fid, &avc->fid) && chunk == tdc->f.chunk) {
	    /* Move it up in the beginning of the list */
	    if (afs_dchashTbl[i] != index)  {
		afs_dcnextTbl[us] = afs_dcnextTbl[index];
		afs_dcnextTbl[index] = afs_dchashTbl[i];
		afs_dchashTbl[i] = index;
	    }
	    MReleaseWriteLock(&afs_xdcache);
	    break;  /* leaving refCount high for caller */
	}
	lockedPutDCache(tdc);
      }
      us = index;
      index = afs_dcnextTbl[index];
    }
    /*
     * If we didn't find the entry, we'll create one.
     */
    if (index == NULLIDX) {
	afs_Trace2(afs_iclSetp, CM_TRACE_GETDCACHE1, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_INT32, chunk);

	if (afs_discardDCList == NULLIDX && afs_freeDCList == NULLIDX) {
	    while (1) {
		if (!setLocks) avc->states |= CDCLock;
		afs_GetDownD(5, (int*)0);	/* just need slots */
		if (!setLocks) avc->states &= (~CDCLock);
		if (afs_discardDCList != NULLIDX || afs_freeDCList != NULLIDX)
		    break;
		/* If we can't get space for 5 mins we give up and panic */
		if (++downDCount > 300)
		    osi_Panic("getdcache"); 
		MReleaseWriteLock(&afs_xdcache);		
		afs_osi_Wait(1000, 0, 0);
		goto RetryLookup;
	    }
	}
	if (afs_discardDCList == NULLIDX ||
	    ((aflags & 2) && afs_freeDCList != NULLIDX)) {
	    afs_indexFlags[afs_freeDCList] &= ~IFFree;
	    tdc = afs_GetDSlot(afs_freeDCList, 0);
	    afs_freeDCList = afs_dvnextTbl[tdc->index];
	    afs_freeDCCount--;
	} else {
	    afs_indexFlags[afs_discardDCList] &= ~IFDiscarded;
	    tdc = afs_GetDSlot(afs_discardDCList, 0);
	    afs_discardDCList = afs_dvnextTbl[tdc->index];
	    afs_discardDCCount--;
	    size = ((tdc->f.chunkBytes + afs_fsfragsize)^afs_fsfragsize)>>10;
	    afs_blocksDiscarded -= size;
	    afs_stats_cmperf.cacheBlocksDiscarded = afs_blocksDiscarded;
	    if (aflags & 2) {
		/* Truncate the chunk so zeroes get filled properly */
		file = afs_CFileOpen(tdc->f.inode);
		afs_CFileTruncate(file, 0);
		afs_CFileClose(file);
		afs_AdjustSize(tdc, 0);
	    }
	}

	/*
	 * Fill in the newly-allocated dcache record.
	 */
	afs_indexFlags[tdc->index] &= ~(IFDirtyPages | IFAnyPages);
	tdc->f.fid = avc->fid;
	afs_indexUnique[tdc->index] = tdc->f.fid.Fid.Unique;
	hones(tdc->f.versionNo);	    /* invalid value */
	tdc->f.chunk = chunk;
	/* XXX */
	if (tdc->lruq.prev == &tdc->lruq) osi_Panic("lruq 1");
	/*
	 * Now add to the two hash chains - note that i is still set
	 * from the above DCHash call.
	 */
	afs_dcnextTbl[tdc->index] = afs_dchashTbl[i];
	afs_dchashTbl[i] = tdc->index;
	i = DVHash(&avc->fid);
	afs_dvnextTbl[tdc->index] = afs_dvhashTbl[i];
	afs_dvhashTbl[i] = tdc->index;
	tdc->flags = DFEntryMod;
	tdc->f.states = 0;
	afs_MaybeWakeupTruncateDaemon();
	MReleaseWriteLock(&afs_xdcache);
    }
  }  /* else hint failed... */

    afs_Trace4(afs_iclSetp, CM_TRACE_GETDCACHE2, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_POINTER, tdc,
	       ICL_TYPE_INT32, hgetlo(tdc->f.versionNo),
	       ICL_TYPE_INT32, hgetlo(avc->m.DataVersion));
    /*
     * Here we have the unlocked entry in tdc, with its refCount
     * incremented.  Note: we don't use the S-lock; it costs concurrency
     * when storing a file back to the server.
     */
    if (setLocks) ObtainReadLock(&avc->lock);

    /*
     * Not a newly created file so we need to check the file's length and
     * compare data versions since someone could have changed the data or we're
     * reading a file written elsewhere. We only want to bypass doing no-op
     * read rpcs on newly created files (dv of 0) since only then we guarantee
     * that this chunk's data hasn't been filled by another client.
     */
    if (!hiszero(avc->m.DataVersion))
	aflags &= ~4;
#if	defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV)
#ifdef	AFS_SGI_ENV
#ifdef AFS_SGI64_ENV
    if (aflags & 4) adjustsize = NBPP;
#else
    if (aflags & 4) adjustsize = 8192;
#endif
#else
    if (aflags & 4) adjustsize = 4096;
#endif
    if (AFS_CHUNKTOBASE(chunk)+adjustsize >= avc->m.Length &&
#else
#if	defined(AFS_SUN_ENV)  || defined(AFS_OSF_ENV)
    if (((aflags & 4) || (AFS_CHUNKTOBASE(chunk) >= avc->m.Length)) &&
#else
    if (AFS_CHUNKTOBASE(chunk) >= avc->m.Length &&
#endif
#endif
	!hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	/* no data in file to read at this position */
	if (setLocks) {
	    ReleaseReadLock(&avc->lock);
	    ObtainWriteLock(&avc->lock,64);
	}
	/* check again, now that we have a write lock */
#if	defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV)
	if (AFS_CHUNKTOBASE(chunk)+adjustsize >= avc->m.Length &&
#else
#if	defined(AFS_SUN_ENV)  || defined(AFS_OSF_ENV)
	if (((aflags & 4) || (AFS_CHUNKTOBASE(chunk) >= avc->m.Length)) &&
#else
	if (AFS_CHUNKTOBASE(chunk) >= avc->m.Length &&
#endif
#endif
	    !hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	    file = afs_CFileOpen(tdc->f.inode);
	    afs_CFileTruncate(file, 0);
	    afs_CFileClose(file);
	    afs_AdjustSize(tdc, 0);
	    hset(tdc->f.versionNo, avc->m.DataVersion);
	    tdc->flags |= DFEntryMod;
	}
	if (setLocks) {
	    ReleaseWriteLock(&avc->lock);
	    ObtainReadLock(&avc->lock);
	}
    }
    if (setLocks) ReleaseReadLock(&avc->lock);

    /*
     * We must read in the whole chunk iff the version number doesn't
     * match.
     */
    if (aflags & 2) {
	/* don't need data, just a unique dcache entry */
	hset(afs_indexTimes[tdc->index], afs_indexCounter);
	hadd32(afs_indexCounter, 1);
	updateV2DC(setLocks,avc,tdc,567);
	return tdc;	/* check if we're done */
    }

    if (setLocks) ObtainReadLock(&avc->lock);
    if (!hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	/*
	 * Version number mismatch.
	 */
	if (setLocks) {
	    ReleaseReadLock(&avc->lock);
	    ObtainWriteLock(&avc->lock,65);
	}

	/*
	 * If data ever existed for this vnode, and this is a text object,
	 * do some clearing.  Now, you'd think you need only do the flush
	 * when VTEXT is on, but VTEXT is turned off when the text object
	 * is freed, while pages are left lying around in memory marked
	 * with this vnode.  If we would reactivate (create a new text
	 * object from) this vnode, we could easily stumble upon some of
	 * these old pages in pagein.  So, we always flush these guys.
	 * Sun has a wonderful lack of useful invariants in this system.
	 *
	 * avc->flushDV is the data version # of the file at the last text
	 * flush.  Clearly, at least, we don't have to flush the file more
	 * often than it changes
	 */
	if (hcmp(avc->flushDV, avc->m.DataVersion) < 0) {
	    /*
	     * By here, the cache entry is always write-locked.  We can
	     * deadlock if we call osi_Flush with the cache entry locked...
	     */
	    ReleaseWriteLock(&avc->lock);
	    osi_FlushText(avc);
	    /*
	     * Call osi_FlushPages in open, read/write, and map, since it
	     * is too hard here to figure out if we should lock the
	     * pvnLock.
	     */
	    ObtainWriteLock(&avc->lock,66);
	}

	/* Watch for standard race condition */
	if (hsame(avc->m.DataVersion, tdc->f.versionNo)) {
	  updateV2DC(0,avc,tdc,569);          /* set hint */
	    if (setLocks) ReleaseWriteLock(&avc->lock);
	    afs_stats_cmperf.dcacheHits++;
	    goto done;
	}

	/* Sleep here when cache needs to be drained. */
	if (setLocks &&
	    (afs_blocksUsed > (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100)) {
	    /* Make sure truncate daemon is running */
	    afs_MaybeWakeupTruncateDaemon();
	    tdc->refCount--; /* we'll re-obtain the dcache when we re-try. */
	    ReleaseWriteLock(&avc->lock);
	    while ((afs_blocksUsed-afs_blocksDiscarded) >
		   (CM_WAITFORDRAINPCT*afs_cacheBlocks)/100) {
		afs_WaitForCacheDrain = 1;
		afs_osi_Sleep(&afs_WaitForCacheDrain);
	    }
	    afs_MaybeFreeDiscardedDCache();
	    /* need to check if someone else got the chunk first. */
	    goto RetryGetDCache;
        }

	/* Do not fetch data beyond truncPos. */
	maxGoodLength = avc->m.Length;
	if (avc->truncPos < maxGoodLength) maxGoodLength = avc->truncPos;
	Position = AFS_CHUNKBASE(abyte);
	if (vType(avc) == VDIR) {
	    size = avc->m.Length;
	    if (size > tdc->f.chunkBytes) {
		/* pre-reserve space for file */
		afs_AdjustSize(tdc, size);
	    }
	    size = 999999999;	    /* max size for transfer */
	}
	else {
	    size = AFS_CHUNKSIZE(abyte);	/* expected max size */
	    /* don't read past end of good data on server */
	    if (Position + size > maxGoodLength)
		size = maxGoodLength - Position;
	    if (size < 0) size = 0; /* Handle random races */
	    if (size > tdc->f.chunkBytes) {
		/* pre-reserve space for file */
		afs_AdjustSize(tdc, size);	/* changes chunkBytes */
		/* max size for transfer still in size */
	    }
	}
	if (afs_mariner && !tdc->f.chunk) 
	    afs_MarinerLog("fetch$Fetching", avc); /* , Position, size, afs_indexCounter );*/
	/*
	 * Right now, we only have one tool, and it's a hammer.  So, we
	 * fetch the whole file.
	 */
	DZap(&tdc->f.inode);	/* pages in cache may be old */
#ifdef  IHINT
        if (file = tdc->ihint) {
	  if (tdc->f.inode == file->inum ) 
	    usedihint++;
	  else {
	    tdc->ihint = 0;
	    afs_UFSClose(file);
	    file = 0;
	    nihints--;
	    file = osi_UFSOpen(tdc->f.inode);
	  }
	}
        else
#endif /* IHINT */
	file = afs_CFileOpen(tdc->f.inode);
	afs_RemoveVCB(&avc->fid);
	tdc->f.states |= DWriting;
	tdc->flags |= DFFetching;
	tdc->validPos = Position; /*Last valid position in this chunk*/
	if (tdc->flags & DFFetchReq) {
	    tdc->flags &= ~DFFetchReq;
	    afs_osi_Wakeup(&tdc->validPos);
	}
	tsmall = (struct tlocal1 *) osi_AllocLargeSpace(sizeof(struct tlocal1));
#ifndef AFS_NOSTATS
	/*
	 * Remember if we are doing the reading from a replicated volume,
	 * and how many times we've zipped around the fetch/analyze loop.
	 */
	fromReplica = (avc->states & CRO) ? 1 : 0;
	numFetchLoops = 0;
	accP = &(afs_stats_cmfullperf.accessinf);
	if (fromReplica)
	    (accP->replicatedRefs)++;
	else
	    (accP->unreplicatedRefs)++;
#endif /* AFS_NOSTATS */
	/* this is a cache miss */
        afs_stats_cmperf.dcacheMisses++; 
	afs_Trace3(afs_iclSetp, CM_TRACE_FETCHPROC, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_INT32, Position, ICL_TYPE_INT32, size);
	do {
	    tc = afs_Conn(&avc->fid, areq, SHARED_LOCK);
	    if (tc) {
#ifndef AFS_NOSTATS
		numFetchLoops++;
		if (fromReplica)
		    (accP->numReplicasAccessed)++;
		
#endif /* AFS_NOSTATS */
		avc->callback = tc->srvr->server;
		ConvertWToSLock(&avc->lock);
		i = osi_Time();
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		tcall = rx_NewCall(tc->id);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */


		XSTATS_START_TIME(AFS_STATS_FS_RPCIDX_FETCHDATA);
#ifdef RX_ENABLE_LOCKS
		AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		code = StartRXAFS_FetchData(tcall,
					    (struct AFSFid *) &avc->fid.Fid,
					    Position, size);
#ifdef RX_ENABLE_LOCKS
		AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		if (code == 0) {

#ifndef AFS_NOSTATS
		    xferP = &(afs_stats_cmfullperf.rpc.fsXferTimes[AFS_STATS_FS_XFERIDX_FETCHDATA]);
		    osi_GetuTime(&xferStartTime);

		    code = afs_CacheFetchProc(tcall, file, Position, tdc, avc,
					  &bytesToXfer, &bytesXferred);

		    osi_GetuTime(&xferStopTime);
		    (xferP->numXfers)++;
		    if (!code) {
			(xferP->numSuccesses)++;
			afs_stats_XferSumBytes[AFS_STATS_FS_XFERIDX_FETCHDATA] += bytesXferred;
			(xferP->sumBytes) += (afs_stats_XferSumBytes[AFS_STATS_FS_XFERIDX_FETCHDATA] >> 10);
			afs_stats_XferSumBytes[AFS_STATS_FS_XFERIDX_FETCHDATA] &= 0x3FF;
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
		    code = afs_CacheFetchProc(tcall, file, Position, tdc, avc, 0, 0);
#endif /* AFS_NOSTATS */
		}
		if (code == 0) {
#ifdef RX_ENABLE_LOCKS
		    AFS_GUNLOCK();
#endif /* RX_ENABLE_LOCKS */
		    code = EndRXAFS_FetchData(tcall,
					      &tsmall->OutStatus,
					      &tsmall->CallBack,
					      &tsmall->tsync);
#ifdef RX_ENABLE_LOCKS
		    AFS_GLOCK();
#endif /* RX_ENABLE_LOCKS */
		}
		XSTATS_END_TIME;
		code1 = rx_EndCall(tcall, code);
		UpgradeSToWLock(&avc->lock,27);
	    }
	    else {
		code = -1;
	    }
	    if ( !code && code1 )
		code = code1;

	    if (code == 0) {
	        /* callback could have been broken (or expired) in a race here, 
		 * but we return the data anyway.  It's as good as we knew about
		 * when we started. */
		/* 
		 * validPos is updated by CacheFetchProc, and can only be 
		 * modifed under an S or W lock, which we've blocked out 
		 */
		size = tdc->validPos - Position;  /* actual segment size */
		if (size < 0) size = 0;
		afs_CFileTruncate(file, size); /* prune it */
	    }
	    else { 
	       ObtainWriteLock(&afs_xcbhash, 453);
	       afs_DequeueCallback(avc);
	       avc->states &= ~(CStatd | CUnique);   
	       avc->callback = (struct server *)0;
	       ReleaseWriteLock(&afs_xcbhash);
	       if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
		   osi_dnlc_purgedp(avc);
	   }

	} while
	    (afs_Analyze(tc, code, &avc->fid, areq,
			 AFS_STATS_FS_RPCIDX_FETCHDATA,
			 SHARED_LOCK, (struct cell *)0));

#ifndef AFS_NOSTATS
	/*
	 * In the case of replicated access, jot down info on the number of
	 * attempts it took before we got through or gave up.
	 */
	if (fromReplica) {
	    if (numFetchLoops <= 1)
		(accP->refFirstReplicaOK)++;
	    if (numFetchLoops > accP->maxReplicasPerRef)
		accP->maxReplicasPerRef = numFetchLoops;
	}
#endif /* AFS_NOSTATS */

	tdc->flags &= ~DFFetching;
	if (tdc->flags & DFWaiting) {
	    tdc->flags &= ~DFWaiting;
	    afs_osi_Wakeup(&tdc->validPos);
	}
	if (avc->execsOrWriters == 0) tdc->f.states &= ~DWriting;

	/* now, if code != 0, we have an error and should punt */
	if (code) {
	    afs_CFileTruncate(file, 0);
	    afs_AdjustSize(tdc, 0);
	    afs_CFileClose(file);
	    ZapDCE(tdc);	/* sets DFEntryMod */
	    if (vType(avc) == VDIR) {
		DZap(&tdc->f.inode);
	    }
#ifdef	AFS_SUN5_ENVX
	    afs_PutDCache(tdc);
#else
	    tdc->refCount--;
#endif
	    ObtainWriteLock(&afs_xcbhash, 454);
	    afs_DequeueCallback(avc);
	    avc->states &= ~( CStatd | CUnique );
	    ReleaseWriteLock(&afs_xcbhash);
	    if (avc->fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
		osi_dnlc_purgedp(avc);
	    if (setLocks) ReleaseWriteLock(&avc->lock);
	    osi_FreeLargeSpace(tsmall);
	    tdc = (struct dcache *) 0;
            goto done;
	}

	/* otherwise we copy in the just-fetched info */
	afs_CFileClose(file);
	afs_AdjustSize(tdc, size);  /* new size */
	/*
	 * Copy appropriate fields into vcache
	 */
	afs_ProcessFS(avc, &tsmall->OutStatus, areq);
	hset64(tdc->f.versionNo, tsmall->OutStatus.dataVersionHigh, tsmall->OutStatus.DataVersion);
	tdc->flags |= DFEntryMod;
	afs_indexFlags[tdc->index] |= IFEverUsed;
	if (setLocks) ReleaseWriteLock(&avc->lock);
	osi_FreeLargeSpace(tsmall);
    } /*Data version numbers don't match*/
    else {
	/*
	 * Data version numbers match.  Release locks if we locked
	 * them, and remember we've had a cache hit.
	 */
	if (setLocks)
	    ReleaseReadLock(&avc->lock);
	afs_stats_cmperf.dcacheHits++;
    }  /*Data version numbers match*/

    updateV2DC(setLocks,avc,tdc,332);    /* set hint */
done:
    /*
     * See if this was a reference to a file in the local cell.
     */
    if (avc->fid.Cell == LOCALCELL)
	afs_stats_cmperf.dlocalAccesses++;
    else
	afs_stats_cmperf.dremoteAccesses++;

    /* Fix up LRU info */

    if (tdc) {
      hset(afs_indexTimes[tdc->index], afs_indexCounter);
      hadd32(afs_indexCounter, 1);

        /* return the data */
        if (vType(avc) == VDIR)
	    *aoffset = abyte;
        else
	    *aoffset = AFS_CHUNKOFFSET(abyte);
        *alen = (tdc->f.chunkBytes - *aoffset);
    }

    return tdc;

} /*afs_GetDCache*/


/*
 * afs_WriteThroughDSlots
 *
 * Description:
 *	Sweep through the dcache slots and write out any modified
 *	in-memory data back on to our caching store.
 *
 * Parameters:
 *	None.
 *
 * Environment:
 *	The afs_xdcache is write-locked through this whole affair.
 */
void
afs_WriteThroughDSlots()

{ /*afs_WriteThroughDSlots*/

    register struct dcache *tdc;
    register afs_int32 i, touchedit=0;

    AFS_STATCNT(afs_WriteThroughDSlots);
    MObtainWriteLock(&afs_xdcache,283);
    for(i = 0; i < afs_cacheFiles; i++) {
	tdc = afs_indexTable[i];
	if (tdc && (tdc->flags & DFEntryMod)) {
	    tdc->flags &= ~DFEntryMod;
	    afs_WriteDCache(tdc, 1);
	    touchedit = 1;
	}
    }
    if (!touchedit && (cacheDiskType != AFS_FCACHE_TYPE_MEM)) {
	/* Touch the file to make sure that the mtime on the file is kept up-to-date
	 * to avoid losing cached files on cold starts because their mtime seems old...
	 */
	struct afs_fheader theader;

	theader.magic = AFS_FHMAGIC;
	theader.firstCSize = AFS_FIRSTCSIZE;
	theader.otherCSize = AFS_OTHERCSIZE;
	theader.version = AFS_CI_VERSION;
	afs_osi_Write(afs_cacheInodep, 0, &theader, sizeof(theader));
    }
    MReleaseWriteLock(&afs_xdcache);

} /*afs_WriteThroughDSlots*/

/*
 * afs_MemGetDSlot
 *
 * Description:
 *	Return a pointer to an freshly initialized dcache entry using
 *	a memory-based cache.
 *
 * Parameters:
 *	aslot : Dcache slot to look at.
 *	tmpdc : Ptr to dcache entry.
 *
 * Environment:
 *	Nothing interesting.
 */

struct dcache *afs_MemGetDSlot(aslot, tmpdc)
     register afs_int32 aslot;
     register struct dcache *tmpdc;

{ /*afs_MemGetDSlot*/

    register afs_int32 code;
    register struct dcache *tdc;
    register char *tfile;

    AFS_STATCNT(afs_MemGetDSlot);
    if (CheckLock(&afs_xdcache) != -1) osi_Panic("getdslot nolock");
    if (aslot < 0 || aslot >= afs_cacheFiles) osi_Panic("getdslot slot");
    tdc = afs_indexTable[aslot];
    if (tdc) {
	QRemove(&tdc->lruq);	    /* move to queue head */
	QAdd(&afs_DLRU, &tdc->lruq);
	tdc->refCount++;
	return tdc;
    }
    if (tmpdc == (struct dcache *)0) {
	if (!afs_freeDSList) afs_GetDownDSlot(4); 
	if (!afs_freeDSList) {
	    /* none free, making one is better than a panic */
	    afs_stats_cmperf.dcacheXAllocs++;	/* count in case we have a leak */
	    tdc = (struct dcache *) afs_osi_Alloc(sizeof (struct dcache));
#ifdef	AFS_AIX32_ENV
	    pin((char *)tdc, sizeof(struct dcache));	/* XXX */
#endif
	} else {
	    tdc = afs_freeDSList;
	    afs_freeDSList = (struct dcache *) tdc->lruq.next;
	}
	tdc->flags = 0;	/* up-to-date, not in free q */
	QAdd(&afs_DLRU, &tdc->lruq);
	if (tdc->lruq.prev == &tdc->lruq) osi_Panic("lruq 3");
    }
    else {
	tdc = tmpdc;
	tdc->f.states = 0;
    }
    
    /* initialize entry */
    tdc->f.fid.Cell = 0;
    tdc->f.fid.Fid.Volume = 0;
    tdc->f.chunk = -1;
    hones(tdc->f.versionNo);
    tdc->f.inode = aslot;
    tdc->flags |= DFEntryMod;
    tdc->refCount = 1;
    tdc->index = aslot;
    afs_indexUnique[aslot] = tdc->f.fid.Fid.Unique;
    
    if (tmpdc == (struct dcache *)0)
	afs_indexTable[aslot] = tdc;
    return tdc;

} /*afs_MemGetDSlot*/

unsigned int last_error = 0, lasterrtime = 0;

/*
 * afs_UFSGetDSlot
 *
 * Description:
 *	Return a pointer to an freshly initialized dcache entry using
 *	a UFS-based disk cache.
 *
 * Parameters:
 *	aslot : Dcache slot to look at.
 *	tmpdc : Ptr to dcache entry.
 *
 * Environment:
 *	afs_xdcache lock write-locked.
 */
struct dcache *afs_UFSGetDSlot(aslot, tmpdc)
    register afs_int32 aslot;
    register struct dcache *tmpdc;

{ /*afs_UFSGetDSlot*/

    register afs_int32 code;
    register struct dcache *tdc;

    AFS_STATCNT(afs_UFSGetDSlot);
    if (CheckLock(&afs_xdcache) != -1) osi_Panic("getdslot nolock");
    if (aslot < 0 || aslot >= afs_cacheFiles) osi_Panic("getdslot slot");
    tdc = afs_indexTable[aslot];
    if (tdc) {
#ifdef	AFS_SUN5_ENVX
	mutex_enter(&tdc->lock);
#endif
	QRemove(&tdc->lruq);	    /* move to queue head */
	QAdd(&afs_DLRU, &tdc->lruq);
	tdc->refCount++;
	return tdc;
    }
    /* otherwise we should read it in from the cache file */
    /*
     * If we weren't passed an in-memory region to place the file info,
     * we have to allocate one.
     */
    if (tmpdc == (struct dcache *)0) {
	if (!afs_freeDSList) afs_GetDownDSlot(4);
	if (!afs_freeDSList) {
	    /* none free, making one is better than a panic */
	    afs_stats_cmperf.dcacheXAllocs++;	/* count in case we have a leak */
	    tdc = (struct dcache *) afs_osi_Alloc(sizeof (struct dcache));
#ifdef	AFS_AIX32_ENV
	    pin((char *)tdc, sizeof(struct dcache));	/* XXX */
#endif
	} else {
	    tdc = afs_freeDSList;
	    afs_freeDSList = (struct dcache *) tdc->lruq.next;
	}
	tdc->flags = 0;	/* up-to-date, not in free q */
	QAdd(&afs_DLRU, &tdc->lruq);
	if (tdc->lruq.prev == &tdc->lruq) osi_Panic("lruq 3");
    }
    else {
	tdc = tmpdc;
	tdc->f.states = 0;
	tdc->ihint = 0;
    }

#ifdef	AFS_SUN5_ENVX
    mutex_enter(&tdc->lock);
#endif
    /*
      * Seek to the aslot'th entry and read it in.
      */
    code = afs_osi_Read(afs_cacheInodep, sizeof(struct fcache) * aslot + sizeof(struct afs_fheader),
		    (char *)(&tdc->f), sizeof(struct fcache));
    if (code != sizeof(struct fcache)) {
	tdc->f.fid.Cell = 0;
	tdc->f.fid.Fid.Volume = 0;
	tdc->f.chunk = -1;
	hones(tdc->f.versionNo);
	tdc->flags |= DFEntryMod;
#if !defined(AFS_SUN5_ENV) && !defined(AFS_OSF_ENV) && !defined(AFS_SGI64_ENV) && !defined(AFS_LINUX20_ENV)
	last_error = getuerror();
#endif
	lasterrtime = osi_Time();
	afs_indexUnique[aslot] = tdc->f.fid.Fid.Unique;
    }
    tdc->refCount = 1;
    tdc->index = aslot;

    /*
     * If we didn't read into a temporary dcache region, update the
     * slot pointer table.
     */
    if (tmpdc == (struct dcache *)0)
	afs_indexTable[aslot] = tdc;
    return tdc;

}  /*afs_UFSGetDSlot*/



/*
 * afs_WriteDCache
 *
 * Description:
 *	write a particular dcache entry back to its home in the
 *	CacheInfo file.
 *
 * Parameters:
 *	adc   : Pointer to the dcache entry to write.
 *	atime : If true, set the modtime on the file to the current time.
 *
 * Environment:
 *	Must be called with the afs_xdcache lock at least read-locked.
 *	The reference count is not changed.
 */

afs_WriteDCache(adc, atime)
    int atime;
    register struct dcache *adc;

{ /*afs_WriteDCache*/

    register struct osi_file *tfile;
    register afs_int32 code;

    if (cacheDiskType == AFS_FCACHE_TYPE_MEM) return 0;
    AFS_STATCNT(afs_WriteDCache);
    if (atime)
	adc->f.modTime = osi_Time();
    /*
     * Seek to the right dcache slot and write the in-memory image out to disk.
     */
    code = afs_osi_Write(afs_cacheInodep, sizeof(struct fcache) * adc->index + sizeof(struct afs_fheader), 
		     (char *)(&adc->f), sizeof(struct fcache));
    if (code != sizeof(struct fcache)) return EIO;
    return 0;

} /*afs_WriteDCache*/



/*
 * afs_wakeup
 *
 * Description:
 *	Wake up users of a particular file waiting for stores to take
 *	place.
 *
 * Parameters:
 *	avc : Ptr to related vcache entry.
 *
 * Environment:
 *	Nothing interesting.
 */

afs_wakeup(avc)
    register struct vcache *avc;

{ /*afs_wakeup*/

    register int i;
    register struct brequest *tb;
    tb = afs_brs;
    AFS_STATCNT(afs_wakeup);
    for (i = 0; i < NBRS; i++, tb++) {
	/* if request is valid and for this file, we've found it */
	if (tb->refCount > 0 && avc == tb->vnode) {

	    /*
	     * If CSafeStore is on, then we don't awaken the guy
	     * waiting for the store until the whole store has finished.
	     * Otherwise, we do it now.  Note that if CSafeStore is on,
	     * the BStore routine actually wakes up the user, instead
	     * of us.
	     * I think this is redundant now because this sort of thing
	     * is already being handled by the higher-level code.
	     */
	    if ((avc->states & CSafeStore) == 0) {
		tb->code = 0;
		tb->flags |= BUVALID;
		if (tb->flags & BUWAIT) {
		    tb->flags &= ~BUWAIT;
		    afs_osi_Wakeup(tb);
		}
	    }
	    break;
	}
    }
    return 0;

} /*afs_wakeup*/


/*
 * afs_InitCacheFile
 *
 * Description:
 *	Given a file name and inode, set up that file to be an
 *	active member in the AFS cache.  This also involves checking
 *	the usability of its data.
 *
 * Parameters:
 *	afile  : Name of the cache file to initialize.
 *	ainode : Inode of the file.
 *
 * Environment:
 *	This function is called only during initialization.
 */

int afs_InitCacheFile(afile, ainode)
    ino_t ainode;
    char *afile;

{ /*afs_InitCacheFile*/

    register afs_int32 code;
#ifdef AFS_LINUX22_ENV
    struct dentry *filevp;
#else
    struct vnode *filevp;
#endif
    afs_int32 index;
    int fileIsBad;
    struct osi_file *tfile;
    struct osi_stat tstat;
    register struct dcache *tdc;

    AFS_STATCNT(afs_InitCacheFile);
    index = afs_stats_cmperf.cacheNumEntries;
    if (index >= afs_cacheFiles) return EINVAL;

    MObtainWriteLock(&afs_xdcache,282);
    tdc = afs_GetDSlot(index, (struct dcache *)0);
    MReleaseWriteLock(&afs_xdcache);
    if (afile) {
	code = gop_lookupname(afile,
			      AFS_UIOSYS,
			      0,
			      (struct vnode **) 0,
			      &filevp);
	if (code) {
	    afs_PutDCache(tdc);
	    return code;
	}
	/*
	 * We have a VN_HOLD on filevp.  Get the useful info out and
	 * return.  We make use of the fact that the cache is in the
	 * UFS file system, and just record the inode number.
	 */
#ifdef AFS_LINUX22_ENV
	tdc->f.inode = VTOI(filevp->d_inode)->i_number;
	dput(filevp);
#else
	tdc->f.inode = afs_vnodeToInumber(filevp);
#ifdef AFS_DEC_ENV
	grele(filevp);
#else
	AFS_RELE((struct vnode *)filevp);
#endif
#endif /* AFS_LINUX22_ENV */
    }
    else {
	tdc->f.inode = ainode;
    }
    fileIsBad = 0;
    if ((tdc->f.states & DWriting) ||
	tdc->f.fid.Fid.Volume == 0) fileIsBad = 1;
    tfile = osi_UFSOpen(tdc->f.inode);
    code = afs_osi_Stat(tfile, &tstat);
    if (code) osi_Panic("initcachefile stat");

    /*
     * If file size doesn't match the cache info file, it's probably bad.
     */
    if (tdc->f.chunkBytes != tstat.size) fileIsBad = 1;
    tdc->f.chunkBytes = 0;

    /*
     * If file changed within T (120?) seconds of cache info file, it's
     * probably bad.  In addition, if slot changed within last T seconds,
     * the cache info file may be incorrectly identified, and so slot
     * may be bad.
     */
    if (cacheInfoModTime < tstat.mtime + 120) fileIsBad = 1;
    if (cacheInfoModTime < tdc->f.modTime + 120) fileIsBad = 1;
    /* In case write through is behind, make sure cache items entry is
     * at least as new as the chunk.
     */
    if (tdc->f.modTime < tstat.mtime) fileIsBad = 1;
    if (fileIsBad) {
	tdc->f.fid.Fid.Volume =	0;  /* not in the hash table */
	if (tstat.size != 0)
	    osi_UFSTruncate(tfile, 0);
	/* put entry in free cache slot list */
	afs_dvnextTbl[tdc->index] = afs_freeDCList;
	afs_freeDCList = index;
	afs_freeDCCount++;
	afs_indexFlags[index] |= IFFree;
	afs_indexUnique[index] = 0;
    }
    else {
	/*
	 * We must put this entry in the appropriate hash tables.
	 * Note that i is still set from the above DCHash call
	 */
	code = DCHash(&tdc->f.fid, tdc->f.chunk);
	afs_dcnextTbl[tdc->index] = afs_dchashTbl[code];	
	afs_dchashTbl[code] = tdc->index;
	code = DVHash(&tdc->f.fid);
	afs_dvnextTbl[tdc->index] = afs_dvhashTbl[code];
	afs_dvhashTbl[code] = tdc->index;
	afs_AdjustSize(tdc, tstat.size);    /* adjust to new size */
	if (tstat.size > 0)
	    /* has nontrivial amt of data */
	    afs_indexFlags[index] |= IFEverUsed;
	afs_stats_cmperf.cacheFilesReused++;
	/*
	 * Initialize index times to file's mod times; init indexCounter
	 * to max thereof
	 */
	hset32(afs_indexTimes[index], tstat.atime);
	if (hgetlo(afs_indexCounter) < tstat.atime) {
	    hset32(afs_indexCounter, tstat.atime);
	}
	afs_indexUnique[index] = tdc->f.fid.Fid.Unique;
    } /*File is not bad*/

    osi_UFSClose(tfile);
    tdc->f.states &= ~DWriting;
    tdc->flags &= ~DFEntryMod;
    /* don't set f.modTime; we're just cleaning up */
    afs_WriteDCache(tdc, 0);
    afs_PutDCache(tdc);
    afs_stats_cmperf.cacheNumEntries++;
    return 0;

} /*afs_InitCacheFile*/


/*Max # of struct dcache's resident at any time*/
/*
 * If 'dchint' is enabled then in-memory dcache min is increased because of
 * crashes...
 */
#define DDSIZE 200

/* 
 * afs_dcacheInit
 *
 * Description:
 *	Initialize dcache related variables.
 */
void afs_dcacheInit(int afiles, int ablocks, int aDentries, int achunk,
		    int aflags)
{
    register struct dcache *tdp;
    int i;
    int code;

    afs_freeDCList = NULLIDX;
    afs_discardDCList = NULLIDX;
    afs_freeDCCount = 0;
    afs_freeDSList = (struct dcache *)0;
    hzero(afs_indexCounter);

    LOCK_INIT(&afs_xdcache, "afs_xdcache");
   
    /*
     * Set chunk size
     */
    if (achunk) {
	if (achunk < 0 || achunk > 30)
	    achunk = 13;		/* Use default */
	AFS_SETCHUNKSIZE(achunk);
    }
    
    if(!aDentries)
	aDentries = DDSIZE;
    
    if(aflags & AFSCALL_INIT_MEMCACHE) {
	/*
	 * Use a memory cache instead of a disk cache
	 */
	cacheDiskType = AFS_FCACHE_TYPE_MEM;
	afs_cacheType = &afs_MemCacheOps;
	afiles = (afiles < aDentries) ? afiles : aDentries; /* min */
	ablocks = afiles * (AFS_FIRSTCSIZE/1024);
	/* ablocks is reported in 1K blocks */
	code = afs_InitMemCache(afiles * AFS_FIRSTCSIZE, AFS_FIRSTCSIZE, aflags);
	if (code != 0) {
	    printf("afsd: memory cache too large for available memory.\n");
	    printf("afsd: AFS files cannot be accessed.\n\n");
	    dcacheDisabled = 1;
	    afiles = ablocks = 0;
	}
	else
	    printf("Memory cache: Allocating %d dcache entries...", aDentries);
    } else {
	cacheDiskType = AFS_FCACHE_TYPE_UFS;
	afs_cacheType = &afs_UfsCacheOps;
    }

    if (aDentries > 512) 
	afs_dhashsize = 2048;
    /* initialize hash tables */
    afs_dvhashTbl = (afs_int32 *) afs_osi_Alloc(afs_dhashsize * sizeof(afs_int32));
    afs_dchashTbl = (afs_int32 *) afs_osi_Alloc(afs_dhashsize * sizeof(afs_int32));
    for(i=0;i< afs_dhashsize;i++) {
	afs_dvhashTbl[i] = NULLIDX;
	afs_dchashTbl[i] = NULLIDX;
    }
    afs_dvnextTbl = (afs_int32 *) afs_osi_Alloc(afiles * sizeof(afs_int32));
    afs_dcnextTbl = (afs_int32 *) afs_osi_Alloc(afiles * sizeof(afs_int32));
    for(i=0;i< afiles;i++) {
	afs_dvnextTbl[i] = NULLIDX;
	afs_dcnextTbl[i] = NULLIDX;
    }
    
    /* Allocate and zero the pointer array to the dcache entries */
    afs_indexTable = (struct dcache **)
	afs_osi_Alloc(sizeof(struct dcache *) * afiles);
    bzero((char *)afs_indexTable, sizeof(struct dcache *) * afiles);
    afs_indexTimes = (afs_hyper_t *) afs_osi_Alloc(afiles * sizeof(afs_hyper_t));
    bzero((char *)afs_indexTimes, afiles * sizeof(afs_hyper_t));
    afs_indexUnique = (afs_int32 *) afs_osi_Alloc(afiles * sizeof(afs_uint32));
    bzero((char *)afs_indexUnique, afiles * sizeof(afs_uint32));
    afs_indexFlags = (u_char *) afs_osi_Alloc(afiles * sizeof(u_char));
    bzero((char *)afs_indexFlags, afiles * sizeof(char));
    
    /* Allocate and thread the struct dcache entries themselves */
    tdp = afs_Initial_freeDSList =
	(struct dcache *) afs_osi_Alloc(aDentries * sizeof(struct dcache));
    bzero((char *)tdp, aDentries * sizeof(struct dcache));
#ifdef	AFS_AIX32_ENV
    pin((char *)afs_indexTable, sizeof(struct dcache *) * afiles);/* XXX */    
    pin((char *)afs_indexTimes, sizeof(afs_hyper_t) * afiles);	/* XXX */    
    pin((char *)afs_indexFlags, sizeof(char) * afiles);		/* XXX */    
    pin((char *)afs_indexUnique, sizeof(afs_int32) * afiles);	/* XXX */    
    pin((char *)tdp, aDentries * sizeof(struct dcache));	/* XXX */    
    pin((char *)afs_dvhashTbl, sizeof(afs_int32) * afs_dhashsize);	/* XXX */    
    pin((char *)afs_dchashTbl, sizeof(afs_int32) * afs_dhashsize);	/* XXX */    
    pin((char *)afs_dcnextTbl, sizeof(afs_int32) * afiles);		/* XXX */    
    pin((char *)afs_dvnextTbl, sizeof(afs_int32) * afiles);		/* XXX */    
#endif

    afs_freeDSList = &tdp[0];
    for(i=0; i < aDentries-1; i++) {
	tdp[i].lruq.next = (struct afs_q *) (&tdp[i+1]);
    }
    tdp[aDentries-1].lruq.next = (struct afs_q *) 0;

    afs_stats_cmperf.cacheBlocksOrig = afs_stats_cmperf.cacheBlocksTotal = afs_cacheBlocks = ablocks;
    afs_ComputeCacheParms();	/* compute parms based on cache size */

    afs_dcentries = aDentries;
    afs_blocksUsed = 0;
    QInit(&afs_DLRU);


}

/*
 * shutdown_dcache
 *
 */
void shutdown_dcache(void)
{
    int i;

    afs_osi_Free(afs_dvnextTbl, afs_cacheFiles * sizeof(afs_int32));
    afs_osi_Free(afs_dcnextTbl, afs_cacheFiles * sizeof(afs_int32));
    afs_osi_Free(afs_indexTable, afs_cacheFiles * sizeof(struct dcache *));
    afs_osi_Free(afs_indexTimes, afs_cacheFiles * sizeof(afs_hyper_t));
    afs_osi_Free(afs_indexUnique, afs_cacheFiles * sizeof(afs_uint32));
    afs_osi_Free(afs_indexFlags, afs_cacheFiles * sizeof(u_char));
    afs_osi_Free(afs_Initial_freeDSList, afs_dcentries * sizeof(struct dcache));
#ifdef	AFS_AIX32_ENV
    unpin((char *)afs_dcnextTbl, afs_cacheFiles * sizeof(afs_int32));
    unpin((char *)afs_dvnextTbl, afs_cacheFiles * sizeof(afs_int32));
    unpin((char *)afs_indexTable, afs_cacheFiles * sizeof(struct dcache *));
    unpin((char *)afs_indexTimes, afs_cacheFiles * sizeof(afs_hyper_t));
    unpin((char *)afs_indexUnique, afs_cacheFiles * sizeof(afs_uint32));
    unpin((u_char *)afs_indexFlags, afs_cacheFiles * sizeof(u_char));
    unpin(afs_Initial_freeDSList, afs_dcentries * sizeof(struct dcache));
#endif


    for(i=0;i< afs_dhashsize;i++) {
	afs_dvhashTbl[i] = NULLIDX;
	afs_dchashTbl[i] = NULLIDX;
    }


    afs_blocksUsed = afs_dcentries = 0;
    hzero(afs_indexCounter);

    afs_freeDCCount =  0;
    afs_freeDCList = NULLIDX;
    afs_discardDCList = NULLIDX;
    afs_freeDSList = afs_Initial_freeDSList = 0;

    LOCK_INIT(&afs_xdcache, "afs_xdcache");
    QInit(&afs_DLRU);

}
