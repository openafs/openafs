/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* dump the database
 * Dump is made to a local file. Structures are dumped in network byte order
 * for transportability between hosts
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/param.h>
#endif
#include <afs/stds.h>
#include <sys/types.h>
#include <ubik.h>
#include <lock.h>
#include <string.h>

#include "database.h"
#include "budb.h"
#include "globals.h"
#include "error_macros.h"
#include "budb_errs.h"
#include "afs/audit.h"


/* dump ubik database - routines to scan the database and dump all
 * 	the information
 */

/* -----------------------
 * synchronization on pipe
 * -----------------------
 */

/* interlocking for database dump */

dumpSyncT dumpSync;
dumpSyncP dumpSyncPtr = &dumpSync;


/* canWrite
 *	check if we should dump more of the database. Waits for the reader
 *	to drain the information before allowing the writer to proceed.
 * exit:
 *	1 - ok to write
 */

afs_int32
canWrite(fid)
     int fid;
{
    afs_int32 code = 0;
    extern dumpSyncP dumpSyncPtr;

    ObtainWriteLock(&dumpSyncPtr->ds_lock);

    /* let the pipe drain */
    while (dumpSyncPtr->ds_bytes > 0) {
	if (dumpSyncPtr->ds_readerStatus == DS_WAITING) {
	    dumpSyncPtr->ds_readerStatus = 0;
	    code = LWP_SignalProcess(&dumpSyncPtr->ds_readerStatus);
	    if (code)
		LogError(code, "canWrite: Signal delivery failed\n");
	}
	dumpSyncPtr->ds_writerStatus = DS_WAITING;
	ReleaseWriteLock(&dumpSyncPtr->ds_lock);
	LWP_WaitProcess(&dumpSyncPtr->ds_writerStatus);
	ObtainWriteLock(&dumpSyncPtr->ds_lock);
    }
    return (1);
}


/* haveWritten
 *	record the fact that nbytes have been written. Signal the reader
 *	to proceed, and unlock.
 * exit:
 *	no return value
 */

void
haveWritten(nbytes)
     afs_int32 nbytes;
{
    afs_int32 code = 0;
    extern dumpSyncP dumpSyncPtr;

    dumpSyncPtr->ds_bytes += nbytes;
    if (dumpSyncPtr->ds_readerStatus == DS_WAITING) {
	dumpSyncPtr->ds_readerStatus = 0;
	code = LWP_SignalProcess(&dumpSyncPtr->ds_readerStatus);
	if (code)
	    LogError(code, "haveWritten: Signal delivery failed\n");
    }
    ReleaseWriteLock(&dumpSyncPtr->ds_lock);
}

/* doneWriting
 *	wait for the reader to drain all the information, and then set the
 *	done flag.
 */

void
doneWriting(error)
     afs_int32 error;
{
    afs_int32 code = 0;

    /* wait for the reader */
    ObtainWriteLock(&dumpSyncPtr->ds_lock);
    while (dumpSyncPtr->ds_readerStatus != DS_WAITING) {
	LogDebug(4, "doneWriting: waiting for Reader\n");
	dumpSyncPtr->ds_writerStatus = DS_WAITING;
	ReleaseWriteLock(&dumpSyncPtr->ds_lock);
	LWP_WaitProcess(&dumpSyncPtr->ds_writerStatus);
	ObtainWriteLock(&dumpSyncPtr->ds_lock);
    }

    LogDebug(4, "doneWriting: setting done\n");

    /* signal that we are done */
    if (error)
	dumpSyncPtr->ds_writerStatus = DS_DONE_ERROR;
    else
	dumpSyncPtr->ds_writerStatus = DS_DONE;
    dumpSyncPtr->ds_readerStatus = 0;
    code = LWP_NoYieldSignal(&dumpSyncPtr->ds_readerStatus);
    if (code)
	LogError(code, "doneWriting: Signal delivery failed\n");
    ReleaseWriteLock(&dumpSyncPtr->ds_lock);
}

/* notes:
 *	ut - setup and pass down
 */

/* writeStructHeader
 *	write header appropriate for requested structure type
 */

afs_int32
writeStructHeader(fid, type)
     int fid;
     afs_int32 type;
{
    struct structDumpHeader hostDumpHeader, netDumpHeader;

    hostDumpHeader.type = type;
    hostDumpHeader.structversion = 1;


    switch (type) {
    case SD_DBHEADER:
	hostDumpHeader.size = sizeof(struct DbHeader);
	break;

    case SD_DUMP:
	hostDumpHeader.size = sizeof(struct budb_dumpEntry);
	break;

    case SD_TAPE:
	hostDumpHeader.size = sizeof(struct budb_tapeEntry);
	break;

    case SD_VOLUME:
	hostDumpHeader.size = sizeof(struct budb_volumeEntry);
	break;

    case SD_END:
	hostDumpHeader.size = 0;
	break;

    default:
	LogError(0, "writeStructHeader: invalid type %d\n", type);
	BUDB_EXIT(1);
    }

    structDumpHeader_hton(&hostDumpHeader, &netDumpHeader);

    if (canWrite(fid) <= 0)
	return (BUDB_DUMPFAILED);
    if (write(fid, &netDumpHeader, sizeof(netDumpHeader)) !=
	sizeof(netDumpHeader))
	return (BUDB_DUMPFAILED);
    haveWritten(sizeof(netDumpHeader));

    return (0);
}

/* writeTextHeader
 *	write header appropriate for requested structure type
 */

afs_int32
writeTextHeader(fid, type)
     int fid;
     afs_int32 type;
{
    struct structDumpHeader hostDumpHeader, netDumpHeader;

    hostDumpHeader.structversion = 1;

    switch (type) {
    case TB_DUMPSCHEDULE:
	hostDumpHeader.type = SD_TEXT_DUMPSCHEDULE;
	break;

    case TB_VOLUMESET:
	hostDumpHeader.type = SD_TEXT_VOLUMESET;
	break;

    case TB_TAPEHOSTS:
	hostDumpHeader.type = SD_TEXT_TAPEHOSTS;
	break;

    default:
	LogError(0, "writeTextHeader: invalid type %d\n", type);
	BUDB_EXIT(1);
    }

    hostDumpHeader.size = ntohl(db.h.textBlock[type].size);
    structDumpHeader_hton(&hostDumpHeader, &netDumpHeader);

    if (canWrite(fid) <= 0)
	return (BUDB_DUMPFAILED);

    if (write(fid, &netDumpHeader, sizeof(netDumpHeader)) !=
	sizeof(netDumpHeader))
	return (BUDB_DUMPFAILED);

    haveWritten(sizeof(netDumpHeader));

    return (0);
}

afs_int32
writeDbHeader(fid)
     int fid;
{
    struct DbHeader header;
    afs_int32 curtime;
    afs_int32 code = 0, tcode;

    extern struct memoryDB db;

    /* check the memory database header for integrity */
    if (db.h.version != db.h.checkVersion)
	ERROR(BUDB_DATABASEINCONSISTENT);

    curtime = time(0);

    /* copy selected fields. Source is in xdr format. */
    header.dbversion = db.h.version;
    header.created = htonl(curtime);
    strcpy(header.cell, "");
    header.lastDumpId = db.h.lastDumpId;
    header.lastInstanceId = db.h.lastInstanceId;
    header.lastTapeId = db.h.lastTapeId;

    tcode = writeStructHeader(fid, SD_DBHEADER);
    if (tcode)
	ERROR(tcode);

    if (canWrite(fid) <= 0)
	ERROR(BUDB_DUMPFAILED);

    if (write(fid, &header, sizeof(header)) != sizeof(header))
	ERROR(BUDB_DUMPFAILED);

    haveWritten(sizeof(header));

  error_exit:
    return (code);
}

/* writeDump
 *	write out a dump entry structure
 */

afs_int32
writeDump(fid, dumpPtr)
     int fid;
     dbDumpP dumpPtr;
{
    struct budb_dumpEntry dumpEntry;
    afs_int32 code = 0, tcode;

    tcode = dumpToBudbDump(dumpPtr, &dumpEntry);
    if (tcode)
	ERROR(tcode);

    writeStructHeader(fid, SD_DUMP);

    if (canWrite(fid) <= 0)
	ERROR(BUDB_DUMPFAILED);

    if (write(fid, &dumpEntry, sizeof(dumpEntry)) != sizeof(dumpEntry))
	ERROR(BUDB_DUMPFAILED);
    haveWritten(sizeof(dumpEntry));

  error_exit:
    return (code);
}

afs_int32
writeTape(fid, tapePtr, dumpid)
     int fid;
     struct tape *tapePtr;
     afs_int32 dumpid;
{
    struct budb_tapeEntry tapeEntry;
    afs_int32 code = 0, tcode;

    tcode = writeStructHeader(fid, SD_TAPE);
    if (tcode)
	ERROR(tcode);

    tapeToBudbTape(tapePtr, &tapeEntry);

    tapeEntry.dump = htonl(dumpid);

    if (canWrite(fid) <= 0)
	ERROR(BUDB_DUMPFAILED);

    if (write(fid, &tapeEntry, sizeof(tapeEntry)) != sizeof(tapeEntry))
	ERROR(BUDB_DUMPFAILED);

    haveWritten(sizeof(tapeEntry));

  error_exit:
    return (code);
}

/* combines volFragment and volInfo */

afs_int32
writeVolume(ut, fid, volFragmentPtr, volInfoPtr, dumpid, tapeName)
     struct ubik_trans *ut;
     int fid;
     struct volFragment *volFragmentPtr;
     struct volInfo *volInfoPtr;
     afs_int32 dumpid;
     char *tapeName;
{
    struct budb_volumeEntry budbVolume;
    afs_int32 code = 0;

    volsToBudbVol(volFragmentPtr, volInfoPtr, &budbVolume);

    budbVolume.dump = htonl(dumpid);
    strcpy(budbVolume.tape, tapeName);

    writeStructHeader(fid, SD_VOLUME);

    if (canWrite(fid) <= 0)
	ERROR(BUDB_DUMPFAILED);

    if (write(fid, &budbVolume, sizeof(budbVolume)) != sizeof(budbVolume))
	ERROR(BUDB_DUMPFAILED);

    haveWritten(sizeof(budbVolume));

  error_exit:
    return (code);
}

/* -------------------
 * handlers for the text blocks
 * -------------------
 */

/* checkLock
 *	make sure a text lock is NOT held
 * exit:
 *	0 - not held
 *	n - error
 */

afs_int32
checkLock(textType)
     afs_int32 textType;
{
    db_lockP lockPtr;

    if ((textType < 0) || (textType > TB_NUM - 1))
	return (BUDB_BADARGUMENT);

    lockPtr = &db.h.textLocks[textType];

    if (lockPtr->lockState != 0)
	return (BUDB_LOCKED);
    return (0);
}

/* checkText
 *	check the integrity of the specified text type
 */

checkText(ut, textType)
     struct ubik_trans *ut;
     afs_int32 textType;
{
    struct textBlock *tbPtr;
    afs_int32 nBytes = 0;	/* accumulated actual size */
    afs_int32 size;
    struct block block;
    dbadr blockAddr;

    afs_int32 code = 0;

    tbPtr = &db.h.textBlock[textType];
    blockAddr = ntohl(tbPtr->textAddr);
    size = ntohl(tbPtr->size);

    while (blockAddr != 0) {
	/* read the block */
	code =
	    cdbread(ut, text_BLOCK, blockAddr, (char *)&block, sizeof(block));
	if (code)
	    ERROR(code);

	/* check its type */
	if (block.h.type != text_BLOCK)
	    ERROR(BUDB_DATABASEINCONSISTENT);

	/* add up the size */
	nBytes += BLOCK_DATA_SIZE;

	blockAddr = ntohl(block.h.next);
    }

    /* ensure that we have at least the expected amount of text */
    if (nBytes < size)
	ERROR(BUDB_DATABASEINCONSISTENT);

  error_exit:
    return (code);
}

/* writeText
 * entry:
 *	textType - type of text block, e.g. TB_DUMPSCHEDULE
 */

afs_int32
writeText(ut, fid, textType)
     struct ubik_trans *ut;
     int fid;
     int textType;
{
    struct textBlock *tbPtr;
    afs_int32 textSize, writeSize;
    dbadr dbAddr;
    struct block block;
    afs_int32 code = 0;

    /* check lock is free */
    code = checkLock(textType);
    if (code)
	ERROR(code);

    /* ensure that this block has the correct type */
    code = checkText(ut, textType);
    if (code) {
	LogError(0, "writeText: text type %d damaged\n", textType);
	ERROR(code);
    }

    tbPtr = &db.h.textBlock[textType];
    textSize = ntohl(tbPtr->size);
    dbAddr = ntohl(tbPtr->textAddr);

    if (!dbAddr)
	goto error_exit;	/* Don't save anything if no blocks */

    writeTextHeader(fid, textType);

    while (dbAddr) {
	code = cdbread(ut, text_BLOCK, dbAddr, (char *)&block, sizeof(block));
	if (code)
	    ERROR(code);

	writeSize = MIN(textSize, BLOCK_DATA_SIZE);
	if (!writeSize)
	    break;

	if (canWrite(fid) <= 0)
	    ERROR(BUDB_DUMPFAILED);

	if (write(fid, &block.a[0], writeSize) != writeSize)
	    ERROR(BUDB_IO);

	haveWritten(writeSize);
	textSize -= writeSize;

	dbAddr = ntohl(block.h.next);
    }

  error_exit:
    return (code);
}

#define MAXAPPENDS 200

afs_int32
writeDatabase(ut, fid)
     struct ubik_trans *ut;
     int fid;
{
    dbadr dbAddr, dbAppAddr;
    struct dump diskDump, apDiskDump;
    dbadr tapeAddr;
    struct tape diskTape;
    dbadr volFragAddr;
    struct volFragment diskVolFragment;
    struct volInfo diskVolInfo;
    int length, hash;
    int old = 0;
    int entrySize;
    afs_int32 code = 0, tcode;
    afs_int32 appDumpAddrs[MAXAPPENDS], numaddrs, appcount, j;

    struct memoryHashTable *mht;

    LogDebug(4, "writeDatabase:\n");

    /* write out a header identifying this database etc */
    tcode = writeDbHeader(fid);
    if (tcode) {
	LogError(tcode, "writeDatabase: Can't write Header\n");
	ERROR(tcode);
    }

    /* write out the tree of dump structures */

    mht = ht_GetType(HT_dumpIden_FUNCTION, &entrySize);
    if (!mht) {
	LogError(tcode, "writeDatabase: Can't get dump type\n");
	ERROR(BUDB_BADARGUMENT);
    }

    for (old = 0; old <= 1; old++) {
	/*oldnew */
	/* only two states, old or not old */
	length = (old ? mht->oldLength : mht->length);
	if (!length)
	    continue;

	for (hash = 0; hash < length; hash++) {
	    /*hashBuckets */
	    /* dump all the dumps in this hash bucket
	     */
	    for (dbAddr = ht_LookupBucket(ut, mht, hash, old); dbAddr; dbAddr = ntohl(diskDump.idHashChain)) {	/*initialDumps */
		/* now check if this dump had any errors/inconsistencies.
		 * If so, don't dump it
		 */
		if (badEntry(dbAddr)) {
		    LogError(0,
			     "writeDatabase: Damaged dump entry at addr 0x%x\n",
			     dbAddr);
		    Log("     Skipping remainder of dumps on hash chain %d\n",
			hash);
		    break;
		}

		tcode =
		    cdbread(ut, dump_BLOCK, dbAddr, &diskDump,
			    sizeof(diskDump));
		if (tcode) {
		    LogError(tcode,
			     "writeDatabase: Can't read dump entry (addr 0x%x)\n",
			     dbAddr);
		    Log("     Skipping remainder of dumps on hash chain %d\n",
			hash);
		    break;
		}

		/* Skip appended dumps, only start with initial dumps */
		if (diskDump.initialDumpID != 0)
		    continue;

		/* Skip appended dumps, only start with initial dumps. Then
		 * follow the appended dump chain so they are in order for restore.
		 */
		appcount = numaddrs = 0;
		for (dbAppAddr = dbAddr; dbAppAddr;
		     dbAppAddr = ntohl(apDiskDump.appendedDumpChain)) {
		    /*appendedDumps */
		    /* Check to see if we have a circular loop of appended dumps */
		    for (j = 0; j < numaddrs; j++) {
			if (appDumpAddrs[j] == dbAppAddr)
			    break;	/* circular loop */
		    }
		    if (j < numaddrs) {	/* circular loop */
			Log("writeDatabase: Circular loop found in appended dumps\n");
			Log("Skipping rest of appended dumps of dumpID %u\n",
			    ntohl(diskDump.id));
			break;
		    }
		    if (numaddrs >= MAXAPPENDS)
			numaddrs = MAXAPPENDS - 1;	/* don't overflow */
		    appDumpAddrs[numaddrs] = dbAppAddr;
		    numaddrs++;

		    /* If we dump a 1000 appended dumps, assume a loop */
		    if (appcount >= 5 * MAXAPPENDS) {
			Log("writeDatabase: Potential circular loop of appended dumps\n");
			Log("Skipping rest of appended dumps of dumpID %u. Dumped %d\n", ntohl(diskDump.id), appcount);
			break;
		    }
		    appcount++;

		    /* Read the dump entry */
		    if (dbAddr == dbAppAddr) {
			/* First time through, don't need to read the dump entry again */
			memcpy(&apDiskDump, &diskDump, sizeof(diskDump));
		    } else {
			if (badEntry(dbAppAddr)) {
			    LogError(0,
				     "writeDatabase: Damaged appended dump entry at addr 0x%x\n",
				     dbAddr);
			    Log("     Skipping this and remainder of appended dumps of initial DumpID %u\n", ntohl(diskDump.id));
			    break;
			}

			tcode =
			    cdbread(ut, dump_BLOCK, dbAppAddr, &apDiskDump,
				    sizeof(apDiskDump));
			if (tcode) {
			    LogError(tcode,
				     "writeDatabase: Can't read appended dump entry (addr 0x%x)\n",
				     dbAppAddr);
			    Log("     Skipping this and remainder of appended dumps of initial DumpID %u\n", ntohl(diskDump.id));
			    break;
			}

			/* Verify that this appended dump points to the initial dump */
			if (ntohl(apDiskDump.initialDumpID) !=
			    ntohl(diskDump.id)) {
			    LogError(0,
				     "writeDatabase: Appended dumpID %u does not reference initial dumpID %u\n",
				     ntohl(apDiskDump.id),
				     ntohl(diskDump.id));
			    Log("     Skipping this appended dump\n");
			    continue;
			}
		    }

		    /* Save the dump entry */
		    tcode = writeDump(fid, &apDiskDump);
		    if (tcode) {
			LogError(tcode,
				 "writeDatabase: Can't write dump entry\n");
			ERROR(tcode);
		    }

		    /* For each tape on this dump
		     */
		    for (tapeAddr = ntohl(apDiskDump.firstTape); tapeAddr; tapeAddr = ntohl(diskTape.nextTape)) {	/*tapes */
			/* read the tape entry */
			tcode =
			    cdbread(ut, tape_BLOCK, tapeAddr, &diskTape,
				    sizeof(diskTape));
			if (tcode) {
			    LogError(tcode,
				     "writeDatabase: Can't read tape entry (addr 0x%x) of dumpID %u\n",
				     tapeAddr, ntohl(apDiskDump.id));
			    Log("     Skipping this and remaining tapes in the dump (and all their volumes)\n");
			    break;
			}

			/* Save the tape entry */
			tcode =
			    writeTape(fid, &diskTape, ntohl(apDiskDump.id));
			if (tcode) {
			    LogError(tcode,
				     "writeDatabase: Can't write tape entry\n");
			    ERROR(tcode);
			}

			/* For each volume on this tape.
			 */
			for (volFragAddr = ntohl(diskTape.firstVol); volFragAddr; volFragAddr = ntohl(diskVolFragment.sameTapeChain)) {	/*volumes */
			    /* Read the volume Fragment entry */
			    tcode =
				cdbread(ut, volFragment_BLOCK, volFragAddr,
					&diskVolFragment,
					sizeof(diskVolFragment));
			    if (tcode) {
				LogError(tcode,
					 "writeDatabase: Can't read volfrag entry (addr 0x%x) of dumpID %u\n",
					 volFragAddr, ntohl(apDiskDump.id));
				Log("     Skipping this and remaining volumes on tape '%s'\n", diskTape.name);
				break;
			    }

			    /* Read the volume Info entry */
			    tcode =
				cdbread(ut, volInfo_BLOCK,
					ntohl(diskVolFragment.vol),
					&diskVolInfo, sizeof(diskVolInfo));
			    if (tcode) {
				LogError(tcode,
					 "writeDatabase: Can't read volinfo entry (addr 0x%x) of dumpID %u\n",
					 ntohl(diskVolFragment.vol),
					 ntohl(apDiskDump.id));
				Log("     Skipping volume on tape '%s'\n",
				    diskTape.name);
				continue;
			    }

			    /* Save the volume entry */
			    tcode =
				writeVolume(ut, fid, &diskVolFragment,
					    &diskVolInfo,
					    ntohl(apDiskDump.id),
					    diskTape.name);
			    if (tcode) {
				LogError(tcode,
					 "writeDatabase: Can't write volume entry\n");
				ERROR(tcode);
			    }
			}	/*volumes */
		    }		/*tapes */
		}		/*appendedDumps */
	    }			/*initialDumps */
	}			/*hashBuckets */
    }				/*oldnew */

    /* write out the textual configuration information */
    tcode = writeText(ut, fid, TB_DUMPSCHEDULE);
    if (tcode) {
	LogError(tcode, "writeDatabase: Can't write dump schedule\n");
	ERROR(tcode);
    }
    tcode = writeText(ut, fid, TB_VOLUMESET);
    if (tcode) {
	LogError(tcode, "writeDatabase: Can't write volume set\n");
	ERROR(tcode);
    }
    tcode = writeText(ut, fid, TB_TAPEHOSTS);
    if (tcode) {
	LogError(tcode, "writeDatabase: Can't write tape hosts\n");
	ERROR(tcode);
    }

    tcode = writeStructHeader(fid, SD_END);
    if (tcode) {
	LogError(tcode, "writeDatabase: Can't write end savedb\n");
	ERROR(tcode);
    }

  error_exit:
    doneWriting(code);
    return (code);
}


#ifdef notdef

afs_int32
canWrite(fid)
     int fid;
{
    afs_int32 in, out, except;
    struct timeval tp;
    afs_int32 code;

    tp.tv_sec = 0;
    tp.tv_usec = 0;

    out = (1 << fid);
    in = 0;
    except = 0;

    code = IOMGR_Select(32, &in, &out, &except, &tp);
    return (code);
}

#endif /* notdef */
