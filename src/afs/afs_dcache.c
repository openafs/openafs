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
#include <afsconfig.h>
#include "afs/param.h"


#include "afs/sysincludes.h"	/*Standard vendor system headers */
#include "afsincludes.h"	/*AFS-based standard headers */
#include "afs/afs_stats.h"	/* statistics */
#include "afs/afs_cbqueue.h"
#include "afs/afs_osidnlc.h"

/* Forward declarations. */
static void afs_GetDownD(int anumber, int *aneedSpace, afs_int32 buckethint);
static int afs_FreeDiscardedDCache(void);
static void afs_DiscardDCache(struct dcache *);
static void afs_FreeDCache(struct dcache *);
/* For split cache */
static afs_int32 afs_DCGetBucket(struct vcache *);
static void afs_DCAdjustSize(struct dcache *, afs_int32, afs_int32);
static void afs_DCMoveBucket(struct dcache *, afs_int32, afs_int32);
static void afs_DCSizeInit(void);
static afs_int32 afs_DCWhichBucket(afs_int32, afs_int32);

/*
 * --------------------- Exported definitions ---------------------
 */
/* For split cache */
afs_int32 afs_blocksUsed_0;    /*!< 1K blocks in cache - in theory is zero */
afs_int32 afs_blocksUsed_1;    /*!< 1K blocks in cache */
afs_int32 afs_blocksUsed_2;    /*!< 1K blocks in cache */
afs_int32 afs_pct1 = -1;
afs_int32 afs_pct2 = -1;
afs_uint32 afs_tpct1 = 0;
afs_uint32 afs_tpct2 = 0;
afs_uint32 splitdcache = 0;

afs_lock_t afs_xdcache;		/*!< Lock: alloc new disk cache entries */
afs_int32 afs_freeDCList;	/*!< Free list for disk cache entries */
afs_int32 afs_freeDCCount;	/*!< Count of elts in freeDCList */
afs_int32 afs_discardDCList;	/*!< Discarded disk cache entries */
afs_int32 afs_discardDCCount;	/*!< Count of elts in discardDCList */
struct dcache *afs_freeDSList;	/*!< Free list for disk slots */
struct dcache *afs_Initial_freeDSList;	/*!< Initial list for above */
afs_dcache_id_t cacheInode;               /*!< Inode for CacheItems file */
struct osi_file *afs_cacheInodep = 0;	/*!< file for CacheItems inode */
struct afs_q afs_DLRU;		/*!< dcache LRU */
afs_int32 afs_dhashsize = 1024;
afs_int32 *afs_dvhashTbl;	/*!< Data cache hash table: hashed by FID + chunk number. */
afs_int32 *afs_dchashTbl;	/*!< Data cache hash table: hashed by FID. */
afs_int32 *afs_dvnextTbl;	/*!< Dcache hash table links */
afs_int32 *afs_dcnextTbl;	/*!< Dcache hash table links */
struct dcache **afs_indexTable;	/*!< Pointers to dcache entries */
afs_hyper_t *afs_indexTimes;	/*!< Dcache entry Access times */
afs_int32 *afs_indexUnique;	/*!< dcache entry Fid.Unique */
unsigned char *afs_indexFlags;	/*!< (only one) Is there data there? */
afs_hyper_t afs_indexCounter;	/*!< Fake time for marking index
				 * entries */
afs_int32 afs_cacheFiles = 0;	/*!< Size of afs_indexTable */
afs_int32 afs_cacheBlocks;	/*!< 1K blocks in cache */
afs_int32 afs_cacheStats;	/*!< Stat entries in cache */
afs_int32 afs_blocksUsed;	/*!< Number of blocks in use */
afs_int32 afs_blocksDiscarded;	/*!<Blocks freed but not truncated */
afs_int32 afs_fsfragsize = AFS_MIN_FRAGSIZE;	/*!< Underlying Filesystem minimum unit
					 *of disk allocation usually 1K
					 *this value is (truefrag -1 ) to
					 *save a bunch of subtracts... */
#ifdef AFS_64BIT_CLIENT
#ifdef AFS_VM_RDWR_ENV
afs_size_t afs_vmMappingEnd;	/* !< For large files (>= 2GB) the VM
				 * mapping an 32bit addressing machines
				 * can only be used below the 2 GB
				 * line. From this point upwards we
				 * must do direct I/O into the cache
				 * files. The value should be on a
				 * chunk boundary. */
#endif /* AFS_VM_RDWR_ENV */
#endif /* AFS_64BIT_CLIENT */

/* The following is used to ensure that new dcache's aren't obtained when
 * the cache is nearly full.
 */
int afs_WaitForCacheDrain = 0;
int afs_TruncateDaemonRunning = 0;
int afs_CacheTooFull = 0;

afs_int32 afs_dcentries;	/*!< In-memory dcache entries */


int dcacheDisabled = 0;

struct afs_cacheOps afs_UfsCacheOps = {
#ifndef HAVE_STRUCT_LABEL_SUPPORT
    osi_UFSOpen,
    osi_UFSTruncate,
    afs_osi_Read,
    afs_osi_Write,
    osi_UFSClose,
    afs_UFSRead,
    afs_UFSWrite,
    afs_UFSGetDSlot,
    afs_UFSGetVolSlot,
    afs_UFSHandleLink,
#else
    .open 	= osi_UFSOpen,
    .truncate	= osi_UFSTruncate,
    .fread	= afs_osi_Read,
    .fwrite	= afs_osi_Write,
    .close	= osi_UFSClose,
    .vread	= afs_UFSRead,
    .vwrite	= afs_UFSWrite,
    .GetDSlot	= afs_UFSGetDSlot,
    .GetVolSlot = afs_UFSGetVolSlot,
    .HandleLink	= afs_UFSHandleLink,
#endif
};

struct afs_cacheOps afs_MemCacheOps = {
#ifndef HAVE_STRUCT_LABEL_SUPPORT
    afs_MemCacheOpen,
    afs_MemCacheTruncate,
    afs_MemReadBlk,
    afs_MemWriteBlk,
    afs_MemCacheClose,
    afs_MemRead,
    afs_MemWrite,
    afs_MemGetDSlot,
    afs_MemGetVolSlot,
    afs_MemHandleLink,
#else
    .open	= afs_MemCacheOpen,
    .truncate	= afs_MemCacheTruncate,
    .fread	= afs_MemReadBlk,
    .fwrite	= afs_MemWriteBlk,
    .close	= afs_MemCacheClose,
    .vread	= afs_MemRead,
    .vwrite	= afs_MemWrite,
    .GetDSlot	= afs_MemGetDSlot,
    .GetVolSlot	= afs_MemGetVolSlot,
    .HandleLink	= afs_MemHandleLink,
#endif
};

int cacheDiskType;		/*Type of backing disk for cache */
struct afs_cacheOps *afs_cacheType;

/*!
 * Where is this vcache's entry associated dcache located/
 * \param avc The vcache entry.
 * \return Bucket index:
 *  	1 : main
 *  	2 : RO
 */
static afs_int32
afs_DCGetBucket(struct vcache *avc)
{
    if (!splitdcache)
	return 1;

    /* This should be replaced with some sort of user configurable function */
    if (avc->f.states & CRO) {
	return 2;
    } else if (avc->f.states & CBackup) {
	return 1;
    } else {
	/* RW */
    }
    /* main bucket */
    return 1;
}

/*!
 * Readjust a dcache's size.
 *
 * \param adc The dcache to be adjusted.
 * \param oldSize Old size for the dcache.
 * \param newSize The new size to be adjusted to.
 *
 */
static void
afs_DCAdjustSize(struct dcache *adc, afs_int32 oldSize, afs_int32 newSize)
{
    afs_int32 adjustSize = newSize - oldSize;

    if (!splitdcache)
	return;

    switch (adc->bucket)
    {
    case 0:
	afs_blocksUsed_0 += adjustSize;
	afs_stats_cmperf.cacheBucket0_Discarded += oldSize;
	break;
    case 1:
	afs_blocksUsed_1 += adjustSize;
	afs_stats_cmperf.cacheBucket1_Discarded += oldSize;
	break;
    case 2:
	afs_blocksUsed_2 += adjustSize;
	afs_stats_cmperf.cacheBucket2_Discarded += oldSize;
	break;
    }

    return;
}

/*!
 * Move a dcache from one bucket to another.
 *
 * \param adc Operate on this dcache.
 * \param size Size in bucket (?).
 * \param newBucket Destination bucket.
 *
 */
static void
afs_DCMoveBucket(struct dcache *adc, afs_int32 size, afs_int32 newBucket)
{
    if (!splitdcache)
	return;

    /* Substract size from old bucket. */
    switch (adc->bucket)
    {
    case 0:
	afs_blocksUsed_0 -= size;
	break;
    case 1:
	afs_blocksUsed_1 -= size;
	break;
    case 2:
	afs_blocksUsed_2 -= size;
	break;
    }

    /* Set new bucket and increase destination bucket size. */
    adc->bucket = newBucket;

    switch (adc->bucket)
    {
    case 0:
	afs_blocksUsed_0 += size;
	break;
    case 1:
	afs_blocksUsed_1 += size;
	break;
    case 2:
	afs_blocksUsed_2 += size;
	break;
    }

    return;
}

/*!
 * Init split caches size.
 */
static void
afs_DCSizeInit(void)
{
    afs_blocksUsed_0 = afs_blocksUsed_1 = afs_blocksUsed_2 = 0;
}


/*!
 * \param phase
 * \param bucket
 */
static afs_int32
afs_DCWhichBucket(afs_int32 phase, afs_int32 bucket)
{
    if (!splitdcache)
	return 0;

    afs_pct1 = afs_blocksUsed_1 / (afs_cacheBlocks / 100);
    afs_pct2 = afs_blocksUsed_2 / (afs_cacheBlocks / 100);

    /* Short cut: if we don't know about it, try to kill it */
    if (phase < 2 && afs_blocksUsed_0)
	return 0;

    if (afs_pct1 > afs_tpct1)
	return 1;
    if (afs_pct2 > afs_tpct2)
	return 2;
    return 0; /* unlikely */
}


/*!
 * Warn about failing to store a file.
 *
 * \param acode Associated error code.
 * \param avolume Volume involved.
 * \param aflags How to handle the output:
 *	aflags & 1: Print out on console
 *	aflags & 2: Print out on controlling tty
 *
 * \note Environment: Call this from close call when vnodeops is RCS unlocked.
 */

void
afs_StoreWarn(afs_int32 acode, afs_int32 avolume,
	      afs_int32 aflags)
{
    static char problem_fmt[] =
	"afs: failed to store file in volume %d (%s)\n";
    static char problem_fmt_w_error[] =
	"afs: failed to store file in volume %d (error %d)\n";
    static char netproblems[] = "network problems";
    static char partfull[] = "partition full";
    static char overquota[] = "over quota";

    AFS_STATCNT(afs_StoreWarn);
    if (acode < 0) {
	/*
	 * Network problems
	 */
	if (aflags & 1)
	    afs_warn(problem_fmt, avolume, netproblems);
	if (aflags & 2)
	    afs_warnuser(problem_fmt, avolume, netproblems);
    } else if (acode == ENOSPC) {
	/*
	 * Partition full
	 */
	if (aflags & 1)
	    afs_warn(problem_fmt, avolume, partfull);
	if (aflags & 2)
	    afs_warnuser(problem_fmt, avolume, partfull);
    } else
#ifdef	EDQUOT
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
}				/*afs_StoreWarn */

/*!
 * Try waking up truncation daemon, if it's worth it.
 */
void
afs_MaybeWakeupTruncateDaemon(void)
{
    if (!afs_CacheTooFull && afs_CacheIsTooFull()) {
	afs_CacheTooFull = 1;
	if (!afs_TruncateDaemonRunning)
	    afs_osi_Wakeup((int *)afs_CacheTruncateDaemon);
    } else if (!afs_TruncateDaemonRunning
	       && afs_blocksDiscarded > CM_MAXDISCARDEDCHUNKS) {
	afs_osi_Wakeup((int *)afs_CacheTruncateDaemon);
    }
}

/*!
 * /struct CTD_stats
 *
 * Keep statistics on run time for afs_CacheTruncateDaemon. This is a
 * struct so we need only export one symbol for AIX.
 */
static struct CTD_stats {
    osi_timeval_t CTD_beforeSleep;
    osi_timeval_t CTD_afterSleep;
    osi_timeval_t CTD_sleepTime;
    osi_timeval_t CTD_runTime;
    int CTD_nSleeps;
} CTD_stats;

u_int afs_min_cache = 0;

/*!
 * If there are waiters for the cache to drain, wake them if
 * the number of free cache blocks reaches the CM_CACHESIZEDDRAINEDPCT.
 *
 * \note Environment:
 *	This routine must be called with the afs_xdcache lock held
 *	(in write mode).
 */
static void
afs_WakeCacheWaitersIfDrained(void)
{
    if (afs_WaitForCacheDrain) {
	if ((afs_blocksUsed - afs_blocksDiscarded) <=
	    PERCENT(CM_CACHESIZEDRAINEDPCT, afs_cacheBlocks)) {
	    afs_WaitForCacheDrain = 0;
	    afs_osi_Wakeup(&afs_WaitForCacheDrain);
	}
    }
}

/*!
 * Keeps the cache clean and free by truncating uneeded files, when used.
 * \param
 * \return
 */
void
afs_CacheTruncateDaemon(void)
{
    osi_timeval_t CTD_tmpTime;
    u_int counter;
    u_int cb_lowat;
    u_int dc_hiwat =
	PERCENT((100 - CM_DCACHECOUNTFREEPCT + CM_DCACHEEXTRAPCT), afs_cacheFiles);
    afs_min_cache =
	(((10 * AFS_CHUNKSIZE(0)) + afs_fsfragsize) & ~afs_fsfragsize) >> 10;

    osi_GetuTime(&CTD_stats.CTD_afterSleep);
    afs_TruncateDaemonRunning = 1;
    while (1) {
	cb_lowat = PERCENT((CM_DCACHESPACEFREEPCT - CM_DCACHEEXTRAPCT), afs_cacheBlocks);
	ObtainWriteLock(&afs_xdcache, 266);
	if (afs_CacheTooFull || afs_WaitForCacheDrain) {
	    int space_needed, slots_needed;
	    /* if we get woken up, we should try to clean something out */
	    for (counter = 0; counter < 10; counter++) {
		space_needed =
		    afs_blocksUsed - afs_blocksDiscarded - cb_lowat;
		if (space_needed < 0)
		    space_needed = 0;
		slots_needed =
		    dc_hiwat - afs_freeDCCount - afs_discardDCCount;
		if (slots_needed < 0)
		    slots_needed = 0;
		if (slots_needed || space_needed)
		    afs_GetDownD(slots_needed, &space_needed, 0);
		if ((space_needed <= 0) && (slots_needed <= 0)) {
		    afs_CacheTooFull = 0;
		    break;
		}
		if (afs_termState == AFSOP_STOP_TRUNCDAEMON)
		    break;
	    }
	    if (!afs_CacheIsTooFull()) {
		afs_CacheTooFull = 0;
		afs_WakeCacheWaitersIfDrained();
	    }
	}	/* end of cache cleanup */
	ReleaseWriteLock(&afs_xdcache);

	/*
	 * This is a defensive check to try to avoid starving threads
	 * that may need the global lock so thay can help free some
	 * cache space. If this thread won't be sleeping or truncating
	 * any cache files then give up the global lock so other
	 * threads get a chance to run.
	 */
	if ((afs_termState != AFSOP_STOP_TRUNCDAEMON) && afs_CacheTooFull
	    && (!afs_blocksDiscarded || afs_WaitForCacheDrain)) {
	    afs_osi_Wait(100, 0, 0);	/* 100 milliseconds */
	}

	/*
	 * This is where we free the discarded cache elements.
	 */
	while (afs_blocksDiscarded && !afs_WaitForCacheDrain
	       && (afs_termState != AFSOP_STOP_TRUNCDAEMON)) {
	    int code = afs_FreeDiscardedDCache();
	    if (code) {
		/* If we can't free any discarded dcache entries, that's okay.
		 * We're just doing this in the background; if someone needs
		 * discarded entries freed, they will try it themselves and/or
		 * signal us that the cache is too full. In any case, we'll
		 * try doing this again the next time we run through the loop.
		 */
		break;
	    }
	}

	/* See if we need to continue to run. Someone may have
	 * signalled us while we were executing.
	 */
	if (!afs_WaitForCacheDrain && !afs_CacheTooFull
	    && (afs_termState != AFSOP_STOP_TRUNCDAEMON)) {
	    /* Collect statistics on truncate daemon. */
	    CTD_stats.CTD_nSleeps++;
	    osi_GetuTime(&CTD_stats.CTD_beforeSleep);
	    afs_stats_GetDiff(CTD_tmpTime, CTD_stats.CTD_afterSleep,
			      CTD_stats.CTD_beforeSleep);
	    afs_stats_AddTo(CTD_stats.CTD_runTime, CTD_tmpTime);

	    afs_TruncateDaemonRunning = 0;
	    afs_osi_Sleep((int *)afs_CacheTruncateDaemon);
	    afs_TruncateDaemonRunning = 1;

	    osi_GetuTime(&CTD_stats.CTD_afterSleep);
	    afs_stats_GetDiff(CTD_tmpTime, CTD_stats.CTD_beforeSleep,
			      CTD_stats.CTD_afterSleep);
	    afs_stats_AddTo(CTD_stats.CTD_sleepTime, CTD_tmpTime);
	}
	if (afs_termState == AFSOP_STOP_TRUNCDAEMON) {
	    afs_termState = AFSOP_STOP_AFSDB;
	    afs_osi_Wakeup(&afs_termState);
	    break;
	}
    }
}


/*!
 * Make adjustment for the new size in the disk cache entry
 *
 * \note Major Assumptions Here:
 *      Assumes that frag size is an integral power of two, less one,
 *      and that this is a two's complement machine.  I don't
 *      know of any filesystems which violate this assumption...
 *
 * \param adc Ptr to dcache entry.
 * \param anewsize New size desired.
 *
 */

void
afs_AdjustSize(struct dcache *adc, afs_int32 newSize)
{
    afs_int32 oldSize;

    AFS_STATCNT(afs_AdjustSize);

    if (newSize > afs_OtherCSize && !(adc->f.fid.Fid.Vnode & 1)) {
        /* No non-dir cache files should be larger than the chunk size.
         * (Directory blobs are fetched in a single chunk file, so directories
         * can be larger.) If someone is requesting that a chunk is larger than
         * the chunk size, something strange is happening. Log a message about
         * it, to give a hint to subsequent strange behavior, if any occurs. */
        static int warned;
        if (!warned) {
            warned = 1;
            afs_warn("afs: Warning: dcache %d is very large (%d > %d). This "
                     "should not happen, but trying to continue regardless. If "
                     "AFS starts hanging or behaving strangely, this might be "
                     "why.\n",
                     adc->index, newSize, afs_OtherCSize);
        }
    }

    adc->dflags |= DFEntryMod;
    oldSize = ((adc->f.chunkBytes + afs_fsfragsize) ^ afs_fsfragsize) >> 10;	/* round up */
    adc->f.chunkBytes = newSize;
    if (!newSize)
	adc->validPos = 0;
    newSize = ((newSize + afs_fsfragsize) ^ afs_fsfragsize) >> 10;	/* round up */
    afs_DCAdjustSize(adc, oldSize, newSize);
    if ((newSize > oldSize) && !AFS_IS_DISCONNECTED) {

	/* We're growing the file, wakeup the daemon */
	afs_MaybeWakeupTruncateDaemon();
    }
    afs_blocksUsed += (newSize - oldSize);
    afs_stats_cmperf.cacheBlocksInUse = afs_blocksUsed;	/* XXX */
}


/*!
 * This routine is responsible for moving at least one entry (but up
 * to some number of them) from the LRU queue to the free queue.
 *
 * \param anumber Number of entries that should ideally be moved.
 * \param aneedSpace How much space we need (1K blocks);
 *
 * \note Environment:
 *	The anumber parameter is just a hint; at least one entry MUST be
 *	moved, or we'll panic.  We must be called with afs_xdcache
 *	write-locked.  We should try to satisfy both anumber and aneedspace,
 *      whichever is more demanding - need to do several things:
 *      1.  only grab up to anumber victims if aneedSpace <= 0, not
 *          the whole set of MAXATONCE.
 *      2.  dynamically choose MAXATONCE to reflect severity of
 *          demand: something like (*aneedSpace >> (logChunk - 9))
 *
 *  \note N.B. if we're called with aneedSpace <= 0 and anumber > 0, that
 *  indicates that the cache is not properly configured/tuned or
 *  something. We should be able to automatically correct that problem.
 */

#define	MAXATONCE   16		/* max we can obtain at once */
static void
afs_GetDownD(int anumber, int *aneedSpace, afs_int32 buckethint)
{

    struct dcache *tdc;
    struct VenusFid *afid;
    afs_int32 i, j;
    afs_hyper_t vtime;
    int skip, phase;
    struct vcache *tvc;
    afs_uint32 victims[MAXATONCE];
    struct dcache *victimDCs[MAXATONCE];
    afs_hyper_t victimTimes[MAXATONCE];	/* youngest (largest LRU time) first */
    afs_uint32 victimPtr;	/* next free item in victim arrays */
    afs_hyper_t maxVictimTime;	/* youngest (largest LRU time) victim */
    afs_uint32 maxVictimPtr;	/* where it is */
    int discard;
    int curbucket;

    AFS_STATCNT(afs_GetDownD);

    if (CheckLock(&afs_xdcache) != -1)
	osi_Panic("getdownd nolock");
    /* decrement anumber first for all dudes in free list */
    /* SHOULD always decrement anumber first, even if aneedSpace >0,
     * because we should try to free space even if anumber <=0 */
    if (!aneedSpace || *aneedSpace <= 0) {
	anumber -= afs_freeDCCount;
	if (anumber <= 0) {
	    return;		/* enough already free */
	}
    }

    /* bounds check parameter */
    if (anumber > MAXATONCE)
	anumber = MAXATONCE;	/* all we can do */

    /* rewrite so phases include a better eligiblity for gc test*/
    /*
     * The phase variable manages reclaims.  Set to 0, the first pass,
     * we don't reclaim active entries, or other than target bucket.
     * Set to 1, we reclaim even active ones in target bucket.
     * Set to 2, we reclaim any inactive one.
     * Set to 3, we reclaim even active ones. On Solaris, we also reclaim
     * entries whose corresponding vcache has a nonempty multiPage list, when
     * possible.
     */
    if (splitdcache) {
	phase = 0;
    } else {
	phase = 4;
    }

    for (i = 0; i < afs_cacheFiles; i++)
	/* turn off all flags */
	afs_indexFlags[i] &= ~IFFlag;

    while (anumber > 0 || (aneedSpace && *aneedSpace > 0)) {
	/* find oldest entries for reclamation */
	maxVictimPtr = victimPtr = 0;
	hzero(maxVictimTime);
	curbucket = afs_DCWhichBucket(phase, buckethint);
	/* select victims from access time array */
	for (i = 0; i < afs_cacheFiles; i++) {
	    if (afs_indexFlags[i] & (IFDataMod | IFFree | IFDiscarded)) {
		/* skip if dirty or already free */
		continue;
	    }
	    tdc = afs_indexTable[i];
	    if (tdc && (curbucket != tdc->bucket) && (phase < 4))
	    {
		/* Wrong bucket; can't use it! */
	        continue;
	    }
	    if (tdc && (tdc->refCount != 0)) {
		/* Referenced; can't use it! */
		continue;
	    }
	    hset(vtime, afs_indexTimes[i]);

	    /* if we've already looked at this one, skip it */
	    if (afs_indexFlags[i] & IFFlag)
		continue;

	    if (victimPtr < MAXATONCE) {
		/* if there's at least one free victim slot left */
		victims[victimPtr] = i;
		hset(victimTimes[victimPtr], vtime);
		if (hcmp(vtime, maxVictimTime) > 0) {
		    hset(maxVictimTime, vtime);
		    maxVictimPtr = victimPtr;
		}
		victimPtr++;
	    } else if (hcmp(vtime, maxVictimTime) < 0) {
		/*
		 * We're older than youngest victim, so we replace at
		 * least one victim
		 */
		/* find youngest (largest LRU) victim */
		j = maxVictimPtr;
		if (j == victimPtr)
		    osi_Panic("getdownd local");
		victims[j] = i;
		hset(victimTimes[j], vtime);
		/* recompute maxVictimTime */
		hset(maxVictimTime, vtime);
		for (j = 0; j < victimPtr; j++)
		    if (hcmp(maxVictimTime, victimTimes[j]) < 0) {
			hset(maxVictimTime, victimTimes[j]);
			maxVictimPtr = j;
		    }
	    }
	}			/* big for loop */

	/* now really reclaim the victims */
	j = 0;			/* flag to track if we actually got any of the victims */
	/* first, hold all the victims, since we're going to release the lock
	 * during the truncate operation.
	 */
	for (i = 0; i < victimPtr; i++) {
	    tdc = afs_GetValidDSlot(victims[i]);
	    /* We got tdc->tlock(R) here */
	    if (tdc && tdc->refCount == 1)
		victimDCs[i] = tdc;
	    else
		victimDCs[i] = 0;
	    if (tdc) {
		ReleaseReadLock(&tdc->tlock);
		if (!victimDCs[i])
		    afs_PutDCache(tdc);
	    }
	}
	for (i = 0; i < victimPtr; i++) {
	    /* q is first elt in dcache entry */
	    tdc = victimDCs[i];
	    /* now, since we're dropping the afs_xdcache lock below, we
	     * have to verify, before proceeding, that there are no other
	     * references to this dcache entry, even now.  Note that we
	     * compare with 1, since we bumped it above when we called
	     * afs_GetValidDSlot to preserve the entry's identity.
	     */
	    if (tdc && tdc->refCount == 1) {
		unsigned char chunkFlags;
		afs_size_t tchunkoffset = 0;
		afid = &tdc->f.fid;
		/* xdcache is lower than the xvcache lock */
		ReleaseWriteLock(&afs_xdcache);
		ObtainReadLock(&afs_xvcache);
		tvc = afs_FindVCache(afid, 0, 0 /* no stats, no vlru */ );
		ReleaseReadLock(&afs_xvcache);
		ObtainWriteLock(&afs_xdcache, 527);
		skip = 0;
		if (tdc->refCount > 1)
		    skip = 1;
		if (tvc) {
		    tchunkoffset = AFS_CHUNKTOBASE(tdc->f.chunk);
		    chunkFlags = afs_indexFlags[tdc->index];
		    if (((phase & 1) == 0) && osi_Active(tvc))
                        skip = 1;
		    if (((phase & 1) == 1) && osi_Active(tvc)
                        && (tvc->f.states & CDCLock)
                        && (chunkFlags & IFAnyPages))
                        skip = 1;
		    if (chunkFlags & IFDataMod)
			skip = 1;
		    afs_Trace4(afs_iclSetp, CM_TRACE_GETDOWND,
			       ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32, skip,
			       ICL_TYPE_INT32, tdc->index, ICL_TYPE_OFFSET,
			       ICL_HANDLE_OFFSET(tchunkoffset));

#if	defined(AFS_SUN5_ENV)
		    /*
		     * Now we try to invalidate pages.  We do this only for
		     * Solaris.  For other platforms, it's OK to recycle a
		     * dcache entry out from under a page, because the strategy
		     * function can call afs_GetDCache().
		     */
		    if (!skip && (chunkFlags & IFAnyPages)) {
			int code;

			ReleaseWriteLock(&afs_xdcache);
			ObtainWriteLock(&tvc->vlock, 543);
			if (!QEmpty(&tvc->multiPage)) {
			    if (phase < 3 || osi_VM_MultiPageConflict(tvc, tdc)) {
				skip = 1;
				goto endmultipage;
			    }
			}
			/* block locking pages */
			tvc->vstates |= VPageCleaning;
			/* block getting new pages */
			tvc->activeV++;
			ReleaseWriteLock(&tvc->vlock);
			/* One last recheck */
			ObtainWriteLock(&afs_xdcache, 333);
			chunkFlags = afs_indexFlags[tdc->index];
			if (tdc->refCount > 1 || (chunkFlags & IFDataMod)
			    || (osi_Active(tvc) && (tvc->f.states & CDCLock)
				&& (chunkFlags & IFAnyPages))) {
			    skip = 1;
			    ReleaseWriteLock(&afs_xdcache);
			    goto endputpage;
			}
			ReleaseWriteLock(&afs_xdcache);

			code = osi_VM_GetDownD(tvc, tdc);

			ObtainWriteLock(&afs_xdcache, 269);
			/* we actually removed all pages, clean and dirty */
			if (code == 0) {
			    afs_indexFlags[tdc->index] &=
				~(IFDirtyPages | IFAnyPages);
			} else
			    skip = 1;
			ReleaseWriteLock(&afs_xdcache);
		      endputpage:
			ObtainWriteLock(&tvc->vlock, 544);
			if (--tvc->activeV == 0
			    && (tvc->vstates & VRevokeWait)) {
			    tvc->vstates &= ~VRevokeWait;
			    afs_osi_Wakeup((char *)&tvc->vstates);

			}
			if (tvc->vstates & VPageCleaning) {
			    tvc->vstates &= ~VPageCleaning;
			    afs_osi_Wakeup((char *)&tvc->vstates);
			}
		      endmultipage:
			ReleaseWriteLock(&tvc->vlock);
		    } else
#endif /* AFS_SUN5_ENV */
		    {
			ReleaseWriteLock(&afs_xdcache);
		    }

		    afs_PutVCache(tvc); /*XXX was AFS_FAST_RELE?*/
		    ObtainWriteLock(&afs_xdcache, 528);
		    if (afs_indexFlags[tdc->index] &
			(IFDataMod | IFDirtyPages | IFAnyPages))
			skip = 1;
		    if (tdc->refCount > 1)
			skip = 1;
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
		    afs_indexFlags[tdc->index] &=
			~(IFDirtyPages | IFAnyPages);
		}
#endif
		if (skip) {
		    /* skip this guy and mark him as recently used */
		    afs_indexFlags[tdc->index] |= IFFlag;
		    afs_Trace4(afs_iclSetp, CM_TRACE_GETDOWND,
			       ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32, 2,
			       ICL_TYPE_INT32, tdc->index, ICL_TYPE_OFFSET,
			       ICL_HANDLE_OFFSET(tchunkoffset));
		} else {
		    /* flush this dude from the data cache and reclaim;
		     * first, make sure no one will care that we damage
		     * it, by removing it from all hash tables.  Then,
		     * melt it down for parts.  Note that any concurrent
		     * (new possibility!) calls to GetDownD won't touch
		     * this guy because his reference count is > 0. */
		    afs_Trace4(afs_iclSetp, CM_TRACE_GETDOWND,
			       ICL_TYPE_POINTER, tvc, ICL_TYPE_INT32, 3,
			       ICL_TYPE_INT32, tdc->index, ICL_TYPE_OFFSET,
			       ICL_HANDLE_OFFSET(tchunkoffset));
		    AFS_STATCNT(afs_gget);
		    afs_HashOutDCache(tdc, 1);
		    if (tdc->f.chunkBytes != 0) {
			discard = 1;
			if (aneedSpace)
			    *aneedSpace -=
				(tdc->f.chunkBytes + afs_fsfragsize) >> 10;
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
	    if (tdc)
		afs_PutDCache(tdc);
	} 			/* end of for victims loop */

	if (phase < 5) {
	    /* Phase is 0 and no one was found, so try phase 1 (ignore
	     * osi_Active flag) */
	    if (j == 0) {
		phase++;
		for (i = 0; i < afs_cacheFiles; i++)
		    /* turn off all flags */
		    afs_indexFlags[i] &= ~IFFlag;
	    }
	} else {
	    /* found no one in phases 0-5, we're hosed */
	    if (victimPtr == 0)
		break;
	}
    }				/* big while loop */

    return;

}				/*afs_GetDownD */


/*!
 * Remove adc from any hash tables that would allow it to be located
 * again by afs_FindDCache or afs_GetDCache.
 *
 * \param adc Pointer to dcache entry to remove from hash tables.
 *
 * \note Locks: Must have the afs_xdcache lock write-locked to call this function.
 *
 */
int
afs_HashOutDCache(struct dcache *adc, int zap)
{
    int i, us;

    AFS_STATCNT(afs_glink);
    if (zap)
	/* we know this guy's in the LRUQ.  We'll move dude into DCQ below */
	DZap(adc);
    /* if this guy is in the hash table, pull him out */
    if (adc->f.fid.Fid.Volume != 0) {
	/* remove entry from first hash chains */
	i = DCHash(&adc->f.fid, adc->f.chunk);
	us = afs_dchashTbl[i];
	if (us == adc->index) {
	    /* first dude in the list */
	    afs_dchashTbl[i] = afs_dcnextTbl[adc->index];
	} else {
	    /* somewhere on the chain */
	    while (us != NULLIDX) {
		if (afs_dcnextTbl[us] == adc->index) {
		    /* found item pointing at the one to delete */
		    afs_dcnextTbl[us] = afs_dcnextTbl[adc->index];
		    break;
		}
		us = afs_dcnextTbl[us];
	    }
	    if (us == NULLIDX)
		osi_Panic("dcache hc");
	}
	/* remove entry from *other* hash chain */
	i = DVHash(&adc->f.fid);
	us = afs_dvhashTbl[i];
	if (us == adc->index) {
	    /* first dude in the list */
	    afs_dvhashTbl[i] = afs_dvnextTbl[adc->index];
	} else {
	    /* somewhere on the chain */
	    while (us != NULLIDX) {
		if (afs_dvnextTbl[us] == adc->index) {
		    /* found item pointing at the one to delete */
		    afs_dvnextTbl[us] = afs_dvnextTbl[adc->index];
		    break;
		}
		us = afs_dvnextTbl[us];
	    }
	    if (us == NULLIDX)
		osi_Panic("dcache hv");
	}
    }

    if (zap) {
    	/* prevent entry from being found on a reboot (it is already out of
     	 * the hash table, but after a crash, we just look at fid fields of
     	 * stable (old) entries).
     	 */
    	 adc->f.fid.Fid.Volume = 0;	/* invalid */

    	/* mark entry as modified */
    	adc->dflags |= DFEntryMod;
    }

    /* all done */
    return 0;
}				/*afs_HashOutDCache */

/*!
 * Flush the given dcache entry, pulling it from hash chains
 * and truncating the associated cache file.
 *
 * \param adc Ptr to dcache entry to flush.
 *
 * \note Environment:
 *	This routine must be called with the afs_xdcache lock held
 *	(in write mode).
 */
void
afs_FlushDCache(struct dcache *adc)
{
    AFS_STATCNT(afs_FlushDCache);
    /*
     * Bump the number of cache files flushed.
     */
    afs_stats_cmperf.cacheFlushes++;

    /* remove from all hash tables */
    afs_HashOutDCache(adc, 1);

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
}				/*afs_FlushDCache */


/*!
 * Put a dcache entry on the free dcache entry list.
 *
 * \param adc dcache entry to free.
 *
 * \note Environment: called with afs_xdcache lock write-locked.
 */
static void
afs_FreeDCache(struct dcache *adc)
{
    /* Thread on free list, update free list count and mark entry as
     * freed in its indexFlags element.  Also, ensure DCache entry gets
     * written out (set DFEntryMod).
     */

    afs_dvnextTbl[adc->index] = afs_freeDCList;
    afs_freeDCList = adc->index;
    afs_freeDCCount++;
    afs_indexFlags[adc->index] |= IFFree;
    adc->dflags |= DFEntryMod;

    afs_WakeCacheWaitersIfDrained();
}				/* afs_FreeDCache */

/*!
 * Discard the cache element by moving it to the discardDCList.
 * This puts the cache element into a quasi-freed state, where
 * the space may be reused, but the file has not been truncated.
 *
 * \note Major Assumptions Here:
 *      Assumes that frag size is an integral power of two, less one,
 *      and that this is a two's complement machine.  I don't
 *      know of any filesystems which violate this assumption...
 *
 * \param adr Ptr to dcache entry.
 *
 * \note Environment:
 *	Must be called with afs_xdcache write-locked.
 */

static void
afs_DiscardDCache(struct dcache *adc)
{
    afs_int32 size;

    AFS_STATCNT(afs_DiscardDCache);

    osi_Assert(adc->refCount == 1);

    size = ((adc->f.chunkBytes + afs_fsfragsize) ^ afs_fsfragsize) >> 10;	/* round up */
    afs_blocksDiscarded += size;
    afs_stats_cmperf.cacheBlocksDiscarded = afs_blocksDiscarded;

    afs_dvnextTbl[adc->index] = afs_discardDCList;
    afs_discardDCList = adc->index;
    afs_discardDCCount++;

    adc->f.fid.Fid.Volume = 0;
    adc->dflags |= DFEntryMod;
    afs_indexFlags[adc->index] |= IFDiscarded;

    afs_WakeCacheWaitersIfDrained();
}				/*afs_DiscardDCache */

/**
 * Get a dcache entry from the discard or free list
 *
 * @param[in] indexp  A pointer to the head of the dcache free list or discard
 *                    list (afs_freeDCList, or afs_discardDCList)
 *
 * @return A dcache from that list, or NULL if none could be retrieved.
 *
 * @pre afs_xdcache is write-locked
 */
static struct dcache *
afs_GetDSlotFromList(afs_int32 *indexp)
{
    struct dcache *tdc;

    for ( ; *indexp != NULLIDX; indexp = &afs_dvnextTbl[*indexp]) {
	tdc = afs_GetUnusedDSlot(*indexp);
	if (tdc) {
	    osi_Assert(tdc->refCount == 1);
	    ReleaseReadLock(&tdc->tlock);
	    *indexp = afs_dvnextTbl[tdc->index];
	    afs_dvnextTbl[tdc->index] = NULLIDX;
	    return tdc;
	}
    }
    return NULL;
}

/*!
 * Free the next element on the list of discarded cache elements.
 *
 * Returns -1 if we encountered an error preventing us from freeing a
 * discarded dcache, or 0 on success.
 */
static int
afs_FreeDiscardedDCache(void)
{
    struct dcache *tdc;
    struct osi_file *tfile;
    afs_int32 size;

    AFS_STATCNT(afs_FreeDiscardedDCache);

    ObtainWriteLock(&afs_xdcache, 510);
    if (!afs_blocksDiscarded) {
	ReleaseWriteLock(&afs_xdcache);
	return 0;
    }

    /*
     * Get an entry from the list of discarded cache elements
     */
    tdc = afs_GetDSlotFromList(&afs_discardDCList);
    if (!tdc) {
	ReleaseWriteLock(&afs_xdcache);
	return -1;
    }

    afs_discardDCCount--;
    size = ((tdc->f.chunkBytes + afs_fsfragsize) ^ afs_fsfragsize) >> 10;	/* round up */
    afs_blocksDiscarded -= size;
    afs_stats_cmperf.cacheBlocksDiscarded = afs_blocksDiscarded;
    /* We can lock because we just took it off the free list */
    ObtainWriteLock(&tdc->lock, 626);
    ReleaseWriteLock(&afs_xdcache);

    /*
     * Truncate the element to reclaim its space
     */
    tfile = afs_CFileOpen(&tdc->f.inode);
    afs_CFileTruncate(tfile, 0);
    afs_CFileClose(tfile);
    afs_AdjustSize(tdc, 0);
    afs_DCMoveBucket(tdc, 0, 0);

    /*
     * Free the element we just truncated
     */
    ObtainWriteLock(&afs_xdcache, 511);
    afs_indexFlags[tdc->index] &= ~IFDiscarded;
    afs_FreeDCache(tdc);
    tdc->f.states &= ~(DRO|DBackup|DRW);
    ReleaseWriteLock(&tdc->lock);
    afs_PutDCache(tdc);
    ReleaseWriteLock(&afs_xdcache);

    return 0;
}

/*!
 * Free as many entries from the list of discarded cache elements
 * as we need to get the free space down below CM_WAITFORDRAINPCT (98%).
 *
 * \return 0
 */
int
afs_MaybeFreeDiscardedDCache(void)
{

    AFS_STATCNT(afs_MaybeFreeDiscardedDCache);

    while (afs_blocksDiscarded
	   && (afs_blocksUsed >
	       PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks))) {
	int code = afs_FreeDiscardedDCache();
	if (code) {
	    /* Callers depend on us to get the afs_blocksDiscarded count down.
	     * If we cannot do that, the callers can spin by calling us over
	     * and over. Panic for now until we can figure out something
	     * better. */
	    osi_Panic("Error freeing discarded dcache");
	}
    }
    return 0;
}

/*!
 * Try to free up a certain number of disk slots.
 *
 * \param anumber Targeted number of disk slots to free up.
 *
 * \note Environment:
 *	Must be called with afs_xdcache write-locked.
 *
 */
static void
afs_GetDownDSlot(int anumber)
{
    struct afs_q *tq, *nq;
    struct dcache *tdc;
    int ix;
    unsigned int cnt;

    AFS_STATCNT(afs_GetDownDSlot);
    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
	osi_Panic("diskless getdowndslot");

    if (CheckLock(&afs_xdcache) != -1)
	osi_Panic("getdowndslot nolock");

    /* decrement anumber first for all dudes in free list */
    for (tdc = afs_freeDSList; tdc; tdc = (struct dcache *)tdc->lruq.next)
	anumber--;
    if (anumber <= 0)
	return;			/* enough already free */

    for (cnt = 0, tq = afs_DLRU.prev; tq != &afs_DLRU && anumber > 0;
	 tq = nq, cnt++) {
	tdc = (struct dcache *)tq;	/* q is first elt in dcache entry */
	nq = QPrev(tq);		/* in case we remove it */
	if (tdc->refCount == 0) {
	    if ((ix = tdc->index) == NULLIDX)
		osi_Panic("getdowndslot");
	    /* pull the entry out of the lruq and put it on the free list */
	    QRemove(&tdc->lruq);

	    /* write-through if modified */
	    if (tdc->dflags & DFEntryMod) {
#if defined(AFS_SGI_ENV) && defined(AFS_SGI_SHORTSTACK)
		/*
		 * ask proxy to do this for us - we don't have the stack space
		 */
		while (tdc->dflags & DFEntryMod) {
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
		tdc->dflags &= ~DFEntryMod;
		osi_Assert(afs_WriteDCache(tdc, 1) == 0);
#endif
	    }

	    /* finally put the entry in the free list */
	    afs_indexTable[ix] = NULL;
	    afs_indexFlags[ix] &= ~IFEverUsed;
	    tdc->index = NULLIDX;
	    tdc->lruq.next = (struct afs_q *)afs_freeDSList;
	    afs_freeDSList = tdc;
	    anumber--;
	}
    }
}				/*afs_GetDownDSlot */


/*
 * afs_RefDCache
 *
 * Description:
 *	Increment the reference count on a disk cache entry,
 *	which already has a non-zero refcount.  In order to
 *	increment the refcount of a zero-reference entry, you
 *	have to hold afs_xdcache.
 *
 * Parameters:
 *	adc : Pointer to the dcache entry to increment.
 *
 * Environment:
 *	Nothing interesting.
 */
int
afs_RefDCache(struct dcache *adc)
{
    ObtainWriteLock(&adc->tlock, 627);
    if (adc->refCount < 0)
	osi_Panic("RefDCache: negative refcount");
    adc->refCount++;
    ReleaseWriteLock(&adc->tlock);
    return 0;
}


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
int
afs_PutDCache(struct dcache *adc)
{
    AFS_STATCNT(afs_PutDCache);
    ObtainWriteLock(&adc->tlock, 276);
    if (adc->refCount <= 0)
	osi_Panic("putdcache");
    --adc->refCount;
    ReleaseWriteLock(&adc->tlock);
    return 0;
}


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
afs_TryToSmush(struct vcache *avc, afs_ucred_t *acred, int sync)
{
    struct dcache *tdc;
    int index;
    int i;
    AFS_STATCNT(afs_TryToSmush);
    afs_Trace2(afs_iclSetp, CM_TRACE_TRYTOSMUSH, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_OFFSET, ICL_HANDLE_OFFSET(avc->f.m.Length));
    sync = 1;			/* XX Temp testing XX */

#if     defined(AFS_SUN5_ENV)
    ObtainWriteLock(&avc->vlock, 573);
    avc->activeV++;		/* block new getpages */
    ReleaseWriteLock(&avc->vlock);
#endif

    /* Flush VM pages */
    osi_VM_TryToSmush(avc, acred, sync);

    /*
     * Get the hash chain containing all dce's for this fid
     */
    i = DVHash(&avc->f.fid);
    ObtainWriteLock(&afs_xdcache, 277);
    for (index = afs_dvhashTbl[i]; index != NULLIDX; index = i) {
	i = afs_dvnextTbl[index];	/* next pointer this hash table */
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
	    int releaseTlock = 1;
	    tdc = afs_GetValidDSlot(index);
	    if (!tdc) {
		/* afs_TryToSmush is best-effort; we may not actually discard
		 * everything, so failure to discard a dcache due to an i/o
		 * error is okay. */
		continue;
	    }
	    if (!FidCmp(&tdc->f.fid, &avc->f.fid)) {
		if (sync) {
		    if ((afs_indexFlags[index] & IFDataMod) == 0
			&& tdc->refCount == 1) {
			ReleaseReadLock(&tdc->tlock);
			releaseTlock = 0;
			afs_FlushDCache(tdc);
		    }
		} else
		    afs_indexTable[index] = 0;
	    }
	    if (releaseTlock)
		ReleaseReadLock(&tdc->tlock);
	    afs_PutDCache(tdc);
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
    ReleaseWriteLock(&afs_xdcache);
    /*
     * It's treated like a callback so that when we do lookups we'll
     * invalidate the unique bit if any
     * trytoSmush occured during the lookup call
     */
    afs_allCBs++;
}

/*
 * afs_DCacheMissingChunks
 *
 * Description
 * 	Given the cached info for a file, return the number of chunks that
 * 	are not available from the dcache.
 *
 * Parameters:
 * 	avc:    Pointer to the (held) vcache entry to look in.
 *
 * Returns:
 * 	The number of chunks which are not currently cached.
 *
 * Environment:
 * 	The vcache entry is held upon entry.
 */

int
afs_DCacheMissingChunks(struct vcache *avc)
{
    int i, index;
    afs_size_t totalLength = 0;
    afs_uint32 totalChunks = 0;
    struct dcache *tdc;

    totalLength = avc->f.m.Length;
    if (avc->f.truncPos < totalLength)
        totalLength = avc->f.truncPos;

    /* Length is 0, no chunk missing. */
    if (totalLength == 0)
    	return 0;

    /* If totalLength is a multiple of chunksize, the last byte appears
     * as being part of the next chunk, which does not exist.
     * Decrementing totalLength by one fixes that.
     */
    totalLength--;
    totalChunks = (AFS_CHUNK(totalLength) + 1);

    /* If we're a directory, we only ever have one chunk, regardless of
     * the size of the dir.
     */
    if (avc->f.fid.Fid.Vnode & 1 || vType(avc) == VDIR)
	totalChunks = 1;

    /*
     printf("Should have %d chunks for %u bytes\n",
    		totalChunks, (totalLength + 1));
    */
    i = DVHash(&avc->f.fid);
    ObtainWriteLock(&afs_xdcache, 1001);
    for (index = afs_dvhashTbl[i]; index != NULLIDX; index = i) {
        i = afs_dvnextTbl[index];
        if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
            tdc = afs_GetValidDSlot(index);
	    if (tdc) {
		if (!FidCmp(&tdc->f.fid, &avc->f.fid)) {
		    totalChunks--;
		}
		ReleaseReadLock(&tdc->tlock);
		afs_PutDCache(tdc);
	    }
        }
    }
    ReleaseWriteLock(&afs_xdcache);

    /*printf("Missing %d chunks\n", totalChunks);*/

    return (totalChunks);
}

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

struct dcache *
afs_FindDCache(struct vcache *avc, afs_size_t abyte)
{
    afs_int32 chunk;
    afs_int32 i, index;
    struct dcache *tdc = NULL;

    AFS_STATCNT(afs_FindDCache);
    chunk = AFS_CHUNK(abyte);

    /*
     * Hash on the [fid, chunk] and get the corresponding dcache index
     * after write-locking the dcache.
     */
    i = DCHash(&avc->f.fid, chunk);
    ObtainWriteLock(&afs_xdcache, 278);
    for (index = afs_dchashTbl[i]; index != NULLIDX; index = afs_dcnextTbl[index]) {
	if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
	    tdc = afs_GetValidDSlot(index);
	    if (!tdc) {
		/* afs_FindDCache is best-effort; we may not find the given
		 * file/offset, so if we cannot find the given dcache due to
		 * i/o errors, that is okay. */
		continue;
	    }
	    ReleaseReadLock(&tdc->tlock);
	    if (!FidCmp(&tdc->f.fid, &avc->f.fid) && chunk == tdc->f.chunk) {
		break;		/* leaving refCount high for caller */
	    }
	    afs_PutDCache(tdc);
	}
    }
    if (index != NULLIDX) {
	hset(afs_indexTimes[tdc->index], afs_indexCounter);
	hadd32(afs_indexCounter, 1);
	ReleaseWriteLock(&afs_xdcache);
	return tdc;
    }
    ReleaseWriteLock(&afs_xdcache);
    return NULL;
}				/*afs_FindDCache */

/* only call these from afs_AllocDCache() */
static struct dcache *
afs_AllocFreeDSlot(void)
{
    struct dcache *tdc;

    tdc = afs_GetDSlotFromList(&afs_freeDCList);
    if (!tdc) {
	return NULL;
    }
    afs_indexFlags[tdc->index] &= ~IFFree;
    ObtainWriteLock(&tdc->lock, 604);
    afs_freeDCCount--;

    return tdc;
}
static struct dcache *
afs_AllocDiscardDSlot(afs_int32 lock)
{
    struct dcache *tdc;
    afs_uint32 size = 0;
    struct osi_file *file;

    tdc = afs_GetDSlotFromList(&afs_discardDCList);
    if (!tdc) {
	return NULL;
    }
    afs_indexFlags[tdc->index] &= ~IFDiscarded;
    ObtainWriteLock(&tdc->lock, 605);
    afs_discardDCCount--;
    size =
	((tdc->f.chunkBytes +
	  afs_fsfragsize) ^ afs_fsfragsize) >> 10;
    tdc->f.states &= ~(DRO|DBackup|DRW);
    afs_DCMoveBucket(tdc, size, 0);
    afs_blocksDiscarded -= size;
    afs_stats_cmperf.cacheBlocksDiscarded = afs_blocksDiscarded;
    if ((lock & 2)) {
	/* Truncate the chunk so zeroes get filled properly */
	file = afs_CFileOpen(&tdc->f.inode);
	afs_CFileTruncate(file, 0);
	afs_CFileClose(file);
	afs_AdjustSize(tdc, 0);
    }

    return tdc;
}

/*!
 * Get a fresh dcache from the free or discarded list.
 *
 * \param avc Who's dcache is this going to be?
 * \param chunk The position where it will be placed in.
 * \param lock How are locks held.
 * \param ashFid If this dcache going to be used for a shadow dir,
 * 		this is it's fid.
 *
 * \note Required locks:
 * 	- afs_xdcache (W)
 * 	- avc (R if (lock & 1) set and W otherwise)
 * \note It write locks the new dcache. The caller must unlock it.
 *
 * \return The new dcache.
 */
struct dcache *
afs_AllocDCache(struct vcache *avc, afs_int32 chunk, afs_int32 lock,
		struct VenusFid *ashFid)
{
    struct dcache *tdc = NULL;

    /* if (lock & 2), prefer 'free' dcaches; otherwise, prefer 'discard'
     * dcaches. In either case, try both if our first choice doesn't work. */
    if ((lock & 2)) {
	tdc = afs_AllocFreeDSlot();
	if (!tdc) {
	    tdc = afs_AllocDiscardDSlot(lock);
	}
    } else {
	tdc = afs_AllocDiscardDSlot(lock);
	if (!tdc) {
	    tdc = afs_AllocFreeDSlot();
	}
    }
    if (!tdc) {
	return NULL;
    }

    /*
     * Locks held:
     * avc->lock(R) if setLocks
     * avc->lock(W) if !setLocks
     * tdc->lock(W)
     * afs_xdcache(W)
     */

    /*
     * Fill in the newly-allocated dcache record.
     */
    afs_indexFlags[tdc->index] &= ~(IFDirtyPages | IFAnyPages);
    if (ashFid)
    	/* Use shadow fid if provided. */
	tdc->f.fid = *ashFid;
    else
    	/* Use normal vcache's fid otherwise. */
    	tdc->f.fid = avc->f.fid;
    if (avc->f.states & CRO)
    	tdc->f.states = DRO;
    else if (avc->f.states & CBackup)
    	tdc->f.states = DBackup;
    else
    	tdc->f.states = DRW;
    afs_DCMoveBucket(tdc, 0, afs_DCGetBucket(avc));
    afs_indexUnique[tdc->index] = tdc->f.fid.Fid.Unique;
    if (!ashFid)
    	hones(tdc->f.versionNo);	/* invalid value */
    tdc->f.chunk = chunk;
    tdc->validPos = AFS_CHUNKTOBASE(chunk);
    /* XXX */
    if (tdc->lruq.prev == &tdc->lruq)
	osi_Panic("lruq 1");

    return tdc;
}

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
 *			4 : called from afs_vnop_write.c
 *			    *alen contains length of data to be written.
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

/*
 * Update the vnode-to-dcache hint if we can get the vnode lock
 * right away.  Assumes dcache entry is at least read-locked.
 */
void
updateV2DC(int lockVc, struct vcache *v, struct dcache *d, int src)
{
    if (!lockVc || 0 == NBObtainWriteLock(&v->lock, src)) {
	if (hsame(v->f.m.DataVersion, d->f.versionNo) && v->callback)
	    v->dchint = d;
	if (lockVc)
	    ReleaseWriteLock(&v->lock);
    }
}

/* avc - Write-locked unless aflags & 1 */
struct dcache *
afs_GetDCache(struct vcache *avc, afs_size_t abyte,
	      struct vrequest *areq, afs_size_t * aoffset,
	      afs_size_t * alen, int aflags)
{
    afs_int32 i, code, shortcut;
#if	defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV)
    afs_int32 adjustsize = 0;
#endif
    int setLocks;
    afs_int32 index;
    afs_int32 us;
    afs_int32 chunk;
    afs_size_t Position = 0;
    afs_int32 size, tlen;	/* size of segment to transfer */
    struct afs_FetchOutput *tsmall = 0;
    struct dcache *tdc;
    struct osi_file *file;
    struct afs_conn *tc;
    int downDCount = 0;
    struct server *newCallback = NULL;
    char setNewCallback;
    char setVcacheStatus;
    char doVcacheUpdate;
    char slowPass = 0;
    int doAdjustSize = 0;
    int doReallyAdjustSize = 0;
    int overWriteWholeChunk = 0;
    struct rx_connection *rxconn;

#ifndef AFS_NOSTATS
    struct afs_stats_AccessInfo *accP;	/*Ptr to access record in stats */
    int fromReplica;		/*Are we reading from a replica? */
    int numFetchLoops;		/*# times around the fetch/analyze loop */
#endif /* AFS_NOSTATS */

    AFS_STATCNT(afs_GetDCache);
    if (dcacheDisabled)
	return NULL;

    setLocks = aflags & 1;

    /*
     * Determine the chunk number and offset within the chunk corresponding
     * to the desired byte.
     */
    if (avc->f.fid.Fid.Vnode & 1) {	/* if (vType(avc) == VDIR) */
	chunk = 0;
    } else {
	chunk = AFS_CHUNK(abyte);
    }

    /* come back to here if we waited for the cache to drain. */
  RetryGetDCache:

    setNewCallback = setVcacheStatus = 0;

    if (setLocks) {
	if (slowPass)
	    ObtainWriteLock(&avc->lock, 616);
	else
	    ObtainReadLock(&avc->lock);
    }

    /*
     * Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     */

    shortcut = 0;

    /* check hints first! (might could use bcmp or some such...) */
    if ((tdc = avc->dchint)) {
	int dcLocked;

	/*
	 * The locking order between afs_xdcache and dcache lock matters.
	 * The hint dcache entry could be anywhere, even on the free list.
	 * Locking afs_xdcache ensures that noone is trying to pull dcache
	 * entries from the free list, and thereby assuming them to be not
	 * referenced and not locked.
	 */
	ObtainReadLock(&afs_xdcache);
	dcLocked = (0 == NBObtainSharedLock(&tdc->lock, 601));

	if (dcLocked && (tdc->index != NULLIDX)
	    && !FidCmp(&tdc->f.fid, &avc->f.fid) && chunk == tdc->f.chunk
	    && !(afs_indexFlags[tdc->index] & (IFFree | IFDiscarded))) {
	    /* got the right one.  It might not be the right version, and it
	     * might be fetching, but it's the right dcache entry.
	     */
	    /* All this code should be integrated better with what follows:
	     * I can save a good bit more time under a write lock if I do..
	     */
	    ObtainWriteLock(&tdc->tlock, 603);
	    tdc->refCount++;
	    ReleaseWriteLock(&tdc->tlock);

	    ReleaseReadLock(&afs_xdcache);
	    shortcut = 1;

	    if (hsame(tdc->f.versionNo, avc->f.m.DataVersion)
		&& !(tdc->dflags & DFFetching)) {

		afs_stats_cmperf.dcacheHits++;
		ObtainWriteLock(&afs_xdcache, 559);
		QRemove(&tdc->lruq);
		QAdd(&afs_DLRU, &tdc->lruq);
		ReleaseWriteLock(&afs_xdcache);

		/* Locks held:
		 * avc->lock(R) if setLocks && !slowPass
		 * avc->lock(W) if !setLocks || slowPass
		 * tdc->lock(S)
		 */
		goto done;
	    }
	} else {
	    if (dcLocked)
		ReleaseSharedLock(&tdc->lock);
	    ReleaseReadLock(&afs_xdcache);
	}

	if (!shortcut)
	    tdc = 0;
    }

    /* Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     * tdc->lock(S) if tdc
     */

    if (!tdc) {			/* If the hint wasn't the right dcache entry */
	int dslot_error = 0;
	/*
	 * Hash on the [fid, chunk] and get the corresponding dcache index
	 * after write-locking the dcache.
	 */
      RetryLookup:

	/* Locks held:
	 * avc->lock(R) if setLocks && !slowPass
	 * avc->lock(W) if !setLocks || slowPass
	 */

	i = DCHash(&avc->f.fid, chunk);
	/* check to make sure our space is fine */
	afs_MaybeWakeupTruncateDaemon();

	ObtainWriteLock(&afs_xdcache, 280);
	us = NULLIDX;
	for (index = afs_dchashTbl[i]; index != NULLIDX; us = index, index = afs_dcnextTbl[index]) {
	    if (afs_indexUnique[index] == avc->f.fid.Fid.Unique) {
		tdc = afs_GetValidDSlot(index);
		if (!tdc) {
		    /* we got an i/o error when trying to get the given dslot,
		     * but do not bail out just yet; it is possible the dcache
		     * we're looking for is elsewhere, so it doesn't matter if
		     * we can't load this one. */
		    dslot_error = 1;
		    continue;
		}
		ReleaseReadLock(&tdc->tlock);
		/*
		 * Locks held:
		 * avc->lock(R) if setLocks && !slowPass
		 * avc->lock(W) if !setLocks || slowPass
		 * afs_xdcache(W)
		 */
		if (!FidCmp(&tdc->f.fid, &avc->f.fid) && chunk == tdc->f.chunk) {
		    /* Move it up in the beginning of the list */
		    if (afs_dchashTbl[i] != index) {
			afs_dcnextTbl[us] = afs_dcnextTbl[index];
			afs_dcnextTbl[index] = afs_dchashTbl[i];
			afs_dchashTbl[i] = index;
		    }
		    ReleaseWriteLock(&afs_xdcache);
		    ObtainSharedLock(&tdc->lock, 606);
		    break;	/* leaving refCount high for caller */
		}
		afs_PutDCache(tdc);
		tdc = 0;
	    }
	}

	/*
	 * If we didn't find the entry, we'll create one.
	 */
	if (index == NULLIDX) {
	    /*
	     * Locks held:
	     * avc->lock(R) if setLocks
	     * avc->lock(W) if !setLocks
	     * afs_xdcache(W)
	     */
	    afs_Trace2(afs_iclSetp, CM_TRACE_GETDCACHE1, ICL_TYPE_POINTER,
		       avc, ICL_TYPE_INT32, chunk);

	    if (dslot_error) {
		/* We couldn't find the dcache we want, but we hit some i/o
		 * errors when trying to find it, so we're not sure if the
		 * dcache we want is in the cache or not. Error out, so we
		 * don't try to possibly create 2 separate dcaches for the
		 * same exact data. */
		ReleaseWriteLock(&afs_xdcache);
		goto done;
	    }

	    if (afs_discardDCList == NULLIDX && afs_freeDCList == NULLIDX) {
		if (!setLocks)
		    avc->f.states |= CDCLock;
		/* just need slots */
		afs_GetDownD(5, (int *)0, afs_DCGetBucket(avc));
		if (!setLocks)
		    avc->f.states &= ~CDCLock;
	    }
	    tdc = afs_AllocDCache(avc, chunk, aflags, NULL);
	    if (!tdc) {
		/* If we can't get space for 5 mins we give up and panic */
		if (++downDCount > 300)
		    osi_Panic("getdcache");
		ReleaseWriteLock(&afs_xdcache);
		/*
		 * Locks held:
		 * avc->lock(R) if setLocks
		 * avc->lock(W) if !setLocks
		 */
		afs_osi_Wait(1000, 0, 0);
		goto RetryLookup;
	    }

	    /*
	     * Locks held:
	     * avc->lock(R) if setLocks
	     * avc->lock(W) if !setLocks
	     * tdc->lock(W)
	     * afs_xdcache(W)
	     */

	    /*
	     * Now add to the two hash chains - note that i is still set
	     * from the above DCHash call.
	     */
	    afs_dcnextTbl[tdc->index] = afs_dchashTbl[i];
	    afs_dchashTbl[i] = tdc->index;
	    i = DVHash(&avc->f.fid);
	    afs_dvnextTbl[tdc->index] = afs_dvhashTbl[i];
	    afs_dvhashTbl[i] = tdc->index;
	    tdc->dflags = DFEntryMod;
	    tdc->mflags = 0;
	    afs_MaybeWakeupTruncateDaemon();
	    ReleaseWriteLock(&afs_xdcache);
	    ConvertWToSLock(&tdc->lock);
	}
    }


    /* vcache->dcache hint failed */
    /*
     * Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     * tdc->lock(S)
     */
    afs_Trace4(afs_iclSetp, CM_TRACE_GETDCACHE2, ICL_TYPE_POINTER, avc,
	       ICL_TYPE_POINTER, tdc, ICL_TYPE_INT32,
	       hgetlo(tdc->f.versionNo), ICL_TYPE_INT32,
	       hgetlo(avc->f.m.DataVersion));
    /*
     * Here we have the entry in tdc, with its refCount incremented.
     * Note: we don't use the S-lock on avc; it costs concurrency when
     * storing a file back to the server.
     */

    /*
     * Not a newly created file so we need to check the file's length and
     * compare data versions since someone could have changed the data or we're
     * reading a file written elsewhere. We only want to bypass doing no-op
     * read rpcs on newly created files (dv of 0) since only then we guarantee
     * that this chunk's data hasn't been filled by another client.
     */
    size = AFS_CHUNKSIZE(abyte);
    if (aflags & 4)		/* called from write */
	tlen = *alen;
    else			/* called from read */
	tlen = tdc->validPos - abyte;
    Position = AFS_CHUNKTOBASE(chunk);
    afs_Trace4(afs_iclSetp, CM_TRACE_GETDCACHE3, ICL_TYPE_INT32, tlen,
	       ICL_TYPE_INT32, aflags, ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(abyte), ICL_TYPE_OFFSET,
	       ICL_HANDLE_OFFSET(Position));
    if ((aflags & 4) && (hiszero(avc->f.m.DataVersion)))
	doAdjustSize = 1;
    if ((AFS_CHUNKTOBASE(chunk) >= avc->f.m.Length) ||
	 ((aflags & 4) && (abyte == Position) && (tlen >= size)))
	overWriteWholeChunk = 1;
    if (doAdjustSize || overWriteWholeChunk) {
#if	defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV)
#ifdef	AFS_SGI_ENV
#ifdef AFS_SGI64_ENV
	if (doAdjustSize)
	    adjustsize = NBPP;
#else /* AFS_SGI64_ENV */
	if (doAdjustSize)
	    adjustsize = 8192;
#endif /* AFS_SGI64_ENV */
#else /* AFS_SGI_ENV */
	if (doAdjustSize)
	    adjustsize = 4096;
#endif /* AFS_SGI_ENV */
	if (AFS_CHUNKTOBASE(chunk) + adjustsize >= avc->f.m.Length &&
#else /* defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV) */
#if	defined(AFS_SUN5_ENV)
	if ((doAdjustSize || (AFS_CHUNKTOBASE(chunk) >= avc->f.m.Length)) &&
#else
	if (AFS_CHUNKTOBASE(chunk) >= avc->f.m.Length &&
#endif
#endif /* defined(AFS_AIX32_ENV) || defined(AFS_SGI_ENV) */
	    !hsame(avc->f.m.DataVersion, tdc->f.versionNo))
	    doReallyAdjustSize = 1;

	if (doReallyAdjustSize || overWriteWholeChunk) {
	    /* no data in file to read at this position */
	    UpgradeSToWLock(&tdc->lock, 607);
	    file = afs_CFileOpen(&tdc->f.inode);
	    afs_CFileTruncate(file, 0);
	    afs_CFileClose(file);
	    afs_AdjustSize(tdc, 0);
	    hset(tdc->f.versionNo, avc->f.m.DataVersion);
	    tdc->dflags |= DFEntryMod;

	    ConvertWToSLock(&tdc->lock);
	}
    }

    /*
     * We must read in the whole chunk if the version number doesn't
     * match.
     */
    if (aflags & 2) {
	/* don't need data, just a unique dcache entry */
	ObtainWriteLock(&afs_xdcache, 608);
	hset(afs_indexTimes[tdc->index], afs_indexCounter);
	hadd32(afs_indexCounter, 1);
	ReleaseWriteLock(&afs_xdcache);

	updateV2DC(setLocks, avc, tdc, 553);
	if (vType(avc) == VDIR)
	    *aoffset = abyte;
	else
	    *aoffset = AFS_CHUNKOFFSET(abyte);
	if (tdc->validPos < abyte)
	    *alen = (afs_size_t) 0;
	else
	    *alen = tdc->validPos - abyte;
	ReleaseSharedLock(&tdc->lock);
	if (setLocks) {
	    if (slowPass)
		ReleaseWriteLock(&avc->lock);
	    else
		ReleaseReadLock(&avc->lock);
	}
	return tdc;		/* check if we're done */
    }

    /*
     * Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     * tdc->lock(S)
     */
    osi_Assert((setLocks && !slowPass) || WriteLocked(&avc->lock));

    setNewCallback = setVcacheStatus = 0;

    /*
     * Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     * tdc->lock(S)
     */
    if (!hsame(avc->f.m.DataVersion, tdc->f.versionNo) && !overWriteWholeChunk) {
	/*
	 * Version number mismatch.
	 */
        /*
         * If we are disconnected, then we can't do much of anything
         * because the data doesn't match the file.
         */
        if (AFS_IS_DISCONNECTED) {
            ReleaseSharedLock(&tdc->lock);
            if (setLocks) {
                if (slowPass)
                    ReleaseWriteLock(&avc->lock);
                else
                    ReleaseReadLock(&avc->lock);
            }
            /* Flush the Dcache */
            afs_PutDCache(tdc);

            return NULL;
        }
	UpgradeSToWLock(&tdc->lock, 609);

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
	if (hcmp(avc->flushDV, avc->f.m.DataVersion) < 0) {
	    /*
	     * By here, the cache entry is always write-locked.  We can
	     * deadlock if we call osi_Flush with the cache entry locked...
	     * Unlock the dcache too.
	     */
	    ReleaseWriteLock(&tdc->lock);
	    if (setLocks && !slowPass)
		ReleaseReadLock(&avc->lock);
	    else
		ReleaseWriteLock(&avc->lock);

	    osi_FlushText(avc);
	    /*
	     * Call osi_FlushPages in open, read/write, and map, since it
	     * is too hard here to figure out if we should lock the
	     * pvnLock.
	     */
	    if (setLocks && !slowPass)
		ObtainReadLock(&avc->lock);
	    else
		ObtainWriteLock(&avc->lock, 66);
	    ObtainWriteLock(&tdc->lock, 610);
	}

	/*
	 * Locks held:
	 * avc->lock(R) if setLocks && !slowPass
	 * avc->lock(W) if !setLocks || slowPass
	 * tdc->lock(W)
	 */

	/* Watch for standard race condition around osi_FlushText */
	if (hsame(avc->f.m.DataVersion, tdc->f.versionNo)) {
	    updateV2DC(setLocks, avc, tdc, 569);	/* set hint */
	    afs_stats_cmperf.dcacheHits++;
	    ConvertWToSLock(&tdc->lock);
	    goto done;
	}

	/* Sleep here when cache needs to be drained. */
	if (setLocks && !slowPass
	    && (afs_blocksUsed >
		PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks))) {
	    /* Make sure truncate daemon is running */
	    afs_MaybeWakeupTruncateDaemon();
	    ObtainWriteLock(&tdc->tlock, 614);
	    tdc->refCount--;	/* we'll re-obtain the dcache when we re-try. */
	    ReleaseWriteLock(&tdc->tlock);
	    ReleaseWriteLock(&tdc->lock);
	    ReleaseReadLock(&avc->lock);
	    while ((afs_blocksUsed - afs_blocksDiscarded) >
		   PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
		afs_WaitForCacheDrain = 1;
		afs_osi_Sleep(&afs_WaitForCacheDrain);
	    }
	    afs_MaybeFreeDiscardedDCache();
	    /* need to check if someone else got the chunk first. */
	    goto RetryGetDCache;
	}

	Position = AFS_CHUNKBASE(abyte);
	if (vType(avc) == VDIR) {
	    size = avc->f.m.Length;
	    if (size > tdc->f.chunkBytes) {
		/* pre-reserve space for file */
		afs_AdjustSize(tdc, size);
	    }
	    size = 999999999;	/* max size for transfer */
	} else {
	    afs_size_t maxGoodLength;

	    /* estimate how much data we're expecting back from the server,
	     * and reserve space in the dcache entry for it */

	    maxGoodLength = avc->f.m.Length;
	    if (avc->f.truncPos < maxGoodLength)
		maxGoodLength = avc->f.truncPos;

	    size = AFS_CHUNKSIZE(abyte);	/* expected max size */
            if (Position > maxGoodLength) { /* If we're beyond EOF */
                size = 0;
	    } else if (Position + size > maxGoodLength) {
		size = maxGoodLength - Position;
            }
            osi_Assert(size >= 0);

	    if (size > tdc->f.chunkBytes) {
		/* pre-reserve estimated space for file */
		afs_AdjustSize(tdc, size);	/* changes chunkBytes */
	    }

	    if (size) {
		/* For the actual fetch, do not limit the request to the
		 * length of the file. If this results in a read past EOF on
		 * the server, the server will just reply with less data than
		 * requested. If we limit ourselves to only requesting data up
		 * to the avc file length, we open ourselves up to races if the
		 * file is extended on the server at about the same time.
		 *
		 * However, we must restrict ourselves to the avc->f.truncPos
		 * length, since this represents an outstanding local
		 * truncation of the file that will be committed to the
		 * fileserver when we actually write the fileserver contents.
		 * If we do not restrict the fetch length based on
		 * avc->f.truncPos, a different truncate operation extending
		 * the file length could cause the old data after
		 * avc->f.truncPos to reappear, instead of extending the file
		 * with NUL bytes. */
		size = AFS_CHUNKSIZE(abyte);
                if (Position > avc->f.truncPos) {
                    size = 0;
		} else if (Position + size > avc->f.truncPos) {
		    size = avc->f.truncPos - Position;
		}
                osi_Assert(size >= 0);
	    }
	}
	if (afs_mariner && !tdc->f.chunk)
	    afs_MarinerLog("fetch$Fetching", avc);	/* , Position, size, afs_indexCounter ); */
	/*
	 * Right now, we only have one tool, and it's a hammer.  So, we
	 * fetch the whole file.
	 */
	DZap(tdc);	/* pages in cache may be old */
	file = afs_CFileOpen(&tdc->f.inode);
	afs_RemoveVCB(&avc->f.fid);
	tdc->f.states |= DWriting;
	tdc->dflags |= DFFetching;
	tdc->validPos = Position;	/*  which is AFS_CHUNKBASE(abyte) */
	if (tdc->mflags & DFFetchReq) {
	    tdc->mflags &= ~DFFetchReq;
	    if (afs_osi_Wakeup(&tdc->validPos) == 0)
		afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAKE, ICL_TYPE_STRING,
			   __FILE__, ICL_TYPE_INT32, __LINE__,
			   ICL_TYPE_POINTER, tdc, ICL_TYPE_INT32,
			   tdc->dflags);
	}
	tsmall =
	    (struct afs_FetchOutput *)osi_AllocLargeSpace(sizeof(struct afs_FetchOutput));
	setVcacheStatus = 0;
#ifndef AFS_NOSTATS
	/*
	 * Remember if we are doing the reading from a replicated volume,
	 * and how many times we've zipped around the fetch/analyze loop.
	 */
	fromReplica = (avc->f.states & CRO) ? 1 : 0;
	numFetchLoops = 0;
	accP = &(afs_stats_cmfullperf.accessinf);
	if (fromReplica)
	    (accP->replicatedRefs)++;
	else
	    (accP->unreplicatedRefs)++;
#endif /* AFS_NOSTATS */
	/* this is a cache miss */
	afs_Trace4(afs_iclSetp, CM_TRACE_FETCHPROC, ICL_TYPE_POINTER, avc,
		   ICL_TYPE_FID, &(avc->f.fid), ICL_TYPE_OFFSET,
		   ICL_HANDLE_OFFSET(Position), ICL_TYPE_INT32, size);

	if (size)
	    afs_stats_cmperf.dcacheMisses++;
	code = 0;
	/*
	 * Dynamic root support:  fetch data from local memory.
	 */
	if (afs_IsDynroot(avc)) {
	    char *dynrootDir;
	    int dynrootLen;

	    afs_GetDynroot(&dynrootDir, &dynrootLen, &tsmall->OutStatus);

	    dynrootDir += Position;
	    dynrootLen -= Position;
	    if (size > dynrootLen)
		size = dynrootLen;
	    if (size < 0)
		size = 0;
	    code = afs_CFileWrite(file, 0, dynrootDir, size);
	    afs_PutDynroot();

	    if (code == size)
		code = 0;
	    else
		code = -1;

	    tdc->validPos = Position + size;
	    afs_CFileTruncate(file, size);	/* prune it */
        } else if (afs_IsDynrootMount(avc)) {
	    char *dynrootDir;
	    int dynrootLen;

	    afs_GetDynrootMount(&dynrootDir, &dynrootLen, &tsmall->OutStatus);

	    dynrootDir += Position;
	    dynrootLen -= Position;
	    if (size > dynrootLen)
		size = dynrootLen;
	    if (size < 0)
		size = 0;
	    code = afs_CFileWrite(file, 0, dynrootDir, size);
	    afs_PutDynroot();

	    if (code == size)
		code = 0;
	    else
		code = -1;

	    tdc->validPos = Position + size;
	    afs_CFileTruncate(file, size);	/* prune it */
	} else
	    /*
	     * Not a dynamic vnode:  do the real fetch.
	     */
	    do {
		/*
		 * Locks held:
		 * avc->lock(R) if setLocks && !slowPass
		 * avc->lock(W) if !setLocks || slowPass
		 * tdc->lock(W)
		 */

		tc = afs_Conn(&avc->f.fid, areq, SHARED_LOCK, &rxconn);
		if (tc) {
#ifndef AFS_NOSTATS
		    numFetchLoops++;
		    if (fromReplica)
			(accP->numReplicasAccessed)++;

#endif /* AFS_NOSTATS */
		    if (!setLocks || slowPass) {
			avc->callback = tc->srvr->server;
		    } else {
			newCallback = tc->srvr->server;
			setNewCallback = 1;
		    }
		    i = osi_Time();
		    code = afs_CacheFetchProc(tc, rxconn, file, Position, tdc,
					       avc, size, tsmall);
		} else
		   code = -1;

		if (code == 0) {
		    /* callback could have been broken (or expired) in a race here,
		     * but we return the data anyway.  It's as good as we knew about
		     * when we started. */
		    /*
		     * validPos is updated by CacheFetchProc, and can only be
		     * modifed under a dcache write lock, which we've blocked out
		     */
		    size = tdc->validPos - Position;	/* actual segment size */
		    if (size < 0)
			size = 0;
		    afs_CFileTruncate(file, size);	/* prune it */
		} else {
		    if (!setLocks || slowPass) {
			ObtainWriteLock(&afs_xcbhash, 453);
			afs_DequeueCallback(avc);
			avc->f.states &= ~(CStatd | CUnique);
			avc->callback = NULL;
			ReleaseWriteLock(&afs_xcbhash);
			if (avc->f.fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
			    osi_dnlc_purgedp(avc);
		    } else {
			/* Something lost.  Forget about performance, and go
			 * back with a vcache write lock.
			 */
			afs_CFileTruncate(file, 0);
			afs_AdjustSize(tdc, 0);
			afs_CFileClose(file);
			osi_FreeLargeSpace(tsmall);
			tsmall = 0;
			ReleaseWriteLock(&tdc->lock);
			afs_PutDCache(tdc);
			tdc = 0;
			ReleaseReadLock(&avc->lock);

			if (tc) {
			    /* If we have a connection, we must put it back,
			     * since afs_Analyze will not be called here. */
			    afs_PutConn(tc, rxconn, SHARED_LOCK);
			}

			slowPass = 1;
			goto RetryGetDCache;
		    }
		}

	    } while (afs_Analyze
		     (tc, rxconn, code, &avc->f.fid, areq,
		      AFS_STATS_FS_RPCIDX_FETCHDATA, SHARED_LOCK, NULL));

	/*
	 * Locks held:
	 * avc->lock(R) if setLocks && !slowPass
	 * avc->lock(W) if !setLocks || slowPass
	 * tdc->lock(W)
	 */

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

	tdc->dflags &= ~DFFetching;
	if (afs_osi_Wakeup(&tdc->validPos) == 0)
	    afs_Trace4(afs_iclSetp, CM_TRACE_DCACHEWAKE, ICL_TYPE_STRING,
		       __FILE__, ICL_TYPE_INT32, __LINE__, ICL_TYPE_POINTER,
		       tdc, ICL_TYPE_INT32, tdc->dflags);
	if (avc->execsOrWriters == 0)
	    tdc->f.states &= ~DWriting;

	/* now, if code != 0, we have an error and should punt.
	 * note that we have the vcache write lock, either because
	 * !setLocks or slowPass.
	 */
	if (code) {
	    afs_CFileTruncate(file, 0);
	    afs_AdjustSize(tdc, 0);
	    afs_CFileClose(file);
	    ZapDCE(tdc);	/* sets DFEntryMod */
	    if (vType(avc) == VDIR) {
		DZap(tdc);
	    }
	    tdc->f.states &= ~(DRO|DBackup|DRW);
	    afs_DCMoveBucket(tdc, 0, 0);
	    ReleaseWriteLock(&tdc->lock);
	    afs_PutDCache(tdc);
	    if (!afs_IsDynroot(avc)) {
		ObtainWriteLock(&afs_xcbhash, 454);
		afs_DequeueCallback(avc);
		avc->f.states &= ~(CStatd | CUnique);
		ReleaseWriteLock(&afs_xcbhash);
		if (avc->f.fid.Fid.Vnode & 1 || (vType(avc) == VDIR))
		    osi_dnlc_purgedp(avc);
		/*
		 * Locks held:
		 * avc->lock(W); assert(!setLocks || slowPass)
		 */
		osi_Assert(!setLocks || slowPass);
	    }
	    tdc = NULL;
	    goto done;
	}

	/* otherwise we copy in the just-fetched info */
	afs_CFileClose(file);
	afs_AdjustSize(tdc, size);	/* new size */
	/*
	 * Copy appropriate fields into vcache.  Status is
	 * copied later where we selectively acquire the
	 * vcache write lock.
	 */
	if (slowPass)
	    afs_ProcessFS(avc, &tsmall->OutStatus, areq);
	else
	    setVcacheStatus = 1;
	hset64(tdc->f.versionNo, tsmall->OutStatus.dataVersionHigh,
	       tsmall->OutStatus.DataVersion);
	tdc->dflags |= DFEntryMod;
	afs_indexFlags[tdc->index] |= IFEverUsed;
	ConvertWToSLock(&tdc->lock);
    } /*Data version numbers don't match */
    else {
	/*
	 * Data version numbers match.
	 */
	afs_stats_cmperf.dcacheHits++;
    }				/*Data version numbers match */

    updateV2DC(setLocks, avc, tdc, 335);	/* set hint */
  done:
    /*
     * Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     * tdc->lock(S) if tdc
     */

    /*
     * See if this was a reference to a file in the local cell.
     */
    if (afs_IsPrimaryCellNum(avc->f.fid.Cell))
	afs_stats_cmperf.dlocalAccesses++;
    else
	afs_stats_cmperf.dremoteAccesses++;

    /* Fix up LRU info */

    if (tdc) {
	ObtainWriteLock(&afs_xdcache, 602);
	hset(afs_indexTimes[tdc->index], afs_indexCounter);
	hadd32(afs_indexCounter, 1);
	ReleaseWriteLock(&afs_xdcache);

	/* return the data */
	if (vType(avc) == VDIR)
	    *aoffset = abyte;
	else
	    *aoffset = AFS_CHUNKOFFSET(abyte);
	*alen = (tdc->f.chunkBytes - *aoffset);
	ReleaseSharedLock(&tdc->lock);
    }

    /*
     * Locks held:
     * avc->lock(R) if setLocks && !slowPass
     * avc->lock(W) if !setLocks || slowPass
     */

    /* Fix up the callback and status values in the vcache */
    doVcacheUpdate = 0;
    if (setLocks && !slowPass) {
	/* DCLOCKXXX
	 *
	 * This is our dirty little secret to parallel fetches.
	 * We don't write-lock the vcache while doing the fetch,
	 * but potentially we'll need to update the vcache after
	 * the fetch is done.
	 *
	 * Drop the read lock and try to re-obtain the write
	 * lock.  If the vcache still has the same DV, it's
	 * ok to go ahead and install the new data.
	 */
	afs_hyper_t currentDV, statusDV;

	hset(currentDV, avc->f.m.DataVersion);

	if (setNewCallback && avc->callback != newCallback)
	    doVcacheUpdate = 1;

	if (tsmall) {
	    hset64(statusDV, tsmall->OutStatus.dataVersionHigh,
		   tsmall->OutStatus.DataVersion);

	    if (setVcacheStatus && avc->f.m.Length != tsmall->OutStatus.Length)
		doVcacheUpdate = 1;
	    if (setVcacheStatus && !hsame(currentDV, statusDV))
		doVcacheUpdate = 1;
	}

	ReleaseReadLock(&avc->lock);

	if (doVcacheUpdate) {
	    ObtainWriteLock(&avc->lock, 615);
	    if (!hsame(avc->f.m.DataVersion, currentDV)) {
		/* We lose.  Someone will beat us to it. */
		doVcacheUpdate = 0;
		ReleaseWriteLock(&avc->lock);
	    }
	}
    }

    /* With slow pass, we've already done all the updates */
    if (slowPass) {
	ReleaseWriteLock(&avc->lock);
    }

    /* Check if we need to perform any last-minute fixes with a write-lock */
    if (!setLocks || doVcacheUpdate) {
	if (setNewCallback)
	    avc->callback = newCallback;
	if (tsmall && setVcacheStatus)
	    afs_ProcessFS(avc, &tsmall->OutStatus, areq);
	if (setLocks)
	    ReleaseWriteLock(&avc->lock);
    }

    if (tsmall)
	osi_FreeLargeSpace(tsmall);

    return tdc;
}				/*afs_GetDCache */


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
afs_WriteThroughDSlots(void)
{
    struct dcache *tdc;
    afs_int32 i, touchedit = 0;

    struct afs_q DirtyQ, *tq;

    AFS_STATCNT(afs_WriteThroughDSlots);

    /*
     * Because of lock ordering, we can't grab dcache locks while
     * holding afs_xdcache.  So we enter xdcache, get a reference
     * for every dcache entry, and exit xdcache.
     */
    ObtainWriteLock(&afs_xdcache, 283);
    QInit(&DirtyQ);
    for (i = 0; i < afs_cacheFiles; i++) {
	tdc = afs_indexTable[i];

	/* Grab tlock in case the existing refcount isn't zero */
	if (tdc && !(afs_indexFlags[i] & (IFFree | IFDiscarded))) {
	    ObtainWriteLock(&tdc->tlock, 623);
	    tdc->refCount++;
	    ReleaseWriteLock(&tdc->tlock);

	    QAdd(&DirtyQ, &tdc->dirty);
	}
    }
    ReleaseWriteLock(&afs_xdcache);

    /*
     * Now, for each dcache entry we found, check if it's dirty.
     * If so, get write-lock, get afs_xdcache, which protects
     * afs_cacheInodep, and flush it.  Don't forget to put back
     * the refcounts.
     */

#define DQTODC(q)	((struct dcache *)(((char *) (q)) - sizeof(struct afs_q)))

    for (tq = DirtyQ.prev; tq != &DirtyQ; tq = QPrev(tq)) {
	tdc = DQTODC(tq);
	if (tdc->dflags & DFEntryMod) {
	    int wrLock;

	    wrLock = (0 == NBObtainWriteLock(&tdc->lock, 619));

	    /* Now that we have the write lock, double-check */
	    if (wrLock && (tdc->dflags & DFEntryMod)) {
		tdc->dflags &= ~DFEntryMod;
		ObtainWriteLock(&afs_xdcache, 620);
		osi_Assert(afs_WriteDCache(tdc, 1) == 0);
		ReleaseWriteLock(&afs_xdcache);
		touchedit = 1;
	    }
	    if (wrLock)
		ReleaseWriteLock(&tdc->lock);
	}

	afs_PutDCache(tdc);
    }

    ObtainWriteLock(&afs_xdcache, 617);
    if (!touchedit && (cacheDiskType != AFS_FCACHE_TYPE_MEM)) {
	/* Touch the file to make sure that the mtime on the file is kept
	 * up-to-date to avoid losing cached files on cold starts because
	 * their mtime seems old...
	 */
	struct afs_fheader theader;

	theader.magic = AFS_FHMAGIC;
	theader.firstCSize = AFS_FIRSTCSIZE;
	theader.otherCSize = AFS_OTHERCSIZE;
	theader.version = AFS_CI_VERSION;
	theader.dataSize = sizeof(struct fcache);
	afs_osi_Write(afs_cacheInodep, 0, &theader, sizeof(theader));
    }
    ReleaseWriteLock(&afs_xdcache);
}

/*
 * afs_MemGetDSlot
 *
 * Description:
 *	Return a pointer to an freshly initialized dcache entry using
 *	a memory-based cache.  The tlock will be read-locked.
 *
 * Parameters:
 *	aslot : Dcache slot to look at.
 *      needvalid : Whether the specified slot should already exist
 *
 * Environment:
 *	Must be called with afs_xdcache write-locked.
 */

struct dcache *
afs_MemGetDSlot(afs_int32 aslot, int indexvalid, int datavalid)
{
    struct dcache *tdc;
    int existing = 0;

    AFS_STATCNT(afs_MemGetDSlot);
    if (CheckLock(&afs_xdcache) != -1)
	osi_Panic("getdslot nolock");
    if (aslot < 0 || aslot >= afs_cacheFiles)
	osi_Panic("getdslot slot %d (of %d)", aslot, afs_cacheFiles);
    tdc = afs_indexTable[aslot];
    if (tdc) {
	QRemove(&tdc->lruq);	/* move to queue head */
	QAdd(&afs_DLRU, &tdc->lruq);
	/* We're holding afs_xdcache, but get tlock in case refCount != 0 */
	ObtainWriteLock(&tdc->tlock, 624);
	tdc->refCount++;
	ConvertWToRLock(&tdc->tlock);
	return tdc;
    }

    /* if 'indexvalid' is true, the slot must already exist and be populated
     * somewhere. for memcache, the only place that dcache entries exist is
     * in memory, so if we did not find it above, something is very wrong. */
    osi_Assert(!indexvalid);

    if (!afs_freeDSList)
	afs_GetDownDSlot(4);
    if (!afs_freeDSList) {
	/* none free, making one is better than a panic */
	afs_stats_cmperf.dcacheXAllocs++;	/* count in case we have a leak */
	tdc = afs_osi_Alloc(sizeof(struct dcache));
	osi_Assert(tdc != NULL);
#ifdef	KERNEL_HAVE_PIN
	pin((char *)tdc, sizeof(struct dcache));	/* XXX */
#endif
    } else {
	tdc = afs_freeDSList;
	afs_freeDSList = (struct dcache *)tdc->lruq.next;
	existing = 1;
    }
    tdc->dflags = 0;	/* up-to-date, not in free q */
    tdc->mflags = 0;
    QAdd(&afs_DLRU, &tdc->lruq);
    if (tdc->lruq.prev == &tdc->lruq)
	osi_Panic("lruq 3");

    /* initialize entry */
    tdc->f.fid.Cell = 0;
    tdc->f.fid.Fid.Volume = 0;
    tdc->f.chunk = -1;
    hones(tdc->f.versionNo);
    tdc->f.inode.mem = aslot;
    tdc->dflags |= DFEntryMod;
    tdc->refCount = 1;
    tdc->index = aslot;
    afs_indexUnique[aslot] = tdc->f.fid.Fid.Unique;

    if (existing) {
	osi_Assert(0 == NBObtainWriteLock(&tdc->lock, 674));
	osi_Assert(0 == NBObtainWriteLock(&tdc->mflock, 675));
	osi_Assert(0 == NBObtainWriteLock(&tdc->tlock, 676));
    }

    AFS_RWLOCK_INIT(&tdc->lock, "dcache lock");
    AFS_RWLOCK_INIT(&tdc->tlock, "dcache tlock");
    AFS_RWLOCK_INIT(&tdc->mflock, "dcache flock");
    ObtainReadLock(&tdc->tlock);

    afs_indexTable[aslot] = tdc;
    return tdc;

}				/*afs_MemGetDSlot */

unsigned int last_error = 0, lasterrtime = 0;

/*
 * afs_UFSGetDSlot
 *
 * Description:
 *	Return a pointer to an freshly initialized dcache entry using
 *	a UFS-based disk cache.  The dcache tlock will be read-locked.
 *
 * Parameters:
 *	aslot : Dcache slot to look at.
 *      indexvalid : 1 if we know the slot we're giving is valid, and thus
 *                   reading the dcache from the disk index should succeed. 0
 *                   if we are initializing a new dcache, and so reading from
 *                   the disk index may fail.
 *      datavalid : 0 if we are loading a dcache entry from the free or
 *                  discard list, so we know the data in the given dcache is
 *                  not valid. 1 if we are loading a known used dcache, so the
 *                  data in the dcache must be valid.
 *
 * Environment:
 *	afs_xdcache lock write-locked.
 */
struct dcache *
afs_UFSGetDSlot(afs_int32 aslot, int indexvalid, int datavalid)
{
    afs_int32 code;
    struct dcache *tdc;
    int existing = 0;
    int entryok;
    int off;

    AFS_STATCNT(afs_UFSGetDSlot);
    if (CheckLock(&afs_xdcache) != -1)
	osi_Panic("getdslot nolock");
    if (aslot < 0 || aslot >= afs_cacheFiles)
	osi_Panic("getdslot slot %d (of %d)", aslot, afs_cacheFiles);
    tdc = afs_indexTable[aslot];
    if (tdc) {
	QRemove(&tdc->lruq);	/* move to queue head */
	QAdd(&afs_DLRU, &tdc->lruq);
	/* Grab tlock in case refCount != 0 */
	ObtainWriteLock(&tdc->tlock, 625);
	tdc->refCount++;
	ConvertWToRLock(&tdc->tlock);
	return tdc;
    }

    /* otherwise we should read it in from the cache file */
    if (!afs_freeDSList)
	afs_GetDownDSlot(4);
    if (!afs_freeDSList) {
	/* none free, making one is better than a panic */
	afs_stats_cmperf.dcacheXAllocs++;	/* count in case we have a leak */
	tdc = afs_osi_Alloc(sizeof(struct dcache));
	osi_Assert(tdc != NULL);
#ifdef	KERNEL_HAVE_PIN
	pin((char *)tdc, sizeof(struct dcache));	/* XXX */
#endif
    } else {
	tdc = afs_freeDSList;
	afs_freeDSList = (struct dcache *)tdc->lruq.next;
	existing = 1;
    }
    tdc->dflags = 0;	/* up-to-date, not in free q */
    tdc->mflags = 0;
    QAdd(&afs_DLRU, &tdc->lruq);
    if (tdc->lruq.prev == &tdc->lruq)
	osi_Panic("lruq 3");

    /*
     * Seek to the aslot'th entry and read it in.
     */
    off = sizeof(struct fcache)*aslot + sizeof(struct afs_fheader);
    code =
	afs_osi_Read(afs_cacheInodep,
		     off, (char *)(&tdc->f),
		     sizeof(struct fcache));
    entryok = 1;
    if (code != sizeof(struct fcache)) {
	entryok = 0;
#if defined(KERNEL_HAVE_UERROR)
	last_error = getuerror();
#else
	last_error = code;
#endif
	lasterrtime = osi_Time();
	if (indexvalid) {
	    struct osi_stat tstat;
	    if (afs_osi_Stat(afs_cacheInodep, &tstat)) {
		tstat.size = -1;
	    }
	    afs_warn("afs: disk cache read error in CacheItems slot %d "
	             "off %d/%d code %d/%d\n",
	             (int)aslot,
	             off, (int)tstat.size,
	             (int)code, (int)sizeof(struct fcache));
	    /* put tdc back on the free dslot list */
	    QRemove(&tdc->lruq);
	    tdc->index = NULLIDX;
	    tdc->lruq.next = (struct afs_q *)afs_freeDSList;
	    afs_freeDSList = tdc;
	    return NULL;
	}
    }
    if (!afs_CellNumValid(tdc->f.fid.Cell)) {
	entryok = 0;
	if (datavalid) {
	    osi_Panic("afs: needed valid dcache but index %d off %d has "
	              "invalid cell num %d\n",
	              (int)aslot, off, (int)tdc->f.fid.Cell);
	}
    }

    if (datavalid && tdc->f.fid.Fid.Volume == 0) {
	osi_Panic("afs: invalid zero-volume dcache entry at slot %d off %d",
	          (int)aslot, off);
    }

    if (indexvalid && !datavalid) {
	/* we know that the given dslot does exist, but the data in it is not
	 * valid. this only occurs when we pull a dslot from the free or
	 * discard list, so be sure not to re-use the data; force invalidation.
	 */
	entryok = 0;
    }

    if (!entryok) {
	tdc->f.fid.Cell = 0;
	tdc->f.fid.Fid.Volume = 0;
	tdc->f.chunk = -1;
	hones(tdc->f.versionNo);
	tdc->dflags |= DFEntryMod;
	afs_indexUnique[aslot] = tdc->f.fid.Fid.Unique;
	tdc->f.states &= ~(DRO|DBackup|DRW);
	afs_DCMoveBucket(tdc, 0, 0);
    } else {
	if (tdc->f.states & DRO) {
	    afs_DCMoveBucket(tdc, 0, 2);
	} else if (tdc->f.states & DBackup) {
	    afs_DCMoveBucket(tdc, 0, 1);
	} else {
	    afs_DCMoveBucket(tdc, 0, 1);
	}
    }
    tdc->refCount = 1;
    tdc->index = aslot;
    if (tdc->f.chunk >= 0)
	tdc->validPos = AFS_CHUNKTOBASE(tdc->f.chunk) + tdc->f.chunkBytes;
    else
	tdc->validPos = 0;

    if (existing) {
	osi_Assert(0 == NBObtainWriteLock(&tdc->lock, 674));
	osi_Assert(0 == NBObtainWriteLock(&tdc->mflock, 675));
	osi_Assert(0 == NBObtainWriteLock(&tdc->tlock, 676));
    }

    AFS_RWLOCK_INIT(&tdc->lock, "dcache lock");
    AFS_RWLOCK_INIT(&tdc->tlock, "dcache tlock");
    AFS_RWLOCK_INIT(&tdc->mflock, "dcache flock");
    ObtainReadLock(&tdc->tlock);

    /*
     * If we didn't read into a temporary dcache region, update the
     * slot pointer table.
     */
    afs_indexTable[aslot] = tdc;
    return tdc;

}				/*afs_UFSGetDSlot */



/*!
 * Write a particular dcache entry back to its home in the
 * CacheInfo file.
 *
 * \param adc Pointer to the dcache entry to write.
 * \param atime If true, set the modtime on the file to the current time.
 *
 * \note Environment:
 *	Must be called with the afs_xdcache lock at least read-locked,
 *	and dcache entry at least read-locked.
 *	The reference count is not changed.
 */

int
afs_WriteDCache(struct dcache *adc, int atime)
{
    afs_int32 code;

    if (cacheDiskType == AFS_FCACHE_TYPE_MEM)
	return 0;
    AFS_STATCNT(afs_WriteDCache);
    osi_Assert(WriteLocked(&afs_xdcache));
    if (atime)
	adc->f.modTime = osi_Time();

    if ((afs_indexFlags[adc->index] & (IFFree | IFDiscarded)) == 0 &&
        adc->f.fid.Fid.Volume == 0) {
	/* If a dcache slot is not on the free or discard list, it must be
	 * in the hash table. Thus, the volume must be non-zero, since that
	 * is how we determine whether or not to unhash the entry when kicking
	 * it out of the cache. Do this check now, since otherwise this can
	 * cause hash table corruption and a panic later on after we read the
	 * entry back in. */
	osi_Panic("afs_WriteDCache zero volume index %d flags 0x%x\n",
	          adc->index, (unsigned)afs_indexFlags[adc->index]);
    }

    /*
     * Seek to the right dcache slot and write the in-memory image out to disk.
     */
    afs_cellname_write();
    code =
	afs_osi_Write(afs_cacheInodep,
		      sizeof(struct fcache) * adc->index +
		      sizeof(struct afs_fheader), (char *)(&adc->f),
		      sizeof(struct fcache));
    if (code != sizeof(struct fcache)) {
	afs_warn("afs: failed to write to CacheItems off %ld code %d/%d\n",
	         (long)(sizeof(struct fcache) * adc->index + sizeof(struct afs_fheader)),
	         (int)code, (int)sizeof(struct fcache));
	return EIO;
    }
    return 0;
}



/*!
 * Wake up users of a particular file waiting for stores to take
 * place.
 *
 * \param avc Ptr to related vcache entry.
 *
 * \note Environment:
 *	Nothing interesting.
 */
int
afs_wakeup(struct vcache *avc)
{
    int i;
    struct brequest *tb;
    tb = afs_brs;
    AFS_STATCNT(afs_wakeup);
    for (i = 0; i < NBRS; i++, tb++) {
	/* if request is valid and for this file, we've found it */
	if (tb->refCount > 0 && avc == tb->vc) {

	    /*
	     * If CSafeStore is on, then we don't awaken the guy
	     * waiting for the store until the whole store has finished.
	     * Otherwise, we do it now.  Note that if CSafeStore is on,
	     * the BStore routine actually wakes up the user, instead
	     * of us.
	     * I think this is redundant now because this sort of thing
	     * is already being handled by the higher-level code.
	     */
	    if ((avc->f.states & CSafeStore) == 0) {
		tb->code_raw = tb->code_checkcode = 0;
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
}

/*!
 * Given a file name and inode, set up that file to be an
 * active member in the AFS cache.  This also involves checking
 * the usability of its data.
 *
 * \param afile Name of the cache file to initialize.
 * \param ainode Inode of the file.
 *
 * \note Environment:
 *	This function is called only during initialization.
 */
int
afs_InitCacheFile(char *afile, ino_t ainode)
{
    afs_int32 code;
    afs_int32 index;
    int fileIsBad;
    struct osi_file *tfile;
    struct osi_stat tstat;
    struct dcache *tdc;

    AFS_STATCNT(afs_InitCacheFile);
    index = afs_stats_cmperf.cacheNumEntries;
    if (index >= afs_cacheFiles)
	return EINVAL;

    ObtainWriteLock(&afs_xdcache, 282);
    tdc = afs_GetNewDSlot(index);
    ReleaseReadLock(&tdc->tlock);
    ReleaseWriteLock(&afs_xdcache);

    ObtainWriteLock(&tdc->lock, 621);
    ObtainWriteLock(&afs_xdcache, 622);
    if (!afile && !ainode) {
	tfile = NULL;
	fileIsBad = 1;
    } else {
	if (afile) {
	    code = afs_LookupInodeByPath(afile, &tdc->f.inode.ufs, NULL);
	    if (code) {
		ReleaseWriteLock(&afs_xdcache);
		ReleaseWriteLock(&tdc->lock);
		afs_PutDCache(tdc);
		return code;
	    }
	} else {
	    /* Add any other 'complex' inode types here ... */
#if !defined(AFS_LINUX26_ENV) && !defined(AFS_CACHE_VNODE_PATH)
	    tdc->f.inode.ufs = ainode;
#else
	    osi_Panic("Can't init cache with inode numbers when complex inodes are "
		      "in use\n");
#endif
	}
	fileIsBad = 0;
	if ((tdc->f.states & DWriting) || tdc->f.fid.Fid.Volume == 0)
	    fileIsBad = 1;
	tfile = osi_UFSOpen(&tdc->f.inode);
	code = afs_osi_Stat(tfile, &tstat);
	if (code)
	    osi_Panic("initcachefile stat");

	/*
	 * If file size doesn't match the cache info file, it's probably bad.
	 */
	if (tdc->f.chunkBytes != tstat.size)
	    fileIsBad = 1;
	/*
	 * If file changed within T (120?) seconds of cache info file, it's
	 * probably bad.  In addition, if slot changed within last T seconds,
	 * the cache info file may be incorrectly identified, and so slot
	 * may be bad.
	 */
	if (cacheInfoModTime < tstat.mtime + 120)
	    fileIsBad = 1;
	if (cacheInfoModTime < tdc->f.modTime + 120)
	    fileIsBad = 1;
	/* In case write through is behind, make sure cache items entry is
	 * at least as new as the chunk.
	 */
	if (tdc->f.modTime < tstat.mtime)
	    fileIsBad = 1;
    }
    tdc->f.chunkBytes = 0;

    if (fileIsBad) {
	tdc->f.fid.Fid.Volume = 0;	/* not in the hash table */
	if (tfile && tstat.size != 0)
	    osi_UFSTruncate(tfile, 0);
	tdc->f.states &= ~(DRO|DBackup|DRW);
	afs_DCMoveBucket(tdc, 0, 0);
	/* put entry in free cache slot list */
	afs_dvnextTbl[tdc->index] = afs_freeDCList;
	afs_freeDCList = index;
	afs_freeDCCount++;
	afs_indexFlags[index] |= IFFree;
	afs_indexUnique[index] = 0;
    } else {
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
	afs_AdjustSize(tdc, tstat.size);	/* adjust to new size */
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
    }				/*File is not bad */

    if (tfile)
	osi_UFSClose(tfile);
    tdc->f.states &= ~DWriting;
    tdc->dflags &= ~DFEntryMod;
    /* don't set f.modTime; we're just cleaning up */
    osi_Assert(afs_WriteDCache(tdc, 0) == 0);
    ReleaseWriteLock(&afs_xdcache);
    ReleaseWriteLock(&tdc->lock);
    afs_PutDCache(tdc);
    afs_stats_cmperf.cacheNumEntries++;
    return 0;
}


/*Max # of struct dcache's resident at any time*/
/*
 * If 'dchint' is enabled then in-memory dcache min is increased because of
 * crashes...
 */
#define DDSIZE 200

/*!
 * Initialize dcache related variables.
 *
 * \param afiles
 * \param ablocks
 * \param aDentries
 * \param achunk
 * \param aflags
 *
 */
void
afs_dcacheInit(int afiles, int ablocks, int aDentries, int achunk, int aflags)
{
    struct dcache *tdp;
    int i;
    int code;

    afs_freeDCList = NULLIDX;
    afs_discardDCList = NULLIDX;
    afs_freeDCCount = 0;
    afs_freeDSList = NULL;
    hzero(afs_indexCounter);

    LOCK_INIT(&afs_xdcache, "afs_xdcache");

    /*
     * Set chunk size
     */
    if (achunk) {
	if (achunk < 0 || achunk > 30)
	    achunk = 13;	/* Use default */
	AFS_SETCHUNKSIZE(achunk);
    }

    if (!aDentries)
	aDentries = DDSIZE;

    if (aDentries > 512)
	afs_dhashsize = 2048;
    /* initialize hash tables */
    afs_dvhashTbl = afs_osi_Alloc(afs_dhashsize * sizeof(afs_int32));
    osi_Assert(afs_dvhashTbl != NULL);
    afs_dchashTbl = afs_osi_Alloc(afs_dhashsize * sizeof(afs_int32));
    osi_Assert(afs_dchashTbl != NULL);
    for (i = 0; i < afs_dhashsize; i++) {
	afs_dvhashTbl[i] = NULLIDX;
	afs_dchashTbl[i] = NULLIDX;
    }
    afs_dvnextTbl = afs_osi_Alloc(afiles * sizeof(afs_int32));
    osi_Assert(afs_dvnextTbl != NULL);
    afs_dcnextTbl = afs_osi_Alloc(afiles * sizeof(afs_int32));
    osi_Assert(afs_dcnextTbl != NULL);
    for (i = 0; i < afiles; i++) {
	afs_dvnextTbl[i] = NULLIDX;
	afs_dcnextTbl[i] = NULLIDX;
    }

    /* Allocate and zero the pointer array to the dcache entries */
    afs_indexTable = afs_osi_Alloc(sizeof(struct dcache *) * afiles);
    osi_Assert(afs_indexTable != NULL);
    memset(afs_indexTable, 0, sizeof(struct dcache *) * afiles);
    afs_indexTimes = afs_osi_Alloc(afiles * sizeof(afs_hyper_t));
    osi_Assert(afs_indexTimes != NULL);
    memset(afs_indexTimes, 0, afiles * sizeof(afs_hyper_t));
    afs_indexUnique = afs_osi_Alloc(afiles * sizeof(afs_uint32));
    osi_Assert(afs_indexUnique != NULL);
    memset(afs_indexUnique, 0, afiles * sizeof(afs_uint32));
    afs_indexFlags = afs_osi_Alloc(afiles * sizeof(u_char));
    osi_Assert(afs_indexFlags != NULL);
    memset(afs_indexFlags, 0, afiles * sizeof(char));

    /* Allocate and thread the struct dcache entries themselves */
    tdp = afs_Initial_freeDSList =
	afs_osi_Alloc(aDentries * sizeof(struct dcache));
    osi_Assert(tdp != NULL);
    memset(tdp, 0, aDentries * sizeof(struct dcache));
#ifdef	KERNEL_HAVE_PIN
    pin((char *)afs_indexTable, sizeof(struct dcache *) * afiles);	/* XXX */
    pin((char *)afs_indexTimes, sizeof(afs_hyper_t) * afiles);	/* XXX */
    pin((char *)afs_indexFlags, sizeof(char) * afiles);	/* XXX */
    pin((char *)afs_indexUnique, sizeof(afs_int32) * afiles);	/* XXX */
    pin((char *)tdp, aDentries * sizeof(struct dcache));	/* XXX */
    pin((char *)afs_dvhashTbl, sizeof(afs_int32) * afs_dhashsize);	/* XXX */
    pin((char *)afs_dchashTbl, sizeof(afs_int32) * afs_dhashsize);	/* XXX */
    pin((char *)afs_dcnextTbl, sizeof(afs_int32) * afiles);	/* XXX */
    pin((char *)afs_dvnextTbl, sizeof(afs_int32) * afiles);	/* XXX */
#endif

    afs_freeDSList = &tdp[0];
    for (i = 0; i < aDentries - 1; i++) {
	tdp[i].lruq.next = (struct afs_q *)(&tdp[i + 1]);
        AFS_RWLOCK_INIT(&tdp[i].lock, "dcache lock");
        AFS_RWLOCK_INIT(&tdp[i].tlock, "dcache tlock");
        AFS_RWLOCK_INIT(&tdp[i].mflock, "dcache flock");
    }
    tdp[aDentries - 1].lruq.next = (struct afs_q *)0;
    AFS_RWLOCK_INIT(&tdp[aDentries - 1].lock, "dcache lock");
    AFS_RWLOCK_INIT(&tdp[aDentries - 1].tlock, "dcache tlock");
    AFS_RWLOCK_INIT(&tdp[aDentries - 1].mflock, "dcache flock");

    afs_stats_cmperf.cacheBlocksOrig = afs_stats_cmperf.cacheBlocksTotal =
	afs_cacheBlocks = ablocks;
    afs_ComputeCacheParms();	/* compute parms based on cache size */

    afs_dcentries = aDentries;
    afs_blocksUsed = 0;
    afs_stats_cmperf.cacheBucket0_Discarded =
	afs_stats_cmperf.cacheBucket1_Discarded =
	afs_stats_cmperf.cacheBucket2_Discarded = 0;
    afs_DCSizeInit();
    QInit(&afs_DLRU);

    if (aflags & AFSCALL_INIT_MEMCACHE) {
	/*
	 * Use a memory cache instead of a disk cache
	 */
	cacheDiskType = AFS_FCACHE_TYPE_MEM;
	afs_cacheType = &afs_MemCacheOps;
	afiles = (afiles < aDentries) ? afiles : aDentries;	/* min */
	ablocks = afiles * (AFS_FIRSTCSIZE / 1024);
	/* ablocks is reported in 1K blocks */
	code = afs_InitMemCache(afiles, AFS_FIRSTCSIZE, aflags);
	if (code != 0) {
	    afs_warn("afsd: memory cache too large for available memory.\n");
	    afs_warn("afsd: AFS files cannot be accessed.\n\n");
	    dcacheDisabled = 1;
	    afiles = ablocks = 0;
	} else
	    afs_warn("Memory cache: Allocating %d dcache entries...",
		   aDentries);
    } else {
	cacheDiskType = AFS_FCACHE_TYPE_UFS;
	afs_cacheType = &afs_UfsCacheOps;
    }
}

/*!
 * Shuts down the cache.
 *
 */
void
shutdown_dcache(void)
{
    int i;

#ifdef AFS_CACHE_VNODE_PATH
    if (cacheDiskType != AFS_FCACHE_TYPE_MEM) {
	struct dcache *tdc;
	for (i = 0; i < afs_cacheFiles; i++) {
	    tdc = afs_indexTable[i];
	    if (tdc) {
		afs_osi_FreeStr(tdc->f.inode.ufs);
	    }
	}
    }
#endif

    afs_osi_Free(afs_dvnextTbl, afs_cacheFiles * sizeof(afs_int32));
    afs_osi_Free(afs_dcnextTbl, afs_cacheFiles * sizeof(afs_int32));
    afs_osi_Free(afs_indexTable, afs_cacheFiles * sizeof(struct dcache *));
    afs_osi_Free(afs_indexTimes, afs_cacheFiles * sizeof(afs_hyper_t));
    afs_osi_Free(afs_indexUnique, afs_cacheFiles * sizeof(afs_uint32));
    afs_osi_Free(afs_indexFlags, afs_cacheFiles * sizeof(u_char));
    afs_osi_Free(afs_Initial_freeDSList,
		 afs_dcentries * sizeof(struct dcache));
#ifdef	KERNEL_HAVE_PIN
    unpin((char *)afs_dcnextTbl, afs_cacheFiles * sizeof(afs_int32));
    unpin((char *)afs_dvnextTbl, afs_cacheFiles * sizeof(afs_int32));
    unpin((char *)afs_indexTable, afs_cacheFiles * sizeof(struct dcache *));
    unpin((char *)afs_indexTimes, afs_cacheFiles * sizeof(afs_hyper_t));
    unpin((char *)afs_indexUnique, afs_cacheFiles * sizeof(afs_uint32));
    unpin((u_char *) afs_indexFlags, afs_cacheFiles * sizeof(u_char));
    unpin(afs_Initial_freeDSList, afs_dcentries * sizeof(struct dcache));
#endif


    for (i = 0; i < afs_dhashsize; i++) {
	afs_dvhashTbl[i] = NULLIDX;
	afs_dchashTbl[i] = NULLIDX;
    }

    afs_osi_Free(afs_dvhashTbl, afs_dhashsize * sizeof(afs_int32));
    afs_osi_Free(afs_dchashTbl, afs_dhashsize * sizeof(afs_int32));

    afs_blocksUsed = afs_dcentries = 0;
    afs_stats_cmperf.cacheBucket0_Discarded =
	afs_stats_cmperf.cacheBucket1_Discarded =
	afs_stats_cmperf.cacheBucket2_Discarded = 0;
    hzero(afs_indexCounter);

    afs_freeDCCount = 0;
    afs_freeDCList = NULLIDX;
    afs_discardDCList = NULLIDX;
    afs_freeDSList = afs_Initial_freeDSList = 0;

    LOCK_INIT(&afs_xdcache, "afs_xdcache");
    QInit(&afs_DLRU);

}

/*!
 * Get a dcache ready for writing, respecting the current cache size limits
 *
 * len is required because afs_GetDCache with flag == 4 expects the length
 * field to be filled. It decides from this whether it's necessary to fetch
 * data into the chunk before writing or not (when the whole chunk is
 * overwritten!).
 *
 * \param avc 		The vcache to fetch a dcache for
 * \param filePos 	The start of the section to be written
 * \param len		The length of the section to be written
 * \param areq
 * \param noLock
 *
 * \return If successful, a reference counted dcache with tdc->lock held. Lock
 *         must be released and afs_PutDCache() called to free dcache.
 *         NULL on  failure
 *
 * \note avc->lock must be held on entry. Function may release and reobtain
 *       avc->lock and GLOCK.
 */

struct dcache *
afs_ObtainDCacheForWriting(struct vcache *avc, afs_size_t filePos,
			   afs_size_t len, struct vrequest *areq,
			   int noLock)
{
    struct dcache *tdc = NULL;
    afs_size_t offset;

    /* read the cached info */
    if (noLock) {
	tdc = afs_FindDCache(avc, filePos);
	if (tdc)
	    ObtainWriteLock(&tdc->lock, 657);
    } else if (afs_blocksUsed >
    	       PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
	tdc = afs_FindDCache(avc, filePos);
	if (tdc) {
	    ObtainWriteLock(&tdc->lock, 658);
	    if (!hsame(tdc->f.versionNo, avc->f.m.DataVersion)
		|| (tdc->dflags & DFFetching)) {
		ReleaseWriteLock(&tdc->lock);
		afs_PutDCache(tdc);
		tdc = NULL;
	    }
	}
	if (!tdc) {
	    afs_MaybeWakeupTruncateDaemon();
	    while (afs_blocksUsed >
		   PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
		ReleaseWriteLock(&avc->lock);
		if (afs_blocksUsed - afs_blocksDiscarded >
		    PERCENT(CM_WAITFORDRAINPCT, afs_cacheBlocks)) {
		    afs_WaitForCacheDrain = 1;
		    afs_osi_Sleep(&afs_WaitForCacheDrain);
		}
		afs_MaybeFreeDiscardedDCache();
		afs_MaybeWakeupTruncateDaemon();
		ObtainWriteLock(&avc->lock, 509);
	    }
	    avc->f.states |= CDirty;
	    tdc = afs_GetDCache(avc, filePos, areq, &offset, &len, 4);
	    if (tdc)
		ObtainWriteLock(&tdc->lock, 659);
	}
    } else {
	tdc = afs_GetDCache(avc, filePos, areq, &offset, &len, 4);
	if (tdc)
	    ObtainWriteLock(&tdc->lock, 660);
    }
    if (tdc) {
	if (!(afs_indexFlags[tdc->index] & IFDataMod)) {
	    afs_stats_cmperf.cacheCurrDirtyChunks++;
	    afs_indexFlags[tdc->index] |= IFDataMod;	/* so it doesn't disappear */
	}
	if (!(tdc->f.states & DWriting)) {
	    /* don't mark entry as mod if we don't have to */
	    tdc->f.states |= DWriting;
	    tdc->dflags |= DFEntryMod;
	}
    }
    return tdc;
}

/*!
 * Make a shadow copy of a dir's dcache. It's used for disconnected
 * operations like remove/create/rename to keep the original directory data.
 * On reconnection, we can diff the original data with the server and get the
 * server changes and with the local data to get the local changes.
 *
 * \param avc The dir vnode.
 * \param adc The dir dcache.
 *
 * \return 0 for success.
 *
 * \note The vcache entry must be write locked.
 * \note The dcache entry must be read locked.
 */
int
afs_MakeShadowDir(struct vcache *avc, struct dcache *adc)
{
    int i, code, ret_code = 0, written, trans_size;
    struct dcache *new_dc = NULL;
    struct osi_file *tfile_src, *tfile_dst;
    struct VenusFid shadow_fid;
    char *data;

    /* Is this a dir? */
    if (vType(avc) != VDIR)
    	return ENOTDIR;

    if (avc->f.shadow.vnode || avc->f.shadow.unique)
	return EEXIST;

    /* Generate a fid for the shadow dir. */
    shadow_fid.Cell = avc->f.fid.Cell;
    shadow_fid.Fid.Volume = avc->f.fid.Fid.Volume;
    afs_GenShadowFid(&shadow_fid);

    ObtainWriteLock(&afs_xdcache, 716);

    /* Get a fresh dcache. */
    new_dc = afs_AllocDCache(avc, 0, 0, &shadow_fid);
    osi_Assert(new_dc);

    ObtainReadLock(&adc->mflock);

    /* Set up the new fid. */
    /* Copy interesting data from original dir dcache. */
    new_dc->mflags = adc->mflags;
    new_dc->dflags = adc->dflags;
    new_dc->f.modTime = adc->f.modTime;
    new_dc->f.versionNo = adc->f.versionNo;
    new_dc->f.states = adc->f.states;
    new_dc->f.chunk= adc->f.chunk;
    new_dc->f.chunkBytes = adc->f.chunkBytes;

    ReleaseReadLock(&adc->mflock);

    /* Now add to the two hash chains */
    i = DCHash(&shadow_fid, 0);
    afs_dcnextTbl[new_dc->index] = afs_dchashTbl[i];
    afs_dchashTbl[i] = new_dc->index;

    i = DVHash(&shadow_fid);
    afs_dvnextTbl[new_dc->index] = afs_dvhashTbl[i];
    afs_dvhashTbl[i] = new_dc->index;

    ReleaseWriteLock(&afs_xdcache);

    /* Alloc a 4k block. */
    data = afs_osi_Alloc(4096);
    if (!data) {
	afs_warn("afs_MakeShadowDir: could not alloc data\n");
	ret_code = ENOMEM;
	goto done;
    }

    /* Open the files. */
    tfile_src = afs_CFileOpen(&adc->f.inode);
    tfile_dst = afs_CFileOpen(&new_dc->f.inode);

    /* And now copy dir dcache data into this dcache,
     * 4k at a time.
     */
    written = 0;
    while (written < adc->f.chunkBytes) {
	trans_size = adc->f.chunkBytes - written;
	if (trans_size > 4096)
	    trans_size = 4096;

	/* Read a chunk from the dcache. */
	code = afs_CFileRead(tfile_src, written, data, trans_size);
	if (code < trans_size) {
	    ret_code = EIO;
	    break;
	}

	/* Write it to the new dcache. */
	code = afs_CFileWrite(tfile_dst, written, data, trans_size);
	if (code < trans_size) {
	    ret_code = EIO;
	    break;
	}

	written+=trans_size;
    }

    afs_CFileClose(tfile_dst);
    afs_CFileClose(tfile_src);

    afs_osi_Free(data, 4096);

    ReleaseWriteLock(&new_dc->lock);
    afs_PutDCache(new_dc);

    if (!ret_code) {
	ObtainWriteLock(&afs_xvcache, 763);
	ObtainWriteLock(&afs_disconDirtyLock, 765);
	QAdd(&afs_disconShadow, &avc->shadowq);
	osi_Assert((afs_RefVCache(avc) == 0));
	ReleaseWriteLock(&afs_disconDirtyLock);
	ReleaseWriteLock(&afs_xvcache);

	avc->f.shadow.vnode = shadow_fid.Fid.Vnode;
	avc->f.shadow.unique = shadow_fid.Fid.Unique;
    }

done:
    return ret_code;
}

/*!
 * Delete the dcaches of a shadow dir.
 *
 * \param avc The vcache containing the shadow fid.
 *
 * \note avc must be write locked.
 */
void
afs_DeleteShadowDir(struct vcache *avc)
{
    struct dcache *tdc;
    struct VenusFid shadow_fid;

    shadow_fid.Cell = avc->f.fid.Cell;
    shadow_fid.Fid.Volume = avc->f.fid.Fid.Volume;
    shadow_fid.Fid.Vnode = avc->f.shadow.vnode;
    shadow_fid.Fid.Unique = avc->f.shadow.unique;

    tdc = afs_FindDCacheByFid(&shadow_fid);
    if (tdc) {
    	afs_HashOutDCache(tdc, 1);
	afs_DiscardDCache(tdc);
    	afs_PutDCache(tdc);
    }
    avc->f.shadow.vnode = avc->f.shadow.unique = 0;
    ObtainWriteLock(&afs_disconDirtyLock, 708);
    QRemove(&avc->shadowq);
    ReleaseWriteLock(&afs_disconDirtyLock);
    afs_PutVCache(avc); /* Because we held it when we added to the queue */
}

/*!
 * Populate a dcache with empty chunks up to a given file size,
 * used before extending a file in order to avoid 'holes' which
 * we can't access in disconnected mode.
 *
 * \param avc   The vcache which is being extended (locked)
 * \param alen  The new length of the file
 *
 */
void
afs_PopulateDCache(struct vcache *avc, afs_size_t apos, struct vrequest *areq)
{
    struct dcache *tdc;
    afs_size_t len, offset;
    afs_int32 start, end;

    /* We're doing this to deal with the situation where we extend
     * by writing after lseek()ing past the end of the file . If that
     * extension skips chunks, then those chunks won't be created, and
     * GetDCache will assume that they have to be fetched from the server.
     * So, for each chunk between the current file position, and the new
     * length we GetDCache for that chunk.
     */

    if (AFS_CHUNK(apos) == 0 || apos <= avc->f.m.Length)
	return;

    if (avc->f.m.Length == 0)
	start = 0;
    else
    	start = AFS_CHUNK(avc->f.m.Length)+1;

    end = AFS_CHUNK(apos);

    while (start<end) {
	len = AFS_CHUNKTOSIZE(start);
	tdc = afs_GetDCache(avc, AFS_CHUNKTOBASE(start), areq, &offset, &len, 4);
	if (tdc)
	    afs_PutDCache(tdc);
	start++;
    }
}
