/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <stdlib.h>
#include <lock.h>

#ifdef AFS_64BIT_IOPS_ENV
#define BUFFER_FID_SIZE (9 + 2*sizeof(char*)/sizeof(int))
#else
#define BUFFER_FID_SIZE (6 + 2*sizeof(char*)/sizeof(int))
#endif

struct buffer {
    /* fid is used for Unique cache key + i/o addressing.
     * fid size is based on 4 + size of inode and size of pointer
     */
    afs_int32 fid[BUFFER_FID_SIZE];
    afs_int32 page;
    afs_int32 accesstime;
    struct buffer *hashNext;
    void *data;
    char lockers;
    char dirty;
    char hashIndex;
    struct Lock lock;
};

#include "dir.h"

struct Lock afs_bufferLock;

/* page size */
#define BUFFER_PAGE_SIZE 2048
/* log page size */
#define LOGPS 11
/* page hash table size */
#define PHSIZE 32
/* The hash table should be somewhat efficient even if there are only
 * a few partitions (less than 32). So the hash for the fileserver is now
 * based on the volume id. This means this macro is dependent upon the
 * layout of DirHandle in viced/viced.h, vol/salvage.h and volser/salvage.h.
 */
#define pHash(fid) ((fid)[0] & (PHSIZE-1))
#define vHash(vid) (vid & (PHSIZE-1))

/* admittedly system dependent, this is the maximum signed 32-bit value */
#define BUFFER_LONG_MAX   2147483647
#ifndef	NULL
#define NULL 0
#endif

static struct buffer **Buffers;

char *BufferData;

static struct buffer *phTable[PHSIZE];	/* page hash table */
static struct buffer *LastBuffer;
int nbuffers;
int timecounter;
static int calls = 0, ios = 0;

struct buffer *newslot(afs_int32 *afid, afs_int32 apage,
		       struct buffer *lp);

/* XXX - This sucks. The correct prototypes for these functions are ...
 *
 * extern void FidZero(DirHandle *);
 * extern int  FidEq(DirHandle *a, DirHandle *b);
 * extern int  ReallyRead(DirHandle *a, int block, char *data);
 */

extern void FidZero(afs_int32 *file);
extern int FidEq(afs_int32 *a, afs_int32 *b);
extern int ReallyRead(afs_int32 *file, int block, char *data);
extern int ReallyWrite(afs_int32 *file, int block, char *data);
extern void FidZap(afs_int32 *file);
extern int  FidVolEq(afs_int32 *file, afs_int32 vid);
extern void FidCpy(afs_int32 *tofile, afs_int32 *fromfile);
extern void Die(char *msg);

int
DStat(int *abuffers, int *acalls, int *aios)
{
    *abuffers = nbuffers;
    *acalls = calls;
    *aios = ios;
    return 0;
}

/**
 * initialize the directory package.
 *
 * @param[in] abuffers  size of directory buffer cache
 *
 * @return operation status
 *    @retval 0 success
 */
int
DInit(int abuffers)
{
    /* Initialize the venus buffer system. */
    int i, tsize;
    struct buffer *tb;
    char *tp;

    Lock_Init(&afs_bufferLock);
    /* Align each element of Buffers on a doubleword boundary */
    tsize = (sizeof(struct buffer) + 7) & ~7;
    tp = (char *)malloc(abuffers * tsize);
    Buffers = (struct buffer **)malloc(abuffers * sizeof(struct buffer *));
    BufferData = (char *)malloc(abuffers * BUFFER_PAGE_SIZE);
    timecounter = 0;
    LastBuffer = (struct buffer *)tp;
    nbuffers = abuffers;
    for (i = 0; i < PHSIZE; i++)
	phTable[i] = 0;
    for (i = 0; i < abuffers; i++) {
	/* Fill in each buffer with an empty indication. */
	tb = (struct buffer *)tp;
	Buffers[i] = tb;
	tp += tsize;
	FidZero(tb->fid);
	tb->accesstime = tb->lockers = 0;
	tb->data = &BufferData[BUFFER_PAGE_SIZE * i];
	tb->hashIndex = 0;
	tb->dirty = 0;
	Lock_Init(&tb->lock);
    }
    return 0;
}

/**
 * read a page out of a directory object.
 *
 * @param[in] fid   directory object fid
 * @param[in] page  page in hash table to be read
 *
 * @return pointer to requested page in directory cache
 *    @retval NULL read failed
 */
void *
DRead(afs_int32 *fid, int page)
{
    /* Read a page from the disk. */
    struct buffer *tb, *tb2, **bufhead;

    ObtainWriteLock(&afs_bufferLock);
    calls++;

#define bufmatch(tb) (tb->page == page && FidEq(tb->fid, fid))
#define buf_Front(head,parent,p) {(parent)->hashNext = (p)->hashNext; (p)->hashNext= *(head);*(head)=(p);}

    /* this apparently-complicated-looking code is simply an example of
     * a little bit of loop unrolling, and is a standard linked-list
     * traversal trick. It saves a few assignments at the the expense
     * of larger code size.  This could be simplified by better use of
     * macros.  With the use of these LRU queues, the old one-cache is
     * probably obsolete.
     */
    if ((tb = phTable[pHash(fid)])) {	/* ASSMT HERE */
	if (bufmatch(tb)) {
	    ObtainWriteLock(&tb->lock);
	    tb->lockers++;
	    ReleaseWriteLock(&afs_bufferLock);
	    tb->accesstime = ++timecounter;
	    ReleaseWriteLock(&tb->lock);
	    return tb->data;
	} else {
	    bufhead = &(phTable[pHash(fid)]);
	    while ((tb2 = tb->hashNext)) {
		if (bufmatch(tb2)) {
		    buf_Front(bufhead, tb, tb2);
		    ObtainWriteLock(&tb2->lock);
		    tb2->lockers++;
		    ReleaseWriteLock(&afs_bufferLock);
		    tb2->accesstime = ++timecounter;
		    ReleaseWriteLock(&tb2->lock);
		    return tb2->data;
		}
		if ((tb = tb2->hashNext)) {	/* ASSIGNMENT HERE! */
		    if (bufmatch(tb)) {
			buf_Front(bufhead, tb2, tb);
			ObtainWriteLock(&tb->lock);
			tb->lockers++;
			ReleaseWriteLock(&afs_bufferLock);
			tb->accesstime = ++timecounter;
			ReleaseWriteLock(&tb->lock);
			return tb->data;
		    }
		} else
		    break;
	    }
	}
    } else
	tb2 = NULL;

    /* can't find it */
    /* The last thing we looked at was either tb or tb2 (or nothing). That
     * is at least the oldest buffer on one particular hash chain, so it's
     * a pretty good place to start looking for the truly oldest buffer.
     */
    tb = newslot(fid, page, (tb ? tb : tb2));
    ios++;
    ObtainWriteLock(&tb->lock);
    tb->lockers++;
    ReleaseWriteLock(&afs_bufferLock);
    if (ReallyRead(tb->fid, tb->page, tb->data)) {
	tb->lockers--;
	FidZap(tb->fid);	/* disaster */
	ReleaseWriteLock(&tb->lock);
	return 0;
    }
    /* Note that findslot sets the page field in the buffer equal to
     * what it is searching for.
     */
    ReleaseWriteLock(&tb->lock);
    return tb->data;
}

static int
FixupBucket(struct buffer *ap)
{
    struct buffer **lp, *tp;
    int i;

    /* first try to get it out of its current hash bucket, in which it might not be */
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
    i = pHash(ap->fid);
    ap->hashIndex = i;		/* remember where we are for deletion */
    ap->hashNext = phTable[i];	/* add us to the list */
    phTable[i] = ap;		/* at the front, since it's LRU */
    return 0;
}

struct buffer *
newslot(afs_int32 *afid, afs_int32 apage, struct buffer *lp)
{
    /* Find a usable buffer slot */
    afs_int32 i;
    afs_int32 lt;
    struct buffer **tbp;

    if (lp && (lp->lockers == 0)) {
	lt = lp->accesstime;
    } else {
	lp = 0;
	lt = BUFFER_LONG_MAX;
    }

    tbp = Buffers;
    for (i = 0; i < nbuffers; i++, tbp++) {
	if ((*tbp)->lockers == 0) {
	    if ((*tbp)->accesstime < lt) {
		lp = (*tbp);
		lt = (*tbp)->accesstime;
	    }
	}
    }

    /* There are no unlocked buffers */
    if (lp == 0) {
	if (lt < 0)
	    Die("accesstime counter wrapped");
	else
	    Die("all buffers locked");
    }

    /* We do not need to lock the buffer here because it has no lockers
     * and the afs_bufferLock prevents other threads from zapping this
     * buffer while we are writing it out */
    if (lp->dirty) {
	if (ReallyWrite(lp->fid, lp->page, lp->data))
	    Die("writing bogus buffer");
	lp->dirty = 0;
    }

    /* Now fill in the header. */
    FidZap(lp->fid);
    FidCpy(lp->fid, afid);	/* set this */
    lp->page = apage;
    lp->accesstime = ++timecounter;

    FixupBucket(lp);		/* move to the right hash bucket */

    return lp;
}

/* Release a buffer, specifying whether or not the buffer has been modified
 * by the locker. */
void
DRelease(void *loc, int flag)
{
    struct buffer *bp = (struct buffer *)loc;
    int index;

    if (!bp)
	return;
    index = ((char *)bp - BufferData) >> LOGPS;
    bp = Buffers[index];
    ObtainWriteLock(&bp->lock);
    bp->lockers--;
    if (flag)
	bp->dirty = 1;
    ReleaseWriteLock(&bp->lock);
}

int
DVOffset(void *ap)
{
    /* Return the byte within a file represented by a buffer pointer. */
    struct buffer *bp = ap;
    int index;

    index = ((char *)bp - BufferData) >> LOGPS;
    if (index < 0 || index >= nbuffers)
	return -1;
    bp = Buffers[index];
    return BUFFER_PAGE_SIZE * bp->page + (char *)ap - (char *)bp->data;
}

void
DZap(afs_int32 *fid)
{
    /* Destroy all buffers pertaining to a particular fid. */
    struct buffer *tb;
    ObtainReadLock(&afs_bufferLock);
    for (tb = phTable[pHash(fid)]; tb; tb = tb->hashNext)
	if (FidEq(tb->fid, fid)) {
	    ObtainWriteLock(&tb->lock);
	    FidZap(tb->fid);
	    tb->dirty = 0;
	    ReleaseWriteLock(&tb->lock);
	}
    ReleaseReadLock(&afs_bufferLock);
}

int
DFlushVolume(afs_int32 vid)
{
    /* Flush all data and release all inode handles for a particular volume */
    struct buffer *tb;
    int code, rcode = 0;
    ObtainReadLock(&afs_bufferLock);
    for (tb = phTable[vHash(vid)]; tb; tb = tb->hashNext)
	if (FidVolEq(tb->fid, vid)) {
	    ObtainWriteLock(&tb->lock);
	    if (tb->dirty) {
		code = ReallyWrite(tb->fid, tb->page, tb->data);
		if (code && !rcode)
		    rcode = code;
		tb->dirty = 0;
	    }
	    FidZap(tb->fid);
	    ReleaseWriteLock(&tb->lock);
	}
    ReleaseReadLock(&afs_bufferLock);
    return rcode;
}

int
DFlushEntry(afs_int32 *fid)
{
    /* Flush pages modified by one entry. */
    struct buffer *tb;
    int code;

    ObtainReadLock(&afs_bufferLock);
    for (tb = phTable[pHash(fid)]; tb; tb = tb->hashNext)
	if (FidEq(tb->fid, fid) && tb->dirty) {
	    ObtainWriteLock(&tb->lock);
	    if (tb->dirty) {
		code = ReallyWrite(tb->fid, tb->page, tb->data);
		if (code) {
		    ReleaseWriteLock(&tb->lock);
		    ReleaseReadLock(&afs_bufferLock);
		    return code;
		}
		tb->dirty = 0;
	    }
	    ReleaseWriteLock(&tb->lock);
	}
    ReleaseReadLock(&afs_bufferLock);
    return 0;
}

int
DFlush(void)
{
    /* Flush all the modified buffers. */
    int i;
    struct buffer **tbp;
    afs_int32 code, rcode;

    rcode = 0;
    tbp = Buffers;
    ObtainReadLock(&afs_bufferLock);
    for (i = 0; i < nbuffers; i++, tbp++) {
	if ((*tbp)->dirty) {
	    ObtainWriteLock(&(*tbp)->lock);
	    (*tbp)->lockers++;
	    ReleaseReadLock(&afs_bufferLock);
	    if ((*tbp)->dirty) {
		code = ReallyWrite((*tbp)->fid, (*tbp)->page, (*tbp)->data);
		if (!code)
		    (*tbp)->dirty = 0;	/* Clear the dirty flag */
		if (code && !rcode) {
		    rcode = code;
		}
	    }
	    (*tbp)->lockers--;
	    ReleaseWriteLock(&(*tbp)->lock);
	    ObtainReadLock(&afs_bufferLock);
	}
    }
    ReleaseReadLock(&afs_bufferLock);
    return rcode;
}

void *
DNew(afs_int32 *fid, int page)
{
    /* Same as read, only do *not* even try to read the page,
     * since it probably doesn't exist.
     */
    struct buffer *tb;
    ObtainWriteLock(&afs_bufferLock);
    if ((tb = newslot(fid, page, 0)) == 0) {
	ReleaseWriteLock(&afs_bufferLock);
	return 0;
    }
    ObtainWriteLock(&tb->lock);
    tb->lockers++;
    ReleaseWriteLock(&afs_bufferLock);
    ReleaseWriteLock(&tb->lock);
    return tb->data;
}
