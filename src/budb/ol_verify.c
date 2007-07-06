/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* ol_verify - online database verification */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <netdb.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <afs/stds.h>
#include <sys/types.h>
#include <lock.h>
#include <ubik.h>
#include "database.h"
#include "error_macros.h"
#include "budb_errs.h"
#include <afs/cellconfig.h>
#include "afs/audit.h"

#undef min
#undef max

/* notes
 * 1) volInfo structures refering to a volume of the same name are
 *	chained together, i.e. the volumes described differ in volid, partition
 *	etc. The structure at the head of this list (the sameNameChain) is 
 *	treated specially. When a delete volInfo request is processed, heads 
 *	are not deleted unless all other items on the sameNameChain are gone.
 *
 *	The result is that volInfo (head) structures may be present
 *	even if no tape structures refer to them. These structures are
 *	unreachable in a top-down tree walk.
 * TO DO
 * 1)	make the verify tolerant of errors. Want to get a summary statistic
 *	indicating how may dumps are lost and how many text blocks lost
 * 2)	Make the recreation instructions write out whatever is good. This
 *	is only for the off-line case.
 */

/* flags associated with each structure. These are set and checked in 
 * the blockMap entries
 */

#define MAP_DUMPHASH            1	/* dump name hash checked */
#define MAP_TAPEHASH            2	/* tape name hash checked */
#define MAP_VOLHASH             4	/* volume name hash checked */
#define MAP_IDHASH              8	/* dump id hash checked */

#define MAP_HASHES (MAP_DUMPHASH | MAP_TAPEHASH | MAP_VOLHASH | MAP_IDHASH)

#define MAP_FREE                0x10	/* item is free */
#define MAP_RECREATE            0x20
#define MAP_HTBLOCK             0x40	/* hash table block */
#define MAP_TAPEONDUMP          0x100
#define MAP_VOLFRAGONTAPE       0x200
#define MAP_VOLFRAGONVOL        0x400
#define MAP_VOLINFOONNAME       0x800
#define MAP_VOLINFONAMEHEAD     0x1000
#define MAP_TEXTBLOCK		0x2000	/* block of text */
#define MAP_APPENDEDDUMP	0x4000

/* one blockMap for every block in the database. Each element of the entries
 *	array describes the status of a data structure/entry in that block
 */

struct blockMap {
    struct blockHeader header;	/* copy of the block header */
    char free;			/* on free list */
    int nEntries;		/* size of the entries arrays */
    afs_uint32 entries[1];	/* describes each entry */
};

/* status for verify call */
struct dbStatus {
    char hostname[64];		/* host on which checked */
    afs_int32 status;		/* ok, not ok */
};

int nBlocks;			/* total number of blocks in db */

struct misc_hash_stats {	/* stats for hashing */
    int max;			/* longest chain length */
    double avg;			/* avg length */
    double std_dev;		/* standard deviation of length */
};

struct misc_data {
    int errors;			/* errors encountered */
    int maxErrors;		/* abort after this many errors */
    int nBlocks;		/* number of database blocks */
    int nDump, nTape, nVolInfo, nVolFrag;	/* counts of each type */
    int nVolName;		/* volInfos w/ head==0 */
    int maxVolsPerVolInfo;	/* maximum list lengths */
    int maxVolsPerTape;
    int maxVolsPerDump;
    int maxVolInfosPerName;
    int maxTapesPerDump;
    int maxAppendsPerDump;
    int freeLength[NBLOCKTYPES];	/* length of free lists */
    int fullyFree[NBLOCKTYPES];	/* free blocks full of free entries */
    int veryLongChain;		/* length of chain to report */
    int checkFragCount;		/* report fragment count errors */
    struct misc_hash_stats dumpName, dumpIden, tapeName, volName;
    FILE *recreate;		/* stream for recreate instructions */
} miscData;
struct misc_data *misc;

struct blockMap **blockMap = 0;	/* initial block map */

/* describes number of entries for each type of block */

int blockEntries[NBLOCKTYPES] = {
    0 /* free_BLOCK */ ,
    NvolFragmentS,
    NvolInfoS,
    NtapeS,
    NdumpS,
    1,				/* hashTable_BLOCK */
    1				/* text block */
};

int blockEntrySize[NBLOCKTYPES] = {
    0 /* free */ ,
    sizeof(((struct vfBlock *) NULL)->a[0]),
    sizeof(((struct viBlock *) NULL)->a[0]),
    sizeof(((struct tBlock *) NULL)->a[0]),
    sizeof(((struct dBlock *) NULL)->a[0]),
    0,
    0,
};

char *typeName[NBLOCKTYPES] = {
    "free",
    "volFragment",
    "volInfo",
    "tape",
    "dump",
    "hashTable",
    "text"
};

int hashBlockType[HT_MAX_FUNCTION + 1] = {
    0,
    dump_BLOCK,
    dump_BLOCK,
    tape_BLOCK,
    volInfo_BLOCK
};

/* Compatibility table for the bits in the blockMap. */

struct mapCompatability {
    short trigger;		/* these bits trigger this element */
} mapC[] = {
MAP_FREE, MAP_HTBLOCK, MAP_DUMPHASH | MAP_IDHASH,
	MAP_TAPEHASH | MAP_TAPEONDUMP, MAP_VOLINFOONNAME,
	MAP_VOLINFONAMEHEAD | MAP_VOLHASH,
	MAP_VOLFRAGONTAPE | MAP_VOLFRAGONVOL, MAP_TEXTBLOCK};

/* no. of entries in the mapC array */
int NMAPCs = (sizeof(mapC) / sizeof(mapC[0]));

/* for identifying stored textual information */

char *textName[TB_NUM] = {
    "Dump Schedule\n",
    "Volume Sets\n",
    "Tape Hosts\n"
};

extern int sizeFunctions[];
extern int nHTBuckets;

#define DBBAD BUDB_DATABASEINCONSISTENT

/* ------------------------------------
 * supporting routines
 * ------------------------------------
 */

/* BumpErrors
 *	increment the error count
 * exit:
 *	0 - continue
 *	1 - maximum errors exceeded
 */

afs_int32
BumpErrors()
{
    if (++miscData.errors >= miscData.maxErrors)
	return (1);
    else
	return (0);
}

/* convertDiskAddress
 *	given a disk address, break it down into a block and entry index. These
 *	permit access to the block map information. The conversion process
 *	compares the supplied address with the alignment/type information
 *	stored in the block map.
 * exit:
 *	0 - ok
 *	BUDB_ADDR - address alignment checks failed
 */

afs_int32
checkDiskAddress(address, type, blockIndexPtr, entryIndexPtr)
     unsigned long address;
     int type;
     int *blockIndexPtr;
     int *entryIndexPtr;
{
    int index, offset;

    if (blockIndexPtr)
	*blockIndexPtr = -1;
    if (entryIndexPtr)
	*entryIndexPtr = -1;

    /* This is safest way to handle totally bogus addresses (eg 0x80001234). */
    if ((address < sizeof(db.h)) || (address >= ntohl(db.h.eofPtr)))
	return BUDB_ADDR;

    address -= sizeof(db.h);
    index = address / BLOCKSIZE;
    offset = address - (index * BLOCKSIZE);
    if (offset % sizeof(afs_int32))	/* alignment check */
	return BUDB_ADDR;
    if (offset && (type > 0) && (type <= MAX_STRUCTURE_BLOCK_TYPE)) {
	offset -= sizeof(struct blockHeader);
	if ((offset < 0) || (offset % blockEntrySize[type]))
	    return BUDB_ADDR;
	offset /= blockEntrySize[type];
	if (offset >= blockEntries[type])
	    return BUDB_ADDR;
    }
    if (blockIndexPtr)
	*blockIndexPtr = index;
    if (entryIndexPtr)
	*entryIndexPtr = offset;
    return 0;
}

/* ConvertDiskAddress
 *	given a disk address, break it down into a block and entry index. These
 *	permit access to the block map information. The conversion process
 *	compares the supplied address with the alignment/type information
 *	stored in the block map.
 * exit:
 *	0 - ok
 *	BUDB_ADDR - address alignment checks failed
 */

afs_int32
ConvertDiskAddress(address, blockIndexPtr, entryIndexPtr)
     afs_uint32 address;
     int *blockIndexPtr;
     int *entryIndexPtr;
{
    int index, type;
    afs_int32 code;

    index = (address - sizeof(db.h)) / BLOCKSIZE;
    type = blockMap[index]->header.type;

    code = checkDiskAddress(address, type, blockIndexPtr, entryIndexPtr);
    return (code);
}

char *
TypeName(index)
     int index;
{
    static char error[36];

    if ((index < 0) || (index >= NBLOCKTYPES)) {
	sprintf(error, "UNKNOWN_TYPE %d", index);
	return (error);
    }
    return (typeName[index]);
}

int
getDumpID(struct ubik_trans *ut,
    struct tape *tapePtr,
    afs_int32 *dumpID)
{
    struct dump d;
    afs_int32 code;

    *dumpID = 0;
    code = dbread(ut, ntohl(tapePtr->dump), &d, sizeof(d));
    if (!code)
	*dumpID = ntohl(d.id);
    return code;
}

/* ------------------------------------
 * verification routines - structure specific
 * ------------------------------------
 */

/* verifyDumpEntry
 *      Follow the tapes entries hanging off of a dump and verify they belong 
 *      to the dump.
 */
afs_int32
verifyDumpEntry(ut, dumpAddr, ai, ao, dumpPtr)
     struct ubik_trans *ut;
     afs_int32 dumpAddr;
     int ai, ao;
     struct dump *dumpPtr;
{
    struct tape tape;
    afs_int32 tapeAddr, tapeCount = 0, volCount = 0, appDumpCount = 0;
    afs_int32 appDumpAddr, appDumpIndex, appDumpOffset;
    struct dump appDump;
    int tapeIndex, tapeOffset, ccheck = 1;
    afs_int32 code = 0, tcode;
    int dumpIndex, dumpOffset;

    tcode = ConvertDiskAddress(dumpAddr, &dumpIndex, &dumpOffset);
    if (tcode) {
	Log("verifyDumpEntry: Invalid dump entry addr 0x%x\n", dumpAddr);
	if (BumpErrors())
	    ERROR(DBBAD);
	ERROR(0);
    }

    /* Step though list of tapes hanging off of this dump */
    for (tapeAddr = ntohl(dumpPtr->firstTape); tapeAddr;
	 tapeAddr = ntohl(tape.nextTape)) {
	tcode = ConvertDiskAddress(tapeAddr, &tapeIndex, &tapeOffset);
	if (tcode) {
	    Log("verifyDumpEntry: Invalid tape entry addr 0x%x (on DumpID %u)\n", tapeAddr, ntohl(dumpPtr->id));
	    Log("     Skipping remainder of tapes in dump\n");
	    if (BumpErrors())
		ERROR(DBBAD);
	    ccheck = 0;
	    break;
	}

	tcode = dbread(ut, tapeAddr, &tape, sizeof(tape));
	if (tcode)
	    ERROR(BUDB_IO);

	if (ntohl(tape.dump) != dumpAddr) {
	    afs_int32 did;

	    getDumpID(ut, &tape, &did);
	    Log("verifyDumpEntry: Tape '%s' (addr 0x%x) doesn't point to\n",
		tape.name, tapeAddr);
	    Log("     dumpID %u (addr 0x%x). Points to DumpID %u (addr 0x%x)\n", ntohl(dumpPtr->id), dumpAddr, did, ntohl(tape.dump));
	    if (BumpErrors())
		return (DBBAD);
	}

	/* Check if this tape entry has been examine already */
	if (blockMap[tapeIndex]->entries[tapeOffset] & MAP_TAPEONDUMP) {
	    Log("verifyDumpEntry: Tape '%s' (addr 0x%x) on multiple dumps\n",
		tape.name, tapeAddr);
	    if (BumpErrors())
		return (DBBAD);
	}
	blockMap[tapeIndex]->entries[tapeOffset] |= MAP_TAPEONDUMP;

	tapeCount++;
	volCount += ntohl(tape.nVolumes);
    }

    if (ccheck && (ntohl(dumpPtr->nVolumes) != volCount)) {
	Log("verifyDumpEntry: DumpID %u (addr 0x%x) volume count of %d is wrong (should be %d)\n", ntohl(dumpPtr->id), dumpAddr, ntohl(dumpPtr->nVolumes), volCount);
	if (BumpErrors())
	    return (DBBAD);
    }

    if (volCount > misc->maxVolsPerDump)
	misc->maxVolsPerDump = volCount;
    if (tapeCount > misc->maxTapesPerDump)
	misc->maxTapesPerDump = tapeCount;

    /* If this is an initial dump, then step though list of appended dumps
     * hanging off of this dump.
     */
    if (ntohl(dumpPtr->initialDumpID) == 0) {
	for (appDumpAddr = ntohl(dumpPtr->appendedDumpChain); appDumpAddr;
	     appDumpAddr = ntohl(appDump.appendedDumpChain)) {

	    tcode =
		ConvertDiskAddress(appDumpAddr, &appDumpIndex,
				   &appDumpOffset);
	    if (tcode) {
		Log("verifyDumpEntry: Invalid appended dump entry addr 0x%x\n", appDumpAddr);
		Log("Skipping remainder of appended dumps\n");
		if (BumpErrors())
		    ERROR(DBBAD);
		break;
	    }

	    /* Read the appended dump in */
	    tcode = dbread(ut, appDumpAddr, &appDump, sizeof(appDump));
	    if (tcode)
		ERROR(BUDB_IO);

	    /* Verify that it points to the parent dump */
	    if (ntohl(appDump.initialDumpID) != ntohl(dumpPtr->id)) {
		Log("verifyDumpEntry: DumpID %u (addr 0x%x) initial DumpID incorrect (is %u, should be %u)\n", ntohl(appDump.id), appDumpAddr, ntohl(appDump.initialDumpID), ntohl(dumpPtr->id));
		if (BumpErrors())
		    return (DBBAD);
	    }

	    /* Check if this appended dump entry has been examined already */
	    if (blockMap[appDumpIndex]->
		entries[appDumpOffset] & MAP_APPENDEDDUMP) {
		Log("verifyDumpEntry: DumpID %u (addr %u) is on multiple appended dump chains\n", ntohl(appDump.id), appDumpAddr);
		Log("Skipping remainder of appended dumps\n");
		if (BumpErrors())
		    return (DBBAD);
		break;
	    }
	    blockMap[appDumpIndex]->entries[appDumpOffset] |=
		MAP_APPENDEDDUMP;

	    appDumpCount++;
	}
    }

    if (appDumpCount > misc->maxAppendsPerDump)
	misc->maxAppendsPerDump = appDumpCount;
    misc->nDump++;

  error_exit:
    return (code);
}

/*
 * verifyTapeEntry
 *      Follw the volume fragments hanging off of a tape entry and verify 
 *      they belong to the tape.
 */
afs_int32
verifyTapeEntry(ut, tapeAddr, ai, ao, tapePtr)
     struct ubik_trans *ut;
     afs_int32 tapeAddr;
     int ai, ao;
     struct tape *tapePtr;
{
    int volCount = 0, ccheck = 1;
    afs_int32 volFragAddr;
    int blockIndex, entryIndex;
    struct volFragment volFragment;
    afs_int32 code = 0, tcode;

    for (volFragAddr = ntohl(tapePtr->firstVol); volFragAddr;
	 volFragAddr = ntohl(volFragment.sameTapeChain)) {
	tcode = ConvertDiskAddress(volFragAddr, &blockIndex, &entryIndex);
	if (tcode) {
	    afs_int32 did;

	    getDumpID(ut, tapePtr, &did);
	    Log("verifyTapeEntry: Invalid volFrag addr 0x%x (on tape '%s' DumpID %u)\n", volFragAddr, tapePtr->name, did);
	    Log("     Skipping remainder of volumes on tape\n");
	    if (BumpErrors())
		ERROR(DBBAD);
	    ccheck = 0;
	    break;
	}

	tcode = dbread(ut, volFragAddr, &volFragment, sizeof(volFragment));
	if (tcode)
	    ERROR(tcode);

	if (ntohl(volFragment.tape) != tapeAddr) {
	    afs_int32 did;

	    getDumpID(ut, tapePtr, &did);
	    Log("verifyTapeEntry: VolFrag (addr 0x%x) doesn't point to \n",
		volFragAddr);
	    Log("     tape '%s' DumpID %u (addr 0x%x). Points to addr 0x%x\n",
		tapePtr->name, did, tapeAddr, ntohl(volFragment.tape));
	    if (BumpErrors())
		ERROR(DBBAD);
	}

	/* Has this volume fragment already been examined */
	if (blockMap[blockIndex]->entries[entryIndex] & MAP_VOLFRAGONTAPE) {
	    Log("verifyTapeEntry: VolFrag (addr %d) on multiple tapes\n",
		volFragAddr);
	    if (BumpErrors())
		ERROR(DBBAD);
	}
	blockMap[blockIndex]->entries[entryIndex] |= MAP_VOLFRAGONTAPE;

	volCount++;
    }

    /* now check computed vs. recorded volume counts */
    if (ccheck && (ntohl(tapePtr->nVolumes) != volCount)) {
	afs_int32 did;

	getDumpID(ut, tapePtr, &did);
	Log("verifyTapeEntry: Tape '%s' DumpID %u (addr 0x%x) volFrag count of %d is wrong (should be %d)\n", tapePtr->name, did, tapeAddr, ntohl(tapePtr->nVolumes), volCount);
	if (BumpErrors())
	    ERROR(DBBAD);
    }

    if (volCount > misc->maxVolsPerTape)
	misc->maxVolsPerTape = volCount;
    misc->nTape++;

  error_exit:
    return (code);
}

/*
 * verifyVolFragEntry
 *      volume fragments are the lowest leaf describing a dump (nothing hangs off of it).
 *      So no check is done agaist it.
 */
afs_int32
verifyVolFragEntry(ut, va, ai, ao, v)
     struct ubik_trans *ut;
     afs_int32 va;
     int ai, ao;
     struct volFragment *v;
{
    misc->nVolFrag++;
    return 0;
}

/* verifyVolInfoEntry
 *      Follow the volume fragments hanging off of a volinfo structure and
 *      verify they belong to the volinfo structure.
 *      If the volinfo structure is at the head of the same name chain, then
 *      also verify all entries are also on the chain.
 */
afs_int32
verifyVolInfoEntry(ut, volInfoAddr, ai, ao, volInfo)
     struct ubik_trans *ut;
     afs_int32 volInfoAddr;
     int ai, ao;
     struct volInfo *volInfo;
{
    int volCount = 0, ccheck = 1;
    afs_int32 volFragAddr;
    int blockIndex, entryIndex;
    struct volFragment volFragment;
    afs_int32 code = 0, tcode;

    /* check each fragment attached to this volinfo structure */
    for (volFragAddr = ntohl(volInfo->firstFragment); volFragAddr;
	 volFragAddr = ntohl(volFragment.sameNameChain)) {
	tcode = ConvertDiskAddress(volFragAddr, &blockIndex, &entryIndex);
	if (tcode) {
	    Log("verifyVolInfoEntry: Invalid volFrag entry addr 0x%x (on volume '%s')\n", volFragAddr, volInfo->name);
	    Log("     Skipping remainder of volumes on tape\n");
	    if (BumpErrors())
		ERROR(DBBAD);
	    ccheck = 0;
	    break;
	}

	tcode = dbread(ut, volFragAddr, &volFragment, sizeof(volFragment));
	if (tcode)
	    ERROR(tcode);

	if (ntohl(volFragment.vol) != volInfoAddr) {
	    Log("verifyVolInfoEntry: volFrag (addr 0x%x) doesn't point to \n",
		volFragAddr);
	    Log("     volInfo '%s' (addr 0x%x). Points to addr 0x%x\n",
		volInfo->name, volInfoAddr, ntohl(volFragment.vol));
	    if (BumpErrors())
		ERROR(DBBAD);
	}

	/* volume fragment already on a volinfo chain? */
	if (blockMap[blockIndex]->entries[entryIndex] & MAP_VOLFRAGONVOL) {
	    Log("verifyVolInfoEntry: VolFrag (addr %d) on multiple volInfo chains\n", volFragAddr);
	    if (BumpErrors())
		ERROR(DBBAD);
	}
	blockMap[blockIndex]->entries[entryIndex] |= MAP_VOLFRAGONVOL;

	volCount++;
    }

    /* check computed vs. recorded number of fragments */
    if (ccheck && misc->checkFragCount
	&& (ntohl(volInfo->nFrags) != volCount)) {
	Log("verifyVolInfoEntry: VolInfo '%s' (addr 0x%x) volFrag count of %d is wrong (should be %d)\n", volInfo->name, volInfoAddr, ntohl(volInfo->nFrags), volCount);
	if (BumpErrors())
	    ERROR(DBBAD);
    }

    if (volCount > misc->maxVolsPerVolInfo)
	misc->maxVolsPerVolInfo = volCount;

    /* Check that all volInfo structures with same name point to the same 
     * head. If sameNameHead == 0, this is the head structure so we check,
     * otherwise ignore
     */
    if (volInfo->sameNameHead == 0) {	/*i */
	int viCount = 1;	/* count this one */
	struct volInfo tvi;
	afs_int32 tviAddr;

	for (tviAddr = ntohl(volInfo->sameNameChain); tviAddr;
	     tviAddr = ntohl(tvi.sameNameChain)) {
	    viCount++;
	    tcode = ConvertDiskAddress(tviAddr, &blockIndex, &entryIndex);
	    if (tcode) {
		Log("verifyVolInfoEntry: Invalid volInfo entry %s addr 0x%x\n", volInfo->name, tviAddr);
		Log("     Skipping remainder of volumes on same name chain\n");
		if (BumpErrors())
		    ERROR(DBBAD);
		code = 0;
		break;
	    }

	    tcode = dbread(ut, tviAddr, &tvi, sizeof(tvi));
	    if (tcode)
		ERROR(tcode);

	    if (ntohl(tvi.sameNameHead) != volInfoAddr) {
		Log("verifyVolInfoEntry: VolInfo '%s' (addr 0x%x) doesn't point to \n", volInfo->name, tviAddr);
		Log("     head of sameName volInfo (addr 0x%x). Points to addr 0x%x\n", volInfoAddr, ntohl(tvi.sameNameHead));
		if (BumpErrors())
		    ERROR(DBBAD);
	    }

	    if (blockMap[blockIndex]->entries[entryIndex] & MAP_VOLINFOONNAME) {
		Log("verifyVolInfoEntry: VolInfo (addr 0x%x) on multiple same name chains\n", tviAddr);
		if (BumpErrors())
		    ERROR(DBBAD);
	    }
	    blockMap[blockIndex]->entries[entryIndex] |= MAP_VOLINFOONNAME;
	}

	/* select the passed in structure flags */
	if (blockMap[ai]->entries[ao] & MAP_VOLINFONAMEHEAD) {
	    Log("verifyVolInfoEntry: VolInfo '%s' (addr 0x%x) is name head multiple times\n", volInfo->name, volInfoAddr);
	    if (BumpErrors())
		ERROR(DBBAD);
	}
	blockMap[ai]->entries[ao] |= MAP_VOLINFONAMEHEAD;

	if (viCount > misc->maxVolInfosPerName)
	    misc->maxVolInfosPerName = viCount;
	misc->nVolName++;
    }
    /*i */
    misc->nVolInfo++;

  error_exit:
    return (code);
}


/* ------------------------------------
 * verification routines - general
 * ------------------------------------
 */

/* verifyBlocks
 *     Read each block header of every 2K block and remember it in our global 
 *     blockMap array. Also check that the type of block is good.
 */
afs_int32
verifyBlocks(ut)
     struct ubik_trans *ut;
{
    struct block block;
    int blocktype;
    afs_int32 blockAddr;
    struct blockMap *ablockMap = 0;
    int bmsize;
    int i;
    afs_int32 code = 0, tcode;

    /* Remember every header of every block in the database */
    for (i = 0; i < nBlocks; i++) {
	/* To avoid the call from timing out, do a poll every 256 blocks */

	/* read the block header */
	blockAddr = sizeof(db.h) + (i * BLOCKSIZE);
	tcode = dbread(ut, blockAddr, (char *)&block.h, sizeof(block.h));
	if (tcode)
	    ERROR(tcode);

	/* check the block type */
	blocktype = block.h.type;	/* char */
	if ((blocktype < 0) || (blocktype >= NBLOCKTYPES)) {
	    Log("Block (index %d addr %d) has invalid type of %d\n", i,
		blockAddr, blocktype);
	    ERROR(BUDB_BLOCKTYPE);
	}

	/* allocate the block map memory */
	bmsize =
	    sizeof(*ablockMap) + (blockEntries[blocktype] -
				  1) * sizeof(ablockMap->entries[0]);
	ablockMap = (struct blockMap *)malloc(bmsize);
	if (!ablockMap)
	    ERROR(BUDB_NOMEM);
	memset(ablockMap, 0, bmsize);

	ablockMap->nEntries = blockEntries[blocktype];

	/* save the block header in the block map */
	memcpy(&ablockMap->header, &block.h, sizeof(ablockMap->header));
	blockMap[i] = ablockMap;
    }

  error_exit:
    return (code);
}

int minvols, maxvols, ttlvols;

/* verifyHashTableBlock
 *      Take a 2K hash table block and traverse its entries. Make sure each entry
 *      is of the correct type for the hash table, is hashed into the correct 
 *      entry and is not threaded on multiple lists.
 */
afs_int32
verifyHashTableBlock(ut, mhtPtr, htBlockPtr, old, length, index, mapBit)
     struct ubik_trans *ut;
     struct memoryHashTable *mhtPtr;
     struct htBlock *htBlockPtr;
     int old;
     afs_int32 length;		/* size of whole hash table */
     int index;			/* base index of this block */
     int mapBit;
{
    int type;			/* hash table type */
    int entrySize;		/* hashed entry size */
    int blockType;		/* block type for this hash table */
    int blockIndex, entryIndex;
    char entry[sizeof(struct block)];
    dbadr entryAddr;
    int hash;			/* calculated hash value for entry */
    int i, count;
    afs_int32 code = 0, tcode;

    type = ntohl(mhtPtr->ht->functionType);
    entrySize = sizeFunctions[type];
    blockType = hashBlockType[type];

    /* step through this hash table block, being careful to stop
     * before the end of the overall hash table
     */

    for (i = 0; (i < nHTBuckets) && (index < length); i++, index++) {	/*f */
	entryAddr = ntohl(htBlockPtr->bucket[i]);

	/* if this is the old hash table, all entries below the progress mark
	 * should have been moved to the new hash table
	 */
	if (old && (index < mhtPtr->progress) && entryAddr) {
	    Log("Old hash table not empty (entry %d) below progress (%d)\n",
		i, mhtPtr->progress);
	    if (BumpErrors())
		ERROR(DBBAD);
	}

	/* now walk down the chain of each bucket */
	for (count = 0; entryAddr; count++) {	/*w */
	    tcode = ConvertDiskAddress(entryAddr, &blockIndex, &entryIndex);
	    if (tcode) {
		Log("verifyHashTableBlock: Invalid hash table chain addr 0x%x\n", entryAddr);
		Log("     Skipping remainder of bucket %d\n", index);
		if (BumpErrors())
		    ERROR(DBBAD);
		code = 0;
		break;
	    }

	    if (blockMap[blockIndex]->header.type != blockType) {
		Log("Hash table chain (block index %d) incorrect\n",
		    blockIndex);
		Log("     Table type %d traverses block type %d\n", blockType,
		    blockMap[blockIndex]->header.type);
		Log("     Skipping remainder of bucket %d\n", index);
		if (BumpErrors())
		    ERROR(DBBAD);
		break;
	    }

	    if (dbread(ut, entryAddr, &entry[0], entrySize))
		ERROR(DBBAD);

	    hash = ht_HashEntry(mhtPtr, &entry[0]) % length;
	    if (hash != index) {	/* if it hashed to some other place */
		Log("Entry (addr 0x%x) bucket %d, should be hashed into bucket %d\n", entryAddr, index, hash);
		if (BumpErrors())
		    ERROR(DBBAD);
	    }

	    /* check if entry has been examined */
	    if (blockMap[blockIndex]->entries[entryIndex] & mapBit) {
		Log("Entry (addr 0x%x) multiply referenced\n", entryAddr);
		if (BumpErrors())
		    ERROR(DBBAD);
	    }
	    blockMap[blockIndex]->entries[entryIndex] |= mapBit;

	    entryAddr = ntohl(*((dbadr *) (entry + mhtPtr->threadOffset)));
	}			/*w */

	/* Log("Bucket %4d contains %d entries\n", index+1, count); */
	ttlvols += count;
	if (count < minvols)
	    minvols = count;
	if (count > maxvols)
	    maxvols = count;
    }				/*f */

  error_exit:
    return (code);
}

/* verifyHashTable
 *      Read each 2K block a hashtable has (both its old hastable and 
 *      new hashtable) and verify the block has not been read before.
 *      Will also make call to verify entries within each 2K block of
 *      the hash table.
 */
afs_int32
verifyHashTable(ut, mhtPtr, mapBit)
     struct ubik_trans *ut;
     struct memoryHashTable *mhtPtr;
     int mapBit;
{
    struct hashTable *htPtr = mhtPtr->ht;

    struct htBlock hashTableBlock;
    int tableLength;		/* # entries */
    int hashblocks;		/* # blocks */
    dbadr tableAddr;		/* disk addr of hash block */
    int blockIndex, entryIndex;
    int old;
    int i;
    afs_int32 code = 0, tcode;

    extern int nHTBuckets;	/* # buckets in a hash table */

    LogDebug(4, "Htable: length %d oldlength %d progress %d\n",
	     mhtPtr->length, mhtPtr->oldLength, mhtPtr->progress);

    /* check both old and current tables */
    for (old = 0; old <= 1; old++) {	/*fo */
	tableLength = (old ? mhtPtr->oldLength : mhtPtr->length);
	if (tableLength == 0)
	    continue;
	tableAddr = (old ? ntohl(htPtr->oldTable) : ntohl(htPtr->table));
	minvols = 999999;
	maxvols = ttlvols = 0;

	/* follow the chain of hashtable blocks - avoid infinite loops */
	hashblocks = ((tableLength - 1) / nHTBuckets) + 1;	/* numb of 2K hashtable blocks */
	for (i = 0; (tableAddr && (i < hashblocks + 5)); i++) {
	    tcode = ConvertDiskAddress(tableAddr, &blockIndex, &entryIndex);
	    if (tcode) {
		Log("verifyHashTable: Invalid hash table chain addr 0x%x\n",
		    tableAddr);
		Log("     Skipping remainder of hash table chain\n");
		if (BumpErrors())
		    return (DBBAD);
		code = 0;
		break;
	    }

	    if (blockMap[blockIndex]->header.type != hashTable_BLOCK) {
		Log("Hashtable block (index %d addr 0x%x) hashtype %d - type %d, expected type %d\n", i + 1, tableAddr, ntohl(htPtr->functionType), blockMap[blockIndex]->header.type, hashTable_BLOCK);
		Log("     Skipping remainder of hash table chain\n");
		if (BumpErrors())
		    ERROR(BUDB_BLOCKTYPE);
		break;
	    }

	    /* check if we've examined this block before */
	    /* mark this (hash table) block as examined  */
	    if (blockMap[blockIndex]->entries[entryIndex] & MAP_HTBLOCK) {
		Log("Hash table block (index %d addr 0x%x) multiple ref\n",
		    i + 1, tableAddr);
		if (BumpErrors())
		    ERROR(BUDB_DATABASEINCONSISTENT);
	    }
	    blockMap[blockIndex]->entries[entryIndex] |= MAP_HTBLOCK;

	    /* Read the actual hash table block */
	    tcode =
		dbread(ut, tableAddr, &hashTableBlock,
		       sizeof(hashTableBlock));
	    if (tcode)
		ERROR(tcode);

	    /* Verify its entries */
	    tcode =
		verifyHashTableBlock(ut, mhtPtr, &hashTableBlock, old,
				     tableLength, (i * nHTBuckets), mapBit);
	    if (tcode)
		ERROR(tcode);

	    tableAddr = ntohl(hashTableBlock.h.next);
	}

	/* Verify numb hash blocks is as it says */
	if (i != hashblocks) {
	    Log("Incorrect number of hash chain blocks: %d (expected %d), hashtype %d\n", i, hashblocks, ntohl(htPtr->functionType));
	    if (BumpErrors())
		ERROR(BUDB_DATABASEINCONSISTENT);
	}

	if (ttlvols)
	    Log("%d entries; %u buckets; min = %d; max = %d\n", ttlvols,
		tableLength, minvols, maxvols);
    }				/*fo */

  error_exit:
    return (code);
}

/* verifyEntryChains
 *	do a linear walk of all the blocks. Check each block of structures
 *	to see if the actual free matches the recorded free. Also check
 *	the integrity of each allocated structure.
 */
afs_int32
verifyEntryChains(ut)
     struct ubik_trans *ut;
{
    afs_int32 code;
    afs_int32 offset;
    int blockIndex, entryIndex;
    char entry[sizeof(struct block)];
    int entrySize;
    int type;
    int nFree;

    static afs_int32(*checkEntry[NBLOCKTYPES]) ()
	= {
	/* FIXME: this list does not match typeName[] and may be incorrect */
	0,			/* free block */
	    verifyVolFragEntry, verifyVolInfoEntry, verifyTapeEntry, verifyDumpEntry, 0	/* text block */
    };

    for (blockIndex = 0; blockIndex < nBlocks; blockIndex++) {	/*f */
	/* ignore non-structure or blocks with invalid type */
	type = blockMap[blockIndex]->header.type;
	if ((type <= 0) || (type > MAX_STRUCTURE_BLOCK_TYPE))
	    continue;

	entrySize = blockEntrySize[type];
	nFree = 0;

	for (entryIndex = 0; entryIndex < blockMap[blockIndex]->nEntries; entryIndex++) {	/*f */
	    offset =
		sizeof(db.h) + (blockIndex * BLOCKSIZE) +
		sizeof(struct blockHeader) + (entryIndex * entrySize);
	    if (dbread(ut, offset, &entry[0], entrySize))
		return BUDB_IO;

	    /* check if entry is free by looking at the first "afs_int32" of the structure */
	    if (*((afs_int32 *) & entry[0]) == 0) {	/* zero is free */
		/* Is it on any hash chain? */
		if (blockMap[blockIndex]->entries[entryIndex] & MAP_HASHES) {
		    Log("Entry: blockindex %d, entryindex %d - marked free but hashed 0x%x\n", blockIndex, entryIndex, blockMap[blockIndex]->entries[entryIndex]);
		    if (BumpErrors())
			return DBBAD;
		}

		blockMap[blockIndex]->entries[entryIndex] |= MAP_FREE;
		nFree++;
	    } else {
		/* check the entry itself for consistency */
		code =
		    (*(checkEntry[type])) (ut, offset, blockIndex, entryIndex,
					   &entry[0]);
		if (code)
		    return code;
	    }
	}			/*f */

	/* check computed free with recorded free entries */
	if (nFree != ntohs(blockMap[blockIndex]->header.nFree)) {
	    Log("Block (index %d) free count %d has %d free structs\n",
		blockIndex, ntohs(blockMap[blockIndex]->header.nFree), nFree);
	    if (BumpErrors())
		return DBBAD;
	}
    }				/*f */

    return 0;
}


afs_int32
verifyFreeLists()
{
    int i;
    afs_int32 addr;
    int blockIndex, entryIndex;
    int nFree;
    afs_int32 code;

    /* for each free list */
    for (i = 0; i < NBLOCKTYPES; i++) {
	misc->fullyFree[i] = misc->freeLength[i] = 0;

	for (addr = ntohl(db.h.freePtrs[i]); addr;
	     addr = ntohl(blockMap[blockIndex]->header.next)) {
	    misc->freeLength[i]++;

	    code = ConvertDiskAddress(addr, &blockIndex, &entryIndex);
	    if (code || (entryIndex != 0)) {
		Log("verifyFreeLists: Invalid free chain addr 0x%x in %s free chain\n", addr, TypeName(i));
		Log("     Skipping remainder of free chain\n");
		if (BumpErrors())
		    return (DBBAD);
		code = 0;
		break;
	    }

	    /* check block type */
	    if (blockMap[blockIndex]->header.type != i) {
		Log("verifyFreeLists: Found %s type in %s free chain (addr 0x%x)\n",
		    TypeName(blockMap[blockIndex]->header.type), TypeName(i),
		    addr);
		if (BumpErrors())
		    return (DBBAD);
	    }

	    /* If entire block isn't free, check if count of free entries is ok */
	    nFree = ntohs(blockMap[blockIndex]->header.nFree);
	    if (i != free_BLOCK) {
		if ((nFree <= 0) || (nFree > blockEntries[i])) {
		    Log("verifyFreeLists: Illegal free count %d on %s free chain\n", nFree, TypeName(i));
		    if (BumpErrors())
			return (DBBAD);
		} else if (nFree == blockEntries[i]) {
		    misc->fullyFree[i]++;
		}
	    }

	    /* Check if already examined the block */
	    if (blockMap[blockIndex]->free) {
		Log("verifyFreeLists: %s free chain block at addr 0x%x multiply threaded\n", TypeName(i), addr);
		if (BumpErrors())
		    return DBBAD;
	    }
	    blockMap[blockIndex]->free++;
	}
    }

    return 0;
}

/* verifyMapBits
 *	Examines each entry to make sure it was traversed appropriately by
 *	checking the bits for compatibility.
 */
afs_int32
verifyMapBits()
{
    int blockIndex, entryIndex, i, entrySize, type, bits;
    afs_int32 offset;

    for (blockIndex = 0; blockIndex < nBlocks; blockIndex++) {
	/* If no entries in this block, then the block should be marked free */
	if ((blockMap[blockIndex]->nEntries == 0)
	    && !blockMap[blockIndex]->free) {
	    Log("verifyMapBits: Orphan free block (index %d)\n", blockIndex);
	    if (BumpErrors())
		return DBBAD;
	}

	/* check each entry */
	for (entryIndex = 0; entryIndex < blockMap[blockIndex]->nEntries; entryIndex++) {	/*f */
	    if ((entryIndex % 1024) == 0)
		IOMGR_Poll();

	    bits = blockMap[blockIndex]->entries[entryIndex];

	    for (i = 0; i < NMAPCs; i++)
		if ((bits & mapC[i].trigger) == mapC[i].trigger)
		    break;

	    if (i >= NMAPCs) {
		char logstr[256];

		type = blockMap[blockIndex]->header.type;
		entrySize = blockEntrySize[type];
		offset =
		    sizeof(db.h) + (blockIndex * BLOCKSIZE) +
		    sizeof(struct blockHeader) + (entryIndex * entrySize);

		sprintf(logstr, "%s entry Block %d, Entry %d, (addr 0x%x)",
			TypeName(type), blockIndex, entryIndex, offset);

		if (!bits)
		    strcat(logstr, ": An orphaned entry");
		if (bits & MAP_FREE)
		    strcat(logstr, ": A valid free block");
		if (bits & MAP_HTBLOCK)
		    strcat(logstr, ": A valid hash-table block");
		if (bits & MAP_TEXTBLOCK)
		    strcat(logstr, ": A valid text block");
		if (bits & (MAP_DUMPHASH | MAP_IDHASH)) {
		    if (!(bits & MAP_DUMPHASH))
			strcat(logstr,
			       ": A dump not on dump-name hash-chain");
		    else if (!(bits & MAP_IDHASH))
			strcat(logstr, ": A dump not on dump-id hash-chain");
		    else
			strcat(logstr, ": A valid dump entry");
		}
		if (bits & (MAP_TAPEHASH | MAP_TAPEONDUMP)) {
		    if (!(bits & MAP_TAPEHASH))
			strcat(logstr,
			       ": A tape not on tape-name hash-chain");
		    else if (!(bits & MAP_TAPEONDUMP))
			strcat(logstr, ": A tape not associated with a dump");
		    else
			strcat(logstr, ": A valid tape entry");
		}
		if (bits & MAP_VOLINFOONNAME)
		    strcat(logstr,
			   ": A valid volInfo on a volume-name chain");
		if (bits & (MAP_VOLINFONAMEHEAD | MAP_VOLHASH)) {
		    if (!(bits & MAP_VOLINFONAMEHEAD))
			strcat(logstr,
			       ": A volInfo not the head of a volume-name hash-chain");
		    else if (!(bits & MAP_VOLHASH))
			strcat(logstr,
			       ": A volInfo not on volume-name hash-chain");
		    else
			strcat(logstr,
			       ": A valid volInfo in volume-name hash-chain");
		}
		if (bits & (MAP_VOLFRAGONTAPE | MAP_VOLFRAGONVOL)) {
		    if (!(bits & MAP_VOLFRAGONTAPE))
			strcat(logstr,
			       ": A volFrag not associated with a tape");
		    else if (!(bits & MAP_VOLFRAGONVOL))
			strcat(logstr,
			       ": A volFrag not associated with a volume");
		    else
			strcat(logstr, ": A valid volFrag entry");
		}
		Log("%s\n", logstr);

		if (BumpErrors())
		    return DBBAD;
	    }
	}			/*f */
    }

    return 0;
}

afs_int32
verifyText(ut)
     struct ubik_trans *ut;
{
    int i;
    afs_int32 code;
    extern afs_int32 verifyTextChain();

    /* check each of the text types in use */
    for (i = 0; i < TB_NUM; i++) {
	Log("Verify Text: %s", textName[i]);
	code = verifyTextChain(ut, &db.h.textBlock[i]);
	if (code)
	    return (code);
    }
    return (0);
}

/* verifyTextChain
 *	check the integrity of a text chain. Also checks the new chain.
 */
afs_int32
verifyTextChain(ut, tbPtr)
     struct ubik_trans *ut;
     struct textBlock *tbPtr;
{
    dbadr blockAddr;
    int blockIndex, entryIndex;
    struct block block;
    afs_int32 size;
    int new;
    afs_int32 code = 0, tcode;

    for (new = 0; new < 2; new++) {
	size = 0;
	blockAddr = ntohl(tbPtr->textAddr);

	for (blockAddr =
	     (new ? ntohl(tbPtr->newTextAddr) : ntohl(tbPtr->textAddr));
	     blockAddr; blockAddr = ntohl(block.h.next)) {
	    tcode = ConvertDiskAddress(blockAddr, &blockIndex, &entryIndex);
	    if (tcode) {
		Log("verifyTextChain: Invalid %s text block addr 0x%x\n",
		    (new ? "new" : ""), blockAddr);
		Log("     Skipping remainder of text chain\n");
		if (BumpErrors())
		    ERROR(tcode);
		break;
	    }

	    tcode = dbread(ut, blockAddr, &block, sizeof(block));
	    if (tcode)
		ERROR(tcode);

	    if (blockMap[blockIndex]->entries[entryIndex] & MAP_TEXTBLOCK) {
		Log("verifyTextChain: Text block (addr 0x%x) multiply chained\n", blockAddr);
		if (BumpErrors())
		    ERROR(DBBAD);
	    }
	    blockMap[blockIndex]->entries[entryIndex] |= MAP_TEXTBLOCK;

	    size += BLOCK_DATA_SIZE;
	}

	if (ntohl(new ? tbPtr->newsize : tbPtr->size) > size) {
	    Log("verifyTextChain: Text block %s size %d > computed capacity %d\n", (new ? "new" : ""), ntohl(new ? tbPtr->newsize : tbPtr->size), size);
	    if (BumpErrors())
		ERROR(DBBAD);
	}
    }

  error_exit:
    return (code);
}

/* -----------------------------------------
 * verification driver routines
 * -----------------------------------------
 */

/* verifyDatabase
 *	Check the integrity of the database
 */

afs_int32
verifyDatabase(ut, recreateFile)
     struct ubik_trans *ut;
     FILE *recreateFile;	/* not used */
{
    afs_int32 eof;
    int bmsize;
    afs_int32 code = 0, tcode;

    extern int nBlocks;		/* no. blocks in database */
    extern struct ubik_dbase *BU_dbase;

    /* clear verification statistics */
    misc = &miscData;
    memset(&miscData, 0, sizeof(miscData));

#ifdef PDEBUG
    miscData.maxErrors = 1000000;
#else
    miscData.maxErrors = 50;	/* Catch the first 50 errors */
#endif
    miscData.veryLongChain = 0;
    miscData.checkFragCount = 1;	/* check frags */

    /* check eofPtr */
    eof = ntohl(db.h.eofPtr);
    eof -= sizeof(db.h);	/* subtract header */
    nBlocks = eof / BLOCKSIZE;

    Log("Verify of backup database started\n");
    Log("Database is %u. %d blocks of %d Bytes\n", eof, nBlocks, BLOCKSIZE);

    if ((eof < 0) || (nBlocks * BLOCKSIZE != eof)) {
	Log("Database eofPtr (%d) bad, blocksize %d\n", eof, BLOCKSIZE);
	ERROR(DBBAD);
    }

    /* set size of database */
    miscData.nBlocks = nBlocks;

    if (nBlocks == 0)
	ERROR(0);		/* Nothing to check? */

    /* construct block map - first level is the array of pointers */
    bmsize = nBlocks * sizeof(struct blockMap *);
    blockMap = (struct blockMap **)malloc(bmsize);
    if (!blockMap)
	ERROR(BUDB_NOMEM);
    memset(blockMap, 0, bmsize);

    /* verify blocks and construct the block map */
    Log("Read header of every block\n");
    tcode = verifyBlocks(ut);
    if (tcode)
	ERROR(tcode);

    /* check the various hash tables */
    Log("Verify volume name hash table\n");
    tcode = verifyHashTable(ut, &db.volName, MAP_VOLHASH);
    if (tcode)
	ERROR(tcode);

    Log("Verify tape name hash table\n");
    tcode = verifyHashTable(ut, &db.tapeName, MAP_TAPEHASH);
    if (tcode)
	ERROR(tcode);

    Log("Verify dump name hash table\n");
    tcode = verifyHashTable(ut, &db.dumpName, MAP_DUMPHASH);
    if (tcode)
	ERROR(tcode);

    Log("Verify dump id hash table\n");
    tcode = verifyHashTable(ut, &db.dumpIden, MAP_IDHASH);
    if (tcode)
	ERROR(tcode);

    /* check the entry chains */
    Log("Verify all blocks and entries\n");
    tcode = verifyEntryChains(ut);
    if (tcode)
	ERROR(tcode);

    /* check text blocks - Log message in verifyText */
    tcode = verifyText(ut);
    if (tcode)
	ERROR(tcode);

    /* check free list */
    Log("Verify Free Lists\n");
    tcode = verifyFreeLists();
    if (tcode)
	ERROR(tcode);

    /* check entry map bit compatibility */

    Log("Verify Map bits\n");
    tcode = verifyMapBits();
    if (tcode)
	ERROR(tcode);

  error_exit:
    /* free the block map */
    if (blockMap != 0) {
	int i;

	/* free all the individual maps */
	for (i = 0; i < nBlocks; i++) {
	    if (blockMap[i])
		free(blockMap[i]);
	}

	/* free the pointer array */
	free(blockMap);
	blockMap = 0;
    }

    if (!tcode) {
	Log("# 2K database blocks    = %d\n", miscData.nBlocks);
	Log("# Dump entries found    = %d. 3 dumps per block\n",
	    miscData.nDump);
	Log("  max tapes   on a dump = %d\n", miscData.maxTapesPerDump);
	Log("  max volumes on a dump = %d\n", miscData.maxVolsPerDump);
	Log("  max appends on a dump = %d\n", miscData.maxAppendsPerDump);
	Log("  # Blocks with space   = %d\n", miscData.freeLength[4]);
	Log("  # of those fully free = %d\n", miscData.fullyFree[4]);
	Log("# Tape entries found    = %d. 20 tapes per block\n",
	    miscData.nTape);
	Log("  max volumes on a tape = %d\n", miscData.maxVolsPerTape);
	Log("  # Blocks with space   = %d\n", miscData.freeLength[3]);
	Log("  # of those fully free = %d\n", miscData.fullyFree[3]);
	Log("# VolInfo entries found = %d. 20 volInfos per block\n",
	    miscData.nVolInfo);
	Log("  # head of sameNameCh  = %d\n", miscData.nVolName);
	Log("  max on a  sameNameCh  = %d\n", miscData.maxVolInfosPerName);
	Log("  max VolFrags on chain = %d\n", miscData.maxVolsPerVolInfo);
	Log("  # Blocks with space   = %d\n", miscData.freeLength[2]);
	Log("  # of those fully free = %d\n", miscData.fullyFree[2]);
	Log("# VolFrag entries found = %d. 45 VolFrags per block\n",
	    miscData.nVolFrag);
	Log("  # Blocks with space   = %d\n", miscData.freeLength[1]);
	Log("  # of those fully free = %d\n", miscData.fullyFree[1]);
	Log("# free blocks           = %d\n", miscData.freeLength[0]);
    }

    Log("Verify of database completed. %d errors found\n", miscData.errors);

    if (miscData.errors && !code)
	code = DBBAD;
    return (code);
}


/* -----------------------------
 * interface routines
 * -----------------------------
 */

/* BUDB_DbVerify
 *	check the integrity of the database
 * exit:
 *	status - integrity: 0, ok; n, not ok (error value)
 *	orphans - no. of orphan blocks
 *	host - address of host that did verification
 */
afs_int32 DbVerify();
afs_int32
SBUDB_DbVerify(call, status, orphans, host)
     struct rx_call *call;
     afs_int32 *status;
     afs_int32 *orphans;
     afs_int32 *host;
{
    afs_int32 code;

    code = DbVerify(call, status, orphans, host);
    osi_auditU(call, BUDB_DBVfyEvent, code, AUD_END);
    return code;
}

afs_int32
DbVerify(call, status, orphans, host)
     struct rx_call *call;
     afs_int32 *status;
     afs_int32 *orphans;
     afs_int32 *host;
{
    struct ubik_trans *ut = 0;
    afs_int32 code = 0, tcode;
    char hostname[64];
    struct hostent *th;

    if (callPermitted(call) == 0)
	ERROR(BUDB_NOTPERMITTED);

    tcode = InitRPC(&ut, LOCKREAD, 1);
    if (tcode)
	ERROR(tcode);

    tcode = verifyDatabase(ut, 0);	/* check the database */
    if (tcode)
	ERROR(tcode);

  error_exit:
    if (ut) {
	if (code)
	    ubik_AbortTrans(ut);
	else
	    code = ubik_EndTrans(ut);
    }

    *status = code;
    *orphans = 0;

    gethostname(hostname, sizeof(hostname));
    th = gethostbyname(hostname);
    if (!th)
	*host = 0;
    else {
	memcpy(host, th->h_addr, sizeof(afs_int32));
	*host = ntohl(*host);
    }

    return (0);
}

/* ----------------------
 * debug support
 * ----------------------
 */

/* check_header
 *	do a simple sanity check on the database header
 */

check_header(callerst)
     char *callerst;
{
    static int iteration_count = 0;
    afs_int32 eof;

    eof = ntohl(db.h.eofPtr);
    if ((eof == 0) || (eof < 0)) {
	Log("Eof check failed, caller %s, eof 0x%x\n", callerst, eof);
    }

    eof -= sizeof(db.h);
    if (eof < 0) {
	Log("Adjusted Eof check failed, caller %s, eof 0x%x\n", callerst,
	    eof);
    }

    iteration_count++;
    if (iteration_count >= 10) {
	Log("Eof ptr is 0x%x\n", eof);
	iteration_count = 0;
    }
}
