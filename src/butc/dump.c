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
#include <sys/time.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <lwp.h>
#include <lock.h>
#include <errno.h>
#include <afs/tcdata.h>
#include <afs/bubasics.h>
#include <afs/budb_client.h>
#include <afs/vldbint.h>
#include <afs/ktime.h>
#include <afs/vlserver.h>
#include <afs/volser.h>
#include <afs/volint.h>
#include <afs/cellconfig.h>

#include "error_macros.h"
#include "butc_xbsa.h"
#include "afs/butx.h"


/* GLOBAL CONFIGURATION PARAMETERS */
extern int dump_namecheck;
extern int queryoperator;
extern int isafile;
extern int forcemultiple;

extern struct ubik_client *cstruct;
dlqlinkT savedEntries;
dlqlinkT entries_to_flush;

afs_int32 flushSavedEntries(), finishDump(), finishTape(), useTape(),
addVolume();

extern struct rx_connection *UV_Bind();

extern afs_int32 groupId;
extern afs_int32 BufferSize;
extern afs_int32 statusSize;
extern FILE *centralLogIO;
afs_int32 lastPass = 0;
#ifdef xbsa
extern afs_int32 xbsaType;
char *butcdumpIdStr = "/backup_afs_volume_dumps";
extern struct butx_transactionInfo butxInfo;
extern char *xbsaObjectOwner;
extern char *appObjectOwner;
extern char *xbsaSecToken;
extern char *xbsalGName;
extern char *globalButcLog;
#endif /*xbsa */

afs_int32 dataSize;		/* Size of data to read on each rx_Read() call */
afs_int32 tapeblocks;		/* Number of 16K tape datablocks in buffer (!CONF_XBSA) */

/* TBD
 *
 * Done 1) dump id generation
 * Done xx) volume fragment number accounting !!  I think.
 * 2) check abort - check after subroutine calls
 * Done 3) trailer anomaly
 * 4) trailer damage indicator after partial dumps ( affects scandump )
 * Done 5) Ensure mount failure logged
 * 6) Ensure bucoord status calls work
 *
 * notes
 * pass 3: 
 *	keep token timeout. If no user reponse (idle time > some period)
 *	and tokens about to time out, terminate dump. This provides at
 *	least something usable.
 */

#define DUMPNAME(dumpname, name, dbDumpId) \
   if (dbDumpId == 0) \
     sprintf(dumpname, "%s", name); \
   else \
     sprintf(dumpname, "%s (DumpId %u)", name, dbDumpId);

#if defined(AFS_NT40_ENV) || (defined(AFS_DARWIN_ENV) && !defined(AFS_DARWIN60_ENV)) || defined(AFS_SUN4_ENV)
int
localtime_r(time_t * t, struct tm *tm)
{
    memcpy(tm, localtime(t), sizeof(struct tm));
    return 0;
}
#endif

struct dumpRock {
    /* status only */
    int tapeSeq;
    int curVolume;		/* index in dumpNode of volume */
    int curVolumeStatus;	/* more explicit dump state */
    afs_uint32 curVolStartPos;	/* Starting position of the current volume */
    afs_uint32 databaseDumpId;	/* real dump id, for db */
    afs_uint32 initialDumpId;	/* the initial dump, for appended dumps */
    afs_int32 volumesDumped;	/* # volumes successfully dumped */
    afs_int32 volumesFailed;	/* # volumes that failed to dump */
    afs_int32 volumesNotDumped;	/* # volumes that were not dumped (didn't fail) */

    /* tape management */
    char tapeName[TC_MAXTAPENAMELEN];
    struct butm_tapeInfo *tapeInfoPtr;
    struct butm_tapeLabel tapeLabel;
    int wroteLabel;		/* If the tape label is written */

    /* database information */
    struct budb_dumpEntry lastDump;	/* the last dump of this volset */
    struct budb_dumpEntry dump;	/* current dump */
    struct budb_tapeEntry tape;	/* current tape, not used -VA */

    /* links to existing info */
    struct dumpNode *node;
};

/* configuration variables */
#define HITEOT(code) ((code == BUTM_IO) || (code == BUTM_EOT) || (code == BUTM_IOCTL))
extern int autoQuery;
extern int maxpass;

afs_int32 tc_EndMargin;
afs_int32 tc_KEndMargin;
static char *bufferBlock;

/* compute the absolute expiration date */
afs_int32
calcExpirationDate(afs_int32 expType, afs_int32 expDate, afs_int32 createTime)
{
    struct ktime_date kd;
    afs_int32 Add_RelDate_to_Time();

    switch (expType) {
    case BC_REL_EXPDATE:
	/* expiration date is relative to the creation time of the dump.
	 * This is the only case that requires any work
	 */
	Int32To_ktimeRelDate(expDate, &kd);
	return (Add_RelDate_to_Time(&kd, createTime));
	break;

    case BC_ABS_EXPDATE:
	return (expDate);
	break;

    case BC_NO_EXPDATE:
    default:
	return (0);
    }
}

afs_int32 curr_bserver = 0;
struct rx_connection *curr_fromconn = (struct rx_connection *)0;

struct rx_connection *
Bind(afs_int32 server)
{
    if (curr_fromconn) {
	if (curr_bserver == server)	/* Keep connection if have it */
	    return (curr_fromconn);

	rx_DestroyConnection(curr_fromconn);	/* Otherwise get rid of it */
	curr_fromconn = (struct rx_connection *)0;
	curr_bserver = 0;
    }

    if (server) {
	curr_fromconn = UV_Bind(server, AFSCONF_VOLUMEPORT);	/* Establish new connection */
	if (curr_fromconn)
	    curr_bserver = server;
    }

    return (curr_fromconn);
}

/* notes
 * 1) save the chunksize or otherwise ensure tape space remaining is
 *	check frequently enough
 * 2) This is called once. For partial dumps, need to 
 *	ensure that the tape device is left in the correct state for
 *	further dumps.
 * 
 */
#define BIGCHUNK 102400

afs_int32
dumpVolume(struct tc_dumpDesc * curDump, struct dumpRock * dparamsPtr)
{
    struct butm_tapeInfo *tapeInfoPtr = dparamsPtr->tapeInfoPtr;
    struct dumpNode *nodePtr = dparamsPtr->node;
    afs_int32 taskId = nodePtr->taskID;
    char *buffer;
    int fragmentNumber;
    afs_int32 volumeFlags;
    afs_int32 kRemaining;
    afs_int32 rc, code = 0;
    afs_int32 toread;
    afs_uint32 volBytesRead;
    afs_uint32 chunkSize;
    afs_int32 bytesread;	/* rx reads */
    int endofvolume = 0;	/* Have we read all volume data */
    int indump = 0;
    int fragmentvolume;
    struct volumeHeader hostVolumeHeader;

    struct rx_call *fromcall = (struct rx_call *)0;
    struct rx_connection *fromconn;
    afs_int32 updatedate, fromtid = 0;
    volEntries volumeInfo;
    afs_int32 bytesWritten;
    afs_uint32 statuscount = statusSize, tsize = 0;

    dparamsPtr->curVolumeStatus = DUMP_NOTHING;

    fromconn = Bind(htonl(curDump->hostAddr));	/* get connection to the server */

    /* Determine when the volume was last cloned and updated */
    volumeInfo.volEntries_val = (volintInfo *) 0;
    volumeInfo.volEntries_len = 0;
    rc = AFSVolListOneVolume(fromconn, curDump->partition, curDump->vid,
			     &volumeInfo);
    if (rc)
	ERROR_EXIT(rc);
    updatedate = volumeInfo.volEntries_val[0].updateDate;
    curDump->cloneDate =
	((curDump->vtype ==
	  RWVOL) ? time(0) : volumeInfo.volEntries_val[0].creationDate);

    if (curDump->date >= curDump->cloneDate)
	ERROR_EXIT(0);		/* not recloned since last dump */
    if (curDump->date > updatedate) {
	dparamsPtr->curVolumeStatus = DUMP_NODUMP;	/* not modified since last dump */
	ERROR_EXIT(0);
    }

    /* Start the volserver transaction and dump */
    rc = AFSVolTransCreate(fromconn, curDump->vid, curDump->partition, ITBusy,
			   &fromtid);
    if (rc)
	ERROR_EXIT(rc);
    fromcall = rx_NewCall(fromconn);

    rc = StartAFSVolDump(fromcall, fromtid, curDump->date);
    if (rc)
	ERROR_EXIT(rc);

    dparamsPtr->curVolumeStatus = DUMP_PARTIAL;
    dparamsPtr->curVolStartPos = tapeInfoPtr->position;

    /* buffer is place in bufferBlock to write volume data.
     * butm_writeFileData() assumes the previous BUTM_HDRSIZE bytes
     * is available to write the tape block header.
     */
    buffer = bufferBlock + BUTM_HDRSIZE;

    /* Dump one volume fragment at a time until we dump the full volume.
     * A volume with more than 1 fragment means the volume will 'span' 
     * 2 or more tapes.
     */
    for (fragmentNumber = 1; !endofvolume; fragmentNumber++) {	/*frag */
	rc = butm_WriteFileBegin(tapeInfoPtr);
	if (rc) {
	    ErrorLog(1, taskId, rc, tapeInfoPtr->error,
		     "Can't write FileBegin on tape\n");
	    ERROR_EXIT(rc);
	}
	indump = 1;		/* first write to tape */

	/* Create and Write the volume header */
	makeVolumeHeader(&hostVolumeHeader, dparamsPtr, fragmentNumber);
	hostVolumeHeader.contd = ((fragmentNumber == 1) ? 0 : TC_VOLCONTD);
	volumeHeader_hton(&hostVolumeHeader, buffer);

	rc = butm_WriteFileData(tapeInfoPtr, buffer, 1,
				sizeof(hostVolumeHeader));
	if (rc) {
	    ErrorLog(1, taskId, rc, tapeInfoPtr->error,
		     "Can't write VolumeHeader on tape\n");
	    ERROR_EXIT(rc);
	}

	bytesWritten = BUTM_BLOCKSIZE;	/* Wrote one tapeblock */
	tsize += bytesWritten;

	/* Start reading volume data, rx_Read(), and dumping to the tape
	 * until we've dumped the entire volume (endofvolume == 1). We can
	 * exit this loop early if we find we are close to the end of the
	 * tape; in which case we dump the next fragment on the next tape.
	 */
	volBytesRead = 0;
	chunkSize = 0;
	fragmentvolume = 0;
	while (!endofvolume && !fragmentvolume) {	/*w */
	    bytesread = 0;

	    /* Check for abort in the middle of writing data */
	    if (volBytesRead >= chunkSize) {
		chunkSize += BIGCHUNK;
		if (checkAbortByTaskId(taskId))
		    ABORT_EXIT(TC_ABORTEDBYREQUEST);

		/* set bytes dumped for backup */
		lock_Status();
		nodePtr->statusNodePtr->nKBytes = tapeInfoPtr->kBytes;
		unlock_Status();
	    }

	    /* Determine how much data to read in upcoming RX_Read() call */
	    toread = dataSize;
	    /* Check if we are close to the EOT. There should at least be some
	     * data on the tape before it is switched. HACK: we have to split a 
	     * volume across tapes because the volume trailer says the dump 
	     * continues on the next tape (and not the filemark). This could 
	     * result in a volume starting on one tape (no volume data dumped) and 
	     * continued on the next tape. It'll work, just requires restore to 
	     * switch tapes. This allows many small volumes (<16K) to be dumped.
	     */
	    kRemaining = butm_remainingKSpace(tapeInfoPtr);
	    if ((kRemaining < tc_KEndMargin)
		&& (volBytesRead
		    || (tapeInfoPtr->position > (isafile ? 3 : 2)))) {
		fragmentvolume = 1;
	    }


	    /* Guess at how much data to read. So we don't write off end of tape */
	    if (kRemaining < (tapeblocks * 16)) {
		if (kRemaining < 0) {
		    toread = BUTM_BLKSIZE;
		} else {
		    toread = ((kRemaining / 16) + 1) * BUTM_BLKSIZE;
		    if (toread > dataSize)
			toread = dataSize;
		}
	    }

	    /* Read some volume data. */
	    if (fragmentvolume) {
		bytesread = 0;
	    } else {
		bytesread = rx_Read(fromcall, buffer, toread);
		volBytesRead += bytesread;
		if (bytesread != toread) {
		    /* Make sure were at end of volume and not a communication error */
		    rc = rx_Error(fromcall);
		    if (rc)
			ERROR_EXIT(rc);
		    endofvolume = 1;
		}
	    }

	    if (fragmentvolume || endofvolume) {
		/* Create a volume trailer appending it to this data block */
		makeVolumeHeader(&hostVolumeHeader, dparamsPtr,
				 fragmentNumber);
		hostVolumeHeader.contd = (endofvolume ? 0 : TC_VOLCONTD);
		hostVolumeHeader.magic = TC_VOLENDMAGIC;
		hostVolumeHeader.endTime = (endofvolume ? time(0) : 0);
		volumeHeader_hton(&hostVolumeHeader, &buffer[bytesread]);
		bytesread += sizeof(hostVolumeHeader);
	    }

	    /* Write the datablock out */
	    /* full data buffer - write it to tape */
	    rc = butm_WriteFileData(tapeInfoPtr, buffer, tapeblocks,
				    bytesread);
	    if (rc) {
		ErrorLog(1, taskId, rc, tapeInfoPtr->error,
			 "Can't write VolumeData on tape\n");
		ERROR_EXIT(rc);
	    }
	    bytesWritten = tapeblocks * BUTM_BLOCKSIZE;
	    tsize += bytesWritten;

	    /* Display a status line every statusSize or at end of volume */
	    if (statusSize
		&& ((tsize >= statuscount) || endofvolume
		    || fragmentvolume)) {
		time_t t = time(0);
		struct tm tm;
		localtime_r(&t, &tm);
		printf("%02d:%02d:%02d: Task %u: %u KB: %s: %u B\n",
		       tm.tm_hour, tm.tm_min, tm.tm_sec, taskId,
		       tapeInfoPtr->kBytes, hostVolumeHeader.volumeName,
		       tsize);
		statuscount = tsize + statusSize;
	    }
	}			/*w */

	/* End the dump before recording it in BUDB as successfully dumped */
	rc = butm_WriteFileEnd(tapeInfoPtr);
	indump = 0;
	if (rc) {
	    ErrorLog(1, taskId, rc, tapeInfoPtr->error,
		     "Can't write FileEnd on tape\n");
	    ERROR_EXIT(rc);
	}

	/* Record in BUDB the volume fragment as succcessfully dumped */
	volumeFlags = ((fragmentNumber == 1) ? BUDB_VOL_FIRSTFRAG : 0);
	if (endofvolume)
	    volumeFlags |= BUDB_VOL_LASTFRAG;
	rc = addVolume(0, dparamsPtr->databaseDumpId, dparamsPtr->tapeName,
		       nodePtr->dumps[dparamsPtr->curVolume].name,
		       nodePtr->dumps[dparamsPtr->curVolume].vid,
		       nodePtr->dumps[dparamsPtr->curVolume].cloneDate,
		       dparamsPtr->curVolStartPos, volBytesRead,
		       (fragmentNumber - 1), volumeFlags);
	if (rc)
	    ABORT_EXIT(rc);

	/* If haven't finished dumping the volume, end this
	 * tape and get the next tape.
	 */
	if (!endofvolume) {
	    /* Write an EOT marker.
	     * Log the error but ignore it since the dump is effectively done.
	     * Scantape will detect continued volume and not read the EOT.
	     */
	    rc = butm_WriteEOT(tapeInfoPtr);
	    if (rc)
		TapeLog(1, taskId, rc, tapeInfoPtr->error,
			"Warning: Can't write End-Of-Dump on tape\n");

	    /* Unmount the tape */
	    unmountTape(taskId, tapeInfoPtr);

	    /* Tell the database the tape is complete (and ok) */
	    rc = finishTape(&dparamsPtr->tape,
			    dparamsPtr->tapeInfoPtr->kBytes +
			    (dparamsPtr->tapeInfoPtr->nBytes ? 1 : 0));
	    if (rc)
		ABORT_EXIT(rc);

	    /* get the next tape. Prompt, mount, and add it into the database */
	    dparamsPtr->tapeSeq++;
	    rc = getDumpTape(dparamsPtr, 1, 0);	/* interactive - no append */
	    if (rc)
		ABORT_EXIT(rc);

	    dparamsPtr->curVolStartPos = tapeInfoPtr->position;
	}
    }				/*frag */

    dparamsPtr->curVolumeStatus = DUMP_SUCCESS;

  error_exit:
    /* 
     * If we hit the end, see if this is the first volume on the tape or not.
     * Also, mark the tape as finished if the tape contains other dumps.
     */
    if (!code)
	code = rc;
    if (HITEOT(code)) {
	ErrorLog(2, taskId, code, tapeInfoPtr->error,
		 "Warning: Dump (%s) hit end-of-tape inferred\n",
		 nodePtr->dumpSetName);

	if (tapeInfoPtr->position == 2) {
	    dparamsPtr->curVolumeStatus = DUMP_NORETRYEOT;
	} else {
	    dparamsPtr->curVolumeStatus = DUMP_RETRY;
	    rc = finishTape(&dparamsPtr->tape,
			    dparamsPtr->tapeInfoPtr->kBytes +
			    (dparamsPtr->tapeInfoPtr->nBytes ? 1 : 0));
	    if (rc)
		ABORT_EXIT(rc);
	}
    }

    /* 
     * This is used when an error occurs part way into a volume dump. Clean
     * the tape state by writing an FileEnd mark. Forgo this action if we hit
     * the end of tape.
     */
    else if (indump) {
	rc = butm_WriteFileEnd(tapeInfoPtr);
	indump = 0;
	if (rc) {
	    ErrorLog(1, taskId, rc, tapeInfoPtr->error,
		     "Can't write FileEnd on tape\n");
	}
    }

    if (fromcall) {
	rc = rx_EndCall(fromcall, 0);
	if (!code)
	    code = rc;
    }

    if (fromtid) {
	afs_int32 rcode;
	rc = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (!code)
	    code = (rc ? rc : rcode);
    }

    return (code);

  abort_exit:
    dparamsPtr->curVolumeStatus = DUMP_FAILED;
    ERROR_EXIT(code);
}

afs_int32
xbsaDumpVolume(struct tc_dumpDesc * curDump, struct dumpRock * dparamsPtr)
{
#ifdef xbsa
    struct butm_tapeInfo *tapeInfoPtr = dparamsPtr->tapeInfoPtr;
    struct dumpNode *nodePtr = dparamsPtr->node;
    char *buffer = bufferBlock;
    afs_int32 taskId = nodePtr->taskID;
    afs_int32 rc, code = 0;
    afs_int32 toread;
    afs_uint32 volBytesRead;
    afs_uint32 chunkSize;
    afs_int32 bytesread;	/* rx reads */
    int endofvolume = 0;	/* Have we read all volume data */
    int begindump = 0, indump = 0;	/* if dump transaction started; if dumping data */
    struct volumeHeader hostVolumeHeader;

    struct rx_call *fromcall = (struct rx_call *)0;
    struct rx_connection *fromconn;
    afs_int32 updatedate, fromtid = 0;
    volEntries volumeInfo;
    afs_int32 bytesWritten;
    afs_uint32 statuscount = statusSize, tsize = 0, esize;
    afs_hyper_t estSize;

    char dumpIdStr[XBSA_MAX_OSNAME];
    char volumeNameStr[XBSA_MAX_PATHNAME];
    static char *dumpDescription = "AFS volume dump";
    static char *objectDescription = "XBSA - butc";

    dparamsPtr->curVolumeStatus = DUMP_NOTHING;

    fromconn = Bind(htonl(curDump->hostAddr));	/* get connection to the server */

    /* Determine when the volume was last cloned and updated */
    volumeInfo.volEntries_val = (volintInfo *) 0;
    volumeInfo.volEntries_len = 0;
    rc = AFSVolListOneVolume(fromconn, curDump->partition, curDump->vid,
			     &volumeInfo);
    if (rc)
	ERROR_EXIT(rc);
    updatedate = volumeInfo.volEntries_val[0].updateDate;
    curDump->cloneDate =
	((curDump->vtype ==
	  RWVOL) ? time(0) : volumeInfo.volEntries_val[0].creationDate);

    /* Get the volume size (in KB) and increase by 25%. Then set as a hyper */
    esize = volumeInfo.volEntries_val[0].size;
    esize += (esize / 4) + 1;

    if (curDump->date >= curDump->cloneDate)
	ERROR_EXIT(0);		/* not recloned since last dump */
    if (curDump->date > updatedate) {
	dparamsPtr->curVolumeStatus = DUMP_NODUMP;	/* not modified since last dump */
	ERROR_EXIT(0);
    }

    /* Start a new XBSA Transaction */
    rc = xbsa_BeginTrans(&butxInfo);
    if (rc != XBSA_SUCCESS) {
	ErrorLog(1, taskId, rc, 0, "Unable to create a new transaction\n");
	ERROR_EXIT(rc);
    }
    begindump = 1;		/* Will need to do an xbsa_EndTrans */

    /* Start the volserver transaction and dump. Once started, the
     * volume status is "partial dump". Also, the transaction with
     * the volserver is idle until the first read. An idle transaction
     * will time out in 600 seconds. After the first rx_Read,
     * the transaction is not idle. See GCTrans().
     */
    rc = AFSVolTransCreate(fromconn, curDump->vid, curDump->partition, ITBusy,
			   &fromtid);
    if (rc)
	ERROR_EXIT(rc);
    fromcall = rx_NewCall(fromconn);

    rc = StartAFSVolDump(fromcall, fromtid, curDump->date);
    if (rc)
	ERROR_EXIT(rc);

    dparamsPtr->curVolumeStatus = DUMP_PARTIAL;
    dparamsPtr->curVolStartPos = tapeInfoPtr->position;

    /* Tell XBSA what the name and size of volume to write */
    strcpy(dumpIdStr, butcdumpIdStr);	/* "backup_afs_volume_dumps" */
    sprintf(volumeNameStr, "/%d", dparamsPtr->databaseDumpId);
    strcat(volumeNameStr, "/");
    strcat(volumeNameStr, curDump->name);	/* <dumpid>/<volname> */
    hset32(estSize, esize);
    hshlft(estSize, 10);	/* Multiply by 1024 so its in KB */

    rc = xbsa_WriteObjectBegin(&butxInfo, dumpIdStr, volumeNameStr,
			       xbsalGName, estSize, dumpDescription,
			       objectDescription);
    if (rc != XBSA_SUCCESS) {
	ErrorLog(1, taskId, rc, 0,
		 "Unable to begin writing of the fileset data to the server\n");
	ERROR_EXIT(rc);
    }
    indump = 1;			/* Will need to do an xbsa_WriteObjectEnd */

    /* Create and Write the volume header */
    makeVolumeHeader(&hostVolumeHeader, dparamsPtr, 1);
    hostVolumeHeader.contd = 0;
    volumeHeader_hton(&hostVolumeHeader, buffer);

    rc = xbsa_WriteObjectData(&butxInfo, buffer, sizeof(struct volumeHeader),
			      &bytesWritten);
    if (rc != XBSA_SUCCESS) {
	ErrorLog(1, taskId, rc, 0,
		 "Unable to write VolumeHeader data to the server\n");
	ERROR_EXIT(rc);
    }
    /* There is a bug in the ADSM library where the bytesWritten is
     * not filled in, so we set it as correct anyway.
     */
    bytesWritten = sizeof(struct volumeHeader);
    if (bytesWritten != sizeof(struct volumeHeader)) {
	ErrorLog(1, taskId, rc, 0,
		 "The size of VolumeHeader written (%d) does not equal its actual size (%d)\n",
		 bytesWritten, sizeof(struct volumeHeader));
	ERROR_EXIT(TC_INTERNALERROR);
    }

    incSize(tapeInfoPtr, sizeof(struct volumeHeader));	/* Increment amount we've written */
    tsize += bytesWritten;

    /* Start reading volume data, rx_Read(), and dumping to the tape
     * until we've dumped the entire volume (endofvolume == 1).
     */
    volBytesRead = 0;
    chunkSize = 0;
    while (!endofvolume) {	/*w */
	bytesread = 0;

	/* Check for abort in the middle of writing data */
	if (volBytesRead >= chunkSize) {
	    chunkSize += BIGCHUNK;
	    if (checkAbortByTaskId(taskId))
		ABORT_EXIT(TC_ABORTEDBYREQUEST);

	    /* set bytes dumped for backup */
	    lock_Status();
	    nodePtr->statusNodePtr->nKBytes = tapeInfoPtr->kBytes;
	    unlock_Status();
	}

	/* Determine how much data to read in upcoming RX_Read() call */
	toread = dataSize;

	/* Read some volume data. */
	bytesread = rx_Read(fromcall, buffer, toread);
	volBytesRead += bytesread;
	if (bytesread != toread) {
	    afs_int32 rcode;

	    /* Make sure were at end of volume and not a communication error */
	    rc = rx_Error(fromcall);
	    if (rc)
		ERROR_EXIT(rc);

	    endofvolume = 1;

	    /* Create a volume trailer appending it to this data block (if not XBSA) */
	    makeVolumeHeader(&hostVolumeHeader, dparamsPtr, 1);
	    hostVolumeHeader.contd = 0;
	    hostVolumeHeader.magic = TC_VOLENDMAGIC;
	    hostVolumeHeader.endTime = time(0);
	    volumeHeader_hton(&hostVolumeHeader, &buffer[bytesread]);
	    bytesread += sizeof(hostVolumeHeader);

	    /* End the dump and transaction with the volserver. We end it now, before
	     * we make the XBSA call because if XBSA blocks, we could time out on the 
	     * volserver (After last read, the transaction with the volserver is idle).
	     */
	    rc = rx_EndCall(fromcall, 0);
	    fromcall = 0;
	    if (rc)
		ERROR_EXIT(rc);

	    rc = AFSVolEndTrans(fromconn, fromtid, &rcode);
	    fromtid = 0;
	    if (rc)
		ERROR_EXIT(rc);
	}

	/* Write the datablock out */
	rc = xbsa_WriteObjectData(&butxInfo, buffer, bytesread,
				  &bytesWritten);
	if (rc != XBSA_SUCCESS) {
	    ErrorLog(1, taskId, rc, 0,
		     "Unable to write data to the server\n");
	    ERROR_EXIT(rc);
	}
	/* There is a bug in the ADSM library where the bytesWritten is
	 * not filled in, so we set it as correct anyway.
	 */
	bytesWritten = bytesread;
	if (bytesWritten != bytesread) {
	    ErrorLog(1, taskId, rc, 0,
		     "The size of data written (%d) does not equal size read (%d)\n",
		     bytesWritten, bytesread);
	    ERROR_EXIT(TC_INTERNALERROR);
	}

	incSize(tapeInfoPtr, bytesread);	/* Increment amount we've written */
	tsize += bytesWritten;

	/* Display a status line every statusSize or at end of volume */
	if (statusSize && ((tsize >= statuscount) || endofvolume)) {
	    time_t t = time(0);
	    struct tm tm;
	    localtime_r(&t, &tm);
	    printf("%02d:%02d:%02d: Task %u: %u KB: %s: %u B\n", tm.tm_hour,
		   tm.tm_min, tm.tm_sec, taskId, tapeInfoPtr->kBytes,
		   hostVolumeHeader.volumeName, tsize);
	    statuscount = tsize + statusSize;
	}
    }				/*w */

    /* End the XBSA transaction before recording it in BUDB as successfully dumped */
    rc = xbsa_WriteObjectEnd(&butxInfo);
    indump = 0;
    if (rc != XBSA_SUCCESS) {
	ErrorLog(1, taskId, rc, 0,
		 "Unable to terminate writing of the volume data to the server");
	ERROR_EXIT(rc);
    }
    rc = xbsa_EndTrans(&butxInfo);
    begindump = 0;
    tapeInfoPtr->position++;
    if (rc != XBSA_SUCCESS) {
	ErrorLog(1, taskId, rc, 0,
		 "Unable to terminate the current transaction");
	ERROR_EXIT(rc);
    }

    /* Record in BUDB the volume fragment as succcessfully dumped */
    rc = addVolume(0, dparamsPtr->databaseDumpId, dparamsPtr->tapeName,
		   nodePtr->dumps[dparamsPtr->curVolume].name,
		   nodePtr->dumps[dparamsPtr->curVolume].vid,
		   nodePtr->dumps[dparamsPtr->curVolume].cloneDate,
		   dparamsPtr->curVolStartPos, volBytesRead, 0 /*frag0 */ ,
		   (BUDB_VOL_FIRSTFRAG | BUDB_VOL_LASTFRAG));
    if (rc)
	ABORT_EXIT(rc);

    dparamsPtr->curVolumeStatus = DUMP_SUCCESS;

  error_exit:
    /* Cleanup after an error occurs part way into a volume dump */
    if (fromcall) {
	rc = rx_EndCall(fromcall, 0);
	if (!code)
	    code = rc;
    }

    if (fromtid) {
	afs_int32 rcode;
	rc = AFSVolEndTrans(fromconn, fromtid, &rcode);
	if (!code)
	    code = (rc ? rc : rcode);
    }

    /* If this dump failed, what happens to successive retries
     * of the volume? How do they get recorded in the XBSA database
     * (overwritten)? If not, we don't record this in the BUDB database
     * so it will not be removed when we delete the dump. What to do?
     * Also if the volume was never recorded in the DB (partial dump).
     */
    if (indump) {
	/* End the Write */
	rc = xbsa_WriteObjectEnd(&butxInfo);
	indump = 0;
	if (rc != XBSA_SUCCESS) {
	    ErrorLog(1, taskId, rc, 0,
		     "Unable to terminate writing of the volume data to the server");
	}
	tapeInfoPtr->position++;
    }

    if (begindump) {
	/* End the XBSA Transaction */
	rc = xbsa_EndTrans(&butxInfo);
	begindump = 0;
	if (rc != XBSA_SUCCESS) {
	    ErrorLog(1, taskId, rc, 0,
		     "Unable to terminate the current transaction");
	}
    }

    return (code);

  abort_exit:
    dparamsPtr->curVolumeStatus = DUMP_FAILED;
    ERROR_EXIT(code);
#else
    return 0;
#endif
}

#define HOSTADDR(sockaddr) (sockaddr)->sin_addr.s_addr

/* dumpPass
 *	Go through the list of volumes to dump, dumping each one. The action
 *	taken when a volume dump fails, depends on the passNumber. At minimum,
 *	the failed volume is remembered.
 * notes:
 *	flushSavedEntries - inconsistent treatment for errors. What should
 *		be done for user aborts?
 */

afs_int32
dumpPass(struct dumpRock * dparamsPtr, int passNumber)
{
    struct dumpNode *nodePtr = dparamsPtr->node;
    struct butm_tapeInfo *tapeInfoPtr = dparamsPtr->tapeInfoPtr;
    afs_int32 taskId = nodePtr->taskID;
    struct tc_dumpDesc *curDump;
    int action, e;
    afs_int32 code = 0, tcode, dvcode;
    char ch;
    char retryPrompt();
    struct vldbentry vldbEntry;
    struct sockaddr_in server;
    afs_int32 tapepos;

    TapeLog(2, taskId, 0, 0, "Starting pass %d\n", passNumber);

    /* while there are more volumes to dump */
    for (dparamsPtr->curVolume = 0; dparamsPtr->curVolume < nodePtr->arraySize; dparamsPtr->curVolume++) {	/*w */
	curDump = &nodePtr->dumps[dparamsPtr->curVolume];
	if (curDump->hostAddr == 0)
	    continue;

	/* set name of current volume being dumped */
	lock_Status();
	strcpy(nodePtr->statusNodePtr->volumeName, curDump->name);
	unlock_Status();

	/* Determine location of the volume.
	 * In case the volume moved has moved.
	 */
	if (passNumber > 1) {	/*pass */
	    tcode =
		bc_GetEntryByID(cstruct, curDump->vid, curDump->vtype,
				&vldbEntry);
	    if (tcode) {
		ErrorLog(0, taskId, tcode, 0,
			 "Volume %s (%u) failed - Can't find volume in VLDB\n",
			 curDump->name, curDump->vid);
		curDump->hostAddr = 0;
		dparamsPtr->volumesFailed++;
		continue;
	    }

	    switch (curDump->vtype) {
	    case BACKVOL:
		if (!(vldbEntry.flags & BACK_EXISTS)) {
		    ErrorLog(0, taskId, 0, 0,
			     "Volume %s (%u) failed - Backup volume no longer exists\n",
			     curDump->name, curDump->vid);
		    curDump->hostAddr = 0;
		    dparamsPtr->volumesFailed++;
		    continue;
		}
		/* Fall into RWVOL case */

	    case RWVOL:
		for (e = 0; e < vldbEntry.nServers; e++) {	/* Find the RW volume */
		    if (vldbEntry.serverFlags[e] & ITSRWVOL)
			break;
		}
		break;

	    case ROVOL:
		/* Try to use the server and partition we found the volume on
		 * Otherwise, use the first RO volume.
		 */
		for (e = 0; e < vldbEntry.nServers; e++) {	/* Find the RO volume */
		    if ((curDump->hostAddr == vldbEntry.serverNumber[e])
			&& (curDump->partition ==
			    vldbEntry.serverPartition[e]))
			break;
		}

		if (e >= vldbEntry.nServers) {	/* Didn't find RO volume */
		    for (e = 0; e < vldbEntry.nServers; e++) {	/* Find the first RO volume */
			if (vldbEntry.serverFlags[e] & ITSROVOL)
			    break;
		    }
		}
		break;

	    default:
		ErrorLog(0, taskId, 0, 0,
			 "Volume %s (%u) failed - Unknown volume type\n",
			 curDump->name, curDump->vid);
		curDump->hostAddr = 0;
		continue;
		break;
	    }

	    if (e >= vldbEntry.nServers) {
		ErrorLog(0, taskId, 0, 0,
			 "Volume %s (%u) failed - Can't find volume entry in VLDB\n",
			 curDump->name, curDump->vid);
		curDump->hostAddr = 0;
		dparamsPtr->volumesFailed++;
		continue;
	    }

	    /* Remember the server and partition the volume exists on */
	    memset(&server, 0, sizeof(server));
	    server.sin_addr.s_addr = vldbEntry.serverNumber[e];
	    server.sin_port = 0;
	    server.sin_family = AF_INET;
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
	    server.sin_len = sizeof(struct sockaddr_in);
#endif
	    curDump->hostAddr = HOSTADDR(&server);
	    curDump->partition = vldbEntry.serverPartition[e];

	    /* Determine date from which to do an incremental dump
	     */
	    if (nodePtr->parent) {
		tcode =
		    bcdb_FindClone(nodePtr->parent, curDump->name,
				   &curDump->date);
		if (tcode)
		    curDump->date = 0;
	    } else {
		curDump->date = 0;	/* do a full dump */
	    }
	}
	/*pass */
	if (checkAbortByTaskId(taskId))
	    ERROR_EXIT(TC_ABORTEDBYREQUEST);

	/* Establish connection to volume - UV_ routine expects 
	 * host address in network order 
	 */
	if (CONF_XBSA) {
	    dvcode = xbsaDumpVolume(curDump, dparamsPtr);
	} else {
	    dvcode = dumpVolume(curDump, dparamsPtr);
	}
	action = dparamsPtr->curVolumeStatus;

	/* Flush volume and tape entries to the database */
	tcode = flushSavedEntries(action);
	if (tcode)
	    ERROR_EXIT(tcode);

	switch (action) {
	case DUMP_SUCCESS:
	    TapeLog(1, taskId, 0, 0, "Volume %s (%u) successfully dumped\n",
		    curDump->name, curDump->vid);
	    if (dvcode)
		ErrorLog(1, taskId, dvcode, 0,
			 "Warning: Termination processing error on volume %s (%u)\n",
			 curDump->name, curDump->vid);

	    curDump->hostAddr = 0;
	    dparamsPtr->volumesDumped++;
	    break;

	case DUMP_PARTIAL:
	case DUMP_NOTHING:
	    if (action == DUMP_PARTIAL) {
		ErrorLog(1, taskId, dvcode, 0,
			 "Volume %s (%u) failed - partially dumped\n",
			 curDump->name, curDump->vid);
	    } else if (dvcode) {
		ErrorLog(0, taskId, dvcode, 0, "Volume %s (%u) failed\n",
			 curDump->name, curDump->vid);
	    } else {
		ErrorLog(0, taskId, dvcode, 0,
			 "Volume %s (%u) not dumped - has not been re-cloned since last dump\n",
			 curDump->name, curDump->vid);
	    }

	    if (passNumber == maxpass) {
		if (!queryoperator)
		    ch = 'o';
		else
		    ch = retryPrompt(curDump->name, curDump->vid, taskId);

		switch (ch) {
		case 'r':	/* retry */
		    dparamsPtr->curVolume--;	/* redump this volume */
		    continue;
		    break;
		case 'o':	/* omit */
		    ErrorLog(1, taskId, 0, 0, "Volume %s (%u) omitted\n",
			     curDump->name, curDump->vid);
		    dparamsPtr->volumesFailed++;
		    break;
		case 'a':	/* abort */
		    TapeLog(1, taskId, 0, 0, "Dump aborted\n");
		    ERROR_EXIT(TC_ABORTEDBYREQUEST);
		    break;
		default:
		    ERROR_EXIT(TC_INTERNALERROR);
		    break;
		}
	    }
	    break;

	case DUMP_RETRY:
	    TapeLog(1, taskId, dvcode, 0,
		    "Volume %s (%u) hit end-of-tape inferred - will retry on next tape\n",
		    curDump->name, curDump->vid);

	    /* Get the next tape */
	    unmountTape(taskId, tapeInfoPtr);

	    dparamsPtr->tapeSeq++;
	    tcode = getDumpTape(dparamsPtr, 1, 0);	/* interactive - no appends */
	    if (tcode)
		ERROR_EXIT(tcode);

	    dparamsPtr->curVolume--;	/* redump this volume */
	    continue;
	    break;

	case DUMP_NORETRYEOT:
	    ErrorLog(1, taskId, 0, 0,
		     "Volume %s (%u) failed - volume larger than tape\n",
		     curDump->name, curDump->vid);

	    /* rewrite the label on the tape - rewind - no need to switch tapes */
	    tcode = butm_Create(tapeInfoPtr, &dparamsPtr->tapeLabel, 1);
	    if (tcode) {
		ErrorLog(0, taskId, tcode, tapeInfoPtr->error,
			 "Can't relabel tape\n");

		unmountTape(taskId, tapeInfoPtr);
		tcode = getDumpTape(dparamsPtr, 1, 0);	/* interactive - no appends */
		if (tcode)
		    ERROR_EXIT(tcode);
	    } else {		/* Record the tape in database */
		tapepos = tapeInfoPtr->position;
		tcode =
		    useTape(&dparamsPtr->tape, dparamsPtr->databaseDumpId,
			    dparamsPtr->tapeName,
			    (dparamsPtr->tapeSeq + dparamsPtr->dump.tapes.b),
			    dparamsPtr->tapeLabel.useCount,
			    dparamsPtr->tapeLabel.creationTime,
			    dparamsPtr->tapeLabel.expirationDate, tapepos);
	    }

	    curDump->hostAddr = 0;
	    dparamsPtr->volumesFailed++;
	    break;

	case DUMP_NODUMP:
	    TapeLog(1, taskId, dvcode, 0,
		    "Volume %s (%u) not dumped - has not been modified since last dump\n",
		    curDump->name, curDump->vid);

	    curDump->hostAddr = 0;
	    dparamsPtr->volumesNotDumped++;
	    break;

	default:
	    ErrorLog(1, taskId, dvcode, 0, "Volume %s (%u) failed\n",
		     curDump->name, curDump->vid);
	    ERROR_EXIT(dvcode);
	    break;
	}
    }				/*w */

  error_exit:
    /* check if we terminated while processing a volume */
    if (dparamsPtr->curVolume < nodePtr->arraySize) {
	TapeLog(2, taskId, 0, 0,
		"Terminated while processing Volume %s (%u)\n", curDump->name,
		curDump->vid);
    }

    /* print a summary of this pass */
    TapeLog(2, taskId, 0, 0, "End of pass %d: Volumes remaining = %d\n",
	    passNumber,
	    nodePtr->arraySize - (dparamsPtr->volumesDumped +
				  dparamsPtr->volumesFailed +
				  dparamsPtr->volumesNotDumped));
    return (code);
}

int
Dumper(struct dumpNode *nodePtr)
{
    struct dumpRock dparams;
    struct butm_tapeInfo tapeInfo;
    int pass;
    int action;
    afs_int32 taskId;
    afs_int32 code = 0;

    /* for volume setup */
    int i;
    int failedvolumes = 0;
    int dumpedvolumes = 0;
    int nodumpvolumes = 0;
    char strlevel[5];
    char msg[20];
    char finishedMsg1[50];
    char finishedMsg2[50];
    time_t startTime = 0;
    time_t endTime = 0;
    afs_int32 allocbufferSize;

    extern struct deviceSyncNode *deviceLatch;
    extern struct tapeConfig globalTapeConfig;
    extern afs_int32 createDump();

    taskId = nodePtr->taskID;	/* Get task Id */
    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);
    clearStatus(taskId, DRIVE_WAIT);

    printf("\n\n");
    TapeLog(2, taskId, 0, 0, "Dump %s\n", nodePtr->dumpSetName);

    /* setup the dump parameters */
    memset(&dparams, 0, sizeof(dparams));
    dparams.node = nodePtr;
    dparams.tapeInfoPtr = &tapeInfo;
    dlqInit(&savedEntries);

    if (!CONF_XBSA) {
	/* Instantiate the tape module */
	tapeInfo.structVersion = BUTM_MAJORVERSION;
	code = butm_file_Instantiate(&tapeInfo, &globalTapeConfig);
	if (code) {
	    ErrorLog(0, taskId, code, tapeInfo.error,
		     "Can't initialize the tape module\n");
	    ERROR_EXIT(code);
	}
    }

    /* check if abort requested while waiting on device latch */
    if (checkAbortByTaskId(taskId))
	ERROR_EXIT(TC_ABORTEDBYREQUEST);

    /* Are there volumes to dump */
    if (nodePtr->arraySize == 0) {
	TLog(taskId, "Dump (%s), no volumes to dump\n", nodePtr->dumpSetName);
	ERROR_EXIT(0);
    }

    /* Allocate a buffer for the dumps. Leave room for header and vol-trailer.
     * dataSize is amount of data to read in each rx_Read() call.
     */
    if (CONF_XBSA) {
	/* XBSA dumps have not header */
	dataSize = BufferSize;
	allocbufferSize = dataSize + sizeof(struct volumeHeader);
    } else {
	tapeblocks = BufferSize / BUTM_BLOCKSIZE;	/* # of 16K tapeblocks */
	dataSize = (tapeblocks * BUTM_BLKSIZE);
	allocbufferSize =
	    BUTM_HDRSIZE + dataSize + sizeof(struct volumeHeader);
    }
    bufferBlock = NULL;
    bufferBlock = malloc(allocbufferSize);
    if (!bufferBlock) {
	ErrorLog(0, taskId, TC_NOMEMORY, 0,
		 "Can't allocate BUFFERSIZE for dumps\n");
	ERROR_EXIT(TC_NOMEMORY);
    }

    /* Determine the dumpid of the most recent dump of this volumeset and dumplevel
     * Used when requesting a tape. Done now because once we create the dump, the
     * routine will then find the newly created dump.
     */
    sprintf(strlevel, "%d", nodePtr->level);
    code =
	bcdb_FindLatestDump(nodePtr->volumeSetName, strlevel,
			    &dparams.lastDump);
    if (code) {
	if (code != BUDB_NODUMPNAME) {
	    ErrorLog(0, taskId, code, 0, "Can't read backup database\n");
	    ERROR_EXIT(code);
	}
	memset(&dparams.lastDump, 0, sizeof(dparams.lastDump));
    }

    code = createDump(&dparams);	/* enter dump into database */
    if (code) {
	ErrorLog(0, taskId, code, 0, "Can't create dump in database\n");
	ERROR_EXIT(code);
    }

    TLog(taskId, "Dump %s (DumpID %u)\n", nodePtr->dumpSetName,
	 dparams.databaseDumpId);

    if (!CONF_XBSA) {
	/* mount the tape and write its label */
	code = getDumpTape(&dparams, autoQuery, nodePtr->doAppend);
    } else {
	/* Create a dummy tape to satisfy backup databae */
	code = getXBSATape(&dparams);
	tapeInfo.position = 1;
    }
    if (code) {
	/* If didn't write the label, remove dump from the database */
	if (!dparams.wroteLabel) {
	    i = bcdb_deleteDump(dparams.databaseDumpId, 0, 0, 0);
	    if (i && (i != BUDB_NOENT))
		ErrorLog(1, taskId, i, 0,
			 "Warning: Can't delete dump %u from database\n",
			 dparams.databaseDumpId);
	    else
		dparams.databaseDumpId = 0;
	}
	ERROR_EXIT(code);	/* exit with code from getTape */
    }

    startTime = time(0);
    for (pass = 1; pass <= maxpass; pass++) {
	lastPass = (pass == maxpass);
	code = dumpPass(&dparams, pass);
	if (code)
	    ERROR_EXIT(code);

	/* if no failed volumes, we're done */
	if ((dparams.volumesDumped + dparams.volumesFailed +
	     dparams.volumesNotDumped) == nodePtr->arraySize)
	    break;
    }

    /* 
     * Log the error but ignore it since the dump is effectively done.
     * Scantape may assume another volume and ask for next tape.
     */
    if (!CONF_XBSA) {
	code = butm_WriteEOT(&tapeInfo);
	if (code)
	    TapeLog(taskId, code, tapeInfo.error,
		    "Warning: Can't write end-of-dump on tape\n");
    }

    code =
	finishTape(&dparams.tape,
		   dparams.tapeInfoPtr->kBytes +
		   (dparams.tapeInfoPtr->nBytes ? 1 : 0));
    if (code)
	ERROR_EXIT(code);

    code = finishDump(&dparams.dump);
    if (code)
	ERROR_EXIT(code);

    action = dparams.curVolumeStatus;
    code = flushSavedEntries(action);
    if (code)
	ERROR_EXIT(code);

  error_exit:
    endTime = time(0);
    Bind(0);
    if (bufferBlock)
	free(bufferBlock);

    if (!CONF_XBSA) {
	unmountTape(taskId, &tapeInfo);
    }
    waitDbWatcher();

    dumpedvolumes = dparams.volumesDumped;
    nodumpvolumes = dparams.volumesNotDumped;
    failedvolumes = nodePtr->arraySize - (dumpedvolumes + nodumpvolumes);

    /* pass back the number of volumes we failed to dump */
    lock_Status();
    nodePtr->statusNodePtr->volsFailed = failedvolumes;
    unlock_Status();

    lastPass = 1;		/* In case we aborted */

    DUMPNAME(finishedMsg1, nodePtr->dumpSetName, dparams.databaseDumpId);
    sprintf(finishedMsg2, "%d volumes dumped", dumpedvolumes);
    if (failedvolumes) {
	sprintf(msg, ", %d failed", failedvolumes);
	strcat(finishedMsg2, msg);
    }
    if (nodumpvolumes) {
	sprintf(msg, ", %d unchanged", nodumpvolumes);
	strcat(finishedMsg2, msg);
    }

    if (code == TC_ABORTEDBYREQUEST) {
	ErrorLog(0, taskId, 0, 0, "%s: Aborted by request. %s\n",
		 finishedMsg1, finishedMsg2);
	clearStatus(taskId, ABORT_REQUEST);
	setStatus(taskId, ABORT_DONE);
    } else if (code) {
	ErrorLog(0, taskId, code, 0, "%s: Finished with errors. %s\n",
		 finishedMsg1, finishedMsg2);
	setStatus(taskId, TASK_ERROR);
    } else {
	TLog(taskId, "%s: Finished. %s\n", finishedMsg1, finishedMsg2);
    }
    lastPass = 0;

    /* Record how long the dump took */
    if (centralLogIO && startTime) {
	long timediff;
	afs_int32 hrs, min, sec, tmp;
	char line[1024];
	struct tm tmstart, tmend;

	localtime_r(&startTime, &tmstart);
	localtime_r(&endTime, &tmend);
	timediff = (int)endTime - (int)startTime;
	hrs = timediff / 3600;
	tmp = timediff % 3600;
	min = tmp / 60;
	sec = tmp % 60;

	sprintf(line,
		"%-5d  %02d/%02d/%04d %02d:%02d:%02d  "
		"%02d/%02d/%04d %02d:%02d:%02d  " "%02d:%02d:%02d  "
		"%s %d of %d volumes dumped (%ld KB)\n", taskId,
		tmstart.tm_mon + 1, tmstart.tm_mday, tmstart.tm_year + 1900,
		tmstart.tm_hour, tmstart.tm_min, tmstart.tm_sec,
		tmend.tm_mon + 1, tmend.tm_mday, tmend.tm_year + 1900,
		tmend.tm_hour, tmend.tm_min, tmend.tm_sec, hrs, min, sec,
		nodePtr->volumeSetName, dumpedvolumes,
		dumpedvolumes + failedvolumes,
		dparams.tapeInfoPtr->kBytes + 1);

	fwrite(line, strlen(line), 1, centralLogIO);
	fflush(centralLogIO);
    }

    setStatus(taskId, TASK_DONE);

    FreeNode(taskId);		/* free the dump node */
    LeaveDeviceQueue(deviceLatch);
    return (code);
}

#define BELLTIME 60		/* 60 seconds before a bell rings */
#define BELLCHAR 7		/* ascii for bell */

/* retryPrompt
 * 	prompt the user to decide how to handle a failed volume dump. The
 *	volume parameters describe the volume that failed
 * entry:
 *	volumeName - name of volume
 *	volumeId - volume id 
 *	taskId - for job contrl
 * fn return:
 *	character typed by user, one of r, o or a
 */

char
retryPrompt(char *volumeName, afs_int32 volumeId, afs_uint32 taskId)
{
    afs_int32 start;
    char ch;
    afs_int32 code = 0;

    setStatus(taskId, OPR_WAIT);
    printf("\nDump of volume %s (%u) failed\n\n", volumeName, volumeId);

    printf("Please select action to be taken for this volume\n");

  again:
    printf("r - retry, try dumping this volume again\n");
    printf("o - omit,  this volume from this dump\n");
    printf("a - abort, the entire dump\n");

    while (1) {
	FFlushInput(stdin);
	putchar(BELLCHAR);
	fflush(stdout);

	start = time(0);
	while (1) {
#ifdef AFS_PTHREAD_ENV
	    code = GetResponseKey(5, &ch);	/* ch stores key pressed */
#else
	    code = LWP_GetResponseKey(5, &ch);	/* ch stores key pressed */
#endif
	    if (code == 1)
		break;		/* input is available */

	    if (checkAbortByTaskId(taskId)) {
		clearStatus(taskId, OPR_WAIT);
		printf
		    ("This tape operation has been aborted by the coordinator\n");
		return 'a';
	    }

	    if (time(0) > start + BELLTIME)
		break;
	}
	/* otherwise, we should beep again, check for abort and go back,
	 * since the GetResponseKey() timed out.
	 */
	if (code == 1)
	    break;		/* input is available */
    }
    clearStatus(taskId, OPR_WAIT);
    if (ch != 'r' && ch != 'o' && ch != 'a') {
	printf("Please select one of the 3 options, r, o or a\n");
	goto again;
    }

    return ch;
}

/* For testing: it prints the tape label */
int
printTapeLabel(struct butm_tapeLabel *tl)
{
    printf("Tape Label\n");
    printf("   structVersion  = %d\n", tl->structVersion);
    printf("   creationTime   = %u\n", tl->creationTime);
    printf("   expirationDate = %u\n", tl->expirationDate);
    printf("   AFSName        = %s\n", tl->AFSName);
    printf("   cell           = %s\n", tl->cell);
    printf("   dumpid         = %d\n", tl->dumpid);
    printf("   useCount       = %d\n", tl->useCount);
    printf("   comment        = %s\n", tl->comment);
    printf("   pName          = %s\n", tl->pName);
    printf("   size           = %u\n", tl->size);
    printf("   dumpPath       = %s\n", tl->dumpPath);
    return 0;
}

/* getXBSATape
 *      Create a tape structure to be satisfy the backup database
 *      even though we don't really use a tape with XBSA.
 */
int
getXBSATape(struct dumpRock *dparamsPtr)
{
    struct dumpNode *nodePtr = dparamsPtr->node;
    struct butm_tapeInfo *tapeInfoPtr = dparamsPtr->tapeInfoPtr;
    struct butm_tapeLabel *tapeLabelPtr = &dparamsPtr->tapeLabel;
    afs_int32 code = 0;

    tc_MakeTapeName(dparamsPtr->tapeName, &nodePtr->tapeSetDesc,
		    dparamsPtr->tapeSeq);

    GetNewLabel(tapeInfoPtr, "" /*pName */ , dparamsPtr->tapeName,
		tapeLabelPtr);
    strcpy(tapeLabelPtr->dumpPath, nodePtr->dumpName);
    tapeLabelPtr->dumpid = dparamsPtr->databaseDumpId;
    tapeLabelPtr->expirationDate =
	calcExpirationDate(nodePtr->tapeSetDesc.expType,
			   nodePtr->tapeSetDesc.expDate, time(0));

    /* printTapeLabel(tapeLabelPtr); For testing */

    code =
	useTape(&dparamsPtr->tape, dparamsPtr->databaseDumpId,
		dparamsPtr->tapeName,
		(dparamsPtr->tapeSeq + dparamsPtr->dump.tapes.b),
		tapeLabelPtr->useCount, tapeLabelPtr->creationTime,
		tapeLabelPtr->expirationDate, 0 /*tape position */ );
    return (code);
}

/* getDumpTape
 *	iterate until the desired tape (as specified by the dump structures)
 *	is mounted.
 * entry:
 *	interactiveFlag
 *		0 - assume the tape is there. Prompt if assumption false
 *		1 - prompt regardless
 */

int
getDumpTape(struct dumpRock *dparamsPtr, int interactiveFlag,
	    afs_int32 append)
{
    struct dumpNode *nodePtr = dparamsPtr->node;
    struct butm_tapeInfo *tapeInfoPtr = dparamsPtr->tapeInfoPtr;
    struct butm_tapeLabel *newTapeLabelPtr = &dparamsPtr->tapeLabel;
    char AFSTapeName[TC_MAXTAPENAMELEN];
    afs_int32 taskId = nodePtr->taskID;
    struct butm_tapeLabel oldTapeLabel;
    struct budb_dumpEntry dumpEntry;
    struct budb_tapeEntry tapeEntry;
    struct budb_volumeEntry volEntry;
    Date expir;
    afs_int32 doAppend;
    afs_int32 code = 0;
    int askForTape;
    int tapecount = 1;
    char strlevel[5];
    afs_int32 tapepos, lastpos;

    extern struct tapeConfig globalTapeConfig;
    extern struct udbHandleS udbHandle;

    askForTape = interactiveFlag;
    dparamsPtr->wroteLabel = 0;

    /* Keep prompting for a tape until we get it right */
    while (1) {
	/* What the name of the tape would be if not appending to it */
	tc_MakeTapeName(AFSTapeName, &nodePtr->tapeSetDesc,
			dparamsPtr->tapeSeq);

	doAppend = append;

	if (askForTape) {
	    code =
		PromptForTape((doAppend ? APPENDOPCODE : WRITEOPCODE),
			      AFSTapeName, dparamsPtr->databaseDumpId, taskId,
			      tapecount);
	    if (code)
		ERROR_EXIT(code);
	}
	askForTape = 1;
	tapecount++;

	/* open the tape device */
	code = butm_Mount(tapeInfoPtr, AFSTapeName);
	if (code) {
	    TapeLog(0, taskId, code, tapeInfoPtr->error, "Can't open tape\n");
	    goto getNewTape;
	}

	/* Read the tape label */
	code = butm_ReadLabel(tapeInfoPtr, &oldTapeLabel, 1);	/* rewind */
	if (code) {
	    if (tapeInfoPtr->error) {
		ErrorLog(0, taskId, code, tapeInfoPtr->error,
			 "Warning: Tape error while reading label (will proceed with dump)\n");
	    }
	    memset(&oldTapeLabel, 0, sizeof(oldTapeLabel));
	}

	/* Check if null tape. Prior 3.3, backup tapes have no dump id */
	if ((strcmp(oldTapeLabel.AFSName, "") == 0)
	    && (oldTapeLabel.dumpid == 0)) {
	    if (doAppend) {
		TLog(taskId,
		     "Dump not found on tape. Proceeding with initial dump\n");
		doAppend = 0;
	    }
	} else if (doAppend) {	/* appending */
	    /* Check that we don't have a database dump tape */
	    if (databaseTape(oldTapeLabel.AFSName)) {
		char gotName[BU_MAXTAPELEN + 32];

		/* label does not match */
		LABELNAME(gotName, &oldTapeLabel);
		TLog(taskId, "Can't append to database tape %s\n", gotName);
		goto getNewTape;
	    }

	    /* Verify that the tape is of version 4 (AFS 3.3) or greater */
	    if (oldTapeLabel.structVersion < TAPE_VERSION_4) {
		TLog(taskId,
		     "Can't append: requires tape version %d or greater\n",
		     TAPE_VERSION_4);
		goto getNewTape;
	    }

	    /* Verify that the last tape of the dump set is in the drive.
	     * volEntry will be zeroed if last dump has no volume entries.
	     */
	    code =
		bcdb_FindLastTape(oldTapeLabel.dumpid, &dumpEntry, &tapeEntry,
				  &volEntry);
	    if (code) {
		ErrorLog(0, taskId, code, 0,
			 "Can't append: Can't find last volume of dumpId %u in database\n",
			 oldTapeLabel.dumpid);
		printf("Please scan the dump in or choose another tape\n");
		goto getNewTape;
	    }
	    lastpos =
		(volEntry.position ? volEntry.position : tapeEntry.labelpos);

	    if (strcmp(TNAME(&oldTapeLabel), tapeEntry.name)) {
		char expName[BU_MAXTAPELEN + 32], gotName[BU_MAXTAPELEN + 32];

		TAPENAME(expName, tapeEntry.name, oldTapeLabel.dumpid);
		LABELNAME(gotName, &oldTapeLabel);

		TLog(taskId,
		     "Can't append: Last tape in dump-set is %s, label seen %s\n",
		     expName, gotName);
		goto getNewTape;
	    }

	    /* After reading the tape label, we now know what it is */
	    strcpy(AFSTapeName, oldTapeLabel.AFSName);	/* the real name */
	    strcpy(tapeInfoPtr->name, oldTapeLabel.AFSName);	/* the real name */

	    /* Position after last volume on the tape */
	    code = butm_SeekEODump(tapeInfoPtr, lastpos);
	    if (code) {
		ErrorLog(0, taskId, code, tapeInfoPtr->error,
			 "Can't append: Can't position to end of dump on tape %s\n",
			 tapeEntry.name);
		goto getNewTape;
	    }

	    /* Track size of tape - set after seek since seek changes the value */
	    tapeInfoPtr->kBytes = tapeEntry.useKBytes;
	} else {		/* not appending */

	    afs_uint32 tapeid;
	    afs_uint32 dmp;
	    struct budb_dumpEntry de, de2;

	    /* Check if tape name is not what expected - null tapes are acceptable
	     * Don't do check if the tape has a user defined label.
	     */
	    if (dump_namecheck && (strcmp(oldTapeLabel.pName, "") == 0)) {
		if (strcmp(oldTapeLabel.AFSName, "") &&	/* not null tape */
		    strcmp(oldTapeLabel.AFSName, AFSTapeName)) {	/* not expected name */
		    TLog(taskId, "Tape label expected %s, label seen %s\n",
			 AFSTapeName, oldTapeLabel.AFSName);
		    goto getNewTape;
		}

		/* Check that we don't have a database dump tape */
		if (databaseTape(oldTapeLabel.AFSName)) {
		    /* label does not match */
		    TLog(taskId,
			 "Tape label expected %s, can't dump to database tape %s\n",
			 AFSTapeName, oldTapeLabel.AFSName);
		    goto getNewTape;
		}
	    }

	    /* Verify the tape has not expired - only check if not appending */
	    if (!tapeExpired(&oldTapeLabel)) {
		TLog(taskId, "This tape has not expired\n");
		goto getNewTape;
	    }

	    /* Given a tape dump with good data, verify we don't overwrite recent dumps
	     * and also verify that the volume will be restorable - if not print warnings
	     */
	    if (oldTapeLabel.dumpid) {
		/* Do not overwrite a tape that belongs to the dump's dumpset */
		tapeid =
		    (dparamsPtr->initialDumpId ? dparamsPtr->
		     initialDumpId : dparamsPtr->databaseDumpId);
		if (oldTapeLabel.dumpid == tapeid) {
		    ErrorLog(0, taskId, 0, 0,
			     "Can't overwrite tape containing the dump in progress\n");
		    goto getNewTape;
		}

		/* Since the dumpset on this tape will be deleted from database, check if
		 * any of the dump's parent-dumps are on this tape.
		 */
		for (dmp = nodePtr->parent; dmp; dmp = de.parent) {
		    code = bcdb_FindDumpByID(dmp, &de);
		    if (code) {
			ErrorLog(0, taskId, 0, 0,
				 "Warning: Can't find parent dump %u in backup database\n",
				 dmp);
			break;
		    }

		    tapeid = (de.initialDumpID ? de.initialDumpID : de.id);
		    if (oldTapeLabel.dumpid == tapeid) {
			ErrorLog(0, taskId, 0, 0,
				 "Can't overwrite the parent dump %s (DumpID %u)\n",
				 de.name, de.id);
			goto getNewTape;
		    }
		}

		/* Since the dumpset on this tape will be deleted from database, check if
		 * any of the dumps in this dumpset are most-recent-dumps.
		 */
		for (dmp = oldTapeLabel.dumpid; dmp; dmp = de.appendedDumpID) {
		    if (dmp == dparamsPtr->lastDump.id) {
			memcpy(&de, &dparamsPtr->lastDump, sizeof(de));
			memcpy(&de2, &dparamsPtr->lastDump, sizeof(de2));
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

		    /* If dump on the tape is the latest dump at this level */
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
	    }			/* if (oldTapeLabel.dumpid) */
	}			/* else not appending */

	/*
	 * Now have the right tape. Create a new label for the tape
	 * Appended labels have the dump's dumpId - labels at beginnings of 
	 *     tape have the initial dump's dumpId.
	 * Appended labels do not increment the useCount.
	 * Labels at beginnings of tape use the most future expiration of the dump set.
	 */
	GetNewLabel(tapeInfoPtr, oldTapeLabel.pName, AFSTapeName,
		    newTapeLabelPtr);
	strcpy(newTapeLabelPtr->dumpPath, nodePtr->dumpName);
	newTapeLabelPtr->expirationDate =
	    calcExpirationDate(nodePtr->tapeSetDesc.expType,
			       nodePtr->tapeSetDesc.expDate, time(0));
	newTapeLabelPtr->dumpid = dparamsPtr->databaseDumpId;
	newTapeLabelPtr->useCount = oldTapeLabel.useCount;

	if (!doAppend) {
	    newTapeLabelPtr->useCount++;
	    if (dparamsPtr->initialDumpId) {
		newTapeLabelPtr->dumpid = dparamsPtr->initialDumpId;
		expir = ExpirationDate(dparamsPtr->initialDumpId);
		if (expir > newTapeLabelPtr->expirationDate)
		    newTapeLabelPtr->expirationDate = expir;
	    }
	}

	/* write the label on the tape - rewind if not appending and vice-versa */
	code = butm_Create(tapeInfoPtr, newTapeLabelPtr, !doAppend);
	if (code) {
	    char gotName[BU_MAXTAPELEN + 32];

	    LABELNAME(gotName, newTapeLabelPtr);
	    TapeLog(0, taskId, code, tapeInfoPtr->error,
		    "Can't label tape as %s\n", gotName);
	    goto getNewTape;
	}
	dparamsPtr->wroteLabel = 1;	/* Remember we wrote the label */
	tapepos = tapeInfoPtr->position - 1;

	strcpy(dparamsPtr->tapeName, TNAME(newTapeLabelPtr));

	/* If appending, set dumpentry in the database as appended. */
	if (doAppend) {
	    char gotName[BU_MAXTAPELEN + 32];

	    nodePtr->tapeSetDesc.b = extractTapeSeq(AFSTapeName);
	    dparamsPtr->dump.tapes.b = nodePtr->tapeSetDesc.b;
	    dparamsPtr->initialDumpId = oldTapeLabel.dumpid;
	    strcpy(nodePtr->tapeSetDesc.format, dumpEntry.tapes.format);

	    code =
		bcdb_MakeDumpAppended(dparamsPtr->databaseDumpId,
				      dparamsPtr->initialDumpId,
				      nodePtr->tapeSetDesc.b);
	    if (code)
		ErrorLog(2, taskId, code, 0,
			 "Warning: Can't append dump %u to dump %u in database\n",
			 dparamsPtr->databaseDumpId,
			 dparamsPtr->initialDumpId);

	    LABELNAME(gotName, &oldTapeLabel);
	    TLog(taskId, "Appending dump %s (DumpID %u) to tape %s\n",
		 nodePtr->dumpSetName, dparamsPtr->databaseDumpId, gotName);
	}

	/* If not appending, delete overwritten dump from the database */
	else {
	    if ((oldTapeLabel.structVersion >= TAPE_VERSION_3)
		&& oldTapeLabel.dumpid) {
		code = bcdb_deleteDump(oldTapeLabel.dumpid, 0, 0, 0);
		if (code && (code != BUDB_NOENT))
		    ErrorLog(0, taskId, code, 0,
			     "Warning: Can't delete old dump %u from database\n",
			     oldTapeLabel.dumpid);
	    }
	}

	code =
	    useTape(&dparamsPtr->tape, dparamsPtr->databaseDumpId,
		    dparamsPtr->tapeName,
		    (dparamsPtr->tapeSeq + dparamsPtr->dump.tapes.b),
		    newTapeLabelPtr->useCount, newTapeLabelPtr->creationTime,
		    newTapeLabelPtr->expirationDate, tapepos);

	/*
	 * The margin of space to check for end of tape is set to the
	 * amount of space used to write an end-of-tape multiplied by 2.
	 * The amount of space is size of a 16K volume trailer, a 16K File
	 * End mark, its EOF marker, a 16K EODump marker, its EOF marker,
	 * and up to two EOF markers done on close (3 16K blocks + 4 EOF
	 * markers).
	 */
	tc_EndMargin = (3 * 16384 + 4 * globalTapeConfig.fileMarkSize) * 2;
	tc_KEndMargin = tc_EndMargin / 1024;
	break;

      getNewTape:
	unmountTape(taskId, tapeInfoPtr);
    }

  error_exit:
    return (code);
}

int
makeVolumeHeader(struct volumeHeader *vhptr, struct dumpRock *dparamsPtr,
		 int fragmentNumber)
{
    struct dumpNode *nodePtr = dparamsPtr->node;
    struct tc_dumpDesc *curDump;
    afs_int32 code = 0;

    curDump = &nodePtr->dumps[dparamsPtr->curVolume];

    memset(vhptr, 0, sizeof(*vhptr));
    strcpy(vhptr->volumeName, curDump->name);
    vhptr->volumeID = curDump->vid;
    vhptr->cloneDate = curDump->cloneDate;
    vhptr->server = curDump->hostAddr;
    vhptr->part = curDump->partition;
    vhptr->from = curDump->date;
    vhptr->frag = fragmentNumber;
    vhptr->contd = 0;
    vhptr->magic = TC_VOLBEGINMAGIC;
    vhptr->dumpID = dparamsPtr->databaseDumpId;	/* real dump id */
    vhptr->level = nodePtr->level;
    vhptr->parentID = nodePtr->parent;
    vhptr->endTime = 0;
    vhptr->versionflags = CUR_TAPE_VERSION;
    strcpy(vhptr->dumpSetName, nodePtr->dumpSetName);
    strcpy(vhptr->preamble, "H++NAME#");
    strcpy(vhptr->postamble, "T--NAME#");
  
    return (code);
}

int
volumeHeader_hton(struct volumeHeader *hostPtr, struct volumeHeader *netPtr)
{
    struct volumeHeader volHdr;

    strcpy(volHdr.preamble, hostPtr->preamble);
    strcpy(volHdr.postamble, hostPtr->postamble);
    strcpy(volHdr.volumeName, hostPtr->volumeName);
    strcpy(volHdr.dumpSetName, hostPtr->dumpSetName);
    volHdr.volumeID = htonl(hostPtr->volumeID);
    volHdr.server = htonl(hostPtr->server);
    volHdr.part = htonl(hostPtr->part);
    volHdr.from = htonl(hostPtr->from);
    volHdr.frag = htonl(hostPtr->frag);
    volHdr.magic = htonl(hostPtr->magic);
    volHdr.contd = htonl(hostPtr->contd);
    volHdr.dumpID = htonl(hostPtr->dumpID);
    volHdr.level = htonl(hostPtr->level);
    volHdr.parentID = htonl(hostPtr->parentID);
    volHdr.endTime = htonl(hostPtr->endTime);
    volHdr.versionflags = htonl(hostPtr->versionflags);
    volHdr.cloneDate = htonl(hostPtr->cloneDate);

    memcpy(netPtr, &volHdr, sizeof(struct volumeHeader));
    return 0;
}

/* database related routines */

afs_int32
createDump(struct dumpRock *dparamsPtr)
{
    struct dumpNode *nodePtr = dparamsPtr->node;
    struct budb_dumpEntry *dumpPtr;
    afs_int32 code = 0;

    dumpPtr = &dparamsPtr->dump;
    memset(dumpPtr, 0, sizeof(*dumpPtr));

    /* id filled in by database */
    dumpPtr->parent = nodePtr->parent;
    dumpPtr->level = nodePtr->level;
    dumpPtr->flags = 0;
#ifdef xbsa
    if (CONF_XBSA) {
	if (xbsaType == XBSA_SERVER_TYPE_ADSM) {
	    strcpy(dumpPtr->tapes.tapeServer, butxInfo.serverName);
	    dumpPtr->flags = BUDB_DUMP_ADSM;
	}
	if (!(butxInfo.serverType & XBSA_SERVER_FLAG_MULTIPLE)) {
	    /* The current server (API) doesn't provide the function required
	     * to specify a server at startup time.  For that reason, we can't
	     * be sure that the server name supplied by the user in the user-
	     * defined configuration file is correct.  We set a flag here so
	     * we know at restore time that the servername info in the backup
	     * database may be incorrect.  We will not allow a server switch
	     * at that time, even if the server at restore time supports
	     * multiple servers.
	     */
	    dumpPtr->flags |= BUDB_DUMP_XBSA_NSS;
	}
    }
#endif
    strcpy(dumpPtr->volumeSetName, nodePtr->volumeSetName);
    strcpy(dumpPtr->dumpPath, nodePtr->dumpName);
    strcpy(dumpPtr->name, nodePtr->dumpSetName);
    dumpPtr->created = 0;	/* let database assign it */
    dumpPtr->incTime = 0;	/* not really used */
    dumpPtr->nVolumes = 0;
    dumpPtr->initialDumpID = 0;

    dumpPtr->tapes.id = groupId;
    dumpPtr->tapes.b = 1;
    dumpPtr->tapes.maxTapes = 0;
    strcpy(dumpPtr->tapes.format, nodePtr->tapeSetDesc.format);

    /* principal filled in by database */

    /* now call the database to create the entry */
    code = bcdb_CreateDump(dumpPtr);
    if (code == 0)
	dparamsPtr->databaseDumpId = dumpPtr->id;

    return (code);
}

#ifdef xbsa
/* InitToServer:
 * Initialize to a specific server. The first time, we remember the
 * server as the original server and go back to it each time we pass 0
 * as the server.
 */
afs_int32
InitToServer(afs_int32 taskId, struct butx_transactionInfo * butxInfoP,
	     char *server)
{
    static char origserver[BSA_MAX_DESC];
    static int init = 0;
    afs_int32 rc, code = 0;

    if (!init) {
	strcpy(origserver, "");
	init = 1;
    }

    if (!server)
	server = origserver;	/* return to original server */
    if (strcmp(server, "") == 0)
	return 0;		/* No server, do nothing */
    if (strcmp(butxInfoP->serverName, server) == 0)
	return 0;		/* same server, do nothing */
    if (strcmp(origserver, "") == 0)
	strcpy(origserver, server);	/* remember original server */

    if (strcmp(butxInfoP->serverName, "") != 0) {
	/* If already connected to a server, disconnect from it.
	 * Check to see if our server does not support switching.
	 */
	if (!(butxInfo.serverType & XBSA_SERVER_FLAG_MULTIPLE)) {
	    ErrorLog(0, taskId, TC_BADTASK, 0,
		     "This version of XBSA libraries does not support switching "
		     "from server %s to server %s\n", butxInfoP->serverName,
		     server);
	    return (TC_BADTASK);
	}

	rc = xbsa_Finalize(&butxInfo);
	if (rc != XBSA_SUCCESS) {
	    ErrorLog(0, taskId, rc, 0,
		     "InitToServer: Unable to terminate the connection to server %s\n",
		     butxInfoP->serverName);
	    ERROR_EXIT(rc);
	}
    }

    /* initialize to the new server */
    rc = xbsa_Initialize(&butxInfo, xbsaObjectOwner, appObjectOwner,
			 xbsaSecToken, server);
    if (rc != XBSA_SUCCESS) {
	ErrorLog(0, taskId, rc, 0,
		 "InitToServer: Unable to initialize the XBSA library to server %s\n",
		 server);
	ERROR_EXIT(rc);
    }

  error_exit:
    return (code);
}


/* DeleteDump
 *
 */
int
DeleteDump(struct deleteDumpIf *ptr)
{
    afs_int32 taskId;
    afs_int32 rc, code = 0;
    afs_uint32 dumpid;
    afs_int32 index, next, dbTime;
    budb_volumeList vl;
    struct budb_dumpEntry dumpEntry;
    char tapeName[BU_MAXTAPELEN];
    char dumpIdStr[XBSA_MAX_OSNAME];
    char volumeNameStr[XBSA_MAX_PATHNAME];
    afs_int32 i;
    int intrans = 0;
    int allnotfound = 1, onenotfound = 0;
    extern struct udbHandleS udbHandle;
    extern struct deviceSyncNode *deviceLatch;

    setStatus(taskId, DRIVE_WAIT);
    EnterDeviceQueue(deviceLatch);
    clearStatus(taskId, DRIVE_WAIT);

    dumpid = ptr->dumpID;
    taskId = ptr->taskId;	/* Get task Id */

    printf("\n\n");
    TapeLog(2, taskId, 0, 0, "Delete Dump %u\n", dumpid);

    vl.budb_volumeList_len = 0;
    vl.budb_volumeList_val = 0;
    tapeName[0] = '\0';

    /* Get the dump info for the dump we are deleting */
    rc = bcdb_FindDumpByID(dumpid, &dumpEntry);
    if (rc) {
	ErrorLog(0, taskId, rc, 0,
		 "Unable to locate dump ID %u in database\n", dumpid);
	setStatus(taskId, TASK_ERROR);
	ERROR_EXIT(rc);
    }

    /* we must make sure that we are configured with the correct type of
     * XBSA server for this dump delete! Only those dumped to an ADSM server.
     */
    if ((xbsaType == XBSA_SERVER_TYPE_ADSM)
	&& !((dumpEntry.flags & (BUDB_DUMP_ADSM | BUDB_DUMP_BUTA)))) {
	ErrorLog(0, taskId, TC_BADTASK, 0,
		 "The dump %u requested for deletion is incompatible with this instance of butc\n",
		 dumpid);
	setStatus(taskId, TASK_ERROR);
	ERROR_EXIT(TC_BADTASK);
    }

    /* Make sure we are connected to the correct server. If not, switch to it if appropriate */
    if ((strlen((char *)dumpEntry.tapes.tapeServer) != 0)
	&& (strcmp((char *)dumpEntry.tapes.tapeServer, butxInfo.serverName) !=
	    0)) {

	/* Check to see if the tapeServer name is trustworthy */
	if ((dumpEntry.flags & (BUDB_DUMP_XBSA_NSS | BUDB_DUMP_BUTA))
	    && !forcemultiple) {
	    /* The dump was made with a version of the XBSA interface
	     * that didn't allow switching of servers, we can't be sure
	     * that the servername in the backup database is correct.  So,
	     * we will check the servername and log it if they don't match;
	     * but we will try to do the delete without switching servers.
	     */
	    TLog(taskId,
		 "The dump %d requested for deletion is on server %s "
		 "but butc is connected to server %s "
		 "(Attempting to delete the dump anyway)\n", dumpid,
		 (char *)dumpEntry.tapes.tapeServer, butxInfo.serverName);
	} else {
	    TLog(taskId,
		 "The dump %u requested for deletion is on server %s "
		 "but butc is connected to server %s "
		 "(switching servers)\n", dumpid,
		 (char *)dumpEntry.tapes.tapeServer, butxInfo.serverName);

	    rc = InitToServer(taskId, &butxInfo,
			      (char *)dumpEntry.tapes.tapeServer);
	    if (rc != XBSA_SUCCESS) {
		setStatus(taskId, TASK_ERROR);
		ERROR_EXIT(rc);
	    }
	}
    }

    /* Start a new Transaction */
    rc = xbsa_BeginTrans(&butxInfo);
    if (rc != XBSA_SUCCESS) {
	ErrorLog(0, taskId, rc, 0, "Unable to create a new transaction\n");
	setStatus(taskId, TASK_ERROR);
	ERROR_EXIT(rc);
    }
    intrans = 1;

    /* Query the backup database for list of volumes to delete */
    for (index = next = 0; index != -1; index = next) {
	rc = ubik_Call_SingleServer(BUDB_GetVolumes, udbHandle.uh_client,
				    UF_SINGLESERVER, BUDB_MAJORVERSION,
				    BUDB_OP_DUMPID, tapeName, dumpid, 0,
				    index, &next, &dbTime, &vl);
	if (rc) {
	    if (rc == BUDB_ENDOFLIST)
		break;
	    ErrorLog(0, taskId, rc, 0, "Can't find volume info for dump %d\n",
		     dumpid);
	    setStatus(taskId, TASK_ERROR);
	    ERROR_EXIT(rc);
	}

	/* Delete all volumes on the list */
	for (i = 0; i < vl.budb_volumeList_len; i++) {
	    if (dumpEntry.flags & BUDB_DUMP_BUTA) {
		/* dump was from buta, use old buta style names */
		sprintf(dumpIdStr, "/%d", dumpid);
		strcpy(volumeNameStr, "/");
		strcat(volumeNameStr, (char *)vl.budb_volumeList_val[i].name);
	    } else {		/* BUDB_DUMP_ADSM */
		/* dump was from butc to ADSM, use butc names */
		strcpy(dumpIdStr, butcdumpIdStr);
		sprintf(volumeNameStr, "/%d", dumpid);
		strcat(volumeNameStr, "/");
		strcat(volumeNameStr, (char *)vl.budb_volumeList_val[i].name);
	    }

	    rc = xbsa_DeleteObject(&butxInfo, dumpIdStr, volumeNameStr);
	    if (rc != XBSA_SUCCESS) {
		ErrorLog(0, taskId, rc, 0,
			 "Unable to delete the object %s of dump %s from the server\n",
			 volumeNameStr, dumpIdStr);
		/* Don't exit if volume was not found */
		if (rc != BUTX_DELETENOVOL) {
		    allnotfound = 0;
		    ERROR_EXIT(rc);
		}
		onenotfound = 1;
	    } else {
		allnotfound = 0;
		TLog(0,
		     "Deleted volume %s (%u) in dumpID %u from the backup server\n",
		     vl.budb_volumeList_val[i].name,
		     vl.budb_volumeList_val[i].id, dumpid);
	    }
	}

	/* free the memory allocated by RPC for this list */
	if (vl.budb_volumeList_val)
	    free(vl.budb_volumeList_val);
	vl.budb_volumeList_len = 0;
	vl.budb_volumeList_val = 0;
    }

  error_exit:
    if (intrans) {
	rc = xbsa_EndTrans(&butxInfo);
	if (rc != XBSA_SUCCESS) {
	    ErrorLog(0, taskId, rc, 0,
		     "Unable to terminate the current transaction\n");
	    setStatus(taskId, TASK_ERROR);
	    ERROR_EXIT(rc);
	}
    }

    /* Switch back to the original server */
    rc = InitToServer(taskId, &butxInfo, NULL);

    if (vl.budb_volumeList_val)
	free(vl.budb_volumeList_val);

    setStatus(taskId, TASK_DONE);
    FreeNode(taskId);		/* free the dump node */
    LeaveDeviceQueue(deviceLatch);

    /* If we don't find any dumps on the server, rather than returning
     * a success, return a failure.
     */
    if (!code && onenotfound && allnotfound) {
	code = BUTX_DELETENOVOL;
	setStatus(taskId, TASK_ERROR);
    }
    return (code);
}
#endif
