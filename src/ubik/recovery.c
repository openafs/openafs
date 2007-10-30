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
#include <time.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <sys/time.h>
#endif
#include <assert.h>
#include <lock.h>
#include <string.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <errno.h>
#include <afs/afsutil.h>

#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

/* This module is responsible for determining when the system has
 * recovered to the point that it can handle new transactions.  It
 * replays logs, polls to determine the current dbase after a crash,
 * and distributes the new database to the others.
 */

/* The sync site associates a version number with each database.  It
 * broadcasts the version associated with its current dbase in every
 * one of its beacon messages.  When the sync site send a dbase to a
 * server, it also sends the db's version.  A non-sync site server can
 * tell if it has the right dbase version by simply comparing the
 * version from the beacon message (uvote_dbVersion) with the version
 * associated with the database (ubik_dbase->version).  The sync site
 * itself simply has one counter to keep track of all of this (again
 * ubik_dbase->version).
 */

/* sync site: routine called when the sync site loses its quorum; this
 * procedure is called "up" from the beacon package.  It resyncs the
 * dbase and nudges the recovery daemon to try to propagate out the
 * changes.  It also resets the recovery daemon's state, since
 * recovery must potentially find a new dbase to propagate out.  This
 * routine should not do anything with variables used by non-sync site
 * servers.
 */

/* if this flag is set, then ubik will use only the primary address 
** ( the address specified in the CellServDB) to contact other 
** ubik servers. Ubik recovery will not try opening connections 
** to the alternate interface addresses. 
*/
int ubikPrimaryAddrOnly;

int
urecovery_ResetState(void)
{
    urecovery_state = 0;
    LWP_NoYieldSignal(&urecovery_state);
    return 0;
}

/* sync site: routine called when a non-sync site server goes down; restarts recovery
 * process to send missing server the new db when it comes back up.
 * This routine should not do anything with variables used by non-sync site servers. 
 */
int
urecovery_LostServer(void)
{
    LWP_NoYieldSignal(&urecovery_state);
    return 0;
}

/* return true iff we have a current database (called by both sync
 * sites and non-sync sites) How do we determine this?  If we're the
 * sync site, we wait until recovery has finished fetching and
 * re-labelling its dbase (it may still be trying to propagate it out
 * to everyone else; that's THEIR problem).  If we're not the sync
 * site, then we must have a dbase labelled with the right version,
 * and we must have a currently-good sync site.
 */
int
urecovery_AllBetter(register struct ubik_dbase *adbase, int areadAny)
{
    register afs_int32 rcode;

    ubik_dprint("allbetter checking\n");
    rcode = 0;


    if (areadAny) {
	if (ubik_dbase->version.epoch > 1)
	    rcode = 1;		/* Happy with any good version of database */
    }

    /* Check if we're sync site and we've got the right data */
    else if (ubeacon_AmSyncSite() && (urecovery_state & UBIK_RECHAVEDB)) {
	rcode = 1;
    }

    /* next, check if we're aux site, and we've ever been sent the
     * right data (note that if a dbase update fails, we won't think
     * that the sync site is still the sync site, 'cause it won't talk
     * to us until a timeout period has gone by.  When we recover, we
     * leave this clear until we get a new dbase */
    else if ((uvote_GetSyncSite() && (vcmp(ubik_dbVersion, ubik_dbase->version) == 0))) {	/* && order is important */
	rcode = 1;
    }

    ubik_dprint("allbetter: returning %d\n", rcode);
    return rcode;
}

/* abort all transactions on this database */
int
urecovery_AbortAll(struct ubik_dbase *adbase)
{
    register struct ubik_trans *tt;
    for (tt = adbase->activeTrans; tt; tt = tt->next) {
	udisk_abort(tt);
    }
    return 0;
}

/* this routine aborts the current remote transaction, if any, if the tid is wrong */
int
urecovery_CheckTid(register struct ubik_tid *atid)
{
    if (ubik_currentTrans) {
	/* there is remote write trans, see if we match, see if this
	 * is a new transaction */
	if (atid->epoch != ubik_currentTrans->tid.epoch
	    || atid->counter > ubik_currentTrans->tid.counter) {
	    /* don't match, abort it */
	    /* If the thread is not waiting for lock - ok to end it */
#if !defined(UBIK_PAUSE)
	    if (ubik_currentTrans->locktype != LOCKWAIT) {
#endif /* UBIK_PAUSE */
		udisk_end(ubik_currentTrans);
#if !defined(UBIK_PAUSE)
	    }
#endif /* UBIK_PAUSE */
	    ubik_currentTrans = (struct ubik_trans *)0;
	}
    }
    return 0;
}

/* log format is defined here, and implicitly in disk.c
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
 *
 * A very simple routine, it just replays the log.  Note that this is a new-value only log, which
 * implies that no uncommitted data is written to the dbase: one writes data to the log, including
 * the commit record, then we allow data to be written through to the dbase.  In our particular
 * implementation, once a transaction is done, we write out the pages to the database, so that
 * our buffer package doesn't have to know about stable and uncommitted data in the memory buffers:
 * any changed data while there is an uncommitted write transaction can be zapped during an
 * abort and the remaining dbase on the disk is exactly the right dbase, without having to read
 * the log.
 */

/* replay logs */
static int
ReplayLog(register struct ubik_dbase *adbase)
{
    afs_int32 opcode;
    register afs_int32 code, tpos;
    int logIsGood;
    afs_int32 len, thisSize, tfile, filePos;
    afs_int32 buffer[4];
    afs_int32 syncFile = -1;
    afs_int32 data[1024];

    /* read the lock twice, once to see whether we have a transaction to deal
     * with that committed, (theoretically, we should support more than one
     * trans in the log at once, but not yet), and once replaying the
     * transactions.  */
    tpos = 0;
    logIsGood = 0;
    /* for now, assume that all ops in log pertain to one transaction; see if there's a commit */
    while (1) {
	code =
	    (*adbase->read) (adbase, LOGFILE, &opcode, tpos,
			     sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    break;
	if (opcode == LOGNEW) {
	    /* handle begin trans */
	    tpos += sizeof(afs_int32);
	} else if (opcode == LOGABORT)
	    break;
	else if (opcode == LOGEND) {
	    logIsGood = 1;
	    break;
	} else if (opcode == LOGTRUNCATE) {
	    tpos += 4;
	    code =
		(*adbase->read) (adbase, LOGFILE, buffer, tpos,
				 2 * sizeof(afs_int32));
	    if (code != 2 * sizeof(afs_int32))
		break;		/* premature eof or io error */
	    tpos += 2 * sizeof(afs_int32);
	} else if (opcode == LOGDATA) {
	    tpos += 4;
	    code =
		(*adbase->read) (adbase, LOGFILE, buffer, tpos,
				 3 * sizeof(afs_int32));
	    if (code != 3 * sizeof(afs_int32))
		break;
	    /* otherwise, skip over the data bytes, too */
	    tpos += buffer[2] + 3 * sizeof(afs_int32);
	} else {
	    ubik_dprint("corrupt log opcode (%d) at position %d\n", opcode,
			tpos);
	    break;		/* corrupt log! */
	}
    }
    if (logIsGood) {
	/* actually do the replay; log should go all the way through the commit record, since
	 * we just read it above. */
	tpos = 0;
	logIsGood = 0;
	syncFile = -1;
	while (1) {
	    code =
		(*adbase->read) (adbase, LOGFILE, &opcode, tpos,
				 sizeof(afs_int32));
	    if (code != sizeof(afs_int32))
		break;
	    if (opcode == LOGNEW) {
		/* handle begin trans */
		tpos += sizeof(afs_int32);
	    } else if (opcode == LOGABORT)
		panic("log abort\n");
	    else if (opcode == LOGEND) {
		tpos += 4;
		code =
		    (*adbase->read) (adbase, LOGFILE, buffer, tpos,
				     2 * sizeof(afs_int32));
		if (code != 2 * sizeof(afs_int32))
		    return UBADLOG;
		code = (*adbase->setlabel) (adbase, 0, buffer);
		if (code)
		    return code;
		logIsGood = 1;
		break;		/* all done now */
	    } else if (opcode == LOGTRUNCATE) {
		tpos += 4;
		code =
		    (*adbase->read) (adbase, LOGFILE, buffer, tpos,
				     2 * sizeof(afs_int32));
		if (code != 2 * sizeof(afs_int32))
		    break;	/* premature eof or io error */
		tpos += 2 * sizeof(afs_int32);
		code =
		    (*adbase->truncate) (adbase, ntohl(buffer[0]),
					 ntohl(buffer[1]));
		if (code)
		    return code;
	    } else if (opcode == LOGDATA) {
		tpos += 4;
		code =
		    (*adbase->read) (adbase, LOGFILE, buffer, tpos,
				     3 * sizeof(afs_int32));
		if (code != 3 * sizeof(afs_int32))
		    break;
		tpos += 3 * sizeof(afs_int32);
		/* otherwise, skip over the data bytes, too */
		len = ntohl(buffer[2]);	/* total number of bytes to copy */
		filePos = ntohl(buffer[1]);
		tfile = ntohl(buffer[0]);
		/* try to minimize file syncs */
		if (syncFile != tfile) {
		    if (syncFile >= 0)
			code = (*adbase->sync) (adbase, syncFile);
		    else
			code = 0;
		    syncFile = tfile;
		    if (code)
			return code;
		}
		while (len > 0) {
		    thisSize = (len > sizeof(data) ? sizeof(data) : len);
		    /* copy sizeof(data) buffer bytes at a time */
		    code =
			(*adbase->read) (adbase, LOGFILE, data, tpos,
					 thisSize);
		    if (code != thisSize)
			return UBADLOG;
		    code =
			(*adbase->write) (adbase, tfile, data, filePos,
					  thisSize);
		    if (code != thisSize)
			return UBADLOG;
		    filePos += thisSize;
		    tpos += thisSize;
		    len -= thisSize;
		}
	    } else {
		ubik_dprint("corrupt log opcode (%d) at position %d\n",
			    opcode, tpos);
		break;		/* corrupt log! */
	    }
	}
	if (logIsGood) {
	    if (syncFile >= 0)
		code = (*adbase->sync) (adbase, syncFile);
	    if (code)
		return code;
	} else {
	    ubik_dprint("Log read error on pass 2\n");
	    return UBADLOG;
	}
    }

    /* now truncate the log, we're done with it */
    code = (*adbase->truncate) (adbase, LOGFILE, 0);
    return code;
}

/* Called at initialization to figure out version of the dbase we really have.
 * This routine is called after replaying the log; it reads the restored labels.
 */
static int
InitializeDB(register struct ubik_dbase *adbase)
{
    register afs_int32 code;

    code = (*adbase->getlabel) (adbase, 0, &adbase->version);
    if (code) {
	/* try setting the label to a new value */
	adbase->version.epoch = 1;	/* value for newly-initialized db */
	adbase->version.counter = 1;
	code = (*adbase->setlabel) (adbase, 0, &adbase->version);
	if (code) {
	    /* failed, try to set it back */
	    adbase->version.epoch = 0;
	    adbase->version.counter = 0;
	    (*adbase->setlabel) (adbase, 0, &adbase->version);
	}
	LWP_NoYieldSignal(&adbase->version);
    }
    return 0;
}

/* initialize the local dbase
 * We replay the logs and then read the resulting file to figure out what version we've really got.
 */
int
urecovery_Initialize(register struct ubik_dbase *adbase)
{
    register afs_int32 code;

    code = ReplayLog(adbase);
    if (code)
	return code;
    code = InitializeDB(adbase);
    return code;
}

/* Main interaction loop for the recovery manager
 * The recovery light-weight process only runs when you're the
 * synchronization site.  It performs the following tasks, if and only
 * if the prerequisite tasks have been performed successfully (it
 * keeps track of which ones have been performed in its bit map,
 * urecovery_state).
 *
 * First, it is responsible for probing that all servers are up.  This
 * is the only operation that must be performed even if this is not
 * yet the sync site, since otherwise this site may not notice that
 * enough other machines are running to even elect this guy to be the
 * sync site.
 *
 * After that, the recovery process does nothing until the beacon and
 * voting modules manage to get this site elected sync site.
 *
 * After becoming sync site, recovery first attempts to find the best
 * database available in the network (it must do this in order to
 * ensure finding the latest committed data).  After finding the right
 * database, it must fetch this dbase to the sync site.
 *
 * After fetching the dbase, it relabels it with a new version number,
 * to ensure that everyone recognizes this dbase as the most recent
 * dbase.
 *
 * One the dbase has been relabelled, this machine can start handling
 * requests.  However, the recovery module still has one more task:
 * propagating the dbase out to everyone who is up in the network.
 */
int
urecovery_Interact(void)
{
    afs_int32 code, tcode;
    struct ubik_server *bestServer = NULL;
    struct ubik_server *ts;
    int dbok, doingRPC, now;
    afs_int32 lastProbeTime, lastDBVCheck;
    /* if we're the sync site, the best db version we've found yet */
    static struct ubik_version bestDBVersion;
    struct ubik_version tversion;
    struct timeval tv;
    int length, tlen, offset, file, nbytes;
    struct rx_call *rxcall;
    char tbuffer[256];
    struct ubik_stat ubikstat;
    struct in_addr inAddr;

    /* otherwise, begin interaction */
    urecovery_state = 0;
    lastProbeTime = 0;
    lastDBVCheck = 0;
    while (1) {
	/* Run through this loop every 4 seconds */
	tv.tv_sec = 4;
	tv.tv_usec = 0;
	IOMGR_Select(0, 0, 0, 0, &tv);

	ubik_dprint("recovery running in state %x\n", urecovery_state);

	/* Every 30 seconds, check all the down servers and mark them
	 * as up if they respond. When a server comes up or found to
	 * not be current, then re-find the the best database and 
	 * propogate it.
	 */
	if ((now = FT_ApproxTime()) > 30 + lastProbeTime) {
	    for (ts = ubik_servers, doingRPC = 0; ts; ts = ts->next) {
		if (!ts->up) {
		    doingRPC = 1;
		    code = DoProbe(ts);
		    if (code == 0) {
			ts->up = 1;
			urecovery_state &= ~UBIK_RECFOUNDDB;
		    }
		} else if (!ts->currentDB) {
		    urecovery_state &= ~UBIK_RECFOUNDDB;
		}
	    }
	    if (doingRPC)
		now = FT_ApproxTime();
	    lastProbeTime = now;
	}

	/* Mark whether we are the sync site */
	if (!ubeacon_AmSyncSite()) {
	    urecovery_state &= ~UBIK_RECSYNCSITE;
	    continue;		/* nothing to do */
	}
	urecovery_state |= UBIK_RECSYNCSITE;

	/* If a server has just come up or if we have not found the 
	 * most current database, then go find the most current db.
	 */
	if (!(urecovery_state & UBIK_RECFOUNDDB)) {
	    bestServer = (struct ubik_server *)0;
	    bestDBVersion.epoch = 0;
	    bestDBVersion.counter = 0;
	    for (ts = ubik_servers; ts; ts = ts->next) {
		if (!ts->up)
		    continue;	/* don't bother with these guys */
		if (ts->isClone)
		    continue;
		code = DISK_GetVersion(ts->disk_rxcid, &ts->version);
		if (code == 0) {
		    /* perhaps this is the best version */
		    if (vcmp(ts->version, bestDBVersion) > 0) {
			/* new best version */
			bestDBVersion = ts->version;
			bestServer = ts;
		    }
		}
	    }
	    /* take into consideration our version. Remember if we,
	     * the sync site, have the best version. Also note that
	     * we may need to send the best version out.
	     */
	    if (vcmp(ubik_dbase->version, bestDBVersion) >= 0) {
		bestDBVersion = ubik_dbase->version;
		bestServer = (struct ubik_server *)0;
		urecovery_state |= UBIK_RECHAVEDB;
	    } else {
		/* Clear the flag only when we know we have to retrieve
		 * the db. Because urecovery_AllBetter() looks at it.
		 */
		urecovery_state &= ~UBIK_RECHAVEDB;
	    }
	    lastDBVCheck = FT_ApproxTime();
	    urecovery_state |= UBIK_RECFOUNDDB;
	    urecovery_state &= ~UBIK_RECSENTDB;
	}
#if defined(UBIK_PAUSE)
	/* it's not possible for UBIK_RECFOUNDDB not to be set here.
	 * However, we might have lost UBIK_RECSYNCSITE, and that
	 * IS important.
	 */
	if (!(urecovery_state & UBIK_RECSYNCSITE))
	    continue;		/* lost sync */
#else
	if (!(urecovery_state & UBIK_RECFOUNDDB))
	    continue;		/* not ready */
#endif /* UBIK_PAUSE */

	/* If we, the sync site, do not have the best db version, then
	 * go and get it from the server that does.
	 */
	if ((urecovery_state & UBIK_RECHAVEDB) || !bestServer) {
	    urecovery_state |= UBIK_RECHAVEDB;
	} else {
	    /* we don't have the best version; we should fetch it. */
#if defined(UBIK_PAUSE)
	    DBHOLD(ubik_dbase);
#else
	    ObtainWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
	    urecovery_AbortAll(ubik_dbase);

	    /* Rx code to do the Bulk fetch */
	    file = 0;
	    offset = 0;
	    rxcall = rx_NewCall(bestServer->disk_rxcid);

	    ubik_print("Ubik: Synchronize database with server %s\n",
		       afs_inet_ntoa(bestServer->addr[0]));

	    code = StartDISK_GetFile(rxcall, file);
	    if (code) {
		ubik_dprint("StartDiskGetFile failed=%d\n", code);
		goto FetchEndCall;
	    }
	    nbytes = rx_Read(rxcall, &length, sizeof(afs_int32));
	    length = ntohl(length);
	    if (nbytes != sizeof(afs_int32)) {
		ubik_dprint("Rx-read length error=%d\n", code = BULK_ERROR);
		code = EIO;
		goto FetchEndCall;
	    }

	    /* Truncate the file firest */
	    code = (*ubik_dbase->truncate) (ubik_dbase, file, 0);
	    if (code) {
		ubik_dprint("truncate io error=%d\n", code);
		goto FetchEndCall;
	    }

	    /* give invalid label during file transit */
	    tversion.epoch = 0;
	    tversion.counter = 0;
	    code = (*ubik_dbase->setlabel) (ubik_dbase, file, &tversion);
	    if (code) {
		ubik_dprint("setlabel io error=%d\n", code);
		goto FetchEndCall;
	    }

	    while (length > 0) {
		tlen = (length > sizeof(tbuffer) ? sizeof(tbuffer) : length);
		nbytes = rx_Read(rxcall, tbuffer, tlen);
		if (nbytes != tlen) {
		    ubik_dprint("Rx-read bulk error=%d\n", code = BULK_ERROR);
		    code = EIO;
		    goto FetchEndCall;
		}
		nbytes =
		    (*ubik_dbase->write) (ubik_dbase, file, tbuffer, offset,
					  tlen);
		if (nbytes != tlen) {
		    code = UIOERROR;
		    goto FetchEndCall;
		}
		offset += tlen;
		length -= tlen;
	    }
	    code = EndDISK_GetFile(rxcall, &tversion);
	  FetchEndCall:
	    tcode = rx_EndCall(rxcall, code);
	    if (!code)
		code = tcode;
	    if (!code) {
		/* we got a new file, set up its header */
		urecovery_state |= UBIK_RECHAVEDB;
		memcpy(&ubik_dbase->version, &tversion,
		       sizeof(struct ubik_version));
		(*ubik_dbase->sync) (ubik_dbase, 0);	/* get data out first */
		/* after data is good, sync disk with correct label */
		code =
		    (*ubik_dbase->setlabel) (ubik_dbase, 0,
					     &ubik_dbase->version);
	    }
	    if (code) {
		ubik_dbase->version.epoch = 0;
		ubik_dbase->version.counter = 0;
		ubik_print("Ubik: Synchronize database failed (error = %d)\n",
			   code);
	    } else {
		ubik_print("Ubik: Synchronize database completed\n");
	    }
	    udisk_Invalidate(ubik_dbase, 0);	/* data has changed */
	    LWP_NoYieldSignal(&ubik_dbase->version);
#if defined(UBIK_PAUSE)
	    DBRELE(ubik_dbase);
#else
	    ReleaseWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
	}
#if defined(UBIK_PAUSE)
	if (!(urecovery_state & UBIK_RECSYNCSITE))
	    continue;		/* lost sync */
#endif /* UBIK_PAUSE */
	if (!(urecovery_state & UBIK_RECHAVEDB))
	    continue;		/* not ready */

	/* If the database was newly initialized, then when we establish quorum, write
	 * a new label. This allows urecovery_AllBetter() to allow access for reads.
	 * Setting it to 2 also allows another site to come along with a newer
	 * database and overwrite this one.
	 */
	if (ubik_dbase->version.epoch == 1) {
#if defined(UBIK_PAUSE)
	    DBHOLD(ubik_dbase);
#else
	    ObtainWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
	    urecovery_AbortAll(ubik_dbase);
	    ubik_epochTime = 2;
	    ubik_dbase->version.epoch = ubik_epochTime;
	    ubik_dbase->version.counter = 1;
	    code =
		(*ubik_dbase->setlabel) (ubik_dbase, 0, &ubik_dbase->version);
	    udisk_Invalidate(ubik_dbase, 0);	/* data may have changed */
	    LWP_NoYieldSignal(&ubik_dbase->version);
#if defined(UBIK_PAUSE)
	    DBRELE(ubik_dbase);
#else
	    ReleaseWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
	}

	/* Check the other sites and send the database to them if they
	 * do not have the current db.
	 */
	if (!(urecovery_state & UBIK_RECSENTDB)) {
	    /* now propagate out new version to everyone else */
	    dbok = 1;		/* start off assuming they all worked */

#if defined(UBIK_PAUSE)
	    DBHOLD(ubik_dbase);
#else
	    ObtainWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
	    /*
	     * Check if a write transaction is in progress. We can't send the
	     * db when a write is in progress here because the db would be
	     * obsolete as soon as it goes there. Also, ops after the begin
	     * trans would reach the recepient and wouldn't find a transaction
	     * pending there.  Frankly, I don't think it's possible to get past
	     * the write-lock above if there is a write transaction in progress,
	     * but then, it won't hurt to check, will it?
	     */
	    if (ubik_dbase->flags & DBWRITING) {
		struct timeval tv;
		int safety = 0;
		tv.tv_sec = 0;
		tv.tv_usec = 50000;
		while ((ubik_dbase->flags & DBWRITING) && (safety < 500)) {
#if defined(UBIK_PAUSE)
		    DBRELE(ubik_dbase);
#else
		    ReleaseWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
		    /* sleep for a little while */
		    IOMGR_Select(0, 0, 0, 0, &tv);
		    tv.tv_usec += 10000;
		    safety++;
#if defined(UBIK_PAUSE)
		    DBHOLD(ubik_dbase);
#else
		    ObtainWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
		}
	    }

	    for (ts = ubik_servers; ts; ts = ts->next) {
		inAddr.s_addr = ts->addr[0];
		if (!ts->up) {
		    ubik_dprint("recovery cannot send version to %s\n",
				afs_inet_ntoa(inAddr.s_addr));
		    dbok = 0;
		    continue;
		}
		ubik_dprint("recovery sending version to %s\n",
			    afs_inet_ntoa(inAddr.s_addr));
		if (vcmp(ts->version, ubik_dbase->version) != 0) {
		    ubik_dprint("recovery stating local database\n");

		    /* Rx code to do the Bulk Store */
		    code = (*ubik_dbase->stat) (ubik_dbase, 0, &ubikstat);
		    if (!code) {
			length = ubikstat.size;
			file = offset = 0;
			rxcall = rx_NewCall(ts->disk_rxcid);
			code =
			    StartDISK_SendFile(rxcall, file, length,
					       &ubik_dbase->version);
			if (code) {
			    ubik_dprint("StartDiskSendFile failed=%d\n",
					code);
			    goto StoreEndCall;
			}
			while (length > 0) {
			    tlen =
				(length >
				 sizeof(tbuffer) ? sizeof(tbuffer) : length);
			    nbytes =
				(*ubik_dbase->read) (ubik_dbase, file,
						     tbuffer, offset, tlen);
			    if (nbytes != tlen) {
				ubik_dprint("Local disk read error=%d\n",
					    code = UIOERROR);
				goto StoreEndCall;
			    }
			    nbytes = rx_Write(rxcall, tbuffer, tlen);
			    if (nbytes != tlen) {
				ubik_dprint("Rx-write bulk error=%d\n", code =
					    BULK_ERROR);
				goto StoreEndCall;
			    }
			    offset += tlen;
			    length -= tlen;
			}
			code = EndDISK_SendFile(rxcall);
		      StoreEndCall:
			code = rx_EndCall(rxcall, code);
		    }
		    if (code == 0) {
			/* we set a new file, process its header */
			ts->version = ubik_dbase->version;
			ts->currentDB = 1;
		    } else
			dbok = 0;
		} else {
		    /* mark file up to date */
		    ts->currentDB = 1;
		}
	    }
#if defined(UBIK_PAUSE)
	    DBRELE(ubik_dbase);
#else
	    ReleaseWriteLock(&ubik_dbase->versionLock);
#endif /* UBIK_PAUSE */
	    if (dbok)
		urecovery_state |= UBIK_RECSENTDB;
	}
    }
}

/*
** send a Probe to all the network address of this server 
** Return 0  if success, else return 1
*/
int
DoProbe(struct ubik_server *server)
{
    struct rx_connection *conns[UBIK_MAX_INTERFACE_ADDR];
    struct rx_connection *connSuccess = 0;
    int i, j;
    afs_uint32 addr;
    char buffer[32];
    extern afs_int32 ubikSecIndex;
    extern struct rx_securityClass *ubikSecClass;

    for (i = 0; (addr = server->addr[i]) && (i < UBIK_MAX_INTERFACE_ADDR);
	 i++) {
	conns[i] =
	    rx_NewConnection(addr, ubik_callPortal, DISK_SERVICE_ID,
			     ubikSecClass, ubikSecIndex);

	/* user requirement to use only the primary interface */
	if (ubikPrimaryAddrOnly) {
	    i = 1;
	    break;
	}
    }
    assert(i);			/* at least one interface address for this server */

    multi_Rx(conns, i) {
	multi_DISK_Probe();
	if (!multi_error) {	/* first success */
	    addr = server->addr[multi_i];	/* successful interface addr */

	    if (server->disk_rxcid)	/* destroy existing conn */
		rx_DestroyConnection(server->disk_rxcid);
	    if (server->vote_rxcid)
		rx_DestroyConnection(server->vote_rxcid);

	    /* make new connections */
	    server->disk_rxcid = conns[multi_i];
	    server->vote_rxcid = rx_NewConnection(addr, ubik_callPortal, VOTE_SERVICE_ID, ubikSecClass, ubikSecIndex);	/* for vote reqs */

	    connSuccess = conns[multi_i];
	    strcpy(buffer, (char *)afs_inet_ntoa(server->addr[0]));
	    ubik_print
		("ubik:server %s is back up: will be contacted through %s\n",
		 buffer, afs_inet_ntoa(addr));

	    multi_Abort;
	}
    } multi_End_Ignore;

    /* Destroy all connections except the one on which we succeeded */
    for (j = 0; j < i; j++)
	if (conns[j] != connSuccess)
	    rx_DestroyConnection(conns[j]);

    if (!connSuccess)
	ubik_dprint("ubik:server %s still down\n",
		    afs_inet_ntoa(server->addr[0]));

    if (connSuccess)
	return 0;		/* success */
    else
	return 1;		/* failure */
}
