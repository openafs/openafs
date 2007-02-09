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
#include <io.h>
#include <fcntl.h>
#else
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#endif
#include <time.h>
#include <sys/types.h>
#include <afs/stds.h>
#include <stdio.h>
#include <lock.h>
#include <ubik.h>
#include <lwp.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include <string.h>
#include <des.h>
#include <afs/cellconfig.h>
#include <errno.h>
#include "budb.h"
#include "budb_errs.h"
#include "database.h"
#include "error_macros.h"
#include "globals.h"
#include "afs/audit.h"


/* dump ubik database - interface routines */

/* badEntry
 *	no checking for now.
 */

afs_int32
badEntry(dbAddr)
     afs_uint32 dbAddr;
{
    /* return entry ok */
    return (0);
}

/* setupDbDump
 *	decode the arguments passed via LWP and dump the database.
 */

setupDbDump(writeFid)
     int writeFid;
{
    afs_int32 code = 0;

    code = InitRPC(&dumpSyncPtr->ut, LOCKREAD, 1);
    if (code)
	goto error_exit;

    code = writeDatabase(dumpSyncPtr->ut, writeFid);
    if (code)
	LogError(code, "writeDatabase failed\n");

    code = close(writeFid);
    if (code)
	LogError(code, "pipe writer close failed\n");

    LogDebug(5, "writeDatabase complete\n");

  error_exit:
    if (dumpSyncPtr->ut)
	ubik_EndTrans(dumpSyncPtr->ut);
    return (code);
}


afs_int32 DumpDB(), RestoreDbHeader();
afs_int32
SBUDB_DumpDB(call, firstcall, maxLength, charListPtr, done)
     struct rx_call *call;
     int firstcall;
     afs_int32 maxLength;
     charListT *charListPtr;
     afs_int32 *done;
{
    afs_int32 code;

    code = DumpDB(call, firstcall, maxLength, charListPtr, done);
    osi_auditU(call, BUDB_DmpDBEvent, code, AUD_END);
    return code;
}

afs_int32
DumpDB(call, firstcall, maxLength, charListPtr, done)
     struct rx_call *call;
     int firstcall;		/* 1 - init.  0 - no init */
     afs_int32 maxLength;
     charListT *charListPtr;
     afs_int32 *done;
{
    PROCESS dumperPid, watcherPid;
    int readSize;
    afs_int32 code = 0;
    extern dumpWatcher();

    if (callPermitted(call) == 0)
	ERROR(BUDB_NOTPERMITTED);

    ObtainWriteLock(&dumpSyncPtr->ds_lock);

    /* If asking for zero bytes, then this is a call to reset the timeToLive
     * timer. Reset it if there is a dump in progress. 
     */
    if (maxLength == 0) {
	charListPtr->charListT_val = NULL;
	charListPtr->charListT_len = 0;

	*done = ((dumpSyncPtr->statusFlags == 0) ? 1 : 0);

	/* reset the clock on dump timeout */
	dumpSyncPtr->timeToLive = time(0) + DUMP_TTL_INC;
	goto error_exit;
    }

    if (dumpSyncPtr->statusFlags == 0) {
	if (!firstcall)
	    ERROR(BUDB_DUMPFAILED);

	LogDebug(5, "Setup dump\n");

	/* no dump in progress - setup and retake lock */
	memset(dumpSyncPtr, 0, sizeof(*dumpSyncPtr));
/*	ObtainWriteLock(&dumpSyncPtr->ds_lock); */

	/* mark dump in progress */
	dumpSyncPtr->statusFlags = 1;

	code = pipe(dumpSyncPtr->pipeFid);
	if (code)
	    ERROR(errno);

	code =
	    LWP_CreateProcess(setupDbDump, 16384, 1,
			      (void *)dumpSyncPtr->pipeFid[1],
			      "Database Dumper", &dumperPid);
	if (code)
	    goto error_exit;

	dumpSyncPtr->dumperPid = dumperPid;
	dumpSyncPtr->timeToLive = time(0) + DUMP_TTL_INC;

	/* now create the watcher thread */
	code =
	    LWP_CreateProcess(dumpWatcher, 16384, 1, 0,
			      "Database Dump Watchdog", &watcherPid);
    } else if (firstcall)
	ERROR(BUDB_LOCKED);

    /* now read the database and feed it to the rpc connection */

    /* wait for data */
    while (dumpSyncPtr->ds_bytes == 0) {
	/* if no more data */
	if ((dumpSyncPtr->ds_writerStatus == DS_DONE)
	    || (dumpSyncPtr->ds_writerStatus == DS_DONE_ERROR)) {
	    break;
	}

	if (dumpSyncPtr->ds_writerStatus == DS_WAITING) {
	    LogDebug(6, "wakup writer\n");
	    dumpSyncPtr->ds_writerStatus = 0;
	    code = LWP_SignalProcess(&dumpSyncPtr->ds_writerStatus);
	    if (code)
		LogError(code, "BUDB_DumpDB: signal delivery failed\n");
	}
	LogDebug(6, "wait for writer\n");
	dumpSyncPtr->ds_readerStatus = DS_WAITING;
	ReleaseWriteLock(&dumpSyncPtr->ds_lock);
	LWP_WaitProcess(&dumpSyncPtr->ds_readerStatus);
	ObtainWriteLock(&dumpSyncPtr->ds_lock);
    }

    charListPtr->charListT_val = (char *)malloc(maxLength);
    readSize =
	read(dumpSyncPtr->pipeFid[0], charListPtr->charListT_val, maxLength);

    /* reset the clock on dump timeout */
    dumpSyncPtr->timeToLive = time(0) + DUMP_TTL_INC;

    LogDebug(4, "read of len %d returned %d\n", maxLength, readSize);

    charListPtr->charListT_len = readSize;

    if (readSize == 0) {	/* last chunk */
	*done = 1;
	close(dumpSyncPtr->pipeFid[0]);
	dumpSyncPtr->statusFlags = 0;
    } else
	*done = 0;

    dumpSyncPtr->ds_bytes -= readSize;
    if (dumpSyncPtr->ds_writerStatus == DS_WAITING) {
	dumpSyncPtr->ds_writerStatus = 0;
	code = LWP_SignalProcess(&dumpSyncPtr->ds_writerStatus);
	if (code)
	    LogError(code, "BUDB_DumpDB: signal delivery failed\n");
    }

  error_exit:
    if (!code && (dumpSyncPtr->ds_writerStatus == DS_DONE_ERROR))
	code = -1;
    ReleaseWriteLock(&dumpSyncPtr->ds_lock);
    return (code);
}

afs_int32
SBUDB_RestoreDbHeader(call, header)
     struct rx_call *call;
     struct DbHeader *header;
{
    afs_int32 code;

    code = RestoreDbHeader(call, header);
    osi_auditU(call, BUDB_RstDBHEvent, code, AUD_END);
    return code;
}

afs_int32
RestoreDbHeader(call, header)
     struct rx_call *call;
     struct DbHeader *header;
{
    struct ubik_trans *ut = 0;
    afs_int32 code = 0;

    extern struct memoryDB db;

    if (callPermitted(call) == 0)
	ERROR(BUDB_NOTPERMITTED);

    code = InitRPC(&ut, LOCKWRITE, 1);
    if (code)
	goto error_exit;

    if (header->dbversion != ntohl(db.h.version))
	ERROR(BUDB_VERSIONMISMATCH);

    /* merge rather than replace the header information */
    if (db.h.lastDumpId < htonl(header->lastDumpId))
	db.h.lastDumpId = htonl(header->lastDumpId);

    if (db.h.lastTapeId < htonl(header->lastTapeId))
	db.h.lastTapeId = htonl(header->lastTapeId);

    if (db.h.lastInstanceId < htonl(header->lastInstanceId))
	db.h.lastInstanceId = htonl(header->lastInstanceId);

    code = dbwrite(ut, 0, (char *)&db.h, sizeof(db.h));
    if (code)
	code = BUDB_IO;

  error_exit:
    if (ut)
	ubik_EndTrans(ut);
    return (code);
}

/* dumpWatcher
 *	monitors the state of a database dump. If the dump calls do not 
 *	reset the time to live value, the dump times out. In that case,
 *	we kill the database traversal thread and clean up all the other 
 *	state. Most importantly, the database is unlocked so that other
 *	transactions can proceed.
 */

dumpWatcher()
{
    afs_int32 code;

    while (1) {			/*w */

	/* printf("dumpWatcher\n"); */
	ObtainWriteLock(&dumpSyncPtr->ds_lock);

	if (dumpSyncPtr->statusFlags == 0) {
	    /* dump has finished */
	    goto exit;
	}

	/* check time to live */
	if (time(0) > dumpSyncPtr->timeToLive) {	/*i */
	    /* dump has exceeded the allocated time - terminate it */
	    LogError(0, "Database dump timeout exceeded: %s",
		     ctime(&dumpSyncPtr->timeToLive));
	    LogError(0, "Terminating database dump\n");

	    close(dumpSyncPtr->pipeFid[0]);
	    close(dumpSyncPtr->pipeFid[1]);

	    code = LWP_DestroyProcess(dumpSyncPtr->dumperPid);
	    if (code)
		LogError(code, "dumpWatcher: failed to kill dump thread\n");

	    if (dumpSyncPtr->ut) {
		code = ubik_AbortTrans(dumpSyncPtr->ut);
		if (code)
		    LogError(code, "Aborting dump transaction\n");
	    }

	    memset(dumpSyncPtr, 0, sizeof(*dumpSyncPtr));
	    goto exit;
	}
	/*i */
	ReleaseWriteLock(&dumpSyncPtr->ds_lock);
	IOMGR_Sleep(5);
    }				/*w */

  exit:
    ReleaseWriteLock(&dumpSyncPtr->ds_lock);
    /* printf("dumpWatcher exit\n"); */
    return (0);
}
