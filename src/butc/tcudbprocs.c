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
#include <io.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <errno.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <afs/afsint.h>
#include <stdio.h>
#include <string.h>
#include <afs/procmgmt.h>
#include <afs/assert.h>
#include <afs/prs_fs.h>
#include <fcntl.h>
#include <afs/nfs.h>
#include <lwp.h>
#include <lock.h>
#include <afs/cellconfig.h>
#include <afs/keys.h>
#include <ubik.h>
#include <afs/acl.h>
#include <afs/tcdata.h>
#include <afs/budb.h>
#include <afs/budb_client.h>
#include <afs/bubasics.h>
#include "error_macros.h"

/* GLOBAL CONFIGURATION PARAMETERS */
#define BIGCHUNK 102400

extern int dump_namecheck;
extern int autoQuery;

/* CreateDBDump
 *      create a dump entry for a saved database 
 */

afs_int32
CreateDBDump(dumpEntryPtr)
     struct budb_dumpEntry *dumpEntryPtr;
{
    afs_int32 code = 0;

    memset(dumpEntryPtr, 0, sizeof(struct budb_dumpEntry));

    strcpy(dumpEntryPtr->name, DUMP_TAPE_NAME);
    strcpy(dumpEntryPtr->tapes.format, DUMP_TAPE_NAME);
    strcat(dumpEntryPtr->tapes.format, ".%d");
    strcpy(dumpEntryPtr->volumeSetName, "");
    strcpy(dumpEntryPtr->dumpPath, "");
    dumpEntryPtr->created = 0;	/* let database assign it */
    dumpEntryPtr->incTime = 0;
    dumpEntryPtr->nVolumes = 0;
    dumpEntryPtr->initialDumpID = 0;
    dumpEntryPtr->parent = 0;
    dumpEntryPtr->level = 0;
    dumpEntryPtr->tapes.maxTapes = 0;
    dumpEntryPtr->tapes.b = 1;

    /* now call the database to create the entry */
    code = bcdb_CreateDump(dumpEntryPtr);
    return (code);
}

struct tapeEntryList {
    struct tapeEntryList *next;
    afs_uint32 oldDumpId;
    struct budb_tapeEntry tapeEnt;
};
struct tapeEntryList *listEntryHead;
struct tapeEntryList *listEntryPtr;
#define tapeEntryPtr (&listEntryPtr->tapeEnt)
struct budb_dumpEntry lastDump;	/* the last dump of this volset */

/* GetDBTape
 *      Load a DB tape, read and over write its label.
 *      Leave the tape mounted.
 */
afs_int32
GetDBTape(taskId, expires, tapeInfoPtr, dumpid, sequence, queryFlag,
	  wroteLabel)
     afs_int32 taskId;
     Date expires;
     struct butm_tapeInfo *tapeInfoPtr;
     afs_uint32 dumpid;
     afs_int32 sequence;
     int queryFlag;
     int *wroteLabel;
{
    afs_int32 code = 0;
    int interactiveFlag;
    char tapeName[BU_MAXTAPELEN];
    char strlevel[5];
    struct timeval tp;
    struct timezone tzp;
    afs_int32 curTime;
    int tapecount = 1;

    struct butm_tapeLabel oldTapeLabel, newLabel;
    struct tapeEntryList *endList;
    extern struct tapeConfig globalTapeConfig;

    /* construct the name of the tape */
    sprintf(tapeName, "%s.%-d", DUMP_TAPE_NAME, sequence);

    interactiveFlag = queryFlag;
    *wroteLabel = 0;

    while (!*wroteLabel) {	/*w */
	if (interactiveFlag) {	/* need a tape to write */
	    code =
		PromptForTape(SAVEDBOPCODE, tapeName, dumpid, taskId,
			      tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	interactiveFlag = 1;
	tapecount++;

	code = butm_Mount(tapeInfoPtr, tapeName);
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error, "Can't open tape\n");
	    goto getNewTape;
	}

	memset(&oldTapeLabel, 0, sizeof(oldTapeLabel));
	code = butm_ReadLabel(tapeInfoPtr, &oldTapeLabel, 1);	/* rewind tape */
	if (code) {
	    oldTapeLabel.useCount = 0;	/* no label exists */
	    oldTapeLabel.structVersion = 0;
	    strcpy(oldTapeLabel.pName, "");
	} else {
	    /* If tape has a name, it must be null or database tape name */
	    if (dump_namecheck && strcmp(oldTapeLabel.AFSName, "")
		&& !databaseTape(oldTapeLabel.AFSName)) {
		char gotName[BU_MAXTAPELEN + 32];

		LABELNAME(gotName, &oldTapeLabel);
		TLog(taskId,
		     "This tape %s must be a database tape or NULL tape\n",
		     gotName);

	      getNewTape:
		unmountTape(taskId, tapeInfoPtr);
		continue;
	    }

	    /* Do not overwrite a tape that belongs to this dump */
	    if (oldTapeLabel.dumpid && (oldTapeLabel.dumpid == dumpid)) {
		ErrorLog(0, taskId, 0, 0,
			 "Can't overwrite tape containing the dump in progress\n");
		goto getNewTape;
	    }

	    /* On first tape, the savedb has not started yet, so the database is not locked 
	     * and we can therefore, access information from it. This is easier to do because
	     * database dumps don't have appended dumps (nor appended).
	     */
	    if (sequence == 1) {
		afs_uint32 dmp;
		struct budb_dumpEntry de, de2;

		/* Verify the tape has not expired
		 * Early database dumps don't have a dumpid 
		 */
		if (!tapeExpired(&oldTapeLabel)) {
		    TLog(taskId, "This tape has not expired\n");
		    goto getNewTape;
		}

		/* Since the dumpset on this tape will be deleted from database, check if
		 * any of the dumps in this dumpset are most-recent-dumps.
		 */
		for (dmp = oldTapeLabel.dumpid; dmp; dmp = de.appendedDumpID) {
		    if (dmp == lastDump.id) {
			memcpy(&de, &lastDump, sizeof(de));
			memcpy(&de2, &lastDump, sizeof(de2));
		    } else {
			code = bcdb_FindDumpByID(dmp, &de);
			if (code)
			    break;
			sprintf(strlevel, "%d", de.level);
			code =
			    bcdb_FindLatestDump(de.volumeSetName, strlevel,
						&de2);
			if (code)
			    continue;
		    }

		    if (de.id == de2.id) {
			if (strcmp(DUMP_TAPE_NAME, de2.name) == 0) {
			    ErrorLog(0, taskId, 0, 0,
				     "Warning: Overwriting most recent dump %s (DumpID %u)\n",
				     de.name, de.id);
			} else {
			    ErrorLog(0, taskId, 0, 0,
				     "Warning: Overwriting most recent dump of the '%s' volumeset: %s (DumpID %u)\n",
				     de.volumeSetName, de.name, de.id);
			}
		    }
		}
	    }

	    /* Otherwise, the savedb is in progress and we can't
	     * access the database (it's locked). So we rely on the 
	     * information available (and not the backup database).
	     */
	    else {
		/* Check the tape's expiration date. Use the expiration on the label */
		gettimeofday(&tp, &tzp);
		curTime = tp.tv_sec;
		if (curTime < oldTapeLabel.expirationDate) {
		    TLog(taskId, "This tape has not expired\n");
		    goto getNewTape;
		}

		/* Check if this previous-dump of the dump-in-progress is on this tape */
		if (oldTapeLabel.dumpid
		    && (oldTapeLabel.dumpid == lastDump.id)) {
		    ErrorLog(0, taskId, 0, 0,
			     "Warning: Overwriting most recent dump %s (DumpID %u)\n",
			     lastDump.name, lastDump.id);
		}

	    }
	}

	GetNewLabel(tapeInfoPtr, oldTapeLabel.pName, tapeName, &newLabel);
	newLabel.expirationDate = expires;
	newLabel.useCount = oldTapeLabel.useCount + 1;
	newLabel.dumpid = dumpid;
	newLabel.size = tapeInfoPtr->tapeSize;

	code = butm_Create(tapeInfoPtr, &newLabel, 1);	/* rewind tape */
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error,
		    "Can't label tape\n");
	    goto getNewTape;
	}

	*wroteLabel = 1;

	/* Initialize a tapeEntry for later inclusion into the database */
	listEntryPtr =
	    (struct tapeEntryList *)malloc(sizeof(struct tapeEntryList));
	if (!listEntryPtr)
	    ERROR_EXIT(TC_NOMEMORY);
	memset(listEntryPtr, 0, sizeof(struct tapeEntryList));

	/* Remember dumpid so we can delete it later */
	if ((oldTapeLabel.structVersion >= TAPE_VERSION_3)
	    && oldTapeLabel.dumpid)
	    listEntryPtr->oldDumpId = oldTapeLabel.dumpid;

	/* Fill in tape entry so we can save it later */
	strcpy(tapeEntryPtr->name, TNAME(&newLabel));
	tapeEntryPtr->flags = BUDB_TAPE_BEINGWRITTEN;
	tapeEntryPtr->written = newLabel.creationTime;
	tapeEntryPtr->expires = expires;
	tapeEntryPtr->seq = sequence;
	tapeEntryPtr->useCount = oldTapeLabel.useCount + 1;
	tapeEntryPtr->dump = dumpid;
	tapeEntryPtr->useKBytes = 0;
	tapeEntryPtr->labelpos = 0;

	/* Thread onto end of single-linked list */
	if (listEntryHead) {
	    endList = listEntryHead;
	    while (endList->next)
		endList = endList->next;
	    endList->next = listEntryPtr;
	} else
	    listEntryHead = listEntryPtr;
    }				/*w */

  error_exit:
    return (code);
}

/* freeTapeList
 *       With the list of tapes, free the structures.
 */

afs_int32
freeTapeList()
{
    struct tapeEntryList *next;

    listEntryPtr = listEntryHead;
    while (listEntryPtr) {
	next = listEntryPtr->next;
	free(listEntryPtr);
	listEntryPtr = next;
    }

    listEntryHead = NULL;
    return (0);
}

/* addTapesToDb
 *       With the list of tapes, add them to the database. 
 *       Also delete any olddumpids that are around.
 */

afs_int32
addTapesToDb(taskId)
     afs_int32 taskId;
{
    afs_int32 code = 0;
    afs_int32 i, new;
    struct tapeEntryList *next;

    listEntryPtr = listEntryHead;
    while (listEntryPtr) {
	next = listEntryPtr->next;

	/* Remove the old database entry */
	if (listEntryPtr->oldDumpId) {
	    i = bcdb_deleteDump(listEntryPtr->oldDumpId, 0, 0, 0);
	    if (i && (i != BUDB_NOENT)) {
		ErrorLog(0, taskId, i, 0,
			 "Unable to delete old DB entry %u.\n",
			 listEntryPtr->oldDumpId);
	    }
	}

	/* Add the tape to the database */
	code = bcdb_UseTape(tapeEntryPtr, &new);
	if (code) {
	    ErrorLog(0, taskId, code, 0, "Can't add tape to database: %s\n",
		     tapeEntryPtr->name);
	    ERROR_EXIT(code);
	}

	code = bcdb_FinishTape(tapeEntryPtr);
	if (code) {
	    ErrorLog(0, taskId, code, 0, "Can't finish tape: %s\n",
		     tapeEntryPtr->name);
	    ERROR_EXIT(code);
	}

	listEntryPtr = next;
    }

  error_exit:
    return (code);
}

/* writeDbDump
 * notes:
 *	this code assumes that the blocksize on reads is smaller than
 *	the blocksize on writes
 */

static
writeDbDump(tapeInfoPtr, taskId, expires, dumpid)
     struct butm_tapeInfo *tapeInfoPtr;
     afs_uint32 taskId;
     Date expires;
     afs_uint32 dumpid;
{
    afs_int32 blockSize;
    afs_int32 writeBufNbytes = 0;
    char *writeBlock = 0;
    char *writeBuffer = 0;
    char *writeBufPtr;
    afs_int32 transferSize;

    char *readBufPtr;
    afs_int32 maxReadSize;

    charListT charList;
    afs_int32 done;
    afs_int32 code;
    afs_int32 chunksize = 0;
    afs_int32 tc_EndMargin, tc_KEndMargin, kRemaining;
    int sequence;
    int wroteLabel;
    int firstcall;
#ifdef AFS_PTHREAD_ENV
    pthread_t alivePid;
    pthread_attr_t tattr;
    AFS_SIGSET_DECL;
#else
    PROCESS alivePid;
#endif

    extern struct tapeConfig globalTapeConfig;
    extern struct udbHandleS udbHandle;

    extern int KeepAlive();

    blockSize = BUTM_BLKSIZE;
    writeBlock = (char *)malloc(BUTM_BLOCKSIZE);
    if (!writeBlock)
	ERROR_EXIT(TC_NOMEMORY);

    writeBuffer = writeBlock + sizeof(struct blockMark);
    memset(writeBuffer, 0, BUTM_BLKSIZE);
    maxReadSize = 1024;

    /* 
     * The margin of space to check for end of tape is set to the 
     * amount of space used to write an end-of-tape multiplied by 2. 
     * The amount of space is size of a 16K EODump marker, its EOF
     * marker, and up to two EOF markers done on close (1 16K blocks +
     * 3 EOF * markers). 
     */
    tc_EndMargin = (16384 + 3 * globalTapeConfig.fileMarkSize) * 2;
    tc_KEndMargin = tc_EndMargin / 1024;

    /* have to write enclose the dump in file marks */
    code = butm_WriteFileBegin(tapeInfoPtr);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfoPtr->error,
		 "Can't write FileBegin on tape\n");
	ERROR_EXIT(code);
    }

    writeBufPtr = &writeBuffer[0];
    firstcall = 1;
    sequence = 1;
    charList.charListT_val = 0;
    charList.charListT_len = 0;

    while (1) {			/*w */
	/* When no data in buffer, read data from the budb_server */
	if (charList.charListT_len == 0) {
	    /* get more data. let rx allocate space */
	    if (charList.charListT_val) {
		free(charList.charListT_val);
		charList.charListT_val = 0;
	    }

	    /* get the data */
	    code =
		ubik_Call_SingleServer(BUDB_DumpDB, udbHandle.uh_client,
				       UF_SINGLESERVER, firstcall,
				       maxReadSize, &charList, &done);
	    if (code) {
		ErrorLog(0, taskId, code, 0, "Can't read database\n");
		ERROR_EXIT(code);
	    }

	    /* If this if the first call to the budb server, create a thread
	     * that will keep the connection alive (during tape changes).
	     */
	    if (firstcall) {
#ifdef AFS_PTHREAD_ENV
		code = pthread_attr_init(&tattr);
		if (code) {
		    ErrorLog(0, taskId, code, 0,
			     "Can't pthread_attr_init Keep-alive process\n");
		    ERROR_EXIT(code);
		}

		code =
		    pthread_attr_setdetachstate(&tattr,
						PTHREAD_CREATE_DETACHED);
		if (code) {
		    ErrorLog(0, taskId, code, 0,
			     "Can't pthread_attr_setdetachstate Keep-alive process\n");
		    ERROR_EXIT(code);
		}

		AFS_SIGSET_CLEAR();
		code = pthread_create(&alivePid, &tattr, KeepAlive, 0);
		AFS_SIGSET_RESTORE();
#else
		code =
		    LWP_CreateProcess(KeepAlive, 16384, 1, (void *)NULL,
				      "Keep-alive process", &alivePid);
#endif
		/* XXX should we check code here ??? XXX */
	    }
	    firstcall = 0;

	    readBufPtr = charList.charListT_val;
	}

	if ((charList.charListT_len == 0) && done)
	    break;

	/* compute how many bytes and transfer to the write Buffer */
	transferSize =
	    (charList.charListT_len <
	     (blockSize -
	      writeBufNbytes)) ? charList.charListT_len : (blockSize -
							   writeBufNbytes);

	memcpy(writeBufPtr, readBufPtr, transferSize);
	charList.charListT_len -= transferSize;
	writeBufPtr += transferSize;
	readBufPtr += transferSize;
	writeBufNbytes += transferSize;

	/* If filled the write buffer, then write it to tape */
	if (writeBufNbytes == blockSize) {
	    code = butm_WriteFileData(tapeInfoPtr, writeBuffer, 1, blockSize);
	    if (code) {
		ErrorLog(0, taskId, code, tapeInfoPtr->error,
			 "Can't write data on tape\n");
		ERROR_EXIT(code);
	    }

	    memset(writeBuffer, 0, blockSize);
	    writeBufPtr = &writeBuffer[0];
	    writeBufNbytes = 0;

	    /* Every BIGCHUNK bytes check if aborted */
	    chunksize += blockSize;
	    if (chunksize > BIGCHUNK) {
		chunksize = 0;
		if (checkAbortByTaskId(taskId))
		    ERROR_EXIT(TC_ABORTEDBYREQUEST);
	    }

	    /*
	     * check if tape is full - since we filled a blockSize worth of data
	     * assume that there is more data.
	     */
	    kRemaining = butm_remainingKSpace(tapeInfoPtr);
	    if (kRemaining < tc_KEndMargin) {
		code = butm_WriteFileEnd(tapeInfoPtr);
		if (code) {
		    ErrorLog(0, taskId, code, tapeInfoPtr->error,
			     "Can't write FileEnd on tape\n");
		    ERROR_EXIT(code);
		}

		code = butm_WriteEOT(tapeInfoPtr);
		if (code) {
		    ErrorLog(0, taskId, code, tapeInfoPtr->error,
			     "Can't write end-of-dump on tape\n");
		    ERROR_EXIT(code);
		}

		/* Mark tape as having been written */
		tapeEntryPtr->useKBytes =
		    tapeInfoPtr->kBytes + (tapeInfoPtr->nBytes ? 1 : 0);
		tapeEntryPtr->flags = BUDB_TAPE_WRITTEN;

		unmountTape(taskId, tapeInfoPtr);

		/* Get next tape and writes its label */
		sequence++;
		code =
		    GetDBTape(taskId, expires, tapeInfoPtr, dumpid, sequence,
			      1, &wroteLabel);
		if (code)
		    ERROR_EXIT(code);

		code = butm_WriteFileBegin(tapeInfoPtr);
		if (code) {
		    ErrorLog(0, taskId, code, tapeInfoPtr->error,
			     "Can't write FileBegin on tape\n");
		    ERROR_EXIT(code);
		}
	    }
	}
    }				/*w */

    /* no more data to be read - if necessary, flush out the last buffer */
    if (writeBufNbytes > 0) {
	code = butm_WriteFileData(tapeInfoPtr, writeBuffer, 1, blockSize);
	if (code) {
	    ErrorLog(1, taskId, code, tapeInfoPtr->error,
		     "Can't write data on tape\n");
	    ERROR_EXIT(code);
	}
    }

    code = butm_WriteFileEnd(tapeInfoPtr);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfoPtr->error,
		 "Can't write FileEnd on tape\n");
	ERROR_EXIT(code);
    }

    /* Mark tape as having been written */
    tapeEntryPtr->useKBytes =
	tapeInfoPtr->kBytes + (tapeInfoPtr->nBytes ? 1 : 0);
    tapeEntryPtr->flags = BUDB_TAPE_WRITTEN;

  error_exit:
    /* Let the KeepAlive process stop on its own */
    code =
	ubik_Call_SingleServer(BUDB_DumpDB, udbHandle.uh_client,
			       UF_END_SINGLESERVER, 0);

    if (writeBlock)
	free(writeBlock);
    if (charList.charListT_val)
	free(charList.charListT_val);
    return (code);
}

/* saveDbToTape
 *	dump backup database to tape
 */

afs_int32
saveDbToTape(saveDbIfPtr)
     struct saveDbIf *saveDbIfPtr;
{
    afs_int32 code = 0;
    afs_int32 i;
    int wroteLabel;
    afs_uint32 taskId;
    Date expires;

    struct butm_tapeInfo tapeInfo;
    struct budb_dumpEntry dumpEntry;

    extern struct deviceSyncNode *deviceLatch;
    extern struct tapeConfig globalTapeConfig;

    expires = (saveDbIfPtr->archiveTime ? NEVERDATE : 0);
    taskId = saveDbIfPtr->taskId;

    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);	/* lock tape device */
    clearStatus(taskId, DRIVE_WAIT);

    printf("\n\n");
    TLog(taskId, "SaveDb\n");

    tapeInfo.structVersion = BUTM_MAJORVERSION;
    code = butm_file_Instantiate(&tapeInfo, &globalTapeConfig);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfo.error,
		 "Can't initialize tape module\n");
	ERROR_EXIT(code);
    }

    /* Determine what the last database dump was */
    memset(&lastDump, 0, sizeof(lastDump));
    code = bcdb_FindLatestDump("", "", &lastDump);
    if (code) {
	if (code != BUDB_NODUMPNAME) {
	    ErrorLog(0, taskId, code, 0, "Can't read backup database\n");
	    ERROR_EXIT(code);
	}
	memset(&lastDump, 0, sizeof(lastDump));
    }

    code = CreateDBDump(&dumpEntry);	/* Create a dump for this tape */
    if (code) {
	ErrorLog(0, taskId, code, 0, "Can't create dump in database\n");
	ERROR_EXIT(code);
    }


    listEntryHead = NULL;

    /* Get the tape and write a new label to it */
    code =
	GetDBTape(taskId, expires, &tapeInfo, dumpEntry.id, 1, autoQuery,
		  &wroteLabel);

    /*
     * If did not write the label, remove created dump 
     * Else if wrote the label, remove old dump from db so it's not saved.
     */
    if (!wroteLabel) {
	i = bcdb_deleteDump(dumpEntry.id, 0, 0, 0);
	dumpEntry.id = 0;
	if (i && (i != BUDB_NOENT))
	    ErrorLog(0, taskId, i, 0, "Unable to delete DB entry %u.\n",
		     dumpEntry.id);
    } else if (listEntryHead->oldDumpId) {
	i = bcdb_deleteDump(listEntryHead->oldDumpId, 0, 0, 0);
	listEntryHead->oldDumpId = 0;
	if (i && (i != BUDB_NOENT)) {
	    ErrorLog(0, taskId, i, 0, "Unable to delete old DB entry %u.\n",
		     listEntryHead->oldDumpId);
	    ERROR_EXIT(i);
	}
    }
    if (code)
	ERROR_EXIT(code);

    TapeLog(1, taskId, 0, 0, "Tape accepted - now dumping database\n");

    /* we have a writable tape */
    code = writeDbDump(&tapeInfo, taskId, expires, dumpEntry.id);
    if (code)
	ERROR_EXIT(code);

    /* Now delete the entries between time 0 and archive-time */
    if (saveDbIfPtr->archiveTime)
	code = bcdb_deleteDump(0, 0, saveDbIfPtr->archiveTime, 0);

  error_exit:
    unmountTape(taskId, &tapeInfo);

    /* Add this dump's tapes to the database and mark it finished */
    if (dumpEntry.id) {
	i = addTapesToDb(taskId);
	if (!code)
	    code = i;

	i = bcdb_FinishDump(&dumpEntry);
	if (!code)
	    code = i;
    }
    freeTapeList();

    if (code == TC_ABORTEDBYREQUEST) {
	TLog(taskId, "SaveDb: Aborted by request\n");
	clearStatus(taskId, ABORT_REQUEST);
	setStatus(taskId, ABORT_DONE);
    } else if (code) {
	TapeLog(0, taskId, code, 0, "SaveDb: Finished with errors\n");
	setStatus(taskId, TASK_ERROR);
    } else {
	TLog(taskId, "SaveDb: Finished\n");
    }
    setStatus(taskId, TASK_DONE);

    free(saveDbIfPtr);
    LeaveDeviceQueue(deviceLatch);
    return (code);
}

struct rstTapeInfo {
    afs_int32 taskId;
    afs_int32 tapeSeq;
    afs_uint32 dumpid;
};

/* makeDbDumpEntry()
 *      Make a database dump entry given a tape label.
 */

afs_int32
makeDbDumpEntry(tapeEntPtr, dumpEntryPtr)
     struct budb_tapeEntry *tapeEntPtr;
     struct budb_dumpEntry *dumpEntryPtr;
{
    memset(dumpEntryPtr, 0, sizeof(struct budb_dumpEntry));

    dumpEntryPtr->id = tapeEntPtr->dump;
    dumpEntryPtr->initialDumpID = 0;
    dumpEntryPtr->parent = 0;
    dumpEntryPtr->level = 0;
    dumpEntryPtr->flags = 0;

    strcpy(dumpEntryPtr->volumeSetName, "");
    strcpy(dumpEntryPtr->dumpPath, "");
    strcpy(dumpEntryPtr->name, DUMP_TAPE_NAME);

    dumpEntryPtr->created = tapeEntPtr->dump;
    dumpEntryPtr->incTime = 0;
    dumpEntryPtr->nVolumes = 0;

    strcpy(dumpEntryPtr->tapes.format, DUMP_TAPE_NAME);
    strcat(dumpEntryPtr->tapes.format, ".%d");
    dumpEntryPtr->tapes.b = tapeEntPtr->seq;
    dumpEntryPtr->tapes.maxTapes = 0;
    return 0;
}

/* readDbTape
 *      prompt for a specific database tape
 */

afs_int32
readDbTape(tapeInfoPtr, rstTapeInfoPtr, query)
     struct butm_tapeInfo *tapeInfoPtr;
     struct rstTapeInfo *rstTapeInfoPtr;
     int query;
{
    afs_int32 code = 0;
    int interactiveFlag;
    afs_int32 taskId;
    struct butm_tapeLabel oldTapeLabel;
    char AFStapeName[BU_MAXTAPELEN], tapeName[BU_MAXTAPELEN];
    struct tapeEntryList *endList;
    int tapecount = 1;
    struct budb_dumpEntry de;
    struct budb_tapeEntry te;

    taskId = rstTapeInfoPtr->taskId;
    interactiveFlag = query;

    /* construct the name of the tape */
    sprintf(AFStapeName, "%s.%-d", DUMP_TAPE_NAME, rstTapeInfoPtr->tapeSeq);
    strcpy(tapeName, AFStapeName);

    /* Will prompt for the latest saved database tape, but will accept any one */
    if (rstTapeInfoPtr->tapeSeq == 1) {
	code = bcdb_FindLatestDump("", "", &de);
	if (!code)
	    rstTapeInfoPtr->dumpid = de.id;
    }
    if (rstTapeInfoPtr->dumpid) {
	code =
	    bcdb_FindTapeSeq(rstTapeInfoPtr->dumpid, rstTapeInfoPtr->tapeSeq,
			     &te);
	if (!code)
	    strcpy(tapeName, te.name);
    }
    code = 0;

    while (1) {			/*w */
	if (interactiveFlag) {	/* need a tape to read */
	    code =
		PromptForTape(RESTOREDBOPCODE, tapeName,
			      rstTapeInfoPtr->dumpid, taskId, tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	interactiveFlag = 1;
	tapecount++;

	code = butm_Mount(tapeInfoPtr, tapeName);
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error, "Can't open tape\n");
	    goto getNewTape;
	}

	code = butm_ReadLabel(tapeInfoPtr, &oldTapeLabel, 1);	/* will rewind the tape */
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error,
		    "Can't read tape label\n");
	    goto getNewTape;
	}

	/* Check for name of tape and matching dump id (if applicable). */
	if ((strcmp(oldTapeLabel.AFSName, AFStapeName) != 0)
	    || ((rstTapeInfoPtr->tapeSeq != 1)
		&& (oldTapeLabel.dumpid != rstTapeInfoPtr->dumpid))) {
	    char expTape[BU_MAXTAPELEN + 32];
	    char gotTape[BU_MAXTAPELEN + 32];

	    TAPENAME(expTape, tapeName, rstTapeInfoPtr->dumpid);
	    TAPENAME(gotTape, oldTapeLabel.AFSName, oldTapeLabel.dumpid);

	    TLog(taskId, "Tape label expected %s, label seen %s\n", expTape,
		 gotTape);
	    goto getNewTape;
	}

	if (rstTapeInfoPtr->tapeSeq == 1)	/* Remember this dumpId */
	    rstTapeInfoPtr->dumpid = oldTapeLabel.dumpid;

	break;

      getNewTape:
	unmountTape(taskId, tapeInfoPtr);
    }				/*w */


    /* Initialize a tapeEntry for later inclusion into the database */
    listEntryPtr =
	(struct tapeEntryList *)malloc(sizeof(struct tapeEntryList));
    if (!listEntryPtr)
	ERROR_EXIT(TC_NOMEMORY);
    memset(listEntryPtr, 0, sizeof(struct tapeEntryList));

    /* Fill in tape entry so we can save it later */
    strcpy(tapeEntryPtr->name, TNAME(&oldTapeLabel));
    tapeEntryPtr->dump = oldTapeLabel.dumpid;
    tapeEntryPtr->flags = BUDB_TAPE_BEINGWRITTEN;
    tapeEntryPtr->written = oldTapeLabel.creationTime;
    tapeEntryPtr->expires = oldTapeLabel.expirationDate;
    tapeEntryPtr->seq = extractTapeSeq(oldTapeLabel.AFSName);
    tapeEntryPtr->useCount = oldTapeLabel.useCount;
    tapeEntryPtr->useKBytes = 0;
    tapeEntryPtr->labelpos = 0;

    /* Thread onto end of single-linked list */
    if (listEntryHead) {
	endList = listEntryHead;
	while (endList->next)
	    endList = endList->next;
	endList->next = listEntryPtr;
    } else
	listEntryHead = listEntryPtr;

  error_exit:
    return (code);
}

static afs_int32 nbytes = 0;	/* # bytes left in buffer */
static
initTapeBuffering()
{
    nbytes = 0;
}


/* restoreDbEntries
 *	restore all the items on the tape
 * entry:
 *	tape positioned after tape label
 */

static
restoreDbEntries(tapeInfoPtr, rstTapeInfoPtr)
     struct butm_tapeInfo *tapeInfoPtr;
     struct rstTapeInfo *rstTapeInfoPtr;
{
    struct structDumpHeader netItemHeader, hostItemHeader;
    afs_int32 more = 1;
    afs_int32 taskId, code = 0;
    int count = 0;

    taskId = rstTapeInfoPtr->taskId;

    /* clear state for the buffer routine(s) */
    initTapeBuffering();

    code = butm_ReadFileBegin(tapeInfoPtr);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfoPtr->error,
		 "Can't read FileBegin on tape\n");
	ERROR_EXIT(code);
    }

    /* get the first item-header */
    memset(&netItemHeader, 0, sizeof(netItemHeader));
    code =
	getTapeData(tapeInfoPtr, rstTapeInfoPtr, &netItemHeader,
		    sizeof(netItemHeader));
    if (code)
	ERROR_EXIT(code);
    structDumpHeader_ntoh(&netItemHeader, &hostItemHeader);

    while (more) {
	switch (hostItemHeader.type) {
	case SD_DBHEADER:
	    code =
		restoreDbHeader(tapeInfoPtr, rstTapeInfoPtr, &hostItemHeader);
	    if (code)
		ERROR_EXIT(code);
	    break;

	case SD_DUMP:
	    if (++count > 25) {	/*every 25 dumps, wait */
		waitDbWatcher();
		count = 0;
	    }
	    code =
		restoreDbDump(tapeInfoPtr, rstTapeInfoPtr, &hostItemHeader);
	    if (code)
		ERROR_EXIT(code);
	    break;

	case SD_TAPE:
	case SD_VOLUME:
	    ERROR_EXIT(-1);
	    break;

	case SD_TEXT_DUMPSCHEDULE:
	case SD_TEXT_VOLUMESET:
	case SD_TEXT_TAPEHOSTS:
	    code = restoreText(tapeInfoPtr, rstTapeInfoPtr, &hostItemHeader);
	    if (code)
		ERROR_EXIT(code);
	    break;

	case SD_END:
	    more = 0;
	    break;

	default:
	    TLog(taskId, "Unknown database header type %d\n",
		 hostItemHeader.type);
	    ERROR_EXIT(-1);
	    break;
	}
    }

    code = butm_ReadFileEnd(tapeInfoPtr);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfoPtr->error,
		 "Can't read EOF on tape\n");
	ERROR_EXIT(code);
    }

    /* Mark tape as having been written */
    tapeEntryPtr->useKBytes =
	tapeInfoPtr->kBytes + (tapeInfoPtr->nBytes ? 1 : 0);
    tapeEntryPtr->flags = BUDB_TAPE_WRITTEN;

  error_exit:
    return (code);
}

/* restoreDbFromTape
 *	restore the backup database from tape.
 */

afs_int32
restoreDbFromTape(taskId)
     afs_uint32 taskId;
{
    afs_int32 code = 0;
    afs_int32 i;
    struct butm_tapeInfo tapeInfo;
    struct rstTapeInfo rstTapeInfo;
    struct budb_dumpEntry dumpEntry;

    extern struct tapeConfig globalTapeConfig;
    extern struct deviceSyncNode *deviceLatch;

    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);	/* lock tape device */
    clearStatus(taskId, DRIVE_WAIT);

    printf("\n\n");
    TLog(taskId, "RestoreDb\n");

    tapeInfo.structVersion = BUTM_MAJORVERSION;
    code = butm_file_Instantiate(&tapeInfo, &globalTapeConfig);
    if (code) {
	ErrorLog(0, taskId, code, tapeInfo.error,
		 "Can't initialize tape module\n");
	ERROR_EXIT(code);
    }

    listEntryHead = NULL;

    rstTapeInfo.taskId = taskId;
    rstTapeInfo.tapeSeq = 1;
    rstTapeInfo.dumpid = 0;

    code = readDbTape(&tapeInfo, &rstTapeInfo, autoQuery);
    if (code)
	ERROR_EXIT(code);

    code = restoreDbEntries(&tapeInfo, &rstTapeInfo);
    if (code)
	ERROR_EXIT(code);

  error_exit:
    /* Now put this dump into the database */
    /* Make a dump entry from first tape   */
    listEntryPtr = listEntryHead;
    if (listEntryPtr) {
	makeDbDumpEntry(tapeEntryPtr, &dumpEntry);
	if (dumpEntry.id != 0) {
	    i = bcdb_CreateDump(&dumpEntry);
	    if (i) {
		if (i == BUDB_DUMPIDEXISTS)
		    fprintf(stderr,
			    "Dump id %d not added to database - already exists\n",
			    dumpEntry.id);
		else
		    TapeLog(0, taskId, i, 0,
			    "Dump id %d not added to database\n",
			    dumpEntry.id);
	    } else {
		i = addTapesToDb(taskId);
		if (!code)
		    code = i;

		i = bcdb_FinishDump(&dumpEntry);
		if (!code)
		    code = i;
	    }
	}
	freeTapeList();
    }

    unmountTape(taskId, &tapeInfo);
    waitDbWatcher();

    if (code == TC_ABORTEDBYREQUEST) {
	TLog(taskId, "RestoreDb: Aborted by request\n");
	clearStatus(taskId, ABORT_REQUEST);
	setStatus(taskId, ABORT_DONE);
    } else if (code) {
	TapeLog(0, taskId, code, 0, "RestoreDb: Finished with errors\n");
	setStatus(taskId, TASK_ERROR);
    } else {
	TLog(taskId, "RestoreDb: Finished\n");
    }

    LeaveDeviceQueue(deviceLatch);
    setStatus(taskId, TASK_DONE);

    return (code);
}

/* KeepAlive
 * 
 *      While dumping the database, keeps the connection alive.  
 *      Every 10 seconds, wake up and ask to read 0 bytes of the database.
 *      This resets the database's internal timer so that it does not 
 *      prematuraly quit (on asking for new tapes and such).
 *      
 *      Use the same udbHandle as writeDbDump so we go to the same server.
 */
int
KeepAlive()
{
    charListT charList;
    afs_int32 code;
    afs_int32 done;

    extern struct udbHandleS udbHandle;

    while (1) {
#ifdef AFS_PTHREAD_ENV
	sleep(5);
#else
	IOMGR_Sleep(5);
#endif
	charList.charListT_val = 0;
	charList.charListT_len = 0;
	code =
	    ubik_Call_SingleServer(BUDB_DumpDB, udbHandle.uh_client,
				   UF_SINGLESERVER, 0, 0, &charList, &done);
	if (code || done)
	    break;
    }
    return 0;
}


/* restoreDbHeader
 *	restore special items in the header
 */

restoreDbHeader(tapeInfo, rstTapeInfoPtr, nextHeader)
     struct butm_tapeInfo *tapeInfo;
     struct rstTapeInfo *rstTapeInfoPtr;
     struct structDumpHeader *nextHeader;
{
    struct structDumpHeader netItemHeader;
    struct DbHeader netDbHeader, hostDbHeader;
    afs_int32 code = 0;

    extern struct udbHandleS udbHandle;

    /* Read the database header */
    memset(&netDbHeader, 0, sizeof(netDbHeader));
    code =
	getTapeData(tapeInfo, rstTapeInfoPtr, &netDbHeader,
		    sizeof(netDbHeader));
    if (code)
	ERROR_EXIT(code);
    DbHeader_ntoh(&netDbHeader, &hostDbHeader);

    /* Add the database header to the database */
    code =
	ubik_BUDB_RestoreDbHeader(udbHandle.uh_client, 0,
		  &hostDbHeader);
    if (code) {
	ErrorLog(0, rstTapeInfoPtr->taskId, code, 0,
		 "Can't restore DB Header\n");
	ERROR_EXIT(code);
    }

    /* get the next item-header */
    memset(nextHeader, 0, sizeof(*nextHeader));
    code =
	getTapeData(tapeInfo, rstTapeInfoPtr, &netItemHeader,
		    sizeof(netItemHeader));
    if (code)
	ERROR_EXIT(code);
    structDumpHeader_ntoh(&netItemHeader, nextHeader);

  error_exit:
    return (code);
}


/* restoreDbDump
 *	restore a single dump, including all its tapes and volumes, from
 *	the tape.
 * entry:
 *	nextHeader - ptr to structure for return value
 * exit:
 *	nextHeader - next structure header from tape
 * notes: 
 *	upon entry, the dump structure header has been read confirming that
 *	a database dump tree exists on the tape
 */

restoreDbDump(tapeInfo, rstTapeInfoPtr, nextHeader)
     struct butm_tapeInfo *tapeInfo;
     struct rstTapeInfo *rstTapeInfoPtr;
     struct structDumpHeader *nextHeader;
{
    struct budb_dumpEntry netDumpEntry, hostDumpEntry;
    struct budb_tapeEntry netTapeEntry, hostTapeEntry;
    struct budb_volumeEntry netVolumeEntry, hostVolumeEntry;
    struct structDumpHeader netItemHeader;
    afs_int32 taskId;
    int restoreThisDump = 1;
    afs_int32 code = 0;

    extern struct udbHandleS udbHandle;

    taskId = rstTapeInfoPtr->taskId;

    /* read dump entry */
    memset(&netDumpEntry, 0, sizeof(netDumpEntry));
    code =
	getTapeData(tapeInfo, rstTapeInfoPtr, &netDumpEntry,
		    sizeof(netDumpEntry));
    if (code)
	ERROR_EXIT(code);

    /* If database tape does not have a dumpid (AFS 3.3) then no initial/appended dumps */
    if (rstTapeInfoPtr->dumpid == 0) {
	netDumpEntry.initialDumpID = 0;
	netDumpEntry.appendedDumpID = 0;
    }

    dumpEntry_ntoh(&netDumpEntry, &hostDumpEntry);

    /* The dump entry for this database tape is incomplete, so don't include it */
    if (hostDumpEntry.id == rstTapeInfoPtr->dumpid)
	restoreThisDump = 0;

    /* add the dump to the database */
    if (restoreThisDump) {
	code =
	    threadEntryDir(&hostDumpEntry, sizeof(hostDumpEntry),
			   DLQ_USEDUMP);
	if (code)
	    ERROR_EXIT(code);
    }

    /* get the next item-header */
    memset(nextHeader, 0, sizeof(*nextHeader));
    code =
	getTapeData(tapeInfo, rstTapeInfoPtr, &netItemHeader,
		    sizeof(netItemHeader));
    if (code)
	ERROR_EXIT(code);
    structDumpHeader_ntoh(&netItemHeader, nextHeader);

    /* Add every tape to the db */
    while (nextHeader->type == SD_TAPE) {	/*t */

	/* read the tape entry */
	memset(&netTapeEntry, 0, sizeof(netTapeEntry));
	code =
	    getTapeData(tapeInfo, rstTapeInfoPtr, &netTapeEntry,
			sizeof(netTapeEntry));
	if (code)
	    ERROR_EXIT(code);
	tapeEntry_ntoh(&netTapeEntry, &hostTapeEntry);

	/* Add the tape to the database */
	if (restoreThisDump) {
	    code =
		threadEntryDir(&hostTapeEntry, sizeof(hostTapeEntry),
			       DLQ_USETAPE);
	    if (code)
		ERROR_EXIT(code);
	}

	/* get the next item-header */
	memset(nextHeader, 0, sizeof(*nextHeader));
	code =
	    getTapeData(tapeInfo, rstTapeInfoPtr, &netItemHeader,
			sizeof(netItemHeader));
	if (code)
	    ERROR_EXIT(code);
	structDumpHeader_ntoh(&netItemHeader, nextHeader);

	/* Add every volume to the db */
	while (nextHeader->type == SD_VOLUME) {	/*v */

	    /* read the volume entry */
	    memset(&netVolumeEntry, 0, sizeof(netVolumeEntry));
	    code =
		getTapeData(tapeInfo, rstTapeInfoPtr, &netVolumeEntry,
			    sizeof(netVolumeEntry));
	    if (code)
		ERROR_EXIT(code);
	    volumeEntry_ntoh(&netVolumeEntry, &hostVolumeEntry);

	    if (restoreThisDump) {
		code =
		    threadEntryDir(&hostVolumeEntry, sizeof(hostVolumeEntry),
				   DLQ_VOLENTRY);
		if (code)
		    ERROR_EXIT(code);
	    }

	    /* get the next item-header */
	    memset(nextHeader, 0, sizeof(*nextHeader));
	    code =
		getTapeData(tapeInfo, rstTapeInfoPtr, &netItemHeader,
			    sizeof(netItemHeader));
	    if (code)
		ERROR_EXIT(code);
	    structDumpHeader_ntoh(&netItemHeader, nextHeader);
	}			/*v */

	/* Finish the tape */
	if (restoreThisDump) {
	    code =
		threadEntryDir(&hostTapeEntry, sizeof(hostTapeEntry),
			       DLQ_FINISHTAPE);
	    if (code)
		ERROR_EXIT(code);
	}
    }				/*t */

    /* Finish the dump */
    if (restoreThisDump) {
	code =
	    threadEntryDir(&hostDumpEntry, sizeof(hostDumpEntry),
			   DLQ_FINISHDUMP);
	if (code)
	    ERROR_EXIT(code);
    }

  error_exit:
    return (code);
}

/* saveTextFile
 *	Save the specified file as configuration text in the ubik database.
 *	Have to setup the client text structure so that we can call
 *	the routine to transmit the text to the db.
 */

afs_int32
saveTextFile(taskId, textType, fileName)
     afs_int32 taskId;
     afs_int32 textType;
     char *fileName;
{
    udbClientTextP ctPtr = 0;
    afs_int32 code = 0;
    int tlock = 0;

    ctPtr = (udbClientTextP) malloc(sizeof(*ctPtr));
    if (!ctPtr)
	ERROR_EXIT(TC_NOMEMORY);

    memset(ctPtr, 0, sizeof(*ctPtr));
    ctPtr->textType = textType;

    /* lock the text in the database */
    code = bc_LockText(ctPtr);
    if (code) {
	ErrorLog(0, taskId, code, 0, "Can't lock text file\n");
	ERROR_EXIT(code);
    }
    tlock = 1;

    ctPtr->textStream = fopen(fileName, "r");
    if (!ctPtr->textStream) {
	ErrorLog(0, taskId, errno, 0, "Can't open text file\n");
	ERROR_EXIT(errno);
    }

    /* now send the text to the database */
    code = bcdb_SaveTextFile(ctPtr);
    if (code) {
	ErrorLog(0, taskId, code, 0, "Can't save text file\n");
	ERROR_EXIT(code);
    }

  error_exit:
    if (ctPtr) {
	if (ctPtr->textStream)
	    fclose(ctPtr->textStream);
	if (tlock)
	    bc_UnlockText(ctPtr);
	free(ctPtr);
    }
    return (code);
}

/* restoreText
 *	read the text off the tape, and store it in the appropriate
 *	text type in the database.
 * entry:
 *	nextHeader - ptr to struct for return information
 * exit:
 * 	nextHeader - struct header for next item on the tape
 */

restoreText(tapeInfo, rstTapeInfoPtr, nextHeader)
     struct butm_tapeInfo *tapeInfo;
     struct rstTapeInfo *rstTapeInfoPtr;
     struct structDumpHeader *nextHeader;
{
    char filename[64];
    afs_int32 nbytes;
    char *readBuffer = 0;
    afs_int32 readBlockSize;
    afs_int32 transferSize;
    struct structDumpHeader netItemHeader;
    int fid = -1;
    afs_int32 code = 0;

    udbClientTextP ctPtr = 0;
    afs_int32 textType;

    ctPtr = (udbClientTextP) malloc(sizeof(*ctPtr));
    if (!ctPtr)
	ERROR_EXIT(TC_NOMEMORY);

    /* determine the type of text block */
    switch (nextHeader->type) {
    case SD_TEXT_DUMPSCHEDULE:
	textType = TB_DUMPSCHEDULE;
	break;

    case SD_TEXT_VOLUMESET:
	textType = TB_VOLUMESET;
	break;

    case SD_TEXT_TAPEHOSTS:
	textType = TB_TAPEHOSTS;
	break;

    default:
	ErrorLog(0, rstTapeInfoPtr->taskId, TC_INTERNALERROR, 0,
		 "Unknown text block\n");
	ERROR_EXIT(TC_INTERNALERROR);
	break;
    }

    /* open the text file */
    sprintf(filename, "%s/bu_XXXXXX", gettmpdir());
#if defined (HAVE_MKSTEMP)
    fid = mkstemp(filename);
#else
    fid = open(mktemp(filename), O_RDWR | O_CREAT | O_EXCL, 0600);
#endif
    if (fid < 0) {
	ErrorLog(0, rstTapeInfoPtr->taskId, errno, 0,
		 "Can't open temporary text file: %s\n", filename);
	ERROR_EXIT(errno);
    }

    /* allocate buffer for text */
    readBlockSize = BUTM_BLKSIZE;
    readBuffer = (char *)malloc(readBlockSize);
    if (!readBuffer)
	ERROR_EXIT(TC_NOMEMORY);

    /* read the text into the temporary file */
    nbytes = nextHeader->size;
    while (nbytes > 0) {
	transferSize = (readBlockSize < nbytes) ? readBlockSize : nbytes;

	/* read it from the tape */
	code =
	    getTapeData(tapeInfo, rstTapeInfoPtr, readBuffer, transferSize);
	if (code)
	    ERROR_EXIT(code);

	/* write to the file */
	if (write(fid, readBuffer, transferSize) != transferSize) {
	    ErrorLog(0, rstTapeInfoPtr->taskId, errno, 0,
		     "Can't write temporary text file: %s\n", filename);
	    ERROR_EXIT(errno);
	}

	nbytes -= transferSize;
    }

    close(fid);
    fid = -1;
    code = saveTextFile(rstTapeInfoPtr->taskId, textType, filename);
    if (code)
	ERROR_EXIT(code);
    unlink(filename);

    /* get the next item-header */
    memset(nextHeader, 0, sizeof(*nextHeader));
    code =
	getTapeData(tapeInfo, rstTapeInfoPtr, &netItemHeader,
		    sizeof(netItemHeader));
    if (code)
	ERROR_EXIT(code);
    structDumpHeader_ntoh(&netItemHeader, nextHeader);

  error_exit:
    if (ctPtr)
	free(ctPtr);
    if (readBuffer)
	free(readBuffer);
    if (fid != -1) {
	close(fid);
	unlink(filename);
    }
    return (code);
}


/* ----------------------------------
 * Tape data buffering - for reading database dumps
 * ----------------------------------
 */

static char *tapeReadBuffer = 0;	/* input buffer */
static char *tapeReadBufferPtr = 0;	/* position in buffer */

/* getTapeData
 *	Read information from tape, and place the requested number of bytes
 *	in the buffer supplied 
 * entry:
 *	tapeInfo
 *	rstTapeInfoPtr - Info about the dump being restored.
 *	buffer - buffer for requested data
 *	requestedBytes - no. of bytes requested
 * exit:
 *	fn retn - 0, ok, n, error
 */

getTapeData(tapeInfoPtr, rstTapeInfoPtr, buffer, requestedBytes)
     struct butm_tapeInfo *tapeInfoPtr;
     struct rstTapeInfo *rstTapeInfoPtr;
     char *buffer;
     afs_int32 requestedBytes;
{
    afs_int32 taskId, transferBytes, new;
    afs_int32 code = 0;
    afs_uint32 dumpid;

    taskId = rstTapeInfoPtr->taskId;

    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    if (!tapeReadBuffer) {
	tapeReadBuffer = (char *)malloc(BUTM_BLOCKSIZE);
	if (!tapeReadBuffer)
	    ERROR_EXIT(TC_NOMEMORY);
    }

    while (requestedBytes > 0) {
	if (nbytes == 0) {
	    tapeReadBufferPtr = &tapeReadBuffer[sizeof(struct blockMark)];

	    /* get more data */
	    code =
		butm_ReadFileData(tapeInfoPtr, tapeReadBufferPtr,
				  BUTM_BLKSIZE, &nbytes);
	    if (code) {
		/* detect if we hit the end-of-tape and get next tape */
		if (code == BUTM_ENDVOLUME) {
		    /* Update fields in tape entry for this tape */
		    tapeEntryPtr->flags = BUDB_TAPE_WRITTEN;
		    tapeEntryPtr->useKBytes =
			tapeInfoPtr->kBytes + (tapeInfoPtr->nBytes ? 1 : 0);

		    unmountTape(taskId, tapeInfoPtr);

		    rstTapeInfoPtr->tapeSeq++;
		    code = readDbTape(tapeInfoPtr, rstTapeInfoPtr, 1);
		    if (code)
			ERROR_EXIT(code);

		    code = butm_ReadFileBegin(tapeInfoPtr);
		    if (code) {
			ErrorLog(0, taskId, code, tapeInfoPtr->error,
				 "Can't read FileBegin on tape\n");
			ERROR_EXIT(code);
		    }

		    continue;
		}

		ErrorLog(0, taskId, code, tapeInfoPtr->error,
			 "Can't read FileData on tape\n");
		ERROR_EXIT(code);
	    }
	}

	/* copy out data */
	transferBytes = (nbytes < requestedBytes) ? nbytes : requestedBytes;
	memcpy(buffer, tapeReadBufferPtr, transferBytes);
	tapeReadBufferPtr += transferBytes;
	buffer += transferBytes;
	nbytes -= transferBytes;
	requestedBytes -= transferBytes;
    }

  error_exit:
    return (code);
}
