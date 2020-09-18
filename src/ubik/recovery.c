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

#include <roken.h>

#include <afs/opr.h>

#ifdef AFS_PTHREAD_ENV
# include <opr/lock.h>
#else
# include <opr/lockstub.h>
#endif

#include <rx/rx.h>
#include <afs/afsutil.h>
#include <afs/cellconfig.h>


#define UBIK_INTERNALS
#include "ubik.h"
#include "ubik_int.h"

/*! \file
 * This module is responsible for determining when the system has
 * recovered to the point that it can handle new transactions.  It
 * replays logs, polls to determine the current dbase after a crash,
 * and distributes the new database to the others.
 *
 * The sync site associates a version number with each database.  It
 * broadcasts the version associated with its current dbase in every
 * one of its beacon messages.  When the sync site send a dbase to a
 * server, it also sends the db's version.  A non-sync site server can
 * tell if it has the right dbase version by simply comparing the
 * version from the beacon message \p uvote_dbVersion with the version
 * associated with the database \p ubik_dbase->version.  The sync site
 * itself simply has one counter to keep track of all of this (again
 * \p ubik_dbase->version).
 *
 * sync site: routine called when the sync site loses its quorum; this
 * procedure is called "up" from the beacon package.  It resyncs the
 * dbase and nudges the recovery daemon to try to propagate out the
 * changes.  It also resets the recovery daemon's state, since
 * recovery must potentially find a new dbase to propagate out.  This
 * routine should not do anything with variables used by non-sync site
 * servers.
 */

/*!
 * if this flag is set, then ubik will use only the primary address
 * (the address specified in the CellServDB) to contact other
 * ubik servers. Ubik recovery will not try opening connections
 * to the alternate interface addresses.
 */
int ubikPrimaryAddrOnly;

int
urecovery_ResetState(void)
{
    urecovery_state = 0;
    return 0;
}

/*!
 * \brief sync site
 *
 * routine called when a non-sync site server goes down; restarts recovery
 * process to send missing server the new db when it comes back up for
 * non-sync site servers.
 *
 * \note This routine should not do anything with variables used by non-sync site servers.
 */
int
urecovery_LostServer(struct ubik_server *ts)
{
    ubeacon_ReinitServer(ts);
    return 0;
}

/*!
 * return true iff we have a current database (called by both sync
 * sites and non-sync sites) How do we determine this?  If we're the
 * sync site, we wait until recovery has finished fetching and
 * re-labelling its dbase (it may still be trying to propagate it out
 * to everyone else; that's THEIR problem).  If we're not the sync
 * site, then we must have a dbase labelled with the right version,
 * and we must have a currently-good sync site.
 */
int
urecovery_AllBetter(struct ubik_dbase *adbase, int areadAny)
{
    afs_int32 rcode;

    ViceLog(25, ("allbetter checking\n"));
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
    else if (uvote_HaveSyncAndVersion(ubik_dbase->version)) {
	rcode = 1;
    }

    ViceLog(25, ("allbetter: returning %d\n", rcode));
    return rcode;
}

/*!
 * \brief abort all transactions on this database
 */
int
urecovery_AbortAll(struct ubik_dbase *adbase)
{
    struct ubik_trans *tt;
    int reads = 0, writes = 0;

    for (tt = adbase->activeTrans; tt; tt = tt->next) {
	if (tt->type == UBIK_WRITETRANS)
	    writes++;
	else
	    reads++;
	udisk_abort(tt);
    }
    ViceLog(0, ("urecovery_AbortAll: just aborted %d read and %d write transactions\n",
		    reads, writes));
    return 0;
}

/*!
 * \brief this routine aborts the current remote transaction, if any, if the tid is wrong
 */
int
urecovery_CheckTid(struct ubik_tid *atid, int abortalways)
{
    if (ubik_currentTrans) {
	/* there is remote write trans, see if we match, see if this
	 * is a new transaction */
	if (atid->epoch != ubik_currentTrans->tid.epoch
	    || atid->counter > ubik_currentTrans->tid.counter || abortalways) {
	    /* don't match, abort it */
	    int endit = 0;
	    /* If the thread is not waiting for lock - ok to end it */
	    if (ubik_currentTrans->locktype != LOCKWAIT) {
		endit = 1;
	    }

	    ViceLog(0, ("urecovery_CheckTid: Aborting/ending bad remote "
			"transaction. (tx %d.%d, atid %d.%d, abortalways %d, "
			"endit %d)\n",
			ubik_currentTrans->tid.epoch,
			ubik_currentTrans->tid.counter,
			atid->epoch, atid->counter,
			abortalways, endit));
	    if (endit) {
		udisk_end(ubik_currentTrans);
	    }
	    ubik_currentTrans = (struct ubik_trans *)0;
	}
    }
    return 0;
}

/*!
 * \brief replay logs
 *
 * log format is defined here, and implicitly in disk.c
 *
 * 4 byte opcode, followed by parameters, each 4 bytes long.  All integers
 * are in logged in network standard byte order, in case we want to move logs
 * from machine-to-machine someday.
 *
 * Begin transaction: opcode \n
 * Commit transaction: opcode, version (8 bytes) \n
 * Truncate file: opcode, file number, length \n
 * Abort transaction: opcode \n
 * Write data: opcode, file, position, length, <length> data bytes \n
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
static int
ReplayLog(struct ubik_dbase *adbase)
{
    afs_int32 opcode;
    afs_int32 code, tpos;
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
	    (*adbase->read) (adbase, LOGFILE, (char *)&opcode, tpos,
			     sizeof(afs_int32));
	if (code != sizeof(afs_int32))
	    break;
	opcode = ntohl(opcode);
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
		(*adbase->read) (adbase, LOGFILE, (char *)buffer, tpos,
				 2 * sizeof(afs_int32));
	    if (code != 2 * sizeof(afs_int32))
		break;		/* premature eof or io error */
	    tpos += 2 * sizeof(afs_int32);
	} else if (opcode == LOGDATA) {
	    tpos += 4;
	    code =
		(*adbase->read) (adbase, LOGFILE, (char *)buffer, tpos,
				 3 * sizeof(afs_int32));
	    if (code != 3 * sizeof(afs_int32))
		break;
	    /* otherwise, skip over the data bytes, too */
	    tpos += ntohl(buffer[2]) + 3 * sizeof(afs_int32);
	} else {
	    ViceLog(0, ("corrupt log opcode (%d) at position %d\n", opcode,
		       tpos));
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
		(*adbase->read) (adbase, LOGFILE, (char *)&opcode, tpos,
				 sizeof(afs_int32));
	    if (code != sizeof(afs_int32))
		break;
	    opcode = ntohl(opcode);
	    if (opcode == LOGNEW) {
		/* handle begin trans */
		tpos += sizeof(afs_int32);
	    } else if (opcode == LOGABORT)
		panic("log abort\n");
	    else if (opcode == LOGEND) {
		struct ubik_version version;
		tpos += 4;
		code =
		    (*adbase->read) (adbase, LOGFILE, (char *)buffer, tpos,
				     2 * sizeof(afs_int32));
		if (code != 2 * sizeof(afs_int32))
		    return UBADLOG;
		version.epoch = ntohl(buffer[0]);
		version.counter = ntohl(buffer[1]);
		code = (*adbase->setlabel) (adbase, 0, &version);
		if (code)
		    return code;
		ViceLog(0, ("Successfully replayed log for interrupted "
		           "transaction; db version is now %ld.%ld\n",
		           (long) version.epoch, (long) version.counter));
		logIsGood = 1;
		break;		/* all done now */
	    } else if (opcode == LOGTRUNCATE) {
		tpos += 4;
		code =
		    (*adbase->read) (adbase, LOGFILE, (char *)buffer, tpos,
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
		    (*adbase->read) (adbase, LOGFILE, (char *)buffer, tpos,
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
			(*adbase->read) (adbase, LOGFILE, (char *)data, tpos,
					 thisSize);
		    if (code != thisSize)
			return UBADLOG;
		    code =
			(*adbase->write) (adbase, tfile, (char *)data, filePos,
					  thisSize);
		    if (code != thisSize)
			return UBADLOG;
		    filePos += thisSize;
		    tpos += thisSize;
		    len -= thisSize;
		}
	    } else {
		ViceLog(0, ("corrupt log opcode (%d) at position %d\n",
			   opcode, tpos));
		break;		/* corrupt log! */
	    }
	}
	if (logIsGood) {
	    if (syncFile >= 0)
		code = (*adbase->sync) (adbase, syncFile);
	    if (code)
		return code;
	} else {
	    ViceLog(0, ("Log read error on pass 2\n"));
	    return UBADLOG;
	}
    }

    /* now truncate the log, we're done with it */
    code = (*adbase->truncate) (adbase, LOGFILE, 0);
    return code;
}

/*! \brief
 * Called at initialization to figure out version of the dbase we really have.
 *
 * This routine is called after replaying the log; it reads the restored labels.
 */
static int
InitializeDB(struct ubik_dbase *adbase)
{
    afs_int32 code;

    code = (*adbase->getlabel) (adbase, 0, &adbase->version);
    if (code) {
	/* try setting the label to a new value */
	UBIK_VERSION_LOCK;
	adbase->version.epoch = 1;	/* value for newly-initialized db */
	adbase->version.counter = 1;
	code = (*adbase->setlabel) (adbase, 0, &adbase->version);
	if (code) {
	    /* failed, try to set it back */
	    adbase->version.epoch = 0;
	    adbase->version.counter = 0;
	    (*adbase->setlabel) (adbase, 0, &adbase->version);
	}
	UBIK_VERSION_UNLOCK;
    }
    return 0;
}

/*!
 * \brief initialize the local ubik_dbase
 *
 * We replay the logs and then read the resulting file to figure out what version we've really got.
 */
int
urecovery_Initialize(struct ubik_dbase *adbase)
{
    afs_int32 code;

    DBHOLD(adbase);
    code = ReplayLog(adbase);
    if (code)
	goto done;
    code = InitializeDB(adbase);
done:
    DBRELE(adbase);
    return code;
}

/*!
 * \brief Main interaction loop for the recovery manager
 *
 * The recovery light-weight process only runs when you're the
 * synchronization site.  It performs the following tasks, if and only
 * if the prerequisite tasks have been performed successfully (it
 * keeps track of which ones have been performed in its bit map,
 * \p urecovery_state).
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
void *
urecovery_Interact(void *dummy)
{
    afs_int32 code;
    struct ubik_server *bestServer = NULL;
    struct ubik_server *ts;
    int dbok, doingRPC, now;
    afs_int32 lastProbeTime;
    /* if we're the sync site, the best db version we've found yet */
    static struct ubik_version bestDBVersion;
    struct ubik_version tversion;
    struct timeval tv;
    int length, tlen, offset, file, nbytes;
    struct rx_call *rxcall;
    char tbuffer[1024];
    struct ubik_stat ubikstat;
    struct in_addr inAddr;
    char hoststr[16];
    char pbuffer[1028];
    int fd = -1;
    afs_int32 pass;
    int first;

    memset(pbuffer, 0, sizeof(pbuffer));

    opr_threadname_set("recovery");

    /* otherwise, begin interaction */
    urecovery_state = 0;
    lastProbeTime = 0;
    for (first = 1; ; first = 0) {
	if (!first) {
	    /* Run through this loop every 4 seconds (but don't wait 4 seconds
	     * the first time around). */
	    tv.tv_sec = 4;
	    tv.tv_usec = 0;
#ifdef AFS_PTHREAD_ENV
	    select(0, 0, 0, 0, &tv);
#else
	    IOMGR_Select(0, 0, 0, 0, &tv);
#endif
	}

	ViceLog(5, ("recovery running in state %x\n", urecovery_state));

	/* Every 30 seconds, check all the down servers and mark them
	 * as up if they respond. When a server comes up or found to
	 * not be current, then re-find the the best database and
	 * propogate it.
	 */
	if ((now = FT_ApproxTime()) > 30 + lastProbeTime) {

	    for (ts = ubik_servers, doingRPC = 0; ts; ts = ts->next) {
		UBIK_BEACON_LOCK;
		if (!ts->up) {
		    UBIK_BEACON_UNLOCK;
		    doingRPC = 1;
		    code = DoProbe(ts);
		    if (code == 0) {
			UBIK_BEACON_LOCK;
			ts->up = 1;
			UBIK_BEACON_UNLOCK;
			DBHOLD(ubik_dbase);
			urecovery_state &= ~UBIK_RECFOUNDDB;
			DBRELE(ubik_dbase);
		    }
		} else {
		    UBIK_BEACON_UNLOCK;
		    DBHOLD(ubik_dbase);
		    if (!ts->currentDB)
			urecovery_state &= ~UBIK_RECFOUNDDB;
		    DBRELE(ubik_dbase);
		}
	    }

	    if (doingRPC)
		now = FT_ApproxTime();
	    lastProbeTime = now;
	}

	/* Mark whether we are the sync site */
	DBHOLD(ubik_dbase);
	if (!ubeacon_AmSyncSite()) {
	    urecovery_state &= ~UBIK_RECSYNCSITE;
	    DBRELE(ubik_dbase);
	    continue;		/* nothing to do */
	}
	urecovery_state |= UBIK_RECSYNCSITE;

	/* If a server has just come up or if we have not found the
	 * most current database, then go find the most current db.
	 */
	if (!(urecovery_state & UBIK_RECFOUNDDB)) {
            int okcalls = 0;
	    DBRELE(ubik_dbase);
	    bestServer = (struct ubik_server *)0;
	    bestDBVersion.epoch = 0;
	    bestDBVersion.counter = 0;
	    for (ts = ubik_servers; ts; ts = ts->next) {
		UBIK_BEACON_LOCK;
		if (!ts->up) {
		    UBIK_BEACON_UNLOCK;
		    continue;	/* don't bother with these guys */
		}
		UBIK_BEACON_UNLOCK;
		if (ts->isClone)
		    continue;
		UBIK_ADDR_LOCK;
		code = DISK_GetVersion(ts->disk_rxcid, &ts->version);
		UBIK_ADDR_UNLOCK;
		if (code == 0) {
                    okcalls++;
		    /* perhaps this is the best version */
		    if (vcmp(ts->version, bestDBVersion) > 0) {
			/* new best version */
			bestDBVersion = ts->version;
			bestServer = ts;
		    }
		}
	    }

	    DBHOLD(ubik_dbase);

            if (okcalls + 1 >= ubik_quorum) {
                /* If we've asked a majority of sites about their db version,
                 * then we can say with confidence that we've found the best db
                 * version. If we haven't contacted most sites (because
                 * GetVersion failed or because we already know the server is
                 * down), then we don't really know if we know about the best
                 * db version. So we can only proceed in here if 'okcalls'
                 * indicates we managed to contact a majority of sites. */

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
                urecovery_state |= UBIK_RECFOUNDDB;
                urecovery_state &= ~UBIK_RECSENTDB;
            }
	}
	if (!(urecovery_state & UBIK_RECFOUNDDB)) {
	    DBRELE(ubik_dbase);
	    continue;		/* not ready */
	}

	/* If we, the sync site, do not have the best db version, then
	 * go and get it from the server that does.
	 */
	if ((urecovery_state & UBIK_RECHAVEDB) || !bestServer) {
	    urecovery_state |= UBIK_RECHAVEDB;
	} else {
	    /* we don't have the best version; we should fetch it. */
	    urecovery_AbortAll(ubik_dbase);

	    pbuffer[0] = '\0';

	    /* Rx code to do the Bulk fetch */
	    file = 0;
	    offset = 0;
	    UBIK_ADDR_LOCK;
	    rxcall = rx_NewCall(bestServer->disk_rxcid);

	    ViceLog(0, ("Ubik: Synchronize database: receive (via GetFile) "
			"from server %s begin\n",
		       afs_inet_ntoa_r(bestServer->addr[0], hoststr)));
	    UBIK_ADDR_UNLOCK;

	    code = StartDISK_GetFile(rxcall, file);
	    if (code) {
		ViceLog(0, ("StartDiskGetFile failed=%d\n", code));
		goto FetchEndCall;
	    }
	    nbytes = rx_Read(rxcall, (char *)&length, sizeof(afs_int32));
	    length = ntohl(length);
	    if (nbytes != sizeof(afs_int32)) {
		ViceLog(0, ("Rx-read length error=%d\n", BULK_ERROR));
		code = EIO;
		goto FetchEndCall;
	    }

	    /* give invalid label during file transit */
	    UBIK_VERSION_LOCK;
	    tversion.epoch = 0;
	    code = (*ubik_dbase->setlabel) (ubik_dbase, file, &tversion);
	    UBIK_VERSION_UNLOCK;
	    if (code) {
		ViceLog(0, ("setlabel io error=%d\n", code));
		goto FetchEndCall;
	    }
	    snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.TMP",
		     ubik_dbase->pathName, (file<0)?"SYS":"",
		     (file<0)?-file:file);
	    fd = open(pbuffer, O_CREAT | O_RDWR | O_TRUNC, 0600);
	    if (fd < 0) {
		code = errno;
		goto FetchEndCall;
	    }
	    code = lseek(fd, HDRSIZE, 0);
	    if (code != HDRSIZE) {
		close(fd);
		goto FetchEndCall;
	    }

	    pass = 0;
	    while (length > 0) {
		tlen = (length > sizeof(tbuffer) ? sizeof(tbuffer) : length);
#ifndef AFS_PTHREAD_ENV
		if (pass % 4 == 0)
		    IOMGR_Poll();
#endif
		nbytes = rx_Read(rxcall, tbuffer, tlen);
		if (nbytes != tlen) {
		    ViceLog(0, ("Rx-read bulk error=%d\n", BULK_ERROR));
		    code = EIO;
		    close(fd);
		    goto FetchEndCall;
		}
		nbytes = write(fd, tbuffer, tlen);
		pass++;
		if (nbytes != tlen) {
		    code = UIOERROR;
		    close(fd);
		    goto FetchEndCall;
		}
		offset += tlen;
		length -= tlen;
	    }
	    code = close(fd);
	    if (code)
		goto FetchEndCall;
	    code = EndDISK_GetFile(rxcall, &tversion);
	  FetchEndCall:
	    code = rx_EndCall(rxcall, code);
	    if (!code) {
		/* we got a new file, set up its header */
		urecovery_state |= UBIK_RECHAVEDB;
		UBIK_VERSION_LOCK;
		memcpy(&ubik_dbase->version, &tversion,
		       sizeof(struct ubik_version));
		snprintf(tbuffer, sizeof(tbuffer), "%s.DB%s%d",
			 ubik_dbase->pathName, (file<0)?"SYS":"",
			 (file<0)?-file:file);
#ifdef AFS_NT40_ENV
		snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.OLD",
			 ubik_dbase->pathName, (file<0)?"SYS":"",
			 (file<0)?-file:file);
		code = unlink(pbuffer);
		if (!code)
		    code = rename(tbuffer, pbuffer);
		snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.TMP",
			 ubik_dbase->pathName, (file<0)?"SYS":"",
			 (file<0)?-file:file);
#endif
		if (!code)
		    code = rename(pbuffer, tbuffer);
		if (!code) {
		    (*ubik_dbase->open) (ubik_dbase, file);
		    /* after data is good, sync disk with correct label */
		    code =
			(*ubik_dbase->setlabel) (ubik_dbase, 0,
						 &ubik_dbase->version);
		}
		UBIK_VERSION_UNLOCK;
#ifdef AFS_NT40_ENV
		snprintf(pbuffer, sizeof(pbuffer), "%s.DB%s%d.OLD",
			 ubik_dbase->pathName, (file<0)?"SYS":"",
			 (file<0)?-file:file);
		unlink(pbuffer);
#endif
	    }
	    if (code) {
		if (pbuffer[0] != '\0') {
		    unlink(pbuffer);
		}
		/*
		 * We will effectively invalidate the old data forever now.
		 * Unclear if we *should* but we do.
		 */
		UBIK_VERSION_LOCK;
		ubik_dbase->version.epoch = 0;
		ubik_dbase->version.counter = 0;
		UBIK_VERSION_UNLOCK;
		ViceLog(0,
		    ("Ubik: Synchronize database: receive (via GetFile) "
		    "from server %s failed (error = %d)\n",
		    afs_inet_ntoa_r(bestServer->addr[0], hoststr), code));
	    } else {
		ViceLog(0,
		    ("Ubik: Synchronize database: receive (via GetFile) "
		    "from server %s complete, version: %d.%d\n",
		    afs_inet_ntoa_r(bestServer->addr[0], hoststr),
		    ubik_dbase->version.epoch, ubik_dbase->version.counter));

		urecovery_state |= UBIK_RECHAVEDB;
	    }
	    udisk_Invalidate(ubik_dbase, 0);	/* data has changed */
	}
	if (!(urecovery_state & UBIK_RECHAVEDB)) {
	    DBRELE(ubik_dbase);
	    continue;		/* not ready */
	}

	/* If the database was newly initialized, then when we establish quorum, write
	 * a new label. This allows urecovery_AllBetter() to allow access for reads.
	 * Setting it to 2 also allows another site to come along with a newer
	 * database and overwrite this one.
	 */
	if (ubik_dbase->version.epoch == 1) {
	    urecovery_AbortAll(ubik_dbase);
	    UBIK_VERSION_LOCK;
	    ubik_dbase->version.epoch = 2;
	    ubik_dbase->version.counter = 1;
	    code =
		(*ubik_dbase->setlabel) (ubik_dbase, 0, &ubik_dbase->version);
	    UBIK_VERSION_UNLOCK;
	    udisk_Invalidate(ubik_dbase, 0);	/* data may have changed */
	}

	/* Check the other sites and send the database to them if they
	 * do not have the current db.
	 */
	if (!(urecovery_state & UBIK_RECSENTDB)) {
	    /* now propagate out new version to everyone else */
	    dbok = 1;		/* start off assuming they all worked */

	    /*
	     * Check if a write transaction is in progress. We can't send the
	     * db when a write is in progress here because the db would be
	     * obsolete as soon as it goes there. Also, ops after the begin
	     * trans would reach the recepient and wouldn't find a transaction
	     * pending there.  Frankly, I don't think it's possible to get past
	     * the write-lock above if there is a write transaction in progress,
	     * but then, it won't hurt to check, will it?
	     */
	    if (ubik_dbase->dbFlags & DBWRITING) {
		struct timeval tv;
		int safety = 0;
		long cur_usec = 50000;
		while ((ubik_dbase->dbFlags & DBWRITING) && (safety < 500)) {
		    DBRELE(ubik_dbase);
		    /* sleep for a little while */
		    tv.tv_sec = 0;
		    tv.tv_usec = cur_usec;
#ifdef AFS_PTHREAD_ENV
		    select(0, 0, 0, 0, &tv);
#else
		    IOMGR_Select(0, 0, 0, 0, &tv);
#endif
		    cur_usec += 10000;
		    safety++;
		    DBHOLD(ubik_dbase);
		}
	    }

	    for (ts = ubik_servers; ts; ts = ts->next) {
		UBIK_ADDR_LOCK;
		inAddr.s_addr = ts->addr[0];
		UBIK_ADDR_UNLOCK;
		UBIK_BEACON_LOCK;
		if (!ts->up) {
		    UBIK_BEACON_UNLOCK;
		    /* It would be nice to have this message at loglevel
		     * 0 as well, but it will log once every 4s for each
		     * down server while in this recovery state.  This
		     * should only be changed to loglevel 0 if it is
		     * also rate-limited.
		     */
		    ViceLog(5, ("recovery cannot send version to %s\n",
				afs_inet_ntoa_r(inAddr.s_addr, hoststr)));
		    dbok = 0;
		    continue;
		}
		UBIK_BEACON_UNLOCK;

		if (vcmp(ts->version, ubik_dbase->version) != 0) {
		    ViceLog(0, ("Synchronize database: send (via SendFile) "
				"to server %s begin\n",
			    afs_inet_ntoa_r(inAddr.s_addr, hoststr)));

		    /* Rx code to do the Bulk Store */
		    code = (*ubik_dbase->stat) (ubik_dbase, 0, &ubikstat);
		    if (!code) {
			length = ubikstat.size;
			file = offset = 0;
			UBIK_ADDR_LOCK;
			rxcall = rx_NewCall(ts->disk_rxcid);
			UBIK_ADDR_UNLOCK;
			code =
			    StartDISK_SendFile(rxcall, file, length,
					       &ubik_dbase->version);
			if (code) {
			    ViceLog(0, ("StartDiskSendFile failed=%d\n",
					code));
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
				code = UIOERROR;
				ViceLog(0, ("Local disk read error=%d\n", code));
				goto StoreEndCall;
			    }
			    nbytes = rx_Write(rxcall, tbuffer, tlen);
			    if (nbytes != tlen) {
				code = BULK_ERROR;
				ViceLog(0, ("Rx-write bulk error=%d\n", code));
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
			ViceLog(0,
			    ("Ubik: Synchronize database: send (via SendFile) "
			    "to server %s complete, version: %d.%d\n",
			    afs_inet_ntoa_r(inAddr.s_addr, hoststr),
			    ts->version.epoch, ts->version.counter));

		    } else {
			dbok = 0;
			ViceLog(0,
			    ("Ubik: Synchronize database: send (via SendFile) "
			     "to server %s failed (error = %d)\n",
			    afs_inet_ntoa_r(inAddr.s_addr, hoststr), code));
		    }
		} else {
		    /* mark file up to date */
		    ts->currentDB = 1;
		}
	    }
	    if (dbok)
		urecovery_state |= UBIK_RECSENTDB;
	}
	DBRELE(ubik_dbase);
    }
    AFS_UNREACHED(return(NULL));
}

/*!
 * \brief send a Probe to all the network address of this server
 *
 * \return 0 if success, else return 1
 */
int
DoProbe(struct ubik_server *server)
{
    struct rx_connection *conns[UBIK_MAX_INTERFACE_ADDR];
    struct rx_connection *connSuccess = 0;
    int i, nconns, success_i = -1;
    afs_uint32 addr;
    char buffer[32];
    char hoststr[16];

    UBIK_ADDR_LOCK;
    for (i = 0; (i < UBIK_MAX_INTERFACE_ADDR) && (addr = server->addr[i]);
	 i++) {
	conns[i] =
	    rx_NewConnection(addr, ubik_callPortal, DISK_SERVICE_ID,
			     addr_globals.ubikSecClass, addr_globals.ubikSecIndex);

	/* user requirement to use only the primary interface */
	if (ubikPrimaryAddrOnly) {
	    i = 1;
	    break;
	}
    }
    UBIK_ADDR_UNLOCK;
    nconns = i;
    opr_Assert(nconns);			/* at least one interface address for this server */

    multi_Rx(conns, nconns) {
	multi_DISK_Probe();
	if (!multi_error) {	/* first success */
	    success_i = multi_i;

	    multi_Abort;
	}
    } multi_End;

    if (success_i >= 0) {
	UBIK_ADDR_LOCK;
	addr = server->addr[success_i];	/* successful interface addr */

	if (server->disk_rxcid)	/* destroy existing conn */
	    rx_DestroyConnection(server->disk_rxcid);
	if (server->vote_rxcid)
	    rx_DestroyConnection(server->vote_rxcid);

	/* make new connections */
	server->disk_rxcid = conns[success_i];
	server->vote_rxcid = rx_NewConnection(addr, ubik_callPortal,
	                                      VOTE_SERVICE_ID, addr_globals.ubikSecClass,
	                                      addr_globals.ubikSecIndex);

	connSuccess = conns[success_i];
	strcpy(buffer, afs_inet_ntoa_r(server->addr[0], hoststr));

	ViceLog(0, ("ubik:server %s is back up: will be contacted through %s\n",
	     buffer, afs_inet_ntoa_r(addr, hoststr)));
	UBIK_ADDR_UNLOCK;
    }

    /* Destroy all connections except the one on which we succeeded */
    for (i = 0; i < nconns; i++)
	if (conns[i] != connSuccess)
	    rx_DestroyConnection(conns[i]);

    if (!connSuccess)
	ViceLog(5, ("ubik:server %s still down\n",
		    afs_inet_ntoa_r(server->addr[0], hoststr)));

    if (connSuccess)
	return 0;		/* success */
    else
	return 1;		/* failure */
}
