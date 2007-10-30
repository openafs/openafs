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

RCSID
    ("$Header$");

#include <sys/types.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#endif
#include <errno.h>
#include <string.h>
#include <lock.h>
#include <rx/xdr.h>



#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

#define	PHSIZE	128
static struct buffer {
    struct ubik_dbase *dbase;	/* dbase within which the buffer resides */
    afs_int32 file;		/* Unique cache key */
    afs_int32 page;		/* page number */
    struct buffer *lru_next;
    struct buffer *lru_prev;
    struct buffer *hashNext;	/* next dude in hash table */
    char *data;			/* ptr to the data */
    char lockers;		/* usage ref count */
    char dirty;			/* is buffer modified */
    char hashIndex;		/* back ptr to hash table */
} *Buffers;

#define pHash(page) ((page) & (PHSIZE-1))

afs_int32 ubik_nBuffers = NBUFFERS;
static struct buffer *phTable[PHSIZE];	/* page hash table */
static struct buffer *LruBuffer;
static int nbuffers;
static int calls = 0, ios = 0, lastb = 0;
static char *BufferData;
static struct buffer *newslot(struct ubik_dbase *adbase, afs_int32 afid,
			      afs_int32 apage);
static initd = 0;
#define	BADFID	    0xffffffff

static DTrunc(struct ubik_dbase *dbase, afs_int32 fid, afs_int32 length);

static struct ubik_trunc *freeTruncList = 0;

/* remove a transaction from the database's active transaction list.  Don't free it */
static int
unthread(struct ubik_trans *atrans)
{
    struct ubik_trans **lt, *tt;
    lt = &atrans->dbase->activeTrans;
    for (tt = *lt; tt; lt = &tt->next, tt = *lt) {
	if (tt == atrans) {
	    /* found it */
	    *lt = tt->next;
	    return 0;
	}
    }
    return 2;			/* no entry */
}

/* some debugging assistance */
int
udisk_Debug(struct ubik_debug *aparm)
{
    struct buffer *tb;
    int i;

    memcpy(&aparm->localVersion, &ubik_dbase->version,
	   sizeof(struct ubik_version));
    aparm->lockedPages = 0;
    aparm->writeLockedPages = 0;
    tb = Buffers;
    for (i = 0; i < nbuffers; i++, tb++) {
	if (tb->lockers) {
	    aparm->lockedPages++;
	    if (tb->dirty)
		aparm->writeLockedPages++;
	}
    }
    return 0;
}

/* log format is defined here, and implicitly in recovery.c
 *
 * 4 byte opcode, followed by parameters, each 4 bytes long.  All integers
 * are in logged in network standard byte order, in case we want to move logs
 * from machine-to-machine someday.
 *
 * Begin transaction: opcode
 * Commit transaction: opcode, version (8 bytes)
 * Truncate file: opcode, file number, length
 * Abort transaction: opcode
 * Write data: opcode, file, position, length, <length> data bytes
 */

/* write an opcode to the log */
int
udisk_LogOpcode(struct ubik_dbase *adbase, afs_int32 aopcode, int async)
{
    struct ubik_stat ustat;
    afs_int32 code;

    /* figure out where to write */
    code = (*adbase->stat) (adbase, LOGFILE, &ustat);
    if (code < 0)
	return code;

    /* setup data and do write */
    aopcode = htonl(aopcode);
    code =
	(*adbase->write) (adbase, LOGFILE, (char *)&aopcode, ustat.size,
			  sizeof(afs_int32));
    if (code != sizeof(afs_int32))
	return UIOERROR;

    /* optionally sync data */
    if (async)
	code = (*adbase->sync) (adbase, LOGFILE);
    else
	code = 0;
    return code;
}

/* log a commit, never syncing */
int
udisk_LogEnd(struct ubik_dbase *adbase, struct ubik_version *aversion)
{
    afs_int32 code;
    afs_int32 data[3];
    struct ubik_stat ustat;

    /* figure out where to write */
    code = (*adbase->stat) (adbase, LOGFILE, &ustat);
    if (code)
	return code;

    /* setup data */
    data[0] = htonl(LOGEND);
    data[1] = htonl(aversion->epoch);
    data[2] = htonl(aversion->counter);

    /* do write */
    code =
	(*adbase->write) (adbase, LOGFILE, (char *)data, ustat.size,
			  3 * sizeof(afs_int32));
    if (code != 3 * sizeof(afs_int32))
	return UIOERROR;

    /* finally sync the log */
    code = (*adbase->sync) (adbase, LOGFILE);
    return code;
}

/* log a truncate operation, never syncing */
int
udisk_LogTruncate(struct ubik_dbase *adbase, afs_int32 afile,
		  afs_int32 alength)
{
    afs_int32 code;
    afs_int32 data[3];
    struct ubik_stat ustat;

    /* figure out where to write */
    code = (*adbase->stat) (adbase, LOGFILE, &ustat);
    if (code < 0)
	return code;

    /* setup data */
    data[0] = htonl(LOGTRUNCATE);
    data[1] = htonl(afile);
    data[2] = htonl(alength);

    /* do write */
    code =
	(*adbase->write) (adbase, LOGFILE, (char *)data, ustat.size,
			  3 * sizeof(afs_int32));
    if (code != 3 * sizeof(afs_int32))
	return UIOERROR;
    return 0;
}

/* write some data to the log, never syncing */
int
udisk_LogWriteData(struct ubik_dbase *adbase, afs_int32 afile, char *abuffer,
		   afs_int32 apos, afs_int32 alen)
{
    struct ubik_stat ustat;
    afs_int32 code;
    afs_int32 data[4];
    afs_int32 lpos;

    /* find end of log */
    code = (*adbase->stat) (adbase, LOGFILE, &ustat);
    lpos = ustat.size;
    if (code < 0)
	return code;

    /* setup header */
    data[0] = htonl(LOGDATA);
    data[1] = htonl(afile);
    data[2] = htonl(apos);
    data[3] = htonl(alen);

    /* write header */
    code =
	(*adbase->write) (adbase, LOGFILE, (char *)data, lpos, 4 * sizeof(afs_int32));
    if (code != 4 * sizeof(afs_int32))
	return UIOERROR;
    lpos += 4 * sizeof(afs_int32);

    /* write data */
    code = (*adbase->write) (adbase, LOGFILE, abuffer, lpos, alen);
    if (code != alen)
	return UIOERROR;
    return 0;
}

static int
DInit(int abuffers)
{
    /* Initialize the venus buffer system. */
    int i;
    struct buffer *tb;
    Buffers = (struct buffer *)malloc(abuffers * sizeof(struct buffer));
    memset(Buffers, 0, abuffers * sizeof(struct buffer));
    BufferData = (char *)malloc(abuffers * UBIK_PAGESIZE);
    nbuffers = abuffers;
    for (i = 0; i < PHSIZE; i++)
	phTable[i] = 0;
    for (i = 0; i < abuffers; i++) {
	/* Fill in each buffer with an empty indication. */
	tb = &Buffers[i];
	tb->lru_next = &(Buffers[i + 1]);
	tb->lru_prev = &(Buffers[i - 1]);
	tb->data = &BufferData[UBIK_PAGESIZE * i];
	tb->file = BADFID;
    }
    Buffers[0].lru_prev = &(Buffers[abuffers - 1]);
    Buffers[abuffers - 1].lru_next = &(Buffers[0]);
    LruBuffer = &(Buffers[0]);
    return 0;
}

/* Take a buffer and mark it as the least recently used buffer */
static void
Dlru(struct buffer *abuf)
{
    if (LruBuffer == abuf)
	return;

    /* Unthread from where it is in the list */
    abuf->lru_next->lru_prev = abuf->lru_prev;
    abuf->lru_prev->lru_next = abuf->lru_next;

    /* Thread onto beginning of LRU list */
    abuf->lru_next = LruBuffer;
    abuf->lru_prev = LruBuffer->lru_prev;

    LruBuffer->lru_prev->lru_next = abuf;
    LruBuffer->lru_prev = abuf;
    LruBuffer = abuf;
}

/* Take a buffer and mark it as the most recently used buffer */
static void
Dmru(struct buffer *abuf)
{
    if (LruBuffer == abuf) {
	LruBuffer = LruBuffer->lru_next;
	return;
    }

    /* Unthread from where it is in the list */
    abuf->lru_next->lru_prev = abuf->lru_prev;
    abuf->lru_prev->lru_next = abuf->lru_next;

    /* Thread onto end of LRU list - making it the MRU buffer */
    abuf->lru_next = LruBuffer;
    abuf->lru_prev = LruBuffer->lru_prev;
    LruBuffer->lru_prev->lru_next = abuf;
    LruBuffer->lru_prev = abuf;
}

/* get a pointer to a particular buffer */
static char *
DRead(struct ubik_dbase *dbase, afs_int32 fid, int page)
{
    /* Read a page from the disk. */
    struct buffer *tb, *lastbuffer;
    afs_int32 code;

    calls++;
    lastbuffer = LruBuffer->lru_prev;

    if ((lastbuffer->page == page) && (lastbuffer->file == fid)
	&& (lastbuffer->dbase == dbase)) {
	tb = lastbuffer;
	tb->lockers++;
	lastb++;
	return tb->data;
    }
    for (tb = phTable[pHash(page)]; tb; tb = tb->hashNext) {
	if (tb->page == page && tb->file == fid && tb->dbase == dbase) {
	    Dmru(tb);
	    tb->lockers++;
	    return tb->data;
	}
    }
    /* can't find it */
    tb = newslot(dbase, fid, page);
    if (!tb)
	return 0;
    memset(tb->data, 0, UBIK_PAGESIZE);

    tb->lockers++;
    code =
	(*dbase->read) (dbase, fid, tb->data, page * UBIK_PAGESIZE,
			UBIK_PAGESIZE);
    if (code < 0) {
	tb->file = BADFID;
	Dlru(tb);
	tb->lockers--;
	ubik_print("Ubik: Error reading database file: errno=%d\n", errno);
	return 0;
    }
    ios++;

    /* Note that findslot sets the page field in the buffer equal to
     * what it is searching for.
     */
    return tb->data;
}

/* zap truncated pages */
static int
DTrunc(struct ubik_dbase *dbase, afs_int32 fid, afs_int32 length)
{
    afs_int32 maxPage;
    struct buffer *tb;
    int i;

    maxPage = (length + UBIK_PAGESIZE - 1) >> UBIK_LOGPAGESIZE;	/* first invalid page now in file */
    for (i = 0, tb = Buffers; i < nbuffers; i++, tb++) {
	if (tb->page >= maxPage && tb->file == fid && tb->dbase == dbase) {
	    tb->file = BADFID;
	    Dlru(tb);
	}
    }
    return 0;
}

/* allocate a truncation entry.  We allocate special entries representing truncations, rather than
    performing them immediately, so that we can abort a transaction easily by simply purging
    the in-core memory buffers and discarding these truncation entries.
*/
static struct ubik_trunc *
GetTrunc(void)
{
    struct ubik_trunc *tt;
    if (!freeTruncList) {
	freeTruncList =
	    (struct ubik_trunc *)malloc(sizeof(struct ubik_trunc));
	freeTruncList->next = (struct ubik_trunc *)0;
    }
    tt = freeTruncList;
    freeTruncList = tt->next;
    return tt;
}

/* free a truncation entry */
static int
PutTrunc(struct ubik_trunc *at)
{
    at->next = freeTruncList;
    freeTruncList = at;
    return 0;
}

/* find a truncation entry for a file, if any */
static struct ubik_trunc *
FindTrunc(struct ubik_trans *atrans, afs_int32 afile)
{
    struct ubik_trunc *tt;
    for (tt = atrans->activeTruncs; tt; tt = tt->next) {
	if (tt->file == afile)
	    return tt;
    }
    return (struct ubik_trunc *)0;
}

/* do truncates associated with trans, and free them */
static int
DoTruncs(struct ubik_trans *atrans)
{
    struct ubik_trunc *tt, *nt;
    int (*tproc) ();
    afs_int32 rcode = 0, code;

    tproc = atrans->dbase->truncate;
    for (tt = atrans->activeTruncs; tt; tt = nt) {
	nt = tt->next;
	DTrunc(atrans->dbase, tt->file, tt->length);	/* zap pages from buffer cache */
	code = (*tproc) (atrans->dbase, tt->file, tt->length);
	if (code)
	    rcode = code;
	PutTrunc(tt);
    }
    /* don't unthread, because we do the entire list's worth here */
    atrans->activeTruncs = (struct ubik_trunc *)0;
    return (rcode);
}

/* mark a fid as invalid */
int
udisk_Invalidate(struct ubik_dbase *adbase, afs_int32 afid)
{
    struct buffer *tb;
    int i;

    for (i = 0, tb = Buffers; i < nbuffers; i++, tb++) {
	if (tb->file == afid) {
	    tb->file = BADFID;
	    Dlru(tb);
	}
    }
    return 0;
}

/* move this page into the correct hash bucket */
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
    i = pHash(ap->page);
    ap->hashIndex = i;		/* remember where we are for deletion */
    ap->hashNext = phTable[i];	/* add us to the list */
    phTable[i] = ap;
    return 0;
}

/* create a new slot for a particular dbase page */
static struct buffer *
newslot(struct ubik_dbase *adbase, afs_int32 afid, afs_int32 apage)
{
    /* Find a usable buffer slot */
    afs_int32 i;
    struct buffer *pp, *tp;

    pp = 0;			/* last pure */
    for (i = 0, tp = LruBuffer; i < nbuffers; i++, tp = tp->lru_next) {
	if (!tp->lockers && !tp->dirty) {
	    pp = tp;
	    break;
	}
    }

    if (pp == 0) {
	/* There are no unlocked buffers that don't need to be written to the disk. */
	ubik_print
	    ("Ubik: Internal Error: Unable to find free buffer in ubik cache\n");
	return NULL;
    }

    /* Now fill in the header. */
    pp->dbase = adbase;
    pp->file = afid;
    pp->page = apage;

    FixupBucket(pp);		/* move to the right hash bucket */
    Dmru(pp);
    return pp;
}

/* Release a buffer, specifying whether or not the buffer has been modified by the locker. */
static void
DRelease(char *ap, int flag)
{
    int index;
    struct buffer *bp;

    if (!ap)
	return;
    index = (int)(ap - (char *)BufferData) >> UBIK_LOGPAGESIZE;
    bp = &(Buffers[index]);
    bp->lockers--;
    if (flag)
	bp->dirty = 1;
    return;
}

/* flush all modified buffers, leaves dirty bits set (they're cleared
 * by DSync).  Note interaction with DSync: you call this thing first,
 * writing the buffers to the disk.  Then you call DSync to sync all the
 * files that were written, and to clear the dirty bits.  You should
 * always call DFlush/DSync as a pair.
 */
static int
DFlush(struct ubik_dbase *adbase)
{
    int i;
    afs_int32 code;
    struct buffer *tb;

    tb = Buffers;
    for (i = 0; i < nbuffers; i++, tb++) {
	if (tb->dirty) {
	    code = tb->page * UBIK_PAGESIZE;	/* offset within file */
	    code =
		(*adbase->write) (adbase, tb->file, tb->data, code,
				  UBIK_PAGESIZE);
	    if (code != UBIK_PAGESIZE)
		return UIOERROR;
	}
    }
    return 0;
}

/* flush all modified buffers */
static int
DAbort(struct ubik_dbase *adbase)
{
    int i;
    struct buffer *tb;

    tb = Buffers;
    for (i = 0; i < nbuffers; i++, tb++) {
	if (tb->dirty) {
	    tb->dirty = 0;
	    tb->file = BADFID;
	    Dlru(tb);
	}
    }
    return 0;
}

/* must only be called after DFlush, due to its interpretation of dirty flag */
static int
DSync(struct ubik_dbase *adbase)
{
    int i;
    afs_int32 code;
    struct buffer *tb;
    afs_int32 file;
    afs_int32 rCode;

    rCode = 0;
    while (1) {
	file = BADFID;
	for (i = 0, tb = Buffers; i < nbuffers; i++, tb++) {
	    if (tb->dirty == 1) {
		if (file == BADFID)
		    file = tb->file;
		if (file != BADFID && tb->file == file)
		    tb->dirty = 0;
	    }
	}
	if (file == BADFID)
	    break;
	/* otherwise we have a file to sync */
	code = (*adbase->sync) (adbase, file);
	if (code)
	    rCode = code;
    }
    return rCode;
}

/* Same as read, only do not even try to read the page */
static char *
DNew(struct ubik_dbase *dbase, afs_int32 fid, int page)
{
    struct buffer *tb;

    if ((tb = newslot(dbase, fid, page)) == 0)
	return NULL;
    tb->lockers++;
    memset(tb->data, 0, UBIK_PAGESIZE);
    return tb->data;
}

/* read data from database */
int
udisk_read(struct ubik_trans *atrans, afs_int32 afile, char *abuffer,
	   afs_int32 apos, afs_int32 alen)
{
    char *bp;
    afs_int32 offset, len, totalLen;
    struct ubik_dbase *dbase;

    if (atrans->flags & TRDONE)
	return UDONE;
    totalLen = 0;
    dbase = atrans->dbase;
    while (alen > 0) {
	bp = DRead(dbase, afile, apos >> UBIK_LOGPAGESIZE);
	if (!bp)
	    return UEOF;
	/* otherwise, min of remaining bytes and end of buffer to user mode */
	offset = apos & (UBIK_PAGESIZE - 1);
	len = UBIK_PAGESIZE - offset;
	if (len > alen)
	    len = alen;
	memcpy(abuffer, bp + offset, len);
	abuffer += len;
	apos += len;
	alen -= len;
	totalLen += len;
	DRelease(bp, 0);
    }
    return 0;
}

/* truncate file */
int
udisk_truncate(struct ubik_trans *atrans, afs_int32 afile, afs_int32 alength)
{
    afs_int32 code;
    struct ubik_trunc *tt;

    if (atrans->flags & TRDONE)
	return UDONE;
    if (atrans->type != UBIK_WRITETRANS)
	return UBADTYPE;

    /* write a truncate log record */
    code = udisk_LogTruncate(atrans->dbase, afile, alength);

    /* don't truncate until commit time */
    tt = FindTrunc(atrans, afile);
    if (!tt) {
	/* this file not truncated yet */
	tt = GetTrunc();
	tt->next = atrans->activeTruncs;
	atrans->activeTruncs = tt;
	tt->file = afile;
	tt->length = alength;
    } else {
	/* already truncated to a certain length */
	if (tt->length > alength)
	    tt->length = alength;
    }
    return code;
}

/* write data to database, using logs */
int
udisk_write(struct ubik_trans *atrans, afs_int32 afile, char *abuffer,
	    afs_int32 apos, afs_int32 alen)
{
    char *bp;
    afs_int32 offset, len, totalLen;
    struct ubik_dbase *dbase;
    struct ubik_trunc *tt;
    afs_int32 code;

    if (atrans->flags & TRDONE)
	return UDONE;
    if (atrans->type != UBIK_WRITETRANS)
	return UBADTYPE;

    dbase = atrans->dbase;
    /* first write the data to the log */
    code = udisk_LogWriteData(dbase, afile, abuffer, apos, alen);
    if (code)
	return code;

    /* expand any truncations of this file */
    tt = FindTrunc(atrans, afile);
    if (tt) {
	if (tt->length < apos + alen) {
	    tt->length = apos + alen;
	}
    }

    /* now update vm */
    totalLen = 0;
    while (alen > 0) {
	bp = DRead(dbase, afile, apos >> UBIK_LOGPAGESIZE);
	if (!bp) {
	    bp = DNew(dbase, afile, apos >> UBIK_LOGPAGESIZE);
	    if (!bp)
		return UIOERROR;
	    memset(bp, 0, UBIK_PAGESIZE);
	}
	/* otherwise, min of remaining bytes and end of buffer to user mode */
	offset = apos & (UBIK_PAGESIZE - 1);
	len = UBIK_PAGESIZE - offset;
	if (len > alen)
	    len = alen;
	memcpy(bp + offset, abuffer, len);
	abuffer += len;
	apos += len;
	alen -= len;
	totalLen += len;
	DRelease(bp, 1);	/* buffer modified */
    }
    return 0;
}

/* begin a new local transaction */
int
udisk_begin(struct ubik_dbase *adbase, int atype, struct ubik_trans **atrans)
{
    afs_int32 code;
    struct ubik_trans *tt;

    *atrans = (struct ubik_trans *)NULL;
    /* Make sure system is initialized before doing anything */
    if (!initd) {
	initd = 1;
	DInit(ubik_nBuffers);
    }
    if (atype == UBIK_WRITETRANS) {
	if (adbase->flags & DBWRITING)
	    return USYNC;
	code = udisk_LogOpcode(adbase, LOGNEW, 0);
	if (code)
	    return code;
    }
    tt = (struct ubik_trans *)malloc(sizeof(struct ubik_trans));
    memset(tt, 0, sizeof(struct ubik_trans));
    tt->dbase = adbase;
    tt->next = adbase->activeTrans;
    adbase->activeTrans = tt;
    tt->type = atype;
    if (atype == UBIK_READTRANS)
	adbase->readers++;
    else if (atype == UBIK_WRITETRANS)
	adbase->flags |= DBWRITING;
    *atrans = tt;
    return 0;
}

/* commit transaction */
int
udisk_commit(struct ubik_trans *atrans)
{
    struct ubik_dbase *dbase;
    afs_int32 code = 0;
    struct ubik_version oldversion, newversion;

    if (atrans->flags & TRDONE)
	return (UTWOENDS);

    if (atrans->type == UBIK_WRITETRANS) {
	dbase = atrans->dbase;

	/* On the first write to the database. We update the versions */
	if (ubeacon_AmSyncSite() && !(urecovery_state & UBIK_RECLABELDB)) {
	    oldversion = dbase->version;
	    newversion.epoch = FT_ApproxTime();;
	    newversion.counter = 1;

	    code = (*dbase->setlabel) (dbase, 0, &newversion);
	    if (code)
		return (code);
	    ubik_epochTime = newversion.epoch;
	    dbase->version = newversion;

	    /* Ignore the error here. If the call fails, the site is
	     * marked down and when we detect it is up again, we will 
	     * send the entire database to it.
	     */
	    ContactQuorum(DISK_SetVersion, atrans, 1 /*CStampVersion */ ,
			  &oldversion, &newversion);
	    urecovery_state |= UBIK_RECLABELDB;
	}

	dbase->version.counter++;	/* bump commit count */
	LWP_NoYieldSignal(&dbase->version);

	code = udisk_LogEnd(dbase, &dbase->version);
	if (code) {
	    dbase->version.counter--;
	    return (code);
	}

	/* If we fail anytime after this, then panic and let the
	 * recovery replay the log. 
	 */
	code = DFlush(dbase);	/* write dirty pages to respective files */
	if (code)
	    panic("Writing Ubik DB modifications\n");
	code = DSync(dbase);	/* sync the files and mark pages not dirty */
	if (code)
	    panic("Synchronizing Ubik DB modifications\n");

	code = DoTruncs(atrans);	/* Perform requested truncations */
	if (code)
	    panic("Truncating Ubik DB\n");

	/* label the committed dbase */
	code = (*dbase->setlabel) (dbase, 0, &dbase->version);
	if (code)
	    panic("Truncating Ubik DB\n");

	code = (*dbase->truncate) (dbase, LOGFILE, 0);	/* discard log (optional) */
	if (code)
	    panic("Truncating Ubik logfile\n");

    }

    /* When the transaction is marked done, it also means the logfile
     * has been truncated.
     */
    atrans->flags |= TRDONE;
    return code;
}

/* abort transaction */
int
udisk_abort(struct ubik_trans *atrans)
{
    struct ubik_dbase *dbase;
    afs_int32 code;

    if (atrans->flags & TRDONE)
	return UTWOENDS;

    /* Check if we are the write trans before logging abort, lest we
     * abort a good write trans in progress. 
     * We don't really care if the LOGABORT gets to the log because we 
     * truncate the log next. If the truncate fails, we panic; for 
     * otherwise, the log entries remain. On restart, replay of the log
     * will do nothing because the abort is there or no LogEnd opcode.
     */
    dbase = atrans->dbase;
    if (atrans->type == UBIK_WRITETRANS && dbase->flags & DBWRITING) {
	udisk_LogOpcode(dbase, LOGABORT, 1);
	code = (*dbase->truncate) (dbase, LOGFILE, 0);
	if (code)
	    panic("Truncating Ubik logfile during an abort\n");
	DAbort(dbase);		/* remove all dirty pages */
    }

    /* When the transaction is marked done, it also means the logfile
     * has been truncated.
     */
    atrans->flags |= (TRABORT | TRDONE);
    return 0;
}

/* destroy a transaction after it has been committed or aborted.  if
 * it hasn't committed before you call this routine, we'll abort the
 * transaction for you.
 */
int
udisk_end(struct ubik_trans *atrans)
{
    struct ubik_dbase *dbase;

#if defined(UBIK_PAUSE)
    /* Another thread is trying to lock this transaction.
     * That can only be an RPC doing SDISK_Lock.
     * Unlock the transaction, 'cause otherwise the other
     * thread will never wake up.  Don't free it because
     * the caller will do that already.
     */
    if (atrans->flags & TRSETLOCK) {
	atrans->flags |= TRSTALE;
	ulock_relLock(atrans);
	return;
    }
#endif /* UBIK_PAUSE */
    if (!(atrans->flags & TRDONE))
	udisk_abort(atrans);
    dbase = atrans->dbase;

    ulock_relLock(atrans);
    unthread(atrans);

    /* check if we are the write trans before unsetting the DBWRITING bit, else
     * we could be unsetting someone else's bit.
     */
    if (atrans->type == UBIK_WRITETRANS && dbase->flags & DBWRITING) {
	dbase->flags &= ~DBWRITING;
    } else {
	dbase->readers--;
    }
    if (atrans->iovec_info.iovec_wrt_val)
	free(atrans->iovec_info.iovec_wrt_val);
    if (atrans->iovec_data.iovec_buf_val)
	free(atrans->iovec_data.iovec_buf_val);
    free(atrans);

    /* Wakeup any writers waiting in BeginTrans() */
    LWP_NoYieldSignal(&dbase->flags);
    return 0;
}
