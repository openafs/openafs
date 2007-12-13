/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"
#include "afsincludes.h"
#if !defined(UKERNEL)
#include "h/param.h"
#include "h/types.h"
#include "h/time.h"
#if	defined(AFS_AIX31_ENV) 
#include "h/limits.h"
#endif
#if	!defined(AFS_AIX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_SGI_ENV) && !defined(AFS_LINUX20_ENV)
#include "h/kernel.h"		/* Doesn't needed, so it should go */
#endif
#endif /* !defined(UKERNEL) */

#include "afs/afs_osi.h"
#include "afsint.h"
#include "afs/lock.h"

#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "h/buf.h"
#endif /* !defined(UKERNEL) */

#include "afs/stds.h"
#include "afs/volerrors.h"
#include "afs/exporter.h"
#include "afs/prs_fs.h"
#include "afs/afs_chunkops.h"
#include "afs/dir.h"

#include "afs/afs_stats.h"
#include "afs/longc_procs.h"
#include "afs/afs.h"

#ifndef	BUF_TIME_MAX
#define	BUF_TIME_MAX	0x7fffffff
#endif
/* number of pages per Unix buffer, when we're using Unix buffer pool */
#define	NPB 4
/* page size */
#define AFS_BUFFER_PAGESIZE 2048
/* log page size */
#define LOGPS 11
/* If you change any of this PH stuff, make sure you don't break DZap() */
/* use last two bits for page */
#define PHPAGEMASK 3
/* use next five bits for fid */
#define PHFIDMASK 124
/* page hash table size - this is pretty intertwined with pHash */
#define PHSIZE (PHPAGEMASK + PHFIDMASK + 1)
/* the pHash macro */
#define pHash(fid,page) ((((afs_int32)(fid)) & PHFIDMASK) \
			 | (page & PHPAGEMASK))

#ifdef	dirty
#undef dirty			/* XXX */
#endif

static struct buffer *Buffers = 0;
static char *BufferData;

#ifdef	AFS_AIX_ENV
extern struct buf *geteblk();
#endif
#ifdef AFS_FBSD_ENV
#define timecounter afs_timecounter
#endif
/* The locks for individual buffer entries are now sometimes obtained while holding the
 * afs_bufferLock. Thus we now have a locking hierarchy: afs_bufferLock -> Buffers[].lock.
 */
static afs_lock_t afs_bufferLock;
static struct buffer *phTable[PHSIZE];	/* page hash table */
static int nbuffers;
static afs_int32 timecounter;

/* Prototypes for static routines */
static struct buffer *afs_newslot(struct dcache *adc, afs_int32 apage,
				  register struct buffer *lp);

static int dinit_flag = 0;
void
DInit(int abuffers)
{
    /* Initialize the venus buffer system. */
    register int i;
    register struct buffer *tb;
#if defined(AFS_USEBUFFERS)
    struct buf *tub;		/* unix buffer for allocation */
#endif

    AFS_STATCNT(DInit);
    if (dinit_flag)
	return;
    dinit_flag = 1;
#if defined(AFS_USEBUFFERS)
    /* round up to next multiple of NPB, since we allocate multiple pages per chunk */
    abuffers = ((abuffers - 1) | (NPB - 1)) + 1;
#endif
    LOCK_INIT(&afs_bufferLock, "afs_bufferLock");
    Buffers =
	(struct buffer *)afs_osi_Alloc(abuffers * sizeof(struct buffer));
#if !defined(AFS_USEBUFFERS)
    BufferData = (char *)afs_osi_Alloc(abuffers * AFS_BUFFER_PAGESIZE);
#endif
    timecounter = 1;
    afs_stats_cmperf.bufAlloced = nbuffers = abuffers;
    for (i = 0; i < PHSIZE; i++)
	phTable[i] = 0;
    for (i = 0; i < abuffers; i++) {
#if defined(AFS_USEBUFFERS)
	if ((i & (NPB - 1)) == 0) {
	    /* time to allocate a fresh buffer */
	    tub = geteblk(AFS_BUFFER_PAGESIZE * NPB);
	    BufferData = (char *)tub->b_un.b_addr;
	}
#endif
	/* Fill in each buffer with an empty indication. */
	tb = &Buffers[i];
	tb->fid = NULLIDX;
	tb->inode = 0;
	tb->accesstime = 0;
	tb->lockers = 0;
#if defined(AFS_USEBUFFERS)
	if ((i & (NPB - 1)) == 0)
	    tb->bufp = tub;
	else
	    tb->bufp = 0;
	tb->data = &BufferData[AFS_BUFFER_PAGESIZE * (i & (NPB - 1))];
#else
	tb->data = &BufferData[AFS_BUFFER_PAGESIZE * i];
#endif
	tb->hashIndex = 0;
	tb->dirty = 0;
	RWLOCK_INIT(&tb->lock, "buffer lock");
    }
    return;
}

void *
DRead(register struct dcache *adc, register int page)
{
    /* Read a page from the disk. */
    register struct buffer *tb, *tb2;
    struct osi_file *tfile;
    int code;

    AFS_STATCNT(DRead);
    MObtainWriteLock(&afs_bufferLock, 256);

#define bufmatch(tb) (tb->page == page && tb->fid == adc->index)
#define buf_Front(head,parent,p) {(parent)->hashNext = (p)->hashNext; (p)->hashNext= *(head);*(head)=(p);}

    /* this apparently-complicated-looking code is simply an example of
     * a little bit of loop unrolling, and is a standard linked-list 
     * traversal trick. It saves a few assignments at the the expense
     * of larger code size.  This could be simplified by better use of
     * macros. 
     */
    if ((tb = phTable[pHash(adc->index, page)])) {
	if (bufmatch(tb)) {
	    MObtainWriteLock(&tb->lock, 257);
	    ReleaseWriteLock(&afs_bufferLock);
	    tb->lockers++;
	    tb->accesstime = timecounter++;
	    AFS_STATS(afs_stats_cmperf.bufHits++);
	    MReleaseWriteLock(&tb->lock);
	    return tb->data;
	} else {
	    register struct buffer **bufhead;
	    bufhead = &(phTable[pHash(adc->index, page)]);
	    while ((tb2 = tb->hashNext)) {
		if (bufmatch(tb2)) {
		    buf_Front(bufhead, tb, tb2);
		    MObtainWriteLock(&tb2->lock, 258);
		    ReleaseWriteLock(&afs_bufferLock);
		    tb2->lockers++;
		    tb2->accesstime = timecounter++;
		    AFS_STATS(afs_stats_cmperf.bufHits++);
		    MReleaseWriteLock(&tb2->lock);
		    return tb2->data;
		}
		if ((tb = tb2->hashNext)) {
		    if (bufmatch(tb)) {
			buf_Front(bufhead, tb2, tb);
			MObtainWriteLock(&tb->lock, 259);
			ReleaseWriteLock(&afs_bufferLock);
			tb->lockers++;
			tb->accesstime = timecounter++;
			AFS_STATS(afs_stats_cmperf.bufHits++);
			MReleaseWriteLock(&tb->lock);
			return tb->data;
		    }
		} else
		    break;
	    }
	}
    } else
	tb2 = NULL;

    AFS_STATS(afs_stats_cmperf.bufMisses++);
    /* can't find it */
    /* The last thing we looked at was either tb or tb2 (or nothing). That
     * is at least the oldest buffer on one particular hash chain, so it's 
     * a pretty good place to start looking for the truly oldest buffer.
     */
    tb = afs_newslot(adc, page, (tb ? tb : tb2));
    if (!tb) {
	MReleaseWriteLock(&afs_bufferLock);
	return NULL;
    }
    MObtainWriteLock(&tb->lock, 260);
    MReleaseWriteLock(&afs_bufferLock);
    tb->lockers++;
    if (page * AFS_BUFFER_PAGESIZE >= adc->f.chunkBytes) {
	tb->fid = NULLIDX;
	tb->inode = 0;
	tb->lockers--;
	MReleaseWriteLock(&tb->lock);
	return NULL;
    }
    tfile = afs_CFileOpen(adc->f.inode);
    code =
	afs_CFileRead(tfile, tb->page * AFS_BUFFER_PAGESIZE, tb->data,
		      AFS_BUFFER_PAGESIZE);
    afs_CFileClose(tfile);
    if (code < AFS_BUFFER_PAGESIZE) {
	tb->fid = NULLIDX;
	tb->inode = 0;
	tb->lockers--;
	MReleaseWriteLock(&tb->lock);
	return NULL;
    }
    /* Note that findslot sets the page field in the buffer equal to
     * what it is searching for. */
    MReleaseWriteLock(&tb->lock);
    return tb->data;
}

static void
FixupBucket(register struct buffer *ap)
{
    register struct buffer **lp, *tp;
    register int i;
    /* first try to get it out of its current hash bucket, in which it
     * might not be */
    AFS_STATCNT(FixupBucket);
    i = ap->hashIndex;
    lp = &phTable[i];
    for (tp = *lp; tp; tp = tp->hashNext) {
	if (tp == ap) {
	    *lp = tp->hashNext;
	    break;
	}
	lp = &tp->hashNext;
    }
    /* now figure the new hash bucket */
    i = pHash(ap->fid, ap->page);
    ap->hashIndex = i;		/* remember where we are for deletion */
    ap->hashNext = phTable[i];	/* add us to the list */
    phTable[i] = ap;		/* at the front, since it's LRU */
}

/* lp is pointer to a fairly-old buffer */
static struct buffer *
afs_newslot(struct dcache *adc, afs_int32 apage, register struct buffer *lp)
{
    /* Find a usable buffer slot */
    register afs_int32 i;
    afs_int32 lt;
    register struct buffer *tp;
    struct osi_file *tfile;

    AFS_STATCNT(afs_newslot);
    /* we take a pointer here to a buffer which was at the end of an
     * LRU hash chain.  Odds are, it's one of the older buffers, not
     * one of the newer.  Having an older buffer to start with may
     * permit us to avoid a few of the assignments in the "typical
     * case" for loop below.
     */
    if (lp && (lp->lockers == 0)) {
	lt = lp->accesstime;
    } else {
	lp = 0;
	lt = BUF_TIME_MAX;
    }

    /* timecounter might have wrapped, if machine is very very busy
     * and stays up for a long time.  Timecounter mustn't wrap twice
     * (positive->negative->positive) before calling newslot, but that
     * would require 2 billion consecutive cache hits... Anyway, the
     * penalty is only that the cache replacement policy will be
     * almost MRU for the next ~2 billion DReads...  newslot doesn't
     * get called nearly as often as DRead, so in order to avoid the
     * performance penalty of using the hypers, it's worth doing the
     * extra check here every time.  It's probably cheaper than doing
     * hcmp, anyway.  There is a little performance hit resulting from
     * resetting all the access times to 0, but it only happens once
     * every month or so, and the access times will rapidly sort
     * themselves back out after just a few more DReads.
     */
    if (timecounter < 0) {
	timecounter = 1;
	tp = Buffers;
	for (i = 0; i < nbuffers; i++, tp++) {
	    tp->accesstime = 0;
	    if (!lp && !tp->lockers)	/* one is as good as the rest, I guess */
		lp = tp;
	}
    } else {
	/* this is the typical case */
	tp = Buffers;
	for (i = 0; i < nbuffers; i++, tp++) {
	    if (tp->lockers == 0) {
		if (tp->accesstime < lt) {
		    lp = tp;
		    lt = tp->accesstime;
		}
	    }
	}
    }

    if (lp == 0) {
	/* There are no unlocked buffers -- this used to panic, but that
	 * seems extreme.  To the best of my knowledge, all the callers
	 * of DRead are prepared to handle a zero return.  Some of them
	 * just panic directly, but not all of them. */
	afs_warn("all buffers locked");
	return 0;
    }

    if (lp->dirty) {
	/* see DFlush for rationale for not getting and locking the dcache */
	tfile = afs_CFileOpen(lp->inode);
	afs_CFileWrite(tfile, lp->page * AFS_BUFFER_PAGESIZE, lp->data,
		       AFS_BUFFER_PAGESIZE);
	lp->dirty = 0;
	afs_CFileClose(tfile);
	AFS_STATS(afs_stats_cmperf.bufFlushDirty++);
    }

    /* Now fill in the header. */
    lp->fid = adc->index;
    lp->inode = adc->f.inode;
    lp->page = apage;
    lp->accesstime = timecounter++;
    FixupBucket(lp);		/* move to the right hash bucket */

    return lp;
}

void
DRelease(register struct buffer *bp, int flag)
{
    /* Release a buffer, specifying whether or not the buffer has been
     * modified by the locker. */
    register int index;
#if defined(AFS_USEBUFFERS)
    register struct buffer *tp;
#endif

    AFS_STATCNT(DRelease);
    if (!bp)
	return;
#if defined(AFS_USEBUFFERS)
    /* look for buffer by scanning Unix buffers for appropriate address */
    tp = Buffers;
    for (index = 0; index < nbuffers; index += NPB, tp += NPB) {
	if ((afs_int32) bp >= (afs_int32) tp->data
	    && (afs_int32) bp <
	    (afs_int32) tp->data + AFS_BUFFER_PAGESIZE * NPB) {
	    /* we found the right range */
	    index += ((afs_int32) bp - (afs_int32) tp->data) >> LOGPS;
	    break;
	}
    }
#else
    index = (((char *)bp) - ((char *)BufferData)) >> LOGPS;
#endif
    bp = &(Buffers[index]);
    MObtainWriteLock(&bp->lock, 261);
    bp->lockers--;
    if (flag)
	bp->dirty = 1;
    MReleaseWriteLock(&bp->lock);
}

int
DVOffset(register void *ap)
{
    /* Return the byte within a file represented by a buffer pointer. */
    register struct buffer *bp;
    register int index;
#if defined(AFS_USEBUFFERS)
    register struct buffer *tp;
#endif
    AFS_STATCNT(DVOffset);
    bp = ap;
#if defined(AFS_USEBUFFERS)
    /* look for buffer by scanning Unix buffers for appropriate address */
    tp = Buffers;
    for (index = 0; index < nbuffers; index += NPB, tp += NPB) {
	if ((afs_int32) bp >= (afs_int32) tp->data
	    && (afs_int32) bp <
	    (afs_int32) tp->data + AFS_BUFFER_PAGESIZE * NPB) {
	    /* we found the right range */
	    index += ((afs_int32) bp - (afs_int32) tp->data) >> LOGPS;
	    break;
	}
    }
#else
    index = (((char *)bp) - ((char *)BufferData)) >> LOGPS;
#endif
    if (index < 0 || index >= nbuffers)
	return -1;
    bp = &(Buffers[index]);
    return AFS_BUFFER_PAGESIZE * bp->page + (int)(((char *)ap) - bp->data);
}

/* 1/1/91 - I've modified the hash function to take the page as well
 * as the *fid, so that lookup will be a bit faster.  That presents some
 * difficulties for Zap, which now has to have some knowledge of the nature
 * of the hash function.  Oh well.  This should use the list traversal 
 * method of DRead...
 */
void
DZap(struct dcache *adc)
{
    register int i;
    /* Destroy all buffers pertaining to a particular fid. */
    register struct buffer *tb;

    AFS_STATCNT(DZap);
    MObtainReadLock(&afs_bufferLock);

    for (i = 0; i <= PHPAGEMASK; i++)
	for (tb = phTable[pHash(adc->index, i)]; tb; tb = tb->hashNext)
	    if (tb->fid == adc->index) {
		MObtainWriteLock(&tb->lock, 262);
		tb->fid = NULLIDX;
		tb->inode = 0;
		tb->dirty = 0;
		MReleaseWriteLock(&tb->lock);
	    }
    MReleaseReadLock(&afs_bufferLock);
}

void
DFlush(void)
{
    /* Flush all the modified buffers. */
    register int i;
    register struct buffer *tb;
    struct osi_file *tfile;

    AFS_STATCNT(DFlush);
    tb = Buffers;
    MObtainReadLock(&afs_bufferLock);
    for (i = 0; i < nbuffers; i++, tb++) {
	if (tb->dirty) {
	    MObtainWriteLock(&tb->lock, 263);
	    tb->lockers++;
	    MReleaseReadLock(&afs_bufferLock);
	    if (tb->dirty) {
		/* it seems safe to do this I/O without having the dcache
		 * locked, since the only things that will update the data in
		 * a directory are the buffer package, which holds the relevant
		 * tb->lock while doing the write, or afs_GetDCache, which 
		 * DZap's the directory while holding the dcache lock.
		 * It is not possible to lock the dcache or even call
		 * afs_GetDSlot to map the index to the dcache since the dir
		 * package's caller has some dcache object locked already (so
		 * we cannot lock afs_xdcache). In addition, we cannot obtain
		 * a dcache lock while holding the tb->lock of the same file
		 * since that can deadlock with DRead/DNew */
		tfile = afs_CFileOpen(tb->inode);
		afs_CFileWrite(tfile, tb->page * AFS_BUFFER_PAGESIZE,
			       tb->data, AFS_BUFFER_PAGESIZE);
		tb->dirty = 0;	/* Clear the dirty flag */
		afs_CFileClose(tfile);
	    }
	    tb->lockers--;
	    MReleaseWriteLock(&tb->lock);
	    MObtainReadLock(&afs_bufferLock);
	}
    }
    MReleaseReadLock(&afs_bufferLock);
}

void *
DNew(register struct dcache *adc, register int page)
{
    /* Same as read, only do *not* even try to read the page, since it probably doesn't exist. */
    register struct buffer *tb;
    AFS_STATCNT(DNew);
    MObtainWriteLock(&afs_bufferLock, 264);
    if ((tb = afs_newslot(adc, page, NULL)) == 0) {
	MReleaseWriteLock(&afs_bufferLock);
	return 0;
    }
    /* extend the chunk, if needed */
    /* Do it now, not in DFlush or afs_newslot when the data is written out,
     * since now our caller has adc->lock writelocked, and we can't acquire
     * that lock (or even map from a fid to a dcache) in afs_newslot or
     * DFlush due to lock hierarchy issues */
    if ((page + 1) * AFS_BUFFER_PAGESIZE > adc->f.chunkBytes) {
	afs_AdjustSize(adc, (page + 1) * AFS_BUFFER_PAGESIZE);
	afs_WriteDCache(adc, 1);
    }
    MObtainWriteLock(&tb->lock, 265);
    MReleaseWriteLock(&afs_bufferLock);
    tb->lockers++;
    MReleaseWriteLock(&tb->lock);
    return tb->data;
}

void
shutdown_bufferpackage(void)
{
#if defined(AFS_USEBUFFERS)
    register struct buffer *tp;
#endif
    int i;

    AFS_STATCNT(shutdown_bufferpackage);
    /* Free all allocated Buffers and associated buffer pages */
    DFlush();
    if (afs_cold_shutdown) {
	dinit_flag = 0;
#if !defined(AFS_USEBUFFERS)
	afs_osi_Free(BufferData, nbuffers * AFS_BUFFER_PAGESIZE);
#else
	tp = Buffers;
	for (i = 0; i < nbuffers; i += NPB, tp += NPB) {
	    /* The following check shouldn't be necessary and it will be removed soon */
	    if (!tp->bufp)
		afs_warn
		    ("shutdown_bufferpackage: bufp == 0!! Shouldn't happen\n");
	    else {
		brelse(tp->bufp);
		tp->bufp = 0;
	    }
	}
#endif
	afs_osi_Free(Buffers, nbuffers * sizeof(struct buffer));
	nbuffers = 0;
	timecounter = 1;
	for (i = 0; i < PHSIZE; i++)
	    phTable[i] = 0;
	memset((char *)&afs_bufferLock, 0, sizeof(afs_lock_t));
    }
}
