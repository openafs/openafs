/* Copyright (C) 1989 Transarc Corporation - All rights reserved */

/*
 * For copyright information, see IPL which you accepted in order to
 * download this software.
 *
 */

#include "../afs/param.h"
#include "../afs/sysincludes.h"
#if !defined(UKERNEL)
#include "../h/param.h"
#include "../h/types.h"
#include "../h/time.h"
#if	defined(AFS_AIX31_ENV) || defined(AFS_DEC_ENV)
#include "../h/limits.h"
#endif
#if	!defined(AFS_AIX_ENV) && !defined(AFS_SUN5_ENV) && !defined(AFS_SGI_ENV) && !defined(AFS_LINUX20_ENV)
#include "../h/kernel.h"    /* Doesn't needed, so it should go */
#endif
#endif /* !defined(UKERNEL) */

#include "../afs/afs_osi.h"
#include "../afsint/afsint.h"
#include "../afs/lock.h"

#if !defined(UKERNEL) && !defined(AFS_LINUX20_ENV)
#include "../h/buf.h"
#endif /* !defined(UKERNEL) */

#include "../afs/stds.h"
#include "../afs/volerrors.h"
#include "../afs/exporter.h"
#include "../afs/prs_fs.h"
#include "../afs/afs_chunkops.h"
#include "../afs/dir.h"

#include "../afs/afs_stats.h"
#include "../afs/longc_procs.h"

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
#define pHash(fid,page) ((((afs_int32)((fid)[0])) & PHFIDMASK) \
			 | (page & PHPAGEMASK))

/* Note: this should agree with the definition in kdump.c */
#if	defined(AFS_OSF_ENV)
#if	!defined(UKERNEL)
#define	AFS_USEBUFFERS	1
#endif
#endif

#ifdef	dirty
#undef dirty	/* XXX */
#endif

struct buffer {
    ino_t fid[1];	/* Unique cache key + i/o addressing */
    afs_int32 page;
    afs_int32 accesstime;
    struct buffer *hashNext;
    char *data;
    char lockers;
    char dirty;
    char hashIndex;
#if AFS_USEBUFFERS
    struct buf *bufp;
#endif
    afs_rwlock_t lock;		/* the lock for this structure */
} *Buffers = 0;

char *BufferData;

#ifdef	AFS_AIX_ENV
extern struct buf *geteblk();
#endif
/* The locks for individual buffer entries are now sometimes obtained while holding the
 * afs_bufferLock. Thus we now have a locking hierarchy: afs_bufferLock -> Buffers[].lock.
 */
static afs_lock_t afs_bufferLock;
static struct buffer *phTable[PHSIZE];	/* page hash table */
int nbuffers;
afs_int32 timecounter;

static struct buffer *afs_newslot();

static int dinit_flag = 0;
void DInit (abuffers)
    int abuffers; {
    /* Initialize the venus buffer system. */
    register int i;
    register struct buffer *tb;
#if AFS_USEBUFFERS
    struct buf *tub;	    /* unix buffer for allocation */
#endif

    AFS_STATCNT(DInit);
    if (dinit_flag) return;
    dinit_flag = 1;
#if AFS_USEBUFFERS
    /* round up to next multiple of NPB, since we allocate multiple pages per chunk */
    abuffers = ((abuffers-1) | (NPB-1)) + 1;
#endif
    LOCK_INIT(&afs_bufferLock, "afs_bufferLock");
    Buffers = (struct buffer *) afs_osi_Alloc(abuffers * sizeof(struct buffer));
#if !AFS_USEBUFFERS
    BufferData = (char *) afs_osi_Alloc(abuffers * AFS_BUFFER_PAGESIZE);
#endif
    timecounter = 1;
    afs_stats_cmperf.bufAlloced = nbuffers = abuffers;
    for(i=0;i<PHSIZE;i++) phTable[i] = 0;
    for (i=0;i<abuffers;i++) {
#if AFS_USEBUFFERS
	if ((i & (NPB-1)) == 0) {
	    /* time to allocate a fresh buffer */
	    tub = geteblk(AFS_BUFFER_PAGESIZE*NPB);
	    BufferData = (char *) tub->b_un.b_addr;
	}
#endif
        /* Fill in each buffer with an empty indication. */
	tb = &Buffers[i];
        dirp_Zap(tb->fid);
        tb->accesstime = 0;
	tb->lockers = 0;
#if AFS_USEBUFFERS
	if ((i & (NPB-1)) == 0) 
	    tb->bufp = tub;
	else
	    tb->bufp = 0;
	tb->data = &BufferData[AFS_BUFFER_PAGESIZE * (i&(NPB-1))];
#else
        tb->data = &BufferData[AFS_BUFFER_PAGESIZE*i];
#endif
	tb->hashIndex = 0;
        tb->dirty = 0;
	RWLOCK_INIT(&tb->lock, "buffer lock");
    }
    return;
}

char *DRead(fid,page)
    register ino_t *fid;
    register int page; {
    /* Read a page from the disk. */
    register struct buffer *tb, *tb2;
    void *tfile;
    register afs_int32 code, *sizep;

    AFS_STATCNT(DRead);
    MObtainWriteLock(&afs_bufferLock,256);

/* some new code added 1/1/92 */
#define bufmatch(tb) (tb->page == page && dirp_Eq(tb->fid, fid))
#define buf_Front(head,parent,p) {(parent)->hashNext = (p)->hashNext; (p)->hashNext= *(head);*(head)=(p);}

    /* this apparently-complicated-looking code is simply an example of
     * a little bit of loop unrolling, and is a standard linked-list 
     * traversal trick. It saves a few assignments at the the expense
     * of larger code size.  This could be simplified by better use of
     * macros. 
     */
    if ( tb = phTable[pHash(fid,page)] ) {  /* ASSMT HERE */
	if (bufmatch(tb)) {
	    MObtainWriteLock(&tb->lock,257);
	    ReleaseWriteLock(&afs_bufferLock);
	    tb->lockers++;
	    tb->accesstime = timecounter++;
	    AFS_STATS(afs_stats_cmperf.bufHits++);
	    MReleaseWriteLock(&tb->lock);
	    return tb->data;
	}
	else {
	  register struct buffer **bufhead;
	  bufhead = &( phTable[pHash(fid,page)] );
	  while (tb2 = tb->hashNext) {
	    if (bufmatch(tb2)) {
	      buf_Front(bufhead,tb,tb2);
	      MObtainWriteLock(&tb2->lock,258);
	      ReleaseWriteLock(&afs_bufferLock);
	      tb2->lockers++;
	      tb2->accesstime = timecounter++;
	      AFS_STATS(afs_stats_cmperf.bufHits++);
	      MReleaseWriteLock(&tb2->lock);
	      return tb2->data;
	    }
	    if (tb = tb2->hashNext) { /* ASSIGNMENT HERE! */ 
	      if (bufmatch(tb)) {
		buf_Front(bufhead,tb2,tb);
		MObtainWriteLock(&tb->lock,259);
		ReleaseWriteLock(&afs_bufferLock);
		tb->lockers++;
		tb->accesstime = timecounter++;
		AFS_STATS(afs_stats_cmperf.bufHits++);
		MReleaseWriteLock(&tb->lock);
		return tb->data;
	      }
	    }
	    else break;
	  }
	}
      }  
    else tb2 = NULL;

    AFS_STATS(afs_stats_cmperf.bufMisses++);
    /* can't find it */
    /* The last thing we looked at was either tb or tb2 (or nothing). That
     * is at least the oldest buffer on one particular hash chain, so it's 
     * a pretty good place to start looking for the truly oldest buffer.
     */
    tb = afs_newslot(fid, page, (tb ? tb : tb2));
    if (!tb) {
      MReleaseWriteLock(&afs_bufferLock);
      return 0;
    }
    tfile = afs_CFileOpen(fid[0]);
    sizep = (afs_int32 *)tfile;
    if (page * AFS_BUFFER_PAGESIZE >= *sizep) {
	dirp_Zap(tb->fid);
	afs_CFileClose(tfile);
	MReleaseWriteLock(&afs_bufferLock);
	return 0;
    }
    MObtainWriteLock(&tb->lock,260);
    MReleaseWriteLock(&afs_bufferLock);
    tb->lockers++;
    code = afs_CFileRead(tfile, tb->page * AFS_BUFFER_PAGESIZE,
			 tb->data, AFS_BUFFER_PAGESIZE);
    afs_CFileClose(tfile);
    if (code < AFS_BUFFER_PAGESIZE) {
	dirp_Zap(tb->fid);
	tb->lockers--;
	MReleaseWriteLock(&tb->lock);
	return 0;
    }
    /* Note that findslot sets the page field in the buffer equal to
     * what it is searching for. */
    MReleaseWriteLock(&tb->lock);
    return tb->data;
}

static void FixupBucket(ap)
    register struct buffer *ap; {
    register struct buffer **lp, *tp;
    register int i;
    /* first try to get it out of its current hash bucket, in which it
     * might not be */
    AFS_STATCNT(FixupBucket);
    i = ap->hashIndex;
    lp = &phTable[i];
    for(tp = *lp; tp; tp=tp->hashNext) {
	if (tp == ap) {
	    *lp = tp->hashNext;
	    break;
	}
	lp = &tp->hashNext;
    }
    /* now figure the new hash bucket */
    i = pHash(ap->fid,ap->page);
    ap->hashIndex = i;		/* remember where we are for deletion */
    ap->hashNext = phTable[i];	/* add us to the list */
    phTable[i] = ap;            /* at the front, since it's LRU */
}

static struct buffer *afs_newslot (afid,apage,lp)
     ino_t *afid;
     afs_int32 apage; 
     register struct buffer *lp;   /* pointer to a fairly-old buffer */
{
    /* Find a usable buffer slot */
    register afs_int32 i;
    afs_int32 lt;
    register struct buffer *tp;
    void *tfile;

    AFS_STATCNT(afs_newslot);
    /* we take a pointer here to a buffer which was at the end of an
     * LRU hash chain.  Odds are, it's one of the older buffers, not
     * one of the newer.  Having an older buffer to start with may
     * permit us to avoid a few of the assignments in the "typical
     * case" for loop below.
     */
    if (lp && (lp->lockers == 0)) {
      lt = lp->accesstime;
    }
    else {
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
      for (i=0;i<nbuffers;i++,tp++) {
	tp->accesstime = 0;
	if (!lp && !tp->lockers)  /* one is as good as the rest, I guess */
	  lp = tp;
      }
    }
    else {
      /* this is the typical case */
      tp = Buffers;
      for (i=0;i<nbuffers;i++,tp++) {
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
      afs_warn ("all buffers locked");
      return 0;
    }

    if (lp->dirty) {
	tfile = afs_CFileOpen(lp->fid[0]);
	afs_CFileWrite(tfile, lp->page * AFS_BUFFER_PAGESIZE, 
		       lp->data, AFS_BUFFER_PAGESIZE);	
        lp->dirty = 0;
	afs_CFileClose(tfile);
	AFS_STATS(afs_stats_cmperf.bufFlushDirty++);
      }

    /* Now fill in the header. */
    dirp_Cpy(lp->fid, afid);	/* set this */
    lp->page = apage;
    lp->accesstime = timecounter++;
    FixupBucket(lp);		/* move to the right hash bucket */

    return lp;
}

void DRelease (bp,flag)
    register struct buffer *bp;
    int flag; {
    /* Release a buffer, specifying whether or not the buffer has been
     * modified by the locker. */
    register int index;
#if AFS_USEBUFFERS
    register struct buffer *tp;
#endif

    AFS_STATCNT(DRelease);
    if (!bp) return;
#if AFS_USEBUFFERS
    /* look for buffer by scanning Unix buffers for appropriate address */
    tp = Buffers;
    for(index = 0; index < nbuffers; index += NPB, tp += NPB) {
	if ((afs_int32)bp >= (afs_int32)tp->data 
	    && (afs_int32)bp < (afs_int32)tp->data + AFS_BUFFER_PAGESIZE*NPB) {
	    /* we found the right range */
	    index += ((afs_int32)bp - (afs_int32)tp->data) >> LOGPS;
	    break;
	}
    }
#else
    index = (((char *)bp)-((char *)BufferData))>>LOGPS;
#endif
    bp = &(Buffers[index]);
    MObtainWriteLock(&bp->lock,261);
    bp->lockers--;
    if (flag) bp->dirty=1;
    MReleaseWriteLock(&bp->lock);
}

DVOffset (ap)
    register void *ap; {
    /* Return the byte within a file represented by a buffer pointer. */
    register struct buffer *bp;
    register int index;
#if AFS_USEBUFFERS
    register struct buffer *tp;
#endif
    AFS_STATCNT(DVOffset);
    bp=ap;
#if AFS_USEBUFFERS
    /* look for buffer by scanning Unix buffers for appropriate address */
    tp = Buffers;
    for(index = 0; index < nbuffers; index += NPB, tp += NPB) {
	if ((afs_int32)bp >= (afs_int32)tp->data && (afs_int32)bp < (afs_int32)tp->data + AFS_BUFFER_PAGESIZE*NPB) {
	    /* we found the right range */
	    index += ((afs_int32)bp - (afs_int32)tp->data) >> LOGPS;
	    break;
	}
    }
#else
    index = (((char *)bp)-((char *)BufferData))>>LOGPS;
#endif
    if (index<0 || index >= nbuffers) return -1;
    bp = &(Buffers[index]);
    return AFS_BUFFER_PAGESIZE*bp->page+(int)(((char *)ap)-bp->data);
}

/* 1/1/91 - I've modified the hash function to take the page as well
 * as the *fid, so that lookup will be a bit faster.  That presents some
 * difficulties for Zap, which now has to have some knowledge of the nature
 * of the hash function.  Oh well.  This should use the list traversal 
 * method of DRead...
 */
void DZap (fid)
    ino_t *fid;
{
    register int i;
    /* Destroy all buffers pertaining to a particular fid. */
    register struct buffer *tb;
    
    AFS_STATCNT(DZap);
    MObtainReadLock(&afs_bufferLock);

    for (i=0;i<=PHPAGEMASK;i++)
    for(tb=phTable[pHash(fid,i)]; tb; tb=tb->hashNext)
        if (dirp_Eq(tb->fid,fid)) {
	    MObtainWriteLock(&tb->lock,262);
            dirp_Zap(tb->fid);
            tb->dirty = 0;
	    MReleaseWriteLock(&tb->lock);
	}
    MReleaseReadLock(&afs_bufferLock);
}

void DFlush () {
    /* Flush all the modified buffers. */
    register int i, code;
    register struct buffer *tb;
    void *tfile;

    AFS_STATCNT(DFlush);
    tb = Buffers;
    MObtainReadLock(&afs_bufferLock);
    for(i=0;i<nbuffers;i++,tb++) {
        if (tb->dirty) {
	    MObtainWriteLock(&tb->lock,263);
	    tb->lockers++;
	    MReleaseReadLock(&afs_bufferLock);
	    if (tb->dirty) {
		tfile = afs_CFileOpen(tb->fid[0]);
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

char *DNew (fid,page)
    register int page;
    register ino_t *fid;
{
    /* Same as read, only do *not* even try to read the page, since it probably doesn't exist. */
    register struct buffer *tb;
    AFS_STATCNT(DNew);
    MObtainWriteLock(&afs_bufferLock,264);
    if ((tb = afs_newslot(fid,page,NULL)) == 0) {
	MReleaseWriteLock(&afs_bufferLock);
	return 0;
    }
    MObtainWriteLock(&tb->lock,265);
    MReleaseWriteLock(&afs_bufferLock);
    tb->lockers++;
    MReleaseWriteLock(&tb->lock);
    return tb->data;
}

void shutdown_bufferpackage() {
#if AFS_USEBUFFERS
    register struct buffer *tp;
#endif
    int i;
    extern int afs_cold_shutdown;

    AFS_STATCNT(shutdown_bufferpackage);
    /* Free all allocated Buffers and associated buffer pages */
    DFlush();
    if (afs_cold_shutdown) {
      dinit_flag = 0;
#if !AFS_USEBUFFERS
      afs_osi_Free(BufferData, nbuffers * AFS_BUFFER_PAGESIZE);
#else
      tp = Buffers;
      for (i=0; i < nbuffers; i+= NPB, tp += NPB) {
	/* The following check shouldn't be necessary and it will be removed soon */
	if (!tp->bufp) 
	    afs_warn("shutdown_bufferpackage: bufp == 0!! Shouldn't happen\n");
	else {
	    brelse(tp->bufp);
	    tp->bufp = 0;
	}
      }
#endif
      afs_osi_Free(Buffers, nbuffers * sizeof(struct buffer));
      nbuffers = 0;
      timecounter = 1;
      for(i=0;i<PHSIZE;i++) phTable[i] = 0;
      bzero((char *)&afs_bufferLock, sizeof(afs_lock_t));
  }
}  
