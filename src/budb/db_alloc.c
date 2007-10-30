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

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <afs/stds.h>
#include <ubik.h>
#include <afs/bubasics.h>
#include "budb_errs.h"
#include "database.h"


/* block and structure allocation routines */

static int nEntries[NBLOCKTYPES];
static int sizeEntries[NBLOCKTYPES];

afs_int32
InitDBalloc()
{
    nEntries[0] = 0;
    sizeEntries[0] = 0;

    nEntries[volFragment_BLOCK] = NvolFragmentS;
    nEntries[volInfo_BLOCK] = NvolInfoS;
    nEntries[tape_BLOCK] = NtapeS;
    nEntries[dump_BLOCK] = NdumpS;

    sizeEntries[volFragment_BLOCK] = sizeof(((struct vfBlock *) NULL)->a[0]);
    sizeEntries[volInfo_BLOCK] = sizeof(((struct viBlock *) NULL)->a[0]);
    sizeEntries[tape_BLOCK] = sizeof(((struct tBlock *) NULL)->a[0]);
    sizeEntries[dump_BLOCK] = sizeof(((struct dBlock *) NULL)->a[0]);

    return 0;
}

/* AllocBlock
 *	allocate a (basic) database block. Extends the database if
 *	no free blocks are available.
 * exit:
 *	0 - aP points to a cleared block
 *	n - error
 */

afs_int32
AllocBlock(ut, block, aP)
     struct ubik_trans *ut;
     struct block *block;	/* copy of data */
     dbadr *aP;			/* db addr of block */
{
    dbadr a;

    if (db.h.freePtrs[0] == 0) {
	/* if there are no free blocks, extend the database */
	LogDebug(2, "AllocBlock: extending db\n");

	a = ntohl(db.h.eofPtr);
	if (set_header_word(ut, eofPtr, htonl(a + BLOCKSIZE)))
	    return BUDB_IO;
    } else {
	a = ntohl(db.h.freePtrs[0]);
	if (dbread(ut, a, (char *)block, sizeof(block->h))	/* read hdr */
	    ||set_header_word(ut, freePtrs[0], block->h.next)	/* set next */
	    )
	    return BUDB_IO;
    }

    /* clear and return the block */
    memset(block, 0, sizeof(*block));
    *aP = a;
    return 0;
}

/* FreeBlock
 *	Free a basic block
 * entry:
 *	bh - block header ptr. Memory copy of the block header.
 *	a - disk address of the block
 */

afs_int32
FreeBlock(ut, bh, a)
     struct ubik_trans *ut;
     struct blockHeader *bh;	/* copy of data */
     dbadr a;			/* db address of block */
{
    if (a != BlockBase(a))
	db_panic("Block addr no good");
    memset(bh, 0, sizeof(*bh));
    bh->next = db.h.freePtrs[0];
    if (set_header_word(ut, freePtrs[0], htonl(a))
	|| dbwrite(ut, a, (char *)bh, sizeof(*bh)))
	return BUDB_IO;
    return 0;
}


/* AllocStructure
 * entry:
 *	type - type of structure to allocate
 *	related - address of related block
 *	saP - db addr of structure
 *	s -  structure data
 */

afs_int32
AllocStructure(ut, type, related, saP, s)
     struct ubik_trans *ut;
     char type;
     dbadr related;
     dbadr *saP;
     char *s;
{
    dbadr a;			/* block addr */
    struct block b;		/* copy of data */
    int i;			/* block structure array index */
    afs_int32 *bs;		/* ptr to first word of structure */
    int nFree;

    if ((type == 0)
	|| (type > MAX_STRUCTURE_BLOCK_TYPE)
	) {
	db_panic("bad structure type");
    }
    bs = (afs_int32 *) b.a;	/* ptr to first structure of block */

    if (db.h.freePtrs[type] == 0) {
	/* no free items of specified type */

	if (AllocBlock(ut, &b, &a)
	    || set_header_word(ut, freePtrs[type], htonl(a))
	    ) {
	    return BUDB_IO;
	}

	b.h.next = 0;
	b.h.type = type;
	b.h.flags = 0;
	b.h.nFree = ntohs(nEntries[type] - 1);
	*bs = 1;		/* not free anymore */

	if (dbwrite(ut, a, (char *)&b, sizeof(b)))
	    return BUDB_IO;
	LogDebug(2, "AllocStructure: allocated new block\n");
    } else {
	int count = 10;

	/* Only do 10 (or so) at a time, to avoid transactions which modify
	 * many buffer.
	 */

	while (1) {
	    a = ntohl(db.h.freePtrs[type]);
	    if (dbread(ut, a, (char *)&b, sizeof(b)))
		return BUDB_IO;

	    nFree = ntohs(b.h.nFree);
	    if (nFree == 0)
		db_panic("nFree is zero");

	    /* Completely empty blocks go to generic free list if there are
	     * more blocks on this free list 
	     */
	    if (b.h.next && (nFree == nEntries[type]) && (count-- > 0)) {
		if (set_header_word(ut, freePtrs[type], b.h.next)
		    || FreeBlock(ut, &b.h, a)
		    ) {
		    return BUDB_IO;
		}
		LogDebug(2, "AllocStrucure: add to free block list\n");
	    } else {
		/* we found a free structure */
		if (nFree == 1) {
		    /* if last free one: unthread block */
		    if (set_header_word(ut, freePtrs[type], b.h.next))
			return BUDB_IO;
		}
		break;
	    }
	}

	/* find the free structure - arbitrarily uses first word as
	 * allocated/free status. PA.
	 */
	i = 0;
	while (*bs) {
	    i++;
	    bs = (afs_int32 *) ((char *)bs + sizeEntries[type]);
	}

	if (i >= nEntries[type])
	    db_panic("free count inconsistent with block");

	b.h.nFree = htons(nFree - 1);
	if (dbwrite(ut, a, (char *)&b, sizeof(b.h)))
	    return BUDB_IO;
    }
    *(afs_int32 *) s = 1;	/* make sure structure is not free */
    *saP = a + ((char *)bs - (char *)&b);

    LogDebug(3, "allocated at %d, block at %d, offset %d\n", *saP, a,
	     ((char *)bs - (char *)&b));
    /* caller must write back at least first word of structure */
    return 0;
}



afs_int32
FreeStructure(ut, type, sa)
     struct ubik_trans *ut;
     char type;			/* type of structure to allocate */
     dbadr sa;			/* db addr of structure */
{
    struct blockHeader bh;	/* header of containing block */
    dbadr a;			/* db address of block */
    int nFree;			/* new free structures count */
    afs_int32 freeWord;

    if ((type == 0) || (type > MAX_STRUCTURE_BLOCK_TYPE))
	db_panic("bad structure type");

    a = BlockBase(sa);
    if (dbread(ut, a, (char *)&bh, sizeof(bh)))
	return BUDB_IO;
    if (type != bh.type)
	db_panic("block and structure of different types");

    bh.nFree = htons(nFree = ntohs(bh.nFree) + 1);
    if (nFree > nEntries[type])
	db_panic("free count too large");
    if (nFree == 1) {		/* add to free list for type */
	bh.next = db.h.freePtrs[type];
	if (set_header_word(ut, freePtrs[type], htonl(a)))
	    return BUDB_IO;
    }

    /* mark the structure as free, and write out block header */
    if (set_word_offset(ut, sa, &freeWord, 0, 0)
	|| dbwrite(ut, a, (char *)&bh, sizeof(bh)))
	return BUDB_IO;
    return 0;
}
