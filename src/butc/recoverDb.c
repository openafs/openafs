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

#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <sys/time.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#endif
#include <sys/types.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <lwp.h>
#include <lock.h>
#include <afs/tcdata.h>
#include <afs/bubasics.h>
#include <afs/budb.h>
#include "error_macros.h"

#define BELLCHAR 7		/* ascii for bell */

/* GLOBAL CONFIGURATION PARAMETERS */
extern int autoQuery;
extern int queryoperator;

/* Handle for the information read from all the tapes of a dump */
afs_int32 tapepos;		/* when read a label, remember its position */
struct tapeScanInfo {
    struct butm_tapeLabel tapeLabel, dumpLabel;
    struct budb_dumpEntry dumpEntry;
    afs_int32 initialDumpId;
    int addDbFlag;
};

extern struct tapeConfig globalTapeConfig;
extern struct deviceSyncNode *deviceLatch;

/* PrintDumpLabel
 *	print out the tape (dump) label.
 */
void
PrintDumpLabel(struct butm_tapeLabel *labelptr)
{
    char tapeName[BU_MAXTAPELEN + 32];
    time_t t;

    printf("Dump label\n");
    printf("----------\n");
    TAPENAME(tapeName, labelptr->pName, labelptr->dumpid);
    printf("permanent tape name = %s\n", tapeName);
    TAPENAME(tapeName, labelptr->AFSName, labelptr->dumpid);
    printf("AFS tape name = %s\n", tapeName);
    t = labelptr->creationTime;
    printf("creationTime = %s", ctime(&t));
    if (labelptr->expirationDate) {
        t = labelptr->expirationDate;
	printf("expirationDate = %s", cTIME(&t));
    }
    printf("cell = %s\n", labelptr->cell);
    printf("size = %u Kbytes\n", labelptr->size);
    printf("dump path = %s\n", labelptr->dumpPath);

    if (labelptr->structVersion >= TAPE_VERSION_3) {
	printf("dump id = %u\n", labelptr->dumpid);
	printf("useCount = %d\n", labelptr->useCount);
    }
    printf("-- End of dump label --\n\n");
}

/* PrintVolumeHeader
 *	print the contents of a volume header. 
 */
static void
PrintVolumeHeader(struct volumeHeader *volHeader)
{
    time_t t;

    printf("-- volume --\n");
    printf("volume name: %s\n", volHeader->volumeName);
    printf("volume ID %d\n", volHeader->volumeID);
    /* printf("server %d\n", volHeader->server); */
    printf("dumpSetName: %s\n", volHeader->dumpSetName);
    printf("dumpID %d\n", volHeader->dumpID);
    printf("level %d\n", volHeader->level);
    printf("parentID %d\n", volHeader->parentID);
    printf("endTime %d\n", volHeader->endTime);
    /* printf("versionflags %d\n", volHeader->versionflags); */
    t = volHeader->cloneDate;
    printf("clonedate %s\n", ctime(&t));
}

/* Ask
 *	ask a question. returns true or false
 * exit:
 *	1 - yes
 *	0 - no
 */

afs_int32
Ask(char *st)
{
    int response;

    while (1) {
	FFlushInput();
	putchar(BELLCHAR);
	printf("%s? (y/n) ", st);
	fflush(stdout);
	response = getchar();
	if (response == 'y')
	    return (1);
	else if (response == 'n' || response == EOF)
	    return (0);
	printf("please answer y/n\n");
    }
}

/* scanVolData
 *	Skips the volume data on the tape. The end of the volume data is
 *	detected by the presence of the volume trailer or by an EOF indication
 *	from butm. This algorithm should be replaced by one that always
 *	detects based on the volume trailer, returning the trailer to the
 *	caller. This is of course more painful.
 * entry:
 *	curTapePtr - tape info structure
 *	Tape must be positioned after volume header
 * exit:
 *	0 - success
 *	3 - empty volume, requires special handling
 *
 *	Tape positioned after data, but before file end marker block.
 *	In case of errors, positioned after the error causing block
 */
#define BIGCHUNK 102400

static int
scanVolData(afs_int32 taskId, struct butm_tapeInfo *curTapePtr, afs_int32 tapeVersion, struct volumeHeader *volumeHeader, struct volumeHeader *volumeTrailer, afs_uint32 *bytesRead)
{
    afs_int32 headBytes, tailBytes;
    char *block = NULL;
    char *buffer[2];
    int hasdata[2], curr, prev;
    afs_uint32 chunkSize = 0;
    afs_int32 nbytes;
    afs_int32 code = 0;
    afs_int32 rcode, tcode;

    memset(volumeHeader, 0, sizeof(struct volumeHeader));

    block = (char *)malloc(2 * BUTM_BLOCKSIZE);
    if (!block)
	return (TC_NOMEMORY);
    buffer[0] = &block[sizeof(struct blockMark)];
    buffer[1] = &block[BUTM_BLOCKSIZE + sizeof(struct blockMark)];
    hasdata[0] = hasdata[1] = 0;
    curr = 0;

    tcode = NextFile(curTapePtr);	/* guarantees we are at a filemark */
    if (tcode)
	ERROR_EXIT(tcode)

	    /* Read the FileBegin FileMark */
	    code = butm_ReadFileBegin(curTapePtr);
    if (code) {
	/*
	 * Tapes made with 3.0 have no software EOT markers. Therefore
	 * at this point, we will most likely get a read error, indicating
	 * the end of this dump
	 */
	if ((tapeVersion == TAPE_VERSION_0)
	    || (tapeVersion == TAPE_VERSION_1)) {
	    /*
	     * then a tape error is possible at this point, and it
	     * signals the end of the dump. Tapes that are continued
	     * have an EOT marker.
	     */
	    TapeLog(0, taskId, code, curTapePtr->error,
		    "Read error - end-of-dump inferred\n");
	    code = BUTM_EOD;
	}

	if (code != BUTM_EOD)
	    ErrorLog(0, taskId, code, curTapePtr->error,
		     "Can't read FileBegin on tape\n");
	ERROR_EXIT(code);
    }

    /* now read the volume header */
    code = ReadVolHeader(taskId, curTapePtr, volumeHeader);
    if (code)
	ERROR_EXIT(code);

    *bytesRead = 0;
    while (1) {			/*w */

	/* Check for abort in the middle of scanning data */
	if (*bytesRead >= chunkSize) {
	    if (checkAbortByTaskId(taskId))
		ERROR_EXIT(TC_ABORTEDBYREQUEST);
	    chunkSize += BIGCHUNK;
	}

	/* 
	 * Read volume date - If prematurely hit the HW EOF 
	 * marker, check to see if data contains a volumetrailer.
	 */
	rcode =
	    butm_ReadFileData(curTapePtr, buffer[curr], BUTM_BLKSIZE,
			      &nbytes);
	if (rcode) {
	    hasdata[curr] = 0;
	    if ((rcode == BUTM_EOF) || (rcode == BUTM_ENDVOLUME))
		break;

	    ErrorLog(0, taskId, rcode, curTapePtr->error,
		     "Can't read FileData on tape\n");
	    ERROR_EXIT(rcode)
	}
	hasdata[curr] = 1;
	*bytesRead += nbytes;

	if ((nbytes != BUTM_BLKSIZE)
	    ||
	    (FindVolTrailer(buffer[curr], nbytes, &tailBytes, volumeTrailer)))
	    break;

	curr = ((curr == 0) ? 1 : 0);	/* Switch buffers */
    }				/*w */

    /* Now verify that there is a volume trailer and its valid and copy it */
    prev = ((curr == 0) ? 1 : 0);
    if (!FindVolTrailer2
	(buffer[prev], (hasdata[prev] ? BUTM_BLKSIZE : 0), &headBytes,
	 buffer[curr], nbytes, &tailBytes, volumeTrailer)) {
	code = TC_MISSINGTRAILER;
	ErrorLog(0, taskId, code, 0, "Missing volume trailer on tape\n");
    } else {
	/* subtract size of the volume trailer from data read */
	*bytesRead -= sizeof(struct volumeHeader);
    }

    /* 
     * If we didn't hit the EOF while reading data, read FileEnd marker 
     * or EOF marker. 
     */
    if (!rcode) {
	tcode = butm_ReadFileEnd(curTapePtr);
	if (tcode) {
	    ErrorLog(0, taskId, tcode, curTapePtr->error,
		     "Can't read EOF on tape\n");
	    ERROR_EXIT(tcode);
	}
    }

  error_exit:
    if (block)
	free(block);
    return (code);
}

/* nextTapeLabel
 *	generate the name of the next tape label expected
 * exit: 
 *	ptr to static string
 */

char *
nextTapeLabel(char *prevTapeName)
{
    char *prevdot;
    char *retval;
    int seq;
    static char buffer[BU_MAXTAPELEN];

    retval = "";

    /* extract information from previous tape label */
    strcpy(buffer, prevTapeName);
    prevdot = strrchr(buffer, '.');
    if (!prevdot)
	return (retval);
    prevdot++;

    seq = extractTapeSeq(prevTapeName);
    seq++;
    sprintf(prevdot, "%-d", seq);

    return (&buffer[0]);
}

/* readDump
 *	Read all the information on a tape. If to add to the database, queue
 *      onto list so another thread adds the entries to the database.
 * entry:
 *      taskid      - butc task number.
 *      tapeInfoPtr - Tape information.
 *      scanInfoPtr - Info to keep so we can add entries to the db.
 * exit:
 *	0     - tape scanned.
 *	non-0 - error. Abort the scan.
 *	moreTapes set to 1 if this is not the last tape in the dump,
 *                       0 if the last tape,
 *                      -1 don't know if last tape or not.
 */

afs_int32 RcreateDump();

static int
readDump(afs_uint32 taskId, struct butm_tapeInfo *tapeInfoPtr, struct tapeScanInfo *scanInfoPtr)
{
    int moreTapes = 1;
    afs_int32 nbytes, flags, seq;
    int newDump = 1, newTape = 1;
    afs_int32 tapePosition;
    afs_int32 code = 0, tcode;
    int badscan;
    struct volumeHeader volHeader, volTrailer;
    struct budb_tapeEntry tapeEntry;
    struct budb_volumeEntry volEntry;

    volEntry.dump = 0;
    PrintDumpLabel(&scanInfoPtr->dumpLabel);

    while (moreTapes) {		/* While there is a tape to read *//*t */
	badscan = 0;
	while (1) {		/* Read each volume on the tape *//*w */
	    moreTapes = -1;
	    tapePosition = tapeInfoPtr->position;	/* remember position */

	    /*
	     * Skip the volume data
	     */
	    tcode =
		scanVolData(taskId, tapeInfoPtr,
			    scanInfoPtr->tapeLabel.structVersion, &volHeader,
			    &volTrailer, &nbytes);
	    if (tcode) {
		badscan++;

		if (tcode == TC_ABORTEDBYREQUEST) {	/* Aborted */
		    ERROR_EXIT(tcode);
		}

		if (tcode == BUTM_EOD) {
		    moreTapes = 0;	/* the end of the dump */
		    break;
		}

		/* Found a volume but it's incomplete. Skip over these */
		if (volHeader.volumeID) {
		    TapeLog(0, taskId, tcode, 0,
			    "Warning: volume %s (%u) ignored. Incomplete\n",
			    volHeader.volumeName, volHeader.volumeID);
		    continue;
		}

		/* No volume was found. We may have hit the EOT or a 
		 * bad-spot. Try to skip over this spot.
		 */
		if (badscan < 2) {	/* allow 2 errors, then fail */
		    TapeLog(0, taskId, tcode, 0,
			    "Warning: Error in scanning tape - will try skipping volume\n");
		    continue;
		}
		if (scanInfoPtr->tapeLabel.structVersion >= TAPE_VERSION_4) {
		    TapeLog(0, taskId, tcode, 0,
			    "Warning: Error in scanning tape - end-of-tape inferred\n");
		    moreTapes = 1;	/* then assume next tape */
		} else {
		    ErrorLog(0, taskId, tcode, 0, "Error in scanning tape\n");
		    /* will ask if there is a next tape */
		}
		break;
	    }

	    PrintVolumeHeader(&volHeader);

	    /* If this is not the first volume fragment, make sure it follows
	     * the last volume fragment 
	     */
	    if (volEntry.dump) {
		if ((volEntry.dump != volHeader.dumpID)
		    || (volEntry.id != volHeader.volumeID)
		    || (volEntry.seq != volHeader.frag - 2)
		    || (strcmp(volEntry.name, volHeader.volumeName))) {
		    TLog(taskId,
			 "Warning: volume %s (%u) ignored. Incomplete - no last fragment\n",
			 volEntry.name, volEntry.id);

		    if (scanInfoPtr->addDbFlag) {
			tcode = flushSavedEntries(DUMP_FAILED);
			if (tcode)
			    ERROR_EXIT(tcode);
			volEntry.dump = 0;
		    }
		}
	    }

	    /* If this is the first volume fragment, make sure says so */
	    if (scanInfoPtr->addDbFlag && !volEntry.dump
		&& (volHeader.frag != 1)) {
		TLog(taskId,
		     "Warning: volume %s (%u) ignored. Incomplete - no first fragment\n",
		     volHeader.volumeName, volHeader.volumeID);
	    }

	    /* Check that this volume belongs to the dump we are scanning */
	    else if (scanInfoPtr->dumpLabel.dumpid
		     && (volHeader.dumpID != scanInfoPtr->dumpLabel.dumpid)) {
		TLog(taskId,
		     "Warning: volume %s (%u) ignored. Expected DumpId %u, got %u\n",
		     volHeader.volumeName, volHeader.volumeID,
		     scanInfoPtr->dumpLabel.dumpid, volHeader.dumpID);
	    }

	    /* Passed tests, Now add to the database (if dbadd flag is set) */
	    else if (scanInfoPtr->addDbFlag) {
		/* Have enough information to create a dump entry */
		if (newDump) {
		    tcode = RcreateDump(scanInfoPtr, &volHeader);
		    if (tcode) {
			ErrorLog(0, taskId, tcode, 0,
				 "Can't add dump %u to database\n",
				 volHeader.dumpID);
			ERROR_EXIT(tcode);
		    }
		    newDump = 0;
		}

		/* Have enough information to create a tape entry */
		if (newTape) {
		    seq = extractTapeSeq(scanInfoPtr->tapeLabel.AFSName);
		    if (seq < 0)
			ERROR_EXIT(TC_INTERNALERROR);

		    tcode =
			useTape(&tapeEntry, volHeader.dumpID,
				TNAME(&scanInfoPtr->tapeLabel), seq,
				scanInfoPtr->tapeLabel.useCount,
				scanInfoPtr->dumpLabel.creationTime,
				scanInfoPtr->dumpLabel.expirationDate,
				tapepos);
		    if (tcode) {
			char gotName[BU_MAXTAPELEN + 32];

			LABELNAME(gotName, &scanInfoPtr->tapeLabel);
			ErrorLog(0, taskId, tcode, 0,
				 "Can't add tape %s for dump %u to database\n",
				 gotName, volHeader.dumpID);
			ERROR_EXIT(tcode);
		    }
		    newTape = 0;
		}

		/* Create the volume entry */
		flags = ((volHeader.frag == 1) ? BUDB_VOL_FIRSTFRAG : 0);
		if (!volTrailer.contd)
		    flags |= BUDB_VOL_LASTFRAG;
		tcode =
		    addVolume(&volEntry, volHeader.dumpID,
			      TNAME(&scanInfoPtr->tapeLabel),
			      volHeader.volumeName, volHeader.volumeID,
			      volHeader.cloneDate, tapePosition, nbytes,
			      (volHeader.frag - 1), flags);
		if (tcode) {
		    ErrorLog(0, taskId, tcode, 0,
			     "Can't add volume %s (%u) for dump %u to database\n",
			     volHeader.volumeName, volHeader.volumeID,
			     volHeader.dumpID);
		    ERROR_EXIT(tcode);
		}
	    }

	    if (volTrailer.contd) {
		/* No need to read the EOD marker, we know there is a next tape */
		moreTapes = 1;
		break;
	    } else {
		if (scanInfoPtr->addDbFlag) {
		    tcode = flushSavedEntries(DUMP_SUCCESS);
		    if (tcode)
			ERROR_EXIT(tcode);
		    volEntry.dump = 0;
		}
	    }
	}			/*w */

	if (!newTape) {
	    if (scanInfoPtr->addDbFlag) {
		tcode =
		    finishTape(&tapeEntry,
			       (tapeInfoPtr->kBytes +
				(tapeInfoPtr->nBytes ? 1 : 0)));
		if (tcode) {
		    char gotName[BU_MAXTAPELEN + 32];

		    LABELNAME(gotName, &scanInfoPtr->tapeLabel);
		    ErrorLog(0, taskId, tcode, 0,
			     "Can't mark tape %s 'completed' for dump %u in database\n",
			     gotName, tapeEntry.dump);
		    ERROR_EXIT(tcode);
		}
	    }
	}

	/* Ask if there is another tape if we can't figure it out */
	if (moreTapes == -1)
	    moreTapes = (queryoperator ? Ask("Are there more tapes") : 1);

	/* Get the next tape label */
	if (moreTapes) {
	    char *tapeName;
	    afs_int32 dumpid;

	    unmountTape(taskId, tapeInfoPtr);

	    tapeName = nextTapeLabel(scanInfoPtr->tapeLabel.AFSName);
	    dumpid = scanInfoPtr->tapeLabel.dumpid;
	    tcode =
		getScanTape(taskId, tapeInfoPtr, tapeName, dumpid, 1,
			    &scanInfoPtr->tapeLabel);
	    if (tcode)
		ERROR_EXIT(tcode);
	    newTape = 1;
	}
    }				/*t */

    if (!newDump) {
	if (scanInfoPtr->addDbFlag) {
	    tcode = finishDump(&scanInfoPtr->dumpEntry);
	    if (tcode) {
		ErrorLog(0, taskId, tcode, 0,
			 "Can't mark dump %u 'completed' in database\n",
			 scanInfoPtr->dumpEntry.id);
	    }

	    tcode = flushSavedEntries(DUMP_SUCCESS);
	    if (tcode)
		ERROR_EXIT(tcode);
	}
    }

  error_exit:
    return (code);
}

/* Will read a dump, then see if there is a dump following it and
 * try to read that dump too.
 * The first tape label is the first dumpLabel.
 */
int
readDumps(afs_uint32 taskId, struct butm_tapeInfo *tapeInfoPtr, struct tapeScanInfo *scanInfoPtr)
{
    afs_int32 code, c;

    memcpy(&scanInfoPtr->dumpLabel, &scanInfoPtr->tapeLabel,
	   sizeof(struct butm_tapeLabel));

    while (1) {
	code = readDump(taskId, tapeInfoPtr, scanInfoPtr);
	if (code)
	    ERROR_EXIT(code);

	if (scanInfoPtr->tapeLabel.structVersion < TAPE_VERSION_4)
	    break;

	/* Remember the initial dump and see if appended dump exists */

	if (!scanInfoPtr->initialDumpId)
	    scanInfoPtr->initialDumpId = scanInfoPtr->dumpEntry.id;

	c = butm_ReadLabel(tapeInfoPtr, &scanInfoPtr->dumpLabel, 0);	/* no rewind */
	tapepos = tapeInfoPtr->position - 1;
	if (c)
	    break;
    }

  error_exit:
    return (code);
}

afs_int32
getScanTape(afs_int32 taskId, struct butm_tapeInfo *tapeInfoPtr, char *tname, afs_int32 tapeId, int prompt, struct butm_tapeLabel *tapeLabelPtr)
{
    afs_int32 code = 0;
    int tapecount = 1;
    afs_int32 curseq;
    char tapename[BU_MAXTAPELEN + 32];
    char gotname[BU_MAXTAPELEN + 32];

    while (1) {
	/* prompt for a tape */
	if (prompt) {
	    code =
		PromptForTape(SCANOPCODE, tname, tapeId, taskId, tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	prompt = 1;
	tapecount++;

	code = butm_Mount(tapeInfoPtr, "");	/* open the tape device */
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error, "Can't open tape\n");
	    goto newtape;
	}

	/* read the label on the tape */
	code = butm_ReadLabel(tapeInfoPtr, tapeLabelPtr, 1);	/* rewind tape */
	if (code) {
	    ErrorLog(0, taskId, code, tapeInfoPtr->error,
		     "Can't read tape label\n");
	    goto newtape;
	}
	tapepos = tapeInfoPtr->position - 1;

	/* Now check that the tape is good */
	TAPENAME(tapename, tname, tapeId);
	TAPENAME(gotname, tapeLabelPtr->AFSName, tapeLabelPtr->dumpid);

	curseq = extractTapeSeq(tapeLabelPtr->AFSName);

	/* Label can't be null or a bad name */
	if (!strcmp(tapeLabelPtr->AFSName, "") || (curseq <= 0)) {
	    TLog(taskId, "Expected tape with dump, label seen %s\n", gotname);
	    goto newtape;
	}

	/* Label can't be a database tape */
	if (databaseTape(tapeLabelPtr->AFSName)) {
	    TLog(taskId,
		 "Expected tape with dump. Can't scan database tape %s\n",
		 gotname);
	    goto newtape;
	}

	/* If no name, accept any tape */
	if (strcmp(tname, "") == 0) {
	    break;		/* Start scan on any tape */
#ifdef notdef
	    if (curseq == 1)
		break;		/* The first tape */
	    else {
		TLog(taskId, "Expected first tape of dump, label seen %s\n",
		     gotname);
		goto newtape;
	    }
#endif
	}

	if (strcmp(tname, tapeLabelPtr->AFSName)
	    || ((tapeLabelPtr->structVersion >= TAPE_VERSION_3)
		&& (tapeLabelPtr->dumpid != tapeId))) {
	    TLog(taskId, "Tape label expected %s, label seen %s\n", tapename,
		 gotname);
	    goto newtape;
	}

	/* We have the correct tape */
	break;

      newtape:
	unmountTape(taskId, tapeInfoPtr);
    }

  error_exit:
    return (code);
}

/* ScanDumps
 *	This set of code fragments read a tape, and add the information to
 * 	the database. Builds a literal structure.
 *	
 */

int
ScanDumps(struct scanTapeIf *ptr)
{
    struct butm_tapeInfo curTapeInfo;
    struct tapeScanInfo tapeScanInfo;
    afs_uint32 taskId;
    afs_int32 code = 0;

    taskId = ptr->taskId;
    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);
    clearStatus(taskId, DRIVE_WAIT);

    printf("\n\n");
    if (ptr->addDbFlag)
	TLog(taskId, "ScanTape and add to the database\n");
    else
	TLog(taskId, "Scantape\n");

    memset(&tapeScanInfo, 0, sizeof(tapeScanInfo));
    tapeScanInfo.addDbFlag = ptr->addDbFlag;

    memset(&curTapeInfo, 0, sizeof(curTapeInfo));
    curTapeInfo.structVersion = BUTM_MAJORVERSION;
    code = butm_file_Instantiate(&curTapeInfo, &globalTapeConfig);
    if (code) {
	ErrorLog(0, taskId, code, curTapeInfo.error,
		 "Can't initialize tape module\n");
	ERROR_EXIT(code);
    }

    code =
	getScanTape(taskId, &curTapeInfo, "", 0, autoQuery,
		    &tapeScanInfo.tapeLabel);
    if (code)
	ERROR_EXIT(code);

    code = readDumps(taskId, &curTapeInfo, &tapeScanInfo);
    if (code)
	ERROR_EXIT(code);

  error_exit:
    unmountTape(taskId, &curTapeInfo);
    waitDbWatcher();

    if (code == TC_ABORTEDBYREQUEST) {
	ErrorLog(0, taskId, 0, 0, "Scantape: Aborted by request\n");
	clearStatus(taskId, ABORT_REQUEST);
	setStatus(taskId, ABORT_DONE);
    } else if (code) {
	ErrorLog(0, taskId, code, 0, "Scantape: Finished with errors\n");
	setStatus(taskId, TASK_ERROR);
    } else {
	TLog(taskId, "Scantape: Finished\n");
    }

    free(ptr);
    setStatus(taskId, TASK_DONE);
    LeaveDeviceQueue(deviceLatch);
    return (code);
}


/* validatePath
 * exit:
 *	0 - not ok
 *	1 - ok
 */
int
validatePath(struct butm_tapeLabel *labelptr, char *pathptr)
{
    char *up, *tp;
    char tapeName[BU_MAXTAPELEN];

    /* check length */
    if (strlen(pathptr) > BU_MAX_DUMP_PATH - 1) {
	fprintf(stderr, "Invalid pathname - too long\n");
	return (0);
    }

    if (!labelptr)
	return (1);

    strcpy(tapeName, labelptr->AFSName);

    tp = strrchr(tapeName, '.');
    if (!tp)
	return (1);
    tp++;

    up = strrchr(pathptr, '/');
    if (!up) {
	fprintf(stderr, "Invalid path name, missing /\n");
	return (0);
    }
    up++;

    if (strcmp(up, tp) != 0) {
	fprintf(stderr, "Invalid path name\n");
	fprintf(stderr,
		"Mismatch between tape dump name '%s' and users dump name '%s'\n",
		tp, up);
	return (0);
    }
    return (1);
}

/* volumesetNamePtr
 *	return a pointer to a (static) volume set name string.
 * entry:
 *	ptr - ptr to a dump name
 * exit:
 *	0 - error. Can't extract volumeset name.
 *	ptr - to static volumeset string.
 */

char *
volumesetNamePtr(char *ptr)
{
    static char vsname[BU_MAXUNAMELEN];
    char *dotPtr;
    int dotIndex;

    dotPtr = strchr(ptr, '.');
    if (!dotPtr)
	return (0);

    dotIndex = dotPtr - ptr;
    if ((dotIndex + 1) > sizeof(vsname))
	return (0);		/* name too long */

    strncpy(&vsname[0], ptr, dotIndex);
    vsname[dotIndex] = 0;	/* ensure null terminated */

    return (&vsname[0]);
}

char *
extractDumpName(char *ptr)
{
    static char dname[BU_MAXTAPELEN];
    char *dotPtr;
    int dotIndex;

    dotPtr = strrchr(ptr, '.');
    if (!dotPtr)
	return (0);

    dotIndex = dotPtr - ptr;
    if ((dotIndex + 1) > sizeof(dname))
	return (0);		/* name too long */

    strncpy(&dname[0], ptr, dotIndex);
    dname[dotIndex] = 0;	/* ensure null terminated */

    return (&dname[0]);
}

/* extractTapeSeq
 *	The routine assumes that tape names have an embedded sequence number
 *	as the trialing component. It is suggested that any tape naming
 *	changes retain the trailing seq. number
 * entry: 
 *	tapename - ptr to tape name 
 * exit:
 *	0 or positive - sequence number
 *	-1 - error, couldn't extract sequence number
 */

int
extractTapeSeq(char *tapename)
{
    char *sptr;

    sptr = strrchr(tapename, '.');
    if (!sptr)
	return (-1);
    sptr++;
    return (atol(sptr));
}

/* databaseTape
 *   returns true or false depending on whether the tape is 
 *   a database tape or not.
 */
int
databaseTape(char *tapeName)
{
    char *sptr;
    int c;

    sptr = strrchr(tapeName, '.');
    if (!sptr)
	return (0);

    c = (int)((char *) sptr - (char *) tapeName);
    if (strncmp(tapeName, DUMP_TAPE_NAME, c) == 0)
	return (1);

    return (0);
}

afs_int32
RcreateDump(struct tapeScanInfo *tapeScanInfoPtr, struct volumeHeader *volHeaderPtr)
{
    afs_int32 code;
    struct butm_tapeLabel *dumpLabelPtr = &tapeScanInfoPtr->dumpLabel;
    struct budb_dumpEntry *dumpEntryPtr = &tapeScanInfoPtr->dumpEntry;

    /* construct dump entry */
    memset(dumpEntryPtr, 0, sizeof(struct budb_dumpEntry));
    dumpEntryPtr->id = volHeaderPtr->dumpID;
    dumpEntryPtr->initialDumpID = tapeScanInfoPtr->initialDumpId;
    dumpEntryPtr->parent = volHeaderPtr->parentID;
    dumpEntryPtr->level = volHeaderPtr->level;
    dumpEntryPtr->created = volHeaderPtr->dumpID;	/* time dump was created */
    dumpEntryPtr->flags = 0;
    dumpEntryPtr->incTime = 0;
    dumpEntryPtr->nVolumes = 0;
    strcpy(dumpEntryPtr->volumeSetName,
	   volumesetNamePtr(volHeaderPtr->dumpSetName));
    strcpy(dumpEntryPtr->dumpPath, dumpLabelPtr->dumpPath);
    strcpy(dumpEntryPtr->name, volHeaderPtr->dumpSetName);
    default_tapeset(&dumpEntryPtr->tapes, volHeaderPtr->dumpSetName);
    dumpEntryPtr->tapes.b = extractTapeSeq(dumpLabelPtr->AFSName);
    copy_ktcPrincipal_to_budbPrincipal(&dumpLabelPtr->creator,
				       &dumpEntryPtr->dumper);

    code = bcdb_CreateDump(dumpEntryPtr);
    return (code);
}
