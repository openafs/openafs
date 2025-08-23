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

#include <roken.h>
#include <afs/opr.h>

#include <afs/afs_lock.h>

#include "dir.h"

#ifdef AFS_64BIT_IOPS_ENV
#define BUFFER_FID_SIZE (9*sizeof(int) + 2*sizeof(char*))
#else
#define BUFFER_FID_SIZE (6*sizeof(int) + 2*sizeof(char*))
#endif

struct buffer {
    /* fid is used for Unique cache key + i/o addressing.
     * fid size is based on 4 + size of inode and size of pointer
     */
    char fid[BUFFER_FID_SIZE];
    afs_int32 page;
    afs_int32 accesstime;
    struct buffer *hashNext;
    void *data;
    char lockers;
    char dirty;
    char hashIndex;
    struct Lock lock;
};

static_inline dir_file_t
bufferDir(struct buffer *b)
{
    return (dir_file_t) &b->fid;
}

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
#define pHash(fid) (((afs_int32 *)fid)[0] & (PHSIZE-1))
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

struct buffer *newslot(dir_file_t dir, afs_int32 apage,
		       struct buffer *lp);

/* XXX - This sucks. The correct prototypes for these functions are ...
 *
 * extern void FidZero(DirHandle *);
 * extern int  FidEq(DirHandle *a, DirHandle *b);
 * extern int  ReallyRead(DirHandle *a, int block, char *data, int *physerr);
 */

extern void FidZero(dir_file_t);
extern int FidEq(dir_file_t, dir_file_t);
extern int ReallyRead(dir_file_t, int block, char *data, int *physerr);
extern int ReallyWrite(dir_file_t, int block, char *data);
extern void FidZap(dir_file_t);
extern int  FidVolEq(dir_file_t, afs_int32 vid);
extern void FidCpy(dir_file_t, dir_file_t fromfile);

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
void
DInit(int abuffers)
{
    /* Initialize the venus buffer system. */
    int i, tsize;
    struct buffer *tb;
    char *tp;

    Lock_Init(&afs_bufferLock);
    /* Align each element of Buffers on a doubleword boundary */
    tsize = (sizeof(struct buffer) + 7) & ~7;
    tp = malloc(abuffers * tsize);
    Buffers = malloc(abuffers * sizeof(struct buffer *));
    BufferData = malloc(abuffers * BUFFER_PAGE_SIZE);
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
	FidZero(bufferDir(tb));
	tb->accesstime = tb->lockers = 0;
	tb->data = &BufferData[BUFFER_PAGE_SIZE * i];
	tb->hashIndex = 0;
	tb->dirty = 0;
	Lock_Init(&tb->lock);
    }
    return;
}

/**
 * read a page out of a directory object.
 *
 * @param[in]	fid	directory object fid
 * @param[in]   page	page in hash table to be read
 * @param[out]	entry	buffer to be filled w/ page contents
 * @param[out]	physerr	(optional) pointer to return possible errno
 *
 * @retval 0	success
 * @retval EIO	logical directory error or physical IO error;
 *		if *physerr was supplied, it will be set to 0
 *		for logical errors, or the errno for physical
 *		errors.
 */
int
DReadWithErrno(dir_file_t fid, int page, struct DirBuffer *entry, int *physerr)
{
    /* Read a page from the disk. */
    struct buffer *tb, *tb2, **bufhead;
    int code;

    if (physerr != NULL)
	*physerr = 0;

    memset(entry, 0, sizeof(struct DirBuffer));

    ObtainWriteLock(&afs_bufferLock);
    calls++;

#define bufmatch(tb,fid) (tb->page == page && FidEq(bufferDir(tb), fid))
#define buf_Front(head,parent,p) {(parent)->hashNext = (p)->hashNext; (p)->hashNext= *(head);*(head)=(p);}

    /* this apparently-complicated-looking code is simply an example of
     * a little bit of loop unrolling, and is a standard linked-list
     * traversal trick. It saves a few assignments at the the expense
     * of larger code size.  This could be simplified by better use of
     * macros.  With the use of these LRU queues, the old one-cache is
     * probably obsolete.
     */
    if ((tb = phTable[pHash(fid)])) {	/* ASSMT HERE */
	if (bufmatch(tb, fid)) {
	    ObtainWriteLock(&tb->lock);
	    tb->lockers++;
	    ReleaseWriteLock(&afs_bufferLock);
	    tb->accesstime = ++timecounter;
	    ReleaseWriteLock(&tb->lock);
	    entry->buffer = tb;
	    entry->data = tb->data;
	    return 0;
	} else {
	    bufhead = &(phTable[pHash(fid)]);
	    while ((tb2 = tb->hashNext)) {
		if (bufmatch(tb2, fid)) {
		    buf_Front(bufhead, tb, tb2);
		    ObtainWriteLock(&tb2->lock);
		    tb2->lockers++;
		    ReleaseWriteLock(&afs_bufferLock);
		    tb2->accesstime = ++timecounter;
		    ReleaseWriteLock(&tb2->lock);
		    entry->buffer = tb2;
		    entry->data = tb2->data;
		    return 0;
		}
		if ((tb = tb2->hashNext)) {	/* ASSIGNMENT HERE! */
		    if (bufmatch(tb, fid)) {
			buf_Front(bufhead, tb2, tb);
			ObtainWriteLock(&tb->lock);
			tb->lockers++;
			ReleaseWriteLock(&afs_bufferLock);
			tb->accesstime = ++timecounter;
			ReleaseWriteLock(&tb->lock);
			entry->buffer = tb;
			entry->data = tb->data;
			return 0;
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
    code = ReallyRead(bufferDir(tb), tb->page, tb->data, physerr);
    if (code != 0) {
	tb->lockers--;
	FidZap(bufferDir(tb));	/* disaster */
	ReleaseWriteLock(&tb->lock);
	return code;
    }
    /* Note that findslot sets the page field in the buffer equal to
     * what it is searching for.
     */
    ReleaseWriteLock(&tb->lock);
    entry->buffer = tb;
    entry->data = tb->data;
    return 0;
}

/**
 * read a page out of a directory object.
 *
 * @param[in]	fid   directory object fid
 * @param[in]	page  page in hash table to be read
 * @param[out]	entry buffer to be filled w/ page contents
 *
 * @retval 0	success
 * @retval EIO	logical directory error or physical IO error
 */
int
DRead(dir_file_t fid, int page, struct DirBuffer *entry)
{
    return DReadWithErrno(fid, page, entry, NULL);
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
    i = pHash(ap);
    ap->hashIndex = i;		/* remember where we are for deletion */
    ap->hashNext = phTable[i];	/* add us to the list */
    phTable[i] = ap;		/* at the front, since it's LRU */
    return 0;
}

struct buffer *
newslot(dir_file_t dir, afs_int32 apage, struct buffer *lp)
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
	if (ReallyWrite(bufferDir(lp), lp->page, lp->data))
	    Die("writing bogus buffer");
	lp->dirty = 0;
    }

    /* Now fill in the header. */
    FidZap(bufferDir(lp));
    FidCpy(bufferDir(lp), dir);	/* set this */
    memset(lp->data, 0, BUFFER_PAGE_SIZE);  /* Don't leak stale data. */
    lp->page = apage;
    lp->accesstime = ++timecounter;

    FixupBucket(lp);		/* move to the right hash bucket */

    return lp;
}

/* Release a buffer, specifying whether or not the buffer has been modified
 * by the locker. */
void
DRelease(struct DirBuffer *entry, int flag)
{
    struct buffer *bp;

    bp = (struct buffer *) entry->buffer;
    if (bp == NULL)
	return;
    ObtainWriteLock(&bp->lock);
    bp->lockers--;
    if (flag)
	bp->dirty = 1;
    ReleaseWriteLock(&bp->lock);
}

/* Return the byte within a file represented by a buffer pointer. */
int
DVOffset(struct DirBuffer *entry)
{
    struct buffer *bp;

    bp = entry->buffer;
    return BUFFER_PAGE_SIZE * bp->page + (char *)entry->data - (char *)bp->data;
}

void
DZap(dir_file_t dir)
{
    /* Destroy all buffers pertaining to a particular fid. */
    struct buffer *tb;
    ObtainReadLock(&afs_bufferLock);
    for (tb = phTable[pHash(dir)]; tb; tb = tb->hashNext)
	if (FidEq(bufferDir(tb), dir)) {
	    ObtainWriteLock(&tb->lock);
	    FidZap(bufferDir(tb));
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
	if (FidVolEq(bufferDir(tb), vid)) {
	    ObtainWriteLock(&tb->lock);
	    if (tb->dirty) {
		code = ReallyWrite(bufferDir(tb), tb->page, tb->data);
		if (code && !rcode)
		    rcode = code;
		tb->dirty = 0;
	    }
	    FidZap(bufferDir(tb));
	    ReleaseWriteLock(&tb->lock);
	}
    ReleaseReadLock(&afs_bufferLock);
    return rcode;
}

int
DFlushEntry(dir_file_t fid)
{
    /* Flush pages modified by one entry. */
    struct buffer *tb;
    int code;

    ObtainReadLock(&afs_bufferLock);
    for (tb = phTable[pHash(fid)]; tb; tb = tb->hashNext)
	if (FidEq(bufferDir(tb), fid) && tb->dirty) {
	    ObtainWriteLock(&tb->lock);
	    if (tb->dirty) {
		code = ReallyWrite(bufferDir(tb), tb->page, tb->data);
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
		code = ReallyWrite(bufferDir(*tbp), (*tbp)->page, (*tbp)->data);
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

/* Same as read, only do *not* even try to read the page,
 * since it probably doesn't exist.
 */
int
DNew(dir_file_t dir, int page, struct DirBuffer *entry)
{
    struct buffer *tb;

    memset(entry,0, sizeof(struct DirBuffer));

    ObtainWriteLock(&afs_bufferLock);
    if ((tb = newslot(dir, page, 0)) == 0) {
	ReleaseWriteLock(&afs_bufferLock);
	return EIO;
    }
    ObtainWriteLock(&tb->lock);
    tb->lockers++;
    ReleaseWriteLock(&afs_bufferLock);
    ReleaseWriteLock(&tb->lock);

    entry->buffer = tb;
    entry->data = tb->data;

    return 0;
}
