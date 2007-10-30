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
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <lwp.h>
#include <lock.h>
#include <afs/tcdata.h>
#include <afs/bubasics.h>
#include <afs/budb_client.h>
#include <afs/vldbint.h>

#include <afs/vlserver.h>
#include <afs/volser.h>
#include <afs/volint.h>
#include <afs/cellconfig.h>

#include "error_macros.h"

dlqlinkT savedEntries;
dlqlinkT entries_to_flush;

int dbWatcherinprogress;

afs_int32
threadEntryDir(char *anEntry, afs_int32 size, afs_int32 type)
{
    dlqlinkP entryPtr;
    char *entry = NULL;
    int tried;

    for (tried = 0; tried < 5; tried++) {
	entryPtr = (dlqlinkP) malloc(sizeof(dlqlinkT));
	entry = (char *)malloc(size);
	if (entryPtr && entry)
	    break;

	/* sleep a minute and try again */
	if (entryPtr)
	    free(entryPtr);
	if (entry)
	    free(entry);

	if ((tried > 0) && !dbWatcherinprogress)
	    return (TC_NOMEMORY);
#ifdef AFS_PTHREAD_ENV
	sleep(60);
#else
	IOMGR_Sleep(60);
#endif
    }
    entryPtr->dlq_prev = entryPtr->dlq_next = (dlqlinkP) NULL;
    entryPtr->dlq_type = type;
    entryPtr->dlq_structPtr = entry;

    memcpy(entry, anEntry, size);
    dlqLinkb(&entries_to_flush, entryPtr);
    return (0);
}

/*
 * threadEntry.
 *     Creates an entry and puts it onto the savedEntries list.
 *     Will retry up to 5 times if not enough memory. Hopfully, the 
 *     Watcher thread will free up some memory for it to continue.
 */

afs_int32
threadEntry(char *anEntry, afs_int32 size, afs_int32 type)
{
    dlqlinkP entryPtr;
    char *entry = NULL;
    int tried;

    for (tried = 0; tried < 5; tried++) {
	entryPtr = (dlqlinkP) malloc(sizeof(dlqlinkT));
	entry = (char *)malloc(size);
	if (entryPtr && entry)
	    break;

	/* sleep a minute and try again */
	if (entryPtr)
	    free(entryPtr);
	if (entry)
	    free(entry);

	if ((tried > 0) && !dbWatcherinprogress)
	    return (TC_NOMEMORY);
#ifdef AFS_PTHREAD_ENV
	sleep(60);
#else
	IOMGR_Sleep(60);
#endif
    }

    entryPtr->dlq_prev = entryPtr->dlq_next = (dlqlinkP) NULL;
    entryPtr->dlq_type = type;
    entryPtr->dlq_structPtr = entry;

    memcpy(entry, anEntry, size);
    dlqLinkb(&savedEntries, (dlqlinkP) entryPtr);
    return (0);
}

/* ------------------------------------------------------------------ */

afs_int32
useDump(struct budb_dumpEntry *dumpEntryPtr)
{
    afs_int32 code = 0;

    code =
	threadEntry(dumpEntryPtr, sizeof(struct budb_dumpEntry), DLQ_USEDUMP);
    return (code);
}

/*
 * finishDump
 *     Creates a dump entry (finished) and puts it onto the savedEntries list.
 */
afs_int32
finishDump(struct budb_dumpEntry *aDumpEntryPtr)
{
    afs_int32 code = 0;

    code =
	threadEntry(aDumpEntryPtr, sizeof(struct budb_dumpEntry),
		    DLQ_FINISHDUMP);
    return (code);
}

/*
 * useTape
 *     Creates a tape entry and puts it onto the savedEntries list.
 */
afs_int32
useTape(struct budb_tapeEntry *aTapeEntryPtr, afs_int32 dumpID, char *tapename, afs_int32 tapeSeq, afs_int32 useCount, Date written, Date expiration, afs_int32 tapepos)
{
    afs_int32 code = 0;

    memset(aTapeEntryPtr, 0, sizeof(struct budb_tapeEntry));
    strcpy(aTapeEntryPtr->name, tapename);
    aTapeEntryPtr->flags = BUDB_TAPE_BEINGWRITTEN;
    aTapeEntryPtr->written = written;	/* When label was written */
    aTapeEntryPtr->expires = expiration;
    aTapeEntryPtr->seq = tapeSeq;
    aTapeEntryPtr->useCount = useCount;
    aTapeEntryPtr->dump = dumpID;
    aTapeEntryPtr->labelpos = tapepos;

    code =
	threadEntry(aTapeEntryPtr, sizeof(struct budb_tapeEntry),
		    DLQ_USETAPE);
    return (code);
}

/*
 * finishTape
 *     Creates a tape entry (finished) and puts it onto the savedEntries list.
 */
afs_int32
finishTape(struct budb_tapeEntry *aTapeEntryPtr, afs_int32 useKBytes)
{
    afs_int32 code = 0;

    aTapeEntryPtr->flags = BUDB_TAPE_WRITTEN;
    aTapeEntryPtr->useKBytes = useKBytes;

    code =
	threadEntry(aTapeEntryPtr, sizeof(struct budb_tapeEntry),
		    DLQ_FINISHTAPE);
    return (code);
}

/*
 * addVolume
 *     Creates a volume entry and puts it onto the savedEntries list.
 */
afs_int32
addVolume(struct budb_volumeEntry *aVolEntryPtr, afs_int32 dumpID, char *tapename, char *volname, afs_int32 volid, Date cloneDate, afs_int32 startPos, afs_int32 volBytes, int fragment, afs_int32 flags)
{
    afs_int32 code = 0;
    int allo = 0;

    if (!aVolEntryPtr) {
	aVolEntryPtr = (struct budb_volumeEntry *)
	    malloc(sizeof(struct budb_volumeEntry));
	if (!aVolEntryPtr)
	    ERROR_EXIT(TC_NOMEMORY);
	allo = 1;
    }

    memset(aVolEntryPtr, 0, sizeof(struct budb_volumeEntry));
    strcpy(aVolEntryPtr->name, volname);
    aVolEntryPtr->flags = flags;
    aVolEntryPtr->id = volid;
    aVolEntryPtr->position = startPos;
    aVolEntryPtr->clone = cloneDate;
    aVolEntryPtr->nBytes = volBytes;
    aVolEntryPtr->seq = fragment;
    aVolEntryPtr->dump = dumpID;
    strcpy(aVolEntryPtr->tape, tapename);

    code =
	threadEntry(aVolEntryPtr, sizeof(struct budb_volumeEntry),
		    DLQ_VOLENTRY);

  error_exit:
    if (code && allo)
	free(aVolEntryPtr);
    return (code);
}

/*
 * flushSavedEntries
 *     Runs through the list of savedEntries and adds the volumes and 
 *     tapes to the database.
 *     A status of DUMP_NORETRYEOT means the tape(s) contains no useful data,
 *     and tapes and volumes should not be added to the DB.
 */
afs_int32
flushSavedEntries(afs_int32 status)
{
    dlqlinkP entryPtr;
    struct budb_tapeEntry *tapePtr;
    struct budb_volumeEntry *volPtr;
    afs_int32 code = 0;

    /*
     * On DUMP_NORETRYEOT, the volume being dumped was the first on the tape and hit the
     * EOT. This means the volume is larger than the tape. Instead of going onto the next
     * tape, backup reuses the tape. Thus, we must remove this tape entry and free it
     * without adding it to the backup database.
     */
    if (status == DUMP_NORETRYEOT) {
	entryPtr = dlqUnlinkb(&savedEntries);
	if (!entryPtr || (entryPtr->dlq_type != DLQ_USETAPE))
	    ERROR_EXIT(TC_INTERNALERROR);

	tapePtr = (struct budb_tapeEntry *)entryPtr->dlq_structPtr;
	if (tapePtr)
	    free(tapePtr);
	if (entryPtr)
	    free(entryPtr);
    }

    /* 
     * Add dump, tape, and volume entries to the list for the dbWatcher to 
     * flush. Volume entries are not added if the volume failed to dump.
     */
    while (entryPtr = dlqUnlinkf(&savedEntries)) {
	if ((entryPtr->dlq_type == DLQ_VOLENTRY) && (status != DUMP_SUCCESS)) {
	    volPtr = (struct budb_volumeEntry *)entryPtr->dlq_structPtr;
	    if (volPtr)
		free(volPtr);
	    if (entryPtr)
		free(entryPtr);
	} else {
	    dlqLinkb(&entries_to_flush, entryPtr);
	}
    }

  error_exit:
    /* Free anything that remains on dlq */
    dlqTraverseQueue(&savedEntries, free, free);
    return (code);
}

void
waitDbWatcher()
{
    int message = 0;

    while (dbWatcherinprogress || !dlqEmpty(&entries_to_flush)) {
	if (!message) {
	    printf("Updating database\n");
	    message++;
	}
#ifdef AFS_PTHREAD_ENV
	sleep(2);
#else
	IOMGR_Sleep(2);
#endif
    }

    if (message) {
	printf("Updating database - done\n");
    }
}

#define MAXVOLUMESTOADD 100
int addvolumes = 1;

void
dbWatcher()
{
    dlqlinkP entryPtr;
    struct budb_dumpEntry *dumpPtr;
    struct budb_tapeEntry *tapePtr;
    struct budb_volumeEntry *volPtr, volumes[MAXVOLUMESTOADD];
    afs_int32 new;
    afs_int32 code = 0;
    int i, c, addedDump;

    dlqInit(&entries_to_flush);
    dlqInit(&savedEntries);

    dbWatcherinprogress = 0;
    addedDump = 1;
    while (1) {
	/*while */
	/* Add tape and volume enties to the backup database */
	while (entryPtr = dlqUnlinkf(&entries_to_flush)) {
	    dbWatcherinprogress = 1;

	    if (!entryPtr->dlq_structPtr) {
		ErrorLog(0, 0, TC_BADQUEUE, 0,
			 "Warning: Invalid database entry - nota added\n");
	    } else
		switch (entryPtr->dlq_type) {
		case DLQ_USEDUMP:
		    dumpPtr =
			(struct budb_dumpEntry *)entryPtr->dlq_structPtr;
		    /* Now call the database to create the entry */
		    code = bcdb_CreateDump(dumpPtr);
		    if (code) {
			if (code == BUDB_DUMPIDEXISTS) {
			    printf
				("Dump %s (DumpID %u) already exists in backup database\n",
				 dumpPtr->name, dumpPtr->id);
			} else {
			    ErrorLog(0, 0, code, 0,
				     "Warning: Can't create dump %s (DumpID %u) in backup database\n",
				     dumpPtr->name, dumpPtr->id);
			}
		    }
		    addedDump = (code ? 0 : 1);
		    break;

		case DLQ_FINISHDUMP:
		    dumpPtr =
			(struct budb_dumpEntry *)entryPtr->dlq_structPtr;
		    if (addedDump) {
			code = bcdb_FinishDump(dumpPtr);
			if (code) {
			    ErrorLog(0, 0, code, 0,
				     "Warning: Can't finish dump %s (DumpID %u) in backup database\n",
				     dumpPtr->name, dumpPtr->id);
			}
		    }
		    addedDump = 1;
		    break;

		case DLQ_USETAPE:
		    tapePtr =
			(struct budb_tapeEntry *)entryPtr->dlq_structPtr;
		    if (addedDump) {
			code = bcdb_UseTape(tapePtr, &new);
			if (code) {
			    ErrorLog(0, 0, code, 0,
				     "Warning: Can't add tape %s of DumpID %u to backup database\n",
				     tapePtr->name, tapePtr->dump);
			}
		    }
		    break;

		case DLQ_FINISHTAPE:
		    tapePtr =
			(struct budb_tapeEntry *)entryPtr->dlq_structPtr;
		    if (addedDump) {
			code = bcdb_FinishTape(tapePtr, &new);
			if (code) {
			    ErrorLog(0, 0, code, 0,
				     "Warning: Can't finish tape %s of DumpID %u in backup database\n",
				     tapePtr->name, tapePtr->dump);
			}
		    }
		    break;

		case DLQ_VOLENTRY:
		    /* collect array of volumes to add to the dump */
		    for (c = 0; c < MAXVOLUMESTOADD; c++) {
			if (c > 0) {	/* don't read the 1st - already did */
			    entryPtr = dlqUnlinkf(&entries_to_flush);	/* Get the next entry */
			    if (!entryPtr)
				break;
			}

			if (entryPtr->dlq_type != DLQ_VOLENTRY) {
			    /* Place back onto list and add the vol entries we have */
			    dlqLinkf(&entries_to_flush, entryPtr);
			    entryPtr = (dlqlinkP) 0;	/* don't want to deallocate below */
			    break;
			}

			volPtr =
			    (struct budb_volumeEntry *)entryPtr->
			    dlq_structPtr;
			if (!volPtr) {
			    ErrorLog(0, 0, TC_BADQUEUE, 0,
				     "Warning: Invalid database entry - not added\n");
			    break;
			}

			memcpy(&volumes[c], volPtr,
			       sizeof(struct budb_volumeEntry));
			free(volPtr);
			free(entryPtr);
			entryPtr = (dlqlinkP) 0;
		    }

		    if (addedDump) {
			if (addvolumes) {
			    code = bcdb_AddVolumes(&volumes[0], c);
			    if (code) {
				if (code < 0)
				    addvolumes = 0;
				else {
				    ErrorLog(0, 0, code, 0,
					     "Warning: Can't add %d volumes to dumpid %u\n",
					     c, volumes[0].dump);
				}
			    }
			}
			if (!addvolumes) {
			    for (i = 0; i < c; i++) {
				code = bcdb_AddVolume(&volumes[i]);
				if (code) {
				    ErrorLog(0, 0, code, 0,
					     "Warning: Can't add volume %s %u to backup database\n",
					     volumes[i].name, volumes[i].id);
				}
			    }
			}
		    }
		    break;

		default:
		    ErrorLog(0, 0, 0, 0,
			     "Warning: dbWatcher: Unrecognized entry type %d\n",
			     entryPtr->dlq_type);
		    break;	/* ignore */
		}

	    if (entryPtr) {
		if (entryPtr->dlq_structPtr)
		    free(entryPtr->dlq_structPtr);
		free(entryPtr);
	    }
	    entryPtr = (dlqlinkP) 0;
	    dumpPtr = (budb_dumpEntry *) 0;
	    volPtr = (budb_volumeEntry *) 0;
	    tapePtr = (budb_tapeEntry *) 0;
	}			/*while */

	dbWatcherinprogress = 0;
#ifdef AFS_PTHREAD_ENV
	sleep(2);
#else
	IOMGR_Sleep(2);
#endif
    }
}
