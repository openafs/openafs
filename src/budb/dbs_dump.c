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
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
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
#include "budb_internal.h"
#include "error_macros.h"
#include "globals.h"
#include "afs/audit.h"

afs_int32 DumpDB(struct rx_call *, int, afs_int32, charListT *, afs_int32 *);
afs_int32 RestoreDbHeader(struct rx_call *, struct DbHeader *);
void *dumpWatcher(void *);

/* dump ubik database - interface routines */

/* badEntry
 *	no checking for now.
 */

afs_int32
badEntry(afs_uint32 dbAddr)
{
    /* return entry ok */
    return (0);
}

/* setupDbDump
 *	decode the arguments passed via LWP and dump the database.
 */

void *
setupDbDump(void *param)
{
    int writeFid = (intptr_t)param;
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
    return (void *)(intptr_t)(code);
}


afs_int32
SBUDB_DumpDB(struct rx_call *call, int firstcall, afs_int32 maxLength,
	     charListT *charListPtr, afs_int32 *done)
{
    afs_int32 code;

    code = DumpDB(call, firstcall, maxLength, charListPtr, done);
    osi_auditU(call, BUDB_DmpDBEvent, code, AUD_END);
    return code;
}

afs_int32
DumpDB(struct rx_call *call,
       int firstcall,		/* 1 - init.  0 - no init */
       afs_int32 maxLength,
       charListT *charListPtr,
       afs_int32 *done)
{
#ifdef AFS_PTHREAD_ENV
    pthread_t dumperPid, watcherPid;
    pthread_attr_t dumperPid_tattr;
    pthread_attr_t watcherPid_tattr;
#else
    PROCESS dumperPid, watcherPid;
#endif
    int readSize;
    afs_int32 code = 0;

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

#ifdef AFS_PTHREAD_ENV
	/* Initialize the condition variables and the mutexes we use
	 * to signal and synchronize the reader and writer threads.
	 */
	CV_INIT(&dumpSyncPtr->ds_readerStatus_cond, "reader cond", CV_DEFAULT, 0);
	CV_INIT(&dumpSyncPtr->ds_writerStatus_cond, "writer cond", CV_DEFAULT, 0);
	MUTEX_INIT(&dumpSyncPtr->ds_readerStatus_mutex, "reader", MUTEX_DEFAULT, 0);
	MUTEX_INIT(&dumpSyncPtr->ds_writerStatus_mutex, "writer", MUTEX_DEFAULT, 0);

	/* Initialize the thread attributes and launch the thread */

	osi_Assert(pthread_attr_init(&dumperPid_tattr) == 0);
	osi_Assert(pthread_attr_setdetachstate(&dumperPid_tattr, PTHREAD_CREATE_DETACHED) == 0);
	osi_Assert(pthread_create(&dumperPid, &dumperPid_tattr, (void *)setupDbDump, NULL) == 0);

#else
	code =
	    LWP_CreateProcess(setupDbDump, 16384, 1,
			      (void *)(intptr_t)dumpSyncPtr->pipeFid[1],
			      "Database Dumper", &dumperPid);
	if (code)
	    goto error_exit;
#endif

	dumpSyncPtr->dumperPid = dumperPid;
	dumpSyncPtr->timeToLive = time(0) + DUMP_TTL_INC;

#ifdef AFS_PTHREAD_ENV
	/* Initialize the thread attributes and launch the thread */

	osi_Assert(pthread_attr_init(&watcherPid_tattr) == 0);
	osi_Assert(pthread_attr_setdetachstate(&watcherPid_tattr, PTHREAD_CREATE_DETACHED) == 0);
	osi_Assert(pthread_create(&watcherPid, &watcherPid_tattr, (void *)dumpWatcher, NULL) == 0);
#else
	/* now create the watcher thread */
	code =
	    LWP_CreateProcess(dumpWatcher, 16384, 1, 0,
			      "Database Dump Watchdog", &watcherPid);
#endif
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
#ifdef AFS_PTHREAD_ENV
	    CV_BROADCAST(&dumpSyncPtr->ds_writerStatus_cond);
#else
	    code = LWP_SignalProcess(&dumpSyncPtr->ds_writerStatus);
	    if (code)
		LogError(code, "BUDB_DumpDB: signal delivery failed\n");
#endif
	}
	LogDebug(6, "wait for writer\n");
	dumpSyncPtr->ds_readerStatus = DS_WAITING;
	ReleaseWriteLock(&dumpSyncPtr->ds_lock);
#ifdef AFS_PTHREAD_ENV
        MUTEX_ENTER(&dumpSyncPtr->ds_readerStatus_mutex);
        CV_WAIT(&dumpSyncPtr->ds_readerStatus_cond, &dumpSyncPtr->ds_readerStatus_mutex);
        MUTEX_EXIT(&dumpSyncPtr->ds_readerStatus_mutex);
#else
	LWP_WaitProcess(&dumpSyncPtr->ds_readerStatus);
#endif
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
#ifdef AFS_PTHREAD_ENV
	CV_BROADCAST(&dumpSyncPtr->ds_writerStatus_cond);
#else
	code = LWP_SignalProcess(&dumpSyncPtr->ds_writerStatus);
	if (code)
	    LogError(code, "BUDB_DumpDB: signal delivery failed\n");
#endif
    }

  error_exit:
    if (!code && (dumpSyncPtr->ds_writerStatus == DS_DONE_ERROR))
	code = -1;
    ReleaseWriteLock(&dumpSyncPtr->ds_lock);
    return (code);
}

afs_int32
SBUDB_RestoreDbHeader(struct rx_call *call, struct DbHeader *header)
{
    afs_int32 code;

    code = RestoreDbHeader(call, header);
    osi_auditU(call, BUDB_RstDBHEvent, code, AUD_END);
    return code;
}

afs_int32
RestoreDbHeader(struct rx_call *call, struct DbHeader *header)
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

void *
dumpWatcher(void *unused)
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
#ifdef AFS_PTHREAD_ENV
	    osi_Assert(pthread_cancel(dumpSyncPtr->dumperPid) == 0);
#else
	    code = LWP_DestroyProcess(dumpSyncPtr->dumperPid);
	    if (code)
		LogError(code, "dumpWatcher: failed to kill dump thread\n");
#endif

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
#ifdef AFS_PTHREAD_ENV
	sleep(5);
#else
	IOMGR_Sleep(5);
#endif
    }				/*w */

  exit:
    ReleaseWriteLock(&dumpSyncPtr->ds_lock);
    /* printf("dumpWatcher exit\n"); */
    return (0);
}
